/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An Egi_SURF GAME, a test program.


		--- Concept and Definition ---

A cube:			A grid space, it MAY holds a cubeimg. OR just blank.

A cubeimg:		An elementary cubic block, basic component to
			form 7 shapes(types) of building_blocks.

A playarea:		An area/zone for game playing.
			A map to record/erase cubeimgs/blocks on grids.

A playblock: 		A dropping/moving building_block.
			It presents in one of 7 shapes(types): Z,S,J,L,O,T,I.

A recorded block:	A building_block that touched down and rested
			on other idle blocks in playarea, its cubeimgs are
			recorded to playarea->map[], and its memspace
			freed.

touch_down:		A bblock hits ground or touches idle blocks at playarea,
			it happens when the bblock is prevented from falling/moving down.

bblock_waitrecord:	A status after a bblock touches down and wait for recording to playarea->map[],
			During that period, the player still has the chance to shift it left/right(UP/DOWN).
			it may succeeds to drop down (free fall) again.


		--- ViewPoint and Direction  ---

UP/DOWN			From the viewpoint of the player.
left/right/down		From the viewpoint of the forward moving bblock.


		----  GamePad Controls ----

Left/Right (ABS_X)      	Shift block Left/Right.
Up/Down	   (ABS_Y)      	Shift block Up/Down.
X	   (KEY_D 32)
Y	   (KEY_H 35)
A 	   (KEY_F 33)        	Rotate block 90Deg clockwise.
B	   (KEY_G 34)
L	   (KEY_J 36)
R	   (KEY_K 37)
SELECT	   (KEY_APOSTROPHE 40)
START      (KEY_GRAVE 41)	Pause/Play.



TODO:
XXX 0  At begin of bblock falling, some position will out of range and cause GAEM_OVER.
1. mstat event reacts too slowly!
2. Render speed LIMIT!
   Surface image rendering lags behind data/variable updating!
   Press GamePad in advance with some predicition!
3. Auto. rotate_adjust for bblock.

Journal:
2021-07-03:
	1. Start programming...
2021-07-04:
	1. PlayArea and elementary cube image.
2021-07-05:
	1. BUILD_BLOCK and its functions.
2021-07-06:
	1. More functions for bblock operation.
	   bblock_touchDown() _touchLeft() _shifLeft() _touchRight() _shiftRight
2021-07-07:
	1. Parse GamePad conkey keys in pmostat...
	2. Check complete lines in PLAYAREA and highlight it.
2021-07-08:
	1. Test to improve functions...  bblock_rotate_c90()!
	2. check_OKlines() / remove_OKlines()
	3. playblock may slip away(shift_left/right) and keep falling again during waitrecord.
2021-07-09:
	1. game_pause().
 (>!<)	2. Reset all motion tokens JUST before parsing user commands, motion tokens are
	   necessary for many purposes.
 (>!<)	3. Erase current playblock image before parsing user commands, and makeup/redraw the
	   old image if NO motion happens under user commands, so the workfb always
	   holds a complete image.


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


const char *pid_lock_file="/var/run/surf_tetris.pid";

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

/* SURF Menus */
const char *menu_names[] = { "File", "Option", "Help"};
enum {
        MENU_FILE       =0,
        MENU_OPTION     =1,
        MENU_HELP       =2,
        MENU_MAX        =3, /* <--- Limit */
};


/* For PlayArea VFBDEV. */
#define PLAYAREA_COLOR	COLOR_RGB_TO16BITS(150,177,162)   /* To mimic black/white LCD of classic handheld game player. */

FBDEV           workfb;           	/* Working Virt FBDEV. represent work area in the surface. */
EGI_IMGBUF      *workimg=NULL;	  	/* Workimg for playarea */
void 		refresh_workimg(void);  /* Update workimg to surfimg to refresh displaying. */

const int 	PlayAreaW=300;		/* Size of play_area */
const int	PlayAreaH=150;
const int	WGrids=20;
const int 	HGrids=10;
bool		finish_lines[20];   /* 20==WGrids,  If line/column X is a complete line, then set finish_lines[x]=true */

/* For Buiding Blocks */
const int 	cube_ssize=15;	/* Side size of the cubeimg, in pixels. */
EGI_IMGBUF	*cubeimg;   	/* Elementary cubic block, basic component to form a building_block. */
EGI_IMGBUF	*eraseimg;  	/* An imgbuf with same color as of PLAYAREA, use as an ERASER! */
EGI_IMGBUF 	*hcimg;		/* Highlighted cubeimg */
EGI_IMGBUF	*tmpimg;

typedef struct building_block {
	int x0, y0;	/* Origin X,Y under playarea coord. system. */
	int type;	/* Shape/Block type */
	int w;		/* Width and height of the bounding box of a building_block, as row/column number of cubes. */
	int h;
	bool *map;	/* cube map, size cw*ch,  1--a cube there; 0--No cube */
} BUILD_BLOCK;

int	mspeed=1;	/* mpseed*cube_ssize per 1000ms */
int     fspeed;		/* Fast speed enabled by cmd_fast_down */
int	cnt_lines;
int	cnt_sorces;


/* Block Types */
enum {
	/* Building blocks */
	SHAPE_Z =0,
	SHAPE_S =1,
	SHAPE_J =2,
	SHAPE_L =3,
	SHAPE_O =4,
	SHAPE_T =5,
	SHAPE_I =6,
	SHAPE_MAX =7,

	/* Other functional blocks */
	PLAYAREA_MAP =8,	/* To map PlayArea */
	COMPLETE_LINE =9,	/* A completed block bar/line  */
	BTYPE_MAX =10		/* Block type MAX */
};

BUILD_BLOCK *shape_Z, *shape_S, *shape_J, *shape_L, *shape_O, *shape_T;
BUILD_BLOCK *playblock; /* The ONLY droping/moving block */
BUILD_BLOCK *playarea;	/* Map of PlayArea */

bool bblock_waitrecord;   /* The moving bblock touched down, waiting to record to playarea.
			   * However, it can still shift_left/right! If the bblock shifts to a position where it can
			   * freefall again, then reset bblock_waitrecord to FALSE again!
			   */

/* GamePad command */
bool cmd_shift_left;  /* Left/Right: relative to the forward moving direction. Shift UP one cube. */
bool cmd_shift_right; /* Shift DOWN one cube */
bool cmd_fast_down;
bool cmd_rotate_c90;
bool cmd_rotate_ac90;
bool cmd_pause_game;

/* Game motion status */
struct block_motion_status {
	bool 	shift_dx	:1;
	bool  	shift_dy	:1;
	bool    rotate_ddeg	:1;
	bool	pause_game	:1;
};
union motion_status{
	int				motions;
  	struct block_motion_status	motion_bits;

} motion_status;

/* Game Basic Functions */
int 	create_cubeimg(void);
int 	create_hcimg(void);
int	create_eraseimg(void);

BUILD_BLOCK 	*create_bblock(int type, int x0, int y0);
void	destroy_bblock(BUILD_BLOCK **block);
void 	draw_bblock(FBDEV *fbdev, const BUILD_BLOCK *block, EGI_IMGBUF *cubeimg);
void 	erase_bblock(FBDEV *fbdev, const BUILD_BLOCK *block);

bool	bblock_touchDown(const BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
int	bblock_shiftDown(BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
bool	bblock_touchLeft(const BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
int 	bblock_shiftLeft(BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
bool	bblock_touchRight(const BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
int 	bblock_shiftRight(BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
int 	bblock_rotate_c90(BUILD_BLOCK *block, const BUILD_BLOCK *playarea);

int 	record_bblock(const BUILD_BLOCK *block,  BUILD_BLOCK *playarea);
int 	check_OKlines(BUILD_BLOCK *playarea, bool *results);
int	remove_OKlines(BUILD_BLOCK *playarea, bool *results);

void	game_over(void);
void	game_pause(void);


/* For text buffer */
char ptxt[1024];	/* Text */
int poff;		/* Offset to ptxt */
EGI_16BIT_COLOR	fcolor=WEGI_COLOR_BLACK;
int fw=20, fh=20;
int fgap=4;


/* Apply SURFACE module default function */
void my_redraw_surface(EGI_SURFUSER *surfuser, int w, int h);
//void  *surfuser_ering_routine(void *args);  /* For LOOP_TEST and ering_test, a little different from module default function. */
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
	int sw=0,sh=0; /* Width and Height of the surface */
	int x0=0,y0=0; /* Origin coord of the surface */
	char *pname=NULL;
	EGI_CLOCK eclock={0};
	int ret;

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

        /* P1. Create working virt. FBDEV. Side 1 pix frame. */
        if( init_virt_fbdev2(&workfb, PlayAreaW, PlayAreaH,-1, PLAYAREA_COLOR) ) {  /* vfb, xres, yres, alpha, color */
                printf("Fail to initialize workfb!\n");
                exit(EXIT_FAILURE);
        }
        workfb.pixcolor_on=true; /* workfb apply FBDEV pixcolor */
	workimg = workfb.virt_fb;
	printf("Workimg size: %dx%d\n", workimg->width, workimg->height);

	/* P2. Create cubeimg+hcimg+eraseimg. */
	if( create_cubeimg()!=0 || create_eraseimg()!=0 || create_hcimg()!=0 )
		exit(EXIT_FAILURE);

	/* P3. Create PLAYAREA */
	playarea=create_bblock(PLAYAREA_MAP, 0, 0);
	if(playarea==NULL)
		exit(EXIT_FAILURE);

START_TEST:
	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	sw=SURF_MAXW;  sh=SURF_MAXH;
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, pid_lock_file, x0, y0, SURF_MAXW,SURF_MAXH, sw, sh, colorType );
	if(surfuser==NULL) {
		int static ntry=0;
		ntry++;
		if(ntry==3)
			exit(EXIT_FAILURE);
		sleep(1);
		goto START_TEST;
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

	/* 3. Give a name for the surface. */
	strncpy(surfshmem->surfname, "游戏:俄罗斯方块", SURFNAME_MAX-1);

	/* 4. First draw surface  */
	surfshmem->bkgcolor=PLAYAREA_COLOR; //WEGI_COLOR_DARKGRAY; /* OR default BLACK */
	surfshmem->topmenu_bkgcolor=egi_color_random(color_light);
	surfshmem->topmenu_hltbkgcolor=WEGI_COLOR_GRAYA;
	surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MIN, MENU_MAX, menu_names); /* Default firstdraw operation */

	/* font color */
	fcolor=COLOR_COMPLEMENT_16BITS(surfshmem->bkgcolor);

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* XXX 5. Create SURFBTNs by blockcopy SURFACE image, after first_drawing the surface! */
        /* 5A. CopyBlock workfb IMGBUF (playarea imgbuf) to surfimg */
        egi_imgbuf_copyBlock( surfimg, workfb.virt_fb, false,   /* dest, src, blend_on */
                              workimg->width, workimg->height,  /* bw, bh */
			      (surfimg->width-PlayAreaW)/2, surfimg->height-5-PlayAreaH, 0, 0);    /* xd, yd, xs,ys */
	/* 5B. Draw a division line above playarea */
	fbset_color2(vfbdev,WEGI_COLOR_BLACK);
	draw_wline_nc(vfbdev, 5, SURF_MAXH-5-PlayAreaH-5, SURF_MAXW-5, SURF_MAXH-5-PlayAreaH-5, 3);
#if 0 	/* 5C. Draw outer rim of playarea */
	fbset_color2(vfbdev,WEGI_COLOR_BLACK);
	draw_rect(vfbdev, (surfimg->width-PlayAreaW)/2-2, 		(surfimg->height-5-PlayAreaH)-2,
			  (surfimg->width-PlayAreaW)/2-2 +PlayAreaW+2, 	(surfimg->height-5-PlayAreaH)-2 +PlayAreaH+2
		  );
#endif

	/* 6. Start Ering routine */
	printf("start ering routine...\n");
	if( pthread_create(&surfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, surfuser) !=0 ) {
		printf("Fail to launch thread_EringRoutine!\n");
		exit(EXIT_FAILURE);
	}

	/* 7. Activate image */
	surfshmem->sync=true;

/* ------ <<<  Surface shmem Critical Zone  */
                pthread_mutex_unlock(&surfshmem->shmem_mutex);


	/* -------------------  <<<  Main Game Loop >>> -------------------- */
	/* Start eclock */
	egi_clock_start(&eclock);

	while( surfshmem->usersig != 1 ) {

		/* M1. Newborn playblock
		 *  1) MUST within PLAYAREA width.
		 *  2) MUST above start line! for we'll check touchDown there!
		 */
		if(playblock==NULL) {
			/* Note: Only init. X <= -4*cube_ssize, then can check if touchs at start, GAME OVER! */
			playblock=create_bblock(mat_random_range(SHAPE_MAX), -4*cube_ssize, mat_random_range(7)*cube_ssize ); /* 7=10-4+1 */
			//playblock=create_bblock(5+mat_random_range(2), -4*cube_ssize, mat_random_range(7)*cube_ssize ); /* 7=10-4+1 */
			//playblock=create_bblock(SHAPE_I, -4*cube_ssize, mat_random_range(7)*cube_ssize ); /* 7=10-4+1 */
			if(playblock==NULL) {
				egi_dperr("Fail to create BBLOCK!");
				exit(-1);
			}

			printf("Create playblock shape=%d, x0=%d\n", playblock->type, playblock->x0);
		}

		/* XXX Erase old position playblock image NOT HERE!!!  now playblock has NEW position data! */

	/* M2. If any motion done for the playblock, redraw to workfb and update workimg. */
	if(motion_status.motions) {
		/* M2.1 Update/Draw PLAYAREA */
		draw_bblock(&workfb, playarea, cubeimg);

		/* M2.2 Draw playblock as position renewed */
		draw_bblock(&workfb, playblock, cubeimg);

		/* M2.3  ***** Refresh workimg/workFB to surfimg. *****
		 * ONLY if surfimg changes:
		 *   1) The bblock position changed.
		 */
		refresh_workimg();

		if(motion_status.motion_bits.shift_dx==true)
			egi_clock_restart(&eclock);  /* <<<< ------- For visual effect! --- */

		/* XXX Reset all motion tokens!
		   XXX move to before parsing user commands. */
		// motion_status.motions=0;

	} /* End if(motion_status.motions) */
	else	/* Makeup with/redraw playblock image as just erased in M7, to keep workfb holding a complete image. */
		draw_bblock(&workfb, playblock, cubeimg);

		/* M3. Check game_pause key */
		if( cmd_pause_game ) {
			/* Backup workimg */
			tmpimg = egi_imgbuf_blockCopy( workimg, 0, 0, workimg->height, workimg->width);

			game_pause();
			refresh_workimg();
			while( cmd_pause_game ) { usleep(500000); };

			/* Restore workimg */
			egi_imgbuf_copyBlock( workimg, tmpimg, false, workimg->width, workimg->height, 0,0,0,0);
			egi_imgbuf_free2(&tmpimg);
			refresh_workimg();
		}

		/* M4. BLANK */

		/* M5A.  If playblock slip away and keep falling during waitrecord, ONLY if a motion is done... */
		if( bblock_waitrecord && motion_status.motions && bblock_touchDown(playblock, playarea)==false ) {
			bblock_waitrecord = false;
			printf("Slip to fall...\n");
		}
		/* M5B.  Check if it hits ground or touches other recorded blocks in playarea! */
		else if( bblock_waitrecord || bblock_touchDown(playblock,  playarea) ) {
			//printf("Touch down!\n");
			/* Wait for record to playarea->map! The player has the last chance to move it left/right(UP/DOWN)! */
			if( bblock_waitrecord
			    && egi_clock_peekCostUsec(&eclock) >= ( fspeed>0 ? 1000/fspeed*1000 : 1000/mspeed*1000 ) ) {
				/* Record bblock to PLAYAREA */
				if( record_bblock(playblock, playarea) == -1) {  /* -1 GAME_OVER */
					printf(" --- GAME OVER --- \n");
					game_over();  /* Give some hint */
					refresh_workimg();
					sleep(3);

					/* Erase image before clearing playarea! */
					// XXX erase_bblock(&workfb, playarea);
					/* Clear whole workimg(playarea) */
					egi_imgbuf_resetColorAlpha(workimg, PLAYAREA_COLOR, -1);

					/* Clear playarea map */
					memset(playarea->map, 0, playarea->w*playarea->h*sizeof(typeof(*playarea->map)));
				}

				/* Destroy the bblock. */
				printf("Record and destroy playblock.\n");
				destroy_bblock(&playblock);

				/* Reset waitrecord */
				bblock_waitrecord=false;

				/* !!!!!!!!! */
				egi_clock_restart(&eclock);

/* <<< -------------- Loop back -------- */
				continue;
			}
			else  if(!bblock_waitrecord)  {  /* Just done checkDown  */
				bblock_waitrecord=true;
				egi_dpstd("BBlock just touchs down!\n");
				///egi_dpstd("Wait for user input.... playblock->x0=%d*cubes\n", playblock->x0/cube_ssize);
			}
		}

		/* M6. Check playarea->map[] results and remove completed lines.   */
		if( bblock_waitrecord==false ) {
			/* playarea may NOT updated to workfb yet */
			draw_bblock(&workfb, playarea, cubeimg);

		    if( (ret=check_OKlines(playarea, finish_lines)) >0 ) {
			printf("---> %d complete lines <---\n", ret);


			/* To show highlight finished lines. */
			/* in check_OKlines() draw highlighted lines */
			//draw_bblock(&workfb, playblock, cubeimg); /* Put current playblock also */

			refresh_workimg();
			usleep(500000);
			remove_OKlines(playarea, finish_lines);

			/* MUST Clear whole workimg(playarea) after remove_OKlines!
			 * draw_bblock() ONLY update cubeimgs where map[] is ture!
			 */
			egi_imgbuf_resetColorAlpha(workimg, PLAYAREA_COLOR, -1);
		    }
		}

		/* NOW: playblock != NULL */

		/* M7. Erase old playblock image,  position data updated after motions! */
		erase_bblock(&workfb, playblock);

		/* Here reset all motion tokens! Just before parsing user commands. */
		motion_status.motions=0;

		/* M8. Parse user command, move OR Rotate bblock  */
		/* M8.1 Rotate:  Check if space is enough to accommodate rotated bblock...HERE! */
		if(cmd_rotate_c90) {
			if( bblock_rotate_c90(playblock, NULL)==0 )
				motion_status.motion_bits.rotate_ddeg=true;

			cmd_rotate_c90 = false;
		}
		/* M8.2 Move playblock: Check touch_down laster... */
		if(cmd_shift_left) {
			if( bblock_shiftLeft(playblock, playarea)==0 )
				motion_status.motion_bits.shift_dy=true;
			cmd_shift_left = false;
		}
		if(cmd_shift_right) {
			if( bblock_shiftRight(playblock, playarea)==0 )
				motion_status.motion_bits.shift_dy=true;
			cmd_shift_right = false;
		}
		if(cmd_fast_down) {
			fspeed = 20;
			cmd_fast_down = false;
		}
		/* M8.3 AT LAST, Moving bblock forward / Falling down..    FastSpeed or NormalSpeed */
		if( egi_clock_peekCostUsec(&eclock) >= ( fspeed>0 ? 1000/fspeed*1000 : 1000/mspeed*1000 ) ) {
			if( !bblock_waitrecord ) {
				if( bblock_shiftDown(playblock, playarea)==0 ) {

					//printf(" Set motion shift_dx=true\n");
					motion_status.motion_bits.shift_dx=true;
				}
			}

			fspeed=0;
			//egi_clock_restart(&eclock); /* For visual effect, restart eclock after update surimg */
		}
	}

	/* ---------------  END Game Loop  ----------------- */


        /* Free SURFBTNs, ---- OK, to be released by egi_unregister_surfuser()!  */
//        for(i=0; i<3; i++)
//                egi_surfBtn_free(&surfshmem->sbtns[i]);

        /* Join ering_routine  */
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
	printf("Joint thread_eringRoutine...\n");
        if( pthread_join(surfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

	/* Unregister and destroy surfuser */
	printf("Unregister surfuser...\n");
	if( egi_unregister_surfuser(&surfuser)!=0 )
		egi_dpstd("Fail to unregister surfuser!\n");

        /* <<<<<  EGI general release   >>>>>> */
        // Ignore;

	printf("Exit OK!\n");
	exit(0);
}


/*--------------------------------------------------------------------
                Mouse Event Callback
                (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event() .
4. Some vars of pmostat->conkeys are 
--------------------------------------------------------------------*/
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	int k;
//	char strtmp[128];

	int dv=0;  /* delt value */

/* --------- E1. Parse Keyboard Event + GamePad ---------- */

	/* Convert GamePad ABS_X/Y value to [1 -1] */
	dv=(pmostat->conkeys.absvalue -127)>>7; /* NOW: abs value [1 -1] and same as FB Coord. */
	if(dv) {
	   egi_dpstd("abskey code: %d\n",pmostat->conkeys.abskey);
	   switch( pmostat->conkeys.abskey ) {
        	case ABS_X:
                	printf("ABS_X: %d\n", dv); /* NOW: abs value [1 -1] and same as FB Coord. */
			if(dv>0)
				cmd_fast_down=true;
                        break;
                case ABS_Y:
                        printf("ABS_Y: %d\n", dv);
			if(dv<0)
				cmd_shift_left=true;
			if(dv>0)
				cmd_shift_right=true;
                        break;
                default:
                        break;
	   }
        }

	/* Lastkey MAY NOT cleared yet! Check press_lastkey!! */
	if(pmostat->conkeys.press_lastkey) {
		egi_dpstd("lastkey code: %d\n",pmostat->conkeys.lastkey);
		switch(pmostat->conkeys.lastkey) {
			case KEY_GRAVE:
				cmd_pause_game=!cmd_pause_game;
				break;
			case KEY_F:
				cmd_rotate_c90=true;
				break;
			default:
				break;
		}
	}


//	egi_dpstd("asciikey code: %d\n",pmostat->conkeys.asciikey);

	/* NOTE: pmostat->chars[] is read from ssh STDIN !!! NOT APPLICABLE HERE !!! */
//	if(pmostat->nch) {
//		printf("KEYs: ");
//		for(k=0; k< pmostat->nch; k++)
//			printf("%d ", pmostat->chars[k]);
//		printf("\n");
//	}


#if 0 ////////////
   	/* ------TEST:  CONKEY group */
	/* Get CONKEYs input */
	if( pmostat->conkeys.nk >0 ) { // || pmostat->conkeys.asciikey ) {
		egi_dpstd("conkeys.nk: %d\n",pmostat->conkeys.nk);
		/* Clear ptxt */
		ptxt[0]=0;

		/* CONKEYs */
	        for(k=0; k < pmostat->conkeys.nk; k++) {
			/* If ASCII_conkey */
			if( pmostat->conkeys.conkeyseq[k] == CONKEYID_ASCII ) {
				sprintf(strtmp,"+Key%u", pmostat->conkeys.asciikey);
				strcat(ptxt, strtmp);
			}
			/* Else: SHIFT/ALT/CTRL/SUPER */
			else {
				if(ptxt[0]) strcat(ptxt, "+"); /* If Not first conkey, add '+'. */
				strcat(ptxt, CONKEY_NAME[ (int)(pmostat->conkeys.conkeyseq[k]) ] );
			}
	        }
  #if 0		/* ASCII_Conkey */
		if( pmostat->conkeys.asciikey ) {
			sprintf(strtmp,"+Key%u", pmostat->conkeys.asciikey);
			strcat(ptxt, strtmp);
		}
  #endif
		/* TODO: Here topbar BTNS will also be redrawn, and effect of mouse on top TBN will also be cleared! */
	        my_redraw_surface(surfuser, surfimg->width, surfimg->height);
	}

  #if 0	/* Parse Gamepad Keys */
	else if( pmostat->conkeys.press_lastkey ) {
		egi_dpstd("lastkey code: %d\n",pmostat->conkeys.lastkey);

                /* Clear ptxt */
		ptxt[0]=0;

		if(pmostat->conkeys.lastkey > KEY_F1-1 && pmostat->conkeys.lastkey < KEY_F10+1 )
			sprintf(ptxt,"F%d", pmostat->conkeys.lastkey-KEY_F1+1);
		else if(pmostat->conkeys.lastkey > KEY_F11-1 && pmostat->conkeys.lastkey < KEY_F12+1 )
			sprintf(ptxt,"F%d", pmostat->conkeys.lastkey-KEY_F11+11);

		/* TODO: Here topbar BTNS will also be redrawn, and effect of mouse on top TBN will also be cleared! */
	        my_redraw_surface(surfuser, surfimg->width, surfimg->height);
	}
	/* Clear text on the surface */
	else if( ptxt[0] !=0 ) {
		 ptxt[0]=0;
	        my_redraw_surface(surfuser, surfimg->width, surfimg->height);
	}
  #endif

#endif

/* --------- E2. Parse STDIN Input ---------- */

/* --------- E3. Parse Mouse Event ---------- */


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


/*----------------------------------
Update workfb imgbuf to surfimg to
refresh displaying.
-----------------------------------*/
void refresh_workimg(void)
{
        //egi_dpstd("Update workimg...\n");

        /* Get surface mutex_lock */
        if( pthread_mutex_lock(&surfshmem->shmem_mutex) !=0 )
                egi_dperr("Fail to get mutex_lock for surface.");
/* ------ >>>  Surface shmem Critical Zone  */

	/* Copyblock workfb imgbuf to surfimg  */
        egi_imgbuf_copyBlock( surfimg, workfb.virt_fb, false,   /* dest, src, blend_on */
                              workimg->width, workimg->height,  /* bw, bh */
			      (surfimg->width-PlayAreaW)/2, surfimg->height-5-PlayAreaH, 0, 0);    /* xd, yd, xs,ys */

/* ------ <<<  Surface shmem Critical Zone  */
        pthread_mutex_unlock(&surfshmem->shmem_mutex);
}


/*---------------------------
Create cubeimg.

Return:
	0	OK
	<0	Fails
---------------------------*/
int  create_cubeimg(void)
{
	FBDEV vfb={0};

	/* Prepare cubeimg */
	cubeimg=egi_imgbuf_createWithoutAlpha(cube_ssize, cube_ssize, WEGI_COLOR_BLACK);
	if(cubeimg==NULL)
		return -1;

  	if( init_virt_fbdev(&vfb, cubeimg, NULL)!=0 ) {
		egi_imgbuf_free2(&cubeimg);
		return -2;
	}
	vfb.pixcolor_on=true;

	fbset_color2(&vfb, PLAYAREA_COLOR);
	draw_rect(&vfb, 0, 0, cube_ssize-1, cube_ssize-1);
	draw_wrect(&vfb, 3, 3, cube_ssize-1-3, cube_ssize-1-3, 2);

	return 0;
}

/*---------------------------
Create highlighted cubeimg.

Return:
	0	OK
	<0	Fails
---------------------------*/
int  create_hcimg(void)
{
	FBDEV vfb={0};

	/* Prepare cubeimg */
	hcimg=egi_imgbuf_createWithoutAlpha(cube_ssize, cube_ssize, WEGI_COLOR_LTYELLOW);
	if(hcimg==NULL)
		return -1;

  	if( init_virt_fbdev(&vfb, hcimg, NULL)!=0 ) {
		egi_imgbuf_free2(&hcimg);
		return -2;
	}
	vfb.pixcolor_on=true;

	fbset_color2(&vfb, PLAYAREA_COLOR);
	draw_rect(&vfb, 0, 0, cube_ssize-1, cube_ssize-1);
	draw_wrect(&vfb, 3, 3, cube_ssize-1-3, cube_ssize-1-3, 2);

	return 0;
}

/*--------------------------
Create eraseimg.
Return:
	0	OK
	<0	Fails
---------------------------*/
int  create_eraseimg(void)
{
	eraseimg=egi_imgbuf_createWithoutAlpha(cube_ssize, cube_ssize, PLAYAREA_COLOR);
	if(eraseimg==NULL)
		return -1;

	return 0;
}


/*---------------------------------------------
Create a building_block.

@shape_type:	Type of the block.
		SHAPE_L, SHAPE_O...etc.
@x0,y0:		Origin of the block.

Return:
	A pointer to BUILD_BLOCK 	OK
	NULL				Fails
----------------------------------------------*/
BUILD_BLOCK *create_bblock(int type, int x0, int y0)
{
	int i;
	BUILD_BLOCK *block=NULL;

	/* Rule out invalid type number */
	if( type<0 || type==SHAPE_MAX || type > BTYPE_MAX-1 )
		return NULL;

	block=calloc(1, sizeof(BUILD_BLOCK));
	if(block==NULL)
		return NULL;

	/* Assign other memebers */
	switch(type) {
        	case SHAPE_Z:
			block->w=3; 	block->h=2;
			block->map=calloc(1, block->w*block->h*sizeof(typeof(*block->map)));
			if(block->map==NULL)
				break;
						/* --- Cube MAP --- */
			block->map[0]=true;	block->map[1]=true;
						block->map[4]=true;	block->map[5]=true;

			break;
		case SHAPE_S:
			block->w=3; 	block->h=2;
			block->map=calloc(1, block->w*block->h*sizeof(typeof(* block->map)));
			if(block->map==NULL)
				break;
						/* --- Cube MAP --- */
						block->map[1]=true;	block->map[2]=true;
			block->map[3]=true;	block->map[4]=true;
			break;
        	case SHAPE_J:
			block->w=2; 	block->h=3;
			block->map=calloc(1, block->w*block->h*sizeof(typeof(* block->map)));
			if(block->map==NULL)
				break;
						/* --- Cube MAP --- */
						block->map[1]=true;
						block->map[3]=true;
			block->map[4]=true;	block->map[5]=true;
			break;
	        case SHAPE_L:
			block->w=2; 	block->h=3;
			block->map=calloc(1, block->w*block->h*sizeof(typeof(* block->map)));
			if(block->map==NULL)
				break;
						/* --- Cube MAP --- */
			block->map[0]=true;
			block->map[2]=true;
			block->map[4]=true;	block->map[5]=true;
			break;
        	case SHAPE_O:
			block->w=2; 	block->h=2;
			block->map=calloc(1, block->w*block->h*sizeof(typeof(* block->map)));
			if(block->map==NULL)
				break;
						/* --- Cube MAP --- */
			block->map[0]=true;	block->map[1]=true;
			block->map[2]=true;	block->map[3]=true;
			break;
        	case SHAPE_T:
			block->w=3; 	block->h=2;
			block->map=calloc(1, block->w*block->h*sizeof(typeof(* block->map)));
			if(block->map==NULL)
				break;
						/* --- Cube MAP --- */
						block->map[1]=true;
			block->map[3]=true;	block->map[4]=true;	block->map[5]=true;
			break;
		case SHAPE_I:
			block->w=1; 	block->h=4;
			block->map=calloc(1, block->w*block->h*sizeof(typeof(* block->map)));
			if(block->map==NULL)
				break;
						/* --- Cube MAP --- */
			block->map[0]=true;
			block->map[1]=true;
			block->map[2]=true;
			block->map[3]=true;
			break;
		case PLAYAREA_MAP:
			block->w=WGrids;
			block->h=HGrids;
                        block->map=calloc(1, block->w*block->h*sizeof(typeof(* block->map)));
                        if(block->map==NULL)
                                break;
			break;
		case COMPLETE_LINE:
			block->w=1;
			block->h=HGrids;
                        block->map=calloc(1, block->w*block->h*sizeof(typeof(* block->map)));
                        if(block->map==NULL)
                                break;
			for(i=0; i < block->h; i++)
				block->map[i]=true;
			break;

		default:
			break;

	} /* switch() */

	/* Check map, if shape type out of range, OR failed to calloc block->map. */
	if(block->map==NULL) {
		free(block);
		block=NULL;
	}

	/* Assign other members */
	block->type=type;
	block->x0=x0;
	block->y0=y0;

	return block;
}

/*---------------------------------------------
Destroy/Free an BUILD_BLOCK
----------------------------------------------*/
void destroy_bblock(BUILD_BLOCK **block)
{
	if( block==NULL || *block==NULL)
		return;

	free( (*block)->map );
	free( (*block) );

	(*block)=NULL;
}



/*---------------------------------------------
Draw a building_block to FBDEV.

@fbdev: Pointer to FBDEV.
@block:	Pointer to a BUILD_BLOCK
@cubeimg: Image of elementary cubic block.
----------------------------------------------*/
void draw_bblock(FBDEV *fbdev, const BUILD_BLOCK *block, EGI_IMGBUF *cubeimg)
{
	int i,j;

	if(fbdev==NULL || block==NULL || block->map==NULL )
		return;

	for(i=0; i< block->h; i++) {
	   for(j=0; j< block->w; j++) {
		if( block->map[i*block->w +j] ) {
		   egi_subimg_writeFB(cubeimg, fbdev, 0, -1,    /* imgbuf, fb, subcolor */
		   		block->x0+cube_ssize*j, block->y0+cube_ssize*i ); /*  x0,y0 */
		}
	   }
	}

}


/*-----------------------------------------------------
Erase a building_block by cover it with PLAYAREA_COLOR.

@fbdev: Pointer to FBDEV.
@block:	Pointer to BUILD_BLOCK
------------------------------------------------------*/
void erase_bblock(FBDEV *fbdev, const BUILD_BLOCK *block)
{
	int i,j;

	if(fbdev==NULL || block==NULL || block->map==NULL)
		return;

	for(i=0; i< block->h; i++) {
	   for(j=0; j< block->w; j++) {
		if( block->map[i*block->w +j] ) {
		   egi_subimg_writeFB(eraseimg, fbdev, 0, -1,    /* imgbuf, fb, subcolor */
		   		block->x0+cube_ssize*j, block->y0+cube_ssize*i ); /*  x0,y0 */
		}
	   }
	}
}

/*-------------------------------------------------------------------
Check if a bblock hits ground or touches down on idle/recorded blocks.

@block: 	Pointer to BUILD_BLOCK
@playarea:	Pointer to BUILD_BLOCK as playarea.

Return:
	True		Touch down.
	False		Still falling, OR fails.
--------------------------------------------------------------------*/
bool bblock_touchDown(const BUILD_BLOCK *block,  const BUILD_BLOCK *playarea)
{
	int i,j;
	int gx,gy;   /* playarea XY in grids/cubes, gx,gy MAY <0!  invalid */
	int index;   /* index of playarea->map[] */

	if(block==NULL || block->map==NULL || playarea==NULL || playarea->map==NULL)
		return false;

	gx = block->x0/cube_ssize;
	gy = block->y0/cube_ssize;

	/* Check if nearby right(DOWN) cube is ALSO occupied by playarea(with recorded cubeimg) */
	for(i=0; i< block->h; i++) {
	   for(j=0; j< block->w; j++) {
		/* ONLY if current block grid/map has cubeimg. */
		if( block->map[i*block->w +j] ) {
			/* gx+j MAY out of PLAYAREA, then index invalid!
			 * However gx+j=-1 is OK! When the bearby_right(DOWN) cube belonges to PLAYAREA
			 * and its index is ALSO valid!
			 */
			if(gx+j < -1) // if( gx+j <0 )
				continue;
			/* gy+i MAY out of PLAYAREA, then index invalid!  */
			else if( gy+i<0 || gy+i > playarea->h-1 )
				continue;

			/* Hit ground! get to bottom of the playarea */
			if( gx+j >= playarea->w-1 ) {
				egi_dpstd("Hit ground!\n");
				return true;
			}

			/* Index of playarea->map[], as the nearby_right cube */
			index = (gy+i)*playarea->w+ gx+j +1; /* +1 nearby_right (DOWN) */

			/* DOUBLE CHECK! Index MAY out of range? */
			if( index <0 || index > playarea->w*playarea->h -1 ) {
				egi_dpstd("Index of playarea->map[] is invalid!\n");
				continue;
			}

			/* If nearby_right grid is ALSO occupied by playarea cubeimg */
			if( playarea->map[index] )
				return true;
		}
	   }
	}

	/* XXX already checked above */
#if 0	/* Check if bblock is just touched down at start line! */
	if( gx+block->w == 0 )  { /* gx = -3, at start line */
		j = block->w-1;
		for( i=0; i< block->h; i++ ) {
			if( block->map[i*block->w +j] ) {
				index = (gy+i)*playarea->w +0;
				if( playarea->map[index] ) {
					printf(" ----> game over <-----\n");
					return true;
				}
			}
		}
	}
#endif

	return false;
}

/*-------------------------------------------------------------------
Check if a forward moving bblock touches at idle/recorded bblocks at
its RIGHT side.
(Left side: relative to the forward moving direction )

@block: 	Pointer to BUILD_BLOCK
@playarea:	Pointer to BUILD_BLOCK as playarea.

Return:
	True		Touch.
	False		No touch, OR fails.
--------------------------------------------------------------------*/
bool bblock_touchLeft(const BUILD_BLOCK *block,  const BUILD_BLOCK *playarea)
{
	int i,j;
	int gx,gy;   /* playarea XY in grids/cubes, gx,gy MAY <0! invalid.  */
	int index;

	if(block==NULL || block->map==NULL || playarea==NULL || playarea->map==NULL)
		return false;

	gx = block->x0/cube_ssize;
	gy = block->y0/cube_ssize;

	/* Check if nearby UP(left) cube is ALSO occupied by playarea cubeimg */
	for(i=0; i< block->h; i++) {
	   for(j=0; j< block->w; j++) {
		/* ONLY if current block grid/map has cubeimg. */
		if( block->map[i*block->w +j] ) {
			/* If nearby UP(left) cube is ALSO occupied by playarea cubeimg */
			index = (gy+i -1)*playarea->w+ gx+j; /* gy+i -1 nearby_UP(left) */
			/* (gy+i-1) MAY <0! and index MAY <0!  */
			if( gy+i-1 > -1 && index >-1 && playarea->map[index] )  /* index MAY <0 */
				return true;
			/* ELSE: touches UP(left) side limit of PLAYAREA */
			else if( gy+i <=0 ) //gy+i-1 <0 )
				return true;
		}
	   }
	}

	return false;
}


/*-------------------------------------------------------------------
Check if a forward moving bblock touches at idle/recorded bblocks at
its RIGHT side.
(Right side: relative to the forward moving direction )

@block: 	Pointer to BUILD_BLOCK
@playarea:	Pointer to BUILD_BLOCK as playarea.

Return:
	True		Touch.
	False		No touch, OR fails.
--------------------------------------------------------------------*/
bool bblock_touchRight(const BUILD_BLOCK *block,  const BUILD_BLOCK *playarea)
{
	int i,j;
	int gx,gy;   /* playarea XY in grids/cubes, gx,gy MAY <0! invalid.  */
	int index;

	if(block==NULL || block->map==NULL || playarea==NULL || playarea->map==NULL)
		return false;

	gx = block->x0/cube_ssize;
	gy = block->y0/cube_ssize;

	/* Check if nearby DOWN(right) cube is ALSO occupied by playarea cubeimg */
	for(i=block->h; i>=0; i--) {
	   for(j=0; j< block->w; j++) {
		/* ONLY if current block grid/map has cubeimg. */
		if( block->map[i*block->w +j] ) {
			/* If nearby DOWN(right) cube is ALSO occupied by playarea cubeimg */
			index = (gy+i +1)*playarea->w + gx+j; /* gy+i +1 nearby_DOWN(right) */
			/* (gy+i+1) MAY <0! and index MAY <0!  */
			if( gy+i+1 > -1 && index >-1 && playarea->map[index] )  /* index MAY <0 */
				return true;
			/* ELSE: touches DOWN(right) side limit of PLAYAREA */
			else if( gy+i >= playarea->h-1 )
				return true;
		}
	   }
	}

	return false;
}


/*-------------------------------------------------------------------
Shift a forward moving bblock one cube/grid down(down to the ground).

@block: 	Pointer to BUILD_BLOCK
@playarea:	Pointer to BUILD_BLOCK as playarea.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------*/
int bblock_shiftDown(BUILD_BLOCK *block,  const BUILD_BLOCK *playarea)
{
	/* OK, check input params in bblock_touchDown() */

	if( bblock_touchDown(block, playarea) ) {
/* TEST: ----------- */
		if(block->type == SHAPE_I) {
			egi_dpstd(" --- Touch down! ---\n");
			getchar();
		}

		return -1;
	}
	else
		block->x0 += cube_ssize;

	return 0;
}


/*-------------------------------------------------------------------
Shift a forward moving bblock one cube/grid LEFT.
(Left side: relative to the forward moving direction )

@block: 	Pointer to BUILD_BLOCK
@playarea:	Pointer to BUILD_BLOCK as playarea.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------*/
int bblock_shiftLeft(BUILD_BLOCK *block,  const BUILD_BLOCK *playarea)
{
	/* OK, check input params in bblock_touchLeft() */

	if( bblock_touchLeft(block, playarea) )
		return -1;
	else
		block->y0 -= cube_ssize;

	return 0;
}


/*-------------------------------------------------------------------
Shift a forward moving bblock one cube/grid RIGHT.
(Left side: relative to the forward moving direction )

@block: 	Pointer to BUILD_BLOCK
@playarea:	Pointer to BUILD_BLOCK as playarea.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------*/
int bblock_shiftRight(BUILD_BLOCK *block,  const BUILD_BLOCK *playarea)
{
	/* OK, check input params in bblock_touchRigth() */

	if( bblock_touchRight(block, playarea) )
		return -1;
	else
		block->y0 += cube_ssize;

	return 0;
}

/*---------------------------------------------
Rotate a building_block 90Deg clockwise.
It auto. adjust left/right(UP/DOWN) a little.


@block:	Pointer to BUILD_BLOCK
@playarea:	Pointer to BUILD_BLOCK as playarea.

Return:
	0	OK
	<0	Fails
----------------------------------------------*/
int bblock_rotate_c90(BUILD_BLOCK *block, const BUILD_BLOCK *playarea)
{
	int i,j;
	bool *tmpmap=NULL;

	if( block==NULL || block->map ==NULL)
		return -1;

	/* No need to rotate SHAPE_O */
	if(block->type == SHAPE_O)
		return 0;

	/* Create tmpmap */
	tmpmap=calloc(1, block->w*block->h*sizeof(bool)); //sizeof(typeof(*block->map)));
	if(block->map==NULL)
		return -1;

	/* Map block->map 90deg clockwise to tmpblock->map */
	for(i=0; i< block->h; i++) {		/* Here h/w is OLD */
	   for(j=0; j< block->w; j++) {
		/* new_w = old_h; new_h=old_w */
		/* i maps to new_w-1-i as new_X, j maps to new_h as new_Y */
		tmpmap[j*block->h + block->h-1-i]= block->map[i*block->w +j];
	   }
	}

	/* Map: x0 ->  x0+(old_w-old_h)/2;  y0 -> y0-(old_w-old_h)/2 */
	block->x0 += (block->w-block->h)/2*cube_ssize;    /* Here h/w is OLD */
	block->y0 -= (block->w-block->h)/2*cube_ssize;

	/* AT LAST:  swap w/h */
	int tmpw=block->w;
	block->w=block->h;
	block->h = tmpw;

	/* Free/Swap map */
	free(block->map);
	block->map=tmpmap;

	return 0;
}


/*--------------------------------------------------------
If a bblock hits ground or other idel bblocks. then it will
be recorded in playarea->map[] and be part of playarea.
A bblock should be destroyed after being recorded.

@block: 	Pointer to BUILD_BLOCK
@playarea:	Pointer to BUILD_BLOCK as playarea.

Return:
	0	OK
	-1	GAME_OVER.
---------------------------------------------------------*/
int  record_bblock(const BUILD_BLOCK *block,  BUILD_BLOCK *playarea)
{
	int i, j;
	int gx,gy;  /* playarea XY in grids/cubes. gx,gy MAY <0! invalid */
	int index;

	if(block==NULL || block->map==NULL || playarea==NULL || playarea->map==NULL)
		return 0;

	gx = block->x0/cube_ssize;
	gy = block->y0/cube_ssize;

	for(i=0; i< block->h; i++) {
	   for(j=0; j< block->w; j++) {
		/* ONLY copy block->map[] with value TURE!
		 * DO NOT copy FALSE! In case playarea->map[] is TRUE there, it will then clear it mistakely.
		 */
		if( block->map[i*block->w +j] ) {
			index = (gy+i)*playarea->w+ gx+j;
			/* (gx+j) MAY <0! and index MAY <0!   !!!!We ignore to check index > MAX limit here!!! */
			if( gx+j> -1 && index > -1 )
			      playarea->map[ index ] = true;
		}
	   }
	}

	/* Check if the right_most cube of the recorded bblock is behind the starting line : GAME OVER! */
	if( gx+block->w-1 < 0)  /* --- GAME OVER! --- */
		return -1;

	return 0;
}

/*-------------------------------------------------
Check if there is any complete lines
in the playarea.

@playarea:	Pointer to BUILD_BLOCK as playarea.
@results:	An array of BOOLs to record resutls.
		If column/bar X is complete, then
		set results[X]=true.

		Caller to enuser space of resutls[]
		to be at least playarea->w *bool.

Return:
	>=0	OK, number of complete lines
	<0	Fails.
--------------------------------------------------*/
int check_OKlines(BUILD_BLOCK *playarea, bool *results)
{
	int i,j,k;
	int cnt;
	if( playarea==NULL || playarea->map==NULL || results==NULL )
		return -1;

	cnt =0;
	for( i=0; i< playarea->w; i++ ) {
	   results[i]=true; /* preset as true */
	   for(j=0; j< playarea->h; j++) {
		if( playarea->map[j*playarea->w +i] != true) {
			results[i]=false;
			break;
		}
	   }

	   /* NOW: resutls[i] is TRUE */
	   if(results[i]) {
	  	cnt++;
/* TEST: -------- draw the complete line with highlighted cubeimg */
	   	for(k=0; k< playarea->h; k++)
		   egi_subimg_writeFB(hcimg, &workfb, 0, -1, cube_ssize*i, cube_ssize*k);   /* imgbuf, fb, subcolor, x0,y0 */
	    }

	}

	return cnt;
}


/*----------------------------------------------------
Remove any complete lines in the playarea
in the playarea.

@playarea:	Pointer to BUILD_BLOCK as playarea.
@results:	An array of BOOLs to record resutls.
		If column/line X is complete, then
		results[x]==ture.
		results[] to be cleared at the end of
		the function.

		Caller to enuser space of resutls[]
		to be at least playarea->w *bool.


Return:
	>=0	OK, number of complete lines
	<0	Fails.
-----------------------------------------------------*/
int remove_OKlines(BUILD_BLOCK *playarea, bool *results)
{
	int i,j,k;
	bool *tmpmap;

	if( playarea==NULL || playarea->map==NULL || results==NULL )
		return -1;

	/* Create tmpmap */
	tmpmap=calloc(1, playarea->w*playarea->h*sizeof(bool));
	if(tmpmap==NULL)
		return -1;

	/* Copy playarea->map[] to tmpmap->map[], skip OKlines! */
	for(i=0; i< playarea->h; i++ ) {
	   k=playarea->w-1; /* For tmpmap */
	   /* Tanverse from bottm to top! skip OKlines */
	   for( j=playarea->w-1; j>=0; j--) {
		if(results[j]==false) {
			/* tmpmap Keeps same h, not same k != j  */
		 	tmpmap[i*playarea->w+k]=playarea->map[i*playarea->w+j];
			k--;  /* Copy ONLY false result lines. */
		}
	   }
	}

	/* Free and Swap map */
	free(playarea->map);
	playarea->map=tmpmap;

	/* Clear results */
	for(i=0; i< playarea->w; i++)
		results[i]=false;

	/* Reutrn number of finished lines */
	return  k+1;
}


/*--------------------------------
Write 'GAVE OVER' on the playarea.
--------------------------------*/
void game_over(void)
{
        FTsymbol_uft8strings_writeFB(   &workfb, egi_sysfonts.bold,       /* FBdev, fontface */
                                        30, 30,(const unsigned char *)"GAME OVER",      /* fw,fh, pstr */
                                        320, 1, 0,                      /* pixpl, lines, fgap */
                                        50, 50,                         /* x0,y0, */
                                        WEGI_COLOR_RED, -1, 200,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
}


/*--------------------------------
Write 'GAVE OVER' on the playarea.
--------------------------------*/
void game_pause(void)
{
        FTsymbol_uft8strings_writeFB(   &workfb, egi_sysfonts.bold,       /* FBdev, fontface */
                                        30, 30,(const unsigned char *)"GAME PAUSE",      /* fw,fh, pstr */
                                        320, 1, 0,                      /* pixpl, lines, fgap */
                                        50, 50,                         /* x0,y0, */
                                        WEGI_COLOR_RED, -1, 200,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
}
