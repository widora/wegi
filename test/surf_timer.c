/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

TODO:
1. Check whether the userver is disconnected.

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include "egi_timer.h"
#include "egi_color.h"
#include "egi_debug.h"
#include "egi_math.h"
#include "egi_FTsymbol.h"
#include "egi_surface.h"
#include "egi_procman.h"

#define ERING_PATH "/tmp/surfman_test"

EGI_SURFSHMEM	*surfshmem=NULL;	/* Only a ref. to surfman->surface */

static int sigQuit;

/* Signal handler for SurfUser */
void signal_handler(int signo)
{
        if(signo==SIGINT) {
                egi_dpstd("SIGINT to quit the process!\n");
//              sigQuit=1;
//		pthread_mutex_unlock(&surfshmem->mutex_lock);
		exit(0);
        }
}


int main(int argc, char **argv)
{
	int opt;
	char strtm[128];
        time_t t;
        struct tm *tmp;
	int det=-5;

        char *delim=",";
        char saveargs[128];
        char *ps=&saveargs[0];

        int sw=0,sh=0; /* Width and Height of the surface */
        int x0=0,y0=0; /* Origin coord of the surface */

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hscS:p:"))!=-1 ) {
                switch(opt) {
                        case 's':
                                break;
                        case 'c':
                                break;
                        case 'S':
                                sw=atoi(strtok_r(optarg, delim, &ps));
                                sh=atoi(strtok_r(NULL, delim, &ps));
                                printf("Surface size W%dxH%d.\n", sw, sh);
                                break;
                        case 'p':
                                x0=atoi(strtok_r(optarg, delim, &ps));
                                y0=atoi(strtok_r(NULL, delim, &ps));
                                printf("Surface origin (%d,%d).\n", x0,y0);
                                break;
                        case 'h':
                                printf("%s: [-hsc]\n", argv[0]);
                                break;
                }
        }


        /* Set signal handler */
//       egi_common_sigAction(SIGINT, signal_handler);

	/*** NOTE:
	 *	1.  The second eface MAY be registered just BEFORE the first eface is unregistered.
	 */
	EGI_SURFUSER	*surfuser=NULL;

	FBDEV 		*vfbdev=NULL;	/* Only a ref. to surfman->vfbdev  */
	EGI_IMGBUF 	*imgbuf=NULL;	/* Only a ref. to surfman->imgbuf */
	SURF_COLOR_TYPE colorType=SURF_RGB565;

        /* Load freetype fonts */
        printf("FTsymbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0) {
                printf("Fail to load FT sysfonts, quit.\n");
		exit(EXIT_FAILURE);
        }


	/* Create a surfuser */
//	surfuser=egi_register_surfuser(ERING_PATH, 30, 240-100, 260, 110, colorType);
	surfuser=egi_register_surfuser(ERING_PATH, x0, y0, 260, 110, colorType);
	if(surfuser==NULL)
		exit(EXIT_FAILURE);

	/* Get ref imgbuf and vfbdev */
	vfbdev=&surfuser->vfbdev;
	imgbuf=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

	/* Set BK color */
	egi_imgbuf_resetColorAlpha(imgbuf, WEGI_COLOR_DARKBLUE, -1); /* Reset color only */
	/* Top bar */
	draw_filled_rect2(vfbdev,WEGI_COLOR_GRAY5, 0, 0, imgbuf->width-1, 30);
#if 0	/* Max/Min icons */
	fbset_color2(vfbdev, WEGI_COLOR_GRAYA);
        draw_rect(vfbdev, imgbuf->width-1-(10+16+10+16), 7, imgbuf->width-1-(10+16+10), 5+16-1); /* Max Icon */
        draw_rect(vfbdev, imgbuf->width-1-(10+16+10+16) +1, 7 +1, imgbuf->width-1-(10+16+10) -1, 5+16-1 -1); /* Max Icon */
        draw_line(vfbdev, imgbuf->width-1-(10+16), 15-1, imgbuf->width-1-10, 15-1);     /* Mix Icon */
        draw_line(vfbdev, imgbuf->width-1-(10+16), 15-1 -1, imgbuf->width-1-10, 15-1 -1);       /* Mix Icon */
#endif

        /* Put Title */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 		/* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)"X _ O    EGI Timer",   /* fw,fh, pstr */
                                        320, 1, 0,                 	/* pixpl, lines, fgap */
                                        5, 5,                         	/* x0,y0, */
		                        WEGI_COLOR_WHITE, -1, 200,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("shmsize: %zdBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* Loop */
	while(!sigQuit) {
		/* Get time */
		t = time(NULL);
           	tmp = localtime(&t);
	        if (tmp == NULL) {
               		perror("localtime");
               		exit(EXIT_FAILURE);
           	}

                /* Get surface mutex_lock */
                if( pthread_mutex_lock(&surfshmem->mutex_lock) !=0 ) {
                          egi_dperr("Fail to get mutex_lock for surface.");
			  tm_delayms(10);
                          continue;
                }
/* ------ >>>  Surface shmem Critical Zone  */

		/* Clear pad */
		draw_filled_rect2(vfbdev,WEGI_COLOR_DARKOCEAN, 0, 30, imgbuf->width-1, imgbuf->height-1);

	        /* Update time */
        	FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, /* FBdev, fontface */
                	                        18, 18, (const UFT8_PCHAR)asctime(tmp),   /* fw,fh, pstr */
                        	                320, 1, 0,                        /* pixpl, lines, fgap */
                                	        15, 40,                         /* x0,y0, */
                                        	WEGI_COLOR_ORANGE, -1, 255,        /* fontcolor, transcolor,opaque */
	                                        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

        	FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 	/* FBdev, fontface */
                	                        24, 24, (const UFT8_PCHAR)"时间你好！",   /* fw,fh, pstr */
                        	                320, 1, 0,                        /* pixpl, lines, fgap */
                                	        80, 67,                         /* x0,y0, */
                                        	WEGI_COLOR_ORANGE, -1, 255,        /* fontcolor, transcolor,opaque */
	                                        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

#if 0		/* Move surfaces */
		surfshmem->y0 += det;
		if( surfshmem->y0 < 10) det=-det;
		else if( surfshmem->y0 > 240-80 ) det=-det;
#endif

	        /* Activate image */
        	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
		pthread_mutex_unlock(&surfshmem->mutex_lock);

		tm_delayms(100);
	}

	/* Unregister and destroy surfuser */
	egi_unregister_surfuser(&surfuser);

	exit(0);
}
