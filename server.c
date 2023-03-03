#include "udpsh_sock.h"
#include <string.h> /* memset */
#include <stdio.h>
#include <arpa/inet.h> /* inet_ntoa */
#include "udpsh_server.h"
#include "udpsh_util.h"

struct udpsh_sock sock_server;
struct udpsh_server_session sessions[4];

void clientack(struct udpsh_sock* sock)
{
    snprintf(sock->buffer, UDPSH_SOCK_BUFSZ,
             UDPSH_SERVER_FUN_ACK);
    udpsh_sock_send(sock);
}

void* session(void* arg)
{
    struct udpsh_server_session* session_client = arg;
    pthread_mutex_init(&session_client->mut, NULL);
    pthread_cond_init(&session_client->cond, NULL);
    while(session_client->id != UDPSH_SERVER_SES_INV)
    {
        printf("waiting for msg raise client %d\n", session_client->id);
        pthread_mutex_lock(&session_client->mut);
        pthread_cond_wait(&session_client->cond, &session_client->mut);
        /* check if session is still valid after waiting */
        if(session_client->id != UDPSH_SERVER_SES_INV)
        {
            snprintf(session_client->global_sock.buffer, UDPSH_SOCK_BUFSZ,
                     "this would be some output");
            printf("cmdbuf: %s\n", session_client->cmdbuf);
            udpsh_sock_send(&session_client->global_sock);
        }
        pthread_mutex_unlock(&session_client->mut);
    }
    printf("id %d is done\n", session_client->id);
    pthread_mutex_destroy(&session_client->mut);
    pthread_cond_destroy(&session_client->cond);

    return NULL;
}

void seswake(struct udpsh_server_session* ses)
{
    printf("waking session %d\n", ses->id);
    pthread_mutex_lock(&ses->mut);
    pthread_cond_signal(&ses->cond);
    pthread_mutex_unlock(&ses->mut);
}

void sesjoin(struct udpsh_server_session* ses)
{
    printf("joining session %d\n", ses->id);
    pthread_join(ses->thread, NULL);
}

void sesinval(struct udpsh_server_session* ses)
{
    ses->id = UDPSH_SERVER_SES_INV;
    printf("invalidating session %d\n", ses->id);
    seswake(ses);
    sesjoin(ses);
    memset(ses, 0, sizeof(struct udpsh_server_session));
}

int main(int argc, char *argv[])
{
    memset(&sessions, 0, sizeof(sessions));
    memset(&sock_server, 0, sizeof(sock_server));

    if(udpsh_sock_make(NULL, &sock_server) != 0)
        return 1;
    if(udpsh_sock_bind(&sock_server) != 0)
        return 1;

    while(1)
    {
        printf("waiting for global msg\n");
        struct udpsh_sock sock_global_client;
        sock_global_client.addrlen = sizeof(struct sockaddr_in);
        sock_global_client.sock = sock_server.sock;
        udpsh_sock_recv(&sock_server, &sock_global_client.addr, &sock_global_client.addrlen);
        clientack(&sock_global_client);

        printf("from %s\nbuffer=%s\n",
               inet_ntoa(sock_global_client.addr.sin_addr),
               sock_server.buffer);

        /* nasty inextensible code here! */
        int parse_sessionid;
        char parse_buf[UDPSH_SOCK_BUFSZ];
        strncpy(parse_buf, sock_server.buffer, UDPSH_SOCK_BUFSZ);
        char parse_cmdbuf[UDPSH_SOCK_BUFSZ / 2];
        char parse_fun[3];
        const char* tok = NULL;
        tok = strtok(parse_buf, UDPSH_SERVER_TOK);
        sscanf(tok, "%s",
               parse_fun);
        tok = strtok(NULL, UDPSH_SERVER_TOK);
        if(tok != NULL)
        {
            sscanf(tok, "%d", &parse_sessionid);
            tok = strtok(NULL, UDPSH_SERVER_TOK);
            if(tok != NULL)
            {
                strncpy(parse_cmdbuf, tok, sizeof(parse_cmdbuf));
            }
        }

        struct udpsh_server_session* session_global = &sessions[parse_sessionid - 1];

        if(strncmp(sock_server.buffer, UDPSH_SERVER_FUN_CON, strlen(UDPSH_SERVER_FUN_CON)) == 0)
        {
            int sessionid = UDPSH_SERVER_SES_INV;
            static const char* invsesserr = "server is at capacity, go away!";
            for(int i = 0; i < UDPSH_ARYSZ(sessions); i++)
            {
                if(sessions[i].thread == 0)
                {
                    sessionid = i + 1;
                    sessions[i].id = sessionid;
                    if(pthread_create(&sessions[i].thread, NULL, session, &sessions[i]) != 0)
                    {
                        sessionid = UDPSH_SERVER_SES_INV;
                        sessions[i].id = sessionid;
                        invsesserr = "Unable to create session thread";
                    }else
                    {
                        break;
                    }
                }
            }

            snprintf(sock_global_client.buffer, UDPSH_SOCK_BUFSZ,
                     "%d", sessionid);
            udpsh_sock_send(&sock_global_client);

            if(sessionid == UDPSH_SERVER_SES_INV)
            {
                snprintf(sock_global_client.buffer, UDPSH_SOCK_BUFSZ,
                         "%s", invsesserr);
                udpsh_sock_send(&sock_global_client);
            }
        }else if(strncmp(sock_server.buffer, UDPSH_SERVER_FUN_DIS, strlen(UDPSH_SERVER_FUN_DIS)) == 0)
        {
            sesinval(session_global);
            clientack(&sock_global_client);
            memset(session_global, 0, sizeof(struct udpsh_server_session));
        }else if(strncmp(sock_server.buffer, UDPSH_SERVER_FUN_EXE, strlen(UDPSH_SERVER_FUN_EXE)) == 0)
        {
            if(session_global->id == UDPSH_SERVER_SES_INV)
            {
                snprintf(sock_global_client.buffer, UDPSH_SOCK_BUFSZ,
                         "invalid session. try reconnecting");
                udpsh_sock_send(&sock_global_client);
                continue;
            }

            memcpy(&session_global->global_sock,
                    &sock_global_client,
                    sizeof(struct udpsh_sock));
            memcpy(&session_global->cmdbuf,
                    parse_cmdbuf,
                    sizeof(session_global->cmdbuf));

            seswake(session_global);
            /* test kill session */
            //sesinval(session_global);
        }
    }

    return 0;
}
