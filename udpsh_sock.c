#include "udpsh_sock.h"

#include "udpsh_config.h"
#ifdef USE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

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

struct udpsh_sock_ssl
{
    #ifdef USE_SSL
    SSL* hnd;
    SSL_CTX* ctx;
    #endif
};


#include <stdio.h>
#include <stdlib.h>
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

int _udpsh_sock_ssl_setup(struct udpsh_sock_ssl* ssl, const int isserver)
{
#ifdef USE_SSL
    const SSL_METHOD* method = DTLS_server_method();
    if(!isserver)
        method = DTLS_client_method();
    ssl->ctx = SSL_CTX_new(method);
    if(ssl->ctx == NULL)
    {
        ERR_print_errors_fp(stderr);
        return 1;
    }
    ssl->hnd = SSL_new(ssl->ctx);
    if(ssl->hnd == NULL)
    {
        ERR_print_errors_fp(stderr);
        return 1;
    }
#endif

    return 0;
}

int udpsh_sock_ssl_init(struct udpsh_sock* udpsh_sock, int isserver)
{
#ifdef USE_SSL
    struct udpsh_sock_ssl ssl;
    memset(&ssl, 0, sizeof(ssl));
    if(_udpsh_sock_ssl_setup(&ssl, isserver) != 0)
    {
        return 1;
    }
    udpsh_sock->ssl = malloc(sizeof(struct udpsh_sock_ssl));
    memcpy(udpsh_sock->ssl, &ssl, sizeof(struct udpsh_sock_ssl));

    return 0;
#else
    printf("recompile with -DUSE_SSL to enable ssl\n");
    return 1;
#endif
}

void udpsh_sock_ssl_terminate(struct udpsh_sock* udpsh_sock)
{
    #ifdef USE_SSL
    if(udpsh_sock->ssl)
    {
        SSL_shutdown(udpsh_sock->ssl->hnd);
        SSL_free(udpsh_sock->ssl->hnd);
        SSL_CTX_free(udpsh_sock->ssl->ctx);
        free(udpsh_sock->ssl);
    }
    #endif
}

int udpsh_sock_ssl_server(struct udpsh_sock* udpsh_sock, const char* certfile, const char* keyfile)
{
    if(udpsh_sock->ssl == NULL)
    {
        printf("udpsh_sock_ssl_init had not succeeded\n");
        return 1;
    }
#ifdef USE_SSL
    if(SSL_use_certificate_chain_file(udpsh_sock->ssl->hnd, certfile) <= 0)
    {
        ERR_print_errors_fp(stderr);
        return 1;
    }

    if(SSL_use_PrivateKey_file(udpsh_sock->ssl->hnd, keyfile, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        return 1;
    }
#endif
    return 0;

}
int udpsh_sock_ssl_client(struct udpsh_sock* udpsh_sock, const char* certfile)
{
    if(udpsh_sock->ssl == NULL)
    {
        printf("udpsh_sock_ssl_init had not succeeded\n");
        return 1;
    }

#ifdef USE_SSL
    /* need cert */
    SSL_CTX_set_verify(udpsh_sock->ssl->ctx, SSL_VERIFY_PEER, NULL);

    if(SSL_CTX_load_verify_locations(udpsh_sock->ssl->ctx, certfile, NULL) == 0)
    {
        ERR_print_errors_fp(stderr);
        return 1;
    }
#endif
    return 0;

}

int udpsh_sock_ssl_connect(struct udpsh_sock* udpsh_sock)
{
#ifdef USE_SSL
    if(connect(udpsh_sock->sock, (const struct sockaddr*)&udpsh_sock->addr, sizeof(udpsh_sock->addr)) != 0)
    {
        perror("cannot connect to socket");
        return 1;
    }

    if(SSL_set_fd(udpsh_sock->ssl->hnd, udpsh_sock->sock) == 0)
    {
        perror("cannot set ssl fd\n");
        return 1;
    }

    int ret = SSL_connect(udpsh_sock->ssl->hnd);
    if(ret <= 0)
    {
        fprintf(stderr, "ssl connect error: %d\n", SSL_get_error(udpsh_sock->ssl->hnd, ret));
        ERR_print_errors_fp(stderr);
        return 1;
    }
#endif
    return 0;

}

int udpsh_sock_ssl_accept(struct udpsh_sock* udpsh_sock, const struct sockaddr_in* addr, socklen_t addrlen)
{
#ifdef USE_SSL
    if(connect(udpsh_sock->sock, (const struct sockaddr*)addr, addrlen) != 0)
    {
        perror("cannot connect to socket");
        return 1;
    }

    if(SSL_set_fd(udpsh_sock->ssl->hnd, udpsh_sock->sock) == 0)
    {
        perror("cannot set ssl fd\n");
        return 1;
    }

    int ret = SSL_accept(udpsh_sock->ssl->hnd);
    if(ret <= 0)
    {
        fprintf(stderr, "ssl accept error: %d\n", SSL_get_error(udpsh_sock->ssl->hnd, ret));
        ERR_print_errors_fp(stderr);
        return 1;
    }
    #endif
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

int udpsh_sock_ssl_read(struct udpsh_sock* udpsh_sock)
{
    int ret = 0;
#ifdef USE_SSL
    ret = SSL_read(udpsh_sock->ssl->hnd, udpsh_sock->buffer, UDPSH_SOCK_BUFSZ);
#endif
    return ret;
}

int udpsh_sock_ssl_write(struct udpsh_sock* udpsh_sock)
{
    int ret = 0;
#ifdef USE_SSL
    size_t len = UDPSH_SOCK_BUFSZ, strln;
    strln = strlen(udpsh_sock->buffer);

    /* send string length + \0 */
    if(strln < len && strln > 0)
        len = strln + 1;

    ret = SSL_write(udpsh_sock->ssl->hnd, udpsh_sock->buffer, len);
#endif
    return ret;
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
    if(recvfrom(to->sock, to->buffer, UDPSH_SOCK_BUFSZ, 0,
             (struct sockaddr*)srcinfo, srcinfo == NULL ? NULL : srcaddrlen) == -1)
    {
        perror("udpsh_sock_recv failed");
        return -1;
    }
    return 0;
}

int udpsh_sock_send(const struct udpsh_sock* to)
{
    size_t len = UDPSH_SOCK_BUFSZ, strln;
    strln = strlen(to->buffer);

    /* send string length + \0 */
    if(strln < len && strln > 0)
        len = strln + 1;

    if(sendto(to->sock, to->buffer, len, 0,
           (const struct sockaddr*)&to->addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("udpsh_sock_send failed");
        return -1;
    }
    return 0;
}
