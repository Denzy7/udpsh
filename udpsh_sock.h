#ifndef UDPSH_SOCK_H
#define UDPSH_SOCK_H

#include <netinet/in.h> /* sockaddr_in, htons */
#include <stddef.h> /* NULL, size */
#define UDPSH_SOCK_PORT 0x3333
#define UDPSH_SOCK_BUFSZ 1024
struct udpsh_sock
{
    int sock;
    struct sockaddr_in addr;
    socklen_t addrlen;
    char buffer[UDPSH_SOCK_BUFSZ];
};

int udpsh_sock_make(const char* ipv4dest, struct udpsh_sock* udpsh_sock);
int udpsh_sock_bind(const struct udpsh_sock* udpsh_sock);
int udpsh_sock_recv(struct udpsh_sock* to, struct sockaddr_in* srcinfo, socklen_t* srcaddrlen);
int udpsh_sock_send(const struct udpsh_sock* to);

#endif // UDPSH_SOCK_H
