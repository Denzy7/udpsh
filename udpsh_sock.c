#include "udpsh_sock.h"

#ifndef _WIN32
#include <sys/socket.h> /* socket */
#include <arpa/inet.h> /* inet_aton */
#include <netdb.h> /* getaddrinfo */
#include <unistd.h> /* close */
#endif

#ifdef _WIN32
#define closesock(sock) closesocket(sock)
#else
#define closesock(sock) close(sock)
#endif

#include <stdio.h>
#include <string.h>
int udpsh_sock_make(const char* ipv4dest, struct udpsh_sock* udpsh_sock)
{
    struct addrinfo hints, *res;
    int status;
    struct sockaddr_in* ip4;
    const char* ipstr = "0.0.0.0";

    if((udpsh_sock->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("cannot create socket");
        return 1;
    }
    ipstr = ipv4dest == NULL ? ipstr : ipv4dest;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if ((status = getaddrinfo(ipstr, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        closesock(udpsh_sock->sock);
        return 1;
    }
    ip4 = (struct sockaddr_in*)res->ai_addr;
    memcpy(&udpsh_sock->addr.sin_addr,
           &ip4->sin_addr,
           sizeof(struct in_addr));
    freeaddrinfo(res);

    udpsh_sock->addr.sin_family = AF_INET;
    udpsh_sock->addr.sin_port = htons(UDPSH_SOCK_PORT);

    /*printf("created socket at %s:%hu successfully\n",
           inet_ntoa(udpsh->addr.sin_addr),
           ntohs(udpsh->addr.sin_port));*/

    return 0;
}

int udpsh_sock_close(const struct udpsh_sock* udpsh_sock)
{
    if(closesock(udpsh_sock->sock) < 0)
    {
        perror("udpsh_sock_close failed");
        return 1;
    }
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
    if(sendto(to->sock, to->buffer, UDPSH_SOCK_BUFSZ, MSG_OOB,
           (const struct sockaddr*)&to->addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("udpsh_sock_send failed");
        return -1;
    }
    return 0;
}
