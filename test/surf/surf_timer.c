/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

TODO:
1. Add SURFUSER routine. Check whether the userver is disconnected.

Journal
2021-03-12:
	1. Add EGI_SURFBUTs for CLOSE/MIN/MAX.
	2. surfuser_ering_routine(), surfuser_parse_mouse_event().

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
#include "egi_debug.h"
#include "egi_math.h"
#include "egi_FTsymbol.h"
#include "egi_surface.h"
#include "egi_procman.h"
#include "egi_input.h"

EGI_SURFUSER	*surfuser=NULL;
EGI_SURFSHMEM	*surfshmem=NULL;	/* Only a ref. to surfuser->surfshmem */
FBDEV 		*vfbdev=NULL;		/* Only a ref. to surfuser->vfbdev  */
EGI_IMGBUF 	*surfimg=NULL;		/* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE colorType=SURF_RGB565;

enum surf_button_ids {
	BTNCLOSE_ID 	= 0,
	BTNMIN_ID 	= 1,
	BTNMAX_ID 	= 2,
};
EGI_SURFBTN	*sbtns[3];  /* MIN,MAX,CLOSE */
static int 	mpbtn=-1;   /* Mouse_pointed button index, <0 None! */

pthread_t thread_EringRoutine;
void *surfuser_ering_routine(void *args);
void surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

static int sigQuit;

/* Signal handler for SurfUser */
void signal_handler(int signo)
{
        if(signo==SIGINT) {
                egi_dpstd("SIGINT to quit the process!\n");
//              sigQuit=1;
//		pthread_mutex_unlock(&surfshmem->mutex_lock);
		exit(0);
        }
}


int main(int argc, char **argv)
{
	int opt;
	//char strtm[128];
        time_t t;
        struct tm *tmp;

        char *delim=",";
        char saveargs[128];
        char *ps=&saveargs[0];

        int sw=0,sh=0; /* Width and Height of the surface */
        int x0=0,y0=0; /* Origin coord of the surface */

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hS:p:"))!=-1 ) {
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
                        case 'h':
                                printf("%s: [-hsc]\n", argv[0]);
                                break;
                }
        }

        /* Set signal handler */
//       egi_common_sigAction(SIGINT, signal_handler);

        /* Load freetype fonts */
        printf("FTsymbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0) {
                printf("Fail to load FT sysfonts, quit.\n");
		exit(EXIT_FAILURE);
        }

	/* Create a surfuser */
//	surfuser=egi_register_surfuser(ERING_PATH, 30, 240-100, 260, 110, colorType);
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, 260, 110, colorType);
	if(surfuser==NULL)
		exit(EXIT_FAILURE);

	/* Get ref imgbuf and vfbdev */
	vfbdev=&surfuser->vfbdev;
	surfimg=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

        /* Assign name to the surface */
        strncpy(surfshmem->surfname, "时间", SURFNAME_MAX-1);

	/* Set BK color */
	egi_imgbuf_resetColorAlpha(surfimg, WEGI_COLOR_DARKBLUE, -1); /* Reset color only */
	/* Top bar */
	draw_filled_rect2(vfbdev,WEGI_COLOR_GRAY5, 0, 0, surfimg->width-1, 30);
#if 0	/* Max/Min icons */
	fbset_color2(vfbdev, WEGI_COLOR_GRAYA);
        draw_rect(vfbdev, surfimg->width-1-(10+16+10+16), 7, surfimg->width-1-(10+16+10), 5+16-1); /* Max Icon */
        draw_rect(vfbdev, surfimg->width-1-(10+16+10+16) +1, 7 +1, surfimg->width-1-(10+16+10) -1, 5+16-1 -1); /* Max Icon */
        draw_line(vfbdev, surfimg->width-1-(10+16), 15-1, surfimg->width-1-10, 15-1);     /* Mix Icon */
        draw_line(vfbdev, surfimg->width-1-(10+16), 15-1 -1, surfimg->width-1-10, 15-1 -1);       /* Mix Icon */
#endif

        /* Put Title */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 		/* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)"X _ O    EGI Timer",   /* fw,fh, pstr */
                                        320, 1, 0,                 	/* pixpl, lines, fgap */
                                        5, 5,                         	/* x0,y0, */
		                        WEGI_COLOR_WHITE, -1, 200,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */


	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("shmsize: %zdBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* Create SURFBTNs by blockcopy image */
	sbtns[BTNCLOSE_ID]=egi_surfbtn_create(surfimg,  2,0, 	   2,0,	    18, 30); /* (imgbuf, xi, yi, x0, y0, w, h) */
	sbtns[BTNMIN_ID]=egi_surfbtn_create(surfimg,    2+18,0,   2+18,0,   18, 30);
	sbtns[BTNMAX_ID]=egi_surfbtn_create(surfimg,    5+18*2,0, 5+18*2,0, 18, 30);

	/* Start Ering routine */
	if( pthread_create(&thread_EringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

	/* Loop */
	while(!sigQuit) {
		/* Get time */
		t = time(NULL);
           	tmp = localtime(&t);
	        if (tmp == NULL) {
               		perror("localtime");
               		exit(EXIT_FAILURE);
           	}

                /* Get surface mutex_lock */
		//printf("Mutex lock...");fflush(stdout);
                if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 ) {
                          egi_dperr("Fail to get mutex_lock for surface.");
			  tm_delayms(10);
                          continue;
                }
		//printf("OK\n");
/* ------ >>>  Surface shmem Critical Zone  */

		/* Clear pad */
		draw_filled_rect2(vfbdev,WEGI_COLOR_DARKOCEAN, 0, 30, surfimg->width-1, surfimg->height-1);

	        /* Update time */
        	FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, /* FBdev, fontface */
                	                        18, 18, (const UFT8_PCHAR)asctime(tmp),   /* fw,fh, pstr */
                        	                320, 1, 0,                        /* pixpl, lines, fgap */
                                	        15, 40,                         /* x0,y0, */
                                        	WEGI_COLOR_ORANGE, -1, 255,        /* fontcolor, transcolor,opaque */
	                                        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

        	FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 	/* FBdev, fontface */
                	                        24, 24, (const UFT8_PCHAR)"时间你好！",   /* fw,fh, pstr */
                        	                320, 1, 0,                        /* pixpl, lines, fgap */
                                	        80, 67,                         /* x0,y0, */
                                        	WEGI_COLOR_ORANGE, -1, 255,        /* fontcolor, transcolor,opaque */
	                                        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

	        /* Activate/Synch image */
        	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
		pthread_mutex_unlock(&surfshmem->shmem_mutex);

		tm_delayms(100);
	}

	/* Unregister and destroy surfuser */
	egi_unregister_surfuser(&surfuser);

	exit(0);
}


/*------------------------------------
SURFUSER input routine thread.
------------------------------------*/
void *surfuser_ering_routine(void *args)
{
	ERING_MSG *emsg=NULL;
	EGI_MOUSE_STATUS *mouse_status;
	int nrecv;

	while(1) {
		/* 1. Init EMSG : ----LOOP TEST EMSG */
		emsg=ering_msg_init();
		if(emsg==NULL)
			exit(EXIT_FAILURE);

		/* 2. Waiting for msg from SURFMAN */
	        egi_dpstd("Waiting in recvmsg...\n");
		nrecv=ering_msg_recv(surfuser->uclit->sockfd, emsg);
		if(nrecv<0) {
                	egi_dpstd("unet_recvmsg() fails!\n");
			continue;
        	}
		/* SURFMAN disconnects */
		else if(nrecv==0)
			exit(EXIT_FAILURE);

	        /* 3. Parse ering messag */
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

		/* Free EMSG */
		ering_msg_free(&emsg);
	}


	return (void *)0;
}


/*--------------------------------------
Parse mouse event and take actions.

Called by surfuser_input_routine()

@surfuser:   Pointer to EGI_SURFUSER.
@pmostat:    Pointer to EGI_MOUSE_STATUS

-----------------------------------------*/
void surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	int i;
	bool	mouseOnBtn; /* Mouse on button */

	FBDEV           *vfbdev;
	//EGI_IMGBUF      *imgbuf;
	EGI_SURFSHMEM   *surfshmem;

	if(surfuser==NULL || pmostat==NULL)
		return;

	/* Get referene */
        vfbdev=&surfuser->vfbdev;
        //imgbuf=surfuser->imgbuf;
        surfshmem=surfuser->surfshmem;

	/* Parse mouse event */

	/* Check if mouse on SURFBTNs */
	for(i=0; i<3; i++) {
		/* Check mouse_on_button */
		mouseOnBtn=egi_point_on_surfbtn( sbtns[i], pmostat->mouseX -surfuser->surfshmem->x0,
								pmostat->mouseY -surfuser->surfshmem->y0 );
		if(mouseOnBtn)
			printf(">>> Touch SURFBTN: %d\n", i);
		else
			printf("<<<< \n");

		/* Mouse on a SURFBTN: Set mpbtn */
		if( mpbtn != i && mouseOnBtn ) {  /* XXX mpbtn <0 ! */
			/* In case mouse move from a nearby SURFBTN, restore its image. */
			if(mpbtn>=0) {
				egi_subimg_writeFB(sbtns[mpbtn]->imgbuf, &surfuser->vfbdev, 0, -1, sbtns[mpbtn]->x0, sbtns[mpbtn]->y0);
			}

			/* Draw a square on the newly touched SURFBTN */
			fbset_color2(&surfuser->vfbdev, WEGI_COLOR_DARKRED);
			draw_rect(&surfuser->vfbdev, sbtns[i]->x0, sbtns[i]->y0,
				     sbtns[i]->x0+sbtns[i]->imgbuf->width-1,sbtns[i]->y0+sbtns[i]->imgbuf->height-1);

			/* Set mpbtn */
			mpbtn=i;

			/* Break for() */
			break;
		}
		/* Mouse leaves SURFBTN: Clear mpbtn */
		else if( mpbtn == i && !mouseOnBtn ) {  /* XXX mpbtn >=0!  */

			/* Draw on original image */
			egi_subimg_writeFB(sbtns[mpbtn]->imgbuf, &surfuser->vfbdev, 0, -1, sbtns[mpbtn]->x0, sbtns[mpbtn]->y0);

			/* clear mpbtn */
			mpbtn=-1;

			break;
		}
		/* Already on BUTTON, sustain... */
		else if( mpbtn == i && mouseOnBtn) {
			break;
		}
	}

	/* Click on top bar */
	if( pmostat->LeftKeyDown ) {
		switch(mpbtn) {
			case BTNCLOSE_ID:
				exit(0);
				break;
			case BTNMIN_ID:
				break;
			case BTNMAX_ID:
				break;
		}
	}

	/* Click under surface top bar */
	if( pmostat->LeftKeyDown && pmostat->mouseY-surfshmem->y0 > 30) {
        	fbset_color2(vfbdev, egi_color_random(color_all));
		draw_filled_circle(vfbdev, pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0, 10);
	}
}
