/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

1. Structs for address:
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

2. sendmsg() v.s. sendto()
   ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                  const struct sockaddr *dest_addr, socklen_t addrlen);

   ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
   struct msghdr {
               void         *msg_name;       // optional addrese
               socklen_t     msg_namelen;    // size of address
               struct iovec *msg_iov;        // scatter/gather array
               size_t        msg_iovlen;     // # elements in msg_iov
               void         *msg_control;    // ancillary data, see below
               size_t        msg_controllen; // ancillary data buffer len
               int           msg_flags;      // flags (unused)
   };


Midas Zhou
midaszhou@yahoo.com
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
#include <egi_inet.h>


/*-----------------------------------------------------
Create an UDP server.

@strIP:	UDP IP address.
	If NULL, auto select by htonl(INADDR_ANY).
@Port:  Port Number.
	If NULL, auto select by system.
@domain:  If ==6, IPv6, otherwise IPv4.
	TODO: IPv6 connection.
Return:
	Pointer to an EGI_UPD_SERV	OK
	NULL				Fails
-------------------------------------------------------*/
EGI_UDP_SERV* inet_create_udpServer(const char *strIP, unsigned short port, int domain)
{
	struct timeval timeout={10,0}; /* Default time out */
	EGI_UDP_SERV *userv=NULL;

	/* Calloc EGI_UDP_SERV */
	userv=calloc(1,sizeof(EGI_UDP_SERV));
	if(userv==NULL)
		return NULL;

	/*--------------------------------------------------
       	AF_INET             IPv4 Internet protocols
       	AF_INET6            IPv6 Internet protocols
       	AF_IPX              IPX - Novell protocols
       	AF_NETLINK          Kernel user interface device
       	AF_X25              ITU-T X.25 / ISO-8208 protocol
       	AF_AX25             Amateur radio AX.25 protocol
       	AF_ATMPVC           Access to raw ATM PVCs
       	AF_APPLETALK        AppleTalk
       	AF_PACKET           Low level packet interface
       	AF_ALG              Interface to kernel crypto API
	---------------------------------------------------*/
	if(domain==6)
		domain=AF_INET6;
	else
		domain=AF_INET;

	/* Create UDP socket fd */
	userv->sockfd=socket(domain, SOCK_DGRAM|SOCK_CLOEXEC, 0);
	if(userv->sockfd<0) {
		printf("%s: Fail to create datagram socket, '%s'\n", __func__, strerror(errno));
		free(userv);
		return NULL;
	}

	/* 2. Init. sockaddr */
	userv->addrSERV.sin_family=domain;
	if(strIP==NULL)
		userv->addrSERV.sin_addr.s_addr=htonl(INADDR_ANY);
	else
		userv->addrSERV.sin_addr.s_addr=inet_addr(strIP);
	userv->addrSERV.sin_port=htons(port);

	/* 3. Bind socket with sockaddr(Assign sockaddr to socekt) */
	if( bind(userv->sockfd,(struct sockaddr *)&(userv->addrSERV), sizeof(userv->addrSERV)) < 0) {
		printf("%s: Fail to bind sockaddr, Err'%s'\n", __func__, strerror(errno));
		free(userv);
		return NULL;
	}

        /* 4. Set default SND/RCV timeout */
        if( setsockopt(userv->sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) <0 ) {
                printf("%s: Fail to setsockopt for snd_timeout, Err'%s'\n", __func__, strerror(errno));
		free(userv);
		return NULL;
        }
        if( setsockopt(userv->sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) <0 ) {
                printf("%s: Fail to setsockopt for recv_timeout, Err'%s'\n", __func__, strerror(errno));
		free(userv);
		return NULL;
        }

	printf("%s: An EGI UDP server created at %s:%d.\n", __func__, inet_ntoa(userv->addrSERV.sin_addr), ntohs(userv->addrSERV.sin_port));

	/* OK */
	return userv;
}


/* ------------------------------------------
Close an UDP SERV and release resoruces.

Return:
	0	OK
	<0	Fails
--------------------------------------------*/
int inet_destroy_udpServer(EGI_UDP_SERV **userv)
{
	if(userv==NULL || *userv==NULL)
		return 0;

        /* Close sockfd */
        if(close((*userv)->sockfd)<0) {
                printf("%s: Fail to close datagram socket, Err'%s'\n", __func__, strerror(errno));
		return -1;
	}

	/* Free mem */
	free(*userv);
	*userv=NULL;

	return 0;
}


/*---------------------------------------------------------
UDP service routines:  Receive data from client and pass
it to the caller AND get data from the caller and send it
to client.

NOTE:
1. For a UDP server, the routine process always start with
   recvfrom().

!!!TODO.
1. NOW: one server <--> one client
2. IPv6 connection.

@userv: Pointer to an EGI_UDP_SERV.

Return:
	0	OK
	<0	Fails
---------------------------------------------------------*/
int inet_udpServer_routine(EGI_UDP_SERV *userv)
{
	struct sockaddr_in rcvCLIT;   /* Client for recvfrom() */
	struct sockaddr_in sndCLIT;   /* Client for sendto() */
	int nrcv,nsnd;
	socklen_t clen;
	char rcvbuff[EGI_MAX_UDP_PDATA_SIZE]; /* EtherNet packet payload MAX. 46-MTU(1500) bytes,  UPD packet payload MAX. 2^16-1-8-20=65507 */
	char sndbuff[EGI_MAX_UDP_PDATA_SIZE];
	int  sndsize;	    /* size of to_send_data */

	/* Check input */
	if( userv==NULL )
		return -1;
	if( userv->callback==NULL ) {
		printf("%s: Callback is NOT defined!\n", __func__);
		return -2;
	}

	/* Loop service processing: TODO poll method  */
	printf("%s: Start UDP service loop processing...\n", __func__);
	while(1) {
		/* Block recv */
                /*  MSG_DONTWAIT (since Linux 2.2)
                 *  Enables  nonblocking  operation; if the operation would block, EAGAIN or EWOULDBLOCK is returned.
                 */
		clen=sizeof(rcvCLIT); /*  Before the call, it should be initialized to adddrSENDR */
		nrcv=recvfrom(userv->sockfd, rcvbuff, sizeof(rcvbuff), MSG_CMSG_CLOEXEC, &rcvCLIT, &clen); /* MSG_DONTWAIT, MSG_ERRQUEUE */
		/* TODO: What if clen > sizeof( struct sockaddr_in ) ! */
		if( clen > sizeof(rcvCLIT) )
			printf("%s: clen > sizeof(rcvCLIT)!\n",__func__);
		/* Datagram sockets in various domains permit zero-length datagrams */
		if(nrcv<0) {
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
			usleep(5000);
			continue;
		}
                else if(nrcv==0) {
                        usleep(10000);
                        continue;
                }
		else if(nrcv>0) {
		        #if 0
			printf("Receive from client '%s:%d' with data: '%s'\n",
				inet_ntoa(rcvCLIT.sin_addr), ntohs(rcvCLIT.sin_port), rcvbuff);
			#endif

		    	/* Process callback: Pass received data and get next send data */
			if(userv->callback) {

				/***  CALLBACK ( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                                 *                     struct sockaddr_in *sndAddr,       char *sndBuff, int *sndSize);
				 *   Pass out: rcvCLIT, rcvbuff, rcvsize
				 *   Take in:  sndCLIT, sndbuff, sndsize
				 */
				sndsize=0; /* reset first */
				userv->callback(&rcvCLIT, rcvbuff, nrcv,  &sndCLIT, sndbuff, &sndsize);

		                /* Send data to server */
				clen=sizeof(sndCLIT); /* Always SAME here!*/
				if( sndsize>0 )
	        		        nsnd=sendto(userv->sockfd, sndbuff, sndsize, 0, &sndCLIT, clen); /* flags: MSG_CONFIRM, MSG_DONTWAI */
        	        		if(nsnd<0){
                	        		printf("%s: Fail to sendto() data to client '%s:%d', Err'%s'\n",
								__func__, inet_ntoa(sndCLIT.sin_addr), ntohs(sndCLIT.sin_port), strerror(errno));
		                	        //GO ON... return -3;
        		        	}
	        	        	else if(nsnd!=sndsize) {
        	        	        	printf("%s: WARNING! Send %d of total %d bytes to '%s:%d'!\n",
								__func__, nsnd, sndsize, inet_ntoa(sndCLIT.sin_addr), ntohs(sndCLIT.sin_port) );
	                	}
		    	}

		    	/* END one round routine.  Let client control session gap */
		    	//usleep(500000);
		}
	}

	return 0;
}


/*-----------------------------------------------------------------
A TEST callback routine for UPD server.
1. Tasks in callback function should be simple and short, normally
just to pass out received data to the Caller, and take in data to
the UDP server.
2. The counter is for one client scenario!

@rcvAddr:       Client address, from which rcvData was received.
@rcvData:       Data received from rcvAddr
@rcvSize:       Data size of rcvData.

@sndAddr:       Client address, to which sndBuff will be sent.
@sndBuff:       Data to send to sndAddr
@sndSize:       Data size of sndBuff

Return:
	0	OK
	<0	Fails
------------------------------------------------------------------*/
int inet_udpServer_TESTcallback( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                                       struct sockaddr_in *sndAddr, 	  char *sndBuff, int *sndSize)
{
	char buff[EGI_MAX_UDP_PDATA_SIZE];
	int  psize=EGI_MAX_UDP_PDATA_SIZE; /* EGI_MAX_UDP_PDATA_SIZE=1024;  UDP packet payload size, in bytes. */
	char *pt=NULL;
	unsigned int clitcount;	      /* Counter in Client message */
	static unsigned int count=0;  /* Counter for received packets, start from 1 */
	static struct sockaddr_in tmpAddr={0};

	/* Counts */
	count++;

	/* Clear data buff */
	memset(buff,0,psize);

	/* Check whether sender's addres changes! */
	if(rcvAddr->sin_port != tmpAddr.sin_port || rcvAddr->sin_addr.s_addr != tmpAddr.sin_addr.s_addr ) {
		count=1;  		/* reset count, start from 1 */
		tmpAddr=*rcvAddr;
	}

	/* ---- 1. Take data from UDP server ---- */
	/* Get data */
	memcpy(buff, rcvData, rcvSize);

	/*  Process/Prepare send data */
	buff[sizeof(buff)-1]=0; /* set EOF */
        pt=strstr(buff,"=");
        if(pt!=NULL) {
                  clitcount=strtoul(pt+1,NULL,10);
                  //sprintf(buff,"Received count=%u", clitcount);
		  sprintf(buff,"clitcount=%u, svrcount=%u   --- drops=%d ---", clitcount, count, clitcount-count);
        }
        else
                 sprintf(buff,"Received count=?");

	/* ---- 2. Pass data to UDP server ---- */
	/* 1k packet payload */
	*sndSize=psize;
	memcpy(sndBuff, buff, psize);
	*sndAddr=*rcvAddr;  /* Send to the same client */

	return 0;
}

/*-----------------------------------------------------
Create an UDP client.

@strIP:	UDP IP address.
	If NULL, auto select by htonl(INADDR_ANY).
@Port:  Port Number.
	If NULL, auto select by system.
@domain:  If ==6, IPv6, otherwise IPv4.
	TODO: IPv6 connection.
Return:
	Pointer to an EGI_UPD_SERV	OK
	NULL				Fails
-------------------------------------------------------*/
EGI_UDP_CLIT* inet_create_udpClient(const char *servIP, unsigned short servPort, int domain)
{
	struct timeval timeout={10,0}; /* Default time out */
        socklen_t clen;
	EGI_UDP_CLIT *uclit=NULL;

	/* Check input */
	if(servIP==NULL)
		return NULL;

	/* Calloc EGI_UDP_SERV */
	uclit=calloc(1,sizeof(EGI_UDP_CLIT));
	if(uclit==NULL)
		return NULL;

	/* Set domain */
	if(domain==6)
		domain=AF_INET6;
	else
		domain=AF_INET;

	/* Create UDP socket fd */
	uclit->sockfd=socket(domain, SOCK_DGRAM|SOCK_CLOEXEC, 0);
	if(uclit->sockfd<0) {
		printf("%s: Fail to create datagram socket, '%s'\n", __func__, strerror(errno));
		free(uclit);
		return NULL;
	}

	/* 2. Init. sockaddr */
	uclit->addrSERV.sin_family=domain;
	uclit->addrSERV.sin_addr.s_addr=inet_addr(servIP);
	uclit->addrSERV.sin_port=htons(servPort);

	/* NOTE: You may bind CLIENT to a given port, mostly not necessary. */

        /* 3. Set default SND/RCV timeout */
        if( setsockopt(uclit->sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) <0 ) {
                printf("%s: Fail to setsockopt for snd_timeout, Err'%s'\n", __func__, strerror(errno));
		free(uclit);
		return NULL;
        }
        if( setsockopt(uclit->sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) <0 ) {
                printf("%s: Fail to setsockopt for recv_timeout, Err'%s'\n", __func__, strerror(errno));
		free(uclit);
		return NULL;
        }

	/* 4. Probe to getsockname() */
        clen=sizeof(typeof(uclit->addrME));
        /* NOTE: getsockname() will be effective only after sendto() operation, UDP will fail to get IP address if PORT is NOT correct */
        sendto(uclit->sockfd, NULL, 0, 0, NULL,0); /* Only let kernel to assign a port number, ignore errors. */
        if( getsockname(uclit->sockfd, (struct sockaddr *)&(uclit->addrME), &clen)!=0 ) {
                printf("%s: Fail to getsockname for client, Err'%s'\n", __func__, strerror(errno));
        }
	printf("%s: An EGI UDP client created at %s:%d, targeting to the server at %s:%d.\n",
			 __func__, inet_ntoa(uclit->addrME.sin_addr), ntohs(uclit->addrME.sin_port),
				   inet_ntoa(uclit->addrSERV.sin_addr), ntohs(uclit->addrSERV.sin_port)	  );

	/* OK */
	return uclit;
}


/* ------------------------------------------
Close an UDP Client and release resoruces.

Return:
	0	OK
	<0	Fails
--------------------------------------------*/
int inet_destroy_udpClient(EGI_UDP_CLIT **uclit)
{
	if(uclit==NULL || *uclit==NULL)
		return 0;

        /* Close sockfd */
        if(close((*uclit)->sockfd)<0) {
                printf("%s: Fail to close datagram socket, Err'%s'\n", __func__, strerror(errno));
		return -1;
	}

	/* Free mem */
	free(*uclit);
	*uclit=NULL;

	return 0;
}

/*------------------------------------------------------------
UDP client routines: Send data to the Server, and wait for
response. then loop routine process: receive data from the
Server and pass it to the Caller, while get data from the Caller
and send it to the Server.

NOTE:
1. For a UDP client, the routine process always start with taking
   in send_data from the Caller and then sendto() the UDP server.

!!!TODO.
1. NOW: one server <--> one client
2. IPv6 connection.

@userv: Pointer to an EGI_UDP_CLIT.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------*/
int inet_udpClient_routine(EGI_UDP_CLIT *uclit)
{
	struct sockaddr_in addrSENDER;   /* Address of the sender(server) from recvfrom(), expected to be same as uclit->addrSERV */
	socklen_t sndlen;
	int nrcv,nsnd;
	socklen_t svrlen;
	char rcvbuff[EGI_MAX_UDP_PDATA_SIZE]; /* EtherNet packet payload MAX. 46-MTU(1500) bytes,  UPD packet payload MAX. 2^16-1-8-20=65507 */
	char sndbuff[EGI_MAX_UDP_PDATA_SIZE];
	int  sndsize;	    /* size of to_send_data */

	/* Check input */
	if( uclit==NULL )
		return -1;
	if( uclit->callback==NULL ) {
		printf("%s: Callback is NOT defined!\n", __func__);
		return -2;
	}

        /* Get sock addr length */
        svrlen=sizeof(typeof(uclit->addrSERV));

	/* Loop service processing: TODO poll method  */
	printf("%s: Start UDP client loop processing...\n", __func__);
	nrcv=0; /* Reset it first */
	while(1) {

		/***  CALLBACK ( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                 *                     				     	  char *sndBuff, int *sndSize);
		 *   Pass out: rcvAddr, rcvbuff, rcvsize
		 *   Take in:   	sndbuff, sndsize
		 */
		sndsize=0; /* Reset it first */
		uclit->callback(&addrSENDER, rcvbuff, nrcv, sndbuff, &sndsize); /* Init. nrcv=0 */

		/* Send data to ther Server */
		if(sndsize>0)
		{
                	nsnd=sendto(uclit->sockfd, sndbuff, sndsize, 0, &uclit->addrSERV, svrlen); /* flags: MSG_CONFIRM, MSG_DONTWAI */
                        if(nsnd<0){
                        	printf("%s: Fail to sendto() data to UDP server '%s:%d', Err'%s'\n",
                                             __func__, inet_ntoa(uclit->addrSERV.sin_addr), ntohs(uclit->addrSERV.sin_port), strerror(errno));
                                            //GO ON... return;
			}
                        else if(nsnd!=sndsize) {
                        	printf("%s: WARNING! Send %d of total %d bytes to '%s:%d'!\n",
                                            __func__, nsnd, sndsize, inet_ntoa(uclit->addrSERV.sin_addr), ntohs(uclit->addrSERV.sin_port) );
			}

		}

		/* Block recvfrom() */
                /*  MSG_DONTWAIT (since Linux 2.2)
                 *  Enables  nonblocking  operation; if the operation would block, EAGAIN or EWOULDBLOCK is returned.
                 */
		sndlen=sizeof(addrSENDER); /*  Before the call, it should be initialized to adddrSENDR */
		nrcv=recvfrom(uclit->sockfd, rcvbuff, sizeof(rcvbuff), MSG_CMSG_CLOEXEC, &addrSENDER, &sndlen); /* MSG_DONTWAIT, MSG_ERRQUEUE */
		/* TODO: What if sndlen > sizeof( addrSENDER ) ! */
		if(sndlen>sizeof(addrSENDER))
			printf("%s: sndlen > sizeof(addrSENDER)!\n",__func__);
		/* Datagram sockets in various domains permit zero-length datagrams */
		if(nrcv<0) {
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
			usleep(5000);
			continue;
		}
                else if(nrcv==0) {
                        usleep(10000);
                        continue;
                }
		/* ELSE: (nrcv>0) */
		#if 0
                printf("%s: %d bytes from server '%s:%d'.\n",
                           __func__, nrcv, inet_ntoa(addrSENDER.sin_addr), ntohs(addrSENDER.sin_port));
		#endif

                /* Loop Session gap */
                usleep(5000); //500000);
	}
}


/*-----------------------------------------------------------------
A TEST callback routine for UPD client.
1. Tasks in callback function should be simple and short, normally
just to pass out received data to the Caller, and take in data to
the UDP client.
2. The counter is for one client scenario!

@rcvAddr:       Server/Sender address, from which rcvData was received.
@rcvData:       Data received from rcvAddr
@rcvSize:       Data size of rcvData.

@sndBuff:       Data to send to UDP server
@sndSize:       Data size of sndBuff

Return:
	0	OK
	<0	Fails
------------------------------------------------------------------*/
int inet_udpClient_TESTcallback( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                                        	  		          char *sndBuff, int *sndSize )
{
	char buff[EGI_MAX_UDP_PDATA_SIZE];
	int  psize=EGI_MAX_UDP_PDATA_SIZE; /* EGI_MAX_UDP_PDATA_SIZE=1024;  UDP packet payload size, in bytes. */
	static unsigned int count=0;  	   /* Counter for sended packets, start from 1 */

	/* ---- 1. Take data from UDP client ---- */
	/* Get data */
	if( rcvSize>0 ) {
		memcpy(buff, rcvData, rcvSize);
		buff[sizeof(buff)-1]=0; /* set EOF as limit */
                printf("%d bytes from server '%s:%d' with data: '%s'\n",
                             	rcvSize, inet_ntoa(rcvAddr->sin_addr), ntohs(rcvAddr->sin_port), buff);
	}

	/* Process/Prepare send data */
        memset(buff,0,psize);
        sprintf(buff,"Hello from client, count=%d", ++count); /* start from 1 */
        printf("Send count=%d ...\n", count);

	/* ---- 2. Pass data to UDP client ---- */
	*sndSize=psize;
	memcpy(sndBuff, buff, psize);

	return 0;
}
