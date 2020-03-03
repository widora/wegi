/*--------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

page creation jobs:
1. egi_create_XXXpage() function.
   1.1 creating eboxes and page.
   1.2 assign thread-runner to the page.
   1.3 assign routine to the page.
   1.4 assign button functions to corresponding eboxes in page.
2. thread-runner functions.
3. egi_XXX_routine() function if not use default egi_page_routine().
4. button reaction functins


Midas Zhou
--------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* usleep,unlink */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include "egi.h"
#include "egi_color.h"
#include "egi_txt.h"
#include "egi_objtxt.h"
#include "egi_btn.h"
#include "egi_page.h"
#include "egi_log.h"
#include "egi_symbol.h"
#include "egi_slider.h"
#include "egi_timer.h"
#include "sound/egi_pcm.h"


#define MPFIFO_NAME "/tmp/mpfifo"	/* fifo for mplayer */
static int pfd=-1;			/* file descriptor for open() fifo */
static FILE *pfil;			/* file stream of popen() mplayer */
static pid_t mpid=-1;			/* pid for mplayer */

static int  nlist=0;			/* list index */
static char *str_option[]={"RADIO","XM","MP3","MPP","ENG"};
static char *str_playlist[]=
{
		"/home/radio.list",
		"/home/xm.list",
		"/mmc/mp3.list",
		"/mmc/music/mp3.list",
		"/mmc/eng/mp3.list"
};
static int list_items=5;

enum mplay_status {
	mp_playing=0,
	mp_pause=1,
	mp_stop=2
};
static enum mplay_status status=mp_stop;
static char *str_pstatus[]={"Play","Pause"};

static int egi_pagemplay_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int react_slider(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int react_play(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int react_next(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int react_prev(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int react_option(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int react_stop(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);

static void* check_volume_runner(EGI_PAGE *page);



#define  SLIDER_ID 100 /* use a big value */


/*------- [  PAGE ::  Mplayer Operation ] ---------
1. create eboxes for 6 buttons and 1 title bar
2. create MPlayer Operation page

Return
	pointer to a page	OK
	NULL			fail
--------------------------------------------------*/
EGI_PAGE *egi_create_mplaypage(void)
{
	int i,j;
	int btnW, btnH;

	EGI_EBOX *mplay_btns[6];
	EGI_DATA_BTN *data_btns[6];
	enum egi_btn_type btn_type;

	/* --------- 1. create buttons --------- */
        for(i=0;i<2;i++) /* row of buttons*/
        {

                for(j=0;j<3;j++) /* column of buttons */
                {
			/* 1. create new data_btns */
			if(3*i+j == 5) {/* make last button square */
				btn_type=btnType_square;
				btnW=60;
				btnH=60;
			}
			else {
				btn_type=btnType_circle;
				btnW=66;
				btnH=66;
			}
			data_btns[3*i+j]=egi_btndata_new(3*i+j, /* int id */
							btn_type, /* enum egi_btn_type shape */
							NULL, /* struct symbol_page *icon */
							0, /* int icon_code */
							&sympg_ascii //&sympg_testfont /* for ebox->tag font */
						);
			/* if fail, try again ... */
			if(data_btns[3*i+j]==NULL)
			{
				printf("egi_create_mplaypage(): fail to call egi_btndata_new() for data_btns[%d]. retry...\n", 3*i+j);
				i--;
				continue;
			}

			/* to show tag on the button */
			data_btns[3*i+j]->showtag=true;

			/* 2. create new btn eboxes */
			mplay_btns[3*i+j]=egi_btnbox_new(NULL, /* put tag later */
							data_btns[3*i+j], /* EGI_DATA_BTN *egi_data */
				        		0, /* bool movable */
						        10+(15+60)*j, 150+(15+60)*i, /* int x0, int y0 */
							btnW,btnH, /* int width, int height */
				       			-1,//1, /* int frame,<0 no frame */
		       					egi_color_random(color_deep) //color,/*int prmcolor */
					   );
			/* if fail, try again ... */
			if(mplay_btns[3*i+j]==NULL)
			{
				printf("egi_create_mplaypage(): fail to call egi_btnbox_new() for mplay_btns[%d]. retry...\n", 3*i+j);
				free(data_btns[3*i+j]);
				data_btns[3*i+j]=NULL;

				i--;
				continue;
			}
			/* set tag color */
			mplay_btns[3*i+j]->tag_color=WEGI_COLOR_WHITE;
			/* set container for ebox*/
			// in egi_page_addlist()
		}
	}

	/* add tags and reaction function here */
	egi_ebox_settag(mplay_btns[0], "Prev");
	mplay_btns[0]->reaction=react_prev;

	egi_ebox_settag(mplay_btns[1], str_pstatus[ !(int)(status)] ); //"Play"); /* or Pause */
	mplay_btns[1]->reaction=react_play;

	egi_ebox_settag(mplay_btns[2], "Next");
	mplay_btns[2]->reaction=react_next;

	egi_ebox_settag(mplay_btns[3], "HOME");
	mplay_btns[3]->reaction=egi_pagemplay_exit;

	egi_ebox_settag(mplay_btns[4], "Stop");
	mplay_btns[4]->reaction=react_stop;

	egi_ebox_settag(mplay_btns[5], str_option[nlist]);
//	mplay_btns[5]->reaction=egi_txtbox_demo; /* txtbox demo */
	mplay_btns[5]->reaction=react_option;
	/* adjust square button position */
	mplay_btns[5]->x0=240-75;
	mplay_btns[5]->y0=230;


	/* --------- 2. create a horizontal sliding bar --------- */
	int sl=200; /* slot length */

	/* set playback volume percert to data_slider->val */
	int pvol; /* percent value */
	egi_getset_pcm_volume(&pvol,NULL);
	//printf("-------pvol=%%%d-----\n",pvol);
	EGI_DATA_BTN *data_slider=egi_sliderdata_new(	/* slider data is a EGI_DATA_BTN + privdata(egi_data_slider) */
	                                /* for btnbox */
        	                        SLIDER_ID, btnType_square,  	/* int id, enum egi_btn_type shape */
                	                NULL,				/* struct symbol_page *icon */
					0,				/* int icon_code, */
                        	        &sympg_testfont,		/* struct symbol_page *font */
	                                /* for slider */
					slidType_horiz,			/* enum egi_slid_type */
        	                        (EGI_POINT){(240-sl)/2,100},	/* slider starting EGI_POINT pxy */
                	                5,sl,	 	  		/* slot width, slot length */
	                       	        pvol*sl/100,      		/* init val, usually to be 0 */
                               	 	WEGI_COLOR_GRAY,  		/* EGI_16BIT_COLOR val_color */
					WEGI_COLOR_GRAY5,   		/* EGI_16BIT_COLOR void_color */
	                                WEGI_COLOR_WHITE   		/* EGI_16BIT_COLOR slider_color */
       	                    );

	EGI_EBOX *sliding_bar=egi_slider_new(
  	 			"volume slider",  	/* char *tag, or NULL to ignore */
			        data_slider, 		/* EGI_DATA_BTN *egi_data */
			        15,15,	     		/* slider block: ebox->width/height, to be Min. if possible */
				50,50,	     		/* touchbox size, twidth/theight */
			        -1,	     		/* int frame,<0 no frame */
			        -1          		/* 1. Let <0, it will draw slider, instead of applying
							      gemo or icon.
			                       		   2. prmcolor geom applys only if prmcolor>=0 and
							      egi_data->icon != NULL.
							 */
		     	   );

	/* reset touch area */
	//egi_ebox_set_touchbox(sliding_bar, );

	/* set reaction function */
	sliding_bar->reaction=react_slider;

	/* --------- 3. create title bar --------- */
	EGI_EBOX *title_bar= create_ebox_titlebar(
	        0, 0, /* int x0, int y0 */
        	0, 2,  /* int offx, int offy */
		WEGI_COLOR_GRAY, //egi_colorGray_random(medium), //light),  /* int16_t bkcolor */
    		NULL	/* char *title */
	);
	egi_txtbox_settitle(title_bar, "   	MPlayer 1.0rc");

	/* --------- 4. create mplay page ------- */
	/* 4.1 create mplay page */
	EGI_PAGE *page_mplay=NULL;
	page_mplay=egi_page_new("page_mplayer");
	while(page_mplay==NULL)
	{
			printf("egi_create_mplaypage(): fail to call egi_page_new(), try again ...\n");
			page_mplay=egi_page_new("page_mplay");
			usleep(100000);
	}
	page_mplay->ebox->prmcolor=egi_colorGray_random(color_light);

	/* 4.2 put pthread runner, remind EGI_PAGE_MAXTHREADS 5  */
        page_mplay->runner[0]=check_volume_runner;

        /* 4.3 set default routine job */
        page_mplay->routine=egi_homepage_routine; /* !!! */
	page_mplay->slide_off=true;		  /* trun off PAGE sliding */

        /* 4.4 set wallpaper */
        page_mplay->fpath="/mmc/mplay_neg.jpg";

	/* add ebox to home page */
	for(i=0;i<6;i++) /* buttons */
		egi_page_addlist(page_mplay, mplay_btns[i]);
	egi_page_addlist(page_mplay, title_bar); /* title bar */
	egi_page_addlist(page_mplay, sliding_bar); /* sliding bar */

	return page_mplay;
}


/*--------------------    RUNNER 1   ----------------------
	Check volume and refresh slider.

  WARNING: This function will be delayed by react_slider(),
  so keep slider postion just for a little while before
  you release it.
----------------------------------------------------------*/
static void* check_volume_runner(EGI_PAGE *page)
{
     int pvol;
     int sval=0;
     int buf;

     EGI_EBOX *slider=egi_page_pickbtn(page, type_slider, SLIDER_ID);
     EGI_DATA_BTN *data_btn=(EGI_DATA_BTN *)(slider->egi_data);
     EGI_DATA_SLIDER *data_slider=(EGI_DATA_SLIDER *)(data_btn->prvdata);

     while(1) {
	   // egi_sleep(0,0,500); /* 300ms */
	   tm_delayms(300);

	   /* check page status for exit */
	   //Not necessary anymore? use pthread_cancel() fro PAGE */
	   if(page->ebox->status==status_page_exiting)
	   	return (void *)0;

	   /* get palyback volume */
	   egi_getset_pcm_volume(&pvol,NULL);
	   buf=pvol*data_slider->sl/100;

	   if(buf==sval)continue;

	   printf("\e[38;5;201;48;5;0m %s: ---pvol %d--- \e[0m\n", __func__, pvol);

	   sval=buf;
	   /* slider value is drivered by ebox->x0 for H slider, so set x0 not val */
	   slider->x0=data_slider->sxy.x+sval-(slider->width>>1);

	   /* refresh it */
           //slider->need_refresh=true;
           //slider->refresh(slider);
           egi_ebox_needrefresh(slider); /* No need for quick response */

     }

  	return (void *)0;
}


/*-------------------------------------------------------------------
Horizontal Sliding bar reaction
return
---------------------------------------------------------------------*/
static int react_slider(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	static int mark;
	EGI_DATA_SLIDER *data_slider;
	int minx,maxx;
	int vol;
//	static char strcmd[50];

        /* bypass unwanted touch status */
        if( touch_data->status != pressed_hold && touch_data->status != pressing )
                return btnret_IDLE;

        /* set mark when press down, !!!! egi_touch_getdata() may miss this status !!! */
        if(touch_data->status==pressing)
        {
                printf("react_slider(): pressing sliding bar....\n");
                mark=ebox->x0;
        }
	else if(touch_data->status==pressed_hold) /* sliding */
	{
		//data_slider=(EGI_DATA_SLIDER *)(((EGI_DATA_BTN *)(ebox->egi_data))->prvdata);
		data_slider=egi_slider_getdata(ebox);
		minx=data_slider->sxy.x-(ebox->width>>1);
		maxx=minx+data_slider->sl-1;

		/* update slider x0y0, it's coupled with touchbox position */
		//printf("touch_data->dx = %d\n",touch_data->dx);
		ebox->x0 = mark+(touch_data->dx);
		if(ebox->x0 < minx) ebox->x0=minx;
		else if(ebox->x0 > maxx) ebox->x0=maxx;

		#if 0   /* set need refresh for PAGE routine */
                egi_ebox_needrefresh(ebox);
		#else  /* or force to refresh EBOX now! -- Quik response! */
                ebox->need_refresh=true;
                ebox->refresh(ebox);
                #endif

		/* adjust volume */
		vol=100*data_slider->val/(data_slider->sl-1);
		egi_getset_pcm_volume(NULL,&vol);

	}

	return btnret_IDLE; /* OK, page need not refresh, ebox self refreshed. */
}


/*----------------------------------------
	Button Play reaction func
----------------------------------------*/
static int react_play(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	int ret;
	char strcmd[256];
	static char *cmdPause="pause\n";

	/* args for mplayer */
	char arg_fin[256]="file=/tmp/mpfifo";
	char arg_playlist[256]="/home/radio.list";
	char *mplayer_args[]={ "mplayer", "-cache", "512", "-cache-min", "5", "-slave", "-input", arg_fin, \
			"-aid", "1", "-loop", "0", "-playlist", arg_playlist, NULL };
//						">/dev/null", "2>&1", NULL };


        /* bypass unwanted touch status */
        if( touch_data->status != pressing )
                return btnret_IDLE;

	/* 1. Make a fifo for mplayer command access */
	if(pfd<0) {
		status=mp_stop;
		//printf("unlink(MPFIFO_NAME)...\n");
		//unlink(MPFIFO_NAME); /* TODO: Get stuck here! */
		remove(MPFIFO_NAME);
		EGI_PLOG(LOGLV_TEST,"%s: mkfifo for mplayer...",__func__);
		if (mkfifo(MPFIFO_NAME,0766) !=0) {
			EGI_PLOG(LOGLV_ERROR,"%s:Fail to mkfifo for mplayer.",__func__);
			return -1;
		}
		EGI_PLOG(LOGLV_ASSERT,"%s: mkfifo finish.",__func__);
	}

	/* 2. If stopped, load mplayer by popen(). */
	if(status==mp_stop) {

		/* set tag as loading... */
		egi_ebox_settag(ebox,"Load...");
		egi_ebox_forcerefresh(ebox);

		printf("--- cmd: start mplayer ---\n");
	        sprintf(strcmd,"mplayer -cache 512 -cache-min 5 -slave -input file=%s -aid 1 -loop 0 -playlist %s >/dev/null 2>&1",
									MPFIFO_NAME,str_playlist[nlist] );

		/* Renew args for mplayer */
		sprintf(arg_fin,"file=%s",MPFIFO_NAME);
		sprintf(arg_playlist,"%s",str_playlist[nlist] );


	#if 0 /* ----------  Call popen() to execute mplayer  --------- */
	    	do {
			EGI_PLOG(LOGLV_TEST,"%s: start to popen() mplayer...",__func__);
			/* TODO: popen() will call fork() to start a new process, it's time consuming!
			 *	 Use UBUS to manage and communicate with sub_processes.
			 */
			pfil=popen(strcmd,"we");
			if(pfil!=NULL) {
				EGI_PLOG(LOGLV_ASSERT,"%s: Succeed to popen() mplayer.",__func__);
				status=mp_playing;
				egi_ebox_settag(ebox, "Pause");
				egi_ebox_forcerefresh(ebox);
			} else {
				EGI_PLOG(LOGLV_ERROR,"%s: Fail to popen() mplayer! retry...",__func__);
			}
		} while( pfil==NULL );

	#else /* -----------  Call vfor() and execv() to launch mplayer --------- */
		while( mpid<0 )
		{
			EGI_PLOG(LOGLV_TEST,"%s: Start to vfork() a process...",__func__);
			mpid=vfork();

			if(mpid==0) {
				EGI_PLOG(LOGLV_TEST,"%s: Start to execv() mplayer...",__func__);
				execv("/bin/mplayer", mplayer_args);
				printf("execv() triggered process quit!\n");
			}
			else if(mpid<0) {
				EGI_PLOG(LOGLV_TEST,"%s: Fail to execv() mplayer, retry...",__func__);
			}
			else {
				EGI_PLOG(LOGLV_TEST,"%s: Succeed to execv() mplayer, retry...",__func__);
				status=mp_playing;
				egi_ebox_settag(ebox, "Pause");
				egi_ebox_forcerefresh(ebox);
			}
		}
	#endif

		/* open fifo for command input */
		pfd=open(MPFIFO_NAME,O_WRONLY|O_CLOEXEC);//|O_NONBLOCK);
		if(pfd==-1) {
			printf("%s:Fail to open mkfifo. \n",__func__);
		       	 return btnret_ERR;
		}
		EGI_PLOG(LOGLV_ASSERT,"%s: Succeed to open '%s' for mplayer cmd input.",__func__,MPFIFO_NAME);

	}
	/* 3. Pause mplayer */
	else if(status==mp_playing) {
		/* check whether mplayer is running */
		// --- TODO: How to check if the fifo pipe is broken?!
		//if(fileno(pfil)<0) {
		//if(pfil==NULL) {

		/* write pause command to mplayer  */
		printf("--- cmd: mplayer pause ---\n");
		ret=write(pfd, cmdPause,strlen(cmdPause));
		if(ret<=0) { /* especailly 0! */
			EGI_PLOG(LOGLV_WARN,"%s: try to write command to mplayer throgh pfd: %s.",
									__func__, strerror(errno) );
			pclose(pfil);
			close(pfd);
			pfd=-1;
			unlink(MPFIFO_NAME);

			status=mp_stop;
			egi_ebox_settag(ebox, "Start");
			egi_ebox_forcerefresh(ebox);
		}
		else {
			status=mp_pause;
			egi_ebox_settag(ebox, "Resume");
			egi_ebox_forcerefresh(ebox);
		}
	}
	/*  4. Activate paused mplayer */
	else if(status==mp_pause) {
		//printf("--------- start re play ------\n");
		status=mp_playing;
		write(pfd, cmdPause,strlen(cmdPause));
		egi_ebox_settag(ebox, "Pause");
		egi_ebox_forcerefresh(ebox);
	}

        return btnret_OK; /* */
}

/*---------------------------------------
	Button next reaction func
---------------------------------------*/
static int react_next(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	static char *cmdNext="pt_step 1\n";

        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	/* mplayer next */
	if(status==mp_playing) {
		//printf("--------- start forward ------\n");
		write(pfd,cmdNext,strlen(cmdNext));
	}

        return btnret_OK; /* */
}


/*------------------------------------
	Button Prev reaction func
------------------------------------*/
static int react_prev(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	static char *cmdPrev="pt_step -1\n";

        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	/* mplayer next */
	if(status==mp_playing) {
		//printf("--------- start prev ------\n");
		write(pfd,cmdPrev,strlen(cmdPrev));
	}

        return btnret_OK; /* */
}


/*---------------------------------------
	Button Stop reaction func
---------------------------------------*/
static int react_stop(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	static char *cmdQuit="quit\n";

        /* bypass unwanted touch status */
        if(touch_data->status != releasing)
                return btnret_IDLE;

	/* mplayer quit */
	if(status==mp_playing || status==mp_pause ) {
		printf("--------- Stop mplayer ------\n");
		write(pfd,cmdQuit,strlen(cmdQuit));

	#if 0 /* ----------  Call popen() to execute mplayer  --------- */
		pclose(pfil);
	#else /* -----------  Call vfor() and execv() to launch mplayer --------- */
		if(mpid>0) {
			kill(mpid, SIGTERM);
			mpid=-1;
		}
	#endif
		status=mp_stop;
	}
	close(pfd);
	pfd=-1;
	unlink(MPFIFO_NAME);

	/* change play/pause_button tag to 'play'  */
	EGI_EBOX *btn_play=egi_page_pickbtn(ebox->container, type_btn, 1);  /* id for play/pause btn is 1 */
	egi_ebox_settag(btn_play,"Play");
	egi_ebox_forcerefresh(btn_play);

        return btnret_OK; /* */
}

/*---------------------------------------
	Button Stop reaction func
---------------------------------------*/
static int react_option(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	static char cmdLoadlist[256];

        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	/* mplayer change playlist */
	if(status==mp_playing) {
		nlist++;
			if(nlist>=list_items)nlist=0;
		sprintf(cmdLoadlist,"loadlist '%s' \n",str_playlist[nlist]);
		write(pfd,cmdLoadlist,strlen(cmdLoadlist));
		/* change button tag accordingly */
		egi_ebox_settag(ebox, str_option[nlist]);

	}

	/* refresh */
	egi_ebox_forcerefresh(ebox);

        return btnret_OK; /* */
}


/*-------------------------------------------
	Button Close function
--------------------------------------------*/
static int egi_pagemplay_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{

       /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

        return btnret_REQUEST_EXIT_PAGE; /* >=00 return to routine; <0 exit this routine */
}

