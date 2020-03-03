/*-------------------------------------------------------------------
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


                (((  --------  PAGE DIVISION  --------  )))
[Y0-Y29]
{0,0},{240-1, 29}               ---  Head title bar

//[Y150-Y265]
//{0,150}, {240-1, 260}           --- box area for subtitle display

[Y266-Y319]
{0,266}, {240-1, 320-1}         --- Buttons


TODO:


Midas Zhou
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "egi_common.h"
#include "sound/egi_pcm.h"
#include "egi_FTsymbol.h"
#include "ffmotion.h"
#include "page_ffmotion.h"

/* icon code for button symbols */
#define ICON_CODE_PREV 		0
#define ICON_CODE_PAUSE 	1
#define ICON_CODE_STOP		10
#define ICON_CODE_PLAY 		3
#define ICON_CODE_NEXT 		2
#define ICON_CODE_EXIT 		5
#define ICON_CODE_SHUFFLE	6	/* pick next file randomly */
#define ICON_CODE_REPEATONE	7	/* repeat current file */
#define ICON_CODE_LOOPALL	8	/* loop all files in the list */
#define ICON_CODE_GOHOME	9

/* Control button IDs */
#define BTN_ID_PREV		0
#define BTN_ID_PLAYPAUSE	1
#define BTN_ID_NEXT		2
#define BTN_ID_EXIT		3	/* SLEEP */
#define BTN_ID_PLAYMODE		4	/* PLAYMODE or EXIT  */

/* Timer slider and txt ID, global */
//#define TIME_SLIDER_ID	100
//#define TIME_TXT0_ID		101
//#define TIME_TXT1_ID		102

static EGI_BOX slide_zone={ {0,0}, {239,230} }; /* Sliding zone for volume adjusting */
static uint16_t btn_symcolor;

/* buttons */
static int btnum=5;
static EGI_DATA_BTN 	*data_btns[5];
static EGI_EBOX 	*ffmot_btns[5];

/* btn group */
static EGI_EBOX		*ebox_group[8];

/* title txt ebox */
static EGI_DATA_TXT	*title_FTtxt;
static EGI_EBOX		*ebox_title;

/* circling/sliding pad for volume adjust */
static EGI_DATA_BTN 	*data_circlePad;
static EGI_EBOX		*ebox_circlePad;

/* touch pad for btn scroll */
static EGI_DATA_BTN 	*data_touchPad;
static EGI_EBOX		*ebox_touchPad;

/* volume txt ebox */
static EGI_DATA_TXT 	*vol_FTtxt;
static EGI_EBOX     	*ebox_voltxt;

/* Timing slider */
static EGI_DATA_BTN 	*data_slider;
static EGI_EBOX 	*time_slider;

/* Time sliding bar txt, for time elapsed and duration */
static EGI_DATA_TXT 	*data_tmtxt[2];
static EGI_EBOX	    	*ebox_tmtxt[2];
static int 		tmSymHeight=18;              /* symbol height */
static int 		tmX0[2], tmY0[2];            /* txt EBOX x0,y0  */


/* static functions */
static int ffmot_prev(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int ffmot_playpause(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int ffmot_next(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int ffmot_playmode(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int ffmot_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
//static int pageFFmot_decorate(EGI_EBOX *ebox);
static int sliding_timebar(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int sliding_volume(EGI_PAGE* page, EGI_TOUCH_DATA * touch_data);
//static int circling_volume(EGI_PAGE* page, EGI_TOUCH_DATA * touch_data);
static int circling_volume(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int scroll_btns(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int refresh_misc(EGI_PAGE *page);


/*---------- [  PAGE ::  FF MUSIC PLAYER ] ---------
1. create eboxes for 4 buttons and 1 title bar

Return
	pointer to a page	OK
	NULL			fail
------------------------------------------------------*/
EGI_PAGE *create_ffmotionPage(void)
{
	int i;

	/* --------- 1. create buttons --------- */
        for(i=0;i<btnum;i++) /* row of buttons*/
        {
		/* 1. create new data_btns */
		data_btns[i]=egi_btndata_new(	i, 		  /* int id */
						btnType_square,   /* enum egi_btn_type shape */
						&sympg_sbuttons,  /* struct symbol_page *icon. If NULL, use geometry. */
						0, 		  /* int icon_code, assign later.. */
						&sympg_testfont   /* for ebox->tag font */
					);
		/* if fail, try again ... */
		if(data_btns[i]==NULL) {
			EGI_PLOG(LOGLV_ERROR,"egi_create_ffplaypage(): fail to call egi_btndata_new() for data_btns[%d]. retry...", i);
			i--;
			continue;
		}

		/* Do not show tag on the button */
//default	data_btns[i]->showtag=false;

		/* 2. create new btn eboxes */
		ffmot_btns[i]=egi_btnbox_new(  NULL, 		  /* put tag later */
						data_btns[i],     /* EGI_DATA_BTN *egi_data */
				        	1, 		  /* bool movable */
					        48*i, 320-48, /* int x0, int y0 */
						48, 48, 	  /* int width, int height */
				       		-1, 		  /* int frame,<0 no frame */
		       				WEGI_COLOR_GRAY  /*int prmcolor, for geom button only. */
					   );
		/* if fail, try again ... */
		if(ffmot_btns[i]==NULL) {
			printf("egi_create_ffplaypage(): fail to call egi_btnbox_new() for ffmot_btns[%d]. retry...\n", i);
			free(data_btns[i]);
			data_btns[i]=NULL;
			i--;
			continue;
		}
	}


	/* get a random color for the icon */
//	btn_symcolor=egi_color_random(color_medium);
//	EGI_PLOG(LOGLV_INFO,"%s: set 24bits btn_symcolor as 0x%06X \n",	__FUNCTION__, COLOR_16TO24BITS(btn_symcolor) );
	btn_symcolor=WEGI_COLOR_GRAYC; //WHITE;//BLACK;//ORANGE;

	/* add tags, set icon_code and reaction function here */
	egi_ebox_settag(ffmot_btns[0], "Prev");
	data_btns[0]->icon_code=(btn_symcolor<<16)+ICON_CODE_PREV; /* SUB_COLOR+CODE */
	ffmot_btns[0]->reaction=ffmot_prev;

	egi_ebox_settag(ffmot_btns[1], "Play&Pause");
	data_btns[1]->icon_code=(btn_symcolor<<16)+ICON_CODE_PAUSE; /* default status is playing*/
	ffmot_btns[1]->reaction=ffmot_playpause;

	egi_ebox_settag(ffmot_btns[2], "Next");
	data_btns[2]->icon_code=(btn_symcolor<<16)+ICON_CODE_NEXT;
	ffmot_btns[2]->reaction=ffmot_next;

	egi_ebox_settag(ffmot_btns[3], "Exit");
	data_btns[3]->icon_code=(btn_symcolor<<16)+ICON_CODE_EXIT;
	ffmot_btns[3]->reaction=ffmot_exit;

	egi_ebox_settag(ffmot_btns[4], "Playmode");
	data_btns[4]->icon_code=(btn_symcolor<<16)+ICON_CODE_LOOPALL;
	ffmot_btns[4]->reaction=ffmot_playmode;


	/* --------- 2.1 create a circling/sliding pad ebox for volume adjust  --------- */
	data_circlePad=egi_btndata_new( 111, 		  /* int id */
				  btnType_square, /* enum egi_btn_type shape */
				  NULL,  	  /* struct symbol_page *icon. If NULL, use geometry. */
				  0, 		  /* int icon_code, assign later.. */
				  NULL   	  /* for ebox->tag font */
				);

	if(data_circlePad != NULL) {
		data_circlePad->touch_effect=NULL;		/* No touch_effect as unmovable */
	}

	ebox_circlePad=egi_btnbox_new(  NULL, 		  /* put tag later */
				  data_circlePad,     	  /* EGI_DATA_BTN *egi_data */
		        	  0, 		  	  /* bool movable */
			          0, 40, 	  	  /* int x0, int y0 */
				  240, 120, 	  	  /* int width, int height */
		       		  -1, 		  	  /* int frame,<0 no frame */
	       			  -1  	  		  /*int prmcolor, for geom button only. */
			   );

	if(ebox_circlePad==NULL)
		printf("%s: Fail to create ebox_circlePad!\n",__func__);
	else
		ebox_circlePad->reaction=circling_volume;	/*  Assign reaction */


	/* --------- 2.2 create a touch_pad for btns scroll --------- */
	data_touchPad=egi_btndata_new( 112, 		/* int id */
				  btnType_square, 	/* enum egi_btn_type shape */
				  NULL,  	  	/* struct symbol_page *icon. If NULL, use geometry. */
				  0, 		  	/* int icon_code, assign later.. */
				  NULL   	  	/* for ebox->tag font */
				);

	if(data_touchPad != NULL) {
		data_touchPad->touch_effect=NULL;	/* No touch_effect */
	}

	ebox_touchPad=egi_btnbox_new(  NULL, 		  /* put tag later */
				  data_touchPad,     	  /* EGI_DATA_BTN *egi_data */
		        	  0, 		  	  /* bool movable */
			          0, 160, 	  	  /* int x0, int y0 */
				  240, 60, 	  	  /* int width, int height */
		       		  -1, 		  	  /* int frame,<0 no frame */
	       			  -1  	  		  /*int prmcolor, for geom button only. */
			   );

	if(ebox_touchPad==NULL)
		printf("%s: Fail to create ebox_touchPad!\n",__func__);
	else
		ebox_touchPad->reaction=scroll_btns;	/*  Assign reaction */


	/* --------- 3. create title bar for movie title --------- */
	title_FTtxt=NULL;
	ebox_title=NULL;

        /* For FTsymbols TXT */
        title_FTtxt=egi_utxtdata_new( 5, 5,                       /* offset from ebox left top */
                                      2, 240-5,                   /* lines, pixels per line */
                                      egi_appfonts.regular,  	  /* font face type */
                                      18, 18,                     /* font width and height, in pixels */
                                      0,                          /* adjust gap, minus also OK */
                                      WEGI_COLOR_GRAYB            /* font color  */
                                    );
        /* assign uft8 string */
        //title_FTtxt->utxt=NULL;

	/* create volume EBOX */
        ebox_title=egi_txtbox_new( "Title",    		/* tag */
                                     title_FTtxt,       /* EGI_DATA_TXT pointer */
                                     true,              /* bool movable */
                                     0, 0,              /* int x0, int y0 */
                                     240,40,            /* width, height(adjusted as per nl and fw) */
                                     frame_none,        /* int frame, -1 or frame_none = no frame */
                                     -1   		/* prmcolor,<0 transparent */
                                   );

        /* --------- 4. create a TXT ebox for Volume value displaying  --------- */
	vol_FTtxt=NULL;
	ebox_voltxt=NULL;
        /* For FTsymbols TXT */
        vol_FTtxt=egi_utxtdata_new( 10, 3,                        /* offset from ebox left top */
                                    1, 100,                       /* lines, pixels per line */
                                    egi_appfonts.bold,//regular,  /* font face type */
                                    20, 20,                       /* font width and height, in pixels */
                                    0,                            /* adjust gap, minus also OK */
                                    WEGI_COLOR_WHITE              /* font color  */
                                  );
        /* assign uft8 string */
        vol_FTtxt->utxt=NULL;

	/* create volume EBOX */
        ebox_voltxt=egi_txtbox_new( "volume_txt",    /* tag */
                                     vol_FTtxt,      /* EGI_DATA_TXT pointer */
                                     true,           /* bool movable */
                                     70, 60,         /* int x0, int y0 */
                                     80, 30,         /* width, height(adjusted as per nl and fw) */
                                     frame_round_rect,  /* int frame, -1 or frame_none = no frame */
                                     WEGI_COLOR_GRAY3   /* prmcolor,<0 transparent*/
                                   );
	/* set frame type */
        //ebox_voltxt->frame_alpha; /* No frame and prmcolor*/

	/* set status as hidden, activate it only by touching */
	ebox_voltxt->status=status_hidden;

        /* --------- 5. create a horizontal sliding bar --------- */
        int sb_len=gv_fb_dev.pos_xres-40; 	/* Sliding slot length */
        int sb_pv=0; 				/* initial  percent value */
	EGI_POINT sbx0y0={ 40/2, 260 }; 	/* starting point */

        data_slider=egi_sliderdata_new(   /* slider data is a EGI_DATA_BTN + privdata(egi_data_slider) */
                                        /* ---for btnbox-- */
                                        1, btnType_square,      /* data id, enum egi_btn_type shape */
                                        NULL,           	/* struct symbol_page *icon */
                                        0,              	/* int icon_code, */
                                        &sympg_testfont,	/* struct symbol_page *font */
                                        /* ---for slider--- */
					slidType_horiz,		/*  enum egi_slid_type */
                                        sbx0y0,			/* slider starting EGI_POINT pxy */
                                        5,sb_len,           	/* slot width, slot length */
                                        sb_pv*sb_len/100,      	/* init val, usually to be 0 */
                                        WEGI_COLOR_GRAY,  	/* EGI_16BIT_COLOR val_color */
                                        WEGI_COLOR_GRAY5,   	/* EGI_16BIT_COLOR void_color */
                                        WEGI_COLOR_WHITE   	/* EGI_16BIT_COLOR slider_color */
                            );

        time_slider=egi_slider_new(
          	                    "volume slider",  		/* char *tag, or NULL to ignore */
                	            data_slider, 		/* EGI_DATA_BTN *egi_data */
                                    20,20,    			/* slider block: ebox->width/height, to be Min. if possible */
                                    50,30,       		/* touchbox size, twidth/theight */
                                    -1,          		/* int frame, <0 no frame */
                                    -1      /* prmcolor
					     * 1. Let <0, it will draw default slider, instead of applying gemo or icon.
                                             * 2. prmcolor geom applys only if prmcolor>=0 and egi_data->icon!=NULL
					     */
                           );
	/* set EBOX id for time_slider */
	time_slider->id=TIME_SLIDER_ID;
        /* set reaction function */
        time_slider->reaction=sliding_timebar;

        /* --------- 6. Create a TXT ebox for displaying time elapsed and duration  ------- */
	tmSymHeight=18;		/* symbol height */

	/* position of txt ebox */
	tmX0[0]=sbx0y0.x;
	tmY0[0]=sbx0y0.y-tmSymHeight-8;
	tmX0[1]=sbx0y0.x+sb_len-50;
	tmY0[1]=tmY0[0];

	/* create playing time TXT EBOX for sliding bar */
	for(i=0; i<2; i++) {
	        /* For symbols TXT */
		data_tmtxt[i]=egi_txtdata_new( 0, 0,    		/* offx,offy from EBOX */
                	      	               1,                       /* lines */
                        	               10,                      /* chars per line */
                                	       &sympg_ascii,       	/* font */
	                                       WEGI_COLOR_WHITE         /* txt color */
        	                      );
		/* create volume EBOX */
        	ebox_tmtxt[i]=egi_txtbox_new( i==0?"TMbar_txt0":"TMbar_txt1",    	/* tag */
                                     data_tmtxt[i],     	/* EGI_DATA_TXT pointer */
                                     true,              	/* bool movable */
                                     tmX0[i], tmY0[i],  	/* int x0, int y0 */
                                     120, 15,           	/* width, height(adjusted as per nl and symheight ) */
                                     -1,  			/* int frame, -1 or frame_none = no frame */
                                     -1	   			/* prmcolor,<0 transparent*/
                                   );
	}
	/* set ID */
	ebox_tmtxt[0]->id=TIME_TXT0_ID;
	ebox_tmtxt[1]->id=TIME_TXT1_ID;

	/* initial value in tmtxt */
	egi_push_datatxt(ebox_tmtxt[0], "00.000", NULL);
	egi_push_datatxt(ebox_tmtxt[1], "00.000", NULL);

	/* --------- 7. create PAGE ffmotion ------- */
	/* 7.1 create PAGE ffmotion  */
	EGI_PAGE *page_ffmotion=egi_page_new("page_ffmotion");
	while(page_ffmotion==NULL)
	{
		printf("egi_create_ffplaypage(): fail to call egi_page_new(), try again ...\n");
		page_ffmotion=egi_page_new("page_ffmotion");
		tm_delayms(10);
	}
	page_ffmotion->ebox->prmcolor=WEGI_COLOR_BLACK;
	/* Set slide off */
	page_ffmotion->slide_off=true;

	/* decoration */
	//page_ffmotion->ebox->method.decorate=pageffmot_decorate; /* draw lower buttons canvas */

        /* 7.2 Put pthread runner */
        page_ffmotion->runner[0]= thread_ffplay_motion;

        /* 7.3 Set default routine job */
        //page_ffmotion->routine=egi_page_routine; /* use default routine function */
	page_ffmotion->routine=egi_homepage_routine;  /* for sliding operation */

//	#if 0
//	page_ffmotion->slide_handler=sliding_volume;  /* sliding handler for volume adjust */
//	#else
//	page_ffmotion->slide_handler=circling_volume; /* sliding handler for volume adjust */
//	#endif

	page_ffmotion->page_refresh_misc=refresh_misc; /* random colro for btn */

        /* 7.4 Set wallpaper */
        //page_ffmotion->fpath="/tmp/mplay.jpg";


	/* 7.5 Add ebox to home page */
	for(i=0; i<btnum; i++) 	/* 7.5.1 Add control buttons */
		egi_page_addlist(page_ffmotion, ffmot_btns[i]);

	for(i=0; i<2; i++)	/* 7.5.2 Add time txt for time sliding bar */
		egi_page_addlist(page_ffmotion, ebox_tmtxt[i]);

	egi_page_addlist(page_ffmotion, ebox_circlePad);  	/* 7.5.3 Add circling pad ebox */
	egi_page_addlist(page_ffmotion, ebox_touchPad); 	/* 7.5.4 Add touch pad ebox */
	egi_page_addlist(page_ffmotion, ebox_title);  		/* 7.5.5 Add title txt ebox */
	egi_page_addlist(page_ffmotion, ebox_voltxt); 		/* 7.5.6 Add volume txt ebox */
	egi_page_addlist(page_ffmotion, time_slider); 		/* 7.5.7 Add time_slider ebox */


	/* ebox group */
	for(i=0; i<5; i++)
		ebox_group[i]=ffmot_btns[i];
	ebox_group[5]=time_slider;
	ebox_group[6]=ebox_tmtxt[0];
	ebox_group[7]=ebox_tmtxt[1];


	return page_ffmotion;
}


/*-----------------  RUNNER 1 --------------------------

-------------------------------------------------------*/
static void pageFFmotion_runner(EGI_PAGE *page)
{

}

/*------------------------ BTN REACTION --------------------------
			ffmotion play PREV
----------------------------------------------------------------*/
static int ffmot_prev(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	/* ffmusic.c is forced to play the next song,
         * so change 'play&pause' button icon from PLAY to PAUSE
	 */
	if((data_btns[BTN_ID_PLAYPAUSE]->icon_code&0xffff)==ICON_CODE_PLAY)
		data_btns[BTN_ID_PLAYPAUSE]->icon_code=(btn_symcolor<<16)+ICON_CODE_PAUSE; /* toggle icon */

	/* set refresh flag for this ebox */
	egi_ebox_needrefresh(ebox);

	/* set ffmot_Ctx->ffcmd, FFplay will reset it. */
	FFmotion_Ctx->ffcmd=cmd_prev;

	/* only react to status 'pressing' */
	return btnret_OK;
}


/*------------------------ BTN REACTION --------------------------
			ffmotion play NEXT
----------------------------------------------------------------*/
static int ffmot_next(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	/* ffmusic.c is forced to play the next song,
         * so change 'play&pause' button icon from PLAY to PAUSE
	 */
	if( (data_btns[BTN_ID_PLAYPAUSE]->icon_code&0xffff)==ICON_CODE_PLAY )
		data_btns[BTN_ID_PLAYPAUSE]->icon_code=(btn_symcolor<<16)+ICON_CODE_PAUSE; /* toggle icon */

	/* set refresh flag for this ebox */
	egi_ebox_needrefresh(ebox);

	/* set FFmotion_Ctx->ffcmd, FFplay will reset it. */
	FFmotion_Ctx->ffcmd=cmd_next;

	return btnret_OK;
}


/*-------------------------  BTN REACTION  ---------------------------
ffmusic palypause
return
----------------------------------------------------------------------*/
static int ffmot_playpause(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	/* toggle the icon between play and pause */
	if( (data_btns[BTN_ID_PLAYPAUSE]->icon_code & 0x0ffff ) == ICON_CODE_PLAY ) {
		/* set FFmotion_Ctx->ffcmd, FFplay will reset it. */
		FFmotion_Ctx->ffcmd=cmd_play;
		data_btns[BTN_ID_PLAYPAUSE]->icon_code=(btn_symcolor<<16)+ICON_CODE_PAUSE; /* toggle icon */
	}
	else {
		/* set FFmotion_Ctx->ffcmd, FFplay will reset it. */
		FFmotion_Ctx->ffcmd=cmd_pause;
		data_btns[BTN_ID_PLAYPAUSE]->icon_code=(btn_symcolor<<16)+ICON_CODE_PLAY;  /* toggle icon */
	}

	/* set refresh flag for this ebox */
	egi_ebox_needrefresh(ebox);

	return btnret_OK;
}


/*-------------------------  BTN REACTION  ---------------------------
ffplay play mode rotate.
return
----------------------------------------------------------------------*/
static int ffmot_playmode(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{

        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

#if   1 /* --------- AS SCREEN ROTATION   ---------*/
	motpage_rotate(gv_fb_dev.pos_rotate+1);	/* +1 rotate clockwise 90deg */
	FFmotion_Ctx->ffcmd=cmd_rotate_clock;


	return btnret_OK;

#elif 0 /*---------  AS PLAYMODE LOOP SELECTION BUTTON  ---------*/
	static int count=0;

	/* only react to status 'pressing' */
	struct egi_data_btn *data_btn=(struct egi_data_btn *)(ebox->egi_data);

	count++;
	if(count>2)
		count=0;

	/* Default LOOPALL, rotate code:  LOOPALL -> REPEATONE -> SHUFFLE */
	data_btn->icon_code=(btn_symcolor<<16)+(ICON_CODE_LOOPALL-count%3);

	/* command for ffplay, ffplay will reset it */
	if(count==0) {					/* LOOP ALL */
		FFmotion_Ctx->ffmode=mode_loop_all;
		FFmotion_Ctx->ffcmd=cmd_mode;		/* set cmd_mode at last!!! as for LOCK. */
	}
	else if(count==1) { 				/* REPEAT ONE */
		FFmotion_Ctx->ffmode=mode_repeat_one;
		FFmotion_Ctx->ffcmd=cmd_mode;
	}
	else if(count==2) { 				/* SHUFFLE */
		FFmotion_Ctx->ffmode=mode_shuffle;
		FFmotion_Ctx->ffcmd=cmd_mode;
	}

	/* set refresh flag for this ebox */
	egi_ebox_needrefresh(ebox);

	return btnret_OK;

#else /* ---------  AS EXIT BUTTON  --------- */


	return btnret_REQUEST_EXIT_PAGE;
#endif

}


/*-------------------------  BTN REACTION  ---------------------------
ffmotion exit
???? do NOT call long sleep function in button functions.
return
----------------------------------------------------------------------*/
static int ffmot_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

#if 1
	/*TEST: send HUP signal to iteself */
	if(raise(SIGUSR1) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to send SIGUSR1 to itself.",__func__);
	}
	EGI_PLOG(LOGLV_ASSERT,"%s: Finish rasie(SIGSUR1), return to page routine...",__func__);

  /* 1. When the page returns from SIGSTOP by signal SIGCONT, status 'pressed_hold' and 'releasing'
   *    will be received one after another,(In rare circumstances, it may receive 2 'preesed_hold')
   *	and the signal handler will call egi_page_needrefresh().
   *    But 'releasing' will trigger btn refresh by egi_btn_touch_effect() just before page refresh.
   *    After refreshed, need_refresh will be reset for this btn, so when egi_page_refresh() is called
   *    later to refresh other elements, this btn will erased by page bkcolor/wallpaper.
   * 2. The 'releasing' status here is NOT a real pen_releasing action, but a status changing signal.
   *    Example: when status transfers from 'pressed_hold' to PEN_UP.
   * 3. To be handled by page routine.
   */

	return pgret_OK; /* need refresh page, a trick here to activate the page after CONT signal */

#else
        egi_msgbox_create("Message:\n   Click! Start to exit page!", 300, WEGI_COLOR_ORANGE);
        return btnret_REQUEST_EXIT_PAGE;  /* end process */
#endif
}


/*-----------------------------------------
	Decoration for the page
-----------------------------------------*/
static int pageFFmot_decorate(EGI_EBOX *ebox)
{
	/* bkcolor for bottom buttons */
	fbset_color(WEGI_COLOR_GRAY3);
        draw_filled_rect(&gv_fb_dev,0,266,239,319);

	return 0;
}


/*-------------------------------------------------------------------
		Horizontal Sliding bar reaction
---------------------------------------------------------------------*/
static int sliding_timebar(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        static int 	mark;
        EGI_DATA_SLIDER *data_slider;
        int 		minx,maxx;
        static int 	pval;		/* playing time position in percentage */
	static int	secval;		/* playing time in seconds */
        int 		tm_h=0,tm_min=0,tm_sec=0;
        char 		strtm[64];
        static char 	strcmd[50];

        /* bypass unwanted touch status */
        if( touch_data->status != pressed_hold && touch_data->status != pressing
					       && touch_data->status != releasing )
                return btnret_IDLE;

        /* set mark when press down, !!!! egi_touch_getdata() may miss this status !!! */
        if(touch_data->status==pressing)
        {
                printf("pressing slider...\n");
                mark=ebox->x0;
        }
        else if(touch_data->status==pressed_hold) /* sliding */
        {
		/* Get slider data */
		data_slider=egi_slider_getdata(ebox);
                minx=data_slider->sxy.x-(ebox->width>>1);
                maxx=minx+data_slider->sl-1;

                /* Update slider x0y0, it's coupled with touchbox position */
                ebox->x0 = mark+(touch_data->dx);
                if(ebox->x0 < minx) ebox->x0=minx;
                else if(ebox->x0 > maxx) ebox->x0=maxx;

                #if 0   /* set need refresh for PAGE routine */
                egi_ebox_needrefresh(ebox);
                #else  /* or force to refresh EBOX now! -- Quik response!*/
		egi_ebox_forcerefresh(ebox);
                #endif

                /* adjust pval in percentage of sliding bar */
                pval=100*data_slider->val/(data_slider->sl-1);
		secval=pval*FFmotion_Ctx->sec_Vduration/100;
		//printf("pval=%%%d\n",pval);

        	/* Update sliding bar TXT immediately */
	        tm_h=secval/3600;
        	tm_min=(secval-tm_h*3600)/60;
	        tm_sec=secval%60;
        	memset(strtm,0,sizeof(strtm));
		if(tm_h>0)
	        	snprintf(strtm, sizeof(strtm)-1, "%d:%02d:%02d",tm_h, tm_min,tm_sec);
		else
	                snprintf(strtm, sizeof(strtm)-1, "%02d:%02d",tm_min, tm_sec);

                egi_push_datatxt(ebox_tmtxt[0], strtm, NULL);
		egi_ebox_forcerefresh(ebox_tmtxt[0]);
        }
	else if(touch_data->status==releasing)  /* Send ffcmd */
	{
		/* Send ffcmd cmd_seek*/
		FFmotion_Ctx->seek_pval=pval;
		FFmotion_Ctx->ffcmd=cmd_seek;
	}


        return btnret_IDLE; /* OK, page need not refresh, ebox self refreshed. */
}


/*----------------------  BTN REACTION  ---------------------------
                   Sliding Operation handler
Slide up/down to adjust ALSA volume.
------------------------------------------------------------------*/
static int sliding_volume(EGI_PAGE* page, EGI_TOUCH_DATA * touch_data)
{
        static int mark;
	static int vol;
	static char strp[64];

	/* bypass outrange sliding */
	if( !point_inbox2( &touch_data->coord, &slide_zone) )
              return btnret_IDLE;

	/* ---- TEST ---- */
	printf("%s: touch(x,y): %d, %d \n",__func__, touch_data->coord.x, touch_data->coord.y );

        /* 1. set mark when press down, !!!! egi_touch_getdata() may miss this status !!! */
        if(touch_data->status==pressing)
        {
                printf("vol pressing\n");
                if( egi_getset_pcm_volume(&mark,NULL) !=0 ) /* get volume */
			mark=0;
		//printf("mark=%d\n",mark);
                return btnret_OK; /* do not refresh page, or status will be cut to release_hold */
        }
        /* 2. adjust button position and refresh */
        else if( touch_data->status==pressed_hold )
        {
		/* unhide vol_FTtxt */
		egi_txtbox_unhide(ebox_voltxt);

                /* adjust volume */
                vol =mark-(touch_data->dy>>3); /* Let not so fast */
		if(vol>100) vol=100;
		else if(vol<0) vol=0;

		printf("%s pvol=%d \n",__func__, vol);

		/* set volume */
                if( egi_getset_pcm_volume(NULL,&vol)==0 )
			sprintf(strp,"音量 %d%%",vol);
		else
			sprintf(strp,"音量 无效");

		//printf("dy=%d, vol=%d\n",touch_data->dy, vol);

		/* set utxt to ebox_voltxt */
		vol_FTtxt->utxt=(unsigned char *)strp;
		#if 0  /* set need refresh for PAGE routine */
		egi_ebox_needrefresh(ebox_voltxt);
		#else  /* or force to refresh EBOX at once! */
		egi_ebox_forcerefresh(ebox_voltxt);
		#endif

		return btnret_OK;
	}
        /* 3. clear volume txt, 'release' */
        else if( touch_data->status==releasing )
        {
		printf("vol releasing\n");
		mark=vol; /* update mark*/

		/* reset vol_FTtxt */
		vol_FTtxt->utxt=NULL;
		#if 0 /* set need refresh */
		egi_ebox_needrefresh(ebox_voltxt);
		#else  /* or refresh now */
		egi_ebox_forcerefresh(ebox_voltxt);
		#endif

		/* Hide to erase image */
		egi_txtbox_hide(ebox_voltxt);

		return btnret_OK;
	}
        else /* bypass unwanted touch status */
              return btnret_IDLE;

}


/*----------------------  BTN REACTION  ---------------------------
                   Circling Operation handler
Circling CW/CCW to adjust ALSA volume.
------------------------------------------------------------------*/
//static int circling_volume(EGI_PAGE* page, EGI_TOUCH_DATA * touch_data)
static int circling_volume(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	int initvol;
	static int vol;
	static char strp[64];
        static bool slide_activated=false; /* indicator */
	static EGI_POINT pts[3];	/* 3 points */
	int	vc;			/* pseudo curvature value */
	int	adv;			/* adjusting value */

	/* bypass outrange sliding */
	if( !point_inbox2( &touch_data->coord, &slide_zone) )
              return btnret_IDLE;

	/* ---- TEST ---- */
	printf("%s: touch(x,y): %d, %d \n",__func__, touch_data->coord.x, touch_data->coord.y );

        /* 1. set mark when press down, !!!! egi_touch_getdata() may miss this status !!! */
        if(touch_data->status==pressing || (slide_activated==false && touch_data->status==pressed_hold) )
        {
                printf("vol pressing\n");
                egi_getset_pcm_volume(&vol,NULL); /* get volume */

		/* reset points coordinates */
		pts[2]=pts[1]=pts[0]=(EGI_POINT){ touch_data->coord.x, touch_data->coord.y };

                /* set indicator */
                slide_activated=true;

                return btnret_OK; /* do not refresh page, or status will be cut to release_hold */
        }
        /* 2. adjust button position and refresh */
        else if( touch_data->status==pressed_hold )
        {
		/* unhide vol_FTtxt */
		egi_txtbox_unhide(ebox_voltxt);

                /* check circling motion */
		pts[0]=pts[1]; pts[1]=pts[2];
		//memcpy((void*)pts, (void *)(pts+1), sizeof(EGI_POINT)<<1);
		pts[2].x=touch_data->coord.x; pts[2].y=touch_data->coord.y;
		printf( " pts0(%d,%d) pts1(%d,%d) pts2(%d,%d).\n",pts[0].x, pts[0].y, pts[1].x, pts[1].y,
								     pts[2].x, pts[2].y );
		vc=mat_pseudo_curvature(pts);
		printf(" vc=%d ",vc);

		/* ajust volume */
		adv=vc>>6;
		if(adv>5)adv=2; /* set LIMIT */
		if(adv<-5)adv=-2;
                vol += adv;

		if(vol>100) vol=100;
		else if(vol<0) vol=0;

		printf("%s pvol=%d \n",__func__, vol);

		/* set volume */
                if( egi_getset_pcm_volume(&initvol,&vol)==0 )
			sprintf(strp,"音量 %d%%",vol);
		else
			sprintf(strp,"音量 无效");
		printf("%s: initvol=%d%%; vol=%d%%\n", __func__, initvol, vol);
		//printf("dy=%d, vol=%d\n",touch_data->dy, vol);

		/* set utxt to ebox_voltxt */
		vol_FTtxt->utxt=(unsigned char *)strp;
		#if 0  /* set need refresh for PAGE routine */
		egi_ebox_needrefresh(ebox_voltxt);
		#else  /* or force to refresh EBOX now! */
		egi_ebox_forcerefresh(ebox_voltxt);
		#endif

		return btnret_OK;
	}
        /* 3. clear volume txt, 'release' */
        else if( touch_data->status==releasing )
        {
		printf("vol releasing\n");

		/* reset vol_FTtxt */
		vol_FTtxt->utxt=NULL;
		#if 0 /* set need refresh */
		egi_ebox_needrefresh(ebox_voltxt);
		#else  /* or refresh now */
		egi_ebox_forcerefresh(ebox);
		#endif

		/* Hide to erase image */
		egi_txtbox_hide(ebox_voltxt);

                /* set indicator */
                slide_activated=false;

		return btnret_OK;
	}
        else /* bypass unwanted touch status */
              return btnret_IDLE;

}


/*--------------------------------------------
       	A Misc. job for PAGE
    Set random color for PAGE btns .
---------------------------------------------*/
static int refresh_misc(EGI_PAGE *page)
{
    int i;

    btn_symcolor=egi_color_random2(color_all, 150);

    for(i=0; i<btnum; i++) {
	if(data_btns[i] != NULL) {
		data_btns[i]->icon_code=(btn_symcolor<<16)+( data_btns[i]->icon_code & 0xffff );
	}
    }

    return 0;
};


/*-----------------------------
Release and free all resources
------------------------------*/
void free_ffmotionPage(void)
{

   /* all EBOXs in the page will be release by calling egi_page_free()
    *  just after page routine returns.
    */


}

/*------------------------------------------------------------
Update title txt.

@title:	Pointer to a title string, with UTF-8 encoding.
	Title

---------------------------------------------------------------*/
void motpage_update_title(const unsigned char *title)
{
	static unsigned char strtmp[512];

	memset(strtmp,0,sizeof(strtmp));

	strncpy((char *)strtmp,(char *)title, sizeof(strtmp)-1);

        title_FTtxt->utxt=strtmp;

	egi_page_needrefresh(ebox_title->container);
}

/*------------------------------------------------------------
Update timing bar and its txt.

In order to improve efficiency, it's better to check whether
tm_druation or tm_elapsed is updated first, then call this
function only if they have been changed.

@tm_duratoin:	duration time in second.
@tm_elapsed:	elapsed time in second.
---------------------------------------------------------------*/
void motpage_update_timingBar(int tm_elapsed, int tm_duration )
{
	int psval;
        char strtm[64];
        int tm_h=0,tm_min=0,tm_sec=0;
	static int old_duration=-1;  /* ensure first tm_duration != old_duration for the first time ! */
	static int old_elapsed=-1;

        /* --- 1. Update sliding bar duration TXT ebox --- */
//	if( old_duration != tm_duration ) {
	        tm_h=tm_duration/3600;
        	tm_min=(tm_duration-tm_h*3600)/60;
	        tm_sec=tm_duration%60;

		old_duration=tm_duration;

        	memset(strtm,0,sizeof(strtm));
		if(tm_h>0)
	        	snprintf(strtm, sizeof(strtm)-1, "%d:%02d:%02d",tm_h, tm_min,tm_sec);
		else
	                snprintf(strtm, sizeof(strtm)-1, "%02d:%02d",tm_min, tm_sec);

                egi_push_datatxt(ebox_tmtxt[1], strtm, NULL);
                egi_ebox_needrefresh(ebox_tmtxt[1]);
//	}

        /* --- 2. Update elapsed time TXT ebox AND slider indicator positon --- */
//	if( old_elapsed != tm_elapsed ) {
		/*  3.1 update elapsed time TXT ebox */
	        tm_h=tm_elapsed/3600;
        	tm_min=(tm_elapsed-tm_h*3600)/60;
	        tm_sec=tm_elapsed%60;

		old_elapsed=tm_elapsed;

	        memset(strtm,0,sizeof(strtm));
        	if(tm_h>0)
        		snprintf(strtm, sizeof(strtm)-1, "%d:%02d:%02d",tm_h, tm_min,tm_sec);
	        else
        	        snprintf(strtm, sizeof(strtm)-1, "%02d:%02d",tm_min, tm_sec);
	        egi_push_datatxt(ebox_tmtxt[0], strtm, NULL);
	        egi_ebox_needrefresh(ebox_tmtxt[0]);

		/*  3.2 update sliding indicator position */
        	if(tm_duration==0) { /* To avoid divisor 0 */
        		//data_slider->val=0;
	                egi_slider_setpsval(time_slider, 0);
        	} else {
                	/* get percentage value and set to time slider */
	        	psval=tm_elapsed*100/tm_duration;
        	        egi_slider_setpsval(time_slider, psval);
	        }
        	egi_ebox_needrefresh(time_slider);
//	}
}


/*-----------------------------------------------------
Change PAGE displaying mode.

@pos  PAGE displaying position.
      0: default X,Y coordinate of FB (Portrait mode)
      1: clockwise rotation 90 deg.   (Landscape mode)
      2: clockwise rotation 180 deg.  (Portrait mode)
      3: clockwise rotation 270 deg.  (Landscape mode)

----------------------------------------------------*/
void motpage_rotate(unsigned char pos)
{
	int i;
	EGI_POINT pxy;			/* sliding bar starting point */
	int sl;  		     	/* length of sliding bar */
	EGI_DATA_SLIDER *prvdata_slider=(EGI_DATA_SLIDER *)data_slider->prvdata;

	/* adjust FBDEV position param */
	fb_position_rotate(&gv_fb_dev, pos);

	/* Get new pxy and sl after FB position rotation! */
	if(pos&0x1) {						/* Landscape mode */
		pxy=(EGI_POINT){40/2, gv_fb_dev.pos_yres-10};
		sl=gv_fb_dev.pos_xres-40-55;
	}
	else {							/* Portrait mode */
		pxy=(EGI_POINT){40/2, gv_fb_dev.pos_yres-10-48};
		sl=gv_fb_dev.pos_xres-40;
	}

	/* reset slider x0y0 and sl */
	egi_slider_reset(time_slider, 0, sl, pxy); /* slider, psval, sl, pxy */

	/* modify position of timing TXT eBox, as of current coord.  */
	ebox_tmtxt[0]->x0=pxy.x;
	ebox_tmtxt[0]->y0=pxy.y-tmSymHeight-8;
	ebox_tmtxt[1]->x0=pxy.x+prvdata_slider->sl-50;
	ebox_tmtxt[1]->y0=ebox_tmtxt[0]->y0;

	/* relocate control Button */
	for(i=0; i<btnum; i++) {
		if(pos&0x1) {		/* Landscape mode */
			ffmot_btns[i]->x0=gv_fb_dev.pos_xres-48;
			ffmot_btns[i]->y0=48*i;
		}
		else {			/* Portrait mode */
			ffmot_btns[i]->x0=48*i;
			ffmot_btns[i]->y0=gv_fb_dev.pos_yres-48;
		}
	}

	/* Other child ebox keep unchanged */

	/* refresh the PAGE */
	egi_page_needrefresh(time_slider->container);
}



/*----------------------  BTN REACTION  ---------------------------
Scroll down/up buttons and timing bar.
------------------------------------------------------------------*/
static int scroll_btns(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	int i,j;
	int delt;
	static bool scroll_up=true;

        /* bypass unwanted touch status */
        if( touch_data->status !=  pressing )
                return btnret_IDLE;

	/* set delt */
	if(scroll_up) {
		printf("Scroll down\n");
		delt=2;
	}
	else {
		printf("Scroll up\n");
		delt=-2;
	}

	/* Step shift btns */
	for(i=0; i<25; i++) {

		for(j=0; j<8; j++) {
			ebox_group[j]->y0 += delt;
			egi_ebox_forcerefresh(ebox_group[j]);
		}

		usleep(50000);
	}

	/* reset token */
	scroll_up = !scroll_up;

	return btnret_OK;
}
