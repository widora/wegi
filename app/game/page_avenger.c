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
4. button reaction functins.
5. page misc. job.

                (((  --------  PAGE DIVISION  --------  )))
[Y0-Y29]
[Y266-Y319]
{0,266}, {240-1, 320-1}         --- Buttons


Midas Zhou
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "egi_common.h"
#include "sound/egi_pcm.h"
#include "egi_FTsymbol.h"
#include "page_avenger.h"
#include "avenger.h"

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

#define TIME_TXT0_ID          101
#define TIME_TXT1_ID          102


static uint16_t btn_symcolor;

/* buttons */
static int btnum=5;
static EGI_DATA_BTN 	*data_btns[5];
static EGI_EBOX 	*ebox_btns[5];

/* title txt ebox */
static EGI_DATA_TXT	*title_FTtxt;
static EGI_EBOX		*ebox_title;

/* Time sliding bar txt, for time elapsed and duration */
static EGI_DATA_TXT 	*data_CreditTxt[2];
static EGI_EBOX	    	*ebox_CreditTxt[2];
static int 		tmSymHeight=18;              /* symbol height */
static int 		tmX0[2], tmY0[2];            /* txt EBOX x0,y0  */


/* static functions */
static int page_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int refresh_misc(EGI_PAGE *page);


/*---------- [  PAGE ::  FF MUSIC PLAYER ] ---------
1. create eboxes for 4 buttons and 1 title bar

Return
	pointer to a page	OK
	NULL			fail
------------------------------------------------------*/
EGI_PAGE *create_avengerPage(void)
{
	int i;

	/* --------- 1. create buttons --------- */
        for(i=0;i<btnum;i++) /* row of buttons*/
        {
		/* 1. create new data_btns */
		data_btns[i]=egi_btndata_new(	i, /* int id */
						btnType_square, /* enum egi_btn_type shape */
						&sympg_sbuttons, /* struct symbol_page *icon. If NULL, use geometry. */
						0, /* int icon_code, assign later.. */
						&sympg_testfont /* for ebox->tag font */
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
		ebox_btns[i]=egi_btnbox_new(  NULL, 		  /* put tag later */
						data_btns[i],     /* EGI_DATA_BTN *egi_data */
				        	1, 		  /* bool movable */
					        48*i, 320-48, /* int x0, int y0 */
						48, 48, 	  /* int width, int height */
				       		-1, 		  /* int frame,<0 no frame */
		       				WEGI_COLOR_GRAY  /*int prmcolor, for geom button only. */
					   );
		/* hide it */
		egi_btnbox_hide(ebox_btns[i]);

		/* if fail, try again ... */
		if(ebox_btns[i]==NULL) {
			printf("egi_create_ffplaypage(): fail to call egi_btnbox_new() for ebox_btns[%d]. retry...\n", i);
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
	egi_ebox_settag(ebox_btns[0], "Prev");
	data_btns[0]->icon_code=(btn_symcolor<<16)+ICON_CODE_PREV; /* SUB_COLOR+CODE */
	ebox_btns[0]->reaction=NULL;

	egi_ebox_settag(ebox_btns[1], "Play&Pause");
	data_btns[1]->icon_code=(btn_symcolor<<16)+ICON_CODE_PAUSE; /* default status is playing*/
	ebox_btns[1]->reaction=NULL;

	egi_ebox_settag(ebox_btns[2], "Next");
	data_btns[2]->icon_code=(btn_symcolor<<16)+ICON_CODE_NEXT;
	ebox_btns[2]->reaction=NULL;

	egi_ebox_settag(ebox_btns[3], "Exit");
	data_btns[3]->icon_code=(btn_symcolor<<16)+ICON_CODE_EXIT;
	ebox_btns[3]->reaction=page_exit;

	egi_ebox_settag(ebox_btns[4], "Playmode");
	data_btns[4]->icon_code=(btn_symcolor<<16)+ICON_CODE_LOOPALL;
	ebox_btns[4]->reaction=NULL;

	/* --------- 2. create title bar for movie title --------- */
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


        /* --------- 3. Create a TXT ebox for displaying time elapsed and duration  ------- */
	tmSymHeight=18;		/* symbol height */

	/* position of txt ebox */
	tmX0[0]=0;
	tmY0[0]=5;
	tmX0[1]=240-90;
	tmY0[1]=5;

	/* create playing time TXT EBOX for sliding bar */
	for(i=0; i<2; i++) {
	        /* For symbols TXT */
		data_CreditTxt[i]=egi_txtdata_new( 0, 0,    		/* offx,offy from EBOX */
                	      	               1,                       /* lines */
                        	               18,                      /* chars per line */
                                	       &sympg_ascii,       	/* font */
	                                       WEGI_COLOR_WHITE         /* txt color */
        	                      );
		/* create volume EBOX */
        	ebox_CreditTxt[i]=egi_txtbox_new( i==0?"score":"level",    	/* tag */
                                     data_CreditTxt[i],     	/* EGI_DATA_TXT pointer */
                                     true,              	/* bool movable */
                                     tmX0[i], tmY0[i],  	/* int x0, int y0 */
                                     120, 15,           	/* width, height(adjusted as per nl and symheight ) */
                                     -1,  			/* int frame, -1 or frame_none = no frame */
                                     -1	   			/* prmcolor,<0 transparent*/
                                   );
	}
	/* set ID */
	ebox_CreditTxt[0]->id=TIME_TXT0_ID;
	ebox_CreditTxt[1]->id=TIME_TXT1_ID;

	/* initial value in tmtxt */
	egi_push_datatxt(ebox_CreditTxt[0], "SCORE: ", NULL);
	egi_push_datatxt(ebox_CreditTxt[1], "LEVEL: ", NULL);

	/* --------- 5. create page ------- */
	/* 5.1 create page */
	EGI_PAGE *page_avenger=egi_page_new("page_avenger");
	while(page_avenger==NULL)
	{
		printf("egi_create_ffplaypage(): fail to call egi_page_new(), try again ...\n");
		page_avenger=egi_page_new("page_avenger");
		tm_delayms(10);
	}
	page_avenger->ebox->prmcolor=WEGI_COLOR_BLACK;

	/* decoration */
	//page_avenger->ebox->method.decorate=page_decorate; /* draw lower buttons canvas */

        /* 5.2 Put pthread runner */
        page_avenger->runner[0]= thread_game_avenger;

        /* 5.3 Set default routine job */
        //page_avenger->routine=egi_page_routine; /* use default routine function */
	page_avenger->routine=egi_homepage_routine;  /* for sliding operation */
	page_avenger->page_refresh_misc=refresh_misc; /* random colro for btn */

        /* 5.4 Set wallpaper */
        //page_avenger->fpath="/mmc/avenger/areana.jpg";
	#ifdef LETS_NOTE
	page_avenger->ebox->frame_img=egi_imgbuf_readfile("/home/midas-zhou/avenger/scene/areana.jpg");
	#else
	page_avenger->ebox->frame_img=egi_imgbuf_readfile("/mmc/avenger/scene/areana.jpg");
	#endif

	/* 5.5 Add ebox to home page */
	for(i=0; i<btnum; i++) 	/* 5.5.1 Add control buttons */
		egi_page_addlist(page_avenger, ebox_btns[i]);

	for(i=0; i<2; i++)	/* 5.5.2 Add time txt for time sliding bar */
		egi_page_addlist(page_avenger, ebox_CreditTxt[i]);

	egi_page_addlist(page_avenger, ebox_title);  	/* 5.5.3 Add title txt ebox */

	return page_avenger;
}


/*-----------------  RUNNER 1 --------------------------

-------------------------------------------------------*/
static void page_runner(EGI_PAGE *page)
{

}


/*--------------------------------------------------------------------
ffmotion exit
???? do NOT call long sleep function in button functions.
return
----------------------------------------------------------------------*/
static int page_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
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
static int page_decorate(EGI_EBOX *ebox)
{
	return 0;
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
void free_avengerPage(void)
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
void  avenpage_update_title(const unsigned char *title)
{
	static unsigned char strtmp[512];

	memset(strtmp,0,sizeof(strtmp));

	strncpy((char *)strtmp, (char *)title, sizeof(strtmp)-1);

        title_FTtxt->utxt=strtmp;

	egi_page_needrefresh(ebox_title->container);
}


/*-------------------------------------------------------
Update credit txt.

-------------------------------------------------------*/
void avenpage_update_creditTxt(int score, int level)
{
	char strtmp[64];

	/* update credit */
	memset(strtmp,0, sizeof(strtmp));
	snprintf(strtmp, 32-1, "SCORE: %03d", score);
	if(score>=0)
		data_CreditTxt[0]->color=WEGI_COLOR_GREEN;
	else
		data_CreditTxt[0]->color=WEGI_COLOR_RED;
	egi_push_datatxt(ebox_CreditTxt[0], strtmp, NULL);
	egi_ebox_forcerefresh(ebox_CreditTxt[0]);

	/* update level */
	memset(strtmp,0, sizeof(strtmp));
	snprintf(strtmp, 32-1, "LEVEL: %d", level);
	egi_push_datatxt(ebox_CreditTxt[1], strtmp, NULL);
	egi_ebox_forcerefresh(ebox_CreditTxt[1]);
}
