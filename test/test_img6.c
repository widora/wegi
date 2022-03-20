/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

TEST: FTsymbol_xxx functions.

Journal:
2021-09-14: Create this file.
	1. Test FTsymbol_uft8strings_writeIMG2().


Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
------------------------------------------------------------------*/
#include <egi_common.h>
#include <unistd.h>
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

#if 1  /* Load freetype fonts */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
        printf("Fail to load FT sysfonts, quit.\n");
        return -1;
  }
#endif

#if 0
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
        printf("Fail to load FT appfonts, quit.\n");
        return -1;
  }
#endif

  FTsymbol_set_SpaceWidth(1.0);

  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;

#if 0  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
        return -1;
#endif

   /* Set sys FB mode */
   fb_set_directFB(&gv_fb_dev, false);
   fb_position_rotate(&gv_fb_dev, 0);

 /* <<<<<  End of EGI general init  >>>>>> */

#if 1 /* ------------------ TEST: FTsymbol_uft8strings_writeIMG2()  ---------------------*/

/*
int  FTsymbol_uft8strings_writeIMG2( EGI_IMGBUF *imgbuf, FT_Face face, int fw, int fh, const unsigned char *pstr,
                                     unsigned int pixpl,  unsigned int lines,  unsigned int gap,
                                     int x0, int y0,  int angle,  int fontcolor,
                                     int *cnt, int *lnleft, int* penx, int* peny )
*/

	int angle;
        UFT8_PCHAR pstr=(UFT8_PCHAR)"Hello_World! 世界你好!\n12345 子丑寅卯辰\nABCEDEFG 甲乙丙丁戊";
	int fw=28, fh=28;
	int bytes;

	/* Start pxy */
	int sx, sy;
	//sx=gv_fb_dev.pos_xres/2;
	//sy=gv_fb_dev.pos_yres/2;
	sx=50;
	sy=240-50; //-50;

        /* Prepare fbimg, and hook it to FB */
        EGI_IMGBUF *fbimg=egi_imgbuf_alloc();
        fbimg->width=gv_fb_dev.pos_xres;
        fbimg->height=gv_fb_dev.pos_yres;
        fbimg->imgbuf=(EGI_16BIT_COLOR*)gv_fb_dev.map_bk;

  int step=2;
  //for(angle=0; angle<90; angle+=1 ) {
   angle=0;
  while(1) {
	#if 0
	if(angle>360) {angle=360; step=-step; };
	if(angle<0) {angle=0; step=-step; };
	#else
	if(angle>90) {angle=90; step=-step; };
	if(angle<0) {angle=0; step=-step; };
	#endif

	/* Clear screen */
	clear_screen(&gv_fb_dev, WEGI_COLOR_GRAY);

	/* Start mark */
	fbset_color2(&gv_fb_dev, WEGI_COLOR_RED);
	draw_circle(&gv_fb_dev, sx, sy, 10);

	bytes=FTsymbol_uft8strings_writeIMG2( fbimg, egi_sysfonts.bold, fw, fh, pstr, /* imgbuf, face, fw, fh, pstr */
					300, 5, 10,		/* pixpl, lines, gap */
					sx, sy, angle, WEGI_COLOR_BLACK, /* x0, y0, angle, fontcolor */
					NULL, NULL, NULL,NULL);	/* *cnt, *lnleft, *penx, *peny */

	printf("FTsymbol write bytes=%d, strlen(pstr)=%d. \n", bytes, strlen((char*)pstr));

	fb_render(&gv_fb_dev);

	tm_delayms(60);

	/* Update angle */
	angle +=step;
   }
	exit(0);
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

