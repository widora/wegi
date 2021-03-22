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
1. SURFMAN routine:
   1.1 Parse keyboard input:  Shortcut key functions.
   1.2 Parse Mouse event:     LeftKeyDown to pick TOP surface.
   1.2 Ering keystat and mostat to the TOP surface.
2. Before access to surfman->surfaces[x], make sure that there
   are surfaces registered in the surfman. If there is NO surface
   available, it will certainly cause segmentation fault!
3. If any SURFUSER do NOT receive ERING MSG, then it MAY cause some
   problem any msg piled up in the kernel space? --'Resource temporarily unavailable'.
4. The SURFMAN erings mostat/keystat only to the top surface, so if a surface
   has NO chance to get to TOP layer, then it will always be blocked at
   ering_msg_recv() in surfuser_ering_routine()!
5. The SURFMAN will ering msg of ERING_SURFACE_BRINGTOP to the surface when:
   5.1 If A surface is brought to TOP after resorting zseq, Usually after a surfuser/surface quits,
       all surfman->surfaces[] then be resorted.
   5.2  A surface is mouse LeftClick picked to be the top surface.
   5.3  A surface in minibar menue is clicked and brought to be top surface.
   5.4  XXX SURFUSER's behavior:
	If a surface is clicked on SURFBTN_MIN and its status is changed to be SURFACE_STATUS_MINIMIZED
        by the SURFUSER itself. SURFMAN's render_thread will then categorize it to minibar menu.
	In such case,

TODO:
1. Surfman_render_thread DO NOT sync with mouse click_pick operation.
   Surfman render mouse position MAY NOT be current mouseXY, which
   appears as visual picking error.  Draw mark.
   Because of CPU speed, and asynchronization between processes.
2. CPU schedule time for each thread MAY be heavely biased.   --- TEST&TBD
3. Test modified MouseDX/DY.
4. Var. surface_downhold no more use! as surface movement is coded in test_surfuser.c.
XXX 5. When a surface is brought to TOP by mostat(click), DO NOT ering the same mostat to the surface!
   For it will cause unwanted effect.
   Example: Only by one click, a surface appears, and at the same time a button of the surface is activated.
XXX 6. XXX When a surface is brought to TOP, there may be NO ering msg to the surface!  Necessary???
   Example: Two surface on dest top, and after one quits, the other will be re-sorted to TOP surface on display.
   --OK, compare surface->userID to find if the TOP surface changes.X
XXX 7. If a surface is clicked on SURFBTN_MIN and its status is changed to be SURFACE_STATUS_MINIMIZED by the SURFUSER itself.
   there'll be NO ering msg to the surface!  Necessary???

   NOTE: 5.6.7 --- SURFACE_STATUS_MINIMIZED to be sent by surfman_render_thread()!
8.


Journal
2021-03-11:
	1. If mouse clicks on leftside minibar menu, then bring the SURFACE
	   to TOP layer.

2021-03-16:
	1. Move codes of 'sending mostat to the TOP surface' to the end
	   of mouse event processing. To avoid the surfuser to change
	   surface STATU before surfman finish its processing.
	2. If mouse Click is to bring a surface to TOP, then we shall end mouse
	   event processing after that, without sending mostat to the surface!
	   (Otherwise, one click CAN bring a surface to TOP, AND min/max/close
	    it simutaneouly! even ....)
	   TODO: If click and keep downhold on a SURFBTN_MIN, then after minimize
	   the surface, the mostat will be passed on to the next TOP surface! and
	   trigger its LeftKeyDownHold event, usullay move the surface to
	   the last position RELATIVE to the mouse mx/my!
	   Same for MAX/CLOSE!

2021-03-19:
	1. XXX If A surface is brought to TOP by resorting zseq, after other surface quits.
	       then also ering ERING_SURFACE_BRINGTOP to the surface.
2021-03-21:
	1. Cancel codes of sending ERING_SURFACE_BRINGTOP, which should be carried out
	   by surfman_render_thread()!


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

#define MCURSOR_NORMAL 	"/mmc/mcursor.png"
#define MCURSOR_GRAB 	"/mmc/mgrab.png"

#define SURFMAN_BKGIMG_PATH	"/mmc/egidesk.jpg"  // "/mmc/linux.jpg"

static EGI_MOUSE_STATUS mostat;
static EGI_MOUSE_STATUS *pmostat;
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus);
int 	lastX;
int 	lastY;
bool	surface_downhold; /* NO USE */

EGI_SURFMAN *surfman;
int	zseq;
int	surfID; /* as index of surfman->surfaces[] */
int	userID; /* NOW as surface->csFD */

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
	int j,k;
	int ch;
	int surfID;

	int opt;
//	int sw=0,sh=0; /* Width and Height of the surface */
//	int x0=0,y0=0; /* Origin coord of the surface */


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
	surfman->mcursor=egi_imgbuf_readfile(MCURSOR_NORMAL);
	surfman->mgrab=egi_imgbuf_readfile(MCURSOR_GRAB);

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
			pthread_mutex_lock(&surfman->surfman_mutex);
	/* ------ >>>  Surfman Critical Zone  */

			printf("surfman->scnt=%d \n", surfman->scnt);
			printf("ch: '%c'(%d)\n",ch, ch);
			k=ch-0x30;
			if( k < SURFMAN_MAX_SURFACES ) {
				/*** NOTE:
				 * Input k as sequence number of all registered surfaces.
				 * However, surfman->surfaces[] sorted in ascending Zseq order, some surfaces[]
				 * are NULL if surfman->scn < SURFMAN_MAX_SURFACES!
				 * So need to transfer k to actual index of surfman->surfaces[].
				 */
				 k=SURFMAN_MAX_SURFACES-surfman->scnt +k;
				 printf("Try to set surface %d top...\n", k);
				 //surfman_bringtop_surface(surfman, k);
				 surfman_bringtop_surface_nolock(surfman, k);
			}

	/* ------ <<<  Surfman Critical Zone  */
			pthread_mutex_unlock(&surfman->surfman_mutex);
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
			/* ------ >>>  Surfman Critical Zone  */

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
					//surfman->surfaces[surfID]->status=SURFACE_STATUS_MINIMIZED;
					surfman->surfaces[surfID]->surfshmem->status=SURFACE_STATUS_MINIMIZED;

			/* ------ <<<  Surfman Critical Zone  */
					pthread_mutex_unlock(&surfman->surfman_mutex);
					break;

				case 'm': /* Maximize the surface */
					printf("Maximize surface.\n");
					pthread_mutex_lock(&surfman->surfman_mutex);

					if(surfman->minsurfaces[0]) {
						//surfman->minsurfaces[0]->status=SURFACE_STATUS_NORMAL;
						surfman->minsurfaces[0]->surfshmem->status=SURFACE_STATUS_NORMAL;
						/* Bring to TOP */
						surfman_bringtop_surface_nolock(surfman, surfman->minsurfaces[0]->id);
					}

					pthread_mutex_unlock(&surfman->surfman_mutex);
					break;

				default:
					break;
			}
		}

		/* W2. Mouse event process */
	        if( egi_mouse_getRequest(pmostat) ) {  /* Apply pmostat->mutex_lock */

	                /* 1. LeftKeyDown: To bring the surface to top, to save current mouseXY */
        	        if( pmostat->LeftKeyDown )  {  //&& !surface_downhold ) {

				pthread_mutex_lock(&surfman->surfman_mutex);
		 /* ------ >>>  Surfman Critical Zone  */

                                surfman->mx  = pmostat->mouseX;
                                surfman->my  = pmostat->mouseY;

				/* A. Check if it clicks on any SURFACEs. */
				zseq=surfman_xyget_Zseq(surfman, surfman->mx, surfman->my);
				printf("Picked zseq=%d\n", zseq);

				surfID=surfman_xyget_surfaceID(surfman, surfman->mx, surfman->my );
				printf("surfID=%d\n",surfID);

				/* NOTE: Sending mostat to the top surface MUST be carried out at last!!
				 * For reasons of:
				 * 1. If it clicks on a surface SURFBTN_MIN, the surfuser may have changed surface STATUS before
				 *    executing following codes! AND MAY ALSO before surfman render the updated surface image/minibar!
				 * 2. The surface is displayed on desk while its STAUTS is already MINIMIZED acutually!
				 * 3. A surface may be brought to TOP by resorting zseq, after other surface quits.
				 */
				/* IF: clicked surface already on TOP layer. ( Minimized surfaces NOT considered! ) */
				// xxx if(surfID==SURFMAN_MAX_SURFACES-1) {
				int topID=surfman_get_TopDispSurfaceID(surfman);
				printf("topID=%d\n",topID);
				if( surfID==surfman_get_TopDispSurfaceID(surfman) ) {
					surface_downhold=true;

					/* If A surface is brought to TOP by resorting zseq, after other surface quits.
					 * ERING MSG TO THE SURFACE!
					 */
					if( userID != surfman->surfaces[surfID]->csFD ) {

				 #if 0 /* CANCELED: ------  BY surfman_render_thread()!!! */
						userID = surfman->surfaces[surfID]->csFD;
						/* Send msg to the surface */
						emsg->type=ERING_SURFACE_BRINGTOP;
					        if( unet_sendmsg( userID, emsg->msghead) <=0 ) /* userID as csFD */
					                	egi_dpstd("Fail to sednmsg ERING_SURFACE_BRINGTOP!\n");
				 #endif /* CANCELED */

					}
					else { /* */
						printf("Already TOP surface!\n");
					}
				}
				/* Else: If clicked surface is NOT current top surface. */
				else if(surfID>=0) { /* <0 as bkground */
					surface_downhold=true;
					printf("Bring surfID %d to TOP\n", surfID);
					/* Bring picked surface to TOP layer */
					surfman_bringtop_surface_nolock(surfman, surfID);
					/* Reset userID, NOW surfaces[SURFMAN_MAX_SURFACES-1] is TOP surface. */
					userID = surfman->surfaces[SURFMAN_MAX_SURFACES-1]->csFD;

				 #if 0 /* CANCELED: ------  BY surfman_render_thread()!!! */
					/* Send msg to the surface */
					emsg->type=ERING_SURFACE_BRINGTOP;
				        if( unet_sendmsg( userID, emsg->msghead) <=0 ) { /* userID as csFD */
					                egi_dpstd("Fail to sednmsg ERING_SURFACE_BRINGTOP!\n");
					}
				 #endif /* CANCELED */

					/* Here, we end mouse event processing without sending mostat to the surface! */
		 /* ------ <<<  Surfman Critical Zone  */
					pthread_mutex_unlock(&surfman->surfman_mutex);
					goto END_MOUSE_EVENT;
				}

				/* B. surfID<0: Check if it clicks on leftside minibar menu, Check IndexMpMinSurf, which is set by
				      surfman_render_thread() based on surfmap->mx,my
				 */
				else if ( surfman->IndexMpMinSurf >= 0 ) {
					/* Bring mouse clicked minisurface to TOP layer, also restore its status. */
					surfman_bringtop_surface_nolock(surfman, surfman->minsurfaces[surfman->IndexMpMinSurf]->id);

				 #if 0 /* CANCELED: ------  BY surfman_render_thread()!!! */
					/* Send msg to the surface */
					emsg->type=ERING_SURFACE_BRINGTOP;
				        if( unet_sendmsg( surfman->surfaces[SURFMAN_MAX_SURFACES-1]->csFD, emsg->msghead) <=0 ) {
					                egi_dpstd("Fail to sednmsg ERING_SURFACE_BRINGTOP!\n");
					}
				#endif /* CANCELED */

					/* Here, we end mouse event processing without sending mostat to the surface! */
		 /* ------ <<<  Surfman Critical Zone  */
					pthread_mutex_unlock(&surfman->surfman_mutex);
					goto END_MOUSE_EVENT;
				}

		 /* ------ <<<  Surfman Critical Zone  */
				pthread_mutex_unlock(&surfman->surfman_mutex);

#if 0 // MOVE TO END_MOUSE_EVENT  /* Reset lastX/Y, to compare with next mouseX/Y to get deviation. */
                	        lastX=pmostat->mouseX;
                        	lastY=pmostat->mouseY;
#endif

	                }
        	        /* 2. LeftKeyDownHold */
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

#if 0 /* NOW: It's SURFUSER's job to move surface */

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
#endif /* END: move surface */

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


#if 1			/* 5. At last, send mouse_status to the TOP(focused) surface */
			/*** NOTE:
			 *   1. If current mostat is a LeftKeyDown on minibar menu, then it brings a surface to TOP,
			 *	and here also immediately ering the same mostat to the surface! It may cause unwanted
			 *	effect.
			 *	Example: Only by one click, a surface appears, and at the same time a button of the surface
			 *	is activated.
			 */

			pthread_mutex_lock(&surfman->surfman_mutex);
	/* ------ >>>  Surfman Critical Zone  */

			if(surfman->scnt) {  /* Only if have surface registered */
				/*** NOTE:
				 *   1. If current surfaces[SURFMAN_MAX_SURFACES-1] is minimized by SURFUSER! then we have
				 *      to pick next top surface on the desk, NOT in the minibar menu!
				 *      OR the minimized surface will keep receiving mostat !!!!
				 *   2. NOT call surfman_bringtop_surface_nolock(), Let it keeps its zseq!
				 */
				for(j=0; j < surfman->scnt; j++) {
					if( surfman->surfaces[SURFMAN_MAX_SURFACES-1-j]->surfshmem->status != SURFACE_STATUS_MINIMIZED ) {
		/* Note: ering_msg_send() is BLOCKING type, if Surfuser DO NOT recv the msg, it will be blocked here!?  OR to kernel buffer. */
						//printf("ering msg send...\n");
						if( ering_msg_send( surfman->surfaces[SURFMAN_MAX_SURFACES-1-j]->csFD,
							emsg, ERING_MOUSE_STATUS, pmostat, sizeof(EGI_MOUSE_STATUS) ) <=0 ) {
							egi_dpstd("Fail to sendmsg ERING_MOUSE_STATUS!\n");
						}
						//printf("ering msg send OK!\n");

						break;
					}
				}
			}

	/* ------ <<<  Surfman Critical Zone  */
			pthread_mutex_unlock(&surfman->surfman_mutex);

#endif /* END send mostat */

END_MOUSE_EVENT:
			/* Reset lastX/Y, to compare with next mouseX/Y to get deviation. */
               	        lastX=pmostat->mouseX;
                       	lastY=pmostat->mouseY;

			/* Put request */
			egi_mouse_putRequest(pmostat);
                }

		tm_delayms(5);
   	} /* End while() */

	/* Free SURFMAN */
	egi_destroy_surfman(&surfman);
	//egi_imgbuf_free(mcursor)(mgrab);

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



