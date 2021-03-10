/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


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

/* For SURFMAN */
EGI_SURFMAN 	*surfman;


/* For SURFUSER */
EGI_SURFUSER	*surfuser;
FBDEV 		*vfbdev;	/* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF 	*imgbuf;	/* Only a ref. to surfuser->imgbuf */
EGI_SURFSHMEM	*surfshmem;	/* Only a ref. to surfuser->surfshmem */
SURF_COLOR_TYPE colorType=SURF_RGB565;

void surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);


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
	bool SetAsServer=true;
	int opt;
	char *delim=",";
	char saveargs[128];
	char *ps=&saveargs[0];

	int sw=0,sh=0; /* Width and Height of the surface */
	int x0=0,y0=0; /* Origin coord of the surface */


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
        while( (opt=getopt(argc,argv,"hscS:p:"))!=-1 ) {
                switch(opt) {
                        case 's':
				SetAsServer=true;
                                break;
                        case 'c':
                                SetAsServer=false;
                                break;
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


 /* ---------- As EGI_SURFMAN ( The Server ) --------- */

 if( SetAsServer ) {
	int ret;
	int k;
	int ch;

        /* Set termI/O 设置终端为直接读取单个字符方式 */
        egi_set_termios();



    while(1) { /////////// LOOP TEST ///////////////

	/* Create an EGI surface manager */
	surfman=egi_create_surfman(ERING_PATH_SURFMAN);
	if(surfman==NULL) {
		//exit(EXIT_FAILURE);
		printf("Try again...\n");
		usleep(50000);
		continue;
	}
	else
		printf("Succeed to create surfman at '%s'!\n", ERING_PATH_SURFMAN);


	/* Do SURFMAN routine jobs while no command */
	k=SURFMAN_MAX_SURFACES-1;
	while( surfman->cmd==0 ) {
		usleep(10000);

		/* Keyboard event */
		/* Switch the top surface */
                ch=0;
                read(STDIN_FILENO, &ch,1);
		if(ch >'0' && ch < '9' ) {  /* 1,2,3...9 */
			printf("surfman->scnt=%d \n", surfman->scnt);
			printf("ch: '%c'(%d)\n",ch, ch);
			k=ch-0x30;
			if( k < SURFMAN_MAX_SURFACES ) {
				 printf("Try to set surface %d top...\n", k);
				 surfman_bringtop_surface(surfman, k);
			}

		}
		else {
			switch(ch) {
				case 'a':
					surfman->surfaces[SURFMAN_MAX_SURFACES-1]->surfshmem->x0 -=10;
					break;
				case 'd':
					surfman->surfaces[SURFMAN_MAX_SURFACES-1]->surfshmem->x0 +=10;
					break;
				case 'w':
					surfman->surfaces[SURFMAN_MAX_SURFACES-1]->surfshmem->y0 -=10;
					break;
				case 's':
					surfman->surfaces[SURFMAN_MAX_SURFACES-1]->surfshmem->y0 +=10;
					break;
				default:
					break;
			}
		}


#if 0		/***  			--- Monitor threads ---
		 *     !!! pthread_kill() returns 0 as OK, BUT there are NO printouts from above two threads. !!!
		 * 1.  pthread_kill():
		 *   	POSIX says that an attempt to use a thread ID whose lifetime has ended produces undefined behavior,
		 * 	and an attempt to use an invalid thread ID in a call to pthread_kill() can, for example,
		 * 	cause a segmentation fault.
		 * 2.  Abrupt break of a surfuser (without unlocking surfshmem->mutex_lock before quit) will force
		 *     surfman_request_process_thread() and surfman_render_thread() to quit??? OR deadlock???
		 * 3.  If unlock surfshmem->mutex_lock before quits, it's OK. it seems two thread trapped in unlocking surfshmem->mutex_lock,
		 *     which has been damaged by the abrupt exit of surfuser?
		 *----------------------------------------------------------------------------------------
		 *  OK! set mutex PTHREAD_MUTEX_ROBUST. When the owner thread dies without unlocking mutex, it can be recovered by any
		 *  other thread.
		 */
		ret=pthread_kill(surfman->repThread, 0);
		if(ret!=0) {
			egi_dperr("repThread is NOT running, or signal invalid!");
			exit(0);
		}
		else {
			//egi_dpstd("repThread is running.\n");
		}

		ret=pthread_kill(surfman->renderThread, 0);
		if(ret!=0) {
			egi_dperr("renderThread is NOT running, or signal invalid!");
			exit(0);
		}
		else {
			//egi_dpstd("renderThread is running.\n");
		}
#endif

	}

	/* Free SURFMAN */
	egi_destroy_surfman(&surfman);

   } /* End while() */

        /* Reset termI/O */
        egi_reset_termios();

	exit(0);
 }


 /* ---------- As EGI_SURFACE Application ( The client ) --------- */
 else {
	/*** NOTE:
	 *	1.  The second eface MAY be registered just BEFORE the first eface is unregistered.
	 */



	/* Set signal handler */
//	egi_common_sigAction(SIGINT, signal_handler);


    while( !sigQuit ) { /////////// LOOP TEST ///////////////

	/* 1. Register/Create a surfuser */
	if( sw!=0 && sh!=0) {
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, sw, sh, colorType );
	}
	else
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, mat_random_range(320-50)-50, mat_random_range(240-50),
        	                                  140+mat_random_range(320/2), 60+mat_random_range(240/2), colorType );
	if(surfuser==NULL) {
		usleep(100000);
		continue;
	}

	/* 2. Get ref. imgbuf and vfbdev */
	vfbdev=&surfuser->vfbdev;
	imgbuf=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

        /* Get surface mutex_lock */
        if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 ) {
        	egi_dperr("Fail to get mutex_lock for surface.");
                tm_delayms(10);
                continue;
        }
/* ------ >>>  Surface shmem Critical Zone  */

	/* Set color */
	egi_imgbuf_resetColorAlpha(imgbuf, egi_color_random(color_all), -1); /* Reset color only */

	/* Draw top bar */
	draw_filled_rect2(vfbdev,WEGI_COLOR_DARKGRAY, 0,0, imgbuf->width-1, 30);
	fbset_color2(vfbdev, WEGI_COLOR_GRAYA);
#if 0
	draw_wrect(vfbdev, imgbuf->width-1-(10+16+10+16), 7, imgbuf->width-1-(10+16+10), 5+16-1, 2); /* Max Icon */
	draw_wline(vfbdev, imgbuf->width-1-(10+16), 15-1, imgbuf->width-1-10, 15-1, 2); /* Mix Icon */
#else
	draw_rect(vfbdev, imgbuf->width-1-(10+16+10+16), 7, imgbuf->width-1-(10+16+10), 5+16-1); /* Max Icon */
	draw_rect(vfbdev, imgbuf->width-1-(10+16+10+16) +1, 7 +1, imgbuf->width-1-(10+16+10) -1, 5+16-1 -1); /* Max Icon */
	draw_line(vfbdev, imgbuf->width-1-(10+16), 15-1, imgbuf->width-1-10, 15-1); 	/* Mix Icon */
	draw_line(vfbdev, imgbuf->width-1-(10+16), 15-1 -1, imgbuf->width-1-10, 15-1 -1); 	/* Mix Icon */
#endif

	/* Draw a circle */
//	draw_circle2(vfbdev, imgbuf->width/2, 60, 20, WEGI_COLOR_WHITE);

        /* Put title. */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, /* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)"EGI_SURF",   /* fw,fh, pstr */
                                        320, 1, 0,                        /* pixpl, lines, fgap */
                                        5, 5,                         /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */


	/* Activate image */
	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);


#if 1 /* TEST: ERING_MSG ---------- */
int nrecv;
ERING_MSG *emsg=NULL;
EGI_MOUSE_STATUS *mouse_status;
while(1) {
	/* Init EMSG */
	emsg=ering_msg_init();
	if(emsg==NULL) exit(EXIT_FAILURE);

	/* Waiting for msg from SURFMAN */
        egi_dpstd("Waiting in recvmsg...\n");
	nrecv=ering_msg_recv(surfuser->uclit->sockfd, emsg);
        //nrecv=unet_recvmsg( surfuser->uclit->sockfd, emsg->msghead);
	if(nrecv<0) {
                egi_dpstd("unet_recvmsg() fails!\n");
		continue;
        }
	else if(nrecv==0)
		exit(EXIT_FAILURE);

        /* Parse result */
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
#endif /* TEST : END --- */


	/* hold */
	while(1) {


		sleep(1);
	}

	/* Unregister and destroy surfuser */
	if( egi_unregister_surfuser(&surfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	/* Hold on */
	usleep(10000);

   } /* End while() */

	exit(0);
 }

}


/*--------------------------------------
Parse mouse event and take actions.

@surfuser:   Pointer to EGI_SURFUSER.
@pmostat:    Pointer to EGI_MOUSE_STATUS

-----------------------------------------*/
void surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	FBDEV           *vfbdev;
	EGI_IMGBUF      *imgbuf;
	EGI_SURFSHMEM   *surfshmem;

	if(surfuser==NULL || pmostat==NULL)
		return;

	/* Get referene */
        vfbdev=&surfuser->vfbdev;
        imgbuf=surfuser->imgbuf;
        surfshmem=surfuser->surfshmem;

	/* Parse mouse event */
	/* Click under surface top bar */
	if( pmostat->LeftKeyDown && pmostat->mouseY-surfshmem->y0 > 30) {
        	fbset_color2(vfbdev, egi_color_random(color_all));
		draw_filled_circle(vfbdev, pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0, 10);
	}
}
