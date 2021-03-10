/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Usage Example:
./surf_alarm -p 50,50 -t "2021-3-7 19:54:00" "定时提醒！"

TODO:
1. Check whether the userver is disconnected.

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


EGI_SURFSHMEM	*surfshmem=NULL;	/* Only a ref. to surfman->surface */

#define PCM_MIXER       	"default"
#define ALARM_SOUND_PATH   	"/mmc/alarm.wav"  /* a very short piece */
EGI_PCMBUF *pcmAlarm;

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
        char *delim=",";
        char saveargs[128];
        char *ps=&saveargs[0];

	time_t tnow;
	time_t tset;
        struct tm tm,tdm;
	int det=-5;

	char *strTask="EGI ALARM!";
	char *strTime;
	int fw,fh;
	int fgap;
        int sw=0,sh=0; /* Width and Height of the surface */
        int x0=0,y0=0; /* Origin coord of the surface */
	int penx=0, peny=0;
	int ln;		/* strtask lines */

	EGI_SURFUSER	*surfuser=NULL;
	FBDEV 		*vfbdev=NULL;	/* Only a ref. to surfman->vfbdev  */
	EGI_IMGBUF 	*imgbuf=NULL;	/* Only a ref. to surfman->imgbuf */
	SURF_COLOR_TYPE colorType=SURF_RGB565_A8;

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

	/* Time ticking */
	printf("Timer is working...\n");
	do {
		tm_delayms(100);
		tnow=time(NULL);
	}
	while( difftime(tset, tnow)>0 );

	/* 1. Create a surfuser */
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, sw, sh, colorType);
	if(surfuser==NULL)
		exit(EXIT_FAILURE);
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */

	/* 2. Get ref imgbuf and vfbdev */
	vfbdev=&surfuser->vfbdev;
	imgbuf=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;
	printf("shmsize: %zdBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* Set BK color */
	//egi_imgbuf_resetColorAlpha(imgbuf, WEGI_COLOR_LTYELLOW, 255); /* Reset color only */
	/* CCFFFF(LTblue), FF99FF,FFCCFF(LTpink), FFFF99(LTyellow) */
	egi_imgbuf_resetColorAlpha(imgbuf, COLOR_RGB_TO16BITS(0xFF,0xFF,0x99), 255); /* Reset color only */

	//egi_imgbuf_fadeOutEdges(EGI_IMGBUF *eimg, EGI_8BIT_ALPHA max_alpha, int width, unsigned int ssmode, int type);
//	egi_imgbuf_fadeOutEdges(imgbuf, 255, 20, 0b1111, 0);
	//int egi_imgbuf_setFrame( EGI_IMGBUF *eimg, enum imgframe_type type, int alpha, int pn, const int *param )

	/* Top bar */
	draw_filled_rect2(vfbdev,WEGI_COLOR_GRAY5, 0, 0, imgbuf->width-1, 30);

	int rad=10;
	egi_imgbuf_setFrame(imgbuf, frame_round_rect, -1, 1, &rad);

	/* AFTER setFrame: black edge */
	fbset_color2(vfbdev, WEGI_COLOR_BLACK);
	draw_roundcorner_wrect(vfbdev, 0, 0, imgbuf->width-1, imgbuf->height-1, 10, 1);

#if 0	/* Max/Min icons */
	fbset_color2(vfbdev, WEGI_COLOR_GRAYA);
        draw_rect(vfbdev, imgbuf->width-1-(10+16+10+16), 7, imgbuf->width-1-(10+16+10), 5+16-1); /* Max Icon */
        draw_rect(vfbdev, imgbuf->width-1-(10+16+10+16) +1, 7 +1, imgbuf->width-1-(10+16+10) -1, 5+16-1 -1); /* Max Icon */
        draw_line(vfbdev, imgbuf->width-1-(10+16), 15-1, imgbuf->width-1-10, 15-1);     /* Mix Icon */
        draw_line(vfbdev, imgbuf->width-1-(10+16), 15-1 -1, imgbuf->width-1-10, 15-1 -1);       /* Mix Icon */
#endif

	/* Write words and strings */
        /* Surface Title */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 		/* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)"X _ O   Timer Alarm",   /* fw,fh, pstr */
                                        320, 1, 0,                 	/* pixpl, lines, fgap */
                                        8, 5,                         	/* x0,y0, */
		                        WEGI_COLOR_ORANGE, -1, 255,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */


	/* Set_Time */
       	FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 	/* FBdev, fontface */
               	                        fw, fh, (const UFT8_PCHAR)strTime,   /* fw,fh, pstr */
                       	                sw-30, ln, fgap,          	     /* pixpl, lines, fgap */
                               	        15, 30+fgap,                 	     /* x0,y0, */
                                       	WEGI_COLOR_BLACK, -1, 255,           /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);             /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

	/* Task description */
       	FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 	/* FBdev, fontface */
               	                        fw, fh, (const UFT8_PCHAR)strTask,   /* fw,fh, pstr */
                       	                sw-30, ln, fgap,          	     /* pixpl, lines, fgap */
                               	        15, 30+fgap+fh+fgap,                 /* x0,y0, */
                                       	WEGI_COLOR_BLACK, -1, 255,           /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);             /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

        /* Activate/synchronize surface */
        surfshmem->sync=true;

	/* Play sound */
        /* mpc_dev, pcmbuf, vstep, nf, nloop, bool *sigstop, bool *sigsynch, bool* sigtrigger */
        egi_pcmbuf_playback(PCM_MIXER, pcmAlarm, 1, 1024, 1, NULL,  NULL, NULL);

	/* Loop */
	while(!sigQuit) {

                /* Get surface mutex_lock */
                if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 ) {
                          egi_dperr("Fail to get mutex_lock for surface.");
			  tm_delayms(10);
                          continue;
                }
/* ------ >>>  Surface shmem Critical Zone  */


#if 0		/* Move surfaces */
		surfshmem->y0 += det;
		if( surfshmem->y0 < 10) det=-det;
		else if( surfshmem->y0 > 240-80 ) det=-det;
#endif

	        /* Activate image */
        	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
		pthread_mutex_unlock(&surfshmem->shmem_mutex);


		tm_delayms(200);
	}

	/* Unregister and destroy surfuser */
	egi_unregister_surfuser(&surfuser);

	exit(0);
}
