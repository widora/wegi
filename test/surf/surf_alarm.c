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
#include "egi_input.h"

#define PCM_MIXER       	"default"
#define ALARM_SOUND_PATH   	"/mmc/alarm.wav"  /* a very short piece */
EGI_PCMBUF *pcmAlarm;
bool	sigstop;

EGI_SURFUSER	*surfuser=NULL;
EGI_SURFSHMEM	*surfshmem=NULL;	   /* Only a ref. to surfuser->surfshmem */
FBDEV 		*vfbdev=NULL;		   /* Only a ref. to surfuser->vfbdev  */
EGI_IMGBUF 	*surfimg=NULL;		   /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE colorType=SURF_RGB565_A8;  /* surfuser->vfbdev color type */

EGI_SURFBTN	*sbtns[3];  /* MIN,MAX,CLOSE */
static int 	mpbtn=-1;   /* Mouse_pointed button index, <0 None! */

pthread_t       thread_EringRoutine;    /* SURFUSER ERING routine */
void 		*surfuser_ering_routine(void *args);
void 		surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);


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
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, 320,240, sw, sh, colorType);
	if(surfuser==NULL)
		exit(EXIT_FAILURE);
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */

	/* 2. Get ref imgbuf and vfbdev */
	vfbdev=&surfuser->vfbdev;
	surfimg=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;
	printf("shmsize: %zdBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

        /* Assign name to the surface */
        strncpy(surfshmem->surfname, "定时提醒", SURFNAME_MAX-1);

	/* Set BK color */
	/* CCFFFF(LTblue), FF99FF,FFCCFF(LTpink), FFFF99(LTyellow) */
	egi_imgbuf_resetColorAlpha(surfimg, COLOR_RGB_TO16BITS(0xFF,0xFF,0x99), 255); /* Reset color only */

	/* Top bar */
	draw_filled_rect2(vfbdev,WEGI_COLOR_GRAY5, 0, 0, surfimg->width-1, 30);

	int rad=10;
	egi_imgbuf_setFrame(surfimg, frame_round_rect, -1, 1, &rad);

	/* AFTER setFrame: black edge */
	fbset_color2(vfbdev, WEGI_COLOR_BLACK);
	draw_roundcorner_wrect(vfbdev, 0, 0, surfimg->width-1, surfimg->height-1, 10, 1);

	/* Write words and strings */
        /* Surface Title */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 		/* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)"X _ O   Timer Alarm",   /* fw,fh, pstr */
                                        320, 1, 0,                 	/* pixpl, lines, fgap */
                                        14, 5,                         	/* x0,y0, */
		                        WEGI_COLOR_ORANGE, -1, 255,     /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */

	/* Set_Time */
       	FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 	/* FBdev, fontface */
               	                        fw, fh, (const UFT8_PCHAR)strTime,   /* fw,fh, pstr */
                       	                sw-30, ln, fgap,          	     /* pixpl, lines, fgap */
                               	        15, 30+fgap,                 	     /* x0,y0, */
                                       	WEGI_COLOR_BLACK, -1, 255,           /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);             /* int *cnt, int *lnleft, int* penx, int* peny */

	/* Task description */
       	FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 	/* FBdev, fontface */
               	                        fw, fh, (const UFT8_PCHAR)strTask,   /* fw,fh, pstr */
                       	                sw-30, ln, fgap,          	     /* pixpl, lines, fgap */
                               	        15, 30+fgap+fh+fgap,                 /* x0,y0, */
                                       	WEGI_COLOR_BLACK, -1, 255,           /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);             /* int *cnt, int *lnleft, int* penx, int* peny */

	/* Create SURFBTNs by blockcopy SURFACE image. */
	sbtns[SURFBTN_CLOSE]=egi_surfbtn_create(surfimg,     10,0,      10,0,      18, 30); /* (imgbuf, xi, yi, x0, y0, w, h) */
	sbtns[SURFBTN_MINIMIZE]=egi_surfbtn_create(surfimg,  12+18,0,   12+18,0,   18, 30);
	sbtns[SURFBTN_MAXIMIZE]=egi_surfbtn_create(surfimg,  14+18*2,0, 14+18*2,0, 18, 30);

	/* Start Ering routine */
	if( pthread_create(&thread_EringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

        /* Activate/synchronize surface */
        surfshmem->sync=true;

	/* Play sound */
        /* mpc_dev, pcmbuf, vstep, nf, nloop, bool *sigstop, bool *sigsynch, bool* sigtrigger */
        egi_pcmbuf_playback(PCM_MIXER, pcmAlarm, 0, 1024, 0, &sigstop,  NULL, NULL);

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

	while(1) {
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
	bool	mouseOnBtn; /* Mouse on button */
	static int lastX, lastY;

	FBDEV           *vfbdev;
	//EGI_IMGBUF      *imgbuf;
	EGI_SURFSHMEM   *surfshmem;

	if(surfuser==NULL || pmostat==NULL)
		return;

	/* 1. Get reference pointer */
        vfbdev=&surfuser->vfbdev;
        //imgbuf=surfuser->imgbuf;
        surfshmem=surfuser->surfshmem;

	/* 2. Check if mouse on SURFBTNs */
	for(i=0; i<3; i++) {
		/* Check mouse_on_button */
		mouseOnBtn=egi_point_on_surfbtn( sbtns[i], pmostat->mouseX -surfuser->surfshmem->x0,
								pmostat->mouseY -surfuser->surfshmem->y0 );
		#if 0 /* TEST: --------- */
		if(mouseOnBtn)
			printf(">>> Touch SURFBTN: %d\n", i);
		else
			printf("<<<< \n");
		#endif

		/* Mouse on a SURFBTN: Set mpbtn */
		if( mpbtn != i && mouseOnBtn ) {  /* XXX mpbtn <0 ! */
			/* In case mouse move from a nearby SURFBTN, restore its image. */
			if(mpbtn>=0) {
				egi_subimg_writeFB(sbtns[mpbtn]->imgbuf, &surfuser->vfbdev, 0, -1, sbtns[mpbtn]->x0, sbtns[mpbtn]->y0);
			}

		#if 0	/* Draw a square on the newly touched SURFBTN */
			fbset_color2(&surfuser->vfbdev, WEGI_COLOR_DARKRED);
			draw_rect(&surfuser->vfbdev, sbtns[i]->x0, sbtns[i]->y0,
				     sbtns[i]->x0+sbtns[i]->imgbuf->width-1,sbtns[i]->y0+sbtns[i]->imgbuf->height-1);
		#else
                        draw_blend_filled_rect(&surfuser->vfbdev, sbtns[i]->x0, sbtns[i]->y0,
						sbtns[i]->x0+sbtns[i]->imgbuf->width-1,sbtns[i]->y0+sbtns[i]->imgbuf->height-1,
						WEGI_COLOR_WHITE, 120);
		#endif

			/* Set mpbtn */
			mpbtn=i;

			/* Break for() */
			break;
		}
		/* Mouse leaves SURFBTN: Clear mpbtn */
		else if( mpbtn == i && !mouseOnBtn ) {  /* XXX mpbtn >=0!  */

			/* Draw/Restor original image */
			egi_subimg_writeFB(sbtns[mpbtn]->imgbuf, &surfuser->vfbdev, 0, -1, sbtns[mpbtn]->x0, sbtns[mpbtn]->y0);

			/* Clear mpbtn */
			mpbtn=-1;

			break;
		}
		/* Already on BUTTON, sustain... */
		else if( mpbtn == i && mouseOnBtn) {
			break;
		}
	}

	/* 3. If click OR?? released on SURFBTNs */
	if( pmostat->LeftKeyDown ) {
	//if( pmostat->LeftKeyUp ) {
		switch(mpbtn) {
			case SURFBTN_CLOSE:
				exit(0);
				break;
			case SURFBTN_MINIMIZE:
				/* Restor to original image */
				egi_subimg_writeFB(sbtns[mpbtn]->imgbuf, &surfuser->vfbdev, 0, -1, sbtns[mpbtn]->x0, sbtns[mpbtn]->y0);

				/* Set status to be SURFACE_STATUS_MINIMIZED */
				surfuser->surfshmem->status=SURFACE_STATUS_MINIMIZED;

				/* Sound OFF*/
				sigstop=true;

				break;
			case SURFBTN_MAXIMIZE:
				break;
		}

                 /* Rest lastX/Y, to compare with next mouseX/Y to get deviation. */
                 lastX=pmostat->mouseX;
                 lastY=pmostat->mouseY;
	}

        /* 4. LeftKeyDownHold: To move surface */
        if( pmostat->LeftKeyDownHold ) {
        	/* Note: As mouseDX/DY already added into mouseX/Y, and mouseX/Y have Limit Value also.
                 * If use mouseDX/DY, the cursor will slip away at four sides of LCD, as Limit Value applys for mouseX/Y,
                 * while mouseDX/DY do NOT has limits!!!
                 * We need to retrive DX/DY from previous mouseX/Y.
                 */
        	pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surfman Critical Zone  */
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
        	pthread_mutex_unlock(&surfshmem->shmem_mutex);

                /* update lastX,Y */
		lastX=pmostat->mouseX; lastY=pmostat->mouseY;
	}
	else {
        	pthread_mutex_lock(&surfshmem->shmem_mutex);

		/* Restore status NORMAL */
		if( surfshmem->status == SURFACE_STATUS_DOWNHOLD )
			surfshmem->status = SURFACE_STATUS_NORMAL;

        	pthread_mutex_unlock(&surfshmem->shmem_mutex);
	}

	/* 5. If click on surface work area. */
	if( pmostat->LeftKeyDown && pmostat->mouseY-surfshmem->y0 > 30) {
        	fbset_color2(vfbdev, egi_color_random(color_all));
		draw_filled_circle(vfbdev, pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0, 10);
	}
}
