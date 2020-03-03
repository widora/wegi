/*--------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple test for displaying PNG files, and write some words to it then save
it to PNG file again.

Usage: test_png input_file output_file

Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"

#define PNG_FIXED_POINT_SUPPORTED 	/* for read */
#define PNG_READ_EXPAND_SUPPORTED

int main(int argc, char **argv)
{
	bool test_save;
	int ret=0;
	int py;
	int dw,dh; /* displaying window width and height */
	EGI_IMGBUF  *eimg=NULL;
	FBDEV vfb;

	if( argc<2 ) {
		printf("usage: %s  input_file  [save_file] \n",argv[0]);
		return -1;
	}
	if( argc > 2)
		test_save=true;
	else
		test_save=false;

        tm_start_egitick();

do { ///////////////////////////   LOOP TEST  ////////////////////////

        /* --- prepare fb device --- */
        if( init_fbdev(&gv_fb_dev) )
		return -1;

	/* load app fonts */
        if(FTsymbol_load_appfonts() !=0 ) {  /* and FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }


	clear_screen(&gv_fb_dev, WEGI_COLOR_BLACK);

	/* load image data to EGI_IMGBUF */
	eimg=egi_imgbuf_new();
	if( egi_imgbuf_loadpng(argv[1], eimg ) ==0 ) {
	}
	else if( egi_imgbuf_loadjpg(argv[1], eimg ) ==0 ) {
	}
	else {
		egi_imgbuf_free(eimg);
		return -2;
	}

/* <<<<<<<<<<<<<<<<<<<<<<<<<   test PNG displaying   >>>>>>>>>>>>>>>>>>>>>>>>> */
#if 1  /* window_position displaying */
	dw=eimg->width>240?240:eimg->width;
	dh=eimg->height>320?320:eimg->height;
//        egi_imgbuf_windisplay(&eimg, &gv_fb_dev, -1, 0, 0, 0, py, dw, dh);
        egi_imgbuf_windisplay2(eimg, &gv_fb_dev, 0, 0, 0, 0, dw, dh);
#elif 0  /* test subimage and subcolor */
	EGI_IMGBOX subimg;
	subimg.x0=0; subimg.y0=0;
	subimg.w=100; subimg.h=100;
	eimg->subimgs=&subimg;
	eimg->subtotal=1;
//	egi_subimg_writeFB(&eimg, &gv_fb_dev, 0, -1, 70, 220);
	egi_subimg_writeFB(eimg, &gv_fb_dev, 0, WEGI_COLOR_WHITE, 70, 220);
#else
        egi_imgbuf_windisplay(eimg, &gv_fb_dev, -1,0, 0, 70, 220, eimg->width, eimg->height);
#endif


/* <<<<<<<<<<<<<<<<<<<<<<<<<   test PNG saving   >>>>>>>>>>>>>>>>>>>>>>>>> */
if (test_save) {

	tm_delayms(500);
	clear_screen(&gv_fb_dev, WEGI_COLOR_BLUE);


	if( init_virt_fbdev(&vfb, eimg) )
		return -2;

        /* write on VIRT FB */
        FTsymbol_unicstrings_writeFB(&vfb, egi_appfonts.special,   /* FBdev, fontface */
                                           36, 36, L"五蘊皆空",    /* fw,fh, pstr */
                                           240, 1, 0,           /* pixpl, lines, gap */
                                           0, 0,                      /* x0,y0, */
                                           WEGI_COLOR_ORANGE, -1, -1);   /* fontcolor, stranscolor,opaque */

	/* display again */
        egi_imgbuf_windisplay2(eimg, &gv_fb_dev, 0, 0, 0, 0, dw, dh);

	/* save to PNG file */
	if(egi_imgbuf_savepng(argv[2], eimg)) {
		printf("Fail to save imgbuf to PNG file!\n");
	}

	tm_delayms(500);
}
/* <<<<<<<< end test saving >>>>>>>> */

	egi_imgbuf_free(eimg);
        FTsymbol_release_allfonts();
	release_fbdev(&gv_fb_dev);
        release_virt_fbdev(&vfb);

} while(0); ///////////////////////////   END LOOP TEST  ////////////////////////

	return ret;
}
