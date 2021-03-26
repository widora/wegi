/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A standard SURFUSER template, with CLOSE/MINIMIZE/MAXIMIZE buttons
on topbar, and adjustable work area.


Note:
1. When SURFMAN exits, ering_msg_recv() returns 0.
2. If the surface has NO chance to get to TOP layer, then it will
   always be blocked at ering_msg_recv() in surfuser_ering_routine()!
3. To apply mouseDX/DY instead of pmostat->mouseX/Y-lastX/Y can avoid last TOP surface
   mostat influence.


TODO:
1. To restrain movement of ADJUSTSIZE_mouse at limited range.
   ----Impossible, SURFMAN controls mostat. sustain.

2. XXX  After movement, the surface status will be restored as SURFACE_STATUS_NORMAL
   and its original status will be lost! (Example: MAXIMIZED )
   ---OK, if surfshmem->nw > 0; then restore status as SURFACE_STATUS_MAXIMIZED.

3. A maximized surface CAN NOT be moved!? --TDB.

4. When a surface is shifted out of TOP layer, its lastX/Y values are still hold!
   Example: If prior TOP surface is minimized/closed by mouse click and keep holding the mouse key down.
   It will influence next time mostat_parsing, as the LeftKeyDownHold mostat will passed to the
   newly selected TOP surface immediately (see test_surfman.c).
   < To reset lastX/lastY on ering_ON would NOT help, as pmostat and emsg BRINGTOP are sent
   by TWO threads, and they MAY reache in WRONG sequence/order! >
   So the saved lastX/Y and inherited LeftKeyDownHold mostat MAY trigger unwanted actions!!

	---- !!! CAVEAT: LeftKeyDownHold may be heri from the last TOP surface !!! ----

5. If a surface is brought to TOP, it will receive LeftKeyDownHold mostat at
   the same time ( just after LeftKeyDown for picking surface in test_surfman.c ).
   XXX Check mpbtn<0 to get rid of it. ( A minized surface usually keep last mpbtn>0 )

6. An idle mostat will keeping streaming from surfman!


Journal
2021-03-17:
	1. Add surfuser_resize_surface().
2021-03-25:
	1. Apply SURFACE module's default operation functions for
	   CLOSE/MIN./MAX. buttons etc.

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include "egi_timer.h"
#include "egi_color.h"
#include "egi_log.h"
#include "egi_debug.h"
#include "egi_math.h"
#include "egi_FTsymbol.h"
#include "egi_surface.h"
#include "egi_procman.h"
#include "egi_input.h"

#ifdef LETS_NOTE
	#define SURF_MAXW	800
	#define SURF_MAXH	600
#else
	#define SURF_MAXW	320
	#define SURF_MAXH	240
#endif



/* For SURFUSER */
EGI_SURFUSER     *surfuser=NULL;
EGI_SURFSHMEM    *surfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *vfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *surfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  colorType=SURF_RGB565;  /* surfuser->vfbdev color type */
EGI_16BIT_COLOR  bkgcolor;
bool		 ering_ON;		/* Surface on TOP, ERING is streaming input data. */
					/* XXX A LeftKeyDown as signal to start parse of mostat stream from ERING,
					 * Before that, all mostat received will be ignored. This is to get rid of
					 * LeftKeyDownHold following LeftKeyDown which is to pick the top surface.
					 */

/* ERING routine */
void            *surfuser_ering_routine(void *args);

	/* To use SURFACE module default function */
// void 	surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat); /* shmem_mutex */
// void 	surfuser_resize_surface(EGI_SURFUSER *surfuser, int w, int h);

/* Signal handler for SurfUser */
void signal_handler(int signo)
{
	if(signo==SIGINT) {
		egi_dpstd("SIGINT to quit the process!\n");
	}
}

/*----------------------------
	   MAIN()
-----------------------------*/
int main(int argc, char **argv)
{
	int i;
	int opt;
	char *delim=",";
	char saveargs[128];
	char *ps=&saveargs[0];

	int sw=0,sh=0; /* Width and Height of the surface */
	int x0=0,y0=0; /* Origin coord of the surface */
	char *pname=NULL;

#if 0	/* Start EGI log */
        if(egi_init_log("/mmc/test_surfman.log") != 0) {
                printf("Fail to init logger, quit.\n");
                exit(EXIT_FAILURE);
        }
        EGI_PLOG(LOGLV_INFO,"%s: Start logging...", argv[0]);
#endif

#if 1        /* Load freetype fonts */
        printf("FTsymbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0) {
                printf("Fail to load FT sysfonts, quit.\n");
                return -1;
        }
#endif

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hS:p:n:"))!=-1 ) {
                switch(opt) {
			case 'S':
				sw=atoi(strtok_r(optarg, delim, &ps));
				sh=atoi(strtok_r(NULL, delim, &ps));
				printf("Surface size W%dxH%d.\n", sw, sh);
				break;
			case 'p':
				x0=atoi(strtok_r(optarg, delim, &ps));
				y0=atoi(strtok_r(NULL, delim, &ps));
				printf("Surface origin (%d,%d).\n", x0,y0);
				break;
			case 'n':
				pname=optarg;
                        case 'h':
				printf("%s: [-hsc]\n", argv[0]);
				break;
		}
	}

	/* Set signal handler */
//	egi_common_sigAction(SIGINT, signal_handler);


START_TEST:

	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	if( sw!=0 && sh!=0) {
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, SURF_MAXW,SURF_MAXH, sw, sh, colorType );
	}
	else
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, mat_random_range(SURF_MAXW-50)-50, mat_random_range(SURF_MAXH-50),
     	                                SURF_MAXW, SURF_MAXH,  140+mat_random_range(SURF_MAXW/2), 60+mat_random_range(SURF_MAXH/2), colorType );
	if(surfuser==NULL) {
		sleep(1);
		goto START_TEST;
//		exit(EXIT_FAILURE);
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	vfbdev=&surfuser->vfbdev;
	surfimg=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

        /* Get surface mutex_lock */
        if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 ) {
        	egi_dperr("Fail to get mutex_lock for surface.");
		exit(EXIT_FAILURE);
        }
/* ------ >>>  Surface shmem Critical Zone  */

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons. */
        surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	surfshmem->redraw_surface 	= surfuser_redraw_surface;
	surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        surfshmem->close_surface 	= surfuser_close_surface;

	/* 3. Give a name for the surface. */
	if(pname)
		strncpy(surfshmem->surfname, pname, SURFNAME_MAX-1);
	else
		pname="EGI_SURF";

	/* 4. Draw surface */
	/* 4.1 Set BKG color */
	surfshmem->bkgcolor=egi_color_random(color_all);
	egi_imgbuf_resetColorAlpha(surfimg, surfshmem->bkgcolor, -1); /* Reset color only */

	/* 4.2 Draw top bar */
	draw_filled_rect2(vfbdev,WEGI_COLOR_GRAY5, 0,0, surfimg->width-1, 30);

	/* 4.3 Draw CLOSE/MIN/MAX Btns */
	#if 0 /* Geom ICON */
	fbset_color2(vfbdev, WEGI_COLOR_WHITE);
	draw_wrect(vfbdev, surfimg->width-1-(10+16), 12, surfimg->width-1-10, 12+15-1, 2); /* Max Icon */
	draw_wline_nc(vfbdev, surfimg->width-1-2*(10+16), 12+15-1, surfimg->width-1-(10+16+10), 12+15-1, 2); /* Min Icon */

	draw_wline(vfbdev, surfimg->width-1-3*(10+16), 12, surfimg->width-1-(3*(10+16)-16), 12+15-1, 2); /* Close Icon */
	draw_wline(vfbdev, surfimg->width-1-3*(10+16), 12+15-1, surfimg->width-1-(3*(10+16)-16), 12, 2); /* Close Icon */
	#else /* FTsymbol ICON */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular,       /* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)"X _ O",   /* fw,fh, pstr */
                                        320, 1, 0,                      /* pixpl, lines, fgap */
                                        surfimg->width-20*3, 5,         /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
	#endif


        /* 4.4 Put surface name/title. */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, /* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)pname,   /* fw,fh, pstr */
                                        320, 1, 0,                        /* pixpl, lines, fgap */
                                        5, 5,                         	  /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */

	/* 4.5 Draw outline rim */
	fbset_color2(vfbdev, WEGI_COLOR_GRAY); //BLACK);
	draw_rect(vfbdev, 0,0, surfshmem->vw-1, surfshmem->vh-1);


	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* 5. Create SURFBTNs by blockcopy SURFACE image, after drawing the surface! */
	int xs=surfimg->width-65;
	surfshmem->sbtns[SURFBTN_CLOSE]=egi_surfbtn_create(surfimg,     xs+2,0+1,      xs+2,0+1,      18, 30-1); /* (imgbuf, xi, yi, x0, y0, w, h) */
	surfshmem->sbtns[SURFBTN_MINIMIZE]=egi_surfbtn_create(surfimg,  xs+2+18,0+1,   xs+2+18,0+1,   18, 30-1);
	surfshmem->sbtns[SURFBTN_MAXIMIZE]=egi_surfbtn_create(surfimg,  xs+5+18*2,0+1, xs+5+18*2,0+1, 18, 30-1);

	/* 6. Start Ering routine */
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

	/* 7. Activate image */
	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);



#if 0	/* Main loop */
	while( surfshmem->usersig != 1 ) {
		sleep(5);

	}

#else	/* LOOP TEST: ------ */
		sleep(3);

		/* To avoid thread_EringRoutine be blocked at ering_msg_recv(), JUST brutly close sockfd
		 * to force the thread to quit.
		 */
		//printf("Close surfuser->uclit->sockfd!\n");
		//if(close(surfuser->uclit->sockfd)!=0)
		printf("Shutdown surfuser->uclit->sockfd!\n");
		if(shutdown(surfuser->uclit->sockfd, SHUT_RDWR)!=0)
			egi_dperr("close sockfd");

		surfshmem->usersig=1;
		printf("Wait to join EringRoutine...\n");
       		/* Make sure mutex unlocked in pthread if any! */
        	if( pthread_join(surfshmem->thread_eringRoutine, NULL)!=0 )
                	egi_dperr("Fail to join eringRoutine");

        	/* Free SURFBTNs */
        	for(i=0; i<3; i++)
                	egi_surfbtn_free(&surfshmem->sbtns[i]);

		/* Unregister and destroy surfuser */
		printf("Unregister surfsuer...\n");
		egi_unregister_surfuser(&surfuser);

		goto START_TEST;
#endif

        /* Free SURFBTNs */
        for(i=0; i<3; i++)
                egi_surfbtn_free(&surfshmem->sbtns[i]);

	/* Unregister and destroy surfuser */
	if( egi_unregister_surfuser(&surfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	exit(0);
}


/*------------------------------------
    SURFUSER's ERING routine thread.
------------------------------------*/
void *surfuser_ering_routine(void *args)
{
	EGI_SURFUSER *surfuser=NULL;
	ERING_MSG *emsg=NULL;
	EGI_MOUSE_STATUS *mouse_status; /* A ref pointer */
	int nrecv;

	/* Get surfuser pointer */
	surfuser=(EGI_SURFUSER *)args;
	if(surfuser==NULL)
		return (void *)-1;

	/* Init ERING_MSG */
	emsg=ering_msg_init();
	if(emsg==NULL)
		return (void *)-1;

	while( surfuser->surfshmem->usersig !=1  ) {
		/* 1. Waiting for msg from SURFMAN, BLOCKED here if NOT the top layer surface! */
//	        egi_dpstd("Waiting in recvmsg...\n");
		nrecv=ering_msg_recv(surfuser->uclit->sockfd, emsg);
		if(nrecv<0) {
                	egi_dpstd("unet_recvmsg() fails!\n");
			continue;
        	}
		/* SURFMAN disconnects */
		else if(nrecv==0) {
			return (void *)-1;
			//exit(EXIT_FAILURE);
		}

	        /* 2. Parse ering messag */
        	switch(emsg->type) {
			/* NOTE: Msg BRINGTOP and MOUSE_STATUS sent by TWO threads, and they MAY reaches NOT in right order! */
	               case ERING_SURFACE_BRINGTOP:
                        	egi_dpstd("Surface is brought to top!\n");
				ering_ON=false;	/* To get rid of LeftKeyDownHOld */
                	        break;
        	       case ERING_SURFACE_RETIRETOP:
                	        egi_dpstd("Surface is retired from top!\n");
                        	break;
		       case ERING_MOUSE_STATUS:
				mouse_status=(EGI_MOUSE_STATUS *)emsg->data;
//				egi_dpstd("MS(X,Y):%d,%d\n", mouse_status->mouseX, mouse_status->mouseY);
				/* Parse mouse event */
				surfuser_parse_mouse_event(surfuser,mouse_status);
				break;
	               default:
        	                egi_dpstd("Undefined MSG from the SURFMAN! data[0]=%d\n", emsg->data[0]);
        	}

	}

	/* Free EMSG */
	ering_msg_free(&emsg);

	return (void *)0;
}


