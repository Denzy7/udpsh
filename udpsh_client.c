#include <stdio.h>
#include <string.h>

/* inet_ntoa */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ws2tcpip.h>
#define snprintf sprintf_s
#else
#include <arpa/inet.h>
#endif

#include "udpsh_sock.h"
#include "udpsh_server.h"

#define STR_CON ".connect"
#define STR_SSL ".ssl"
#define STR_QUIT ".quit"
#define STR_HELP ".help"
#define STR_DISCON ".disconnect"

void disconn();
void shellhelp();
void serverack();

int sessionid = UDPSH_SERVER_SES_INV;
struct udpsh_sock sock_server;
int sock_server_ssl;
int usessl = 0;

int main()
{
    // we want half the size to avoid snprintf truncation warnings
    char inputbuf[UDPSH_SOCK_BUFSZ / 2];
    int running = 1;
    size_t inputbuf_strlen, i;
    const char* conaddr;
#ifdef _WIN32
   WORD wsa_ver;
   WSADATA wsa_data;
#endif

#ifdef _WIN32
   wsa_ver = MAKEWORD(2, 2);
   if(WSAStartup(wsa_ver, &wsa_data) != 0)
   {
       fprintf(stderr, "WSAStartup failed: err=%d\n", WSAGetLastError());
       return 1;
   }

   if(HIBYTE(wsa_data.wVersion) != 2 || LOBYTE(wsa_data.wVersion) != 2)
   {
       fprintf(stderr, "WinSock2 unavailable: err=%d\n", WSAGetLastError());
       WSACleanup();
       return 1;
   }
#endif

    memset(&sock_server, 0, sizeof(sock_server));
    printf("welcome to udpsh! type " STR_HELP " for help\n");
    while(running)
    {
        if(sessionid == UDPSH_SERVER_SES_INV)
            printf("\n> ");
        else
            printf("\n$ ");

        fgets(inputbuf, sizeof(inputbuf), stdin);

        /* remove blank and newline */
        inputbuf_strlen = strlen(inputbuf);
        if(inputbuf[inputbuf_strlen - 1] == ' ')
            inputbuf[inputbuf_strlen - 1] = 0;

        for(i = 0; i < inputbuf_strlen; i++)
        {
            if(inputbuf[i] == '\n'){
                inputbuf[i] = 0;
                break;
            }
        }

        if(strncmp(inputbuf,STR_QUIT, strlen(STR_QUIT)) == 0)
        {
            disconn();
            sessionid = UDPSH_SERVER_SES_INV;
            running = 0;
        }else if(strncmp(inputbuf,STR_HELP, strlen(STR_HELP)) == 0)
        {
            shellhelp();
        }else if(strncmp(inputbuf,STR_DISCON, strlen(STR_DISCON)) == 0)
        {
            if(sessionid == UDPSH_SERVER_SES_INV)
            {
                printf("not connected to server!\n");
                continue;
            }else
            {
                disconn();
            }
        }else if(strncmp(inputbuf,STR_CON, strlen(STR_CON)) == 0)
        {
            if(sessionid != UDPSH_SERVER_SES_INV)
            {
                printf("already connected to %s\n",
                       inet_ntoa(sock_server.addr.sin_addr));
                continue;
            }

            conaddr = inputbuf + strlen(STR_CON) + 1; /* +1 space */
            if(udpsh_sock_make(conaddr, &sock_server) != 0)
            {
                printf("cannot create socket to server at %s\n", conaddr);
                continue;
            }
            snprintf(sock_server.buffer, UDPSH_SOCK_BUFSZ,
                "%s", UDPSH_SERVER_FUN_CON);
            udpsh_sock_send(&sock_server);
            serverack();
            printf("waiting for server to reply with sessionid... ");
            fflush(stdout);
            /* server should reply with our sessionid here */
            udpsh_sock_recv(&sock_server, NULL, NULL);
            sscanf(sock_server.buffer, "%d", &sessionid);
            if(sessionid == UDPSH_SERVER_SES_INV)
            {
                udpsh_sock_recv(&sock_server, NULL, NULL);
                printf("unable to connect. server says it's because: %s\n",
                       sock_server.buffer);
            }else
            {
                printf("connected to %s using sessionid %d!\n",
                       inet_ntoa(sock_server.addr.sin_addr), sessionid);
            }
        }else if(strncmp(inputbuf,STR_SSL, strlen(STR_SSL)) == 0)
        {
            if(strlen(inputbuf) == strlen(STR_SSL))
            {
                usessl = !usessl;
            }else
            {
                const char* cert = inputbuf + strlen(STR_SSL) + 1;
                if(udpsh_sock_ssl_client(&sock_server, cert) == 1)
                {
                    fprintf(stderr, "ssl init failed\n");
                }else
                {
                    printf("loaded %s successfully\n", cert);
                    usessl = 1;
                }
            }
            printf("usessl=%d\n", usessl);
        }else
        {
            if(sessionid == UDPSH_SERVER_SES_INV)
            {
                printf("unknown udpsh command. type " STR_HELP " for help\n");
            }else
            {
                if(usessl)
                {
                    /* ssl negotitaion */
                    snprintf(sock_server.buffer, UDPSH_SOCK_BUFSZ,
                            "%s",
                            UDPSH_SERVER_FUN_SSL);
                    udpsh_sock_send(&sock_server);
                    serverack();

                    /* server's ssl status */
                    udpsh_sock_recv(&sock_server, NULL, NULL);
                    sscanf(sock_server.buffer, "%d", &sock_server_ssl);
                    if(sock_server_ssl == 0)
                    {
                        printf("server does not support ssl. consider disabling it (insecure)\n");
                    }else
                    {
                        udpsh_sock_ssl_connect(&sock_server);
                    }
                }

                snprintf(sock_server.buffer, UDPSH_SOCK_BUFSZ,
                         "%s%s%d%s%s",
                         UDPSH_SERVER_FUN_EXE, UDPSH_SERVER_TOK,
                         sessionid, UDPSH_SERVER_TOK,
                         inputbuf);
                if(usessl && sock_server_ssl){
                    udpsh_sock_ssl_write(&sock_server);
                }
                else{
                    udpsh_sock_send(&sock_server);
                    serverack();
                }

                /* server should reply with command output */
                printf("waiting for output from server... ");
                fflush(stdout);
                if(usessl && sock_server_ssl){
                    udpsh_sock_ssl_read(&sock_server);
                }
                else{
                    udpsh_sock_recv(&sock_server, NULL, NULL);
                }
                printf("done\n");
                printf("\n%s", sock_server.buffer);
            }
        }
    }
#ifdef _WIN32
    WSACleanup();
#endif
    printf("\nHAVE A NICE DAY!\n\n");

    return 0;
}

void serverack()
{
    printf("waiting for server ack... ");
    fflush(stdout);
    udpsh_sock_recv(&sock_server, NULL, NULL);
    if(strncmp(sock_server.buffer, UDPSH_SERVER_FUN_ACK, strlen(UDPSH_SERVER_FUN_ACK)) == 0)
    {
        printf("done\n");
    }
}

void shellhelp()
{
    printf(STR_CON" [ipv4-address]: connect to server\n"
           STR_SSL" [cert]: toggle ssl if no cert is provided. off by default\n"
           STR_DISCON": disconnect current server\n"
           STR_HELP": this message\n"
           STR_QUIT": you would not beleive it!\n");
}

void disconn()
{
    if(sessionid == UDPSH_SERVER_SES_INV)
        return;

    printf("disconnecting...\n");

    snprintf(sock_server.buffer, UDPSH_SOCK_BUFSZ,
             "%s%s%d",
             UDPSH_SERVER_FUN_DIS, UDPSH_SERVER_TOK,
             sessionid);
    udpsh_sock_send(&sock_server);
    serverack();
    sessionid = UDPSH_SERVER_SES_INV;

    /* wait for disconnection message */
    printf("waiting for disconnection message from server... ");
    fflush(stdout);
    udpsh_sock_recv(&sock_server, NULL, NULL);
    printf("done\n");
    printf("%s", sock_server.buffer);

    udpsh_sock_close(&sock_server);
}
