/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

To pick a color in palette, and return value in EGI_16BIT_COLOR.

Color classification method:
        Douglas.R.Jacobs' RGB Hex Triplet Color Chart
        http://www.jakesan.com


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_FTsymbol.h>
#include <egi_pcm.h>

static void btn_react(EGI_RECTBTN *btn);

/* RGB Triplet Color Chart is divided into 18 blocks, each block has two columns,  with 6 colors for each column,
 *  and the screen displays one block one time.
 */
static int k;	/* Block index 0-17, see above mentioned. */

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
  fb_position_rotate(&gv_fb_dev,3);

 /* <<<<<  End of EGI general init  >>>>>> */


    		  /*-----------------------------------
		   *            Main Program
    		   -----------------------------------*/

	int i,j;
	char strtmp[128];
	EGI_TOUCH_DATA	touch_data;
	EGI_16BIT_COLOR pixcolor;
	uint8_t		u8color;
	EGI_BOX	 	paletteBox={ { 0, 0 }, { 240-1, 240-1 } };
	EGI_IMGBUF 	*padimg=egi_imgbuf_create( 60, 240, 80, WEGI_COLOR_BLACK );

	/* Buttons */
	#define MAX_BTNS		3
	#define BTNID_NEXT		0
	#define BTNID_PREV		1
	#define BTNID_CLEAR		2
	EGI_RECTBTN 			btns[MAX_BTNS]={0};

	btns[BTNID_NEXT]=(EGI_RECTBTN) { .id=BTNID_NEXT, .color=WEGI_COLOR_GRAY,
					 .x0=320-80, .y0=0, .offy=-3, .width=80-1, .height=40, .sw=4,
	    				 .pressed=false, .reaction=btn_react };
	btns[BTNID_PREV]=(EGI_RECTBTN) { .id=BTNID_PREV, .color=WEGI_COLOR_GRAY,
					 .x0=320-80, .y0=40, .offy=-3, .width=80-1, .height=40, .sw=4,
	    				 .pressed=false, .reaction=btn_react };
	btns[BTNID_CLEAR]=(EGI_RECTBTN) { .id=BTNID_CLEAR, .color=WEGI_COLOR_GRAY, .fw=18, .fh=18,
					 .x0=320-80, .y0=80, .offy=-2, .width=80-1, .height=40, .sw=4,
	    				 .pressed=false, .reaction=btn_react };

	/* Clear FB backbuff */
	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_BLACK);
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY1, 320-80-1, 0, 320-1, 240-1);

	/* Draw buttons */
	egi_sbtn_refresh(&btns[BTNID_NEXT],"NEXT");
	egi_sbtn_refresh(&btns[BTNID_PREV],"PREV");
	egi_sbtn_refresh(&btns[BTNID_CLEAR],"CLEAR");

	/* Mark */
	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                      	22, 22,(const unsigned char *)"   EGI\n 调色板",    /* fw,fh, pstr */
                                       	320, 4, 4,                                  /* pixpl, lines, gap */
                                       	240, 150,                                    /* x0,y0, */
                                       	WEGI_COLOR_WHITE, -1, 255,       /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

	/* Render */
	fb_render(&gv_fb_dev);

while(1) {  /*  --- LOOP ---- */

  for(k=-4; k<18; k++) {
	printf(" --- k=%d ---\n", k);

	/* 1. Display GRAY SCALE colors */
	if(k==-4) {
		for(i=0; i<240; i++) {
			u8color=255-255*i/(240-1);
			fbset_color(COLOR_RGB_TO16BITS(u8color,u8color,u8color));
			draw_line(&gv_fb_dev, 0, i, 240-1, i);
		}
	}
	/* 2. Display RED SCALE colors */
	else if(k==-3) {
		for(i=0; i<240; i++) {
			u8color=255-255*i/(240-1);
			fbset_color(COLOR_RGB_TO16BITS(u8color,0,0));
			draw_line(&gv_fb_dev, 0, i, 240-1, i);
		}
	}
	/* 3. Display GREEN SCALE colors */
	else if(k==-2) {
		for(i=0; i<240; i++) {
			u8color=255-255*i/(240-1);
			fbset_color(COLOR_RGB_TO16BITS(0,u8color,0));
			draw_line(&gv_fb_dev, 0, i, 240-1, i);
		}
	}
	/* 4. Display BLUE SCALE colors */
	else if(k==-1) {
		for(i=0; i<240; i++) {
			u8color=255-255*i/(240-1);
			fbset_color(COLOR_RGB_TO16BITS(0,0,u8color));
			draw_line(&gv_fb_dev, 0, i, 240-1, i);
		}
	}
	/* 5. Display other colors */
	else {
		/* Draw 6_rows and 2_columns color blocks  = one 2Colomn_BLOCK */
		for(i=0; i<6; i++) {
			/* Draw color blocks */
			/* k/3 --- ZONE ROW (6), k%3 --- 2colomn_BLOCK (3)=one ZONE  6*3=18 2colomns for screen  */
			draw_filled_rect2( &gv_fb_dev, COLOR_24TO16BITS(0xFFFFFF-0x330000*(k/3)-0x3300*2*(k%3)-0x33*i),
											0,   i*40,  120-1, (i+1)*40-1 );
			draw_filled_rect2( &gv_fb_dev, COLOR_24TO16BITS(0xFFFFFF-0x330000*(k/3)-0x3300*(2*(k%3)+1)-0x33*i),
											120, i*40,  240-1, (i+1)*40-1 );
		}

		/* Draw grids */
		fbset_color(WEGI_COLOR_BLACK);
		for(i=0; i<6+1; i++)
			//draw_wline_nc(&gv_fb_dev, 0, i*40, 240-1, i*40, 1); /* int x1,int y1,int x2,int y2, unsigned int w */
			draw_line(&gv_fb_dev, 0, i*40, 240-1, i*40); /* int x1,int y1,int x2,int y2, unsigned int w */
		for(i=0; i<2+1; i++)
			draw_wline_nc(&gv_fb_dev, i*120-1, 0, i*120-1, 240-1, 1); /* int x1,int y1,int x2,int y2, unsigned int w */
			//draw_line(&gv_fb_dev, i==0?0:(i*120-1), 0, i==0?0:(i*120-1), 240-1); /* int x1,int y1,int x2,int y2, unsigned int w */
	}


	/* Render */
	fb_render(&gv_fb_dev);


  	while(1) {	/* ----- Wait for button touch ----- */
		if(egi_touch_timeWait_press(-1, 0, &touch_data)!=0)
	                continue;

      		/* Touch_data converted to the same coord as of FB */
        	egi_touch_fbpos_data(&gv_fb_dev, &touch_data);

		/* Touched pallete area: Display color value  */
		if( point_inbox(&touch_data.coord, &paletteBox) ) {
			/* Get pixel color */
			pixcolor=fbget_pixColor(&gv_fb_dev, touch_data.coord.x, touch_data.coord.y);
			fbset_color(pixcolor);
			/* Show color and its value */
			draw_filled_box(&gv_fb_dev, &paletteBox);
			memset(strtmp,0,sizeof(strtmp));
			sprintf(strtmp,"16BIT: 0x%04X\n24BIT: 0x%06X\n",pixcolor,COLOR_16TO24BITS(pixcolor));
			egi_subimg_writeFB(padimg, &gv_fb_dev, 0, -1, 0, 0); /* imgbuf, fbdev, subnum, subcolor, x0, y0 */
	       	 	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        	20, 20,(const unsigned char *)strtmp,    /* fw,fh, pstr */
                                        	320, 2, 4,                                  /* pixpl, lines, gap */
                                        	10, 5,                                    /* x0,y0, */
                                        	WEGI_COLOR_WHITE, -1, 255,       /* fontcolor, transcolor,opaque */
                                        	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */


			fb_render(&gv_fb_dev);
			continue; /* Go to wait while() */
		}

	        /* Check if touched any of the buttons */
        	for(j=0; j<MAX_BTNS; j++) {
                	if( egi_touch_on_rectBTN(&touch_data, &btns[j]) ) { //&& touch_data.status==pressing ) {  /* Re_check event! */
                        	printf("Button_%d touched!\n",j);
	                        btns[j].reaction(&btns[j]);
        	                fb_render(&gv_fb_dev);
                	        break;
	                }
        	}

	        /* j==MAX_BTNS No button touched!  Assume that only one button may be touched each time!  */
        	/* Touch missed: go to wait while() again... */
	        if( j==MAX_BTNS ) {
			//fb_render(&gv_fb_dev);
			continue;
	        }
		else {	/* A button touched: Finally get out of wait while()! */
			break;
		}

  	} /* End while() wait for button touch */
    } /* End for(k) */
} /* End while() loop */

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
Bounce_back reaction function
For all buttons
------------------------------*/
static void btn_react(EGI_RECTBTN *btn)
{
        EGI_TOUCH_DATA touch_data;

        /* Change status */
        btn->pressed=true;

        /* Draw button */
	if(btn->id==BTNID_NEXT)
	        egi_sbtn_refresh(btn, "next");
	else if(btn->id==BTNID_PREV)
		egi_sbtn_refresh(btn, "prev");
	else if(btn->id==BTNID_CLEAR)
		egi_sbtn_refresh(btn, "clear");

	/* Render FB */
        fb_render(&gv_fb_dev);

	/* Wait for next event */
    #if 0  /* OPTION_1: Wait for Touch releasing */
        egi_touch_timeWait_release(-1, 0, &touch_data);
    #else  /* OPTION_2: Wait for Focus lose */
        do{
                tm_delayms(50);
                while(!egi_touch_getdata(&touch_data));
                egi_touch_fbpos_data(&gv_fb_dev, &touch_data);
        }
        while( egi_touch_on_rectBTN(&touch_data, btn) );
    #endif

        btn->pressed=false;

	/*  COMMAND */

	/* 1. Go to next color samples */
	if(btn->id==BTNID_NEXT) {
		/* Do nothing, let k increment */
	        egi_sbtn_refresh(btn, "NEXT");
	}
	/* 2. Go to previous color samples */
	else if(btn->id==BTNID_PREV) {
		/* Move back k (and m) */
		if(k==-4) {
			k=17-1;
		}
		else
			k-=2;

		egi_sbtn_refresh(btn, "PREV");
	}
	/* 3. Refresh/Clear current samples */
	else if(btn->id==BTNID_CLEAR) {
		/* clear current color area */
		k -=1;

		egi_sbtn_refresh(btn, "CLEAR");
	}
}



