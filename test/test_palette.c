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

/* RGB Triplet Color Chart is devided into 18 blocks, each block has two columns,  with 6 colors for each column,
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
	EGI_TOUCH_DATA	touch_data;

	#define MAX_BTNS	2
	#define BTNID_NEXT	0
	#define BTNID_PREV	1
	EGI_RECTBTN btns[2]={0};

	btns[BTNID_NEXT]=(EGI_RECTBTN) { .id=BTNID_NEXT, .color=WEGI_COLOR_GRAY,
					 .x0=320-80+1, .y0=0, .offy=-3, .width=80-1, .height=40, .sw=4,
	    				 .pressed=false, .reaction=btn_react };
	btns[BTNID_PREV]=(EGI_RECTBTN) { .id=BTNID_PREV, .color=WEGI_COLOR_GRAY,
					 .x0=320-80+1, .y0=40, .offy=-3, .width=80-1, .height=40, .sw=4,
	    				 .pressed=false, .reaction=btn_react };

	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_BLACK);
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY1, 320-80, 0, 320-1, 240-1);

	/* Draw button */
	egi_sbtn_refresh(&btns[BTNID_NEXT],"NEXT");
	egi_sbtn_refresh(&btns[BTNID_PREV],"PREV");

	/* Render */
	fb_render(&gv_fb_dev);

	/*  --- LOOP ---- */
while(1) {

  for(k=0; k<18; k++) {
	printf(" --- k=%d ---\n", k);

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
		draw_wline_nc(&gv_fb_dev, 0, i*40, 240-1, i*40, 1); /* int x1,int y1,int x2,int y2, unsigned int w */
	for(i=0; i<2+1; i++)
		draw_wline_nc(&gv_fb_dev, i*120, 0, i*120, 320-1, 1); /* int x1,int y1,int x2,int y2, unsigned int w */

	/* Render */
	fb_render(&gv_fb_dev);

	/* ----- Wait for button touch ----- */
	 if(egi_touch_timeWait_press(-1, 0, &touch_data)!=0)
                continue;
        /* Touch_data converted to the same coord as of FB */
        egi_touch_fbpos_data(&gv_fb_dev, &touch_data);

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
        /* Touch missed: Ripple effect */
        if( j==MAX_BTNS ) {
		fb_render(&gv_fb_dev);
                continue;
        }

    }
}

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


/*------------------------------
Bounce_back reaction function
------------------------------*/
static void btn_react(EGI_RECTBTN *btn)
{
        EGI_TOUCH_DATA touch_data;

        /* Draw pressed button */
        btn->pressed=true;

        /* Draw button */
	if(btn->id==BTNID_NEXT)
	        egi_sbtn_refresh(btn, "next");
	else if(btn->id==BTNID_PREV)
		egi_sbtn_refresh(btn, "prev");

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
	if(btn->id==BTNID_NEXT) {
		/* Do nothing, let k and m increment */
	        egi_sbtn_refresh(btn, "NEXT");
	}
	else if(btn->id==BTNID_PREV) {
		/* Move back k (and m) */
		if(k==0) {
			k=17-1;
		}
		else
			k-=2;

		egi_sbtn_refresh(btn, "PREV");
	}
}



