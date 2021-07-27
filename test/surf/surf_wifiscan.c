/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple graphical Wifi Scanner.

Usage:
	./surf_wifiscan

Mouse RightClick to turn ON/OFF RCMenu, the graph will react after a
while.

Note:
1. Sometimes, 'aps' command results in several APs, and have to
   reboot OP. (Power supply?)

Journal
2021-04-01:
	1. Apply pixcolor_on=true for workfb.
	2. sysfont FOR SURFSYS, appfont for APP.
2021-04-03:
	1. Add TickBox in RightClickMenu.
2021-05-11:
        1. TEST: surfmsg_send()SURFMSG_REQUEST_REFRESH

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pty.h>
#include <sys/wait.h>
#include "egi_timer.h"
#include "egi_log.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"
#include "egi_cstring.h"
#include "egi_iwinfo.h"
#include "egi_debug.h"
#include "egi_surface.h"

/* PID lock file, to ensure only one running instatnce. */
const char *pid_lock_file="/var/run/surf_wifiscan.pid";

/* For SURFUSER */
EGI_SURFUSER     *surfuser=NULL;
EGI_SURFSHMEM    *surfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *vfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev,
					  * TODO: If defines a working FBDEV, then not necessary?
					  */
EGI_IMGBUF       *surfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  colorType=SURF_RGB565;  /* surfuser->vfbdev color type */
EGI_16BIT_COLOR  bkgcolor;

/* For working VFBDEV */
FBDEV            workfb;           /* Working Virt FBDEV. Sizeof work area in the surface. */
EGI_IMGBUF	 *workimg=NULL;

/* For RightClick Box */
/** NOTE:
 *	1. RCMenu is drawn directly to surface vfbdev, while wifi curves drawn to workfb first,
 *	   then copy to surface vfbdev.
 */
bool		RCMenu_ON;
ESURF_BOX	*RCBox;		/* RightClick Box */
ESURF_TICKBOX	*TickBox[3];	/* TickBox on RCBox. [0] CPU LOAD [1] WIFI SIGNAL [2] WIFI RATE */
bool		display_curve[3];
enum {
	CURVE_CPU_LOAD 		=0,
	CURVE_WIFI_SIGNAL 	=1,
	CURVE_WIFI_RATE		=2,
};

/* Ering Routine */
void  *surfuser_ering_routine(void *surfuser);

/* Surface OP functions */
void my_redraw_surface(EGI_SURFUSER *surfuser, int w, int h);
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

void print_help(const char* cmd)
{
        printf("Usage: %s [-h]\n", cmd);
        printf("        -h   Help \n");
}

void parse_apinfo(char *info, int index);
void draw_bkg(FBDEV *fbdev);
void FBwrite_total(FBDEV *fbdev, int total);
void FBwrite_tip(FBDEV *fbdev);
void draw_cpuload(FBDEV *fbdev);
void update_surfimg(void);

int firstdraw_RCBox(FBDEV *vfb);
void draw_RCBox(FBDEV *vfb, int cx0, int cy0);

int blpx=22;		/* Base line left point XY */
int blpy=(240-30)-8;
int fw=12;
int fh=12;

int cfg=5;	/* Central freq. gap: 5MHz */
int ppm=3;	/* Pixel per MHz */
//int pps=2;	/* Pixel per signal strength(percentage) 2*100 */
float pps=1.8;	/* Pixel per signal strength(percentage) 1.8*100 */


/*----------------------------
	   MAIN
-----------------------------*/
int main(int argc, char** argv)
{
	int opt;
	int i;

	int shpid;
	int retpid;
	int status;
	int ptyfd;
	int nread;
	char obuf[2048];
        size_t nbuf;     	/* bytes of data in obuf */
	char *pnl;	 	/* pointer to a newline */
	char linebuf[256]; 	/* As we already know abt. the size */
	int linesize;
	int index;
	char ptyname[256]={0};

	char *surfname=NULL;

        /* <<<<<  EGI general init  >>>>>> */
#if 0
        printf("tm_start_egitick()...\n");
        tm_start_egitick();		   	/* start sys tick */
#endif
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_wifiscan") != 0) {	/* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }

#if 0
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {   	/* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
#endif


	/* Load freetype fonts */
  	printf("FTsymbol_load_sysfonts()...\n");
  	if(FTsymbol_load_sysfonts() !=0) {
        	printf("Fail to load FT sysfonts, quit.\n");
        	return -1;
  	}
  	printf("FTsymbol_load_appfonts()...\n");
        if(FTsymbol_load_appfonts() !=0 ) { /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

  	//FTsymbol_set_SpaceWidth(1);

  	/* Start touch read thread */
#if 0
  	printf("Start touchread thread...\n");
  	if(egi_start_touchread() !=0)
        	return -1;
#endif


 /* <<<<<  End of EGI general init  >>>>>> */

        /* Parse input option */
        while( (opt=getopt(argc,argv,"h"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
                                break;
		}
	}
        if( optind < argc ) {
	}

	/* 0. Create working virt. FBDEV. Side 1 pix frame. */
	if( init_virt_fbdev2(&workfb, 320-2, 240-SURF_TOPBAR_HEIGHT-1, -1, 0) ) {  /* vfb, xres,yres, alpha, color */
		printf("Fail to initialize workfb!\n");
		exit(EXIT_FAILURE);
	}
	workfb.pixcolor_on=true; /* workfb apply FBDEV pixcolor */

        /* 1. Register/Create a surfuser */
        printf("Register to create a surfuser...\n");
        surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, pid_lock_file, 0, 0, 320, 240,320, 240, colorType );
        if(surfuser==NULL) {
                exit(EXIT_FAILURE);
        }

        /* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
        vfbdev=&surfuser->vfbdev;
        surfimg=surfuser->imgbuf;
        surfshmem=surfuser->surfshmem;

        /* Get surface mutex_lock */
//        pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons. */
	// Defualt: surfuser_ering_routine() calls surfuser_parse_mouse_event();
        surfshmem->minimize_surface     = surfuser_minimize_surface;    /* Surface module default functions */
        surfshmem->redraw_surface       = my_redraw_surface; 	//surfuser_redraw_surface;
        surfshmem->maximize_surface     = surfuser_maximize_surface;    /* Need redraw */
        surfshmem->normalize_surface    = surfuser_normalize_surface;   /* Need redraw */
        surfshmem->close_surface        = surfuser_close_surface;
	surfshmem->user_mouse_event	= my_mouse_event;

        /* 4. Give a name for the surface. */
        surfname="WiFi扫描仪";
        if(surfname)
                strncpy(surfshmem->surfname, surfname, SURFNAME_MAX-1);
        else
                surfname="EGI_SURF";

	/* Create RCBox and TickBoxes on it */
	firstdraw_RCBox(&workfb);
	TickBox[CURVE_CPU_LOAD]->ticked=false;
	TickBox[CURVE_WIFI_SIGNAL]->ticked=false;
	TickBox[CURVE_WIFI_RATE]->ticked=false;

        /* 5. First draw surface */
        //surfshmem->bkgcolor= WEGI_COLOR_DARKGRAY1; /* OR default BLACK */
        surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MIN|TOPBTN_MAX, 0, NULL); /* Default firstdraw operation */

	/* Create displaying window */
	draw_bkg(&workfb);
	FBwrite_tip(&workfb);

	/* Update surfuser IMGBUF, by copying workfb IMGBUF to surfuser->imgbuf */
	update_surfimg();

        /* Test EGI_SURFACE */
        printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
        printf("Shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
                        surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

        /* 6. Start Ering routine */
        if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
                printf("Fail to launch thread_EringRoutine!\n");
                exit(EXIT_FAILURE);
        }

        /* 7. Activate image */
        surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
//                pthread_mutex_unlock(&surfshmem->shmem_mutex);


/* ------ LOOP ------ */
while( surfshmem->usersig != 1 ) {

	EGI_PLOG(LOGLV_TEST,"Start to forkpty...");

	index=0;

	/* W1. Draw BKG */
	draw_bkg(&workfb);

	/* W2. Draw CPULOAD */
	if(display_curve[CURVE_CPU_LOAD])
		draw_cpuload(&workfb);

	if( !display_curve[CURVE_WIFI_SIGNAL]) {
		sleep(1);
		//tm_delayms(100);
		//usleep(20000);
		goto END_APSCAN;
	}

	/* W3. Create a pty shell to execute aps */
	shpid=forkpty(&ptyfd, ptyname, NULL, NULL);
        /* W4: Child process to execute shell command */
	if( shpid==0) {
		/* Widora aps RETURN Examples:
		 *  ( -- Ch  SSID              BSSID               Security               Signal(%)W-Mode  ExtCH  NT WPS DPID -- )
		 *	12  Chinawwwwww      55:55:55:55:55:55   WPA1PSKWPA2PSK/AES     81       11b/g/n BELOW  In YES
		 *	11  ChinaNet-678     66:66:66:66:66:66   WPA2PSK/TKIPAES        65       11b/g/n NONE   In  NO
		 *	7   CMCC-RED         77:77:77:77:77:77   WPA1PSKWPA2PSK/TKIPAES 55       11b/g/n NONE   In  NO

		 * Note: You MUST login as root, OR iwpriv_set_SiteSurvey will FAIL!
		 *	midas@Widora: iwpriv ra0 set SiteSurvey=0
		 *	Interface doesn't accept private ioctl...
		 *	set (8BE2): Operation not permitted
		 */
                execl("/bin/sh", "sh", "-c", "aps | grep 11b/g/n", (char *)0);

		/* On success, execve() does not return! */
		EGI_PLOG(LOGLV_CRITICAL, "execl() fail!");
		exit(0);
        }
	else if( shpid <0 ){
		EGI_PLOG(LOGLV_CRITICAL, "Fail to forkpty! Err'%s'.  retry later...", strerror(errno));
		sleep(3);
		continue;
	}

	printf("ptyname: %s\n",ptyname);
	EGI_PLOG(LOGLV_TEST,"Start to read aps results and parse data...");


	/* W5: Parent process to get execl() result */
        bzero(obuf,sizeof(obuf));
        nbuf=0;
        do {
		/* Check remaining space of obuf[] */
                if(0==sizeof(obuf)-nbuf-1) {
	        	/* TODO: memgrow obuf */
                	EGI_PLOG(LOGLV_TEST,"Buffer NOT enough! nbuf=%zd!", nbuf);
			break;
		}
                else
                        printf("nbuf=%zd \n", nbuf);

		/* Continue to read output from ptyfd */
                nread=read(ptyfd, obuf+nbuf, sizeof(obuf)-nbuf-1);
                printf("nread=%zd\n",nread);
                if(nread>0) {
                	nbuf += nread;
			obuf[nbuf]='\0';
                }
                else if(nread==0) {
                	printf("nread=0...\n");
                }
                else  { /* nread<0 */
			/* Maybe ptyfd already read out, and ptyfd is unavailable.
			 * However data in obuf[] has NOT been completely parsed yet.
			 */
                }
                printf("Remain obuf[]: \n%s\n",obuf);

		/* Extract data from obuf[] line by line. */
                pnl=strstr(obuf,"\n");  /* Check if there is a NL in obuf[]. */
                if( pnl!=NULL || nread<0 ) {  /* If nread<0, get remaining data, even withou NL. */
                	/* Copy to linebuf, TODO: memgrow linebuf */
                        if( pnl==NULL && nread<0 ) /* shell pid ends, and remains in obuf without '\n'! */
                        	linesize=strlen(obuf);
                        else  /* A line with '\n' at end. */
                        	linesize=pnl+1-obuf;

                        /* Limit linesize */
                        if(linesize > sizeof(linebuf)-1 ) {
                        	printf("linebuf is NOT enough!, current linesize=%zd\n",linesize);
	                        linesize=sizeof(linebuf)-1;  /* linesize LIMIT */
                        }

			/* Copy the line into obuf[], with '\n'. */
                      	strncpy(linebuf, obuf, linesize);
                        linebuf[linesize]='\0';

                        /* Move copied line out from obuf[], and update nbuf. */
                        memmove(obuf, obuf+linesize, nbuf-linesize+1); /* +1 EOF */
                        nbuf -= linesize;
                        obuf[nbuf]='\0';

			/* Parse result line. parse and draw wifi signal curves. */
			printf("Result: '%s'", linebuf);
			parse_apinfo(linebuf, index++);

		}
		else if( pnl==NULL && nread>0 ) {
			
		}

	} while( nread>=0 || nbuf>0 );


	/* Put total number of APs */
	FBwrite_total(&workfb, index);

END_APSCAN:

	pthread_mutex_lock(&workfb.VFrameImg->img_mutex);
/* ------ >>>  workfb.virt_fb Critical Zone  */

	/* Render VBF. Save to VFrameImg */
	vfb_render(&workfb);

	pthread_mutex_unlock(&workfb.VFrameImg->img_mutex);;
/* ------ <<<  workfb.virt_fb Critical Zone  */

	/* Update surfuser IMGBUF, by copying workfb IMGBUF to surfuser->imgbuf */
	update_surfimg();  /* Also update RCMenu */

	/* TEST-----:  MSG request for SURFMAN to render/refresh.  */
        surfmsg_send(surfuser->msgid, SURFMSG_REQUEST_REFRESH, NULL, IPC_NOWAIT);

	/* Close and release ptyfd */
	if( display_curve[CURVE_WIFI_SIGNAL] ) {
		close(ptyfd);
		/* waitpid */
		retpid=waitpid(shpid, &status, 0);
		EGI_PLOG(LOGLV_CRITICAL, "waitpid get retpid=%d", retpid);
		if(WIFEXITED(status))
			EGI_PLOG(LOGLV_CRITICAL, "Child %d exited normally!", shpid);
		else
			EGI_PLOG(LOGLV_CRITICAL, "Child %d is abnormal!", shpid);
	}

} /* END LOOP */

END_PROG:

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
        if( egi_unregister_surfuser(&surfuser)!=0 )
                egi_dpstd("Fail to unregister surfuser!\n");

        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
        printf("egi_quit_log()...\n");
        egi_quit_log();
        printf("<-------  END  ------>\n");

return 0;
}


/*------------------------------------
    SURFUSER's ERING routine thread.
------------------------------------*/
void *surfuser_ering_routine(void *args)
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
		    #if LOOP_TEST
			return (void *)-1;
		    #else
			egi_dperr("ering_msg_recv() nrecv==0!");
			exit(EXIT_FAILURE);
		    #endif
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
				if(mouse_status->RightKeyDownHold)
                			printf(" -- RightKeyDownHold!\n");
				//egi_dpstd("MS(X,Y):%d,%d\n", mouse_status->mouseX, mouse_status->mouseY);
				/* Parse mouse event */
				surfuser_parse_mouse_event(surfuser,mouse_status);  /* mutex_lock */
				/* Always reset MEVENT after parsing, to let SURFMAN continue to ering mouse event */
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

/*-----------------------------------------------
Parse AP information, draw signal strength curve.

Ch  SSID     BSSID               Security               Signal(%)W-Mode  ExtCH  NT WPS DPID
------------------------------------------------------------------------------------------
12  Chinaw   55:55:55:55:55:55   WPA1PSKWPA2PSK/AES     81       11b/g/n BELOW  In YES

-----------------------------------------------*/
void parse_apinfo(char *info, int index)
{
	UFT8_CHAR ssid[256]={0}; /* SSID */
	int ch;		/* Channel */
	int signal; 	/* In percent */
	int bw;		/* HT bandwidth, Mode HT20, HT40+/- */
	char *ps;
	int k;
	char *delim=" "; /* SPACE as delimer */
	int px_cf;	/* Cetral Freq. Mark coordinate X */
	int py_cf;	/* Cetral Freq. Mark coordinate Y */
	EGI_POINT pts[3];

	/* The first 3 words[] of info[]: Channel, SSID, BSSID */
	int len;
	char *pt=NULL, *pt2=NULL;
	char info_CHANNEL[16];
	char info_SSID[512];
	char info_copy[512]; /* Copy of info. */

	printf("parse_apinfo...\n");

	/* Copy info, for anonymouse SSID */
	strncpy(info_copy,info, sizeof(info_copy)-1);

	/* sss */
	if(index>30)
		EGI_PLOG(LOGLV_CRITICAL, "Info[%d]: %s", index, info);

START_PARSE:
	/* Defaule mode HT20 */
	bw=20;

        /* Get parameters from string info. */
        ps=strtok(info, delim);
        for(k=0; ps!=NULL; k++) {
		switch(k) {
			case 0:	/* Channel */
				strncpy(info_CHANNEL, ps, sizeof(info_CHANNEL)-1);
				ch=atoi(ps);
				break;
			case 1: /* SSID,  (BSSID if anonymous AP) */
				strncpy(info_SSID, ps, sizeof(info_SSID)-2); /* -1 for BLANK, -1 for EOF */
				strncpy((char *)ssid, ps, sizeof(ssid)-1);
				break;
			case 2: /* BSSID */
				break;
			case 3: /* Security */
				break;
			case 4: /* Signal strength in percentage */
				signal=atoi(ps);
				printf("Signal=%d\n", signal);
				break;
			case 5: /* W-Mode */
				break;
			case 6: /* ExtCH, HT40+ OR HT40-  */
				if(strstr(ps,"ABOVE") || strstr(ps,"BELOW"))
					bw=40;
				break;
			case 7: /* NT */
				break;
			case 8: /* WPS */
				break;
		}
		ps=strtok(NULL, delim);
	}

	/*** Assume with anonymous SSID:
	 *	Ch  SSID     BSSID               Security            Signal(%)  W-Mode  ExtCH  NT WPS DPID
	 *	---------------------------------------------------------------------------------------
	 *	12           55:55:55:55:55:55   WPA1PSKWPA2PSK/AES  81         11b/g/n BELOW  In YES
	 */
	if(k==9) {
		egi_dpstd("AP with anonymouse SSID\n");
		/* NOW ino_SSID[] is acutally stored with BSSID */

		/* Check space */
		len=strlen(info_SSID);
		if( strlen(info)+len+1 +1 > sizeof(info_copy) ) {
			egi_dpstd("NOT enough space in info_copy[]!\n");
			return;
		}
		info_SSID[len]=' ';		/* Insert a BLANK */

		/* Make BSSID as SSID */
		pt=strstr(info_copy, " ") +1; 	/* pointer to content aft. first BLANK */
		pt2=cstr_trim_space(pt); 	/* skip/trim spaces */
		memmove(pt+len+1, pt2, strlen(pt2)); /* make space for inserting BSSID */
		memcpy(pt, info_SSID, len+1);	/* Insert BSSID at SSID position, +1 for a BLANK */
		egi_dpstd("----- Refined anonymouse AP info:\n %s\n", info_copy);

		/* Reset info */
		info=info_copy;
		goto START_PARSE;
	}
	/* Avoid uncomplete info, drop it. */
	else if(k<9) {  /* including LN token, 10 items in each info string. */
		egi_dpstd("Uncomplete info,  k=%d\n",k);
		return;
	}

	/* Draw channel */

	/* 256COLOR Select */
	EGI_16BIT_COLOR color;
	unsigned int color_codes[]={
	  9,10,11,12,13,14,15,36,39,100,105,144,148,155,158,169,197,201,202,205,207,208,213,214,219,220,227,230
	};
	int color_total=sizeof(color_codes)/sizeof(color_codes[0]);
	int code;
	code=(cstr_hash_string(ssid, 0)%1000)%color_total;
	printf("ssid: %s, color code=%d\n", ssid, code);
	#if 0
	color=egi_256color_code(color_codes[code]);
	#else
	color=egi_256color_code(color_codes[index%color_total]);
	#endif


	/* Cal. top point XY of sginal strength arc. */
	if(ch==14) {
		px_cf=blpx+10*ppm+(12*cfg+12)*ppm;
		py_cf=blpy-signal*pps-1;
	}
	else {
		px_cf=blpx+10*ppm+(ch-1)*cfg*ppm;
		py_cf=blpy-signal*pps-1;
	}

	/* Draw an arc to indicate bandwidth and signal strength */
	pts[0].x=px_cf-bw/2*ppm; 	pts[0].y=blpy;
	pts[1].x=px_cf;			pts[1].y=py_cf;
	pts[2].x=px_cf+bw/2*ppm;  	pts[2].y=blpy;

//	workfb.pixcolor_on=true; /* Set BEFORE fbset_color2 */
	fbset_color2(&workfb, color);

	workfb.antialias_on=true;
	draw_spline(&workfb, 3, pts, 2, 1);
	workfb.antialias_on=false;
//	workfb.pixcolor_on=false;

//	draw_filled_spline(&workfb, 3,pts, 2, 1, blpy, color, 50); /* baseY, color, alpha */

	/* Mark SSID */
	int pixlen=FTsymbol_uft8strings_pixlen(egi_appfonts.regular, fw, fh, ssid);
	if(pixlen<0)
		printf("Mark SSID: %s, pixlen=%d\n", ssid, pixlen);
        FTsymbol_uft8strings_writeFB(   &workfb, egi_appfonts.regular, 	/* FBdev, fontface */
                                        fw, fh,(const UFT8_PCHAR)ssid,  /* fw,fh, pstr */
                                        320, 1, 0,      		/* pixpl, lines, fgap */
                                        px_cf-pixlen/2, py_cf-fw-2,     /* x0,y0, */
                                        color, -1, 255,                 /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */
}


/*------------------------
Draw back ground on workfb.
-------------------------*/
void draw_bkg(FBDEV *fbdev)
{
	int i,j;
	char percent[8]={0};
	int freqcent;
	int sq;	/* Square side */

	printf("Draw_bkg\n");
        /* Prepare backgroup grids image for transparent image */
	sq=10*pps;
        for(i=0; i < fbdev->vinfo.yres/sq +1; i++) {
        	for(j=0; j < fbdev->vinfo.xres/sq +1; j++) {
                	if( ( i*(fbdev->vinfo.xres/sq)+j ) %2 )
                        	fbset_color2(fbdev, WEGI_COLOR_DARKGRAY);
                        else
                        	fbset_color2(fbdev, WEGI_COLOR_DARKGRAY1);
                        //draw_filled_rect(fbdev, j*sq, i*sq , (j+1)*sq-1, (i+1)*sq-1 );
                        draw_filled_rect(fbdev, j*sq, blpy-i*sq , (j+1)*sq-1, blpy-(i+1)*sq+1 );
                }
        }

	/* Draw lower banner */
	fbset_color2(fbdev, WEGI_COLOR_GRAY);
	draw_filled_rect(fbdev, 0,blpy+1, 320-1,240-1);

	/* Draw Channel Freq. Center Mark */
	fbset_color2(fbdev, WEGI_COLOR_BLACK);
	/* CHANNEL 1-13 */
	for(i=0; i<13; i++) {
		freqcent=blpx+(10+cfg*i)*ppm;
		draw_filled_rect(fbdev, freqcent -2, blpy+1, freqcent +2, blpy+10);
	}
	/* CHANNEL 14 */
	freqcent=blpx+(10+cfg*12+12)*ppm;
	draw_filled_rect(fbdev, freqcent -2, blpy+1,  freqcent +2, blpy+10);

	/* Draw signal percentage mark */
	printf("Draw signal\n");
	fbset_color2(fbdev, WEGI_COLOR_GRAY);
	for(i=0; i<11; i++) {
		sprintf(percent,"%d",10*i);
	        FTsymbol_uft8strings_writeFB(   fbdev, egi_appfonts.regular, /* FBdev, fontface */
        	                                fw, fh,(const UFT8_PCHAR)percent,    /* fw,fh, pstr */
                	                        320, 1, 0,      		 /* pixpl, lines, fgap */
                        	                320-25, blpy-fh-i*pps*10,        /* x0,y0, */
         	                      	        WEGI_COLOR_WHITE, -1, 180,       /* fontcolor, transcolor,opaque */
	                                       	NULL, NULL, NULL, NULL);      	 /* int *cnt, int *lnleft, int* penx, int* peny */
	}
}

/*-----------------------
Put total number of APs
------------------------*/
void FBwrite_total(FBDEV *fbdev, int total)
{
	char str[32];

	printf("Write total\n");
	sprintf(str,"(%d APs)",total);
        FTsymbol_uft8strings_writeFB(   fbdev, egi_appfonts.regular, /* FBdev, fontface */
       	                                15, 15,(const UFT8_PCHAR)str, 	  /* fw,fh, pstr */
               	                        320, 1, 0,      		  /* pixpl, lines, fgap */
                       	                230, 2,         		  /* x0,y0, */
       	                      	        WEGI_COLOR_WHITE, -1, 180,        /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      	  /* int *cnt, int *lnleft, int* penx, int* peny */
}

/*------------------------
 FBwrite tips.
-------------------------*/
void FBwrite_tip(FBDEV *fbdev)
{
	/* Put scanning tip... */
        FTsymbol_uft8strings_writeFB(   fbdev, egi_appfonts.regular, /* FBdev, fontface */
       	                                24, 24,(const UFT8_PCHAR)"Scanning...",	  /* fw,fh, pstr */
               	                        320, 1, 0,      		  /* pixpl, lines, fgap */
                       	                100, 100-15,         		  /* x0,y0, */
       	                      	        WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      	  /* int *cnt, int *lnleft, int* penx, int* peny */
}

/*----------------------------------
	Draw cpu load line.

Max of CPU avgload is 5, as 100% mark.
-----------------------------------*/
void draw_cpuload(FBDEV *fbdev)
{
	int i;
	static EGI_POINT pts[17]={0};
	int st=sizeof(pts)/sizeof(pts[0]);	/* Size of vload[] */
        float avgload[3]={0};
	int sq=10*pps;		/* %10 mark pixlen */

	printf("Draw_cpuload\n");

	/* Init pts[].y */
	if(pts[st-1].x==0) {
		for(i=0; i<st; i++)
			pts[i].y=blpy;
	}

        /* Get average load */
        if( iw_read_cpuload( avgload )!=0 ) {
        	printf("Fail to read avgload!\n");
        }
        else {
        	/* Shift vload/pts values */
                memmove( pts, pts+1, (st-1)*sizeof(EGI_POINT));

                /* Update pts[st-1].Y as new vload */
                pts[st-1].y=blpy-avgload[0]*(10*sq)/5.0;   /* Set Max of avg is 5, as 100%, pixel 10*20 */
		if( pts[st-1].y < (blpy-10*sq) )
			pts[st-1].y=blpy-10*sq;  /* Limit */
        }

	/* Re_assign  pts.X */
	for(i=0; i<st; i++)
		pts[i].x=i*sq;

	/* Draw spline of CPU load */
	fbset_color2(fbdev, WEGI_COLOR_RED);
	fbdev->antialias_on=true;
	draw_spline(fbdev, st, pts, 2, 1);
	fbdev->antialias_on=false;

	//draw_filled_spline(fbdev, st, pts, 2, 1, blpy, WEGI_COLOR_RED, 60); /* baseY, color, alpha */

	/* Write avgload */
	printf("Write avgload %.1f\n", avgload[0]);
	char str[32];
	sprintf(str,"[%.1f]",avgload[0]);
        FTsymbol_uft8strings_writeFB(   fbdev, egi_appfonts.regular, /* FBdev, fontface */
       	                                15, 15,(const UFT8_PCHAR)str, 	/* fw,fh, pstr */
               	                        320, 1, 0,      		/* pixpl, lines, fgap */
                       	                260, pts[st-1].y-30,  		/* x0,y0, */
       	                      	        WEGI_COLOR_RED, -1, 255,       	/* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */
}

/*-------------------------------
Update content of surfuser->imgbuf
by copying workfb IMGBUF.
--------------------------------*/
void update_surfimg(void)
{
	printf("Update surfimg...\n");

        /* Get surface mutex_lock */
        if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 )
                egi_dperr("Fail to get mutex_lock for surface.");
/* ------ >>>  Surface shmem Critical Zone  */

	/* Copy workfb IMGBUF to  surfuser->imgbuf */
	egi_imgbuf_copyBlock( surfimg, workfb.virt_fb, false,	/* dest, src, blend_on */
                              surfimg->width-2, surfimg->height-SURF_TOPBAR_HEIGHT-1, 1, SURF_TOPBAR_HEIGHT, 0, 0);    /* bw, bh, xd, yd, xs,ys */
			      //workfb.virt_fb->width, workfb.virt_fb->height, 1, SURF_TOPBAR_HEIGHT, 0, 0); 	/* bw, bh, xd, yd, xs,ys */

	/* Draw RCBox direct to surfimg(vfbdev), (cx0,cy0)=(0,0) */
	if(RCMenu_ON)
		draw_RCBox(vfbdev, 0, 0);

/* ------ <<<  Surface shmem Critical Zone  */
        pthread_mutex_unlock(&surfshmem->shmem_mutex);
}


/*-------------------  Surface Operation Func  ------------------
Adjust surface size, redraw surface imgbuf.

@surfuser:      Pointer to EGI_SURFUSER.
@w,h:           New size of surface imgbuf.
---------------------------------------------------------------*/
void my_redraw_surface(EGI_SURFUSER *surfuser, int w, int h)
{
        /* Redraw default function first */
        surfuser_redraw_surface(surfuser, w, h); /* imgbuf w,h updated */

        /* Copy workfb IMGBUF to  surfuser->imgbuf */
        egi_imgbuf_copyBlock( surfimg, workfb.VFrameImg, false,   /* dest, src, blend_on */
                              surfimg->width-2, surfimg->height-SURF_TOPBAR_HEIGHT-1, 1, SURF_TOPBAR_HEIGHT, 0, 0);    /* bw, bh, xd, yd, xs,ys */

	/* Draw RCBox direct to surfimg(vfbdev), (cx0,cy0)=(0,0) */
	if(RCMenu_ON)
		draw_RCBox(vfbdev, 0, 0);

}


/*--------------------------------------
	Create RCBox and TickBoxes[]
Return:
	0	OK
	<0	Fails
--------------------------------------*/
int firstdraw_RCBox(FBDEV *vfb)
{

	int tw=18;	/* TickBox size */
	int th=18;

	int bw=130;     /* RCBOX size */
	int bh=tw*3+8*4;

	int i;

	if(vfb==NULL || vfb->virt==false) {
		egi_dpstd("Invalid virt. FBDEV!\n");
		return -1;
	}

	/* Draw RCBox */
	fbset_color2(vfb, WEGI_COLOR_DARKGRAY);
	draw_filled_rect(vfb, 0, 0, bw-1, bh-1);
	/* Draw rim */
	fbset_color2(vfb, WEGI_COLOR_GRAY);
	draw_rect(vfb, 0,0, bw-1, bh-1);

	/* Write Tip */
        FTsymbol_uft8strings_writeFB(   vfb, egi_appfonts.regular, 		/* FBdev, fontface */
       	                                15, 15,(const UFT8_PCHAR)"CPU负荷", 	/* fw,fh, pstr */
               	                        bw, 1, 0,      			/* pixpl, lines, fgap */
                       	                5+tw+15, 8,  			/* x0,y0, */
       	                      	        WEGI_COLOR_WHITE, -1, 255,    	/* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */

        FTsymbol_uft8strings_writeFB(   vfb, egi_appfonts.regular, 		/* FBdev, fontface */
       	                                15, 15,(const UFT8_PCHAR)"WIFI信号", 	/* fw,fh, pstr */
               	                        bw, 1, 0,      			/* pixpl, lines, fgap */
                       	                5+tw+15, th+8*2,  			/* x0,y0, */
       	                      	        WEGI_COLOR_WHITE, -1, 255,    	/* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */

        FTsymbol_uft8strings_writeFB(   vfb, egi_appfonts.regular, 		/* FBdev, fontface */
       	                                15, 15,(const UFT8_PCHAR)"WIFI流量", 	/* fw,fh, pstr */
               	                        bw, 1, 0,      			/* pixpl, lines, fgap */
                       	                5+tw+15, 2*th+8*3,  			/* x0,y0, */
       	                      	        WEGI_COLOR_WHITE, -1, 255,    	/* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */


	/* Create RCBox */
	RCBox=egi_surfBox_create(vfb->virt_fb, 0,0, 0,0, bw, bh);	/* xi,yi, x0, y0, w, h */
	if(RCBox==NULL) {
		egi_dpstd("Fail to create RCBox\n");
		return -2;
	}

	/* Draw unticked TickBox */
	fbset_color2(vfb, WEGI_COLOR_WHITE);
	draw_rect(vfb,0,0, tw-1, th-1);

	/* Create TickBox with unticked imgbuf */
	for(i=0; i<3; i++) {
		TickBox[i]=egi_surfTickBox_create(vfb->virt_fb, 0,0, 10, (th+8)*i+8, tw, th);	/* xi,yi, x0, y0, w, h */
		if(TickBox[i]==NULL) {
			egi_dpstd("Fail to create TickBox[%d].\n",i);
			return -3;
		}
	}

	/* Draw ticked TickBox */
	fbset_color2(vfb, WEGI_COLOR_LTGREEN);
	draw_filled_rect(vfb, tw/4, th/4,  tw*3/4, th*3/4);

	/* Create TickBox->imgbuf_ticked */
	for(i=0; i<3; i++) {
	        /* To copy a block from imgbuf as image for unticked box */
       		TickBox[i]->imgbuf_ticked=egi_imgbuf_blockCopy(vfb->virt_fb, 0,0, tw, th); /* fb, xi, yi, h, w */
       		if(TickBox[i]->imgbuf_unticked==NULL) {
			egi_dpstd("Fail to blockCopy TickBox[%d].imgbuf_ticked.\n", i);
			return -4;
		}
        }

	return 0;
}


/*-------------------------------------------------
Draw RCBox and TickBoxes[]
@cx0,cy0: RCBox's container origin relative to vfb.
	  If container is VFB, then (cx0,cy0)=(0,0).
--------------------------------------------------*/
void draw_RCBox(FBDEV *vfb, int cx0, int cy0)
{
	egi_surfBox_writeFB(vfb, RCBox, cx0, cy0);
	egi_surfTickBox_writeFB(vfb, TickBox[0], RCBox->x0, RCBox->y0);
	egi_surfTickBox_writeFB(vfb, TickBox[1], RCBox->x0, RCBox->y0);
	egi_surfTickBox_writeFB(vfb, TickBox[2], RCBox->x0, RCBox->y0);
}


/*--------------------------------------------------------------
		Mouse Event Callback
	     (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.

---------------------------------------------------------------*/
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	int i;

	/* 1. RightClick to turn on/off RCMenu */
	if( pmostat->RightKeyDown
	    && egi_point_on_surface(surfshmem, pmostat->mouseX, pmostat->mouseY) ) {

		/* Toggle RCMenu */
		RCMenu_ON=!RCMenu_ON;

		if(RCMenu_ON) {
			/* Update position for RCBox, relative to surfshmem */
			RCBox->x0=pmostat->mouseX -surfshmem->x0;
			RCBox->y0=pmostat->mouseY -surfshmem->y0;

			/* Draw RCBox direct to surfimg(vfbdev), (cx0,cy0)=(0,0) */
			draw_RCBox(vfbdev, 0, 0);
		}
		else {
			/* Copy workfb->VFrameImg to surfuser->imgbuf. (Without RCBox) */
			pthread_mutex_lock(&workfb.VFrameImg->img_mutex);
	/* ------ >>>  workfb.VFrameImg Critical Zone  */

        		egi_imgbuf_copyBlock( surfimg, workfb.VFrameImg, false,   /* dest, src, blend_on,  bw, bh, xd, yd, xs,ys */
                		              surfimg->width-2, surfimg->height-SURF_TOPBAR_HEIGHT-1, 1, SURF_TOPBAR_HEIGHT, 0, 0);

			pthread_mutex_unlock(&workfb.VFrameImg->img_mutex);
	/* ------ <<<  workfb.VFrameImg Critical Zone  */
		}

	}
	/* 2. LeftClick to tick/untick TickBox[]. */
	else if(pmostat->LeftKeyDown) {
		/* Tick box SELECTION */
		if(RCMenu_ON) {
			for(i=0; i<3; i++) {				/* X,Y relative to its container. RCBox relative to surfimg */
				if( egi_point_on_surfTickBox( TickBox[i], pmostat->mouseX -surfshmem->x0 -RCBox->x0,
							       pmostat->mouseY -surfshmem->y0 -RCBox->y0 ) ) {

					TickBox[i]->ticked = !TickBox[i]->ticked;
					display_curve[i]=TickBox[i]->ticked;

					/* Draw to surfshmem */
					draw_RCBox(vfbdev, 0,0);
				}
			}
		}
	}
	/* 3. OTher mevent */

}
