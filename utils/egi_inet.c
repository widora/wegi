/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A UPD/TCP communication module.
NOW: For IPv4 ONLY!

1. Structs for address:
struct sockaddr {
	sa_family_t sa_family;
	char        sa_data[14];
}

	--- IPv4 ---
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

3. A UDP Server is calling Non_blocking recvfrom(), while a UDP Client is calling
   blocking recvfrom(). A UDP Server needs to transmit data as quickly as
   possible, while a UDP Client only receives data at most time, responds to the
   server only occasionaly. You can also ajust TimeOut for blocking type func.
   recvfrom().

4. UDP Tx/Rx speeds depends on many facts and situations.
   It's difficult to always gear up with the Server with a right timing:
   packet size, numbers of clients, backcall() strategy, request conflicts,
   system task schedule, ....

   1 UDP server, 2 UDP Clients, keep Two_Way transmitting. --- Seems Good and stable!  Server Tx/Rx=4.5Mbps

Midas Zhou
midaszhou@yahoo.com
--------------------------------------------------------------------------*/
#include <sys/types.h> /* this header file is not required on Linux */
#include <sys/socket.h>
#include <sys/time.h>
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
	If NULL, auto selected by htonl(INADDR_ANY).
@Port:  Port Number.
	If 0, auto selected by system.
@domain:  If ==6, IPv6, otherwise IPv4.
	TODO: IPv6 connection.
Return:
	Pointer to an EGI_UPD_SERV	OK
	NULL				Fails
-------------------------------------------------------*/
EGI_UDP_SERV* inet_create_udpServer(const char *strIP, unsigned short port, int domain)
{
	struct timeval timeout={10,0}; /* Default time out send/receive */
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
	#if 0
	if(domain==6)
		domain=AF_INET6;
	else
		domain=AF_INET;
	#else
	domain=AF_INET;
	#endif

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


/*--------------------------------------------------------------
UDP service routines:  Receive data from client and pass
it to the caller AND get data from the caller and send it
to client.

	------ One Routine, One caller -------

NOTE:
1. For a UDP server, the routine process always start with
   recvfrom().
2. If sndCLIT is meaningless, then sendto() will NOT execute.
   SO a caller may stop server to sendto() by clearing sndCLIT.

   			!!! WARNING !!!
   The counter peer client should inform routine Caller to clear/reset
   sndCLIT before close the connection! OR the routine will NEVER
   stop sending data, even the peer client is closed.
   UDP is a kind of connectionless link!

3. nrcv<0. means no data from client.

TODO.
1. IPv6 connection.
2. Use select/poll to monitor IO fd.

@userv: Pointer to an EGI_UDP_SERV.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------*/
int inet_udpServer_routine(EGI_UDP_SERV *userv)
{
	struct sockaddr_in rcvCLIT={0};   /* Client for recvfrom() */
	struct sockaddr_in sndCLIT={0};   /* Client for sendto() */
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

	/*** Loop service processing: ***/
	printf("%s: Start UDP service loop processing...\n", __func__);
	while(1) {
		/* ---- UDP Server: NON_Blocking recvfrom() ---- */
                /*  MSG_DONTWAIT (since Linux 2.2)
                 *  Enables  nonblocking  operation; if the operation would block, EAGAIN or EWOULDBLOCK is returned.
                 */
		clen=sizeof(rcvCLIT); /*  Before callING recvfrom(), it should be initialized */
		nrcv=recvfrom(userv->sockfd, rcvbuff, sizeof(rcvbuff), MSG_CMSG_CLOEXEC|MSG_DONTWAIT, &rcvCLIT, &clen); /* MSG_DONTWAIT, MSG_ERRQUEUE */
		/* TODO: What if clen != sizeof( struct sockaddr_in ) ! */
		if( clen != sizeof(rcvCLIT) )
			printf("%s: clen != sizeof(rcvCLIT)!\n",__func__);
		/* Datagram sockets in various domains permit zero-length datagrams */
		if(nrcv<0) {
			//printf("%s: Fail to call recvfrom(), Err'%s'\n", __func__, strerror(errno));

			if( errno != EAGAIN && errno != EWOULDBLOCK )
				return -2;

			#if 0 /* --- TEST --- */
			/* EAGAIN||EWOULDBLOCK:  go on... */
                        /* EAGAIN||EWOULDBLOCK:  go on... */
                        else if(errno==EAGAIN)          printf("%s: errno==EAGAIN.\n",__func__);
                        else if(errno==EWOULDBLOCK)     printf("%s: errno==EWOULDBLOCK.\n",__func__);

            		/***
                         * 1. The socket is marked nonblocking and the receive operation would block,
                         * 2. or a receive timeout had been set and the timeout expired before data was received.
                         */
			#endif
		}
		/*---------------------------
                else if(nrcv==0) {
                        usleep(10000);
                        continue;
                }
		else if(nrcv>0) {
		}
		--------------------------*/

		/* ---NOW---: nrcv>0 || nrcv<0 || nrc==0 */

	        #if 0 /* --- TEST --- */
		printf("Receive from client '%s:%d' with data: '%s'\n",
			inet_ntoa(rcvCLIT.sin_addr), ntohs(rcvCLIT.sin_port), rcvbuff);
		#endif

	    	/* Process callback: Pass received data and get next send data */

		/***  CALLBACK ( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                 *                     struct sockaddr_in *sndAddr,       char *sndBuff, int *sndSize);
		 *   Pass out: rcvCLIT, rcvbuff, rcvsize
		 *   Take in:  sndCLIT, sndbuff, sndsize
		 */
		sndsize=0; /* reset first */
		userv->callback(&rcvCLIT, rcvbuff, nrcv,  &sndCLIT, sndbuff, &sndsize);

                /* Send data to server */
		clen=sizeof(sndCLIT); /* Always SAME here!*/

		/* Sendto: Only If sndCLIT is meaningful and sndaSize >0 */
		if( sndCLIT.sin_addr.s_addr !=0  && sndsize>0 ) {
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

	    	/* END one round routine. sleep if NO DATA received */
		if(nrcv<0)
	    		usleep(5000);
	}

	return 0;
}


/*-----------------------------------------------------------------
A TEST callback routine for UPD server.
1. Tasks in callback function should be simple and short, normally
just to pass out received data to the Caller, and take in data to
the UDP server.
2. In this TESTcallback, the server do the 'count+1' job, and then
   respond the count result to the client! There may be several
   clients, but OK, it always send to the sndAddr=rcvAddr!


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
	int  psize=EGI_MAX_UDP_PDATA_SIZE; /* EGI_MAX_UDP_PDATA_SIZE=1024;  UDP packet payload size, in bytes. */
	char buff[psize];
	char *pt=NULL;
	unsigned int clitcount;	      /* Counter in Client message */
	static struct sockaddr_in tmpAddr={0};

	/* Only if received data */
	if( rcvSize < 0) {
		*sndSize=0;
		return 0;
	}

	/* Check whether sender's addres changes! */
	if(rcvAddr->sin_port != tmpAddr.sin_port || rcvAddr->sin_addr.s_addr != tmpAddr.sin_addr.s_addr ) {
		tmpAddr=*rcvAddr;
	}

	/* ---- 1. Take data from EGI_UDP_SERV ---- */

	/*  Process/Prepare send data */
	/* Get data */
	memcpy(buff, rcvData, rcvSize);

	/* For test purpose only, to set an EOF for string. so we may NOT need to memset(buff,0,psize)  */
	if(psize>1024)
		buff[1024]=0;
	else
		buff[psize-1]=0; /* set EOF */

	/* Get count in client message and respond with clitcount+1. */
        pt=strstr(buff,"=");
        if(pt!=NULL) {
                  clitcount=strtoul(pt+1,NULL,10);
		  //sprintf(buff,"clitcount=%u, svrcount=%u   --- drops=%d ---", clitcount, count, clitcount-count);
                  sprintf(buff,"Respond count=%u", clitcount+1);
        }
        else {
                 //sprintf(buff,"Received count=?");
                  sprintf(buff,"Respond count=%u", clitcount+1);  /* Old clitcount */
	}

	/* ---- 2. Pass data to EGI_UDP_SERV ---- */
	/* 1k packet payload */
	*sndSize=psize;
	memcpy(sndBuff, buff, psize);
	*sndAddr=*rcvAddr;  /* Send to the same client */

	return 0;
}


/*------------------------------------------------------------
Create an UDP client.
And always send out a zero length datagram to probe the server.
as a sign to the server that a new client/session MAY begin.

@strIP:	Server UDP IP address.
@Port:  Server Port Number.
@domain:  If ==6, IPv6, otherwise IPv4.
	TODO: IPv6 connection.
Return:
	Pointer to an EGI_UPD_SERV	OK
	NULL				Fails
-------------------------------------------------------------*/
EGI_UDP_CLIT* inet_create_udpClient(const char *servIP, unsigned short servPort, int domain)
{
	struct timeval timeout={10,0}; /* Default time out */
        socklen_t clen;
	EGI_UDP_CLIT *uclit=NULL;

	/* Check input */
	if(servIP==NULL)
		return NULL;

	/* Calloc EGI_UDP_CLIT */
	uclit=calloc(1,sizeof(EGI_UDP_CLIT));
	if(uclit==NULL)
		return NULL;

	/* Set domain */
	#if 0
	if(domain==6)
		domain=AF_INET6;
	else
		domain=AF_INET;
	#else
	domain=AF_INET;
	#endif

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

	uclit->addrME.sin_family=domain;
	uclit->addrME.sin_addr.s_addr=htonl(INADDR_ANY);
	printf("%s: Initial addrME at '%s:%d'\n", __func__, inet_ntoa(uclit->addrME.sin_addr), ntohs(uclit->addrME.sin_port));

	/* NOTE: You may bind CLIENT to a given port, mostly not necessary. If 0, let kernel decide. */
	if( bind(uclit->sockfd,(struct sockaddr *)&(uclit->addrME), sizeof(uclit->addrME)) < 0)
		printf("%s: Fail to bind ME to sockfd, Err'%s'\n", __func__, strerror(errno));


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

	/* 4. To probe the server */
   //#if 0  /* Send 0 data to probe the Server, Only to trigger to let kernel to assign a port number, ignore errors. */
	int nsnd;
	/* For Datagram, dest_addr and addrlen MUST NOT be NULL and 0 */
        nsnd=sendto(uclit->sockfd, NULL, 0, 0, (struct sockaddr *)&uclit->addrSERV,sizeof( typeof(uclit->addrSERV)));
	printf("%s: Sendto() %d lenght datagram to probe the server!\n",__func__, nsnd);
   //#else
	/*** For UDP is an unconnected link, it always return OK for connect()!!!
	 * After calling connect(), the kernel would save link information and relate it to an IMCP!
	 * Example:
         *   ----If call connect() only:
         *   It returns several ECONNREFUSED errors when call recvfrom(BLOCK), if the counter part server is NOT ready!
	 *   and  EAGAIN/EWOULDBLOCK for timeout.
	 *   ----if call sendto() only :
	 *   It returns only EAGAIN/EWOULDBLOCK for timeout when call recvfrom(BLOCK), if the counter part server is NOT ready!
	 *   man: "Connectionless sockets may dissolve the association by connecting to an address with the  sa_family
       	 *   member of sockaddr set to AF_UNSPEC (supported on Linux since kernel 2.2). "
         */
        if( connect(uclit->sockfd, (struct sockaddr *)&uclit->addrSERV, sizeof( typeof(uclit->addrSERV)) )==0 ) {
		printf("%s: Connect to UDP server at %s:%d successfully!\n",
						__func__, inet_ntoa(uclit->addrSERV.sin_addr), ntohs(uclit->addrSERV.sin_port) );
	} else {
                printf("%s: Fail to connect to the UDP server, Err'%s'\n", __func__, strerror(errno));
	}
    //#endif

	/*** Try to getsockname for ME. TODO: Returned addrME.sin_addr is NOT correct, it appears to truncated addrSERV !!!
         *	NOT for UDP ?
         */
        clen=sizeof(uclit->addrME);
        if( getsockname(uclit->sockfd, (struct sockaddr *)&(uclit->addrME), &clen)!=0 ) {
                printf("%s: Fail to getsockname for client, Err'%s'\n", __func__, strerror(errno));
        }
	else
		printf("%s: An EGI UDP client created at ???%s:%d, targeting to the server at %s:%d.\n",
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

	------ One Routine, One caller -------

NOTE:
1. For a UDP client, the routine process always start with taking
   in send_data from the Caller and then sendto() the UDP server.

!!!TODO.
1. IPv6 connection.
2. Use select/poll to monitor IO fd.

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
	int  sndsize=-1;	    /* size of to_send_data */

	/* Check input */
	if( uclit==NULL )
		return -1;
	if( uclit->callback==NULL ) {
		printf("%s: Callback is NOT defined!\n", __func__);
		return -2;
	}

        /* Get sock addr length */
        svrlen=sizeof(typeof(uclit->addrSERV));

	/*** Loop service processing ***/
	printf("%s: Start UDP client loop processing...\n", __func__);
	nrcv=0; /* Reset it first */
	while(1) {

		/***  CALLBACK ( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                 *                     				     	  char *sndBuff, int *sndSize);
		 *   Pass out: rcvAddr, rcvbuff, rcvsize
		 *   Take in:   	sndbuff, sndsize
		 */
		sndsize=-1; /* Reset it first */
		uclit->callback(&addrSENDER, rcvbuff, nrcv, sndbuff, &sndsize); /* Init. nrcv=0 */

		/* Send data to ther Server */
		if(sndsize>-1) /* permit zero-length datagrams */
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

		/* ---- UDP Client: Blocking recvfrom() ---- */
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
			switch(errno) {
				case EAGAIN:  /* Note: EAGAIN == EWOULDBLOCK */
					printf("%s: errno==EAGAIN or EWOULDBLOCK.\n",__func__);
					break;
				case ECONNREFUSED: /* Usually the counter part is NOT ready */
					printf("%s: errno==ECONNREFUSED.\n",__func__);
				     	break;
				default:
					return -2;
			}

                        /***
                         * 1. The socket is marked nonblocking and the receive operation would block,
                         * 2. or a receive timeout had been set and the timeout expired before data was received.
                         */

			/* NOTE: too fast calling recvfrom() will interfere other UDP sessions...? */
			//BLOCKED: usleep(5000);
			continue;
		}

		/* Datagram sockets in various domains permit zero-length datagrams. */
                //else if(nrcv==0) {
                //        usleep(10000);
                //        continue;
                //}

		/* ELSE: (nrcv>=0) */
		#if 0
                printf("%s: %d bytes from server '%s:%d'.\n",
                           __func__, nrcv, inet_ntoa(addrSENDER.sin_addr), ntohs(addrSENDER.sin_port));
		#endif

                /* Loop Session gap */
                usleep(5000); //500000);
	}
}


/*-------------------------------------------------------------------
A TEST callback routine for UPD client.
1. Tasks in callback function should be simple and short, normally
   just to pass out received data to the Caller, and take in data to
   the UDP client to send out.
2. In this TESTcallback, the client just bounce backe received count
   number, and let the server do 'count++' job. If received count
   number is NOT as expected svrcount=count+1, then a packet must have
   been dropped/missed.

@rcvAddr:       Server/Sender address, from which rcvData was received.
@rcvData:       Data received from rcvAddr
@rcvSize:       Data size of rcvData.

@sndBuff:       Data to send to UDP server
@sndSize:       Data size of sndBuff

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------*/
int inet_udpClient_TESTcallback( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                                        	  		          char *sndBuff, int *sndSize )
{
	int  psize=EGI_MAX_UDP_PDATA_SIZE; /* EGI_MAX_UDP_PDATA_SIZE=1024;  UDP packet payload size, in bytes. */
	char buff[psize];
	static unsigned int count=0;  	   /* Counter for sended packets, start from 0, the  server do the 'count+1' job! */
	unsigned int svrcount=0;	   /* count from the server */
	char *pt=NULL;
	int miss=0;			   /* number of droped or delayed packets */

	/* Only received data, Init. rcvSize=0  */
	if( rcvSize<0 ) {
		return 0;
	}

	/* ---- 1. Take data from EGI_UDP_CLIT ---- */
	/* Get data */
	if( rcvSize>0 ) {
		memcpy(buff, rcvData, rcvSize);  /* Server set EOF */
                printf("%d bytes from server '%s:%d' with data: '%s'\n",
                             	rcvSize, inet_ntoa(rcvAddr->sin_addr), ntohs(rcvAddr->sin_port), buff);

		/* Extract count from the server, the server do the 'count+1' job! */
        	pt=strstr(buff,"=");
        	if(pt!=NULL) {
                  	svrcount=strtoul(pt+1,NULL,10);

			/* Check drops */
			if(svrcount != count+1)
				miss++;

			/* Bounce back to server, who will do the 'count+1' job. */
			count=svrcount;

			printf("Received svrcount=%d, --- miss:%d ---\n", svrcount,miss);
		}
		/* ELSE: re_send count */
	}

	/* Process/Prepare send data to EGI_UDP_CLIT */
        //Set EOF instead, memset(buff,0,psize);
        /* For test purpose only, to set an EOF for string. so we may NOT need to memset(buff,0,psize). */
        if(psize>1024)
                buff[1024]=0;
        else
                buff[psize-1]=0; /* set EOF */

        sprintf(buff,"Hello from client, count=%d. --- miss:%d ---", count, miss); /* start from 0 */

	/* ---- 2. Pass data to UDP client ---- */
	*sndSize=psize;
	memcpy(sndBuff, buff, psize);

	return 0;
}


/*-----------------------------------------------------
Create a TCP server.

@strIP:	TCP IP address.
	If NULL, auto selected by htonl(INADDR_ANY).
@Port:  Port Number.
	If 0, auto selected by system.
@domain:  If ==6, IPv6, otherwise IPv4.
	TODO: IPv6 connection.
Return:
	Pointer to an EGI_TCP_SERV	OK
	NULL				Fails
-------------------------------------------------------*/
EGI_TCP_SERV* inet_create_tcpServer(const char *strIP, unsigned short port, int domain)
{
	struct timeval timeout={10,0}; /* Default time out for send/receive */
	EGI_TCP_SERV *userv=NULL;

	/* Calloc EGI_TCP_SERV */
	userv=calloc(1,sizeof(EGI_TCP_SERV));
	if(userv==NULL)
		return NULL;

	/* Default backlog */
	userv->backlog=MAX_TCP_BACKLOG;

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
	#if 0
	if(domain==6)
		domain=AF_INET6;
	else
		domain=AF_INET;
	#else
	domain=AF_INET;
	#endif

	/* Create TCP socket fd */
	userv->sockfd=socket(domain, SOCK_STREAM|SOCK_CLOEXEC, 0);
	if(userv->sockfd<0) {
		printf("%s: Fail to create stream socket, '%s'\n", __func__, strerror(errno));
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
	printf("%s: An TCP server created at %s:%d.\n", __func__, inet_ntoa(userv->addrSERV.sin_addr), ntohs(userv->addrSERV.sin_port));

	/* 5. If port==0, Get system allocated port number */
	if(userv->addrSERV.sin_port == 0) {
	        socklen_t clen=sizeof(userv->addrSERV);
        	if( getsockname(userv->sockfd, (struct sockaddr *)&(userv->addrSERV), &clen)!=0 ) {
                	printf("%s: Fail to getsockname, Err'%s'\n", __func__, strerror(errno));
	        }
		else
			printf("%s: system auto. allocates port number: %d\n", __func__, ntohs(userv->addrSERV.sin_port));
	}

	/* OK */
	return userv;
}


/* ------------------------------------------
Close a TCP SERV and release resoruces.

Return:
	0	OK
	<0	Fails
--------------------------------------------*/
int inet_destroy_tcpServer(EGI_TCP_SERV **userv)
{
	if(userv==NULL || *userv==NULL)
		return 0;

	/* Make sure all sessions/clients have been safely closed/disconnected! */

        /* Close main sockfd */
        if(close((*userv)->sockfd)<0) {
                printf("%s: Fail to close datagram socket, Err'%s'\n", __func__, strerror(errno));
		return -1;
	}

	/* Free mem */
	free(*userv);
	*userv=NULL;

	return 0;
}


/*--------------------------------------------------------------
TCP service routines:
1. Accept new clients and put to session list, then start a thread
   to precess C/S session.

NOTE:

TODO.
1. IPv6 connection.
2. Use select/poll to monitor IO fd.

@userv: Pointer to an EGI_TCP_SERV.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------*/
int inet_tcpServer_routine(EGI_TCP_SERV *userv)
{
	int csfd;			   /* fd for c/s */
	struct sockaddr_in addrCLIT={0};   /* Client for recvfrom() */
	socklen_t addrLen;

	/* Check input */
	if( userv==NULL )
		return -1;
	if( userv->session_handler==NULL ) {
		printf("%s: session_handler is NOT defined!\n", __func__);
		return -2;
	}

	/* Start to listen */
	if( listen(userv->sockfd, userv->backlog)<0 ) {
		printf("%s: Fail to listen() sockfd, Err'%s'\n", __func__, strerror(errno));
		return -3;
	}

	/*** Loop service processing: ***/
	printf("%s: Start TCP service loop processing...\n", __func__);
	while(1) {

		/* Max. clients */
		if(userv->ccnt==EGI_MAX_TCP_CLIENTS) {
			printf("%s: Clients number hits Max. limit of %d!\n",__func__, EGI_MAX_TCP_CLIENTS);
			usleep(10000);
			continue;
		}

		/* Accept clients, BLOCKING method */
		addrLen=sizeof(struct sockaddr);
		csfd=accept(userv->sockfd, (struct sockaddr *)&addrCLIT, &addrLen);
		if(csfd<0) {
			switch(errno) {
				case EAGAIN: //EWOULDBLOCK
					break;
				default:
					printf("%s: Fail to accept a new client, errno=%d Err'%s'.  continue...\n",
												__func__, errno, strerror(errno));
					break;
			}
			continue;
		}

		/* New client */
		userv->sessions[userv->ccnt].csFD=csfd;
		userv->sessions[userv->ccnt].addrCLIT=addrCLIT;

		/* Start a thread to process C/S ssesion */
		if( pthread_create( &userv->sessions[userv->ccnt].csthread, NULL, userv->session_handler,
									(void *)(userv->sessions+userv->ccnt)) <0 ) {
			printf("%s: Fail to start C/S session thread!\n", __func__);
		}
		else {
			userv->sessions[userv->ccnt].alive=true;
		}

		/* Counter clinets */
		userv->ccnt++;

		printf("%s: Accept a new TCP client from %s:%d, totally %d clients now.\n",
				__func__, inet_ntoa(addrCLIT.sin_addr), ntohs(addrCLIT.sin_port), userv->ccnt );
	}

	return 0;
}


/*-----------------------------------------------------
	A default TCP server session handler.
-----------------------------------------------------*/
void* TEST_tcpServer_session_handler(void *arg)
{
	EGI_TCP_SESSION *session=(EGI_TCP_SESSION *)arg;
	if(session==NULL)
		return (void *)-1;

	struct sockaddr_in *addrCLIT=&session->addrCLIT;
	int sfd=session->csFD;
	struct timeval tm_stamp;
	char buff[128];
	int nsnd;

   while(1) {
	gettimeofday(&tm_stamp, NULL);
	sprintf(buff, "TCP session handler: Reply to client '%s:%d' at %ld.%ld",
				inet_ntoa(addrCLIT->sin_addr), ntohs(addrCLIT->sin_port), tm_stamp.tv_sec, tm_stamp.tv_usec);
	nsnd=write(sfd, buff, strlen(buff)+1);
	sleep(3);
   }

	return (void *)0;
}


/*-----------------------------------------------------
Create a TCP client.

@strIP:	Server TCP IP address.
@Port:  Server Port Number.
@domain:  If ==6, IPv6, otherwise IPv4.
	TODO: IPv6 connection.
Return:
	Pointer to an EGI_TCP_CLIT	OK
	NULL				Fails
-------------------------------------------------------*/
EGI_TCP_CLIT* inet_create_tcpClient(const char *servIP, unsigned short servPort, int domain)
{
	struct timeval timeout={10,0}; /* Default time out for send/receive */
	EGI_TCP_CLIT *uclit=NULL;

	/* Check input */
	if(servIP==NULL)
		return NULL;

	/* Calloc EGI_TCP_CLIT */
	uclit=calloc(1,sizeof(EGI_TCP_CLIT));
	if(uclit==NULL)
		return NULL;

	/* Set domain */
	#if 0
	if(domain==6)
		domain=AF_INET6;
	else
		domain=AF_INET;
	#else
	domain=AF_INET;
	#endif

	/* 1. Create TCP socket fd */
	uclit->sockfd=socket(domain, SOCK_STREAM|SOCK_CLOEXEC, 0);
	if(uclit->sockfd<0) {
		printf("%s: Fail to create stream socket, '%s'\n", __func__, strerror(errno));
		free(uclit);
		return NULL;
	}

	/* 2. Init. sockaddr */
	uclit->addrSERV.sin_family=domain;
	uclit->addrSERV.sin_addr.s_addr=inet_addr(servIP);
	uclit->addrSERV.sin_port=htons(servPort);

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

	/* 4. Connect to the server */
	if( connect(uclit->sockfd, (struct sockaddr *)&uclit->addrSERV, sizeof(struct sockaddr)) <0 ) {
                printf("%s: Fail to connect() to the server, Err'%s'\n", __func__, strerror(errno));
		free(uclit);
		return NULL;
	}

	/* Try to getsockname */
        socklen_t clen=sizeof(uclit->addrME);
        if( getsockname(uclit->sockfd, (struct sockaddr *)&(uclit->addrME), &clen)!=0 ) {
                printf("%s: Fail to getsockname for client, Err'%s'\n", __func__, strerror(errno));
        }
	else
		printf("%s: An EGI TCP client created at %s:%d, targeting to the server at %s:%d.\n",
			 __func__, inet_ntoa(uclit->addrME.sin_addr), ntohs(uclit->addrME.sin_port),
				   inet_ntoa(uclit->addrSERV.sin_addr), ntohs(uclit->addrSERV.sin_port)	  );

	/* OK */
	return uclit;
}


/* ------------------------------------------
Close an TCP Client and release resoruces.

Return:
	0	OK
	<0	Fails
--------------------------------------------*/
int inet_destroy_tcpClient(EGI_TCP_CLIT **uclit)
{
	if(uclit==NULL || *uclit==NULL)
		return 0;

	/* Make sure its session has been safely closed! */

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
