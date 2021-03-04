/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

TODO:
1. Surfman_render_thread DO NOT sync with mouse click_pick operation.
   Surfman render mouse position MAY NOT be current mouseXY, which
   appears as visual picking error.  Draw mark.
2. CPU schedule time for each thread MAY be heavely biased.

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

#define MCURSOR_ICON_PATH 	"/mmc/mcursor.png"

static EGI_MOUSE_STATUS mostat;
static EGI_MOUSE_STATUS *pmostat;
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus);
int lastX;
int lastY;
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

	/* 1. Create an EGI surface manager */
	surfman=egi_create_surfman(ERING_PATH_SURFMAN);
	if(surfman==NULL)
		exit(EXIT_FAILURE);
	else
		printf("Succeed to create surfman at '%s'!\n", ERING_PATH_SURFMAN);

	/* Load imgbuf for mouse cursor */
	surfman->mcursor=egi_imgbuf_readfile(MCURSOR_ICON_PATH);

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

		/* W.1  Keyboard event */
		/* Switch the top surface */
                ch=0;
                read(STDIN_FILENO, &ch,1);
		if(ch >'0' && ch < '9' ) {  /* 1,2,3...9 */
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
				default:
					break;
			}
		}

		/* W2. Mouse event */
	        if( egi_mouse_getRequest(pmostat) ) {
	                /* 1. LeftKeyDown: To bring the surface to top, to save current mouseXY */
        	        if(pmostat->LeftKeyDown) {

				pthread_mutex_lock(&surfman->surfman_mutex);
	 /* ------ >>>  Surfman Critical Zone  */

                                surfman->mx  = pmostat->mouseX;
                                surfman->my  = pmostat->mouseY;

				zseq=surfman_get_Zseq(surfman, surfman->mx, surfman->my);
				printf("Picked zseq=%d\n", zseq);

				surfID=surfman_get_surfaceID(surfman, surfman->mx, surfman->my );
				printf("surfID=%d\n",surfID);

				if(surfID>=0) { /* <0 as bkground */
					surface_downhold=true;

					/* Bring picked surface to TOP layer */
					surfman_bringtop_surface_nolock(surfman, surfID);
				}

	 /* ------ <<<  Surfman Critical Zone  */
				pthread_mutex_unlock(&surfman->surfman_mutex);

				/* Rest lastX/Y, to compare with next mouseX/Y to get deviation. */
                	        lastX=pmostat->mouseX;
                        	lastY=pmostat->mouseY;

	                }
        	        /* 2. LeftKeyDownHold: To pan image */
                	else if(pmostat->LeftKeyDownHold) {
                        	/* Note: As mouseDX/DY already added into mouseX/Y, and mouseX/Y have Limit Value also.
	                         * If use mouseDX/DY, the cursor will slip away at four sides of LCD, as Limit Value applys for mouseX/Y,
        	                 * while mouseDX/DY do NOT has limits!!!
                	         * We need to retrive DX/DY from previous mouseX/Y.
                        	 */

				pthread_mutex_lock(&surfman->surfman_mutex);
	 /* ------ >>>  Surfman Critical Zone  */

  			   if(surface_downhold) {
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

				surface_downhold=false;

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

		usleep(10000);
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

	/* Wait until last mouse_event_request is cleared */
        while( egi_mouse_checkRequest(mostatus) && !mostatus->cmd_end_loopread_mouse ) { usleep(2000); };

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
        pmostat=mostatus;

        /* 2. Request for respond */
        mostat.request = true;
        pmostat->request = true;

/*  --- <<<  Critical Zone  */
        /* Put mutex lock */
        pthread_mutex_unlock(&mostatus->mutex);

}


