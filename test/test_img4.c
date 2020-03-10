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

        int width=100; 	   // alpha transition belt width.  atoi(argv[1]);
        int ssmode=0b1111; // 4 sides.	//atoi(argv[2]);
        int type=0; 	//atoi(argv[3]);
	char strtmp[32];

	EGI_IMGBUF *frontimg=NULL;
	EGI_IMGBUF *backimg=NULL;
	backimg=egi_imgbuf_readfile("/mmc/backimg.jpg");

	if(argc>1)
		width=atoi(argv[1]);


while(1) {

	printf("fade type=%d\n", type);

	/* WriteFB backimg */
	egi_subimg_writeFB( backimg, &gv_fb_dev, 0, -1, (320-backimg->width)/2, (240-backimg->height)/2);  /* imgbuf, fb_dev, subnum, subcolor, x0, y0  */

	/* Read in frontimg */
	frontimg=egi_imgbuf_readfile("/mmc/frontimg.jpg");

	/* Add fading_out effect to frontimg */
	egi_imgbuf_fadeOutEdges(frontimg, width, ssmode, type);

	/* WriteFB frontimg */
	egi_subimg_writeFB( frontimg, &gv_fb_dev, 0, -1, (320-frontimg->width)/2, (240-frontimg->height)/2); /* imgbuf, fb_dev, subnum, subcolor, x0, y0  */

	/* WriteFB fading type */
        snprintf(strtmp, sizeof(strtmp)-1, "type=%d", type);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,             /* FBdev, fontface */
                                          24, 24,(const unsigned char *)strtmp,    /* fw,fh, pstr */
                                          300, 1, 0,                               /* pixpl, lines, gap */
                                          10, 5,                              	   /* x0,y0, */
                                          WEGI_COLOR_RED, -1, -1,                /* fontcolor, transcolor,opaque */
                                          NULL, NULL, NULL, NULL  );          /* int *cnt, int *lnleft, int* penx, int* peny */

	/* Free  frontimg */
	egi_imgbuf_free2(&frontimg);

	egi_sleep(1, 1, 500);

	type++;
	if(type>4)type=0;
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

