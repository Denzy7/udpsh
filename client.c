#include "udpsh_sock.h"
#include <stdio.h>
#include <string.h>

int main()
{
    struct udpsh_sock sock_server;
    memset(&sock_server, 0, sizeof(sock_server));
    char inputbuf[UDPSH_SOCK_BUFSZ];
    int running = 1;
    udpsh_sock_make("127.0.0.1", &sock_server);

    while(running)
    {
        printf("enter msg\n");
        fgets(inputbuf, UDPSH_SOCK_BUFSZ, stdin);
        strncpy(sock_server.buffer, inputbuf, UDPSH_SOCK_BUFSZ);
        udpsh_sock_send(&sock_server);
        printf("waiting for ack\n");
        udpsh_sock_recv(&sock_server, NULL, NULL);
        printf("ack received\n");
    }

    return 0;
}
