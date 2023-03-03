#include "udpsh_sock.h"
#include <string.h> /* memset */
#include <stdio.h>
int main(int argc, char *argv[])
{
    struct udpsh_sock sock_server;
    struct udpsh_sock sock_client;

    memset(&sock_server, 0, sizeof(sock_server));
    udpsh_sock_make(NULL, &sock_server);
    udpsh_sock_bind(&sock_server);
    while(1)
    {
        printf("waiting for msg\n");
        sock_client.addrlen = sizeof(struct sockaddr_in);
        sock_client.sock = sock_server.sock;
        udpsh_sock_recv(&sock_server, &sock_client.addr, &sock_client.addrlen);
        printf("%s\n", sock_server.buffer);
        printf("sending ack\n");
        snprintf(sock_client.buffer, UDPSH_SOCK_BUFSZ, "ACK");
        udpsh_sock_send(&sock_client);
    }


    return 0;
}
