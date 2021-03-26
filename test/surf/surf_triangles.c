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

static int sigQuit;

/* Signal handler for SurfUser */
void signal_handler(int signo)
{
        if(signo==SIGINT) {
                egi_dpstd("SIGINT to quit the process!\n");
                sigQuit=1;
        }
}


int main(int argc, char **argv)
{
	int opt;
	int det=1;

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
        egi_common_sigAction(SIGINT, signal_handler);


	/*** NOTE:
	 *	1.  The second eface MAY be registered just BEFORE the first eface is unregistered.
	 */
	EGI_SURFUSER	*surfuser=NULL;

	FBDEV 		*vfbdev=NULL;	/* Only a ref. to surfman->vfbdev  */
	EGI_IMGBUF 	*imgbuf=NULL;	/* Only a ref. to surfman->imgbuf */
	EGI_SURFSHMEM	*surfshmem=NULL;	/* Only a ref. to surfman->surface */
	SURF_COLOR_TYPE colorType=SURF_RGB565;

	int i;
        EGI_BOX box;
        EGI_BOX tri_box;
        EGI_POINT pts[3];
	EGI_16BIT_COLOR color;

        /* Load freetype fonts */
        printf("FTsymbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0) {
                printf("Fail to load FT sysfonts, quit.\n");
		exit(EXIT_FAILURE);
        }


	/* Create a surfuser */
#if 1
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, 240, 150, colorType);
#else
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, 400, 300, colorType);
#endif
	if(surfuser==NULL)
		exit(EXIT_FAILURE);

	/* Get ref imgbuf and vfbdev */
	vfbdev=&surfuser->vfbdev;
	imgbuf=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

	/* Assign display box */
	box.startxy.x=0;
	box.startxy.y=0; //=(EGI_POINT){0,0};
	box.endxy.x=imgbuf->width-1;
	box.endxy.y=imgbuf->height-1;

	/* Set BK color */
	egi_imgbuf_resetColorAlpha(imgbuf, WEGI_COLOR_LTSKIN, -1); /* Reset color only */

	/* Top bar */
	draw_filled_rect2(vfbdev,WEGI_COLOR_DARKGRAY, 0, 0, imgbuf->width-1, 30);
#if 0	/* Max/Min icons */
	fbset_color2(vfbdev, WEGI_COLOR_GRAYA);
        draw_rect(vfbdev, imgbuf->width-1-(10+16+10+16), 7, imgbuf->width-1-(10+16+10), 5+16-1); /* Max Icon */
        draw_rect(vfbdev, imgbuf->width-1-(10+16+10+16) +1, 7 +1, imgbuf->width-1-(10+16+10) -1, 5+16-1 -1); /* Max Icon */
        draw_line(vfbdev, imgbuf->width-1-(10+16), 15-1, imgbuf->width-1-10, 15-1);     /* Mix Icon */
        draw_line(vfbdev, imgbuf->width-1-(10+16), 15-1 -1, imgbuf->width-1-10, 15-1 -1);       /* Mix Icon */
#endif

        /* Put Title */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 		/* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)"X _ O    EGI Triangles",   /* fw,fh, pstr */
                                        320, 1, 0,                 	/* pixpl, lines, fgap */
                                        5, 5,                         	/* x0,y0, */
		                        WEGI_COLOR_WHITE, -1, 200,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* Loop */
	det=1;
	while(!sigQuit) {

                /* Get surface mutex_lock */
                if( pthread_mutex_lock(&surfshmem->mutex_lock) !=0 ) {
                          egi_dperr("Fail to get mutex_lock for surface.");
                          tm_delayms(10);
                          continue;
                }
/* ------ >>>  Surface shmem Critical Zone  */

		/* Draw a triangle inside the box */
	        egi_randp_inbox(&tri_box.startxy, &box);
        	tri_box.endxy.x=tri_box.startxy.x+50;
	        tri_box.endxy.y=tri_box.startxy.y+50;
        	for(i=0;i<3;i++) {
                	egi_randp_inbox(pts+i, &tri_box);
//			printf("pt: (%d,%d),(%d,%d),(%d,%d)\n", pts[0].x,pts[0].y, pts[1].x,pts[1].y,pts[2].x,pts[2].y );
		}

	        color=egi_color_random(color_all);
        	fbset_color2(vfbdev, color);
	        draw_filled_triangle(vfbdev, pts);

#if 0
        	gv_fb_dev.antialias_on=true;
	        draw_triangle(vfbdev, pts);
        	gv_fb_dev.antialias_on=false;
#endif


#if 1
        /* Put Title */
	draw_filled_rect2(vfbdev,WEGI_COLOR_DARKGRAY, 0, 0, imgbuf->width-1, 30);
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, 		/* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)"X _ O    EGI Triangles",   /* fw,fh, pstr */
                                        320, 1, 0,                 	/* pixpl, lines, fgap */
                                        5, 5,                         	/* x0,y0, */
		                        WEGI_COLOR_WHITE, -1, 200,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */

#endif

#if 0		/* Move surfaces */
		surfshmem->y0 += det;
		surfshmem->x0 += det*4/3;
		if( surfshmem->y0 > 240-80 ) det=-det;
		else if( surfshmem->y0 < -50 ) det=-det;
#endif

                /* Activate image */
                surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->mutex_lock);

		tm_delayms(50);
	}

	/* Unregister and destroy surfuser */
	egi_unregister_surfuser(&surfuser);

	exit(0);
}
