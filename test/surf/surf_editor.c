/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Journal:
2021-06-29:
	1. Start....

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>

#include "egi_timer.h"
#include "egi_color.h"
#include "egi_log.h"
#include "egi_debug.h"
#include "egi_math.h"
#include "egi_FTsymbol.h"
#include "egi_surface.h"
#include "egi_procman.h"
#include "egi_input.h"
#include "egi_FTcharmap.h"

//const char *pid_lock_file="/var/run/surfman_guider.pid";

/* For SURFUSER */
EGI_SURFUSER     *surfuser=NULL;
EGI_SURFSHMEM    *surfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *vfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *surfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  colorType=SURF_RGB565;  /* surfuser->vfbdev color type */
EGI_16BIT_COLOR  bkgcolor;
//bool		 ering_ON;		/* Surface on TOP, ERING is streaming input data. */
					/* XXX A LeftKeyDown as signal to start parse of mostat stream from ERING,
					 * Before that, all mostat received will be ignored. This is to get rid of
					 * LeftKeyDownHold following LeftKeyDown which is to pick the top surface.
					 */
/* For text buffer */
char ptxt[1024];	/* Text */
int poff;		/* Offset to ptxt */
EGI_16BIT_COLOR	fcolor=WEGI_COLOR_BLACK;

/* For CHARMAP */
#define         CHMAP_TXTBUFF_SIZE      1024 /* Initial text buffer size for EGI_FTCHAR_MAP.txtbuff */
#define         CHMAP_SIZE              2048 /* Initial value, MAX chars that may displayed in the txtbox */

static EGI_FTCHAR_MAP *chmap;   /* FTchar map to hold char position data */
static unsigned int chns;       /* total number of chars in a string */
static int fw=18;               /* Font size */
static int fh=20;
static int lndis;		/* Line distance */
static int fgap=2; //20/4;      /* Text line fgap : TRICKY ^_- */
static int tlns;		/* Total available line space for displaying txt */
static int smargin=5;           /* left and right side margin of text area */
static int tmargin=2;           /* top margin of text area */

/* SURF Menus */
const char *menu_names[] = { "File", "Option", "Help"};
enum {
        MENU_FILE       =0,
        MENU_OPTION     =1,
        MENU_HELP       =2,
        MENU_MAX        =3, /* <--- Limit */
};
//ESURF_BOX       *menus[MENU_MAX];
int menuW;      /* variable */
int menuH=24;

bool mouseOnMenu;
int mpmenu=-1;  /* Index of mouse touched menu, <0 invalid */

void menu_file();
void menu_option(EGI_SURFSHMEM *surfcaller, int x0, int y0);
void menu_help(EGI_SURFSHMEM *surfcaller, int x0, int y0);


/* Apply SURFACE module default function */
void my_redraw_surface(EGI_SURFUSER *surfuser, int w, int h);
//void  *surfuser_ering_routine(void *args);
//void  surfuser_parse_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat); /* shmem_mutex */
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

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
	int sw=0,sh=0; /* Width and Height of the surface */
	int x0=0,y0=0; /* Origin coord of the surface */


	/* <<<<<  EGI general initialization   >>>>>> */

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

	/* Set signal handler */
//	egi_common_sigAction(SIGINT, signal_handler);


        /* Total available lines of space for displaying chars */
	sw=200; sh=180;
        lndis=fh+fgap;
	tlns=(sh-SURF_TOPBAR_HEIGHT-4)/lndis;

        /* Create FTcharmap */
        chmap=FTcharmap_create( CHMAP_TXTBUFF_SIZE, 2, SURF_TOPBAR_HEIGHT+2,         /* txtsize,  x0, y0  */
                        sh-SURF_TOPBAR_HEIGHT-4, sw-4, smargin, tmargin,      /*  height, width, offx, offy */
                        egi_sysfonts.regular, CHMAP_SIZE, tlns, sw-4-2*smargin, lndis,   /*  typeface, mapsize, lines, pixpl, lndis */
                        WEGI_COLOR_GRAY, WEGI_COLOR_BLACK, true, false );      /*  bkgcolor, fontcolor, charColorMap_ON, hlmarkColorMap_ON */
        if(chmap==NULL) {
		egi_dperr("Fail to create Charmap!");
		exit(EXIT_FAILURE);
	}


REGISTER_SURFUSER:
	/* 1. Register/Create a surfuser */
	egi_dpstd("Register to create a surfuser...\n");
	if( sw!=0 && sh!=0) {
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, NULL, x0, y0, SURF_MAXW,SURF_MAXH, sw, sh, colorType );
	}
	else
		surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, NULL, mat_random_range(SURF_MAXW-50)-50, mat_random_range(SURF_MAXH-50),
     	                                SURF_MAXW, SURF_MAXH,  140+mat_random_range(SURF_MAXW/2), 60+mat_random_range(SURF_MAXH/2), colorType );
	if(surfuser==NULL) {
                int static ntry=0;
                ntry++;
                if(ntry==3)
                        exit(EXIT_FAILURE);

                usleep(100000);
                goto REGISTER_SURFUSER;
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

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons etc. */
	// Defualt: surfuser_ering_routine() calls surfuser_parse_mouse_event();
        surfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	surfshmem->redraw_surface 	= my_redraw_surface;
	surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        surfshmem->close_surface 	= surfuser_close_surface;
        surfshmem->user_mouse_event     = my_mouse_event;
        //surfshmem->draw_canvas          = my_draw_canvas;

	/* 4. Give a name for the surface. */
	strncpy(surfshmem->surfname, "记事本", SURFNAME_MAX-1);

	/* 5. First draw surface, load icons, create controls...etc. */
	surfshmem->bkgcolor=WEGI_COLOR_GRAYB; //egi_color_random(color_all); /* OR default BLACK */
	surfshmem->topmenu_bkgcolor=WEGI_COLOR_GRAY2;
	surfshmem->topmenu_hltbkgcolor=WEGI_COLOR_LTYELLOW; //WEGI_COLOR_LTSKIN;
	surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MAX|TOPBTN_MIN, MENU_MAX, menu_names); /* Default firstdraw operation */

	/* 5.1 Set font color */
	fcolor=COLOR_COMPLEMENT_16BITS(surfshmem->bkgcolor);

 	/* XXX 5.2 Draw/Create top menus[] ----> OK, integrated into surfuser_firstdraw_surface()! */

	/* Test EGI_SURFACE */
	egi_dpstd("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	egi_dpstd("Shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);


	/* 6. Start Ering routine */
	egi_dpstd("start ering routine...\n");
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		egi_dperr("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

	/* 7. Activate image */
	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
        pthread_mutex_unlock(&surfshmem->shmem_mutex);


	/* Main loop */
	while( surfshmem->usersig != 1 ) {
		tm_delayms(100);

	}

        /* Join ering_routine  */
	// surfuser)->surfshmem->usersig =1;  // Useless if thread is busy calling a BLOCKING function.
        /* To force eringRoutine to quit , for sockfd MAY be blocked at ering_msg_recv()! */
        tm_delayms(200); /* Let eringRoutine to try to exit by itself, at signal surfshmem->usersig =1 */
        if( surfshmem->eringRoutine_running ) {
                if( pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) !=0)
                        egi_dperr("Fail to pthread_setcancelstate eringRoutine, it MAY already quit!");
                if( pthread_cancel( surfshmem->thread_eringRoutine ) !=0)
                        egi_dperr( "Fail to pthread_cancel eringRoutine, it MAY already quit!");
                else
                        egi_dpstd("OK to cancel eringRoutine thread!\n");
        }

        /* Make sure mutex unlocked in pthread if any! */
	egi_dpstd("Joint thread_eringRoutine...\n");
        if( pthread_join(surfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

	/* Unregister and destroy surfuser */
	egi_dpstd("Unregister surfuser...\n");
	if( egi_unregister_surfuser(&surfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

	/* Free charmap */
        FTcharmap_free(&chmap);

	/* <<<<<  EGI general release   >>>>>> */
	// Ignore;

	egi_dpstd("Exit OK!\n");
	exit(0);
}


/*--------------------------------------------------------------------
                Mouse Event Callback
                (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event() .
4. Some vars of pmostat->conkeys will NOT auto. reset!
--------------------------------------------------------------------*/
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	int i;
	int k;
	ESURF_LABEL     **menus;
	EGI_SURFSHMEM    *surfshmem=surfuser->surfshmem;

	menus=&(surfuser->surfshmem->menus[0]);

	/* 1. Get key_input */
	if( pmostat->nch ) {
		egi_dpstd("chars: %-32.32s, nch=%d\n", pmostat->chars, pmostat->nch);

	   /* Write each char in pmostat->chars[] into ptxt[] */
	   for(k=0; k< pmostat->nch; k++) {
		/* Write ch into text buffer */
		if( poff < 1024-1 ) {
			if( poff)
				poff--;  /* To earse last '|' */

			if( poff>0 && pmostat->chars[k] == 127  ) { /* Backsapce */
				poff -=1; /*  Ok, NOW poff -= 2 */
				ptxt[poff+1]='\0';
			}
			else
				ptxt[poff++]=pmostat->chars[k];

			/* Pust '|' at last */
			ptxt[poff++]='|';
		}
	   }

		/* Redraw surface to update text.
		 * TODO: Here topbar BTNS will also be redrawn, and effect of mouse on top TBN will also be cleared! */
		/* Note: Call FTsymbol_uft8strings_writeFB() is NOT enough, need to clear surface bkg first. */
		my_redraw_surface(surfuser, surfimg->width, surfimg->height);
	}

        /* 2. Check if mouse hovers over any menu */
	for(i=0; i<MENU_MAX; i++) {
        	/* A. Check mouse_on_menu */
                mouseOnMenu=egi_point_on_surfBox( (ESURF_BOX *)menus[i], pmostat->mouseX -surfuser->surfshmem->x0,
                			                    pmostat->mouseY -surfuser->surfshmem->y0 );

		/* B. If the mouse just moves onto a menu */
                if(  mouseOnMenu && surfshmem->mpmenu != i ) {
                	egi_dpstd("Touch a MENU mpmenu=%d, i=%d\n", surfshmem->mpmenu, i);
                        /* B.1 In case mouse move from a nearby menu, restore its image. */
                        if( surfshmem->mpmenu>=0 ) {
                        	egi_subimg_writeFB(menus[surfshmem->mpmenu]->imgbuf, vfbdev,
                                			0, -1, menus[surfshmem->mpmenu]->x0, menus[surfshmem->mpmenu]->y0);
                        }
                        /* B.2 Put effect on the newly touched SURFBTN */
                        #if 0 /* Mask */
                        draw_blend_filled_rect(vfbdev, menus[i]->x0, menus[i]->y0,
                        			menus[i]->x0+menus[i]->imgbuf->width-1,menus[i]->y0+menus[i]->imgbuf->height-1,
                                                WEGI_COLOR_WHITE, 100);
                        #else /* imgbuf_effect */
                        egi_subimg_writeFB(menus[i]->imgbuf_effect, vfbdev,
                                                       0, -1, menus[i]->x0, menus[i]->y0);
                        #endif

                        /* B.3 Update mpmenu */
                        surfshmem->mpmenu=i;

                        /* B.4 Break for() */
                        break;
		}
                /* C. If the mouse leaves a menu: Clear mpmenu */
		else if( !mouseOnMenu && surfshmem->mpmenu == i ) {
			/* C.1 Draw/Restor original image */
                        egi_subimg_writeFB(menus[surfshmem->mpmenu]->imgbuf, vfbdev, 0, -1,
						menus[surfshmem->mpmenu]->x0, menus[surfshmem->mpmenu]->y0);

                        /* C.2 Reset pressed and Clear mpbtn */
                        //menus[mpmenu]->pressed=false;
                        surfshmem->mpmenu=-1;

                        /* C.3 Break for() */
                        break;
		}
                /* D. Still on the menu, sustain... */
		else if( mouseOnMenu && surfshmem->mpmenu == i ) {
                	break;
                }
	} /* EDN for(menus) */

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
                                        fw, fh, (const UFT8_PCHAR)ptxt, /* fw,fh, pstr */
                                        w-10, (h-SURF_TOPBAR_HEIGHT-10)/(fh+fgap) +1, fgap,  /* pixpl, lines, fgap */
                                        5, SURF_TOPBAR_HEIGHT+SURF_TOPMENU_HEIGHT+5,        /* x0,y0, */
                                        fcolor, -1, 255,  		/* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
}


