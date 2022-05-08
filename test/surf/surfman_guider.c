/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Create an EGI Desktop guide bar.

Usage:
	Sound Control:
		Move mouse onto ICON_SOUND and roll mouse wheel to turn volume UP/DOWN.
	WiFi Control:
		Move mouse onto ICON_WIFI and click to turn ON/OFF WiFi connection.
	Brightness Control:
		Move mouse onto 'EGI_Desk' and click to activate surf_brightness.
		Roll mouse wheel to adjust brightness, Right_click or ESC to end.
	MenuListTree:
		Move mouse over item and click to select it.
		Right_click to quit menulist surface, OR Left_click NOT on any item.

Journal:
2021-05-27: Start programming...
            Finish.
	    Auto. relocation/alignment to TOP/BOTTOM of desk.
2021-05-31:
        1. Test: Auto. hide the surface...
			--- As a TOP_surface ---
	Alway receives mstat from ERING, and OK to reset/set sync to hide/show!

			--- As a NON_TOP_surface ---
	It receives mstat ONLY WHEN the mouse is within	its surface area,
	and it STOP receiving ONCE the mouse is out of the range.
	It means if the surface is displayed as a NON_TOP_surface then it
        CAN NOT reset sync to hide again! OR if it's hidden then it CAN NOT
	set sync to show itself neither.

2021-06-1:
	1. Update labicon_WIFI as per real_time RSSI.
	2. Update labicon_SOUND as per real_time audio volume.
2021-06-04:
	1. Ubus call to turn WiFi UP/DOWN.
	2. Make dynamic_icon effect when reloading WiFi interface.
2021-06-05:
	1. bool guider_in_position.
2022-04-14:
	1. Add a surfuser for brightness adjustment.
2022-04-24/25/26:
	1. Add a surfuser for calender.
2022-04-29:
	1. Add surf_menuListTree()
2022-05-01:
	1. Add surfmlist_mouse_event()
2022-05-03:
	1. surf_menuListTree(): return an MenuID, if it's MENU_ID_BACKLIGHT, then activate
	   SURFACE for backlight control.
2022-05-06:
	1. Add void* surf_monitorSystem() for MENU_ID_MONITOR_SYSTEM.  <----- As a thread_surface.
2022-05-07:
	1. Add void* surf_configSystem() for MENU_ID_CONFIG_SYSTEM.  <----- As a thread_surface.

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/sysinfo.h>

#include "egi_timer.h"
#include "egi_color.h"
#include "egi_log.h"
#include "egi_debug.h"
#include "egi_math.h"
#include "egi_FTsymbol.h"
#include "egi_surface.h"
#include "egi_procman.h"
#include "egi_input.h"
#include "egi_iwinfo.h"
#include "egi_pcm.h"
#include "egi_utils.h"

/* PID lock file, to ensure only one running instatnce. */
const char *pid_lock_file="/var/run/surfman_guider.pid";

/* SURF position */
bool guider_in_position;	/* True: If guider bar is in designated position. */

/* SURF Lable_Icons */
enum {
        LABICON_STATUS  =0,     /* CPU */
        LABICON_TIME    =1,     /* Clock */
        LABICON_SOUND   =2,     /* Sound */
        LABICON_WIFI	=3,     /* WiFi Info. */
	LABICON_SYSTEM  =4,     /* System */
        LABICON_MAX     =5     /* <--- Limit */
};
ESURF_LABEL *labicons[LABICON_MAX];
int mp=-1;   /* Index of mouse touched labicons[], <0 invalid. */

EGI_CLOCK eclock;
EGI_IMGBUF *wifi_icons[5+1]; /* RSSI LEVEL: 0,1,2,3,4; No_Connection 5 */
int wifi_icon_index; 	/* Index of wifi_icons[]. */
int wifi_level; 	/* WiFi signal strength level 0,1,2,3,4,5 */
bool wifi_reloading; /* WiFi interface is reloading */

EGI_IMGBUF *volume_icons[4];
int volume_icon_index; /* Index of volume_icons[] */
int volume_level;	/* 0,1,2,3 */
int vol_pval;		/* Volume percentage value */

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

/* Wait pid */
int wait_pid;
int wait_status;

/* Apply SURFACE module default function */
//void  *surfuser_ering_routine(void *args);
//void  surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat); /* shmem_mutex */
void my_firstdraw_surface(EGI_SURFUSER *surfuser, int options);
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);
void labicon_time_update(void);

/* Surface for Brightness/Backlight Control */
void surf_brightness(EGI_SURFSHMEM *surfowner, int x0, int y0);
void draw_canvas_brightness(EGI_SURFUSER *surfuser);
void surfbright_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

/* Surface for Calender */
void surf_calender(EGI_SURFUSER *surfowner);
void calender_draw_canvas(EGI_SURFUSER *msurfuser);
void calender_update(EGI_SURFUSER *msurfuser);

/* Surface for MenuListTree */

enum {  /*  MID List, MUST >0!  */
	MENU_ID_BACKLIGHT	=1,
	MENU_ID_SOUND		=2,
	MENU_ID_CONFIG_SYSTEM	=3,
	MENU_ID_MONITOR_SYSTEM  =4,
};

ESURF_MENULIST *menulist;  /* Root menulist for LABICON_SYSTEM */
bool menulist_ON;
int surf_menuListTree(EGI_SURFUSER *surfowner);
void surfmlist_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

/* Surface for system monitor */
pthread_t thread_monitorSystem;
void* surf_monitorSystem(void *argv);

/* Surface for system configuration */
pthread_t thread_configSystem;
void* surf_configSystem(void *argv);

//"ubus call network.wireless down",
const char *arg_wifi_down[3]={
	"network.wireless",
	"down",
	NULL,
};
const char *arg_wifi_up[3]={
	"network.wireless",
	"up",
	NULL,
};

/* Ubus call netifd */
int ubus_cli_call(const char *ubus_socket, int argc, const char**argv);
void *thread_ubus_wifi_down(void *arg);
void *thread_ubus_wifi_up(void *arg);

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
	//int i;
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

#if 0   /* Enter private dir */
        chdir("/tmp/.egi");
        mkdir("surf_xxx", 0777); /* 0777 ??? drwxr-xr-x */
        if(chdir("/tmp/.egi/surf_xxx")!=0) {
                egi_dperr("Fail to make/enter private dir!");
                exit(EXIT_FAILURE);
        }
#endif



REGISTER_SURFUSER:
	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	x0=-5; y0=SURF_MAXH-30;	sw=SURF_MAXW +5*2; sh=SURF_TOPBAR_HEIGHT;
	surfuser=egi_register_surfuser( ERING_PATH_SURFMAN, pid_lock_file, x0, y0, SURF_MAXW,SURF_MAXH, sw, sh, colorType );
	if(surfuser==NULL) {
		/* If instance already running, exit */
		int fd;
		if( (fd=egi_lock_pidfile(pid_lock_file)) <=0 )
			exit(EXIT_FAILURE);
		else
			close(fd);

		/* Try 3 time */
                int static ntry=0;
                ntry++;
                if(ntry==3)
                        exit(EXIT_FAILURE);

		usleep(100000);
		goto REGISTER_SURFUSER;
	}
	guider_in_position=true;

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

        /* 3. Assign OP functions, to connect with CLOSE/MIN./MAX. buttons etc. */
#if 0
	// Defualt: surfuser_ering_routine() calls surfuser_parse_mouse_event();
        surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	surfshmem->redraw_surface 	= surfuser_redraw_surface;
	surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        surfshmem->close_surface 	= surfuser_close_surface;
        surfshmem->draw_canvas          = my_draw_canvas;
#endif
        surfshmem->user_mouse_event     = my_mouse_event;

	/* 4. Give a name for the surface. */
	strncpy(surfshmem->surfname, "EGI Desktop", SURFNAME_MAX-1);

	/* 5. First draw surface */
	surfshmem->bkgcolor=WEGI_COLOR_GRAY; /* OR default BLACK */
	//surfuser_firstdraw_surface(surfuser, TOPBAR_NAME); /* Default firstdraw operation */
	my_firstdraw_surface(surfuser, TOPBAR_NAME); /* Default firstdraw operation */

	/* Load 24x24 icons */
	wifi_icons[0]=egi_imgbuf_readfile("/mmc/icons/wifi_0.png");	egi_imgbuf_resetColorAlpha(wifi_icons[0], WEGI_COLOR_WHITE, -1);
	wifi_icons[1]=egi_imgbuf_readfile("/mmc/icons/wifi_25.png"); 	egi_imgbuf_resetColorAlpha(wifi_icons[1], WEGI_COLOR_WHITE, -1);
	wifi_icons[2]=egi_imgbuf_readfile("/mmc/icons/wifi_50.png");	egi_imgbuf_resetColorAlpha(wifi_icons[2], WEGI_COLOR_WHITE, -1);
	wifi_icons[3]=egi_imgbuf_readfile("/mmc/icons/wifi_75.png");	egi_imgbuf_resetColorAlpha(wifi_icons[3], WEGI_COLOR_WHITE, -1);
	wifi_icons[4]=egi_imgbuf_readfile("/mmc/icons/wifi_100.png");	egi_imgbuf_resetColorAlpha(wifi_icons[4], WEGI_COLOR_WHITE, -1);
	wifi_icons[5]=egi_imgbuf_readfile("/mmc/icons/wifi_no_connect.png"); egi_imgbuf_resetColorAlpha(wifi_icons[5], WEGI_COLOR_WHITE, -1);

	volume_icons[0]=egi_imgbuf_readfile("/mmc/icons/avolume_0.png"); egi_imgbuf_resetColorAlpha(volume_icons[0], WEGI_COLOR_WHITE, -1);
	volume_icons[1]=egi_imgbuf_readfile("/mmc/icons/avolume_1.png"); egi_imgbuf_resetColorAlpha(volume_icons[1], WEGI_COLOR_WHITE, -1);
	volume_icons[2]=egi_imgbuf_readfile("/mmc/icons/avolume_2.png"); egi_imgbuf_resetColorAlpha(volume_icons[2], WEGI_COLOR_WHITE, -1);
	volume_icons[3]=egi_imgbuf_readfile("/mmc/icons/avolume_3.png"); egi_imgbuf_resetColorAlpha(volume_icons[3], WEGI_COLOR_WHITE, -1);

//	EGI_IMGBUF *audio_volume_100=egi_imgbuf_readfile("/mmc/icons/audio_volume_100.png");
	EGI_IMGBUF *battery_100_charging=egi_imgbuf_readfile("/mmc/icons/battery_100_charging.png");
	EGI_IMGBUF *pinyin_icon=egi_imgbuf_readfile("/mmc/icons/pinyin_icon.png");
	EGI_IMGBUF *system_icon=egi_imgbuf_readfile("/mmc/icons/system.png");

	/* Create Labels */
        labicons[LABICON_TIME]=egi_surfLab_create(surfimg, SURF_MAXW-5-60, 1, SURF_MAXW-5-60, 1, 80, 30-8); //2); /* img,xi,yi,x0,y0,w,h */
        egi_surfLab_updateText(labicons[LABICON_TIME], "00:00");
        egi_surfLab_writeFB(vfbdev, labicons[LABICON_TIME], egi_sysfonts.bold, 16, 16, WEGI_COLOR_LTYELLOW, 0, 4);

        labicons[LABICON_WIFI]=egi_surfLab_create(surfimg, SURF_MAXW-5-60 -40-30, 2, SURF_MAXW-5-60 -40-30, 2, 30, 30-2); /* img,xi,yi,x0,y0,w,h */
	labicons[LABICON_WIFI]->front_imgbuf=wifi_icons[0]; /* A ref. pointer */
	egi_surfLab_writeFB(vfbdev, labicons[LABICON_WIFI], egi_sysfonts.bold, 16, 16, WEGI_COLOR_WHITE, 0, 0);

        labicons[LABICON_SOUND]=egi_surfLab_create(surfimg, SURF_MAXW-5-60 -40-30-30, 2, SURF_MAXW-5-60 -40-30-30, 2, 30,30-2);/* img,xi,yi,x0,y0,w,h */
	labicons[LABICON_SOUND]->front_imgbuf=volume_icons[1];
	egi_surfLab_writeFB(vfbdev, labicons[LABICON_SOUND], egi_sysfonts.bold, 16, 16, WEGI_COLOR_WHITE, 0, 0);

	//labicons[LABICON_SYSTEM]=egi_surfLab_create(surfimg, SURF_MAXW-5-60 -40, 3, SURF_MAXW-5-60 -40, 3, 30, 30-2);
	labicons[LABICON_SYSTEM]=egi_surfLab_create(surfimg, SURF_MAXW-5-60 -40, 3, SURF_MAXW-5-60 -40-30-30-30, 3, 30, 30-2);
	labicons[LABICON_SYSTEM]->front_imgbuf=system_icon;
	egi_surfLab_writeFB(vfbdev, labicons[LABICON_SYSTEM], egi_sysfonts.bold, 16,16, WEGI_COLOR_WHITE, 0,0);

	/* Put icons from Right to Left */
//	egi_subimg_writeFB(battery_100_charging, vfbdev, 0, WEGI_COLOR_PINK, SURF_MAXW-5-60 -40, 3);
	egi_subimg_writeFB(pinyin_icon, vfbdev, 0, -1,  SURF_MAXW-5-60 -40, 4); //SURF_MAXW-5-60 -40-30-30-30, 4);

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* 6. Start Ering routine */
	printf("start ering routine...\n");
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

	/* 7. Activate image */
	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);

	/* Main loop */
	while( surfshmem->usersig != 1 ) {
		tm_delayms(50);

		/* 1. Update clock */
        	pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */
		labicon_time_update();
/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);

		/* 2. Update WIFI RSSI Level */
		wifi_level=iw_get_rssi("apcli0",NULL); //get_wifi_level();
		//printf("wifi_level=%d\n", wifi_level);
		if( wifi_reloading && wifi_level<0 ) {	/* WiFi interface is reloading */
			if( egi_clock_peekCostUsec(&eclock) > 300000 ) {
				wifi_icon_index ++;
				/* Loop wifi icon 0->4 */
				if(wifi_icon_index > 4)
					wifi_icon_index=0;
				egi_clock_restart(&eclock);
			}
		}
		else if( wifi_level<0 ) { /* Disconnected */
			wifi_reloading=false;
			/* icon index 5 as Disconnected */
			wifi_level=5;
			wifi_icon_index=5;
		}
		else if( wifi_level>=0 ) {
			wifi_reloading=false;
			wifi_icon_index=wifi_level;
		}

        	pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>> Surface shmem Critical Zone */

			/* Update LAB front_imagbuf */
			labicons[LABICON_WIFI]->front_imgbuf=wifi_icons[wifi_icon_index];
			egi_surfLab_writeFB(vfbdev, labicons[LABICON_WIFI], egi_sysfonts.bold, 16, 16, WEGI_COLOR_WHITE, 0, 0);

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);


		/* 3. Update PCM volume */
        	pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>> Surface shmem Critical Zone */
		egi_getset_pcm_volume(NULL, &vol_pval, NULL);
		volume_level=vol_pval/30;
		if(volume_level != volume_icon_index) {
			volume_icon_index=volume_level;
			/* Update LAB front_imagbuf */
			labicons[LABICON_SOUND]->front_imgbuf=volume_icons[volume_icon_index];
			egi_surfLab_writeFB(vfbdev, labicons[LABICON_SOUND], egi_sysfonts.bold, 16, 16, WEGI_COLOR_WHITE, 0, 0);
		}
/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);

		/* Wait pid */
		if( (wait_pid=waitpid(-1, &wait_status, WNOHANG)) >0 ) {
                        egi_dpstd("A child process PID=%d exits, wait_status=%d!\n", wait_pid, wait_status);
                }

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

	printf("Surfman_Guider: Exit OK!\n");
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
                                        10, 4,                             /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */
	}

#if 1   /* 5. Draw outline rim */
	//fbset_color(SURF_OUTLINE_COLOR);
	fbset_color2(vfbdev, WEGI_COLOR_BLACK);
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
        egi_surfLab_writeFB(vfbdev, labicons[LABICON_TIME], egi_sysfonts.regular, 16, 16, WEGI_COLOR_LTYELLOW, 0, 4);
}

/////////////////// Surface for Brightness Adjusting ///////////////////
/*-------------------------------------------
A SURF to show brightness value bar.
surfowner: NOT used!
--------------------------------------------*/
void surf_brightness(EGI_SURFSHMEM *surfowner, int x0, int y0)
{
	int msw=220;
	int msh=40;

	EGI_SURFUSER *msurfuser=NULL;
	EGI_SURFSHMEM *msurfshmem=NULL; /* Only a ref. to surfuser->surfshemem */
	//FBDEV	 *mvfbdev=NULL;	      /* Only a ref. to &surfuser->vfbdev */
	//EGI_IMGBUF *msurfimg=NULL;    /* Only a ref. to surfuser->imgbuf */
	SURF_COLOR_TYPE mcolorType=SURF_RGB565_A8; /* surfuser->vfbdev color type */
	//EGI_16BIT_COLOR  mbkgcolor;

	/* 1. Register/Create a surfuser */
	egi_dpstd("Register to create a surfuser...\n");
	msurfuser=egi_register_surfuser(ERING_PATH_SURFMAN, NULL, (320-msw)/2, (240-msh)/2, //surfowner->x0+x0, surfowner->y0+y0,
					 msw, msh, msw, msh, mcolorType); /* Fixed size */
	if(msurfuser==NULL) {
		egi_dpstd(DBG_RED"Fail to register surface!\n"DBG_RESET);
		return;
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	//mvfbdev=&msurfuser->vfbdev;
	//msurfuser->vfbdev.pixcolor_on=true;  //OK, default
	//msurfimg=msurfuser->imgbuf;
	msurfshmem=msurfuser->surfshmem;

	/* 3. Assgin OP function, to connect with CLOSE/MIN./MAX. buttons etc. */
	//msurfshmem->minimize_surface	= surfuser_minimize_surface; /* Surface module default function */
	//msurfshmem->redraw_surface	= surfuser_redraw_surface;
	//msurfshmem->maximize_surface	= surfuser_maximize_surface; /* Need resize */
	//msurfshmem->normalize_surface	= surfuser_normalize_surfac; /* Need resize */
	//msurfshmem->close_surface	= surfuser_close_surface;
	msurfshmem->draw_canvas	        = draw_canvas_brightness;
	msurfshmem->user_mouse_event  = surfbright_mouse_event;

	/* 4. Name for the surface */
	//strncpy(msurfshmem->surfname, "Help", SURFNAME_MAX-1);

	/* 5. First draw surface */
	msurfshmem->bkgcolor=WEGI_COLOR_GRAYA; /* OR default BLACK */
	/* First draw */
	surfuser_firstdraw_surface(msurfuser, TOPBAR_NONE|SURFFRAME_NONE, 0, NULL);  /* Default firstdraw operation */

	/* 6. Start Ering routine */
	egi_dpstd("Start ering routine...\n");
	if( pthread_create(&msurfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, msurfuser)!=0 ) {
		egi_dperr("Fail to launch thread EringRoutine!");
		if(egi_unregister_surfuser(&msurfuser)!=0)
			egi_dpstd("Fail to unregister surfuser!\n");
		return;
	}

	/* 7. Other jobs... */

	/* 8. Activate image */
	msurfshmem->sync=true;

	/* ===== Main Loop ===== */
	while(msurfshmem->usersig !=1 ) {
		usleep(100000);
	}

	/* P1. Join ering routine */
	//surfuser->surfshmem->usersig=1; // Useless if thread is busy calling a BLOCKING function.
	egi_dpstd("Cancel thread...\n");
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	/* Make sure mutex unlocked in pthread if any! */
	egi_dpstd("Join thread_eringRoutine...\n");
	if( pthread_join(msurfshmem->thread_eringRoutine, NULL)!=0)
		egi_dperr("Fail to join eringRoutine!\n");

	/* P2. Unregister and destroy surfuser */
	egi_dpstd("Unregister surfuser...\n");
	if( egi_unregister_surfuser(&msurfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	egi_dpstd("Surf_Brightness: Exit OK!\n");
}

/*--------------------------------------
Draw canvas, as frame_round_rect.
---------------------------------------*/
void draw_canvas_brightness(EGI_SURFUSER *msurfuser)
{
	int duty;
	int rad=6;
        egi_imgbuf_resetColorAlpha(msurfuser->imgbuf, WEGI_COLOR_DARKPURPLE, 120); //180);  /*  imgbuf, color, alpha */
        egi_imgbuf_setFrame(msurfuser->imgbuf, frame_round_rect, -1, 1, &rad);

	EGI_IMGBUF *icon=egi_imgbuf_readfile("/mmc/icons/brightness_24x24.png");
	egi_subimg_writeFB(icon, &msurfuser->vfbdev, 0, -1, 8, 8);

	/* Birghtness value bar */
	fbset_color2(&msurfuser->vfbdev, WEGI_COLOR_GRAY2);
	draw_wline_nc(&msurfuser->vfbdev, 40, 20, 40+160, 20, 11);

	fbset_color2(&msurfuser->vfbdev, WEGI_COLOR_WHITE);
	egi_getset_backlightPwmDuty(0, &duty, NULL, 0);
	draw_wline_nc(&msurfuser->vfbdev, 40, 20, 40+160*duty/100, 20, 11);
}


/*-----------------------------------------------------------------
                Mouse Event Callback
               (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event() .
------------------------------------------------------------------*/
void surfbright_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{

/* --------- E1. Parse Keyboard Event ---------- */

	/* Parse CONKEYs F1...F12 */
	if( pmostat->conkeys.press_F1 )
        	printf("Press F1\n");

        /* Lastkey MAY NOT cleared yet! Check press_lastkey!! */
        if(pmostat->conkeys.press_lastkey) {
                egi_dpstd("lastkey code: %d\n",pmostat->conkeys.lastkey);
                switch(pmostat->conkeys.lastkey) {
                        case KEY_ESC:
				surfuser->surfshmem->usersig=1;
                                break;
                        default:
                                break;
                }
        }
        /* NOTE: pmostat->chars[] is read from ssh STDIN !!! NOT APPLICABLE here for GamePad !!! */


/* --------- E2. Parse STDIN Input ---------- */
	if(pmostat->nch>0)
		egi_dpstd("chars: %-32.32s, nch=%d\n", pmostat->chars, pmostat->nch);

/* --------- E3. Parse Mouse Event ---------- */

	/* Adjust brightness */
	if( pmostat->mouseDZ ) {
		egi_dpstd("mouseDZ\n");

		int duty;
                egi_getset_backlightPwmDuty(0, NULL, NULL, -pmostat->mouseDZ*1);
        	egi_getset_backlightPwmDuty(0, &duty, NULL, 0);

        	/* Update birghtness bar */
        	pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>> Surface shmem Critical Zone */
	        fbset_color2(&surfuser->vfbdev, WEGI_COLOR_GRAY2);
        	draw_wline_nc(&surfuser->vfbdev, 40, 20, 40+160, 20, 11);
	        fbset_color2(&surfuser->vfbdev, WEGI_COLOR_WHITE);
	        draw_wline_nc(&surfuser->vfbdev, 40, 20, 40+160*duty/100, 20, 11);
/* ------ <<<  Surface shmem Critical Zone  */
        	pthread_mutex_unlock(&surfshmem->shmem_mutex);
	}
	/* Quit surf_brightness: right click on the surface area. */
	if( pmostat->RightKeyDown ) {
        	if( pxy_inbox( pmostat->mouseX, pmostat->mouseY, surfuser->surfshmem->x0, surfuser->surfshmem->y0,
                     surfuser->surfshmem->x0+surfuser->surfshmem->vw, surfuser->surfshmem->y0+surfuser->surfshmem->vh ) ) {
			/* Signal to quit */
			surfuser->surfshmem->usersig=SURFUSER_SIG_QUIT;
		}
	}
}

/////////////////// Surface for Date and Calender  ///////////////////

/*---------------------------------------------
A SURF to show brightness value bar.
surfowner: NOT used!

Note:
1. Calender content to be updated every 1 second.

-----------------------------------------------*/
void surf_calender(EGI_SURFUSER *surfowner)
{
	int msw=230;  /* 5+ 30*7 +5 */
	int msh=220;  /* 35 +30 +25*6 +5 */
	//int x0=0, y0=0;  /* Origin relative to surfowner */

	/* 0. Varaibles and Refes. */
	EGI_SURFUSER *msurfuser=NULL;
	EGI_SURFSHMEM *msurfshmem=NULL; /* Only a ref. to surfuser->surfshemem */
	//FBDEV	 *mvfbdev=NULL;	      /* Only a ref. to &surfuser->vfbdev */
	//EGI_IMGBUF *msurfimg=NULL;    /* Only a ref. to surfuser->imgbuf */
	SURF_COLOR_TYPE mcolorType=SURF_RGB565_A8; /* surfuser->vfbdev color type */
	//EGI_16BIT_COLOR  mbkgcolor;

	/* 1. Register/Create a surfuser */
	egi_dpstd("Register to create a surfuser...\n");
	msurfuser=egi_register_surfuser(ERING_PATH_SURFMAN, NULL, (320-msw)/2, (240-msh)/2, //surfowner->x0+x0, surfowner->y0+y0,
					 msw, msh, msw, msh, mcolorType); /* Fixed size */
	if(msurfuser==NULL) {
		egi_dpstd(DBG_RED"Fail to register surface!\n"DBG_RESET);
		return;
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	//mvfbdev=&msurfuser->vfbdev;
	//mvfbdev->pixcolor_on=true;  // OK, default
	//msurfimg=msurfuser->imgbuf;
	msurfshmem=msurfuser->surfshmem;

	/* 3. Assgin OP function, connect with CLOSE/MIN./MAX. buttons etc. */
	//msurfshmem->minimize_surface	= surfuser_minimize_surface; /* Surface module default function */
	//msurfshmem->redraw_surface	= surfuser_redraw_surface;
	//msurfshmem->maximize_surface	= surfuser_maximize_surface; /* Need resize */
	//msurfshmem->normalize_surface	= surfuser_normalize_surfac; /* Need resize */
	msurfshmem->close_surface	= surfuser_close_surface;
	msurfshmem->draw_canvas	        = calender_draw_canvas;
	//msurfshmem->user_mouse_event  = surfbright_mouse_event;

	/* 4. Name for the surface */
	strncpy(msurfshmem->surfname, "日历", SURFNAME_MAX-1);

	/* 5. First draw surface */
	msurfshmem->bkgcolor=WEGI_COLOR_GRAYA; /* OR default BLACK */
	/* First draw */
	surfuser_firstdraw_surface(msurfuser, TOPBTN_CLOSE|SURFFRAME_NONE, 0, NULL);  /* Default firstdraw operation */

	/* 6. Start Ering routine */
	egi_dpstd("Start ering routine...\n");
	if( pthread_create(&msurfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, msurfuser)!=0 ) {
		egi_dperr("Fail to launch thread EringRoutine!");
		if(egi_unregister_surfuser(&msurfuser)!=0)
			egi_dpstd("Fail to unregister surfuser!\n");
		return;
	}

	/* 7. Other jobs... */

	/* 8. Activate image */
	msurfshmem->sync=true;

	/* ===== Main Loop ===== */
	while(msurfshmem->usersig !=SURFUSER_SIG_QUIT ) {

		pthread_mutex_lock(&msurfshmem->shmem_mutex);
		//calender_draw_canvas(msurfuser);
 		calender_update(msurfuser);
		pthread_mutex_unlock(&msurfshmem->shmem_mutex);

		sleep(1);
	}

	/* P1. Join ering routine */
	//surfuser->surfshmem->usersig=1; // Useless if thread is busy calling a BLOCKING function.
	egi_dpstd("Cancel thread...\n");
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	/* Make sure mutex unlocked in pthread if any! */
	egi_dpstd("Join thread_eringRoutine...\n");
	if( pthread_join(msurfshmem->thread_eringRoutine, NULL)!=0)
		egi_dperr("Fail to join eringRoutine!\n");

	/* P2. Unregister and destroy surfuser */
	egi_dpstd("Unregister surfuser...\n");
	if( egi_unregister_surfuser(&msurfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	egi_dpstd("Surf_Brightness: Exit OK!\n");
}



/*--------------------------------------
Draw canvas, as frame_round_rect.
---------------------------------------*/
void calender_draw_canvas(EGI_SURFUSER *msurfuser)
{
	int rad=6;

        egi_imgbuf_resetColorAlpha(msurfuser->imgbuf, WEGI_COLOR_DARKPURPLE, 255);  /*  imgbuf, color, alpha */

	/* Draw Topbar */
	draw_filled_rect2(&msurfuser->vfbdev, COLOR_DimGray, 0,0, msurfuser->imgbuf->width-1, SURF_TOPBAR_HEIGHT-1);
	/* Make round corners */
        egi_imgbuf_setFrame(msurfuser->imgbuf, frame_round_rect, -1, 1, &rad); /* For SURF_RGB565_A8 */


//	EGI_IMGBUF *icon=egi_imgbuf_readfile("/mmc/icons/brightness_24x24.png");
//	egi_subimg_writeFB(icon, &msurfuser->vfbdev, 0, -1, 8, 8);

}
/*---------------------------------------
Display calender content
----------------------------------------*/
void calender_update(EGI_SURFUSER *msurfuser)
{
	//msw=230;  /* 5+ 30*7 +5 */
        //msh=220;  /* 35 +30 +25*6 +5 */

	int k;
	int pixlen;
	char strtmp[128];
	int monthdays; /* Total days in this month */
	int weekday;   /* Day of the week (0-6, Sunday = 0) */
	int firstweekday;  /* weekday of the 1st day this month */
	const char *strWeekday[]={"日", "一", "二", "三", "四", "五", "六"};  /* sunday =0 */

	/* Clear Topbar area, -30: DO NOT cover TOPBTN_CLOSE */
	draw_filled_rect2(&msurfuser->vfbdev, COLOR_DimGray, 0,0, msurfuser->imgbuf->width-1 -30, SURF_TOPBAR_HEIGHT-1);

	/* Clear calender back ground */
	draw_filled_rect2(&msurfuser->vfbdev, WEGI_COLOR_DARKPURPLE, 0,SURF_TOPBAR_HEIGHT,
							msurfuser->imgbuf->width-1, msurfuser->imgbuf->height-1);
	/* Make round corners */
	int rad=6;
        egi_imgbuf_setFrame(msurfuser->imgbuf, frame_round_rect, -1, 1, &rad); /* For SURF_RGB565_A8 */

	/* Get local time */
 	time_t ttm=time(NULL);
	struct tm *localTM;	/* Today, now */

	time_t ttm2;
	struct tm tmpTM;	/* First day (sunday) on the calender, usually day of the last month. */

	localTM=localtime(&ttm);  /* The return value points to a statically allocated struct  */

	/* Get days in this Month  */
	if( localTM->tm_mon +1 != 2 )
		monthdays=tm_MonthDays[localTM->tm_mon];  /* tm_mon: Month (0-11) */
	else
		monthdays=tm_daysInFeb(localTM->tm_year+1900);

	/* Get weekday of 1st day this month */
 	firstweekday=localTM->tm_wday-((localTM->tm_mday-1)%7);     /* tm_mday:  day of the month (1-31) */
	if(firstweekday<0)
		firstweekday +=7;

	/* Go back to the first day(sunday) on the calender, usually to be day of the last month. */
	ttm2=time(NULL);
	ttm2 -= (localTM->tm_mday+firstweekday-1)*24*3600;
	localtime_r(&ttm2, &tmpTM);

	/* Write date on top */
	sprintf(strtmp, "%d年%d月%d日 星期%s", localTM->tm_year+1900, localTM->tm_mon+1, localTM->tm_mday, strWeekday[localTM->tm_wday]);
        FTsymbol_uft8strings_writeFB( &msurfuser->vfbdev, egi_sysfonts.regular,        /* FBdev, fontface */
                                      16, 16, (const UFT8_PCHAR)strtmp, 	/* fw,fh, pstr */
                                      320, 1, 0,                        /* pixpl, lines, fgap */
                                      10, 5,                           /* x0,y0, */
                                      COLOR_White, -1, 240,             /* fontcolor, transcolor,opaque */
                                      NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */

	/* Write Weekday title: 日, 一, 二, 三, 四, 五, 六 */
	for(k=0; k<7; k++) {
	        pixlen=FTsymbol_uft8strings_pixlen( egi_sysfonts.regular, 18, 18, (const UFT8_PCHAR)strWeekday[k]);
	        FTsymbol_uft8strings_writeFB( &msurfuser->vfbdev, egi_sysfonts.regular,        /* FBdev, fontface */
                                      18, 18, (const UFT8_PCHAR)strWeekday[k], 		/* fw,fh, pstr */
                                      320, 1, 0,                        /* pixpl, lines, fgap */
                                      10+30*k+(30-pixlen)/2, 35,         /* x0,y0, */
                                      COLOR_Tomato, -1, 240,        /* fontcolor, transcolor,opaque */
                                      NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */
	}

	/* Draw pads for Saturday and Sunday */
	for(k=0; k<monthdays; k++) {
		weekday=(firstweekday+k)%7;
		if(weekday==0 || weekday==6 ) {
			draw_filled_rect2(&msurfuser->vfbdev, COLOR_DarkOliveGreen,  10+30*weekday, 35+30+25*((firstweekday+k)/7),
								    10+30*weekday +30-1, 35+30+25*((firstweekday+k)/7) +25-1);
		}
	}

	/* Display days of the LAST/PREV month remaining on the calender */
	if(firstweekday!=0) {

		for(k=0; k<firstweekday; k++) {
			weekday=k;
			sprintf(strtmp, "%d", tmpTM.tm_mday+k);
		        pixlen=FTsymbol_uft8strings_pixlen( egi_sysfonts.bold, 18, 18, (const UFT8_PCHAR)strtmp);
	                FTsymbol_uft8strings_writeFB( &msurfuser->vfbdev, egi_sysfonts.bold,        /* FBdev, fontface */
                                      18, 18, (const UFT8_PCHAR)strtmp,                 /* fw,fh, pstr */
                                      320, 1, 0,                          /* pixpl, lines, fgap */
                                      10+30*weekday+(30-pixlen)/2, 35+30, /* x0,y0, */
                                      COLOR_Gray, -1, 200,            /* fontcolor, transcolor,opaque */
                                      NULL, NULL, NULL, NULL);            /* int *cnt, int *lnleft, int* penx, int* peny */
		}
	}

	/* Draw a pad for today  30x25  */
	draw_filled_rect2(&msurfuser->vfbdev, COLOR_DeepPink, 10+30*localTM->tm_wday, 35+30+25*((firstweekday-1+localTM->tm_mday)/7),
					10+30*localTM->tm_wday +30-1, 35+30+25*((firstweekday-1+localTM->tm_mday)/7) +25-1 );

	/* Write Month days. 30x25 block for one day.  */
	for(k=0; k<monthdays; k++) {
		weekday=(firstweekday+k)%7;
		sprintf(strtmp, "%d", k+1);
	        pixlen=FTsymbol_uft8strings_pixlen( egi_sysfonts.bold, 18, 18, (const UFT8_PCHAR)strtmp);
	        FTsymbol_uft8strings_writeFB( &msurfuser->vfbdev, egi_sysfonts.bold,        /* FBdev, fontface */
                                      18, 18, (const UFT8_PCHAR)strtmp, 		/* fw,fh, pstr */
                                      320, 1, 0,                        /* pixpl, lines, fgap */
                                      10+30*weekday+(30-pixlen)/2, 35+30+25*((firstweekday+k)/7), /* x0,y0, */
                                      COLOR_Cornsilk, -1, 200,        /* fontcolor, transcolor,opaque */
                                      NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */
	}

	/* Write days of the NEXT month to fill up the calender space */
	//if( (firstweekday+monthdays)/7 <5 ) {
	if( firstweekday+monthdays < 6*7 ) {
		int j;  /* mday for NEXT month */
		weekday=(firstweekday+monthdays)%7; /* the first weekday for the NEXT month */
		//for(k=weekday,j=1; k<7; k++,j++) {
		for(k=firstweekday+monthdays, j=1; k<6*7; k++,j++) {
			sprintf(strtmp, "%d", j);
			pixlen=FTsymbol_uft8strings_pixlen( egi_sysfonts.bold, 18, 18, (const UFT8_PCHAR)strtmp);
		        FTsymbol_uft8strings_writeFB( &msurfuser->vfbdev, egi_sysfonts.bold,        /* FBdev, fontface */
        	                              18, 18, (const UFT8_PCHAR)strtmp, 		/* fw,fh, pstr */
                	                      320, 1, 0,                        /* pixpl, lines, fgap */
                        	              10+30*((weekday+j-1)%7)+(30-pixlen)/2, 35+30+25*(k/7), /* x0,y0, */
                                	      COLOR_Gray, -1, 200,        /* fontcolor, transcolor,opaque */
                                      	      NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */
		}
	}

}

/*------------------------------------------------------------------
                Mouse Event Callback
                (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event() .
--------------------------------------------------------------------*/
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	int ret;
	//printf("(%d,%d)\n", pmostat->mouseX, pmostat->mouseY);

#if 0	/* To hide/unhide surface, !!! see NOTE/Journal for its drawbacks. */
	if( pxy_inbox( pmostat->mouseX, pmostat->mouseY, surfshmem->x0, surfshmem->y0,
				 surfshmem->x0+surfshmem->vw, surfshmem->y0+surfshmem->vh ) == false ) {
		/* Hide the surface */
		surfshmem->sync=false;

		return;
	}
	else
		surfshmem->sync=true;
#endif

	/* If click */
	if( pmostat->LeftKeyDown ) {
		/* Click on WiFi LABICON */
        	if( egi_point_on_surfBox( (ESURF_BOX *)labicons[LABICON_WIFI],
                	                pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0) ) {

			/* TurnOFF apcli0 */
			if( iw_is_running("apcli0") ) {
				egi_dpstd("ifdown apcli0...\n");
				//iw_ifdown("apcli0"); /* This is NOT enough, netifd/ap_client will auto. reconnect!? */
			#if 0 /* 1: vfor() to carry ubus call */
			    #ifndef LETS_NOTE
				/* Fork to exe ubus_call */
				int pid;
				if( (pid=vfork())<0)
					egi_dpstd("Vfork error!\n");
				else if(pid==0) {
					ubus_cli_call(NULL, 2, arg_wifi_down);
					exit(0);
				}
				egi_dpstd("Ok ubus call wifi_down!\n");
			    #endif
			#else /* 2: Thread to carry ubus call */
				egi_dpstd("Start thread ubus call...\n");
				pthread_t thread_ucall;
				pthread_create(&thread_ucall, NULL, &thread_ubus_wifi_down, NULL);
				egi_dpstd("thread_ucall OK.\n");
			#endif
				//XX iwpriv("ra0", "ApCliEnable", "0");
				//XX system("wifi down");
			}
			/* TurnON apcli0 */
			else {
				egi_dpstd("ifup apcli0...\n");
				/* Cost time! This is NOT enough if you ubus_called to put it DOWN before! also ubus_call to turn it UP. */
				// iw_ifup("apcli0");
			#if 0 /* 1: vfor() to carry ubus call */
			    #ifndef LETS_NOTE
				/* Fork to exe ubus_call */
				int pid;
				if( (pid=vfork())<0)
					egi_dpstd("Vfork error!\n");
				else if(pid==0) {
					ubus_cli_call(NULL, 2, arg_wifi_up);
					exit(0);
				}
				egi_dpstd("Ok ubus call wifi_up!\n");
			    #endif
			#else /* 2: Thread to carry ubus call */
				egi_dpstd("Start thread ubus call...\n");
				pthread_t thread_ucall;
				pthread_create(&thread_ucall, NULL, &thread_ubus_wifi_up, NULL);
				egi_dpstd("thread_ucall OK.\n");
			#endif
				//XX iwpriv("ra0", "ApCliEnable", "1");
				//XX system("wifi up");
			}
		}
		else if ( egi_point_on_surfBox( (ESURF_BOX *)labicons[LABICON_TIME],
					pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0) ) {
			pthread_mutex_unlock(&surfshmem->shmem_mutex);
			surf_calender(surfuser);
			pthread_mutex_lock(&surfshmem->shmem_mutex);
		}
		else if( egi_point_on_surfBox( (ESURF_BOX *)labicons[LABICON_SYSTEM],
					pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0) ) {
			pthread_mutex_unlock(&surfshmem->shmem_mutex);
			ret=surf_menuListTree(surfuser);
			printf("Select menulist ID: %d\n", ret);
			switch(ret) {
			   case MENU_ID_BACKLIGHT:
				surf_brightness(surfshmem, 0,0 ); break;
			   case MENU_ID_CONFIG_SYSTEM:
				pthread_create(&thread_configSystem, NULL, surf_configSystem, NULL);
				//egi_crun_stdSurfInfo((UFT8_PCHAR)"System config", (UFT8_PCHAR)"TODO...", (320-200)/2, (240-100)/2, 200,100);
				break;
			   case MENU_ID_SOUND:
				egi_crun_stdSurfInfo((UFT8_PCHAR)"Sound", (UFT8_PCHAR)"TODO...", (320-200)/2, (240-100)/2, 200,100);
				break;
			   case MENU_ID_MONITOR_SYSTEM:
				//egi_crun_stdSurfInfo((UFT8_PCHAR)"System Monitor", (UFT8_PCHAR)"TODO...", (320-200)/2, (240-100)/2, 200,100);
				pthread_create(&thread_monitorSystem, NULL, surf_monitorSystem, NULL);
				break;
			   default: break;
			}
			pthread_mutex_lock(&surfshmem->shmem_mutex);
		}

	        #if 0	/* Click on left corner for Brightness Control */
		if( pxy_inbox( pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0,  // px,py, x1,y1, x2,y2
		    0, 0, 40, surfshmem->vh ) ) {
			pthread_mutex_unlock(&surfshmem->shmem_mutex);
			//menulist_ON=true;
			surf_brightness(surfshmem, 0,0 );
			pthread_mutex_lock(&surfshmem->shmem_mutex);
		}
                #endif
	}
	/* If Surface is downhold for moving */
	else if( guider_in_position && surfshmem->status == SURFACE_STATUS_DOWNHOLD ) {
		guider_in_position=false;
	}
	/* If LeftKeyUp, then readjust position: LefitKeyUp is a one hit event.. */
	else if( pmostat->LeftKeyUp || ( !guider_in_position && surfshmem->status != SURFACE_STATUS_DOWNHOLD )) {
		/* Locate GuideBar to TOP of the desk. */
		if( surfshmem->y0 < SURF_MAXH/2 ) {
			surfshmem->x0 = -5;
			surfshmem->y0 = -1;
		}
		/* Locate GuideBar to BOTTOM of the desk. */
		else {
			surfshmem->x0 = -5;
			surfshmem->y0 = SURF_MAXH-30;
		}

		guider_in_position=true;
	}

	/* If on ICON SOUND */
	if( egi_point_on_surfBox( (ESURF_BOX *)labicons[LABICON_SOUND],
				pmostat->mouseX-surfshmem->x0, pmostat->mouseY-surfshmem->y0) ) {
		if( pmostat->mouseDZ ) {
			//printf("DZ=%d\n", pmostat->mouseDZ);
			//vol_pval += -pmostat->mouseDZ*5;
			//egi_getset_pcm_volume(NULL, &vol_pval);
			egi_adjust_pcm_volume(NULL,-pmostat->mouseDZ*2);
		}
	}

#if 0	/* If on 'EGI_Desktop' MidasHK_2022_04_11 */
	static int duty=50;
	if( (pmostat->mouseX-surfshmem->x0 >0) && (pmostat->mouseX-surfshmem->x0 <60)
	   && (pmostat->mouseY-surfshmem->y0 >0) && (pmostat->mouseY-surfshmem->y0 < surfshmem->vh ) )
	{
		if( pmostat->mouseDZ ) {
			egi_getset_backlightPwmDuty(0, &duty, NULL, -pmostat->mouseDZ*2);
			duty += -pmostat->mouseDZ*2;
			if(duty>100)duty=100;
			else if(duty<0)duty=0;
			printf("PWM duty=%d\n", duty);
		}
	}
#endif
}


/*--------------- Ubus Client call host ------------
	Ubus Client call host
With reference to: ubus-2015-05-25/cli.c
Copyright (C) 2011 Felix Fietkau <nbd@openwrt.org>
GNU Lesser General Public License version 2.1
----------------------------------------------------*/
#ifndef LETS_NOTES

#include <libubox/blobmsg_json.h>
#include "libubus.h"
static bool simple_output=false;
static void receive_call_result_data(struct ubus_request *req, int type, struct blob_attr *msg)
{
        char *str;
        if (!msg)
                return;

        str = blobmsg_format_json_indent(msg, true, simple_output ? -1 : 0);
        egi_dpstd("%s\n", str);
        free(str);
}

int ubus_cli_call(const char *ubus_socket, int argc, const char **argv)
{
	struct ubus_context *ctx=NULL;
	struct blob_buf b={0};
        uint32_t id;
        int ret;
	int timeout=30;

        if (argc < 2 || argc > 3)
                return -2;

        ctx = ubus_connect(ubus_socket);
        if (!ctx) {
                if (!simple_output)
                        fprintf(stderr, "Failed to connect to ubus\n");
                return -1;
        }

        blob_buf_init(&b, 0);
        if (argc == 3 && !blobmsg_add_json_from_string(&b, argv[2])) {
                if (!simple_output)
                        fprintf(stderr, "Failed to parse message data\n");
                return -1;
        }

        ret = ubus_lookup_id(ctx, argv[0], &id);
        if (ret) {
		printf("ubus_lookup_id fails!\n");
                return ret;
	}

	printf("ubus_invoke...\n");
        return ubus_invoke(ctx, id, argv[1], b.head, receive_call_result_data, NULL, timeout * 1000);
}
#else  /* ----- For LETS_NOTE, No UBUS.  ---- */
int ubus_cli_call(const char *ubus_socket, int argc, const char **argv)
{

}

#endif

/* ------- Thread function --------- */
void *thread_ubus_wifi_down(void *arg)
{
	pthread_detach(pthread_self());

	iw_ifdown("apcli0"); /* This is NOT enough, netifd/ap_client will auto. reconnect!? */

	ubus_cli_call(NULL, 2, arg_wifi_down);

	return (void *)0;
}
void *thread_ubus_wifi_up(void *arg)
{
	pthread_detach(pthread_self());

        /* Cost time! This is NOT enough if you ubus_called to put it DOWN before! also ubus_call to turn it UP. */
	wifi_reloading=true; /* put it before iw_ifup() */
	egi_clock_stop(&eclock);
	egi_clock_start(&eclock);
        iw_ifup("apcli0");

	ubus_cli_call(NULL, 2, arg_wifi_up);

	return (void *)0;
}


/////////////////////  Surface for MenuListTree ////////////////////////////////


/*-------------------------------------------
A SURF to show brightness value bar.
surfowner: NOT used!

Return:
	<0  Fails
	=0  No item selected!
	>0  ID of selected menuListTree item.
--------------------------------------------*/
int surf_menuListTree(EGI_SURFUSER *surfowner)
{
	int ret=0;
	int msw=100;
	int msh=100;

	int menuListMode;

	//int x0=0, y0=0;  /* Origin relative to surfowner */
	ESURF_MENULIST *mlistMonitor;
	ESURF_MENULIST *mlistSetting;
	ESURF_MENULIST *mlistDP;
	ESURF_MENULIST *mlistSensors;
	ESURF_MENULIST *mlistNetstat;
	ESURF_MENULIST *mlistNetSetting;

	/* Menu Position Mode */
	if(surfowner->surfshmem->y0 < 50)
		menuListMode=MENULIST_ROOTMODE_LEFT_TOP;
	else
		menuListMode=MENULIST_ROOTMODE_LEFT_BOTTOM;

        /* P1. Create a MenuList Tree */
        /* P1.1 Create root mlist: ( mode, root, x0, y0, mw, mh, face, fw, fh, capacity) */
        menulist=egi_surfMenuList_create(menuListMode, true, 0, 0, /* At this point, layout is not clear. x0/y0 set at P3 */
                                                                  110, 30, egi_sysfonts.regular, 16, 16, 8 );
	mlistMonitor=egi_surfMenuList_create(menuListMode, false, 0,0, 110, 30, egi_sysfonts.bold, 16, 16, 8 );
	mlistSetting=egi_surfMenuList_create(menuListMode, false, 0,0, 110, 30, egi_sysfonts.bold, 16, 16, 8 );
	mlistDP=egi_surfMenuList_create(menuListMode, false, 0,0, 110, 30, egi_sysfonts.bold, 16, 16, 8 );
	if(menuListMode==MENULIST_ROOTMODE_LEFT_BOTTOM) {
		mlistSensors=egi_surfMenuList_create(menuListMode, false, 0, 30*3, 110, 30, egi_sysfonts.bold, 16, 16, 8 );
		mlistNetstat=egi_surfMenuList_create(menuListMode, false, 0, 30*2, 110, 30, egi_sysfonts.bold, 16, 16, 8 );
	} else {  /* menuListMode==MENULIST_ROOTMODE_LEFT_TOP */
		mlistSensors=egi_surfMenuList_create(menuListMode, false, 0, -30*3, 110, 30, egi_sysfonts.bold, 16, 16, 8 );
		mlistNetstat=egi_surfMenuList_create(menuListMode, false, 0, -30*2, 110, 30, egi_sysfonts.bold, 16, 16, 8 );
	}

	mlistNetSetting=egi_surfMenuList_create(menuListMode, false, 0,0, 110, 30, egi_sysfonts.bold, 16, 16, 8 );

	/* P2.0 Add itmes to mlistDP */
        egi_surfMenuList_addItem(mlistDP, "DGPS", NULL, NULL);
        egi_surfMenuList_addItem(mlistDP, "Wind", NULL, NULL);
        egi_surfMenuList_addItem(mlistDP, "Gyrocompass", NULL, NULL);
        egi_surfMenuList_addItem(mlistDP, "MRU", NULL, NULL);

        /* P2.1 Add items to mlistSensors */
        egi_surfMenuList_addItem(mlistSensors, "DP", mlistDP, NULL);
        egi_surfMenuList_addItem(mlistSensors, "Sensor1", NULL, NULL);
        egi_surfMenuList_addItem(mlistSensors, "Sensor2", NULL, NULL);
        egi_surfMenuList_addItem(mlistSensors, "Sensor3", NULL, NULL);

	/* P2.2 Add items to mlistNetstat */
	egi_surfMenuList_addItem(mlistSetting, "System", NULL, NULL); mlistSetting->mitems[0].id=MENU_ID_CONFIG_SYSTEM;
	egi_surfMenuList_addItem(mlistSetting, "Sound", NULL, NULL); mlistSetting->mitems[1].id=MENU_ID_SOUND;
	egi_surfMenuList_addItem(mlistSetting, "BackLight", NULL, NULL); mlistSetting->mitems[2].id=MENU_ID_BACKLIGHT;
	egi_surfMenuList_addItem(mlistSetting, "Power", NULL, NULL);
        /* P2.3 Add items to mlistMonitor */
        //egi_surfMenuList_addItem(mlistMonitor, "CPU Temp", NULL, NULL);
        //egi_surfMenuList_addItem(mlistMonitor, "CPU Load", NULL, NULL);
	egi_surfMenuList_addItem(mlistMonitor, "System", NULL, NULL);  mlistMonitor->mitems[0].id=MENU_ID_MONITOR_SYSTEM;
        egi_surfMenuList_addItem(mlistMonitor, "Netstat", mlistNetstat, NULL);
        egi_surfMenuList_addItem(mlistMonitor, "Sensors", mlistSensors, NULL);

	/* P3.1 Add items to mlistNetSetting */
	egi_surfMenuList_addItem(mlistNetSetting, "IP", NULL, NULL);
	egi_surfMenuList_addItem(mlistNetSetting, "WiFi", NULL, NULL);

	/* P3.2 Add items to mlistNetstat */
	egi_surfMenuList_addItem(mlistNetstat, "Setting", mlistNetSetting, NULL);
	egi_surfMenuList_addItem(mlistNetstat, "Interface", NULL, NULL);
	egi_surfMenuList_addItem(mlistNetstat, "Traffic", NULL, NULL);

        /* P4. Add items to menulist */
        egi_surfMenuList_addItem(menulist, "设置", mlistSetting, NULL);
        egi_surfMenuList_addItem(menulist, "Monitor", mlistMonitor, NULL);
        egi_surfMenuList_addItem(menulist, "Preference", NULL, NULL);

	/* P2. Get MAX layout size */
	egi_dpstd("Get layout size...\n");
	if( egi_surfMenuList_getMaxLayoutSize(menulist, &msw, &msh)<0 )
		egi_dpstd(DBG_RED"Fail to get layout size!\n"DBG_RESET);
        egi_dpstd("Max layout size: W%dxH%d\n", msw, msh);

	/* P3. Set MenuList Origin X0,Y0 according to layout size. x0y0 is under msurfusr imgbuf COORD  */
	if(menuListMode==MENULIST_ROOTMODE_LEFT_BOTTOM) {
		menulist->x0=0;
		menulist->y0=msh-2;
	}
	else  { /* MENULIST_ROOTMODE_LEFT_TOP */
		menulist->x0=0;
		menulist->y0=2;
	}

	/* 0. Varaibles and Refes. */
	EGI_SURFUSER *msurfuser=NULL;
	EGI_SURFSHMEM *msurfshmem=NULL; /* Only a ref. to surfuser->surfshemem */
	FBDEV	 *mvfbdev=NULL;	      /* Only a ref. to &surfuser->vfbdev */
	//EGI_IMGBUF *msurfimg=NULL;    /* Only a ref. to surfuser->imgbuf */
	SURF_COLOR_TYPE mcolorType=SURF_RGB565_A8; /* surfuser->vfbdev color type */
	//EGI_16BIT_COLOR  mbkgcolor;

	/* 1. Register/Create a surfuser */
	egi_dpstd("Register to create a surfuser...\n");
	msurfuser=egi_register_surfuser(ERING_PATH_SURFMAN, NULL, (320-msw)/2, (240-msh)/2, /* To adjust x0y0 at 5.Firstdraw */
					 msw, msh, msw, msh, mcolorType); /* Fixed size */
	if(msurfuser==NULL) {
		egi_dpstd(DBG_RED"Fail to register surface!\n"DBG_RESET);
		return -1;
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	mvfbdev=&msurfuser->vfbdev;
	//mvfbdev->pixcolor_on=true;  // OK, default
	//msurfimg=msurfuser->imgbuf;
	msurfshmem=msurfuser->surfshmem;

	/* 3. Assgin OP function, connect with CLOSE/MIN./MAX. buttons etc. */
	//msurfshmem->minimize_surface	= surfuser_minimize_surface; /* Surface module default function */
	//msurfshmem->redraw_surface	= surfuser_redraw_surface;
	//msurfshmem->maximize_surface	= surfuser_maximize_surface; /* Need resize */
	//msurfshmem->normalize_surface	= surfuser_normalize_surfac; /* Need resize */
	msurfshmem->close_surface	= surfuser_close_surface;

//	msurfshmem->draw_canvas	        = mlistTree_draw_canvas;
	msurfshmem->user_mouse_event  = surfmlist_mouse_event;

	/* 4. Name for the surface */
	strncpy(msurfshmem->surfname, "MenuTree", SURFNAME_MAX-1);

	/* 5. First draw surface */
	//msurfshmem->bkgcolor=WEGI_COLOR_GRAYA; /* OR default BLACK */
	/* First draw, SURFFRAME_NONE */
	//surfuser_firstdraw_surface(msurfuser, TOPBAR_NONE, 0, NULL);  /* Default firstdraw operation */

	/* Adjust menulist SURFACE relating to surfowner */
	if(menuListMode==MENULIST_ROOTMODE_LEFT_BOTTOM) {
		msurfshmem->x0 = surfowner->surfshmem->x0 + 120;
		msurfshmem->y0 = surfowner->surfshmem->y0 - msh-2; /* upward */
	} else {  /* MENULIST_ROOTMODE_LEFT_TOP */
		msurfshmem->x0 = surfowner->surfshmem->x0 + 120;
		msurfshmem->y0 = surfowner->surfshmem->y0 + surfowner->surfshmem->vh+2; /* downward */
	}
	egi_surfMenuList_writeFB(mvfbdev, menulist, 0,0, 0); /* fbdev, mlist, offx, offy,  select_idx */

	/* 6. Start Ering routine */
	egi_dpstd("Start ering routine...\n");
	if( pthread_create(&msurfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, msurfuser)!=0 ) {
		egi_dperr("Fail to launch thread EringRoutine!");
		if(egi_unregister_surfuser(&msurfuser)!=0)
			egi_dpstd("Fail to unregister surfuser!\n");
		return -1;
	}

	/* 7. Other jobs... */

	/* 8. Activate image */
	msurfshmem->sync=true;

	/* ===== Main Loop ===== */
	while(msurfshmem->usersig !=SURFUSER_SIG_QUIT ) {

		usleep(100000);
	}

	/* P1. Join ering routine */
	//surfuser->surfshmem->usersig=1; // Useless if thread is busy calling a BLOCKING function.
	egi_dpstd("Cancel thread...\n");
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	/* Make sure mutex unlocked in pthread if any! */
	egi_dpstd("Join thread_eringRoutine...\n");
	if( pthread_join(msurfshmem->thread_eringRoutine, NULL)!=0)
		egi_dperr("Fail to join eringRoutine!\n");

	/* P1a. Get retval */
	ret=msurfuser->retval;

	/* P2. Unregister and destroy surfuser */
	egi_dpstd("Unregister surfuser...\n");
	if( egi_unregister_surfuser(&msurfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	/* P3. Free MenuList Tree */
	egi_surfMenuList_free(&menulist);

	egi_dpstd("Surf_menuListTree: Exit OK!\n");

	return ret;
}


/*-----------------------------------------------------------------
                Mouse Event Callback
               (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event().
------------------------------------------------------------------*/
void surfmlist_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
        //FBDEV *mvfbdev=&surfuser->vfbdev;
	EGI_IMGBUF *msurfimg=surfuser->imgbuf;
        EGI_SURFSHMEM *msurfshmem=surfuser->surfshmem;

/* --------- E1. Parse Keyboard Event ---------- */
/* --------- E2. Parse STDIN Input ---------- */
/* --------- E3. Parse Mouse Event ---------- */

	/* Bypass SURFUSER_SIG_QUIT */
	if(surfuser->surfshmem->usersig==SURFUSER_SIG_QUIT) {
		return;
	}

	/* To quit surface */
	//if( pmostat->LeftKeyDown || pmostat->RightKeyDown) {
	if( pmostat->RightKeyDown) {
		/* Signal to quit */
                surfuser->surfshmem->usersig=SURFUSER_SIG_QUIT;
	}
	/* Click menu item. */
	else if(pmostat->LeftKeyDown) {
		EGI_8BIT_ALPHA alpha;
                //bool inbox=pxy_inbox( pmostat->mouseX, pmostat->mouseY, surfuser->surfshmem->x0, surfuser->surfshmem->y0,
                //     surfuser->surfshmem->x0+surfuser->surfshmem->vw, surfuser->surfshmem->y0+surfuser->surfshmem->vh ) );

		/* C1. If click out of surface area. (imgbuf, px,py, *color, *alpha)  */
		if( egi_imgbuf_getColor( msurfimg, pmostat->mouseX-surfuser->surfshmem->x0,
 					 pmostat->mouseY-surfuser->surfshmem->y0, NULL, &alpha) <0 ) {
                        surfuser->surfshmem->usersig=SURFUSER_SIG_QUIT;
		}
	#if 1	/* C2. OR click on blank area of the menulist <----- To lock holddown movement. */
		else if( alpha==0 )
                        surfuser->surfshmem->usersig=SURFUSER_SIG_QUIT;
	#endif

		/* C3. OK, click on menlist item! */
		else {
		   /* Update selection path */
                   egi_surfMenuList_updatePath(menulist, pmostat->mouseX-msurfshmem->x0, pmostat->mouseY-msurfshmem->y0);

/* TEST: -------- */
		   if(menulist->path[menulist->depth]->fidx >=0) {
			printf("MenuList: Click on '%s' with ID=%d\n",
					menulist->path[menulist->depth]->mitems[menulist->path[menulist->depth]->fidx].descript,
					menulist->path[menulist->depth]->mitems[menulist->path[menulist->depth]->fidx].id
					);
		   }

		   /* Get selected item, the end node item. */
		   surfuser->retval=egi_surfMenuList_getItemID(menulist);

		   /* Signal to quit, NO selection, NO quit: on the subMenu Root etc. */
		   if(surfuser->retval>0)
			surfuser->surfshmem->usersig=SURFUSER_SIG_QUIT;
		}

	}
	/* To view/expand/display menu list  */
	else if( pmostat->mouseDX || pmostat->mouseDY ) {
                egi_surfMenuList_updatePath(menulist, pmostat->mouseX-msurfshmem->x0, pmostat->mouseY-msurfshmem->y0);
		/* Clear msurfimg  */
		egi_imgbuf_resetColorAlpha(msurfimg, -1, 0);  /*  imgbuf, color, alpha */
		/* Present Menulist tree */
	 	egi_surfMenuList_writeFB(&surfuser->vfbdev, menulist, 0,0, 0); //msurfshmem->vh, 0); /* fbdev, mlist, offx, offy, select_idx */
	}
}



////////////////////  Surface Config_System: a thread fucntion //////////////////
void* surf_configSystem(void *argv)
{
	egi_crun_stdSurfInfo((UFT8_PCHAR)"System config", (UFT8_PCHAR)"TODO...", (320-200)/2, (240-100)/2, 200,100);

	return (void *)0;
}

////////////////////  Surface Monitor_System: a thread fucntion //////////////////
void* surf_monitorSystem(void *argv)
{
	const char *mpid_lock_file="/var/run/surfman_guider_monitorSystem.pid";

	int msw=260;
	int msh=150;

	char strLoadInfo[128];
	char strMemInfo[256];
	int fd;
	struct sysinfo sinfo;
	EGI_16BIT_COLOR fontColor=COLOR_White;

	/* Detach thread */
	pthread_detach(pthread_self());

	/* 0. Define variables and Refs */
	EGI_SURFUSER  *msurfuser=NULL;
	EGI_SURFSHMEM *msurfshmem=NULL; /* Only a ref. to surfuser->surfshmem */
	FBDEV	      *mvfbdev=NULL;    /* Only a ref. to &surfuser->vfbdev */
        EGI_IMGBUF    *msurfimg=NULL;   /* Only a ref. to surfuser->imgbuf */
        SURF_COLOR_TYPE mcolorType=SURF_RGB565;   /* surfuser->vfbdev color type */
	//EGI_16BIT_COLOR mbkgcolor;

	/* 1. Register/Create a surfuser */
	egi_dpstd("Register to create a surfuser...\n");
	msurfuser=egi_register_surfuser(ERING_PATH_SURFMAN, mpid_lock_file, (320-msw)/2,(240-msh)/2, msw,msh,msw,msh, mcolorType);
	if(msurfuser==NULL) {
		egi_dpstd(DBG_RED"Fail to register surface!\n"DBG_RESET);
		return (void *)-1;
	}

	/* 2. Get Ref. pointers to vfbdev/surfimg/surfshmem */
	mvfbdev=&msurfuser->vfbdev;
	//mvfbdev->pixcolor_on=true; //OK, default
	msurfimg=msurfuser->imgbuf;
	msurfshmem=msurfuser->surfshmem;

	/* 3. Assign OP functions, to connect with CLOSE/MIN./MAX. buttons etc. */
	msurfshmem->minimize_surface  = surfuser_minimize_surface; /* Surface module default function */
	//msurfshmem->redraw_surface	= surfuser_redraw_surface;
	//msurfshmem->maximize_surface	= surfuser_maximize_surface; /* Need resize */
	//msurfshmem->normalize_surface	= surfuser_normalize_surfac; /* Need resize */
	msurfshmem->close_surface	= surfuser_close_surface;
	//msurfshmem->draw_canvas	= my_draw_canvas;
	//msurfshmem->user_mouse_event  = my_mouse_event;

	/* 4. Name for the surface */
	strncpy(msurfshmem->surfname, "System Monitor", SURFNAME_MAX-1);

	/* 5. First draw surface */
	/* 5.1 Set colors */
	msurfshmem->bkgcolor=COLOR_Black; //COLOR_Linen; /* OR defalt BLACK */
	//msurfshmem->topmenu_bkgcolor=xxx;
	//msurfshmem->topmenu_hltbkgcolor=xxx;
	/* 5.2 First draw */
	surfuser_firstdraw_surface(msurfuser, TOPBTN_CLOSE|TOPBTN_MIN, 0, NULL);  /* Default firstdraw operation */

	/* 6. Start Ering Routine  */
	egi_dpstd("Start ering routine...\n");
	if( pthread_create(&msurfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, msurfuser)!=0 ) {
		egi_dperr("Fail to launch thread EringRoutine!\n");
		if(egi_unregister_surfuser(&msurfuser)!=0)
			egi_dpstd("Fail to unregister surfuser!\n");
		return (void *)-1;
	}

	/* 7. Other jobs... */

	/* 8. Activate image */
	msurfshmem->sync=true;

	/* ===== Main Loop ===== */
	while(msurfshmem->usersig !=SURFUSER_SIG_QUIT ) {

		/* Read loadavg */
		memset(strLoadInfo,0,sizeof(strLoadInfo));
		fd=open("/proc/loadavg", O_RDONLY|O_CLOEXEC);
		strcpy(strLoadInfo,"Load average:\n");
       	   	if(read(fd,strLoadInfo+strlen(strLoadInfo), 35)>0) {
		       printf("loadavg: %s\n",strLoadInfo);
        	}
		close(fd);

		/* Read meminfo */
		memset(strMemInfo,0,sizeof(strMemInfo));
		if(sysinfo(&sinfo)==0) {
			sprintf(strMemInfo, "Memory Info:\n%luK total, %luK free,\n%luK shrd, %luK buff",
				sinfo.totalram/1024, sinfo.freeram/1024, sinfo.sharedram/1024, sinfo.bufferram/1024);
		}

		pthread_mutex_lock(&msurfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */

		/* Redraw surface. NOPE! This will clear TopButton effect. */
		//surfuser_redraw_surface(msurfuser, msurfshmem->vw, msurfshmem->vh);

		/* Assume rim line width =1, (imgbuf, color,alpha, px,py,h, w) */
		egi_imgbuf_blockResetColorAlpha(msurfimg, msurfshmem->bkgcolor, -1, 1, SURF_TOPBAR_HEIGHT,
						msurfshmem->vh-1-SURF_TOPBAR_HEIGHT, msurfshmem->vw-1-1);

		/* Write loadavg/meminfo */
                FTsymbol_uft8strings_writeFB(mvfbdev, egi_sysfonts.bold,        /* FBdev, fontface */
                                        18, 18, (const UFT8_PCHAR)strLoadInfo, /* fw,fh, pstr */
                                        msw-10, 2, 18/4,                   /* pixpl, lines, fgap */
                                        10, 30+5,                          /* x0,y0, */
                                        fontColor, -1, 255,         /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);           /* int *cnt, int *lnleft, int* penx, int* peny */

                FTsymbol_uft8strings_writeFB(mvfbdev, egi_sysfonts.bold,        /* FBdev, fontface */
                                        18, 18, (const UFT8_PCHAR)strMemInfo, /* fw,fh, pstr */
                                        msw-10, 6, 18/4,                   /* pixpl, lines, fgap */
                                        10, 30+5 +45,                      /* x0,y0, */
                                        fontColor, -1, 255,         /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);           /* int *cnt, int *lnleft, int* penx, int* peny */

/* ------ <<<  Surface shmem Critical Zone  */
		pthread_mutex_unlock(&msurfshmem->shmem_mutex);

		sleep(1);
	}

	/* P1. Join ering routine */
	//surfuser->surfshmem->usersig=SURFUSER_SIG_QUIT; //Useless if thread is busy calling a BLOCKING function.
	egi_dpstd("Cancel thread...\n");
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	/* Make sure mutex unlocked in pthread if any! */
	egi_dpstd("Join thread eringRoutine...\n");
	if(pthread_join(msurfshmem->thread_eringRoutine, NULL)!=0)
		egi_dperr("Fail to join eringRoutine\n");

	/* P2. Unregister and destroy surfuser */
	egi_dpstd("Unregister surfuser...\n");
	if(egi_unregister_surfuser(&msurfuser)!=0)
		egi_dpstd("Fail to unregister surfuser!\n");

	egi_dpstd("Surface exit OK!\n");

	return (void *)0;
}
