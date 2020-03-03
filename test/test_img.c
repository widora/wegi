/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test image blur/soft functions.

Usage:
	./test_img  fpath(jpg or png)

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_pcm.h"
#include "egi_FTsymbol.h"


int main(int argc, char** argv)
{
	int i,j,k;
	int ret;
	int blur_size;

	struct timeval tm_start;
	struct timeval tm_end;

        /* <<<<<  EGI general init  >>>>>> */
#if 1
        printf("tm_start_egitick()...\n");
        tm_start_egitick();		   	/* start sys tick */
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_fb") != 0) {	/* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
#endif
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {   	/* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        if(FTsymbol_load_appfonts() !=0 ) {  	/* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }
        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )		/* init sys FB */
                return -1;

	/* <<<<<  End EGI Init  >>>>> */



int rad=200;
EGI_IMGBUF* pimg=NULL;
EGI_IMGBUF* eimg=NULL;
EGI_IMGBUF* softimg=NULL;
EGI_IMGBUF* frameimg=NULL;

//show_jpg("/tmp/home.jpg",&gv_fb_dev, false, 0, 0);


#if 1 /* ------------- 0. TEST egi_imgbuf_alloc() and egi_imgbuf_loadjpg(), egi_imgbuf_loadpng() ----------*/
pimg=egi_imgbuf_alloc();

if( egi_imgbuf_loadjpg(argv[1],pimg)!=0 && egi_imgbuf_loadpng(argv[1],pimg)!=0 ) {
  printf(" Fail to load file %s!\n", argv[1]);
  exit(-1);
}

egi_subimg_writeFB(pimg, &gv_fb_dev, 0, -1, 0,0);  /* imgbuf, fbdev, subnum, subcolor, x0, y0 */
egi_imgbuf_free(pimg);

exit(0);
#endif /* <<<<<<    >>>>>> */


pimg=egi_imgbuf_readfile(argv[1]);

blur_size=45;

do {    ////////////////////////////   1.  LOOP TEST   /////////////////////////////////


#if 0	/* --- change blur_size --- */
	if(blur_size > 35 )
		blur_size -=5;
	else
		blur_size -=3;

	if(blur_size <= 0) {
		tm_delayms(2000);
		blur_size=75;
	}

#else   /* --- keep blur_size --- */
	blur_size=0; //13;
#endif
	printf("blur_size=%d\n", blur_size);



/* ------------------------------------------------------------------------------------
NOTE:
1. egi_imgbuf_avgsoft(), operating 2D array data of color/alpha, is faster than
   egi_imgbuf_avgsoft2() with 1D array data.    --doubt !!??????
   This is because 2D array is much faster for sorting/picking data.!!!? ---doubt

study cases:
1.   1000x500 JPG        nearly same speed for all blur_size.

     ( !!!!!  Strange for 1024x901(odd?)  !!!!! )
2.   1024x901 PNG,RBG    blur_size>13, 2D faster; blur_size<13, nearly same speed.
     1024x900 PNG,RBG    nearly same speed for all blur_size, 1D just a litter slower.

3.   1200x1920 PNG,RGBA  nearly same speed for all blur_size.
     1922x1201 PNG,RGBA  nearly same speed for all blur_size.


-------------------------------------------------------------------------------- */

#if 1 /* <<<<<<<<<<<<<<<<<    1.1 TEST  egi_imgbuf_avgsoft(), 2D array  >>>>>>>>>>>>>> */

	gettimeofday(&tm_start,NULL);
	softimg=egi_imgbuf_avgsoft(pimg, blur_size, true, false); /* eimg, size, alpha_on, hold_on */
	gettimeofday(&tm_end,NULL);
	printf("egi_imgbuf_avgsoft() 2D time cost: %dms\n",tm_signed_diffms(tm_start, tm_end));
	/* set frame */
	#if 1
	egi_imgbuf_setFrame( softimg, frame_round_rect,	/* EGI_IMGBUF, enum imgframe_type */
	                     -1, 1, &rad );		/*  init alpha, int pn, const int *param */
	#endif
	/* display */
	show_jpg("/tmp/home.jpg",&gv_fb_dev, false, 0, 0);

	/* display imgbuf */
	if(softimg==NULL)
		printf("start windisplay img==NULL.\n");
	else {
		printf("start windisplay img H%dxW%d.\n",softimg->height, softimg->width);
		gettimeofday(&tm_start,NULL);
		egi_imgbuf_windisplay( softimg, &gv_fb_dev, -1,    	/* img, FB, subcolor */
        	                       0, 0,   				/* int xp, int yp */
				       0, 0, softimg->width, softimg->height   /* xw, yw, winw,  winh */
				      );
		gettimeofday(&tm_end,NULL);
		printf("windisplay time cost: %dms\n",tm_signed_diffms(tm_start, tm_end));
	}

	/* free softimg */
	egi_imgbuf_free(softimg);

	tm_delayms(500);
#endif /* ---- End test egi_imgbuf_avgsoft() ---- */


#if 1 /* <<<<<<<<<<<<<<<<<   1.2 TEST  egi_imgbuf_avgsoft2(), 1D array  >>>>>>>>>>>>>> */
	gettimeofday(&tm_start,NULL);
	softimg=egi_imgbuf_avgsoft2(pimg, blur_size, true);
	gettimeofday(&tm_end,NULL);
	printf("egi_imgbuf_avgsoft2(): 1D time cost: %dms\n",tm_signed_diffms(tm_start, tm_end));
	/* set frame */
	#if 1
	egi_imgbuf_setFrame( softimg, frame_round_rect,	/* EGI_IMGBUF, enum imgframe_type */
 	                     -1, 1, &rad );		/*  init alpha, int pn, const int *param */
	#endif
	/* display */
	printf("start windisplay...\n");
	show_jpg("/tmp/fish.jpg",&gv_fb_dev, false, 0, 0);
	/* display imgbuf */
	if(softimg==NULL) {
		printf("start windisplay img==NULL.\n");
	}
	else {
		printf("start windisplay img H%dxW%d.\n",softimg->height, softimg->width);
		gettimeofday(&tm_start,NULL);
		egi_imgbuf_windisplay( softimg, &gv_fb_dev, -1,    	/* img, FB, subcolor */
        	                       0, 0,   				/* int xp, int yp */
				       0, 0, softimg->width, softimg->height   /* xw, yw, winw,  winh */
				      );
		gettimeofday(&tm_end,NULL);
		printf("windisplay time cost: %dms\n",tm_signed_diffms(tm_start, tm_end));
	}

	/* free softimg */
	egi_imgbuf_free(softimg);

	tm_delayms(500);
#endif /* ---- End test egi_imgbuf_avgsoft2() ---- */

//	tm_delayms(250/blur_size); //2000);


#if 0  /* ------------- Test egi_imgbuf_newFrameImg() ----------- */
        framimg=egi_imgbuf_newFrameImg( 80, 180,		  	/* int height, int width */
                          	  255, egi_color_random(color_medium), 	/* alpha, color */
				  frame_round_rect,	  		/* enum imgframe_type */
                               	  1, &rad );		  		/* int pn, int *param */

	egi_imgbuf_windisplay( frameimg, &gv_fb_dev, -1,    	/* img, FB, subcolor */
                               0, 0,   				/* int xp, int yp */
			       30,30, eimg->width, eimg->height   /* xw, yw, winw,  winh */
			      );

	egi_imgbuf_free(frameimg);
#endif



}while(1); ///////////////////////////   END LOOP TEST   ///////////////////////////////

	egi_imgbuf_free(pimg);


        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
        printf("egi_quit_log()...\n");
        egi_quit_log();
        printf("<-------  END  ------>\n");

return 0;
}


/* ------------------------- TEST RESULT ------------------

	 <<<  -----   processing a PNG image data, size 1024 x 901. RGB -----   >>>

blur_size=59
avgsoft 2D time cost: 12468ms
avgsoft 1D time cost: 28766ms
start windisplay...
windisplay time cost: 48ms
blur_size=44
avgsoft 2D time cost: 9631ms
avgsoft 1D time cost: 20279ms
start windisplay...
windisplay time cost: 48ms
blur_size=29
avgsoft 2D time cost: 6785ms
avgsoft 1D time cost: 13483ms
start windisplay...
windisplay time cost: 48ms
blur_size=26
avgsoft 2D time cost: 6238ms
avgsoft 1D time cost: 12683ms
start windisplay...
windisplay time cost: 42ms
blur_size=23
avgsoft 2D time cost: 5604ms
avgsoft 1D time cost: 10852ms
start windisplay...
windisplay time cost: 49ms
blur_size=20
avgsoft 2D time cost: 4978ms
avgsoft 1D time cost: 9701ms
start windisplay...
windisplay time cost: 48ms
blur_size=17
avgsoft 2D time cost: 4372ms
avgsoft 1D time cost: 5617ms
start windisplay...
windisplay time cost: 49ms
blur_size=14
avgsoft 2D time cost: 3800ms
avgsoft 1D time cost: 4025ms
start windisplay...
windisplay time cost: 49ms
blur_size=11
avgsoft 2D time cost: 3250ms
avgsoft 1D time cost: 3343ms
start windisplay...
windisplay time cost: 49ms
blur_size=8
avgsoft 2D time cost: 2684ms
avgsoft 1D time cost: 2782ms
start windisplay...
windisplay time cost: 48ms
blur_size=5
avgsoft 2D time cost: 2096ms
avgsoft 1D time cost: 2192ms
start windisplay...
windisplay time cost: 48ms
blur_size=2
avgsoft 2D time cost: 1491ms
avgsoft 1D time cost: 1554ms
start windisplay...
windisplay time cost: 48ms

		<<< ---- blur_size ==1 ---- >>>
blur_size=1
avgsoft 2D time cost: 1173ms
avgsoft 1D time cost: 1161ms
start windisplay...
windisplay time cost: 42ms
blur_size=1
avgsoft 2D time cost: 1158ms
avgsoft 1D time cost: 1163ms
start windisplay...
windisplay time cost: 42ms
blur_size=1
avgsoft 2D time cost: 1153ms
avgsoft 1D time cost: 1163ms
start windisplay...
windisplay time cost: 42ms
blur_size=1
avgsoft 2D time cost: 1160ms
avgsoft 1D time cost: 1161ms
start windisplay...
windisplay time cost: 43ms
blur_size=1
avgsoft 2D time cost: 1153ms
avgsoft 1D time cost: 1158ms


	 <<<  -----   processing a PNG image data, size 1024 x 900 RGB -----   >>>
blur_size=40
egi_imgbuf_avgsoft: --- alpha off ---
egi_imgbuf_avgsoft: malloc 2D array for ineimg->pcolros, palphas ...
egi_imgbuf_avgsoft() 2D time cost: 9988ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 44ms
egi_imgbuf_avgsoft2: --- alpha off ---
egi_imgbuf_avgsoft2(): 1D time cost: 10299ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 156ms
blur_size=35
egi_imgbuf_avgsoft: --- alpha off ---
egi_imgbuf_avgsoft() 2D time cost: 8916ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 44ms
egi_imgbuf_avgsoft2: --- alpha off ---
egi_imgbuf_avgsoft2(): 1D time cost: 9147ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 44ms
blur_size=32
egi_imgbuf_avgsoft: --- alpha off ---
egi_imgbuf_avgsoft() 2D time cost: 8340ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 44ms
egi_imgbuf_avgsoft2: --- alpha off ---
egi_imgbuf_avgsoft2(): 1D time cost: 8422ms
start windisplay...

	 <<<  -----   Typical data for processing a PNG image data, size 532 x 709. -----   >>>

blur_size=59
avgsoft 2D time cost: 6423ms
avgsoft 1D time cost: 7012ms
start windisplay...
windisplay time cost: 48ms
blur_size=44
avgsoft 2D time cost: 5859ms
avgsoft 1D time cost: 5511ms
start windisplay...
windisplay time cost: 42ms
blur_size=29
avgsoft 2D time cost: 3518ms
avgsoft 1D time cost: 4033ms
start windisplay...
windisplay time cost: 37ms
blur_size=26
avgsoft 2D time cost: 3203ms
avgsoft 1D time cost: 3739ms
start windisplay...
windisplay time cost: 37ms
blur_size=23
avgsoft 2D time cost: 2923ms
avgsoft 1D time cost: 3422ms
start windisplay...
windisplay time cost: 34ms
blur_size=20
avgsoft 2D time cost: 2687ms
avgsoft 1D time cost: 3118ms
start windisplay...
windisplay time cost: 33ms
blur_size=17
avgsoft 2D time cost: 2383ms
avgsoft 1D time cost: 2338ms
start windisplay...
windisplay time cost: 32ms
blur_size=14
avgsoft 2D time cost: 2091ms
avgsoft 1D time cost: 1991ms
start windisplay...
windisplay time cost: 30ms
blur_size=11
avgsoft 2D time cost: 1808ms
avgsoft 1D time cost: 1680ms
start windisplay...
windisplay time cost: 28ms
blur_size=8
avgsoft 2D time cost: 1503ms
avgsoft 1D time cost: 1400ms
start windisplay...
windisplay time cost: 26ms
blur_size=5
avgsoft 2D time cost: 1159ms
avgsoft 1D time cost: 1056ms
start windisplay...
windisplay time cost: 23ms
blur_size=2
avgsoft 2D time cost: 896ms
avgsoft 1D time cost: 763ms
start windisplay...
windisplay time cost: 20ms

		<<< ---- blur_size ==1 ---- >>>

blur_size=1
avgsoft 2D time cost: 722ms
avgsoft 1D time cost: 550ms
start windisplay...
windisplay time cost: 20ms
blur_size=1
avgsoft 2D time cost: 720ms
avgsoft 1D time cost: 546ms
start windisplay...
windisplay time cost: 19ms
blur_size=1
avgsoft 2D time cost: 722ms
avgsoft 1D time cost: 549ms


	 <<<  -----   processing a JPG image data, size 1000x500, frames 3 -----   >>>

blur_size=40
egi_imgbuf_avgsoft: malloc 2D array for ineimg->pcolros, palphas ...
egi_imgbuf_avgsoft() 2D time cost: 5064ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 36ms
egi_imgbuf_avgsoft2(): 1D time cost: 5057ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 37ms
blur_size=35
egi_imgbuf_avgsoft() 2D time cost: 4509ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 36ms
egi_imgbuf_avgsoft2(): 1D time cost: 4507ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 36ms
blur_size=32
egi_imgbuf_avgsoft() 2D time cost: 4190ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 36ms
egi_imgbuf_avgsoft2(): 1D time cost: 4165ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 36ms
blur_size=29
egi_imgbuf_avgsoft() 2D time cost: 3864ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 36ms
egi_imgbuf_avgsoft2(): 1D time cost: 3844ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 36ms

	 <<<  -----   processing a PNG image data, size 1200x1920, RGBA -----   >>>

blur_size=40
egi_imgbuf_avgsoft: malloc 2D array for ineimg->pcolros, palphas ...
egi_imgbuf_avgsoft() 2D time cost: 34316ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 184ms
egi_imgbuf_avgsoft2(): 1D time cost: 33748ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 184ms
blur_size=35
egi_imgbuf_avgsoft() 2D time cost: 30556ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 185ms
egi_imgbuf_avgsoft2(): 1D time cost: 30940ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 185ms
blur_size=32
egi_imgbuf_avgsoft() 2D time cost: 28606ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 185ms
egi_imgbuf_avgsoft2(): 1D time cost: 28383ms
......
blur_size=32
egi_imgbuf_avgsoft() 2D time cost: 29069ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 185ms
egi_imgbuf_avgsoft2(): 1D time cost: 28366ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 184ms
blur_size=29
egi_imgbuf_avgsoft() 2D time cost: 26785ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 186ms
egi_imgbuf_avgsoft2(): 1D time cost: 27091ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 184ms
blur_size=26
egi_imgbuf_avgsoft() 2D time cost: 24817ms
start windisplay...
start jpeg_finish_decompress()...
windisplay time cost: 184ms


----------------------------------------------------------*/
