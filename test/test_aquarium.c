/*-----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of creating a digital aquarium with EIG_GIFs and EGI_IMGBUFs.

Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------------*/
#include <stdint.h>
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_cstring.h>
#include <egi_FTsymbol.h>
#include <egi_gif.h>

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
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
        printf("Fail to load FT appfonts, quit.\n");
        return -1;
  }

  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;
#if 0
  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
        return -1;
#endif

  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,false);
  fb_position_rotate(&gv_fb_dev,0);

 /* <<<<<  End of EGI general init  >>>>>> */



    		  /*-----------------------------------
		   *            Main Program
    		   -----------------------------------*/
	int angle;
	EGI_IMGBUF	*bkimg;
	EGI_GIF		*clownfish;
	EGI_GIF		*bigfish;
	EGI_GIF		*smallfish;

	const char *fpic_ocean="/mmc/ocean_water.jpg";
	const char *fgif_clownfish="/mmc/clownfish.gif";
	const char *fgif_bigfish="/mmc/bigfish.gif";
	const char *fgif_smallfish="/mmc/smallfish.gif";

	bkimg=egi_imgbuf_readfile(fpic_ocean);
	if(bkimg==NULL) {
		printf("Fail to read '%s'!\n", fpic_ocean);
		exit(EXIT_FAILURE);
	}
	clownfish=egi_gif_slurpFile(fgif_clownfish, true);
	if(clownfish==NULL) {
		printf("Fail to slurp gif file '%s'!\n", fgif_clownfish);
		exit(EXIT_FAILURE);
	}
	bigfish=egi_gif_slurpFile(fgif_bigfish, true);
	if(bigfish==NULL) {
		printf("Fail to slurp gif file '%s'!\n", fgif_bigfish);
		exit(EXIT_FAILURE);
	}
	smallfish=egi_gif_slurpFile(fgif_smallfish, true);
	if(smallfish==NULL) {
		printf("Fail to slurp gif file '%s'!\n", fgif_smallfish);
		exit(EXIT_FAILURE);
	}

	EGI_GIF_CONTEXT  clownfish_ctxt= {
			.fbdev=NULL, 	/* &gv_fb_dev;  write to imgbuf */
                        .egif=clownfish,
                        .nloop=-1,	/* nonstop */
                        .DirectFB_ON=false,
                        .User_DisposalMode=-1,
                        .User_TransColor=-1,
                        .User_BkgColor=-1,
			/* write to imgbuf, ignore other params. */
	};
	EGI_GIF_CONTEXT  bigfish_ctxt= clownfish_ctxt;
	bigfish_ctxt.egif = bigfish;
	EGI_GIF_CONTEXT  smallfish_ctxt= clownfish_ctxt;
	smallfish_ctxt.egif = smallfish;

	/* Display back ground scene */
        egi_imgbuf_windisplay( bkimg, &gv_fb_dev, -1,          	/* img, fb, subcolor */
                               0, 0, 0, 0,                      /* xp,yp  xw,yw */
                               bkimg->width, bkimg->height);    /* winw, winh */

	/* Start thread to loop updating EGI_GIF.Simgbuf */
        if( egi_gif_runDisplayThread(&clownfish_ctxt) !=0 ) {
                printf("Fail to run thread for clownfish.gif!\n");
		exit(EXIT_FAILURE);
	}
        if( egi_gif_runDisplayThread(&bigfish_ctxt) !=0 ) {
		printf("Fail to run thread for bigfish.gif!\n");
		exit(EXIT_FAILURE);
	}
        if( egi_gif_runDisplayThread(&smallfish_ctxt) !=0 ) {
                printf("Fail to run thread for smallfish.gif!\n");
		exit(EXIT_FAILURE);
	}

        /* Init FB back_ground buffer page */
        //memcpy(gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.map_bk, gv_fb_dev.screensize);
	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
	fb_render(&gv_fb_dev);

        /* =============   Aquarium Motion Loop   ============= */
while(1) {
	angle+=2;
	if(angle>=360) angle=0;

        /* Refresh working buffer with bkg buffer */
        //memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.screensize);
	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */

        /* Beware of displaying sequence! */
	egi_image_rotdisplay( clownfish->Simgbuf, &gv_fb_dev,  angle+100,		 /*  imgbuf, fb_dev, angle */
				clownfish->Simgbuf->width/2, 160,	320/2, 240/2 );  /* xri,yri,  xrl,yrl */

	//while(!bigfish_ctxt.egif->Simgbuf_ready){ printf("---\n"); }; //tm_delayms(5);};
	egi_image_rotdisplay( bigfish->Simgbuf, &gv_fb_dev,  angle,			 /*  imgbuf, fb_dev, angle */
				clownfish->Simgbuf->width/2, 200, 320/2, 240/2 );  /* xri,yri,  xrl,yrl */

	egi_subimg_writeFB( smallfish->Simgbuf, &gv_fb_dev, 0, -1, 0, 100); /* imgbuf, fb_dev, subnum, subcolor, x0, y0 */

	/* Put a tab at bottom */
         FTsymbol_uft8strings_writeFB(  &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        35, 35,(const unsigned char *)"EGI AQUARIUM",    /* fw,fh, pstr */
                                        320, 1, 0,                                  /* pixpl, lines, gap */
                                        15, 240-40,                                    /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 130,    /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

	/* Render to screen */
	fb_render(&gv_fb_dev);
//	exit(1);
	tm_delayms(20);

}/* end While() */


	/* Stop and join thread */
	egi_gif_stopDisplayThread(clownfish);
	egi_gif_stopDisplayThread(bigfish);
	egi_gif_stopDisplayThread(smallfish);


 /* ----- MY release ---- */
 egi_imgbuf_free(bkimg);
 egi_gif_free(&clownfish);
 egi_gif_free(&bigfish);
 egi_gif_free(&smallfish);


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


