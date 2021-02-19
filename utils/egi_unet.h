/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
---------------------------------------------------------------------*/
#ifndef __EGI_UNET_H__
#define __EGI_UNET_H__

#include <stdio.h>
#include <stdbool.h>
#include <sys/un.h>
#include <sys/socket.h>


/* EGI_USERV_SESSION */
typedef struct egi_unet_server_session EGI_USERV_SESSION;
struct egi_unet_server_session {
        int             sessionID;               /* ID, ref. index of EGI_USERV_SESSION.sessions[], NOT as sequence number! */
        int             csFD;                    /* C/S session fd */
        struct          sockaddr_un addrCLIT;    /* Client address */
        bool            alive;                   /* flag on, if active/living, ?? race condition possible! check csFD. */
        int             cmd;                     /* Commad to session handler, NOW: 1 to end routine. */
};  /* At server side */


/* EGI_USERV */
typedef struct egi_unix_server EGI_USERV;
struct egi_unix_server {
        int                     sockfd;		/* accept fd, NONBLOCK */
        struct sockaddr_un      addrSERV;       /* Self address */

#define MAX_UNET_BACKLOG        16
        int                     backlog;        /* BACKLOG for accept(), init. in inet_create_Userver() as MAX_UNET_BACKLOG */
        int                     cmd;            /* NOW: 1 to end acpthread! */

        /* Listen/accept sessions */
	pthread_t		acpthread;	/* Listen/accept thread */
        bool                    acpthread_on;   /* acpthread IS running! */
        int                     ccnt;           /* Clients/Sessions counter */

#define MAX_UNET_CLIENTS     	8
        EGI_USERV_SESSION       sessions[MAX_UNET_CLIENTS];  	/* Sessions */
};

/* EGI_UCLIT */
typedef struct egi_unix_client EGI_UCLIT;
struct egi_unix_client {
        int                     sockfd;
        struct sockaddr_un      addrSERV;
};

EGI_USERV* unet_create_Userver(const char *svrpath);
int unet_destroy_Userver(EGI_USERV **userv );

EGI_UCLIT* unet_create_Uclient(const char *svrpath);
int unet_destroy_Uclient(EGI_UCLIT **uclit );


int unet_sendmsg(int sockfd,  struct msghdr *msg);
int unet_recvmsg(int sockfd,  struct msghdr *msg);


/////////////////////////////      EGI SURFACE      ////////////////////////////////

/*** Note
 *  Openwrt has no wrapper function of memfd_create() in the C library.
int unet_destroy_Uclient(EGI_UCLIT **uclit ) *  and NOT include head file  <sys/memfd.h>
 *  syscall(SYS_memfd_create, ...) needs following option keywords.
 *  "However,  when  using syscall() to make a system call, the caller might need to handle architecture-dependent details; this
 *   requirement is most commonly encountered on certain 32-bit architectures." man-syscall
 */
#ifndef  MFD_CLOEXEC
 /* flags for memfd_create(2) (unsigned int) */
 #define MFD_CLOEXEC             0x0001U
 #define MFD_ALLOW_SEALING       0x0002U
#endif


typedef struct egi_surface 		EGI_SURFACE;
typedef struct egi_surface_manager	EGI_SURFMAN;

struct egi_surface_manager {
	EGI_USERV	userv;
	/* Surface request process */
	/* Surface render process */
};

/***  			--- EGI_SURFACE ---
 *   1. Mem space of the struct shall be allocated together.
 *   2. A surface pointer is create by mmap() memfd, it must use munmap() to free it.
 *   3. Normally, NO pointer member is allowed.
 *   4. An EGI_SURFACE is to be created, and rendered to FB by EGI_SURFMAN.
 *   5. Pixel data of an EGI_SURFACE to is to be updated an EGI_SURFACE application.
 */
struct egi_surface {
	size_t		memsize;

        /* Variable paramas, for APP to write */
        int             x0;             /* Origin XY, relative to FB/LCD coord. */
        int             y0;

        /* Fixed params */
        int             width;
        int             height;
        int             pixsize;        /* sizeof EGI_16BIT_COLOR (default), EGI_24BIT_COLOR
					 * Include or exclude alpha value.
					 */

	/* TODO&TDB: Necessary ??? */
        size_t          off_color;      /* Offset to color data */
        size_t          off_alpha;      /* Offset to alpah data
					 * If 0: No alpha data. OR alpha included in color
					 */

        /* Variable data, for APP to write */
        unsigned char   data[];
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
};

/* Functions for EGI_RING and EGI_SURFACE  */
int egi_create_memfd(const char *name, size_t memsize);
EGI_SURFACE *egi_mmap_surface(int memfd);
int egi_munmap_surface(EGI_SURFACE **eface);

EGI_SURFACE *ering_request_surface(int sockfd, int x0, int y0, int w, int h, int pixsize);


#endif
