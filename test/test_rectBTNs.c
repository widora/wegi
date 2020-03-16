/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A demo for simple rectangular buttons. It creates a switch button
and a bounce_back button, then test it's reactions.

A more beautiful button is usually an (PNG/JPG) image icon.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_FTsymbol.h>


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

  EGI_TOUCH_DATA  touch_data;
  EGI_16BIT_COLOR bkcolor=WEGI_COLOR_GRAY1;

  /* A Switch/Toggle button */
  EGI_RECTBTN 	  btn0={ .color=WEGI_COLOR_GRAY1, .x0=100, .y0=30, .width=120, .height=50, .sw=4, .pressed=false };
  btn0.color=bkcolor;			/* button color */
  btn0.reaction=switch_button_react;	/* Reaction callback */

  /* A Bounce_back button */
  EGI_RECTBTN 	  btn1={ .color=WEGI_COLOR_GRAY1, .x0=100, .y0=110, .width=120, .height=50, .sw=4, .pressed=false };
  btn1.color=bkcolor;				/* button color */
  btn1.reaction=bounceback_button_react;	/* Reaction callback */


  /* Clear screen and init. scene */
  fb_clear_backBuff(&gv_fb_dev, bkcolor);
  writeFB_button(&btn0, "OK", WEGI_COLOR_BLACK);
  writeFB_button(&btn1, "off", WEGI_COLOR_BLACK);
  fb_page_refresh(&gv_fb_dev, 0);

  while(1) {
  	if(egi_touch_timeWait_press(-1, 0, &touch_data)!=0)
		continue;
        /* touch_data converted to the same coord as of FB */
        egi_touch_fbpos_data(&gv_fb_dev, &touch_data);

	/* Check if touched any of the buttons */
	if( egi_touch_on_rectBTN(&touch_data, &btn0) && touch_data.status==pressing ) {
		printf("Button_0 touched!\n");
		btn0.reaction(&btn0);
	  	fb_page_refresh(&gv_fb_dev, 0);
	}
	else if (egi_touch_on_rectBTN(&touch_data, &btn1) && touch_data.status==pressing ) {
		printf("Button_1 touched!\n");
		btn1.reaction(&btn1);
  		fb_page_refresh(&gv_fb_dev, 0);
	}
	else {
        	/* Ripple effect */
	        ripple_mark(touch_data.coord, 185, WEGI_COLOR_YELLOW);

		continue;
	}

   }



 /* --- my releae --- */

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
	int fw=25;
	int fh=25;
	int pixlen;

	if(btn==NULL)return;

	/* Always clear before write */
	draw_filled_rect2(&gv_fb_dev, btn->color, btn->x0, btn->y0, btn->x0+btn->width, btn->y0+btn->height);

	/* Draw frame ( fbdev, type, x0, y0, width, height, w ) */
  	draw_button_frame( &gv_fb_dev, btn->pressed, btn->color, btn->x0, btn->y0, btn->width, btn->height, btn->sw);

	int bith=FTsymbol_get_symheight(egi_sysfonts.bold, fw, fh);

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
-----------------------------------------*/
void switch_button_react(EGI_RECTBTN *btn)
{
	if(btn==NULL)
		return;

        /* Toggle/switch button */
        btn->pressed=!btn->pressed;

        /* Draw button */
        writeFB_button(btn, btn->pressed?"ON":"OFF", WEGI_COLOR_BLACK);

        /* Write Hello World! */
        if(btn->pressed) {
		draw_filled_rect2(&gv_fb_dev, btn->color, 0, 190, 320-1, 240-1);
                FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                                24, 24,(const unsigned char *)"A toggle button!",    /* fw,fh, pstr */
                                                320, 1, 0,                                  /* pixpl, lines, gap */
                                                20, 190,                                    /* x0,y0, */
                                                WEGI_COLOR_FIREBRICK, -1, 255,       /* fontcolor, transcolor,opaque */
                                                NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	}
	else
		/* Clear Hello World */
		draw_filled_rect2(&gv_fb_dev, btn->color, 0, 190, 320-1, 240-1);


}


/*----------------------------------------------
	Bounce_back button reaction
Pressed_hold then wait for releasing touch event.
-----------------------------------------------*/
void bounceback_button_react(EGI_RECTBTN *btn)
{
	EGI_TOUCH_DATA touch_data;

	/* Draw pressed button */
        btn->pressed=true;

	/* Draw button */
  	writeFB_button(btn, "on", WEGI_COLOR_BLACK);

	/* Write Hello World! */
	draw_filled_rect2(&gv_fb_dev, btn->color, 0, 190, 320-1, 240-1);
	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
               		                24, 24,(const unsigned char *)"A bounce_back button!",    /* fw,fh, pstr */
                               	 	320, 1, 0,                                  /* pixpl, lines, gap */
                               		20, 190,                                    /* x0,y0, */
                               		WEGI_COLOR_FIREBRICK, -1, 255,       /* fontcolor, transcolor,opaque */
                               		NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
  	fb_page_refresh(&gv_fb_dev, 0);

	/* Wait for touch releasing events */
	printf("Wait for release...\n");
  	egi_touch_timeWait_release(-1, 0, &touch_data);
	printf("...touch released!\n");
        btn->pressed=false;

	/* Draw released button */
  	writeFB_button(btn, "off", WEGI_COLOR_BLACK);

	/* Clear Hello World */
	draw_filled_rect2(&gv_fb_dev, btn->color, 0, 190, 320-1, 240-1);

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

