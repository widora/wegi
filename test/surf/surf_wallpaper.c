/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A tool program to set wallpaper for SURFMAN.
TEST: DO NOT display the surface.

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include "egi_timer.h"
#include "egi_color.h"
#include "egi_log.h"
#include "egi_debug.h"
#include "egi_math.h"
#include "egi_FTsymbol.h"
#include "egi_surface.h"
#include "egi_procman.h"
#include "egi_input.h"

/* Width and Height of the MAIN surface */
int 	sw=200;
int 	sh=160;

/* For SURFMSG DATA */
#define MSGQTEXT_MAXLEN 1024
struct smg_data {
	long msg_type;
        char msg_text[MSGQTEXT_MAXLEN];
} msgdata;
int msgerr;

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
//	int i;
	int ret;
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


	/* Set signal handler */
//	egi_common_sigAction(SIGINT, signal_handler);

	/* Enter private dir */
	chdir("/tmp/.egi");
	mkdir("wallpaper", 0777); /* 0777 ??? drwxr-xr-x */
	if(chdir("/tmp/.egi/wallpaper")!=0) {
		egi_dperr("Surf_wallpaper: Fail to make/enter private dir!");
		exit(EXIT_FAILURE);
	}

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
		exit(EXIT_FAILURE);
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	vfbdev=&surfuser->vfbdev;
	surfimg=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

        /* Get surface mutex_lock */
        if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 ) {
        	egi_dperr("Fail to get mutex_lock for surface.");
		exit(EXIT_FAILURE);
        }
/* ------ >>>  Surface shmem Critical Zone  */

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons etc. */
	// Defualt: surfuser_ering_routine() calls surfuser_parse_mouse_event();
        surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	//surfshmem->redraw_surface 	= surfuser_redraw_surface;
	//surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need redraw */
	//surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resraw */
        surfshmem->close_surface 	= surfuser_close_surface;
	//surfshmem->user_mouse_event	= my_mouse_event;
	//surfshmem->draw_canvas		= my_draw_canvas;

	/* 4. Name for the surface. */
	strncpy(surfshmem->surfname, "桌面背景设置", SURFNAME_MAX-1);

	/* 5. First draw surface */
	surfshmem->bkgcolor=egi_color_random(color_all); /* OR default BLACK */
	surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MAX|TOPBTN_MIN); /* Default firstdraw operation */
	/* 5A. Set Alpha Frame and Draw outline rim */

	/* ... Draw/Create SurfControls (Buttons/Lables/Menus/...)  */

	/* 6. Start Ering routine */
	printf("start ering routine...\n");
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

	/* 7. Activate image, ....TEST: DO NOT display it. */
//	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
	pthread_mutex_unlock(&surfshmem->shmem_mutex);


	/* ========== Main loop ========== */
	bool ret_ok=false;
	while( surfshmem->usersig != 1 ) {
		tm_delayms(100);

		if(ret_ok)
			continue;

#ifdef LETS_NOTE
		ret=system("/home/midas-zhou/bing_today.sh");
#else
		ret=system("/home/bing_today.sh");
#endif
		egi_dpstd("System ret=%d\n", ret);
		if( WIFEXITED(ret) ) {
			/* Send WallPaper file path to SURFMAN */
			msgdata.msg_type =SURFMSG_REQUEST_UPDATE_WALLPAPER;
			msgdata.msg_text[MSGQTEXT_MAXLEN-1]='\0';
			strncpy(msgdata.msg_text, "/tmp/bing_today.jpg", MSGQTEXT_MAXLEN-1);
                        msgerr=msgsnd(surfuser->msgid, (void *)&msgdata, MSGQTEXT_MAXLEN, 0);     /* IPC_NOWAIT */
                        if(msgerr!=0) {
                        	printf("Faill to msgsnd!\n");
                        }
                        else
                        	printf("Msg sent out type=%ld!\n", msgdata.msg_type);

/* QUIT........................ */
			surfshmem->usersig=1;

			/* set token */
			ret_ok=true;
		}
	}

        /* Pos_1: Join ering_routine  */
        // surfuser)->surfshmem->usersig =1;  // Useless if thread is busy calling a BLOCKING function.
	printf("Cancel thread...\n");
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        /* Make sure mutex unlocked in pthread if any! */
	printf("Joint thread_eringRoutine...\n");
        if( pthread_join(surfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

	/* Pos_2: Unregister and destroy surfuser */
	printf("Unregister surfuser...\n");
	if( egi_unregister_surfuser(&surfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	printf("Exit OK!\n");
	exit(0);
}

