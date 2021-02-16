/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple AF_UNIX local communication module.

1. struct sockaddr_un
        #define UNIX_PATH_MAX   108
        struct sockaddr_un {
                __kernel_sa_family_t sun_family; // AF_UNIX
                char sun_path[UNIX_PATH_MAX];    //pathname
        };

2. struct msghdr
        struct msghdr {
               void         *msg_name;       // optional address
               socklen_t     msg_namelen;    // size of address
               struct iovec *msg_iov;        // scatter/gather array
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
#include "egi_unet.h"
#include "egi_timer.h"

static void* userv_listen_thread(void *arg);

/*-----------------------------------------------------
1. Create an AF_UNIX socket sever for local communication.
and type of socket is SOCK_STREAM.
2. Start a thread to listen and accept cliens.

@svrpath:	userver path. (Must be full path!)

Return:
	0	OK
	<0	Fails
------------------------------------------------------*/
EGI_USERV* unet_create_Userver(const char *svrpath)
{
	EGI_USERV *userv=NULL;

        /* 1. Calloc EGI_USERV */
        userv=calloc(1,sizeof(EGI_USERV));
        if(userv==NULL)
                return NULL;

	/* 2. Create socket  */
	userv->sockfd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0);
	if(userv->sockfd <0) {
		printf("%s: Fail to create socket! Err'%s'.\n", __func__, strerror(errno));
		free(userv);
		return NULL;
	}

	/* 3. If old path exists, remove it */
	if( access(svrpath,F_OK) ==0 ) {
		if( unlink(svrpath)!=0 ) {
			printf("%s: Fail to remove old svrpath! Err'%s'.\n", __func__, strerror(errno));
			goto END_FUNC;
		}
	}

	/* 4. Prepare address struct */
	memset(&userv->addrSERV, 0, sizeof(typeof(userv->addrSERV)));
	userv->addrSERV.sun_family = AF_UNIX;
	strncpy(userv->addrSERV.sun_path, svrpath, sizeof(userv->addrSERV.sun_path)-1);

	/* 5. Bind address */
	if( bind(userv->sockfd, (struct sockaddr *)&userv->addrSERV, sizeof(typeof(userv->addrSERV))) !=0 ) {
		printf("%s: Fail to bind address for sockfd! Err'%s'.\n", __func__, strerror(errno));
		goto END_FUNC;
	}

	/* 6. Set backlog */
	userv->backlog=MAX_UNET_BACKLOG;

	/* 7. Start listen/accept thread */
	if( pthread_create(&userv->acpthread, NULL, (void *)userv_listen_thread, userv) !=0 ) {
		printf("%s: Fail to launch userv->acpthread! Err'%s'.\n", __func__, strerror(errno));
		goto END_FUNC;
	}

#if 0	/* DO NOT!!! Detach the thread */
	if(pthread_detach(userv->acpthread)!=0) {
        	printf("%s: Fail to detach userv->acpthread! Err'%s'.\n", __func__, strerror(errno));
	        //go on...
	} else {
		printf("%s: OK! Userv->acpthread detached!\n", __func__);
	}
#endif

	/* 8. OK */
	return userv;

END_FUNC: /* Fails */
	if( access(svrpath,F_OK) ==0 ) {
		if( unlink(svrpath)!=0 )
			printf("%s: Fail to remove old svrpath! Err'%s'.\n", __func__, strerror(errno));
	}
	close(userv->sockfd);
        free(userv);
        return NULL;
}


/*---------------- A thread function -------------
Start EGI_USERV listen and accept thread.
Disconnected clients will NOT be deteced here!

@arg	Pointer to an EGI_USERV.

Return:
	0	OK
	<0	Fails
-----------------------------------------------*/
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
                printf("%s: Fail to listen() sockfd, Err'%s'.\n", __func__, strerror(errno));
                return (void *)-2;
        }

	/* 2. Accepting clients ... */
	userv->acpthread_on=true;
	while( userv->cmd !=1 ) {

		/* 2.1 Re_count clients, and get an available userv->sessions[] for next new client. */
		userv->ccnt=0;
		index=-1;
		for(i=0; i<MAX_UNET_CLIENTS; i++) {
			if( index<0 && userv->sessions[i].alive==false )
				index=i;  /* availble index for next session */
			if( userv->sessions[i].alive==true )
				userv->ccnt++;
		}
		printf("%s: Recount active sessions, userv->ccnt=%d\n", __func__, userv->ccnt);

		/* 2.1 Check Max. clients */
		if(index<0) {
			printf("%s: Clients number hits Max. limit of %d!\n",__func__, MAX_UNET_CLIENTS);
			tm_delayms(500);
			continue;
		}

		/* 2.3 Accept clients */
		printf("%s: accept() waiting...\n", __func__);
		addrLen=sizeof(addrCLIT);
		//csfd=accept4(userv->sockfd, (struct sockaddr *)&addrCLIT, &addrLen, SOCK_NONBLOCK|SOCK_CLOEXEC);
		csfd=accept(userv->sockfd, (struct sockaddr *)&addrCLIT, &addrLen);
		if(csfd<0) {
			switch(errno) {
                                #if(EWOULDBLOCK!=EAGAIN)
                                case EWOULDBLOCK:
                                #endif
				case EAGAIN:
					//tm_delayms(10); /* if NONBLOCKING */
					continue;  /* ---> continue to while() */
					break;
				case EINTR:
					printf("%s: accept() interrupted! errno=%d Err'%s'.  continue...\n",
										__func__, errno, strerror(errno));
					continue;
					break;
				default:
					printf("%s: Fail to accept a new client, errno=%d Err'%s'.  continue...\n",
												__func__, errno, strerror(errno));
					//tm_delayms(20); /* if NONBLOCKING */
					/* TODO: End routine if it's a deadly failure!  */
					continue;  /* ---> whatever, continue to while() */
			}
		}

		/* 2.4 Proceed to add a new client ...  */
		userv->sessions[index].sessionID=index;
		userv->sessions[index].csFD=csfd;
		userv->sessions[index].addrCLIT=addrCLIT;
		userv->sessions[index].alive=true;
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

	/* Joint acpthread */
	(*userv)->cmd=1;  /* CMD to end thread */
	if( pthread_join((*userv)->acpthread, NULL)!=0 ) {
		printf("%s: Fail to join acpthread!,  Err'%s'\n", __func__, strerror(errno));
		// Go on...
	}

        /* Make sure all sessions/clients have been safely closed/disconnected! */

	/* Close session fd */
        for(i=0; i<MAX_UNET_CLIENTS; i++) {
	        if( (*userv)->sessions[i].alive==true )
			close((*userv)->sessions[i].csFD);
	}

        /* Close main sockfd */
        if(close((*userv)->sockfd)<0) {
                printf("%s: Fail to close sockfd, Err'%s'\n", __func__, strerror(errno));
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
		printf("%s: Fail to create socket! Err'%s'.\n", __func__, strerror(errno));
		free(uclit);
		return NULL;
	}

	/* Prepare address struct */
	memset(&uclit->addrSERV, 0, sizeof(typeof(uclit->addrSERV)));
	uclit->addrSERV.sun_family = AF_LOCAL;
	strncpy(uclit->addrSERV.sun_path, svrpath, sizeof(uclit->addrSERV.sun_path)-1);

	/* Connect to the server */
	if( connect(uclit->sockfd, (struct sockaddr *)&uclit->addrSERV, sizeof(typeof(uclit->addrSERV))) !=0 ) {
		printf("%s: Fail to connect to the server! Err'%s'.\n", __func__, strerror(errno));
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
                printf("%s: Fail to close sockfd, Err'%s'\n", __func__, strerror(errno));
                return -1;
        }

        /* Free mem */
        free(*uclit);
        *uclit=NULL;

        return 0;
}
