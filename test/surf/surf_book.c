/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of creating a surface for displaying text on it.

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
#include "egi_log.h"
#include "egi_debug.h"
#include "egi_math.h"
#include "egi_FTsymbol.h"
#include "egi_surface.h"
#include "egi_procman.h"
#include "egi_input.h"
#include "egi_utils.h"

#ifdef LETS_NOTE
	#define SURF_MAXW	800
	#define SURF_MAXH	600
#else
	#define SURF_MAXW	320
	#define SURF_MAXH	240
#endif


/* For SURFUSER */
EGI_SURFUSER     *surfuser=NULL;
EGI_SURFSHMEM    *surfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *vfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *surfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  colorType=SURF_RGB565;  /* surfuser->vfbdev color type */
EGI_16BIT_COLOR  bkgcolor;

/* Ering Routine */
void            *surfuser_ering_routine(void *surfuser);

/* To use SURFACE module default function */
//void 	surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat); /* shmem_mutex */
//void 	surfuser_resize_surface(EGI_SURFUSER *surfuser, int w, int h);
void my_redraw_surface(EGI_SURFUSER *surfuser, int w, int h);

/* For text */
const char *fpath="/mmc/xyj.txt";
EGI_FILEMMAP *fmap;
UFT8_PCHAR ptxt;
int fw=18;
int fh=18;
int fgap=5;

/* Signal handler for SurfUser */
void signal_handler(int signo)
{
	if(signo==SIGINT) {
		egi_dpstd("SIGINT to quit the process!\n");
	}
}

/*----------------------------
	   MAIN()
-----------------------------*/
int main(int argc, char **argv)
{
	int i;
	int opt;
	char *delim=",";
	char saveargs[128];
	char *ps=&saveargs[0];

	int sw=0,sh=0; /* Width and Height of the surface */
	int x0=0,y0=0; /* Origin coord of the surface */
	char *pname=NULL;

#if 0	/* Start EGI log */
        if(egi_init_log("/mmc/test_surfman.log") != 0) {
                printf("Fail to init logger, quit.\n");
                exit(EXIT_FAILURE);
        }
        EGI_PLOG(LOGLV_INFO,"%s: Start logging...", argv[0]);
#endif

#if 1        /* Load freetype fonts */
        printf("FTsymbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0) {
                printf("Fail to load FT sysfonts, quit.\n");
                return -1;
        }
#endif

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hS:p:n:"))!=-1 ) {
                switch(opt) {
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
			case 'n':
				pname=optarg;
                        case 'h':
				printf("%s: [-hsc]\n", argv[0]);
				break;
		}
	}

	/* Set signal handler */
//	egi_common_sigAction(SIGINT, signal_handler);

	/* Create fontbuffer */
	egi_fontbuffer=FTsymbol_create_fontBuffer(egi_sysfonts.regular, fw, fh,  0x3300, 0x33FF-0x3300);  /* face, fw, fh, unistart, size */

	/* 0. Fmap text file */
        fmap=egi_fmap_create(fpath, 0, PROT_READ, MAP_PRIVATE);
        if(fmap==NULL) {
        	printf("Fail to create fmap for '%s'!\n", fpath);
                        exit(EXIT_FAILURE);
                }
        printf("fmap with fisze=%lld for '%s' created!\n", fmap->fsize, fpath);
	/* Get ptxt */
	ptxt=(UFT8_PCHAR)fmap->fp;

	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	if( sw!=0 && sh!=0) {
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, SURF_MAXW,SURF_MAXH, sw, sh, colorType );
	}
	else
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, mat_random_range(SURF_MAXW-50)-50, mat_random_range(SURF_MAXH-50),
     	                                SURF_MAXW, SURF_MAXH,  140+mat_random_range(SURF_MAXW/2), 60+mat_random_range(SURF_MAXH/2), colorType );
	if(surfuser==NULL) {
		exit(EXIT_FAILURE);
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	vfbdev=&surfuser->vfbdev;
	surfimg=surfuser->imgbuf;
	surfshmem=surfuser->surfshmem;

        /* Get surface mutex_lock */
        if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 ) {
        	egi_dperr("Fail to get mutex_lock for surface.");
		exit(EXIT_FAILURE);
        }
/* ------ >>>  Surface shmem Critical Zone  */

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons. */
        surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	surfshmem->redraw_surface 	= my_redraw_surface; //surfuser_redraw_surface;
	surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        surfshmem->close_surface 	= surfuser_close_surface;

	/* 3. Give a name for the surface. */
	pname="西游记";
	if(pname)
		strncpy(surfshmem->surfname, pname, SURFNAME_MAX-1);
	else
		pname="EGI_SURF";

	/* 4. Draw surface */
	/* 4.1 Set BKG color */
	surfshmem->bkgcolor=WEGI_COLOR_DARKBLUE; //egi_color_random(color_all);
	egi_imgbuf_resetColorAlpha(surfimg, surfshmem->bkgcolor, -1); /* Reset color only */

	/* 4.2 Draw top bar */
	draw_filled_rect2(vfbdev,WEGI_COLOR_GRAY5, 0,0, surfimg->width-1, 30);

	/* 4.3 Draw CLOSE/MIN/MAX Btns */
	#if 0 /* Geom ICON */
	fbset_color2(vfbdev, WEGI_COLOR_WHITE);
	draw_wrect(vfbdev, surfimg->width-1-(10+16), 12, surfimg->width-1-10, 12+15-1, 2); /* Max Icon */
	draw_wline_nc(vfbdev, surfimg->width-1-2*(10+16), 12+15-1, surfimg->width-1-(10+16+10), 12+15-1, 2); /* Min Icon */

	draw_wline(vfbdev, surfimg->width-1-3*(10+16), 12, surfimg->width-1-(3*(10+16)-16), 12+15-1, 2); /* Close Icon */
	draw_wline(vfbdev, surfimg->width-1-3*(10+16), 12+15-1, surfimg->width-1-(3*(10+16)-16), 12, 2); /* Close Icon */
	#else /* FTsymbol ICON */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular,       /* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)"X _ O",   /* fw,fh, pstr */
                                        320, 1, 0,                      /* pixpl, lines, fgap */
                                        surfimg->width-20*3, 5,         /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
	#endif

        /* 4.4 Put surface name/title. */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular, /* FBdev, fontface */
                                        18, 18,(const UFT8_PCHAR)pname,   /* fw,fh, pstr */
                                        320, 1, 0,                        /* pixpl, lines, fgap */
                                        5, 5,                         	  /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 200,    	/* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /* int *cnt, int *lnleft, int* penx, int* peny */

	/* 4.5 Draw outline rim */
	fbset_color2(vfbdev, WEGI_COLOR_GRAY); //BLACK);
	draw_rect(vfbdev, 0,0, surfshmem->vw-1, surfshmem->vh-1);


	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* 5. Create SURFBTNs by blockcopy SURFACE image, after drawing the surface! */
	int xs=surfimg->width-65;
	surfshmem->sbtns[SURFBTN_CLOSE]=egi_surfbtn_create(surfimg,     xs+2,0+1,      xs+2,0+1,      18, 30-1); /* (imgbuf, xi, yi, x0, y0, w, h) */
	surfshmem->sbtns[SURFBTN_MINIMIZE]=egi_surfbtn_create(surfimg,  xs+2+18,0+1,   xs+2+18,0+1,   18, 30-1);
	surfshmem->sbtns[SURFBTN_MAXIMIZE]=egi_surfbtn_create(surfimg,  xs+5+18*2,0+1, xs+5+18*2,0+1, 18, 30-1);

	/* 6. Write text */

        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular,   /* FBdev, fontface */
                                        fw, fh, ptxt,   		/* fw,fh, pstr */
                                        surfimg->width-10, (surfimg->height-SURF_TOPBAR_WIDTH-10)/(fh+fgap), fgap,  /* pixpl, lines, fgap */
                                        5, SURF_TOPBAR_WIDTH+5,         /* x0,y0, */
                                        WEGI_COLOR_ORANGE, -1, 255,  /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */

	/* 7. Start Ering routine */
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

	/* 7. Activate image */
	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);

	/* Main loop */
	while( surfshmem->usersig != 1 ) {

		sleep(5);

	}

        /* Free SURFBTNs */
        for(i=0; i<3; i++)
                egi_surfbtn_free(&surfshmem->sbtns[i]);

	/* Unregister and destroy surfuser */
	if( egi_unregister_surfuser(&surfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	/* Free fmap */
	egi_fmap_free(&fmap);

	/* Free fontBuffer */
	FTsymbol_free_fontBuffer(&egi_fontbuffer);

	exit(0);
}


/*------------------------------------
    SURFUSER's ERING routine thread.
------------------------------------*/
void *surfuser_ering_routine(void *args)
{
	EGI_SURFUSER *surfuser=NULL;
	ERING_MSG *emsg=NULL;
	EGI_MOUSE_STATUS *mouse_status; /* A ref pointer */
	int nrecv;

	/* Get surfuser pointer */
	surfuser=(EGI_SURFUSER *)args;
	if(surfuser==NULL)
		return (void *)-1;

	/* Init ERING_MSG */
	emsg=ering_msg_init();
	if(emsg==NULL)
		return (void *)-1;

	while( surfuser->surfshmem->usersig !=1  ) {
		/* 1. Waiting for msg from SURFMAN, BLOCKED here if NOT the top layer surface! */
//	        egi_dpstd("Waiting in recvmsg...\n");
		nrecv=ering_msg_recv(surfuser->uclit->sockfd, emsg);
		if(nrecv<0) {
                	egi_dpstd("unet_recvmsg() fails!\n");
			continue;
        	}
		/* SURFMAN disconnects */
		else if(nrecv==0) {
			//return (void *)-1;
			exit(EXIT_FAILURE);
		}

	        /* 2. Parse ering messag */
        	switch(emsg->type) {
			/* NOTE: Msg BRINGTOP and MOUSE_STATUS sent by TWO threads, and they MAY reaches NOT in right order! */
	               case ERING_SURFACE_BRINGTOP:
                        	egi_dpstd("Surface is brought to top!\n");
                	        break;
        	       case ERING_SURFACE_RETIRETOP:
                	        egi_dpstd("Surface is retired from top!\n");
                        	break;
		       case ERING_MOUSE_STATUS:
				mouse_status=(EGI_MOUSE_STATUS *)emsg->data;
//				egi_dpstd("MS(X,Y):%d,%d\n", mouse_status->mouseX, mouse_status->mouseY);
				/* Parse mouse event */
				surfuser_parse_mouse_event(surfuser,mouse_status);
				break;
	               default:
        	                egi_dpstd("Undefined MSG from the SURFMAN! data[0]=%d\n", emsg->data[0]);
        	}

	}

	/* Free EMSG */
	ering_msg_free(&emsg);

	return (void *)0;
}


/*---------------------------------------------------------------
Adjust surface size, redraw surface imgbuf.

@surfuser:      Pointer to EGI_SURFUSER.
@w,h:           New size of surface imgbuf.
---------------------------------------------------------------*/
void my_redraw_surface(EGI_SURFUSER *surfuser, int w, int h)
{
	/* Redraw default function first */
	surfuser_redraw_surface(surfuser, w, h); /* imgbuf w,h updated */

	/* Rewrite txt */
        FTsymbol_uft8strings_writeFB(   vfbdev, egi_sysfonts.regular,   /* FBdev, fontface */
                                        fw, fh, ptxt,   		/* fw,fh, pstr */
                                        w-10, (h-SURF_TOPBAR_WIDTH-10)/(fh+fgap), fgap,  /* pixpl, lines, fgap */
                                        5, SURF_TOPBAR_WIDTH+5,         /* x0,y0, */
                                        WEGI_COLOR_ORANGE, -1, 255,  /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
}
