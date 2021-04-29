/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Usage:
	./surf_wetradio  ( /home/texture/metal_v.png )

   Volum adjust: Put mouse cursor on PLAY button and roll mouse wheel.
   Radio select: Click on the panel and select in ListBox.

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

TODO:
1. When close MAIN surface, close all child surfaces at first!


Journal:
2021-04-21:
	1. A simple UI/UX framework design for the APP.
2021-04-22:
	1. Buttons and displaying panel labels.
	2. Radio control functions.
2021-04-23:
	1. Add load_playlist()
2021-04-24:
	1. Mouse event on LAB_RSNAME
2021-04-25:
	1. Add surf_ListBox()
2021-04-27:
	1. Improve ListBox_mouse_event
2021-04-28:
	1. Add another version of load_playlist()

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


#define RADIO_LIST_PATH "/home/FM_list"

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
int playlist_idx;
int playlist_max;

/* Load radio playlist from text file to playlist[] */
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
	SIG_NEXT,
	SIG_CHANGE,	/* Select a new radio station */
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

/* SURF Lables */
enum {
	LAB_STATUS	=0,	/* Status: connecting, playing, stop */
	LAB_RSNAME	=1,	/* Radio station name */
	LAB_DETAILS	=2,	/* Detail of data stream */
	LAB_MAX		=3	/* <--- Limit */
};
ESURF_LABEL *labels[LAB_MAX];
int mplab=-1;   /* Index of mouse touched lab, <0 invalid. */


/* User surface functions */
void 	my_draw_canvas(EGI_SURFUSER *surfuser);
void 	my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

/* ERING routine */
void	*surfuser_ering_routine(void *args);

/* ListBox, for selecting radio */
int 	surf_ListBox(EGI_SURFSHMEM *surfcaller, int x0, int y0);
void 	ListBox_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

int egi_surfListBox_addItem(ESURF_LISTBOX *listbox, const char *pstr);
void egi_surfListBox_redraw(EGI_SURFUSER *psurfuser, ESURF_LISTBOX *listbox);


/* Signal handler for SurfUser */
void signal_handler(int signo)
{
        if(signo==SIGINT) {
                egi_dpstd("SIGINT to quit the process!\n");
		surfuser_close_surface(&surfuser);
        }
}


/*----------------------------
	   MAIN()
-----------------------------*/
int main(int argc, char **argv)
{
	//int i,j;
	int x0=0,y0=0; /* Origin coord of the surface */
	char *pname=NULL;

        /* Set signal handler */
      	egi_common_sigAction(SIGINT, signal_handler);

	/* Load palylist */
	char *fpath=NULL;
	if(argc>2) fpath=argv[2];
	else	   fpath=RADIO_LIST_PATH;
	if( load_playlist(fpath, &playlist,  &playlist_max) <0 ) {
		printf("Fail to load '%s'.\n", RADIO_LIST_PATH);
		exit(EXIT_FAILURE);
	}
	if(playlist_max<=0) {
		printf("No legal addresses in '%s'!", RADIO_LIST_PATH);
		exit(EXIT_FAILURE);
	}

#if 0	/* Start EGI log */
        if(egi_init_log("/mmc/surf_wetradio.log") != 0) {
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
	labels[LAB_STATUS]=egi_surfLab_create(surfimg,20,SURF_TOPBAR_HEIGHT+5+2,20,SURF_TOPBAR_HEIGHT+5+2, panW-20, 25); /* img,xi,yi,x0,y0,w,h */
	egi_surfLab_updateText(labels[LAB_STATUS], "Stop");
	egi_surfLab_writeFB(vfbdev, labels[LAB_STATUS], egi_sysfonts.regular, 18, 18, WEGI_COLOR_LTYELLOW, 0, 0);
	/* 9.2 Station Name */
	labels[LAB_RSNAME]=egi_surfLab_create(surfimg,20,SURF_TOPBAR_HEIGHT+5+32,20,SURF_TOPBAR_HEIGHT+5+32, panW-20, 25); /* img,xi,yi,x0,y0,w,h */
	egi_surfLab_updateText(labels[LAB_RSNAME], playlist[playlist_idx].name);
	egi_surfLab_writeFB(vfbdev, labels[LAB_RSNAME], egi_sysfonts.regular, 18, 18, WEGI_COLOR_GREEN, 0, 0);
	/* 9.3 Stream detail */
	labels[LAB_DETAILS]=egi_surfLab_create(surfimg,20,SURF_TOPBAR_HEIGHT+5+60,20,SURF_TOPBAR_HEIGHT+5+60, panW-20, 20); /* img,xi,yi,x0,y0,w,h */
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
				playlist_idx++;
			else if ( radioSIG == SIG_PREV )
				playlist_idx--;

			if( playlist_idx > playlist_max-1 )
				playlist_idx=0;
			if( playlist_idx < 0 )
				playlist_idx = playlist_max-1;

			/* Update Labels */
        		pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */
			egi_surfLab_updateText(labels[LAB_STATUS], "Connecting...");
			egi_surfLab_writeFB(vfbdev, labels[LAB_STATUS], egi_sysfonts.regular, 18, 18, WEGI_COLOR_LTYELLOW, 0, 0);
			egi_surfLab_updateText(labels[LAB_RSNAME], playlist[playlist_idx].name);
			egi_surfLab_writeFB(vfbdev, labels[LAB_RSNAME], egi_sysfonts.regular, 18, 18, WEGI_COLOR_GREEN, 0, 0);
/* ------ <<<  Surface shmem Critical Zone  */
	                pthread_mutex_unlock(&surfshmem->shmem_mutex);

			start_radio(playlist[playlist_idx].address);

			radioSTAT = STAT_CONNECT;  /* Main loop to check STAT_PLAY, if stream file found. */
			radioSIG = SIG_NONE;
		}
		if(  radioSIG == SIG_PLAY && radioSTAT != STAT_PLAY ) {
        		pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */
			egi_surfLab_updateText(labels[LAB_STATUS], "Connecting...");
			egi_surfLab_writeFB(vfbdev, labels[LAB_STATUS], egi_sysfonts.regular, 18, 18, WEGI_COLOR_LTYELLOW, 0, 0);
			egi_surfLab_updateText(labels[LAB_RSNAME], playlist[playlist_idx].name);
			egi_surfLab_writeFB(vfbdev, labels[LAB_RSNAME], egi_sysfonts.regular, 18, 18, WEGI_COLOR_GREEN, 0, 0);
/* ------ <<<  Surface shmem Critical Zone  */
	                pthread_mutex_unlock(&surfshmem->shmem_mutex);

			start_radio(playlist[playlist_idx].address);

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
		else if( radioSIG == SIG_CHANGE ) { /* Change station, playlist_idx changed by surf_ListBox() */
			/* Update Labels */
        		pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */
			egi_surfLab_updateText(labels[LAB_RSNAME], playlist[playlist_idx].name);
			egi_surfLab_writeFB(vfbdev, labels[LAB_RSNAME], egi_sysfonts.regular, 18, 18, WEGI_COLOR_GREEN, 0, 0);
/* ------ <<<  Surface shmem Critical Zone  */
	                pthread_mutex_unlock(&surfshmem->shmem_mutex);

			if( radioSTAT == STAT_PLAY || radioSTAT == STAT_CONNECT ) {
				stop_radio();

        			pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */
				egi_surfLab_updateText(labels[LAB_STATUS], "Connecting...");
				egi_surfLab_writeFB(vfbdev, labels[LAB_STATUS], egi_sysfonts.regular, 18, 18, WEGI_COLOR_LTYELLOW, 0, 0);
/* ------ <<<  Surface shmem Critical Zone  */
	                	pthread_mutex_unlock(&surfshmem->shmem_mutex);

				start_radio(playlist[playlist_idx].address);
				radioSTAT = STAT_CONNECT;  /* Main loop to check STAT_PLAY, if stream file found. */
			}

			radioSIG = SIG_NONE;
		}
		else {  /* SIG_NEXT/PREV + STAT_STOP, In this case radioSIG will NEVER be cleared by auto! */
			radioSIG = SIG_NONE;
		}

		/* Check RADIO status */
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
		else if( radioSTAT == STAT_PLAY ) {
			if( stat("/tmp/a.stream", &sb)!=0 && stat("/tmp/b.stream", &sb)!=0 ) {
				radioSTAT = STAT_CONNECT;
        			pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */
				egi_surfLab_updateText(labels[LAB_STATUS], "Connecting...");
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


/*--------------------------------------------------------------------
                Mouse Event Callback
                (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event() .
--------------------------------------------------------------------*/
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
        int i;
	static bool mouseOnBtn=false;
	static bool mouseOnLab=false;

	/* --------- Rule out mouse postion out of workarea -------- */


        /* 1. !!!FIRST:  Check if mouse hovers over any button */
#if 0  /* Rule out range box of contorl buttons */
  if( mouseOnBtn ||  ( pmostat->mouseX > surfshmem->x0 && pmostat->mouseX < surfshmem->x0+surfshmem->vw
	&& pmostat->mouseY > surfshmem->y0 + SURF_TOPBAR_HEIGHT+5+panH+5 && pmostat->mouseY < surfshmem->y0+surfshmem->vh )
    ) {
#endif

	for(i=0; i<BTN_VALID_MAX; i++) {
                /* A. Check mouse_on_button */
                if( btns[i] ) {
                        mouseOnBtn=egi_point_on_surfBtn( btns[i], pmostat->mouseX -surfuser->surfshmem->x0,
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

	/* Check if on LAB_RSNAME */
	if( !mouseOnBtn ) {
		mouseOnLab=egi_point_on_surfBox( (ESURF_BOX *)labels[LAB_RSNAME], pmostat->mouseX -surfuser->surfshmem->x0,
                                                                pmostat->mouseY -surfuser->surfshmem->y0 );
		/* If the mouse just moves onto LAB */
		if(mouseOnLab && mplab<0 ) {
		    	/* Same effect: Put mask */
			draw_blend_filled_rect(vfbdev,  labels[LAB_RSNAME]->x0, labels[LAB_RSNAME]->y0,
        						labels[LAB_RSNAME]->x0+labels[LAB_RSNAME]->imgbuf->width-1 -1,
							labels[LAB_RSNAME]->y0+labels[LAB_RSNAME]->imgbuf->height-1 -1,
                					WEGI_COLOR_LTYELLOW, 180);

			mplab=LAB_RSNAME;
		}
		/* If the mouse leaves the LAB */
		else if(!mouseOnLab && mplab== LAB_RSNAME) {
			egi_surfLab_writeFB(vfbdev, labels[LAB_RSNAME], egi_sysfonts.regular, 18, 18, WEGI_COLOR_GREEN, 0, 0);
			mplab =-1;
		}
	}

#if 0
    } /* End range box check */
#endif

	/* NOW: mpbtn updated */

        /* 2. If LeftKeyDown(Click) on buttons */
        if( pmostat->LeftKeyDown ) {
	   /* 2.1 Mouse Click on Buttons */
	   if( mouseOnBtn ) {
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

	   } /* End mouseOnBtn */

	   /* 2.1 Mouse Click on LAB_RSNAME: Activate ListBox */
           else if(  mplab==LAB_RSNAME ) {
			int index;
			pthread_mutex_unlock(&surfshmem->shmem_mutex);
			index=surf_ListBox(surfshmem, 20,20);
			if(index>=0 && index != playlist_idx ) {
				playlist_idx = index;
				egi_dpstd("Change radio: playlist_idx =%d\n", playlist_idx);
				radioSIG = SIG_CHANGE;
			}
			pthread_mutex_lock(&surfshmem->shmem_mutex);
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


#if 0 ////////////////  PLAYLIST FORMAT 1  //////////////////////
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
	char *pt;
	struct radio_info *list=NULL;
	int capacity=64;	/* Capacity of list[], also and incremental memgrow items */
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

		/* 1. Read Address */
		pt=strtok(linebuff, " 	");  /* BLANK, TAB */
		/* 1.1 Check */
		if(pt==NULL) continue;			 /* If nothing */
		if( strstr(pt,"://")==NULL )continue;	 /* If NO  '://' */
		/* 1.2 Get Address */
		strncpy( list[cnt].address, pt, RADIO_ADDR_MAXLEN-1);
		/* OK list[].address[last] ='\0', as calloc() */

		/* 2. Read Name */
		pt=strtok(NULL, "\n");	/* NL */
		/* 2.1 Check */
		if(pt==NULL) continue;
		/* 2.2 Get rid of leading TABs/SPACEs */
		while( *pt==0x9 || *pt==0x20 ) pt++;
		if(*pt=='\0') continue;
		/* 2.3 Get Name */
		strncpy( list[cnt].name, pt, RADIO_NAME_MAXLEN-1); /* pt==NULL will cause strncpy segfault */

		/* 3. Count and grow capacity */
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

	if( cnt==0 ) {
		egi_dpstd("No radio address found!\n");
		return -2;
	}
	else
		egi_dpstd("Read in totally %d radio addresses!\n", cnt);

	return 0;
}

#else ////////////////  PLAYLIST FORMAT  2  //////////////////////
/*-----------------------------------------------
Load playlist from a text file, which contains URL
and radio name in each line separated by a comma:

古典音乐,http://...
乡村音乐,mms://....

Return:
	>0	Partially read in.
	=0	OK
	<0	Fails
-----------------------------------------------*/
int load_playlist(const char *fpath, struct radio_info **infoList,  int *listCnt)
{
	FILE *fil;
	char linebuff[RADIO_ADDR_MAXLEN+RADIO_NAME_MAXLEN+8];
	char *pt;
	struct radio_info *list=NULL;
	int capacity=64;	/* Capacity of list[], also and incremental memgrow items */
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

		/* 1. Read Name */
		pt=strtok(linebuff, ",");  /* comma  */
		/* 1.1 Check */
		if(pt==NULL) continue;			 /* If nothing */
		/* 1.2 Get Name */
		strncpy( list[cnt].name, pt, RADIO_NAME_MAXLEN-1); /* pt==NULL will cause strncpy segfault */
		/* OK list[].name[last] ='\0' as calloc() */

		/* 2. Read Address */
		pt=strtok(NULL, ",");	/*  */
		/* 2.1 Check */
		if(pt==NULL) continue;
		if( strstr(pt,"://")==NULL )continue;	 /* If NO  '://' */
		/* 2.2 Get rid of leading TABs/SPACEs */
		while( *pt==0x9 || *pt==0x20 ) pt++;
		if(*pt=='\0') continue;
		/* 2.3 Get Address */
		strncpy( list[cnt].address, pt, RADIO_ADDR_MAXLEN-1);

		/* 3. Count and grow capacity */
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

	if( cnt==0 ) {
		egi_dpstd("No radio address found!\n");
		return -2;
	}
	else
		egi_dpstd("Read in totally %d radio addresses!\n", cnt);

	return 0;
}
#endif  //////////////////////////////////////////////////////////


/*----------------------------------------------------------
To create a SURFACE for selecting radio.

Note:
1. If it's called in mouse event function, DO NOT forget to
   unlock shmem_mutex.

@x0,y0: Origin coordinate relative to SURFACE of the caller.
	( also as previous level surface )

Return:
	>=0		Ok, index of list item.
	<0		Fails.
-----------------------------------------------------------*/
int surf_ListBox(EGI_SURFSHMEM *surfcaller, int x0, int y0)
{
	int i;

	/* Surface outline size */
	int msw=panW+30;
	int msh=180;

	/* For list box */
	ESURF_LISTBOX	*listbox=NULL;

	/* Get ref. pointer */
EGI_SURFUSER     *msurfuser=NULL;
EGI_SURFSHMEM    *msurfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *mvfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *msurfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  mcolorType=SURF_RGB565;  /* surfuser->vfbdev color type */
EGI_16BIT_COLOR  mbkgcolor=WEGI_COLOR_GRAYB;

	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	msurfuser=egi_register_surfuser(ERING_PATH_SURFMAN, surfcaller->x0+x0, surfcaller->y0+y0,
								msw, msh, msw, msh, mcolorType ); /* Fixed size */
	if(msurfuser==NULL) {
		printf("Fail to register surfuser!\n");
		return -1;
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	mvfbdev=&msurfuser->vfbdev;
	msurfimg=msurfuser->imgbuf;
	msurfshmem=msurfuser->surfshmem;

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons etc. */
        //xsurfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	//xsurfshmem->redraw_surface 	= surfuser_redraw_surface;
	//xsurfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	//xsurfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        msurfshmem->close_surface 	= surfuser_close_surface;
	msurfshmem->user_mouse_event	= ListBox_mouse_event;

	/* 4. Name for the surface. */
	strncpy(msurfshmem->surfname, "选择电台", SURFNAME_MAX-1);

	/* 5. First draw ListBox surface. */
	msurfshmem->bkgcolor=mbkgcolor; /* OR default BLACK */
	surfuser_firstdraw_surface(msurfuser, TOPBTN_CLOSE); /* Default firstdraw operation */
//	surfuser_firstdraw_surface(msurfuser, TOPBAR_NONE); /* Default firstdraw operation */

	/* 6. Draw ListBox */
	/* 6.1 Firstdraw/Create ListBox */
	draw_filled_rect2(mvfbdev, WEGI_COLOR_WHITE, 1, SURF_TOPBAR_HEIGHT, 1+((msw-2)-ESURF_LISTBOX_SCROLLBAR_WIDTH)-1, msh-2);
	listbox=egi_surfListBox_create(msurfimg, 1, SURF_TOPBAR_HEIGHT, 1, SURF_TOPBAR_HEIGHT,  /* imgbuf, xi, yi, x0, y0, w, h, ListBarH */
						   msw-2, msh-SURF_TOPBAR_HEIGHT-1, 16+4 );
	if( listbox == NULL ) {
		egi_dpstd("Fail to create ListBox!\n");
		egi_unregister_surfuser(&msurfuser);
		return -2;
	}

	/* Set font size */
	listbox->fh=16; listbox->fw=16;

	/* Add items to listbox */
	for(i=0; i<playlist_max; i++)
		egi_surfListBox_addItem(listbox, playlist[i].name);

	//XXX set  FirstIdx 
	listbox->FirstIdx=playlist_idx;

	/* 6.2 Draw ListBox content */
	if( playlist_idx <0 )
		playlist_idx=0;

#if 1  ////////////// Call egi_surfListBox_redraw() ////////////////////
	/* Cal. GuideBlockH */
	if( playlist_max <=0 )
		listbox->GuideBlockH = listbox->ListBoxH;
	else {
		listbox->GuideBlockH= listbox->ListBoxH * ( listbox->MaxViewItems<playlist_max? listbox->MaxViewItems : playlist_max )/playlist_max;
		if(listbox->GuideBlockH<1)
			listbox->GuideBlockH=1;
	}

	/* Cal. listbox->pastPos */
	if( playlist_idx>0 && playlist_max>0 ) {
		listbox->pastPos = listbox->ScrollBarH*playlist_idx/playlist_max;
                if(listbox->pastPos<0)
                        listbox->pastPos=0;
                else if( listbox->pastPos > listbox->ScrollBarH - listbox->GuideBlockH)
                        listbox->pastPos=listbox->ScrollBarH - listbox->GuideBlockH;
	}
	else
		listbox->pastPos = 0;

	/* Redraw LISTBOX with content */
	egi_surfListBox_redraw(msurfuser, listbox);

#else  ///////////////////////   XXX egi_surfListBox_redraw()   //////////////////////

	for(i=0; i< listbox->MaxViewItems && i < playlist_max-playlist_idx; i++) {
		FTsymbol_uft8strings_writeFB( &listbox->ListFB, egi_appfonts.regular,   /* FBdev, fontface */
        	       	                  listbox->fw, listbox->fh, (UFT8_PCHAR)playlist[i+playlist_idx].name,		/* fw,fh, pstr */
                	       	          listbox->ListBoxW, 1, 0,   	/* pixpl, lines, fgap */
                        	       	  5, i*listbox->ListBarH +2,  	/* x0,y0, */
                                	  WEGI_COLOR_BLACK, -1, 255,    /* fontcolor, transcolor,opaque */
	       	                          NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	}
	/* Paste ListImgbuf to surface */
	egi_subimg_writeFB(listbox->ListImgbuf, mvfbdev, 0, -1, listbox->x0, listbox->y0);

	/* 6.3 Draw ListBox vertical scroll bar and GuideBlock */
	/* Cal. GuideBlockH */
	if( playlist_max <=0 )
		listbox->GuideBlockH = listbox->ListBoxH;
	else {
		listbox->GuideBlockH= listbox->ListBoxH * ( listbox->MaxViewItems<playlist_max? listbox->MaxViewItems : playlist_max )/playlist_max;
		if(listbox->GuideBlockH<1)
			listbox->GuideBlockH=1;
	}
	/* Cal. listbox->pastPos */
	if( playlist_idx>0 && playlist_max>0 ) {
		listbox->pastPos = listbox->ScrollBarH*playlist_idx/playlist_max;
                if(listbox->pastPos<0)
                        listbox->pastPos=0;
                else if( listbox->pastPos > listbox->ScrollBarH - listbox->GuideBlockH)
                        listbox->pastPos=listbox->ScrollBarH - listbox->GuideBlockH;
	}
	else
		listbox->pastPos = 0;

	/* Draw scroll GUIDEBLOCK */
	draw_filled_rect2(mvfbdev, WEGI_COLOR_ORANGE, listbox->x0 +listbox->ListBoxW, listbox->y0 +listbox->pastPos,
			listbox->x0 +listbox->ListBoxW +listbox->GuideBlockW-1, listbox->y0 +listbox->pastPos +listbox->GuideBlockH-1 );

#endif //////////////////////////////////


	/* 7. Pre_set listbox->SelectIdx as invalid */
//	listbox->FirstIdx = -1;
	listbox->SelectIdx = -1;

	/* 8. Put listbox in SURFSHMEME.sboxes[]  */
	msurfshmem->sboxes[0]=(ESURF_BOX *)listbox;

	/* 9. Start Ering routine */
	printf("start ering routine...\n");
	if( pthread_create(&msurfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, msurfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		if( egi_unregister_surfuser(&msurfuser)!=0 )
			egi_dpstd("Fail to unregister surfuser!\n");
	}

	/* 10. Activate image */
	msurfshmem->sync=true;

 	/* ====== Main Loop ====== */
	 while( msurfshmem->usersig != 1 ) {
		usleep(100000);
	};

	/* Check returned selectIdx */
	printf("Return selectIdx=%d\n", listbox->SelectIdx);

        /* [POST_1] Join ering_routine  */
        // surfuser)->surfshmem->usersig =1;  // Useless if thread is busy calling a BLOCKING function.
	egi_dpstd("Cancel thread...\n");
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        /* Make sure mutex unlocked in pthread if any! */
	egi_dpstd("Joint thread_eringRoutine...\n");
        if( pthread_join(msurfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

	/* Unregister and destroy surfuser */
	egi_dpstd("Unregister surfuser...\n");
	if( egi_unregister_surfuser(&msurfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	egi_dpstd("Exit OK!\n");

	/* Return listbox item index */
	return listbox->SelectIdx;
}


/*-------------------------------------------------------------------------
                 Mouse Event Callback
            	(shmem_mutex locked!)

pmostat: mouseXYZ data is relative to SYS FB!

This is for ListBox surfaces.
1. It's a callback function to be embedded in surfuser_parse_mouse_event().
   To assign to SURFSHMEM.user_mouse_event().
2. XXX ---OK  			!!! WARNING !!!
   This function is for mouse events of all ListBox surfaces created by MAIN surface.
   When each mevent session ends, it MUST reset static vars.(FirstIdx, SelectIdx)
   OR their values will be remained for next ListBox surface, which will cause
   unexpected results or errors!
   And when mevent session starts, set FirstIdx=playlist_idx first.
3. When ListBox is created, FirstIdx is always set as current playlist_idx.
4. Click on list item to set listbox->SelectIdx = index;
   and msurfshmem->usersig = 1 to quit the ListBox surface.
5. Roll mouse wheel up/down to scroll up/down ListBox content.
6. Click in scroll bar area to page_up/page_down.
7. 
-------------------------------------------------------------------------*/
void ListBox_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
        int i;
	static int FirstIdx=-1;	   /* The first item index in ListBox, variable.
				    * Assign to listbox->FirstIdx when changes. (mouse wheel rolls)
				    * Reset to -1 when mevet session ends!
				    */

	static int SelectIdx=-1;   /* Selected index, variable,
				    * Pass to listbox->SelectIdx when clicked. (otherwise listbox->SelectIdx==-1)
				    * as NO item selected.
				    * Reset to -1 when mevet session ends!
				    */
	int index;
	int px,py;		   /* px,py  relative to surfshmem */

	bool mouseOnListBox=false;
	bool mouseOnScrollBar=false;
	bool mouseOnGuideBlock=false;

	/* --------- Rule out mouse postion out of workarea -------- */

	/* Get ref. pointers to vfbdev/surfimg/surfshmem */
	EGI_SURFSHMEM   *msurfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
	FBDEV           *mvfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
	EGI_IMGBUF	*msurfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
	ESURF_LISTBOX	*listbox=NULL;

	/* [PRE_1] Get ref. pointer */
	msurfshmem=surfuser->surfshmem;
	mvfbdev=&surfuser->vfbdev;
	msurfimg=surfuser->imgbuf;
	listbox=(ESURF_LISTBOX *)msurfshmem->sboxes[0];
	if(listbox==NULL)
		return;

	/* [PRE_2] Check Usersig */
	if( msurfshmem->usersig == 1 )
		return;

	/* [PRE_3] Transfer mouseXY to under current SURFACE coordinate. */
	px=pmostat->mouseX -msurfshmem->x0;
	py=pmostat->mouseY -msurfshmem->y0;

      	/* 0. Update/Init FirstIdx and pastPos. For each mevent session.
	 * Note:
	 *  If the SURFACE closed by clicking on SURFBTN_CLOSE, func private var. FirstIdx will NOT be reset!
	 *  ( since we haven't defined user_close_surfuser(). )
	 *  while listbox->FirstIdx always init. as -1  when listbox is created!
	 */
//	if( listbox->FirstIdx < 0 ) {  /* If start of new round mevent session. */
	if( surfuser->mevent_suspend ) { /* If start of new round mevent session. */
		printf(" Init FirstIdx ...\n");
		/* Init FirstIdx */
		FirstIdx=playlist_idx;
	//	listbox->FirstIdx=playlist_idx;

                /* Update listbox->pastPos, ScrollBarH set in surf_ListBox() */
                listbox->pastPos = listbox->ScrollBarH*FirstIdx/playlist_max;
                if(listbox->pastPos<0)
                        listbox->pastPos=0;
                else if( listbox->pastPos > listbox->ScrollBarH - listbox->GuideBlockH)
                        listbox->pastPos=listbox->ScrollBarH - listbox->GuideBlockH;

		/* XXX Limit: Make ListBox always full. */
		//if( FirstIdx > playlist_max-listbox->MaxViewItems && playlist_max>listbox->MaxViewItems )
		//	FirstIdx=playlist_max-listbox->MaxViewItems;
	}

	/* 1. Check if mouse hovers over any ARAEA */
	/* 1.1 On ListBox */
	if( pxy_inbox( px,py, listbox->x0, listbox->y0,  listbox->x0 + listbox->ListBoxW-1, listbox->y0 + listbox->ListBoxH-1)  )
	{
		mouseOnListBox=true;
	}
	/* 1.2 On scroll control block */
	else if ( pxy_inbox( px, py, listbox->x0 + listbox->ListBoxW, listbox->y0+listbox->pastPos,
					listbox->x0 + listbox->ListBoxW+ listbox->ScrollBarW -1,
					listbox->y0+listbox->pastPos+ listbox->GuideBlockH-1 ) )
	{
		mouseOnGuideBlock=true;
	}
	/* 1.3 On scroll bar area */
	else if ( pxy_inbox( px, py, listbox->x0 + listbox->ListBoxW, listbox->y0,
				     listbox->x0 + listbox->ListBoxW+ listbox->ScrollBarW -1, listbox->y0+ listbox->ScrollBarH -1) )
	{
		mouseOnScrollBar=true;
	}

	/* 2. If mouseDZ (mouse wheel), scroll and update content in ListBox */
//	int fw=16, fh=16, fgap=5;	/* Font size */
	if( mouseOnListBox && pmostat->mouseDZ ) {

		/* A. Refresh bkgIMG */
		#if 0 /* Write all ListBox BKIMG  */
		//egi_surfBox_writeFB(mvfbdev, (ESURF_BOX *)listbox, 0, 0);
		#else /* Refresh bkimg of scroll bar to surface imgbuf */
		egi_imgbuf_copyBlock( msurfimg, listbox->imgbuf, false, /*  destimg, srcimg, blendON */
				      listbox->ScrollBarW, listbox->ScrollBarH, /* bw,bh */
				      listbox->x0+listbox->ListBoxW, listbox->y0, /* xd,yd */
				      listbox->ListBoxW, 0); /* xs,ys */
		#endif

#if 0 /* --------  B. Use mouseDZ to driver listbox->pastPos ----------- */

		/* Update position of scroll bar GUIDEBLOCK */
		listbox->pastPos += pmostat->mouseDZ;
		if(listbox->pastPos<0)
			listbox->pastPos=0;
		else if( listbox->pastPos > listbox->ListBoxH - listbox->GuideBlockH)
			listbox->pastPos=listbox->ListBoxH - listbox->GuideBlockH;

		/* Update first index for items displayed in ListBox */
		FirstIdx=playlist_max*listbox->pastPos/listbox->ListBoxH;

		/* CtrBlockH: cal. in surf_ListBox(). */

		/* Draw scroll bar CONTORL_BLOCK */
		draw_filled_rect2(mvfbdev, WEGI_COLOR_ORANGE, listbox->x0 +listbox->ListBoxW, listbox->y0 +listbox->pastPos,
				listbox->x0 +listbox->ListBoxW +listbox->GuideBlockW-1, listbox->y0 +listbox->pastPos +listbox->GuideBlockH-1 );


#else /* --------  B. Use mouseDZ to driver FirstIdx ----------- */

		/* Update first index  */
		FirstIdx += pmostat->mouseDZ;
		if(FirstIdx < 0)
			FirstIdx=0;
		else if( FirstIdx > playlist_max-1)
			FirstIdx=playlist_max-1;

		/* CtrBlockH: cal. in surf_ListBox(). */

		/* Update listbox->pastPos */
		listbox->pastPos = listbox->ScrollBarH*FirstIdx/playlist_max;
		if(listbox->pastPos<0)
			listbox->pastPos=0;
		else if( listbox->pastPos > listbox->ScrollBarH - listbox->GuideBlockH)
			listbox->pastPos=listbox->ScrollBarH - listbox->GuideBlockH;

		/* Draw scroll bar GUIDEBLOCK */
		draw_filled_rect2(mvfbdev, WEGI_COLOR_ORANGE, listbox->x0 +listbox->ListBoxW, listbox->y0 +listbox->pastPos,
				listbox->x0 +listbox->ListBoxW +listbox->GuideBlockW-1, listbox->y0 +listbox->pastPos +listbox->GuideBlockH-1 );

#endif /* ----- END mouseDZ driver ----- */

		/* E. Limit FirstIdx to keep ListBox FULL of itmes. */
		if( FirstIdx > playlist_max-listbox->MaxViewItems && playlist_max>listbox->MaxViewItems )
			FirstIdx = playlist_max-listbox->MaxViewItems;

		printf("MouseDZ: FirstIdx=%d, ListBoxMaxViewItems=%d\n",FirstIdx, listbox->MaxViewItems);

		/* F. Assign to listbox->FirstIdx */
//		listbox->FirstIdx = FirstIdx;

		/* G. Refresh bkimg for ListImgbuf */
		egi_imgbuf_copyBlock( listbox->ListImgbuf, listbox->imgbuf, false, /*  destimg, srcimg, blendON */
					listbox->ListBoxW, listbox->ListBoxH, 0,0,  0,0 ); /*  bw,bh, xd,yd, xs,ys */

		/* H. Update/write ListBox items to ListImgbuf */
		for(i=0; i< listbox->MaxViewItems && i+FirstIdx < playlist_max; i++) {
			FTsymbol_uft8strings_writeFB( &listbox->ListFB, egi_appfonts.regular,   	/* FBdev, fontface */
        	       		                  listbox->fw, listbox->fh, (UFT8_PCHAR)playlist[i+FirstIdx].name,	/* fw,fh, pstr */
                	       		          listbox->ListBoxW, 1, 0,         		/* pixpl, lines, fgap */
                        	       		  5, i*listbox->ListBarH +2,  			/* x0,y0, */
	                                	  WEGI_COLOR_BLACK, -1, 255,     	/* fontcolor, transcolor,opaque */
		       	                          NULL, NULL, NULL, NULL);        	/* int *cnt, int *lnleft, int* penx, int* peny */
		}
		/* I. Paste ListImgbuf to surface */
		egi_subimg_writeFB(listbox->ListImgbuf, mvfbdev, 0, -1, listbox->x0, listbox->y0);
	}

	/* 3. Get mouse pointed item, and change its bkg color to hightlight it.
	 *    then set SelectIdx = index.
	 */
	else if( mouseOnListBox ) {

	      	/* A. Update first item index in ListBox, OK see at beginning case 0. */

		/* B. Cal. current moused pointed item index */
		index= FirstIdx +(py-SURF_TOPBAR_HEIGHT)/listbox->ListBarH;

		/* C. Limit index, in case BLANK items. */
		if( index > playlist_max-1 )
			return;
                printf("Playlist_idx=%d, FirstIdx=%d, index=%d, SelectIdx=%d\n", playlist_idx, FirstIdx, index, SelectIdx);

		/* D. Redraw ListBox if select_index changes */
		if( SelectIdx < 0 || SelectIdx != index ) {

			/* --1. Restor previous selected ListBar: redrwa bkgimg and content */
			if( SelectIdx >=0 && SelectIdx !=index ) {

				/* --1.1 Refresh item bkimg */
				egi_imgbuf_copyBlock( listbox->ListImgbuf, listbox->imgbuf, false, /*  destimg, srcimg, blendON */
					listbox->ListBoxW, listbox->ListBarH, 	      /* bw, bh */
					0, listbox->ListBarH*(SelectIdx-FirstIdx),    /* xd, yd */
					0, listbox->ListBarH*(SelectIdx-FirstIdx) );  /*xs,ys */

				/* --1.2 Restore item content to ListImgbuf */
				FTsymbol_uft8strings_writeFB( &listbox->ListFB, egi_appfonts.regular,   	/* FBdev, fontface */
        	       			                  listbox->fw, listbox->fh, (UFT8_PCHAR)playlist[SelectIdx].name,	/* fw,fh, pstr */
                	       			          listbox->ListBoxW, 1, 0,         		/* pixpl, lines, fgap */
                        	       			  5, (SelectIdx-FirstIdx)*listbox->ListBarH +2,  /* x0,y0, */
	                                		  WEGI_COLOR_BLACK, -1, 255,     	/* fontcolor, transcolor,opaque */
			       	                          NULL, NULL, NULL, NULL);        	/* int *cnt, int *lnleft, int* penx, int* peny */

			}

			/* --2. Draw/highlight newly selected items */
			#if 0 /* Mark newly selected item */
			draw_blend_filled_rect( mvfbdev, listbox->x0, listbox->y0+(index-FirstIdx)*listbox->ListBarH,
						 listbox->x0+listbox->ListBoxW -1, listbox->y0+(index-FirstIdx+1)*listbox->ListBarH-1,
               					 WEGI_COLOR_GRAY1, 180);

			#else /* Change newly selected ListBar bkgimg */
			draw_filled_rect2(&listbox->ListFB,  WEGI_COLOR_GRAY1, 0, listbox->ListBarH*(index-FirstIdx),
							listbox->ListBoxW-1, listbox->ListBarH*(index-FirstIdx+1)-1 );

			FTsymbol_uft8strings_writeFB( &listbox->ListFB, egi_appfonts.regular,   /* FBdev, fontface */
       	       			                  listbox->fw, listbox->fh, (UFT8_PCHAR)playlist[index].name,	/* fw,fh, pstr */
               	       			          listbox->ListBoxW, 1, 0,         		/* pixpl, lines, fgap */
                       	       			  5, (index-FirstIdx)*listbox->ListBarH +2,  	/* x0,y0, */
                                		  WEGI_COLOR_BLACK, -1, 255,     	/* fontcolor, transcolor,opaque */
		       	                          NULL, NULL, NULL, NULL);        	/* int *cnt, int *lnleft, int* penx, int* peny */

			#endif

			/* --3. Paste modified ListImgbuf to surface */
			egi_subimg_writeFB(listbox->ListImgbuf, mvfbdev, 0, -1, listbox->x0, listbox->y0);

			/* --4. Upate SelectIdx at last */
			SelectIdx = index;
		}
	}

	/* TODO: Close the SURFACE by clicking on SURFBTN_CLOSE */
	/* 4. Click on ListBox, return SelectIdx and signal to quit. */
	if( pmostat->LeftKeyDown && mouseOnListBox ) {

		/* 4.1 Upate ListBox SelectIdx ONLY when click, If by Close BTN, it keeps as -1. */
		listbox->SelectIdx = SelectIdx;
		printf("Click on ListBox, selectIdx=%d\n", listbox->SelectIdx);

		/* 4.2 Reset static FirstIdx and SelectIdx  */
		FirstIdx = -1;
		SelectIdx = -1;

		/* 4.3 Signal to quit Listbox surface */
		msurfshmem->usersig = 1;
	}
	/* 5. Click on ScrollBar to page up/down */
	else if( pmostat->LeftKeyDown && mouseOnScrollBar ) {
		/* Click before scroll GuideBlock: Update FirstIdx */
		if( py < listbox->y0+listbox->pastPos ) {
			//printf("Scroll page up!\n");
			FirstIdx -= listbox->MaxViewItems;
			if(FirstIdx < 0)
				FirstIdx = 0;
		}
		else if ( py >  listbox->y0+listbox->pastPos + listbox->GuideBlockH ) {
			//printf("Scroll page down!\n");
			FirstIdx += listbox->MaxViewItems;
			if(FirstIdx > playlist_max-1 )
				FirstIdx = playlist_max-1;
		}

		/* Update listbox->pastPos */
		listbox->pastPos = listbox->ScrollBarH*FirstIdx/playlist_max;
		if(listbox->pastPos<0)
			listbox->pastPos=0;
		else if( listbox->pastPos > listbox->ScrollBarH - listbox->GuideBlockH)
			listbox->pastPos=listbox->ScrollBarH - listbox->GuideBlockH;

		/* Refresh whole bkimg */
		egi_surfBox_writeFB(mvfbdev, (ESURF_BOX *)listbox, 0, 0);

		/* Draw scroll bar CONTORL_BLOCK */
		draw_filled_rect2(mvfbdev, WEGI_COLOR_ORANGE, listbox->x0 +listbox->ListBoxW, listbox->y0 +listbox->pastPos,
			listbox->x0 +listbox->ListBoxW +listbox->GuideBlockW-1, listbox->y0 +listbox->pastPos +listbox->GuideBlockH-1 );

               	/* Refresh bkimg for ListImgbuf */
                egi_imgbuf_copyBlock( listbox->ListImgbuf, listbox->imgbuf, false, /*  destimg, srcimg, blendON */
       	                                listbox->ListBoxW, listbox->ListBoxH, 0,0,  0,0 ); /*  bw,bh, xd,yd, xs,ys */

               	/* Update/write ListBox items to ListImgbuf */
                for(i=0; i< listbox->MaxViewItems && i+FirstIdx < playlist_max; i++) {
       	                FTsymbol_uft8strings_writeFB( &listbox->ListFB, egi_appfonts.regular,           /* FBdev, fontface */
               	                                  listbox->fw, listbox->fh, (UFT8_PCHAR)playlist[i+FirstIdx].name,        /* fw,fh, pstr */
                       	                          listbox->ListBoxW, 1, 0,                      /* pixpl, lines, fgap */
                               	                  5, i*listbox->ListBarH +2,                    /* x0,y0, */
                                       	          WEGI_COLOR_BLACK, -1, 255,            /* fontcolor, transcolor,opaque */
                                               	  NULL, NULL, NULL, NULL);              /* int *cnt, int *lnleft, int* penx, int* peny */
                }
       	        /* Paste ListImgbuf to surface */
               	egi_subimg_writeFB(listbox->ListImgbuf, mvfbdev, 0, -1, listbox->x0, listbox->y0);
	}
}


/*--------------------------------------------------
Add/copy item content to listbox->list[].

@listbox	Pointer to ESURF_LISTBOX
@pstr		Pointer to item content

Return:
	0	OK
	<0	Fails
--------------------------------------------------*/
int egi_surfListBox_addItem(ESURF_LISTBOX *listbox, const char *pstr)
{
	if( listbox == NULL || pstr == NULL )
		return -1;

	/* Check ListCapacity and growsize for listbox->list */
	if( listbox->TotalItems >= listbox->ListCapacity ) {

		if( egi_mem_grow((void **)&listbox->list, ESURF_LISTBOX_ITEM_MAXLEN*listbox->ListCapacity,	/* oldsize */
						     ESURF_LISTBOX_ITEM_MAXLEN*sizeof(char)*ESURF_LISTBOX_CAPACITY_GROWSIZE)  /* moresize */
		    <0 ) {
			egi_dpstd("Fail to memgrow for listbox->list!\n");
			return -2;
		}

		listbox->ListCapacity += ESURF_LISTBOX_CAPACITY_GROWSIZE;
	}

	/* Copy content into list */
	strncpy(listbox->list[listbox->TotalItems], pstr, ESURF_LISTBOX_ITEM_MAXLEN-1);

	/* Update TotalItems */
	listbox->TotalItems++;

	return 0;
}


/*--------------------------------------------------------------------
Redraw listbox and its view content to the surface( surfuser->imgbuf).

@psurfuser:	Pointer to EGI_SURFUSER.
@listbox:	Pointer to ESURF_LISTBOX.

---------------------------------------------------------------------*/
void egi_surfListBox_redraw(EGI_SURFUSER *psurfuser, ESURF_LISTBOX *listbox)
{
	int i;

	if( psurfuser==NULL || listbox==NULL )
		return;

	if( listbox->list==NULL || listbox->list[0]==NULL )
		return;

	/* Get ref. pointers to vfbdev/surfimg/surfshmem */
	//EGI_SURFSHMEM   *psurfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
	FBDEV           *pvfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
	EGI_IMGBUF	*psurfimg=NULL;          /* Only a ref. to surfuser->imgbuf */

	//psurfshmem=surfuser->surfshmem;
	pvfbdev=(FBDEV *)&psurfuser->vfbdev;
	psurfimg=psurfuser->imgbuf;

	/* 1. Refresh bkgIMG. ONLY for scrollbar area here. */
        //egi_surfBox_writeFB(mvfbdev, (ESURF_BOX *)listbox, 0, 0);
        egi_imgbuf_copyBlock( psurfimg, listbox->imgbuf, false, 			/* destimg, srcimg, blendON */
        			listbox->ScrollBarW, listbox->ScrollBarH, 	/* bw,bh */
                                listbox->x0+listbox->ListBoxW, listbox->y0, 	/* xd,yd */
                                listbox->ListBoxW, 0); 				/* xs,ys */

	/* 2. Draw scroll bar GUIDEBLOCK */
	draw_filled_rect2(pvfbdev, WEGI_COLOR_ORANGE, listbox->x0 +listbox->ListBoxW, listbox->y0 +listbox->pastPos,
			listbox->x0 +listbox->ListBoxW +listbox->GuideBlockW-1, listbox->y0 +listbox->pastPos +listbox->GuideBlockH-1 );

      	/* 3. Refresh bkimg for ListImgbuf */
        egi_imgbuf_copyBlock( listbox->ListImgbuf, listbox->imgbuf, false, 	/* destimg, srcimg, blendON */
                               listbox->ListBoxW, listbox->ListBoxH, 0,0,  0,0 ); /* bw,bh, xd,yd, xs,ys */

      	/* 4. Update/write ListBox items to ListImgbuf */
	if(listbox->MaxViewItems<1) {
	      	egi_subimg_writeFB(listbox->ListImgbuf, pvfbdev, 0, -1, listbox->x0, listbox->y0);
		return;
	}
        for(i=0; i< listbox->MaxViewItems && i+listbox->FirstIdx < listbox->TotalItems; i++) {
        	FTsymbol_uft8strings_writeFB( &listbox->ListFB, listbox->face,          /* FBdev, fontface */
               	                                  listbox->fw, listbox->fh,		/* fw,fh */
						  (UFT8_PCHAR)listbox->list[i+listbox->FirstIdx],   /* pstr */
                       	                          listbox->ListBoxW, 1, 0,              /* pixpl, lines, fgap */
                               	                  5, i*listbox->ListBarH +2,            /* x0,y0, */
                                       	          WEGI_COLOR_BLACK, -1, 255,            /* fontcolor, transcolor,opaque */
                                               	  NULL, NULL, NULL, NULL);              /* int *cnt, int *lnleft, int* penx, int* peny */
       	}

        /* 5. Paste ListImgbuf to surface */
      	egi_subimg_writeFB(listbox->ListImgbuf, pvfbdev, 0, -1, listbox->x0, listbox->y0);
}
