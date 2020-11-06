/*---------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A program to calibrate resistive touch pad by 5_points_method.
EGI config file will be updated then with the results.

Note:
1. Only consider linear X/Y factors for Touch_XY to Screen_XY mapping.
2. Center point x[4]y[4] is the base point for coordinate mapping/converting.
3. Simple, but not good for average touch pad.

		X--- Calibration refrence points
		       ( A 320x240 LCD )

   X <-------------------  Max. 320 ------------------> X
   ^  x[2]y[2]				      x[0]y[0]  ^
   |							|
   |							|
   |							|
  Max. 240		   X x[4]y[4]		     Max. 240
   |							|
   |							|
   |							|
   V   x[3]y[3]				      x[1]y[1]	V
   X <-------------------  Max. 320 ------------------> X

   factX=dLpx/dTpx   factY=dLpy/dTpy  ( dLpx --- delta LCD px, dTpy --- delta TOUCH PAD py )

Midas Zhou
midaszhou@yahoo.com
--------------------------------------------------------------------------*/
#include <stdint.h>
#include <xpt2046.h>
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_cstring.h>
#include <egi_FTsymbol.h>


static float 	factX=1.0;	/* Linear factor:  dLpx/dTpx */
static float 	factY=1.0;	/* Linear factor:  dLpy/dTpy */
static int 	seqnum;		/** Sequence number:
				 *  0:  Left Top point			1: Right Top point
				 *  2:  Left Bottom pint		3: Right Bottom point
				 *  4:  CENTER point
				 *  >=5: Finish
				 */
//static int	  Lpx[5]= { 0, 240-1,    0,    240-1, 240/2 };  /* LCD refrence X,  In order of seqnum */
//static int	  Lpy[5]= { 0,   0,    320-1,  320-1, 320/2 };  /* LCD refrence Y,  In order of seqnum */

static int	  Lpx[5]= { 35, 240-1-35,    35,    240-1-35, 240/2 };  /* LCD refrence X,  In order of seqnum */
static int	  Lpy[5]= { 35,   35,    320-1-35,  320-1-35, 320/2 };  /* LCD refrence Y,  In order of seqnum */

static uint16_t   Tpx[5],Tpy[5];	/* Raw touch data picked correspondingt to Lpx[], Lpy[] */

static void draw_mark(int seqnum);
static void write_txt(char *txt, int px, int py);

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
  fb_set_directFB(&gv_fb_dev,true);
  fb_position_rotate(&gv_fb_dev,1);  /* relative to touch_coord */

 /* <<<<<  End of EGI general init  >>>>>> */


    		  /*-----------------------------------
		   *            Main Program
    		   -----------------------------------*/

	int ret;
//	uint16_t TpxTest, TpyTest;
//	uint16_t LpxTest, LpyTest;
	EGI_TOUCH_DATA	touch;
	EGI_POINT	startxy;
	EGI_POINT	endxy;

START_CALIBRATION:
	/* 1. Pick touch points coordinate */
	for(seqnum=0; seqnum<5; seqnum++)
	{
		/* Hint and refrence mark */
		draw_mark(seqnum);

		/* Read touch data, xy */
		do {
			tm_delayms(5); /* Necessary wait,just for XPT to prepare data */
                	ret=xpt_getraw_xy(&Tpx[seqnum],&Tpy[seqnum]);
		} while ( ret != XPT_READ_STATUS_COMPLETE );

		printf("Tpx[%d]=%d, Tpy[%d]=%d\n", seqnum, Tpx[seqnum], seqnum, Tpy[seqnum]);
		draw_mark(seqnum+1);

		/* Wait for PENUP */
		while( xpt_getraw_xy(NULL,NULL) != XPT_READ_STATUS_PENUP ) { tm_delayms(20); };
  	}


	/* 2. Calculate factX, factY */
	factX= 1.0*((Lpx[1]-Lpx[0] + Lpx[3]-Lpx[2])/2) / ((Tpx[1]-Tpx[0] + Tpx[3]-Tpx[2])/2);    /* Usually take Lpx[1]-Lpx[0] == Lpx[3]-Lpx[2] */
	factY= 1.0*((Lpy[2]-Lpy[0] + Lpy[3]-Lpy[1])/2) / ((Tpy[2]-Tpy[0] + Tpy[3]-Tpy[1])/2);    /* Usually take Lpx[1]-Lpx[0] == Lpx[3]-Lpx[2] */
	printf("factX=%f, factY=%f  tbaseX=%d tbaseY=%d\n", factX, factY, Tpx[4], Tpy[4]);


	/* 3. Set factX,factY */
	xpt_set_factors(factX, factY, Tpx[4], Tpy[4]);


	/* 4. Update EGI config file */
	char strfact[64];
	memset(strfact,0,sizeof(strfact));
	sprintf(strfact,"%.3f", factX);
	egi_update_config_value("TOUCH_PAD", "xpt_factX", strfact);
	memset(strfact,0,sizeof(strfact));
	sprintf(strfact,"%.3f", factY);
	egi_update_config_value("TOUCH_PAD", "xpt_factY", strfact);
	memset(strfact,0,sizeof(strfact));
	sprintf(strfact,"%d", Tpx[4]);
	egi_update_config_value("TOUCH_PAD", "xpt_tbaseX", strfact);
	memset(strfact,0,sizeof(strfact));
	sprintf(strfact,"%d", Tpy[4]);
	egi_update_config_value("TOUCH_PAD", "xpt_tbaseY", strfact);


	/* 5. Check calibration result, Tpx[4]Tpy[4] is the base point  */

	/* Draw refrence box */
	fbset_color(WEGI_COLOR_GREEN);
	draw_rect(&gv_fb_dev, Lpx[0], Lpy[0], Lpx[3], Lpy[3]);

	#if 0  /* --- Option: Test 20 points --- */
	for(i=0; i<20; i++) {   /* Test 20 points */
		/* Read touch data, xy */
		do {
			tm_delayms(5); /* Necessary wait,just for XPT to prepare data */
                	ret=xpt_getraw_xy(&TpxTest,&TpyTest);
		} while ( ret != XPT_READ_STATUS_COMPLETE );

		/* Map to LCD coordinates */
		LpxTest=(TpxTest-Tpx[4])*factX + 240/2;
		LpyTest=(TpyTest-Tpy[4])*factY + 320/2;

		/* Draw a circle there */
		fbset_color(WEGI_COLOR_RED);
		draw_circle(&gv_fb_dev, LpxTest, LpyTest, 5);

		/* Wait for PENUP */
		while( xpt_getraw_xy(NULL,NULL) != XPT_READ_STATUS_PENUP ) { tm_delayms(20); };
	}

	#else  /* --- Option: Test sketching --- */

	/* Wait touch down */
	while(1) {
		egi_touch_timeWait_press(-1, 0, &touch);
//      	egi_touch_fbpos_data(&gv_fb_dev, &touch);

		/* Press: Get start point */
        	startxy=touch.coord;
        	endxy=startxy;

		while (1) {
                	while(!egi_touch_getdata(&touch));
//                	egi_touch_fbpos_data(&gv_fb_dev, &touch);

	                /* Releasing: confirm the end point */
        	        if(touch.status==releasing || touch.status==released_hold) {
                	        break;
                	}

	                /* Get end point */
        	        startxy=endxy;
                	endxy=touch.coord;

			printf("Startxy: %d,%d		Endxy: %d,%d\n", startxy.x, startxy.y, endxy.x, endxy.y );

        	        /* DriectFB Draw Line */
                	fbset_color(WEGI_COLOR_YELLOW);
	                draw_wline(&gv_fb_dev, startxy.x, startxy.y, endxy.x, endxy.y,5);
		}
	}
	#endif

	/* For next round */
	clear_screen(&gv_fb_dev,WEGI_COLOR_DARKGRAY);
	write_txt("准备进入新一轮测试...", 30, 320/2-40);
	tm_delayms(3000);

  goto START_CALIBRATION;

 /* ----- MY release ---- */


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



/*-------------------------------
Draw a mark of reference point on
the LCD.
@seqnum
------------------------------*/
static void draw_mark(int seqnum )
{
	int r=5;		/* Radium of circle mark */
	int crosslen=18;	/* Length of cross mark */

	/* Clear screen first */
	clear_screen(&gv_fb_dev,WEGI_COLOR_DARKGRAY);


	/* Switch seqnum and show Tips */
	fbset_color(WEGI_COLOR_RED);
	switch(seqnum)
	{
		case 0:	/* Left Top */
			//printf("Press Left Top point.\n ");
			write_txt("请点击左上角标记点\n 点中后会跳到下一点．", 30, 50); break;
		case 1:	/* Right Top */
			//printf("Press Right Top point.\n ");
			write_txt("请点击右上角标记点", 30, 50); break;
		case 2: /* Left Bottom */
			//printf("Press Left Bottom point.\n ");
			write_txt("请点击左下角标记点", 30, 320-70); break;
		case 3: /* Right Bottom */
			//printf("Press Right Bottom point.\n ");
			write_txt("请点击右下角标记点", 30, 320-70); break;
		case 4: /* CENTER */
			//printf("Press Center point.\n ");
			write_txt("请点击中心标记点", 30, 320/2-40); break;
		default:
			write_txt("完成校准,请点击测试．．．", 20, 5);
			break;
	}

	/* Draw a circle and a cross */
	if( seqnum < 5) {
		draw_circle(&gv_fb_dev, Lpx[seqnum], Lpy[seqnum], r);
		draw_line(&gv_fb_dev, Lpx[seqnum]-crosslen/2, Lpy[seqnum], Lpx[seqnum]+crosslen/2, Lpy[seqnum]);
		draw_line(&gv_fb_dev, Lpx[seqnum], Lpy[seqnum]-crosslen/2, Lpx[seqnum], Lpy[seqnum]+crosslen/2);
	}

	/* Render  */
	fb_render(&gv_fb_dev);
}


/*-------------------------------
	Write TXT
@txt: 	Input text
@px,py:	LCD X/Y for start point.
-------------------------------*/
static void write_txt(char *txt, int px, int py)
{
       FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        20, 20,(const unsigned char *)txt,    /* fw,fh, pstr */
                                        240, 2, 4,                                  /* pixpl, lines, gap */
                                        px, py,                                    /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
}
