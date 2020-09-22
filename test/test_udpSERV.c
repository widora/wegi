/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <sys/types.h> /* this header file is not required on Linux */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> /* inet_ntoa() */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int static UDP_SERV_process(int sockfd, struct sockaddr *addrCLIT);


/*-------------------------------------------------------------
struct sockaddr {
	sa_family_t sa_family;
	char        sa_data[14];
}

struct sockaddr_in {
	short int 		sin_family;
	unsigend short it	sin_port;
	struct in_addr		sin_addr;
	unsigned char		sin_zero[8];
}

struct in_addr {
	union {
		struct { u_char s_b1,s_b2,s_b3,s_b4; } S_un_b;
		struct { u_short s_w1,s_w2;} S_un_w;
		u_long S_addr;
	} S_un;
	#define s_addr S_un.S_addr

}

1.

-------------------------------------------------------------*/
int main(void)
{
	int ret=0;
	int sockfd;
	struct sockaddr_in addr_SERV;
	struct sockaddr_in addr_CLIT;
	int port_SERV=8765;
	struct timeval timeout={3,0}; /* Time out */

	/* Create socket fd */
	sockfd=socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd<0) {
		printf("%s: Fail to create datagram socket, '%s'\n", __func__, strerror(errno));
		return -1;
	}

	/* Init. sockaddr */
	memset(&addr_SERV,0,sizeof(addr_SERV));
	addr_SERV.sin_family=AF_INET;
	addr_SERV.sin_addr.s_addr=inet_addr("192.168.8.1"); // htonl(INADDR_ANY);
	addr_SERV.sin_port=htons(port_SERV);

        /* Set timeout */
        if( setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) <0 ) {
                printf("%s: Fail to setsockopt for snd_timeout, Err'%s'\n", __func__, strerror(errno));
                ret=-2;
                goto END_FUNC;
        }
        if( setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) <0 ) {
                printf("%s: Fail to setsockopt for recv_timeout, Err'%s'\n", __func__, strerror(errno));
                ret=-2;
                goto END_FUNC;
        }

	/* Bind socket with sockaddr(Assign sockaddr to socekt) */
	if( bind(sockfd,(struct sockaddr *)&addr_SERV, sizeof(addr_SERV)) < 0) {
		printf("%s: Fail to bind sockaddr, Err'%s'\n", __func__, strerror(errno));
		ret=-3;
		goto END_FUNC;
	}



	/* --- TEST --- */
	printf("Create UDP server at '%s:%d'\n", inet_ntoa(addr_SERV.sin_addr), ntohs(addr_SERV.sin_port));

	/* UDP Servering Process */
	if( UDP_SERV_process(sockfd, (struct sockaddr *)&addr_CLIT) <0 ) {
		ret=-4;
		goto END_FUNC;
	}


END_FUNC:
        /* Close sockfd */
        if(close(sockfd)<0) {
                printf("%s: Fail to close datagram socket, '%s'\n", __func__, strerror(errno));
                ret=-5;
        }

	return ret;
}

/*----------------------------------------------------------------
UDP Server process handler.

@sockfd: 	Server sockfd
@addrCLIT:

1. The client will control session speed by usleep().
----------------------------------------------------------------*/
int static UDP_SERV_process(int sockfd, struct sockaddr *addrCLIT)
{
	int nrecv,nsend;
	socklen_t clen;
	char buff[1024];
	unsigned int count;
	char *pt=NULL;

	if( sockfd<0 || addrCLIT==NULL )
		return -1;

	/* Loop receiving and processing */
	while(1) {
		nrecv=recvfrom(sockfd, buff, sizeof(buff), MSG_CMSG_CLOEXEC, addrCLIT, &clen); /* MSG_DONTWAIT, MSG_ERRQUEUE */
		/* Datagram sockets in various domains permit zero-length datagrams */
		if(nrecv<0) {
			printf("%s: Fail to call recvfrom(), Err'%s'\n", __func__, strerror(errno));
			if( errno!=EAGAIN && errno!=EWOULDBLOCK )
				return -2;
			/* EAGAIN||EWOULDBLOCK:  go on... */
                        /* EAGAIN||EWOULDBLOCK:  go on... */
                        else if(errno==EAGAIN)          printf("%s: errno==EAGAIN.\n",__func__);
                        else if(errno==EWOULDBLOCK)     printf("%s: errno==EWOULDBLOCK.\n",__func__);
                        /***
                         * 1. The socket is marked nonblocking and the receive operation would block,
                         * 2. or a receive timeout had been set and the timeout expired before data was received.
                         */

			/* NOTE: too fast calling recvfrom() will interfere other UDP sessions...? */
			usleep(500000);
			continue;
		}
                else if(nrecv==0) {
                        usleep(100000);
                        continue;
                }
		else if(nrecv>0) {
			buff[nrecv]=0; /* EOF for string */
			printf("Receive from client '%s:%d' with data: '%s'\n",
				inet_ntoa( ((struct sockaddr_in *)addrCLIT)->sin_addr ), ntohs(((struct sockaddr_in *)addrCLIT)->sin_port), buff);

			/* Let client control session gap */
			//usleep(500000);
		}

		/* --- Responde to client --- */
                /* Prepare data */
		pt=strstr(buff,"=");
		if(pt!=NULL) {
			count=strtoul(pt+1,NULL,10);
	                sprintf(buff,"Receive count=%u", count);
		}
		else
	                sprintf(buff,"Receive count=?");
		printf("buff:%s\n",buff);

                /* Send data to server */
                nsend=sendto(sockfd, buff, strlen(buff), 0, addrCLIT, clen); /* flags: MSG_CONFIRM, MSG_DONTWAI */
                if(nsend<0){
                        printf("%s: Fail to sendto() data to the server, Err'%s'\n", __func__, strerror(errno));
                        return -3;
                }
                else if(nsend!=strlen(buff)) {
                        printf("%s: WARNING! Send %d of total %d bytes!\n", __func__, nsend, strlen(buff) );
                }
                /*  MSG_DONTWAIT (since Linux 2.2)
                 *  Enables  nonblocking  operation; if the operation would block, EAGAIN or EWOULDBLOCK is returned.
                 */

	}

	return 0;
}
