/*--------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Note:
1. Do not slide the button too fast,
2. A button may be insensitive to a double click !!!!
   Suppose that it has single-click reaction method defined, and it takes long enough time.
   After that, the second pressing event usually passes away when the routine process turns
   back to receive the touch data.

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
------------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* usleep */
#include "egi.h"
#include "egi_color.h"
#include "egi_txt.h"
#include "egi_objtxt.h"
#include "egi_btn.h"
#include "egi_page.h"
#include "egi_symbol.h"
#include "egi_fbgeom.h"


static int colorbtn_react(EGI_EBOX * ebox, EGI_TOUCH_DATA *  touch_data);
static int pageslide_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);


/*---------- [  PAGE ::  Test Slide Bar ] ---------
1. create eboxes for 6 buttons and 1 title bar
2. create Slide TEST page

Return
	pointer to a page	OK
	NULL			fail
------------------------------------------------------*/
EGI_PAGE *egi_create_slidepage(void)
{
	int i,j;

	EGI_EBOX *btns[6];
	EGI_DATA_BTN *data_btns[6];

	/* --------- 1. create buttons --------- */
        for(i=0;i<2;i++) /* row of buttons*/
        {
                for(j=0;j<3;j++) /* column of buttons */
                {
			/* 1. create new data_btns */
			data_btns[3*i+j]=egi_btndata_new(3*i+j, /* int id */
							btnType_circle, /* enum egi_btn_type shape */
							NULL, /* struct symbol_page *icon */
							0, /* int icon_code */
							&sympg_testfont /* for ebox->tag font */
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
			btns[3*i+j]=egi_btnbox_new(NULL, /* put tag later */
							data_btns[3*i+j], /* EGI_DATA_BTN *egi_data */
				        		true, /* bool movable */
						        15+(15+60)*j, 210+(20+60)*i, /* int x0, int y0 */
							60,120, /* int width, int height */
				       			0, /* int frame,<0 no frame */
		       					egi_color_random(color_medium) /*int prmcolor */
						   );
			/* if fail, try again ... */
			if(data_btns[3*i+j]==NULL)
			{
				printf("egi_create_mplaypage(): fail to call egi_btnbox_new() for home_btns[%d]. retry...\n", 3*i+j);
				free(data_btns[3*i+j]);
				data_btns[3*i+j]=NULL;

				i--;
				continue;
			}
		}
	}

	/* add tags and reaction function here */
	egi_ebox_settag(btns[0], "Red");
	btns[0]->prmcolor=WEGI_COLOR_RED;
	btns[0]->reaction=colorbtn_react;

	egi_ebox_settag(btns[1], "Green");
	btns[1]->prmcolor=WEGI_COLOR_GREEN;
	btns[1]->reaction=colorbtn_react;

	egi_ebox_settag(btns[2], "Blue");
	btns[2]->prmcolor=WEGI_COLOR_BLUE;
	btns[2]->reaction=colorbtn_react;

	egi_ebox_settag(btns[3], "Close");
	btns[3]->reaction=pageslide_exit;

	egi_ebox_settag(btns[4], "BTN_4");
	egi_ebox_settag(btns[5], "BTN_5");

	/* hide 4,5 btns */
	draw_line(&gv_fb_dev,19,319,61,319);

	/* --------- 2. create title bar --------- */
	EGI_EBOX *title_bar= create_ebox_titlebar(
	        0, 0, /* int x0, int y0 */
        	0, 2,  /* int offx, int offy */
		egi_colorGray_random(color_medium), //light),  /* int16_t bkcolor */
    		NULL	/* char *title */
	);
	egi_txtbox_settitle(title_bar, "      Test Sliding Bar ");


	/* --------- 3. create slide_test page ------- */
	/* 3.1 create page */
	EGI_PAGE *page_slide=NULL;
	page_slide=egi_page_new("page_slide");
	while(page_slide==NULL)
	{
			printf("egi_create_slidepage(): fail to call egi_page_new(), try again ...\n");
			page_slide=egi_page_new("page_slide");
			usleep(100000);
	}
	page_slide->ebox->prmcolor=egi_colorGray_random(color_light);
	page_slide->fpath="/tmp/slide.jpg";

        /* 3.2 put pthread runner */
        //page_mplay->runner[0]= ;

        /* 3.3 set default routine job */
        page_slide->routine=egi_page_routine; /* use default */

        /* 3.4 set wallpaper */
        //page_slide->fpath="/tmp/mplay.jpg";

	/* add ebox to home page */
	for(i=0;i<6;i++) /* buttons */
		egi_page_addlist(page_slide, btns[i]);

	egi_page_addlist(page_slide, title_bar); /* title bar */

	return page_slide;
}


/*-----------------  RUNNER 1 --------------------------

-------------------------------------------------------*/
static void egi_pageslide_runner(EGI_PAGE *page)
{

}

/*------------------------------------------------------------------
btn_close function:
return
----------------------------------------------------------------------*/
static int pageslide_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	return btnret_REQUEST_EXIT_PAGE;
}

/*-------------------------------------------------------------------
button sliding
return
---------------------------------------------------------------------*/
static int colorbtn_react(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        int ty=0;
	int dy=0;
	int nbt; /* nbt=0: RED,  nbt=1: GREE   nbt=2: BLUE */
	char string[50]={0};
	uint16_t color;

	EGI_DATA_TXT *data_txt=ebox->egi_data;
	nbt=data_txt->id;

	if(nbt>2)
	{
		printf("colorbtn_react(): nbt>2 --------\n");
		return 1;
	}
	static int mark[3];//={220,220,220};
	static uint8_t r,g,b;

	if(touch_data->status==db_pressing)
	{
		printf("colorbtn_react(): double click, end the test!\n");
		egi_msgbox_create("Message:\n\n    End sliding bar test.", 1000, WEGI_COLOR_ORANGE);
		return btnret_REQUEST_EXIT_PAGE;/* exit the page  */
	}

	/* set mark when press down, !!!! egi_touch_getdata() may miss this status !!! */
	if(touch_data->status==pressing)
	{
		printf("colorbtn_react(): redbtn is pressing ....\n");
		mark[nbt]=ebox->y0;
	}
	else if(touch_data->status==pressed_hold)
	{
		//printf("colorbtn_react(): touch_data->dy=%d \n", touch_data->dy);
                ty = mark[nbt]+touch_data->dy;
                // set limit, range 220-75=145
                if(ty<95)ty=95;
                if(ty>210)ty=210;

		if(nbt==0) /*red btn ebox:  adjust 5bits red value */
		{
			r=(210-ty+1)*(32-1)/115;
			r=31/3+r*2/3; /* set Mini. value,so it will not be too dark */
			//printf("ty=%d, r=%d\n",ty,r);
			ebox->prmcolor=r<<11;
		}
		if(nbt==1) /*green btn ebox: adjust 6bits green value */
		{
			g=(210-ty+1)*(64-1)/115;
			g=63/3+g*2/3; /* set Mini. value */
			//printf("ty=%d, g=%d\n",ty,g);
			ebox->prmcolor=g<<5;
		}
		if(nbt==2) /*blue btn ebox:  adjust 5bits blue value */
		{
			b=(210-ty+1)*(32-1)/115;
			b=31/3+b*2/3; /* set Mini. value */
			//printf("ty=%d, b=%d\n",ty,b);
			ebox->prmcolor=b;
		}
		//printf("r, g, b = %d, %d, %d \n",r,g,b);
		/* draw block*/
		color=COLOR_RGB_TO16BITS( r<<3,g<<2,b<<3 );
		fbset_color(color);
		draw_filled_rect(&gv_fb_dev,0,30,240-1,30+60-1);
		fbset_color(0);
		draw_rect(&gv_fb_dev,0,30,240-1,30+60-1);
		/* write color value */
		sprintf(string,"16bit_RGB: 0x%X",color);
		symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, 0, 1, 5, 32, string, -1);

		/* update ebox parameters */
		ebox->y0 = ty;
		ebox->need_refresh=true;
		ebox->refresh(ebox);
	}

	return btnret_OK; /* 0 refresh container page */
}

