/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_FTsymbol.h>
#include <egi_pcm.h>

#define USE_EFFECT_MASK
#define TOUCH_POS       3

/* --- Define Buttons --- */
#define MAX_BTNS	12
EGI_RECTBTN 		btns[MAX_BTNS];

/* Button tags */
char *btn_tags[MAX_BTNS]={ "BTN0","BTN1","BTN2","BTN3","BTN4","BTN5","BTN6","BTN7",NULL, };

/* Button React Functions */
static void btn_react(EGI_RECTBTN *btn);

EGI_IMGBUF *iconsImg;		/* Original icons */
EGI_IMGBUF *iconsEffectImg;	/* Effect icons (Add effect to original icons), same size and same .subimgs[] */
EGI_IMGBUF *effectImg_pink;     /* Effect mask image */
EGI_IMGBUF *effectImg_white;
EGI_IMGBUF *effectImg_orange;


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
  fb_position_rotate(&gv_fb_dev,0);

 /* <<<<<  End of EGI general init  >>>>>> */


    		  /*-----------------------------------
		   *            Main Program
    		   -----------------------------------*/

	int i,j;
	EGI_TOUCH_DATA	touch_data;

	int icons_w=74;
	int icons_h=74;
	EGI_IMGBUF 	*iconsImg=egi_imgbuf_readfile("/mmc/gray_icons.png");  /* 5 rows, 6+(6pix)+6 columns */
	EGI_IMGBUF 	*iconsEffectImg=egi_imgbuf_readfile("/mmc/gray_icons_effect.png");  /* 5 rows, 6+(6pix)+6 columns */
	int iconRow=1;  /* 0-4 */
	int xoff;


        /* Define Icon Box in the icons_image */
        if(egi_imgbuf_setSubImgs(iconsImg, 12)!=0) exit(1);
        if(egi_imgbuf_setSubImgs(iconsEffectImg, 12)!=0) exit(1);

	/* Define buttons and icons */
  	for(i=0; i<12; i++) {
		/* --- Button attributes --- */
        	btns[i].id=i;
		btns[i].color=WEGI_COLOR_OCEAN;
	        btns[i].x0=10+(i%4)*icons_w;    /* 4X4 icons on screen */
		btns[i].y0=10+(i/4)*icons_h;
        	btns[i].width=icons_w;
		btns[i].height=icons_h;
	        btns[i].sw=4;
	        btns[i].reaction=btn_react;
        	btns[i].pressed=false;
		#ifdef USE_EFFECT_MASK /* Use mask image effect to add on to iconsImg */
		btns[i].pressimg=iconsImg;
		#else /* Use new iconsEffectImg  */
		btns[i].pressimg=iconsEffectImg;
		#endif
		btns[i].pressimg_subnum=i;
		btns[i].releaseimg=iconsImg;
		btns[i].releaseimg_subnum=i;

		/* --- Icons Boxes --- */
		/* We need first row, 12 icons in:  5 rows 6+(6pix)+6 columns,  */
		if(i%12==11)xoff=-5;
		else xoff=0;
        	iconsImg->subimgs[i]=(EGI_IMGBOX){ 18+(i%12/6)*4+(i%12)*icons_w+xoff, 135+iconRow*icons_h, icons_w , icons_h};
        	iconsEffectImg->subimgs[i]=(EGI_IMGBOX){ 18+(i%12/6)*4+(i%12)*icons_w+xoff, 135+iconRow*icons_h, icons_w , icons_h};
	}

	/* Prepare effect mask image */
	/* Pink mask */
	effectImg_pink=egi_imgbuf_create( icons_h, icons_w, 230, WEGI_COLOR_PINK );  /* h,w, alpha, color */
	egi_imgbuf_fadeOutCircle( effectImg_pink, 230, 10, 30, 0); /* eimg, max_alpha, rad, width, int type */
	egi_imgbuf_blur_update(&effectImg_pink, 4, true);     /* **pimg, int size, bool alpha_on */
	/* White mask */
	effectImg_white=egi_imgbuf_create( icons_h, icons_w, 200, WEGI_COLOR_WHITE );  /* h,w, alpha, color */
	egi_imgbuf_fadeOutCircle( effectImg_white, 200, 12, 24, 0); /* eimg, max_alpha, rad, width, int type */
	egi_imgbuf_blur_update(&effectImg_white, 3, true);     /* **pimg, int size, bool alpha_on */
	/* Orange mask */
	effectImg_orange=egi_imgbuf_create( icons_h, icons_w, 230, WEGI_COLOR_ORANGE );  /* h,w, alpha, color */
	egi_imgbuf_fadeOutCircle( effectImg_orange, 230, 10, 30, 0); /* eimg, max_alpha, rad, width, int type */
	egi_imgbuf_blur_update(&effectImg_orange, 4, true);     /* **pimg, int size, bool alpha_on */

	/* Clear FB backbuff */
	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_BLACK);

	/* Draw buttons */
	for(i=0; i<12; i++)
		egi_sbtn_refresh(&btns[i], btn_tags[i]);

#if 0
	/* Mark */
	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                      	20, 20,(const unsigned char *)" E_CAD\n    v1.0",    /* fw,fh, pstr */
                                       	320, 4, 4,                                  /* pixpl, lines, gap */
                                       	240, 130,                                    /* x0,y0, */
                                       	WEGI_COLOR_WHITE, -1, 255,       /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
#endif

	/* Render */
	fb_render(&gv_fb_dev);


	/* ----- Loop Wait for button touch ----- */
  	while(1) {
		if(egi_touch_timeWait_press(-1, 0, &touch_data)!=0)
	                continue;

      		/* Touch_data converted to the same coord as of FB */
        	egi_touch_fbpos_data(&gv_fb_dev, &touch_data, 3);

	        /* Check if touched any of the buttons */
        	for(j=0; j<MAX_BTNS; j++) {
                	if( egi_touch_on_rectBTN(&touch_data, &btns[j]) ) { //&& touch_data.status==pressing ) {  /* Re_check event! */
                        	printf("Button_%d touched!\n",j);
	                        btns[j].reaction(&btns[j]);
        	                fb_render(&gv_fb_dev);
                	        break;  /* Break for() */
	                }
        	}

	        /* j==MAX_BTNS No button touched!  Assume that only one button may be touched each time!  */
        	/* Touch Drawing board area  */
	        if( j==MAX_BTNS ) {
			fb_render(&gv_fb_dev);
			//continue; /* Got to wait for touch while() again... */
	        }
		else {	/* A button touched: Finally get out of wait while()! */
			//continue;
		}

  	} /* End while() wait for button touch */


 /* ----- MY release ---- */
 egi_imgbuf_free2(&effectImg_pink);
 egi_imgbuf_free2(&effectImg_white);
 egi_imgbuf_free2(&effectImg_orange);
 egi_imgbuf_free2(&iconsImg);

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
        EGI_TOUCH_DATA touch;

#if 1	/* --- 1. For Bounce_Back button --- */
	btn->pressed=true;

	#ifdef USE_EFFECT_MASK
	if((btn->id%3) == 0)
		egi_subimg_writeFB(effectImg_pink,&gv_fb_dev, 0, -1, btn->x0, btn->y0);
	else if( (btn->id%3) == 1 )
		egi_subimg_writeFB(effectImg_white,&gv_fb_dev, 0, -1, btn->x0, btn->y0);
	else
		egi_subimg_writeFB(effectImg_orange,&gv_fb_dev, 0, -1, btn->x0, btn->y0);
	#else
	  /* use new effect icons image */
	  egi_sbtn_refresh(btn, btn_tags[btn->id]);
	#endif

	fb_render(&gv_fb_dev);

	/* Wait for releasing or focus lose */
        #if 1  /* OPTION_1: Wait for Touch releasing */
       	egi_touch_timeWait_release(-1, 0, &touch);
        #else  /* OPTION_2: Wait for Focus lose */
       	do{
               	tm_delayms(50);
                while(!egi_touch_getdata(&touch));
       	        egi_touch_fbpos_data(&gv_fb_dev, &touch, TOUCH_POS);
        }
       	while( egi_touch_on_rectBTN(&touch, btn) );
        #endif
       	btn->pressed=false;

        /* Draw released button */
//        tm_delayms(30); /*  hold on for a while before refresh, in case a too fast 'touch_and_release' event! */
        egi_sbtn_refresh(btn, btn_tags[btn->id]);

#endif


#if 0	/*  --- 2. For Switch/Toggle Buttons --- */
	int i;

        /* Toggle status: Only switch to press_down */
	if(!btn->pressed) {
        	btn->pressed=true;
		cad_draw_function=cad_draw_functions[btn->id];  /* Set current CAD draw function */
	}
        /* Draw all button: reset other buttons  */
	for(i=0; i<MAX_BTNS; i++) {
		if(&btns[i]!=btn)
			btns[i].pressed=false;
	        egi_sbtn_refresh(btns+i, btn_tags[i]);
	}

	/* Render FB */
        fb_render(&gv_fb_dev);

#endif

}

