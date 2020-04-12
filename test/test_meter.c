/*---------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of creating a CPU load meter/indicator.

Midas Zhou
midaszhou@yahoo.com
--------------------------------------------------------------------------*/
#include <stdint.h>
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_cstring.h>
#include <egi_FTsymbol.h>


int  percent_cpuLoad(void);


/*------------------
    	MAIN
-------------------*/
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

	int angle=0;
	int barmax=280;
	EGI_IMGBUF 	*meterPanel=egi_imgbuf_readfile("/mmc/meter.png");
	EGI_IMGBUF 	*needle=egi_imgbuf_readfile("/mmc/myneedle.png");

	/* Display back ground scene */
	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_GRAY5);
        egi_imgbuf_windisplay( meterPanel, &gv_fb_dev, -1,          	/* img, fb, subcolor */
                               0, 0,					/* xp, yp */
		(320-meterPanel->width)/2, (240-meterPanel->height)/2+40,      /* xw, yw */
                               meterPanel->width, meterPanel->height);    /* winw, winh */

	/* Draw CPU Load division color mark */
	fbset_alpha(&gv_fb_dev, 252);
	fbset_color(WEGI_COLOR_GREEN);
	draw_warc(&gv_fb_dev, 320/2, 240-8, 50, -MATH_PI, -MATH_PI/2-0.01, 45); /* dev, x0, y0,  r, Sang, Eang,  w */
	fbset_color(WEGI_COLOR_YELLOW);
	draw_warc(&gv_fb_dev, 320/2, 240-8, 50, -MATH_PI/2, -MATH_PI/6, 45); /* dev, x0, y0,  r, Sang, Eang,  w */
	fbset_color(WEGI_COLOR_RED);
	draw_warc(&gv_fb_dev, 320/2, 240-8, 50, -MATH_PI/6, 0, 45); /* dev, x0, y0,  r, Sang, Eang,  w */
	fbreset_alpha(&gv_fb_dev);

	/* Draw bar frame */
	fbset_color(WEGI_COLOR_GRAYB);
	draw_roundcorner_wrect( &gv_fb_dev, 20, 40, 20+barmax, 60, 10, 3); 	/* fbdev, x1,y1, x2,y2, r, w */

	/* Put to FBDEV_BKG_BUFF */
	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */

//	egi_image_rotdisplay( needle, &gv_fb_dev, 30, 115,20, 320/2, 240-8 );    /* imgbuf, fbdev, angle, xri,yri,  xrl,yrl */

	fb_render(&gv_fb_dev);

        /* =============   Main Loop   ============= */
	int delt=2;
	int load_percent=0;
while(1) {

	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */

	//angle+=delt;
	load_percent=percent_cpuLoad();
	angle=180*load_percent/100;

	egi_image_rotdisplay( needle, &gv_fb_dev, angle, 115,20, 320/2, 240-8 );    	/* imgbuf, fbdev, angle, xri,yri,  xrl,yrl */

	fbset_alpha(&gv_fb_dev, 250);
	if(load_percent<50)
		fbset_color(WEGI_COLOR_GREEN);
	else if(load_percent<(50+50*2/3))
		fbset_color(WEGI_COLOR_YELLOW);
	else
		fbset_color(WEGI_COLOR_RED);
	draw_wline(&gv_fb_dev, 20+9, (40+60)/2, barmax*load_percent/100, (40+60)/2, ((60-40)-3) );
	fbreset_alpha(&gv_fb_dev);

	fb_render(&gv_fb_dev);
	tm_delayms(200);

	//if(angle>=180 || angle<=0 ) { delt=-delt; tm_delayms(1000);}
	//else if( angle==(45/2*2) || angle==90 || angle==(135/2*2) )tm_delayms(1000);

}/* end While() */


 	/* ----- MY release ---- */
 	egi_imgbuf_free(meterPanel);


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
                                        20, 10,                                 /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

	close(fd);
	return load;
}
