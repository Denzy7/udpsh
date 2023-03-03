#include "udpsh_sock.h"

#include <sys/socket.h> /* socket */
#include <arpa/inet.h> /* inet_aton */

#include <stdio.h>
int udpsh_sock_make(const char* ipv4dest, struct udpsh_sock* udpsh_sock)
{
    if((udpsh_sock->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("cannot create socket");
        return 1;
    }
    udpsh_sock->addr.sin_family = AF_INET;
    if(ipv4dest == NULL)
    {
        udpsh_sock->addr.sin_addr.s_addr = INADDR_ANY;
    }else
    {
        if(inet_aton(ipv4dest, &udpsh_sock->addr.sin_addr) == 0)
        {
            printf("cannot parse input address\n");
            return 1;
        }
    }
    udpsh_sock->addr.sin_port = htons(UDPSH_SOCK_PORT);

    /*printf("created socket at %s:%hu successfully\n",
           inet_ntoa(udpsh->addr.sin_addr),
           ntohs(udpsh->addr.sin_port));*/

    return 0;
}

int udpsh_sock_bind(const struct udpsh_sock* udpsh_sock)
{
    if(bind(udpsh_sock->sock, (const struct sockaddr*)&udpsh_sock->addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("cannot bind socket to address");
        return 1;
    }
    return 0;
}

int udpsh_sock_recv(struct udpsh_sock* to, struct sockaddr_in* srcinfo, socklen_t* srcaddrlen)
{
    if(recvfrom(to->sock, to->buffer, UDPSH_SOCK_BUFSZ, MSG_WAITALL,
             (struct sockaddr*)srcinfo, srcinfo == NULL ? NULL : srcaddrlen) == -1)
    {
        perror("udpsh_sock_recv failed");
        return -1;
    }
    return 0;
}

int udpsh_sock_send(const struct udpsh_sock* to)
{
    if(sendto(to->sock, to->buffer, UDPSH_SOCK_BUFSZ, MSG_CONFIRM,
           (const struct sockaddr*)&to->addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("udpsh_sock_send failed");
        return -1;
    }
    return 0;
}
