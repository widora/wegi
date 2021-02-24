/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A module for SURFACE, SURFMAN, SURFUSER.

Note:
1. Anonymous shared memory leakage/not_freed??  Busybox: ipcs ipcrm


Jurnal
2021-02-22:
	1. Add surfman_render_thread().
	2. Apply surfman_mutex.
2021-02-23:
	1. Add surface_compare_zseq() and surface_insertSort_zseq().
2021-02-24:
	1. Add EGI_SURFUSER, egi_register_surfuser()/egi_unregister_surfuser()

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
#include "egi_fbgeom.h"
#include "egi_image.h"
#include "egi_math.h"

static void* surfman_request_process_thread(void *surfman);
static void * surfman_render_thread(void *surfman);

/*** NOTE: To keep consistent with definition of SURF_COLOR_TYPE! */
static int surf_colortype_pixsize[]=
{
	[SURF_RGB565] 		=2,
	[SURF_RGB565_A8] 	=3,
        [SURF_RGB888] 		=3,
        [SURF_RGB888_A8]	=4,
        [SURF_RGBA8888]		=4,
	[SURF_COLOR_MAX]	=4,
};
/*-------------------------------------------------
Get pixel size as per the color type. Pixel size is
the sum of RBG_size and ALPHA_size.
In case colorType outof range, then return 4 as MAX!
--------------------------------------------------*/
int surf_get_pixsize(SURF_COLOR_TYPE colorType)
{
	if( colorType >= SURF_COLOR_MAX )  /* In case... */
		return surf_colortype_pixsize[SURF_COLOR_MAX];
	else
		return surf_colortype_pixsize[colorType];
}

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


/*---------------------------------------------------
Compare surfaces with surface->zseq value.

A NULL surface always has Min. zseq value!

Return:
        Relative Priority Sequence position:
        -1 (<0)   eface1->zseq < eface2->zseq
         0 (=0)   eface1->zseq = eface2->zseq
         1 (>0)   eface1->zset > eface2->zseq
---------------------------------------------------*/
int surface_compare_zseq(const EGI_SURFACE *eface1, const EGI_SURFACE *eface2)
{
	/* Check NULL surfaces */
	if( eface1 == NULL && eface2 == NULL )
		return 0;
	else if( eface1 == NULL )
		return -1;
	else if( eface2 == NULL )
		return 1;

	/* NOW: eface1 != NULL && eface2 != NULL */
	if( eface1->zseq < eface2->zseq )
		return -1;
	else if( eface1->zseq == eface2->zseq)
		return 0;
	else /* eface1->zseq > eface2->zseq */
		return 1;
}


/*----------------------------------------------------------------
Sort an EGI_SURFACE array by Insertion Sort algorithm, to rearrange
surfaces in ascending order of surface->zseq!
Then re_assign id AND serial zseq for all surfaces!

Note:
1. The caller MUST enusre surfaces array has at least n memmbers!
2. NULL surface to be rearranged at start of the array.

@surfaces:	An array of surface pointers.
@n:		Size of the array. n>1!

------------------------------------------------------------------*/
void surface_insertSort_zseq(EGI_SURFACE **surfaces, int n)
{
	int i,k;
	int maxzseq;
	EGI_SURFACE *tmp;

	if( surfaces==NULL || n==1)
		return;

	for(i=1; i<n; i++) {
		tmp=surfaces[i];
		for(k=i; k>0 && surface_compare_zseq(surfaces[k-1], tmp)>0 ; k--) {
			surfaces[k]=surfaces[k-1]; /* swap nearby */
		}

		/* Settle the inserting surface point at last swapped place */
		surfaces[k]=tmp;
	}


///// NOTE: if the function is applied for quickSort, then following MUST NOT be included /////////

	/* Get maxzseq */
	maxzseq=0;
	for(i=0; i< SURFMAN_MAX_SURFACES; i++)
		if(surfaces[i]) maxzseq++;

	/* Reset all surface->id as index of surfman->surfaces[], Reset all zseq*/
	for(i=SURFMAN_MAX_SURFACES-1; i>=0; i--) {
		if(surfaces[i]==NULL) {
			break;
		}
		else {
			surfaces[i]->id=i;
			surfaces[i]->zseq = maxzseq--;
		}
	}
}


/*-------------------------------------------------------------
Register/create an EGI_SURFUSER from an EGI_SURFMAN via svrpath.

TODO: NOW, Support ColorType SURF_RGB565 and SURF_RGB565_A8.

@svrpath:	Path to an EGI_SURFMAN.
@x0,y0:		Surface position relative to FB
@w,h:		Surface width and height
@colorType:	Surface color data type

Return:
	A pointer to EGI_SURFUSER	OK
	NULL				Fails
-------------------------------------------------------------*/
EGI_SURFUSER *egi_register_surfuser( const char *svrpath,
				   int x0, int y0, int w, int h, SURF_COLOR_TYPE colorType )
{
	EGI_SURFUSER *surfuser;

	/* Check input */
	if( colorType != SURF_RGB565 && colorType != SURF_RGB565_A8 ) {
		egi_dpstd("NOW Only support colorType SURF_RGB565 AND SURF_RGB565_A8!\n");
		return NULL;
	}

	/* 1. Calloc surfuser */
	surfuser=calloc(1, sizeof(EGI_SURFUSER));
	if(surfuser==NULL) {
		return NULL;
	}

	/* 2. Link to EGI_SURFMAN */
	surfuser->uclit=unet_create_Uclient(svrpath);
	if(surfuser->uclit==NULL) {
		egi_dpstd("Fail to create Uclient!\n");
		free(surfuser);
		return NULL;
	}

	/* 3. Request for a surface */
	surfuser->surface=ering_request_surface( surfuser->uclit->sockfd, x0, y0, w, h, colorType);
	if(surfuser->surface==NULL) {
		egi_dpstd("Fail to request surface!\n");
		unet_destroy_Uclient(&surfuser->uclit);
		free(surfuser);
		return NULL;
	}

	/* 4. Allocate an EGI_IMGBUF */
	surfuser->imgbuf=egi_imgbuf_alloc();
	if(surfuser->imgbuf==NULL) {
		egi_dpstd("Fail to allocate imgbuf!\n");
		egi_munmap_surface(&surfuser->surface);
		unet_destroy_Uclient(&surfuser->uclit);
		free(surfuser);
		return NULL;
	}

	/* 5. Map surface data to imgbuf */
	surfuser->imgbuf->width = surfuser->surface->width;
	surfuser->imgbuf->height = surfuser->surface->height;
	surfuser->imgbuf->imgbuf = (EGI_16BIT_COLOR *)surfuser->surface->color; /* SURF_RGB565 ! */
        if( surfuser->surface->off_alpha >0 )
        	surfuser->imgbuf->alpha = (EGI_8BIT_ALPHA *)(surfuser->surface+surfuser->surface->off_alpha);
        else
                surfuser->imgbuf->alpha = NULL;

	/* 6. Init. virtual FBDEV */
	if( init_virt_fbdev(&surfuser->vfbdev, surfuser->imgbuf) != 0 ) {
		egi_dpstd("Fail to init vfbdev!\n");
		surfuser->imgbuf->imgbuf=NULL; /* Unlink to surface data before free imgbuf */
		surfuser->imgbuf->alpha=NULL;
		egi_imgbuf_free2(&surfuser->imgbuf);
		egi_munmap_surface(&surfuser->surface);
		unet_destroy_Uclient(&surfuser->uclit);
		free(surfuser);
		return NULL;
	}

	/* OK */
	return surfuser;
}


/*---------------------------------------------------------
Unregister/destory an EGI_SURFUSER.

!!! WARNING !!! Unlink imgbuf data before destory the surfuser.

@surfuser:	Pointer to pointer to an EGI_SURFUSER

Return:
	0	OK
	<0	Fails
----------------------------------------------------------*/
int egi_unregister_surfuser(EGI_SURFUSER **surfuser)
{
	int ret=0;

	if(surfuser==NULL || (*surfuser)==NULL)
		return -1;

        /* 1. Release virtual FBDEV */
        release_virt_fbdev(&(*surfuser)->vfbdev);

	/* 2. Unlink imgbuf data and free the imgbuf */
	(*surfuser)->imgbuf->imgbuf=NULL;
	(*surfuser)->imgbuf->alpha=NULL;
	egi_imgbuf_free2(&(*surfuser)->imgbuf);

        /* 3. Unmap surface */
        if( egi_munmap_surface(&(*surfuser)->surface) !=0 )
		ret = -1;

        /* 4. Free uclit */
        if( unet_destroy_Uclient(&(*surfuser)->uclit) !=0 )
		ret += -(1<<1);

        /* 5. Free surfuser */
        free(*surfuser);
	*surfuser=NULL;

	return ret;
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
@colorType	Surface color type
//@pixsize   Pixel size. as of 16bits/24bits/32bits color.
	   Also including alpha size!
Return:
	A pointer to EGI_SURFACE	OK
	NULL				Fails
----------------------------------------------------------*/
EGI_SURFACE *ering_request_surface(int sockfd, int x0, int y0, int w, int h, SURF_COLOR_TYPE colorType)
{
	EGI_SURFACE *eface;

	struct msghdr msg={0};
	struct cmsghdr *cmsg;
	int data[16]={0};
	struct iovec iov={.iov_base=data, .iov_len=sizeof(data) };
	int memfd;

	/* Check input */
	if( w<1 || h<1 )
		return NULL;

	/* 1. Prepare request params, put in data[] */
	memset(data,0,sizeof(data));
	data[0]=ERING_REQUEST_ESURFACE;
	data[1]=x0; data[2]=y0;
	data[3]=w;  data[4]=h;
	data[5]=colorType;

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

//	int pixsize;
	int memfd;
	//size_t memsize;
	int sfID; /* surface id, as index of surfman->surface[] */

	/* Surface request MSG buffer */
	struct msghdr msg={0};
	struct cmsghdr *cmsg;
	int data[16]={0};     /* data[0]:ering_request_type, data[1]:x0, data[2]:y0, data[3]:width, data[4]:height, data[5]:colorTYpe */
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
	while( surfman->cmd != 1 ) {

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
//			egi_dpstd("select() rfd=0!\n");
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
	if( data[5]> SURF_COLOR_MAX-1 )
		egi_dpstd(" !!! WARNING !!! Surface color type out of range!\n");

	egi_dpstd("Request for an surface of %dx%dx%dBpp, origin at (%d,%d)\n",
			data[3], data[4], surf_get_pixsize(data[5]), data[1], data[2]);		/* W,H,pixsize, x0,y0 */

	/* Register an surface to surfman */
	sfID=surfman_register_surface( surfman, i, data[1], data[2], data[3], data[4], data[5] ); /* surfman, userID, x0, y0, w, h, colorType */
	if(sfID<0) {
		egi_dpstd("surfman_register_surface() fails!\n");
		if(surfman->scnt==SURFMAN_MAX_SURFACES)
			data[0]=ERING_MAX_LIMIT;
		else
	                data[0]=ERING_RESULT_ERR;
                unet_sendmsg(sessions[i].csFD, &msg);
                continue; /* ----> GOTO traverse select list: for(i=0; i<USERV_MAX_CLIENTS...) */
	}
	egi_dpstd("surfman->Surface[%d] registered!\n", sfID);

  /* <<<--- END register_surface  ------- */

    /* >>>--- Reply to the surfuser --- */
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
    /* >>>--- END Reply to surfuser --- */

						} /* END else( data[0] == ERING_REQUEST_ESURFACE ) */

					} /* END Case C */

/* <<<--- ( End Request Pricessing ) --------------------- */

				} /* END if( csFD>0 && FD_ISSET(csFD, &set_uclits) ) */

			} /* END for() */
		} /* END Case 3 */

	} /* END while() */

	surfman->repthread_on=false;
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

	/* 2. Init surfman_mutex */
        if(pthread_mutex_init(&surfman->surfman_mutex, NULL) != 0) {
                egi_dperr("Fail to initiate surfman_mutex.");
                free(surfman);
                return NULL;
        }

	/* 3. Create an EGI_USERV */
	surfman->userv=unet_create_Userver(svrpath);
	if(surfman->userv==NULL) {
		pthread_mutex_destroy(&surfman->surfman_mutex);
		free(surfman);
		return NULL;
	}
	egi_dpstd("Surfman userv_listen_thread starts!\n");
	/* NOW:  userv_listen_thread() starts. */

	/* 4. Start surfman_request_process_thread() */
        if( pthread_create(&surfman->repthread, NULL, (void *)surfman_request_process_thread, surfman) !=0 ) {
                egi_dperr("Fail to launch surfman->repthread to process request!");
		unet_destroy_Userver(&surfman->userv);
		pthread_mutex_destroy(&surfman->surfman_mutex);
		free(surfman);
		return NULL;
        }
	egi_dpstd("surfman_request_process_thread starts!\n");

	/* 5. Init FBDEV */
	surfman->fbdev.devname = gv_fb_dev.devname;  /* USE global FBDEV */
	if( init_fbdev(&surfman->fbdev) !=0 ) {
		egi_dpstd("Fail to init_fbdev\n");
		egi_destroy_surfman(&surfman);
		return NULL;
	}
	egi_dpstd("surfman->fbdev is initialized!\n");

	/* 6. Start surfman_render_thread() */
        if( pthread_create(&surfman->renderThread, NULL, (void *)surfman_render_thread, surfman) !=0 ) {
                egi_dperr("Fail to launch surfman->renderThread to render surfaces!");
		egi_destroy_surfman(&surfman);
		return NULL;
        }
	egi_dpstd("surfman_render_thread starts!\n");

	/* OK */
	return surfman;
}


/*--------------------------------------------------
Destroy an EGI_SURFMAN.

!!! Make sure all sessions/clients have been safely closed/disconnected!

@surfman	Pointer to a pointer to an EGI_SURFMAN.

Return:
	0	OK
	<0	Fail in some way.
---------------------------------------------------*/
int egi_destroy_surfman(EGI_SURFMAN **surfman)
{
	int i;
	int ret=0;
	EGI_USERV *userv=NULL;

	if(surfman==NULL || *surfman==NULL)
		return 0;

	/* Get pointer to userv */
	userv=(*surfman)->userv;

	/* 1. Join renderThread */
	(*surfman)->cmd=1;
	if( pthread_join((*surfman)->renderThread, NULL) !=0 ) {
        	egi_dperr("Fail to join renderThread");
                ret += -1;
                //Go on...
        }

	/* 2. Release FBDEV */
	if( (*surfman)->fbdev.map_fb != NULL ) {
		release_fbdev(&(*surfman)->fbdev);
	}

	/* 3. Join repthread */
//	if( (*surfman)->repthread_on ) {  /*  NOPE! If repthread starts but repthread_on not set, it will cause mem leakage then. */
		(*surfman)->cmd=1; /* CMD to end thread */
		egi_dpstd("Join repthread...\n");
		if( pthread_join( (*surfman)->repthread, NULL)!=0) {
			egi_dperr("Fail to join repthread");
			ret += -(1<<2);
			//Go on...
		}
//	}

	/* 4. unregister surfusers: close userv->sessions[].csFD and unmap all related surfaces! */
	for(i=0; i<USERV_MAX_CLIENTS; i++) {
		if( userv->sessions[i].csFD >0 )
			if( surfman_unregister_surfUser(*surfman, i)<0 )
				ret = -( (-ret) | (1<<3) );
	}

	/* 5. Destroy userver */
	if( unet_destroy_Userver(&userv)!=0 ) {
		egi_dpstd("Fail to destroy userv!\n");
		ret += -(1<<4);
		//Go on...
	}

	/* 6. Destroy surfman_mutex */
        pthread_mutex_unlock(&(*surfman)->surfman_mutex); /* Necessary ??? */
        if(pthread_mutex_destroy(&(*surfman)->surfman_mutex) !=0 )
		egi_dperr("Fail to destroy surfman_mutex!");

	/* 6. Free self */
	free( *surfman );
	*surfman=NULL;

	return ret;
}


/*---------------------------------------------------------------------
Create and register a surface into surfman->surfaces[] for the surfuser
as specified by userID.

@surfman	Pointer to a pointer to an EGI_SURFMAN.
@userID		SurfuserID == SessionID of related surfman->userv->session[].
@x0,y0     	Initial origin of the surface.
@w,h       	Width and height of a surface.
@colorType	Surface color type
//@pixsize   	Pixel size. as of 16bits/24bits/32bits color.
		Also including alpha size!

TODO:
1. To deal with more colorType.

Return:
	>=0	OK, Index of the surfman->surfaces[]
	<0	Fails
----------------------------------------------------------------------*/
int surfman_register_surface( EGI_SURFMAN *surfman, int userID,
			      int x0, int y0, int w, int h, SURF_COLOR_TYPE colorType )
{
	size_t memsize;
	int memfd;
	int pixsize;
	EGI_SURFACE *eface=NULL;
	int k;
	int id;

	/* Check input */
	if(surfman==NULL)
		return -1;
	if( userID > USERV_MAX_CLIENTS-1  || userID<0 )
		return -1;
	if( w<1 || h<1 )
		return -1;

        /* Get mutex lock */
	egi_dpstd("Get mutex lock...\n");
        if(pthread_mutex_lock(&surfman->surfman_mutex) !=0 ) {
                return -1;
        }
 /* ------ >>>  Critical Zone  */

	/* Confirm sessionID */
	if( surfman->userv->sessions[userID].alive==false ) {
		egi_dpstd("userv->sessions[%d] is NOT alive!\n", userID);
		pthread_mutex_unlock(&surfman->surfman_mutex);
		return -2;
	}

	/* 1. Check MAX SURFACES limit */
	if(surfman->scnt == SURFMAN_MAX_SURFACES) {
		egi_dpstd("Fail to create surface due to limit of SURFMAN_MAX_SURFACES!\n");
		pthread_mutex_unlock(&surfman->surfman_mutex);
		return -3;
	}

	/* Get pixel size */
	pixsize=surf_get_pixsize(colorType);

	/* 2. Create memfd, the file descriptor is open with (O_RDWR) and O_LARGEFILE. */
	memsize=sizeof(EGI_SURFACE)+w*h*pixsize; /* 1bytes for alpha */
	memfd=egi_create_memfd("eface_test", memsize);
	if(memfd<0) {
		egi_dpstd("Fail to create memfd!\n");
		pthread_mutex_unlock(&surfman->surfman_mutex);
		return -4;
	}

	/* 3. Mmap to get an EGI_SURFACE */
	eface=egi_mmap_surface(memfd);
	if(eface==NULL) {
		egi_dpstd("Fail to mmap surface!\n");
		close(memfd);
		pthread_mutex_unlock(&surfman->surfman_mutex);
		return -5;
	}
	/* Do NOT close(memfd) here! To be closed by surfman_unregister_surface(). */

	/* 4. Register to surfman->surfaces[],  TODO: Move this item code forward!!!  */
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
		pthread_mutex_unlock(&surfman->surfman_mutex);
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
	eface->colorType=colorType;
	//eface->pixsize=pixsize;
	/* Set zseq as scnt */
	eface->zseq=surfman->scnt; /* ==scnt, The lastest surface has the biggest zseq!  All surface.zseq >0!  */

	/* 6. Assign Color/Alpha offset */
	eface->off_color = eface->color - (unsigned char *)eface;
	/* For independent ALPHA data */
	switch( colorType ) {
		case SURF_RGB565_A8:
			eface->off_alpha = eface->off_color + 2*eface->width*eface->height;
			break;
		case SURF_RGB888_A8:
			eface->off_alpha = eface->off_color + 3*eface->width*eface->height;
			break;
		default: /* =0, NOT independent alpha data */
			eface->off_alpha =0;
	}

	/* 7. Sort surfman->surfaces[] in acending order of their zseq value */
	surface_insertSort_zseq(&surfman->surfaces[0], SURFMAN_MAX_SURFACES);

/* TEST: ------------ */
	egi_dpstd("insertSort zseq: ");
	for(k=0; k<SURFMAN_MAX_SURFACES; k++)
		printf(" %d", surfman->surfaces[k] ? surfman->surfaces[k]->zseq : 0);

	/* 8. Get the right id NOW */
	id=eface->id;

/* ------- <<<  Critical Zone  */
        /* put mutex lock for ineimg */
        pthread_mutex_unlock(&surfman->surfman_mutex);

	/* OK */
	return id;
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
	int i;
	EGI_SURFACE *eface=NULL;

	/* Check input */
	if(surfman==NULL)
		return -1;
	if(surfID <0 || surfID > SURFMAN_MAX_SURFACES-1 )
		return -2;

        /* Get mutex lock */
        if(pthread_mutex_lock(&surfman->surfman_mutex) !=0 ) {
                return -1;
        }
 /* ------ >>>  Critical Zone  */

	/* Check */
	if(surfman->surfaces[surfID]==NULL) {
	        pthread_mutex_unlock(&surfman->surfman_mutex);
		return -3;
	}

	/* 1. Close memfd */
        if(close(surfman->surfaces[surfID]->memfd)<0) {
                egi_dperr("close(memfd)");
	        pthread_mutex_unlock(&surfman->surfman_mutex);
		return -4;
	}

	/* 2. Munmap surface */
	eface=surfman->surfaces[surfID];
	if( egi_munmap_surface(&eface) !=0 ) {
		egi_dpstd("Fail to munamp surface[%d]!\n", surfID);
	        pthread_mutex_unlock(&surfman->surfman_mutex);
		return -5;
	}

	/* 3. Reset surfman->surfaces and update scnt */
	surfman->surfaces[surfID]=NULL; /* !!! */
	surfman->scnt -=1;

	/* 4. Sort surfman->surfaces[] in acending order of their zseq value */
	surface_insertSort_zseq(&surfman->surfaces[0], SURFMAN_MAX_SURFACES);

/* TEST: ------------ */
	for(i=0; i<SURFMAN_MAX_SURFACES; i++)
		egi_dpstd(" %d", surfman->surfaces[i] ? surfman->surfaces[i]->zseq : 0);
	printf("\n");

	/* 5. Update surfman data */
	egi_dpstd("\t\t----- (-)surfaces[%d], scnt=%d -----\n", surfID, surfman->scnt);


        /* put mutex lock for ineimg */
        pthread_mutex_unlock(&surfman->surfman_mutex);

	return 0;
}


/*-------------------------------------------------------------------
Unregister a surface user in a surfman. by deleting sessions[i].csFD
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

        /* Get mutex lock */
        if(pthread_mutex_lock(&surfman->surfman_mutex) !=0 ) {
                return -1;
        }
 /* ------ >>>  Critical Zone  */

	/* 1. Get related csFD */
	csFD=surfman->userv->sessions[sessionID].csFD;
	if(csFD <= 0) {
		egi_dpstd("csFD<=0!");
	        pthread_mutex_unlock(&surfman->surfman_mutex);
		return -2;
	}

	/* 2. unmap all related surfaces */
	cnt=0;
	for(k=0; k<SURFMAN_MAX_SURFACES; k++) {
		if( surfman->surfaces[k]!=NULL && surfman->surfaces[k]->csFD == csFD ) {

			/* Unregister surfaces, maybe more than 1! */
		 	/* TODO: Following to be replaced by surfman_unregister_surface() XXX NOPE, double mutex_lock!!! */

			/* Close memfd */
		        if(close(surfman->surfaces[k]->memfd)<0) {
                		egi_dperr("close(memfd)");
			        //pthread_mutex_unlock(&surfman->surfman_mutex);
		                //return -4;  Go on....
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
			else {
				egi_dpstd("Fail to unmap surfman->surfaces[%d]!\n", k);
				//Go on ....
			}
		}
	}
	if(cnt==0)
		egi_dpstd("OOOPS! No surface related to surfUser: userv->sessions[%d].\n", sessionID);


	/* 3. close csFD */
	if( close(csFD)!=0 )
		egi_dperr("close(csFD) falis.");
		//Go on...

	/* 4. Remove session socket and decrease counter */
	surfman->userv->sessions[sessionID].csFD = 0;
	surfman->userv->sessions[sessionID].alive = false;
	surfman->userv->ccnt -= 1;
       	egi_dpstd("\t\t----- (-)userv->sessions[%d], ccnt=%d -----\n", sessionID, surfman->userv->ccnt);

	/* 5. Resort surfman->surfaces[] in acending order of their zseq value */
	surface_insertSort_zseq(&surfman->surfaces[0], SURFMAN_MAX_SURFACES);

/* ------- <<<  Critical Zone  */
        /* put mutex lock for ineimg */
        pthread_mutex_unlock(&surfman->surfman_mutex);

	return cnt;
}

/*-----------------------------------------------
Render surfman->surfaces[] one by one, and bring
them to FB to display.
NOW: only supports SURF_RGB565

TODO: More surfaces[]->colorType support.


-----------------------------------------------*/
static void * surfman_render_thread(void *arg)
{
	int i;
	EGI_IMGBUF *imgbuf=NULL; /* An EGI_IMGBUF to temporarily hold surface color data */
	EGI_SURFACE *surface=NULL;
	EGI_SURFMAN *surfman=(EGI_SURFMAN *)arg;
	if( surfman==NULL )
		return (void *)-1;

	/* To confirm FBDEV open and available */
	if( surfman->fbdev.fbfd <=0 ) {
		egi_dpstd("Surfman fbdev.fbfd is invalid!\n");
		return (void *)-2;
	}

	/* Allocate imgbuf */
	imgbuf=egi_imgbuf_alloc();
	if(imgbuf==NULL) {
		egi_dpstd("Fail to allocate imgbfu!\n");
		return (void *)-4;
	}

	/* Routine of rendering registered surfaces */
	surfman->renderThread_on = true;
	while( surfman->cmd !=1 ) {
		/* Draw backgroud */
		fb_clear_workBuff(&surfman->fbdev, WEGI_COLOR_GRAY2);

		/* Get surfman_mutex ONE BY ONE!  TODO: TBD */
	        if(pthread_mutex_lock(&surfman->surfman_mutex) !=0 ) {
			egi_dperr("Fail to get surfman_mutex!");
               		usleep(1000);
			continue;
       		}
 /* ------ >>>  Critical Zone  */

		/* Draw surfaces: TODO, surface_mutex  */
		//for(i=0; i<SURFMAN_MAX_SURFACES; i++) {
		/* Traverse from surface with bigger zseqs to smller ones */
		for(i=SURFMAN_MAX_SURFACES-1; i>=0; i--) {
			//egi_dpstd("Draw surfaces[%d]...\n",i);

			/* Check surface availability */
			surface = surfman->surfaces[i];
			/* As surfman->surfaces sorted in ascending order of zseq */
			if( surface == NULL ) {
			        pthread_mutex_unlock(&surfman->surfman_mutex);
				break;
			}

	/* TEST: ------------ */
			printf("surfaces[%d].zseq= %d\n", i, surface->zseq);

			/* Render surface */
			if( surface !=NULL ) {
				/* TODO: NOW support SURF_RGB565 only */
				if( surface->colorType != SURF_RGB565 ) {
				        //pthread_mutex_unlock(&surfman->surfman_mutex);
					continue;
				}

			#if 0	/* TEST: ------ */
				//egi_dpstd("Draw surfaces[%d]...\n",i);
				draw_filled_rect2( &surfman->fbdev, egi_256color_code(mat_random_range(255)),
							surface->x0, surface->y0,
							surface->x0+surface->width-1, surface->y0+surface->height-1 );
			        pthread_mutex_unlock(&surfman->surfman_mutex);
				continue;
			#endif

				/* Map surface data to EGI_IMGBUF */
				imgbuf->width = surface->width;
				imgbuf->height = surface->height;
				imgbuf->imgbuf = (EGI_16BIT_COLOR *)surface->color;
				if(surface->off_alpha >0 )
					imgbuf->alpha = (EGI_8BIT_ALPHA *)(surface+surface->off_alpha);
				else
					imgbuf->alpha = NULL;

				/* writeFB imgbuf */
				egi_subimg_writeFB(imgbuf, &surfman->fbdev, 0, -1,  surface->x0, surface->y0);

			}

		} /* END for() traverse all surfaces */

/* ------- <<<  Critical Zone  */
	        /* put mutex lock for ineimg */
		pthread_mutex_unlock(&surfman->surfman_mutex);

		/* Render and bring to FB */
		fb_render(&surfman->fbdev);
		tm_delayms(200);
	}

	/* Free imgbuf */
	egi_imgbuf_free2(&imgbuf);

	/* Reset token */
	surfman->renderThread_on = false;

	return (void *)0;
}

