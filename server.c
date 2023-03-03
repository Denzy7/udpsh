#include "udpsh_sock.h"
#include <stddef.h> /* NULL */
int main(int argc, char *argv[])
{
    struct udpsh_sock sh;
    udpsh_sock_make(NULL, &sh);

    return 0;
}
