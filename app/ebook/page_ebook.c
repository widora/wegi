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

[Y30-Y260]
{0,30}, {240-1, 260}            --- Image/subtitle Displaying Zone
[Y150-Y260] Sub_displaying

[Y150-Y265]
{0,150}, {240-1, 260}           --- box area for subtitle display

[Y266-Y319]
{0,266}, {240-1, 320-1}         --- Buttons

TODO:
	1. To exit and free APP gracefully.

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#include "egi_common.h"
#include "egi_cstring.h"
#include "egi_FTsymbol.h"
#include "page_ebook.h"

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

static uint16_t btn_symcolor;

static int ebook_prev(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int ebook_playpause(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int ebook_next(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int ebook_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int ebook_hangup(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int ebook_decorate(EGI_EBOX *ebox);


/* ebook file */
static int fd=-1;;
static int fsize;
static struct stat sb;
static const unsigned char *faddr;

/* ebook displaying */
static const char *fpath="/mmc/xyj_uft8.txt";

static  wchar_t *title=L"《西 游 记》";
static  int fh,fw; /* font Height,Width */
static  int lines;
static  int pixpl;
static  int gap;   /* line gap */
static  int offp;
static  int x0,y0;
static  int wtotal;
static  int nwrite;
static  bool refresh_ebook;
static  EGI_FILO *filo;


/*----------------------------------------------------------
 for page->page_refresh_misc()
 Remind that we are at page_refresh_misc(), which is carried
 out at last of page refresh operation
----------------------------------------------------------*/
static int page_refresh_ebook(EGI_PAGE *page)
{

    /* 1. If (fd<0) open and mmap file */
    if(fd<0) {

	EGI_PLOG(LOGLV_TEST,"%s: Start to open and MMAP file %s.", __func__, fpath);

	/* init params */
   	x0=5;		/* Offset */
	y0=30;
   	fw=19;		/* font size */
	fh=19;
	pixpl=240-x0;	/* pixles per line */
	offp=0;		/* file position offset */
   	lines=12;	/* number of txt lines */
   	gap=5;		/* Gap between txt lines */

	/* init filo */
	if(filo==NULL) {
	        filo=egi_malloc_filo( 1<<10, sizeof(int), 0b01); //|0b10 ); /* enable double/halve realloc */
	        if(filo==NULL) {
			EGI_PLOG(LOGLV_ERROR,"%s:Fail to init EGI FILO, quit.", __func__);
        	        return -1;
        	}
	}

        /* 1.1 open wchar book file */
        fd=open(fpath,O_RDONLY);
        if(fd<0) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to open %s.", __func__, fpath);
                perror("---open file---");
                return -1;
        }
        /* 1.2 obtain file stat, OR fseek(fd, 0L, SEEK_END); fsize=ftell(fp); */
        if( fstat(fd,&sb)<0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to call fstat() for file size.", __func__);
                perror("---fstat---");
                return -2;
        }
        fsize=sb.st_size;
        /* 1.3 mmap txt file */
        faddr=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
        if(faddr==MAP_FAILED) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to call mmap() for file %s.", __func__, fpath);
                perror("---mmap---");
                return -3;
        }

        /* 1.4 get total wchars in the book */
        wtotal=cstr_strcount_uft8(faddr);
	EGI_PLOG(LOGLV_TEST,"%s: Finish open and MMAP file %s, totally %d characters in the book.",
										__func__, fpath, wtotal);

        /* 1.5 write book title  */
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,  	/* FBdev, fontface */
                                          35, 35, title,               	/* fw,fh, pstr */
                                          240, 1,  gap,           	/* pixpl, lines, gap */
                                          30, 120,                      /* x0,y0, */
                                          WEGI_COLOR_BLACK, -1, -1 );   /* fontcolor, stranscolor,opaque */
   }


#if 1 /* TODO: TXT display area can NOT be auto. refreshed, to define it as a txt_EBOX! later. */
   /* 2. If (fd>0), write to FB from the last start_reading position of the book */
   else if(refresh_ebook==true) {
	/* reel back to the last start_reading position */
	//offp = offp-nwrite>0 ? offp-nwrite : 0;
        /* write book content: UFT-8 string to FB */
	egi_filo_push(filo,(void *)(&offp)); /* push current starting offp */
       	nwrite=FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.regular,   /* FBdev, fontface */
                                          fw, fh, faddr+offp,     	 	 /* fw,fh, pstr */
                                          pixpl-x0, lines,  gap,         /* pixpl, lines, gap */
                                          x0, y0,                      	 /* x0,y0, */
                                          WEGI_COLOR_BLACK, -1, -1 );    /* fontcolor, stranscolor,opaque */
	offp+=nwrite;

	refresh_ebook=false;
   }
#endif

  return 0;
}


/*---------- [  PAGE ::  EBOOK ] ---------
1. create eboxes for 4 buttons and 1 title bar

Return
	pointer to a page	OK
	NULL			fail
------------------------------------------------------*/
EGI_PAGE *create_ebook_page(void)
{
	int i;
	int btnum=5;
	EGI_EBOX *ebook_btns[5];
	EGI_DATA_BTN *data_btns[5];

	/* --------- 1. create buttons --------- */
        for(i=0;i<btnum;i++) /* row of buttons*/
        {
		/* 1. create new data_btns */
		data_btns[i]=egi_btndata_new(	i, 		 /* int id */
						btnType_square,  /* enum egi_btn_type shape */
						&sympg_sbuttons, /* struct symbol_page *icon. If NULL, use geometry. */
						0, 		 /* int icon_code, assign later.. */
						&sympg_testfont  /* for ebox->tag font */
					);
		/* if fail, try again ... */
		if(data_btns[i]==NULL)
		{
			EGI_PLOG(LOGLV_ERROR,"%s: Fail to call egi_btndata_new() for data_btns[%d]. retry...", i);
			i--;
			continue;
		}

		/* Do not show tag on the button */
		data_btns[i]->showtag=false;
		/* set opaque value */
		data_btns[i]->opaque=35;

		/* 2. create btn eboxes */
		ebook_btns[i]=egi_btnbox_new(   NULL, 		  /* put tag later */
						data_btns[i], 	  /* EGI_DATA_BTN *egi_data */
				        	false, 		  /* bool movable */
					        48*i, 320-(60-5), /* int x0, int y0 */
						48, 60, 	  /* int width, int height */
				       		0, 		  /* int frame,<0 no frame */
		       				egi_color_random(color_medium) /*int prmcolor, for geom button only. */
					   );
		/* if fail, try again ... */
		if(ebook_btns[i]==NULL)
		{
			EGI_PLOG(LOGLV_ERROR, "%s: Fail to call egi_btnbox_new() for ebook_btns[%d]. retry...",
												 __func__, i);
			free(data_btns[i]);
			data_btns[i]=NULL;
			i--;
			continue;
		}
	}

	/* get a random color for the icon */
//	btn_symcolor=egi_color_random(medium);
//	EGI_PLOG(LOGLV_INFO,"%s: set 24bits btn_symcolor as 0x%06X \n",	__FUNCTION__, COLOR_16TO24BITS(btn_symcolor) );
	btn_symcolor=WEGI_COLOR_WHITE;//ORANGE;

	/* add tags, set icon_code and reaction function here */
	egi_ebox_settag(ebook_btns[0], "Prev");
	data_btns[0]->icon_code=(btn_symcolor<<16)+ICON_CODE_PREV; /* SUB_COLOR+CODE */
	ebook_btns[0]->reaction=ebook_prev;

	egi_ebox_settag(ebook_btns[1], "Playpause");
	data_btns[1]->icon_code=(btn_symcolor<<16)+ICON_CODE_PAUSE; /* default status is playing*/
	ebook_btns[1]->reaction=ebook_playpause;

	egi_ebox_settag(ebook_btns[2], "Next");
	data_btns[2]->icon_code=(btn_symcolor<<16)+ICON_CODE_NEXT;
	ebook_btns[2]->reaction=ebook_next;

	egi_ebox_settag(ebook_btns[3], "Hangup");
	data_btns[3]->icon_code=(btn_symcolor<<16)+ICON_CODE_EXIT;
	ebook_btns[3]->reaction=ebook_hangup;

	egi_ebox_settag(ebook_btns[4], "Exit");
	data_btns[4]->icon_code=(btn_symcolor<<16)+ICON_CODE_LOOPALL;
	ebook_btns[4]->reaction=ebook_exit;

	/* --------- 2. create title bar --------- */
#if 0
	EGI_EBOX *title_bar= create_ebox_titlebar(
	        0, 0, /* int x0, int y0 */
        	0, 2,  /* int offx, int offy, offset for txt */
		WEGI_COLOR_GRAY, //egi_colorgray_random(medium), //light),  /* int16_t bkcolor */
    		NULL	/* char *title */
	);
	egi_txtbox_settitle(title_bar, "	eFFplay V0.0 ");
#endif

	/* --------- 3. create page ebook ------- */
	/* 3.1 create page ebook */
	EGI_PAGE *page_ebook=egi_page_new("page_ebook");
	while(page_ebook==NULL)
	{
		printf("%s: fail to call egi_page_new(), try again ...\n", __func__);
		page_ebook=egi_page_new("page_ebook");
		tm_delayms(10);
	}
	/* of BK color, if NO wallpaper defined */
	page_ebook->ebox->prmcolor=WEGI_COLOR_BLACK;
	/* decoration */
	page_ebook->ebox->method.decorate=ebook_decorate;

        /* 3.2 Put pthread runner */
//        page_ebook->runner[0]= ;

        /* 3.3 set default routine job */
        page_ebook->routine=egi_page_routine; /* use default routine function */

        /* 3.4 Set wallpaper */
        page_ebook->fpath="/mmc/bookbk.png";

	/* 3.5 Assign misc. refresh job for page */
	page_ebook->page_refresh_misc=page_refresh_ebook;

	/* 3.6 Add eboxes to home page */
	for(i=0;i<btnum;i++) /* add buttons */
		egi_page_addlist(page_ebook, ebook_btns[i]);

//	egi_page_addlist(page_ebook, title_bar); /* add title bar */

	return page_ebook;
}


/*-----------------  RUNNER 1 --------------------------

-------------------------------------------------------*/
static void ebook_runner(EGI_PAGE *page)
{

}

/*--------------------------------------------------------------------
ebook PREV
return
----------------------------------------------------------------------*/
static int ebook_prev(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	/* reel back to the previous offp */
	egi_filo_pop(filo, NULL);
	egi_filo_pop(filo,(void *)&offp);

	refresh_ebook=true;

	return pgret_OK;	/* Force to refresh PAGE */
}

/*--------------------------------------------------------------------
ebook palypause
return
-------------------------------------------------------------------*/
static int ebook_playpause(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	return btnret_OK;
}

/*-----------------------------------------------------------
ebook NEXT
return
-----------------------------------------------------------*/
static int ebook_next(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	refresh_ebook=true;

	return pgret_OK; 	/* fore to refresh page */
}

/*--------------------------------------------------------------------
ebook Display Mode
return
----------------------------------------------------------------------*/
static int ebook_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	static int count=0;

        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	return btnret_REQUEST_EXIT_PAGE;
}

/*--------------------------------------------------------------------
EBOOK hangup and put to background.
return
----------------------------------------------------------------------*/
static int ebook_hangup(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

#if 1
	/*TEST: send HUP signal to iteself */
	if(raise(SIGUSR1) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to send SIGUSR1 to itself.",__func__);
	}
	EGI_PLOG(LOGLV_ERROR,"%s: Finish rasie(SIGSUR1), return to page routine...",__func__);

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

	/* set ebook need_refresh token here */
	offp-=nwrite;		/* Reel back reading position */
	refresh_ebook=true;
	if(offp<0)offp=0;

	/* need refresh page, a trick here to activate the page after CONT signal */
	return pgret_OK;

#else
        egi_msgbox_create("Message:\n   Click! Start to exit page!", 300, WEGI_COLOR_ORANGE);
        return btnret_REQUEST_EXIT_PAGE;  /* end process */
#endif
}


/*-----------------------------------------
	Decoration for the page
-----------------------------------------*/
static int ebook_decorate(EGI_EBOX *ebox)
{
	/* bkcolor for bottom buttons */
//	fbset_color(WEGI_COLOR_GRAY3);
//        draw_filled_rect(&gv_fb_dev,0,266,239,319);

	return 0;
}


/*-----------------------------
Release and free all resources
------------------------------*/
void free_ebook_page(void)
{
   if(fd>0) {
	 close(fd);
	 fd=-1;
	 munmap(faddr, fsize);
	 faddr=NULL;
   }

   if(filo != NULL) {
	egi_free_filo(filo);
	filo=NULL;
   }


   /* all EBOXs in the page will be release by calling egi_page_free()
    *  just after page routine returns.
    */

}
