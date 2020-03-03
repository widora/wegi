/*--------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple test for displaying PNG/JPG files.

Usage: loop_show   path/*.*


Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include "egi_fbgeom.h"
#include "egi_bjp.h"
#include "egi_image.h"
#include "egi_color.h"

#define PNG_FIXED_POINT_SUPPORTED /* for read */
#define PNG_READ_EXPAND_SUPPORTED

int main(int argc, char **argv)
{
	int ret=0;
	int n;

	EGI_IMGBUF  eimg={0};

	if( argc<2 ) {
		printf("Please enter png file name!\n");
		return -1;
	}

        /* --- prepare fb device --- */
        init_fbdev(&gv_fb_dev);

 /* display all png files in input list */
 for(n=1; n<argc; n++) {
	printf("	----- %d/%d ---- \n",n,argc-1);

	if( egi_imgbuf_loadpng(argv[n], &eimg ) ==0 ) {
	}
	else if( egi_imgbuf_loadjpg(argv[n], &eimg ) ==0 ) {
	}
	else {
		/* loop ... */
		if(n==(argc-1))
			n=0;
		continue;
	}

        /* <<<<<<<    Flush FB and Turn on FILO  >>>>>>> */
        printf("Flush pixel data in FILO, start  ---> ");
        fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
        fb_filo_on(&gv_fb_dev); /* start collecting old FB pixel data */

        /* window_position displaying */
#if 0
	int dw,dh; /* displaying window width and height */
	dw=eimg.width>240?240:eimg.width;
	dh=eimg.height>320?320:eimg.height;
        egi_imgbuf_windisplay(&eimg, &gv_fb_dev, -1, 0, 0, 0, 0, dw, dh);
//        egi_imgbuf_windisplay2(&eimg, &gv_fb_dev, 0, 0, 0, 0, dw, dh);
#elif 1  /* test subimage and subcolor */
        EGI_IMGBOX subimg;
        subimg.x0=0; subimg.y0=0;
        subimg.w=eimg.width; subimg.h=eimg.height;
        eimg.subimgs=&subimg;
        eimg.submax=0;
        egi_subimg_writeFB(&eimg, &gv_fb_dev, 0, -1, 0, 0);
//        egi_subimg_writeFB(&eimg, &gv_fb_dev, 0, WEGI_COLOR_WHITE, 93, 240);
#else
        egi_imgbuf_windisplay(&eimg, &gv_fb_dev, -1,0, 0, 70, 220, eimg.width, eimg.height);
#endif
      	/* <<<<<<<    Turn off FILO  >>>>>>> */
        fb_filo_off(&gv_fb_dev);

	//sleep(1);
	usleep(500000);
	egi_imgbuf_cleardata(&eimg);

	/* loop ... */
	if(n==(argc-1))
		n=0;

  } /* end of for() */

	release_fbdev(&gv_fb_dev);

	return ret;
}
