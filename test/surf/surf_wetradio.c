/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Usage:
	./surf_wetradio  ( /home/texture/metal_v.png )

	Put mouse cursor on PLAY button and roll mouse wheel to adjust
	volume.

Note:
1. One of possible wetradio.sh:
----------------- wetradio.sh --------------
#!/bin/sh
killall -9 http_aac
killall -9 radio_aacdecode
sleep 1
rm a.stream
rm b.stream
if [ $# -lt 1 ]; then
  echo "No address, quit."
  exit 1
fi
# Loop downloading live stream
screen -dmS HTTPAAC /home/http_aac ${*}
# Loop decoding and playing downloaded files
screen -dmS ACCDECODE /home/radio_aacdecode
--------------------------------------------

Journal:
2021-04-21:
	1. A simple UI/UX framework design for the APP.
2021-04-22:
	1. Buttons and displaying panel labels.
	2. Radio control functions.
2021-04-23:
	1. Add load_playlist()

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
#include "egi_surfcontrols.h"
#include "egi_procman.h"
#include "egi_input.h"
#include "egi_pcm.h"
#include "egi_utils.h"

/* Width and Height of the surface */
int 	sw=240;
int 	sh=200;

/* Size of Display Panel */
int 	panW=220;
int 	panH=80;

/* Radio Playlist */
struct radio_info
{
#define RADIO_ADDR_MAXLEN	128
#define RADIO_NAME_MAXLEN	64
	char address[RADIO_ADDR_MAXLEN];	/* Live Stream address */
	char name[RADIO_NAME_MAXLEN];		/* Station name */
};

struct radio_info *playlist;
int list_idx;
int list_max;

/* Load palylist from text file */
int load_playlist(const char *fpath, struct radio_info **infoList,  int *listCnt);

/* Call system() to stop/start radio */
void stop_radio(void);
void start_radio(const char *address);

/* Play status/signals */
enum {
        STAT_PLAY,
        STAT_STOP,
        STAT_CONNECT, 	/* Connecting to the server */

	SIG_NONE,
	SIG_PLAY,
	SIG_STOP,
	SIG_PREV,
	SIG_NEXT
};
int radioSTAT=STAT_STOP;
int radioSIG=SIG_NONE;


//#include "sys_incbin.h"
//INCBIN(NormalIcons, "/home/midas-zhou/Pictures/icons/gray_icons_normal_block.png");

/* For SURFUSER */
EGI_SURFUSER     *surfuser=NULL;
EGI_SURFSHMEM    *surfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *vfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *surfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  colorType=SURF_RGB565_A8;  /* surfuser->vfbdev color type */
EGI_16BIT_COLOR  bkgcolor;
EGI_IMGBUF 	*surftexture=NULL;
int        	 *pUserSig;                   /* Pointer to surfshmem->usersig, ==1 for quit. */

/* For SURF Buttons */
enum {
        /* Working/On_display buttons */
        BTN_PREV        =0,
        BTN_PLAY        =1,
        BTN_NEXT        =2,
        BTN_VALID_MAX   =3, /* <--- Max number of working buttons. */

        BTN_PLAY_SWITCH =4,  /* To switch with BTN_PLAY */

        BTN_MAX         =5, /* <--- Limit, for allocation. */
};
ESURF_BTN  * btns[BTN_MAX];
ESURF_BTN  * btn_tmp=NULL;
int btnW=68;            /* Button size, same as original icon size. */
int btnH=68;
int btnGap=5;

int btnW2=45;		/* Small buttons */
int btnH2=45;

//bool mouseOnBtn;
int mpbtn=-1;   /* Index of mouse touched button, <0 invalid. */

/* SURF Menus */
enum {
	LAB_STATUS	=0,	/* Status: connecting, playing, stop */
	LAB_RSNAME	=1,	/* Radio station name */
	LAB_DETAILS	=2,	/* Detail of data stream */
	LAB_MAX		=3	/* <--- Limit */
};
ESURF_LABEL *labels[LAB_MAX];


/* SURF Lables */



/* User surface functions */
void 	my_draw_canvas(EGI_SURFUSER *surfuser);
void 	my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

/* ERING routine */
void	*surfuser_ering_routine(void *args);



/*----------------------------
	   MAIN()
-----------------------------*/
int main(int argc, char **argv)
{
	//int i,j;
	int x0=0,y0=0; /* Origin coord of the surface */
	char *pname=NULL;

	/* Load palylist */
	if( load_playlist("/home/FM_list", &playlist,  &list_max) <0 )
		exit(EXIT_FAILURE);


#if 0	/* Start EGI log */
        if(egi_init_log("/mmc/surf_madplay.log") != 0) {
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

#if 1        /* Load freetype fonts */
        printf("FTsymbol_load_appfonts()...\n");
        if(FTsymbol_load_appfonts() !=0) {
                printf("Fail to load FT appfonts, quit.\n");
                return -1;
        }
#endif
	/* Prepare mixer simple element for volume preparation */
  	egi_getset_pcm_volume(NULL,NULL);

	/* Enter private dir */
	chdir("/tmp/.egi");
	mkdir("radioNet", S_IRWXG);
	if(chdir("/tmp/.egi/radioNet")!=0) {
		printf("Fail to enter private dir! Err'%s'\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Load surftexture */
	if( argc >1 )
		surftexture=egi_imgbuf_readfile(argv[1]);
	else
		surftexture=egi_imgbuf_readfile("/mmc/texture/metal_grids.png");

	/* Load Noraml Icons */
	EGI_IMGBUF *icons_normal=egi_imgbuf_readfile("/mmc/play_buttons.png");

	/* Create buttons */
	btns[BTN_PLAY]=egi_surfBtn_create(icons_normal, btnW+btnGap, 0, (sw-btnW)/2, SURF_TOPBAR_HEIGHT+5+80+5, btnW, btnH);
	btns[BTN_PLAY_SWITCH]=egi_surfBtn_create(icons_normal, btnW+btnGap, btnW*2+btnGap*2, (sw-btnW)/2, SURF_TOPBAR_HEIGHT+5+80+5, btnW, btnH);

	btns[BTN_PREV]=egi_surfBtn_create( icons_normal, btnW*2+btnGap*2, btnW+btnGap,
					   ((sw-btnW)/2-btnW2)/2 +10, SURF_TOPBAR_HEIGHT+5+80+5 +btnH/2 -btnH2/2, btnW, btnH);
	egi_imgbuf_resize_update(&btns[BTN_PREV]->imgbuf, true, btnW2, btnH2); /* Resize */

	btns[BTN_NEXT]=egi_surfBtn_create( icons_normal, btnW+btnGap, btnW+btnGap,
					   sw-( (sw-btnW)/4 +btnW2/2 ) -10, SURF_TOPBAR_HEIGHT+5+80+5 +btnH/2 -btnH2/2, btnW, btnH);
	egi_imgbuf_resize_update(&btns[BTN_NEXT]->imgbuf, true, btnW2, btnH2); /* Resize */

	/* TEST: -------- */
	if(btns[BTN_PLAY]->imgbuf==NULL)
		printf("-----Normal imgbuf NULL!!!\n");

	/* Load Effect Icons */
	EGI_IMGBUF *icons_effect=egi_imgbuf_readfile("/mmc/play_buttons_effect.png");

	/* Set imgbuf_effect for buttons: egi_imgbuf_blockCopy(inimg, px, py, height, width) */
	btns[BTN_PLAY]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, btnW+btnGap, 0, btnW, btnH);
	btns[BTN_PLAY_SWITCH]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, btnW+btnGap, btnW*2+btnGap*2, btnW, btnH);

	btns[BTN_PREV]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, btnW*2+btnGap*2, btnW+btnGap, btnW, btnH);
	egi_imgbuf_resize_update(&btns[BTN_PREV]->imgbuf_effect, true, btnW2, btnH2); /* Resize */

	btns[BTN_NEXT]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, btnW+btnGap, btnW+btnGap, btnW, btnH);
	egi_imgbuf_resize_update(&btns[BTN_NEXT]->imgbuf_effect, true, btnW2, btnH2); /* Resize */

	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, sw, sh, sw, sh, colorType ); /* Fixed size */
	if(surfuser==NULL) {
		printf("Fail to register surfuser!\n");
		exit(EXIT_FAILURE);
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	vfbdev=&surfuser->vfbdev;
	surfimg=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

	/* EGI_SURFACE Info */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

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
	surfshmem->draw_canvas		= my_draw_canvas;

	pUserSig = &surfshmem->usersig;

	/* 4. Name for the surface. */
	pname="WetRadio";
	strncpy(surfshmem->surfname, pname, SURFNAME_MAX-1);

	/* 5. First draw surface. */
	//surfshmem->bkgcolor=WEGI_COLOR_GRAY3; /* OR default BLACK */
	surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MIN); /* Default firstdraw operation */
        /* 5A. Set Alpha Frame and Draw outline rim */
        int rad=10;
        egi_imgbuf_setFrame(surfimg, frame_round_rect, -1, 1, &rad);

#if 1        /* AFTER setFrame: black rim.  TODO --- BUG BUG BUG!!!! */
        fbset_color2(vfbdev, WEGI_COLOR_BLACK);
        draw_roundcorner_wrect(vfbdev, 0, 0, surfimg->width-1, surfimg->height-1, rad, 1);
#endif

	/* 6. Draw buttons */
	egi_surfBtn_writeFB(vfbdev, btns[BTN_PLAY], SURFBTN_IMGBUF_NORMAL, 0, 0);
	egi_surfBtn_writeFB(vfbdev, btns[BTN_PREV], SURFBTN_IMGBUF_NORMAL, 0, 0);
	egi_surfBtn_writeFB(vfbdev, btns[BTN_NEXT], SURFBTN_IMGBUF_NORMAL, 0, 0);

	/* 7. Draw displaying panel */
	draw_blend_filled_roundcorner_rect(vfbdev, 10, SURF_TOPBAR_HEIGHT+5, 10+panW, SURF_TOPBAR_HEIGHT+5+panH, 5, /* x1,y1,x2,y2,r */
                                                        WEGI_COLOR_DARKGRAY1, 255);	/* color, alpha */
	/* 8. Draw/Create menus */

	/* 9. Draw/Create Lables */
	/* 9.1 Play statu */
	labels[LAB_STATUS]=egi_surfLab_create(surfimg,20,SURF_TOPBAR_HEIGHT+5+2,20,SURF_TOPBAR_HEIGHT+5+2, panW, 25); /* img,xi,yi,x0,y0,w,h */
	egi_surfLab_updateText(labels[LAB_STATUS], "Stop");
	egi_surfLab_writeFB(vfbdev, labels[LAB_STATUS], egi_sysfonts.regular, 18, 18, WEGI_COLOR_LTYELLOW, 0, 0);
	/* 9.2 Station Name */
	labels[LAB_RSNAME]=egi_surfLab_create(surfimg,20,SURF_TOPBAR_HEIGHT+5+32,20,SURF_TOPBAR_HEIGHT+5+32, panW, 25); /* img,xi,yi,x0,y0,w,h */
	egi_surfLab_updateText(labels[LAB_RSNAME], playlist[list_idx].name);
	egi_surfLab_writeFB(vfbdev, labels[LAB_RSNAME], egi_sysfonts.regular, 18, 18, WEGI_COLOR_GREEN, 0, 0);
	/* 9.3 Stream detail */
	labels[LAB_DETAILS]=egi_surfLab_create(surfimg,20,SURF_TOPBAR_HEIGHT+5+60,20,SURF_TOPBAR_HEIGHT+5+60, panW, 20); /* img,xi,yi,x0,y0,w,h */
	egi_surfLab_updateText(labels[LAB_DETAILS], "AAC_LC Ch=2 48KHz");
	egi_surfLab_writeFB(vfbdev, labels[LAB_DETAILS], egi_sysfonts.regular, 14, 14, WEGI_COLOR_GREEN, 0, 0);

	/* 10. Start Ering routine: Use module default routine function. */
	printf("start ering routine...\n");
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

	/* 11. Activate image */
	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);

	/* ====== Main Loop ====== */
	struct stat     sb;
	while( surfshmem->usersig != 1 ) {

		/* Parse signal */
		if( ( radioSIG == SIG_NEXT || radioSIG == SIG_PREV) && (radioSTAT == STAT_PLAY || radioSTAT == STAT_CONNECT) ) {
			/* Stop it first */
			stop_radio();

			/* Shift playlist */
			if( radioSIG == SIG_NEXT )
				list_idx++;
			else if ( radioSIG == SIG_PREV )
				list_idx--;

			if( list_idx > list_max-1 )
				list_idx=0;
			if( list_idx < 0 )
				list_idx = list_max-1;

			/* Update Labels */
        		pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */
			egi_surfLab_updateText(labels[LAB_STATUS], "Connecting...");
			egi_surfLab_writeFB(vfbdev, labels[LAB_STATUS], egi_sysfonts.regular, 18, 18, WEGI_COLOR_LTYELLOW, 0, 0);
			egi_surfLab_updateText(labels[LAB_RSNAME], playlist[list_idx].name);
			egi_surfLab_writeFB(vfbdev, labels[LAB_RSNAME], egi_sysfonts.regular, 18, 18, WEGI_COLOR_GREEN, 0, 0);
/* ------ <<<  Surface shmem Critical Zone  */
	                pthread_mutex_unlock(&surfshmem->shmem_mutex);

			start_radio(playlist[list_idx].address);

			radioSTAT = STAT_CONNECT;  /* Main loop to check STAT_PLAY, if stream file found. */
			radioSIG = SIG_NONE;
		}
		if(  radioSIG == SIG_PLAY && radioSTAT != STAT_PLAY ) {
        		pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */
			egi_surfLab_updateText(labels[LAB_STATUS], "Connecting...");
			egi_surfLab_writeFB(vfbdev, labels[LAB_STATUS], egi_sysfonts.regular, 18, 18, WEGI_COLOR_LTYELLOW, 0, 0);
			egi_surfLab_updateText(labels[LAB_RSNAME], playlist[list_idx].name);
			egi_surfLab_writeFB(vfbdev, labels[LAB_RSNAME], egi_sysfonts.regular, 18, 18, WEGI_COLOR_GREEN, 0, 0);
/* ------ <<<  Surface shmem Critical Zone  */
	                pthread_mutex_unlock(&surfshmem->shmem_mutex);

			start_radio(playlist[list_idx].address);

			radioSTAT = STAT_CONNECT;  /* Main loop to check STAT_PLAY, if stream file found. */
			radioSIG = SIG_NONE;
		}
		else if( radioSIG == SIG_STOP ) {
        		pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */
			egi_surfLab_updateText(labels[LAB_STATUS], "Stop");
			egi_surfLab_writeFB(vfbdev, labels[LAB_STATUS], egi_sysfonts.regular, 18, 18, WEGI_COLOR_LTYELLOW, 0, 0);
/* ------ <<<  Surface shmem Critical Zone  */
	                pthread_mutex_unlock(&surfshmem->shmem_mutex);

			stop_radio();
			radioSTAT = STAT_STOP;
			radioSIG = SIG_NONE;
		}
		else {  /* SIG_NEXT/PREV + STAT_STOP, In this case radioSIG will NEVER be cleared by auto! */
			radioSIG = SIG_NONE;
		}

		/* Check status */
		if( radioSTAT == STAT_CONNECT ) {
			if( stat("/tmp/a.stream", &sb)==0 && sb.st_size>0 ) {
				radioSTAT = STAT_PLAY;
        			pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */
				egi_surfLab_updateText(labels[LAB_STATUS], "Playing...");
				egi_surfLab_writeFB(vfbdev, labels[LAB_STATUS], egi_sysfonts.regular, 18, 18, WEGI_COLOR_WHITE, 0, 0);
/* ------ <<<  Surface shmem Critical Zone  */
		                pthread_mutex_unlock(&surfshmem->shmem_mutex);

			}
		}

		usleep(100000);
	}

	/* Stop radio */
	stop_radio();

        /* Pos_1: Join ering_routine  */
        // surfuser)->surfshmem->usersig =1;  // Useless if thread is busy calling a BLOCKING function.
	egi_dpstd("Cancel thread...\n");
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        /* Make sure mutex unlocked in pthread if any! */
	egi_dpstd("Joint thread_eringRoutine...\n");
        if( pthread_join(surfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

	/* Pos_2: Unregister and destroy surfuser */
	egi_dpstd("Unregister surfuser...\n");
	if( egi_unregister_surfuser(&surfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	egi_dpstd("Exit OK!\n");
	exit(0);
}


/*---------------------------------
     Stop/Start radio playing.
----------------------------------*/
void stop_radio(void)
{
	char strcmd[1024];
	strcmd[1024-1]='\0';

	snprintf(strcmd, sizeof(strcmd)-1, "/home/wetradio.sh");
	system( strcmd );

	remove("/tmp/a.stream");
	remove("/tmp/b.stream");
}
void start_radio(const char *address)
{
	char strcmd[1024];
	strcmd[1024-1]='\0';

        snprintf(strcmd, sizeof(strcmd)-1, "/home/wetradio.sh %s", address);
        system( strcmd );
}

#if 0/////////////////////////////////////////
/*------------------------------------
    SURFUSER's ERING routine thread.
------------------------------------*/
void *__surfuser_ering_routine(void *args)
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

	/* Hint */
	egi_dpstd("SURFACE %s starts ERING routine. \n", surfuser->surfshmem->surfname);

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
			egi_dperr("ering_msg_recv() nrecv==0! SURFMAN disconnects!!");
			//continue;
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
#endif /////////////////////////////////////////////


/*---------------------------------------------------------
Draw background/canvas for the surface before
firstdraw.
This function is to be called by module's default function
surfuser_firstdraw_surface().
-----------------------------------------------------------*/
void  my_draw_canvas(EGI_SURFUSER *surfuser)
{
        if(surfuser==NULL || surfuser->surfshmem==NULL)
                return;

        /* Ref. pointers */
        FBDEV  *vfbdev=&surfuser->vfbdev;
        EGI_IMGBUF *surfimg=surfuser->imgbuf;
        EGI_SURFSHMEM *surfshmem=surfuser->surfshmem;

	/* Puts surftexture */
	egi_subimg_writeFB( surftexture, vfbdev, 0, -1, 0, 0);

	/* Put a pad as topbar */
	draw_blend_filled_rect(vfbdev, 0, 0, surfshmem->vw-1, SURF_TOPBAR_HEIGHT-1,  WEGI_COLOR_OCEAN, 160);
}



/*------------------------------------------------------------------------------
                Mouse Event Callback
             (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event() .
-------------------------------------------------------------------------------*/
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
        int i;
	static bool mouseOnBtn=false;

	/* --------- Rule out mouse postion out of workarea -------- */


        /* 1. !!!FIRST:  Check if mouse hovers over any button */
  /* Rule out range box of contorl buttons */
  if( mouseOnBtn ||  ( pmostat->mouseX > surfshmem->x0 && pmostat->mouseX < surfshmem->x0+surfshmem->vw
	&& pmostat->mouseY > surfshmem->y0 + SURF_TOPBAR_HEIGHT+5+panH+5 && pmostat->mouseY < surfshmem->y0+surfshmem->vh )
    ) {

	for(i=0; i<BTN_VALID_MAX; i++) {
                /* A. Check mouse_on_button */
                if( btns[i] ) {
                        mouseOnBtn=egi_point_on_surfbtn( btns[i], pmostat->mouseX -surfuser->surfshmem->x0,
                                                                pmostat->mouseY -surfuser->surfshmem->y0 );
                }
		// else
		//	continue;

                #if 1 /* TEST: --------- */
                if(mouseOnBtn)
                        printf(">>> Touch btns[%d].\n", i);
                #endif

		/* B. If the mouse just moves onto a SURFBTN */
	        if(  mouseOnBtn && mpbtn != i ) {
			egi_dpstd("Touch BTN btns[%d], prev. mpbtn=%d\n", i, mpbtn);
                	/* B.1 In case mouse move from a nearby!! SURFBTN, restore its button image. */
	                if( mpbtn>=0 ) {  /* Here mpbtn is previous  */
				egi_surfBtn_writeFB(vfbdev, btns[mpbtn], SURFBTN_IMGBUF_NORMAL, 0, 0);
                	}
                        /* B.2 Put effect on the newly touched SURFBTN */
                        if( btns[i] ) {  /* ONLY IF sbtns[i] valid! */
			   #if 0 /* Mask */
	                        draw_blend_filled_rect(vfbdev, btns[i]->x0, btns[i]->y0,
        	                		btns[i]->x0+btns[i]->imgbuf->width-1,btns[i]->y0+btns[i]->imgbuf->height-1,
                	                	WEGI_COLOR_BLUE, 100);
			   #else /* imgbuf_effect */
				egi_surfBtn_writeFB(vfbdev, btns[i], SURFBTN_IMGBUF_EFFECT, 0, 0);
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
			egi_surfBtn_writeFB(vfbdev, btns[mpbtn], SURFBTN_IMGBUF_NORMAL, 0, 0);

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
    }

	/* NOW: mpbtn updated */

        /* 2. If LeftKeyDown(Click) on buttons */
        if( pmostat->LeftKeyDown && mouseOnBtn ) {
                egi_dpstd("LeftKeyDown mpbtn=%d\n", mpbtn);

		if(mpbtn != BTN_PLAY) {
		    	#if 0   /* Same effect: Put mask */
			draw_blend_filled_rect(vfbdev, btns[mpbtn]->x0 +1, btns[mpbtn]->y0 +1,
        			btns[mpbtn]->x0+btns[mpbtn]->imgbuf->width-1 -1, btns[mpbtn]->y0+btns[mpbtn]->imgbuf->height-1 -1,
                		WEGI_COLOR_DARKGRAY, 160);
		    	#else /* Apply imgbuf_effect */
			egi_surfBtn_writeFB(vfbdev, btns[mpbtn], SURFBTN_IMGBUF_EFFECT, 0, 0);
		    	#endif
		}

		/* Set pressed */
		btns[mpbtn]->pressed=true;

                /* If any SURFBTN is touched, do reaction! */
                switch(mpbtn) {
                        case BTN_PREV:
				if(radioSIG != SIG_NONE)
					break;

				/* Inch down to make some effect */
				egi_surfBtn_writeFB(vfbdev, btns[mpbtn], SURFBTN_IMGBUF_EFFECT, 0, 1);
				radioSIG = SIG_PREV;
				break;
			case BTN_PLAY:
				/* Check signal */
				if(radioSIG != SIG_NONE)
					break;

				/* Toggle btn_play/btn_stop */
				btn_tmp=btns[BTN_PLAY];
				btns[BTN_PLAY]=btns[BTN_PLAY_SWITCH];
				btns[BTN_PLAY_SWITCH]=btn_tmp;

				/* Switch */
				if( radioSTAT == STAT_STOP ) {
					radioSIG = SIG_PLAY;
				}
				else if( radioSTAT == STAT_PLAY || radioSTAT == STAT_CONNECT ) {
					radioSIG = SIG_STOP;
				}

				/* Draw new pressed button */
			        #if 1 /* Normal btn image + rim OR igmbuf_effect */
				egi_surfBtn_writeFB(vfbdev, btns[mpbtn], SURFBTN_IMGBUF_EFFECT, 0, 0);
				#else	 /* Apply imgbuf_pressed */
				egi_surfBtn_writeFB(vfbdev, btns[mpbtn], SURFBTN_IMGBUF_PRESSED, 0, 0);
				#endif

				break;
			case BTN_NEXT:
				if(radioSIG != SIG_NONE)
					break;

				/* Inch down to make some effect */
				egi_surfBtn_writeFB(vfbdev, btns[mpbtn], SURFBTN_IMGBUF_EFFECT, 0, 1);
				radioSIG = SIG_NEXT;
				break;
		}
	}

	/* 3. LeftKeyUp */
	if( pmostat->LeftKeyUp && mouseOnBtn ) {
		/* Resume button with normal image */
		if(btns[mpbtn]->pressed) {
			if(btns[mpbtn]->imgbuf_effect)
				egi_surfBtn_writeFB(vfbdev, btns[mpbtn], SURFBTN_IMGBUF_EFFECT, 0, 0);
			else
				egi_surfBtn_writeFB(vfbdev, btns[mpbtn], SURFBTN_IMGBUF_NORMAL, 0, 0);

			btns[mpbtn]->pressed=false;
		}
	}

	/* 4. If mouseDZ and mouse hovers on... */
	if( pmostat->mouseDZ ) {
		int pvol=0;  /* Volume [0 100] */
	   	if(mpbtn == BTN_PLAY ) {
			egi_adjust_pcm_volume( -pmostat->mouseDZ);
			egi_getset_pcm_volume(&pvol, NULL);

			/* Base Line */
			fbset_color2(vfbdev, WEGI_COLOR_WHITE);
			draw_wline_nc(vfbdev, btns[BTN_PLAY]->x0+5, btns[BTN_PLAY]->y0+34,
					      btns[BTN_PLAY]->x0+5 +55, btns[BTN_PLAY]->y0+34, 3);
			/* Volume Line */
			fbset_color2(vfbdev, WEGI_COLOR_ORANGE);
			draw_wline_nc(vfbdev, btns[BTN_PLAY]->x0+5, btns[BTN_PLAY]->y0+34,
					      btns[BTN_PLAY]->x0+5 +55*pvol/100, btns[BTN_PLAY]->y0+34, 3);
		}
	}

}


/*-----------------------------------------------
Load playlist from a text file, which contains URL
and radio name in each line separated by blanks:

http://...      古典音乐
mms://....	乡村音乐

Return:
	>0	Partially read in.
	=0	OK
	<0	Fails
-----------------------------------------------*/
int load_playlist(const char *fpath, struct radio_info **infoList,  int *listCnt)
{
	FILE *fil;
	char linebuff[RADIO_ADDR_MAXLEN+RADIO_NAME_MAXLEN+8];
//	char *delim=" 	\n"; /* SPACE, TAB, NL */
	char *pt;
	struct radio_info *list=NULL;
	int capacity=64;
	int cnt;

	fil=fopen(fpath, "r");
	if(fil==NULL) {
		egi_dperr("Fail fopen()");
		return -1;
	}

	list=calloc(capacity, sizeof(struct radio_info));
	if(list==NULL)
		return -2;


	cnt=0;
	while( fgets(linebuff, sizeof(linebuff), fil) != NULL )
	{
		printf("Linebuff: %s\n", linebuff);

		/* Read Address */
		pt=strtok(linebuff, " 	");  /* BLANK, TAB */
		if(pt==NULL) continue;
		strncpy( list[cnt].address, pt, RADIO_ADDR_MAXLEN-1);
		/* OK list[].address[last] ='\0' */

		/* Read Name */
		pt=strtok(NULL, "\n");	/* NL */
		if(pt==NULL) continue;
		/* Get rid of leading TABs/SPACEs */
		while( *pt==0x9 || *pt==0x20 ) pt++;
		if(*pt=='\0') continue;
		strncpy( list[cnt].name, pt, RADIO_NAME_MAXLEN-1); /* pt==NULL will cause strncpy segfault */

		/* Count and grow capacity */
		cnt++;
		if( cnt == capacity ) {
			if( egi_mem_grow((void**)&list, capacity*sizeof(struct radio_info), 64*sizeof(struct radio_info)) !=0 ) {
				*infoList = list;
				*listCnt = cnt;
				return 1;
			}
			capacity += 64;
		}
	}

	/* Pass out resutl */
	*infoList = list;
	*listCnt = cnt;
	egi_dpstd("Read in totally %d radio addresses!\n", cnt);

	return 0;
}
