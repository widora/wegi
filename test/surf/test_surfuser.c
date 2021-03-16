/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Note:
1. When SURFMAN exits, ering_msg_recv() returns 0.

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


/* For SURFUSER */
EGI_SURFUSER     *surfuser=NULL;
EGI_SURFSHMEM    *surfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *vfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *surfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE colorType=SURF_RGB565;   /* surfuser->vfbdev color type */

/* SURFACE APPEARANCE SIZE */
#define 	SURF_TOPBAR_WIDTH	30

EGI_SURFBTN     *sbtns[3];  /* MIN,MAX,CLOSE */
static int      mpbtn=-1;   /* Mouse_pointed button index, <0 None! */

pthread_t       thread_EringRoutine;    /* SURFUSER ERING routine */
void            *surfuser_ering_routine(void *args);
void 		surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

static int sigQuit;

/* Signal handler for SurfUser */
void signal_handler(int signo)
{
	if(signo==SIGINT) {
		egi_dpstd("SIGINT to quit the process!\n");
		sigQuit=1;
	}
}


int main(int argc, char **argv)
{
	int i;
	int opt;
	char *delim=",";
	char saveargs[128];
	char *ps=&saveargs[0];

	int sw=0,sh=0; /* Width and Height of the surface */
	int x0=0,y0=0; /* Origin coord of the surface */
	char *pname;


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
	sigQuit=0;
	mpbtn=-1;

	/* 1. Register/Create a surfuser */
	if( sw!=0 && sh!=0) {
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, 320,240, sw, sh, colorType );
	}
	else
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, mat_random_range(320-50)-50, mat_random_range(240-50),
        	                                320,240,  140+mat_random_range(320/2), 60+mat_random_range(240/2), colorType );
	if(surfuser==NULL) {
		sleep(1);
		goto START_TEST;
//		exit(EXIT_FAILURE);
	}

	/* 2. Get ref. imgbuf and vfbdev */
	vfbdev=&surfuser->vfbdev;
	surfimg=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

        /* Get surface mutex_lock */
        if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 ) {
        	egi_dperr("Fail to get mutex_lock for surface.");
		exit(EXIT_FAILURE);
        }
/* ------ >>>  Surface shmem Critical Zone  */

	/* 3. Assign name to the surface */
	if(pname)
		strncpy(surfshmem->surfname, pname, SURFNAME_MAX-1);
	else
		pname="EGI_SURF";


	/* Set color */
	egi_imgbuf_resetColorAlpha(surfimg, egi_color_random(color_all), -1); /* Reset color only */

	/* Draw top bar */
	draw_filled_rect2(vfbdev,WEGI_COLOR_GRAY5, 0,0, surfimg->width-1, 30);

	/* Draw CLOSE/MIN/MAX Btns */
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
                                        NULL, NULL, NULL, NULL);        /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	#endif


	/* Draw a circle */
//	draw_circle2(vfbdev, surfimg->width/2, 60, 20, WEGI_COLOR_WHITE);

        /* Put title. */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, /* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)pname,   /* fw,fh, pstr */
                                        320, 1, 0,                        /* pixpl, lines, fgap */
                                        5, 5,                         /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

	/* Draw outline rim */
	fbset_color2(vfbdev, WEGI_COLOR_BLACK);
	draw_rect(vfbdev, 0,0, surfshmem->vw-1, surfshmem->vh-1);

	/* Activate image */
	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* Create SURFBTNs by blockcopy SURFACE image. */
	int xs=surfimg->width-65;
	sbtns[SURFBTN_CLOSE]=egi_surfbtn_create(surfimg,     xs+2,0+1,      xs+2,0+1,      18, 30-1); /* (imgbuf, xi, yi, x0, y0, w, h) */
	sbtns[SURFBTN_MINIMIZE]=egi_surfbtn_create(surfimg,  xs+2+18,0+1,   xs+2+18,0+1,   18, 30-1);
	sbtns[SURFBTN_MAXIMIZE]=egi_surfbtn_create(surfimg,  xs+5+18*2,0+1, xs+5+18*2,0+1, 18, 30-1);

	/* Start Ering routine */
	if( pthread_create(&thread_EringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}


#if 0	/* Main loop */
	while(1) {
		sleep(5);
	}

#else	/* LOOP TEST:------ */
		sleep(3);

		sigQuit=1;
		pthread_join(thread_EringRoutine, NULL);

		for(i=0; i<3; i++)
			egi_surfbtn_free(&sbtns[i]);

		egi_unregister_surfuser(&surfuser);
		goto START_TEST;
#endif

	/* Free buttons */
	for(i=0; i<3; i++)
		egi_surfbtn_free(&sbtns[i]);

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
	ERING_MSG *emsg=NULL;
	EGI_MOUSE_STATUS *mouse_status;
	int nrecv;

	/* Init ERING_MSG */
	emsg=ering_msg_init();
	if(emsg==NULL)
		return (void *)-1;

	while( !sigQuit ) {
		/* 1. Waiting for msg from SURFMAN */
	        //egi_dpstd("Waiting in recvmsg...\n");
		nrecv=ering_msg_recv(surfuser->uclit->sockfd, emsg);
		if(nrecv<0) {
                	egi_dpstd("unet_recvmsg() fails!\n");
			continue;
        	}
		/* SURFMAN disconnects */
		else if(nrecv==0)
			exit(EXIT_FAILURE);

	        /* 2. Parse ering messag */
        	switch(emsg->type) {
	               case ERING_SURFACE_BRINGTOP:
                        	egi_dpstd("Surface is brought to top!\n");
                	        break;
        	       case ERING_SURFACE_RETIRETOP:
                	        egi_dpstd("Surface is retired from top!\n");
                        	break;
		       case ERING_MOUSE_STATUS:
				mouse_status=(EGI_MOUSE_STATUS *)emsg->data;
				egi_dpstd("MS(X,Y):%d,%d\n", mouse_status->mouseX, mouse_status->mouseY);
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


/*-------------  For SURFUSER  -------------------
Parse mouse event and take actions.

Called by surfuser_input_routine()

@surfuser:   Pointer to EGI_SURFUSER.
@pmostat:    Pointer to EGI_MOUSE_STATUS

-----------------------------------------------*/
void surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	int i;
	static int lastX, lastY;
	bool	mouseOnBtn; /* Mouse on button */

	FBDEV           *vfbdev;
	//EGI_IMGBUF      *imgbuf;
	EGI_SURFSHMEM   *surfshmem;

	if(surfuser==NULL || pmostat==NULL)
		return;

	/* 1. Get reference pointer */
        vfbdev=&surfuser->vfbdev;
        //imgbuf=surfuser->imgbuf;
        surfshmem=surfuser->surfshmem;

       	pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surfman Critical Zone  */

	/* 2. Check if mouse on SURFBTNs */
	if( !pmostat->LeftKeyDownHold ) {	/* To rule out DownHold! */
	    for(i=0; i<3; i++) {

		/* A. Check mouse_on_button */
		mouseOnBtn=egi_point_on_surfbtn( sbtns[i], pmostat->mouseX -surfuser->surfshmem->x0,
								pmostat->mouseY -surfuser->surfshmem->y0 );
		#if 1 /* TEST: --------- */
		if(mouseOnBtn)
			printf(">>> Touch SURFBTN: %d\n", i);
		else
			printf("<<<< \n");
		#endif

		/* B. If the mouse just moves onto a SURFBTN */
		if( mpbtn != i && mouseOnBtn ) {  /* XXX mpbtn <0 ! */
			/* B.1 In case mouse move from a nearby SURFBTN, restore its image. */
			if(mpbtn>=0) {
				egi_subimg_writeFB(sbtns[mpbtn]->imgbuf, &surfuser->vfbdev, 0, -1, sbtns[mpbtn]->x0, sbtns[mpbtn]->y0);
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

			/* B.3 Set mpbtn as button index */
			mpbtn=i;

			/* B.4 Break for() */
			break;
		}
		/* C. If the mouse leaves SURFBTN: Clear mpbtn */
		else if( mpbtn == i && !mouseOnBtn ) {  /* XXX mpbtn >=0!  */

			/* C.1 Draw/Restor original image */
			egi_subimg_writeFB(sbtns[mpbtn]->imgbuf, &surfuser->vfbdev, 0, -1, sbtns[mpbtn]->x0, sbtns[mpbtn]->y0);

			/* C.2 Clear mpbtn */
			mpbtn=-1;

			/* C.3 Break for() */
			break;
		}
		/* D. Still on the BUTTON, sustain... */
		else if( mpbtn == i && mouseOnBtn) {
			break;
		}


	   } /* END For() */
	}

	/* 3. If LeftClick OR?? released on SURFBTNs */
	if( pmostat->LeftKeyDown ) {
		egi_dpstd("LeftKeyDown\n");
	//if( pmostat->LeftKeyUp ) {
		/* If any SURFBTN is touched, do reaction! */
		switch(mpbtn) {
			case SURFBTN_CLOSE:
				exit(0);
				break;
			case SURFBTN_MINIMIZE:
				/* Restor to original image */
				egi_subimg_writeFB(sbtns[mpbtn]->imgbuf, &surfuser->vfbdev, 0, -1, sbtns[mpbtn]->x0, sbtns[mpbtn]->y0);

				/* Set status to be SURFACE_STATUS_MINIMIZED */
				surfuser->surfshmem->status=SURFACE_STATUS_MINIMIZED;
				break;
			case SURFBTN_MAXIMIZE:
				break;
		}

                 /* Update lastX/Y, to compare with next mouseX/Y to get deviation. */
		 lastX=pmostat->mouseX; lastY=pmostat->mouseY;
	}

        /* 4. LeftKeyDownHold: To move surface */
	/* Note: If You click a minimized surface on the MiniBar and keep downhold, then mostat LeftKeyDownHold
    	 *	 will be passed here immediately as the surface is brought to TOP. In this case, the lastX/Y here
	 *	 is JUST the coordinate of last time mouse_clicking point on SURFBTN_MINMIZE! and now it certainly
	 *	 will restore position of the surface as RELATIVE to the mouse as last time clicking on SURFBTN_MINIMIZE!
	 *       So you will see the mouse is upon the SURFBTN_MINMIZE just exactly at the same postion as before!
	 *	 --- OK, modify test_surfman.c:  move codes of 'sending mostat to the surface' to the end of mouse_even_processing.
	 */
        if( pmostat->LeftKeyDownHold ) {
		printf("--DH--\n");
        	/* Note: As mouseDX/DY already added into mouseX/Y, and mouseX/Y have Limit Value also.
                 * If use mouseDX/DY, the cursor will slip away at four sides of LCD, as Limit Value applys for mouseX/Y,
                 * while mouseDX/DY do NOT has limits!!!
                 * We need to retrive DX/DY from previous mouseX/Y.
                 */
//        	pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surfman Critical Zone  */

		/* If mouse is on surface topbar, then update x0,y0 to adjust its position */
		if( lastX > surfshmem->x0 && lastX < surfshmem->x0+surfshmem->vw
				          && lastY > surfshmem->y0 && lastY < surfshmem->y0+30 ) /* TopBarWidth 30 */
		{
			/* Update surface x0,y0 */
			surfshmem->x0 += (pmostat->mouseX-lastX); /* Delt x0 */
			surfshmem->y0 += (pmostat->mouseY-lastY); /* Delt y0 */

			/* Set status DOWNHOLD */
			surfshmem->status = SURFACE_STATUS_DOWNHOLD;
		}
/* ------ <<<  Surface shmem Critical Zone  */
//        	pthread_mutex_unlock(&surfshmem->shmem_mutex);

                /* Update lastX,Y */
		lastX=pmostat->mouseX; lastY=pmostat->mouseY;
	}
	/* 4.A ELSE: !LeftKeyDownHold: Restore status to NORMAL */
	else {
		if( surfshmem->status == SURFACE_STATUS_DOWNHOLD )
			surfshmem->status = SURFACE_STATUS_NORMAL;
	}

	/* 5. If click on surface work area. */
	if( pmostat->LeftKeyDown && pmostat->mouseY-surfshmem->y0 > 30) {

        	fbset_color2(vfbdev, egi_color_random(color_all));
		draw_filled_circle(vfbdev, pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0, 10);
	}

/* ------ <<<  Surface shmem Critical Zone  */
        	pthread_mutex_unlock(&surfshmem->shmem_mutex);

}


/*---------------------------------------------------
Adjust surface size, redraw surface imgbuf.
----------------------------------------------------*/
void surfuser_resize_surface(EGI_SURFUSER *surfuser, int w, int h)
{
	if(surfuser==NULL)
		return;

	/* Size limit */
	if(w<1 || h<1)
		return;
	if( w > surfuser->surfshmem->maxW || h > surfuser->surfshmem->maxH )
		return;


}
