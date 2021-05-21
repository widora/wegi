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
#include <unistd.h>

#include "egi_timer.h"
#include "egi_color.h"
#include "egi_log.h"
#include "egi_debug.h"
#include "egi_math.h"
#include "egi_FTsymbol.h"
#include "egi_surface.h"
#include "egi_procman.h"
#include "egi_input.h"

#define LOOP_TEST	0	/* Surface birdth and death loop test */

/* For SURFUSER */
EGI_SURFUSER     *surfuser=NULL;
EGI_SURFSHMEM    *surfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *vfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *surfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  colorType=SURF_RGB565;  /* surfuser->vfbdev color type */
EGI_16BIT_COLOR  bkgcolor;
//bool		 ering_ON;		/* Surface on TOP, ERING is streaming input data. */
					/* XXX A LeftKeyDown as signal to start parse of mostat stream from ERING,
					 * Before that, all mostat received will be ignored. This is to get rid of
					 * LeftKeyDownHold following LeftKeyDown which is to pick the top surface.
					 */

/* Apply SURFACE module default function */
//void  *surfuser_ering_routine(void *args);
//void  surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat); /* shmem_mutex */

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

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons etc. */
	// Defualt: surfuser_ering_routine() calls surfuser_parse_mouse_event();
        surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	surfshmem->redraw_surface 	= surfuser_redraw_surface;
	surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        surfshmem->close_surface 	= surfuser_close_surface;

	/* 3. Give a name for the surface. */
	if(pname)
		strncpy(surfshmem->surfname, pname, SURFNAME_MAX-1);
	else
		strncpy(surfshmem->surfname, "EGI_SURF", SURFNAME_MAX-1);

	/* 4. First draw surface */
	surfshmem->bkgcolor=egi_color_random(color_all); /* OR default BLACK */
	surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MAX|TOPBTN_MIN); /* Default firstdraw operation */
//	surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MAX); /* Default firstdraw operation */
//	surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE); /* Default firstdraw operation */
//	surfuser_firstdraw_surface(surfuser, TOPBTN_MAX|TOPBTN_MIN); /* Default firstdraw operation */

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* XXX 5. Create SURFBTNs by blockcopy SURFACE image, after first_drawing the surface! */

	/* 6. Start Ering routine */
	printf("start ering routine...\n");
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

	/* 7. Activate image */
	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);


#if LOOP_TEST	/* Birth&Death LOOP TEST: ------ */
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

        	/* Free SURFBTNs, ---- OK, to be released by egi_unregister_surfuser()! */
        	//for(i=0; i<3; i++)
                //	egi_surfBtn_free(&surfshmem->sbtns[i]);

		/* Unregister and destroy surfuser */
		printf("Unregister surfsuer...\n");
		egi_unregister_surfuser(&surfuser);

		goto START_TEST;
#else

	/* Main loop */
	while( surfshmem->usersig != 1 ) {
		tm_delayms(100);
		//sleep(1);
	}

#endif

        /* Free SURFBTNs, ---- OK, to be released by egi_unregister_surfuser()!  */
//        for(i=0; i<3; i++)
//                egi_surfBtn_free(&surfshmem->sbtns[i]);

        /* Join ering_routine  */
        // surfuser)->surfshmem->usersig =1;  // Useless if thread is busy calling a BLOCKING function.
	printf("Cancel thread...\n");
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        /* Make sure mutex unlocked in pthread if any! */
	printf("Joint thread_eringRoutine...\n");
        if( pthread_join(surfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

	/* Unregister and destroy surfuser */
	printf("Unregister surfuser...\n");
	if( egi_unregister_surfuser(&surfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	printf("Exit OK!\n");
	exit(0);
}

#if 0 ///////////////////  Module Default Function  //////////////////////
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
	        //egi_dpstd("Waiting in recvmsg...\n");
		nrecv=ering_msg_recv(surfuser->uclit->sockfd, emsg);
		if(nrecv<0) {
                	egi_dpstd("unet_recvmsg() fails!\n");
			continue;
        	}
		/* SURFMAN disconnects */
		else if(nrecv==0) {
		    #if LOOP_TEST
			return (void *)-1;
		    #else
			egi_dperr("ering_msg_recv() nrecv==0!");
			exit(EXIT_FAILURE);
		    #endif
		}

	        /* 2. Parse ering messag */
        	switch(emsg->type) {
			/* NOTE: Msg BRINGTOP and MOUSE_STATUS sent by TWO threads, and they MAY reaches NOT in right order! */
	               case ERING_SURFACE_BRINGTOP:
                        	egi_dpstd("Surface is brought to top!\n");
				//ering_ON=false;	/* To get rid of LeftKeyDownHOld */
				surfuser->ering_bringTop=true; /* Canceled XXX As start of a new round of mevent session */
                	        break;
/* NOTE:
 *	0. Emsg MAY NOT reach to the surface in right sequence:
 *         As SURFMAN's renderThread(after one surface minimized, to pick next TOP surface and ering BRINGTOP)
 *         and SURFMAN's routine job ( ering mstat to the TOP surface. ) are two separated threads, and their ERING jobs
 *         are NOT syncronized! So it's NOT reliable to deem ERING_SURFACE_BRINGTOP emsg as start of a new mevent round!
 * 	1. If the mouse was on topbar of a surface when its last mevent session ended, and NOW the same surface is brought up to TOP
 *	   while previous TOP surface is minimized by clicking and keep_hold_down on TOPBTN_MIN.
 *	2. We SHOULD NOT use ERING_SURFACE_BRINGTOP esmg as the start signal of a new mevent round!!!
 *	3. Instead, we should use member of surfshmem , OR other mouse status...such as 'Release DH', 'Release...'
 *
 *	Study following cases:

  	XXX 1. Error: MOUSE_STATUS gets before BRINGTOP.
...
--DH--
 --1--
... ...
--DH--
 --1--
--- Release DH --- ( END  of Last Session )
--DH--			 <------ Should be after BRINGTOP emsg!
 --1--
surfuser_ering_routine(): Surface is brought to top!
surfuser_parse_mouse_event(): BringTop
surfuser_parse_mouse_event(): LeftKeyDown mpbtn=-1
--DH--
 --1--
--DH--
 --1--
 ... ...

  	XXX 2. Error: NO BRINGTOP emsg at all!
 ...
--- Release DH --- ( END of Last Session )
			 <------ Should get BRINGTOP emsg!
--DH--
 --1--
--DH--
 --1--
--DH--
 --1--
...
	VVV 3. Correct: MOUSE_STATUS gets after BRINGTOP.
...
--- Release DH --- ( END of Last Session )
surfuser_parse_mouse_event(): LeftKeyDown mpbtn=-1
--- Release DH ---
surfuser_ering_routine(): Surface is brought to top!
surfuser_parse_mouse_event(): BringTop
surfuser_parse_mouse_event(): Touch a BTN mpbtn=-1, i=0
--DH--
--DH--
--DH--
--DH--
--DH--
--DH--
--DH--
...
*/

        	       case ERING_SURFACE_RETIRETOP:
                	        egi_dpstd("Surface is retired from top!\n");
                        	break;
		       case ERING_MOUSE_STATUS:
				mouse_status=(EGI_MOUSE_STATUS *)emsg->data;
				//egi_dpstd("MS(X,Y):%d,%d\n", mouse_status->mouseX, mouse_status->mouseY);
				/* Parse mouse event */
				surfuser_parse_mouse_event(surfuser,mouse_status);  /* mutex_lock */
				/* Always reset MEVENT after parsing, to let SURFMAN continue to ering mevent. SURFMAN sets MEVENT before ering. */
				surfuser->surfshmem->flags &= (~SURFACE_FLAG_MEVENT);
				break;
	               default:
        	                egi_dpstd("Undefined MSG from the SURFMAN! data[0]=%d\n", emsg->data[0]);
        	}

	}

	/* Free EMSG */
	ering_msg_free(&emsg);

	egi_dpstd("Exit thread.\n");
	return (void *)0;
}

#endif //////////////////////////////////////////

