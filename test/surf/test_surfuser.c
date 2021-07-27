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

7. When ering is busy transfering key_inputs and mouse_event , mouse on topbar btn NOT react!

Journal
2021-03-17:
	1. Add surfuser_resize_surface().
2021-03-25:
	1. Apply SURFACE module's default operation functions for
	   CLOSE/MIN./MAX. buttons etc.
2021-06-08:
	1. Recive pmostat->ch and push to ptxt, then write to surface.
2021-06-09:
	1. Recive pmostat->chars and push to ptxt, then write to surface.

2021-06-18:
	1. my_mouse_event(): Test CONKEYs....
2021-06-22:
	1. my_mouse_event(): Test CONKEYs group.
2021-06-24:
	1. surfuser_ering_routine(): case ERING_SURFACE_CLOSE, surfman request to close the surface.
2021-07-01:
	1. Add top menus[].
2021-07-20:
	1. Apply my_close_surface()

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

//const char *pid_lock_file="/var/run/test_surfuser.pid";

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
/* SURF Menus */
const char *menu_names[] = { "File", "Option", "Help"};
enum {
        MENU_FILE       =0,
        MENU_OPTION     =1,
        MENU_HELP       =2,
        MENU_MAX        =3, /* <--- Limit */
};


/* For text buffer */
char ptxt[1024];	/* Text */
int poff;		/* Offset to ptxt */
EGI_16BIT_COLOR	fcolor=WEGI_COLOR_BLACK;
int fw=20, fh=20;
int fgap=4;


/* Apply SURFACE module default function */
void my_redraw_surface(EGI_SURFUSER *surfuser, int w, int h);
void  *surfuser_ering_routine(void *args);  /* For LOOP_TEST and ering_test, a little different from module default function. */
//void  surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat); /* shmem_mutex */
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);
void my_close_surface(EGI_SURFUSER *surfuer);

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

        /* <<<<<  EGI general initialization   >>>>>> */

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
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, NULL, x0, y0, SURF_MAXW,SURF_MAXH, sw, sh, colorType );
	}
	else
		/* Note: Too small vw/vh(for first_draw) will make outside topmenus[] BLACK! */
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, NULL, mat_random_range(SURF_MAXW-50)-50, mat_random_range(SURF_MAXH-50),
     	                                SURF_MAXW, SURF_MAXH,  140+mat_random_range(SURF_MAXW/2), 60+mat_random_range(SURF_MAXH/2), colorType );
	/* Retry 3 times */
	if(surfuser==NULL) {
		int static ntry=0;
		ntry++;
		if(ntry==3)
			exit(EXIT_FAILURE);
		sleep(1);
		goto START_TEST;
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	vfbdev=&surfuser->vfbdev;
	surfimg=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

        /* Get surface mutex_lock.  ..Usually it's OK to be ignored! */
        if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 ) {
        	egi_dperr("Fail to get mutex_lock for surface.");
		exit(EXIT_FAILURE);
        }
/* ------ >>>  Surface shmem Critical Zone  */

        /* 3. Assign surface OP functions, for CLOSE/MIN./MAX. buttons etc.
	 *    To be module default ones, OR user defined, OR leave it as NULL.
         */
	// Defualt: surfuser_ering_routine() calls surfuser_parse_mouse_event();
        surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	surfshmem->redraw_surface 	= my_redraw_surface;
	surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        surfshmem->close_surface 	= surfuser_close_surface;
        surfshmem->user_mouse_event     = my_mouse_event;
        //surfshmem->draw_canvas          = my_draw_canvas;
	surfshmem->user_close_surface   = my_close_surface;

	/* 4. Give a name for the surface. */
	if(pname)
		strncpy(surfshmem->surfname, pname, SURFNAME_MAX-1);
	else
		strncpy(surfshmem->surfname, "EGI_SURF", SURFNAME_MAX-1);

	/* 5. First draw surface, set up TOPBTNs and TOPMENUs. */
	surfshmem->bkgcolor=egi_color_random(color_all); /* OR default BLACK */
	surfshmem->topmenu_bkgcolor=egi_color_random(color_light);
	surfshmem->topmenu_hltbkgcolor=WEGI_COLOR_GRAYA;
	surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MAX|TOPBTN_MIN, MENU_MAX, menu_names); /* Default firstdraw operation */

	/* font color */
	fcolor=COLOR_COMPLEMENT_16BITS(surfshmem->bkgcolor);

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* XXX Create SURFBTNs by blockcopy SURFACE image, after first_drawing the surface! */

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

		surfshmem->flags &= (~SURFACE_FLAG_MEVENT);
		//sleep(1);
	}

#endif

        /* Free SURFBTNs, ---- OK, to be released by egi_unregister_surfuser()!  */
//        for(i=0; i<3; i++)
//                egi_surfBtn_free(&surfshmem->sbtns[i]);

	/* Post_0:	--- CAVEAT! ---
	 *  If the program happens to be trapped in a loop when surfshem->usersig==1 is invoked (click on X etc.),
      	 *  The coder MUST ensure that it can avoid a dead loop under such circumstance (by checking surfshem->usersig in the loop etc.)
	 *  OR the SURFMAN may unregister the surface while the SURFUSER still runs and holds resources!
  	 */

        /* Post_1: Join ering_routine  */
        /* To force eringRoutine to quit , for sockfd MAY be blocked at ering_msg_recv()! */
	tm_delayms(200); /* Let eringRoutine to try to exit by itself, at signal surfshmem->usersig =1 */
        if( surfshmem->eringRoutine_running ) {
                if( pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) !=0)
                        egi_dperr("Fail to pthread_setcancelstate eringRoutine, it MAY already quit!");
		/* Note:
   		 * 	" A thread's cancellation type, determined by pthread_setcanceltype(3), may be either asynchronous or
		 * 	  deferred (the default for new threads). " ---MAN pthread_setcancelstate
		 */
                if( pthread_cancel( surfshmem->thread_eringRoutine ) !=0)
                        egi_dperr( "Fail to pthread_cancel eringRoutine, it MAY already quit!");
                else
                        egi_dpstd("OK to cancel eringRoutine thread!\n");
        }
        /* Make sure mutex unlocked in pthread if any! */
	printf("Joint thread_eringRoutine...\n");
        if( pthread_join(surfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

	/* Post_2: Unregister and destroy surfuser */
	printf("Unregister surfuser...\n");
	if( egi_unregister_surfuser(&surfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

        /* <<<<<  EGI general release   >>>>>> */
        // Ignore;

	printf("Exit OK!\n");
	exit(0);
}


/*--------------------------------------------------------------------
                Mouse Event Callback
                (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event().
4. Some vars of pmostat->conkeys will NOT auto. reset!
--------------------------------------------------------------------*/
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	int k;
	char strtmp[128];


#if 0 /* --------- E1. Parse Keyboard Event ---------- */

   	/* ------TEST:  CONKEY group */
	/* Get CONKEYs input */
	if( pmostat->conkeys.nk >0 ) { // || pmostat->conkeys.asciikey ) {
		/* Clear ptxt */
		ptxt[0]=0;

		/* CONKEYs */
	        for(k=0; k < pmostat->conkeys.nk; k++) {
			/* If ASCII_conkey */
			if( pmostat->conkeys.conkeyseq[k] == CONKEYID_ASCII ) {
				sprintf(strtmp,"+Key%u", pmostat->conkeys.asciikey);
				strcat(ptxt, strtmp);
			}
			/* Else: SHIFT/ALT/CTRL/SUPER */
			else {
				if(ptxt[0]) strcat(ptxt, "+"); /* If Not first conkey, add '+'. */
				strcat(ptxt, CONKEY_NAME[ (int)(pmostat->conkeys.conkeyseq[k]) ] );
			}
	        }
#if 0		/* ASCII_Conkey */
		if( pmostat->conkeys.asciikey ) {
			sprintf(strtmp,"+Key%u", pmostat->conkeys.asciikey);
			strcat(ptxt, strtmp);
		}
#endif
		/* TODO: Here topbar BTNS will also be redrawn, and effect of mouse on top TBN will also be cleared! */
	        my_redraw_surface(surfuser, surfimg->width, surfimg->height);
	}
	/* Parse Function Key F1~F12, KEY_F1(59) ~ KEY_F10(68), KEY_F11(87), KEY_F12(88)  */
	else if( pmostat->conkeys.press_lastkey ) {
                /* Clear ptxt */
		ptxt[0]=0;

		if(pmostat->conkeys.lastkey > KEY_F1-1 && pmostat->conkeys.lastkey < KEY_F10+1 )
			sprintf(ptxt,"F%d", pmostat->conkeys.lastkey-KEY_F1+1);
		else if(pmostat->conkeys.lastkey > KEY_F11-1 && pmostat->conkeys.lastkey < KEY_F12+1 )
			sprintf(ptxt,"F%d", pmostat->conkeys.lastkey-KEY_F11+11);

		/* TODO: Here topbar BTNS will also be redrawn, and effect of mouse on top TBN will also be cleared! */
	        my_redraw_surface(surfuser, surfimg->width, surfimg->height);
	}
	/* Clear text on the surface */
	else if( ptxt[0] !=0 ) {
		 ptxt[0]=0;
	        my_redraw_surface(surfuser, surfimg->width, surfimg->height);
	}


#else /* --------- E2. Parse STDIN Input ---------- */

	/* Get ASCII input */
	if( pmostat->nch )
	{
		egi_dpstd("chars: %-32.32s, nch=%d\n", pmostat->chars, pmostat->nch);

		/* If Escape Sequence. TODO: A Escape sequence MAY be broken!!! and NOT in one ering msg.??  */
		if( pmostat->chars[0] == 033 ) {
			egi_dpstd("Escape sequence!\n");
		}
		/* Else: Printable ASCIIs +BACKSPACE */
		else {
		   /* Write each char in pmostat->chars[] into ptxt[] */
		   for(k=0; k< pmostat->nch; k++) {
			/* Write ch into text buffer */
			if( poff < 1024-1 ) {
				if( poff)
					poff--;  /* To earse last '|' */

				if( poff>0 && pmostat->chars[k] == 127  ) { /* Backsapce */
					poff -=1; /*  Ok, NOW poff -= 2 */
					ptxt[poff+1]='\0';
				}
				else
					ptxt[poff++]=pmostat->chars[k];

				/* Pust '|' at last */
				ptxt[poff++]='|';
			}
		   }

		   /* Redraw surface to update text.
		    * TODO: Here topbar BTNS will also be redrawn, and effect of mouse on top TBN will also be cleared! */

		   /* Note: Call FTsymbol_uft8strings_writeFB() is NOT enough, need to clear surface bkg first. */
		   my_redraw_surface(surfuser, surfimg->width, surfimg->height);
		}
	}


/* --------- E3. Parse Mouse Event ---------- */

#endif

}



/*---------------------------------------------------------------
Adjust surface size, redraw surface imgbuf.

@surfuser:      Pointer to EGI_SURFUSER.
@w,h:           New size of surface imgbuf.
---------------------------------------------------------------*/
void my_redraw_surface(EGI_SURFUSER *surfuser, int w, int h)
{
        /* Redraw default function first */
        surfuser_redraw_surface(surfuser, w, h); /* imgbuf w,h updated */

        /* Rewrite txt */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular,   /* FBdev, fontface */
                                        fw, fh, (const UFT8_PCHAR)ptxt, /* fw,fh, pstr */
                                        w-10, (h-SURF_TOPBAR_HEIGHT-10)/(fh+fgap) +1, fgap,  /* pixpl, lines, fgap */
                                        5, SURF_TOPBAR_HEIGHT+SURF_TOPMENU_HEIGHT+5,        /* x0,y0, */
                                        fcolor, -1, 255,  		/* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
}


/*-----------------------------------------------------
    SURFUSER's ERING routine thread.
A little different from module default routine function
, for LOOP_TEST and ering_test.
------------------------------------------------------*/
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

        /* Set indicator */
        surfuser->surfshmem->eringRoutine_running=true;

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

		egi_dpstd("Ering_msg_recv OK\n");

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
		       case ERING_SURFACE_CLOSE:
				egi_dpstd("Surfman request to close the surface!\n");
				surfuser->surfshmem->usersig =1;
				break;
		       case ERING_MOUSE_STATUS:
				mouse_status=(EGI_MOUSE_STATUS *)emsg->data;
				if(mouse_status->nch !=0)
					egi_dpstd("Input chars: %-32.32s\n", mouse_status->chars);
				egi_dpstd("MS(X,Y):%d,%d\n", mouse_status->mouseX, mouse_status->mouseY);
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

        /* Reset indicator */
        surfuser->surfshmem->eringRoutine_running=false;

	egi_dpstd("Exit thread.\n");
	return (void *)0;
}


/*-------------------------------------------------
User's code in surfuser_close_surface(), as for
surfshmem->user_close_surface().

@surfuser:      Pointer to EGI_SURFUSER.
-------------------------------------------------*/
void my_close_surface(EGI_SURFUSER *surfuer)
{
	int x0,y0;
	int sw=220, sh=140;

	x0=(SURF_MAXW-sw)/2;
	y0=(SURF_MAXH-sh)/2; /* sw, sh will adjusted in stdSurfConfimr(), MAY NOT fitted. */

        /* Unlock to let surfman read flags. */
        pthread_mutex_unlock(&surfuser->surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */

        /* Reset MEVENT to let SURFMAN continue to ering mevent. SURFMAN sets MEVENT before ering. --mutex_unlock! */
        surfuser->surfshmem->flags &= (~SURFACE_FLAG_MEVENT);

        /* Confirm to close, surfuser->retval =STDSURFCONFIRM_RET_OK if confirmed. */
        surfuser->retval = egi_crun_stdSurfConfirm( (UFT8_PCHAR)"Caution",
                                     (UFT8_PCHAR)"感谢测试TaskSurface! \n大佬,你确定要退出了吗?",
				     x0, y0, sw, sh);
                                     //surfuser->surfshmem->x0+50, surfuser->surfshmem->y0+50, 220, 100);

/* ------ <<<  Surface shmem Critical Zone  */
        pthread_mutex_lock(&surfuser->surfshmem->shmem_mutex);

}

