/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test image alpha mask effect.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_FTsymbol.h>

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
   fb_set_directFB(&gv_fb_dev,true);
   fb_position_rotate(&gv_fb_dev,3);

 /* <<<<<  End of EGI general init  >>>>>> */



#if 1 ////////////   TEST egi_imgbuf_fadeOutEdges()  /////////////.

	EGI_POINT tripts[3]={ {95, 100 }, {95, 140}, {145, 120} }; /* a triangle mark */

	int k;
	printf("Usage: %s   width ssmode type file\n", argv[0]);
	if(argc<2) {
		printf("Please input width of fading belt, ssmod, type and file path!\n");
		exit(1);
	}

	int width=atoi(argv[1]);
	int ssmode=atoi(argv[2]);
	int type=atoi(argv[3]);


	EGI_IMGBUF *imgbuf=NULL;
	imgbuf=egi_imgbuf_readfile("/mmc/band.png");

	EGI_IMGBUF *imgbk=egi_imgbuf_readfile("/mmc/linux.jpg");
	egi_subimg_writeFB( imgbk, &gv_fb_dev, 0, -1, 0, 0);  /* imgbuf, fb_dev, subnum, subcolor, x0, y0  */


	EGI_IMGBUF *tmpimg=NULL;  /* to backup */
	tmpimg=egi_imgbuf_blockCopy( imgbk, (320-imgbuf->width)/2, 0, 240, imgbuf->width);   /* imgbuf, px, py, height, width */

	EGI_IMGBUF *ptimg=NULL;

	int py=0;
	int step=7;

while(1) {

	ptimg=egi_imgbuf_blockCopy( imgbuf, 0, py, 240, imgbuf->width);   /* imgbuf, px, py, height, width */

	egi_imgbuf_fadeOutEdges(ptimg, width, ssmode, type); //0b0101, 60);

	egi_subimg_writeFB( tmpimg, &gv_fb_dev, 0, -1, (320-ptimg->width)/2, 0); /* restor backup bkimg  */
	egi_subimg_writeFB( ptimg, &gv_fb_dev, 0, -1, (320-ptimg->width)/2, 0);//(240-imgbuf->height)/2 );  /* imgbuf, fb_dev, subnum, subcolor, x0, y0  */

        fbset_color(WEGI_COLOR_RED);
        draw_filled_triangle(&gv_fb_dev, tripts); /* draw a triangle mark */


	egi_sleep(1,0, 55);

	py += step;
	if( py > imgbuf->height-240+1 ) {
		step=-step;
	}
	else if(py < 0) {
		step=-step;
		py=0;
	}
}
	//fb_page_refresh(&gv_fb_dev,0);
#endif





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

 //printf("release virtual fbdev...\n");
 //release_virt_fbdev(&vfb);

 printf("egi_end_touchread()...\n");
 egi_end_touchread();
 #if 0
 printf("egi_quit_log()...\n");
 egi_quit_log();
 #endif
 printf("<-------  END  ------>\n");


return 0;
}

