/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A module for SURFACE, SURFMAN, SURFUSER.

Midas Zhou
midaszhou@yahoo.com
---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/select.h>
#include "egi_unet.h"
#include "egi_timer.h"
#include "egi_debug.h"
#include "egi_surface.h"


static void* surfman_request_process_thread(void *arg);

/*--------------------------------------------------------
Create an anonymous file(as anonymous memory) and ftruncate
its size.

@name:	  Name of the anonymous file.
@memsize: Inital size of the file

Reutrn:
	>0	OK, the file descriptor that refers to the
		anonymous memory.
	<0	Fails
--------------------------------------------------------*/
int egi_create_memfd(const char *name, size_t memsize)
{
	int memfd;

        /* Create memfd, the file descriptor is open with (O_RDWR) and O_LARGEFILE. */
#ifdef  SYSCALL_MEMFD_CREATE /* see egi_unet.h */
        memfd=syscall(SYS_memfd_create, name, 0); /* MFD_ALLOW_SEALING , MFD_CLOEXEC */
#else
	memfd=memfd_create(name,0); /* MFD_ALLOW_SEALING , MFD_CLOEXEC */
#endif
        if(memfd<0) {
                egi_dperr("Fail to create memfd!");
		return -1;
        }

        if( ftruncate(memfd, memsize) <0 ) {
		egi_dperr("ftruncate");
		close(memfd);
		return -2;
	}
//      fsync(memfd);

	return memfd;
}

/*-------------------------------------------------
Mmap fd to get pointer to an EGI_SURFACE

Note: memfd NOT close here!

@memfd:   A file descriptor created by memfd_create.

Reutrn:
	A pointer to EGI_SURFACE	OK
	NULL				Fails
--------------------------------------------------*/
EGI_SURFACE *egi_mmap_surface(int memfd)
{
	EGI_SURFACE *eface=NULL;
        struct stat sb;

        if (fstat(memfd, &sb)<0) {
                //printf("%s: fstat Err'%s'.\n", __func__, strerror(errno));
		egi_dperr("fstat");
                return NULL;
        }

        eface=mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, 0);
        if(eface==MAP_FAILED) {
                //printf("%s: Fail to mmap memfd! Err'%s'.\n", __func__, strerror(errno));
		egi_dperr("Fail to mmap memfd!");
                return NULL;
        }

	return eface;
}

/*------------------------------------------
Unmap an EGI_SURFACE.

@eface:  Pointer to EGI_SURFACE.

Return:
	0	OK
	<0	Fails
------------------------------------------*/
int egi_munmap_surface(EGI_SURFACE **eface)
{
	if(eface==NULL|| (*eface)==NULL)
		return 0;

	if( *eface==MAP_FAILED )
		return 0;

        /* Unmap surface */
        if( munmap(*eface, (*eface)->memsize)!=0 ) {
        	//printf("%s: Fail to unmap eface!\n",__func__);
		egi_dperr("Fail to unmap eface!");
		return -1;
	}

	/* Reset *eface */
	*eface = NULL;

	return 0;
}

/*---------------------------------------------------------
Request a surface from the Surfman via local socket.
Return a surface pointer to shared memory space.

Note:
1. It will be BLOCKED while waiting for reply.
2. DO NOT forget to munmap it after use!

@sockfd    A valid UNIX SOCK_STREAM type socket, connected
	   to an EGI_SURFACE server.
	   Assume to be BLOCKING type.
@x0,y0	   Initial origin of the surface.
@w,h	   Width and height of a surface.
@pixsize   Pixel size. as of 16bits/24bits/32bits color.
	   Also including alpha size!
Return:
	A pointer to EGI_SURFACE	OK
	NULL				Fails
----------------------------------------------------------*/
EGI_SURFACE *ering_request_surface(int sockfd, int x0, int y0, int w, int h, int pixsize)
{
	EGI_SURFACE *eface;

	struct msghdr msg={0};
	struct cmsghdr *cmsg;
	int data[16]={0};
	struct iovec iov={.iov_base=data, .iov_len=sizeof(data) };
	int memfd;

	/* Check input */
	if( w<1 || h<1 || pixsize<1 )
		return NULL;

	/* 1. Prepare request params, put in data[] */
	memset(data,0,sizeof(data));
	data[0]=ERING_REQUEST_ESURFACE;
	data[1]=x0; data[2]=y0;
	data[3]=w;  data[4]=h;
	data[5]=pixsize;

	/* 2. MSG iov data  */
	msg.msg_iov=&iov;  /* iov holds data */
	msg.msg_iovlen=1;

	/* 3. Send msg to server */
	if( unet_sendmsg(sockfd, &msg) <=0 ) {
		//printf("%s: unet_sendmsg() fails!\n", __func__);
		egi_dpstd("unet_sendmsg() fails!\n");
		return NULL;
	}

	/* 4. Prepare MSG buffer, to receive memfd */
	union {
	  /* Wrapped in a union in ordert o ensure it's suitably aligned */
		char buf[CMSG_SPACE(sizeof(memfd))]; /* CMSG_SPACE(): space with header and required alignment. */
		struct cmsghdr align;
	} u;
        msg.msg_control = u.buf;
        msg.msg_controllen = sizeof(u.buf);

	/* 5. Wait to receive msg */
	egi_dpstd("Request msg sent out, wait for reply...\n");
	if( unet_recvmsg(sockfd, &msg) <=0 ) {
		egi_dpstd("unet_recvmsg() fails!\n");
		return NULL;
	}

	/* 6. Check request result */
	if( data[0]!=ERING_RESULT_OK ) {
		switch(data[0]) {
			case ERING_RESULT_ERR:
				egi_dpstd("Request fails for 'ERING_RESULT_ERR'!\n");
				break;
			case ERING_MAX_LIMIT:
				egi_dpstd("Request fails for 'ERING_MAX_LIMIT'!\n");
				break;
		}
		return NULL;
	}

	/* 7. Get returned memfd */
	cmsg = CMSG_FIRSTHDR(&msg);
	memfd = *(int *)CMSG_DATA(cmsg);
	//memcpy(&memfd, (int *)CMSG_DATA(cmsg), n*sizeof(int));

	egi_dpstd("get memfd=%d\n", memfd);

  	/* 8. Mmap memfd to get pointer to EGI_SURFACE */
	eface=egi_mmap_surface(memfd);
	if(eface==NULL) {
		close(memfd);
		return NULL;
	}

	/* 9. Close memfd */
	if(close(memfd)<0) {
		//printf("Fail to close memfd, Err'%s'.!\n", strerror(errno));
		egi_dperr("Fail to close memfd.");
	}

	/* OK */
	return eface;
}


/*---------------- A thread function -------------
Start EGI_SURFMAN request_process thread.

Disconnected clients will NOT be detected here!

@arg	Pointer to an EGI_SURFMAN.

Return:
	0	OK
	<0	Fails
------------------------------------------------*/
static void* surfman_request_process_thread(void *arg)
{
	int i;  	/* USERV_MAX_CLIENTS */
//	int k;		/* SURFMAN_MAX_SURFACES */
	int rfds;
	int csFD;
	int maxfd;
	fd_set set_uclits;
	struct timeval tm;
	int nrcv;
	//int nsnd;

	int memfd;
	//size_t memsize;
	int sfID; /* surface id, as index of surfman->surface[] */

	/* Surface request MSG buffer */
	struct msghdr msg={0};
	struct cmsghdr *cmsg;
	int data[16]={0};
	struct iovec iov={.iov_base=&data, .iov_len=sizeof(data) };

	/* Prepare MSG_control buffer: msg.msg_control = u.buf */
	union {
	  /* Wrapped in a union in ordert o ensure it's suitably aligned */
		char buf[CMSG_SPACE(sizeof(memfd))]; /* CMSG_SPACE(): space with header and required alignment. */
		struct cmsghdr align;
	} u;

	EGI_SURFMAN *surfman=(EGI_SURFMAN *)arg;
	if(surfman==NULL || surfman->userv==NULL)
		return (void *)-1;

	EGI_USERV_SESSION *sessions=surfman->userv->sessions;
	if(sessions==NULL)
		return (void *)-2;

	/* Set select wait timeout */
	tm.tv_sec=0;
	tm.tv_usec=100000;

	/* Enter routine */
	egi_dpstd("Start routine job...\n");
	surfman->repthread_on=true;
	while(1) {

		/* If there is no surfuser connected */
		if( surfman->userv->ccnt==0) {
			usleep(50000);
			continue;
		}

		/* Add all clients to select set */
		maxfd=0;
		FD_ZERO(&set_uclits);
		for(i=0; i<USERV_MAX_CLIENTS; i++) {
			//if( surfman->userv->sessions[i].alive ) { //csFD>0 )
			if( sessions[i].alive ) {
				csFD=sessions[i].csFD;
				//csFD=surfman->userv->sessions[i].csFD;
				FD_SET(csFD, &set_uclits);
				if( csFD > maxfd )
					maxfd=csFD;
			}
		}
		//egi_dpstd("maxfd=%d\n", maxfd);

		/* select readable fds, all fds to be NONBLOCKING. */
		rfds=select(maxfd+1, &set_uclits, NULL, NULL, &tm);
		/* Case 1: No readbale fds, No request */
		if(rfds==0) {
			//egi_dpstd("select() rfd=0!\n");
			usleep(50000);
			continue;
		}
		/* Case 2: Error occurs */
		else if(rfds<0) {
			egi_dperr("select() rfds<0");
			continue; /* --- continue anyway --- */
		}
		/* Case 3: Readable fds, Request from cliets */
		else if(rfds>0) {
			egi_dpstd("select() rfd=%d\n", rfds);

			/* Traverse and check select set. */
			for(i=0; i<USERV_MAX_CLIENTS; i++) {
				//csFD=surfman->userv->sessions[i].csFD;
				csFD=sessions[i].csFD;
				if( csFD>0 && FD_ISSET(csFD, &set_uclits) ) {
					egi_dpstd("Request MSG from uclient ('%s'), csFD=%d,\n",
									//surfman->userv->sessions[i].addrCLIT.sun_path, csFD);
									sessions[i].addrCLIT.sun_path, csFD);

/* >>>--- ( Processing Surface Request ) --------------------- */

					/* Prepare MSG iov data buffer for recvmsg */
					memset(data,0,sizeof(data));
					msg.msg_iov=&iov;  /* iov holds data */
					msg.msg_iovlen=1;

					/* Receive request msg from selected socket */
					nrcv=unet_recvmsg(csFD, &msg);
					/* Case A: Client ends session */
					if( nrcv==0  ) {  // xxx&& rfds==1 ) {
						egi_dpstd("userv->sessions[%d] ends by counter peer '%s'!\n",
									i, sessions[i].addrCLIT.sun_path );

						/* Unregister related surfuser(socket session) ! */
						surfman_unregister_surfUser(surfman, i); /* i as sessionID */
					}
					/* Case B: Error ocurrs */
					else if(nrcv<0) {
						egi_dperr("unet_recvmsg() from csFD=%d fails!", csFD);
					}
					/* Case C: Msg from client */
					else { /* nrcv > 0 */

						/* Check request type */
						if( data[0] != ERING_REQUEST_ESURFACE ) {
							egi_dpstd("ering_request_type =%d! Can not handle it.\n", data[0]);
						}
						else {

  /* >>>--- Register an EGI_SURFACE  ------- */
	egi_dpstd("Request for an surface of %dx%dx%dbpp, origin at (%d,%d)\n",
			data[3], data[4], data[5], data[1], data[2]);		/* W,H,pixsize, x0,y0 */

	/* Register an surface to surfman */
	sfID=surfman_register_surface( surfman, i, data[1], data[2], data[3], data[4], data[5] ); /* surfman, userID, x0, y0, w, h, pixsize */
	if(sfID<0) {
		egi_dpstd("surfman_register_surface() fails!\n");
		if(surfman->scnt==SURFMAN_MAX_SURFACES)
			data[0]=ERING_MAX_LIMIT;
		else
	                data[0]=ERING_RESULT_ERR;
                unet_sendmsg(sessions[i].csFD, &msg);
                continue; /* ----> GOTO traverse select list: for(i=0; i<USERV_MAX_CLIENTS...) */
	}

  /* >>>--- END register_surface  ------- */

	/* Get underlying memfd of the surface */
	memfd=surfman->surfaces[sfID]->memfd;

	/* Prepare replay data: MSG iov data */
	memset(data, 0, sizeof(data));
	data[0]=ERING_RESULT_OK;
	msg.msg_iov=&iov;
	msg.msg_iovlen=1;

	/* Prepare replay data: MSG control, to contain memfd. */
	msg.msg_control = u.buf;
	msg.msg_controllen = sizeof(u.buf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level= SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;	/* For file descriptor transfer. */
	cmsg->cmsg_len	= CMSG_LEN(sizeof(memfd)); /* cmsg_len: with required alignment, take cmsg_data length as argment. */

	/* Put memfd to cmsg_data */
	// *(int *)CMSG_DATA(cmsg)=memfd; /* !!! CMSG_DATA() is constant ??!!! */
	memcpy((int *)CMSG_DATA(cmsg), &memfd, sizeof(memfd));

	/* Reply to surface user. */
	if( unet_sendmsg(sessions[i].csFD, &msg) <=0 ) {
		egi_dpstd("unet_sendmsg() fails!\n");
		surfman_unregister_surface( surfman, sfID ); /* unregister the surface */
		// TODO: ... suspend or destroy the surface ...
	}

						} /* --- else */

					} /* END Case C */

/* <<<--- ( End Request Pricessing ) --------------------- */

				}
			} /* END for() */
		} /* END Case 3 */

	} /* END while() */


	return (void *)0;
}


/*-------------------------------------------------
Create an EGI_SURFMAN (surface manager).

@svrpath:	Path of the surfman.

Return:
	A pointer to EGI_SURFACE	OK
	NULL				Fails
--------------------------------------------------*/
EGI_SURFMAN *egi_create_surfman(const char *svrpath)
{
	EGI_SURFMAN *surfman=NULL;

	/* 1. Allocate surfman */
	surfman=calloc(1, sizeof(EGI_SURFMAN));
	if(surfman==NULL) {
		egi_dperr("calloc surfman");
		return NULL;
	}

	/* 2. Create an EGI_USERV */
	surfman->userv=unet_create_Userver(svrpath);
	if(surfman->userv==NULL) {
		free(surfman);
		return NULL;
	}
	egi_dpstd("Surfman userv_listen_thread starts!\n");
	/* NOW:  userv_listen_thread() starts. */

	/* 3. Start surfman_request_process_thread() */
        if( pthread_create(&surfman->repthread, NULL, (void *)surfman_request_process_thread, surfman) !=0 ) {
                egi_dperr("Fail to launch surfman->repthread to process request!");
		unet_destroy_Userver(&surfman->userv);
		free(surfman);
		return NULL;
        }
	egi_dpstd("Surfman request_process_thread starts!\n");

	/* OK */
	return surfman;
}


/*--------------------------------------------------
Destroy an EGI_SURFMAN.

@surfman	Pointer to a pointer to an EGI_SURFMAN.

Return:
	0	OK
	<0	Fail in some way.
---------------------------------------------------*/
int egi_destroy_surfman(EGI_SURFMAN **surfman)
{
	int i;
	int ret=0;

	if(surfman==NULL || *surfman==NULL)
		return 0;

	/* 1. Join repthread */
	(*surfman)->cmd=1; /* CMD to end thread */
	if( pthread_join( (*surfman)->repthread, NULL)!=0) {
		egi_dperr("Fail to join repthread");
		ret += -1;
		//Go on...
	}

	/* 2. Destroy userver */
	/* Make sure all sessions/clients have been safely closed/disconnected! */
	if( unet_destroy_Userver(&(*surfman)->userv)!=0 ) {
		egi_dpstd("Fail to destroy userv!\n");
		ret += -(1<<1);
		//Go on...
	}

	/* 3. Unmap all surfaces */
	/* Make sure all surfaces have no user any more.. */
//	EGI_SURFACE *eface;
	for(i=0; i<SURFMAN_MAX_SURFACES; i++) {
		if( (*surfman)->surfaces[i] ) {
			//eface=(*surfman)->surfaces[i];
			if( egi_munmap_surface(&((*surfman)->surfaces[i])) !=0 ) {
				egi_dpstd("Fail to unmap surfman->surface[%d]!\n", i);
				ret += -(1<<2);
				//Go on...
			}
		}
	}

	return ret;
}


/*---------------------------------------------------------------------
Create and register a surface into surfman->surfaces[] for the surfuser
as specified by userID.

@surfman	Pointer to a pointer to an EGI_SURFMAN.
@userID		SurfuserID == SessionID of related surfman->userv->session[].
@x0,y0     	Initial origin of the surface.
@w,h       	Width and height of a surface.
@pixsize   	Pixel size. as of 16bits/24bits/32bits color.
		Also including alpha size!
Return:
	>=0	OK, Index of the surfman->surfaces[]
	<0	Fails
----------------------------------------------------------------------*/
int surfman_register_surface( EGI_SURFMAN *surfman, int userID,
			      int x0, int y0, int w, int h, int pixsize )
{
	size_t memsize;
	int memfd;
	EGI_SURFACE *eface=NULL;
	int k;

	/* Check input */
	if(surfman==NULL)
		return -1;
	if( userID > USERV_MAX_CLIENTS-1  || userID<0 )
		return -1;
	if( w<1 || h<1 || pixsize<1 )
		return -1;

	/* Confirm sessionID */
	if( surfman->userv->sessions[userID].alive==false ) {
		egi_dpstd("userv->sessions[%d] is NOT alive!\n", userID);
		return -2;
	}

	/* Check MAX SURFACES limit */
	if(surfman->scnt == SURFMAN_MAX_SURFACES) {
		egi_dpstd("Fail to create surface due to limit of SURFMAN_MAX_SURFACES!\n");
		return -3;
	}

	/* 1. Create memfd, the file descriptor is open with (O_RDWR) and O_LARGEFILE. */
	memsize=sizeof(EGI_SURFACE)+w*h*pixsize; /* 1bytes for alpha */
	memfd=egi_create_memfd("eface_test", memsize);
	if(memfd<0) {
		egi_dpstd("Fail to create memfd!\n");
		return -4;
	}

	/* 2. Mmap to get an EGI_SURFACE */
	eface=egi_mmap_surface(memfd);
	if(eface==NULL) {
		egi_dpstd("Fail to mmap surface!\n");
		close(memfd);
		return -5;
	}
	/* Do NOT close(memfd) here! */

	/* 4. Register to surfman->surfaces[] */
	for(k=0; k<SURFMAN_MAX_SURFACES; k++) {
		if( surfman->surfaces[k]==NULL ) {
			surfman->surfaces[k]=eface;
			surfman->scnt +=1; /* increase surfaces counter */
			egi_dpstd("\t\t----- (+)surfaces[%d], scnt=%d -----\n", k, surfman->scnt);

			break;
		}
	}
	/* Double check */
	if(k==SURFMAN_MAX_SURFACES) {
		egi_dpstd("OOOPS! k==SURFMAN_MAX_SURFACES!");
		egi_munmap_surface(&eface);
		close(memfd);
		return -6;
	}

	/* 5. Assign ID and other members */
	eface->id=k;		 /* <--- ID */
	eface->csFD=surfman->userv->sessions[userID].csFD;  /* <--- csFD / surf_userID */
	eface->memfd=memfd;
	eface->memsize=memsize;
	eface->x0=x0;
	eface->y0=y0;
	eface->width=w;
	eface->height=h;
	eface->pixsize=pixsize;

	/* OK */
	return k;
}

/*----------------------------------------------------------------
Unregister/unmap a surface from surfman->surfaces[].

@surfman        Pointer to a pointer to an EGI_SURFMAN.
@surfID         Surface ID as of surfman->surfaces[surfID]

Return:
        0     	OK
        <0      Fails
----------------------------------------------------------------*/
int surfman_unregister_surface( EGI_SURFMAN *surfman, int surfID )
{
	EGI_SURFACE *eface=NULL;

	/* Check input */
	if(surfman==NULL)
		return -1;
	if(surfID <0 || surfID > SURFMAN_MAX_SURFACES-1 )
		return -2;
	if(surfman->surfaces[surfID]==NULL)
		return -3;

	/* Close memfd */
        if(close(surfman->surfaces[surfID]->memfd)<0) {
                egi_dperr("close(memfd)");
		return -4;
	}

	/* Munmap surface */
	eface=surfman->surfaces[surfID];
	if( egi_munmap_surface(&eface) !=0 ) {
		egi_dpstd("Fail to munamp surface[%d]!\n", surfID);
		return -5;
	}
	surfman->surfaces[surfID]=NULL; /* !!! */

	/* Update surfman data */
	//surfman->surfaces[surfID]=NULL;
	surfman->scnt -=1;
	egi_dpstd("\t\t----- (-)surfaces[%d], scnt=%d -----\n", surfID, surfman->scnt);

	return 0;
}


/*-------------------------------------------------------------------
Unregister a surface user in a surfman. by deleting  sessions[i].csFD
and all related surfman->surfaces[]

Note:
1. The session[i].csFD MAY has no surfaces registered!

@surfman	Pointer to a pointer to an EGI_SURFMAN.
@sessionID	SessionID of related surfman->userv->session[].

Return:
	>=0	OK, number of surfaces unregistered.
	<0	Fails
------------------------------------------------------------------*/
int surfman_unregister_surfUser(EGI_SURFMAN *surfman, int sessionID)
{
	int k;
	int cnt;
	int csFD;
	EGI_SURFACE *eface=NULL;

	if(surfman==NULL)
		return -1;
	if(sessionID<0 || sessionID > USERV_MAX_CLIENTS-1)
		return -2;

	//NOTE: surfman->scnt MAYBE <1;

	/* 1. Get related csFD */
	csFD=surfman->userv->sessions[sessionID].csFD;

	/* 2. unmap all related surfaces */
	cnt=0;
	for(k=0; k<SURFMAN_MAX_SURFACES; k++) {
		if( surfman->surfaces[k]!=NULL && surfman->surfaces[k]->csFD == csFD ) {

			/* Unregister surfaces, maybe more than 1! */
		 	/* TODO: Following to be replaced by surfman_unregister_surface() */

			/* Close memfd */
		        if(close(surfman->surfaces[k]->memfd)<0) {
                		egi_dperr("close(memfd)");
		                return -4;
        		}
			/* Unmap related surfaces */
			eface=surfman->surfaces[k];
			if( egi_munmap_surface(&eface)==0 ) {
				/* !!! */
				surfman->surfaces[k]=NULL;
				/* Update counter */
				surfman->scnt -=1;
		        	egi_dpstd("\t\t----- (-)surfaces[%d], scnt=%d -----\n", k, surfman->scnt);
				cnt++;
			}
			else
				egi_dpstd("Fail to unmap surfman->surfaces[%d]!\n", k);
		}
	}
	if(cnt==0)
		egi_dpstd("OOOPS! No surface related to surfUser: userv->sessions[%d].\n", sessionID);

	/* TODO: mutex_lock for ccnt/sessions[] */

	/* 3. close csFD */
	if( close(csFD)!=0 )
		egi_dperr("close(csFD) falis.");

	/* 4. Remove session socket and decrease counter */
	surfman->userv->sessions[sessionID].csFD = 0;
	surfman->userv->sessions[sessionID].alive = false;
	surfman->userv->ccnt -= 1;
       	egi_dpstd("\t\t----- (-)userv->sessions[%d], ccnt=%d -----\n", sessionID, surfman->userv->ccnt);

	return cnt;
}

