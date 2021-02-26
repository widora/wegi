/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


An example of roller control for time setting.
Add a click sound effect for the HOUR roller.


         --- 320x240 Screen Layout ---
	-------------------------------
   90	|    |	 |   |   |   |   |     |
	|    ___     ___     ___       |
	|xp'|   |   |   |   |   |      |
   60	|   |_H_|   |_M_|   |_s_|      |
	|			       |
	|    |   |   |   |   |   |     |
   90   -------------------------------
	  xp   60   20  60  20  60

	 H:M:S digits block: 60x60

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_FTsymbol.h>
#include <egi_pcm.h>

#define HOUR_ZONE   	0
#define MINUTE_ZONE 	1
#define SECOND_ZONE 	2

#define TOUCH_POS	3

#define PCM_MIXER       "default"
#define CLICK_SOUND	"/mmc/alarm.wav" //kaka.wav"  /* a very short piece */

EGI_PCMBUF *pcmClick;
bool sigTrigger;

void fbwrite_time(int hour, int min, int sec, int xp);
void refresh_digits(int zone, int xp, int off, EGI_IMGBUF *eimg);

static void *thread_tick(void *arg)
{
        /* mpc_dev, pcmbuf, vstep, nf, nloop, bool *sigstop, bool *sigsynch, bool* sigtrigger */
        egi_pcmbuf_playback(PCM_MIXER, pcmClick, 0, 1024*8, 0, NULL,  NULL,  &sigTrigger);

	return (void *)0;
}

int main(void)
{
 /* <<<<<<  EGI general init  >>>>>> */

#if 1 /* Start sys tick */
 printf("tm_start_egitick()...\n");
 tm_start_egitick();
#endif

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
  FTsymbol_set_SpaceWidth(0.5);

  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
	return -1;

#if 1  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
	return -1;
#endif

   /* Set sys FB mode: default */
   fb_set_directFB(&gv_fb_dev,false);
   fb_position_rotate(&gv_fb_dev,0);


 /* <<<<<  End of EGI general init  >>>>>> */


	FBDEV vfb;		 	/* virtual FB */
	EGI_IMGBUF *vfbimg=NULL;
	int mark;

	int i,j;
	int xp;
	int hour=0;
	int min=0;
	int sec=0;
	int tmp;
	char strnum[5];
	EGI_TOUCH_DATA touch_data;


	pthread_t threadClick;

	/* load pcm */
	pcmClick=egi_pcmbuf_readfile(CLICK_SOUND);
	if(pcmClick==NULL)exit(1);

	pthread_create(&threadClick,NULL,(void *)thread_tick,NULL);
	/* Init Vrit FB */
	//vfbimg=egi_imgbuf_alloc();
	//egi_imgbuf_init(vfbimg, 60*61, 60, false);
	vfbimg=egi_imgbuf_create( 60*61, 60, 255,WEGI_COLOR_GRAYC); /* height, width, alpha, color */
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
	egi_imgbuf_savepng("/tmp/band.png", vfbimg);  /* You can use showpic.c to display and check it. */

        /* Set sys FB mode: default */
//        fb_set_directFB(&gv_fb_dev,false);
//        fb_position_rotate(&gv_fb_dev,3);

	/* Draw scene backgroud  */
        fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_GRAY5);    /*  GRAY3 COLOR_RGB_TO16BITS(0x99,0x99,0x99) , GRAY4 0x88*/

	/* Fading bands */
	xp=20; /* - xp - */
	EGI_BOX hour_band = {{xp,0}, {xp+60,240-1}};
	EGI_BOX min_band  = {{xp+80,0}, {xp+140,240-1}};
	EGI_BOX sec_band  = {{xp+160,0}, {xp+220,240-1}};

	/* BUTTON OK box */
	EGI_BOX btn_ok	  = {{xp+240+2, 10}, {320-1-2, 60+10}};

	/* Draw bands */
	int lw=1;
	for(i=0; i<3; i++) {
		/* from top, downward */
		for(j=0; j<90/lw+1; j++) {
			//fbset_color(COLOR_RGB_TO16BITS(0x77+j*135/(90/lw),0x77+j*135/(90/lw),0x77+j*135/(90/lw)));
			fbset_color(COLOR_RGB_TO16BITS(0x44+j*185/(90/lw),0x44+j*185/(90/lw),0x44+j*185/(90/lw)));
			draw_wline_nc(&gv_fb_dev, xp+80*i, lw*j-2, xp+80*i+60-1, lw*j-2, lw);
		}
	}
	for(i=0; i<3; i++) {
		/* from bottom, upward */
		for(j=0; j<90/lw; j++) {
			fbset_color(COLOR_RGB_TO16BITS(0x44+j*185/(90/lw),0x44+j*185/(90/lw),0x44+j*185/(90/lw)));
			draw_wline_nc(&gv_fb_dev, xp+80*i, 240-lw*j+2, xp+80*i+60-1, 240-lw*j+2, lw);
		}
	}

	/* To shift digits left side */
	xp=20;  /* - xp' - */

	/* Draw time displaying block */
	for(i=0; i<3; i++)
	        draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYC,xp+80*i, 90+1, xp+60+80*i-1, 240-90);  /* number zones */

	/* Draw OK button */
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY, xp+240, 0, 320-1, 240-1);
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYB, btn_ok.startxy.x, btn_ok.startxy.y, btn_ok.endxy.x, btn_ok.endxy.y);  /* ok btn */
        fbset_color(WEGI_COLOR_BLACK);
	draw_wrect(&gv_fb_dev,btn_ok.startxy.x, btn_ok.startxy.y, btn_ok.endxy.x, btn_ok.endxy.y, 1);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        32, 32,(const unsigned char *)"OK",   //"确认",     /* fw,fh, pstr */
                                        100,1, 0,                           	/* pixpl, lines, gap */
                                        xp+240+5, 20,    // touch effective area               	/* x0,y0, */
                                        WEGI_COLOR_BLACK, -1, -1,               /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */


        /* Init FB back ground buffer page with working buffer */
        fb_copy_FBbuffer(&gv_fb_dev, 0, 1);     /* fb_dev, from_numpg, to_numpg */

	/* init TIME displaying */
	fbwrite_time(1,2,3,xp);


	/* Refresh page */
	fb_page_refresh(&gv_fb_dev,0);

while(1){

#if 0   /* Wait for touch event:  If last touch missed,  the pen has to leave the screen and touch again. */
        if( egi_touch_timeWait_press(0, 500, &touch_data)==0 ) {

#else   /* To get touch data again: If last touch missed any bands, just continue to read touch data and parse, then
	 * if the pen leaves the screen, start a new round again.
	 */
        if( egi_touch_getdata(&touch_data) && touch_data.status != released_hold ) {
		if(touch_data.status == releasing) {
			tm_delayms(10);
			continue;
		}
#endif
		//printf("Raw: x=%d,y=%d \n",touch_data.coord.x, touch_data.coord.y);
		egi_touch_fbpos_data(&gv_fb_dev, &touch_data, TOUCH_POS);  /* touch_data change to same coord as of FB */

		/* To roll hour band */
		if(point_inbox(&touch_data.coord, &hour_band)) {
			printf("--hour--\n");
			tmp=hour; /* tmp. hour number */
			mark=hour*60;
			while( touch_data.status == pressing ||touch_data.status == pressed_hold ) {
				/* Adjust touch data coord. system to the same as FB pos_rotate.*/
				egi_touch_fbpos_data(&gv_fb_dev, &touch_data, TOUCH_POS);

				/* --- Continous Scrolling --- */
				mark = hour*60-((touch_data.dy*3)>>1);		/* !!! adjust scrolling speed */
				if(mark<0)mark=0;				/* Limit to 0-23 */
				else if(mark>23*60)mark=23*60;

				/*Check if reach threshold */
				if(tmp != (mark+30)/60) {
					tmp=(mark+30)/60;
					if(sigTrigger==false)
						sigTrigger=true;  /* To trigger click sound */
					egi_sleep(1,0,30);
				}
				/* To make a sluggish effect */
				if( mark< tmp*60-25 || mark>tmp*60+25) {
					refresh_digits(HOUR_ZONE, xp, mark, vfbimg);
				}
				else
					refresh_digits(HOUR_ZONE, xp, tmp*60, vfbimg);

				while(egi_touch_getdata(&touch_data)==false);
			}
			/* reset hour when release touch */
			hour =(mark+30)/60;
			printf("reset hour=%d\n",hour);
			refresh_digits(HOUR_ZONE, xp, hour*60, vfbimg);
		}

		/* To roll minute band */
		else if(point_inbox(&touch_data.coord, &min_band)) {
			printf("--min--\n");
			tmp=min;
			mark=min*60;
			while( touch_data.status == pressing ||touch_data.status == pressed_hold ) {
				egi_touch_fbpos_data(&gv_fb_dev, &touch_data, TOUCH_POS);
				//printf("x=%d, y=%d, dx=%d, dy=%d,\n",
				//		touch_data.coord.x, touch_data.coord.y, touch_data.dx, touch_data.dy);

                                mark = min*60-((touch_data.dy*3)>>1);		/* !!! adjust scrolling speed */
				if(mark<0)mark=0;				/* Limit to 0-59 */
				else if(mark>59*60)mark=59*60;

				refresh_digits(MINUTE_ZONE, xp, mark, vfbimg);
				while(egi_touch_getdata(&touch_data)==false);
			}
			/* reset minute when release touch */
			min =(mark+30)/60;
			printf("reset min=%d\n",min);
			refresh_digits(MINUTE_ZONE, xp, min*60, vfbimg);
		}
		/* To roll second band */
		else if(point_inbox(&touch_data.coord, &sec_band)) {
			printf("--sec--\n");
			mark=sec*60;
			while( touch_data.status == pressing ||touch_data.status == pressed_hold ) {
				egi_touch_fbpos_data(&gv_fb_dev, &touch_data, TOUCH_POS);
                                mark = sec*60-((touch_data.dy*3)>>1);		/* !!! adjust scrolling speed */
				if(mark<0)mark=0;				/* Limit to 0-59 */
				else if(mark>59*60)mark=59*60;
				refresh_digits(SECOND_ZONE, xp, mark, vfbimg);
				while(egi_touch_getdata(&touch_data)==false);
			}
			/* reset minute when release touch */
			sec =(mark+30)/60;
			printf("reset sec=%d\n",sec);
			refresh_digits(SECOND_ZONE, xp, sec*60, vfbimg);
		}
		/* Button OK */
		else if(point_inbox(&touch_data.coord,&btn_ok) && touch_data.status==pressing) {
			printf(" -- ok --  x=%d,y=%d \n",touch_data.coord.x, touch_data.coord.y);
			//fbset_color(WEGI_COLOR_RED);
			//draw_filled_circle(&gv_fb_dev, touch_data.coord.x, touch_data.coord.y, 5);
			//fb_lines_refresh(&gv_fb_dev, 0, 0, 80); /* fbdev, numpg, startln, n */
		}
		/* Touch missed! */
		else {
			printf(" --- Touch missed! x=%d,y=%d. --- \n", touch_data.coord.x, touch_data.coord.y);
		}

	} /* End of one roll */
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


/*----------------------------------------------
   Write time to FB at format of H:M:S
@hour,min,sec:  hours,minutes and seconds digits
@xp: 		block offset postion
-----------------------------------------------*/
void fbwrite_time(int hour, int min, int sec, int xp)
{
	int i;
	char strtm[16]={0};

	snprintf(strtm,13,"%02d : %02d : %02d", hour,min,sec);

        /* Draw time displaying block */
//        for(i=0; i<3; i++)
//                draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYB, 50+80*i, 90, 50+60+80*i, 240-90);  /* number zones */

	/* Prepare FB working buffer, copy from FB back ground buffer */
	fb_copy_FBbuffer(&gv_fb_dev, 1, 0);	/* fb_dev, from_numpg, to_numpg */


        /* WriteFB utxt */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        40, 40,(const unsigned char *)strtm,     /* fw,fh, pstr */
                                        320-10, 2, 2,                           /* pixpl, lines, gap */
                                        xp+5, 95,                               /* x0,y0, */
                                        WEGI_COLOR_PINK, -1, -1,               /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */

	/* display working buffer */
	fb_page_refresh(&gv_fb_dev,0);
}


/*-----------------------------------------------------
   Refresh/display digits of Hour,Minute, or Seconds only.

@zone:  0 hour
	1 minute
	2 second
        digits in the zone standing for
	hours, minutes or seconds.
@imgoff:   offset from top of the eimg to current position
	of scrolling.

@eimg:  An EGI_IMGBUF holding the digits image.
@xp:    block offset from x=0.

-------------------------------------------------------*/
void refresh_digits(int zone, int xp, int imgoff, EGI_IMGBUF *eimg)
{
	/* writFB numbers */
	egi_imgbuf_windisplay( eimg, &gv_fb_dev, -1,  /* imgbuf, fbdev, subcolor */
							    /* yw+1 AND winH-1 to fit into height of 60x60 */
        		        0, imgoff, xp+zone*80, 90+1,   /*  xp, yp, xw, yw */
                                60, 60-1 );          	    /*  winW, winH */

	/* refresh partial FB, NOT applicable for all FBPOS */
        // fb_lines_refresh(&gv_fb_dev, 0, 320-xp-60-zone*80, 60); /* fbdev, numpg, startln, n */

	fb_render(&gv_fb_dev);

}
