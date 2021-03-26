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
#include "egi_input.h"

#define ERING_PATH_SURFMAN	"/tmp/.egi/ering_surfman"

/* Surface Appearance Geom */
#define         SURF_TOPBAR_WIDTH       30


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
typedef struct egi_surface_button	EGI_SURFBTN;

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

/***
			--- An EGI_SURFACE_BUTTON ---
 1. A simple button on surface.
*/
struct egi_surface_button {
	int		x0;	/* Button position relative to SURFACE */
	int		y0;
	EGI_IMGBUF	*imgbuf; /* To hold image */

	/* Reactions / Operations */
	// On_Touch:
	// On_Click:
	// On_dblClick:

};

EGI_SURFBTN *egi_surfbtn_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h);
void egi_surfbtn_free(EGI_SURFBTN **sbtn);
bool egi_point_on_surfbtn(EGI_SURFBTN *sbtn, int x, int y);

/* SURFBTN IDs */
enum surf_button_ids {
        SURFBTN_CLOSE           = 0,
        SURFBTN_MINIMIZE        = 1,
        SURFBTN_MAXIMIZE        = 2,
        /* More slef-defined IDs */
};


/*** 			--- An EGI_SURFUSER ---
 */
struct egi_surface_user {
	EGI_UCLIT 	*uclit;			/* A uClient to EGI_SURFMAN's uServer. */
	EGI_SURFSHMEM	*surfshmem;		/* Shared memory data in a surface, to release by egi_munmap_surfshmem() */

						/* TODO: More than 1 SURFACEs for a SURFUSER ???!
						 * 	1. means more surfshmem. ( and vfbdev? imgbuf?)
						 * 	2. This will complicate ering_msg_send() to each surface.
						 */

	FBDEV  		vfbdev;			/* Virtual FBDEV, statically allocated. */
	EGI_IMGBUF 	*imgbuf;		/* Imgbuf as linked to virtual FBDEV.
						 * 1. Only allocate a EGI_IMGBUF struct.
						 * 2. Its color/alpha data only a ref. pointer to surfshmem->color[]
						 * 3. Its width/height to be adjusted with surface resize/redraw operation.
						 */
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
 *
 * 2. The SURFMAN manages all mouse icons(imgbufs)! SURFSHMEM applys a certain mouse icon by setting its ref ID.
 * 3. The SURFMAN controls and dispatchs mouse data, always to the TOP surface only.
 *
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
					/* Note:  Define USERV_MAX_CLIENTS (8+1)!  >8 !!!
					 *        Then recvmsg() can pass 9th request to surfman, who can reply
					 *        request fails for 'ERING_MAX_LIMIT'!
					 */
	EGI_SURFACE		*surfaces[SURFMAN_MAX_SURFACES];  /* Pointers to surfaces
								   * Always keep in ascending order of their zseq values!
								   * whenever: 1. register OR unregister a surface.
								   *	       2. bring OR retire a surface to/from TOP layer.
								   */
	// const		EGI_SURFACE		*pslist[SURFMAN_MAX_SURFACES];	/* Only a reference */
	pthread_mutex_t 	 surfman_mutex; /* 1. If there's a thread accessing surfaces[i], it MUST NOT be freed/unregistered!
						 *    To lock/unlock for each surface!
						 */

	/*  Relate sessions[] with surfaces[], so when session ends, surfaces to be unregistered.
	 *  OK, NOW see: surfman_unregister_surfuser();
	 */

	EGI_SURFACE	*minsurfaces[SURFMAN_MAX_SURFACES];	/* Pointers to minimized surfaces
								 * 1. Push sequence: from [0] to  [SURFMAN_MAX_SURFACES]
								 * 2. It's updated each time before surfman renders surfaces[].
								 */
	int		mincnt;				/* Counter of minimized surfaces */
	int		IndexMpMinSurf;   /* Index of mouse pointed minsurfaces[], <0 NOT ON minibar! */

	/* 3. Surface render process */
	EGI_IMGBUF	*mcursor;	  /* Mouse cursor imgbuf -- NORMAL */
	EGI_IMGBUF	*mgrab;		  /* Mouse cursor --- GRAB */
					  /* TODO: An imgbuf array for cursor icons. All mouse imgbufs are managed by SURFMAN. */
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
 *   1. Space of SURFSHMEM is a chunk of anonymous shared memory created by the SURFMAN, who then
 *	ering the correspoinding memfd to the SURFUSER. ( Ref. create_memfd() )
 *   2. Since a SURFSHMEM pointer is created by mmap() memfd, it must use munmap() to free it.
 *   3. Normally, NO pointer member is allowed. ---OK, for SURFSUER use only.
 *   4. An EGI_SURFACE is to be created, and rendered to FB by EGI_SURFMAN.
 *   5. Pixel data of an EGI_SURFACE is to be updated by a SURFUSER.
 *   6. Most members of SURFSHMEM shall be updated ONLY by the SURFUSER, espcially pointers!
 *	The SURFMAN only initilize some common param memebers of SURFSHMEM.
 *
 * TODO:
 *   1. If surface memfds does NOT reach the surface user, or keep inactive for a long time.
 *	check and remove it.
 *   2. Surfman data protection from Surfuser!  Spin off all private data to a new struct.
 *      --- OK Surfuser get EGI_SURFSHMEM pointer only
 */

/* --- A surface commonly shared data : mostly for the SURFUSER --- */
struct egi_surface_shmem {

	#define SURFNAME_MAX	256
	char		surfname[SURFNAME_MAX];	 /* Name of the surface/application, which MAY appears on surface topbar */

	size_t		shmsize;	/* Total memory allocated for the shmem
					 * EGI_SURFACE also holds the data!
					 * For unmap shmem.
					 */

	/* Mutex: PTHREAD_PROCESS_SHARED + PTHREAD_MUTEX_ROBUST */
	pthread_mutexattr_t mutexattr;	/* MUST also in the shared memory */
	pthread_mutex_t	shmem_mutex; 	/* Porcess_shared mutex lock for surface common data. */

	int		status;		/* Surface status, CAN SET OR READ.
					 * 1. SURFMAN and SURFUSER CAN set the status.
					 * 2. surfman_render_thread will read the status!
					 */

	pthread_t       thread_eringRoutine;    /* SURFUSER ERING routine */
	int		usersig;	/* user signal: NOW 1 as quit surface ERING routine. */

	bool		hidden;		/* True: invisible. */
	bool		downhold;	/* If the surface is hold down ... XXX no use? */

//	bool		syncAtop;	/* */
	bool		sync;		/* True: surface image is ready. False: surface image is NOT ready. */
					/* TODO: More. NOW, only for activate the surface after surface is registered AND
					 * image is ready! If NOT, a black(calloc raw memory) image may be brought to FB.
					 */

        /* Surface Geomtrical Params */
        int             x0;             /* [Surfuser]: Origin XY, relative to FB/LCD coord. */
        int             y0;

	int		maxW;		/* Fixed Max surface size, width and height. Same as egi_surface.width and heigth */
	int		maxH;
					/* Note: If surface is MAXIMIZED, vw/vh keeps the same!
					 * SURFMAN will use registered width/height (mem alloc params) to render the surface.
					 */

	int		vw;	  	/* Adjusted surface size, To be limited within [1 maxW] and [1 maxH]*/
	int		vh;		/* SURFMAN will use this to render surface image */


	/* NOTE:
	 * 1. Normal origin and size for NORMALIZE,  when MAXIMIZE a surface we need to save its origin and size,
	 *    and restore them when later NORMALIZE it.
	 * 2. nw,nh are also tokens of status, when nw(nh)>0, the surface is MAXIMIZED! and if nw(nh)==0, it's NORMALIZED!
	 * 3. When the surface is moved from minibar menu to desk top, surfman_bringtop_surface(_nolock)() will check
	 *    nw(nh) to reset the stauts.
	 */
	int		nx;
	int		ny;
	int		nw;
	int		nh;

	/* Default buttons on topbar */
	EGI_SURFBTN     *sbtns[3];      /* MINIMIZE, MAXIMIZE, CLOSE. If NULL, ignore.  */
					/* NOW to be allocated/released by SURFUSER */
	int		mpbtn;		/* Index of mouse touched buttons, as index of sbtns[]
					 * <0 invalid.
					 */

	/*** Surface Operations: All by SURFUSER.
	 *   1. They are just pointers, MAY be initilized with default OP functions.
	 *   2. Usually, at least resize_surface() should be redefined and tailor_made for the application.
	 */
	/* 1. Move */
		// Just modify (x0,y0)
		void (*move_surface)(EGI_SURFUSER *surfuser, int x0, int y0);
	/* 2. Resize: When surface resizes, shall redraw all elemets in the surface accordingly. */
		void (*redraw_surface)(EGI_SURFUSER *surfuser, int w, int h);
	/* 3. Maximize */
		// Just call resize() with given w/h
		void (*maximize_surface)(EGI_SURFUSER *surfuser);
	/* 4. Normalize */
		void (*normalize_surface)(EGI_SURFUSER *surfuser);
	/* 5. Minimize */
		// Put surface to Minibar Menu.
		void (*minimize_surface)(EGI_SURFUSER *surfuser);
	/* 6. Close */
		void (*close_surface)(EGI_SURFUSER **surfuser);

	/* Surface default color, as backgroud */
	EGI_16BIT_COLOR	bkgcolor;

	/*  Denpend on colorType. */
//xxx   int          off_color;      // Offset to color data
				        /* NOW: No use! Access directly to pointer ->color[]
				        */

        int          off_alpha;      	/* Offset to alpah data, ONLY if applicable.
					 * If 0: No alpha data. OR alpha data included in color, SURF_RGBA8888 etc.
					 */


	/* At last: Imgbuf color data */
        unsigned char   color[];	/*  [Surfuser] wirte, [Surfman] read. */
        /** Followed by: ( see surfman_register_surface() to get off_alpha;
         *   colors[];
         *   alpha[];  ( If applicable )
         */
};

struct egi_surface {

/* ---- Surfman private data --- */
//	unsigned int	uid;		/* Surface uid, unique for each surface. */

	unsigned int	id;		/* [Surfman RW]: NOW as index of the surfman->surfaces[]
					 * !!! --- Volatile value --- !!!
					 * 1. It's volatile! as surfman->surfaces[] to is being sorted when a new
					 *    surface is registered/unregistered! Use memfd instead to locate the surface.
				         * 2. MUST update id accordingly as sequence/index of surfman->surfaces[] changes!
					 */
//	const char 	name[];		Replaced by SURFSHMEM.surfname[]

	int		csFD;		/* surf_userID, as of sessionID of userv->session[sessionID].
					 * Two surface MAY have same csFD. ?
					 * !!! test_surfman.c: Use csFD to identify a surface.
					 */

	int 		zseq;		/* 1. Sequence as of zbuffer, =0 for BackGround. For active surface: 1,2,3,4...
					 *    Each surface has a unique zseq, and all zseqs are in serial.
					 * 2. Surface with bigger zseq to be rendered ahead of smaller one.
					 * 3. To update when surfman->surfaces[] add/delete a surface, and keep Max_zseq = surfman.scnt!
					 *    The latest registered/added surface always holds the Max_zseq.
					 * 4. ALWAYAS keep surfman->surfaces[]->zseq in ascending order!
					 */

	int		memfd;		/* memfd for shmem of shurfshmem.  Keep until egi_destroy_surfman().
					 * 1. The surfuser's corresponding memfd is deleted after it mmaps to get the surface pointer.
					 * 2. File descriptor of the anonymous file that allocates memory space for the surface
					 *    To be closed by surfman_unregister_surface()
					 * 3. A unique value for each surface!
					 */
	size_t		shmsize;	/* in bytes, Total memory allocated for surfshmem.
					 * EGI_SURFSHMEM also holds the data!
					 * For unmap shmem.
					 */

        /* Fixed params, only surfman can modify/update upon the APP request? */
        int             width;		/* As MAX size of a surface, with mem space alloced in surfshmem. */
        int             height;

	SURF_COLOR_TYPE colorType;
//xxx   int             pixsize;        /* [Surfman] */
					 /* sizeof EGI_16BIT_COLOR (default), EGI_24BIT_COLOR ...
					 * + sizeof alpha
					 * Include or exclude alpha value.
					 */

//	int		status;		/* Surface status, CAN SET OR READ. */
					/* 1. SURFMAN and SURFUSER CAN set the status.
					 * 2. surfman_render_thread will read the status!
					 */

	/* Commonly shared data, operated by the SURFUSER mostly. */
	EGI_SURFSHMEM	*surfshmem;	/* memfd_created memory, call egi_munmap_surfshmem() to unmap/release. */
};

enum surface_status {
	SURFACE_STATUS_NORMAL	  	=1,
	SURFACE_STATUS_MINIMIZED  	=2,
	SURFACE_STATUS_MAXIMIZED  	=3,
	SURFACE_STATUS_DOWNHOLD   	=4,  	/* Downhold by mouse */
	SURFACE_STATUS_ADJUSTSIZE	=5,	/* Adjusting surface size */
	SURFACE_STATUS_RIGHTADJUST	=6,	/* Adjusting surface size by moving right edge */
	SURFACE_STATUS_DOWNADJUST	=7,
};


#if  0 /* NOTE: Following move to egi_unet.h */
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
};
#endif /* END */


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
EGI_SURFUSER *egi_register_surfuser( const char *svrpath, int x0, int y0, int maxW, int maxH, int w, int h, SURF_COLOR_TYPE colorType );
int egi_unregister_surfuser(EGI_SURFUSER **surfuser);

	/* Default surface operations */
__attribute__((weak)) void surfuser_move_surface(EGI_SURFUSER *surfuser, int x0, int y0);
__attribute__((weak)) void surfuser_redraw_surface(EGI_SURFUSER *surfuser, int w, int h);
__attribute__((weak)) void surfuser_maximize_surface(EGI_SURFUSER *surfuser);
__attribute__((weak)) void surfuser_normalize_surface(EGI_SURFUSER *surfuser);
__attribute__((weak)) void surfuser_minimize_surface(EGI_SURFUSER *surfuser);
__attribute__((weak)) void surfuser_close_surface(EGI_SURFUSER **surfuser);
__attribute__((weak)) void surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

/* Functions for   --- EGI_SURFMAN ---   */
EGI_SURFMAN *egi_create_surfman(const char *svrpath);	/* surfman_mutex, */
int egi_destroy_surfman(EGI_SURFMAN **surfman);		/* surfman_mutex, */

int surfman_register_surface( EGI_SURFMAN *surfman, int userID,		/* surfman_mutex, sort zseq, */
			      int x0, int y0, int MaxW, int MaxH, int w, int h, SURF_COLOR_TYPE colorType); /* ret index of surfman->surfaces[] */
int surfman_unregister_surface( EGI_SURFMAN *surfman, int surfID );	/* surfman_mutex, sort zseq, */
int surfman_bringtop_surface_nolock(EGI_SURFMAN *surfman, int surfID);	/* sort zseq, */
int surfman_bringtop_surface(EGI_SURFMAN *surfman, int surfID);		/* surfman_mutex, sort zseq, */

// static void* surfman_request_process_thread(void *surfman);
int surfman_unregister_surfUser(EGI_SURFMAN *surfman, int sessionID);	/* surfman_mutex, sort zseq, */
// static void * surfman_render_thread(void *surfman);			/* surfman_mutex */

int surfman_xyget_Zseq(EGI_SURFMAN *surfman, int x, int y);
int surfman_xyget_surfaceID(EGI_SURFMAN *surfman, int x, int y);
int surfman_get_TopDispSurfaceID(EGI_SURFMAN *surfman);

/* Functions for   --- EGI_RING ---   */
EGI_SURFSHMEM *ering_request_surface(int sockfd, int x0, int y0, int maxW, int maxH, int w, int h, SURF_COLOR_TYPE colorType);


#endif
