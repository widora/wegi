/*-------------------------------------------------------------------------
page creation jobs:
1. egi_create_XXXpage() function.
   1.1 create eboxes and page.
   1.2 assign thread-runner to the page.
   1.3 assign routine to the page.
   1.4 assign button functions to corresponding eboxes in page.
2. Define thread-runner functions.
3. Define egi_XXX_routine() function if not use default egi_page_routine().
4. Define reaction functins for each button, mainly for creating pages.
5. Define touch_slide handler.
5. Group buttons for sliding.

TODO:
	1. different values for button return, and page return,
	   that egi_page_routine() can distinguish.
	2. Pack page activate and free action.

Midas Zhou
-------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h> /* usleep */
#include <errno.h>
#include <sys/wait.h>
#include "egi_common.h"
#include "egi_procman.h"
#include "egi_FTsymbol.h"
#include "egi_pagehome.h"
#include "egi_pagetest.h"
#include "egi_pagemplay.h"
#include "egi_pageopenwrt.h"
#include "egi_pagebook.h"
//#include "egi_pageffplay.h"
#include "iot/egi_iotclient.h"
#include "utils/egi_iwinfo.h"
#include "egi_pagestock.h"

#define CALENDAR_BTN_ID	2
#define PIC_EBOX_ID	1234

#define RUNNER_CPULOAD_ID	0
#define RUNNER_IOTLOAD_ID	1
#define RUNNER_CLOCKTIME_ID	2
#define	RUNNER_WEATHERICON_ID	3

static void display_cpuload(EGI_PAGE *page);
static void display_iotload(EGI_PAGE *page);
static void update_clocktime(EGI_PAGE *page);
static void update_weathericon(EGI_PAGE *page);

static int egi_homebtn_mplay(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int egi_homebtn_openwrt(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int egi_homebtn_book(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int egi_homebtn_test(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int egi_homebtn_ffmusic(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int egi_homebtn_ebook(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int egi_homebtn_stock(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int slide_handler(EGI_PAGE* page, EGI_TOUCH_DATA * touch_data);

static int deco_calendar(EGI_EBOX *ebox);
//static int egi_process_activate_APP(pid_t *apid, char* app_path);


static	EGI_EBOX *home_btns[9*HOME_PAGE_MAX]; 		/* set MAX */
static	EGI_DATA_BTN *data_btns[9*HOME_PAGE_MAX];	/* set MAX */

static	EGI_DATA_TXT *head_txt;
static	EGI_EBOX  *ebox_headbar;

static EGI_DATA_TXT *time_txt;
static EGI_EBOX *time_box;

static EGI_DATA_PIC *data_pic;
static EGI_EBOX *pic_box;


static struct calendar_data
{
	int  km;
	char month[3+1];

	int  kw;
	char week[3+1];

	int  kd;
	char day[2+1];
} caldata={0};

static  int npg=(4 < HOME_PAGE_MAX ? 4 : HOME_PAGE_MAX ); /* set MAX page number */

/* NOTE: 9 buttons for each home page */
static	int nr=2; /* btn row number */
static	int nc=3; /* btn column number */


/*------------- [  PAGE ::   Home Page  ] -------------
create home page
write tag corresponding to each button.

Return
	pointer to a page	OK
	NULL			fail
------------------------------------------------------*/
EGI_PAGE *egi_create_homepage(void)
{
	int k; /*number of pages */
	int i,j;
	int nid;
	int nicode; /* icon code number */
	struct symbol_page *sympg=NULL;
	char strtmp[30];

	/* -------- 1. create button eboxes -------- */
  for(k=0; k<npg; k++) /* page number */
  {
        for(i=0;i<nr;i++) /* row of buttons*/
        {
                for(j=0;j<nc;j++) /* column of buttons */
                {
			nid=k*nr*nc+nc*i+j;

			/* for PAGE 1,2,3 */
			if(k<3) {
				nicode=nid;
				sympg=&sympg_buttons;
			}
			/* for PAGE 4 */
			else {
				nicode=0;   /* TBD later */
				sympg=&sympg_buttons; /* TBD later */
			}

			/* 1.1 create new data_btns */
			data_btns[nid]=egi_btndata_new(nid, /* int id */
							btnType_square, /* enum egi_btn_type shape */
							sympg, /* struct symbol_page *icon */
							nicode, /* int icon_code */
							&sympg_testfont /* for ebox->tag font */
						);
			/* if fail, try again ... */
			if(data_btns[nid]==NULL)
			{
				printf("egi_create_homepage(): fail to call egi_btndata_new() for data_btns[%d]. retry...\n", 3*i+j);
				if(j>0)j--;
				continue;
			}

			/* 2.2 create new btn eboxes */
			home_btns[nid]=egi_btnbox_new(NULL, /* put tag later */
							data_btns[nid], /* EGI_DATA_BTN *egi_data */
				        		1, /* bool movable */
						        15+(15+60)*j + 240*k, /* int x0 */
							90+(15+60)*i, /* int y0 */
							60,60, /* int width, int height */
				       			0, /* int frame, -1 no frame */
		       					-1 /* int prmcolor, no prmcolor */
						   );

			/* if fail, try again ... */
			if(home_btns[nid]==NULL)
			{
				printf("egi_create_homepage(): fail to call egi_btnbox_new() for home_btns[%d]. retry...\n", 3*i+j);
				free(data_btns[nid]);
				data_btns[nid]=NULL;

				if(j>0)j--;
				continue;
			}

			/* set default tag */
			sprintf(strtmp,"btn_%d",nid);
			egi_ebox_settag(home_btns[nid],strtmp);
		}
	}
   }

	/* 1.3 add button tags and reactions here */
	egi_ebox_settag(home_btns[0], "btn_mplayer");
	home_btns[0]->reaction=egi_homebtn_mplay;

	egi_ebox_settag(home_btns[1], "btn_test");
	home_btns[1]->reaction=egi_homebtn_test;
	data_btns[1]->opaque=160;

	egi_ebox_settag(home_btns[2], "btn_alarm");

	egi_ebox_settag(home_btns[3], "FFmusic");
	home_btns[3]->reaction=egi_homebtn_ffmusic;

	egi_ebox_settag(home_btns[4], "btn_key");

	egi_ebox_settag(home_btns[5], "btn_book");
	home_btns[5]->reaction=egi_homebtn_book;

	egi_ebox_settag(home_btns[6], "btn_chart");
	home_btns[6]->reaction=egi_homebtn_stock;

	/* create a bulb */
	egi_ebox_settag(home_btns[7], "bulb_frame");
	data_btns[7]->touch_effect=NULL; /* disable touch effect */

	egi_ebox_settag(home_btns[11], "0");//btn_iot"); /* id=7; as for bulb */
	data_btns[11]->touch_effect=NULL; /* disable touch effect */
	data_btns[11]->icon_code=11; /* SUB_COLOR + ICON_CODE, redefine SUB_COLOR later */
	data_btns[11]->showtag=true; /* display tag */
	data_btns[11]->font=&sympg_numbfont;
	home_btns[11]->tag_color=WEGI_COLOR_ORANGE;
	printf("btns[11]->egi_data->icon_code=%d\n",((EGI_DATA_BTN *)(home_btns[11]->egi_data))->icon_code);

	egi_ebox_settag(home_btns[8], "btn_radio");

	egi_ebox_settag(home_btns[9], "btn_linphone");
	home_btns[9]->reaction=egi_homebtn_openwrt;

	egi_ebox_settag(home_btns[10], "btn_ebook");
	home_btns[10]->reaction=egi_homebtn_ebook;


	/* the 4th page */
	for(i=3*2*3; i<18+6; i++) {
		sprintf(strtmp,"btn_%d",i);
		data_btns[i]->icon_code=9;
		egi_ebox_settag(home_btns[i],strtmp);
	}

        /* --------- 1.4 create a time txt  --------- */
        time_txt=egi_txtdata_new(
                        0,0, /* offset X,Y */
                        1, /*int nl, lines  */
                        64, /*int llen, chars per line, however also limited by ebox width */
                        &sympg_testfont, /*struct symbol_page *font */
                        WEGI_COLOR_WHITE//BLACK /* int16_t color */
	         );
        time_box=egi_txtbox_new(
                        "time_box", /* tag, or put later */
                        time_txt,  /* EGI_DATA_TXT pointer */
                        true, /* bool movable, for refresh */
                        80,40, /* int x0, int y0 */
                        100,20, /* int width;  int height,which also related with symheight,nl and offy */
                        -1, /* int frame, 0=simple frmae, -1=no frame */
                        -1 /*int prmcolor*/
                );

        /* --------- 1.5 create a pic box --------- */
	printf("Create PIC ebox for weather Info...\n");
        /* allocate data_pic */
        data_pic= egi_picdata_new( 0,  0,       /* int offx, int offy */
                                   NULL,        /* EGI_IMGBUF  default 60, 120 */
                                   0,  0,       /* int imgpx, int imgpy */
				   -1,		/* image canvas color, <0 as transparent */
                                   NULL     	/* struct symbol_page *font */
                                );
        /* set pic_box title */
        //data_pic->title="Happy Linux EGI!";
        pic_box=egi_picbox_new( "pic_box", 	/* char *tag, or NULL to ignore */
                                  data_pic,  	/* EGI_DATA_PIC *egi_data */
                                  1,         	/* bool movable */
                                  0, 250, 	/* x0, y0 for host ebox*/
                                  -1,         	/* int frame */
                                  -1		/* int prmcolor,applys only if prmcolor>=0  */
        );
	pic_box->id=PIC_EBOX_ID; /* set ebox ID */


        /* --------- 1.6 set deco for calendar ebox --------- */
	home_btns[CALENDAR_BTN_ID]->method.decorate=deco_calendar;

        /* --------- 2. create home head-bar --------- */
	printf("Create home head-bar...\n");
        /* create head_txt */
        head_txt=egi_txtdata_new(
                0,0, /* offset X,Y */
                1, /*int nl, lines  */
                9, /*int llen, chars per line, 1 for end token! */
                &sympg_icons, /*struct symbol_page *font */
                -1  /* < 0, use default symbol color in img, int16_t color */
        );
        /* fpath == NULL */
        /* create head ebox */
        ebox_headbar= egi_txtbox_new(
                "home_headbar", /* tag */
                head_txt,  /* EGI_DATA_TXT pointer */
                false, /* let home wallpaper take over, bool movable */
                0,0, /* int x0, int y0 */
                240,30, /* int width, int height */
                -1, /* int frame, -1=no frame */
                -1 //egi_colorgray_random(medium)/*int prmcolor, -1 transparent*/
        );
	/* set symbols in home head bar */
	head_txt->txt[0][0]=4;
	head_txt->txt[0][1]=6;
	head_txt->txt[0][2]=10;
	head_txt->txt[0][3]=28; /* 6 is space */
	head_txt->txt[0][4]=6;//33;
	head_txt->txt[0][5]=6;//36;
	head_txt->txt[0][6]=6;
	head_txt->txt[0][7]=6;
	/* !!! the last  MUST end with /0 */

	/* ---------- 3.create home page -------- */
	/* 3.1 create page */
	EGI_PAGE *page_home=NULL;
	page_home=egi_page_new("page_home");
	while(page_home==NULL)
	{
			printf("egi_create_homepage(): fail to call egi_page_new(), try again ...\n");
			page_home=egi_page_new("page_home");
			//usleep(100000);
	}
	/* set bk color, applicable only if fpath==NULL  */
	page_home->ebox->prmcolor=WEGI_COLOR_OCEAN;

	/* 3.2 put pthread runners, remind EGI_PAGE_MAXTHREADS 5  */
	page_home->runner[RUNNER_CPULOAD_ID]=(void *)display_cpuload;
	page_home->runner[RUNNER_IOTLOAD_ID]=(void *)display_iotload;
	page_home->runner[RUNNER_CLOCKTIME_ID]=(void *)update_clocktime;
	page_home->runner[RUNNER_WEATHERICON_ID]=(void *)update_weathericon; //egi_iotclient;

	/* 3.3 set default routine job */
	page_home->routine=egi_homepage_routine;

	/* 3.4 set wallpaper */
	page_home->fpath="/home/home.jpg";

	/* 3.5 set touch sliding handler */
	page_home->slide_handler=slide_handler;

	/* add ebox to home page */
	/* beware of the sequence of the ebox list */
	for(i=0;i<npg*nr*nc;i++)
		egi_page_addlist(page_home, home_btns[i]);

	//turn_off bulb bkimg!	egi_page_addlist(page_home, bkimg_btn7);
	egi_page_addlist(page_home, ebox_headbar);
	egi_page_addlist(page_home, time_box);
	egi_page_addlist(page_home, pic_box);
//	egi_page_addlist(page_home, calendar_box);

	return page_home;
}

/*----------------  RUNNER 1 ----------------------
display cpu load in home head-bar with motion icons

1. read /proc/loadavg to get the value
	loadavg 1-6,  >5 alarm
2. corresponding symmic_cpuload[] index from 0-5.
-------------------------------------------------*/
static void display_cpuload(EGI_PAGE *page)
{
	int load=0;
	int fd;
	char strload[5]={0}; /* read in 4 byte */

        /* open symbol image file */
        fd=open("/proc/loadavg", O_RDONLY|O_CLOEXEC);
        if(fd<0)
        {
                printf("display_cpuload(): fail to open /proc/loadavg!\n");
                perror("display_cpuload()");
                return;
        }

	EGI_PDEBUG(DBG_PAGE,"page '%s':  runner thread display_cpuload() is activated!.\n",page->ebox->tag);
	while(1)
	{
		lseek(fd,0,SEEK_SET);
		read(fd,strload,4);
		//printf("----------------- strload: %s -----------------\n",strload);
		load=atoi(strload);/* for symmic_cpuload[], index from 0 to 5 */
		if(load>5)
			load=5;
		//printf("----------------- load: %d -----------------\n",load);
		/* load cpuload motion icons
		 *	  symbol_motion_string() is with---sleep---function inside
		 * WARNING: use global FB dev may cause rase condition
		 */
  	 	symbol_motion_string(&gv_fb_dev, 155-load*15, &sympg_icons,
		 					1, 210,0, &symmic_cpuload[load][0]);

		/* handler for signal_suspend, wait until runner_cond comes */
		egi_runner_sigSuspend_handler(page);
	}
}

/*--------------  RUNNER 2 -----------------
Display IoT motion icon.
Display RSSI icon.
------------------------------------------*/
static void display_iotload(EGI_PAGE *page)
{
	int rssi;
	int index; /* index for RSSI of  sympg_icons[index]  */

	EGI_PDEBUG(DBG_PAGE,"page '%s':  runner thread display_iotload() is activated!.\n"
										,page->ebox->tag);
	while(1)
	{
		/* 1. load IoT motion icons
			  symbol_motion_string() is with sleep function */
  	 	symbol_motion_string(&gv_fb_dev, 120, &sympg_icons, 1, 180,0, symmic_iotload);

		/* 2. get RSSI value */
		iw_get_rssi(&rssi);
		if(rssi > -65) index=5;
		else if(rssi > -73) index=4;
		else if(rssi > -80) index=3;
		else if(rssi > -94) index=2;
		else 	index=1;
		//EGI_PDEBUG(DBG_PAGE,"egi_display_itoload(): rssi=%d; index=%d \n",rssi,index);

		/* 3. draw RSSI symbol
		 * WARNING: use global FB dev may cause rase condition
		 */
		symbol_writeFB(&gv_fb_dev, &sympg_icons, SYM_NOSUB_COLOR, 0, 0, 0, index, 0);/*bkcolor=0*/

		/* handler for signal_suspend ( if get suspend signal, wait until runner_cond comes) */
		egi_runner_sigSuspend_handler(page);
	}
}


/*-----------------  RUNNER 3 --------------------------
Update time tag for the home_clock,but do NOT refresh,
and update caldata for calender btn decoration.
Let page routine do refreshing.

TODO: Use NTPC to update time.
-------------------------------------------------------*/
static void update_clocktime(EGI_PAGE *page)
{
	char strtm[32];

        time_t tm_t; /* time in seconds */
        struct tm *tm_s; /* time in struct */

	EGI_PDEBUG(DBG_PAGE,"page '%s':  runner thread update_clocktime() is activated!.\n"
										,page->ebox->tag);
	/* put init string in caldata */
        time(&tm_t);
        tm_s=localtime(&tm_t);
	caldata.km=tm_s->tm_mon;
	caldata.kw=tm_s->tm_wday;
	caldata.kd=tm_s->tm_mday;

//        snprintf((char *)(caldata.month),3+1,"%s", str_month[tm_s->tm_mon] );
//        snprintf((char *)caldata.week,3+1,"%s", str_weekday[tm_s->tm_wday]);
//        snprintf((char *)(caldata.day),2+1,"%d", tm_s->tm_mday);

	/* refresh to trigger deco_calender() */
	egi_ebox_needrefresh(home_btns[CALENDAR_BTN_ID]);

	/* loop updating data */
	while(1) {
		/* get time string for 24H Clock ebox */
		tm_get_strtime(strtm);

		/* update 24H Clock time box txt */
		egi_push_datatxt(time_box, strtm, NULL);
		egi_ebox_needrefresh(time_box);

		/* update Calendar month/weeday/day index every minute */
		if(atoi(strtm+6)==0) {
		        //time(&tm_t);
		        //tm_s=localtime(&tm_t);
		        //snprintf((char *)caldata.month,3+1,"%s", str_month[tm_s->tm_mon]);
		        //snprintf((char *)caldata.week,3+1,"%s", str_weekday[tm_s->tm_wday]);
		        //snprintf((char *)caldata.day,2+1,"%d", tm_s->tm_mday);
		        time(&tm_t);
		        tm_s=localtime(&tm_t);
			caldata.km=tm_s->tm_mon;
			caldata.kw=tm_s->tm_wday;
			caldata.kd=tm_s->tm_mday;

			egi_ebox_needrefresh(home_btns[CALENDAR_BTN_ID]);
		}

		egi_sleep(0,0,500);
		//printf("----[ %s-%s  %s ]------\n", caldata.month, caldata.day,strtm);

		/* handler for signal_suspend ( if get suspend signal, wait until runner_cond comes) */
		egi_runner_sigSuspend_handler(page);
	}
}


/*----------------------------------
Ebox deco function for calendar.
-----------------------------------*/
static int deco_calendar(EGI_EBOX *ebox)
{
	int x0,y0;
	int pixlen;
	int fw,fh;
	char strtm[32];

	/* ----- Use UFT-8 encoding FT fonts ----- */
	if( egi_sysfonts.regular != NULL ) {
		/* Month */
		fw=15; fh=15;
 		pixlen=FTsymbol_uft8strings_pixlen( egi_sysfonts.regular, fw, fh, stru8_month[caldata.km]);
	        x0=ebox->x0+((60-pixlen)>>1);
	        y0=ebox->y0;
		FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_sysfonts.regular, 	 /* FBdev, fontface */
                                                fw, fh, stru8_month[caldata.km], /* fw,fh, pstr */
                                                60, 1, 0,                        /* pixpl, lines, gap */
                                                x0, y0,                          /* x0,y0, */
                                                WEGI_COLOR_BLACK, -1, -1,        /* fontcolor, transcolor,opaque */
						NULL, NULL, NULL, NULL );
		/* Week */
		fw=15; fh=15;
	 	pixlen=FTsymbol_uft8strings_pixlen( egi_sysfonts.regular, fw, fh, stru8_weekday[caldata.kw]);
        	x0=ebox->x0+((60-pixlen)>>1);
	        y0=ebox->y0+42;
		FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_sysfonts.regular, 	 /* FBdev, fontface */
                                                fw, fh, stru8_weekday[caldata.kw], /* fw,fh, pstr */
                                                60, 1, 0,                      /* pixpl, lines, gap */
                                                x0, y0,                          /* x0,y0, */
                                                WEGI_COLOR_BLACK, -1, -1,      /* fontcolor, transcolor,opaque */
						NULL, NULL, NULL, NULL );
	}
	/* ----- Use ASCII sympg ----- */
	else {
		/* Month */
//		pixlen=symbol_string_pixlen(caldata.month, &sympg_testfont);
		pixlen=symbol_string_pixlen(str_month[caldata.km], &sympg_testfont);
		x0=ebox->x0+((60-pixlen)>>1);
		y0=ebox->y0-5;
		symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, WEGI_COLOR_BLACK,
					SYM_FONT_DEFAULT_TRANSPCOLOR, x0, y0, str_month[caldata.km], -1);
		/* Week */
		pixlen=symbol_string_pixlen(str_weekday[caldata.kw], &sympg_testfont);
		x0=ebox->x0+((60-pixlen)>>1);
		y0=ebox->y0+40;
		symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, WEGI_COLOR_BLACK,
					SYM_FONT_DEFAULT_TRANSPCOLOR, x0, y0, str_weekday[caldata.kw], -1);
	}
	/* Day */
        snprintf((char *)(caldata.day),2+1,"%d", caldata.kd);
	pixlen=symbol_string_pixlen(caldata.day, &sympg_numbfont);
	x0=ebox->x0+((60-pixlen)>>1);
	y0=ebox->y0+22;
	symbol_string_writeFB(&gv_fb_dev, &sympg_numbfont, WEGI_COLOR_BLACK,
				SYM_FONT_DEFAULT_TRANSPCOLOR, x0, y0, caldata.day, -1);

	return 0;
}


/*-----------------  RUNNER 4 --------------------------
Update heweather data
-------------------------------------------------------*/
static void update_weathericon(EGI_PAGE *page)
{
   int 		i, j;
   unsigned int off;
   EGI_EBOX 	*ebox=NULL;
   EGI_IMGBUF   *eimg=NULL;
   EGI_IMGBUF 	*picimg=NULL;
   int		subindex;
   int		subnum;
   FBDEV	vfb;
   char heweather_path[]="/tmp/.egi/heweather/now.png";

//   subnum=3;
   subindex=0;

   /* HTTPS GET HeWeather Data periodically */
   while(1) {

	/* fetch PIC ebox for heweather, in case it has not yet been loaded. */
	ebox=egi_page_pickebox(page, type_pic, PIC_EBOX_ID);
	if(ebox == NULL) {
		printf("%s: egi_page_pickebox() return NULL. sleep and retry later.\n",__func__);
		goto SLEEP_WAITING;
	}

	/* allocate eimg, the ownership will be deprived by egi_picbox_renewimg() later */
	if(eimg==NULL) {
//		printf("eimg=egi_imgbuf_alloc()!\n");
		eimg=egi_imgbuf_alloc(); //new()
	}

        /* load weather icon png file */
        if( egi_imgbuf_loadpng( heweather_path, eimg) !=0 ) {   /* mutex inside */
                printf("%s: Fail to loadpng at %s!\n", __func__, heweather_path);
		goto SLEEP_WAITING;
        }
	else{
//		printf("%s: Succeed to load PNG icon: height=%d, width=%d \n",__func__,
//							eimg->height, eimg->width);

		/* get subimg number */
		subnum=eimg->height/60;

		/* set subimg */
		eimg->submax=subnum-1; /* 2 image data */
		eimg->subimgs=calloc(subnum,sizeof(EGI_IMGBOX));
		for(i=0; i<subnum; i++)
			eimg->subimgs[i]=(EGI_IMGBOX){0,i*60,240,60};
	}

	/* TEST HERE <---------- init Virt FB */
	picimg=egi_imgbuf_alloc();//new();
	egi_imgbuf_init(picimg,60,240);
	init_virt_fbdev(&vfb, picimg);

	/* put subimg to picimg */
	subindex++;
	subindex=subindex%subnum;
	egi_subimg_writeFB(eimg, &vfb, subindex, -1, 0, 0);

	/* renew image in PIC box,old data deleted firt. !!!ownership of imgbuf transfered!!! */
	if(egi_picbox_renewimg(ebox, &picimg)!=0) {
		printf("%s: Fail to renew imgbuf for PIC ebox.\n",__func__);
		goto SLEEP_WAITING;
	}else{
//		printf("%s: egi_ebox_needrefresh() pic box...\n",__func__);
		egi_ebox_needrefresh(ebox);
	}

	/* TEST LOOP --------> release virt FB */
	egi_imgbuf_free(picimg);
	release_virt_fbdev(&vfb);

SLEEP_WAITING:
//	printf("%s: Start Delay or Sleep ....\n",__func__);
	egi_sleep(0,5,0); /* 3s */

	/* handler for signal_suspend, If get suspend_signal,wait until runner_cond comes.
	 * !!! Meaningless, since the period of thread loop is too big!
	 */
	egi_runner_sigSuspend_handler(page);
  }

}


/*----------------------------------------------
objet defined routine jobs of home page

1. see default egi_page_routine() in egi_page.c
-----------------------------------------------*/
void egi_home_routine(void)
{
 	/* 1. check wifi signal stress */

	/* 2. check CPU load */

	/* 3. check MQTT connections */

	/* 4. update home_page head-bar icons */
}


/*-----------------------------------------------------------------
			button  MPLAY
------------------------------------------------------------------*/
static int egi_homebtn_mplay(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	/* bypass unwanted touch status */
	if(touch_data->status != pressing)
		return btnret_IDLE;

	/* suspend runner CPULOAD */
	egi_suspend_runner(ebox->container, RUNNER_CPULOAD_ID);
	egi_suspend_runner(ebox->container, RUNNER_IOTLOAD_ID);

	/* create page and load the page */
        EGI_PAGE *page_mplay=egi_create_mplaypage();
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is created.", page_mplay->ebox->tag);

	/* activate to display it */
        egi_page_activate(page_mplay);
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is activated.", page_mplay->ebox->tag);

	/* get into routine loop */
	EGI_PLOG(LOGLV_INFO,"Now trap into routine of [page '%s']...", page_mplay->ebox->tag);
        page_mplay->routine(page_mplay);

	/* get out of routine loop */
	EGI_PLOG(LOGLV_INFO,"Exit routine of [page '%s'], start to free the page...", page_mplay->ebox->tag);
	egi_page_free(page_mplay);

	/* resume runner CPULOAD */
	egi_resume_runner(ebox->container, RUNNER_CPULOAD_ID);
	egi_resume_runner(ebox->container, RUNNER_IOTLOAD_ID);

	return pgret_OK;
}

/*-------------------------------------------------------------------
			button OPENWRT
--------------------------------------------------------------------*/
static int egi_homebtn_openwrt(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	/* bypass unwanted touch status */
	if(touch_data->status != pressing)
		return btnret_IDLE;

	/* create page and load the page */
        EGI_PAGE *page_openwrt=egi_create_openwrtpage();
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is created.", page_openwrt->ebox->tag);

	/* activate to display it */
        egi_page_activate(page_openwrt);
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is activated.", page_openwrt->ebox->tag);

	/* get into routine loop */
	EGI_PLOG(LOGLV_INFO,"Now trap into routine of [page '%s']...", page_openwrt->ebox->tag);
        page_openwrt->routine(page_openwrt);

	/* get out of routine loop */
	EGI_PLOG(LOGLV_INFO,"Exit routine of [page '%s'], start to free the page...", page_openwrt->ebox->tag);
	egi_page_free(page_openwrt);

	return pgret_OK;
}

/*---------------------------------------------------------------------
			button  TXTBOOK
----------------------------------------------------------------------*/
static int egi_homebtn_book(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{

	/* bypass unwanted touch status */
	if(touch_data->status != pressing)
		return btnret_IDLE;

	/* create page and load the page */
        EGI_PAGE *page_book=egi_create_bookpage();
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is created.", page_book->ebox->tag);

	/* activate to display it */
        egi_page_activate(page_book);
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is activated.", page_book->ebox->tag);

	/* get into routine loop */
	EGI_PLOG(LOGLV_INFO,"Now trap into routine of [page '%s']...", page_book->ebox->tag);
        page_book->routine(page_book);

	/* get out of routine loop */
	EGI_PLOG(LOGLV_INFO,"Exit routine of [page '%s'], start to free the page...", page_book->ebox->tag);
	egi_page_free(page_book);

	return pgret_OK;
}

/*------------------------------------------------------------------
			button TEST
-------------------------------------------------------------------*/
static int egi_homebtn_test(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	/* bypass unwanted touch status */
	if(touch_data->status != pressing)
		return btnret_IDLE;

	/* create page and load the page */
        EGI_PAGE *page_test=egi_create_testpage();
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is created.", page_test->ebox->tag);

        egi_page_activate(page_test);
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is activated.", page_test->ebox->tag);

	/* get into routine loop */
	EGI_PLOG(LOGLV_INFO,"Now trap into routine of [page '%s']...", page_test->ebox->tag);
        page_test->routine(page_test);

	/* get out of routine loop */
	EGI_PLOG(LOGLV_INFO,"Exit routine of [page '%s'], start to free the page...", page_test->ebox->tag);
	egi_page_free(page_test);

	return pgret_OK;
}

/*--------------------------------------------------------------------
			button FFPLAY
----------------------------------------------------------------------*/
static int egi_homebtn_ffmusic(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	int status;
	int ret;
	static pid_t pid_ffmusic=-1;

	/* bypass unwanted touch status */
	if( touch_data->status != pressing )
		return btnret_IDLE;

	/* suspend runner CPULOAD */
	egi_suspend_runner(ebox->container, RUNNER_CPULOAD_ID);
	egi_suspend_runner(ebox->container, RUNNER_IOTLOAD_ID);

#if 0////////////// (   OLD CODES  ) ////////////////
	/* create page and load the page */
        EGI_PAGE *page_ffplay=egi_create_ffplaypage();
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is created.", page_ffplay->ebox->tag);

        egi_page_activate(page_ffplay);
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is activated.", page_ffplay->ebox->tag);

	/* get into routine loop */
	EGI_PLOG(LOGLV_INFO,"Now trap into routine of [page '%s']...", page_ffplay->ebox->tag);
        page_ffplay->routine(page_ffplay);

//	/* exit a button activated page, set refresh flag for the host page, before page freeing.*/
//	egi_page_needrefresh(ebox->container); pgret_OK will set refresh

	/* get out of routine loop */
	EGI_PLOG(LOGLV_INFO,"Exit routine of [page '%s'], start to free the page...",
									page_ffplay->ebox->tag);
	egi_page_free(page_ffplay);

#endif

   	/* TODO: SIGCHLD handler for exiting child processes
	 *	NOTE: STOP/CONTINUE/EXIT of a child process will produce signal SIGCHLD!
    	 */

	/* activate APP and wait untill it STOP or TERM */
        egi_process_activate_APP(&pid_ffmusic, "/home/app_ffmusic");

	/* resume runner CPULOAD */
	egi_resume_runner(ebox->container, RUNNER_CPULOAD_ID);
	egi_resume_runner(ebox->container, RUNNER_IOTLOAD_ID);


	return pgret_OK; /* to refresh PAGE anyway */
}

/*--------------------------------------------------------------------
			button EBOOK
----------------------------------------------------------------------*/
static int egi_homebtn_ebook(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	int status;
	int ret;
	static pid_t pid_ebook=-1;

	/* bypass unwanted touch status */
	if( touch_data->status != pressing )
		return btnret_IDLE;

	/* activate APP and wait untill it SOTP or TERM */
        egi_process_activate_APP(&pid_ebook, "/home/app_ebook");

	return pgret_OK; /* to refresh PAGE anyway */
}


/*----------------------------------------------------------------------------
			button stock chart
-----------------------------------------------------------------------------*/
static int egi_homebtn_stock(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	/* bypass unwanted touch status */
	if(touch_data->status != pressing)
		return btnret_IDLE;

	/* create page and load the page */
        EGI_PAGE *page_stock=egi_create_pagestock();
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is created.", page_stock->ebox->tag);

        egi_page_activate(page_stock);
	EGI_PLOG(LOGLV_INFO,"[page '%s'] is activated.", page_stock->ebox->tag);

	/* get into routine loop */
	EGI_PLOG(LOGLV_INFO,"Now trap into routine of [page '%s']...", page_stock->ebox->tag);
        page_stock->routine(page_stock);

	/* get out of routine loop */
	EGI_PLOG(LOGLV_INFO,"Exit routine of [page '%s'], start to free the page...", page_stock->ebox->tag);
	egi_page_free(page_stock);

	return pgret_OK;
}


/*-------------------------------------------------------------------
			SLIDE HANDLER
---------------------------------------------------------------------*/
static int slide_handler(EGI_PAGE* page, EGI_TOUCH_DATA * touch_data)
{
        static int mark[9*HOME_PAGE_MAX]; /* >= nc*nr */
	int ns; /* start grid: 0,1,2,... */
	int step=0;
	int limit;
	int ntp;
	//int token=0;
	int tmp;
	enum egi_touch_status status;
        int i,j,k,m;
	//EGI_TOUCH_DATA touch;

	status=touch_data->status;

        /* 1. set mark when press down, !!!! egi_touch_getdata() may miss this status !!! */
        if(status==pressing)
        {
                printf("active homepage slide touch handler....\n");
		for(i=0;i<npg*nc*nr;i++)
	                mark[i]=home_btns[i]->x0;

		return btnret_OK; /* do not refresh page, or status will be cut to release_hold */
        }

	/* 2. adjust button position and refresh */
        else if( status==pressed_hold )
	{

#if 1  /* SLIDE_ALGRITHM 1: slide by dx value */
		step=tmp;
		/* check PAGE_0 RIGHT POSITION limit */
		limit=mark[0] + (touch_data->dx<<1);
		if( limit > 160 || limit < -3*240 - 160 )
			return btnret_OK;

		for(i=0;i<npg*nc*nr;i++) {
			/* check limit */
			home_btns[i]->x0 = mark[i] + touch_data->dx + (touch_data->dx>>1); /* x2 accelerate sliding speed */
			egi_ebox_needrefresh(home_btns[i]);
		}

    #if 0   /* Test  loopback */
	/* Pending !!!!: X positon of pages are NOT continous any more!!!  */
		for(i=0;i<npg;i++) {
			if(home_btns[i*nc*nr]->x0 < -240*3 ) { /* 3,NOT 4! */
				for(j=0;j<nc*nr;j++)
					home_btns[i*nc*nr+j]->x0 += 240*4;
			}
			else if( home_btns[i*nc*nr+nc-1]->x0 + home_btns[0]->width > 240*4-1 ) {
				for(j=0;j<nc*nr;j++)
					home_btns[i*nc*nr+j]->x0 -= 240*4;
			}
		}
    #endif   /* end loop test */

		/* group refresh to avoid bkimg interference */
		egi_btngroup_refresh(home_btns, npg*nc*nr);
		tm_delayms(75);


#else  /* SLIDE_ALGRITHM 2:  auto slide if dx great that a threshold value */
		if( touch_data->dx > 15 )
			token=1;
		else  if(touch_data->dx < -15 )
			token=-1;
		else
			token=0;

		if(token != 0) {
	    	    for(step=1; step < 240/40+1; step++) {
			for(i=0;i<npg*nc*nr;i++) {
				home_btns[i]->x0 = mark[i] + token*step*40; //( touch_data->dx << 1); /* x2 accelerate sliding speed */
				//if( home_btns[i]->x0 > (0-home_btns[i]->width) && home_btns[i]->x0 < 240 )
				egi_ebox_needrefresh(home_btns[i]);
			}
			/* group refresh to avoid bkimg interference */
			egi_btngroup_refresh(home_btns, npg*nc*nr);
			tm_delayms(75);
		   }

		   /* wait for release_hold to pass following pressed_hold status */
		   do {
                   	tm_delayms(55);
                   	egi_touch_getdata(&touch);
                   }while(touch.status != released_hold);

		}
#endif

		return btnret_OK;
	}


	/* 3. re_align all btns in 2X3 grids OR re_align to a PAGE  */
	else if( status==releasing )
   	{
		/* align X0 to PAGE */
		ns=( home_btns[0]->x0 - 100)/240; /* 80 threshold */

		/* motion effect: take m steps to move buttons to the position
	         */
		step=20; /* move 10 pixels per step */
		ntp= ( (ns*240+15) - home_btns[0]->x0 )/step;

		if(ntp<0) {
			ntp=-ntp;
			step=-step;
		}
		if(ntp>0){
			for(m=1; m<=ntp; m++) {
				for(i=0; i<npg*nr*nc; i++) {
					home_btns[i]->x0 += step;
					egi_ebox_needrefresh(home_btns[i]);
				}
				egi_btngroup_refresh(home_btns, npg*nc*nr);
				tm_delayms(35);
			}
		}

#if 1
		/* final position */
		for(k=0; k<npg; k++) {
			for(i=0; i<nr; i++) {
				for(j=0; j<nc; j++) {
					home_btns[k*nc*nr+i*nc+j]->x0=15+(15+60)*j+(k+ns)*240;
					egi_ebox_needrefresh(home_btns[k*nc*nr+i*nc+j]);
				}
			}
		}
		/* group refresh to avoid bkimg interference */
		egi_btngroup_refresh(home_btns, npg*nc*nr);
#endif

		return btnret_OK; /* refresh page */
   	}

   	else /* bypass unwanted touch status */
              return btnret_IDLE;
}


#if 0 /////////////////////////////////////////////////////////////
/*-----------------------------------------------------------
Activate an APP and wait for its state to change.

1. If apid<0 then vfork() and excev() the APP.
2. If apid>0, try to send SIGCONT to activate it.
3. Wait for state change for the apid, that means the parent
   process is hung up until apid state changes(STOP/TERM).

@apid:		PID of the subprocess.
@app_path:	Path to an executiable APP file.

Return:
	<0	Fail to execute or send signal to the APP.
	0	Ok, succeed to execute APP, and get the latest
		changed state.
-------------------------------------------------------------*/
static int egi_process_activate_APP(pid_t *apid, char* app_path)
{
	int status;
	int ret;

	EGI_PDEBUG(DBG_PAGE,"------- start check accessibility of the APP --------\n");
	if( access(app_path, X_OK) !=0 ) {
		EGI_PDEBUG(DBG_PAGE,"'%s' is not a recognizble executive file.\n", app_path);
		return -1;
	}

   	/* TODO: SIGCHLD handler for exiting child processes
	 *	NOTE: STOP/CONTINUE/EXIT of a child process will produce signal SIGCHLD!
    	 */

	/* 1. If app_ffplay not running, fork() and execv() to launch it. */
  	if(*apid<0) {

		EGI_PDEBUG(DBG_PAGE,"----- start fork APP -----\n");
		*apid=vfork();

		/* In APP */
		if(*apid==0) {
			EGI_PDEBUG(DBG_PAGE,"----- start execv APP -----\n");
			execv(app_path,NULL);
			/* Warning!!! if fails! it still holds whole copied context data!!! */
               		EGI_PLOG(LOGLV_ERROR, "%s: fail to execv '%s' after fork(), error:%s",
								__func__, app_path, strerror(errno) );
			exit(255); /* 8bits(255) will be passed to parent */
		}
		/* In the caller's context */
		else if(*apid <0) {
               		EGI_PLOG(LOGLV_ERROR, "%s: Fail to launch APP '%s'!",__func__, app_path);
			return -2;
		}
		else {
        	       	EGI_PLOG(LOGLV_CRITICAL, "%s: APP '%s' launched successfully!\n",
										__func__, app_path);
		}
    	}
    	/* 2. Else, assume APP is hungup(stopped), send SIGCONT to activate it. */
    	else {
        	       	EGI_PLOG(LOGLV_CRITICAL, "%s: send SIGCONT to activate '%s'[pid:%d].",
										__func__, app_path, *apid);
			if( kill(*apid,SIGCONT)<0 ) {
				EGI_PLOG(LOGLV_ERROR, "%s: Fail to send SIGCONT to '%s'[pid:%d].",
										__func__, app_path, *apid);
				return -3;
			}
    	}

    	/* 3. Wait for status of APP to change */
    	if(*apid>0) {
		/* wait for status changes,a state change is considered to be:
		 * 1. the child is terminated;
		 * 2. the child is stopped by a signal;
		 * 3. or the child is resumed by a signal.
		 */
		waitpid(*apid, &status, WUNTRACED); /* block |WCONTINUED */

		/* 1. Exit normally */
	       	if(WIFEXITED(status)){
                	ret=WEXITSTATUS(status);
               		EGI_PLOG(LOGLV_CRITICAL, "%s: APP '%s' exits with ret value: %d.",
									__func__, app_path, ret);
			*apid=-1; /* retset */
	       	}
		/* 2. Terminated by signal, in case APP already terminated. */
	       	else if( WIFSIGNALED(status) ) {
                	EGI_PLOG(LOGLV_CRITICAL, "%s: APP '%s' terminated by signal number: %d.",
								__func__, app_path, WTERMSIG(status));
			*apid=-1; /* reset */
        	}
		/* 3. Stopped */
		else if(WIFSTOPPED(status)) {
	        	        EGI_PLOG(LOGLV_CRITICAL, "%s: APP '%s' is just STOPPED!",
										 __func__, app_path);
		}
		/* 4. Other status */
		else {
        	        EGI_PLOG(LOGLV_WARN, "%s: APP '%s' returned with unparsed status=%d",
									__func__, app_path, status);
			*apid=-1; /* reset */
		}
     	}

	return 0;
}

#endif ///////////////////////////////////////////////////////////////////

