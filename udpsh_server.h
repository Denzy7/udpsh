#ifndef SERVER_H
#define SERVER_H
#include <pthread.h>
#include "udpsh_sock.h"

/* server functions */
#define UDPSH_SERVER_FUN_ACK "ack"
#define UDPSH_SERVER_FUN_CON "con"
#define UDPSH_SERVER_FUN_EXE "exe"
#define UDPSH_SERVER_FUN_DIS "dis"

/* invalid session i.e. when not connected */
#define UDPSH_SERVER_SES_INV 0

/* server token msg tokenizer */
#define UDPSH_SERVER_TOK ";"

struct udpsh_server_session
{
    pthread_t thread;
    pthread_cond_t cond;
    pthread_mutex_t mut;
    int id;
    struct udpsh_sock sock;
    struct udpsh_sock global_sock;
    char cmdbuf[UDPSH_SOCK_BUFSZ / 2];
};

#endif /*SERVER_H*/
