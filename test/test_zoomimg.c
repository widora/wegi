/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to resize an image.

Usage:  ./test_zoomimg  fpath(JPG or PNG)

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
int scale;
int block_width;
EGI_IMGBUF* pimg=NULL; /* input picture image */
EGI_IMGBUF* blockimg=NULL;
EGI_IMGBUF* zoomimg=NULL;

   show_jpg("/tmp/home.jpg",&gv_fb_dev, false, 0, 0);


k=0;
do {    ////////////////////////////  3. LOOP TEST   /////////////////////////////////

   /* free and realloc pimg */
   egi_imgbuf_free(pimg); pimg=NULL;
   pimg=egi_imgbuf_alloc();

   /* load picture to pimg */
   if( egi_imgbuf_loadjpg( argv[1],pimg )==0 || egi_imgbuf_loadpng(argv[1],pimg)==0 ) {
   		printf(" Succeed to load file %s!\n", argv[1]);
   } else {
		printf(" Fail to load file %s!\n", argv[1]);
		exit(1);
   }

   show_jpg("/tmp/home.jpg",&gv_fb_dev, false, 0, 0);

   /* i as the px value of the block in the pimg */
   block_width=pimg->width;
   for(i=0; i< pimg->width/2; i+=(block_width*4/240+2) ) { //i+=20 ) {

	/* new block width */
	block_width=pimg->width-(i<<1);

	/* copy the block from pimg to eimg */
        blockimg=egi_imgbuf_blockCopy( pimg, i, i*320/240,   /* ineimg, px, py */
				       block_width*320/240, block_width );   /* height, width */

	/* resize to 240x320 */
	zoomimg=egi_imgbuf_resize( blockimg, 240, 320 );   /* eimg, width, height */

	/* display eimg */
	egi_imgbuf_windisplay( zoomimg, &gv_fb_dev, -1,    	/* img, FB, subcolor */
                               0, 0,  				/* int xp, int yp */
			       0, 0,				/* xw, yw , align left */
			       240, 320   			/* winw,  winh */
			      );

	egi_imgbuf_free(blockimg); blockimg=NULL;
	egi_imgbuf_free(zoomimg);zoomimg=NULL;
	tm_delayms(150);

    }

    tm_delayms(150);
} while(1); ////////////////////////////    LOOP TEST   /////////////////////////////////

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


