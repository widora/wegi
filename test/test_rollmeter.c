/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of drawing a kind of rolling type meter, under directFB mode.
To draw it under Non_directFB mode can avoid tear lines.

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


#if 1  //////////////  Create marking image for a Roller ///////////////
	int i, j;
	char strnum[16];

        /* Init Vrit FB */
	FBDEV vfb;
	EGI_IMGBUF *vfbimg=NULL;
        vfbimg=egi_imgbuf_create( 60*61, 80, 255, WEGI_COLOR_GRAYC); /* height, width, alpha, color */
        if( init_virt_fbdev(&vfb, vfbimg) !=0 ){
                printf("Fail to init virtual FB!\n");
                exit(-1);
        }

        /* Create vfbimg of digits 00-59,00, each subimg size 60x60 */
        for(i=0; i<61; i++) {
                memset(strnum,0,sizeof(strnum));
                if(i==0 || i==60 )
                        strcat(strnum,"00");
                else
                        snprintf(strnum,3,"%02d",i);
                /* Write numbers to vfbimg */
                FTsymbol_uft8strings_writeFB(   &vfb, egi_sysfonts.bold,                /* FBdev, fontface */
                                                30, 30,(const unsigned char *)strnum,   /* fw,fh, pstr */
                                                80, 1, 0,                               /* pixpl, lines, gap */
                                                40, 5+i*60-25,                             /* x0,y0, */
                                                WEGI_COLOR_BLACK, -1, -1,                /* fontcolor, transcolor,opaque */
                                                NULL, NULL, NULL, NULL  );          /* int *cnt, int *lnleft, int* penx, int* peny */
                /* May add a division line */
                fbset_color(WEGI_COLOR_BLACK);
		for(j=0; j<10; j++)
                	draw_wline_nc(&vfb, 0, 60*i+6*j, j==0?30:(j==5?25:20), 60*i+6*j, j==0?3:1);
        }
        egi_imgbuf_savepng("/tmp/band.png", vfbimg);  /* You can use showpic.c to display and check it. */
#endif   //////////////////////////////////////////////////




#if 1 ////////////   TEST egi_imgbuf_fadeOutEdges()  /////////////.

	int pmark=120;  /* position of the indicator */
	EGI_POINT tripts[3]={ {55, 110 }, {55, 130}, {105, pmark} }; /* a triangle mark */

	int width=140;		/* width of aplah transition belt */
	int ssmode=0b0101;	/* sides selection */
	int type=2;		/* fade_out type */

	if(argc>2)
		width=atoi(argv[1]);
	if(argc>3)
		type=atoi(argv[2]);
	printf("Usage: %s fading_belt_width  fading_type ; Or no input params.\n", argv[0]);
	printf("fading_belt_width= %d\n", width);
	printf("fading_type= %d\n",type);

	EGI_IMGBUF *imgbuf=NULL;
	imgbuf=egi_imgbuf_readfile("/tmp/band.png");

	EGI_IMGBUF *imgbk=egi_imgbuf_readfile("/mmc/linux.jpg");
	egi_subimg_writeFB( imgbk, &gv_fb_dev, 0, -1, 0, 0);  /* imgbuf, fb_dev, subnum, subcolor, x0, y0  */


	//EGI_IMGBUF *tmpimg=NULL;  /* to backup parital  imgbk */
	//tmpimg=egi_imgbuf_blockCopy( imgbk, (320-imgbuf->width)/2, 0, 240, imgbuf->width);   /* imgbuf, px, py, height, width */

	EGI_IMGBUF *ptimg=NULL;

	int py=0;
	int step=5;

	/* Draw canvas */
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY3, 0, 0, 320-1, 240-1); //200, 240-1);
	/* Draw roller backcolor */
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_DARKGRAY, 90 -20, 0, 90+80+5, 240-1);

while(1) {

	/* Extract block from imgbuf to ptimg */
	ptimg=egi_imgbuf_blockCopy( imgbuf, 0, py, 240, imgbuf->width);   /* imgbuf, px, py, height, width */
	/* Add fading_out effect to ptimg */
	egi_imgbuf_fadeOutEdges(ptimg, width, ssmode, type);

	/* Draw ptimg as Roller */
	//egi_subimg_writeFB( tmpimg, &gv_fb_dev, 0, -1, (320-ptimg->width)/2-30, 0); /* restor backup bkimg  */
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_DARKGRAY, 90, 0, 90+80, 240-1);
	egi_subimg_writeFB( ptimg, &gv_fb_dev, 0, -1, 90, 0);//(240-imgbuf->height)/2 );  /* imgbuf, fb_dev, subnum, subcolor, x0, y0  */

	/* Draw a frame */
	fbset_color(WEGI_COLOR_FIREBRICK);
	draw_wrect(&gv_fb_dev, 90-20, 0, 90+80+5, 240-1,7);

	/* Draw indicator mark */
        fbset_color(WEGI_COLOR_RED);
        draw_filled_triangle(&gv_fb_dev, tripts);

	 /* Reading number */
	 draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_LTYELLOW, 200, 10, 300, 60);
	 fbset_color(WEGI_COLOR_FIREBRICK);
	 draw_wrect(&gv_fb_dev, 200, 10, 300, 60, 5);
	 snprintf(strnum, 5, "%3.1f", 1.0*(py+pmark)/60 );
         FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,                /* FBdev, fontface */
                                          30, 30,(const unsigned char *)strnum,   /* fw,fh, pstr */
                                          80, 1, 0,                               /* pixpl, lines, gap */
                                          200+10, 10+7,                              /* x0,y0, */
                                          WEGI_COLOR_BLACK, -1, -1,                /* fontcolor, transcolor,opaque */
                                          NULL, NULL, NULL, NULL  );          /* int *cnt, int *lnleft, int* penx, int* peny */

	egi_imgbuf_free(ptimg);ptimg=NULL;
	egi_sleep(1,0, 60);

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

