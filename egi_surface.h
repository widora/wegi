/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
---------------------------------------------------------------------*/
#ifndef __EGI_SURFACE_H__
#define __EGI_SURFACE_H__

#include <stdio.h>
#include <stdbool.h>
#include <sys/un.h>
#include <sys/socket.h>
#include "egi_unet.h"
#include "egi_fbdev.h"


#define ERING_PATH_SURFMAN	"/tmp/.egi/ering_surfman"


/*** Note
 *  Openwrt has no wrapper function of memfd_create() in the C library.
int unet_destroy_Uclient(EGI_UCLIT **uclit ) *  and NOT include head file  <sys/memfd.h>
 *  syscall(SYS_memfd_create, ...) needs following option keywords.
 *  "However,  when  using syscall() to make a system call, the caller might need to handle architecture-dependent details; this
 *   requirement is most commonly encountered on certain 32-bit architectures." man-syscall
 */
#ifndef  MFD_CLOEXEC
 #define SYSCALL_MEMFD_CREATE
 /* flags for memfd_create(2) (unsigned int) */
 #define MFD_CLOEXEC             0x0001U
 #define MFD_ALLOW_SEALING       0x0002U
#endif

typedef struct egi_surface 		EGI_SURFACE;
typedef struct egi_surface_shmem 	EGI_SURFSHMEM;
typedef struct egi_surface_manager	EGI_SURFMAN;
typedef struct egi_surface_user		EGI_SURFUSER;


/* Surface color data type */
typedef enum surf_color_type		SURF_COLOR_TYPE;
/* !!! WARNING !!! To keep consistent with static int surf_colortype_pixsize[] */
enum surf_color_type {
	SURF_RGB565	=0,	/* (NOW: As default) No alpha data:  surf->color[] */
	SURF_RGB565_A8	=1,	/* Independent alpah data, 8bits for each pixel:  surf->color[] + surf->alpah[] */
	SURF_RGB888	=2,
	SURF_RGB888_A8	=3,
	SURF_RGBA8888	=4,	/* With integrated alpha data: surf->color[] */
	SURF_COLOR_MAX 	=5,     /*  This is also as end token, see surf_get_pixsize() */
};


/*** 			--- An EGI_SURFUSER ---
 */
struct egi_surface_user {
	EGI_UCLIT 	*uclit;		/* A uClient to EGI_SURFMAN's uServer. */
	EGI_SURFSHMEM	*surfshmem;	/* Shared memory data in a surface, to release by egi_munmap_surfshmem() */
	FBDEV  		vfbdev;		/* Virtual FBDEV, statically allocated. */
	EGI_IMGBUF 	*imgbuf;	/* imgbuf for vfbdev;
					 * Link it to surface->color[] data to virtual FBDEV */
};


/*** 			--- An EGI_SURFMAN ---
 *
 * 1. When an EGI_SURMAN is created, there are 3 threads running for routine jobs.
 *    1.1 userv_listen_thread:
 *	  1.1.1 To accept and add surfusers.
 *    1.2 surfman_request_process_thread(void *surfman):
 *    	  1.2.1 To create and register surfaces, upon surfuser's request.
 *	  1.2.2 To retire surfuser if it disconnets.
 *    1.3 surfman_render_thread:
 *	  1.3.1 To render all surfaces.
 */
struct egi_surface_manager {
	/* 1. Surface user manager */
	EGI_USERV	*userv;		/* A userver, to accept clients
					 * Each userv->sessions[] stands for a surface user (surfuser).
					 * One surface user may register more than 1 surface.
					 */
		/* ----( EGI_USERV )---
        	 * 1.1 Userv->sessions[] to be accepted/added by userv_listen_thread(void *userv);
		 * 1.2 Userv->sessions[] to be retired by surfman_request_process_thread(void *surfman);
		 */

        int              cmd;           /* cmd to surfman, NOW: 1 to end acpthread & renderThread! */

        /* 2. Surfman request processing: create and register surfaces.  */
	pthread_t		repThread;	/* Request_processing thread */
        bool                    repThread_on;   /*  rpthread IS running! */
        int                     scnt;           /* Active/nonNULL surfaces counter */

#define SURFMAN_MAX_SURFACES	8	/* TODO:  A list to manage all surfaces */
	EGI_SURFACE		*surfaces[SURFMAN_MAX_SURFACES];  /* It's sorted in ascending order of their zseq values! */
	// const		EGI_SURFACE		*pslist[SURFMAN_MAX_SURFACES];	/* Only a reference */
	pthread_mutex_t 	 surfman_mutex; /* 1. If there's a thread accessing surfaces[i], it MUST NOT be freed/unregistered!
						 *    To lock/unlock for each surface!
						 */

	/*  Relate sessions[] with surfaces[], so when session ends, surfaces to be unregistered.
	 *  OK, NOW see: surfman_unregister_surfuser();
	 */

	/* 3. Surface render process */
	EGI_IMGBUF	*mcursor;	  /* Mouse cursor imgbuf */
	int		mx;		  /* Mouse cursor position */
	int		my;

	EGI_IMGBUF	*bkgimg;	  /* back ground image, as wall paper. */
	EGI_IMGBUF	*imgbuf;	  /* An empty imgbuf struct to temp. link with color/alpha data of a surface
					   * Draw the imgbuf to the fbdev then.
					   *  --- NOT Applied yet ---
					   */
	FBDEV 		fbdev;		  /* Init .devname = gv_fb_dev.devname, statically allocated.
					   * Zbuff ON.
				 	   */
	pthread_t	renderThread;	  /* Render processing thread */
	bool		renderThread_on;  /* renderThread is running */



};

/***  			--- EGI_SURFACE ---
 *   1. Mem space of the struct shall be allocated together.
 *   2. A surface pointer is create by mmap() memfd, it must use munmap() to free it.
 *   3. Normally, NO pointer member is allowed.
 *   4. An EGI_SURFACE is to be created, and rendered to FB by EGI_SURFMAN.
 *   5. Pixel data of an EGI_SURFACE is to be updated by an EGI_SURFACE application.
 *
 * TODO:
 *   1. If surface memfds does NOT reach the surface user, or keep inactive for a long time.
 *	check and remove it.
 *   2. Surfman data protection from Surfuser!  Spin off all private data to a new struct.
 *      --- OK Surfuser get EGI_SURFSHMEM pointer only
 */

/* ---- Common shared data --- */
struct egi_surface_shmem {
	size_t		shmsize;	/* Total memory allocated for the shmem
					 * EGI_SURFACE also holds the data!
					 */

	/* Mutex: PTHREAD_PROCESS_SHARED + PTHREAD_MUTEX_ROBUST */
	pthread_mutexattr_t mutexattr;	/* MUST also in the shared memory */
	pthread_mutex_t	mutex_lock; 	/* Porcess_shared mutex lock for surface common data. */
	int		usersig;	/* user signal */
	bool		hidden;		/* True: invisible. */

	/*  Denpend on colorType. */
//xxx   int          off_color;      // Offset to color data
				        /* NOW: No use! Access directly to pointer ->color[]
				        */

        int          off_alpha;      /* Offset to alpah data, ONLY if applicable.
					 * If 0: No alpha data. OR alpha included in color
					 */

	int		vw;	  	/* Adjusted surface size */
	int		vh;

	bool		sync;		/* True: surface image is ready. False: surface image is NOT ready. */
					/* TODO: More. NOW, only for activate the surface after surface is regitered AND
					 * image is ready! If NOT, a black(calloc raw memory) image may be brought to FB.
					 */
        /* Variable paramas, for Surfuser to write */
        int             x0;             /* [Surfuser]: Origin XY, relative to FB/LCD coord. */
        int             y0;

        /* Variable data, for APP to write */
        unsigned char   color[];		/*  [Surfuser] wirte, [Surfman] read. */
        /** Followed by: ( see surfman_register_surface() to get off_alpha;
         *   colors[];
         *   alpha[];  ( If applicable )
         */
};

struct egi_surface {

/* ---- Surfman private data --- */
	unsigned int	id;		/* [Surfman RW]: surface ID, NOW as index of the surfman->surfaces[]
					 * !!! --- Volatile value --- !!!
					 * 1. It's volatile! as surfman->surfaces[] to is being sorted when a new
					 *    surface is registered/unregistered! Use memfd instead to locate the surface.
					 */
//	const char 	name[];

	int		csFD;		/* surf_userID, as of sessionID of userv->session[sessionID]. */

	int 		zseq;		/* 1. Sequence as of zbuffer, =0 for BackGround. For active surface: 1,2,3,4...
					 *    Each surface has a unique zseq, and all zseqs are in serial.
					 * 2. Surface with bigger zseq to be rendered ahead of smaller one.
					 * 3. To update when surfman->surfaces[] add/delete a surface, and keep Max_zseq = surfman.scnt!
					 *    The latest registered/added surface always holds the Max_zseq.
					 */

	int		memfd;		/* memfd for shmem of shurfshmem.  Keep until egi_destroy_surfman().
					 * 1. The surfuser's corresponding memfd is deleted after it mmaps to get the surface pointer.
					 * 2. File descriptor of the anonymous file that allocates memory space for the surface
					 *    To be closed by surfman_unregister_surface()
					 * 3. A unique value for each surface!
					 */
	size_t		shmsize;	/* in bytes, Total memory allocated for the struct
					 * EGI_SURFSHMEM also holds the data!
					 */

        /* Fixed params, only surfman can modify/update upon the APP request? */
        int             width;
        int             height;

	SURF_COLOR_TYPE colorType;
//xxx   int             pixsize;        /* [Surfman] */
					 /* sizeof EGI_16BIT_COLOR (default), EGI_24BIT_COLOR ...
					 * + sizeof alpha
					 * Include or exclude alpha value.
					 */
	EGI_SURFSHMEM	*surfshmem;	/* memfd_created memory, call egi_munmap_surfshmem() to unmap/release. */
};

enum ering_request_type {
        ERING_REQUEST_NONE      =0,
        ERING_REQUEST_EINPUT    =1,
        ERING_REQUEST_ESURFACE  =2,  /* Params: int x0, int y0, int w, int h, int pixsize */
};

enum ering_msg_type {
	ERING_SURFMAN_ERR	=0,
	ERING_SURFACE_BRINGTOP	=1, /* Surfman bring/retire client surface to/from the top */
	ERING_SURFACE_RETIRETOP =2,
	ERING_MOUSE_STATUS	=3,
};

enum ering_result_type {
        ERING_RESULT_OK         =0,
        ERING_RESULT_ERR        =1,
	ERING_MAX_LIMIT		=2,  	/* Surfaces/... Max limit */
z};

/* Functions for   --- EGI_SURFACE ---   */
int surf_get_pixsize(SURF_COLOR_TYPE colorType);

int egi_create_memfd(const char *name, size_t memsize);
EGI_SURFSHMEM *egi_mmap_surfshmem(int memfd); 		/* mmap(memfd) to get surface pointer */
int egi_munmap_surfshmem(EGI_SURFSHMEM **shmem);	/* unmap(surfshmem) */

int surface_compare_zseq(const EGI_SURFACE *eface1, const EGI_SURFACE *eface2);
void surface_insertSort_zseq(EGI_SURFACE **surfaces, int n); /* Re_assign all surface->id AND surface->zseq */
/* If the function is applied for quickSort, then above re_assignment  MUST NOT be included */
/* TODO: surface_quickSort_zseq() */


/* Functions for   --- EGI_SURFUSER ---   */
EGI_SURFUSER *egi_register_surfuser( const char *svrpath, int x0, int y0, int w, int h, SURF_COLOR_TYPE colorType );
int egi_unregister_surfuser(EGI_SURFUSER **surfuser);

/* Functions for   --- EGI_SURFMAN ---   */
EGI_SURFMAN *egi_create_surfman(const char *svrpath);	/* surfman_mutex, */
int egi_destroy_surfman(EGI_SURFMAN **surfman);		/* surfman_mutex, */

int surfman_register_surface( EGI_SURFMAN *surfman, int userID,		/* surfman_mutex, sort zseq, */
			      int x0, int y0, int w, int h, SURF_COLOR_TYPE colorType); /* ret index of surfman->surfaces[] */
int surfman_unregister_surface( EGI_SURFMAN *surfman, int surfID );	/* surfman_mutex, sort zseq, */
int surfman_bringtop_surface_nolock(EGI_SURFMAN *surfman, int surfID);	/* sort zseq, */
int surfman_bringtop_surface(EGI_SURFMAN *surfman, int surfID);		/* surfman_mutex, sort zseq, */

// static void* surfman_request_process_thread(void *surfman);
int surfman_unregister_surfUser(EGI_SURFMAN *surfman, int sessionID);	/* surfman_mutex, sort zseq, */

// static void * surfman_render_thread(void *surfman);			/* surfman_mutex */

int surfman_xyget_Zseq(EGI_SURFMAN *surfman, int x, int y);
int surfman_xyget_surfaceID(EGI_SURFMAN *surfman, int x, int y);

/* Functions for   --- EGI_RING ---   */
EGI_SURFSHMEM *ering_request_surface(int sockfd, int x0, int y0, int w, int h, SURF_COLOR_TYPE colorType);


#endif
