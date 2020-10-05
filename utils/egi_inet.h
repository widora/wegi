/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#ifndef __EGI_INET_H__
#define __EGI_INET_H__

#include <netinet/in.h>
#include <stdbool.h>
			/* ------------ UDP C/S ----------- */

/*** TCP/UDP packet payload
  EtherNet frame packet payload: 46-MTU(1500) bytes. Max.1500.
  PPPoE frame packet palyload:   46-MTU(1500-8=1492) bytes. Max. 1492?

  Single IP packet Max. payload:	MTU(1500)-IPhead(20)			=1480 Bytes.
  Single TCP packet Max. payload:	MTU(1500)-IPhead(20)-TCPhead(20)	=1460 Bytes.
  Single UDP packet Max.payload:	MTU(1500)-IPhead(20)-UDPhead(8)		=1472 bytes.
	       when MTU=576: 		MTU(576)-IPhead(20)-UDPhead(8)		=548 bytes.

  UPD datagram Max. size: 2^16-1-8-20=65507
  TCP datagram Max. size: No limit.
***/

//#define EGI_MAX_UDP_PDATA_SIZE	1024   /* Max. UDP packet payload size(exclude 8bytes UDP packet header), limited for MTU.  */
#define EGI_MAX_UDP_PDATA_SIZE	(1024*60)

/*---------------------------------------------------------------
          EGI UDP Server/Client Process Callback Functions
Tasks in callback function should be simple and short, normally
just to pass out received data to the Caller, and take in data to
the UDP server/client.

@rcvAddr:	Counter part address, from which rcvData was received.
@rcvData:	Data received from rcvCLIT
@rcvSize:	Data size of rcvData. if<=0, ignore rcvData!

@sndAddr:	Counter part address, to which sndBuff will be sent.
@sndBuff:	Data to send to sndCLIT
@sndSize:	Data size of sndBuff, if<=0, ignore sndBuff.
---------------------------------------------------------------*/
typedef int (* EGI_UDPSERV_CALLBACK)( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
				            struct sockaddr_in *sndAddr,       char *sndBuff, int *sndSize);


typedef int (* EGI_UDPCLIT_CALLBACK)( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
				                   			       char *sndBuff, int *sndSize);


/* EGI UDP SERVER */
typedef struct egi_udp_server EGI_UDP_SERV;
struct egi_udp_server {
	int sockfd;
	struct sockaddr_in addrSERV;
	EGI_UDPSERV_CALLBACK callback;
};

/* EGI UDP CLIENT */
typedef struct egi_udp_client EGI_UDP_CLIT;
struct egi_udp_client {
	int sockfd;
	struct sockaddr_in addrSERV;
	struct sockaddr_in addrME;	/* Self address */
	EGI_UDPCLIT_CALLBACK callback; /* Same as serv callback */
};

/* UDP C/S Basic Functions */
EGI_UDP_SERV* 	inet_create_udpServer(const char *strIP, unsigned short port, int domain);
int 		inet_destroy_udpServer(EGI_UDP_SERV **userv);
int 		inet_udpServer_routine(EGI_UDP_SERV *userv);
int 		inet_udpServer_TESTcallback( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                                       		   struct sockaddr_in *sndAddr,       char *sndBuff, int *sndSize);

EGI_UDP_CLIT* 	inet_create_udpClient(const char *servIP, unsigned short servPort, int domain);
int 		inet_destroy_udpClient(EGI_UDP_CLIT **uclit);
int 		inet_udpClient_routine(EGI_UDP_CLIT *uclit);
int 		inet_udpClient_TESTcallback( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                                       		          			      char *sndBuff, int *sndSize);


			/* ------------ TCP C/S ----------- */

#define EGI_MAX_TCP_PDATA_SIZE	(1024*60)  /* Actually there is NO limit for TCP stream data size */
#define MAX_TCP_BACKLOG		16

/* Callback functions */

typedef struct egi_tcp_session {
	int    csFD;                    /* C/S session fd */
        struct sockaddr_in addrCLIT;    /* Client address */
	pthread_t csthread;		/* C/S session thread */
	bool   alive;
}EGI_TCP_SESSION;

/* EGI TCP SERVER */
#define EGI_MAX_TCP_CLIENTS	16
typedef struct egi_tcp_server EGI_TCP_SERV;
struct egi_tcp_server {

	int sockfd;
	struct sockaddr_in addrSERV;		/* Self address */
	int backlog;

	/* TCP session */
	int ccnt;				/* Clients/Sessions counter */
	EGI_TCP_SESSION sessions[EGI_MAX_TCP_CLIENTS];
        void *          (*session_handler)(void *);  // (EGI_TCP_SESSION *session);

};

/* EGI UDP CLIENT */
typedef struct egi_tcp_client EGI_TCP_CLIT;
struct egi_tcp_client {
	int sockfd;
	struct sockaddr_in addrSERV;
	struct sockaddr_in addrME;	/* Self address */
};



/* TCP C/S Basic Functions */
EGI_TCP_SERV* 	inet_create_tcpServer(const char *strIP, unsigned short port, int domain);
int 		inet_destroy_tcpServer(EGI_TCP_SERV **userv);
int 		inet_tcpServer_routine(EGI_TCP_SERV *userv);
void* 		TEST_tcpServer_session_handler(void *arg);

EGI_TCP_CLIT* 	inet_create_tcpClient(const char *servIP, unsigned short servPort, int domain);
int 		inet_destroy_tcpClient(EGI_TCP_CLIT **uclit);

#endif
