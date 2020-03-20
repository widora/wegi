/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


To pick a color in palette, and return value in EGI_16BIT_COLOR.

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_FTsymbol.h>
#include <egi_pcm.h>

void btn_react(EGI_RECTBTN *btn);


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
	int k;	/* pages, each page 6 color blocks */

	EGI_RECTBTN btn[2]={0};

	btn[0]=(EGI_RECTBTN) {  .color=WEGI_COLOR_GRAY, .x0=320-80, .y0=0, .offy=-3, .width=80, .height=40, .sw=4,
	    			.pressed=false, .reaction=btn_react };
	btn[1]=(EGI_RECTBTN) {  .color=WEGI_COLOR_GRAY, .x0=320-80, .y0=40, .offy=-3, .width=80, .height=40, .sw=4,
	    			.pressed=false, .reaction=btn_react };


	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_BLACK);
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY1, 320-80, 0, 320-1, 240-1);

	/* Draw 6_rows and 2_columns color blocks */
	for(i=0; i<6; i++) {
		/* Draw color blocks */
		draw_filled_rect2( &gv_fb_dev, COLOR_24TO16BITS(0xFFFFFF-0x33*i), 	0,   i*40,  120-1, (i+1)*40-1 );
		draw_filled_rect2( &gv_fb_dev, COLOR_24TO16BITS(0xFFFFFF-0x3300-0x33*i), 120, i*40,  240-1, (i+1)*40-1 );
	}

	/* Draw grids */
	fbset_color(WEGI_COLOR_BLACK);
	for(i=0; i<6+1; i++)
		draw_wline_nc(&gv_fb_dev, 0, i*40, 240-1, i*40, 1); /* int x1,int y1,int x2,int y2, unsigned int w */
	for(i=0; i<2+1; i++)
		draw_wline_nc(&gv_fb_dev, i*120, 0, i*120, 320-1, 1); /* int x1,int y1,int x2,int y2, unsigned int w */

	/* Draw button */
	egi_sbtn_refresh(&btn[0],"NEXT");
	egi_sbtn_refresh(&btn[1],"PREV");

	/* Render */
	fb_render(&gv_fb_dev);



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


/*--------------------------
Button reaction function
---------------------------*/
void btn_react(EGI_RECTBTN *btn)
{


}



