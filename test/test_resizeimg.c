/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to resize an image.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"


int main(int argc, char** argv)
{
	int i,j,k;
	int ret;

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
        /* <<<<<  END EGI general init  >>>>>> */



int rad=200;
EGI_IMGBUF* pimg=NULL; /* input picture image */
EGI_IMGBUF* eimg=NULL;
EGI_IMGBUF* softimg=NULL;

show_jpg("/tmp/home.jpg",&gv_fb_dev, false, 0, 0);



  pimg=egi_imgbuf_alloc();


//(egi_imgbuf_loadjpg("/tmp/test.jpg", pimg);
//egi_imgbuf_loadpng("/tmp/test.png", pimg);

   if( egi_imgbuf_loadjpg(argv[1],pimg)==0 || egi_imgbuf_loadpng(argv[1],pimg)==0 ) {
   		printf(" Succeed to load file %s!\n", argv[1]);
   } else {
		printf(" Fail to load file %s!\n", argv[1]);
		exit(1);
   }


#if 0 /////////////////////////   1. TEST image block copy   ///////////////////
	EGI_IMGBUF *blockimg=NULL;
	blockimg=egi_imgbuf_blockCopy( pimg,  			/* ineimg */
                                       0 , 0, 200, 200 );   /* px, py, height, width */

     if(blockimg != NULL) {
	egi_imgbuf_windisplay( blockimg, &gv_fb_dev, -1,    	/* img, FB, subcolor */
                               0, 0,   				/* int xp, int yp */
			       0, 0,				/* xw, yw , align left */
			       blockimg->width, blockimg->height   /* winw,  winh */
			       //0, 0, eimg->width>240?240:eimg->width , eimg->height   /* xw, yw, winw,  winh */
			      );
     }

	exit(0);
#endif

#if 1 /////////////////////////   2. TEST resize to 1x1    ///////////////////////////
	eimg=egi_imgbuf_resize( pimg, 1,1 );   /* eimg, width, height */
	if(eimg==NULL) {
		printf("Fail to resize to 1x1(2x2)!\n");
		exit(-1);
	}
	printf("start windisplay 2x2 image...\n");
	clear_screen(&gv_fb_dev, WEGI_COLOR_BLACK);
	egi_imgbuf_windisplay( eimg, &gv_fb_dev, -1,    	/* img, FB, subcolor */
                               0, 0,   				/* int xp, */
			       0, 0,				/* xw, yw */
			       eimg->width, eimg->height   /* winw,  winh */
			      );
	egi_imgbuf_free(eimg);

//	exit(0);
#endif

k=0;
do {    ////////////////////////////  3. LOOP TEST   /////////////////////////////////

   egi_imgbuf_free(pimg); pimg=NULL;
   pimg=egi_imgbuf_alloc();
   if( egi_imgbuf_loadjpg( argv[1],pimg )==0 || egi_imgbuf_loadpng(argv[1],pimg)==0 ) {
   		printf(" Succeed to load file %s!\n", argv[1]);
   } else {
		printf(" Fail to load file %s!\n", argv[1]);
		exit(1);
   }


   printf("show jpg...\n");
   show_jpg("/tmp/home.jpg",&gv_fb_dev, false, 0, 0);

   #if  1 /* ----- scale step 12/240 for W240H320 image  -------- */
   for(i=24, j=32; i<=240*4; i+=24, j+=32 ) {
   #else  /* ----- scale step 1/240 for W240H320 image  -------- */
   for(i=0, j=0; i<=240*3; i+=1 ) {
   #endif

	k++;
	//show_jpg("/tmp/home.jpg",&gv_fb_dev, false, 0, 0);

	/* resize */
	eimg=egi_imgbuf_resize( pimg, i, i*320/240 );   /* eimg, width, height */
	//eimg=egi_imgbuf_resize( pimg, i, i ); /* eimg, width, height */
	if(eimg==NULL)
		exit(-1);

	#if 0 /* >>>>>>  copy a block to replace pimg >>>>>>>>> */
	/* if size is big as 240x320, then copy a 240x320 size block to replace pimg */
	if(i>240-1) {
		egi_imgbuf_free(pimg);
		pimg=egi_imgbuf_blockCopy( eimg,  			/* ineimg */
                                           (i-240)/2,0, 320, 240 );   /* px, py, height, width */
		/* re_adjust i,j */
		i=240;j=320;
	}
	#endif  /* <<<<<<< END <<<<<<< */

	printf("start windisplay...\n");
	egi_imgbuf_windisplay( eimg, &gv_fb_dev, -1,    	/* img, FB, subcolor */
                               //0, 0,  			/* int xp, int yp */
			       i>240 ? (i-240)/2:0,		/* xw  */
			       i>240 ? (i-240)*pimg->height/2/pimg->width:0, 	/* yw  */
			       0, 0,				/* xw, yw , align left */
			       eimg->width, eimg->height   /* winw,  winh */
			       //0, 0, eimg->width>240?240:eimg->width , eimg->height   /* xw, yw, winw,  winh */
			      );

	egi_imgbuf_free(eimg);
	tm_delayms(150);

	/* break for() */
	if(k>60) {
		k=0;
		break;
	}
    }

    tm_delayms(3000);

}while(1); ////////////////////////////    LOOP TEST   /////////////////////////////////

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


