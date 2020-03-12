/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple puzz game to test egi_imgbuf_copyBlock().

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_FTsymbol.h>
#include <egi_pcm.h>

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
   fb_set_directFB(&gv_fb_dev,false);
   fb_position_rotate(&gv_fb_dev,3);

 /* <<<<<  End of EGI general init  >>>>>> */
	int i,j;

	/* block size */
	int bw=106; /* 320/3 */
	int bh=120; /* 240/2 */
	int n;
	int cells[6]={0,1,2,3,4,5};  /* Example:
				      *   cells[3] = 4;  means No.3 screen/canvas block is placed with No.4 image block.
				      *
				      * The original image is separated into 6 blocks and sorted for 0 to 5.
				      * Also the Screen or image canvas is devide into 6 blocks. The cells[] contain the
				      * index numbers of those separated image blocks which located in the cells respectively.
				      */
	int swap_index[2];  /* containing screen/canvas block numbers, for swap operation. */
	int xp,yp;
	int tmp;


#if 0  /* ----- TEST  egi_shuffle_intArray() ----- */
	int nt=50;
	int test[50];
	for(i=0; i<nt; i++)
		test[i]=i;

	egi_shuffle_intArray(test, nt);
	printf("Shuffled test[]");
	for(i=0; i<nt; i++)
		printf(" %d",test[i]);
	printf("\n");
	exit(1);
#endif

	EGI_TOUCH_DATA touch_data;

	EGI_IMGBUF *originimg=egi_imgbuf_readfile("/mmc/puzz.jpg");

	EGI_IMGBUF *playimg=egi_imgbuf_createWithoutAlpha(240, 320, 0);
	EGI_IMGBUF *swapimg=egi_imgbuf_createWithoutAlpha(bh, bw, 0);
	EGI_IMGBUF *padimg=egi_imgbuf_create(80, 320, 180, WEGI_COLOR_GRAY3);

	EGI_PCMBUF *pcmwin=egi_pcmbuf_readfile("/mmc/sound/yougreat.wav");

	if(!originimg ) {
		printf("Fail to read image file!\n");
		exit(1);
	}
	else if(!playimg || !swapimg) {
		printf("Fail to create playimg or swapimg!\n");
		exit(1);
	}

	/* Shuffer originimg to playimg */
	egi_shuffle_intArray(cells, 6); /* shuffle index */
	printf("Cells[]: ");
	for(j=0; j<6; j++) {
		printf(" %d",cells[j]);
		/* destimg, srcimg, bw, bh, xd, yd, xs, ys */
		egi_imgbuf_copyBlock(playimg, originimg, bw, bh, j%3*bw, j/3*bh, cells[j]%3*bw, cells[j]/3*bh);
	}
	printf("/n");
	/* display playimg */
	egi_subimg_writeFB(playimg, &gv_fb_dev, 0, -1, 0, 0); /* imgbuf, fb_dev, subnum, subcolor, x0, y0  */
	fb_page_refresh(&gv_fb_dev, 0);

	n=0;
while(1) {
        if( egi_touch_timeWait_press(0, 100, &touch_data)==0 ) {

		/* touch_data change to same coord as of FB */
		egi_touch_fbpos_data(&gv_fb_dev, &touch_data);

		/* Get index number of the touched block area, of the screen  */
		swap_index[n++]=3*(touch_data.coord.y/bh)+(touch_data.coord.x/bw);

		/* Put a frame mark */
		xp=(swap_index[n-1]%3)*bw;
		yp=(swap_index[n-1]/3)*bh;
        	fbset_color(WEGI_COLOR_PINK);
        	draw_wrect(&gv_fb_dev, xp, yp, xp+bw-1, yp+bh-1, 5);
		fb_page_refresh(&gv_fb_dev, 0);
		egi_sleep(1,0,50);

		/* Swap block image */
		if(n==2) {
			/* reset n */
			n=0;

			/* copy swap_index[0] block to swapimg */
			egi_imgbuf_copyBlock(swapimg, playimg, bw, bh, 0, 0, bw*(swap_index[0]%3), bh*(swap_index[0]/3)); /* destimg, srcimg, bw, bh, xd, yd, xs, ys */
			/* copy swap_index[1] block to swap_index[0] block area */
			egi_imgbuf_copyBlock(playimg, playimg, bw, bh,  bw*(swap_index[0]%3), bh*(swap_index[0]/3), bw*(swap_index[1]%3), bh*(swap_index[1]/3)); /* destimg, srcimg, bw, bh, xd, yd, xs, ys */
			/* copy swapimg to swap_index[1] block area */
			egi_imgbuf_copyBlock(playimg, swapimg, bw, bh,  bw*(swap_index[1]%3), bh*(swap_index[1]/3), 0, 0); /* destimg, srcimg, bw, bh, xd, yd, xs, ys */

			/* Display update playimg */
			egi_subimg_writeFB(playimg, &gv_fb_dev, 0, -1, 0, 0); /* imgbuf, fb_dev, subnum, subcolor, x0, y0  */
			fb_page_refresh(&gv_fb_dev, 0);

			/* Swap cell[] */
			tmp=cells[swap_index[0]];
			cells[swap_index[0]] = cells[swap_index[1]];
			cells[swap_index[1]] = tmp;
			printf("Sorting: ");
			for(i=0; i<6; i++)printf(" %d",cells[i]);
			printf("\n");

			/* Chec result */
			for(i=0; i<6; i++) {
				if(cells[i]!=i) break;
				else if(i < 6-1 ) continue;

				/*  --- YOU WIN --- */
				printf("   --- YOU WIN --- \n");
				egi_subimg_writeFB(padimg, &gv_fb_dev, 0, -1, 0, 80); /* imgbuf, fb_dev, subnum, subcolor, x0, y0  */
			        FTsymbol_uft8strings_writeFB(  &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
        	                        50, 50,(const unsigned char *)"YOU WIN",    /* fw,fh, pstr */
                	                320, 1, 1,                           	    /* pixpl, lines, gap */
                        	        40, 85,                                    /* x0,y0, */
                                	WEGI_COLOR_PINK, -1, -1,       /* fontcolor, transcolor,opaque */
	                                NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
				fb_page_refresh(&gv_fb_dev, 0);
				egi_pcmbuf_playback("default", pcmwin, 0, 2048, /* dev_name, pcmbuf, int vstep, unsigned int nf */
                                                     1, NULL, NULL, NULL);      /* int nloop, bool *sigstop, bool *sigsynch, bool* sigtrigger */

				egi_sleep(1,2,0);

				/* --- Start Next Puzz --- */
				/* Shuffer originimg to playimg */
				egi_shuffle_intArray(cells, 6); /* shuffle index */
				for(j=0; j<6; j++)
					/* destimg, srcimg, bw, bh, xd, yd, xs, ys */
					egi_imgbuf_copyBlock(playimg, originimg, bw, bh, j%3*bw, j/3*bh, cells[j]%3*bw, cells[j]/3*bh);

				/* display playimg */
				egi_subimg_writeFB(playimg, &gv_fb_dev, 0, -1, 0, 0); /* imgbuf, fb_dev, subnum, subcolor, x0, y0  */
				fb_page_refresh(&gv_fb_dev, 0);
			}
		}
	}
}

 /* --- my releae --- */
 egi_imgbuf_free(originimg);
 egi_imgbuf_free(playimg);
 egi_imgbuf_free(swapimg);
 egi_imgbuf_free(padimg);

 egi_pcmbuf_free(&pcmwin);


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


