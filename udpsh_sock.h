#ifndef UDPSH_SOCK_H
#define UDPSH_SOCK_H

/* sockaddr_in, socklen_t */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

#include <stddef.h> /* NULL, size */
#define UDPSH_SOCK_PORT 0x3333
#define UDPSH_SOCK_PORT_S UDPSH_SOCK_PORT + 1
#define UDPSH_SOCK_BUFSZ 1024

struct udpsh_sock_ssl;

struct udpsh_sock
{
#ifdef _WIN32
    SOCKET sock;
#else
    int sock;
#endif
    struct sockaddr_in addr;
    socklen_t addrlen;
    char buffer[UDPSH_SOCK_BUFSZ];
    struct udpsh_sock_ssl* ssl;
    int ssl_enabled;
};

int udpsh_sock_make(const char* ipv4dest, struct udpsh_sock* udpsh_sock);
int udpsh_sock_ssl_server(struct udpsh_sock* udpsh_sock, const char* certfile, const char* keyfile);
int udpsh_sock_ssl_accept(struct udpsh_sock* udpsh_sock, const struct sockaddr_in* addr, socklen_t addrlen);

int udpsh_sock_ssl_client(struct udpsh_sock* udpsh_sock, const char* certfile);
int udpsh_sock_ssl_connect(struct udpsh_sock* udpsh_sock);

int udpsh_sock_ssl_read(struct udpsh_sock* udpsh_sock);
int udpsh_sock_ssl_write(struct udpsh_sock* udpsh_sock);

int udpsh_sock_close(const struct udpsh_sock* udpsh_sock);
int udpsh_sock_bind(const struct udpsh_sock* udpsh_sock);
int udpsh_sock_recv(struct udpsh_sock* to, struct sockaddr_in* srcinfo, socklen_t* srcaddrlen);
int udpsh_sock_send(const struct udpsh_sock* to);

#endif /* UDPSH_SOCK_H */
