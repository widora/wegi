/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Draw random triangles on a surface.

Journal:
2021-05-06:
	1. Test message queue IPC.

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

/* For SURFUSER */
EGI_SURFUSER     *surfuser=NULL;
EGI_SURFSHMEM    *surfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *vfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *surfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  colorType=SURF_RGB565_A8;  /* surfuser->vfbdev color type */
EGI_16BIT_COLOR  bkgcolor;
EGI_IMGBUF      *surftexture=NULL;
int              *pUserSig;                   /* Pointer to surfshmem->usersig, ==1 for quit. */

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


	/* 1. Register/Create a surfuser */
#if 1
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, 320,240, 240, 150, colorType);
#else
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, x0, y0, 400, 300, 400,300, colorType);
#endif
	if(surfuser==NULL)
		exit(EXIT_FAILURE);

        /* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
        vfbdev=&surfuser->vfbdev;
        surfimg=surfuser->imgbuf;
        surfshmem=surfuser->surfshmem;

        /* 3. Assign OP functions, connect with CLOSE/MIN./MAX. buttons etc. */
	// Defualt: surfuser_ering_routine() calls surfuser_parse_mouse_event();
        surfshmem->minimize_surface     = surfuser_minimize_surface;    /* Surface module default functions */
        //surfshmem->redraw_surface     = surfuser_redraw_surface;
        //surfshmem->maximize_surface   = surfuser_maximize_surface;    /* Need resize */
        //surfshmem->normalize_surface  = surfuser_normalize_surface;   /* Need resize */
        surfshmem->close_surface        = surfuser_close_surface;
        //surfshmem->user_mouse_event     = my_mouse_event;
        //surfshmem->draw_canvas          = my_draw_canvas;

        pUserSig = &surfshmem->usersig;

        /* 4. Name for the surface. */
        char *pname="三角形";
        strncpy(surfshmem->surfname, pname, SURFNAME_MAX-1);

        /* 5. First draw surface. */
        //surfshmem->bkgcolor=WEGI_COLOR_GRAY3; /* OR default BLACK */
        surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MIN); /* Default firstdraw operation */
        /* 5A. Set Alpha Frame and Draw outline rim */

	/* 6. Draw/Create buttons/menus */

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

        /* 7. Start Ering routine: Use module default routine function. */
        printf("start ering routine...\n");
        if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
                egi_dperr("Fail to launch thread_EringRoutine!");
                exit(EXIT_FAILURE);
        }

	/* Assign display box */
	box.startxy.x=0+1;
	box.startxy.y=SURF_TOPBAR_HEIGHT;
	box.endxy.x=surfshmem->vw-1 -1;
	box.endxy.y=surfshmem->vh-1 -1;

/* TEST: --------- Get surfman message queue ------ */
        int msgid=-1;
	int msgerr;
	int msgsize;
	SURFMSG_DATA msg_data={0};

        key_t  mqkey=ftok(ERING_PATH_SURFMAN, SURFMAN_MSGKEY_PROJID);
	if(mqkey<0)
		egi_dperr("Fail ftok()");


/* TEST: --- END --- */

	/* Loop */
	while ( surfshmem->usersig != 1 ) {

		/* Random 3points inside the box */
	        egi_randp_inbox(&tri_box.startxy, &box);

        	tri_box.endxy.x=tri_box.startxy.x+50;
		if( tri_box.endxy.x > surfshmem->vw-1 -1)
			tri_box.endxy.x = surfshmem->vw-1 -1;

	        tri_box.endxy.y=tri_box.startxy.y+50;
		if( tri_box.endxy.y > surfshmem->vh-1 -1)
			tri_box.endxy.y = surfshmem->vh-1 -1;

        	for(i=0;i<3;i++) {
                	egi_randp_inbox(pts+i, &tri_box);
//			printf("pt: (%d,%d),(%d,%d),(%d,%d)\n", pts[0].x,pts[0].y, pts[1].x,pts[1].y,pts[2].x,pts[2].y );
		}

                /* Get surface mutex_lock */
                if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 ) {
                          egi_dperr("Fail to get mutex_lock for surface.");
                          continue;
                }
/* ------ >>>  Surface shmem Critical Zone  */

		/* Draw a triangle inside the box */
	        color=egi_color_random(color_all);
        	fbset_color2(vfbdev, color);
	        draw_filled_triangle(vfbdev, pts);

#if 0
        	gv_fb_dev.antialias_on=true;
	        draw_triangle(vfbdev, pts);
        	gv_fb_dev.antialias_on=false;
#endif

                /* Activate image */
                surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);

		tm_delayms(1000);


/* TEST: --------- Get surfman message queue and msgsnd  ------ */
	if(msgid<0)
		msgid = msgget(mqkey, 0); //IPC_CREAT);
        if(msgid<0) {
                egi_dperr("Fail msgget()!");
	}
	else {
		egi_dpstd("Msgget msgid=%d\n",msgid);

		/* Prepare msg data */
		msg_data.msg_type = SURFMSG_REQUEST_DISPINFO;
		strncpy(msg_data.msg_text, "Hello!", SURFMSG_TEXT_MAX-1);
		msg_data.msg_text[SURFMSG_TEXT_MAX-1]='\0';
		msgsize=strlen(msg_data.msg_text)+1;

		/* Note: msgsnd() is never automatically restarted after being interrupted by a signal handler */
		msgerr=msgsnd(msgid, (void *)&msg_data, msgsize, 0); /* IPC_NOWAIT */
		if(msgerr) {
                	egi_dperr("Fail msgsnd()");
		}

#if 0	 /* -----TEST:  Remove the message queue, awakening all waiting reader and writer processes
        	 * (with an error return and  errno set to EIDRM).
		 * ------ !!!WARING!!! SURFMAN is the Owner of msgid! -----
         	 */
	        if( msgctl(msgid, IPC_RMID, 0) !=0 )
        	        egi_dperr("Fail to remove msgid!");
		else {
			egi_dpstd("Succeed to remvoe msgid!");
			msgid=-1;
		}
#endif


	}

/* TEST: --- END --- */

	} /* END while() */

        /* Pos_1: Join ering_routine  */
        // surfuser)->surfshmem->usersig =1;  // Useless if thread is busy calling a BLOCKING function.
        egi_dpstd("Cancel thread...\n");
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        /* Make sure mutex unlocked in pthread if any! */
        egi_dpstd("Joint thread_eringRoutine...\n");
        if( pthread_join(surfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

        /* Pos_2: Unregister and destroy surfuser */
        egi_dpstd("Unregister surfuser...\n");
        if( egi_unregister_surfuser(&surfuser)!=0 )
                egi_dpstd("Fail to unregister surfuser!\n");

        egi_dpstd("Exit OK!\n");
        exit(0);
}
