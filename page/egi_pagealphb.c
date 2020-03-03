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


static int page_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);


/*------- [  PAGE ::  Alphabet input ] ---------
Return
	pointer to a page	OK
	NULL			fail
--------------------------------------------------*/
EGI_PAGE *create_pagealpha(void)
{
	int i,j;
	int btnW=40;
	int btnH=40;
	char strtag[2]={0,0};

	EGI_EBOX *alpha_btns[36];
	EGI_DATA_BTN *data_btns[36];
	//EGI_16BIT_COLOR color;

	/* --------- 1. create buttons --------- */
        for(i=0;i<6;i++) /* row of buttons*/
        {
                for(j=0;j<6;j++) /* column of buttons */
                {
			/* 1. create new data_btns */
			data_btns[6*i+j]=egi_btndata_new(6*i+j, /* int id */
							btnType_square, /* enum egi_btn_type shape */
							NULL, /* struct symbol_page *icon */
							0, /* int icon_code */
							&sympg_testfont /* for ebox->tag font */
						);
			/* if fail, try again ... */
			if(data_btns[6*i+j]==NULL)
			{
				printf("%s: fail to call egi_btndata_new() for data_btns[%d]. retry...\n",
											  __func__, 6*i+j);
				i--;
				continue;
			}

			/* to show tag on the button */
			data_btns[6*i+j]->showtag=true;

			/* 2. create new btn eboxes */
			alpha_btns[6*i+j]=egi_btnbox_new(NULL, /* put tag later */
							data_btns[6*i+j], /* EGI_DATA_BTN *egi_data */
				        		0, /* bool movable */
						        40*j, 80+40*i, /* int x0, int y0 */
							btnW,btnH, /* int width, int height */
				       			0,//1, /* int frame,<0 no frame */
		       					egi_color_random(color_deep) //color,/*int prmcolor */
					   );
			/* if fail, try again ... */
			if(alpha_btns[6*i+j]==NULL)
			{
				printf("%s: fail to call egi_btnbox_new() for alpha_btns[%d]. retry...\n",
								__func__, 6*i+j);
				free(data_btns[6*i+j]);
				data_btns[6*i+j]=NULL;

				i--;
				continue;
			}
			/* set tag and color */
			strtag[0]=6*i+j+41;
			egi_ebox_settag(alpha_btns[6*i+j],strtag);
			alpha_btns[6*i+j]->tag_color=WEGI_COLOR_WHITE;
		}
	}

	/* add tags and reaction function here */
//	egi_ebox_settag(alpha_btns[0], "Prev");
//	alpha_btns[0]->reaction=react_prev;


	/* --------- 2. create alphabet page ------- */
	/* 2.1 create alphabet page */
	EGI_PAGE *page_alpha=NULL;
	page_alpha=egi_page_new("page_alphabet");
	while(page_alpha==NULL)
	{
			printf("%s: fail to call egi_page_new(), try again ...\n",__func__);
			page_alpha=egi_page_new("page_alpha");
			usleep(100000);
	}
	page_alpha->ebox->prmcolor=WEGI_COLOR_BLACK; //egi_colorgray_random(light);

	/* 2.2 put pthread runner, remind EGI_PAGE_MAXTHREADS 5  */
        //page_alpha->runner[0]=check_volume_runner;

        /* 2.3 set default routine job */
        page_alpha->routine=egi_page_routine; /* use default */

        /* 2.4 set wallpaper */
        //page_alpha->fpath="/tmp/mplay_neg.jpg";

	/* add ebox to home page */
	for(i=0;i<36;i++) /* buttons */
		egi_page_addlist(page_alpha, alpha_btns[i]);
	//egi_page_addlist(page_alpha, title_bar); /* title bar */

	return page_alpha;
}



/*---------------------------------------
	Button OK reaction func
---------------------------------------*/
static int react_OK(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

        return btnret_OK; /* */
}


/*-------------------------------------------
	Button Close function
--------------------------------------------*/
static int page_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{

       /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

        return btnret_REQUEST_EXIT_PAGE; /* >=00 return to routine; <0 exit this routine */
}

