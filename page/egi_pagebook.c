/*-------------------------------------------------------------------------
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
4. add button reaction functins, mainly for creating pages.

TODO:
        1. different values for button return, and page return,
           that egi_page_routine() can distinguish.
        2. pack page activate and free action.

Midas Zhou
---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* usleep */
#include "egi.h"
#include "egi_debug.h"
#include "egi_color.h"
#include "egi_txt.h"
#include "egi_objtxt.h"
#include "egi_btn.h"
#include "egi_page.h"
#include "egi_symbol.h"
#include "egi_pagehome.h"
#include "egi_pagemplay.h"
#include "egi_pagebook.h"


static void egi_pagebook_runner(EGI_PAGE *page);
static int book_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int book_forward(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int book_backward(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);

#define BOOK_EBOX_ID    12

/*------------- [  PAGE ::   Home Book  ] -------------
create book page
Create an 240x320 size txt ebox

Return
        pointer to a page       OK
        NULL                    fail
------------------------------------------------------*/
EGI_PAGE *egi_create_bookpage(void)
{
        int i,j;

        EGI_PAGE 	*page_book;
        EGI_DATA_TXT	*book_txt;
	EGI_EBOX	*ebox_book;
        EGI_EBOX  	*title_bar;

	int txtbox_height = 320-30;
	int offx=5;			/* offset X,Y of txt to the box */
	int offy=5;

	/* 1. create book_txt */
	book_txt=egi_txtdata_new(
		offx,offy, /* offset X,Y */
      	  	(txtbox_height-offy)/sympg_ascii.symheight, /*11,int nl, lines  */
       	 	30, // 24, /*int llen, chars per line */
        	&sympg_ascii, //testfont, /*struct symbol_page *font */
        	WEGI_COLOR_BLACK /* uint16_t color */
	);

	/* 2. set book txt fpath */
	book_txt->fpath="/home/memo.txt";

	/* 3. create txt ebox */
	ebox_book= egi_txtbox_new(
		"book", 		/* tag */
        	book_txt,  		/* EGI_DATA_TXT pointer */
        	true, 			/* bool movable */
       	 	0,30, 		   	/* int x0, int y0 */
		/* OBSELETE: tricky, here create a roll-back seam */
        	240,txtbox_height, 	/* int width, int height */
        	-1, 		   	/* int frame, -1=no frame */
        	WEGI_COLOR_ORANGE  	/*int prmcolor*/
	);
	ebox_book->id=BOOK_EBOX_ID;

        /* 4. create title bar */
        title_bar= create_ebox_titlebar(
                0, 0, /* int x0, int y0 */
                0, 2,  /* int offx, int offy */
                WEGI_COLOR_GRAY, //egi_colorgray_random(light),  /* int16_t bkcolor */
                NULL    /* char *title */
        );
        egi_txtbox_settitle(title_bar, "	Today's News ");

	/*  5. create three play buttons */
        EGI_EBOX *play_btns[3]; /* 0-left, 1-exit,  2-right */
        EGI_DATA_BTN *data_btns[3];
	int sym_code[3]={11,2,10}; /*left(back),exit,right(forward)---- int icon_code for sympg_icons_2 */
	for(i=0;i<3;i++)
	{
	       	/* 5.1. create data_btns for UP ARROW */
        	data_btns[i]=egi_btndata_new(i, /* int id */
 	      		btnType_square, /* enum egi_btn_type shape */
       			&sympg_icons_2, /* struct symbol_page *icon */
       			sym_code[i], /* int icon_code for sympg_icons_2 */
			&sympg_testfont /* for ebox->tag font */
        	);
        	/* if fail, try again ... */
                if(data_btns[i]==NULL) {
                	printf("egi_create_mplaypage(): fail to call egi_btndata_new() for	\
										data_btns[%d] fails.\n",i );
                        i--;
                        continue;
                }
		/* set opaque for button icon */
		data_btns[i]->opaque=4;

        	/* 5.2. create new btn eboxes */
                play_btns[i]=egi_btnbox_new(NULL, /* put tag later */
                                            data_btns[i], /* EGI_DATA_BTN *egi_data */
                                            0, /* bool movable */
                                            10+(60+20)*i, 320-60, /* int x0, int y0 */
                                            60,60, /* int width, int height */
                                            -1, /* int frame,<0 no frame */
                                            -1 /*int prmcolor */
                                          );
               /* if fail, try again ... */
               if(play_btns[i]==NULL) {
               		printf("egi_create_bookpage(): fail to call egi_btnbox_new() for play_btns[%d] \n",i);
                        free(data_btns[i]);
                        data_btns[i]=NULL;

                        i--;
                        continue;
               }
	}

        /* 5.3 add button tags and reactions here */
        egi_ebox_settag(play_btns[0], "backward");
        play_btns[0]->reaction=book_backward;
        egi_ebox_settag(play_btns[1], "exit");
        play_btns[1]->reaction=book_exit;
        egi_ebox_settag(play_btns[2], "forward");
        play_btns[2]->reaction=book_forward;

        /* 6. create book page  */
        /* 6.1 create book page */
        page_book=egi_page_new("page_book");
        while(page_book==NULL) {
                        printf("egi_create_bookpage(): fail to call egi_page_new(), try again ...\n");
                        page_book=egi_page_new("page_book");
                        usleep(100000);
        }
        /* 6.2 put pthread runner */
        //page_mplay->runner[0]= ;

        /* 6.3 set default routine job */
        page_book->routine=egi_page_routine; /* use default */

        /* 6.4 set wallpaper */
        //no wallpaper!! page_book->fpath="/tmp/mplay.jpg";

	/* 6.5  add to page ebox list and
	   6.6  set container */
	/*----- BEWARE OF THE SEQUENCE FOR REFRESH--------*/
	egi_page_addlist(page_book,ebox_book); /* book */
	ebox_book->container=page_book;

 	egi_page_addlist(page_book,title_bar); /* title bar */
	title_bar->container=page_book;

	for(i=0;i<3;i++) { /* buttons */
	 	egi_page_addlist(page_book,play_btns[i]);
		play_btns[i]->container=page_book;
	}

	return page_book;
}


/*-----------------  RUNNER 1 --------------------------
-------------------------------------------------------*/
static void egi_pagebook_runner(EGI_PAGE *page)
{

}

/*-----------------------------------------------------------------
button forward:
other  >0 OK
       <0 fails
------------------------------------------------------------------*/
static int book_forward(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	EGI_PDEBUG(DBG_TEST,"book_forward() triggered. \n");
	EGI_EBOX *txt_ebox;

	/* get its container */
	EGI_PAGE *page=ebox->container;

	/* find the txt ebox in the page
	    egi_page_pickbtn() will verify the integrity of the page*/
	txt_ebox=egi_page_pickebox(page, type_txt, BOOK_EBOX_ID); //book_id);
	if(txt_ebox == NULL) {
		printf("[page '%s'] book_forward(): fail to find txt_ebox with id=%d .\n",
									page->ebox->tag,BOOK_EBOX_ID);
		return btnret_ERR;
	}

	/* set read direction */
	egi_txtbox_set_direct(txt_ebox, 1); /* > 0 foreward */

	/* refresh the txt ebox */
	/* !!!! as we return pgret_OK, egi_page_routine() will refresh the host page !!!! */
        return pgret_OK; /*return pgret_OK to refresh page */
}

/*------------------------------------------------------------------
button backward:
return to routine -1, (<0 for button exit,while =0 for rountine exit
other  >0 OK
       <0 fails
------------------------------------------------------------------*/
static int book_backward(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

//        egi_msgbox_create("Message:\n  Backward function not defined yet!", 1000, WEGI_COLOR_GRAY2);

	EGI_PDEBUG(DBG_TEST,"book_backward() triggered. \n");
	EGI_EBOX *txt_ebox;

	/* get its container */
	EGI_PAGE *page=ebox->container;

	/* find the txt ebox in the page
	 *   egi_page_pickbtn() will verify the integrity of the page
         */
	txt_ebox=egi_page_pickebox(page, type_txt, BOOK_EBOX_ID); //book_id);
	if(txt_ebox == NULL) {
		printf("[page '%s'] book_backward(): fail to find txt ebox with id=%d .\n",
									page->ebox->tag,BOOK_EBOX_ID);
		return btnret_ERR;
	}

	/* set read direction */
	egi_txtbox_set_direct(txt_ebox, -1); /* < 0 backward */

        return pgret_OK; /* return pgret_OK to refresh page */
}

/*-------------------------------------------------------
btn_close function:
return to routine -1,  (while =0 for rountine exit)
other  >0 OK
       <0 fails
---------------------------------------------------------*/
static int book_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{

        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

        /* create page and load the page */
	return btnret_REQUEST_EXIT_PAGE;
}


