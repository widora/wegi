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
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <egi_inet.h>


int main(void)
{
        /* Create UDP server */
        EGI_UDP_CLIT *uclit=inet_create_udpClient("192.168.8.1", 8765, 4);
        if(uclit==NULL)
                exit(1);

        /* Set callback */
        uclit->callback=inet_udpClient_TESTcallback;

        /* Process UDP server routine */
        inet_udpClient_routine(uclit);

        /* Free and destroy */
        inet_destroy_udpClient(&uclit);

        return 0;
}




#if 0 ///////////////////////////////////////////////////////////////////////

int static UDP_CLIENT_process(int sockfd, struct sockaddr *addrSERV);

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

	/* Init. server sockaddr */
	memset(&addr_SERV,0,sizeof(addr_SERV));
	addr_SERV.sin_family=AF_INET;
	addr_SERV.sin_addr.s_addr=inet_addr("192.168.8.1"); //htonl(INADDR_ANY);
	addr_SERV.sin_port=htons(port_SERV);

	/* NOTE: You may bind CLIENT to a given port, mostly not necessary. */
#if 0
	memset(&addr_CLIT,0,sizeof(addr_CLIT));  /* Init sin_port=0, to let kernel decide */
	addr_CLIT.sin_family=AF_INET;
	addr_CLIT.sin_addr.s_addr=htonl(INADDR_ANY);
	addr_CLIT.sin_port=0;			/* Let kernel decide */
        if( bind(sockfd,(struct sockaddr *)&addr_CLIT, sizeof(addr_CLIT)) < 0) {
                printf("%s: Fail to bind CLIENT sockaddr, Err'%s'\n", __func__, strerror(errno));
                ret=-2;
                goto END_FUNC;
        }
#endif

	/* Set timeout */
	if( setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) <0 ) {
                printf("%s: Fail to setsockopt for snd_timeout, Err'%s'\n", __func__, strerror(errno));
		ret=-3;
		goto END_FUNC;
	}
	if( setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) <0 ) {
                printf("%s: Fail to setsockopt for recv_timeout, Err'%s'\n", __func__, strerror(errno));
		ret=-3;
		goto END_FUNC;
	}
       	printf("Create a UDP client for server of '%s:%d'\n", inet_ntoa(addr_SERV.sin_addr), ntohs(addr_SERV.sin_port));

#if 1   /* --- TEST: getsockname() --- */
	socklen_t clen=sizeof(addr_CLIT);
	/* NOTE: getsockname will be effective only after sendto() operation, UDP will fail to get IP address, only PORT is correct */
	sendto(sockfd, NULL, 0, 0, NULL,0); /* flags: MSG_CONFIRM, MSG_DONTWAI */
	if( getsockname(sockfd, (struct sockaddr *)&addr_CLIT, &clen)!=0 ) {
                printf("%s: Fail to getsockname for client, Err'%s'\n", __func__, strerror(errno));
	}
	else {
		printf("Return client addr len=%d, input addr len=%d\n", clen, sizeof(addr_CLIT));
         	printf("Create a UDP client at PORT '%d' for server at '%s:%d'\n", ntohs(addr_CLIT.sin_port),
								inet_ntoa(addr_SERV.sin_addr), ntohs(addr_SERV.sin_port));
	}
#endif

	/* UDP Client Process */
	if( UDP_CLIENT_process(sockfd, (struct sockaddr *)&addr_SERV) <0 ) {
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


/*-----------------------------------------------------------------

@sockfd:        client sockfd
@addrSERV:	Server address

1. The client will control session speed by usleep().

------------------------------------------------------------------*/
int static UDP_CLIENT_process(int sockfd, struct sockaddr *addrSERV)
{
	int nrecv, nsend;
	socklen_t slen;
	socklen_t rlen;
	char buff[EGI_MAX_UDP_PDATA_SIZE];
	int  psize=EGI_MAX_UDP_PDATA_SIZE;	/* EGI_MAX_UDP_PDATA_SIZE=1024 Bytes, UDP packet payload size */
	struct sockaddr_in addrRECV; /* Received addr */
	unsigned int count=0;

	if( sockfd<0 || addrSERV==NULL)
		return -1;

	/* Init addrRECVE */
	memset(&addrRECV,0,sizeof(addrRECV));

	/* Get sock addr length */
	slen=sizeof(*addrSERV);

	/* Loop receiving and processing */
	while(1) {

		/* Prepare data */
		memset(buff,0,psize);
		sprintf(buff,"Hello from client, count=%d", ++count); /* start from 1 */
		printf("Send count=%d ...\n", count);

		/* Send data to server */
		//nsend=sendto(sockfd, buff, strlen(buff), 0, addrSERV, slen); /* flags: MSG_CONFIRM, MSG_DONTWAI */
		nsend=sendto(sockfd, buff, psize, 0, addrSERV, slen); /* flags: MSG_CONFIRM, MSG_DONTWAI */
		if(nsend<0){
			printf("%s: Fail to sendto() data to the server, Err'%s'\n", __func__, strerror(errno));
			return -2;
		}
		else if(nsend!=psize) {
			printf("%s: WARNING! Send %d of total %d bytes!\n", __func__, nsend, psize);
		}
		/*  MSG_DONTWAIT (since Linux 2.2)
                 *  Enables  nonblocking  operation; if the operation would block, EAGAIN or EWOULDBLOCK is returned.
		 */

		/* Receive data from server */
		nrecv=recvfrom(sockfd, buff, sizeof(buff), MSG_CMSG_CLOEXEC, (struct sockaddr *)&addrRECV, &rlen); /* MSG_ERRQUEUE */
		/* Datagram sockets in various domains permit zero-length datagrams */
		if(nrecv<0) {
			printf("%s: Fail to recvfrom() from the server, Err'%s'\n", __func__, strerror(errno));
			if( errno!=EAGAIN && errno!=EWOULDBLOCK )
				return -2;
			/* EAGAIN||EWOULDBLOCK:  go on... */
			else if(errno==EAGAIN) 		printf("%s: errno==EAGAIN.\n",__func__);
			else if(errno==EWOULDBLOCK) 	printf("%s: errno==EWOULDBLOCK.\n",__func__);
			/***
			 * 1. The socket is marked nonblocking and the receive operation would block,
			 * 2. or a receive timeout had been set and the timeout expired before data was received.
			 */

			/* NOTE: too fast calling recvfrom() will interfere other UDP sessions...? */
			usleep(500000);
			continue;
		}
		else if(nrecv==0) {
			usleep(200000);
			continue;
		}
		else if(nrecv>0) {
			buff[sizeof(buff)-1]=0; //buff[nrecv]=0; /* set EOF for string */
                        printf("%d bytes from server '%s:%d' with data: '%s'\n",
                                nrecv, inet_ntoa(((struct sockaddr_in)addrRECV).sin_addr), ntohs(((struct sockaddr_in)addrRECV).sin_port), buff);
		}

		/* Loop Session gap */
		usleep(5000); //500000);
	}

	return 0;
}


#endif ///////////////////////////////////////////////////////////////////////////////
