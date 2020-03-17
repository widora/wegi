/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A demo for simple rectangular buttons. It creates a switch button
and a bounce_back button, then test it's reactions.

NOTES:
1. For a switch/toggle button:
   When it is touched, it will keep pressed_down status though pen up,
   until another touch to switch back to released status.

2. For a bounce_back button:
   It keeps pressed_down status only when it is being touched/pressed,
   and will bounce back to released status once you raise up pen/finger.

3. A black banner will show up at the bottom of the screen when a
   button is touched, to indicate the type of the button. and with
   restore to back ground color when the button is released.

4. Add your command at reaction functions and a simple system() call is
   a quick workaround.

5. Note: A more beautiful button is usually an (PNG/JPG) image icon.

Midas Zhou
midaszhou@yahoo.com
--------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_FTsymbol.h>

#define PANEL_BKGCOLOR 	WEGI_COLOR_DARKGRAY 	/* The panel back ground color */

#define ID_ONOFF	0
#define	ID_NEXT		1
#define ID_MODE		2

static EGI_RECTBTN	  btns[3];
static EGI_IMGBUF	  *blockimg=NULL;		/* To store save banner area image */
static int play_mode;					/* 0- radio, 1-mp3 */

void writeFB_button(EGI_RECTBTN *btn0, char *tag, EGI_16BIT_COLOR tagcolor);
void switch_button_react(EGI_RECTBTN *btn);
void bounceback_button_react(EGI_RECTBTN *btn);
void ripple_mark(EGI_POINT touch_pxy, uint8_t alpha, EGI_16BIT_COLOR color);


int main(int argc, char **argv)
{

 /* <<<<<<  EGI general init  >>>>>> */

  /* Start sys tick */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();
  #if 0
  /* Start egi log */
  printf("egi_init_log()...\n");
  if(egi_init_log("/mmc/log_scrollinput")!=0) {
        printf("Fail to init egi logger, quit.\n");
        return -1;
  }
  /* Load symbol pages */
  printf("FTsymbol_load_allpages()...\n");
  if(FTsymbol_load_allpages() !=0) /* FT derived sympg_ascii */
        printf("Fail to load FTsym pages! go on anyway...\n");
  printf("symbol_load_allpages()...\n");
  if(symbol_load_allpages() !=0) {
        printf("Fail to load sym pages, quit.\n");
        return -1;
  }
  #endif
  /* Load freetype fonts */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
        printf("Fail to load FT sysfonts, quit.\n");
        return -1;
  }
  #if 0
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
        printf("Fail to load FT appfonts, quit.\n");
        return -1;
  }
  #endif
  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;

  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
        return -1;

  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,false);
  fb_position_rotate(&gv_fb_dev,3);

 /* <<<<<  End of EGI general init  >>>>>> */



	    		  /*-----------------------------------
    			   *            Main Program
    			   -----------------------------------*/

  int i;
  EGI_IMGBUF	  *bkimg=NULL;  	/* Backgroud image */
  EGI_TOUCH_DATA  touch_data;
  EGI_IMGBUF	  *bannerMask=NULL;	/* A banner mask */


  /* Init. bannerimg */
  bannerMask=egi_imgbuf_create( 240-190, 320, 55, WEGI_COLOR_BLACK);//DARKGRAY); /* height, width, alpha, color */
  if(bannerMask==NULL)
	printf("Fail to create bannerMask!\n");

  /* Assign buttons' attributors */
  for(i=0; i<3; i++) {
	btns[i].id=i;
  	btns[i].x0=50;		btns[i].y0=10+60*i;
	btns[i].width=120;	btns[i].height=50;
	btns[i].sw=4;
	btns[i].reaction=(i%2==0?switch_button_react:bounceback_button_react);
	btns[i].pressed=false;
  }
  btns[ID_ONOFF].color=COLOR_24TO16BITS(0x66cc99);	/* ON /OFF */
  btns[ID_NEXT].color=COLOR_24TO16BITS(0x6666ff);	/* NEXT */
  btns[ID_MODE].color=COLOR_24TO16BITS(0xff6699);	/* Mode: Radio/MP3 */

  /* Clear screen and init. scene */
  if( (bkimg=egi_imgbuf_readfile("/mmc/linux.jpg")) != NULL ) {
	/* put a mask on bkimg for banner */
	egi_imgbuf_copyBlock(bkimg, bannerMask, true, 320, 240-190, 0, 190, 0, 0); /* destimg, srcimg, blendON, bw, bh, xd, yd, xs, ys */
	/* writeFB bkimg */
	egi_subimg_writeFB(bkimg, &gv_fb_dev, 0, -1, 0, 0);  /* img, fbdev, subnum, subcolor, x0, y0 */
	/* Save banner area */
	blockimg=egi_imgbuf_blockCopy(bkimg, 0, 190, 240-190, 320);  /* ineimg, px, py, height, width */
  }
  else  /* If no bkimage then use pure color */
  	fb_clear_backBuff(&gv_fb_dev, PANEL_BKGCOLOR);

  /* Put buttuns */
  writeFB_button(&btns[0], "OFF", WEGI_COLOR_BLACK);
  writeFB_button(&btns[1], "NEXT", WEGI_COLOR_BLACK);
  writeFB_button(&btns[2], "RADIO", WEGI_COLOR_BLACK);

  /* Bring to screen */
  fb_page_refresh(&gv_fb_dev, 0);

  /* Loop touch events and button reactions: Assume that only one button may be touched each time!  */
  while(1) {
  	if(egi_touch_timeWait_press(-1, 0, &touch_data)!=0)
		continue;
        /* Touch_data converted to the same coord as of FB */
        egi_touch_fbpos_data(&gv_fb_dev, &touch_data);

	/* Check if touched any of the buttons */
	for(i=0; i<3; i++) {
		if( egi_touch_on_rectBTN(&touch_data, &btns[i]) && touch_data.status==pressing ) {  /* Re_check event! */
			printf("Button_%d touched!\n",i);
			btns[i].reaction(&btns[i]);
	  		fb_page_refresh(&gv_fb_dev, 0);
			break;
		}
	}

	/* i==3 No button touched!  Assume that only one button may be touched each time!  */
       	/* Touch missed: Ripple effect */
	if( i==3 ) {
	        ripple_mark(touch_data.coord, 185, WEGI_COLOR_YELLOW);
		continue;
	}

   }


 /* --- my releae --- */
 egi_imgbuf_free2(&bkimg);
 egi_imgbuf_free2(&blockimg);
 egi_imgbuf_free2(&bannerMask);


	    		  /*-----------------------------------
    			   *         End of Main Program
    			   -----------------------------------*/


 /* <<<<<  EGI general release 	 >>>>>> */
 printf("FTsymbol_release_allfonts()...\n");
 FTsymbol_release_allfonts(); /* release sysfonts and appfonts */
 printf("symbol_release_allpages()...\n");
 symbol_release_allpages(); /* release all symbol pages*/
 printf("FTsymbol_release_allpage()...\n");
 FTsymbol_release_allpages(); /* release FT derived symbol pages: sympg_ascii */

 printf("fb_filo_flush() and release_fbdev()...\n");
 fb_filo_flush(&gv_fb_dev);
 release_fbdev(&gv_fb_dev);
 #if 0
 printf("release virtual fbdev...\n");
 release_virt_fbdev(&vfb);
 #endif
 printf("egi_end_touchread()...\n");
 egi_end_touchread();
 #if 0
 printf("egi_quit_log()...\n");
 egi_quit_log();
 #endif
 printf("<-------  END  ------>\n");


return 0;
}



/*-----------------------------------
    Draw button and put tag
-----------------------------------*/
void writeFB_button(EGI_RECTBTN *btn, char *tag, EGI_16BIT_COLOR tagcolor)
{
	int fw=22;
	int fh=22;
	int pixlen;
	int bith;

	if(btn==NULL)return;

	/* Always clear before write */
	draw_filled_rect2(&gv_fb_dev, btn->color, btn->x0, btn->y0, btn->x0+btn->width-1, btn->y0+btn->height-1);

	/* Draw frame ( fbdev, type, x0, y0, width, height, w ) */
  	draw_button_frame( &gv_fb_dev, btn->pressed, btn->color, btn->x0, btn->y0, btn->width, btn->height, btn->sw);

	bith=FTsymbol_get_symheight(egi_sysfonts.bold, fw, fh);

	pixlen=FTsymbol_uft8strings_pixlen( egi_sysfonts.bold, fw, fh,(const unsigned char *)tag);

 	/* Write tag */
  	FTsymbol_uft8strings_writeFB(  &gv_fb_dev, egi_sysfonts.bold,          	/* FBdev, fontface */
        	                       fw, fh,(const unsigned char *)tag,      	/* fw,fh, pstr */
                	               320, 1, 0,                              	/* pixpl, lines, gap */
				       btn->x0+(btn->width-pixlen)/2,		/* x0 */
				btn->y0+(btn->height-fh)/2-(fh-bith)/2 +(btn->pressed?2:0),		/* y0 */
                                       tagcolor, -1, 255,       			/* fontcolor, transcolor,opaque */
                                       NULL, NULL, NULL, NULL);      		/* int *cnt, int *lnleft, int* penx, int* peny */
}


/*----------------------------------------
	Switch/Toggle button reaction

For btns[ID_ONOFF] and btns[ID_MODE]
-----------------------------------------*/
void switch_button_react(EGI_RECTBTN *btn)
{
	if(btn==NULL)
		return;

	/* ID_MODE button only applicable when btns[ID_ONOFF] if released! */
	if(btn->id==ID_MODE && btns[ID_ONOFF].pressed==true)
		return;

        /* Toggle/switch button */
        btn->pressed=!btn->pressed;

        /* Draw button */
	if(btn->id == ID_ONOFF)
	        writeFB_button(btn, btn->pressed?"ON":"OFF", WEGI_COLOR_BLACK);
	else if(btn->id == ID_MODE)
	        writeFB_button(btn, btn->pressed?"MP3":"RADIO", WEGI_COLOR_BLACK);

        /* Write Hello World! */
        if(btn->pressed) {
		if(blockimg!=NULL)
			egi_subimg_writeFB(blockimg, &gv_fb_dev, 0, -1, 0, 190); /* Use saved image block */
		else
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_DARKGRAY, 0, 190, 320-1, 240-1);  /* OR a black banner */
                FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                                24, 24,(const unsigned char *)"A toggle button!",    /* fw,fh, pstr */
                                                320, 1, 0,                                  /* pixpl, lines, gap */
                                                20, 190+10,                                    /* x0,y0, */
                                                btn->color, -1, 255,       /* fontcolor, transcolor,opaque */
                                                NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	}
	else {
		/* Clear Hello World with PANEL_BKGCOLOR */
		if(blockimg!=NULL)
			egi_subimg_writeFB(blockimg, &gv_fb_dev, 0, -1, 0, 190);  /* Use saved image block */
		else
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_DARKGRAY, 0, 190, 320-1, 240-1);  /* OR a black banner */
	}

	/* --- My command --- */
	if(btn->pressed) {
		if(btn->id==ID_ONOFF) {
			if(play_mode==0)
				system("/home/myradio.sh start &");
			else if(play_mode==1)
				system("/home/mp3.sh &");
		}
		else if(btn->id==ID_MODE)
			play_mode=1;
	}
	else {
		if(btn->id==ID_ONOFF) {
			if(play_mode==0)
				system("/home/myradio.sh quit &");
			else if(play_mode==1)
				system("/home/mp_quit.sh &");
		}
		else if(btn->id==ID_MODE)
			play_mode=0;
	}
}


/*----------------------------------------------
	Bounce_back button reaction
Pressed_hold then wait for releasing touch event.

For btns[ID_NEXT]
-----------------------------------------------*/
void bounceback_button_react(EGI_RECTBTN *btn)
{
	EGI_TOUCH_DATA touch_data;

	/* Draw pressed button */
        btn->pressed=true;

	/* Draw button */
  	writeFB_button(btn, "next", WEGI_COLOR_BLACK);

	/* Write Hello World! clear banner area first */
	if(blockimg!=NULL)
		egi_subimg_writeFB(blockimg, &gv_fb_dev, 0, -1, 0, 190);  /* Use saved image block */
	else
		draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_DARKGRAY, 0, 190, 320-1, 240-1);  /* OR a black banner */
	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
      	       		                24, 24,(const unsigned char *)"A bounce_back button!",    /* fw,fh, pstr */
               	               	 	320, 1, 0,                                  /* pixpl, lines, gap */
                       	       		20, 190+10,                                    /* x0,y0, */
                       			btn->color, -1, 255,       /* fontcolor, transcolor,opaque */
                       			NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
  	fb_page_refresh(&gv_fb_dev, 0);


	/* Wait for touch releasing events */
	printf("Wait for release...\n");
  	egi_touch_timeWait_release(-1, 0, &touch_data);
	printf("...touch released!\n");
        btn->pressed=false;

	/* Draw released button */
  	writeFB_button(btn, "NEXT", WEGI_COLOR_BLACK);

	/* Clear Hello World with PANEL_BKGCOLOR */
	if(blockimg!=NULL)
		egi_subimg_writeFB(blockimg, &gv_fb_dev, 0, -1, 0, 190);  /* Use saved image block */
	else
		draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_DARKGRAY, 0, 190, 320-1, 240-1);  /* Or a black banner */

	/* --- My command --- */
	if(play_mode==0)
		system("/home/myradio.sh next &");
	else if(play_mode==1)
		system("/home/mp_next.sh 1 &");
}


/* ----------------------------------------------------------------------
Draw a water ripple mark.

@alpha:         Alpha value for color blend
@touch_pxy:     Point coordinate
---------------------------------------------------------------------- */
void ripple_mark(EGI_POINT touch_pxy, uint8_t alpha, EGI_16BIT_COLOR color)
{
        int i,k;
        int rad[]={ 20, 30, 40, 50, 60, 70, 80, 100, 120};
        int wid[]={ 7, 7, 6, 6, 5,3,2,1,1};


   for(i=0; i<8; i++) //	i<sizeof(rad)/sizeof(rad[0]); i++)
   {
        /* Turn on FB filo and set map pointer */
        fb_filo_on(&gv_fb_dev);
        gv_fb_dev.map_bk=gv_fb_dev.map_fb;

         for(k=0; k<8; k++) {
	     if(i-k>=0) {
	        draw_blend_filled_annulus(&gv_fb_dev,
        	                    touch_pxy.x, touch_pxy.y,
                	            rad[i-k], wid[i], ((k%8)%2)==0 ? WEGI_COLOR_WHITE : WEGI_COLOR_DARKGRAY, alpha );     /* radius, width, color, alpha */
	     }
	}

        tm_delayms(60);
        fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */

        /* Turn off FB filo and reset map pointer */
        gv_fb_dev.map_bk=gv_fb_dev.map_buff;
        fb_filo_off(&gv_fb_dev);
   }

}

