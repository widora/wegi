/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Create an EGI Desktop guide bar.

Journal:
2021-05-27: Start programming...
            Finish.

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


/* SURF Lable_Icons */
enum {
        LABICON_STATUS  =0,     /* CPU */
        LABICON_TIME    =1,     /* Clock */
        LABICON_SOUND   =2,     /* Detail of data stream */
        LABICON_WIFI	=3,     /* WiFi Info. */
        LABICON_MAX     =4      /* <--- Limit */
};
ESURF_LABEL *labicons[LABICON_MAX];
int mp=-1;   /* Index of mouse touched labicons[], <0 invalid. */


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
void my_firstdraw_surface(EGI_SURFUSER *surfuser, int options);
void labicon_time_update(void);

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

	/* Set signal handler */
//	egi_common_sigAction(SIGINT, signal_handler);

REGISTER_SURFUSER:
	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	x0=-5; y0=SURF_MAXH-30;	sw=SURF_MAXW +5*2; sh=SURF_TOPBAR_HEIGHT;
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, SURF_MAXW,SURF_MAXH, sw, sh, colorType );
	if(surfuser==NULL) {
		usleep(100000);
		goto REGISTER_SURFUSER;
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
#if 0
	// Defualt: surfuser_ering_routine() calls surfuser_parse_mouse_event();
        surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	surfshmem->redraw_surface 	= surfuser_redraw_surface;
	surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        surfshmem->close_surface 	= surfuser_close_surface;
#endif

	/* 3. Give a name for the surface. */
	strncpy(surfshmem->surfname, "EGI Desktop", SURFNAME_MAX-1);

	/* 4. First draw surface */
	surfshmem->bkgcolor=WEGI_COLOR_GRAY; /* OR default BLACK */
	//surfuser_firstdraw_surface(surfuser, TOPBAR_NAME); /* Default firstdraw operation */
	my_firstdraw_surface(surfuser, TOPBAR_NAME); /* Default firstdraw operation */

	/* Create Label */
        labicons[LABICON_TIME]=egi_surfLab_create(surfimg, SURF_MAXW-5-60, 1, SURF_MAXW-5-60, 1, 80, 30-2); /* img,xi,yi,x0,y0,w,h */
        egi_surfLab_updateText(labicons[LABICON_TIME], "00:00");
        egi_surfLab_writeFB(vfbdev, labicons[LABICON_TIME], egi_sysfonts.bold, 16, 16, WEGI_COLOR_LTYELLOW, 0, 5);

	/* 24x24 icons */
	EGI_IMGBUF *wifi_100=egi_imgbuf_readfile("/mmc/icons/wifi_100.png");
	EGI_IMGBUF *audio_volume_100=egi_imgbuf_readfile("/mmc/icons/audio_volume_100.png");
	EGI_IMGBUF *battery_100_charging=egi_imgbuf_readfile("/mmc/icons/battery_100_charging.png");
	egi_subimg_writeFB(wifi_100, vfbdev, 0, WEGI_COLOR_WHITE, SURF_MAXW-5-60-40, 2);
	egi_subimg_writeFB(battery_100_charging, vfbdev, 0, WEGI_COLOR_PINK, SURF_MAXW-5-60-40-40, 3);
	egi_subimg_writeFB(audio_volume_100, vfbdev, 0, WEGI_COLOR_WHITE, SURF_MAXW-5-60-40-40-30, 2);


	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* 5. Start Ering routine */
	printf("start ering routine...\n");
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

	/* 6. Activate image */
	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);

	/* Main loop */
	while( surfshmem->usersig != 1 ) {
		tm_delayms(100);

        	/* Get surface mutex_lock */
        	pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */

		labicon_time_update();

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);

	}

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



/*---------------------------------------------------------
Just modify default surfuser_firstdraw_surface() a little...

1. Change surfname font face to 'egi_sysfonts.bold'.
2. Change edge color to BLACK

@surfuser:	Pointer to EGI_SURFUSER.
@options:	Apprance and Buttons on top bar, optional.
		TOPBAR_NONE | TOPBAR_COLOR | TOPBAR_NAME
		TOPBTN_CLOSE | TOPBTN_MIN | TOPBTN_MAX.
		0 No topbar
----------------------------------------------------------*/
void my_firstdraw_surface(EGI_SURFUSER *surfuser, int options)
{
	if(surfuser==NULL || surfuser->surfshmem==NULL)
		return;

//     		pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surfman Critical Zone  */

	/* 0. Ref. pointers */
	FBDEV  *vfbdev=&surfuser->vfbdev;
	EGI_IMGBUF *surfimg=surfuser->imgbuf;
        EGI_SURFSHMEM *surfshmem=surfuser->surfshmem;

	/* Size limit */
	if( surfshmem->vw > surfshmem->maxW )
		surfshmem->vw=surfshmem->maxW;

	if( surfshmem->vh > surfshmem->maxH )
		surfshmem->vh=surfshmem->maxH;

	/* 1. Draw background / canvas */
        /* 1.1B Set BKG color/alpha for surfimg, If surface colorType has NO alpha, then ignore. */
       	egi_imgbuf_resetColorAlpha(surfimg, surfshmem->bkgcolor, 255); //-1); /* Reset color only */

        /* 1.2B  Draw top bar. */
	if( options >= TOPBAR_COLOR )
        	draw_filled_rect2(vfbdev,SURF_TOPBAR_COLOR, 0,0, surfimg->width-1, SURF_TOPBAR_HEIGHT-1);

	/* 2. Case reserved intently. */

        /* 3. Draw CLOSE/MIN/MAX Btns */

	/* Put icon char in order of 'X_O' */
	/* Draw Char as icon */

        /* 4. Put surface name/title on topbar */
	if( options >= TOPBAR_NAME ) {
	        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.bold, 	  /* FBdev, fontface */
                                        16, 16, (const UFT8_PCHAR) surfshmem->surfname, /* fw,fh, pstr */
                                        320, 1, 0,                        /* pixpl, lines, fgap */
                                        10, 5,                             /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */
	}

#if 1   /* 5. Draw outline rim */
	//fbset_color(SURF_OUTLINE_COLOR);
	fbset_color(WEGI_COLOR_BLACK);
        draw_rect(vfbdev, 0,0, surfshmem->vw-1, surfshmem->vh-1);
#endif

        /* 6. Create SURFBTNs by blockcopy SURFACE image, after first_drawing the surface! */

//       	pthread_mutex_unlock(&surfshmem->shmem_mutex);
/* <<< ------  Surfman Critical Zone  */

}

/*------------------------------------
Update time for labicon[LABICON_TIME]

-------------------------------------*/
void labicon_time_update(void)
{
	char strtmp[32];

	tm_get_strtime(strtmp);
        egi_surfLab_updateText(labicons[LABICON_TIME], strtmp);
        egi_surfLab_writeFB(vfbdev, labicons[LABICON_TIME], egi_sysfonts.bold, 16, 16, WEGI_COLOR_LTYELLOW, 0, 5);
}
