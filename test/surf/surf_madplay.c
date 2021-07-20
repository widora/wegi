/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

	EGI_SURF graphic version of madplay.

Usage:
	1. Play control
		BTN_PREV:  Click to play previous MP3 file.
			   Roll the wheel down to FBSEEK.
		BTN_PLAY:  Click to toggle Play/Pause
		BTN_NEXT:  Click to play next MP3 file.
			   Roll the wheel up to FFSEEK.
	2. Volume control:
		BTN_VOLUME: Move mouse onto the button and roll the wheel to adjust volume.
	3. Menu control:
		MENU_HELP:  Click to create a surface to display help content.
		MENU_OPTION: Click to create a surface for TEST purpose.
		TODO: MENU_FILE, MENU_OPTION

	4. Support keyboard application control keys:
		KEY_PALYPAUSE,KEY_VOLUEMUP/DOWN, KEY_PREVIOUSSONG/NEXTSONG

Multi_threads Scheme:
	1  Main loop:
	   ----> madplay process:   ---> header(), decode()
	2. thread_eringRoutine loop:					<sysfonts>
	   ----> surfuser_parse_mouse_event()   ---> redraw_surface()
		       |
		       --> 3. child thread_eringRoutine loop:		<appfonts>
				----> surfuser_parse_mouse_event()   ---> redraw_surface()

Note:
1. Quit process:
   1.1 Click on TOPBTN_CLOSE to set usersig=1.
   1.2 mad_flow header() check *pUserSig==1 and returns MAD_FLOW_STOP
       to quit current decoding session.
   2.3 main loop checks usersig==1, then break loop and release all
       sources before it ends program.

TODO:
1. EGI fonts arrangement for several threads.
   To cache/buffer different sizes of fonts.

Journal:
2021-04-10:
	1. Add minimad codes for MP3 decoding and playing.
2021-04-11:
	1. Add lab_passtime, lab_mp3name.
2021-04-12:
	1. mad_flow output(): correct nchannels and samplerate.
	2. Add menus[].
2021-04-13:
	1. Add BTN_VOLUME for SYS volume control. +Mute/Demute control.
2021-04-14:
	1. Roll the wheel to FFSEEK/FBSEEK.
2021-04-15:
	1. TEST: INCBIN   https://github.com/graphitemaster/incbin
	2. Add menu_help().
2021-04-20:
	1. Add menu_option()
	2. Add testsurf_mouse_event()
	3. menu_option surface recursive test.
2021-05-16:
	1. Search all MP3 file in the defaul diretory.
2021-05-21:
	1. TEST: To Register/FirstDraw/Sync surface and display it as early as possible.
2021-06-22:
	1. my_mouse_event(): Parse keyboard CONKEYs (pmostat->conkeys).
2021-06-24:
	1. Apply surface_module default surfuser_ering_routine().
2021-07-01:
	1. Apply surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MIN, MENU_MAX, menu_names)
	   auto draw menus[].

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
#include "egi_surfcontrols.h"
#include "egi_procman.h"
#include "egi_input.h"

#include "sys_incbin.h"
#include "surf_madplay.h"

/* PID lock file, to ensure only one running instatnce. */
const char *pid_lock_file="/var/run/surf_madplay.pid";

#ifdef LETS_NOTE
	#define DEFAULT_MP3_PATH "/home/midas-zhou/Music"
	INCBIN(NormalIcons, "/home/midas-zhou/Pictures/icons/gray_icons_normal_block.png");
	INCBIN(EffectIcons, "/home/midas-zhou/Pictures/icons/gray_icons_effect_block.png");
	INCBIN(BrightIcons, "/home/midas-zhou/Pictures/icons/gray_icons_bright.png");
#else
	#define DEFAULT_MP3_PATH "/mmc/music"

	INCBIN(NormalIcons, "/home/midas-zhou/Pictures/icons/gray_icons_normal_block.png");
	INCBIN(EffectIcons, "/home/midas-zhou/Pictures/icons/gray_icons_effect_block.png");
	INCBIN(BrightIcons, "/home/midas-zhou/Pictures/icons/gray_icons_bright.png");
#endif

/* For SURFUSER */
EGI_SURFUSER     *surfuser=NULL;
EGI_SURFSHMEM    *surfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *vfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *surfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  colorType=SURF_RGB565;  /* surfuser->vfbdev color type */
EGI_16BIT_COLOR  bkgcolor;

/* For SURF Buttons */
EGI_IMGBUF *icons_normal;
EGI_IMGBUF *icons_effect;
EGI_IMGBUF *icons_pressed;

enum {
	/* Working/On_display buttons */
	BTN_PREV 	=0,
	BTN_PLAY	=1,
	BTN_NEXT	=2,
	BTN_VOLUME	=3,
	BTN_VALID_MAX	=4, /* <--- Max number of working buttons. */

	/* Standby/switching buttons */
	BTN_PLAY_SWITCH	=5,  /* To switch with BTN_PLAY: PLAY/STOP */
	BTN_VOLUME_SWITCH=6,  /* To switch witch BTN_VOLUME */

	BTN_MAX		=7, /* <--- Limit, for allocation. */
};

ESURF_BTN  * btns[BTN_MAX];
ESURF_BTN  * btn_tmp=NULL;
int btnW=50;		/* Button size, same as original icon size. */
int btnH=50;
bool mouseOnBtn;
int mpbtn=-1;	/* Index of mouse touched button, <0 invalid. */

/* SURF Menus */
const char *menu_names[] = { "File", "Option", "Help"};
enum {
	MENU_FILE	=0,
	MENU_OPTION	=1,
	MENU_HELP	=2,
	MENU_MAX	=3, /* <--- Limit */
};
//ESURF_BOX	*menus[MENU_MAX];


int menuW;	/* var. */
int menuH=24;
bool mouseOnMenu;
//int mpmenu=-1;	/* Index of mouse touched menu, <0 invalid */


void menu_file();
void menu_option(EGI_SURFSHMEM *surfcaller, int x0, int y0);
void menu_help(EGI_SURFSHMEM *surfcaller, int x0, int y0);


/* ESURF Lables */
ESURF_BOX *lab_passtime; /* Label for passage of playing time. */
ESURF_BOX *lab_mp3name;  /* Label for MP3 file name. */

/* ERING routine. Use module default function.  */
//void            *surfuser_ering_routine(void *args);

/* Apply SURFACE module default function */
// void  surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat); /* shmem_mutex */
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

/* TEST: for menu_option surface */
void testsurf_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

void draw_PassTime(long intus); /* Passage of playing time */
void draw_mp3name(char *name);  /* MP3 file name */


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
	int i,j;
	int sw=0,sh=0; /* Width and Height of the surface */
	int x0=0,y0=0; /* Origin coord of the surface */
	char *pname=NULL;
	char **mp3_paths=NULL;
	int files;	/* Total number of MP3 files */


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

#if 0	/* Create sysfont buffer, for fw=15,fh=15. */
	printf("Create buffer for sysfont.bold...\n");
	egi_fontbuffer=FTsymbol_create_fontBuffer(egi_sysfonts.bold, 15, 15, 0x4E00, 21000);  /* face, fw, fh, unistart, size */
	if(egi_fontbuffer)
		printf("Buffer for sysfont.bold is created!\n");
	else
		printf("Fail to create sysfont.bold!\n");
#endif

/* Register and FirstDraw: Here or after all buttons created --------> */
	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	x0=0;y0=0;	sw=240; sh=170;
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, pid_lock_file, x0, y0, sw, sh, sw, sh, colorType ); /* Fixed size */
	if(surfuser==NULL) {
		printf("Fail to register surfuser!\n");
		exit(EXIT_FAILURE);
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	vfbdev=&surfuser->vfbdev;
	surfimg=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons etc. */
	// Defualt: surfuser_ering_routine() calls surfuser_parse_mouse_event();
        surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	//surfshmem->redraw_surface 	= surfuser_redraw_surface;
	//surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	//surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        surfshmem->close_surface 	= surfuser_close_surface;
	surfshmem->user_mouse_event	= my_mouse_event;

	pUserSig = &surfshmem->usersig;

	/* 4. Name for the surface. */
	pname="MadPlayer";
	strncpy(surfshmem->surfname, pname, SURFNAME_MAX-1);

	/* 5. First draw surface. */
	surfshmem->bkgcolor=  WEGI_COLOR_DARKOCEAN; //DRAKGRAY; /* OR default BLACK */
	surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MIN, MENU_MAX, menu_names); /* Default firstdraw operation */

	/* ----------> Activate image immediately! */
	surfshmem->sync=true;

	/* Search all MP3 file in the defaul diretory */
	mp3_paths=egi_alloc_search_files(DEFAULT_MP3_PATH, "mp3", &files);
	if(mp3_paths==NULL) {
		printf("No MP3 found!\n");
		exit(EXIT_FAILURE);
	}

	/* Set signal handler */
//	egi_common_sigAction(SIGINT, signal_handler);

	/* Enter private dir */
	chdir("/tmp/.egi");
	mkdir("madplay", 0770); /* "It is modified by the process's umask in the usual way: in the
       				   absence of a default ACL, the mode of the created directory is (mode & ~umask & 0777)." -MAN */
	if(chdir("/tmp/.egi/madplay")!=0) {
		egi_dperr("Fail to make/enter private dir!");
		exit(EXIT_FAILURE);
	}

	/* Load Noraml Icons */
	//icons_normal=egi_imgbuf_readfile("/mmc/gray_icons_normal_block.png");
	egi_copy_to_file("gray_icons_normal_block.png", gNormalIconsData, gNormalIconsSize, 0); 	 /* fpath, pstr, size, endtok */
	icons_normal=egi_imgbuf_readfile("gray_icons_normal_block.png");
	if( egi_imgbuf_setSubImgs(icons_normal, 12*5)!=0 ) {
		egi_dperr("Fail to setSubImgs for icons_normal!\n");
		exit(EXIT_FAILURE);
	}
	/* Set subimgs */
	for( i=0; i<5; i++ )
	   for(j=0; j<12; j++) {
		icons_normal->subimgs[i*12+j].x0= 25+j*75.5;		/* 25 margin, 75.5 Hdis */
		icons_normal->subimgs[i*12+j].y0= 145+i*73.5;		/* 73.5 Vdis */
		icons_normal->subimgs[i*12+j].w= 50;
		icons_normal->subimgs[i*12+j].h= 50;
	}

	/* Load Effect Icons */
	//icons_effect=egi_imgbuf_readfile("/mmc/gray_icons_effect_block.png");
	egi_copy_to_file("gray_icons_effect_block.png", gEffectIconsData, gEffectIconsSize, 0); 	 /* fpath, pstr, size, endtok */
	icons_effect=egi_imgbuf_readfile("gray_icons_effect_block.png");
	if( egi_imgbuf_setSubImgs(icons_effect, 12*5)!=0 ) {
		egi_dperr("Fail to setSubImgs for icons_effect!\n");
		exit(EXIT_FAILURE);
	}
	/* Set subimgs */
	for( i=0; i<5; i++ )
	   for(j=0; j<12; j++) {
		icons_effect->subimgs[i*12+j].x0= 25+j*75.5;		/* 25 Left_margin, 75.5 Hdis */
		icons_effect->subimgs[i*12+j].y0= 145+i*73.5;		/* 73.5 Vdis */
		icons_effect->subimgs[i*12+j].w= btnW;			/* 50x50 */
		icons_effect->subimgs[i*12+j].h= btnH;
	}

	/* Load Pressed Icons */
	//icons_pressed=egi_imgbuf_readfile("/mmc/gray_icons_bright.png");
	egi_copy_to_file("gray_icons_bright.png", gBrightIconsData, gBrightIconsSize, 0);          /* fpath, pstr, size, endtok */
	icons_pressed=egi_imgbuf_readfile("gray_icons_bright.png");
	if( egi_imgbuf_setSubImgs(icons_pressed, 12*5)!=0 ) {
		printf("Fail to setSubImgs for icons_pressed!\n");
		exit(EXIT_FAILURE);
	}
	/* Set subimgs */
	for( i=0; i<5; i++ )
	   for(j=0; j<12; j++) {
		icons_pressed->subimgs[i*12+j].x0= 25+j*75.5;		/* 25 Left_margin, 75.5 Hdis */
		icons_pressed->subimgs[i*12+j].y0= 145+i*73.5;		/* 73.5 Vdis */
		icons_pressed->subimgs[i*12+j].w= btnW;			/* 50x50 */
		icons_pressed->subimgs[i*12+j].h= btnH;
	}

	/* Create controls buttons: ( imgbuf, xi, yi, x0, y0, w, h ) */
	btns[BTN_PREV]=egi_surfBtn_create(icons_normal, 25+7*75.5, 145+4*73.5, 1+7, SURF_TOPBAR_HEIGHT+55, btnW, btnH);
	btns[BTN_PLAY]=egi_surfBtn_create(icons_normal, 25+6*75.5, 145+3*73.5, 1+7*2+50, SURF_TOPBAR_HEIGHT+55, btnW, btnH);
	btns[BTN_PLAY_SWITCH]=egi_surfBtn_create(icons_normal, 25+7*75.5, 145+3*73.5, 1+7*2+50, SURF_TOPBAR_HEIGHT+55, btnW, btnH);
	btns[BTN_NEXT]=egi_surfBtn_create(icons_normal, 25+6*75.5, 145+4*73.5, 1+7*3+50*2, SURF_TOPBAR_HEIGHT+55, btnW, btnH);
	btns[BTN_VOLUME]=egi_surfBtn_create(icons_normal, 25+10*75.5, 145+4*73.5, 1+7*4+50*3, SURF_TOPBAR_HEIGHT+55, btnW, btnH);
	btns[BTN_VOLUME_SWITCH]=egi_surfBtn_create(icons_normal, 25+11*75.5, 145+4*73.5, 1+7*4+50*3, SURF_TOPBAR_HEIGHT+55, btnW, btnH);

	/* Set imgbuf_effect: egi_imgbuf_blockCopy(inimg, px, py, height, width) */
	btns[BTN_PREV]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, 25+7*75.5, 145+4*73.5, btnW, btnH);
	btns[BTN_PLAY]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, 25+6*75.5, 145+3*73.5, btnW, btnH);
	btns[BTN_PLAY_SWITCH]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, 25+7*75.5, 145+3*73.5, btnW, btnH);
	btns[BTN_NEXT]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, 25+6*75.5, 145+4*73.5, btnW, btnH);
	btns[BTN_VOLUME]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, 25+10*75.5, 145+4*73.5, btnW, btnH);
	btns[BTN_VOLUME_SWITCH]->imgbuf_effect = egi_imgbuf_blockCopy(icons_effect, 25+11*75.5, 145+4*73.5, btnW, btnH);

	/* imgbuf_pressed, OR use mask: egi_imgbuf_blockCopy(inimg, px, py, height, width) */
	btns[BTN_PREV]->imgbuf_pressed = egi_imgbuf_blockCopy(icons_pressed, 25+7*75.5, 145+4*73.5, btnW, btnH);
	btns[BTN_PLAY]->imgbuf_pressed = egi_imgbuf_blockCopy(icons_pressed, 25+6*75.5, 145+3*73.5, btnW, btnH);
	btns[BTN_PLAY_SWITCH]->imgbuf_pressed = egi_imgbuf_blockCopy(icons_pressed, 25+7*75.5, 145+3*73.5, btnW, btnH);
	btns[BTN_NEXT]->imgbuf_pressed = egi_imgbuf_blockCopy(icons_pressed, 25+6*75.5, 145+4*73.5, btnW, btnH);

/* <--------- Register and FirstDraw: Here or before all buttons created... */

        /* Get surface mutex_lock */
        if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 ) {
        	egi_dperr("Fail to get mutex_lock for surface.");
		exit(EXIT_FAILURE);
        }
/* ------ >>>  Surface shmem Critical Zone  */

	/* 6. BLANK */

	/* 7. Create Lables */
	/* Create ESURF_BOX lab_mp3name */
	lab_mp3name=egi_surfBox_create(surfimg, 15, SURF_TOPBAR_HEIGHT+20+10, 15, SURF_TOPBAR_HEIGHT+20+10, 200, 20); /* imgbuf, xi, yi,  x0, y0, w, h */
	/* Create ESURF_BOX lab_passtime */
	lab_passtime=egi_surfBox_create(surfimg, 42, 142, 42, 142, 180, 20); /* imgbuf, xi, yi,  x0, y0, w, h */

	/* 8. First draw btns */
        egi_subimg_writeFB(btns[BTN_PREV]->imgbuf, vfbdev, 0, -1, btns[BTN_PREV]->x0, btns[BTN_PREV]->y0);
        egi_subimg_writeFB(btns[BTN_PLAY]->imgbuf, vfbdev, 0, -1, btns[BTN_PLAY]->x0, btns[BTN_PLAY]->y0);
        egi_subimg_writeFB(btns[BTN_NEXT]->imgbuf, vfbdev, 0, -1, btns[BTN_NEXT]->x0, btns[BTN_NEXT]->y0);
        egi_subimg_writeFB(btns[BTN_VOLUME]->imgbuf, vfbdev, 0, -1, btns[BTN_VOLUME]->x0, btns[BTN_VOLUME]->y0);

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* 9. Start Ering routine */
	printf("start ering routine...\n");
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		egi_dperr("Fail to launch thread_EringRoutine!");
		exit(EXIT_FAILURE);
	}

	/* 10. Activate image */
//	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);


  	/* MAD_0: Set termI/O 设置终端为直接读取单个字符方式 */
	egi_dpstd("Set termios()...\n");
  	egi_set_termios();

	/* MAD_1: Prepare vol 启动系统声音调整设备 */
	egi_dpstd("Getset PCM volume...\n");
  	egi_getset_pcm_volume(NULL,NULL);

  	/* MAD_2: Open pcm playback device 打开PCM播放设备. Ubuntu takes pcm devie exclusively!? */
	egi_dpstd("Open pcm_handle...\n");
  	if( snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) <0 ) {  /* SND_PCM_NONBLOCK,SND_PCM_ASYNC */
        	egi_dperr("Fail to open PCM playback device!\n");
	        return 1;
  	}


 /* ====== Main Loop ====== */
 while( surfshmem->usersig != 1 ) {

   /* Play all files 逐个播放MP3文件 */
   for(i=0; i<files; i++) {

while(madSTAT==STAT_PAUSE && surfshmem->usersig!=1) {
       if(madCMD==CMD_PLAY)
		break;
	usleep(100000);
};
	if( surfshmem->usersig==1 )
		break;

	egi_dpstd("Start to play %s\n", mp3_paths[i]);

        /* MAD_: Init filo 建立一个FILO用于存放MP3 HEADER　*/
        filo_headpos=egi_malloc_filo(1024, sizeof(MP3_HEADER_POS), FILOMEM_AUTO_DOUBLE);
        if(filo_headpos==NULL) {
                printf("Fail to malloc headpos FILO!\n");
                exit(EXIT_FAILURE);
        }

        /* MAD_: Reset 重置参数 */
        duration_frac=0;
        duration_sec=0;
        timelapse_frac=0;
        timelapse_sec=0;
        header_cnt=0;
        pcmparam_ready=false;
        pcmdev_ready=false;
        poff=0;
	timelapse=0;
	preset_timelapse=0;

	madCMD=CMD_NONE;

        /* MAD_: Mmap input file 映射当前MP3文件 */
madSTAT=STAT_LOAD;
//	draw_mp3name(basename(argv[i+1]));
	draw_mp3name(basename(mp3_paths[i]));
	snprintf(strPassTime, sizeof(strPassTime), "Loading...");
	draw_PassTime(0);

//        mp3_fmap=egi_fmap_create(argv[i+1], 0, PROT_READ, MAP_PRIVATE);
	egi_dpstd("Load file '%s'...\n", mp3_paths[i]);
        mp3_fmap=egi_fmap_create(mp3_paths[i], 0, PROT_READ, MAP_PRIVATE);
        if(mp3_fmap==NULL) {
	        egi_filo_free(&filo_headpos);
		continue;
		/*  ----------> continue for()  */
	}
        fsize=mp3_fmap->fsize;

        /* MAD_: To fill filo_headpos and calculation time duration. */
        decode_preprocess((const unsigned char *)mp3_fmap->fp, mp3_fmap->fsize);

        /* MAD_	: Calculate duration 计算总时长 */
        duration=(1.0*duration_frac/MAD_TIMER_RESOLUTION) + duration_sec;
        printf("\rDuration: %.1f seconds\n", duration);
        durHour=duration/3600; durMin=(duration-durHour*3600)/60;
        durfSec=duration-durHour*3600-durMin*60;

        /* MAD_ : Decoding 解码播放 */
madSTAT=STAT_PLAY;
//        printf(" Start playing...\n %s\n size=%lldk\n", argv[i+1], mp3_fmap->fsize>>10);
        printf(" Start playing...\n %s\n size=%lldk\n", mp3_paths[i], mp3_fmap->fsize>>10);
        mp3_decode((const unsigned char *)mp3_fmap->fp, mp3_fmap->fsize);

        /* MAD_ : Release source 释放相关资源　*/
        free(data_s16l); data_s16l=NULL;
        egi_fmap_free(&mp3_fmap);
        egi_filo_free(&filo_headpos);

	/* MAD_ : Parse command, after decode() quits at CMD_PREV/NEXT */
	if( madCMD == CMD_PREV ) {
        	if(i==0) i=-1;
                else i-=2;
		//printf("CMD_PREV, change to i=%d\n", i);
		madCMD=CMD_NONE;
	}
	else if( madCMD==CMD_NEXT ) { /* If CMD_NEXT, just go through. */
		madCMD=CMD_NONE;
	}

	/* MAD_ : Check usersig to quit */
	if( surfshmem->usersig==1 )
		break;

   } /* End for() */

 } /* END main_loop */

	/* MAD_FREE: */
	/* Close pcm handle */
  	snd_pcm_close(pcm_handle);
  	/* Reset termI/O */
  	egi_reset_termios();

        /* Free SURFBTNs */
        for(i=0; i<3; i++)
                egi_surfBtn_free(&surfshmem->sbtns[i]);

        /* Join ering_routine  */
        // surfuser)->surfshmem->usersig =1;  // Useless if thread is busy calling a BLOCKING function.
	egi_dpstd("Cancel thread...\n");
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        /* Make sure mutex unlocked in pthread if any! */
	egi_dpstd("Joint thread_eringRoutine...\n");
        if( pthread_join(surfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

	/* Unregister and destroy surfuser */
	egi_dpstd("Unregister surfuser...\n");
	if( egi_unregister_surfuser(&surfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	egi_dpstd("Exit OK!\n");
	exit(0);
}


/*-----------------------------------------------------------------
                Mouse Event Callback
               (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event() .
------------------------------------------------------------------*/
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
        int i;
	int pdelt=0;
	int pvol=0;  /* Volume [0 100] */

/* --------- E1. Parse Keyboard Event ---------- */

/* Parse CONKEYs F1...F12 */
if( pmostat->conkeys.press_F1 )
	printf("Press F1\n");

/* Switch CONKEYS.lastkey  */
if( pmostat->conkeys.press_lastkey )
{
   switch( pmostat->conkeys.lastkey )
   {
	case KEY_PLAYPAUSE:
		if( madSTAT!=STAT_LOAD ) {
			printf("Press PLAYPAUSE\n");

			/* Toggle btn_play/btn_pause */
			btn_tmp=btns[BTN_PLAY];
			btns[BTN_PLAY]=btns[BTN_PLAY_SWITCH];
			btns[BTN_PLAY_SWITCH]=btn_tmp;

			/* Draw new pressed button */
		        #if 1 /* Normal btn image + rim OR igmbuf_effect */
			egi_subimg_writeFB(btns[BTN_PLAY]->imgbuf_effect, vfbdev, 0, -1, btns[BTN_PLAY]->x0, btns[BTN_PLAY]->y0);
			#else /* Apply imgbuf_pressed */
			egi_subimg_writeFB(btns[BTN_PLAY]->imgbuf_pressed, vfbdev, 0, -1, btns[BTN_PLAY]->x0, btns[BTN_PLAY]->y0);
			#endif

			/* Swith MAD COMMAND, madSTAT set in mad_flow header(). */
			if(madSTAT==STAT_PLAY) {
				madCMD=CMD_PAUSE;
			}
			else if(madSTAT==STAT_PAUSE) {
				madCMD=CMD_PLAY;
			}
	   	}
		break;
	case KEY_VOLUMEUP:
	case KEY_VOLUMEDOWN:
	   	if( pmostat->conkeys.lastkey == KEY_VOLUMEUP ) {
			egi_dpstd("KEY_VOLUMEUP\n");
			pdelt=5;
		}
		else if( pmostat->conkeys.lastkey == KEY_VOLUMEDOWN ) {
			egi_dpstd("KEY_VOLUMEDOWN\n");
			pdelt=-5;
		}
		if(pdelt) {
			egi_adjust_pcm_volume(pdelt);
			egi_getset_pcm_volume(&pvol, NULL);

			/* Draw Base Line */
			fbset_color2(vfbdev, WEGI_COLOR_WHITE);
			draw_wline_nc(vfbdev, btns[BTN_VOLUME]->x0+5, btns[BTN_VOLUME]->y0+42,
					      btns[BTN_VOLUME]->x0+5 +40, btns[BTN_VOLUME]->y0+42, 3);

			/* Draw Volume Line */
			fbset_color2(vfbdev, WEGI_COLOR_ORANGE);
			draw_wline_nc(vfbdev, btns[BTN_VOLUME]->x0+5, btns[BTN_VOLUME]->y0+42,
					      btns[BTN_VOLUME]->x0+5 +40*pvol/100, btns[BTN_VOLUME]->y0+42, 3);
	   	}
		break;
	case KEY_PREVIOUSSONG:
		/* If current it's in STAT_PAUSE, then changes to STAT_PLAY, and switch BTN_PLAY. */
		if(madSTAT==STAT_PAUSE) {
			/* Toggle btn_play/btn_pause, and update btn image */
			btn_tmp=btns[BTN_PLAY];
			btns[BTN_PLAY]=btns[BTN_PLAY_SWITCH];
			btns[BTN_PLAY_SWITCH]=btn_tmp;
			egi_subimg_writeFB(btns[BTN_PLAY]->imgbuf, vfbdev,
						0, -1, btns[BTN_PLAY]->x0, btns[BTN_PLAY]->y0);
		}
		madCMD=CMD_PREV;
		break;
	case KEY_NEXTSONG:
		/* If current it's in STAT_PAUSE, then changes to STAT_PLAY, and switch BTN_PLAY. */
		if(madSTAT==STAT_PAUSE) {
			/* Toggle btn_play/btn_pause, and update btn image */
			btn_tmp=btns[BTN_PLAY];
			btns[BTN_PLAY]=btns[BTN_PLAY_SWITCH];
			btns[BTN_PLAY_SWITCH]=btn_tmp;
			egi_subimg_writeFB(btns[BTN_PLAY]->imgbuf, vfbdev,
						0, -1, btns[BTN_PLAY]->x0, btns[BTN_PLAY]->y0);
		}
		madCMD=CMD_NEXT;
		break;

	default:
		break;
   } /* Switch */
} /* conkey.press_lastkey */


/* --------- E2. Parse Mouse Event ---------- */

	/* --------- Rule out mouse postion out of workarea -------- */
	#if 0 /* NOTE: MAYBE you DO NOT need it! */
	if( pmostat->mouseX < surfshmem->x0 || pmostat->mouseX > surfshmem->y0+surfshmem->vw -1
	    ||  pmostat->mouseY < surfshmem->y0 || pmostat->mouseY > surfshmem->y0+surfshmem->vh -1
	  )
		return;
	#endif

        /* 1. !!!FIRST:  Check if mouse hovers over any button */
	/* TODO: Rule out range box of contorl buttons */
	for(i=0; i<BTN_VALID_MAX; i++) {
                /* A. Check mouse_on_button */
                if( btns[i] ) {
                        mouseOnBtn=egi_point_on_surfBtn( btns[i], pmostat->mouseX -surfuser->surfshmem->x0,
                                                                pmostat->mouseY -surfuser->surfshmem->y0 );
                }
//		else
//			continue;

                #if 0 /* TEST: --------- */
                if(mouseOnBtn)
                        printf(">>> Touch btns[%d].\n", i);
                #endif

		/* B. If the mouse just moves onto a SURFBTN */
	        if(  mouseOnBtn && mpbtn != i ) {
			egi_dpstd("Touch BTN btns[%d], prev. mpbtn=%d\n", i, mpbtn);
                	/* B.1 In case mouse move from a nearby SURFBTN, restore its button image. */
	                if( mpbtn>=0 ) {
        	        	egi_subimg_writeFB(btns[mpbtn]->imgbuf, vfbdev,
	                	        	0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);
                	}
                        /* B.2 Put effect on the newly touched SURFBTN */
                        if( btns[i] ) {  /* ONLY IF sbtns[i] valid! */
			   #if 0 /* Mask */
	                        draw_blend_filled_rect(vfbdev, btns[i]->x0, btns[i]->y0,
        	                		btns[i]->x0+btns[i]->imgbuf->width-1,btns[i]->y0+btns[i]->imgbuf->height-1,
                	                	WEGI_COLOR_BLUE, 100);
			   #else /* imgbuf_effect */
				egi_subimg_writeFB(btns[i]->imgbuf_effect, vfbdev,
                                                0, -1, btns[i]->x0, btns[i]->y0);
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
                        egi_subimg_writeFB(btns[mpbtn]->imgbuf, vfbdev, 0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);

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

	/* NOW: mpbtn updated */

	/* 1.A Check if mouse hovers over any menu */
	if( !mouseOnBtn ) {
		for(i=0; i<MENU_MAX; i++) {
	                /* A. Check mouse_on_menu */
                	mouseOnMenu=egi_point_on_surfBox( (ESURF_BOX *)surfshmem->menus[i], pmostat->mouseX -surfuser->surfshmem->x0,
                                                                	pmostat->mouseY -surfuser->surfshmem->y0 );

			/* B. If the mouse just moves onto a menu */
		        if(  mouseOnMenu && surfshmem->mpmenu != i ) {
				egi_dpstd("Touch a MENU. prev mpmenu=%d, i=%d\n", surfshmem->mpmenu, i);
                		/* B.1 In case mouse move from a nearby menu, restore its image. */
		                if( surfshmem->mpmenu>=0 ) {
        		        	egi_subimg_writeFB( surfshmem->menus[surfshmem->mpmenu]->imgbuf, vfbdev,
	        	        	        	0, -1, surfshmem->menus[surfshmem->mpmenu]->x0, surfshmem->menus[surfshmem->mpmenu]->y0);
                		}
	                        /* B.2 Put effect on the newly touched SURFBTN */
			   	#if 1 /* Mask */
	                	draw_blend_filled_rect(vfbdev, surfshmem->menus[i]->x0, surfshmem->menus[i]->y0,
        	                			surfshmem->menus[i]->x0+surfshmem->menus[i]->imgbuf->width-1,
							surfshmem->menus[i]->y0+surfshmem->menus[i]->imgbuf->height-1,
                	                		WEGI_COLOR_WHITE, 100);
				#else /* imgbuf_effect */
				egi_subimg_writeFB(surfshmem->menus[i]->imgbuf_effect, vfbdev,
                	                               0, -1, surfshmem->menus[i]->x0, surfshmem->menus[i]->y0);
				#endif

        	                /* B.3 Update mpmenu */
                	        surfshmem->mpmenu=i;

                        	/* B.4 Break for() */
	                        break;
        	 	}
                	/* C. If the mouse leaves a menu: Clear mpmenu */
	                else if( !mouseOnMenu && surfshmem->mpmenu == i ) {

        	                /* C.1 Draw/Restor original image */
                	        egi_subimg_writeFB(surfshmem->menus[surfshmem->mpmenu]->imgbuf, vfbdev, 0, -1,
							surfshmem->menus[surfshmem->mpmenu]->x0, surfshmem->menus[surfshmem->mpmenu]->y0);

                        	/* C.2 Reset pressed and Clear mpbtn */
				//menus[mpmenu]->pressed=false;
        	                surfshmem->mpmenu=-1;

                	        /* C.3 Break for() */
                        	break;
	                }
        	        /* D. Still on the menu, sustain... */
                	else if( mouseOnMenu && surfshmem->mpmenu == i ) {
                        	break;
	                }
		} /* EDN for(menus) */

	}


        /* 2. If LeftKeyDown(Click) on buttons */
        if( pmostat->LeftKeyDown && mouseOnBtn ) {
                egi_dpstd("LeftKeyDown mpbtn=%d\n", mpbtn);

		if(mpbtn != BTN_PLAY) {
		    	#if 1   /* Same effect: Put mask */
			draw_blend_filled_rect(vfbdev, btns[mpbtn]->x0 +1, btns[mpbtn]->y0 +1,
        			btns[mpbtn]->x0+btns[mpbtn]->imgbuf->width-1 -1, btns[mpbtn]->y0+btns[mpbtn]->imgbuf->height-1 -1,
                		WEGI_COLOR_DARKGRAY, 160);
		    	#else /* Apply imgbuf_effect */
			egi_subimg_writeFB(btns[mpbtn]->imgbuf_pressed, vfbdev, 0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);
		    	#endif
		}

		/* Set pressed */
		btns[mpbtn]->pressed=true;

                /* If any SURFBTN is touched, do reaction! */
                switch(mpbtn) {
                        case BTN_PREV:

				/* If current in STAT_PAUSE, then changes to STAT_PLAY, and switch BTN_PLAY. */
				if(madSTAT==STAT_PAUSE) {
					/* Toggle btn_play/btn_pause, and update btn image */
					btn_tmp=btns[BTN_PLAY];
					btns[BTN_PLAY]=btns[BTN_PLAY_SWITCH];
					btns[BTN_PLAY_SWITCH]=btn_tmp;
					egi_subimg_writeFB(btns[BTN_PLAY]->imgbuf, vfbdev,
								0, -1, btns[BTN_PLAY]->x0, btns[BTN_PLAY]->y0);
				}

				madCMD=CMD_PREV;

				break;
			case BTN_PLAY:
				/* Skip when loading */
				if(madSTAT==STAT_LOAD)
					break;

				/* Toggle btn_play/btn_pause */
				btn_tmp=btns[BTN_PLAY];
				btns[BTN_PLAY]=btns[BTN_PLAY_SWITCH];
				btns[BTN_PLAY_SWITCH]=btn_tmp;

				/* Draw new pressed button */
			        #if 1 /* Normal btn image + rim OR igmbuf_effect */
				egi_subimg_writeFB(btns[mpbtn]->imgbuf_effect, vfbdev, 0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);
				//egi_subimg_writeFB(btns[mpbtn]->imgbuf, vfbdev, 0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);
				//draw_blend_filled_rect(vfbdev, btns[mpbtn]->x0 +1, btns[mpbtn]->y0 +1,
        			//	btns[mpbtn]->x0+btns[mpbtn]->imgbuf->width-1 -1, btns[mpbtn]->y0+btns[mpbtn]->imgbuf->height-1 -1,
	                	//	WEGI_COLOR_DARKGRAY, 160);
				#else /* Apply imgbuf_pressed */
				egi_subimg_writeFB(btns[mpbtn]->imgbuf_pressed, vfbdev, 0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);
				#endif

				/* Swith MAD COMMAND, madSTAT set in mad_flow header(). */
				if(madSTAT==STAT_PLAY) {
					madCMD=CMD_PAUSE;
				}
				else if(madSTAT==STAT_PAUSE) {
					madCMD=CMD_PLAY;
				}

				break;
			case BTN_NEXT:
				/* If current it's in STAT_PAUSE, then changes to STAT_PLAY, and switch BTN_PLAY. */
				if(madSTAT==STAT_PAUSE) {
					/* Toggle btn_play/btn_pause, and update btn image */
					btn_tmp=btns[BTN_PLAY];
					btns[BTN_PLAY]=btns[BTN_PLAY_SWITCH];
					btns[BTN_PLAY_SWITCH]=btn_tmp;
					egi_subimg_writeFB(btns[BTN_PLAY]->imgbuf, vfbdev,
								0, -1, btns[BTN_PLAY]->x0, btns[BTN_PLAY]->y0);
				}

				madCMD=CMD_NEXT;
				break;
			case BTN_VOLUME:

				/* Swich mute/nonmute */
				btn_tmp=btns[BTN_VOLUME];
				btns[BTN_VOLUME]=btns[BTN_VOLUME_SWITCH];
				btns[BTN_VOLUME_SWITCH]=btn_tmp;
				egi_subimg_writeFB(btns[BTN_VOLUME]->imgbuf_effect, vfbdev,  /* mouse hover on, to use Effect */
							0, -1, btns[BTN_VOLUME]->x0, btns[BTN_VOLUME]->y0);

				/* Mute/Demute */
				egi_pcm_mute();

				break;
		}
	}

        /* 3. If LeftKeyDown(Click) on menu */
        if( pmostat->LeftKeyDown && mouseOnMenu ) {
                egi_dpstd("LeftKeyDown mpmenu=%d\n", surfshmem->mpmenu);

                /* If any SURFBTN is touched, do reaction! */
                switch(surfshmem->mpmenu) {
                        case MENU_FILE:
				break;
			case MENU_OPTION:
				pthread_mutex_unlock(&surfshmem->shmem_mutex);
				menu_option(surfshmem, 20, 20);
				pthread_mutex_lock(&surfshmem->shmem_mutex);
				break;
			case MENU_HELP:
				pthread_mutex_unlock(&surfshmem->shmem_mutex);
				menu_help(surfshmem, 20, 20);
				pthread_mutex_lock(&surfshmem->shmem_mutex);
				break;
		}
	}

	/* 3. LeftKeyUp */
	if( pmostat->LeftKeyUp && mouseOnBtn) {
		/* Resume button with normal image */
		if(btns[mpbtn]->pressed) {
			//egi_subimg_writeFB(btns[mpbtn]->imgbuf, vfbdev, 0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);
			egi_subimg_writeFB(btns[mpbtn]->imgbuf_effect, vfbdev, 0, -1, btns[mpbtn]->x0, btns[mpbtn]->y0);
			btns[mpbtn]->pressed=false;
		}
	}

	/* 4. If mouseDZ and mouse hovers on... */
	if( pmostat->mouseDZ ) {
		int pvol=0;  /* Volume [0 100] */
	   	switch(mpbtn) {
			case BTN_VOLUME:  /* Adjust volume */
				egi_adjust_pcm_volume( -pmostat->mouseDZ);
				egi_getset_pcm_volume(&pvol, NULL);

				/* Base Line */
				fbset_color2(vfbdev, WEGI_COLOR_WHITE);
				draw_wline_nc(vfbdev, btns[BTN_VOLUME]->x0+5, btns[BTN_VOLUME]->y0+42,
						      btns[BTN_VOLUME]->x0+5 +40, btns[BTN_VOLUME]->y0+42, 3);
				/* Volume Line */
				fbset_color2(vfbdev, WEGI_COLOR_ORANGE);
				draw_wline_nc(vfbdev, btns[BTN_VOLUME]->x0+5, btns[BTN_VOLUME]->y0+42,
						      btns[BTN_VOLUME]->x0+5 +40*pvol/100, btns[BTN_VOLUME]->y0+42, 3);
				break;
			case BTN_PREV:	/* Fast Backward */
				if( madSTAT==STAT_PLAY &&  -pmostat->mouseDZ < 0 ) {
					ffseek_val=pmostat->mouseDZ;
					madCMD=CMD_FBSEEK;
				}

				break;
			case BTN_NEXT:	/* Fast Forward */
				if( madSTAT==STAT_PLAY && -pmostat->mouseDZ > 0 ) {
					ffseek_val=-pmostat->mouseDZ;
					madCMD=CMD_FFSEEK;
				}
				break;

	   	}/* End switch */
	}
}


/*------------------------------------
Write passage of time for MP3 playing.

@intus:  Interval in usec.
------------------------------------*/
void draw_PassTime(long intus)
{
	static EGI_CLOCK eclock={0};
	long usec;

        pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */

	/* First time */
	if(eclock.status==ECLOCK_STATUS_IDLE) {
		egi_clock_start(&eclock);

		egi_surfBox_writeFB(vfbdev, lab_passtime,  0, 0);
  		FTsymbol_uft8strings_writeFB( vfbdev, egi_sysfonts.bold,   /* FBdev, fontface */
                	                  15, 15, (UFT8_PCHAR)strPassTime,	/* fw,fh, pstr */
                        	          180, 1, 0,         /* pixpl, lines, fgap */
                                	  lab_passtime->x0, lab_passtime->y0,    /* x0,y0, */
	                                  WEGI_COLOR_WHITE, -1, 220,     /* fontcolor, transcolor,opaque */
        	                          NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
	}

	usec=egi_clock_peekCostUsec(&eclock);
	if(usec >= intus) {
		egi_surfBox_writeFB(vfbdev, lab_passtime,  0, 0);
  		FTsymbol_uft8strings_writeFB( vfbdev, egi_sysfonts.bold,   /* FBdev, fontface */
                	                  15, 15, (UFT8_PCHAR)strPassTime,	/* fw,fh, pstr */
                        	          180, 1, 0,         /* pixpl, lines, fgap */
                                	  lab_passtime->x0, lab_passtime->y0,    /* x0,y0, */
	                                  WEGI_COLOR_WHITE, -1, 220,     /* fontcolor, transcolor,opaque */
        	                          NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
		egi_clock_stop(&eclock);
		egi_clock_start(&eclock);
	}

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);
}


/*------------------------------------
Write name of MP3 file on lab_mp3name.

------------------------------------*/
void draw_mp3name(char *name)
{
	char strbuf[256];
	int len;
	if(strlen(name)>4) {
		strncpy(strbuf, name, sizeof(strbuf)-1);
		len=strlen(strbuf);
		strbuf[len-4]='\0'; /* get rid of '.mp3' */
	}

        pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */

	egi_surfBox_writeFB(vfbdev, lab_mp3name,  0, 0);
	FTsymbol_uft8strings_writeFB( vfbdev, egi_sysfonts.bold,   /* FBdev, fontface */
               	                  15, 15, (UFT8_PCHAR)strbuf,	/* fw,fh, pstr */
                       	          200, 1, 0,         /* pixpl, lines, fgap */
                               	  lab_mp3name->x0, lab_mp3name->y0,    /* x0,y0, */
                                  WEGI_COLOR_ORANGE, -1, 220,     /* fontcolor, transcolor,opaque */
       	                          NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);
}


/*----------------------------------------------------------
To create a SURFACE for menu help.

@x0,y0: Origin coordinate relative to SURFACE of the caller.
	( also as previous level surface )

Note:
1. If it's called in mouse event function, DO NOT forget to
   unlock shmem_mutex.

TODO:
1. XXX When the menu SURFACE is brought up to TOP, while just before the SURFMAN
   refreshs the SYS FBDEV, if you happens to click on the father SURFACE at that point,
   the menu SURFACE would then hide behind the father SURFACE, and mstat CAN NOT
   reach to it!
   XXX ---- OK! Always top the surface with biggest value of level

---------------------------------------------------------*/
void menu_help(EGI_SURFSHMEM *surfcaller, int x0, int y0)
{
	int msw=210;
	int msh=150;

	const char *str_help = "   EGI图形版Madplay\n\n \
This program is under license of GNU GPL v2.\n \
 Enjoy!";

EGI_SURFUSER     *msurfuser=NULL;
EGI_SURFSHMEM    *msurfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *mvfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
//EGI_IMGBUF       *msurfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  mcolorType=SURF_RGB565;  /* surfuser->vfbdev color type */
//EGI_16BIT_COLOR  mbkgcolor;

	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	msurfuser=egi_register_surfuser(ERING_PATH_SURFMAN, NULL, surfcaller->x0+x0, surfcaller->y0+y0,
								msw, msh, msw, msh, mcolorType ); /* Fixed size */
	if(msurfuser==NULL) {
		printf("Fail to register surfuser!\n");
		return;
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	mvfbdev=&msurfuser->vfbdev;
	//msurfimg=msurfuser->imgbuf;
	msurfshmem=msurfuser->surfshmem;

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons etc. */
        //surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	//surfshmem->redraw_surface 	= surfuser_redraw_surface;
	//surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	//surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        msurfshmem->close_surface 	= surfuser_close_surface;
	//surfshmem->user_mouse_event	= my_mouse_event;

	/* 4. Name for the surface. */
	strncpy(msurfshmem->surfname, "Help", SURFNAME_MAX-1);

	/* 5. First draw surface. */
	msurfshmem->bkgcolor=WEGI_COLOR_GRAYA; /* OR default BLACK */
	surfuser_firstdraw_surface(msurfuser, TOPBTN_CLOSE, 0, NULL); /* Default firstdraw operation */

	/* 6. Start Ering routine */
	printf("start ering routine...\n");
	if( pthread_create(&msurfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, msurfuser) !=0 ) {
		egi_dperr("Fail to launch thread_EringRoutine!");
		if( egi_unregister_surfuser(&msurfuser)!=0 )
			egi_dpstd("Fail to unregister surfuser!\n");
	}

	/* 7. Write content */
	FTsymbol_uft8strings_writeFB( mvfbdev, egi_appfonts.bold,   	/* FBdev, fontface */
               	                  15, 15, (UFT8_PCHAR)str_help,		/* fw,fh, pstr */
                       	          msw-10, 10, 3,         		/* pixpl, lines, fgap */
                               	  5,  35,    		  		/* x0,y0, */
                                  WEGI_COLOR_BLACK, -1, 240,     	/* fontcolor, transcolor,opaque */
       	                          NULL, NULL, NULL, NULL);        	/* int *cnt, int *lnleft, int* penx, int* peny */

	/* 7. Activate image */
	msurfshmem->sync=true;

 	/* ====== Main Loop ====== */
	 while( msurfshmem->usersig != 1 ) {
		usleep(100000);
	};

        /* Join ering_routine  */
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
}



/*------------------------------------------------------------
To create a SURFACE for menu help.

@surfcaller:	SURFSHMEM of A SURFACE who calls the function.
@x0,y0: 	Origin coordinate relative to SURFACE of the caller.
		( also as previous level surface )

------------------------------------------------------------*/
void menu_option(EGI_SURFSHMEM *surfcaller, int x0, int y0)
{
	int msw=180; /* Surface outline size */
	int msh=160;
	int i;

	const char *str_help = "Options:\n1. -----\n2. ----- \n3. -----";

EGI_SURFUSER     *msurfuser=NULL;
EGI_SURFSHMEM    *msurfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *mvfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *msurfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  mcolorType=SURF_RGB565;  /* surfuser->vfbdev color type */
//EGI_16BIT_COLOR  mbkgcolor;

	if(surfcaller==NULL)
		return;

REGISTER_SURFUSER:
	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	msurfuser=egi_register_surfuser(ERING_PATH_SURFMAN, NULL, surfcaller->x0+x0, surfcaller->y0+y0,
								msw, msh, msw, msh, mcolorType ); /* Fixed size */
	if(msurfuser==NULL) {
                /* If instance already running, exit */
                int fd;
                if( (fd=egi_lock_pidfile(pid_lock_file)) <=0 )
                        exit(EXIT_FAILURE);
                else
                        close(fd);

                usleep(100000);
                goto REGISTER_SURFUSER;
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	mvfbdev=&msurfuser->vfbdev;
	msurfimg=msurfuser->imgbuf;
	msurfshmem=msurfuser->surfshmem;

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons etc. */
        //surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	//surfshmem->redraw_surface 	= surfuser_redraw_surface;
	//surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	//surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        msurfshmem->close_surface 	= surfuser_close_surface;
	msurfshmem->user_mouse_event	= testsurf_mouse_event;

	/* 4. Name for the surface. */
	strncpy(msurfshmem->surfname, "Option", SURFNAME_MAX-1);

	/* 5. First draw surface. */
	msurfshmem->bkgcolor=WEGI_COLOR_GRAYA; /* OR default BLACK */
	surfuser_firstdraw_surface(msurfuser, TOPBTN_CLOSE, 0, NULL); /* Default firstdraw operation */

	/* 6. Draw top menus */
	fbset_color2(mvfbdev, WEGI_COLOR_MAROON); //FIREBRICK);
	draw_filled_rect(mvfbdev, 1, SURF_TOPBAR_HEIGHT+1, mvfbdev->virt_fb->width-2, SURF_TOPBAR_HEIGHT+1+menuH -1);
	int penx; int startx;
	penx=1+10;
	for(i=0; i<TESTBOX_MAXNUM; i++) {
		startx=penx;
		FTsymbol_uft8strings_writeFB( mvfbdev, egi_appfonts.regular,   	/* FBdev, fontface */
               		                  18, 18, (UFT8_PCHAR)menu_names[i],	/* fw,fh, pstr */
                       		          100, 1, 0,         			/* pixpl, lines, fgap */
                               		  startx, SURF_TOPBAR_HEIGHT+1,		/* x0,y0, */
	                                  WEGI_COLOR_WHITE, -1, 200,     	/* fontcolor, transcolor,opaque */
       		                          NULL, NULL, &penx, NULL);        	/* int *cnt, int *lnleft, int* penx, int* peny */

		/* Create menu/buttons  BTN_FILE, BNT_OPTION, BTN_HELP  (imgbuf, xi, yi,  x0, y0, w, h) */
		msurfshmem->sboxes[i]=egi_surfBox_create(msurfimg, startx-(10), SURF_TOPBAR_HEIGHT+1,  /* 1+10, name offset from box size */
						     startx-(10), SURF_TOPBAR_HEIGHT+1, (penx+20)-startx, menuH);
		penx +=15; /* As gap between 2 menu names */
	}

	/* 6. Start Ering routine */
	printf("start ering routine...\n");
	if( pthread_create(&msurfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, msurfuser) !=0 ) {
		egi_dperr("Fail to launch thread_EringRoutine!");
		if( egi_unregister_surfuser(&msurfuser)!=0 )
			egi_dpstd("Fail to unregister surfuser!\n");
	}

	/* 7. Write content */
	FTsymbol_uft8strings_writeFB( mvfbdev, egi_appfonts.bold,   	/* FBdev, fontface */
               	                  16, 16, (UFT8_PCHAR)str_help,		/* fw,fh, pstr */
                       	          msw-10, 10, 7,         		/* pixpl, lines, fgap */
                               	  10,  SURF_TOPBAR_HEIGHT+1+menuH+10,	/* x0,y0, */
                                  WEGI_COLOR_BLACK, -1, 240,     	/* fontcolor, transcolor,opaque */
       	                          NULL, NULL, NULL, NULL);        	/* int *cnt, int *lnleft, int* penx, int* peny */

	/* 7. Activate image */
	msurfshmem->sync=true;

 	/* ====== Loop ====== */
	 while( msurfshmem->usersig != 1 ) {
		usleep(100000);
	};

        /* Free SURFBOXs (menu box) */
        for(i=0; i<TESTBOX_MAXNUM; i++)
                egi_surfBox_free(&msurfshmem->sboxes[i]);

        /* Join ering_routine  */
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
}


/*-------------------------------------------------------------------------
                Mouse Event Callback
             (shmem_mutex locked!)

This is for menu_option surfaces.

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event() .
-------------------------------------------------------------------------*/
void testsurf_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
        int i;
	bool mouseOnMenu=false;
//	int  mpmenu=-1;

	/* --------- Rule out mouse postion out of workarea -------- */


	/* Get ref. pointers to vfbdev/surfimg/surfshmem */
	EGI_SURFSHMEM    *msurfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
	FBDEV            *mvfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
	//EGI_IMGBUF       *msurfimg=NULL;          /* Only a ref. to surfuser->imgbuf */

	msurfshmem=surfuser->surfshmem;
	mvfbdev=&surfuser->vfbdev;
	//msurfimg=surfuser->imgbuf;

	/* 1.A Check if mouse hovers over any menu */
	/* NOTE: Current surface MAY NOT be the TOP_surface.... */
	//if(!mouseOnBtn) {
	if(true) {
		for(i=0; i<TESTBOX_MAXNUM; i++) {
        	        /* A. Check mouse_on_menu */
	               	mouseOnMenu=egi_point_on_surfBox( msurfshmem->sboxes[i], pmostat->mouseX -surfuser->surfshmem->x0,
                                                                	pmostat->mouseY -surfuser->surfshmem->y0 );

			/* B. If the mouse just moves onto a menu */
		        if(  mouseOnMenu && msurfshmem->mpbox != i ) {
				egi_dpstd("Touch a MENU mpbox=%d, i=%d\n", msurfshmem->mpbox, i);
                		/* B.1 In case mouse move from a nearby menu, restore its image. */
		                if( msurfshmem->mpbox>=0 ) {
        		        	egi_subimg_writeFB((msurfshmem->sboxes[msurfshmem->mpbox])->imgbuf, mvfbdev, 0, -1,
							msurfshmem->sboxes[msurfshmem->mpbox]->x0, msurfshmem->sboxes[msurfshmem->mpbox]->y0);
                		}
	                        /* B.2 Put effect on the newly touched SURFBTN */
			   	#if 1 /* Mask */
	                	draw_blend_filled_rect(mvfbdev, msurfshmem->sboxes[i]->x0, msurfshmem->sboxes[i]->y0,
        	                			msurfshmem->sboxes[i]->x0 + msurfshmem->sboxes[i]->imgbuf->width-1,
							msurfshmem->sboxes[i]->y0 + msurfshmem->sboxes[i]->imgbuf->height-1,
                	                		WEGI_COLOR_WHITE, 100);
				#else /* imgbuf_effect */
				egi_subimg_writeFB(msurfshmem->sboxes[i]->imgbuf_effect, mvfbdev,
                	                               0, -1, msurfshmem->sboxes[i]->x0, msurfshmem->sboxes[i]->y0);
				#endif

        	                /* B.3 Update mpbox */
                	        msurfshmem->mpbox=i;

                        	/* B.4 Break for() */
	                        break;
        	 	}
                	/* C. If the mouse leaves a menu: Clear mpbox */
	                else if( !mouseOnMenu && msurfshmem->mpbox == i ) {

        	                /* C.1 Draw/Restor original image */
                	        egi_subimg_writeFB(msurfshmem->sboxes[msurfshmem->mpbox]->imgbuf, mvfbdev, 0, -1,
							msurfshmem->sboxes[msurfshmem->mpbox]->x0,
							msurfshmem->sboxes[msurfshmem->mpbox]->y0);

                        	/* C.2 Reset pressed and Clear mpbtn */
        	                msurfshmem->mpbox=-1;

                	        /* C.3 Break for() */
                        	break;
	                }
        	        /* D. Still on the menu, sustain... */
                	else if( mouseOnMenu && msurfshmem->mpbox == i ) {
                        	break;
	                }
		} /* EDN for(menus) */

	}

        /* 2. If LeftKeyDown(Click) on menu */
        if( pmostat->LeftKeyDown && mouseOnMenu ) {
                egi_dpstd("LeftKeyDown mpbox=%d\n", msurfshmem->mpbox);

                /* If any SURFBTN is touched, do reaction! */
                switch(msurfshmem->mpbox) {
                        case MENU_FILE:
				break;
			case MENU_OPTION:
				pthread_mutex_unlock(&msurfshmem->shmem_mutex);
				menu_option(msurfshmem, 20, 20);
				pthread_mutex_lock(&msurfshmem->shmem_mutex);
				break;
			case MENU_HELP:
				break;
		}
	}

}
