/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

	EGI SURFACE version of madplay.

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
#include "egi_surfcontrols.h"
#include "egi_procman.h"
#include "egi_input.h"


/* For SURFUSER */
EGI_SURFUSER     *surfuser=NULL;
EGI_SURFSHMEM    *surfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *vfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *surfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  colorType=SURF_RGB565;  /* surfuser->vfbdev color type */
EGI_16BIT_COLOR  bkgcolor;

/* For SURF Buttons */
EGI_IMGBUF *icons_normal;
EGI_IMGBUF *icons_effect;

enum {
	BTN_PREV 	=0,
	BTN_PLAY	=1,
	BTN_NEXT	=2,
	BTN_PAUSE	=3,
	BTN_MAX		=4, /* <--- Limit */
};
ESURF_BTN  * btns[BTN_MAX];
int btnW=50;
int btnH=50;
bool mouseOnBtn;
int mpbtn=-1;	/* Index of mouse touched button */

/* ERING routine */
void            *surfuser_ering_routine(void *args);

/* Apply SURFACE module default function */
// void  surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat); /* shmem_mutex */
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

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
	int i,j;
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

	/* Set signal handler */
//	egi_common_sigAction(SIGINT, signal_handler);

	/* Load Icons */
	icons_normal=egi_imgbuf_readfile("/mmc/gray_icons_normal.png");
	if( egi_imgbuf_setSubImgs(icons_normal, 12*5)!=0 ) {
		printf("Fail to setSubImgs for icons_normal!\n");
		exit(EXIT_FAILURE);
	}
	/* Set subimgs */
	for( i=0; i<5; i++ )
	   for(j=0; j<12; j++) {
		icons_normal->subimgs[i*12+j].x0= 25+j*75.5;		/* 25 margin, 75.5 Hdis */
		icons_normal->subimgs[i*12+j].y0= 145+i*73.5;		/* 73.5 Vdis */
		icons_normal->subimgs[i*12+j].w= 50;
		icons_normal->subimgs[i*12+j].h= 50;
	}

	icons_effect=egi_imgbuf_readfile("/mmc/gray_icons_effect.png");
	if( egi_imgbuf_setSubImgs(icons_effect, 12*5)!=0 ) {
		printf("Fail to setSubImgs for icons_effect!\n");
		exit(EXIT_FAILURE);
	}
	/* Set subimgs */
	for( i=0; i<5; i++ )
	   for(j=0; j<12; j++) {
		icons_effect->subimgs[i*12+j].x0= 25+j*75.5;		/* 25 Left_margin, 75.5 Hdis */
		icons_effect->subimgs[i*12+j].y0= 145+i*73.5;		/* 73.5 Vdis */
		icons_effect->subimgs[i*12+j].w= btnW;			/* 50x50 */
		icons_effect->subimgs[i*12+j].h= btnH;
	}

	/* Create buttons: ( imgbuf, xi, yi, x0, y0, w, h ) */
	btns[BTN_PREV]=egi_surfbtn_create(icons_normal, 25+7*75.5, 145+4*73.5, 1+22, SURF_TOPBAR_HEIGHT+10, btnW, btnH);
	btns[BTN_PLAY]=egi_surfbtn_create(icons_normal, 25+6*75.5, 145+3*73.5, 1+22*2+50, SURF_TOPBAR_HEIGHT+10, btnW, btnH);
	btns[BTN_NEXT]=egi_surfbtn_create(icons_normal, 25+6*75.5, 145+4*73.5, 1+22*3+50*2, SURF_TOPBAR_HEIGHT+10, btnW, btnH);
	/* Set imgbuf_effect */
	btns[BTN_PREV]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, 25+7*75.5, 145+4*73.5, btnW, btnH); /* inimg, px, py, height, width */
	btns[BTN_PLAY]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, 25+6*75.5, 145+3*73.5, btnW, btnH); /* inimg, px, py, height, width */
	btns[BTN_NEXT]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, 25+6*75.5, 145+4*73.5, btnW, btnH); /* inimg, px, py, height, width */
	/* imgbuf_pressed use mask */

	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	x0=0;y0=0;	sw=240; sh=160;
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, sw, sh, sw, sh, colorType ); /* Fixed size */
	if(surfuser==NULL) {
		printf("Fail to register surfuser!\n");
		exit(EXIT_FAILURE);
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
        surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	//surfshmem->redraw_surface 	= surfuser_redraw_surface;
	//surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	//surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        surfshmem->close_surface 	= surfuser_close_surface;

	surfshmem->user_mouse_event	= my_mouse_event;

	/* 4. Name for the surface. */
	pname="MadPlayer";
	strncpy(surfshmem->surfname, pname, SURFNAME_MAX-1);

	/* 5. First draw surface. */
	surfshmem->bkgcolor=WEGI_COLOR_DARKGRAY; /* OR default BLACK */
	surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MIN); /* Default firstdraw operation */

	/* Draw btns */
        egi_subimg_writeFB(btns[BTN_PREV]->imgbuf, vfbdev, 0, -1, btns[BTN_PREV]->x0, btns[BTN_PREV]->y0);
        egi_subimg_writeFB(btns[BTN_PLAY]->imgbuf, vfbdev, 0, -1, btns[BTN_PLAY]->x0, btns[BTN_PLAY]->y0);
        egi_subimg_writeFB(btns[BTN_NEXT]->imgbuf, vfbdev, 0, -1, btns[BTN_NEXT]->x0, btns[BTN_NEXT]->y0);

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

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


	/* Main loop */
	while( surfshmem->usersig != 1 ) {
		tm_delayms(100);
		//sleep(1);
	}

        /* Free SURFBTNs */
        for(i=0; i<3; i++)
                egi_surfbtn_free(&surfshmem->sbtns[i]);

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
			egi_dperr("ering_msg_recv() nrecv==0!");
			exit(EXIT_FAILURE);
		}

	        /* 2. Parse ering messag */
        	switch(emsg->type) {
			/* NOTE: Msg BRINGTOP and MOUSE_STATUS sent by TWO threads, and they MAY reaches NOT in right order! */
	               case ERING_SURFACE_BRINGTOP:
                        	egi_dpstd("Surface is brought to top!\n");
                	        break;
        	       case ERING_SURFACE_RETIRETOP:
                	        egi_dpstd("Surface is retired from top!\n");
                        	break;
		       case ERING_MOUSE_STATUS:
				mouse_status=(EGI_MOUSE_STATUS *)emsg->data;
				//egi_dpstd("MS(X,Y):%d,%d\n", mouse_status->mouseX, mouse_status->mouseY);
				/* Parse mouse event */
				surfuser_parse_mouse_event(surfuser,mouse_status);  /* mutex_lock */
				/* Always reset MEVENT after parsing, to let SURFMAN continue to ering mevent */
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


/*--------------------------------------------------------------
                Mouse Event Callback
             (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event() .
---------------------------------------------------------------*/
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
        int i;

        /* 1. !!!FIRST:  Check if mouse touches any button */
	for(i=0; i<BTN_MAX; i++) {
                /* A. Check mouse_on_button */
                if( btns[i] ) {
                        mouseOnBtn=egi_point_on_surfbtn( btns[i], pmostat->mouseX -surfuser->surfshmem->x0,
                                                                pmostat->mouseY -surfuser->surfshmem->y0 );
                }
//		else
//			continue;

                #if 1 /* TEST: --------- */
                if(mouseOnBtn)
                        printf(">>> Touch btns[%d].\n", i);
                #endif

		/* B. If the mouse just moves onto a SURFBTN */
	        if(  mouseOnBtn && mpbtn != i ) {
			egi_dpstd("Touch a BTN mpbtn=%d, i=%d\n", mpbtn, i);
                	/* B.1 In case mouse move from a nearby SURFBTN, restore its button image. */
	                if( mpbtn>=0 ) {
        	        	egi_subimg_writeFB(btns[mpbtn]->imgbuf, vfbdev,
	                	        	0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);
                	}
                        /* B.2 Put effect on the newly touched SURFBTN */
                        if( btns[i] ) {  /* ONLY IF sbtns[i] valid! */
			   #if 0 /* Mask */
	                        draw_blend_filled_rect(vfbdev, btns[i]->x0, btns[i]->y0,
        	                		btns[i]->x0+btns[i]->imgbuf->width-1,btns[i]->y0+btns[i]->imgbuf->height-1,
                	                	WEGI_COLOR_BLUE, 100);
			   #else /* imgbuf_effect */
				printf("Draw effect\n");
				egi_subimg_writeFB(btns[i]->imgbuf_effect, vfbdev,
                                                0, -1, btns[i]->x0, btns[i]->y0);
			   #endif
			}

                        /* B.3 Update mpbtn */
                        mpbtn=i;

                        /* B.4 Break for() */
                        break;
         	}
                /* C. If the mouse leaves SURFBTN: Clear mpbtn */
                else if( !mouseOnBtn && mpbtn == i ) {

                        /* C.1 Draw/Restor original image */
                        egi_subimg_writeFB(btns[mpbtn]->imgbuf, vfbdev, 0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);

                        /* C.2 Reset pressed and Clear mpbtn */
			btns[mpbtn]->pressed=false;
                        mpbtn=-1;

                        /* C.3 Break for() */
                        break;
                }
                /* D. Still on the BUTTON, sustain... */
                else if( mouseOnBtn && mpbtn == i ) {
                        break;
                }

	} /* END for() */

	/* NOW: mpbtn updated */

        /* 2. If LeftKeyDown(Click) on buttons */
        if( pmostat->LeftKeyDown && mouseOnBtn ) {
                egi_dpstd("LeftKeyDown mpbtn=%d\n", mpbtn);

		/* Same effect: Put mask */
		draw_blend_filled_rect(vfbdev, btns[mpbtn]->x0, btns[mpbtn]->y0,
        		btns[mpbtn]->x0+btns[mpbtn]->imgbuf->width-1,btns[mpbtn]->y0+btns[mpbtn]->imgbuf->height-1,
                	WEGI_COLOR_DARKGRAY, 150);

		/* Set pressed */
		btns[mpbtn]->pressed=true;

                /* If any SURFBTN is touched, do reaction! */
                switch(mpbtn) {
                        case BTN_PREV:
				break;
			case BTN_PLAY:
				break;
			case BTN_NEXT:
				break;
		}
	}

	/* 3. LeftKeyUp */
	if( pmostat->LeftKeyUp && mouseOnBtn) {
		if(btns[mpbtn]->pressed) {
			/* Resume button with normal image */
			//egi_subimg_writeFB(btns[mpbtn]->imgbuf, vfbdev, 0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);
			egi_subimg_writeFB(btns[mpbtn]->imgbuf_effect, vfbdev, 0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);
			btns[mpbtn]->pressed=false;
		}
	}
}
