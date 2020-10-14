/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Note:
1. All structs are in private byte-order, without consideration of
   portability!



Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#ifndef __EGI_INET_H__
#define __EGI_INET_H__

#include <netinet/in.h>
#include <stdbool.h>

typedef struct egi_inet_packet	EGI_INETPACK;
typedef struct egi_inet_msgdata EGI_INET_MSGDATA;

typedef struct egi_udp_server EGI_UDP_SERV;
typedef struct egi_udp_client EGI_UDP_CLIT;
typedef struct egi_tcp_server EGI_TCP_SERV;
typedef struct egi_tcp_client EGI_TCP_CLIT;

			/* ------------ EGI_INETPACK : An abstract packet model ----------- */

/*** Note:
 * 1. Create your private MSGDATA, and nest it into data[].
 */
struct egi_inet_packet {
	int  packsize;	/* Total size
			 *   packsize=headsize+privdata_size+sizeof(EGI_INETPACK)
			 *   headsize>=0; datasize>=0
			 */

	int  headsize;	/* size of head[] in data[], MAY be 0! */

	//char *head;
	//char *privdata;

	char data[];  	/*   head[headsize] + privdata[privdata_size]
			 *   head[]:     MAY be null.
			 *   privdata[]: data OR nested sub_packets...
			 */
};

EGI_INETPACK* inet_ipacket_create(int headsize, int privdata_size);
void inet_ipacket_free(EGI_INETPACK** ipack);
void*  IPACK_HEADER(EGI_INETPACK *ipack);
void*  IPACK_PRIVDATA(EGI_INETPACK *ipack);

			/* ------------ EGI_INET_MSGDATA : An practical packet model ----------- */
/*** Note:
 *  1. This MSGDATA struct is NOT suitable/efficient for high frequency small packets transaction, as size of msg part is fixed!
 *     You may like to create a private MSGDATA struct based on EGI_INETPACK, with adjustable headsize.
 *  2. An appropriate size of MSGDATA can improve effeciency in recv()/send() opertaion???
 *     Example: take 1448 Bytes, as the Max Single TCP payload for 1500 MTU. then no need to be re_fregemented for each packet.
 *  3. Limit size of msg to be 548 bytes.
 */
struct egi_inet_msgdata {
	struct egi_inet_msg {
		#define MSGDATA_CMSG_SIZE 	512            /* <= 548 -sizeof(nl_datasize)4-sizeof(strcut timeval)8 */
		char 		cmsg[MSGDATA_CMSG_SIZE];	/* chars/string message */
		/* --- OR to be allocated for each case */
		//char 	*cmsg;

		uint32_t	nl_datasize;		/* Net order long type, Limit 2^31
							 * In practice, actual data[] length may be greater than nl_datasize!
							 * and the original data size is kept in another var.
							 */
		/* --- OR to define private order, net order is NOT necessary for two friendly ENDs! */
		//char 	datasize[4];			/* size for attached data,  Inet data size limit 2^31 */

		struct timeval  tmstamp;		/* Time stamp in private order */
		//TODO: encrypt stamp/signature

	} msg;

	char data[];
};

/* INET MSGDATA functions */
EGI_INET_MSGDATA* inet_msgdata_create(uint32_t datasize);
void inet_msgdata_free(EGI_INET_MSGDATA** msgdata);
int inet_msgdata_getDataSize(const EGI_INET_MSGDATA *msgdata);
int inet_msgdata_reallocData(EGI_INET_MSGDATA **msgdata, int datasize);
int inet_msgdata_getTotalSize(const EGI_INET_MSGDATA *msgdata);
int inet_msgdata_updateTimeStamp(EGI_INET_MSGDATA *msgdata);
int inet_msgdata_loadData(EGI_INET_MSGDATA *msgdata, const char *data); /* load data into msgdata->data[] */
int inet_msgdata_copyData(const EGI_INET_MSGDATA *msgdata, char *data); /* copy data from msgdata-data[] */
int inet_msgdata_recv(int sockfd, EGI_INET_MSGDATA *msgdata);
int inet_msgdata_send(int sockfd, const EGI_INET_MSGDATA *msgdata);


			/* ------------ Signal handling ----------- */

/* Signal handling */
void inet_signals_handler(int signum);
int inet_register_sigAction(int signum, void(*handler)(int));
int inet_default_sigAction(void);

			/* ------------ UDP C/S ----------- */

/******** MTU and TCP/UDP packet payload ********

  Traditional MTU 1500Bytes
  Jumboframe MTU 9000Bytes for morden fast Ethernet.

  EtherNet frame packet payload: 	46-MTU(1500) bytes. Max.1500.
  PPPoE frame packet palyload:   	46~MTU(1500-8=1492) bytes. Max. 1492?

  Single IP packet Max. payload:	MTU(1500)-IPhead(20)					=1480 Bytes.
  Single TCP packet Max. payload:	MTU(1500)-IPhead(20)-TCPhead(20)			=1460 Bytes.
					MTU(1500)-IPhead(20)-TCPhead(20)-TimestampOption(12)	=1448 Bytes.
  TCP MMS (Max. Segment Size)
  Single UDP packet Max.payload:	MTU(1500)-IPhead(20)-UDPhead(8)				=1472 bytes.
	       when MTU=576: 		MTU(576)-IPhead(20)-UDPhead(8)				=548 bytes.

  UPD datagram  Max. size: 2^16-1-8-20=65507
  TCP datagram  Max. size: Stream, as no limit.

************************************************/


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


/* EGI_UDP_SERV & EGI_UDP_CLIT */
struct egi_udp_server {
	int sockfd;
	struct sockaddr_in addrSERV;
	EGI_UDPSERV_CALLBACK callback;
};

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

#define EGI_MAX_TCP_PDATA_SIZE	(1024*64)  /* Actually there is NO limit for TCP stream data size */

/* TCP Session for Server */
typedef struct egi_tcp_server_session {
	int		sessionID;		 /* ID, index of EGI_TCP_SERV_SESSION.sessions[]  */
	int    		csFD;                    /* C/S session fd */
        struct 		sockaddr_in addrCLIT;    /* Client address */
	bool   		alive;			 /* flag on, if active/living, ?? race condition possible! check csFD. */

	pthread_t 	csthread;		 /* C/S session thread */
	int		cmd;			 /* Commad to session handler, NOW: 1 to end routine. */
}EGI_TCP_SERV_SESSION;  /* At server side */


/* EGI_TCP_SERV & EGI_TCP_CLIT */
struct egi_tcp_server {
	int 			sockfd;		/* For accept */
	struct sockaddr_in 	addrSERV;	/* Self address */
#define MAX_TCP_BACKLOG		16
	int 			backlog;	/* BACKLOG for accept(), init. in inet_create_tcpServer() as MAX_TCP_BACKLOG */
	bool			active;    	/* Server routine IS running! */
	int			cmd;		/* Commad to routine loop, NOW: 1 to end routine. */

	/* TCP sessions */
	int 			ccnt;				/* Clients/Sessions counter */
#define EGI_MAX_TCP_CLIENTS	8
	EGI_TCP_SERV_SESSION 	sessions[EGI_MAX_TCP_CLIENTS];  /* Sessions */
        void *          	(*session_handler)(void *arg);  /* Designed as a detached thread session handler!
								 * input arg as EGI_TCP_SERV_SESSION *session
								 */

};

struct egi_tcp_client {
	int 	sockfd;
	struct sockaddr_in addrSERV;
	struct sockaddr_in addrME;	/* Self address */
};


/* TCP C/S Basic Functions */
int inet_tcp_recv(int sockfd, void *data, size_t packsize);
int inet_tcp_send(int sockfd, const void *data, size_t packsize);

EGI_TCP_SERV* 	inet_create_tcpServer(const char *strIP, unsigned short port, int domain);
int 		inet_destroy_tcpServer(EGI_TCP_SERV **userv);
int 		inet_tcpServer_routine(EGI_TCP_SERV *userv);
void* 		TEST_tcpServer_session_handler(void *arg);    /* A thread function */

EGI_TCP_CLIT* 	inet_create_tcpClient(const char *servIP, unsigned short servPort, int domain);
int 		inet_destroy_tcpClient(EGI_TCP_CLIT **uclit);
int 		TEST_tcpClient_routine(EGI_TCP_CLIT *uclit, int packsize, int gms);
#endif
