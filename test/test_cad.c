/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple CAD example, to draw line, circle and sketch.

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_FTsymbol.h>
#include <egi_pcm.h>


#define CADBOARD_BKGCOLOR	WEGI_COLOR_BLACK	/* CAD drawing board back ground color */

/* --- Define Buttons --- */
#define MAX_BTNS		5
#define BTNID_LINE		0
#define BTNID_CIRCLE		1
#define BTNID_SKETCH		2
#define BTNID_SPLINE		3
#define BTNID_CLEAR		4
EGI_RECTBTN 			btns[MAX_BTNS];

/* Button tags */
char *btn_tags[MAX_BTNS]={ "Line","Circle","Sketch","Spline","Clear" };

/* Button React Functions */
static void btn_react(EGI_RECTBTN *btn);

/* CAD draw functions */
static void cad_line(EGI_TOUCH_DATA* touch_data);
static void cad_circle(EGI_TOUCH_DATA* touch_data);
static void cad_sketch(EGI_TOUCH_DATA* touch_data);
static void cad_spline(EGI_TOUCH_DATA* touch_data);

typedef void (* CAD_DRAW_FUNCTION)(EGI_TOUCH_DATA *);
CAD_DRAW_FUNCTION cad_draw_functions[MAX_BTNS]={ cad_line, cad_circle, cad_sketch, cad_spline };
CAD_DRAW_FUNCTION cad_draw_function;	/* --- Current selected CAD draw function ---- */


/*-------------------------------------------
Convert touch X,Y to under FB coord.
--------------------------------------------*/
void convert_touchxy(EGI_TOUCH_DATA *touch_data)
{
#if 0 // NOPTE: gv_fb_dev.pos_xres is changed in cad_skecth(), cad_line() ,cad_circle() !!!
	egi_touch_fbpos_data(&gv_fb_dev, touch_data, -90); /* Default FB and TOUCH coord NOT same! */
#else
        int px=320-1-touch_data->coord.y;
	touch_data->coord.y = touch_data->coord.x;
	touch_data->coord.x = px;
#endif
}


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
  fb_position_rotate(&gv_fb_dev, 0); //3);

 /* <<<<<  End of EGI general init  >>>>>> */


    		  /*-----------------------------------
		   *            Main Program
    		   -----------------------------------*/

	int i,j;
	char strtmp[128];
	EGI_POINT tchpt;
	EGI_TOUCH_DATA	touch_data;
	EGI_IMGBUF 	*padimg=egi_imgbuf_create( 60, 240, 80, WEGI_COLOR_BLACK );

	/* --- Button attributes --- */
	btns[BTNID_LINE]=(EGI_RECTBTN) { .id=BTNID_LINE, .color=WEGI_COLOR_GRAY, .fw=18, .fh=18,
					 .x0=320-80, .y0=0, .offy=-3, .width=80-1, .height=40, .sw=4,
	    				 .pressed=false, .reaction=btn_react };
	btns[BTNID_CIRCLE]=(EGI_RECTBTN) { .id=BTNID_CIRCLE, .color=WEGI_COLOR_GRAY, .fw=18, .fh=18,
					 .x0=320-80, .y0=40, .offy=-3, .width=80-1, .height=40, .sw=4,
	    				 .pressed=false, .reaction=btn_react };
	btns[BTNID_SKETCH]=(EGI_RECTBTN) { .id=BTNID_SKETCH, .color=WEGI_COLOR_GRAY, .fw=18, .fh=18,
					 .x0=320-80, .y0=80, .offy=-2, .width=80-1, .height=40, .sw=4,
	    				 .pressed=false, .reaction=btn_react };
	btns[BTNID_SPLINE]=(EGI_RECTBTN) { .id=BTNID_SPLINE, .color=WEGI_COLOR_GRAY, .fw=18, .fh=18,
					 .x0=320-80, .y0=120, .offy=-2, .width=80-1, .height=40, .sw=4,
	    				 .pressed=false, .reaction=btn_react };
	btns[BTNID_CLEAR]=(EGI_RECTBTN) { .id=BTNID_CLEAR, .color=WEGI_COLOR_GRAY, .fw=18, .fh=18,
					 .x0=320-80, .y0=240-40, .offy=-2, .width=80-1, .height=40, .sw=4,
	    				 .pressed=false, .reaction=btn_react };

	/* Clear FB backbuff */
	fb_clear_backBuff(&gv_fb_dev, CADBOARD_BKGCOLOR);

	/* Draw limit box */
	//draw_rect(&gv_fb_dev, 0,0, gv_fb_dev.pos_xres-1, gv_fb_dev.pos_yres-1);
	//fb_render(&gv_fb_dev);

	/* Draw btn area */
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY1, 320-80-1, 0, 320-1, 240-1);


	/* Draw grids */
	fbset_color(WEGI_COLOR_WHITE);
	for(i=0; i<6; i++) {
		for(j=0; j<7; j++) {
			draw_dot(&gv_fb_dev, 20+i*40, 20+j*40);
		}
	}

	/* Draw buttons */
	egi_sbtn_refresh(&btns[BTNID_LINE], btn_tags[BTNID_LINE]);
	egi_sbtn_refresh(&btns[BTNID_CIRCLE], btn_tags[BTNID_CIRCLE]);
	egi_sbtn_refresh(&btns[BTNID_SKETCH], btn_tags[BTNID_SKETCH]);
	egi_sbtn_refresh(&btns[BTNID_SPLINE], btn_tags[BTNID_SPLINE]);
	egi_sbtn_refresh(&btns[BTNID_CLEAR], btn_tags[BTNID_CLEAR]);

	/* Mark */
	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                      	16, 16,(const unsigned char *)" E_CAD\n   v1.0",    /* fw,fh, pstr */
                                       	320, 4, 4,                                  /* pixpl, lines, gap */
                                       	240, 160,                                    /* x0,y0, */
                                       	WEGI_COLOR_DARKBLUE, -1, 255,       /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

	/* Render */
	fb_render(&gv_fb_dev);

	/* ----- Loop Wait for button touch ----- */
  	while(1) {
		if(egi_touch_timeWait_press(-1, 0, &touch_data)!=0)
	                continue;

      		/* Touch_data converted to the same coord as of FB */
		convert_touchxy(&touch_data);

	        /* 1, Check if touched any of the buttons */
		printf("Check touch point (%d,%d) \n", touch_data.coord.x, touch_data.coord.y);

        	for(j=0; j<MAX_BTNS; j++) {
                	if( egi_touch_on_rectBTN(&touch_data, &btns[j]) ) { //&& touch_data.status==pressing ) {  /* Re_check event! */
                        	printf("Button_%d touched!\n",j);
	                        btns[j].reaction(&btns[j]);
        	                fb_render(&gv_fb_dev);
                	        break;  /* Break for() */
	                }
        	}

        	/* 2, Touch Drawing board area  */
	        /*  j==MAX_BTNS No button touched!  Assume that only one button may be touched each time!  */
	        if( j==MAX_BTNS ) {
			if(cad_draw_function)
				cad_draw_function(&touch_data);
			fb_render(&gv_fb_dev);
			//continue; /* Got to wait for touch while() again... */
	        }
		else {	/* A button touched: Finally get out of wait while()! */
			//continue;
		}

  	} /* End while() wait for button touch */


 /* ----- MY release ---- */
 egi_imgbuf_free2(&padimg);


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


/*------------------------------
Switch/toggle reaction function
For all buttons
------------------------------*/
static void btn_react(EGI_RECTBTN *btn)
{
	int i,j;
        EGI_TOUCH_DATA touch;

	/* --- 1. For Clear button --- */
	if(btn->id == BTNID_CLEAR) {
		btn->pressed=true;
	        egi_sbtn_refresh(btn, btn_tags[btn->id]);

		/* Clear drawing arae */
		fbset_color(CADBOARD_BKGCOLOR);
		draw_filled_rect(&gv_fb_dev, 0,0, 240-1,240-1);

		/* Draw grids */
		fbset_color(WEGI_COLOR_WHITE);
		for(i=0; i<6; i++) {
			for(j=0; j<7; j++) {
				draw_dot(&gv_fb_dev, 20+i*40, 20+j*40);
			}
		}

		fb_render(&gv_fb_dev);

	        #if 0  /* OPTION_1: Wait for Touch releasing */
        	egi_touch_timeWait_release(-1, 0, &touch);
	        #else  /* OPTION_2: Wait for Focus lose */
        	do{
                	tm_delayms(50);
	                while(!egi_touch_getdata(&touch));
        	        egi_touch_fbpos_data(&gv_fb_dev, &touch, -90);
	        }
        	while( egi_touch_on_rectBTN(&touch, btn) );
	        #endif
        	btn->pressed=false;

	        /* Draw released button */
        	tm_delayms(30); /*  hold on for a while before refresh, in case a too fast 'touch_and_release' event! */
	        egi_sbtn_refresh(btn, btn_tags[btn->id]);

		return ;
	}

	/*  --- 2. For CAD draw func Buttons --- */

        /* Toggle status: Only switch to press_down */
	if(!btn->pressed) {
        	btn->pressed=true;
		cad_draw_function=cad_draw_functions[btn->id];  /* Set current CAD draw function */
	}
        /* Draw all button: reset other buttons  */
	for(i=0; i<MAX_BTNS; i++) {
		if(&btns[i]!=btn)
			btns[i].pressed=false;

		if(btns[i].id==BTNID_SKETCH &&  btns[i].pressed==true )
			egi_sbtn_refresh(btns+i, "草图");
		else
		        egi_sbtn_refresh(btns+i, btn_tags[i]);
	}

	/* Render FB */
        fb_render(&gv_fb_dev);
}


/*---------------------------------------------
		Draw a line
Current touched point as start point of the line,
and next releasing point defines the end point.
---------------------------------------------*/
static void cad_line(EGI_TOUCH_DATA* touch_data)
{
	EGI_TOUCH_DATA touch;
	EGI_POINT startxy;
	EGI_POINT endxy;

	/* Press: Get start point */
	startxy=touch_data->coord;
	endxy=startxy;

	/* dump filo */
	fb_filo_dump(&gv_fb_dev);

	/* Set screen size to 240x240 */
	gv_fb_dev.pos_xres=240;

	while(1) {

        	while(!egi_touch_getdata(&touch));
            	//egi_touch_fbpos_data(&gv_fb_dev, &touch);
		convert_touchxy(&touch);

		/* Releasing: confirm the end point */
		if(touch.status==releasing || touch.status==released_hold) {
			/* Draw to FB buffer */
			/* OR copy to FB buffer */
			fbset_color(WEGI_COLOR_GREEN);
			draw_line(&gv_fb_dev, startxy.x, startxy.y, endxy.x, endxy.y);

			fb_render(&gv_fb_dev);

			/* Reset screen size back to 240x320 */
			gv_fb_dev.pos_xres=320;

			return;
		}

		/* Get end point */
		endxy=touch.coord;

		/* Set limits */
		if(endxy.x>240-1)endxy.x=240-1;

	        /* Turn on FB filo and set map pointer */
		fb_set_directFB(&gv_fb_dev, true);
        	fb_filo_on(&gv_fb_dev);
        	fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */

		/* DriectFB Draw Line */
		fbset_color(WEGI_COLOR_GREEN);
		draw_line(&gv_fb_dev, startxy.x, startxy.y, endxy.x, endxy.y);
	        tm_delayms(60);

        	/* Turn off FB filo and reset map pointer */
		fb_set_directFB(&gv_fb_dev, false);
        	fb_filo_off(&gv_fb_dev);
	}
}


/*-----------------------------------------------
		Draw a circle
Current touched point as center, and next
releasing point defines the radius.
------------------------------------------------*/
static void cad_circle(EGI_TOUCH_DATA* touch_data)
{
	EGI_TOUCH_DATA touch;
	EGI_POINT startxy;
	EGI_POINT endxy;

	/* Press: Get start point */
	startxy=touch_data->coord;
	endxy=startxy;

	/* Dump filo */
	fb_filo_dump(&gv_fb_dev);

	/* Set screen size to 240x240 */
	gv_fb_dev.pos_xres=240;

	while(1) {

        	while(!egi_touch_getdata(&touch));
            	//egi_touch_fbpos_data(&gv_fb_dev, &touch);
		convert_touchxy(&touch);

		/* Releasing: confirm the end point */
		if(touch.status==releasing || touch.status==released_hold) {
			/* draw to FB buffer */
			/* OR copy to FB buffer */
			fbset_color(WEGI_COLOR_RED);
			draw_circle( &gv_fb_dev, startxy.x, startxy.y,
				     sqrt( (endxy.x-startxy.x)*(endxy.x-startxy.x)+(endxy.y-startxy.y)*(endxy.y-startxy.y) )
			   );

			fb_render(&gv_fb_dev);

			/* Reset screen size back to 240x320 */
			gv_fb_dev.pos_xres=320;

			return;
		}

		/* Release: Get end point */
		endxy=touch.coord;

		/* TODO: draw_circle_limit() */

	        /* Turn on FB filo and set map pointer */
		fb_set_directFB(&gv_fb_dev, true);
        	fb_filo_on(&gv_fb_dev);
        	fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */

		/* DriectFB Draw Line */
		fbset_color(WEGI_COLOR_RED);
		draw_circle( &gv_fb_dev, startxy.x, startxy.y,
			     sqrt( (endxy.x-startxy.x)*(endxy.x-startxy.x)+(endxy.y-startxy.y)*(endxy.y-startxy.y) )
			   );
	        //tm_delayms(60);

        	/* Turn off FB filo and reset map pointer */
		fb_set_directFB(&gv_fb_dev, false);
        	fb_filo_off(&gv_fb_dev);
	}
}


/*-------------------------------------------------
		   Draw Sketch
Current touched point as start point of sketching,
continously trace touch point and draw lines, until
pen release.
-------------------------------------------------*/
static void cad_sketch(EGI_TOUCH_DATA* touch_data)
{
	EGI_TOUCH_DATA touch;
	EGI_POINT startxy;
	EGI_POINT endxy;
	int penw=3;

	/* Press: Get start point */
	startxy=touch_data->coord;
	endxy=startxy;

	/* Set as direct FB */
	fb_set_directFB(&gv_fb_dev, true);

	/* Set screen size to 240x240 */
	gv_fb_dev.pos_xres=240;

	while(1) {

        	while(!egi_touch_getdata(&touch));
		convert_touchxy(&touch);

		/* Releasing: confirm the end point */
		if(touch.status==releasing || touch.status==released_hold) {
			/* Restore map pointer */
			fb_set_directFB(&gv_fb_dev,false);

			/* Draw to FB buffer */
			/* OR copy to FB buffer */
			fb_page_saveToBuff(&gv_fb_dev, 0);
			fb_render(&gv_fb_dev);

			/* Reset screen size back to 240x320 */
			gv_fb_dev.pos_xres=320;
			return;
		}

		/* Get end point */
		startxy=endxy;
		endxy=touch.coord;

		/* Set limits : Consider pen width */
		if(endxy.x>240-1)endxy.x=240-1;

		/* DriectFB Draw Line */
		fbset_color(WEGI_COLOR_YELLOW);
		//draw_line(&gv_fb_dev, startxy.x, startxy.y, endxy.x, endxy.y);
		draw_wline(&gv_fb_dev, startxy.x, startxy.y, endxy.x, endxy.y, penw);
//	        tm_delayms(60);
	}
}


/*-------------------------------------------------
		   Draw Sketch
Current touched point as start point of spline,
continously trace touch point and draw splines, until
gets 5 points.
-------------------------------------------------*/
static void cad_spline(EGI_TOUCH_DATA* touch_data)
{
	int i,k;
	EGI_TOUCH_DATA touch;

	static int np=128;
	EGI_POINT  pts[np];
	int 	   penw=3;
	int	   mr=1;	/* Radius of point mark */
	bool	   stop_spline=false;

	/* Dump filo */
	fb_filo_dump(&gv_fb_dev);

	/* Set as direct FB */
	fb_set_directFB(&gv_fb_dev, true);
	fb_filo_on(&gv_fb_dev);

	/* Set screen size to 240x240 */
	gv_fb_dev.pos_xres=240;

	/* Get start point */
	pts[0]=touch_data->coord;
	fbset_color(WEGI_COLOR_LTBLUE);
	draw_circle(&gv_fb_dev, pts[0].x, pts[0].y, mr);

	/* np_point spline */
	for(i=1; i<np; i++) {
                while( egi_touch_timeWait_press(-1, 0, &touch)!=0 );
		convert_touchxy(&touch);

		/* get pxy */
		pts[i]=touch.coord;

        	fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */

		if(egi_touch_on_rectBTN(&touch, &btns[BTNID_SPLINE])) {
			stop_spline=true;
			i--;
		}

		/* Draw to working buff if MAX np, OR touch btn_spline to stop */
		if( i==np-1 || stop_spline )
		        fb_set_directFB(&gv_fb_dev, false);

		/* Draw spline */
		fbset_color(WEGI_COLOR_LTBLUE);
		for(k=0; k<i+1; k++) {
			draw_circle(&gv_fb_dev, pts[k].x, pts[k].y, mr);
		}
		fbset_color(WEGI_COLOR_GREEN);
		draw_spline2(&gv_fb_dev, i+1, pts, 1, penw);
		printf("%d point spline\n",i+1);

		/* Refresh btn */
		if(stop_spline) {
			printf("refresh btn spline!\n");
			btns[BTNID_SPLINE].pressed=false;
			gv_fb_dev.pos_xres=320;
			egi_sbtn_refresh(btns+BTNID_SPLINE, btn_tags[BTNID_SPLINE]);
			cad_draw_function=NULL;

			break;
		}

	}

        /* Turn off FB filo and reset map pointer */
        fb_filo_off(&gv_fb_dev);

	fb_render(&gv_fb_dev);

	/* Reset screen size back to 240x320 */
	gv_fb_dev.pos_xres=320;
}
