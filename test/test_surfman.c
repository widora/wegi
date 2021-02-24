/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


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


#define ERING_PATH "/tmp/surfman_test"


int main(int argc, char **argv)
{
	bool SetAsServer=true;
	int opt;

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hsc"))!=-1 ) {
                switch(opt) {
                        case 's':
				SetAsServer=true;
                                break;
                        case 'c':
                                SetAsServer=false;
                                break;
                        case 'h':
				printf("%s: [-hsc]\n", argv[0]);
				break;
		}
	}



 /* ---------- As EGI_SURFMAN ( The Server ) --------- */

 if( SetAsServer ) {
	EGI_SURFMAN *surfman;


    while(1) { /////////// LOOP TEST ///////////////

	/* Create an EGI surface manager */
	surfman=egi_create_surfman(ERING_PATH);
	if(surfman==NULL) {
		//exit(EXIT_FAILURE);
		printf("Try again...\n");
		usleep(50000);
		continue;
	}
	else
		printf("Succeed to create surfman at '%s'!\n", ERING_PATH);

	/* Do SURFMAN routine jobs while no command */
	while( surfman->cmd==0 ) {
		usleep(10000);
	}

	/* Free SURFMAN */
	egi_destroy_surfman(&surfman);

	//exit(0);

   } /* End while() */

	exit(0);
 }

 /* ---------- As EGI_SURFACE Application ( The client ) --------- */
 else {
	/*** NOTE:
	 *	1.  The second eface MAY be registered just BEFORE the first eface is unregistered.
	 */
	EGI_SURFUSER	*surfuser=NULL;

	FBDEV 		*vfbdev=NULL;	/* Only a ref. to surfman->vfbdev  */
	EGI_IMGBUF 	*imgbuf=NULL;	/* Only a ref. to surfman->imgbuf */
	EGI_SURFACE	*eface=NULL;	/* Only a ref. to surfman->surface */

        /* Load freetype fonts */
        printf("FTsymbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0) {
                printf("Fail to load FT sysfonts, quit.\n");
                return -1;
        }


    while(1) { /////////// LOOP TEST ///////////////

	/* Create a surfuser */
	surfuser=egi_register_surfuser(ERING_PATH, mat_random_range(320/2), mat_random_range(240/2),
                                           80+mat_random_range(320/2), 60+mat_random_range(240/2), SURF_RGB565 );
	if(surfuser==NULL) {
		usleep(100000);
		continue;
	}

	/* Get ref imgbuf and vfbdev */
	vfbdev=&surfuser->vfbdev;
	imgbuf=surfuser->imgbuf;
	eface=surfuser->surface;

	/* Set color */
	egi_imgbuf_resetColorAlpha(imgbuf, egi_color_random(color_all), -1); /* Reset color only */

	/* Draw a circle */
	draw_filled_rect2(vfbdev,WEGI_COLOR_DARKGRAY, 0,0, imgbuf->width-1, 30);
	draw_circle2(vfbdev, imgbuf->width/2, 60, 20, WEGI_COLOR_WHITE);

        /* Put title. */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, /* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)"EGI Surface",   /* fw,fh, pstr */
                                        320, 1, 0,                        /* pixpl, lines, fgap */
                                        5, 5,                         /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Memsize: %uBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			eface->memsize, eface->width, eface->height, surf_get_pixsize(eface->colorType), eface->x0, eface->y0);

	/* hold */
	sleep(1);
	//usleep(10000);

	/* Unregister and destroy surfuser */
	egi_unregister_surfuser(&surfuser);

	/* Hold on */
	usleep(10000);

   } /* End while() */

	exit(0);
 }

}
