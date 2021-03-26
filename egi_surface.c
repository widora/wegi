/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A module for SURFACE, SURFUSER, SURFMAN.

			----- Concept and Definition -----

SURFACE(SURFSHMEM):   An imgbuf and relevant image data allocated for an application.
	   It defines an area for interface of (image) displaying and (mouse) interacting.
	   At SURFUSER side, it can only access SURFSHMEM part of the SURFACE.

	   The TOP Surface is the current focued surface on the desktop, and it's
	   the sole surface that gets mouse/keyboard input from SURFMAN via ERING.

SURFUSER:  The owner of a SURFACE, an uclient to SURFMAN. (TODO: It MAY hold
	   several SURFACEs?).
	   However it can only access SURFSHMEM part of the SURFACE, as
	   a way to protect private data for the SURFMAN.
      >>>  The SURFUSER (or a thread of it) is always blocked at ering_msg_recv() when its
	   surface is NOT on the TOP layer, and starts to parse input data from SURFMAN once
	   it's activated.

SURFMAN:   It's the manager of all SURFACEs and SURFUSERs, rendering surface
	   imgbufs to FB, and communicating with surfusers.
	   It totally contorls data of all SURFACEs.
      <<<  The SURFMAN keeps eringing input data to the TOP surface.


			----- Routine Process ----
SURFUSER:
  1. Register a SURFUSER (with a SURFACE) to SURFMAN by calling egi_register_surfuser(),
     via ERING_PATH_SURFMAN.
  2. Add (more) SURFACEs to a registered SURFUSER by calling ering_request_surface(),
     via connected UNIX type sockfd.
  3. Receive ERING message from the SURFMAN by calling ering_msg_recv(BLOCKED),
     via connected UNIX type sockfd. < Mouse/Keyboard data >
  4. Do routine jobs, update image data to SURFSHMEM and synchronize with SURFMAN.

SURFMAN:
  1. Create a SURFMAN by calling egi_create_surfman(), which initializes envs with:
     1.1 Establish ERING_PATH_SURFMAN for ERING.
     1.2 Initilize/mmap FB device.
  2. When an EGI_SURMAN is created, there are 3 threads running for routine jobs.
     2.1 userv_listen_thread(): To accept connection from SURFUER uclint, and save to
	 a session list.
     2.2 surfman_request_process_thread(void *surfman): Listen to SURFUSERs by select().
         2.2.1 Upon SURFUSER's request, to create and register SURFACEs.
         2.2.2 Retire/unregister SURFUSER if it disconnets from ERING.
     2.3 surfman_render_thread():
         2.3.1 To render all surfaces/mouse/menus to FB.
	 2.3.2 Synchronize with all SURFUSER(SURFSHMEM).
  3. Main thread of the SURFMAN.
     3.1 Scan mouse and keyboard input, and parse it.
     3.2 Update mouse position, as for surfman ->mx,my.
     3.3 Adjust SURFACE rendering sequence, bring a SURFACE to top by mouse clicking.
     3.4 Minimize/Maximize/Close SURFACEs. (by mouse clicking)
     3.5 Send mouse status to SURFUSER of the current top(focused) surface.
     3.6 Send keyboard input to SURFUSER of the current top(focused) surface.
     3.7 Move SURFACEs by mouse dragging. (OR by SURFUSER?)


Note:
1. Anonymous shared memory leakage/not_freed??  Busybox: ipcs ipcrm

TODO:
1. XXX Abrupt break of a surfuser will force surfman_request_process_thread()
   and surfman_render_thread() to quit!!!
   --- OK, Apply PTHREAD_MUTEX_ROBUST.

2. Check sync/need_refresh token of each surface before rendering, skip
   it if the surface image is not updated, thus may help surfman cutdown rendering time.
   However, mouse cursor has to be refreshed directly on the working buffer.
   since hardware does NOT support 2_layer frame buffer, working buffer
   need redraw be the mouse cursor....

3. XXX The last SURFUSER's register request has exactly 50% chance to fail with unet_recvmsg() errno=131 Err'Connection reset by peer'.
   ---OK

Journal
2021-02-22:
	1. Add surfman_render_thread().
	2. Apply surfman_mutex.
2021-02-23:
	1. Add surface_compare_zseq() and surface_insertSort_zseq().
2021-02-24:
	1. Add EGI_SURFUSER, egi_register_surfuser()/egi_unregister_surfuser()
2021-02-25:
	1. Add mutex_lock and mutexattr for surface
2021-02-26:
	1. Separate EGI_SURFSHMEM from EGI_SURFACE.
2021-02-27:
	1. Apply PTHREAD_MUTEX_ROBUST for surfshmem->shmem_mutex.
2021-03-2:
	1. Apply surfman->mcursor.
2021-03-3:
	1. Add surfman_xyget_Zseq().
	2. Add surfman_xyget_surfaceID().
	3. surfman_bringtop_surface_nolock().
2021-03-10:
	1. Test Minimize/Maximize surfaces.
	2. surfman_bringtop_surface_nolock(): Update surfaces[]->id!
2021-03-11:
	1. Definition and description of SURFACE, SURFUSER, SURFMAN.
	2. Put SURFACE name to leftside minibar menu. (struct memeber surfnam[] for SURFSHMEM)
	3. Mouse click on leftside minibar to bring the SURFACE to TOP layer.
	   (Add struct memeber IndexMpMinSurf for SURFMAN)
2021-03-12:
	1. Add EGI_SURFBTN: egi_surfbtn_create(), egi_surfbtn_free().
	2. Add egi_point_on_surfbtn()
2021-03-13:
	1. Move surface->status to surface->surfshmem->status, so SURFUSER can access.
2021-03-15:
	1. surfman_render_thread(): If surface status is downhold, then draw a rim to highlight it.
2021-03-16:
	1. Add maxW,maxH for EGI_SURFSHMEM, and add them to following functions
	   as input paramters:
	   egi_register_surfuser(), surfman_register_surface(), ering_request_surface()
2021-03-21:
	1. surfman_render_thread(): Send ERING_SURFACE_BRINGTOP to the new selected TOP surface.
	   A surface is brought to TOP by means of:
		A. Picked by surfman. see mouse click event in test_surfman.c.
		B. The prior TOP surface is minimized by surfuser. see mouse event in test_surfuser.c
		c. The prior TOP surface is closed by surfuser.
	2. Add EGI_SURFSHMEM memebers:  .sbtns[3],   .mptbn
2021-03-22:
	1. Add: surfuser_resize_surface(), surfuser_parse_mouse_event().
	2. Add EGI_SURFSHMEM memebers: .resize_surface()
2021-03-24:
	1. Add default surface operations:move/resize/minimize/maximize/normalize/close
2021-03-25:
	1. Rename surfuser_resize_surface() to  surfuser_redraw_surface()

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
#include "egi_log.h"
#include "egi_unet.h"
#include "egi_timer.h"
#include "egi_debug.h"
#include "egi_surface.h"
#include "egi_fbgeom.h"
#include "egi_image.h"
#include "egi_math.h"
#include "egi_FTsymbol.h"
#include "egi_input.h"

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
Mmap fd to get pointer to an EGI_SURFSHMEM

Note: memfd NOT close here!

@memfd:   A file descriptor created by memfd_create.

Reutrn:
	A pointer to EGI_SURFSHMEM	OK
	NULL				Fails
--------------------------------------------------*/
EGI_SURFSHMEM *egi_mmap_surfshmem(int memfd)
{
	EGI_SURFSHMEM *surfshmem=NULL;
        struct stat sb;

        if (fstat(memfd, &sb)<0) {
		egi_dperr("fstat");
                return NULL;
        }

        surfshmem=mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED, memfd, 0);
        if(surfshmem==MAP_FAILED) {
		egi_dperr("Fail to mmap memfd!");
                return NULL;
        }

	return surfshmem;
}


/*------------------------------------------
Unmap an EGI_SURFSHMEM.

@eface:  Pointer to EGI_SURFSHMEM.

Return:
	0	OK
	<0	Fails
------------------------------------------*/
int egi_munmap_surfshmem(EGI_SURFSHMEM **shmem)
{
	if(shmem==NULL|| (*shmem)==NULL)
		return 0;

	if( *shmem==MAP_FAILED )
		return 0;

        /* Unmap surface */
        if( munmap(*shmem, (*shmem)->shmsize)!=0 ) {
        	//printf("%s: Fail to unmap eface!\n",__func__);
		egi_dperr("Fail to unmap eface!");
		return -1;
	}

	/* Reset *shmem */
	*shmem = NULL;

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

	/* Reset all surface->id as index of surfman->surfaces[], Reset all zseq */
	for(i=SURFMAN_MAX_SURFACES-1; i>=0; i--) {
		if(surfaces[i]==NULL) {  /* NULL followed by NULL */
			break;
		}
		else {
			surfaces[i]->id=i;  /* Reset index of surfman->surfaces[] as ascending order */
			surfaces[i]->zseq = maxzseq--;
		}
	}
}


/*-------------------------------------------------------------
Register/create an EGI_SURFUSER from an EGI_SURFMAN via svrpath.

TODO: NOW, Support ColorType SURF_RGB565 and SURF_RGB565_A8.

@svrpath:	Path to an EGI_SURFMAN.
@x0,y0:		Surface position relative to FB
@maxW,maxH:	Max. Surface width and height
@w,h:		Default surface width and height.
@colorType:	Surface color data type

Return:
	A pointer to EGI_SURFUSER	OK
	NULL				Fails
-------------------------------------------------------------*/
EGI_SURFUSER *egi_register_surfuser( const char *svrpath,
				   int x0, int y0, int maxW, int maxH, int w, int h, SURF_COLOR_TYPE colorType )
{
	EGI_SURFUSER *surfuser;

	/* Check input */
	if( colorType != SURF_RGB565 && colorType != SURF_RGB565_A8 ) {
		egi_dpstd("NOW Only support colorType for SURF_RGB565 and SURF_RGB565_A8!\n");
		return NULL;
	}
	if(w<1 || h<1)
		return NULL;
	if(maxW<w) maxW=w;
	if(maxH<h) maxH=h;

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
	egi_dpstd("Uclient succeed to connect to '%s'.\n", svrpath);

	/* 3. Request for a surface and get shared memory data */
	surfuser->surfshmem=ering_request_surface( surfuser->uclit->sockfd, x0, y0, maxW, maxH, w, h, colorType);
	if(surfuser->surfshmem==NULL) {
		egi_dpstd("Fail to request surface!\n");
		unet_destroy_Uclient(&surfuser->uclit);
		free(surfuser);
		return NULL;
	}

	/* 4. Allocate an EGI_IMGBUF, for its vfbdev. */
	surfuser->imgbuf=egi_imgbuf_alloc();
	if(surfuser->imgbuf==NULL) {
		egi_dpstd("Fail to allocate imgbuf!\n");
		egi_munmap_surfshmem(&surfuser->surfshmem);
		unet_destroy_Uclient(&surfuser->uclit);
		free(surfuser);
		return NULL;
	}

	/* 5. Map surface data to imgbuf */
	surfuser->imgbuf->width = w;	/* Default surface size */
	surfuser->imgbuf->height = h;
	surfuser->imgbuf->imgbuf = (EGI_16BIT_COLOR *)surfuser->surfshmem->color; /* SURF_RGB565 ! */
        if( surfuser->surfshmem->off_alpha >0 )
        	surfuser->imgbuf->alpha = (EGI_8BIT_ALPHA *)(surfuser->surfshmem->color+surfuser->surfshmem->off_alpha);
        else
                surfuser->imgbuf->alpha = NULL;

	/* 6. Init. virtual FBDEV */
	if( init_virt_fbdev(&surfuser->vfbdev, surfuser->imgbuf) != 0 ) {
		egi_dpstd("Fail to init vfbdev!\n");
		surfuser->imgbuf->imgbuf=NULL; /* Unlink to surface data before free imgbuf */
		surfuser->imgbuf->alpha=NULL;
		egi_imgbuf_free2(&surfuser->imgbuf);
		egi_munmap_surfshmem(&surfuser->surfshmem);
		unet_destroy_Uclient(&surfuser->uclit);
		free(surfuser);
		return NULL;
	}

	/* xxxx 7. Init first 3 surfuser->surfbtns[] as for CLOSE/MIN/MAX */


	/* OK */
	return surfuser;
}


/*---------------------------------------------------------
Unregister/destory an EGI_SURFUSER.

!!! WARNING !!! Imgbuf data is unlinked before destory the surfuser.

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

	/* 0. Make sure surfshmem->shmem_mutex unlocked! */
	//pthread_mutex_unlock(&surfshmem->shmem_mutex);

#if 0   /* XXX NOPE!!  Should NOT put in egi_unregister_surfuser()! */
	/* 1. Join ering_routine  */
	(*surfuser)->surfshmem->usersig =1;  // Useless if thread is busy calling a BLOCKING function.
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	tm_delayms(10);
	/* Make sure mutex unlocked in pthread if any! */
	if( pthread_join((*surfuser)->surfshmem->thread_eringRoutine, NULL)!=0 ) {
		egi_dperr("Fail to join eringRoutine");
		ret = -1;
	}
#endif

        /* 2. Release virtual FBDEV */
        release_virt_fbdev(&(*surfuser)->vfbdev);

	/* 3. Unlink imgbuf data and free the holding imgbuf */
	(*surfuser)->imgbuf->imgbuf=NULL;
	(*surfuser)->imgbuf->alpha=NULL;
	egi_imgbuf_free2(&(*surfuser)->imgbuf);

        /* 4. Unmap surfshmem */
        if( egi_munmap_surfshmem(&(*surfuser)->surfshmem) !=0 )
		ret += -(1<<1);

        /* 5. Free uclit, and close its socket. SURFMAN should receive signal to retire the surface. */
        if( unet_destroy_Uclient(&(*surfuser)->uclit) !=0 )
		ret += -(1<<2);

        /* 6. Free surfuser struct */
        free(*surfuser);
	*surfuser=NULL;

	return ret;
}


/*---------------------------------------------------------
Request a surface from the Surfman via local socket.
Return a surfshmem pointer to shared memory space.

Note:
1. It will be BLOCKED while waiting for reply.
2. DO NOT forget to munmap it after use!

@sockfd    A valid UNIX SOCK_STREAM type socket, connected
	   to an EGI_SURFACE server.
	   Assume to be BLOCKING type.
@x0,y0	   Initial origin of the surface.
@maxW,maxH Max. surface size.
@w,h	   Default width and height of a surface.
@colorType	Surface color type
	   Also including alpha size!
Return:
	A pointer to EGI_SURFSHEM	OK
	NULL				Fails
----------------------------------------------------------*/
EGI_SURFSHMEM *ering_request_surface(int sockfd, int x0, int y0, int maxW, int maxH, int w, int h, SURF_COLOR_TYPE colorType)
{
	EGI_SURFSHMEM *surfshm=NULL;

	struct msghdr msg={0};
	struct cmsghdr *cmsg;
	int data[16]={0};
	struct iovec iov={.iov_base=data, .iov_len=sizeof(data) };
	int memfd;

	/* Check input */
	if( w<1 || h<1 )
		return NULL;
	if(maxW<w) maxW=w;
	if(maxH<h) maxH=h;

	/* 1. Prepare request params, put in data[]
         * Request data[] content:
         * data[0]:ering_request_type,
         * data[1]:x0, data[2]:y0,
         * data[3]:maxW, data[4]:maxH, data[5]:width, data[6]:height, data[7]:colorTYpe
         */
	memset(data,0,sizeof(data));
	data[0]=ERING_REQUEST_ESURFACE;
	data[1]=x0; 	data[2]=y0;
	data[3]=maxW;  	data[4]=maxH;
	data[5]=w;  	data[6]=h;
	data[7]=colorType;

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
			default:
				egi_dpstd("Request fails for undefined error!\n");
		}
		return NULL;
	}

	/* 7. Get returned memfd */
	cmsg = CMSG_FIRSTHDR(&msg);
	/* TODO: warning: dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing] */
	memfd = *(int *)CMSG_DATA(cmsg);
	//memcpy(&memfd, (int *)CMSG_DATA(cmsg), n*sizeof(int));

	egi_dpstd("get memfd=%d\n", memfd);

  	/* 8. Mmap memfd to get pointer to EGI_SURFACE */
	surfshm=egi_mmap_surfshmem(memfd);
	if(surfshm==NULL) {
		close(memfd);
		return NULL;
	}

	/* 9. Close memfd  */
	if(close(memfd)<0) {
		//printf("Fail to close memfd, Err'%s'.!\n", strerror(errno));
		egi_dperr("Fail to close memfd.");
	}

	/* OK */
	return surfshm;
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
	int rfds;
	int csFD;
	int maxfd;
	fd_set set_uclits;
	struct timeval tm;
	int nrcv;
	//int nsnd;

	int memfd;
	int sfID; /* surface id, as index of surfman->surface[] */

	/* Surface request MSG buffer */
	struct msghdr msg={0};
	struct cmsghdr *cmsg;
	/*** Request data[] content:
	 * data[0]:ering_request_type,
   	 * data[1]:x0, data[2]:y0,
	 * data[3]:maxW, data[4]:maxH, data[5]:width, data[6]:height, data[7]:colorTYpe
	 */
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
	tm.tv_usec=500000;

	/* Enter routine loop */
	egi_dpstd("Start routine job...\n");
	surfman->repThread_on=true;
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

		/* select readable fds, all fds to be NONBLOCKING. */
//		egi_dpstd("Select maxfd=%d\n", maxfd);

		tm.tv_sec=0;
		tm.tv_usec=500000;
		rfds=select(maxfd+1, &set_uclits, NULL, NULL, &tm);
		/* Case 1: No readbale fds, No request */
		if(rfds==0) {
			//egi_dpstd("select() rfd=0!\n");
			//usleep(50000);
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
						if( surfman_unregister_surfUser(surfman, i) <0 ) /* i as sessionID */
							egi_dpstd("Fail to unregister surfUser sessionID=%d \n", i);
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
	if( data[7]> SURF_COLOR_MAX-1 )
		egi_dpstd(" !!! WARNING !!! Surface color type out of range!\n");

	egi_dpstd("Request for an surface of Max.%dx%dx%dBpp, origin at (%d,%d). \n",
			data[3], data[4], surf_get_pixsize(data[7]), data[1], data[2]);		/* W,H,pixsize, x0,y0 */

	/* Register a surface to surfman
         *Request data[] content:
         * data[0]:ering_request_type,
         * data[1]:x0, data[2]:y0,
         * data[3]:maxW, data[4]:maxH, data[5]:width, data[6]:height, data[7]:colorTYpe
         */
	sfID=surfman_register_surface( surfman, i, data[1], data[2], data[3], data[4], data[5], data[6], data[7] );
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

	surfman->repThread_on=false;
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
        if( pthread_create(&surfman->repThread, NULL, (void *)surfman_request_process_thread, surfman) !=0 ) {
                egi_dperr("Fail to launch surfman->repThread to process request!");
		unet_destroy_Userver(&surfman->userv);
		pthread_mutex_destroy(&surfman->surfman_mutex);
		free(surfman);
		return NULL;
        }
	egi_dpstd("surfman_request_process_thread starts!\n");

	/* 5. Allocate imgbuf */
	surfman->imgbuf=egi_imgbuf_alloc();
	if(surfman->imgbuf==NULL) {
		egi_dpstd("Fail to allocate imgbfu!\n");
		/* -------- NOW: use egi_destroy_surfman() aft surfman->repThread, ... */
		egi_destroy_surfman(&surfman);
		pthread_mutex_destroy(&surfman->surfman_mutex);
		return NULL;
	}

	/* 6. Init FBDEV */
	surfman->fbdev.devname = gv_fb_dev.devname;  /* USE global FB device name */
	if( init_fbdev(&surfman->fbdev) !=0 ) {
		egi_dpstd("Fail to init_fbdev\n");
		egi_destroy_surfman(&surfman);
		return NULL;
	}
	/* Zbuffer ON */
	surfman->fbdev.zbuff_on=true;
	egi_dpstd("surfman->fbdev is initialized with zbuff_on!\n");

	/* 7. Start surfman_render_thread() */
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
	/* Make sure mutex unlocked in pthread if any! */
	if( pthread_join((*surfman)->renderThread, NULL) !=0 ) {
        	egi_dperr("Fail to join renderThread");
                ret += -1;
                //Go on...
        }

	/* 2. Release FBDEV */
	if( (*surfman)->fbdev.map_fb != NULL ) {
		release_fbdev(&(*surfman)->fbdev);
	}

	/* 3. Free imgbuf */
	if((*surfman)->imgbuf)
		egi_imgbuf_free2(&(*surfman)->imgbuf);


	/* TODO: Free more cursor icons here?? */
	// mcursor, mgrab

	/* 4. Join repThread */
//	if( (*surfman)->repThread_on ) {  /*  NOPE! If repThread starts but repThread_on not set, it will cause mem leakage then. */
		(*surfman)->cmd=1; /* CMD to end thread */
		egi_dpstd("Join repThread...\n");
		/* Make sure mutex unlocked in pthread if any! */
		if( pthread_join( (*surfman)->repThread, NULL)!=0) {
			egi_dperr("Fail to join repThread");
			ret += -(1<<2);
			//Go on...
		}
//	}

	/* 5. Unregister surfusers: close userv->sessions[].csFD and unmap all related surfaces! */
	for(i=0; i<USERV_MAX_CLIENTS; i++) {
		if( userv->sessions[i].csFD >0 )
			if( surfman_unregister_surfUser(*surfman, i)<0 )
				ret = -( (-ret) | (1<<3) );
	}

	/* 6. Destroy userver */
	if( unet_destroy_Userver(&userv)!=0 ) {
		egi_dpstd("Fail to destroy userv!\n");
		ret += -(1<<4);
		//Go on...
	}

	/* 7. Destroy surfman_mutex */
        pthread_mutex_unlock(&(*surfman)->surfman_mutex); /* Necessary ??? */
        if(pthread_mutex_destroy(&(*surfman)->surfman_mutex) !=0 )
		egi_dperr("Fail to destroy surfman_mutex!");

	/* 8. Free self */
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
@maxW,maxH     	Max. width and height of a surface.
@w,h		Default width and height of a surface.
@colorType	Surface color type

TODO:
1. To deal with more colorType.

Return:
	>=0	OK, Index of the surfman->surfaces[]
	<0	Fails
----------------------------------------------------------------------*/
int surfman_register_surface( EGI_SURFMAN *surfman, int userID,
			      int x0, int y0, int maxW, int maxH, int w, int h, SURF_COLOR_TYPE colorType )
{
	size_t shmsize;
	int memfd;
	int pixsize;
	EGI_SURFACE *eface=NULL;
	int k;

	/* Check input */
	if(surfman==NULL)
		return -1;
	if( userID > USERV_MAX_CLIENTS-1  || userID<0 )
		return -1;
	if( w<1 || h<1 )
		return -1;
	if(maxW<w) maxW=w;
	if(maxH<h) maxH=h;

        /* Get mutex lock */
	egi_dpstd("Get surfman mutex ...\n");
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
	if(surfman->scnt >= SURFMAN_MAX_SURFACES) {
		egi_dpstd("Fail to create surface due to limit of SURFMAN_MAX_SURFACES!\n");
		pthread_mutex_unlock(&surfman->surfman_mutex);
		return -3;
	}

	/* 2. Allocate EGI_SURFACE */
	eface=calloc(1, sizeof(EGI_SURFACE));
	if(eface==NULL) {
		egi_dperr("Fail to calloc eface!");
		pthread_mutex_unlock(&surfman->surfman_mutex);
		return -4;
	}

	/* 3. Get pixel size, pixsize also includes alpha byte. */
	pixsize=surf_get_pixsize(colorType);

	/* 4. Create memfd, the file descriptor is open with (O_RDWR) and O_LARGEFILE. */
	shmsize=sizeof(EGI_SURFSHMEM)+maxW*maxH*pixsize; /* pixsize: color+alpha bits. */
	memfd=egi_create_memfd("surface_shmem", shmsize);
	if(memfd<0) {
		egi_dpstd("Fail to create memfd!\n");

		free(eface);

		pthread_mutex_unlock(&surfman->surfman_mutex);
		return -5;
	}

	/* 5. Mmap to get an EGI_SURFSHMEM */
	eface->surfshmem=egi_mmap_surfshmem(memfd);
	if(eface->surfshmem==NULL) {
		egi_dpstd("Fail to mmap surface!\n");

		close(memfd);
		free(eface);

		pthread_mutex_unlock(&surfman->surfman_mutex);
		return -6;
	}
	/* Do NOT close(memfd) here! To be closed by surfman_unregister_surface(). */

	/* 6. Init surface mutex_lock */
	if( pthread_mutexattr_setpshared(&(eface->surfshmem)->mutexattr, PTHREAD_PROCESS_SHARED) !=0 ) {
		egi_dpstd("Fail to set shared mutexattr!");
		// GO on anyway...
	}
	/* "When the owner thread dies without unlocking mutex, any further attemps to call pthread_mutex_lock()
	 *  will succeed and return EOWNERDEAD to indicate that the original owner no longer exists and the mutex
	 *  is in an inconsistent state. Usually after EOWNERDEAD is returned, the next owner should call pthread_mutex_consistent()
	 *  on the acquired mutex to make it consistent again before using it any further."  Linux manual
         */
	if( pthread_mutexattr_setrobust(&(eface->surfshmem)->mutexattr,PTHREAD_MUTEX_ROBUST) !=0 ) {
	}
	if(pthread_mutex_init(&eface->surfshmem->shmem_mutex, &eface->surfshmem->mutexattr) != 0) {
                egi_dperr("Fail to initiate mutex_lock.");

		egi_munmap_surfshmem(&eface->surfshmem);
		close(memfd);
		free(eface);

		pthread_mutex_unlock(&surfman->surfman_mutex);
                return -5;
        }

	/* 7. Register to surfman->surfaces[] */
	for(k=0; k<SURFMAN_MAX_SURFACES; k++) {
		if( surfman->surfaces[k]==NULL ) {
			surfman->surfaces[k]=eface;
			surfman->scnt +=1; /* increase surfaces counter */
			egi_dpstd("\t\t----- (+)surfaces[%d], scnt=%d -----\n", k, surfman->scnt);
			/* Must break to stop k increment */
			break;
		}
	}
	/* Double check */
	if(k==SURFMAN_MAX_SURFACES ) {
		egi_dpstd("OOOPS! k==SURFMAN_MAX_SURFACES!");

        	pthread_mutexattr_destroy(&eface->surfshmem->mutexattr);
       		pthread_mutex_destroy(&eface->surfshmem->shmem_mutex);

		egi_munmap_surfshmem(&eface->surfshmem);
		close(memfd);
		free(eface);

		pthread_mutex_unlock(&surfman->surfman_mutex);
		return -6;
	}

	/* 6. Assign ID and other members */
	/* 6.1 Surfman part */
	eface->id=k;		 /* <--- ID */
	eface->csFD=surfman->userv->sessions[userID].csFD;  /* <--- csFD / surf_userID */
	eface->memfd=memfd;
	eface->shmsize=shmsize;
	eface->width=maxW;	/* Max size of surface, for surfshmem allocation */
	eface->height=maxH;
	eface->colorType=colorType;

	/* Set zseq as scnt m this can NOT ensure to bring the surface to top.???  */
	// eface->zseq=surfman->scnt; /* ==scnt, The lastest surface has the biggest zseq!  All surface.zseq >0!  */
	/* Set zseq as  SURFMAN_MAX_SURFACES */
	eface->zseq=SURFMAN_MAX_SURFACES; /* Top it first, Let surface_insertSort_zseq() to adjust later. */

	/* 6.2 For surfshmem */
	eface->surfshmem->shmsize=shmsize;
	eface->surfshmem->maxW=maxW;   	/* As fixed Max. Width and Height */
	eface->surfshmem->maxH=maxH;
	eface->surfshmem->vw=w;  	/* Default size, to be adjusted by SURFUSER */
        eface->surfshmem->vh=h;
        eface->surfshmem->x0=x0;
        eface->surfshmem->y0=y0;

	eface->surfshmem->mpbtn=-1;	/* No button touched by mouse */

	/* 6.3. Assign Color/Alpha offset for shurfshmem */
	//eface->off_color = eface->color - (unsigned char *)eface;
	/* For independent ALPHA data */
	switch( colorType ) {
		case SURF_RGB565_A8:
			eface->surfshmem->off_alpha =2*maxW*maxH;
			break;
		case SURF_RGB888_A8:
			eface->surfshmem->off_alpha =3*maxW*maxH;
			break;
		default: /* =0, NOT independent alpha data */
			eface->surfshmem->off_alpha =0;
	}

	/* 7. Sort surfman->surfaces[] in ascending order of their zseq value */
	surface_insertSort_zseq(&surfman->surfaces[0], SURFMAN_MAX_SURFACES);

/* TEST: ------------ */
	egi_dpstd("insertSort zseq: ");
	for(k=0; k<SURFMAN_MAX_SURFACES; k++)
		printf(" %d", surfman->surfaces[k] ? surfman->surfaces[k]->zseq : 0);
	printf("\n");


/* ------- <<<  Critical Zone  */
        /* put mutex lock for ineimg */
        pthread_mutex_unlock(&surfman->surfman_mutex);

	/* OK */
	return eface->id;
}


/*----------------------------------------------------------------
Unregister/unmap a surface from surfman->surfaces[].

Note:
1. Also cross check surfman_unregister_surfUser().

@surfman Pointer to a pointer to an EGI_SURFMAN.
@surfID Surface ID as of surfman->surfaces[surfID]

Return:
        0     	OK
        <0      Fails
----------------------------------------------------------------*/
int surfman_unregister_surface( EGI_SURFMAN *surfman, int surfID )
{
	int i;
	EGI_SURFACE *eface=NULL;
	int mutexret;

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

	/* Get pointer to the surface */
	eface=surfman->surfaces[surfID];

  	/* 1. Destroy mutex lock */
	/* 1.1 Get mutex lock, Not necessary here?? */
	mutexret=pthread_mutex_lock(&eface->surfshmem->shmem_mutex);
	if( mutexret ==EOWNERDEAD ) {
		egi_dperr("Shmem mutex owner is dead, try to make mutex consistent...!");
		EGI_PLOG(LOGLV_CRITICAL, "%s:Shmem mutex owner is dead, try to make mutex consistent...", __func__);
		if( pthread_mutex_consistent(&eface->surfshmem->shmem_mutex) !=0 ) {
			egi_dperr("Fail to make a robust mutex consistent!");
			EGI_PLOG(LOGLV_ERROR, "%s: Fail to make a robust mutex consistent!", __func__);
		}
		else {
			egi_dpstd("Succeed to make mutex consistent!");
			EGI_PLOG(LOGLV_CRITICAL,"%s: Succeed to make mutex consistent!", __func__);
		}
	}
	else if(mutexret!=0)
              		egi_dperr("Fail to unlock mutex!");

        if(pthread_mutexattr_destroy(&eface->surfshmem->mutexattr) !=0 )
                egi_dperr("Fail to destroy mutexattr!");
        if(pthread_mutex_destroy(&eface->surfshmem->shmem_mutex) !=0 )
                egi_dperr("Fail to destroy mutex_lock!");

	/* 2. Close memfd */
        if(close(eface->memfd)<0) {
                egi_dperr("close(memfd)");
	        pthread_mutex_unlock(&surfman->surfman_mutex);
		return -4;
	}

	/* 3. Munmap surfshmem */
	if( egi_munmap_surfshmem(&eface->surfshmem) !=0 ) {
		egi_dpstd("Fail to munamp surfaces[%d]->surfshmem!\n", surfID);
	        pthread_mutex_unlock(&surfman->surfman_mutex);
		return -5;
	}
	/* Free surface struct */
	free(surfman->surfaces[surfID]);

	/* 4. Reset surfman->surfaces and update scnt */
	surfman->surfaces[surfID]=NULL; /* !!! */
	surfman->scnt -=1;

	/* 5. Re_sort surfman->surfaces[] in ascending order of their zseq value */
	surface_insertSort_zseq(&surfman->surfaces[0], SURFMAN_MAX_SURFACES);

/* TEST: ------------ */
	for(i=0; i<SURFMAN_MAX_SURFACES; i++)
		egi_dpstd(" %d", surfman->surfaces[i] ? surfman->surfaces[i]->zseq : 0);
	printf("\n");


	egi_dpstd("\t\t----- (-)surfaces[%d], scnt=%d -----\n", surfID, surfman->scnt);

/* ------- <<<  Critical Zone  */
        /* put mutex lock for ineimg */
        pthread_mutex_unlock(&surfman->surfman_mutex);

	return 0;
}


/*-------------------------------------------------------------------
1. Bring surfman->surfaces[surfID] to the top layer by updating
   its zseq and its id!
2. Reset its STATUS to NORMAL.

Assume that zseq of input surfman->surfaces are sorted in ascending order.

@surfman	Pointer to a pointer to an EGI_SURFMAN.
@surfID		Index to surfman->surfaces[].

Return:
	0	OK
	<0	Fails
-------------------------------------------------------------------*/
int surfman_bringtop_surface_nolock(EGI_SURFMAN *surfman, int surfID)
{
	int i;
	EGI_SURFACE *tmpface=NULL;

	/* Check input */
	if(surfman==NULL)
		return -1;
	if(surfID <0 || surfID > SURFMAN_MAX_SURFACES-1 )
		return -2;

	/* Check */
	if(surfman->surfaces[surfID]==NULL) {
		return -3;
	}

	/* If already at top! XXX Max. zseq does NOT necessary means TOP surface, MAYBE in minibar menu. */
	//if(surfman->surfaces[surfID]->zseq==surfman->scnt) {
	//	return 0;
	//}

	/* In case bring surface from minibar menu (MINIMIZED) to TOP, reset its STAUTS. */
	if( surfman->surfaces[surfID]->surfshmem->status == SURFACE_STATUS_MINIMIZED )
	{
		/* If nw==0: Reset Status to NORMAL*/
		if( surfman->surfaces[surfID]->surfshmem->nw == 0 )
			surfman->surfaces[surfID]->surfshmem->status=SURFACE_STATUS_NORMAL;
		else  /* If nw>0: Reset status to MAXIMIZED */
			surfman->surfaces[surfID]->surfshmem->status=SURFACE_STATUS_MAXIMIZED;
	}

	/* Set its zseq to Max. of the surfman->surfaces */
	egi_dpstd("Set surfaces[%d] zseq to %d\n",surfID, surfman->scnt);
	surfman->surfaces[surfID]->zseq=surfman->scnt;
	tmpface=surfman->surfaces[surfID];

	/* Decrease zseq of surfaces above surface[surfID] */
	for(i=surfID+1; i<SURFMAN_MAX_SURFACES; i++) {
		/* Move down 1 layer */
		surfman->surfaces[i]->zseq -=1;
		/* Move down */
		surfman->surfaces[i-1] = surfman->surfaces[i];
		/* Reset id as index */
		surfman->surfaces[i-1]->id=i-1;
	}

	/* Set surfaces[surfID] as the top surface */
	surfman->surfaces[SURFMAN_MAX_SURFACES-1]=tmpface;
	surfman->surfaces[SURFMAN_MAX_SURFACES-1]->id=SURFMAN_MAX_SURFACES-1; /* Reset is */

	return 0;
}

/*-----------------------------------------------------------
Bring surfman->surfaces[surfID] to the top layer by updating
its zseq.

Assume that zseq of surfman->surfaces are sorted in ascending order.

@surfman	Pointer to a pointer to an EGI_SURFMAN.
@surfID		Index to surfman->surfaces[].

Return:
	0	OK
	<0	Fails
------------------------------------------------------------*/
int surfman_bringtop_surface(EGI_SURFMAN *surfman, int surfID)
{
	int ret;

	/* Check input */
	if(surfman==NULL)
		return -1;

        /* Get mutex lock */
        if(pthread_mutex_lock(&surfman->surfman_mutex) !=0 ) {
                return -3;
        }
 /* ------ >>>  Critical Zone  */

	ret=surfman_bringtop_surface_nolock(surfman, surfID);

#if 0 //////////// Replaced by surfman_bringtop_surface_nolock() /////
	/* Check */
	if(surfman->surfaces[surfID]==NULL) {
	        pthread_mutex_unlock(&surfman->surfman_mutex);
		return -3;
	}

	/* Set its zseq to Max. of the surfman->surfaces */
	egi_dpstd("Set surfaces[%d] zseq to %d\n",surfID, surfman->scnt);
	surfman->surfaces[surfID]->zseq=surfman->scnt;
	tmpface=surfman->surfaces[surfID];

	/* Decrease zseq of surfaces above surface[surfID] */
	for(i=surfID+1; i<SURFMAN_MAX_SURFACES; i++) {
		/* Move down 1 layer */
		surfman->surfaces[i]->zseq -=1;
		/* Move down */
		surfman->surfaces[i-1] = surfman->surfaces[i];
		/* Reset id as index */
		surfman->surfaces[i-1]->id=i-1;
	}

	/* Set surfaces[surfID] as the top surface */
	 surfman->surfaces[SURFMAN_MAX_SURFACES-1]=tmpface;
	surfman->surfaces[SURFMAN_MAX_SURFACES-1]->id=SURFMAN_MAX_SURFACES-1; /* Reset is */

#endif /////////////////////

/* ------- <<<  Critical Zone  */
        /* put mutex lock for ineimg */
        pthread_mutex_unlock(&surfman->surfman_mutex);

	return ret;
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
	int mutexret;

	if(surfman==NULL)
		return -1;

	if(sessionID<0 || sessionID > USERV_MAX_CLIENTS-1) {
		egi_dperr("Invalid sessionID:%d", sessionID);
		return -2;
	}

	//NOTE: surfman->scnt MAYBE <1;

        /* Get mutex lock */
        if(pthread_mutex_lock(&surfman->surfman_mutex) !=0 ) {
		egi_dperr("Fail mutex_lock");
                return -1;
        }
 /* ------ >>>  Critical Zone  */

	/* Note:
	 *    1. All clients(csFDs) are accpeted by userv_listen_thread() in surfman->userv.
	 *       All clients(csFDs) are closed here by surfman_unregister_surfUser().
	 *	 One producer one consumer scenario!
	 */


	/* 1. Get related csFD */
	csFD=surfman->userv->sessions[sessionID].csFD;
	if(csFD <= 0) {
		egi_dpstd("csFD<=0!");
	        pthread_mutex_unlock(&surfman->surfman_mutex);
		return -2;
	}

	/* 2. Unmap all related surfaces->surfshmem and free surfaces */
	cnt=0;
	for(k=0; k<SURFMAN_MAX_SURFACES; k++) {
		/* Get eface as ref. */
		eface=surfman->surfaces[k];
		if( eface!=NULL && eface->csFD == csFD ) {

			/* Unregister surfaces, maybe more than 1! */
		 	/* TODO: Following to be replaced by surfman_unregister_surface() XXX NOPE, double mutex_lock!!! */

		  	/* A.1 Destroy mutex lock */
			/* A.1.1 Get mutex lock, Not necessary here?? */

#ifndef LETS_NOTE	/* TODO: LETS_NOTE core dump here! */
			egi_dpstd("mutex lock shmem_mutex ...\n");
			mutexret=pthread_mutex_lock(&eface->surfshmem->shmem_mutex);
			if( mutexret ==EOWNERDEAD ) {
				egi_dperr("Shmem mutex owner is dead, try to make mutex consistent...!"); /* Err'Success'. ? */
				if( pthread_mutex_consistent(&eface->surfshmem->shmem_mutex) !=0 )
					egi_dperr("Fail to make a robust mutex consistent!");
				else
					egi_dpstd("Succeed to make mutex consistent!");
			}
			else if(mutexret!=0)
                		egi_dperr("Fail to unlock mutexattr!");
#endif

			/* A.1.2 destroy mutexattr */
			egi_dpstd("Destroy mutexattr ...\n");
		        if(pthread_mutexattr_destroy(&eface->surfshmem->mutexattr) !=0 )
                		egi_dperr("Fail to destroy mutexattr!");
			/* A.1.3 destroy mutex */
			egi_dpstd("Destroy shmem_mutex ...\n");
		        if(pthread_mutex_destroy(&eface->surfshmem->shmem_mutex) !=0 ) /* TODO: Fail to destroy mutex_lock! Err'Success'. */
                		egi_dperr("Fail to destroy mutex_lock!");

			/* A.2 Close memfd */
		        if(close(surfman->surfaces[k]->memfd)<0) {
                		egi_dperr("close(memfd)");
			        //pthread_mutex_unlock(&surfman->surfman_mutex);
		                //return -4;  Go on....
        		}

			/* A.3 Unmap and related surfaces */
			if( egi_munmap_surfshmem(&eface->surfshmem) !=0 ) {
				egi_dpstd("Fail to unmap surfman->surfaces[%d]!\n", k);
			}
			//Else:  Go on ....

			/* A.4 Free surface struct */
			free(surfman->surfaces[k]);
			surfman->surfaces[k]=NULL;

			/* A.5 Update surface counter */
			surfman->scnt -=1;
		        egi_dpstd("\t\t----- (-)surfaces[%d], scnt=%d -----\n", k, surfman->scnt);

			/* Counter for released surfaces */
			cnt++;
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

	/* 5. Resort surfman->surfaces[] in ascending order of their zseq value */
	surface_insertSort_zseq(&surfman->surfaces[0], SURFMAN_MAX_SURFACES);

	/* Bring the max Zseq surface to top layer. */
//	if(surfman->surfaces[SURFMAN_MAX_SURFACES-1])
//		surfman_bringtop_surface_nolock(surfman, int surfID);

/* ------- <<<  Critical Zone  */
        /* put mutex lock for ineimg */
        pthread_mutex_unlock(&surfman->surfman_mutex);

	return cnt;
}


/*----------------------------------------------------------------
Return BFDEV.zbuff[] value of the given x,y. It is
the same as of current surfman.surfaces[].zseq value
, as ascendingly sorted.
If (x,y) if out of FB Screen, the it returns zseq of the
bkground layer.

@surfman	Pointer to a pointer to an EGI_SURFMAN.
@x,y:           Pixel coordinate value. under FB.pos_rotate coord!

Return:
        0       Fails or out of range.  ( 0 as for bkground layer.)
        >=0     Ok.
-----------------------------------------------------------------*/
int surfman_xyget_Zseq(EGI_SURFMAN *surfman, int x, int y)
{
	if(surfman==NULL || surfman->fbdev.fbfd<=0 )
		return 0;

	return fbget_zbuff(&surfman->fbdev, x, y);

}


/*----------------------------------------------------------------
Return surface ID to surfman->surfaces[].

@surfman	Pointer to a pointer to an EGI_SURFMAN.
@x,y:           Pixel coordinate value. under FB.pos_rotate coord!

Return:
        <0      BKGround / Out of range / Fails.
		( zseq=0 as for bkground layer.)
        >=0     Ok, as index to surfman->surfaces[].
-----------------------------------------------------------------*/
int surfman_xyget_surfaceID(EGI_SURFMAN *surfman, int x, int y)
{
	int zseq;

	zseq=fbget_zbuff(&surfman->fbdev, x, y); /* 0 for bkground layer */

	return  zseq>0 ? SURFMAN_MAX_SURFACES -surfman->scnt +zseq -1 : -1;
}


/*----------------------------------------------------------------
Return index of a surfman->surfaces[], which appears at the top of
the desk, Not necessary to has biggest zseq!
Also consider STATUS and minimized surfaces are NOT counted in.

Assume all surfman->surfaces in ascending order, as ascendingly sorted.

@surfman	Pointer to a pointer to an EGI_SURFMAN.

Return:
        <0      Fails, OR no surface at display.
        >=0     Ok.
-----------------------------------------------------------------*/
int surfman_get_TopDispSurfaceID(EGI_SURFMAN *surfman)
{
	int i;

	if(surfman==NULL || surfman->scnt<1)
		return -1;

#if 1	/* Assume surfaces[] are sorted in ascending order of zseq! */
	for(i=0; i < surfman->scnt; i++) {

		/* In case sorting error! */
		if(surfman->surfaces[SURFMAN_MAX_SURFACES-1-i]==NULL)
			return -2;;

		//egi_dpstd("surfaces[%d] status=%d\n", SURFMAN_MAX_SURFACES-1-i,
		//		surfman->surfaces[SURFMAN_MAX_SURFACES-1-i]->surfshmem->status );
        	if( surfman->surfaces[SURFMAN_MAX_SURFACES-1-i]->surfshmem->status != SURFACE_STATUS_MINIMIZED )
			return SURFMAN_MAX_SURFACES-1-i;
	}

#else
	for(i=SURFMAN_MAX_SURFACES-1; i>=0; i--) {

		/* In case sorting error! */
		if(surfman->surfaces[i]==NULL) {
			return -2;;
		}

		//if(surfman->surfaces[i]->status !=SURFACE_STATUS_MINIMIZED )
		if(surfman->surfaces[i]->surfshmem->status !=SURFACE_STATUS_MINIMIZED )
			return i;
	}
#endif
	return -3;
}


/*-----------------------------------------------
wRender surfman->surfaces[] one by one, and bring
them to FB to display.
NOW: only supports SURF_RGB565

TODO: More surfaces[]->colorType support.

-----------------------------------------------*/
static void * surfman_render_thread(void *arg)
{
	int i;
	int mutexret;

	EGI_SURFSHMEM *surfshmem=NULL; /* Ref. */
	EGI_SURFACE *surface=NULL;
	EGI_SURFMAN *surfman=(EGI_SURFMAN *)arg;
	char *pstr;
	int  TopDispSurfID; /* Top displayed surface index as of surfman->surfaces[],  Invalid if <0. */
	int  userID=-1; 	    /* NOW as surface->csFD, Invalid if <0 */

	EGI_IMGBUF *imgbuf=NULL;  /* An EGI_IMGBUF to temporarily hold surface color data, NOT APPLIED! */
	EGI_IMGBUF *mrefimg=NULL; /* Just a ref. to an imgbuf */
	int  mdx=0, mdy=0;	  	  /* Mouse icon offset from tip point */

	int MiniBarWidth=120; /* Left side minibar menu */
	int MiniBarHeight=30;

	if( surfman==NULL )
		return (void *)-1;

        ERING_MSG *emsg=ering_msg_init();
        if(emsg==NULL) {
		egi_dperr("Fail to init ERING_MSG!");
		return (void *)-2;
	}

	/* A.1 To confirm FBDEV open and available */
	if( surfman->fbdev.fbfd <=0 ) {
		egi_dpstd("Surfman fbdev.fbfd is invalid!\n");
		return (void *)-2;
	}

	/* A.2 Allocate imgbuf */
	imgbuf=egi_imgbuf_alloc();
	if(imgbuf==NULL) {
		egi_dpstd("Fail to allocate imgbfu!\n");
		return (void *)-3;
	}

	/* A.3 Routine of rendering registered surfaces */
	surfman->renderThread_on = true;
	while( surfman->cmd !=1 ) {

		/* B.1 Get surfman_mutex ONE BY ONE!  TODO: TBD */
		//egi_dpstd("Try surfman mutex lock...\n");
	        if(pthread_mutex_lock(&surfman->surfman_mutex) !=0 ) {
	        //if(pthread_mutex_trylock(&surfman->surfman_mutex) !=0 ) {
			egi_dperr("Fail to get surfman_mutex!"); /* Err'Inappropriate ioctl for device'. */
               		tm_delayms(5);
			continue;
       		}
 /* ------ >>>  Surfman Critical Zone  */

		/* B.2 Draw backgroud, and zbuff values all reset to 0. */
		//fb_clear_workBuff(&surfman->fbdev, WEGI_COLOR_GRAY2);
		fb_reset_zbuff(&surfman->fbdev);

		/* B.2a Default mouse icon */
		mrefimg=surfman->mcursor;

#if 0		/* Set wallpaper: Here OR at end. */
		if(surfman->bkgimg) {
			surfman->fbdev.pixz=0;
			egi_subimg_writeFB(surfman->bkgimg, &surfman->fbdev, 0, -1, 0, 0);
		}
#endif

		/* B.2b Get TopDispSurfID: <0 No surface on desk, OR all at minibar menu.  */
		TopDispSurfID=surfman_get_TopDispSurfaceID(surfman);
		if( TopDispSurfID>=0 && userID != surfman->surfaces[TopDispSurfID]->csFD ) {
			userID = surfman->surfaces[TopDispSurfID]->csFD;
			/* NEW TOP surface! send ERING_SURFACE_BRINGTOP to it. */
                        /* Send msg to the surface */
                        emsg->type=ERING_SURFACE_BRINGTOP;
                        if( unet_sendmsg( userID, emsg->msghead) <=0 )
                        		egi_dpstd("Fail to sednmsg ERING_SURFACE_BRINGTOP!\n");
		}

		/* B.2c Clear minsurfaces */
		//memset(surfman->minsurfaces, 0, SURFMAN_MAX_SURFACES*sizeof(typeof(surfman->minsurfaces)));
		for(i=0; i<SURFMAN_MAX_SURFACES; i++)
			surfman->minsurfaces[i]=NULL;
		surfman->mincnt=0;

		/* B.3 Draw and render surfaces */
		/* NOTE:
		 * 1. Traverse from surface with bigger zseqs to smaller ones.
 		 *    OR reversed sequence?
		 * 2. ALPHA effect need a right render sequence!
		 */
		//egi_dpstd("Render surfaces...\n");
		for(i=SURFMAN_MAX_SURFACES-1; i>=0; i--) {  	     /* From top layer to bkground */
		//for(i=0; i<SURFMAN_MAX_SURFACES; i++) {	     /* From bkground to top layer */
			//egi_dpstd("Draw surfaces[%d]...\n",i);

			/* C.1 Check surface availability */
			surface = surfman->surfaces[i];
			/* As surfman->surfaces sorted in ascending order of zseq, NULL followed by NULL. */
			if( surface == NULL ) {
				//break;
				continue;  /* To traverse surfaces */
			}
//			egi_dpstd("surfaces[%d].zseq= %d\n", i, surface->zseq);

			/* C.1A Check status_minimized */
			//if( surface->status == SURFACE_STATUS_MINIMIZED ) {
			if( surface->surfshmem->status == SURFACE_STATUS_MINIMIZED ) {
				surfman->minsurfaces[surfman->mincnt]=surface;
				surfman->mincnt++;
				continue;
			}
			/* MINIMIZED surface will NOT be rendered!
			 *  --- NOW --- :  Following surfaces are all of normal size and displayed on desk top!
			 */

			/* C.2 Get surfshmem as ref. */
			surfshmem=surface->surfshmem;

			/* C.3 Get surface mutex_lock */
                        mutexret=pthread_mutex_lock(&surfshmem->shmem_mutex);
                        if( mutexret==EOWNERDEAD ) {
                                egi_dperr("Shmem mutex of surface[%d]'s owner is dead, try to make mutex consistent...!", i);
				EGI_PLOG(LOGLV_CRITICAL, "%s: Shmem mutex of surface[%d]'s owner is dead, try to make mutex consistent.",
														__func__, i);
                                if( pthread_mutex_consistent(&surfshmem->shmem_mutex) !=0 ) {
                                        egi_dperr("Fail to make a robust mutex consistent!");
					EGI_PLOG(LOGLV_CRITICAL, "%s: Fail to make a robust mutex consistent!", __func__);
					//Go on ....
				}
                                else {
                                        egi_dpstd("Succeed to make mutex consistent!");
					EGI_PLOG(LOGLV_CRITICAL, "%s: Succeed to make mutex consistent.", __func__);
				}
                        }
                        else if(mutexret!=0) {
				egi_dperr("Fail to get mutex_lock for shmem of surface[%d].", i);
				continue;  /* To traverse surfaces */
       			}
	/* ------ >>>  Surfshmem Critical Zone  */

			/* Check sync, TODO more for sync. */
			if( surfshmem->sync==false ) {
				pthread_mutex_unlock(&surfshmem->shmem_mutex);
				continue; /* To traverse surfaces */
			}

			/* C.4  TODO: NOW support SURF_RGB565 and SURF_RGB565_A8 only */
			if( surface->colorType != SURF_RGB565 && surface->colorType != SURF_RGB565_A8 ) {
				egi_dpstd("Unsupported colorType!\n");
			        pthread_mutex_unlock(&surfshmem->shmem_mutex);
				continue; /* To traverse surfaces */
			}

			/* C.5 Link surface data to EGI_IMGBUF */
#if 0 /* NOT necessary */
			if( surfshmem->status == SURFACE_STATUS_MAXIMIZED ) {
				imgbuf->width = surface->width;
				imgbuf->height = surface->height;
			}
			else {
#endif
				imgbuf->width = surfshmem->vw > surface->width ? surface->width : surfshmem->vw;
				imgbuf->height = surfshmem->vh > surface->height ? surface->height : surfshmem->vh;
			//}

			imgbuf->imgbuf = (EGI_16BIT_COLOR *)surfshmem->color;
			if(surfshmem->off_alpha >0 )
				imgbuf->alpha = (EGI_8BIT_ALPHA *)(surfshmem->color+surfshmem->off_alpha);
			else
				imgbuf->alpha = NULL;

			/* C.6 Set Zseq as pix Z value */
			surfman->fbdev.pixz=surface->zseq;

			/* C.7 writeFB imgbuf */
			egi_subimg_writeFB(imgbuf, &surfman->fbdev, 0, -1,  surfshmem->x0, surfshmem->y0);

			/* C.7A. If status downhold, draw a rim to highlight it. */
			if( surfshmem->status == SURFACE_STATUS_DOWNHOLD
			   || surfshmem->status == SURFACE_STATUS_ADJUSTSIZE ) //RIGHTADJUST )
			{
				fbset_color2(&surfman->fbdev, WEGI_COLOR_GRAY);
				draw_wrect( &surfman->fbdev, surfshmem->x0, surfshmem->y0 ,
					    surfshmem->x0+surfshmem->vw-1, surfshmem->y0+surfshmem->vh-1, 2); //w=2
			}

			/* C.7A Change mouse icon for surface status */
			/* If status rightadjust,  reset mouse icon. */
			if( i==TopDispSurfID ) {  /* ONLY FOR TOP LAYER SURFACE!  */
				if(surfshmem->status == SURFACE_STATUS_ADJUSTSIZE) { //RIGHTADJUST) {
					mdx=-15; mdy=-15;
					mrefimg = surfman->mgrab;
				}
				else {
					mdx=0; mdy=0;
					mrefimg = surfman->mcursor;
				}
			}

	/* ------ <<<  Surfshmem Critical Zone  */
			/* C.8 unlock surfshmem mutex */
			pthread_mutex_unlock(&surfshmem->shmem_mutex);

		} /* END for() traverse all surfaces */

		/* B.X Draw Wall paper, Here OR at begin */
		if(surfman->bkgimg) {
			surfman->fbdev.pixz=0;
			egi_subimg_writeFB(surfman->bkgimg, &surfman->fbdev, 0, -1, 0, 0);
		}
		else {  /* Black */
			surfman->fbdev.pixz=0;
			draw_filled_rect2(&surfman->fbdev, WEGI_COLOR_BLACK, 0,0, surfman->fbdev.pos_xres-1, surfman->fbdev.pos_yres-1 );
		}

		/* B.XX Draw minimized surfaces */
		surfman->fbdev.pixz=0; 		/* All minimized surfaces drawn just overlap bkground! */
		surfman->IndexMpMinSurf = -1;   /* Initial mouse NOT on minibar menu */
		for(i=0; i < surfman->mincnt; i++) {

			/* Get ref. to surfshmem */
			surfshmem=surfman->minsurfaces[i]->surfshmem;

			/* Check whether the mouse is on the left minibar menu */
			if( surfman->mx < MiniBarWidth && surfman->my < surfman->mincnt*MiniBarHeight ) {
				if( surfman_xyget_Zseq(surfman, surfman->mx, surfman->my)==0 ) /* Mouse on pixz=0 level */
					surfman->IndexMpMinSurf=surfman->my/MiniBarHeight;
			}

			/* Draw MiniBar Menu */
			draw_blend_filled_rect(&surfman->fbdev,0, i*MiniBarHeight, MiniBarWidth-1, (i+1)*MiniBarHeight,
					/* Set color for mouse pointed minibar menu */
	                                 i== surfman->IndexMpMinSurf ? WEGI_COLOR_DARKRED:WEGI_COLOR_DARKGRAY, 160);
			fbset_color2(&surfman->fbdev, WEGI_COLOR_GRAYB); /* Draw div. line */
			draw_wline(&surfman->fbdev, 0, (i+1)*MiniBarHeight, MiniBarWidth-1, (i+1)*MiniBarHeight, 2);

			/* Write surface Name on MiniBar Menu */
			if(surfshmem->surfname)
				pstr=surfshmem->surfname;
			else
				pstr="EGI_SURF";

		        FTsymbol_uft8strings_writeFB(&surfman->fbdev, egi_sysfonts.regular, /* FBdev, fontface */
                                       18, 18,(const UFT8_PCHAR)pstr,    /* fw,fh, pstr */
                                       MiniBarWidth-10-5, 1, 0,     	         /* pixpl, lines, fgap */
                                       10, i*30+5,	                 /* x0,y0, */
                                       WEGI_COLOR_WHITE, -1, 160,        /* fontcolor, transcolor,opaque */
                                       NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */
		}

		/* B.4 Draw cursor, disable zbuff and always make it at top. */
		if(mrefimg) {
			surfman->fbdev.zbuff_on = false;
					  /* img, fbdev, subnum,subcolor,x0,y0 */
			//egi_subimg_writeFB(surfman->mcursor, &surfman->fbdev, 0, -1, surfman->mx, surfman->my);
			egi_subimg_writeFB(mrefimg, &surfman->fbdev, 0, -1, surfman->mx + mdx, surfman->my +mdy);
			surfman->fbdev.zbuff_on = true;
		}

/* ------- <<<  Surfman Critical Zone  */
	        /* B.5 put mutex lock */
		pthread_mutex_unlock(&surfman->surfman_mutex);

		/* B.6 Render and bring to FB */
		fb_render(&surfman->fbdev);
		#ifdef LETS_NOTE
		tm_delayms(5);
		#else
		tm_delayms(60);
		#endif
	}

	/* A.4 Unlink imgbuf data */
	imgbuf->imgbuf=NULL;
	imgbuf->alpha=NULL;

	/* A.5 Free imgbuf */
	egi_imgbuf_free2(&imgbuf);

	/* A.6 Reset token */
	surfman->renderThread_on = false;

	return (void *)0;
}


#if 0
/*---------------------------------------------------------------
Write image of a surface to FB.

@surfman	Pointer to an EGI_SURFMAN
@surface	Pointer to an EGI_SURFACE

----------------------------------------------------------------*/
int surfman_display_surface(EGI_SURFMAN *surfman, EGI_SURFACE *surface)
{
        int i;

        if( surfman==NULL || surface==NULL )
		return -1;

        /* 1. To confirm FBDEV open and available */
        if( surfman->fbdev.fbfd <=0 ) {
                egi_dpstd("Surfman fbdev.fbfd is invalid!\n");
                return -2;
        }

}
#endif


/*---------------------------------------------------------------
Create an EGI_SURFBTN and blockcopy its imgbuf from input imgbuf.

@imgbuf		An EGI_IMGBUF holds image icons.
@xi,yi		Button image origin relative to above imgbuf.
@x0,y0		Button origin relative to surface imgbuf.
@w,h		Width and height of the button image.

Return:
	A pointer to EGI_SURFBTN	ok
	NULL				Fails
----------------------------------------------------------------*/
EGI_SURFBTN *egi_surfbtn_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h)
{
	EGI_SURFBTN	*sbtn=NULL;

	if(imgbuf==NULL)
		return NULL;

	/* w/h check inside egi_image functions */
	//if(w<1 || h<1)
	//	return NULL;

	/* Calloc SURFBTN */
	sbtn=calloc(1, sizeof(EGI_SURFBTN));
	if(sbtn==NULL)
		return NULL;

	/* To copy a block from imgbuf as button image */
	sbtn->imgbuf=egi_imgbuf_blockCopy(imgbuf, xi, yi, h, w);
	if(sbtn->imgbuf==NULL) {
		free(sbtn);
		return NULL;
	}

	/* Assign members */
	sbtn->x0=x0;
	sbtn->y0=y0;

	return sbtn;
}


/*---------------------------------------
	Free an EGI_SURFBTN.
---------------------------------------*/
void egi_surfbtn_free(EGI_SURFBTN **sbtn)
{
	if(sbtn==NULL || *sbtn==NULL)
		return;

	/* Free imgbuf */
	egi_imgbuf_free((*sbtn)->imgbuf);

	/* Free struct */
	free(*sbtn);

	*sbtn=NULL;
}


/*---------------------------------------
Check if point (x,y) in on the SURFBTN.

@sbtn	EGI_SURFBTN
@x,y	Point coordinate, relative to SURFACE!

Return:
	True or False.
----------------------------------------*/
inline bool egi_point_on_surfbtn(EGI_SURFBTN *sbtn, int x, int y)
{
	if(sbtn==NULL || sbtn->imgbuf==NULL)
		return false;

	if( x < sbtn->x0 || x > sbtn->x0 + sbtn->imgbuf->width )
		return false;
	if( y < sbtn->y0 || y > sbtn->y0 + sbtn->imgbuf->height )
		return false;

	return true;
}



	/* =======================     Functions for SURFUSER    ====================  */


/*-------------  For SURFUSER  -------------------
Parse mouse event and take actions.
Called by surfuser_input_routine()

@surfuser:   Pointer to EGI_SURFUSER.
@pmostat:    Pointer to EGI_MOUSE_STATUS
-------------------------------------------------*/
void surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	int i;
	static int lastX, lastY;
	bool	mouseOnBtn; 	/* Mouse on button */
	bool	first_touched;  /* If click on */

	/* Ref. pointers */
	FBDEV           *vfbdev;
	//EGI_IMGBUF      *imgbuf;
	EGI_SURFSHMEM   *surfshmem;
	EGI_SURFBTN	*sbtns[3];

	if(surfuser==NULL || pmostat==NULL)
		return;

	/* TODO the lastX/Y MAY be values of last time TOP surface. */
#if 0	/* Check ering_ON, Reset lastX/Y.  NOT synch with
	 * NOTE:
	 */
	if( !ering_ON ) {
		printf("ering_ON!\n");
		lastX=pmostat->mouseX;
		lastY=pmostat->mouseY;
		ering_ON=true;
	}
#endif

	/* 1. Get reference pointer */
        vfbdev=&surfuser->vfbdev;
        //imgbuf=surfuser->imgbuf;
        surfshmem=surfuser->surfshmem;
	sbtns[0]=surfshmem->sbtns[0];
	sbtns[1]=surfshmem->sbtns[1];
	sbtns[2]=surfshmem->sbtns[2];

       	pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surfman Critical Zone  */

	/* 2. Check if mouse on SURFBTNs of topbar. */
	if( !pmostat->LeftKeyDownHold ) {	/* To rule out DownHold! Example: mouse DownHold move/slide to topbar. */
	    for(i=0; i<3; i++) {

		/* A. Check mouse_on_button */
		if( surfshmem->sbtns[i] ) {
			mouseOnBtn=egi_point_on_surfbtn( sbtns[i], pmostat->mouseX -surfuser->surfshmem->x0,
								pmostat->mouseY -surfuser->surfshmem->y0 );
		}

		#if 0 /* TEST: --------- */
		if(mouseOnBtn)
			printf(">>> Touch SURFBTN: %d\n", i);
		else
			printf("<<<< \n");
		#endif

		/* B. If the mouse just moves onto a SURFBTN */
		if( surfshmem->mpbtn != i && mouseOnBtn ) {  /* XXX surfshmem->mpbtn <0 ! */
			/* B.1 In case mouse move from a nearby SURFBTN, restore its button image. */
			if(surfshmem->mpbtn>=0) {
				egi_subimg_writeFB(sbtns[surfshmem->mpbtn]->imgbuf, &surfuser->vfbdev,
							0, -1, sbtns[surfshmem->mpbtn]->x0, sbtns[surfshmem->mpbtn]->y0);
			}

			/* B.2 Draw a square on the newly touched SURFBTN */
			#if 0
			fbset_color2(&surfuser->vfbdev, WEGI_COLOR_DARKRED);
			draw_rect(&surfuser->vfbdev, sbtns[i]->x0, sbtns[i]->y0,
				     sbtns[i]->x0+sbtns[i]->imgbuf->width-1,sbtns[i]->y0+sbtns[i]->imgbuf->height-1);
		        #else
                        draw_blend_filled_rect(&surfuser->vfbdev, sbtns[i]->x0, sbtns[i]->y0,
						sbtns[i]->x0+sbtns[i]->imgbuf->width-1,sbtns[i]->y0+sbtns[i]->imgbuf->height-1,
						WEGI_COLOR_WHITE, 120);
		        #endif

			/* B.3 Set surfshmem->mpbtn as button index */
			surfshmem->mpbtn=i;

			/* B.4 Break for() */
			break;
		}
		/* C. If the mouse leaves SURFBTN: Clear surfshmem->mpbtn */
		else if( surfshmem->mpbtn == i && !mouseOnBtn ) {  /* XXX surfshmem->mpbtn >=0!  */

			/* C.1 Draw/Restor original image */
			egi_subimg_writeFB(sbtns[surfshmem->mpbtn]->imgbuf, &surfuser->vfbdev, 0, -1, sbtns[surfshmem->mpbtn]->x0, sbtns[surfshmem->mpbtn]->y0);

			/* C.2 Clear surfshmem->mpbtn */
			surfshmem->mpbtn=-1;

			/* C.3 Break for() */
			break;
		}
		/* D. Still on the BUTTON, sustain... */
		else if( surfshmem->mpbtn == i && mouseOnBtn ) {
			break;
		}

	   } /* END For() */
	}

	/* 3. If LeftClick OR?? released on SURFBTNs */
	if( pmostat->LeftKeyDown ) {
		egi_dpstd("LeftKeyDown\n");
//	if( pmostat->LeftKeyUp ) {
//		egi_dpstd("LeftKeyUp\n");
		/* If any SURFBTN is touched, do reaction! */
		switch(surfshmem->mpbtn) {
			case SURFBTN_CLOSE:
				if( surfshmem->close_surface )
					surfshmem->close_surface(&surfuser);
				break;
			case SURFBTN_MINIMIZE:
				/* Restore original sbtns image */
				//egi_subimg_writeFB(sbtns[surfshmem->mpbtn]->imgbuf, &surfuser->vfbdev,
				//			0, -1, sbtns[surfshmem->mpbtn]->x0, sbtns[surfshmem->mpbtn]->y0);

				if( surfshmem->minimize_surface )
					surfshmem->minimize_surface(surfuser);

				break;
			case SURFBTN_MAXIMIZE:
				if( surfshmem->redraw_surface ) {  /* max./nor. need redraw_surface function */
					/* Maximize */
					if( surfshmem->status != SURFACE_STATUS_MAXIMIZED ) {
						if( surfshmem->maximize_surface )
							surfshmem->maximize_surface(surfuser);
					}
					/* Normalize */
					else {
						if( surfshmem->normalize_surface )
							surfshmem->normalize_surface(surfuser);
					}
				}
				break;
		}

                 /* Update lastX/Y, to compare with next mouseX/Y to get deviation. */
		 lastX=pmostat->mouseX; lastY=pmostat->mouseY;
	}

        /* 4. LeftKeyDownHold: To move surface OR Adjust surface size. */
	/* Note: If You click a minimized surface on the MiniBar and keep downhold, then mostat LeftKeyDownHold
    	 *	 will be passed here immediately as the surface is brought to TOP. In this case, the lastX/Y here
	 *	 is JUST the coordinate of last time mouse_clicking point on SURFBTN_MINMIZE! and now it certainly
	 *	 will restore position of the surface as RELATIVE to the mouse as last time clicking on SURFBTN_MINIMIZE!
	 *       So you will see the mouse is upon the SURFBTN_MINMIZE just exactly at the same postion as before!
	 *
	 *	 --- OK, modify test_surfman.c:  Skip first LeftKeyDown mostat, and move codes of 'sending mostat to the surface'
	 *	to the end of mouse_even_processing.
	 */
        if( pmostat->LeftKeyDownHold && surfshmem->mpbtn<0 ) {
		printf("--DH--\n");
        	/* Note: As mouseDX/DY already added into mouseX/Y, and mouseX/Y have Limit Value also.
                 * If use mouseDX/DY, the cursor will slip away at four sides of LCD, as Limit Value applys for mouseX/Y,
                 * while mouseDX/DY do NOT has limits!!!
                 * We need to retrive DX/DY from previous mouseX/Y.
		 * ---OK, Modify egi_input.c
                 */

		/* Case 1. Downhold on topbar to move the surface
		 * A. If mouse is on surface topbar, then update x0,y0 to adjust its position
		 * B. Exclude status ADJUSTSIZE, to avoid ADJUSTSIZE mouse moving up to topbar.
		 */
		if( surfshmem->status != SURFACE_STATUS_ADJUSTSIZE
		    && lastX > surfshmem->x0 && lastX < surfshmem->x0+surfshmem->vw
		    && lastY > surfshmem->y0 && lastY < surfshmem->y0+30 ) /* TopBarWidth 30 */
		{
			printf(" --1-- \n");
//		fbset_color2(vfbdev,WEGI_COLOR_RED);
//		draw_filled_circle(vfbdev, pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0, 10);
			/* Update surface x0,y0 */
			surfshmem->x0 += pmostat->mouseDX; // (pmostat->mouseX-lastX); /* Delt x0 */
			surfshmem->y0 += pmostat->mouseDY; // (pmostat->mouseY-lastY); /* Delt y0 */

			/* Set status DOWNHOLD
			 ** NOTE:
			 *  After movement the status will be set as SURFACE_STATUS_NORMAL!
			 */

			surfshmem->status = SURFACE_STATUS_DOWNHOLD;
		}

		/* PRE 2-.  Check redraw_surface() function */
		else if( surfshmem->redraw_surface==NULL ) {
			goto  END_ADJUST;
		}

		/* Case 2. Downhold on RightDownCorner:  To adjust size of surface
		 * Effective edge width 15 pixs.
		 */
		else if( lastX > surfshmem->x0 + surfshmem->vw - 15  && lastX < surfshmem->x0 + surfshmem->vw
			 && lastY > surfshmem->y0 + surfshmem->vh - 15  && lastY < surfshmem->y0 + surfshmem->vh )
		{
			printf(" --2--\n");
//		fbset_color2(vfbdev,WEGI_COLOR_GREEN);
//		draw_filled_circle(vfbdev, pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0, 10);

			/* Set status */
			surfshmem->status = SURFACE_STATUS_ADJUSTSIZE; //RIGHTADJUST;

			/* Update surface size */
			surfshmem->vw += pmostat->mouseDX; // (pmostat->mouseX-lastX);
			surfshmem->vh += pmostat->mouseDY; // (pmostat->mouseY-lastY);

			if( surfshmem->vw > surfshmem->maxW )   /* MaxW Limit */
				surfshmem->vw=surfshmem->maxW;
			if( surfshmem->vw < 30 )		/* MinW limit */
				surfshmem->vw = 30;

			if( surfshmem->vh > surfshmem->maxH )   /* MaxH Limit */
				surfshmem->vh=surfshmem->maxH;
			if( surfshmem->vh < 30+15 )		/* MinH limit */
				surfshmem->vh = 30+15;

			/* Resize and redraw surface */
			if(pmostat->mouseDX || pmostat->mouseDY) {
				if(surfshmem->redraw_surface)
					surfshmem->redraw_surface(surfuser, surfshmem->vw,  surfshmem->vh);
			}
		}

		/* Case 3. Downhold on RightEdge: To adjust size of surface
		 * Effective edge width 10 pixs.
		 */
		else if( lastX > surfshmem->x0 + surfshmem->vw - 15  && lastX < surfshmem->x0 + surfshmem->vw
			 && lastY > surfshmem->y0 + 30  && lastY < surfshmem->y0 + surfshmem->vh )
		{
			printf(" --3--\n");
//		fbset_color2(vfbdev,WEGI_COLOR_BLUE);
//		draw_filled_circle(vfbdev, pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0, 10);
			/* Set status RIGHTADJUST */
			surfshmem->status = SURFACE_STATUS_ADJUSTSIZE; //RIGHTADJUST;

			/* Update surface size */
			surfshmem->vw += pmostat->mouseDX; //(pmostat->mouseX-lastX); /* Delt x0 */

			if( surfshmem->vw > surfshmem->maxW )   /* Max Limit */
				surfshmem->vw=surfshmem->maxW;
			if( surfshmem->vw < 30 )		/* Min limit */
				surfshmem->vw = 30;

			/* Resize and redraw surface */
			if(pmostat->mouseDX || pmostat->mouseDY)
				if(surfshmem->redraw_surface)
					surfshmem->redraw_surface(surfuser, surfshmem->vw,  surfshmem->vh);
		}

                /* Update lastX,Y */
		lastX=pmostat->mouseX; lastY=pmostat->mouseY;

		/* To skip drawing on workarea */
		goto END_FUNC;
	}
	/* 4.A ELSE: !LeftKeyDownHold: Restore status to NORMAL */
	else {
//		printf("--- Release DH ---\n");
		if( surfshmem->status == SURFACE_STATUS_DOWNHOLD
		    || surfshmem->status == SURFACE_STATUS_ADJUSTSIZE)  //RIGHTADJUST )
		{
			/* Token as MIXMIZED */
			if( surfshmem->nw >0 )
				surfshmem->status = SURFACE_STATUS_MAXIMIZED;
			else
				surfshmem->status = SURFACE_STATUS_NORMAL;
		}
	}

END_ADJUST:

	/* 5. If click on surface work area.
	 * Note: If above case 3 click to maximize the surface, then the click will pass down and draw a dot here!
	 * 	 so we need to check mpbtn. OR move case_5 just after case_3.
	 */
	if(  surfshmem->mpbtn<0 && pmostat->LeftKeyDown && pmostat->mouseY-surfshmem->y0 > 30) {

        	fbset_color2(vfbdev, egi_color_random(color_all));
		draw_filled_circle(vfbdev, pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0, 10);
	}


END_FUNC:
/* ------ <<<  Surface shmem Critical Zone  */
        	pthread_mutex_unlock(&surfshmem->shmem_mutex);

}

/*---------------------------------------------------
Move a surface by updating its origin(x0,y0).

@surfuser:	Pointer to EGI_SURFUSER.
@x0,y0:		New origin coordinate relatvie to SYS.

----------------------------------------------------*/
void surfuser_move_surface(EGI_SURFUSER *surfuser, int x0, int y0)
{
	if(surfuser==NULL || surfuser->surfshmem==NULL)
                return;

	surfuser->surfshmem->x0=x0;
	surfuser->surfshmem->y0=y0;
}


/*---------------------------------------------------
     Adjust surface size, redraw surface imgbuf.

@surfuser:	Pointer to EGI_SURFUSER.
@w,h:		New size of surface imgbuf.

----------------------------------------------------*/
void surfuser_redraw_surface(EGI_SURFUSER *surfuser, int w, int h)
{
	if(surfuser==NULL || surfuser->surfshmem==NULL)
		return;

	/* 1. Size limit */
	if(w<1 || h<1)
		return;

	if( w > surfuser->surfshmem->maxW )
		w=surfuser->surfshmem->maxW;

	if( h > surfuser->surfshmem->maxH )
		h=surfuser->surfshmem->maxH;

//     		pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surfman Critical Zone  */

	/* OK, Since the caller gets mutex_lock, no need here... */

	/* 2. Ref. pointers */
	FBDEV  *vfbdev=&surfuser->vfbdev;
	EGI_IMGBUF *surfimg=surfuser->imgbuf;
        EGI_SURFSHMEM *surfshmem=surfuser->surfshmem;
	EGI_SURFBTN *sbtns[3];
	sbtns[0]=surfshmem->sbtns[0];
	sbtns[1]=surfshmem->sbtns[1];
	sbtns[2]=surfshmem->sbtns[2];

	/* 3. Adjust surfimg width and height, BUT not its memspace. */
	surfimg->width=w;
	surfimg->height=h;

	/* 4. Reinit virtual FBDEV */
	reinit_virt_fbdev(vfbdev, surfimg);

	/* 5. Adjust surfshmem param vw/vh */
	surfshmem->vw=w;
	surfshmem->vh=h;

	/* 6. Redraw content */

        /* 6. Reset bkg color */
        egi_imgbuf_resetColorAlpha(surfimg, surfshmem->bkgcolor, -1); /* Reset color only */

        /* 6.1 Draw top bar */
        draw_filled_rect2(vfbdev, WEGI_COLOR_GRAY5, 0,0, surfimg->width-1, 30);

	/* 6.2 Draw outline rim */
	fbset_color2(vfbdev, WEGI_COLOR_GRAY); //BLACK);
	draw_rect(vfbdev, 0,0, surfshmem->vw-1, surfshmem->vh-1);

        /* 6.3 Put surface name/title. */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, /* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)surfshmem->surfname,   /* fw,fh, pstr */
                                        320, 1, 0,                        /* pixpl, lines, fgap */
                                        5, 5,                         /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */

        /* 6.4 Draw CLOSE/MIN/MAX Btns */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular,       /* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)"X _ O",   /* fw,fh, pstr */
                                        320, 1, 0,                      /* pixpl, lines, fgap */
                                        surfimg->width-20*3, 5,         /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */

	/* 6.5 Reset topbar SURFBTNs: CLOSE/MIN/MAX */
        int xs=surfimg->width-65;
	if(sbtns[SURFBTN_CLOSE]) {
		sbtns[SURFBTN_CLOSE]->x0 = xs+2;
		egi_subimg_writeFB( sbtns[SURFBTN_CLOSE]->imgbuf, vfbdev, 0, -1,
							sbtns[SURFBTN_CLOSE]->x0, sbtns[SURFBTN_CLOSE]->y0);
	}
	if(sbtns[SURFBTN_MINIMIZE]) {
		sbtns[SURFBTN_MINIMIZE]->x0 = xs+2+18;
		egi_subimg_writeFB( sbtns[SURFBTN_MINIMIZE]->imgbuf, vfbdev, 0, -1,
							sbtns[SURFBTN_MINIMIZE]->x0, sbtns[SURFBTN_MINIMIZE]->y0);
	}
	if(sbtns[SURFBTN_MAXIMIZE]) {
		sbtns[SURFBTN_MAXIMIZE]->x0 = xs+5+18*2;
		egi_subimg_writeFB( sbtns[SURFBTN_MAXIMIZE]->imgbuf, vfbdev, 0, -1,
							sbtns[SURFBTN_MAXIMIZE]->x0, sbtns[SURFBTN_MAXIMIZE]->y0);
	}

//       	pthread_mutex_unlock(&surfshmem->shmem_mutex);
/* <<< ------  Surfman Critical Zone  */

}


/*---------------------------------------------------
Maximize a surface by resize the surface to maxW/maxH.
And set its status to SURFACE_STATUS_MAXIMIZED.

@surfuser:	Pointer to EGI_SURFUSER.
----------------------------------------------------*/
void surfuser_maximize_surface(EGI_SURFUSER *surfuser)
{
	if(surfuser==NULL || surfuser->surfshmem==NULL)
		return;

	/* Get ref pointer of surfshmem */
	EGI_SURFSHMEM *surfshmem=surfuser->surfshmem;

       	/* Save size and origin coordinate, for normalize later. */
        surfshmem->nx=surfshmem->x0;
        surfshmem->ny=surfshmem->y0;
        surfshmem->nw = surfshmem->vw;
        surfshmem->nh = surfshmem->vh;

        /* Reset x0,y0 */
        surfshmem->x0=0;
        surfshmem->y0=0;

        /* Max. surface size, surfshmem->vw/vh will be adjusted. */
	if(surfshmem->redraw_surface)
        	surfshmem->redraw_surface(surfuser, surfshmem->maxW, surfshmem->maxH);

        /* Update status */
        surfshmem->status = SURFACE_STATUS_MAXIMIZED;
}


/*--------------------------------------------------------
Normalize a surface by resize it to last time saved nw/nh.
And set its status to SURFACE_STATUS_NORMAL.

@surfuser:	Pointer to EGI_SURFUSER.
---------------------------------------------------------*/
void surfuser_normalize_surface(EGI_SURFUSER *surfuser)
{
	if(surfuser==NULL || surfuser->surfshmem==NULL)
		return;

	/* Get ref pointer of surfshmem */
	EGI_SURFSHMEM *surfshmem=surfuser->surfshmem;

        /* Restor size and origin */
        surfshmem->x0=surfshmem->nx;
        surfshmem->y0=surfshmem->ny;
        surfshmem->vw = surfshmem->nw;
        surfshmem->vh = surfshmem->nh;

	if(surfshmem->redraw_surface)
        	surfshmem->redraw_surface(surfuser, surfshmem->vw, surfshmem->vh);

        /* Update status */
        surfshmem->status = SURFACE_STATUS_NORMAL;

        /* Reset nw, nh, also as token for STATUS_NORMAL */
        surfshmem->nw =0;
        surfshmem->nh =0;
}


/*-------------------------------------------------------------------
Minimize a surface by setting its status to SURFACE_STATUS_MINIMIZED.
The surface will then be put to Minibar menu by SURFMAN.

@surfuser:      Pointer to EGI_SURFUSER.
-------------------------------------------------------------------*/
void surfuser_minimize_surface(EGI_SURFUSER *surfuser)
{
        if(surfuser==NULL || surfuser->surfshmem==NULL)
                return;

	EGI_SURFBTN *sbtnMIN=surfuser->surfshmem->sbtns[SURFBTN_MINIMIZE];

        /* Restore to original sbtn image. OR next time when brought to TOP, old btn image keeps! */
	if(sbtnMIN)
	        egi_subimg_writeFB(sbtnMIN->imgbuf, &surfuser->vfbdev, 0, -1, sbtnMIN->x0, sbtnMIN->y0);

        /* Set status to be SURFACE_STATUS_MINIMIZED */
        surfuser->surfshmem->status=SURFACE_STATUS_MINIMIZED;

}


/*-------------------------------------------------
Close a surface.
Here just put a signal to end main loop!

@surfuser:      Pointer to EGI_SURFUSER.
-------------------------------------------------*/
void surfuser_close_surface(EGI_SURFUSER **surfuser)
{
        if( surfuser==NULL || (*surfuser)==NULL )
                return;

	/* Free and release. */

#if 0   /** XXX NOPE!
	 *  Instead, to end thread_eringRoutine by usersig 1
	 */
        if( egi_unregister_surfuser(surfuser)!=0 )
                egi_dpstd("Fail to unregister surfuser!\n");
#endif

	/* Just put signal to end main loop! */
	(*surfuser)->surfshmem->usersig =1;

	/* Shut down sockfd to quit ering */
        /* To avoid thread_EringRoutine be blocked at ering_msg_recv(), JUST brutly close sockfd
         * to force the thread to quit.
         */
        printf("Shutdown surfuser->uclit->sockfd!\n");
        if(shutdown( (*surfuser)->uclit->sockfd, SHUT_RDWR)!=0)
		egi_dperr("close sockfd");

	/* Abrupt exit */
        //exit(0);
}
