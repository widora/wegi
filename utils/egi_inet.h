/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Note:
1. All structs/data in ipackets are in private byte-order, without
   consideration of portability!



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

/* Some TCP/UDP common functions */
int inet_sock_setTimeOut(int sockfd, long sndtmo_sec, long sndtmo_usec, long rcvtmo_sec, long rcvtmo_usec);

int inet_udp_recvfrom(int sockfd, struct sockaddr *src_addr, void *data, size_t len);
int inet_udp_sendto(int sockfd, const struct sockaddr *dest_addr, const void *data, size_t packsize);

int inet_tcpsock_setUserTimeOut(int sockfd, unsigned long usertmo_ms);
int inet_tcpsock_keepalive(int sockfd, int idle_sec, int intvl_sec, int probes);
int inet_tcp_recv(int sockfd, void *data, size_t packsize);		/* A BLOCK TIMEOUT make it return fail! */
int inet_tcp_send(int sockfd, const void *data, size_t packsize);	/* A BLOCK TIMEOUT make it return fail! */
int inet_tcp_getState(int sockfd);


			/* ------------ An EGI_INETPACK : An abstract packet model ----------- */

/*** Note:
 * 1. Create your private MSGDATA, and nest it into data[].
 */
struct egi_inet_packet {
	int  packsize;	/*  Total size
			 *   packsize=sizeof(EGI_INETPACK)+headsize+privdata_size
			 *   headsize>=0; datasize>=0
			 */

	int  headsize;	/*   private head, size of head[] in data[], MAY be 0! */
	char data[];  	/*   head[headsize] + privdata[privdata_size]
			 *   head[]:     MAY be null.
			 *   privdata[]: data OR nested sub_ipackets...
			 */
};

void*  IPACK_HEADER(EGI_INETPACK *ipack);
void*  IPACK_PRIVDATA(EGI_INETPACK *ipack);
int    IPACK_DATASIZE(const EGI_INETPACK *ipack);
int    IPACK_PRIVDATASIZE(const EGI_INETPACK *ipack);

EGI_INETPACK* 	inet_ipacket_create(int headsize, void *head, int privdata_size, void * privdata);
void 	inet_ipacket_free(EGI_INETPACK** ipack);
int 	inet_ipacket_loadData(EGI_INETPACK *ipack, int off, const void *data, int size); /* Normally: off == sizeof(PRIV_HEADER) */
int 	inet_ipacket_pushData(EGI_INETPACK **ipack, const void *data, int size);
int 	inet_ipacket_reallocData(EGI_INETPACK **ipack, int datasize);

int 	inet_ipacket_udpSend(int sockfd, const struct sockaddr *dest_addr, const EGI_INETPACK *ipack);
int 	inet_ipacket_udpRecv(int sockfd, struct sockaddr *src_addr, EGI_INETPACK **ipack);

int 	inet_ipacket_tcpSend(int sockfd, const EGI_INETPACK *ipack);
int 	inet_ipacket_tcpRecv(int sockfd, EGI_INETPACK **ipack);       /* Auto. realloc ipack according to leading ipack.packsize */



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
__attribute__((weak)) void inet_sigpipe_handler(int signum);
__attribute__((weak)) void inet_sigint_handler(int signum);
int inet_default_sigAction(void);





			/* ------------ UDP C/S ----------- */

#define EGI_MAX_UDP_PDATA_SIZE	65507	/* Actual MAX. 2^16-1-8-20(IP header)=65507Bs, Or Err'Message too long'
					 * This value also limit static send/recv buffer in UDP routine functions
 					 */
#define UDP_USER_TIMER_SET      20      /* In seconds. As in inet_tcp_recvfrrom() and inet_udp_sendto(), to count total time waiting/trying
                                         * when RCVTIMEO/SNDTIMEO is set.
                                         * If rcv(BLOCK)/send(BLOCK) fails persistently, and total time amount to the value,
                                         * then the function will give up trying.
					 * It should be greater than timeout value in inet_create_udpServer()
					 */


/*------------------------------------------------------------------------
          EGI UDP Server/Client Process Callback Functions
Tasks in callback function should be simple and short, normally
just to pass out received data to the Caller, and take bring data into
UDP Server/Client routine.

			!!! WARNING !!!
All pointers are lvalues, which've been allocated in _routine().

@updcmd:	Command code

@rcvAddr:	Counter part address, from which rcvData was received.
@rcvData:	Data received from rcvCLIT
@rcvSize:	Data size of rcvData. if<=0, ignore rcvData!

@sndAddr:	Counter part address, to which sndBuff will be sent.
@sndBuff:	Data to send to sndCLIT
@sndSize:	Data size of sndBuff, if<=0, ignore sndBuff.


Note:
1. If *sndSize NOT re_assigned to a positive value, server/client rountine
will NOT proceed to sendto().



Return:
	0 	Ok
	<0      Fails, the xxx_routine() will NOT proceed to sendto().
--------------------------------------------------------------------------*/
typedef int (* EGI_UDPSERV_CALLBACK)( int *cmdcode, const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
				            	   struct sockaddr_in *sndAddr,       char *sndBuff, int *sndSize);

typedef int (* EGI_UDPCLIT_CALLBACK)( int *cmdcode, const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
				                   			       char *sndBuff, int *sndSize);


/*** EGI_UDP_SERV & EGI_UDP_CLIT
 *
 *   			An EGI UDP Server/Client Model
 * 1. ONLY One routine process for receiving/sending all datagrams, and a backcall
 *    function to pass out/in all datagrams. The caller SHALL take responsiblity
 *    to identify clients and handle sessions respectively.
 * 2. Set xxx_waitus according to specific scenarios, For instance, tune xxx_waitus
 *    to adjust MEM/CPU load and improve packet lost/error ratio.
 * 3. In most case, it works in 'AN Active Sender and A Passive Receiver' mode, and
 *    recv_waitus is usually 0, while send_waitus is NOT 0!
 */

#define UDPCMD_NONE  		0
#define UDPCMD_END_ROUTINE  	1

struct egi_udp_server {
	int 			sockfd;		/* Socket descriptor */
	struct sockaddr_in 	addrSERV;	/* Sefl address */
	EGI_UDPSERV_CALLBACK 	callback;	/* Callback function */
	unsigned int		idle_waitus;	/* Sleep us for idle looping, to relax CPU load. */
	unsigned int		send_waitus;	/* Sleep us after sendto(), to relax traffic pressure through whole link. */
						/* For server, recvfrom() is MSG_DONTWAIT mode */
	unsigned int 		recv_waitus;	/* Sleep us after recvfrom() */
	int			cmdcode;	/* Commad code to routine loop, NOW: 1 to end routine. */
};

struct egi_udp_client {
	int 			sockfd;		/* Socket descriptor */
	struct sockaddr_in 	addrSERV;	/* Server address */
	struct sockaddr_in 	addrME;		/* Self address */
	EGI_UDPCLIT_CALLBACK 	callback; 	/* Callback function */
	unsigned int		idle_waitus;	/* Sleep us for idle looping, to relax CPU load. */
	unsigned int		send_waitus;	/* Sleep us after each sendto(), to relax traffic pressure through whole link. */
						/* For client, recvfrom() is BLOCKING mode. */
	unsigned int 		recv_waitus;	/* Sleep us after recvfrom() */
	int			cmdcode;	/* Commad code to routine loop, NOW: 1 to end routine. */
};

/* UDP C/S Basic Functions */
EGI_UDP_SERV* 	inet_create_udpServer(const char *strIP, unsigned short port, int domain);
int 		inet_destroy_udpServer(EGI_UDP_SERV **userv);
int 		inet_udpServer_routine(EGI_UDP_SERV *userv);
int 		inet_udpServer_TESTcallback( int *cmdcode, const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                                       		   	struct sockaddr_in *sndAddr,       char *sndBuff, int *sndSize);
					     /* !!!WARNING: All pointers are lvalues, allocated in _routine() */

EGI_UDP_CLIT* 	inet_create_udpClient(const char *servIP, unsigned short servPort, const char *myIP, int domain);
int 		inet_destroy_udpClient(EGI_UDP_CLIT **uclit);
int 		inet_udpClient_routine(EGI_UDP_CLIT *uclit);
int 		inet_udpClient_TESTcallback( int *cmdcode, const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                                       		          			      char *sndBuff, int *sndSize);
					     /* !!!WARNING: All pointers are lvalues, allocated in _routine() */

			/* ------------ TCP C/S ----------- */

#define EGI_MAX_TCP_PDATA_SIZE	(1024*64)  /* Actually there is NO limit for TCP stream data size */

/* TCP Session: for server routine process */
typedef struct egi_tcp_server_session {
	int		sessionID;		 /* ID, ref. index of EGI_TCP_SERV_SESSION.sessions[], NOT as sequence number! */
	int    		csFD;                    /* C/S session fd */
        struct 		sockaddr_in addrCLIT;    /* Client address */
	bool   		alive;			 /* flag on, if active/living, ?? race condition possible! check csFD. */

	pthread_t 	csthread;		 /* C/S session thread */
	int		cmd;			 /* Commad to session handler, NOW: 1 to end routine. */
} EGI_TCP_SERV_SESSION;  /* At server side */


/* FOR TEST ONLY ---------------------------------------------------- */

/***		------  EGI_TCP_SERV & EGI_TCP_CLIT  ------
 *
 *                         An EGI TCP Server Model
 * A main thread for accepting all clients, and one session handling thread
 * is created for each accepted client. The caller SHALL design its session
 * handling/processing thread function.
 */

/*** 		----- KEEPALIVE TIME  v.s. SND/RCV TIMEOUT -----

 * A. Set KEEPALIVE TIME:
 *	!!!--- If it's running a TCP send(), then keepalive probe will be suspended/prevented ---!!!
 *      Usually for slow speed and/or long time connections, there may be NO data flow on the link for a certain long time.
 *   	It needs to probe the remote peer intervally and make sure that it is still connected.
 * 	A KEEPALIVE probe is supposed to be carried out during idle time.

 * B. TCP_USER_TIMEOUT:
 *	!!!--- It functions only during TCP sending. ---!!!
 *	"it specifies the maximum amount of time in milliseconds that transmitted data may remain unacknowledged
 *      before TCP will forcibly close the corresponding connection and return ETIMEDOUT to the application."
 * 	"when used with the TCP keepalive (SO_KEEPALIVE) option, TCP_USER_TIMEOUT will override keepalive to ..."  ( man 7 tcp )
 * 	You'd better apply ONLY one of the them.

 * C. Set SNDTIMEO/RCVTIMEO  or  SNDTIMEO/RCVTIMEO + User_Timer:
 *	1. Usually for high speed and/or short time connections, and data flow MUST keep consistently on the link.
 *	   Long delay or too many brokes(TimeOuts) on send()/recv() may cause upper-layer application failure,
 *	   and it needs to stop OR retart under such circumstances.
 *	2. Only a send() (OR keepalive probe) can actively probe abnormal disconnection and invoke a ETIMEDOUT error,
 *    	   as it will wait ACK msg after sendout, and can reckon total time passed.
 *     	   A recv() function can NOT detect and return such an error by itself! Your may add a timer to decide when to abort.
 *	3. RCVTIMEO is much more accurate than SNDTIMEO, which is only a rough reference value?

 * NOTE:
 *	1. Only a sending (OR keepalive probing) can actively probe abnormal disconnection and invoke a ETIMEDOUT error,
 *	 ( send() trys abt. 13-30Min before return ETIMEDOUT )
 *	2. For BLOCK type send()/recv(), Use SNDTIMEO/RCVTIMEO is OK?
 *	3. For NONBLOCK or select method, Use KEEPALVIE +  TCP_USER_TIMEOUT is OK?
 *
 */

#define TCP_KEEPALIVE_IDLE	20	/* If socket goes idle in this value seconds, start to probe remote peer then.
					 * !!! WARNING !!!  This value SHALL less than TCP SND/RECV TimeOut! or it will be overrided.
					 * Also see following TCP_SERV/CLIT_SNDTIMEO/SNDTIMEO.
					 */
#define TCP_KEEPALIVE_INTVL	3	/* It will probe _PROBES times with interval of _INTVL seconds before return ETIMEOUT */
#define TCP_KEEPALIVE_PROBES	3

#define TCP_USER_TIMER_SET	30	/* In seconds. As in inet_tcp_recv() and inet_tcp_send(), to count total time waiting/trying
					 * when RCVTIMEO/SNDTIMEO is set.
					 * If rcv(BLOCK)/send(BLOCK) fails persistently, and total time amount to the value,
					 * then it's deemed as lost connection.
					 * !!! WARINGI !!! This value should be greater than xx_RCVTIMEO/xx_SNDTIMEO, because
					 * actual USER_TIMER value should plus one round of RCVTIMEO/SNDTIMEO.
					 */

#define TCP_USER_TRNASTIMEO	10000   /* in ms, for TCP_USER_TIMEOUT, to replace send() 15 Min
					 * Seems Just a rough value?
					 * The option has no effect on when TCP retransmits a packet, nor when a keepalive probe is sent.
					 */

#define TCP_SERV_SNDTIMEO	35	/* If send(BLOCK) is running, TCP_KEEPALIVE will be disabled/suspended/ingored!? whatever its value.*/
		/* NOTE 'man 7 tcp': "Moreover, when used with the TCP keepalive (SO_KEEPALIVE) option, TCP_USER_TIMEOUT will override keepalive
	       	 * to determine when to close a connection due to keepalive failure." */
#define TCP_SERV_RCVTIMEO	35	/* recv(BLOCK): If TCP_KEEPALIVE applys, consider to be greater than Keepalive_SumAllTime +allowance
					 * OR it will be disabled/suspended/ingored.
					 */

#define TCP_CLIT_SNDTIMEO    35		/* If send(BLOCK) is running, TCP_KEEPALIVE will be disabled/suspended/ingored!? wathever its value. */
#define TCP_CLIT_RCVTIMEO    35		/* recv(BLOCK): If TCP_KEEPALIVE applys, consider to be greater than Keepalive_SumAllTime +allowance
					 * OR it will be disabled/suspended/ingored.
					 * If TCP_USER_TIMEOUT is set, then it be will forever blocked!
					 */

/* END FOR TEST ONLY ---------------------------------------------------- */

#define TCPCMD_NONE  		0
#define TCPCMD_END_SESSION  	1

/*** EGI_TCP_SERV ***/
struct egi_tcp_server {

	int 			sockfd;		/* For accept() */
	struct sockaddr_in 	addrSERV;	/* Self address */
	struct timeval		rcvtimeout,sndtimeout;	/* TimeOut, 0 as sys default */
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

/*** EGI_TCP_CLIT ***/
struct egi_tcp_client {

	int 	sockfd;
	struct sockaddr_in  addrSERV;
	struct sockaddr_in  addrME;	/* Self address */
	struct timeval	    rcvtimeout,sndtimeout;
};


/* TCP C/S Basic Functions */

EGI_TCP_SERV* 	inet_create_tcpServer(const char *strIP, unsigned short port, int domain, unsigned int sndtimeo, unsigned int rcvtimeo);
int 		inet_destroy_tcpServer(EGI_TCP_SERV **userv);	/* close and free */
int 		inet_tcpServer_routine(EGI_TCP_SERV *userv);
void* 		TEST_tcpServer_session_handler(void *arg);    /* A thread function */

EGI_TCP_CLIT* 	inet_create_tcpClient(const char *servIP, unsigned short servPort, int domain, unsigned int sndtimeo, unsigned int rcvtimeo);
int 		inet_destroy_tcpClient(EGI_TCP_CLIT **uclit);	/* close and free */
int 		TEST_tcpClient_routine(EGI_TCP_CLIT *uclit, int packsize, int gms);
#endif
