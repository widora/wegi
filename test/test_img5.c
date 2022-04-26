/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test image operation for icon file.

Journal:
2021-06-07:
	1. Test: egi_imgbuf_savejpg().
2021-08-21:
	1. Test: egi_imgbuf_mapTriWriteFB()/_mapTriWriteFB2()

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

#if 0  /* Load freetype fonts */
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
   fb_set_directFB(&gv_fb_dev,false);
   fb_position_rotate(&gv_fb_dev, 0);

 /* <<<<<  End of EGI general init  >>>>>> */

#if 0 /* ------------------ TEST: EGI_IMGMOTION  ---------------------*/
//int egi_imgmotion_saveHeader(const char *fpath, int width, int height, int delayms, int compress);
//int egi_imgmotion_appendFrame(const char *fpath, EGI_16BIT_COLOR *imgbuf);

  #if 0 /* Compress to frame */
	egi_imgmotion_saveHeader("/tmp/test.mtn", 320, 240, 10, 1);
	egi_imgmotion_saveFrame("/tmp/test.mtn", NULL); /* Just print header */

        /* Prepare fbimg, and hook it to FB */
	EGI_IMGBUF *fbimg=egi_imgbuf_alloc();
        fbimg->width=gv_fb_dev.pos_xres;
        fbimg->height=gv_fb_dev.pos_yres;
        fbimg->imgbuf=(EGI_16BIT_COLOR*)gv_fb_dev.map_fb; /* map_bk */
	/* Save fbimg to file */
	egi_imgmotion_saveFrame("/tmp/test.mtn", fbimg);
	egi_imgmotion_saveFrame("/tmp/test.mtn", NULL); /* Just print header */
  #endif

  #if 1
	int delayms=0;
	if(argc<2)
	     printf("Usage: %s motion_file [delayms] \n", argv[0]);
	if(argc>2) {
		delayms=atoi(argv[2]);
		printf("delayms=%d\n", delayms);
	}
	while(1)
	    	egi_imgmotion_playFile(argv[1], &gv_fb_dev, delayms, 0,0);

	exit(0);
  #endif

#endif


#if 1 /* ------------------ TEST: egi_imgbuf_mapTriWriteFB()  ---------------------*/
   EGI_POINT pts[3];
   int ang=0;
   struct {
   	float u;
   	float v;
   } uvs[3];

   clear_screen(&gv_fb_dev, WEGI_COLOR_DARKPURPLE);

EGI_IMGBUF *eimg=egi_imgbuf_readfile(argv[1]);
   egi_subimg_writeFB(eimg, &gv_fb_dev, 0, -1, 0, 0);

 #if 1
   #define  MAPTRI_MOTION_FILE "/mmc/test_maptri.motion"
   if(egi_imgmotion_saveHeader(MAPTRI_MOTION_FILE, 320, 240, 10, 1)==0)
	printf("Imgmotion file '%s' is created!\n", MAPTRI_MOTION_FILE);
   else {
	printf("Fail to create imgmotion file!\n");
	exit(-1);
   }
   EGI_IMGBUF *fbimg=egi_imgbuf_createLinkFBDEV(&gv_fb_dev);
   if(fbimg==NULL) exit(-1);

 #endif

   /* Mapping uv on original image */
   uvs[0].u=190.0/320.0;   uvs[0].v=130.0/240.0;
   uvs[1].u=250.0/320.0;   uvs[1].v=5.0/240.0;
   uvs[2].u=310.0/320.0;   uvs[2].v=130.0/240.0;

   fbset_color2(&gv_fb_dev, WEGI_COLOR_RED);
   draw_line(&gv_fb_dev, uvs[0].u*320,uvs[0].v*240,  uvs[1].u*320,uvs[1].v*240);
   draw_line(&gv_fb_dev, uvs[0].u*320,uvs[0].v*240,  uvs[2].u*320,uvs[2].v*240);
   draw_line(&gv_fb_dev, uvs[2].u*320,uvs[2].v*240,  uvs[1].u*320,uvs[1].v*240);

   fb_render(&gv_fb_dev);
   fb_copy_FBbuffer(&gv_fb_dev,FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);

 //  sleep(2);

   /* Triangle potins on the screen */
   pts[0].x=5;      pts[0].y=240/2;
   pts[2].x=5+240;  pts[2].y=240/2;

   while(1) {
	if(ang>=0 && ang<=30)  ang+=2;
	else if(ang>=150 && ang<=210) ang+=2;
	else if(ang>=330) ang+=2;
	else
	   ang +=5;

	//if(ang>360) ang=0;
	if(ang>360) exit(0);

	pts[1].x = 5+120+ 120*cos(MATH_PI*ang/180);
	pts[1].y = 120+ 120*sin(MATH_PI*ang/180);

   	fb_copy_FBbuffer(&gv_fb_dev,FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);

	#if 1 /* OPTION_1: Matrix mapping */
   	egi_imgbuf_mapTriWriteFB(eimg, &gv_fb_dev,	/* u0,v0,u1,v1,u2,v2,  x0,y0,z0, x1,y1,z1, x2,y2,z2 */
				uvs[0].u, uvs[0].v,
				uvs[1].u, uvs[1].v,
				uvs[2].u, uvs[2].v,
				pts[0].x, pts[0].y, 2,
				pts[1].x, pts[1].y, 0,
				pts[2].x, pts[2].y, 0
			);

        #elif 0 /* OPTION_2: Barycentric coordinates mapping, FLOAT x/y */
   	egi_imgbuf_mapTriWriteFB2(eimg, &gv_fb_dev,	/* u0,v0,u1,v1,u2,v2,  x0,y0, x1,y1, x2,y2*/
				uvs[0].u, uvs[0].v,
				uvs[1].u, uvs[1].v,
				uvs[2].u, uvs[2].v,
				pts[0].x, pts[0].y,
				pts[1].x, pts[1].y,
				pts[2].x, pts[2].y
			);
        #else /* OPTION_3: Barycentric coordinates mapping, INT x/y */
   	egi_imgbuf_mapTriWriteFB2(eimg, &gv_fb_dev,	/* u0,v0,u1,v1,u2,v2,  x0,y0, x1,y1, x2,y2*/
				uvs[0].u, uvs[0].v,
				uvs[1].u, uvs[1].v,
				uvs[2].u, uvs[2].v,
				roundf(pts[0].x), roundf(pts[0].y),
				roundf(pts[1].x), roundf(pts[1].y),
				roundf(pts[2].x), roundf(pts[2].y)
			);
	#endif

#if 1	/* Draw outline/edges. */
	gv_fb_dev.antialias_on=true;
   	fbset_color2(&gv_fb_dev, WEGI_COLOR_RED);
   	draw_line(&gv_fb_dev, pts[0].x,pts[0].y, pts[1].x,pts[1].y);
   	draw_line(&gv_fb_dev, pts[0].x,pts[0].y, pts[2].x,pts[2].y);
   	draw_line(&gv_fb_dev, pts[1].x,pts[1].y, pts[2].x,pts[2].y);
	gv_fb_dev.antialias_on=false;
#endif
	fb_render(&gv_fb_dev);

 #if 1
        egi_imgmotion_saveFrame(MAPTRI_MOTION_FILE, fbimg);
 #endif

	tm_delayms(60);
	//sleep(1);
  }
  //egi_imgbuf_freeLinkFBDEV(&fbimg);

exit(0);

#endif


#if 0 /* ------------------ TEST: egi_imgbuf_savejpg()  ---------------------*/
	int i;
	EGI_IMGBUF *eimg=egi_imgbuf_readfile(argv[1]);
	EGI_IMGBUF *tmpimg;
	char strtmp[128];

while(1) {
   for(i=0; i<120; i+=5) {
	printf("Compress quality: %d\n", i);
	sprintf(strtmp,"JPEG quality=%d", i);

#if 1	/* Display original JPG */
       // fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);
        egi_imgbuf_windisplay2( eimg, &gv_fb_dev,             /* imgbuf, fb_dev */
                                0, 0, 0, 0,                   /* xp, yp, xw, yw */
                                eimg->width, eimg->height); /* winw, winh */

        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.bold, 	  /* FBdev, fontface */
					20, 20, (const UFT8_PCHAR)"Original", /* fw,fh, pstr */
                                        320, 1, 0,                        /* pixpl, lines, gap */
                                        10, 10,                          /* x0,y0, */
                                        WEGI_COLOR_RED, -1, 255, 	/* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */

        fb_render(&gv_fb_dev);
	tm_delayms(500);
#endif

	/* Save to JPG */
	egi_imgbuf_savejpg("/tmp/test_savejpg.jpg", eimg, i);

	/* Read JPG and show */
	tmpimg=egi_imgbuf_readfile("/tmp/test_savejpg.jpg");

        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);
        egi_imgbuf_windisplay2( tmpimg, &gv_fb_dev,             /* imgbuf, fb_dev */
                                0, 0, 0, 0,                   /* xp, yp, xw, yw */
                                tmpimg->width, tmpimg->height); /* winw, winh */

        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.bold, 	  /* FBdev, fontface */
					20, 20, (const UFT8_PCHAR)strtmp, /* fw,fh, pstr */
                                        320, 1, 0,                        /* pixpl, lines, gap */
                                        10, 10,                          /* x0,y0, */
                                        WEGI_COLOR_RED, -1, 255, 	/* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */

        fb_render(&gv_fb_dev);

        egi_imgbuf_free2(&tmpimg);

	tm_delayms(500);
   }

} /* END while() */

#endif


#if 0 /* ------------------ TEST: ICON file operation  ---------------------*/
	int i,j;
	EGI_IMGBUF *icons_normal=NULL;
	FBDEV vfbdev={0};


        /* Load Noraml Icons */
        //icons_normal=egi_imgbuf_readfile("/mmc/gray_icons_normal.png");
        icons_normal=egi_imgbuf_readfile("/mmc/gray_icons_effect.png");
        if( egi_imgbuf_setSubImgs(icons_normal, 12*5)!=0 ) {
                printf("Fail to setSubImgs for icons_normal!\n");
                exit(EXIT_FAILURE);
        }
	if(icons_normal->alpha)
		printf("PNG file contains alpha data!\n");
	else
		printf("PNG file has NO alpha data!\n");

        /* Set subimgs */
        for( i=0; i<5; i++ )
           for(j=0; j<12; j++) {
                icons_normal->subimgs[i*12+j].x0= 25+j*75.5;            /* 25 margin, 75.5 Hdis */
                icons_normal->subimgs[i*12+j].y0= 145+i*73.5;           /* 73.5 Vdis */
                icons_normal->subimgs[i*12+j].w= 50;
                icons_normal->subimgs[i*12+j].h= 50;
        }

	/* Init virtual FBDEV with icons_normal */
	if( init_virt_fbdev(&vfbdev, icons_normal, NULL) != 0 ) {
		printf("Fail to init_virt_fbdev!\n");
		exit(EXIT_FAILURE);
	}

	/* Draw division blocks on icons_normal image */
	fbset_color2(&vfbdev, WEGI_COLOR_DARKGRAY);
        for( i=0; i<5; i++ )
           for(j=0; j<12; j++) {
		draw_rect(&vfbdev, 25+j*75.5, 145+i*73.5,  25+j*75.5+50-1, 145+i*73.5+50-1);
	}

	/* Save new icon image to file */
	egi_imgbuf_savepng("/mmc/test_img5_icons.png", icons_normal);

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

