/*-----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of a timer controled by input TXT.
!!! For 'One Server One Client' scenario only !!!

Note:
1. Run timer server in back ground:
	test_timer -s >/dev/null 2>&1 &  ( OR use screen )
2. Send TXT as command:
	test_timer "一小时3刻钟后提醒"
	test_timer "取消提醒"

    命令格式说明：
    1. 命令中如果没有时间词语，则会立即执行操作。如有，则计时等待，
       设定时间到时执行操作。
    2. 一个命令只能有一个目标和一个操作类：
	“5秒后打开收音机”
	“一小时3刻钟后关闭红灯和蓝灯”
    3. 同样操作词后可以跟多个目标：
	“15分钟后打开收音机和所有的灯”
    4. 如果命令中包含“提醒”关键词，设定时间到时会发报警声。
    5. 输入“取消提醒” 则结束当前提醒设置或结束当前报警音乐。

3. To incoorperate with ASR:
   screen -dmS TXTSVR ./txt_timer -s
   autorec_timer  /tmp/asr_snd.pcm  ( autorec_timer ---> asr_timer.sh ---> txt_timer(test_timer)  )

4. Two sound playing threads

Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------*/
#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include "egi_common.h"
#include "egi_cstring.h"
#include "egi_FTsymbol.h"
#include "egi_gif.h"
#include "egi_shmem.h"
#include "egi_pcm.h"

#define SHMEM_SIZE 		(1024*4)
#define TXTDATA_OFFSET		1024
#define TXTDATA_SIZE 		1024
#define DEFAULT_ALARM_SOUND	"/mmc/alarm.wav"
#define DEFAULT_TICK_SOUND	"/mmc/tick.wav"
#define TIMER_BKGIMAGE		"/mmc/linux.jpg"
#define PCM_MIXER		"mymixer"

/* Timer data struct */
typedef struct {
	time_t  ts;  		/*  time span/duration value for countdown, typedef long time_t   */
	time_t  tp;		/*  tp=tnow+ts. Seconds from 1970-01-01 00:00:00 +0000 UTC to the preset time point.   */

	bool	statActive;	/*  status/indicator: true while timer is ticking  */
	bool	sigStart;	/*  signal: to start timer */
	bool	sigStop;	/*  signal: to stop timing and/or alarming  */
	bool    sigQuit;	/*  signal: to quit the timer thread */

	EGI_PCMBUF *pcmalarm;   /*  for alarm sound effect */
	EGI_PCMBUF *pcmtick;    /*  for ticking effect 	*/
	int	vstep;		/*  vstep for volume control */

	bool	tickSynch;	/*  signal: for synchronizing ticking sound */
	bool	sigNoTick;	/*  signal: to stop ticking sound */
	bool	statTick;	/*  status/indicator: true while playing pcmtick */
	bool	sigNoAlarm;	/*  signal: to stop alarming */
	bool	statAlarm;	/*  status/indicator: true while playing pcmalarm */

	char	utxtCmd[TXTDATA_SIZE];	/* UFT-8 TXT Command, to be executed when preset time has come. */
	bool	SetLights[3];	/*  set lights ON/OFF */
	bool	SetHidden;	/*  set hidden as ON/OFF */
} etimer_t;
etimer_t timer1={ .SetLights[0]=1, .SetLights[1]=1, .SetLights[2]=1 };

/* rotating image */
EGI_IMGBUF *imgbuf_circle;
int img_angle;


/* lights */
static EGI_16BIT_COLOR light_colors[]={ WEGI_COLOR_BLUE, WEGI_COLOR_RED, WEGI_COLOR_GREEN };

/* timer function */
static void *timer_process(void *arg);
static void *thread_ticking(void *arg);
static void *thread_alarming(void *arg);
static void writefb_datetime(void);
static void writefb_lights(void);
static void parse_utxtCmd(const char *utxt);
static void writefb_utxtCmd(const char *utxt);
static void touch_event(void);


/*=====================================
		MAIN  PROG
======================================*/
int main(int argc, char **argv)
{
	int dts=1; /* Default displaying for 1 second */
	int opt;
	int i;
	double sang;
	bool Is_Server=false;
	bool sigstop=false;
	char *ptxt=NULL;	/* pointer to client input txt, to shm */
	char *pdata=NULL;	/* pointer to txt data in shm, pdata=shm_msg.shm_map+TXTDATA_OFFSET */
	char *wavpath=NULL;
        char strtm[128]={0};  /* Buffer for extracted time-related string */
	pthread_t	timer_thread;
	time_t	ts;

	if(argc < 1)
		return 2;

        /* parse input option */
        while( (opt=getopt(argc,argv,"hsv:w:t:q"))!=-1)
        {
                switch(opt)
                {
                       	case 'h':
                           printf("usage:  %s [-hsw:t:q] text \n", argv[0]);
                           printf("	-h   help \n");
			   printf("	-s   to start as server. If shmem exists in /dev/shm/, then it will be removed first.\n");
			   printf("	-w   to indicate wav file as for alarm sound, or use default.\n");
			   printf("	-q   to end the server process.\n");
                           printf("	-t   holdon time, in seconds.\n");
                           printf("	text 	TXT as for command \n");
                           return 0;
			case 's':
			   Is_Server=true;
			   break;
			case 'v':
			   timer1.vstep=atoi(optarg);
			   printf("Volume control vstep=%d for timer tick/alarm.\n",timer1.vstep);
			   break;
			case 'w':
			   wavpath=optarg;
			   printf("Use alarm sound file '%s'\n",wavpath);
			   break;
		 	case 't':
			   dts=atoi(optarg);
			   printf("dts=%d\n",dts);
			   break;
                       	case 'q':
                           printf("Signal to quit timer server...\n");
                           sigstop=true;
                           break;
                       	default:
                           break;
                }
        }
	if(optind<argc) {
		ptxt=argv[optind];
		printf("Input TXT: %s\n",ptxt);
	}

        /* ------------ SHM_MSG communication ---------- */
        EGI_SHMEM shm_msg= {
          .shm_name="timer_ctrl",
          .shm_size=SHMEM_SIZE,
        };


	/* Open shmem,  For server, If /dev/shm/timer_ctrl exits, remove it first.  TODO: */
	if( Is_Server ) {
		if( egi_shmem_remove(shm_msg.shm_name) ==0 )
			printf("Old shm dev removed!\n");
	}
        if( egi_shmem_open(&shm_msg) !=0 ) {
                printf("Fail to open shm_msg!\n");
                exit(1);
        }

	/* Assign shm data starting address */
	pdata=shm_msg.shm_map+TXTDATA_OFFSET;
	memset( shm_msg.msg_data->msg,0,64 );
	memset( pdata, 0, TXTDATA_SIZE); /* clear data block */


/* >>>>>>>>>>>>>>>   Start Client Job  <<<<<<<<<<<<<<< */
        if( Is_Server==false )
	{
	  if(shm_msg.msg_data->active ) {         /* If msg_data is ACTIVE already */
		printf("Start client job...\n");
                if(shm_msg.msg_data->msg[0] != '\0')
                        printf("Msg in shared mem '%s' is: %s\n", shm_msg.shm_name, shm_msg.msg_data->msg);
                if( ptxt != NULL ) {
			shm_msg.msg_data->signum=dts;
			strncpy( shm_msg.msg_data->msg, "New data available", 64);
                        strcpy( pdata, ptxt);
			printf("Finishing copying TXT to shmem!\n");
                }

                /* ---- signal to quit the main process ----- */
                if( sigstop ) {
                        printf("Signal to stop and wait....\n");
                        while( shm_msg.msg_data->active) {
                                shm_msg.msg_data->sigstop=true;
                                usleep(100000);
                        }
		}
               	egi_shmem_close(&shm_msg);
		printf("End client job!\n");
               	exit(1);
	    }
	    else {	/* If msg_data is ACTIVE already */
		printf("Server is already down!\n");
		exit(1);
	   }
        }
/* >>>>>>>>>>>>>   END of Client Job  <<<<<<<<<<<<<<< */



/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   Start Server Job   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
	printf("Start server job...\n");
        if( !shm_msg.msg_data->active )
        {
                shm_msg.msg_data->active=true;
                printf("You are the first to handle this mem!\n");
        }


        /* <<<<<  EGI general init  >>>>>> */

        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */
#if 0
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_timer") != 0) {        /* start logger */
                printf("Fail to init egi logger,quit.\n");
                return -1;
        }
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {       /* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
#endif

        printf("FTsymbol_load_allpages()...\n");
        if(FTsymbol_load_allpages() !=0 ) {
		printf("Fail to load FTsym pages,quit.\n");
		return -2;
	}

        printf("symbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;

#if 1
        printf("start touchread thread...\n");
        egi_start_touchread();                  /* start touch read thread */
#endif
        /* <<<<------------------  End EGI Init  ----------------->>>> */


	/* Load sound pcmbuf for Timer */
	if(wavpath != NULL) {
		timer1.pcmalarm=egi_pcmbuf_readfile(wavpath);
        	if( timer1.pcmalarm==NULL )
                	printf("Fail to load sound file '%s' for alarm sound.\n", wavpath);
	}
	if(timer1.pcmalarm==NULL) {
		timer1.pcmalarm=egi_pcmbuf_readfile(DEFAULT_ALARM_SOUND);
        	if( timer1.pcmalarm==NULL )
                	printf("Fail to load sound file '%s' for default alarm sound.\n", DEFAULT_ALARM_SOUND);
        }
	timer1.pcmtick=egi_pcmbuf_readfile(DEFAULT_TICK_SOUND);
	if(timer1.pcmtick==NULL)
               	printf("Fail to load sound file '%s' for default ticking sound.\n", DEFAULT_TICK_SOUND);

	/* Set FB mode */
	fb_set_directFB(&gv_fb_dev, false);
	fb_position_rotate(&gv_fb_dev,3);

	/* Init FB working buffer with image */
        EGI_IMGBUF *eimg=egi_imgbuf_readfile(TIMER_BKGIMAGE);
        if(eimg==NULL)
                printf("Fail to read and load file '%s'!", TIMER_BKGIMAGE);
        egi_imgbuf_windisplay( eimg, &gv_fb_dev, -1,
                               0, 0, 0, 0,
                               gv_fb_dev.pos_xres, gv_fb_dev.pos_yres );

	/* Draw lights */
	//writefb_lights();

	/* Draw timing circle */
	fbset_color(WEGI_COLOR_GRAY3);//LTGREEN);
	/* FBDEV *dev, int x0, int y0, int r, float Sang, float Eang, unsigned int w */
	draw_arc(&gv_fb_dev, 100, 120, 90, -MATH_PI/2, MATH_PI/2*3, 10);
	fbset_color(WEGI_COLOR_GRAY);
	draw_arc(&gv_fb_dev, 100, 120, 80, -MATH_PI/2, MATH_PI/2*3, 10);


	/* Draw second_mark circle */
	fbset_color(WEGI_COLOR_WHITE);
	for(i=0; i<60; i++) {
		sang=(90.0-1.0*i/60.0*360)/180*MATH_PI;
		/* FBDEV *dev,int x1,int y1,int x2,int y2, unsigned w */
		draw_wline_nc(&gv_fb_dev, 100+75.0*cos(sang), 120-75.0*sin(sang), 100+85.0*cos(sang), 120-85.0*sin(sang), 3);
	}

	/* Init FB back ground buffer page with working buffer */
        memcpy(gv_fb_dev.map_bk+gv_fb_dev.screensize, gv_fb_dev.map_bk, gv_fb_dev.screensize);

	/* Write 00:00:00 */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          	/* FBdev, fontface */
                                        30, 30, (const unsigned char *)"00:00:00",    	/* fw,fh, pstr */
                                        320-10, 5, 4,                           /* pixpl, lines, gap */
                                        37, 100,                                /* x0,y0, */
                                        WEGI_COLOR_GRAY, -1, -1,      	/* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL  );      /* int *cnt, int *lnleft, int* penx, int* peny */
	/* Display */
        fb_page_refresh(&gv_fb_dev,0);

        /* Init FB 3rd buffer page, with mark 00:00:00  */
        fb_page_saveToBuff(&gv_fb_dev, 2);

	/* Start timer thread */
	printf("Start Timer thread ...\n");
	if( pthread_create(&timer_thread,NULL,timer_process, (void *)&timer1) != 0 ) {
		printf("Fail to start timer process!\n");
		exit(-1);
	}

	printf("Start timer control service loop ...\n");
while(1) {

	/* Check EGI_SHM_MSG: Signal to quit */
        if(shm_msg.msg_data->sigstop ) {
        	printf(" ------ shm_msg: sigstop received! ------ \n");
                /* reset data */
                shm_msg.msg_data->signum=0;
                shm_msg.msg_data->active=false;
                shm_msg.msg_data->sigstop=false;

		break;
	}

	/* Check txt data for display  */
	if( pdata[0]=='\0') {
		//printf(" --- \n");
		//usleep(200000);
		egi_sleep(0,0,100);
		continue;
	} else {
		printf("shm_msg.msg_data->signum=%d\n", shm_msg.msg_data->signum);
		printf("shm_msg.msg_data->msg: %s\n", shm_msg.msg_data->msg);
        	printf("shm priv data: %s\n", pdata);
	}


	/* If pdata[0] != '\0':  Extract time related string */
	memset(strtm,0,sizeof(strtm));
	if( cstr_extract_ChnUft8TimeStr(pdata, strtm, sizeof(strtm))==0 )
        		printf("Extract time string from pdata: %s\n",strtm);
	/* Get ts */
	ts=(time_t)cstr_getSecFrom_ChnUft8TimeStr(strtm, &timer1.tp);


	/* Simple NLU: parse input txt data and extract key words */


	/* ------------------- Parse Timer Command --------------------- */
	/* CASE 1:  --- Set the timer --- */
	if(  timer1.statActive==false
		/* Note: OR if timer1.ts>0, ignore following keywords checking !! Just start timer and parse command when preset time comes! */
	     	&& ( strstr(pdata,"定时") || strstr(pdata,"提醒") || strstr(pdata,"后") || strstr(pdata,"再过")
		     || strstr(pdata,"开") || strstr(pdata,"关") || strstr(pdata,"放") )
          )
	{
		/* assign timer1.ts */
		timer1.ts=ts;

		/* Reset Timer and check time span in seconds */
		timer1.sigStop=false;
		timer1.sigQuit=false;
		timer1.sigNoTick=false;
		timer1.sigNoAlarm=false;

		/* Start timer only ts is valid! */
		if( timer1.ts > 0 ) {
			/* set sigStart at last! */
			timer1.sigStart=true;
   			printf("Totally %ld seconds.\n",(long)timer1.ts);

			/* Save TXT command to timer */
			strcpy(timer1.utxtCmd, pdata);
		}
	}
	/* CASE 2:  --- Stop timer alarming sound --- */
	else if( timer1.statActive==true
		 	&& ( strstr(pdata,"结束") || strstr(pdata,"取消") || strstr(pdata,"停止") || strstr(pdata,"关闭"))
		 	&& ( strstr(pdata,"时") || strstr(pdata,"提醒")  || strstr(pdata,"定时") || strstr(pdata,"闹钟") || strstr(pdata,"响") )
	        )
	{
		timer1.sigStop=true;
	}
	/* CASE 3:  --- Timer is working! --- */
	else if ( timer1.statActive==true )
		printf("Timer is busy!\n");


	/* ------------------ Parse NON_Timer Command, parse it wright now! --------------------- */

	/* CASE ts<=0 :  Parse and execute if NO timer setting command found in TXT. Otherwise to be parsed in Timer process when time is up. */
	if( ts <= 0 ) {
		parse_utxtCmd(pdata);
	}


	/* Reset shmem data */
	pdata[0]='\0';
}

	/* Join timer thread */
	printf("Try to join timer_thread...\n");
	timer1.sigStop=true; /* To end loop playing alarm PCM first if it's just doing so! ... */
	timer1.sigQuit=true;
	if( pthread_join(timer_thread, NULL) != 0)
               	printf("Fail to joint timer thread!\n");
       	else
		printf("Timer thread is joined!\n");


	/* Free Timer data */
	printf("Try to free pcm data in etimer ...\n");
	if(timer1.pcmalarm)
		egi_pcmbuf_free(&timer1.pcmalarm);
	if(timer1.pcmtick)
		egi_pcmbuf_free(&timer1.pcmtick);

	/* Close shmem */
	printf("Closing shmem...\n");
        egi_shmem_close(&shm_msg);

        /* <<<<<-----------------  EGI general release  ----------------->>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
        printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev); /* Flush FB filo if necessary */
        release_fbdev(&gv_fb_dev);
        printf("egi_end_touchread()...\n");
        egi_end_touchread();
        printf("egi_quit_log()...\n");
#if 0
        egi_quit_log();
        printf("<-------  END  ------>\n");
#endif
        return 0;
}



/*---------------------------------
	Timer process function
----------------------------------*/
static void *timer_process(void *arg)
{
	long 	tset=0;			/* set seconds */
	long 	ts=0;			/* left seconds */
	time_t 	tm_end=0; 		/* end time in time_t */
	struct 	tm *localtm_end;	/* localt time in struct tm */
	int 	hour,min,second;
	char 	strCntTm[128];		/* time for countdown */
	char 	strSetTm[128];		/* preset time, in local time */
	float 	fang;			/* for arc */
	float 	sang;			/* for second hand */
	bool 	once_more=false;  	/* init as false */
	pthread_t tick_thread;
	pthread_t alarm_thread;
	etimer_t *etimer=(etimer_t *)arg;
	EGI_POINT tripts[3]={ {92, 132 }, {92+16, 132}, {92+16/2, 132+16} }; /* a triangle mark */

	/* Load page with 00:00:00  */
	fb_page_restoreFromBuff(&gv_fb_dev, 2);

   while(1) {

	/* Check signal to quit */
	if( etimer->sigQuit==true ) {
		printf("%s:Signaled to quit timer thread!\n",__func__);
		/* Try to stop ticking */
		etimer->sigNoTick=true;
		if(etimer->statTick) {
			if(pthread_join(tick_thread,NULL)!=0)
				printf("%s:Fail to join tick_thread!\n",__func__);
			else
				etimer->statTick=false;
		}
		/* Try to stop alarming */
		etimer->sigNoAlarm=true;
		if(etimer->statAlarm) {
			if(pthread_join(alarm_thread,NULL)!=0)
				printf("%s:Fail to join alarm_thread!\n",__func__);
			else
				etimer->statAlarm=false;
		}

		etimer->sigQuit=false;
		break;
	}
	/* Check signal to start timer */
	else if( etimer->sigStart && etimer->statActive==false && etimer->ts>0 ) {

		/* restore screen */
		fb_page_restoreFromBuff(&gv_fb_dev, 1);

		/* get timer set seconds */
		tset=ts=etimer->ts;
		tm_end=time(NULL)+tset;

		/* convert to preset time point,in local time */
		localtm_end=localtime(&tm_end);
		sprintf(strSetTm,"%02d:%02d:%02d",localtm_end->tm_hour, localtm_end->tm_min, localtm_end->tm_sec);

		/* reset etimer params */
		etimer->statActive=true;
		etimer->sigStart=false;

		/* reset once_more token */
		once_more=false;

		/* start ticking sound */
		etimer->sigNoTick=false;
		if( pthread_create(&tick_thread, NULL, thread_ticking, arg) != 0 )
			printf("%s: Fail to start ticking thread!\n",__func__);
		else
			etimer->statTick=true;
	}
	/* Continue to wait if not active */
	else if(etimer->statActive==false) {
		/* restore back */
	        memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_bk+2*gv_fb_dev.screensize, gv_fb_dev.screensize);

		/* Display current time */
		writefb_datetime();
	        /* Display lights */
        	writefb_lights();

		/* Check touch  */
		img_angle+=3;
			if(img_angle>360) img_angle%=360;
		touch_event();

		/* refresh FB */
		if(!etimer->SetHidden)
			fb_page_refresh(&gv_fb_dev,0);

		/* sleep a while */
		egi_sleep(0,0,75);
		continue;
	}

	/* Check signal to stop timer */
	if(etimer->sigStop) {
		/* Try to stop ticking */
		etimer->sigNoTick=true;
		if(etimer->statTick) {
			if(pthread_join(tick_thread,NULL)!=0)
				printf("%s:Fail to join tick_thread!\n",__func__);
			else
				etimer->statTick=false;
		}
		/* Try to stop alarming */
		etimer->sigNoAlarm=true;
		if(etimer->statAlarm) {
			if(pthread_join(alarm_thread,NULL)!=0)
				printf("%s:Fail to join alarm_thread!\n",__func__);
			else
				etimer->statAlarm=false;
		}

		/* Reset Timer signals and indicators */
		etimer->ts=0;
		etimer->statActive=false;
		etimer->sigStop=false;
		etimer->sigNoTick=false;
		etimer->sigNoAlarm=false;

		/* restore screen */
		fb_page_restoreFromBuff(&gv_fb_dev, 2);

		continue;
	}


	/* Convert remaining time to H:M:S  */
	memset(strCntTm,0,sizeof(strCntTm));
	hour=ts/3600;
	min=(ts-hour*3600)/60;
	second=ts-hour*3600-min*60;
	sprintf(strCntTm,"%02d:%02d:%02d", hour,min,second );

        /*  Init FB working buffer page with back ground buffer */
        memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_bk+gv_fb_dev.screensize, gv_fb_dev.screensize);

	/* Draw a moving oject as a moving second hand */
	fbset_color(WEGI_COLOR_GREEN);
	/* center(100,120) r=80 */
	sang=(90.0-(tset-ts)%60/60.0*360)/180*MATH_PI;
	if(once_more && ts!=0 ){ /* skip when ts==0, but enable first time when ts turns to be 0 */
		sang += 360.0/(2*60)/180*MATH_PI; /* for 0.5s */
	}
	//printf("tset=%ld, ts=%ld, sang=%f \n",tset, ts, sang);
	/* draw_wline_nc(FBDEV *dev,int x1,int y1,int x2,int y2, unsigned w) */
	// draw_wline_nc(&gv_fb_dev, 100+65*cos(sang), 120-65*sin(sang), 100+85.0*cos(sang),120-85.0*sin(sang), 7);

	/* Draw a small dot. draw_filled_circle(FBDEV *dev, int x, int y, int r) */
	draw_filled_circle(&gv_fb_dev, 100+75*cos(sang), 120-75*sin(sang), 10);
	//fbprint_fbcolor();
	//fbprint_fbpix(&gv_fb_dev);

	/* Timing Display */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        20, 20,(const unsigned char *)"剩余时间",    /* fw,fh, pstr */
                                        320-10, 1, 4,                           /* pixpl, lines, gap */
                                        60, 68,                                /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, -1,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	/* Write countdown time */
	FTsymbol_uft8strings_writeFB( 	&gv_fb_dev, egi_sysfonts.bold,  	/* FBdev, fontface */
				      	28, 28,(const unsigned char *)strCntTm, 	/* fw,fh, pstr */
				      	320-10, 5, 4,                    	/* pixpl, lines, gap */
					40, 98,                          	/* x0,y0, */
                                     	WEGI_COLOR_ORANGE, -1, -1,      /* fontcolor, transcolor,opaque */
                                     	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */


	#if 0
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        16, 16,(const unsigned char *)"设定时间",    /* fw,fh, pstr */
                                        320-10, 1, 4,                           /* pixpl, lines, gap */
                                        67, 132,                                /* x0,y0, */
                                        WEGI_COLOR_GRAY, -1, -1,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        18, 18,(const unsigned char *)strSetTm,    /* fw,fh, pstr */
                                        320-10, 1, 4,                           /* pixpl, lines, gap */
                                        62, 150,                                /* x0,y0, */
                                        WEGI_COLOR_GRAY, -1, -1,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	#else
	/* Write preset local time */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular,          /* FBdev, fontface */
                                        20, 20,(const unsigned char *)strSetTm,    /* fw,fh, pstr */
                                        320-10, 1, 4,                           /* pixpl, lines, gap */
                                        60, 150,                                /* x0,y0, */
                                        WEGI_COLOR_ORANGE, -1, -1,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

	#endif

	fbset_color(WEGI_COLOR_ORANGE);
	//fang=-MATH_PI/2+(2*MATH_PI)*(tset-ts)/tset;
	fang=MATH_PI*3/2-2*MATH_PI*ts/tset;
	draw_arc(&gv_fb_dev, 100, 120, 90, -MATH_PI/2, fang, 10);

	/* Display current time H:M:S */
	writefb_datetime();
	writefb_lights();

	/* Display memo message */
	if((tset-ts)/2%2==0 || ts==0)
		writefb_utxtCmd(etimer->utxtCmd);

	/* Display a triangle mark */
	if(once_more) {
		fbset_color(WEGI_COLOR_GREEN);
		draw_filled_triangle(&gv_fb_dev, tripts); /* draw a triangle mark */

	}

	/* Refresh FB */
	if(!etimer->SetHidden)
		fb_page_refresh(&gv_fb_dev,0);

	/* --------- While alraming, loop back -------- */
	if(etimer->statAlarm && ts==0 )
		continue;

	/* synchronize with ticking */
	//if(ts%4==0)
	//	etimer->tickSynch=true;

	/* Refresh 2 times for 1 second */
	if(once_more) {
		egi_sleep(0,0,400);
		once_more=false;
		continue;
	} else {
		once_more=true;
	}


	/* Wait for next second, sleep and decrement, skip ts==0  */
	while( tm_end-ts >= time(NULL) && ts>0 ) {
		egi_sleep(0,0,100);
	}

	/* Stop ticking and start to alarm, skip while alarming  */
	if(ts==0 && etimer->statAlarm==false ) {
		/* reset */
		etimer->ts=0;

		/* Disable/stop ticking first */
		etimer->sigNoTick=true;
		if(pthread_join(tick_thread,NULL)!=0)
			printf("%s:Fail to join tick_thread!\n",__func__);
		else
			etimer->statTick=false;
		etimer->sigNoTick=false; /* reset sig */

		/* Start alarming */
		if( pthread_create(&alarm_thread, NULL, thread_alarming, arg) != 0 )
			printf("%s: Fail to start alarming thread!\n",__func__);
		else
			etimer->statAlarm=true;

		/* ---- Execute TXT command in etimer->utxtCmd[] ---- */
		parse_utxtCmd(etimer->utxtCmd);
		writefb_lights();
	}

	/* ts decrement */
	ts--;
	if(ts<0)ts=0; /* Loop for displaying 00:00:00 */
  }

	/* say Bye Bye */
	fb_shift_buffPage(&gv_fb_dev,2);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        24, 24,(const unsigned char *)"Bye Bye ...",    /* fw,fh, pstr */
                                        160, 1, 0,                           /* pixpl, lines, gap */
                                        180, 200,                                /* x0,y0, */
                                        WEGI_COLOR_ORANGE, -1, -1,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

	/* restore screen */
	fb_page_restoreFromBuff(&gv_fb_dev, 2);

	/* reset timer */
	etimer->ts=0;
	etimer->tp=0;
	etimer->statActive=false;
	etimer->statTick=false;
	etimer->statAlarm=false;
	etimer->sigStop=false;
	etimer->sigQuit=false;
	etimer->tickSynch=false;
	etimer->sigNoTick=false;
	etimer->sigNoAlarm=false;

	printf("End timer process!\n");

	return (void *)0;
}

/*------------------------------------------
      Sound effect for timer ticking
-------------------------------------------*/
static void *thread_ticking(void *arg)
{
     etimer_t *etimer=(etimer_t *)arg;

//    pthread_detach(pthread_self());  To avoid multimixer error
     egi_pcmbuf_playback(PCM_MIXER, (const EGI_PCMBUF *)etimer->pcmtick, etimer->vstep, 1024, 0, &etimer->sigNoTick, NULL,NULL); //&etimer->tickSynch);

//     pthread_exit((void *)0);
	return (void *)0;
}


/*------------------------------------------
      Sound effect for timer alarming
-------------------------------------------*/
static void *thread_alarming(void *arg)
{
     etimer_t *etimer=(etimer_t *)arg;

//    pthread_detach(pthread_self());  To avoid multimixer error

     /* Alarm, Max. 100 loops */
     egi_pcmbuf_playback(PCM_MIXER, (const EGI_PCMBUF *)etimer->pcmalarm, etimer->vstep, 1024, 100, &etimer->sigNoAlarm, NULL,NULL);
     etimer->sigStop=true;

//     pthread_exit((void *)0);
	return (void *)0;
}



/*------------------------------------
     Write date and time to FB
-------------------------------------*/
static void writefb_datetime(void)
{
	char strHMS[32];  	/* Hour_Min_Sec */
	char ustrMD[64]; 	/* Month_Day in uft-8 chn */

       	tm_get_strtime(strHMS);
	tm_get_ustrday(ustrMD);

	#if 0
        symbol_string_writeFB(&gv_fb_dev, &sympg_ascii, WEGI_COLOR_LTBLUE,	/* sympg_ascii abt.18x18 */
                                       SYM_FONT_DEFAULT_TRANSPCOLOR, 215, 165, strHMS, -1);
	#else
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
               	                        20, 20,(const unsigned char *)strHMS,   /* fw,fh, pstr */
                       	                320-10, 1, 4,                           /* pixpl, lines, gap */
                               	        205, 165,                         /* x0,y0, */
                                       	WEGI_COLOR_GRAYB, -1, -1,      	  /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      	  /* int *cnt, int *lnleft, int* penx, int* peny */
	#endif

       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
               	                        20, 20,(const unsigned char *)ustrMD,   /* fw,fh, pstr */
                       	                320-10, 1, 4,                           /* pixpl, lines, gap */
                               	        214, 140,                         /* x0,y0, */
                                       	WEGI_COLOR_GRAYB, -1, -1,      	  /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      	  /* int *cnt, int *lnleft, int* penx, int* peny */
}


/*----------------------------------------------------------
    Parse and exectue uft-8 TXT command
1. Parse light control and set TIMER.SetLights[]
2. TODO: Use ANN to learn and build a mode as for simple NLU.
-----------------------------------------------------------*/
static void parse_utxtCmd(const char *utxt)
{
	if(utxt==NULL)return;

	printf(" -------- Parse etimer->utxtCmd: %s \n", utxt);

	/* Disable alarm, if NOT required */
	if(!strstr(utxt,"提醒")) {
		printf("--- Alarm is off! ---\n");
		timer1.sigNoAlarm=true;
	}
	else
		timer1.sigNoAlarm=false;

	/* Enable/Disable SetHidden */
	if( strstr(utxt,"现身") || strstr(utxt,"出来") ) {
		timer1.SetHidden=false;
		system("/etc/init.d/juhe-news stop");
	}
        else if( ( strstr(utxt,"隐藏") || strstr(utxt,"隐身") )
                && ( strstr(utxt,"停止") || strstr(utxt,"结束") ) ) {
		timer1.SetHidden=false;
		system("/etc/init.d/juhe-news stop");
        }
	else if( strstr(utxt,"隐藏") || strstr(utxt,"隐身") ) {
		timer1.SetHidden=true;
		system("/etc/init.d/juhe-news start");
	}

	/* Control Lights */
        if(strstr(utxt,"开")) {
            if(strstr(utxt,"灯")) {
                if( strstr(utxt,"蓝") )
			timer1.SetLights[0]=true;
                if(strstr(utxt,"红") )
			timer1.SetLights[1]=true;
                if(strstr(utxt,"绿") )
			timer1.SetLights[2]=true;
                if(strstr(utxt,"所有") || strstr(utxt,"全部") )
			timer1.SetLights[0]=timer1.SetLights[1]=timer1.SetLights[2]=true;
            }
        }
        if(strstr(utxt,"关")) {
            if(strstr(utxt,"灯")) {
                if( strstr(utxt,"蓝") )
                        timer1.SetLights[0]=false;
                if(strstr(utxt,"红") )
                        timer1.SetLights[1]=false;
                if(strstr(utxt,"绿") )
                        timer1.SetLights[2]=false;
                if(strstr(utxt,"所有") || strstr(utxt,"全部") )
			timer1.SetLights[0]=timer1.SetLights[1]=timer1.SetLights[2]=false;
            }
        }

        /* MPLAYER  NEXT/PREV control
	 * Note: Parsing sequence is improtant!!!
	 */
        if( ( strstr(utxt,"首") || strstr(utxt,"曲") || strstr(utxt,"歌") || strstr(utxt,"台") || strstr(utxt,"节目") || strstr(utxt,"个") )
	    && ( strstr(utxt,"下") || strstr(utxt,"进") ) 	/* strstr(utxt,"后") 与定时 “后” 冲突  */
	  )
	{
			printf("Timer NLU: Play next...\n");
                 	system("/home/mp_next.sh 1");
			system("/home/vmplay.sh next &");
                	system("/home/myradio.sh next &");
	}
	else if( ( strstr(utxt,"首") || strstr(utxt,"曲") || strstr(utxt,"歌") || strstr(utxt,"台") || strstr(utxt,"节目") )
		 && ( strstr(utxt,"前") ||  strstr(utxt,"上") ||  strstr(utxt,"退") )
	       )
	{
			printf("Timer NLU: Play prev...\n");
                 	system("/home/mp_next.sh -1");
			system("/home/vmplay.sh prev &");
                	system("/home/myradio.sh prev &");
        }
	/* MPLAYER PLAY control */
	else if( ( strstr(utxt,"停止") || strstr(utxt,"关闭") || strstr(utxt,"结束") )
		&&  ( strstr(utxt,"电影") || strstr(utxt,"MTV") ) )
	{
		timer1.SetHidden=false;
		system("/home/vmplay.sh quit &");
	}
	else if(  ( strstr(utxt,"停止") || strstr(utxt,"关闭") || strstr(utxt,"结束") )
		&& ( strstr(utxt,"歌曲") || strstr(utxt,"MP3") || strstr(utxt,"mp3") || strstr(utxt,"音乐")) )
	{
		printf("Timer NLU: Quit MP ...\n");
		system("/home/mp_quit.sh &");
	}

        else if( ( strstr(utxt,"停止") || strstr(utxt,"关闭") || strstr(utxt,"结束") )
                && ( strstr(utxt,"广播") || strstr(utxt,"收音机") ) )
        {
                printf("Timer NLU: stop radio ...\n");
                system("/home/myradio.sh quit &");
        }
	/* Stop all */
	else if ( ( strstr(utxt,"停止") || strstr(utxt,"结束") )
		 &&  strstr(utxt,"播放")  )
	{
		printf("Timer NLU: stop all, vmplay/mp3/radio ...\n");
		system("/home/vmplay.sh quit &");
		system("/home/mp_quit.sh &");
		system("/home/myradio.sh quit &");
	}
	else if( strstr(utxt,"播放") && ( !strstr(utxt,"停止") && !strstr(utxt,"结束") )
		&& ( strstr(utxt,"音乐") || strstr(utxt,"歌曲") || strstr(utxt,"MP3") || strstr(utxt,"mp3") ) )
	{
		printf("Timer NLU: Start to play mp3...\n");
		system("/home/mp3.sh");
	}
        else if( ( ( strstr(utxt,"打开") || strstr(utxt,"播放") ) && !strstr(utxt,"停止")  )
                 && ( strstr(utxt,"广播") || strstr(utxt,"新闻") || strstr(utxt,"收音机") )
	        )
        {
                printf("Timer NLU: Start to play radio...\n");
                system("/home/myradio.sh start &");
        }

	else if( strstr(utxt,"播放") && ( !strstr(utxt,"停止") && !strstr(utxt,"结束") )
		 && ( strstr(utxt,"电影") || strstr(utxt,"MTV") ) )
	{
		printf("Timer NLU: Start to play video...\n");
		timer1.SetHidden=true;
		system("/home/vmplay.sh start &");
	}

	/* General volume control */
	if( strstr(utxt,"声") || strstr(utxt,"音") ) {
		if( strstr(utxt,"大") || strstr(utxt,"高") || strstr(utxt,"增") ) {
			printf("Timer NLU: increase volume...\n");
			//system("/home/mp_vol.sh up &");
			//system("/home/myradio.sh up &");
                        egi_adjust_pcm_volume(+5);

		}
		else if( strstr(utxt,"小") || strstr(utxt,"低") ||  strstr(utxt,"减") ) {
			printf("Timer NLU: decrease volume...\n");
			//system("/home/mp_vol.sh down &");
			//system("/home/myradio.sh down &");
			egi_adjust_pcm_volume(-5);
		}
	}

}

/*--------------------------------------------------
 Write lights to FB according to etimer->SetLights
--------------------------------------------------*/
static void writefb_lights(void)
{
	int i;
	EGI_16BIT_COLOR color;

	for(i=0; i<3; i++) {
		if(timer1.SetLights[i])
			color=light_colors[i];
		else
			color=WEGI_COLOR_DARKGRAY;
                draw_filled_rect2(&gv_fb_dev, color, 185+i*(30+15), 200, 185+30+i*(30+15), 200+30);
	}
}

/*-----------------------------------------
	 Write timer.utxtCmd[]
------------------------------------------*/
static void writefb_utxtCmd(const char *utxt)
{
	if(utxt[0]=='\0')
		return;
	/* Draw a pad */
	draw_blend_filled_rect(&gv_fb_dev, 0, 95, 319, 95+40, WEGI_COLOR_GRAY, 230); /* fbdev, x1, y1, x2, y2, color, alpha */
	/* WriteFB utxt in 2 lines */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        20, 20,(const unsigned char *)utxt,   	/* fw,fh, pstr */
                                        320-10, 2, 2,                           /* pixpl, lines, gap */
                                        10, 95+8,                         		/* x0,y0, */
                                        WEGI_COLOR_BLACK, -1, -1,         	/* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */
}


/*----------------------------
      Timer touch event
----------------------------*/
static void touch_event(void)
{
	int vc=0; 			/* pseudo_curvature */
        static EGI_POINT pts[3];        /* 3 points */
	EGI_TOUCH_DATA touch_data;
	EGI_POINT center={100,120};

	/* center(100,120) r=80 */
	int rad=80;
//	static EGI_IMGBUF *imgbuf_circle=NULL; /* Duration: until end of main() */
//	static int angle=0;
	EGI_IMGBUF *rotimg=NULL;

	#if 0
	if(imgbuf_circle==NULL) {
		imgbuf_circle=egi_imgbuf_newFrameImg( 160, 160,					/* height, width */
                                 		      200,  WEGI_COLOR_PINK,frame_round_rect,	/* alpha, color, imgframe_type */
                                 		      1, &rad ); 				/* pn, *param */
	}
	#else
	if(imgbuf_circle==NULL) {
		imgbuf_circle=egi_imgbuf_readfile("/mmc/linuxpet.jpg");
		if( egi_imgbuf_setFrame(imgbuf_circle, frame_round_rect, 255, 1, &rad) !=0 )
			return;
	}
	#endif

	if( egi_touch_getdata(&touch_data) ) {
		if( touch_data.status==pressed_hold || touch_data.status==pressing ) {

			if( touch_data.status==pressing ) {
	                	/* reset points coordinates */
        	        	pts[2]=pts[1]=pts[0]=(EGI_POINT){ touch_data.coord.x, touch_data.coord.y };
			}

			if( touch_data.status==pressed_hold ) {
	                	/* check circling motion */
        	        	pts[0]=pts[1]; pts[1]=pts[2];
                		pts[2].x=touch_data.coord.x; pts[2].y=touch_data.coord.y;
                		//printf( " pts0(%d,%d) pts1(%d,%d) pts2(%d,%d).\n",pts[0].x, pts[0].y, pts[1].x, pts[1].y,
                        	//                                           pts[2].x, pts[2].y );
                		vc=mat_pseudo_curvature(pts);
                		printf(" vc=%d, dx=%d, dy=%d",vc, touch_data.dx, touch_data.dy);
			}

			/* update img_angle */
			img_angle+=(vc>>6);
				if(img_angle>360) img_angle%=360;

			/* rotate the image */
			rotimg=egi_imgbuf_rotate(imgbuf_circle, img_angle);

			/* map touch XY to pos_rotate FB */
			egi_touch_fbpos_data(&gv_fb_dev, &touch_data);

			/* Now TOUCH_PAD and POS_ROTATE FB are at the same coord, check if touch inside the circle */
			if ( point_incircle( &touch_data.coord, &center, 80)==false ) {
				printf("Out of circe!\n");
				return;
			}
			printf(" 	--- Circle touched ---\n");

			/* center(100,120) r=80 */
			//SLOW: draw_blend_filled_circle(&gv_fb_dev, 100, 120, 80, WEGI_COLOR_TURQUOISE, 200);
			//egi_subimg_writeFB(rotimg, &gv_fb_dev, 0, -1, 100-rotimg->width/2, 120-rotimg->height/2);	 /* imgbuf, fbdev, subnum, subcolor, x0, y0 */

			//egi_imgbuf_free(rotimg);
		}
	}

	/* rotate the image */
	rotimg=egi_imgbuf_rotate(imgbuf_circle, img_angle);

	//SLOW: draw_blend_filled_circle(&gv_fb_dev, 100, 120, 80, WEGI_COLOR_TURQUOISE, 200);
	egi_subimg_writeFB(rotimg, &gv_fb_dev, 0, -1, 100-rotimg->width/2, 120-rotimg->height/2);  /* imgbuf, fbdev, subnum, subcolor, x0, y0 */
	egi_imgbuf_free(rotimg);

}

