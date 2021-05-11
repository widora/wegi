/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Usage Example:
	./surf_alarm -p 50,50 -t "2021-3-7 19:54:00" "定时提醒！"

Note:
1. If time is NOT due, the surface appears in minibar waiting in mute.
2. Click MINIZE button to mute the alarm and bring the surface to
   minibar.

TODO:
1. Check whether the userver is disconnected.

Journal:
2021-4-21:
	1. Preset status as MINIMIZED.
	2. Mute the alarm when the surface is minized.


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
#include "egi_pcm.h"
#include "egi_procman.h"
#include "egi_input.h"

#define PCM_MIXER       	"default"
#define ALARM_SOUND_PATH   	"/mmc/alarm.wav"  /* a very short piece */

EGI_PCMBUF 	*pcmAlarm;
bool		sigStopPlay;

EGI_SURFUSER	*surfuser=NULL;
EGI_SURFSHMEM	*surfshmem=NULL;	   /* Only a ref. to surfuser->surfshmem */
FBDEV 		*vfbdev=NULL;		   /* Only a ref. to surfuser->vfbdev  */
EGI_IMGBUF 	*surfimg=NULL;		   /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE colorType=SURF_RGB565_A8;  /* surfuser->vfbdev color type */

//void *surfuser_ering_routine(void *args); /* Use module default function */
void surfuser_firstdraw_surface(EGI_SURFUSER *surfuser, int topbtns);

void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

/* Signal handler for SurfUser */
void signal_handler(int signo)
{
        if(signo==SIGINT) {
                egi_dpstd("SIGINT to quit the process!\n");
		exit(0);
        }
}


int main(int argc, char **argv)
{
	int i;
	int opt;
        char *delim=",";
        char saveargs[128];
        char *ps=&saveargs[0];

	time_t tnow;
	time_t tset;
        struct tm tm,tdm;

	char *strTask="EGI ALARM!";
	char *strTime;
	int fw,fh;
	int fgap;
        int sw=0,sh=0; /* Width and Height of the surface */
        int x0=0,y0=0; /* Origin coord of the surface */
	int penx=0, peny=0;
	int ln;		/* strtask lines */

        /* Set signal handler */
//       egi_common_sigAction(SIGINT, signal_handler);

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hp:t:"))!=-1 ) {
                switch(opt) {
                        case 'p':
                                x0=atoi(strtok_r(optarg, delim, &ps));
                                y0=atoi(strtok_r(NULL, delim, &ps));
                                printf("Surface origin (%d,%d).\n", x0,y0);
                                break;
                        case 't':	/* Set Alarm time */
				strTime=optarg;
				printf("strTime:%s\n",strTime);
				if( strstr(strTime, "-") ) {
				   if( strptime(optarg, "%Y-%m-%d %H:%M:%S", &tm)==NULL) {  /* %T Equivalent to %H:%M:%S. */
					printf("Error, Please input time with format Y-m-d H:M:S! \n");
					exit(EXIT_FAILURE);
				   }
				}
				else {
				   tnow=time(NULL); /* Fill in year/month/day ...*/
				   localtime_r(&tnow, &tm);
				   /* Get H:M:S */
				   if( strptime(optarg, "%H:%M:%S", &tdm)==NULL) {  /* %T Equivalent to %H:%M:%S. */
					printf("Error, Please input time with format H:M:S! \n");
					exit(EXIT_FAILURE);
				   }
				   /* Put H:M:S */
				   tm.tm_hour=tdm.tm_hour;
				   tm.tm_min=tdm.tm_min;
				   tm.tm_sec=tdm.tm_sec;
				}
				tset=mktime(&tm);
				printf("tset:%d\n",(int)tset);
                                break;
                        case 'h':
                                printf("%s: [-hsc]\n", argv[0]);
                                break;
                }
        }
	printf("optind=%d\n", optind);
        if( optind < argc ) {
		strTask=argv[optind];
		printf("Task:%s\n", strTask);
        } else {
                printf("No task description!\n");
        }

        /* Load freetype fonts */
        printf("FTsymbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0) {
                printf("Fail to load FT sysfonts, quit.\n");
		exit(EXIT_FAILURE);
        }

	/* Decide surface size */
	fw=18; fh=18;
	fgap=5;
//	shmin=30+fgap+fh+fgap+10;  /* 30-topbar, 5-gap, fh-strtime, 10-bottom_margin */
	sw=220; 		  /* Fix sw, For X_O Title width */

	/* Cal sheight for strtask  */
       	FTsymbol_uft8strings_writeFB(   NULL, egi_sysfonts.regular, 	/* FBdev, fontface */
               	                        fw, fh, (const UFT8_PCHAR)strTask,   /* fw,fh, pstr */
                       	                sw-30, 100, fgap,              	/* pixpl, lines, fgap */
                               	        15, 30+fgap+fh+fgap,            /* x0,y0, topbar+gap+strtm+gap */
                                       	WEGI_COLOR_BLACK, -1, 255,      /* fontcolor, transcolor,opaque */
                                        NULL, &ln, &penx, &peny);      /* int *cnt, int *lnleft, int* penx, int* peny */
	printf("penx=%d, peny=%d\n",penx, peny);
	sh=peny+fh+fgap+10;  /* Total height needed */
	ln=100-ln;

        /* Load pcm */
        pcmAlarm=egi_pcmbuf_readfile(ALARM_SOUND_PATH);
        if(pcmAlarm==NULL)exit(1);

	/* 1. Create a surfuser */
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, sw,sh, sw, sh, colorType); /* Fixed size */
	if(surfuser==NULL)
		exit(EXIT_FAILURE);
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */

	/* 2. Get ref imgbuf and vfbdev */
	vfbdev=&surfuser->vfbdev;
	surfimg=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons. */
	// Defualt: surfuser_ering_routine() calls surfuser_parse_mouse_event();
        surfshmem->minimize_surface     = surfuser_minimize_surface;    /* Surface module default functions */
        surfshmem->close_surface        = surfuser_close_surface;
     	// Disable size_adjust: surfshmem->redraw_surface       =  surfuser_redraw_surface;
        surfshmem->maximize_surface     = surfuser_maximize_surface;    /* Need redraw */
        surfshmem->normalize_surface    = surfuser_normalize_surface;   /* Need redraw */
	surfshmem->user_mouse_event     = my_mouse_event;

        /* 4. Assign name to the surface */
        //strncpy(surfshmem->surfname, "定时提醒", SURFNAME_MAX-1);
        strncpy(surfshmem->surfname, strTask, SURFNAME_MAX-1);

        /* 5. First draw surface */
        surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MIN); /* Default firstdraw operation */

	/* Write set_time */
       	FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 	/* FBdev, fontface */
               	                        fw, fh, (const UFT8_PCHAR)strTime,   /* fw,fh, pstr */
                       	                sw-30, ln, fgap,          	     /* pixpl, lines, fgap */
                               	        15, 30+fgap,                 	     /* x0,y0, */
                                       	WEGI_COLOR_BLACK, -1, 255,           /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);             /* int *cnt, int *lnleft, int* penx, int* peny */
	/* Write task description */
       	FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 	/* FBdev, fontface */
               	                        fw, fh, (const UFT8_PCHAR)strTask,   /* fw,fh, pstr */
                       	                sw-30, ln, fgap,          	     /* pixpl, lines, fgap */
                               	        15, 30+fgap+fh+fgap,                 /* x0,y0, */
                                       	WEGI_COLOR_BLACK, -1, 255,           /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);             /* int *cnt, int *lnleft, int* penx, int* peny */

	printf("shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* 5. Start Ering routine */
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

	/* Preset as MIN window */
	surfshmem->status= SURFACE_STATUS_MINIMIZED;

        /* Activate/synchronize surface */
        surfshmem->sync=true;

#if 1	/* Time ticking.... */
	printf("Timer is working...\n");
	do {
		tm_delayms(100);
		tnow=time(NULL);
	}
	while( difftime(tset, tnow)>0 );
#endif

	sigStopPlay=false;

	surfshmem->status= SURFACE_STATUS_NORMAL;

	/* Play sound */
        /* mpc_dev, pcmbuf, vstep, nf, nloop, bool *sigstop, bool *sigsynch, bool* sigtrigger */
        //egi_pcmbuf_playback(PCM_MIXER, pcmAlarm, 0, 1024, 100, (bool *)&surfshmem->usersig,  NULL, NULL);
        egi_pcmbuf_playback(PCM_MIXER, pcmAlarm, 0, 1024, 100, &sigStopPlay,  NULL, NULL);

	/* Loop */
	while( surfshmem->usersig != 1 ) {

                /* Get surface mutex_lock */
                if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 ) {
                          egi_dperr("Fail to get mutex_lock for surface.");
			  tm_delayms(10);
                          continue;
                }
/* ------ >>>  Surface shmem Critical Zone  */

	        /* Activate image */
        	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
		pthread_mutex_unlock(&surfshmem->shmem_mutex);

		tm_delayms(200);
	}

        /* In case */
	if( pcmAlarm->pcm_handle ) {
	        printf("Close pcmAlarm->pcm_handle...\n");
		snd_pcm_close(pcmAlarm->pcm_handle);
	}

        /* Free SURFBTNs */
        for(i=0; i<3; i++)
                egi_surfBtn_free(&surfshmem->sbtns[i]);

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


#if 0 ////////////////// To use module default function //////////////
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
			egi_dpstd("SURFMAN disconnects!\n");
			exit(EXIT_FAILURE);
		}

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
				//egi_dpstd("MS(X,Y):%d,%d\n", mouse_status->mouseX, mouse_status->mouseY);
				/* Parse mouse event */
				surfuser_parse_mouse_event(surfuser,mouse_status);
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
#endif ////////////////////////////////////////


/*----------------- Re-define firstdraw  ---------------------
Draw the surface at first time.
Create SURFBTNs by blockcopy SURFACE image.

@surfuser:	Pointer to EGI_SURFUSER.
@topbtns:	Buttons on top bar, optional.
		TOPBTN_CLOSE | TOPBTN_MIN | TOPBTN_MAX.
		0 No buttons on topbar.
------------------------------------------------------------*/
void surfuser_firstdraw_surface(EGI_SURFUSER *surfuser, int topbtns)
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

	/* 1. Set BK color */
	/* CCFFFF(LTblue), FF99FF,FFCCFF(LTpink), FFFF99(LTyellow) */
	//egi_imgbuf_resetColorAlpha(surfimg, COLOR_RGB_TO16BITS(0xCC,0xFF,0xFF), 255); /* Reset color only */
	egi_imgbuf_resetColorAlpha(surfimg, egi_color_random(color_light), 255); /* Reset color only */

	/* 2. Draw top bar */
	draw_filled_rect2(vfbdev,WEGI_COLOR_GRAY5, 0, 0, surfimg->width-1, SURF_TOPBAR_HEIGHT-1);


        /* 3. Draw CLOSE/MIN/MAX Btns */

	/* Put btn Chars */
	int i;
	char btnchar[8]={0};
	int  nbs=0;

	/* Put icon char in order of 'X_O' */
	if( topbtns&TOPBTN_CLOSE ) {
		strcat(btnchar,"X");
		nbs++;
	}
	if( topbtns&TOPBTN_MIN ) {
		strcat(btnchar,"_");
		nbs++;
	}
	if( topbtns&TOPBTN_MAX ) {
		strcat(btnchar,"O");
		nbs++;
	}

	/* Draw Char as icon */
	for(i=nbs; i>0; i--) {
		/* Font face: SourceHanSansSC-Normal */
		FTsymbol_unicode_writeFB( vfbdev, egi_sysfonts.regular,	/* FBdev, fontface */
					  18, 18, btnchar[i-1],   	/* fw,fh, pstr */
					  NULL, surfimg->width-20*(nbs-i+1), 5,	/* *xlef, x0,y0 */
					  WEGI_COLOR_WHITE, -1, 200    /* fontcolor, transcolor, opaque */
					);
	}

        /* 4. Put surface name/title on topbar */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 	  /* FBdev, fontface */
                                        18, 18, (const UFT8_PCHAR) surfshmem->surfname, /* fw,fh, pstr */
                                        320, 1, 0,                        /* pixpl, lines, fgap */
                                        5, 5,                             /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */

        /* 5. Set Alpha Frame and Draw outline rim */
	int rad=10;
	egi_imgbuf_setFrame(surfimg, frame_round_rect, -1, 1, &rad);
	/* AFTER setFrame: black rim. */
	fbset_color2(vfbdev, WEGI_COLOR_BLACK);
	draw_roundcorner_wrect(vfbdev, 0, 0, surfimg->width-1, surfimg->height-1, rad, 1);

        /* 6. Create SURFBTNs by blockcopy SURFACE image, after first_drawing the surface! */
        int xs=surfimg->width-20*nbs -3;
	i=0;
	/* By the order of 'X_O' */
	if( topbtns&TOPBTN_CLOSE ) {
		surfshmem->sbtns[TOPBTN_CLOSE_INDEX]=egi_surfBtn_create(surfimg,  xs,0+1,    xs,0+1,  18, 30-1);
									/* (imgbuf, xi, yi, x0, y0, w, h) */
		i++;
	}
	if( topbtns&TOPBTN_MIN ) {
	        surfshmem->sbtns[TOPBTN_MIN_INDEX]=egi_surfBtn_create(surfimg,  xs+i*20,0+1,  xs+i*20,0+1,   18, 30-1);
		i++;
	}
	if( topbtns&TOPBTN_MAX )
	        surfshmem->sbtns[TOPBTN_MAX_INDEX]=egi_surfBtn_create(surfimg,  xs+i*20,0+1,  xs+i*20,0+1, 18, 30-1);

//       	pthread_mutex_unlock(&surfshmem->shmem_mutex);
/* <<< ------  Surfman Critical Zone  */

}


/*-----------------------------------------------------------------
                Mouse Event Callback
                (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event() .

To stop playback music.
------------------------------------------------------------------*/
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	/* Since user's mevent callback function is after default mevent process....*/
	if( surfshmem->status == SURFACE_STATUS_MINIMIZED || surfshmem->usersig==1 )
		sigStopPlay=true;


}
