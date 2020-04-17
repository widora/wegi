/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of creating a CPU load meter/indicator.

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <stdint.h>
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_cstring.h>
#include <egi_FTsymbol.h>
#include <sys/sysinfo.h>

int  	percent_cpuLoad(void);
int 	display_sysinfo(void);
void 	draw_needle(int length, int angle, int xrl, int yrl, EGI_16BIT_COLOR color);


/*-----------------------------
    	      MAIN
-----------------------------*/
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
  #endif

  #if 0
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

  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,false);
  fb_position_rotate(&gv_fb_dev,3);

 /* <<<<<  End of EGI general init  >>>>>> */



    		  /*-----------------------------------
		   *            Main Program
    		   -----------------------------------*/

	int delt=2;
	int load_percent=0;
	int angle=0;

	/* Horizontal progress bar params */
	int barFX0=20;		       	/* Frame left top point */
	int barFY0=30;
	int barMax=260;		  	/* Max. length of progress bar itsel */
	int barFLW=3;		       	/* Frame line width */
	int barFH=16;		       	/* Frame height, in mid of frame line */
	int barFW=barMax+barFH;        	/* Frame width */
	int barR=10;   		       	/* R of round corner */
	barR=barR>barFH/2 ? barFH/2 : barR;
	int barStartX=barFX0+barFLW+(barFH-barFLW)/2 -1;   /* Progree bar start X coord. */
	int barEndX;					/* Progree bar end X coord. */

	EGI_IMGBUF 	*meterPanel=egi_imgbuf_readfile("/home/meter.png");
	EGI_IMGBUF 	*needle=egi_imgbuf_readfile("/home/myneedle.png");
	int  xrl=320/2;	  /* needle turning center, relative to LCD fb_rotate coord.  */
	int  yrl=240-8;
	int  xri=115;	  /* needle turning center, relative to needle imgbuf coord.  */
	int  yri=20;

        int mark=0;
	EGI_TOUCH_DATA  touch_data;

	EGI_16BIT_COLOR	needle_color=WEGI_COLOR_OCEAN;

	/* Display back ground scene */
	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_GRAY5);

	egi_imgbuf_flipY( meterPanel );
	gv_fb_dev.lumadelt=50;
        egi_imgbuf_windisplay( meterPanel, &gv_fb_dev, -1,          	/* img, fb, subcolor */
                               0, 0,					/* xp, yp */
		(320-meterPanel->width)/2, (240-meterPanel->height)/2+40,      /* xw, yw */
                               meterPanel->width, meterPanel->height);    /* winw, winh */
	gv_fb_dev.lumadelt=0;

	/* Draw CPU Load division color mark */
	fbset_alpha(&gv_fb_dev, 252);
	fbset_color(WEGI_COLOR_GREEN);
	draw_warc(&gv_fb_dev, 320/2, 240-8, 50, -MATH_PI, -MATH_PI/2-0.02, 45); /* dev, x0, y0,  r, Sang, Eang,  w */
	fbset_color(WEGI_COLOR_YELLOW);
	draw_warc(&gv_fb_dev, 320/2, 240-8, 50, -MATH_PI/2, -MATH_PI/6-0.02, 45); /* dev, x0, y0,  r, Sang, Eang,  w */
	fbset_color(WEGI_COLOR_RED);
	draw_warc(&gv_fb_dev, 320/2, 240-8, 50, -MATH_PI/6, 0, 45); /* dev, x0, y0,  r, Sang, Eang,  w */
	fbreset_alpha(&gv_fb_dev);

	/* Draw empty progress bar  */
	fbset_color(WEGI_COLOR_BLACK); /* Frame color */
	draw_roundcorner_wrect( &gv_fb_dev, barFX0, barFY0, barFX0+barFW, barFY0+barFH, barR, barFLW); 	/* fbdev, x1,y1, x2,y2, r, w */
	fbset_color(WEGI_COLOR_GRAY);  /* Bar color */
	barEndX=barStartX+barMax;
	draw_wline(&gv_fb_dev, barStartX, barFY0+barFH/2, barStartX+barMax, barFY0+barFH/2, barFH-barFLW );

	/* Put to FBDEV_BKG_BUFF */
	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */

	/* Rend the scene */
	fb_render(&gv_fb_dev);

        /* =============   Main Loop   ============= */
while(1) {
	/* If touch down: use touch data to control Meter and Bar */

//	if( egi_touch_timeWait_press(0, 100, &touch_data)==0 ) {
        if( egi_touch_getdata(&touch_data) ) {
		while( touch_data.status == pressing || touch_data.status == pressed_hold ) {
                	/* Adjust touch data coord. system to the same as FB pos_rotate.*/
                        egi_touch_fbpos_data(&gv_fb_dev, &touch_data);

                        /* Continous sliding */
                        mark = touch_data.dx;
                        if(mark<0)mark=0;
                        else if(mark > barMax)mark=barMax;

			fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */

			/* Draw needle */
			angle=180*mark/barMax;
			#if 0 /* Use image */
			egi_image_rotdisplay( needle, &gv_fb_dev, angle, xri,yri, xrl, yrl );	/* imgbuf, fbdev, angle, xri,yri,  xrl,yrl */
			#else /* Draw it */
 			draw_needle(100, angle, xrl, yrl, needle_color); /* length, angle, xrl,yrl, color */
			#endif

			/* Draw bar and needle */
			load_percent=mark*100/barMax;
			if(load_percent<50)
				fbset_color(WEGI_COLOR_GREEN);
			else if(load_percent<(50+50*2/3))
				fbset_color(WEGI_COLOR_YELLOW);
			else
				fbset_color(WEGI_COLOR_RED);
			draw_wline(&gv_fb_dev, barStartX, barFY0+barFH/2, barStartX+mark, barFY0+barFH/2, barFH-barFLW );

			/* Run time */
			display_sysinfo();

			/* Render scene */
			fb_render(&gv_fb_dev);

			/* Read touch data */
                        while(egi_touch_getdata(&touch_data)==false);
                }
		continue;
	}

	/* Reset FB working buffer with background image */
	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */

	/* Get CPU load */
	load_percent=percent_cpuLoad();

	/* Draw indicator needle */
	angle=180*load_percent/100;
	#if 0  /* Use image */
	egi_image_rotdisplay( needle, &gv_fb_dev, angle, xri,yri, xrl, yrl );	/* imgbuf, fbdev, angle, xri,yri,  xrl,yrl */
	#else  /* Draw it */
 	draw_needle(100, angle, xrl, yrl, needle_color); /* length, angle, xrl,yrl, color */
	#endif

	/* Draw progress bar */
	if(load_percent<50)
		fbset_color(WEGI_COLOR_GREEN);
	else if(load_percent<(50+50*2/3))
		fbset_color(WEGI_COLOR_YELLOW);
	else
		fbset_color(WEGI_COLOR_RED);
	//fbset_alpha(&gv_fb_dev, 250);  /* If you want to see darker circles at ends of the bar, turn on it */
	barEndX=barStartX+barMax*load_percent/100;
	draw_wline(&gv_fb_dev, barStartX, barFY0+barFH/2, barEndX, barFY0+barFH/2, barFH-barFLW );
	//fbreset_alpha(&gv_fb_dev);

	/* Run time */
	display_sysinfo();

	fb_render(&gv_fb_dev);
//	tm_delayms(50);

}/* end While() */


 	/* ----- MY release ---- */
 	egi_imgbuf_free(meterPanel);
 	egi_imgbuf_free(needle);


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


/*----------------------------------
Return CPU Load in pecent% value
Reutrn:
	>=0	OK
	<0	Fail
----------------------------------*/
int  percent_cpuLoad(void)
{
        int load=0;
        int fd;
        char strload[36]={0}; /* read in 4 byte */

        fd=open("/proc/loadavg", O_RDONLY|O_CLOEXEC);
        if(fd<0) {
                printf("Fail to open /proc/loadavg!\n");
                return -1;
        }
        if(read(fd,strload, 35)<0) {   /* in form of x.xx */
		close(fd);
		return -2;
	}
        load=atof(strload)*100/6; /* set 6 as MAX. of loadavg */
	if(load>100)load=100;

	/* Write to LCD */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        16, 16,(const unsigned char *)strload,  /* fw,fh, pstr */
                                        320, 1, 15,                             /* pixpl, lines, gap */
                                        25, 5,                                 /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

	close(fd);
	return load;
}

/*-----------------------------------------------------------------
Display system running time.

struct sysinfo {
      long uptime;             // Seconds since boot
      unsigned long loads[3];  // 1, 5, and 15 minute load averages
      unsigned long totalram;  // Total usable main memory size
      unsigned long freeram;   // Available memory size
      unsigned long sharedram; // Amount of shared memory
      unsigned long bufferram; // Memory used by buffers
      unsigned long totalswap; // Total swap space size
      unsigned long freeswap;  // swap space still available
      unsigned short procs;    // Number of current processes
      unsigned long totalhigh; // Total high memory size
      unsigned long freehigh;  // Available high memory size
      unsigned int mem_unit;   // Memory unit size in bytes
      char _f[20-2*sizeof(long)-sizeof(int)]; // Padding to 64 bytes
};


Return:
	>=0 	OK
	<0	Fails
-------------------------------------------------------------------*/
int display_sysinfo(void)
{
	struct sysinfo info;
	char strtmp[256]={0};
	long	secs;
	int percent;
	EGI_16BIT_COLOR color;

	if ( sysinfo(&info) != 0)
		return -1;

	secs=info.uptime;
	sprintf(strtmp,"运行时间:　%ld天%ld小时%ld分钟", secs/86400, (secs%86400)/3600, (secs%3600)/60 );
//	sprintf(strtmp,"RUNTIME: %ldD_%ldH_%ldM", secs/86400, (secs%86400)/3600, (secs%3600)/60 );

	//printf("Total Mem: %ldM, Free Mem: %ldM\n", (info.totalram)>>20, (info.freeram)>>20);

	/* Write to LCD */
	int fw=18;
	int fh=18;
	int pixlen=FTsymbol_uft8strings_pixlen( egi_sysfonts.bold, fw, fh, (const unsigned char *)strtmp);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)strtmp,  /* fw,fh, pstr */
                                        320, 1, 15,                             /* pixpl, lines, gap */
                                        (320-pixlen)/2, 52, //48                     /* x0,y0, */
                                        WEGI_COLOR_ORANGE, -1, 255,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	/* Free MEM */
	sprintf(strtmp,"FreeM\n%ldM",(info.freeram)>>20);
	percent=100*(info.freeram>>20)/(info.totalram>>20);
	if(percent>=50)
		color=WEGI_COLOR_GREEN;
	else if(percent>=50*2/3)
		color=WEGI_COLOR_YELLOW;
	else
		color=WEGI_COLOR_RED;
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        14, 14,(const unsigned char *)strtmp,  	/* fw,fh, pstr */
                                        320, 2, 5,                             	/* pixpl, lines, gap */
                                        5, 90,                     	/* x0,y0, */
                                        color, -1, 255,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	/* Total MEM */
	sprintf(strtmp,"TotalM\n% ldM",(info.totalram)>>20);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        14, 14,(const unsigned char *)strtmp,  	/* fw,fh, pstr */
                                        320, 2, 5,                             	/* pixpl, lines, gap */
                                        320-56, 90,                     	/* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	return 0;
}

/*-------------------------------------------
Draw needle

@length:	Length of the needle
@angle:		Angle of the needle, left 0
@xrl,yrl:	Turning Center of the needle
@color:		Color for the needle
--------------------------------------------*/
void draw_needle(int length, int angle, int xrl, int yrl, EGI_16BIT_COLOR color)
{
	fbset_color2(&gv_fb_dev, color);
	/* draw circle */
	draw_filled_circle(&gv_fb_dev, xrl, yrl, 12);
	/* Draw half Slice */
	//draw_filled_pieSlice(&gv_fb_dev, xrl, yrl, 25, -MATH_PI, 0);  /* dev, x0, y0, r, float Sang, float Eang */
	/* draw_wline(FBDEV *dev, x1,y1, x2,y2, unsigned int w) */
	float_draw_wline(&gv_fb_dev, xrl, yrl, xrl-length*cos(MATH_PI*angle/180), yrl-length*sin(MATH_PI*angle/180), 6, false);
	//draw_wline(&gv_fb_dev, xrl, yrl, xrl-length*cos(MATH_PI*angle/180), yrl-length*sin(MATH_PI*angle/180), 6);

}
