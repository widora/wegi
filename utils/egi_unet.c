/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

SURFMAN, SURFUSER

Note:
1. A simple AF_UNIX local communication module.
2. All functions based on UNIX domain sockets!


----- Struct -----

1. struct sockaddr_un
        #define UNIX_PATH_MAX   108
        struct sockaddr_un {
                __kernel_sa_family_t sun_family; // AF_UNIX
                char sun_path[UNIX_PATH_MAX];    //pathname
        };

2. struct msghdr
        struct msghdr {
               void         *msg_name;       // optional address
	       //used on an unconnected socket to specify the target address for a datagram
               socklen_t     msg_namelen;    // size of address
               struct iovec *msg_iov;        // scatter/gather array, writev(), readv()
               size_t        msg_iovlen;     // # elements in msg_iov
               void         *msg_control;    // ancillary data, see below
               size_t        msg_controllen; // ancillary data buffer len
               int           msg_flags;      // flags (unused)
           };

3. struct cmsghdr and CMSG_xxx
        struct cmsghdr {
           socklen_t cmsg_len;    // data byte count, including header
           int       cmsg_level;  // originating protocol
           int       cmsg_type;   // protocol-specific type
           // followed by unsigned char cmsg_data[];
       };

       struct cmsghdr *CMSG_FIRSTHDR(struct msghdr *msgh);
       struct cmsghdr *CMSG_NXTHDR(struct msghdr *msgh, struct cmsghdr *cmsg);
       size_t CMSG_ALIGN(size_t length);
       size_t CMSG_SPACE(size_t length);
       size_t CMSG_LEN(size_t length);
       unsigned char *CMSG_DATA(struct cmsghdr *cmsg);


Journal:
2021-02-17/18:
	1. Add EGI_RING, EGI_SURFACE concept test functions.
2021-02-21:
	1. Spin off EGI_SURFACE functions.
2021-03-5:
	1. unet_create_Userver(): change userv->sockfd=socket() from NONBLOCK to BLOCK type.
2021-03-7:
	1. Add: ERING_MSG, ering_msg_init(), ering_msg_free().
2021-03-8:
	1. Add: ering_msg_send(), ering_msg_recv();
2021-03-9:
	1. unet_sendmsg()/ering_msg_send(): Add flag MSG_DONTWAIT to make it NON_BLOCKING.
2021-03-10:
	1. userv_listen_thread(): Move checking_available_slot after calling_accept().
2021-06-24:
	1. ering_msg_send(): Consider if(data!=NULL && len!=0)...

Midas Zhou
midaszhou@yahoo.com
---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <libgen.h> /* dirname() */

/* EGI_SURFACE */
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/select.h>

#include "egi_utils.h"
#include "egi_unet.h"
#include "egi_timer.h"
#include "egi_debug.h"

static void* userv_listen_thread(void *arg);


	/* ========================  Signal Handling Functions  ======================== */
			   /* NOTE: OR to use egi_procman module */

/*--------------------------------------
	Signal handlers
----------------------------------------*/
void unet_signals_handler(int signum)
{
	switch(signum) {
		case SIGPIPE:
			printf("Catch a SIGPIPPE!\n");
			break;
		case SIGINT:
			printf("Catch a SIGINT!\n");
			break;
		default:
			printf("Catch signum %d\n", signum);
	}

}
__attribute__((weak)) void unet_sigpipe_handler(int signum)
{
	printf("Catch a SIGPIPE!\n");
}

__attribute__((weak)) void unet_sigint_handler(int signum)
{
	printf("Catch a SIGINT!\n");
}


/*--------------------------------------------------
Set a signal acition.

@signum:        Signal number.
@handler:       Handler for the signal

Return:
        0       Ok
        <0      Fail
---------------------------------------------------*/
int unet_register_sigAction(int signum, void(*handler)(int))
{
	struct sigaction sigact;

	// .sa_mask: a  mask of signals which should be blocked during execution of the signal handler
        sigemptyset(&sigact.sa_mask);
	// the signal which triggered the handler will be blocked, unless the SA_NODEFER flag is used.
        //sigact.sa_flags|=SA_NODEFER;
        sigact.sa_handler=handler;
        if(sigaction(signum, &sigact, NULL) <0 ){
                printf("%s: Fail to call sigaction() for signum %d.\n",  __func__, signum);
                return -1;
        }

        return 0;
}

/*-----------------------------
Set default signal actions.
------------------------------*/
int unet_default_sigAction(void)
{
	if( unet_register_sigAction(SIGPIPE,unet_sigpipe_handler)<0 )
		return -1;

//	if( unet_register_sigAction(SIGINT,unet_sigint_handler)<0 )
//		return -1;

	return 0;
}


	/* ========================  Unet Functions  ========================== */


/*-----------------------------------------------------
1. Create an AF_UNIX socket sever for local communication.
and type of socket is SOCK_STREAM.
2. Start a thread to listen and accept cliens.

@svrpath:	userver path. (Must be full path!)

Return:
	Poiter to an EGI_USERV	  OK
	NULL			  Fails
------------------------------------------------------*/
EGI_USERV* unet_create_Userver(const char *svrpath)
{
	EGI_USERV *userv=NULL;

        /* 1. Calloc EGI_USERV */
        userv=calloc(1,sizeof(EGI_USERV));
        if(userv==NULL)
                return NULL;

	/* 2. Create socket  */
	//userv->sockfd = socket(AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
	userv->sockfd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0);
	if(userv->sockfd <0) {
		//printf("%s: Fail to create socket! Err'%s'.\n", __func__, strerror(errno));
		egi_dperr("Fail to create socket!");
		free(userv);
		return NULL;
	}

	/* 3. If old path exists, fail OR remove it? */
	if( access(svrpath,F_OK) ==0 ) {
	   #if 0  /* TODO: */
		//printf("%s: svrpath already exists!\n", __func__);
		egi_dpstd("svrpath already exists!\n");
		/* DO NOT goto END_FUNC, it will try to unlink svrpath again! */
		close(userv->sockfd);
		free(userv);
		return NULL;
	   #else
		//printf("%s: svrpath already exists! try to unlink it...\n", __func__);
		egi_dpstd("svrpath already exists! try to unlink it...\n");
		if( unlink(svrpath)!=0 ) {
			//printf("%s: Fail to remove old svrpath! Err'%s'.\n", __func__, strerror(errno));
			egi_dperr("Fail to remove old svrpath!");
			goto END_FUNC;
		}
	   #endif
	}

	/* 4. Prepare address struct */
	memset(&userv->addrSERV, 0, sizeof(typeof(userv->addrSERV)));
	userv->addrSERV.sun_family = AF_UNIX;
	strncpy(userv->addrSERV.sun_path, svrpath, sizeof(userv->addrSERV.sun_path)-1);

	/* 5. Bind address */
	if( bind(userv->sockfd, (struct sockaddr *)&userv->addrSERV, sizeof(typeof(userv->addrSERV))) !=0 ) {
		//printf("%s: Fail to bind address for sockfd! Err'%s'.\n", __func__, strerror(errno));
		egi_dperr("Fail to bind address for sockfd!");
		goto END_FUNC;
	}

	/* 6. Set backlog */
	userv->backlog=USERV_MAX_BACKLOG;

	/* 7. Start listen/accept thread */
	if( pthread_create(&userv->acpthread, NULL, (void *)userv_listen_thread, userv) !=0 ) {
		//printf("%s: Fail to launch userv->acpthread! Err'%s'.\n", __func__, strerror(errno));
		egi_dperr("Fail to launch userv->acpthread!");
		goto END_FUNC;
	}
	egi_dpstd("Userv accept_thread starts...\n");

#if 0	/* DO NOT!!! Detach the thread */
	if(pthread_detach(userv->acpthread)!=0) {
        	//printf("%s: Fail to detach userv->acpthread! Err'%s'.\n", __func__, strerror(errno));
		egi_dperr("Fail to detach userv->acpthread!");
	        //go on...
	} else {
		egi_dpstd("OK! Userv->acpthread detached!\n");
	}
#endif

	/* 8. OK */
	return userv;

END_FUNC: /* Fails */
	if( access(svrpath,F_OK) ==0 ) {
		if( unlink(svrpath)!=0 )
			//printf("%s: Fail to remove old svrpath! Err'%s'.\n", __func__, strerror(errno));
			egi_dperr("Fail to remove old svrpath!");
	}
	close(userv->sockfd);
        free(userv);
        return NULL;
}


/*---------------- A thread function -------------
Start EGI_USERV listen and accept thread.
Disconnected clients will NOT be detected here!

@arg	Pointer to an EGI_USERV.
------------------------------------------------*/
static void* userv_listen_thread(void *arg)
{
	int i;
	int index;
	int csfd=0;
	struct sockaddr_un addrCLIT={0};
	socklen_t addrLen;

	EGI_USERV* userv=(EGI_USERV *)arg;
	if(userv==NULL || userv->sockfd<=0 )
                return (void *)-1;

        /* 1. Start listening */
        if( listen(userv->sockfd, userv->backlog) <0 ) {
                //printf("%s: Fail to listen() sockfd, Err'%s'.\n", __func__, strerror(errno));
		egi_dperr("Fail to listen() sockfd.");
                return (void *)-2;
        }

	/* 2. Accepting clients ... */
	userv->acpthread_on=true;
	egi_dpstd("Start loop of accepting clients...\n");
	while( userv->cmd !=1 ) {

#if 0 ///////////////// MOVE BACKWARD //////////////
		/* 2.1 Re_count clients, and get an available userv->sessions[] for next new client. */
		userv->ccnt=0;
		index=-1;
		for(i=0; i<USERV_MAX_CLIENTS; i++) {
			/* 2.1.1 Find a slot in userv->sessions[] */
			if( index<0 && userv->sessions[i].alive==false )
				index=i;  /* availble index for next session */
			/* 2.1.2 Re_count clients */
			if( userv->sessions[i].alive==true )
				userv->ccnt++;
		}
		//printf("%s: Recount active sessions, userv->ccnt=%d\n", __func__, userv->ccnt);

		/* 2.2 Check Max. clients */
		if(index<0) {
			//printf("%s: Clients number hits Max. limit of %d!\n",__func__, USERV_MAX_CLIENTS);
			egi_dpstd("Clients number hits Max. limit of %d!\n", USERV_MAX_CLIENTS);
			tm_delayms(500);

			/*** NOTE:
			 * DO NOT continue to while() here, see following 2.3A, to avoid the thread to occupy all process time.???
			 * Just a litte better... Can NOT avoid the case.
			 */
			// continue;
		}
#endif //////////////////////////


		/* 2.3 Accept clients */
		//egi_dpstd("accept() waiting...\n");
		addrLen=sizeof(addrCLIT);
	//csfd=accept4(userv->sockfd, (struct sockaddr *)&addrCLIT, &addrLen, SOCK_NONBLOCK|SOCK_CLOEXEC); /* flags for the new open file!!! */
		csfd=accept(userv->sockfd, (struct sockaddr *)&addrCLIT, &addrLen);
		if(csfd<0) {
			switch(errno) {
                                #if(EWOULDBLOCK!=EAGAIN)
                                case EWOULDBLOCK:
                                #endif
				case EAGAIN:
					egi_dperr("EWOULDBLOCK or EAGAIN");
					tm_delayms(10); /* if NONBLOCKING */
					continue;  /* ---> continue to while() */
					break;
				case EINTR:
					//printf("%s: accept() interrupted! errno=%d Err'%s'.  continue...\n",
					//					__func__, errno, strerror(errno));
					egi_dperr("accept() interrupted! errno=%d", errno);
					continue;
					break;
				default:
					//printf("%s: Fail to accept a new client, errno=%d Err'%s'.  continue...\n",
					//							__func__, errno, strerror(errno));
					egi_dperr("Fail to accept a new client, errno=%d", errno);

					//tm_delayms(20); /* if NONBLOCKING */
					/* TODO: End routine if it's a deadly failure!  */
					continue;  /* ---> whatever, continue to while() */
			}
		}

		/* NOW: csfd >0 */

#if 1
		/* 2.1 Re_count clients, and get an available userv->sessions[] for next new client. */
		userv->ccnt=0;
		index=-1;
		for(i=0; i<USERV_MAX_CLIENTS; i++) {
			/* 2.1.1 Find a slot in userv->sessions[] */
			if( index<0 && userv->sessions[i].alive==false )
				index=i;  /* availble index for next session */
			/* 2.1.2 Re_count clients */
			if( userv->sessions[i].alive==true )
				userv->ccnt++;
		}
		//printf("%s: Recount active sessions, userv->ccnt=%d\n", __func__, userv->ccnt);

		/* 2.2 Check Max. clients */
		if(index<0) {
			//printf("%s: Clients number hits Max. limit of %d!\n",__func__, USERV_MAX_CLIENTS);
			egi_dpstd("Clients number hits Max. limit of %d!\n", USERV_MAX_CLIENTS);
			tm_delayms(500);

			/*** NOTE:
			 * DO NOT continue to while() here, see following 2.3A, to avoid the thread to occupy all process time.???
			 * Just a litte better... Can NOT avoid the case.
			 */
			// continue;
		}
#endif

		/* 2.3A If get USERV_MAX_CLIENTS!  */
		if(index<0) {
			close(csfd);
			continue;
		}

	/*** NOTE:
	 * "On  Linux, the new socket returned by accept() does not inherit file status flags such as O_NONBLOCK and O_ASYNC
	 * from the listening socket.  This behavior differs from the canonical BSD sockets implementation.
 	 * Portable programs should not rely on inheritance or noninheritance of file status flags and always explicitly
	 * set all required flags on the socket returned from accept(). " --- man accept
	 */
		/* 2.4 Proceed to add a new client ...  */
		userv->sessions[index].sessionID=index;
		userv->sessions[index].csFD=csfd;
		userv->sessions[index].addrCLIT=addrCLIT;
		userv->sessions[index].alive=true;
		userv->ccnt++;

		//egi_dpstd("session[%d] starts with csFD=%d\n", index, csfd);
       		egi_dpstd("\t\t\t----- (+)userv->sessions[%d], ccnt=%d -----\n", index, userv->ccnt);
	}

	/* End thread */
	userv->acpthread_on=false;
	return (void *)0;
}


/*-------------------------------------------
Destroy an EGI_USERV.

@userv	Pointer to a pointer to an EGI_USERV.

Return:
	0	OK
	<0	Fails
-------------------------------------------*/
int unet_destroy_Userver(EGI_USERV **userv )
{
	int i;

	if(userv==NULL || *userv==NULL)
                return 0;

	/* Join acpthread */
	(*userv)->cmd=1;  /* CMD to end thread */
	//printf("%s: Join acpthread...\n", __func__);
	if( pthread_join((*userv)->acpthread, NULL)!=0 ) {
		//printf("%s: Fail to join acpthread! Err'%s'\n", __func__, strerror(errno));
		egi_dperr("Fail to join acpthread!");
		// Go on...
	}

        /* Make sure all sessions/clients have been safely closed/disconnected! */

	/* Close session fd */
        for(i=0; i<USERV_MAX_CLIENTS; i++) {
	        if( (*userv)->sessions[i].alive==true ) {
			close((*userv)->sessions[i].csFD);
			/* Reset token */
			(*userv)->sessions[i].csFD=0;
			(*userv)->sessions[i].alive=false;
		}
	}

        /* Close main sockfd */
        if(close((*userv)->sockfd)<0) {
                //printf("%s: Fail to close sockfd, Err'%s'\n", __func__, strerror(errno));
		egi_dperr("Fail to close sockfd.");
                return -1;
        }

        /* Free mem */
        free(*userv);
        *userv=NULL;

        return 0;
}


/*-----------------------------------------------------
Create an AF_UNIX socket sever for local communication.
and type of socket is SOCK_STREAM.

@svrpath:	userver path. (Must be full path!)

Return:
	0	OK
	<0	Fails
------------------------------------------------------*/
EGI_UCLIT* unet_create_Uclient(const char *svrpath)
{
	EGI_UCLIT *uclit=NULL;

        /* Calloc EGI_USERV */
        uclit=calloc(1,sizeof(EGI_UCLIT));
        if(uclit==NULL)
                return NULL;

	/* Create socket  */
	uclit->sockfd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0);
	if(uclit->sockfd <0) {
		//printf("%s: Fail to create socket! Err'%s'.\n", __func__, strerror(errno));
		egi_dperr("Fail to create socket!");
		free(uclit);
		return NULL;
	}

	/* Prepare address struct */
	memset(&uclit->addrSERV, 0, sizeof(typeof(uclit->addrSERV)));
	uclit->addrSERV.sun_family = AF_LOCAL;
	strncpy(uclit->addrSERV.sun_path, svrpath, sizeof(uclit->addrSERV.sun_path)-1);

	/* Connect to the server */
	if( connect(uclit->sockfd, (struct sockaddr *)&uclit->addrSERV, sizeof(typeof(uclit->addrSERV))) !=0 ) {
		//printf("%s: Fail to connect to the server! Err'%s'.\n", __func__, strerror(errno));
		egi_dperr("Fail to connect to the server!");
		close(uclit->sockfd);
		free(uclit);
		return NULL;
	}

	/* Assign other members */


	return uclit;
}


/*-------------------------------------------
Destroy an EGI_UCLIT.

@uclit	Pointer to a pointer to an EGI_UCLIT.

Return:
	0	OK
	<0	Fails
-------------------------------------------*/
int unet_destroy_Uclient(EGI_UCLIT **uclit )
{
        if(uclit==NULL || *uclit==NULL)
                return 0;

        /* Make sure all sessions/clients have been safely closed/disconnected! */

        /* Close main sockfd */
        if(close((*uclit)->sockfd)<0) {
                //printf("%s: Fail to close sockfd, Err'%s'\n", __func__, strerror(errno));
		egi_dperr("Fail to close sockfd!");
                return -1;
        }

        /* Free mem */
        free(*uclit);
        *uclit=NULL;

        return 0;
}


/*----------------------------------------------------------------
Send out msg through a AF_UNIX socket.

@sockfd  A valid UNIX SOCK_STREAM type socket.
	 xxxx NOPE!  Assume to be BLOCKING type.
@msg	 Pointer to MSG.

Return:
	>0	OK, the number of bytes sent.
		It excludes ancillary(msg_control) data length!
	<0	Fails
----------------------------------------------------------------*/
int unet_sendmsg(int sockfd,  struct msghdr *msg)
{
	int nsnd;
	if(msg==NULL)
		return -1;

	/* Send MSG */
	nsnd=sendmsg(sockfd, msg, MSG_NOSIGNAL|MSG_DONTWAIT);
	if(nsnd>0) {
		//printf("%s: OK, sendmsg() %d bytes!\n", __func__, nsnd);
//		egi_dpstd("OK, sendmsg() %d bytes!\n",nsnd);
	}
	else if(nsnd==0) {
		//printf("%s: nsnd=0! for sendmsg()!\n", __func__);
		egi_dpstd("nsnd=0! for sendmsg()!\n");
	}
	else { /* nsnd < 0 */
	        switch(errno) {
        		#if(EWOULDBLOCK!=EAGAIN)
                	case EWOULDBLOCK:
                        #endif
                        case EAGAIN:  //EWOULDBLOCK, for it would block */
                        	//printf("%s\n", strerror(errno));
				egi_dperr("Eagain/EwouldBlock");
				break;

			/* If  the  message  is  too  long  to pass atomically through the underlying protocol,
			 *  the error EMSGSIZE is returned, and the message is not transmitted.
       			 */
			case EMSGSIZE:
				//printf("%s: Message too long! Err'%s'.\n", __func__, strerror(errno));
				egi_dperr("Message too long!");
				break;

                        case EPIPE:
				//printf("%s: Err'%s'.\n",__func__, strerror(errno));
				egi_dperr("Epipe");
				break;
			default:
				//printf("%s: Err'%s'.\n",__func__, strerror(errno));
				egi_dperr("sendmsg fails.");
		}
	}

	return nsnd;
}


/*-----------------------------------------------
Receive a msg through a AF_UNIX socket.
The Caller should also check msg->msg_flags.

@sockfd  A valid UNIX SOCK_STREAM type socket.
	 Assume to be BLOCKING type.
@msg	 Pointer to MSG.
	 The Caller must ensure enough space!

Return:
	>0	OK, the number of bytes received.
		DO NOT include ancillary(msg_control) data length!
	=0	Counter peer quits.
	<0	Fails
------------------------------------------------*/
int unet_recvmsg(int sockfd,  struct msghdr *msg)
{
	int nrcv;
	if(msg==NULL)
		return -1;

	/* MSG_WAITALL: Wait all data, but still may fail? see man */
	nrcv=recvmsg(sockfd, msg, MSG_WAITALL); /* MSG_CMSG_CLOEXEC,  MSG_NOWAIT */
	if(nrcv>0) {
//		egi_dpstd("OK, recvmsg() %d bytes!\n", nrcv);
	}
	else if(nrcv==0) {
		egi_dperr("nrcv=0! counter peer quits the session!");
	}
	else { /* nsnd < 0 */
	        switch(errno) {
        		#if(EWOULDBLOCK!=EAGAIN)
                	case EWOULDBLOCK:
                        #endif
                        case EAGAIN:  //EWOULDBLOCK, for datastream it woudl block */
				egi_dperr("Eagain/EwouldBlock");
				break;
			case EBADF:
				egi_dperr("EBADF");
				break;
			case ECONNREFUSED:
				egi_dperr("ECONNREFUSED");
				break;
			case EFAULT:
				egi_dperr("EFAULT");
				break;
			case EINTR:
				egi_dperr("EINTR");
				break;
			case EINVAL:
				egi_dperr("EINVAL");
                                break;
			case ENOMEM:
                                egi_dperr("ENOMEM");
                                break;
			case ENOTCONN:
                                egi_dperr("ENOTCONN");
                                break;
			case ENOTSOCK:
                                egi_dperr("ENOTCONN");
                                break;
			/* Case errno==131: Err'Connection reset by peer'. */

/*-----------------------------
       ECONNREFUSED
              A remote host refused to allow the network connection (typically because it is not running the requested service).

       EFAULT The receive buffer pointer(s) point outside the process's address space.

       EINTR  The receive was interrupted by delivery of a signal before any data were available; see signal(7).

       EINVAL Invalid argument passed.

       ENOMEM Could not allocate memory for recvmsg().

       ENOTCONN
              The socket is associated with a connection-oriented protocol and has not been connected (see connect(2) and accept(2)).

       ENOTSOCK
              The file descriptor sockfd does not refer to a socket.

------------------------*/

			default:
				egi_dperr("recvmsg fails, errno=%d. ", errno);  /* errno=131 Err'Connection reset by peer'. */
		}
	}

	/* Check msg->msg_flags */
	if( msg->msg_flags != 0 ) {
		//printf("%s: msg_flags=%d, the MSG is incomplete!\n",__func__, msg->msg_flags);
		egi_dpstd("msg_flags=%d, the MSG is incomplete!\n", msg->msg_flags);
	}

	return nrcv;
}


/*--------------------------------------------
Create and init an ERING_MSG.

emsg->data length is fixed to be ERING_MSG_DATALEN.
//@capacity:   Allocated space size of emsg->data.


Return:
	Pointer to ERING_MSG	Ok
	NULL			Fails
----------------------------------------------*/
ERING_MSG* ering_msg_init(void)
{

#if 0 ///////
        struct msghdr {
               void         *msg_name;       // optional address
               //used on an unconnected socket to specify the target address for a datagram
               socklen_t     msg_namelen;    // size of address
               struct iovec *msg_iov;        // scatter/gather array, writev(), readv()
               size_t        msg_iovlen;     // # elements in msg_iov
               void         *msg_control;    // ancillary data, see below
               size_t        msg_controllen; // ancillary data buffer len
               int           msg_flags;      // flags (unused)
           };

        struct msghdr msg={0};
        struct cmsghdr *cmsg;
        int data[16]={0};
        struct iovec iov={.iov_base=data, .iov_len=sizeof(data) };

        /* MSG iov data  */
        msg.msg_iov=&iov;  /* iov holds data */
        msg.msg_iovlen=1;

#endif //////////////

	ERING_MSG *emsg=NULL;

	/* 1. Calloc ERING_MSG */
	emsg=calloc(1, sizeof(ERING_MSG));
	if(emsg==NULL)
		return NULL;

	/* 2. Calloc msghead */
	emsg->msghead=calloc(1, sizeof(struct msghdr));
	if(emsg->msghead==NULL) {
		free(emsg);
		return NULL;
	}

	/* 3. Calloc 2*iovec */
	emsg->msghead->msg_iov=calloc(2, sizeof(struct iovec));
	if(emsg->msghead->msg_iov==NULL) {
		free(emsg->msghead);
		free(emsg);
		return NULL;
	}

	/* 4. Calloc  data, fixed data length.  */
	emsg->data=calloc(1, ERING_MSG_DATALEN);
	if(emsg->data==NULL) {
		free(emsg->msghead->msg_iov);
		free(emsg->msghead);
		free(emsg);
		return NULL;
	}

	/* 5. Assign msghead memebers */

	/* 5.1 MSG IOVECs: [0]TYPE, [1]DATA */
	emsg->msghead->msg_iovlen=2;

	/* 5.2 MSG TYPE */
	emsg->msghead->msg_iov[0].iov_base=&emsg->type;
	emsg->msghead->msg_iov[0].iov_len=sizeof(typeof(emsg->type));

	/* 5.3 MSG DATA */
	emsg->msghead->msg_iov[1].iov_base=emsg->data;		/* MAY be temp. adjusted to acutual input data pointer. */
	emsg->msghead->msg_iov[1].iov_len=ERING_MSG_DATALEN;	/* Fixed */


	/* OK */
	return emsg;
}


/*----------------------------------------------
Free an ERING_MSG.

@datalen:	data length in INT,  NOT size!!!

Return:
	Pointer to ERING_MSG	Ok
	NULL			Fails
----------------------------------------------*/
void ering_msg_free(ERING_MSG **emsg)
{
	if(emsg==NULL || *emsg==NULL)
		return;

	/* Free data */
	free((*emsg)->data);

	/* Free iovec */
	free((*emsg)->msghead->msg_iov);

	/* Free msghead and emsg */
	free((*emsg)->msghead);
	free(*emsg);

	*emsg=NULL;
}


/*--------------------------------------------------------------------------
Send out ERING_MSG through an AF_UNIX socket.

@sockfd  	A valid UNIX SOCK_STREAM type socket.
	 	Assume to be BLOCKING type.
@emsg	 	Pointer to ERING_MSG.
@msg_type    	Msg type.
@data	 	Payload data in ERING_MSG data, as per MSG type.
@len	 	In bytes, lenght of payload data. (Limit: ERING_MSG_DATALEN)

Return:
	>0	OK, the number of bytes sent.
		It excludes ancillary(msg_control) data length!
	<0	Fails
--------------------------------------------------------------------------*/
int ering_msg_send(int sockfd, ERING_MSG *emsg,  int msg_type, const void *data,  size_t len)
{
	int ret=0;
//	void *tmp;

	if(emsg==NULL)
		return -1;

	/* 1. Check size, Too big may make recv_end data outflow. */
	if(len > ERING_MSG_DATALEN) {
		egi_dpstd("Input payload data length (%d) out of capacity (%d)!\n", len, ERING_MSG_DATALEN);
		return -2;
	}

	/* 2. Msg type */
	emsg->type=msg_type;

	/* 3. Copy payload data, OR temp. refer to data?? */
#if 0	/* REFER */
	tmp=emsg->msghead->msg_iov[1].iov_base;
	emsg->msghead->msg_iov[1].iov_base=data;
#else   /* COPY */
	if(data!=NULL && len!=0)
		memcpy(emsg->data, data, len);
#endif
	emsg->msghead->msg_iov[1].iov_len=ERING_MSG_DATALEN; /* Fixed data length as for data stream transfer. */

	/* 5. Send msg */
	ret=unet_sendmsg(sockfd,  emsg->msghead);

	/* 6. Reset msg_iov[1].iov_base */
#if 0   /* REFER */
	emsg->msghead->msg_iov[1].iov_base=tmp;
#endif

	/* OK */
	return ret;
}


/*--------------------------------------------------------------------
Receive ERING_MSG through an AF_UNIX socket.
(The Caller should also check msg->msg_flags.)

@sockfd  A valid UNIX SOCK_STREAM type socket.
	 Assume to be BLOCKING type.
@emsg	 Pointer to ERING_MSG.

Return:
	>0	OK, the number of bytes received.
		DO NOT include ancillary(msg_control) data length!
	=0	Counter peer quits.
	<0	Fails
---------------------------------------------------------------------*/
int ering_msg_recv(int sockfd, ERING_MSG *emsg)
{
	int ret;

	if(emsg==NULL) {
		egi_dpstd("Input emsg is NULL!\n");
		return -1;
	}

	/* ALWAYS fixed msg_iov[1].iov_len */
	//emsg->msghead->msg_iov[1].iov_len=ERING_MSG_DATALEN;

	/* Receive msg */
	ret=unet_recvmsg(sockfd,  emsg->msghead);

	/* Check emsg->msghead->msg_flags */

	return ret;
}
