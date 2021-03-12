/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Usage:
1. Press 0-7 to bring indexed SURFACE to top.
   Keep pressing 0 to traverse all SURFACEs.
3. Press 'N' to minimize current top SURFACE.
4. Press 'M' to maximize current top SURFACE.
5. Press 'a','d','w','s' to move SURFACE LEFT,RIGHT,UP,DOWN respectively.

Note:
1. Before access to surfman->surfaces[x], make sure that there
   are surfaces registered in the surfman. If there is NO surface
   available, it will certainly cause segmentation fault!
2. If the SURFUSER do NOT receive ERING MSG, then it MAY cause some
   problem any msg piled up in the kernel space? --'Resource temporarily unavailable'.


TODO:
1. Surfman_render_thread DO NOT sync with mouse click_pick operation.
   Surfman render mouse position MAY NOT be current mouseXY, which
   appears as visual picking error.  Draw mark.
2. CPU schedule time for each thread MAY be heavely biased.


Journal
2021-03-11:
	1. If mouse clicks on leftside minibar menu, then bring the SURFACE
	   to TOP layer.

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "egi_timer.h"
#include "egi_color.h"
#include "egi_log.h"
#include "egi_debug.h"
#include "egi_math.h"
#include "egi_FTsymbol.h"
#include "egi_surface.h"
#include "egi_procman.h"
#include "egi_input.h"

#define MCURSOR_ICON_PATH 	"/mmc/mcursor.png"
#define SURFMAN_BKGIMG_PATH	"/mmc/egidesk.jpg"  // "/mmc/linux.jpg"

static EGI_MOUSE_STATUS mostat;
static EGI_MOUSE_STATUS *pmostat;
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus);
int 	lastX;
int 	lastY;
bool	surface_downhold;

EGI_SURFMAN *surfman;
int	zseq;
int	surfID;

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
//	int ret;
	int k;
	int ch;
	int surfID;

	int opt;
//	int sw=0,sh=0; /* Width and Height of the surface */
	int x0=0,y0=0; /* Origin coord of the surface */


#if 0	/* Start EGI log */
        if(egi_init_log("/mmc/test_surfman.log") != 0) {
                printf("Fail to init logger, quit.\n");
                exit(EXIT_FAILURE);
        }
        EGI_PLOG(LOGLV_INFO,"%s: Start logging...", argv[0]);
#endif

#if 1   /* Load freetype fonts */
        printf("FTsymbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0) {
                printf("Fail to load FT sysfonts, quit.\n");
                return -1;
        }
#endif

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hscS:p:"))!=-1 ) {
                switch(opt) {
                        case 'h':
				printf("%s: [-hsc]\n", argv[0]);
				break;
		}
	}

  	/* Start sys tick */
//  	tm_start_egitick();

	/* Create an ERING_MSG */
	printf("Create an ERING_MSG, sizeof(EGI_MOUSE_STATUS)=%d ....\n", sizeof(EGI_MOUSE_STATUS));
        ERING_MSG *emsg=ering_msg_init();
        if(emsg==NULL) exit(EXIT_FAILURE);

	/* 1. Create an EGI surface manager */
	surfman=egi_create_surfman(ERING_PATH_SURFMAN);
	if(surfman==NULL)
		exit(EXIT_FAILURE);
	else
		printf("Succeed to create surfman at '%s'!\n", ERING_PATH_SURFMAN);

	/* 1.A Load imgbuf for mouse cursor */
	surfman->mcursor=egi_imgbuf_readfile(MCURSOR_ICON_PATH);
	/* 1.B Load wallpaper */
	surfman->bkgimg=egi_imgbuf_readfile(SURFMAN_BKGIMG_PATH);

        /* 2. Set termI/O 设置终端为直接读取单个字符方式 */
        egi_set_termios();

	/* 3. Set mouse callback function and start mouse readloop */
	/* ! Refere pos_xres/yres for mouse movement limit. */
	gv_fb_dev.pos_xres=surfman->fbdev.pos_xres;
	gv_fb_dev.pos_yres=surfman->fbdev.pos_yres;
  	if( egi_start_mouseread(NULL, mouse_callback) <0 )
		exit(EXIT_FAILURE);

	/* 4. Do SURFMAN routine jobs while no command */
	k=SURFMAN_MAX_SURFACES-1;
	while( surfman->cmd==0 ) {

		/* TODO: Select/Epoll mouse/keyboard event, poll_input_event */

		/* W.1  Keyboard event */
		/* Switch the top surface */
                ch=0;
                read(STDIN_FILENO, &ch,1);
		if(ch >='0' && ch < '8' ) {  /* 1,2,3...9 */
			printf("surfman->scnt=%d \n", surfman->scnt);
			printf("ch: '%c'(%d)\n",ch, ch);
			k=ch-0x30;
			if( k < SURFMAN_MAX_SURFACES ) {
				 printf("Try to set surface %d top...\n", k);
				 surfman_bringtop_surface(surfman, k);
			}
		}
		else {
			switch(ch) {
				/* TEST--------: Move surface, surfman_mutex ignored. !!!! scnt>0 !!! */
				case 'a':
					surfman->surfaces[SURFMAN_MAX_SURFACES-1]->surfshmem->x0 -=10;
					break;
				case 'd':
					surfman->surfaces[SURFMAN_MAX_SURFACES-1]->surfshmem->x0 +=10;
					break;
				case 'w':
					surfman->surfaces[SURFMAN_MAX_SURFACES-1]->surfshmem->y0 -=10;
					break;
				case 's':
					surfman->surfaces[SURFMAN_MAX_SURFACES-1]->surfshmem->y0 +=10;
					break;

				/* TEST--------: Minimize / Maximize surfaces */
				case 'n': /* Minimize the surface */
					printf("Minimize surface.\n");
					pthread_mutex_lock(&surfman->surfman_mutex);

					/* If all surface minimized */
					if(surfman->scnt == surfman->mincnt) {
						pthread_mutex_unlock(&surfman->surfman_mutex);
						break;
					}

					/* Get index of current toplayer surface */
					surfID=surfman_get_TopDispSurfaceID(surfman);
					if(surfID<0) {
						pthread_mutex_unlock(&surfman->surfman_mutex);
						break;
					}

					/* Change status */
					surfman->surfaces[surfID]->status=SURFACE_STATUS_MINIMIZED;

					pthread_mutex_unlock(&surfman->surfman_mutex);
					break;

				case 'm': /* Maximize the surface */
					printf("Maximize surface.\n");
					pthread_mutex_lock(&surfman->surfman_mutex);

					if(surfman->minsurfaces[0]) {
						surfman->minsurfaces[0]->status=SURFACE_STATUS_NORMAL;
						/* Bring to TOP */
						surfman_bringtop_surface_nolock(surfman, surfman->minsurfaces[0]->id);
					}

					pthread_mutex_unlock(&surfman->surfman_mutex);
					break;

				default:
					break;
			}
		}

		/* W2. Mouse event */
	        if( egi_mouse_getRequest(pmostat) ) {  /* Apply pmostat->mutex_lock */

			/* 0. Send mouse_status to the TOP surface */

			pthread_mutex_lock(&surfman->surfman_mutex);
	/* ------ >>>  Surfman Critical Zone  */

			if(surfman->scnt) {  /* Only if have surface registered */
				/* Note: ering_msg_send() is BLOCKING type, if Surfuser DO NOT recv the msg, it will blocked here! */
				//printf("ering msg send...\n");
				if( ering_msg_send( surfman->surfaces[SURFMAN_MAX_SURFACES-1]->csFD,
						    emsg, ERING_MOUSE_STATUS, pmostat, sizeof(EGI_MOUSE_STATUS)) <=0 ) {
					egi_dpstd("Fail to sendmsg ERING_MOUSE_STATUS!\n");
				}
				//printf("ering msg send OK!\n");
			}

	/* ------ <<<  Surfman Critical Zone  */
			pthread_mutex_unlock(&surfman->surfman_mutex);


	                /* 1. LeftKeyDown: To bring the surface to top, to save current mouseXY */
        	        if(!surface_downhold && pmostat->LeftKeyDown) {

/*-----------------------------------------------------------------
...
Picked zseq=3
surfID=5
surfman_bringtop_surface_nolock(): Set surfaces[5] zseq to 5
Picked zseq=3	   	 <------- Leftkek_Down NOT cleared! and trigger again!! at that time, surfman_render_thread
surfID=5			    does NOT DO render, and therefore FBDEV.zbuff[] keeps OLD data!!!!
surfman_bringtop_surface_nolock(): Set surfaces[5] zseq to 5
-Leftkey DownHold!
Hold surface
-Leftkey Up!
...
...
surfman_bringtop_surface_nolock(): Set surfaces[3] zseq to 5
-Leftkey Up!
-Leftkey Down!		<-------- Miss  KeyDown event!!!
-Leftkey Up!

...

Hold surface
-Leftkey DownHold!
Hold surface
Hold surface
-Leftkey DownHold!
Hold surface
-Leftkey Up!		<----------Miss KeyUp event!!!
-Leftkey Down!
-Leftkey DownHold!
----------------------------------------------------------------------*/


				pthread_mutex_lock(&surfman->surfman_mutex);
		 /* ------ >>>  Surfman Critical Zone  */

                                surfman->mx  = pmostat->mouseX;
                                surfman->my  = pmostat->mouseY;

				/* A. Check if it clicks on any SURFACEs. */
				zseq=surfman_xyget_Zseq(surfman, surfman->mx, surfman->my);
				printf("Picked zseq=%d\n", zseq);

				surfID=surfman_xyget_surfaceID(surfman, surfman->mx, surfman->my );
				printf("surfID=%d\n",surfID);

				if(surfID>=0) { /* <0 as bkground */
					surface_downhold=true;

					/* Bring picked surface to TOP layer */
					surfman_bringtop_surface_nolock(surfman, surfID);
					/* Send msg to the surface */
					emsg->type=ERING_SURFACE_BRINGTOP;
				        if( unet_sendmsg( surfman->surfaces[SURFMAN_MAX_SURFACES-1]->csFD, emsg->msghead) <=0 ) {
					                egi_dpstd("Fail to sednmsg ERING_SURFACE_BRINGTOP!\n");
					}

				}
				/* B. Check if it clicks on leftside minibar menu, IndexMpMinSurf is set by surfman_render_thread(). */
				else if ( surfman->IndexMpMinSurf >= 0 ) {
					/* Bring mouse clicked minisurface to TOP layer */
					surfman_bringtop_surface_nolock(surfman, surfman->minsurfaces[surfman->IndexMpMinSurf]->id);
					/* Send msg to the surface */
					emsg->type=ERING_SURFACE_BRINGTOP;
				        if( unet_sendmsg( surfman->surfaces[SURFMAN_MAX_SURFACES-1]->csFD, emsg->msghead) <=0 ) {
					                egi_dpstd("Fail to sednmsg ERING_SURFACE_BRINGTOP!\n");
					}
				}

		 /* ------ <<<  Surfman Critical Zone  */
				pthread_mutex_unlock(&surfman->surfman_mutex);

				/* Rest lastX/Y, to compare with next mouseX/Y to get deviation. */
                	        lastX=pmostat->mouseX;
                        	lastY=pmostat->mouseY;

	                }
        	        /* 2. LeftKeyDownHold: To move surface */
                	else if(pmostat->LeftKeyDownHold) {
                        	/* Note: As mouseDX/DY already added into mouseX/Y, and mouseX/Y have Limit Value also.
	                         * If use mouseDX/DY, the cursor will slip away at four sides of LCD, as Limit Value applys for mouseX/Y,
        	                 * while mouseDX/DY do NOT has limits!!!
                	         * We need to retrive DX/DY from previous mouseX/Y.
                        	 */
				//printf("LeftKeyDH mutex lock..."); fflush(stdout);
				pthread_mutex_lock(&surfman->surfman_mutex);
				//printf("LeftKeyDH lock OK\n");
		/* ------ >>>  Surfman Critical Zone  */

  			   	if(surface_downhold && surfman->scnt ) {  /* Only if have surface registered! */
					printf("Hold surface\n");
					/* Downhold surface is always on the top layer */
					surfID=SURFMAN_MAX_SURFACES-1;

					/* Update surface x0,y0 */
	                	        surfman->surfaces[surfID]->surfshmem->x0  += (pmostat->mouseX-lastX); /* Delt x0 */
        	       	        	surfman->surfaces[surfID]->surfshmem->y0  += (pmostat->mouseY-lastY); /* Delt y0 */

				}/* End surface_downhold */
				   else {
					printf("Hold non\n");

			   	}
				/* Whatever, update mouse position always. */
                                //surfman->mx  += (pmostat->mouseX-lastX); /* Delt x0 */
                                //surfman->my  += (pmostat->mouseY-lastY); /* Delt y0 */

                                surfman->mx  = pmostat->mouseX;
                                surfman->my  = pmostat->mouseY;

				//printf("dx,dy: (%d,%d)\n", pmostat->mouseX-lastX, pmostat->mouseY-lastY);
				//printf("mx,my: (%d,%d)\n", surfman->mx, surfman->my);

	 	/* ------ <<<  Surfman Critical Zone  */
				pthread_mutex_unlock(&surfman->surfman_mutex);

                        	/* update lastX,Y */
	                        lastX=pmostat->mouseX; lastY=pmostat->mouseY;

				//usleep(5000);
			}
			/* 3. LeftKeyUp: to release current downhold surface */
			else if(pmostat->LeftKeyUp ) {
				if(surface_downhold) {
					printf("Unhold surface\n");
					surface_downhold=false;
				}

				pthread_mutex_lock(&surfman->surfman_mutex);
		 /* ------ >>>  Surfman Critical Zone  */

				surfman->mx = pmostat->mouseX;
				surfman->my = pmostat->mouseY;

		 /* ------ <<<  Surfman Critical Zone  */
				pthread_mutex_unlock(&surfman->surfman_mutex);
			}
			/* 4. Update mouseXY to surfman */
			else {

				pthread_mutex_lock(&surfman->surfman_mutex);
		 /* ------ >>>  Surfman Critical Zone  */

                                surfman->mx  = pmostat->mouseX;
                                surfman->my  = pmostat->mouseY;

		 /* ------ <<<  Surfman Critical Zone  */
				pthread_mutex_unlock(&surfman->surfman_mutex);
			}

			/* Put request */
			egi_mouse_putRequest(pmostat);
                }

		usleep(100000);
   	} /* End while() */

	/* Free SURFMAN */
	egi_destroy_surfman(&surfman);

        /* Reset termI/O */
        egi_reset_termios();

	exit(0);
}


/*------------------------------------------------------------------------------
		Callback for mouse input
Callback runs in MouseRead thread.
1. If lock_mousectrl is TRUE, then any operation to champ MUST be locked! just to
   avoid race condition with main thread.
2. Just update mouseXYZ
3. Check and set ACTIVE token for menus.
4. If click in TXTBOX, set typing cursor or marks.
5. The function will be pending until there is a mouse event.

--------------------------------------------------------------------------------*/
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus)
{

#if 0        /* Check mouse Left Key status */
	if( mostatus->LeftKeyDown ) {
		printf("LeftKeyDown!\n");
	}
#endif

	/* Wait until last mouse_event_request is cleared --- In loopread_mous thread, after mouse_callback */
//        while( egi_mouse_checkRequest(mostatus) && !mostatus->cmd_end_loopread_mouse ) { usleep(2000); };

        /* If request to quit mouse thread */
        if( mostatus->cmd_end_loopread_mouse )
                return;

        /* Get mostatus mutex lock */
        if( pthread_mutex_lock(&mostatus->mutex) !=0 ) {
                printf("%s: Fail to mutex lock mostatus.mutex!\n",__func__);
        }
 /*  --- >>>  Critical Zone  */

        /* 1. Pass out mouse data */
        mostat=*mostatus;
        pmostat=mostatus;		/* !!! NOT mutex_locked */

        /* 2. Request for respond */
        mostat.request = true;
        pmostat->request = true;  	/* !!! NOT mutex_locked */


/*  --- <<<  Critical Zone  */
        /* Put mutex lock */
        pthread_mutex_unlock(&mostatus->mutex);

	/* NOTE:
	 * It goes to mouse event select again,  and when mouse_loopread thread reads next mouse even BEFORE
	 * the main thread locks/pares  above passed mostatus, it will MISS it!
	 * Example: Leftkey_Up event is missed and next Leftkey_DownHold is parsed.
		...
		-Leftkey DownHold!
		Hold surface
		-Leftkey Up!    <--- Miss KeyUp event!!! (unhold surface)
		-Leftkey Down!
		-Leftkey DownHold!
		-Leftkey DownHold!
		Hold surface
		...
         */
}


