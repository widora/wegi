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

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>

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
        LABICON_MAX     =4      /* <--- Limit */
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

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons etc. */
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

	/* Create Labels */
        labicons[LABICON_TIME]=egi_surfLab_create(surfimg, SURF_MAXW-5-60, 1, SURF_MAXW-5-60, 1, 80, 30-2); /* img,xi,yi,x0,y0,w,h */
        egi_surfLab_updateText(labicons[LABICON_TIME], "00:00");
        egi_surfLab_writeFB(vfbdev, labicons[LABICON_TIME], egi_sysfonts.bold, 16, 16, WEGI_COLOR_LTYELLOW, 0, 4);

        labicons[LABICON_WIFI]=egi_surfLab_create(surfimg, SURF_MAXW-5-60 -40-30, 2, SURF_MAXW-5-60 -40-30, 2, 30, 30-2); /* img,xi,yi,x0,y0,w,h */
	labicons[LABICON_WIFI]->front_imgbuf=wifi_icons[0]; /* A ref. pointer */
	egi_surfLab_writeFB(vfbdev, labicons[LABICON_WIFI], egi_sysfonts.bold, 16, 16, WEGI_COLOR_WHITE, 0, 0);

        labicons[LABICON_SOUND]=egi_surfLab_create(surfimg, SURF_MAXW-5-60 -40-30-30, 2, SURF_MAXW-5-60 -40-30-30, 2, 30,30-2);/* img,xi,yi,x0,y0,w,h */
	labicons[LABICON_SOUND]->front_imgbuf=volume_icons[1];
	egi_surfLab_writeFB(vfbdev, labicons[LABICON_SOUND], egi_sysfonts.bold, 16, 16, WEGI_COLOR_WHITE, 0, 0);

	/* Put icons from Right to Left */
	egi_subimg_writeFB(battery_100_charging, vfbdev, 0, WEGI_COLOR_PINK, SURF_MAXW-5-60 -40, 3);
//	egi_subimg_writeFB(audio_volume_100, vfbdev, 0, WEGI_COLOR_WHITE, SURF_MAXW-5-60 -40-30-30, 2);
	egi_subimg_writeFB(pinyin_icon, vfbdev, 0, -1, SURF_MAXW-5-60 -40-30-30-30, 4);

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
		egi_getset_pcm_volume(&vol_pval, NULL);
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
                                        10, 4,                             /* x0,y0, */
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
        egi_surfLab_writeFB(vfbdev, labicons[LABICON_TIME], egi_sysfonts.regular, 16, 16, WEGI_COLOR_LTYELLOW, 0, 4);
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
			egi_adjust_pcm_volume(-pmostat->mouseDZ*2);
		}
	}

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

