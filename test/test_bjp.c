/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test EGI BMPJPG unctions

Midas Zhou
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include "egi_timer.h"
#include "egi_fbgeom.h"
#include "egi_color.h"
#include "egi_log.h"
#include "egi.h"
#include "egi_bjp.h"

int main(int argc, char **argv)
{
	char path[128]={0};
	FBDEV fb_dev={ .devname="/dev/fb0", .fbfd=-1,  };

	/* --- init logger --- */
/*
  	if(egi_init_log("/mmc/log_color") != 0)
	{
		printf("Fail to init logger,quit.\n");
		return -1;
	}
*/
        /* --- start egi tick --- */
//        tm_start_egitick();

        /* --- prepare fb device --- */
        if(init_fbdev(&fb_dev)!=0) {
		printf("Fail to init fbdev!\n");
		exit(EXIT_FAILURE);
	}

        /* Set sys FB mode */
        fb_set_directFB(&fb_dev,false);
        fb_position_rotate(&fb_dev,0);


#if 1   /*  --------------  TEST:  egi_save_FBpng()  ------------- */
        /* get time stamp */
        time_t t=time(NULL);
        struct tm *tm=localtime(&t);

	if(argc>1)
	     sprintf(path,argv[1]);
	else {
		//sprintf(path,"/tmp/FB%d-%02d-%02d_%02d:%02d:%02d.png",
		sprintf(path,"/tmp/FB%d-%02d-%02d_%02d_%02d_%02d.png",
        	                   tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour, tm->tm_min,tm->tm_sec);
	}

	/* set pos_rotate, WARNING!!! Make sure that screen image is posed the same way! */
	//fb_dev.pos_rotate=1;
	egi_save_FBpng(&fb_dev, path); /* imgbuf will rotated as per pos_rotate */

	exit(0);
#endif


#if 0   /*  --------------  TEST:  egi_save_FBbmp()  ------------- */
        /* get time stamp */
        time_t t=time(NULL);
        struct tm *tm=localtime(&t);

	if(argc>1)
	     sprintf(path,argv[1]);
	else {
		sprintf(path,"/tmp/FB%d-%02d-%02d_%02d:%02d:%02d.bmp",
        	                   tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour, tm->tm_min,tm->tm_sec);
	}
	egi_save_FBbmp(&fb_dev, path);

// show bmp
//        show_bmp(argv[1],&fb_dev,0,0,0);/* 0-BALCK_ON, 1-BLACK_OFF, 0,0-x0y0 */
//	return 0;
#endif	/* -------- END TEST  ------- */


#if 1   /*  --------------  TEST:  egi_raompic_inwin()  ------------- */
	if( argc < 2 ) {
		printf("Usage: %s file\n", argv[0]);
		exit(-1);
	}
	else {
		egi_roampic_inwin( argv[1], &fb_dev,		/* fpath, fb */
			   	10, 100000, 0, 0, 240, 320 );  	/* step, ntrip, xw, yw, winw, winh */

	}
#endif


  	/* quit logger */
//  	egi_quit_log();

        /* close fb dev */
	release_fbdev(&fb_dev);

	return 0;
}

