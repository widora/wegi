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
   1.5 if the function needs to hook to an ebox(btn etc), the arguments to
	be inform of  int (*reaction)(EGI_EBOX *, EGI_TOUCH_DATA touch_data).

2. thread-runner functions.
3. egi_XXX_routine() function if not use default egi_page_routine().
4. button reaction functins

Midas Zhou
---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* usleep */
#include "egi.h"
#include "egi_log.h"
#include "egi_color.h"
#include "egi_txt.h"
#include "egi_objtxt.h"
#include "egi_btn.h"
#include "egi_touch.h"
#include "egi_page.h"
#include "egi_symbol.h"
#include "egi_pageopenwrt.h"
#include "egi_objlist.h"
#include "egi_pageslide.h"

static void egi_pagetest_runner(EGI_PAGE *page);
static int egi_pagetest_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA *touch_data);
static int egi_dbclick_test(EGI_EBOX * ebox, EGI_TOUCH_DATA *touch_data);
static int egi_slidebar_test(EGI_EBOX * ebox, EGI_TOUCH_DATA *touch_data);


/*------ [  PAGE  ::  OpenWRT System Information ] ------
1. create eboxes for 6 buttons and 1 title bar
2. create OpenWRT system page

Return
	pointer to a page	OK
	NULL			fail
--------------------------------------------------------*/
EGI_PAGE *egi_create_testpage(EGI_EBOX * ebox, EGI_TOUCH_DATA *touch_data)
{
	int i,j;

	EGI_EBOX *test_btns[5];
	EGI_DATA_BTN *data_btns[5];

	/* --------- 1. create buttons --------- */
        for(j=0;j<5;j++) /* column of buttons */
        {
		/* 1. create new data_btns */
		data_btns[j]=egi_btndata_new(   j, /* int id */
						btnType_square, /* enum egi_btn_type shape */
						NULL, /* for icon, struct symbol_page *icon */
						0, /* int icon_code */
						&sympg_testfont /* for ebox->tag font */
					);
		/* if fail, try again ... */
		if(data_btns[j]==NULL)
		{
			printf("egi_create_mplaypage(): fail to call egi_btndata_new() for	\
									data_btns[%d]. retry...\n", j);
			j--;
			continue;
		}

                /* to show tag on the button */
                data_btns[j]->showtag=true;

		/* 2. create new btn eboxes */
		test_btns[j]=egi_btnbox_new(NULL, /* put tag later */
					data_btns[j], /* EGI_DATA_BTN *egi_data */
			       		0, /* bool movable */
				        30, 50+(40+15)*j, /* int x0, int y0 */
					180,40, /* int width, int height */
			 		0, /* int frame,<0 no frame */
		       			WEGI_COLOR_GRAY//egi_color_random(medium) /*int prmcolor */
				  );

		/* if fail, try again ... */
		if(data_btns[j]==NULL)
		{
			printf("egi_create_openwrtpage(): fail to call egi_btnbox_new() for open_btns[%d]. retry...\n", 3*i+j);
			free(data_btns[j]);
			data_btns[j]=NULL;
			j--;
			continue;
		}
	}

	/* ----- 1.1 add tags and button reaction function here ------ */
	egi_ebox_settag(test_btns[0], "Ebox_TXT Demon");
	test_btns[0]->reaction=egi_txtbox_demo;

	egi_ebox_settag(test_btns[1], "List Demon");
	test_btns[1]->reaction=egi_listbox_test;

	egi_ebox_settag(test_btns[2], "Double Click");
	test_btns[2]->reaction=egi_dbclick_test;

	egi_ebox_settag(test_btns[3], "Sliding Bar Test");
	test_btns[3]->reaction=egi_slidebar_test;

	egi_ebox_settag(test_btns[4], "EXIT");
	test_btns[4]->reaction=egi_pagetest_exit;


	/* --------- 2. create title bar --------- */
	EGI_EBOX *title_bar= create_ebox_titlebar(
	        0, 0, /* int x0, int y0 */
        	0, 2,  /* int offx, int offy */
		//egi_color_random(light),  /* uint16_t bkcolor */
		egi_colorGray_random(color_light),  /* uint16_t bkcolor */
    		NULL	/* char *title */
	);
	egi_txtbox_settitle(title_bar, "      Test Functions");


	/* --------- 3. create test page ------- */
	EGI_PAGE *page_test=NULL;
	page_test=egi_page_new("page_test");
	while(page_test==NULL)
	{
			printf("egi_create_testpage(): fail to call egi_page_new(), try again ...\n");
			page_test=egi_page_new("page_test");
			usleep(100000);
	}
	/* set page prmcolor */
	page_test->ebox->prmcolor= egi_colorGray_random(color_medium);

	/* set wallpaper */
	//page_openwrt->fpath="/tmp/mplay.jpg";

	/* add ebox to home page */
	for(i=0;i<5;i++) /* buttons */
		egi_page_addlist(page_test, test_btns[i]);
	egi_page_addlist(page_test, title_bar); /* title bar */

	return page_test;
}



/*-----------------  RUNNER 1 --------------------------

-------------------------------------------------------*/
static void egi_pagetest_runner(EGI_PAGE *page)
{

}

/*-----------------------------------
btn_close function:
return
-----------------------------------*/
static int egi_pagetest_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)// enum egi_touch_status btn_status)
{
        if( touch_data->status==pressing )
        {
                return btnret_REQUEST_EXIT_PAGE;
        }

        return btnret_IDLE;
}


/*-----------------------------------------------------------
int (*reaction)(EGI_EBOX *, enum egi_touch_status);
egi_dbclick_test()
-----------------------------------------------------------*/
static int egi_dbclick_test(EGI_EBOX * ebox , EGI_TOUCH_DATA * touch_data)//enum egi_touch_status btn_status)
{
	if(touch_data->status!=db_pressing)
		return btnret_IDLE;

	egi_msgbox_create("Message:\n    Double Click!", 1000, WEGI_COLOR_ORANGE);
	return btnret_OK;
}


/*-----------------------------------------------------------------------------
button 'sliding bar test' function:
a Page function
------------------------------------------------------------------------------*/
static int egi_slidebar_test(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	/* bypass unwanted touch */
	if(touch_data->status != pressing )
		return btnret_IDLE;

        EGI_PAGE *page_slide=egi_create_slidepage();
        EGI_PLOG(LOGLV_INFO,"[page '%s'] is created.\n", page_slide->ebox->tag);

	/* activate page, refresh image */
        egi_page_activate(page_slide);
        EGI_PLOG(LOGLV_INFO,"[page '%s'] is activated.\n", page_slide->ebox->tag);

        /* get into routine loop */
        EGI_PLOG(LOGLV_INFO,"trap into routine of [page '%s'] ...\n", page_slide->ebox->tag);
        page_slide->routine(page_slide);

        /* exit a button activated page, set refresh flag for the host page, before page freeing.*/
//        egi_page_needrefresh(ebox->container);

        /* get out of routine loop */
        EGI_PLOG(LOGLV_INFO,"Exit routine of [page '%s'], start to free the page...\n", page_slide->ebox->tag);
        egi_page_free(page_slide);


        return pgret_OK; /* button activated page */
}





