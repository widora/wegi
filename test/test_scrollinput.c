/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_FTsymbol.h>


void fbwrite_time(int hour, int min, int sec);


int main(void)
{


 /* <<<<<  EGI general init  >>>>>> */

 /* Start sys tick */
 printf("tm_start_egitick()...\n");
 tm_start_egitick();

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

  /* Load freetype fonts */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
	printf("Fail to load FT sysfonts, quit.\n");
	return -1;
  }
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
	printf("Fail to load FT appfonts, quit.\n");
	return -1;
  }

  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
	return -1;

  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
	return -1;

 /* <<<<<  End of EGI general init  >>>>>> */

	int i,j;
	int hour=0;
	int min=0;
	int sec=0;
	int tmp;
	EGI_TOUCH_DATA touch_data;

        /* Set FB mode: default */
        //fb_set_directFB(&gv_fb_dev, false);
        fb_position_rotate(&gv_fb_dev,3);

	/* Draw scene backgroud  */
        fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_GRAY5);    /*  GRAY3 COLOR_RGB_TO16BITS(0x99,0x99,0x99) , GRAY4 0x88*/

	/* Draw time displaying block */
	for(i=0; i<3; i++)
	        draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYB, 50+80*i, 90, 50+60+80*i, 240-90);  /* number zones */

	/* Fading bands */
	EGI_BOX hour_band = {{50,0},{110,240-1}};
	EGI_BOX min_band = {{130,0},{190,240-1}};
	EGI_BOX sec_band = {{210,0},{270,240-1}};
	int lw=1;
	for(i=0; i<3; i++) {
		/* from top, downward */
		for(j=0; j<90/lw; j++) {
			//fbset_color(COLOR_RGB_TO16BITS(0x77+j*lw+40,0x77+j*lw+40,0x77+j*lw+40));
			fbset_color(COLOR_RGB_TO16BITS(0x77+j*135/(90/lw),0x77+j*135/(90/lw),0x77+j*135/(90/lw)));
			draw_wline_nc(&gv_fb_dev, 50+80*i, lw*j-2, 50+80*i+60, lw*j-2, lw);
		}
	}
	for(i=0; i<3; i++) {
		/* from bottom, upward */
		for(j=0; j<90/lw; j++) {
			//fbset_color(COLOR_RGB_TO16BITS(0x77+j*lw+40,0x77+j*lw+40,0x77+j*lw+40));
			fbset_color(COLOR_RGB_TO16BITS(0x77+j*135/(90/lw),0x77+j*135/(90/lw),0x77+j*135/(90/lw)));
			draw_wline_nc(&gv_fb_dev, 50+80*i, 240-lw*j+2, 50+80*i+60, 240-lw*j+2, lw);
		}
	}

        /* Init FB back ground buffer page with working buffer */
        fb_copy_FBbuffer(&gv_fb_dev, 0, 1);     /* fb_dev, from_numpg, to_numpg */

	/* init TIME displaying */
	fbwrite_time(0,0,0);

	/* Refresh page */
	fb_page_refresh(&gv_fb_dev,0);

while(1){
        /* Wait for touch event */
        if( egi_touch_timeWait_press(0, 500, &touch_data)==0 ) {

		egi_touch_fbpos_data(&gv_fb_dev, &touch_data);  /* touch_data change to same coord as of FB */

		/* scroll hour band */
		if(point_inbox(&touch_data.coord, &hour_band)) {
			printf("--hour--\n");
			tmp=hour;
			while( touch_data.status == pressing ||touch_data.status == pressed_hold ) {
				egi_touch_fbpos_data(&gv_fb_dev, &touch_data);
				hour =  tmp -touch_data.dy/20;
				printf("hour=%d\n",hour);
				if(hour>23)hour%=24;
				else if(hour<0)hour=0;
				fbwrite_time(hour, min, sec);
				while(egi_touch_getdata(&touch_data)==false);
			}
		}

		/* scroll minute band */
		else if(point_inbox(&touch_data.coord, &min_band)) {
			printf("--min--\n");
			tmp=min;
			while( touch_data.status == pressing ||touch_data.status == pressed_hold ) {
				egi_touch_fbpos_data(&gv_fb_dev, &touch_data);
				min =  tmp -touch_data.dy/20;
				printf("min=%d\n",min);
				if(min>59)min%=60;
				else if(min<0)min=0;
				fbwrite_time(hour, min, sec);
				while(egi_touch_getdata(&touch_data)==false);
			}
		}
		/* scroll second band */
		else if(point_inbox(&touch_data.coord, &sec_band)) {
			printf("--sec--\n");
			tmp=sec;
			while( touch_data.status == pressing ||touch_data.status == pressed_hold ) {
				egi_touch_fbpos_data(&gv_fb_dev, &touch_data);
				sec =  tmp -touch_data.dy/20;
				printf("sec=%d\n",sec);
				if(sec>59)sec%=60;
				else if(sec<0)sec=0;
				fbwrite_time(hour, min, sec);
				while(egi_touch_getdata(&touch_data)==false);
			}
		}
	}

}


 /* <<<<<  EGI general release  >>>>>> */
 printf("FTsymbol_release_allfonts()...\n");
 FTsymbol_release_allfonts(); /* release sysfonts and appfonts */
 printf("symbol_release_allpages()...\n");
 symbol_release_allpages(); /* release all symbol pages*/
 printf("FTsymbol_release_allpage()...\n");
 FTsymbol_release_allpages(); /* release FT derived symbol pages: sympg_ascii */

 printf("fb_filo_flush() and release_fbdev()...\n");
 fb_filo_flush(&gv_fb_dev);
 release_fbdev(&gv_fb_dev);

 printf("egi_end_touchread()...\n");
 egi_end_touchread();
 printf("egi_quit_log()...\n");
 egi_quit_log();
 printf("<-------  END  ------>\n");


return 0;
}


/*------------------------------------------
   Write time to FB at format of H:M:S
-------------------------------------------*/
void fbwrite_time(int hour, int min, int sec)
{
	int i;
	char strtm[16]={0};

	snprintf(strtm,13,"%02d : %02d : %02d", hour,min,sec);

        /* Draw time displaying block */
        for(i=0; i<3; i++)
                draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYB, 50+80*i, 90, 50+60+80*i, 240-90);  /* number zones */

	/* Prepare FB working buffer, copy from FB back ground buffer */
	fb_copy_FBbuffer(&gv_fb_dev, 1, 0);	/* fb_dev, from_numpg, to_numpg */


        /* WriteFB utxt */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        40, 40,(const unsigned char *)strtm,     /* fw,fh, pstr */
                                        320-10, 2, 2,                           /* pixpl, lines, gap */
                                        55, 95,                                       /* x0,y0, */
                                        WEGI_COLOR_PINK, -1, -1,               /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */


#if 0
        /* WriteFB utxt */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        40, 40,(const unsigned char *)"14 : 29 : 55",     /* fw,fh, pstr */
                                        320-10, 2, 2,                           /* pixpl, lines, gap */
                                        55, 95,                                       /* x0,y0, */
                                        WEGI_COLOR_PINK, -1, -1,               /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */
#endif

	/* display working buffer it */
	fb_page_refresh(&gv_fb_dev,0);

}
