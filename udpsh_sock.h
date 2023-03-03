#ifndef UDPSH_SOCK_H
#define UDPSH_SOCK_H

#include <netinet/in.h> /* sockaddr_in, htons */
#define UDPSH_SOCK_PORT 0xf00d
#define UDPSH_SOCK_BUFSZ 1024
struct udpsh_sock
{
    int sock;
    struct sockaddr_in addr;
    char buffer[UDPSH_SOCK_BUFSZ];
};

int udpsh_sock_make(const char* ip, struct udpsh_sock* udpsh_sock);

#endif // UDPSH_SOCK_H
