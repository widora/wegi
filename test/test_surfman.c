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


#if 1   //////////////    TEST: ering request     /////////////////

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

	EGI_UCLIT *uclit;
	EGI_IMGBUF *imgbuf;


    while(1) { /////////// LOOP TEST ///////////////

	/* Link to ering */
	uclit=unet_create_Uclient(ERING_PATH);
	if(uclit==NULL) {
		sleep(1);
		//exit(EXIT_FAILURE);
		continue;
	}
	else
		printf("Succeed to create uclit to '%s'!\n", ERING_PATH);

	/* ering request surface */
	EGI_SURFACE *eface=NULL;
	eface=ering_request_surface( uclit->sockfd, mat_random_range(320/2), mat_random_range(240/2),
					   80+mat_random_range(320/2), 60+mat_random_range(240/2), SURF_RGB565); //_A8);
	if(eface==NULL) {
		//exit(EXIT_FAILURE);
		/* Hold on for a while for surfman to ... */
		sleep(1);

		unet_destroy_Uclient(&uclit);
		continue;
	}

	/* Allocate an EGI_IMGBUF */
	imgbuf=egi_imgbuf_alloc();
	if(imgbuf==NULL)
		exit(EXIT_FAILURE);

	/* Map surface data to imgbuf */
	imgbuf->width = eface->width;
	imgbuf->height = eface->height;
	imgbuf->imgbuf = (EGI_16BIT_COLOR *)eface->color; /* SURF_RGB565 ! */
        if(eface->off_alpha >0 )
        	imgbuf->alpha = (EGI_8BIT_ALPHA *)(eface+eface->off_alpha);
        else
                imgbuf->alpha = NULL;

	/* Set color */
	egi_imgbuf_resetColorAlpha(imgbuf, egi_color_random(color_all), -1); /* Reset color only */

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Memsize: %uBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			eface->memsize, eface->width, eface->height, surf_get_pixsize(eface->colorType), eface->x0, eface->y0);

	/* hold */
	sleep(1);
	//usleep(10000);

	/* Unlink to surface data */
	imgbuf->imgbuf=NULL;
	imgbuf->alpha=NULL;
	/* Free imgbuf */
	egi_imgbuf_free2(&imgbuf);

	/* Unmap surface */
	egi_munmap_surface(&eface);

	/* Free uclit */
	unet_destroy_Uclient(&uclit);

	/* Hold on */
	usleep(10000);

   } /* End while() */

	exit(0);
 }

#endif


}
