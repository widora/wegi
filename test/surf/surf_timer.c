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

static EGI_SURFUSER	*surfuser=NULL;
static EGI_SURFSHMEM	*surfshmem=NULL;	/* Only a ref. to surfuser->surfshmem */
static FBDEV 		*vfbdev=NULL;		/* Only a ref. to surfuser->vfbdev  */
static EGI_IMGBUF 	*surfimg=NULL;		/* Only a ref. to surfuser->imgbuf */
static SURF_COLOR_TYPE colorType=SURF_RGB565;   /* surfuser->vfbdev color type */

void 		*surfuser_ering_routine(void *args);

//void 		surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);


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
	int i;
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
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, 320,240, 260, 110, colorType);
	if(surfuser==NULL)
		exit(EXIT_FAILURE);

	/* Get ref imgbuf and vfbdev */
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
	// Defualt: surfuser_ering_routine() calls surfuser_parse_mouse_event();
	surfshmem->minimize_surface = surfuser_minimize_surface; /* Surface module default functions */
	surfshmem->close_surface = surfuser_close_surface;

        /* 4. Assign name to the surface */
        strncpy(surfshmem->surfname, "时间", SURFNAME_MAX-1);

        /* 5. First draw surface */
        surfshmem->bkgcolor=WEGI_COLOR_DARKBLUE; /* OR default BLACK */
        surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MIN); /* Default firstdraw operation */

	/* 6. Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* 7. Start Ering routine */
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);


	/* ----- Main Loop ----- */
	while( surfshmem->usersig !=1 ) {
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
                        	                320, 1, 0,                      /* pixpl, lines, fgap */
                                	        15, 40,                       	/* x0,y0, */
                                        	WEGI_COLOR_ORANGE, -1, 255,     /* fontcolor, transcolor,opaque */
	                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */

        	FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 	/* FBdev, fontface */
                	                        24, 24, (const UFT8_PCHAR)"时间你好！",   /* fw,fh, pstr */
                        	                320, 1, 0,                      /* pixpl, lines, fgap */
                                	        80, 67,                         /* x0,y0, */
                                        	WEGI_COLOR_ORANGE, -1, 255,     /* fontcolor, transcolor,opaque */
	                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */

	        /* Draw outline rim */
        	fbset_color2(vfbdev, WEGI_COLOR_GRAY);
	        draw_rect(vfbdev, 0, 0, surfshmem->vw-1, surfshmem->vh-1);

	        /* Activate/Synch image */
        	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
		pthread_mutex_unlock(&surfshmem->shmem_mutex);

		tm_delayms(100);
	}

	/* Free SURFBTNs */
	for(i=0; i<3; i++)
		egi_surfbtn_free(&surfshmem->sbtns[i]);

        /* Join ering_routine  */
   	// surfuser)->surfshmem->usersig =1;  // Useless if thread is busy calling a BLOCKING function.
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	/* Make sure mutex unlocked in pthread if any! */
        if( pthread_join(surfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

	/* Unregister and destroy surfuser */
	egi_unregister_surfuser(&surfuser);

	exit(0);
}


/*------------------------------------
    SURFUSER's ERING routine thread.
------------------------------------*/
void *surfuser_ering_routine(void *args)
{
        EGI_SURFUSER *surfuser=NULL;
	ERING_MSG *emsg=NULL;
	EGI_MOUSE_STATUS *mouse_status;
	int nrecv;

        /* Get surfuser pointer */
        surfuser=(EGI_SURFUSER *)args;
        if(surfuser==NULL)
                return (void *)-1;

	/* Init ERING_MSG */
	emsg=ering_msg_init();
	if(emsg==NULL)
		return (void *)-1;

	while( surfuser->surfshmem->usersig !=1 ) {
		/* 1. Waiting for msg from SURFMAN */
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
			/* NOTE: Msg BRINGTOP and MOUSE_STATUS sent by TWO threads, and they MAY reaches NOT in right sequence! */
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
				surfuser_parse_mouse_event(surfuser,mouse_status);
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


