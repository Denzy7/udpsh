#include <arpa/inet.h> /* inet_ntoa */
#include <errno.h>         /* errno */
#include <pthread.h>
#include <signal.h> /* signal */
#include <stdio.h>        /* snprintf */
#include <stdlib.h>     /* exit */
#include <string.h>     /* memset */
#include <sys/wait.h> /* waitpid */
#include <time.h>         /* time(NULL) */
#include <unistd.h>     /* fork, execvp */

#include "udpsh_server.h"
#include "udpsh_sock.h"
#include "udpsh_util.h"

struct udpsh_server_session {
    pthread_t thread;
    pthread_cond_t cond;
    pthread_mutex_t mut;
    int id;
    struct udpsh_sock sock;
    struct udpsh_sock global_sock;
    char cmdbuf[UDPSH_SOCK_BUFSZ / 2];
};

struct udpsh_sock sock_server;
struct udpsh_server_session sessions[4];
int pktloss = 0;
int usessl = 0;

/* session thread function */
void *session(void *arg);

/* wake a waiting session */
void seswake(struct udpsh_server_session *ses);

/* join a finished session thread */
void sesjoin(struct udpsh_server_session *ses);

/* invalidate session */
void sesinval(struct udpsh_server_session *ses);

/* send client acknowledgement */
void clientack(struct udpsh_sock *sock);

/* compare address*/
int addrcmp(struct in_addr *a, struct in_addr *b);

/* send big buffer via smaller sock buffer */
void sendchunked(const void *buf, const size_t buflen, struct udpsh_sock *sock);

/* signal that causes program to exit */
void sigexits(int sig);

void serverhelp() {
    printf("udpsh_server help\n"
                 "-pktloss PACKET_LOSS\n"
                 "-sslcert CERT: use with -sslkey\n"
                 "-sslkey KEY: use with -sslcert\n"
                 "\n");
}

int main(int argc, char *argv[]) {
    const char *cert = NULL, *key = NULL;
    serverhelp();
    off_t certlen = 0;
    char *certmem = NULL;
    for (int i = 0; i < argc; i++) {
        if (strstr(argv[i], "-pktloss")) {
            sscanf(argv[i + 1], "%d", &pktloss);
            printf("set pktloss = %d\n", pktloss);
        } else if (strstr(argv[i], "-sslcert")) {
            cert = argv[i + 1];
            printf("set sslcert = %s\n", cert);
        } else if (strstr(argv[i], "-sslkey")) {
            key = argv[i + 1];
            printf("set sslkey = %s\n", key);
        }
    }

    signal(SIGINT, sigexits);

    srand(time(NULL));

    memset(&sessions, 0, sizeof(sessions));
    memset(&sock_server, 0, sizeof(sock_server));

    if (udpsh_sock_make(NULL, &sock_server) != 0)
        return 1;
    if (udpsh_sock_bind(&sock_server) != 0)
        return 1;

    if (cert && key) {
        if (udpsh_sock_ssl_init(&sock_server, 1) == 1 ||
                udpsh_sock_ssl_server(&sock_server, cert, key) == 1) {
            fprintf(stderr, "ssl init failed\n");
        } else {
            usessl = 1;
            FILE *certfile = fopen(cert, "rb");
            fseek(certfile, 0, SEEK_END);
            certlen = ftello(certfile);
            rewind(certfile);

            certmem = malloc(certlen);
            fread(certmem, 1, certlen, certfile);
            fclose(certfile);
        }
    }

    while (1) {
        printf("waiting for global msg\n");
        struct udpsh_sock sock_global_client;
        memset(&sock_global_client, 0, sizeof(sock_global_client));
        sock_global_client.addrlen = sizeof(struct sockaddr_in);
        sock_global_client.sock = sock_server.sock;
        udpsh_sock_recv(&sock_server, &sock_global_client.addr,
                                        &sock_global_client.addrlen);
        clientack(&sock_global_client);

        printf("from %s\nbuffer=%s\n", inet_ntoa(sock_global_client.addr.sin_addr),
                     sock_server.buffer);

        int ssl_session = 0;
        if (strncmp(sock_server.buffer, UDPSH_SERVER_FUN_SSL,
                                strlen(UDPSH_SERVER_FUN_SSL)) == 0) {
            snprintf(sock_global_client.buffer, UDPSH_SOCK_BUFSZ, "%d", usessl);
            udpsh_sock_send(&sock_global_client);
            if (usessl) {
                udpsh_sock_ssl_accept(&sock_server, &sock_global_client.addr,
                                                            sock_global_client.addrlen);
                printf("accept ssl from %s\n",
                             inet_ntoa(sock_global_client.addr.sin_addr));
                udpsh_sock_ssl_read(&sock_server);
                ssl_session = 1;
                printf("sslread: %s\n", sock_server.buffer);
                sock_global_client.ssl = sock_server.ssl;
            }
        }

        /* nasty inextensible code here! */
        int parse_sessionid = 0;
        char parse_buf[UDPSH_SOCK_BUFSZ];
        strncpy(parse_buf, sock_server.buffer, UDPSH_SOCK_BUFSZ);
        char parse_cmdbuf[UDPSH_SOCK_BUFSZ / 2];
        char parse_fun[3];
        const char *tok = NULL;
        tok = strtok(parse_buf, UDPSH_SERVER_TOK);
        sscanf(tok, "%s", parse_fun);
        tok = strtok(NULL, UDPSH_SERVER_TOK);
        if (tok != NULL) {
            sscanf(tok, "%d", &parse_sessionid);
            tok = strtok(NULL, UDPSH_SERVER_TOK);
            if (tok != NULL) {
                strncpy(parse_cmdbuf, tok, sizeof(parse_cmdbuf));
            }
        }

        struct udpsh_server_session *session_global =
                &sessions[parse_sessionid - 1];
        if (strncmp(sock_server.buffer, UDPSH_SERVER_FUN_CON,
                                strlen(UDPSH_SERVER_FUN_CON)) == 0) {
            int sessionid = UDPSH_SERVER_SES_INV;
            static const char *invsesserr = "server is at capacity, go away!";
            for (int i = 0; i < UDPSH_ARYSZ(sessions); i++) {
                if (sessions[i].id == UDPSH_SERVER_SES_INV) {
                    sessionid = i + 1;
                    sessions[i].id = sessionid;

                    /* copy inital addr */
                    memcpy(&sessions[i].sock.addr.sin_addr,
                                 &sock_global_client.addr.sin_addr, sizeof(struct in_addr));

                    if (pthread_create(&sessions[i].thread, NULL, session,
                                                         &sessions[i]) != 0) {
                        sessionid = UDPSH_SERVER_SES_INV;
                        sessions[i].id = sessionid;
                        invsesserr = "Unable to create session thread";
                    } else {
                        break;
                    }
                }
            }

            snprintf(sock_global_client.buffer, UDPSH_SOCK_BUFSZ, "%d", sessionid);
            if (ssl_session && usessl) {
                udpsh_sock_ssl_write(&sock_global_client);
            } else {
                udpsh_sock_send(&sock_global_client);
            }

            if (sessionid == UDPSH_SERVER_SES_INV) {
                snprintf(sock_global_client.buffer, UDPSH_SOCK_BUFSZ, "%s", invsesserr);
                udpsh_sock_send(&sock_global_client);
            }
        } else if (strncmp(sock_server.buffer, UDPSH_SERVER_FUN_DIS,
                                             strlen(UDPSH_SERVER_FUN_DIS)) == 0) {
            if (addrcmp(&session_global->sock.addr.sin_addr,
                                    &sock_global_client.addr.sin_addr) != 0 &&
                    session_global->id != UDPSH_SERVER_SES_INV) {
                snprintf(sock_global_client.buffer, UDPSH_SOCK_BUFSZ,
                                 "inconsistent address a=%s != b=%s try reconnecting",
                                 inet_ntoa(session_global->sock.addr.sin_addr),
                                 inet_ntoa(sock_global_client.addr.sin_addr));
                udpsh_sock_send(&sock_global_client);
                continue;
            }
            snprintf(sock_global_client.buffer, UDPSH_SOCK_BUFSZ,
                             "disconnected successfully");
            udpsh_sock_send(&sock_global_client);

            sesinval(session_global);
            //                        memset(session_global, 0, sizeof(struct
            //                        udpsh_server_session));
        } else if (strncmp(sock_server.buffer, UDPSH_SERVER_FUN_EXE,
                                             strlen(UDPSH_SERVER_FUN_EXE)) == 0) {

            if (session_global->id == UDPSH_SERVER_SES_INV) {
                printf("inv\n");
                snprintf(sock_global_client.buffer, UDPSH_SOCK_BUFSZ,
                                 "invalid session. try reconnecting");
                udpsh_sock_send(&sock_global_client);
                continue;
            }

            memcpy(&session_global->global_sock, &sock_global_client,
                         sizeof(struct udpsh_sock));
            memcpy(&session_global->cmdbuf, parse_cmdbuf,
                         sizeof(session_global->cmdbuf));

            /* set ssl sess */
            session_global->global_sock.ssl_enabled = ssl_session;

            seswake(session_global);
            /* test kill session */
            // sesinval(session_global);
        } else if (strncmp(sock_server.buffer, UDPSH_SERVER_FUN_DIE,
                                             strlen(UDPSH_SERVER_FUN_DIE)) == 0) {
            // only server can die (SIGINT)
            char addrstr[32];
            snprintf(addrstr, sizeof(addrstr), "%s",
                             inet_ntoa(sock_global_client.addr.sin_addr));
            if (strncmp(addrstr, "127.0.0.1", 9) == 0) {
                break;
            } else {
                printf("WARNING::%s IS TRYING TO KILL SERVER!\n",
                             inet_ntoa(sock_global_client.addr.sin_addr));
            }
        } else if (strncmp(sock_server.buffer, UDPSH_SERVER_FUN_CRT,
                                             strlen(UDPSH_SERVER_FUN_CRT)) == 0) {
            snprintf(sock_global_client.buffer, sizeof(sock_global_client.buffer),
                             "%lu", certlen);
            udpsh_sock_send(&sock_global_client);
            if (usessl) {
                sendchunked(certmem, certlen, &sock_global_client);
            }
        }
    }
    for (size_t i = 0; i < UDPSH_ARYSZ(sessions); i++) {
        if (sessions[i].id != UDPSH_SERVER_SES_INV)
            sesinval(&sessions[i]);
    }
    udpsh_sock_ssl_terminate(&sock_server);
    printf("clean exit\n");
    return 0;
}

void clientack(struct udpsh_sock *sock) {
    snprintf(sock->buffer, UDPSH_SOCK_BUFSZ, UDPSH_SERVER_FUN_ACK);
    udpsh_sock_send(sock);
}

int addrcmp(struct in_addr *a, struct in_addr *b) {
    return memcmp(a, b, sizeof(struct in_addr));
}

void sendchunked(const void *buf, const size_t buflen,
                                 struct udpsh_sock *sock) {
    size_t consumed = 0;
    size_t chunksz = sizeof(sock->buffer);

    /* chunk can fit in buffer */
    if (buflen < chunksz)
        chunksz = buflen;

    size_t iters = buflen / chunksz;

    /* just one send */
    if (iters == 0)
        iters++;

    for (size_t i = 0; i < iters; i++) {
        memset(sock->buffer, 0, chunksz);
        memcpy(sock->buffer, buf + consumed, chunksz);
        udpsh_sock_send_buf(sock, sock->buffer, chunksz);
        consumed += chunksz;
    }

    chunksz = buflen - consumed;

    /* send remaining */
    if (consumed < buflen) {
        memset(sock->buffer, 0, chunksz);
        memcpy(sock->buffer, buf + consumed, chunksz);
        udpsh_sock_send_buf(sock, sock->buffer, chunksz);
    }
}

void *session(void *arg) {
    struct udpsh_server_session *session_client = arg;
    pthread_mutex_init(&session_client->mut, NULL);
    pthread_cond_init(&session_client->cond, NULL);

    while (session_client->id != UDPSH_SERVER_SES_INV) {
        //                printf("waiting for msg raise client %d\n", session_client->id);
        pthread_mutex_lock(&session_client->mut);
        pthread_cond_wait(&session_client->cond, &session_client->mut);
        /* check if session is still valid after waiting */
        if (addrcmp(&session_client->sock.addr.sin_addr,
                                &session_client->global_sock.addr.sin_addr) != 0 &&
                session_client->id != UDPSH_SERVER_SES_INV) {
            snprintf(session_client->global_sock.buffer, UDPSH_SOCK_BUFSZ,
                             "inconsistent address a=%s != b=%s try reconnecting",
                             inet_ntoa(session_client->sock.addr.sin_addr),
                             inet_ntoa(session_client->global_sock.addr.sin_addr));
        } else if (session_client->id != UDPSH_SERVER_SES_INV) {
            /* split buf into separate strings */
            int max_args = 2;
            char **argv = malloc(max_args * sizeof(char *));
            int argc = 0;
            char *token = strtok(session_client->cmdbuf, " ");
            while (token != NULL) {
                if (argc >= max_args - 1) {
                    max_args *= 2;
                    argv = realloc(argv, max_args * sizeof(char *));
                }
                argv[argc++] = token;
                token = strtok(NULL, " ");
            }
            argv[argc] = NULL;

            int ipc[2];
            if (pipe(ipc) < 0) {
                perror("pipe error");
                snprintf(session_client->global_sock.buffer, UDPSH_SOCK_BUFSZ,
                                 "pipe error");
            } else {
                pid_t ch = fork();
                int status;
                if (ch < 0) {
                    perror("fork error");
                    snprintf(session_client->global_sock.buffer, UDPSH_SOCK_BUFSZ,
                                     "fork error");
                } else if (ch == 0) {
                    /* child */
                    if (dup2(ipc[1], STDOUT_FILENO) < 0 ||
                            dup2(ipc[1], STDERR_FILENO) < 0) {
                        perror("dup2 error");
                        write(ipc[1], "dup2 error", 7);
                    } else {
                        if (execvp(argv[0], argv) < 0) {
                            char execerrmsg[256];
                            snprintf(execerrmsg, sizeof(execerrmsg),
                                             "server execvp error: %s", strerror(errno));
                            write(ipc[1], execerrmsg, strlen(execerrmsg));
                        }
                    }
                    exit(1);
                } else {
                    /* parent */
                    //                                        printf("waiting for %d... ", ch);
                    fflush(stdout);
                    waitpid(ch, &status, 0);
                    //                                        printf("done. status=%d\n", status);
                    memset(session_client->global_sock.buffer, 0,
                                 sizeof(session_client->global_sock.buffer));
                    read(ipc[0], session_client->global_sock.buffer,
                             sizeof(session_client->global_sock.buffer));
                    //                                        printf("%s\n",
                    //                                        session_client->global_sock.buffer);
                    close(ipc[0]);
                    close(ipc[1]);
                    free(argv);
                }
            }
        }

        if (pktloss) {
            int strat = 0;
            size_t bfsz = strlen(session_client->global_sock.buffer);
            size_t times = (pktloss / 100.0) * bfsz;
            for (size_t i = 0; i < times; i++) {
                size_t at = rand() % bfsz;
                if (strat == 0) {
                    /* flip a bit */
                    session_client->global_sock.buffer[at - 1] ^= 0xff;
                } else if (strat == 1) {
                    /* replace byte with random character */
                    session_client->global_sock.buffer[at - 1] = rand() % 255;
                } else if (strat == 2) {
                    /* replace with asterisk */
                    session_client->global_sock.buffer[at - 1] = '*';
                } else {
                    strat = 0;
                }

                strat++;
            }
        }

        if (session_client->id != UDPSH_SERVER_SES_INV) {
            if (session_client->global_sock.ssl_enabled)
                udpsh_sock_ssl_write(&session_client->global_sock);
            else
                udpsh_sock_send(&session_client->global_sock);
        }

        pthread_mutex_unlock(&session_client->mut);
    }
    //        printf("id %lu is done\n", session_client->thread);
    pthread_mutex_destroy(&session_client->mut);
    pthread_cond_destroy(&session_client->cond);

    return NULL;
}

void seswake(struct udpsh_server_session *ses) {
    //        printf("waking session %d\n", ses->id);
    pthread_mutex_lock(&ses->mut);
    pthread_cond_signal(&ses->cond);
    pthread_mutex_unlock(&ses->mut);
}

void sesjoin(struct udpsh_server_session *ses) {
    //        printf("joining session %lu\n", ses->thread);
    pthread_join(ses->thread, NULL);
}

void sesinval(struct udpsh_server_session *ses) {
    printf("invalidating session %d\n", ses->id);
    ses->id = UDPSH_SERVER_SES_INV;
    seswake(ses);
    sesjoin(ses);
    memset(ses, 0, sizeof(struct udpsh_server_session));
}

void sigexits(int sig) {
    snprintf(sock_server.buffer, UDPSH_SOCK_BUFSZ, "%s", UDPSH_SERVER_FUN_DIE);
    udpsh_sock_send(&sock_server);
}
