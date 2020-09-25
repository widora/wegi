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

/* EtherNet packet payload MAX. 46-MTU(1500) bytes,  UPD packet payload MAX. 2^16-1-8-20=65507 */
#define EGI_MAX_UDP_PDATA_SIZE	1024   /* Max. UDP packet payload size(exclude 8bytes UDP packet header), limited for MTU.  */

/*---------------------------------------------------------------
           UDP SERVER PROCESS callback function
Tasks in callback function should be simple and short, normally
just to pass out received data to the Caller, and take in data to
the UDP server.

@rcvCLIT:	Client address, from which rcvData was sent.
@rcvData:	Data received from rcvCLIT
@rcvSize:	Data size of rcvData.

@sndCLIT:	Client address, to which sndBuff will be sent.
@sndBuff:	Data to send to sndCLIT
@sndSize:	Data size of sndBuff
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


/* Functions */
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

#endif
