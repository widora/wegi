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
typedef struct egi_surface_manager	EGI_SURFMAN;
//typedef struct egi_surface_user	EGI_SURFUSER;

/*** 			--- EGI_SURMAN ---
 * 1. When an EGI_SURMAN is created, there are 2 threads running for routine jobs.
 *    1.1 userv_listen_thread:
 *	  1.1.1 To accept and add surfusers.
 *    1.2 surfman_request_process_thread(void *surfman):
 *    	  1.2.1 To create and register surfaces, upon surfuser's request.
 *	  1.2.2 To retire surfuser if it disconnets.
 *
 */
struct egi_surface_manager {
	EGI_USERV	*userv;		/* A userver, to accept clients
					 * Each userv->sessions[] stands for a surface user (surfuser).
					 * One surface user may register more than 1 surface.
					 */
	/* ----( EGI_USERV )---
         * 1. Userv->sessions[] to be accepted/added by userv_listen_thread(void *userv);
	 * 2. Userv->sessions[] to be retired by surfman_request_process_thread(void *surfman);
	 */

        int              cmd;           /* cmd to surfman, NOW: 1 to end acpthread! */

        /* Surfman request processing: create and register surfaces.  */
	pthread_t		repthread;	/* Request_processing thread */
        bool                    repthread_on;   /*  rpthread IS running! */
        int                     scnt;           /* Active/nonNULL surfaces counter */

#define SURFMAN_MAX_SURFACES	8	/* TODO:  A list to manage all surfaces */
	EGI_SURFACE		*surfaces[SURFMAN_MAX_SURFACES];

	/* TODO: To relate sessions[] with surfaces[], so when session ends, surfaces to be unregistered.
	 *	 OK, NOW see: surfman_unregister_surfuser();
	 */

	/* Surface render process */
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
 *   2. Surfman data protection from Surfusre!
 */
struct egi_surface {
	unsigned int	id;		/* [Surfman RW]: surface ID, NOW as index of the surfman->surfaces[] */
	int		csFD;		/* [Surfman RW]: surf_userID, as of sessionID of userv->session[sessionID]. */

	int 		zseq;		/* Sequence as of zbuffer */

	int		memfd;		/* [Surfman RW]: memfd as of the Surfman!
					 * File descriptor of the anonymous file that allocates memory space for the surface
					 * To be closed by surfman_unregister_surface()
					 */
	size_t		memsize;	/* [Surfman RW]: in bytes, Total memory allocated for the struct */


	int		usersig;	/* [Surfuser RW]: user signal */
	bool		hidden;		/* [Surfuser RW]: True: invisible. */

        /* Fixed params, only surfman can modify/update upon the APP request. */
        int             width;		/* [Surfman] */
        int             height;		/* [Surfman] */
        int             pixsize;        /* [Surfman]
					 * sizeof EGI_16BIT_COLOR (default), EGI_24BIT_COLOR ...
					 * + sizeof alpha
					 * Include or exclude alpha value.
					 */

	/* TODO&TDB: Necessary ??? */
        size_t          off_color;      /* Offset to color data */
        size_t          off_alpha;      /* Offset to alpah data
					 * If 0: No alpha data. OR alpha included in color
					 */

        /* Variable paramas, for Surfuser to write */
        int             x0;             /* [Surfuser]: Origin XY, relative to FB/LCD coord. */
        int             y0;

        /* Variable data, for APP to write */
        unsigned char   data[];		/*  [Surfuser] wirte, [Surfman] read. */
        /** Followed by:
         * unsigned char  colors[];
         * EGI_8BIT_ALPHA alpha[];
         */
};

enum ering_request_type {
        ERING_REQUEST_NONE      =0,
        ERING_REQUEST_EINPUT    =1,
        ERING_REQUEST_ESURFACE  =2,  /* Params: int x0, int y0, int w, int h, int pixsize */
};

enum ering_result_type {
        ERING_RESULT_OK         =0,
        ERING_RESULT_ERR        =1,
	ERING_MAX_LIMIT		=2,  	/* Surfaces/... Max limit */
};

/* Functions for   --- EGI_SURFACE ---   */
int egi_create_memfd(const char *name, size_t memsize);
EGI_SURFACE *egi_mmap_surface(int memfd); 		/* mmap(memfd) to get surface pointer */
int egi_munmap_surface(EGI_SURFACE **eface);		/* unmap(surface) */

/* Functions for   --- EGI_SURFMAN ---   */
EGI_SURFMAN *egi_create_surfman(const char *svrpath);
int egi_destroy_surfman(EGI_SURFMAN **surfman);

int surfman_register_surface(EGI_SURFMAN *surfman, int userID, int x0, int y0, int w, int h, int pixsize); /* ret index of surfman->surfaces[] */
int surfman_unregister_surface( EGI_SURFMAN *surfman, int surfID );

//static void* surfman_request_process_thread(void *surfman);
int surfman_unregister_surfUser(EGI_SURFMAN *surfman, int sessionID);

/* Functions for   --- EGI_RING ---   */
EGI_SURFACE *ering_request_surface(int sockfd, int x0, int y0, int w, int h, int pixsize);


#endif
