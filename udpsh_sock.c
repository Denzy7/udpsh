#include "udpsh_sock.h"

#include <sys/socket.h> /* socket */
#include <arpa/inet.h> /* inet_aton */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int udpsh_sock_make(const char* ip, struct udpsh_sock* udpsh_sock)
{
    if((udpsh_sock->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("cannot create socket");
        return 1;
    }
    udpsh_sock->addr.sin_family = AF_INET;
    if(ip != NULL)
    {
        if(inet_aton(ip, &udpsh_sock->addr.sin_addr) == 0)
        {
            printf("cannot parse input address\n");
            return 1;
        }
    }else
    {
        udpsh_sock->addr.sin_addr.s_addr = INADDR_ANY;
    }
    udpsh_sock->addr.sin_port = htons(UDPSH_SOCK_PORT);

    if(bind(udpsh_sock->sock, (const struct sockaddr*)&udpsh_sock->addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("cannot bind socket to address");
        return 1;
    }

//    printf("created socket at %s:%hu successfully\n",
//           inet_ntoa(udpsh->addr.sin_addr),
//           ntohs(udpsh->addr.sin_port));

    return 0;
}
