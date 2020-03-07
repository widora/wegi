/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


An example of scrolling input for time setting.

         --- 320x240 Screen Layout ---
	-------------------------------
   90	|    |	 |   |   |   |   |     |
	|     ___     ___     ___      |
	|    |   |   |   |   |   |     |
   60	|    |___|   |___|   |___|     |
	|			       |
	|    |   |   |   |   |   |     |
   90   -------------------------------
	  50  60   20  60  20  60   50

	H:M:S digits block: 60x60

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_FTsymbol.h>

#define HOUR_ZONE   0
#define MINUTE_ZONE 1
#define SECOND_ZONE 2

void fbwrite_time(int hour, int min, int sec);
void refresh_numbers(int zone, int off, EGI_IMGBUF *eimg);

int main(void)
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

 /* <<<<<  End of EGI general init  >>>>>> */


	FBDEV vfb;		 	/* virtual FB */
	EGI_IMGBUF *vfbimg=NULL;
	int mark;

	int i,j;
	int hour=0;
	int min=0;
	int sec=0;
	//int tmp;
	char strnum[5];
	EGI_TOUCH_DATA touch_data;

	/* Init Vrit FB */
	//vfbimg=egi_imgbuf_alloc();
	//egi_imgbuf_init(vfbimg, 60*61, 60, false);
	vfbimg=egi_imgbuf_create( 60*61, 60, 255,WEGI_COLOR_GRAYB); /* height, width, alpha, color */
        if( init_virt_fbdev(&vfb, vfbimg) !=0 ){
		printf("Fail to init virtual FB!\n");
		exit(-1);
	}

	/* Create vfbimg of digits 00-59,00, each subimg size 60x60 */
        for(i=0; i<61; i++) {
		memset(strnum,0,sizeof(strnum));
		if(i==0 || i==60 )
			strcat(strnum,"00");
		else
			snprintf(strnum,3,"%02d",i);
        	/* Write numbers to vfbimg */
        	FTsymbol_uft8strings_writeFB(   &vfb, egi_sysfonts.bold,            	/* FBdev, fontface */
                	                        40, 40,(const unsigned char *)strnum,  	/* fw,fh, pstr */
                        	                60, 1, 0,                              	/* pixpl, lines, gap */
                                	        5, 5+i*60,                            	/* x0,y0, */
                                        	WEGI_COLOR_PINK, -1, -1,              	/* fontcolor, transcolor,opaque */
	                                        NULL, NULL, NULL, NULL 	);          /* int *cnt, int *lnleft, int* penx, int* peny */
		/* May add a division line */
		// fbset_color(WEGI_COLOR_BLACK);
		// draw_wline_nc(&vfb, 0, 60*i, 60-1, 60*i, 1);
	}
	//TEST: egi_imgbuf_savepng("/tmp/band.png", vfbimg);  /* You can use showpic.c to display and check it. */

        /* Set sys FB mode: default */
        fb_set_directFB(&gv_fb_dev,false);
        fb_position_rotate(&gv_fb_dev,3);

	/* Draw scene backgroud  */
        fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_GRAY5);    /*  GRAY3 COLOR_RGB_TO16BITS(0x99,0x99,0x99) , GRAY4 0x88*/

	/* Draw time displaying block */
	for(i=0; i<3; i++)
	        draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYB, 50+80*i, 90+1, 50+60+80*i-1, 240-90);  /* number zones */

	/* Fading bands */
	EGI_BOX hour_band = {{50,0},{110,240-1}};
	EGI_BOX min_band = {{130,0},{190,240-1}};
	EGI_BOX sec_band = {{210,0},{270,240-1}};
	int lw=1;
	for(i=0; i<3; i++) {
		/* from top, downward */
		for(j=0; j<90/lw+1; j++) {
			//fbset_color(COLOR_RGB_TO16BITS(0x77+j*lw+40,0x77+j*lw+40,0x77+j*lw+40));
			//fbset_color(COLOR_RGB_TO16BITS(0x77+j*135/(90/lw),0x77+j*135/(90/lw),0x77+j*135/(90/lw)));
			fbset_color(COLOR_RGB_TO16BITS(0x44+j*185/(90/lw),0x44+j*185/(90/lw),0x44+j*185/(90/lw)));
			draw_wline_nc(&gv_fb_dev, 50+80*i, lw*j-2, 50+80*i+60-1, lw*j-2, lw);
		}
	}
	for(i=0; i<3; i++) {
		/* from bottom, upward */
		for(j=0; j<90/lw; j++) {
			fbset_color(COLOR_RGB_TO16BITS(0x44+j*185/(90/lw),0x44+j*185/(90/lw),0x44+j*185/(90/lw)));
			draw_wline_nc(&gv_fb_dev, 50+80*i, 240-lw*j+2, 50+80*i+60-1, 240-lw*j+2, lw);
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
			// tmp=hour; /* for uncontinous scrolling */
			mark=hour*60;
			while( touch_data.status == pressing ||touch_data.status == pressed_hold ) {
				egi_touch_fbpos_data(&gv_fb_dev, &touch_data);

				/* ----- Uncontinuous Scrolling -----
				  hour =  tmp -touch_data.dy/20;
				  printf("hour=%d\n",hour);
				  if(hour>23)hour%=24;
				  else if(hour<0)hour=0;
				  fbwrite_time(hour, min, sec);
				*/

				/* --- Continous Scrolling --- */
				mark = hour*60-((touch_data.dy*3)>>1);		/* !!! adjust scrolling speed */
				if(mark<0)mark=0;				/* Limit to 0-23 */
				else if(mark>23*60)mark=23*60;
				refresh_numbers(HOUR_ZONE, mark, vfbimg);
				while(egi_touch_getdata(&touch_data)==false);
			}
			/* reset hour when release touch */
			hour =(mark+30)/60;
			printf("reset hour=%d\n",hour);
			refresh_numbers(HOUR_ZONE, hour*60, vfbimg);
		}

		/* scroll minute band */
		else if(point_inbox(&touch_data.coord, &min_band)) {
			printf("--min--\n");
			mark=min*60;
			while( touch_data.status == pressing ||touch_data.status == pressed_hold ) {
				egi_touch_fbpos_data(&gv_fb_dev, &touch_data);
                                mark = min*60-((touch_data.dy*3)>>1);		/* !!! adjust scrolling speed */
				if(mark<0)mark=0;				/* Limit to 0-59 */
				else if(mark>59*60)mark=59*60;
				refresh_numbers(MINUTE_ZONE, mark, vfbimg);
				while(egi_touch_getdata(&touch_data)==false);
			}
			/* reset minute when release touch */
			min =(mark+30)/60;
			printf("reset min=%d\n",min);
			refresh_numbers(MINUTE_ZONE, min*60, vfbimg);
		}
		/* scroll second band */
		else if(point_inbox(&touch_data.coord, &sec_band)) {
			printf("--sec--\n");
			mark=sec*60;
			while( touch_data.status == pressing ||touch_data.status == pressed_hold ) {
				egi_touch_fbpos_data(&gv_fb_dev, &touch_data);
                                mark = sec*60-((touch_data.dy*3)>>1);		/* !!! adjust scrolling speed */
				if(mark<0)mark=0;				/* Limit to 0-59 */
				else if(mark>59*60)mark=59*60;
				refresh_numbers(SECOND_ZONE, mark, vfbimg);
				while(egi_touch_getdata(&touch_data)==false);
			}
			/* reset minute when release touch */
			sec =(mark+30)/60;
			printf("reset sec=%d\n",sec);
			refresh_numbers(SECOND_ZONE, sec*60, vfbimg);
		}
	}

}


 /* -----  my release  ----- */
 egi_imgbuf_free(vfbimg);


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

 printf("release virtual fbdev...\n");
 release_virt_fbdev(&vfb);

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

	/* display working buffer */
	fb_page_refresh(&gv_fb_dev,0);
}


/*-----------------------------------------------------
   Refresh/display digits of Hour,Minute, or
Seconds only.

@zone:  0 hour
	1 minute
	2 second
        digits in the zone standing for
	hours, minutes or seconds.
@off:   offset from top of the eimg to current position
	of scrolling.

@eimg:  An EGI_IMGBUF holding the digits image.

-------------------------------------------------------*/
void refresh_numbers(int zone, int off, EGI_IMGBUF *eimg)
{
	/* writFB numbers */
	egi_imgbuf_windisplay( eimg, &gv_fb_dev, -1,  /* imgbuf, fbdev, subcolor */
							    /* yw+1 AND winH-1 to fit into height of 60x60 */
        			0, off, 50+zone*80, 90+1,   /*  xp, yp, xw, yw */
                                60, 60-1 );          	    /*  winW, winH */
	/* refresh partial FB */
        fb_lines_refresh(&gv_fb_dev, 0, 210-zone*80, 60); /* fbdev, numpg, startln, n */

}
