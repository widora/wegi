/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is for testing EGI only!!!

			!!! --- WARNING --- !!!

Since Game,Name and Trademark of 'Tetris' are all registered, you shall
obtain the License/Agreement from the property Owner before you issue
the game!



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
			it may succeed to drop down (free fall) again.


		--- ViewPoint and Direction  ---

UP/DOWN			From the viewpoint of the player.
left/right/down		From the viewpoint of the forward moving bblock.


		----  GamePad Controls ----

Left/Right (ABS_X)      	Shift block Left/Right.  Right -> Forward
Up/Down	   (ABS_Y)      	Shift block Up/Down.

 Note:  Every pressing of an ABS key will move the playblock by one cube,
   	If you keep pressing the key, ONLY AFTER a short while will it start
	to repeat moving on...


X	   (KEY_D 32)
Y	   (KEY_H 35)
A 	   (KEY_F 33)        	Rotate block 90Deg clockwise.
B	   (KEY_G 34)
L	   (KEY_J 36)
R	   (KEY_K 37)
SELECT	   (KEY_APOSTROPHE 40)
START      (KEY_GRAVE 41)	Pause/Play.



Note:
1. Because of rendering speed limit, what you see on the screen actually lags behind
   what is happening at the time. So if the speed level is high, you would have to
   operate GamePad in advance.
   Example: If you press key to shift the bblock just at the sight of touchDown, it may
   fails! because at that time bblock_waitrecord time may already pasted!

2. If a playblock touches with the left(UP) sideline of the PLAYAREA, it CAN still rotate anyway!
   however, if it touches with the right(DOWN) sideline of the PLAYAREA, it usually CAN NOT!
   This is because the rotation center is set at its right(UP) end!
	( right(UP):    right --- From the viewpoint of the forward moving bblock.
	  	       	   UP --- From the viewpoint of the player. )

TODO:
0. XXX At begin of bblock falling, some position will out of range and cause GAEM_OVER.
1. ERING of mstat is rather slow, and it makes game response also slowly!
   XXX ---> OK. Call game_input(), to read GamePad events directly.
2. Render speed LIMIT!
   Surface image rendering lags behind data/variable updating!
   Press GamePad in advance with some predicition!
3. XXX Auto. rotate_adjust for bblock. --OK.
4. XXX After GAME_PAUSE for a certain long time, eclock status will NOT be __RUNNING!
   gettimeofday() error?  ---OK

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
	4. game_input(): to read kbd event dev directly.
2021-07-10:
	1. game_input(): Add ABS KEY repeating function.
2021-07-11:
	1. Add lab_scores to display SCORE.
	2. Adjust key repeat delay time.
2021-07-12:
	1. Add bblock_interfere().
	2. destroy_bblock() changes to bblock_destroy().
	3. Add bblock_rotateAdjust_c90().
2021-07-13:
	1. Test bblock_rotateAdjust_c90().
2021-07-14:
	1. Add Menu_Help surface and its functions.
2021-07-15:
	1. Add Menu_Option surface and its functions.
2021-07-16:
	1. Add create_cubeimg_lib(), cubeimg is pointer to cubeimg_lib[].
	2. End cmd_pause_game when surfshmem->usersig==1!

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


const char *pid_lock_file=NULL; //"/var/run/surf_tetris.pid";

#define  DIRECT_KBD_READ	/* Read kstat directly from event devs, instead of from ERING. */
#define  REPEAT_WAITMS	250	/* ABS_Key Repeat delay time, in ms */

/* For SURFUSER */
EGI_SURFUSER     *surfuser=NULL;
EGI_SURFSHMEM    *surfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
FBDEV            *vfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
EGI_IMGBUF       *surfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
SURF_COLOR_TYPE  colorType=SURF_RGB565;  /* surfuser->vfbdev color type */
EGI_16BIT_COLOR  bkgcolor;

/* SURF Menus */
const char *menu_names[] = { "File", "Option", "Help", "We"};
enum {
        MENU_FILE       =0,
        MENU_OPTION     =1,
        MENU_HELP       =2,
	MENU_WE		=3,
        MENU_MAX        =4, /* <--- Limit */
};
void menu_help(EGI_SURFUSER *surfuser);
void menu_option(EGI_SURFUSER *surfuser);
void menu_we(EGI_SURFUSER *surfcaller);

const char *about_we="Widora和它的小企鹅们, and EGI is a Linux/GNU player.";


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
const int 	cube_ssize=15;		/* Side size of the cubeimg, in pixels. */

enum {
	CTYPE_CLASSIC =0,	/* Type of cubeimg */
	CTYPE_ROUND   =1,
	CTYPE_FRAME   =2,	/* GREEN */
	CTYPE_FRAME2  =3,	/* OCEAN */
	CTYPE_FRAME3  =4,	/* PINK */
	CTYPE_MAX     =5
};
EGI_IMGBUF	*cubeimg_lib[CTYPE_MAX]; /* All cubeimgs available. select one to apply for cubeimg. */
unsigned long	ctype_index=0;		/* As index of cubeimg_lib[],
					 * To be selected/changed in MenuOption_mouse_event()
					 */
EGI_IMGBUF	*cubeimg;   		/* Pointer to an elementary cubic block, basic component to form a building_block.
					 * cubeimg = cubeimg_lib[ctype_index];
					 */
EGI_IMGBUF	*eraseimg;  		/* An imgbuf with same color as of PLAYAREA, use as an ERASER! */
EGI_IMGBUF 	*hcimg;			/* Highlighted cubeimg */
EGI_IMGBUF	*tmpimg;

typedef struct building_block {
	int x0, y0;	/* Origin X,Y under playarea coord. system. */
	int type;	/* Shape/Block type */
	int w;		/* Width and height of the bounding box of a building_block, as row/column number of cubes. */
	int h;
	bool *map;	/* cube map, size cw*ch,  1--a cube there; 0--No cube */
} BUILD_BLOCK;

int	mspeed=2;	/* Falling down speed: mpseed*cube_ssize per 1000ms */
int     fspeed;		/* Fast speed enabled by cmd_fast_down
			 * To substitue mspeed ONLY IF >0.
			 */
int	cnt_lines;	/* Counter for completed lines */
int	cnt_scores;	/* Counter for scores, NOW cnt_scores==cnt_lines */

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

//BUILD_BLOCK *shape_Z, *shape_S, *shape_J, *shape_L, *shape_O, *shape_T;
BUILD_BLOCK *playblock; /* The ONLY droping/moving block */
BUILD_BLOCK *playarea;	/* Map of PlayArea */

bool bblock_waitrecord;   /* The moving bblock touched down, waiting to record to playarea.
			   * However, it can still shift_left/right! If the bblock shifts to a position where it can
			   * freefall again, then bblock_waitrecord will reset to FALSE again!
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
int 	create_cubeimg_lib(void);
int 	create_hcimg(void);
int	create_eraseimg(void);

BUILD_BLOCK 	*create_bblock(int type, int x0, int y0);
void	bblock_destroy(BUILD_BLOCK **block);
void 	draw_bblock(FBDEV *fbdev, const BUILD_BLOCK *block, EGI_IMGBUF *cubeimg);
void 	erase_bblock(FBDEV *fbdev, const BUILD_BLOCK *block);

bool	bblock_touchDown(const BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
int	bblock_shiftDown(BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
bool	bblock_touchLeft(const BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
int 	bblock_shiftLeft(BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
bool	bblock_touchRight(const BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
int 	bblock_shiftRight(BUILD_BLOCK *block,  const BUILD_BLOCK *playarea);
int 	bblock_rotate_c90(BUILD_BLOCK *block); // const BUILD_BLOCK *playarea);
int 	bblock_rotateAdjust_c90(BUILD_BLOCK **pblock, const BUILD_BLOCK *playarea);
bool	bblock_interfere(const BUILD_BLOCK *block, const BUILD_BLOCK *playarea);

int 	record_bblock(const BUILD_BLOCK *block,  BUILD_BLOCK *playarea);
int 	check_OKlines(BUILD_BLOCK *playarea, bool *results);
int	remove_OKlines(BUILD_BLOCK *playarea, bool *results);

void	game_over(void);
void	game_pause(void);
void    game_input(void); /* Read and parse GamePad input */

ESURF_LABEL	*lab_score;		/* Label for displaying scores */


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
void my_close_surface(EGI_SURFUSER *surfuer);

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
	long long usec;
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

        /* SplitWord control */
        FTsymbol_disable_SplitWord();

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
	if( create_cubeimg_lib()!=0 || create_eraseimg()!=0 || create_hcimg()!=0 )
		exit(EXIT_FAILURE);

	/* P3. Create PLAYAREA */
	playarea=create_bblock(PLAYAREA_MAP, 0, 0);
	if(playarea==NULL)
		exit(EXIT_FAILURE);

START_TEST:
	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	sw=320;  sh=240;
	surfuser=egi_register_surfuser(ERING_PATH_SURFMAN, pid_lock_file, x0, y0, 320,240, sw, sh, colorType );
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
	//surfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	//surfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        surfshmem->close_surface 	= surfuser_close_surface;
	surfshmem->user_close_surface   = my_close_surface;
#ifndef DIRECT_KBD_READ
        surfshmem->user_mouse_event     = my_mouse_event;
#endif
        //surfshmem->draw_canvas          = my_draw_canvas;

	/* 3. Give a name for the surface. */
	strncpy(surfshmem->surfname, "游戏:俄罗斯方块", SURFNAME_MAX-1);

	/* 4. First draw surface  */
	surfshmem->bkgcolor=PLAYAREA_COLOR; //WEGI_COLOR_DARKGRAY; /* OR default BLACK */
	surfshmem->topmenu_bkgcolor=egi_color_random(color_light);
	surfshmem->topmenu_hltbkgcolor=WEGI_COLOR_GRAYA;
	surfuser_firstdraw_surface(surfuser, TOPBTN_CLOSE|TOPBTN_MIN, MENU_MAX, menu_names); /* Default firstdraw operation */

	/* 4A. Set menu_react functions */
	surfshmem->menu_react[MENU_HELP] = menu_help;
	surfshmem->menu_react[MENU_OPTION] = menu_option;
	surfshmem->menu_react[MENU_WE] = menu_we;

	/* font color */
	fcolor=COLOR_COMPLEMENT_16BITS(surfshmem->bkgcolor);

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Shmsize: %zdBytes  Geom: %dx%dx%dBpp  Origin at (%d,%d). \n",
			surfshmem->shmsize, surfshmem->vw, surfshmem->vh, surf_get_pixsize(colorType), surfshmem->x0, surfshmem->y0);

	/* XXX 5. Create SURFBTNs by blockcopy SURFACE image, after first_drawing the surface! */
	/* 5_1. Create LABEL SCORE */
	lab_score=egi_surfLab_create(surfimg, 10, SURF_TOPBAR_HEIGHT+SURF_TOPMENU_HEIGHT, /* imgbuf, xi, yi, x0, y0, w, h */
					      10, SURF_TOPBAR_HEIGHT+SURF_TOPMENU_HEIGHT, 100, 20 );
	egi_surfLab_updateText(lab_score, "Scores: 0");
	/* fbdev, lab, face, fw, fh, color, cx0, cy0 */
	egi_surfLab_writeFB(vfbdev, lab_score, egi_sysfonts.regular, 18,18, WEGI_COLOR_BLACK, 0,0);

        /* 5A. CopyBlock workfb IMGBUF (playarea imgbuf) to surfimg */
        egi_imgbuf_copyBlock( surfimg, workfb.virt_fb, false,   /* dest, src, blend_on */
                              workimg->width, workimg->height,  /* bw, bh */
			      (surfimg->width-PlayAreaW)/2, surfimg->height-5-PlayAreaH, 0, 0);    /* xd, yd, xs,ys */
	/* 5B. Draw a division line above playarea */
	fbset_color2(vfbdev,WEGI_COLOR_BLACK);
	draw_wline_nc(vfbdev, 5, 240-5-PlayAreaH-5, 320-5, 240-5-PlayAreaH-5, 3);
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

		/* M0. Set cubimg */
		cubeimg=cubeimg_lib[ ctype_index<CTYPE_MAX ? ctype_index:0 ];

		/* M1. Newborn playblock */
		if(playblock==NULL) {
			/* Note:
			 *  1. Only init. X <= -4*cube_ssize, then can check if touchs at start, GAME OVER!
			 *  2. MUST within PLAYAREA width.
			 *  3. MUST above start line! for we'll check touchDown there!
			 */
			playblock=create_bblock(mat_random_range(SHAPE_MAX), -4*cube_ssize, mat_random_range(7)*cube_ssize ); /* 7=10-4+1 */
			//playblock=create_bblock(5+mat_random_range(2), -4*cube_ssize, mat_random_range(7)*cube_ssize ); /* 7=10-4+1 */
			//playblock=create_bblock(SHAPE_I, -4*cube_ssize, mat_random_range(7)*cube_ssize ); /* 7=10-4+1 */
			if(playblock==NULL) {
				egi_dperr("Fail to create BBLOCK!");
				exit(-1);
			}

			printf("Create playblock shape=%d, x0=%d\n", playblock->type, playblock->x0);
		}

		/* M2. If any motion done for the playblock, redraw to workfb and update workimg. */
		if(motion_status.motions) {
			/* M2.1 Update/Draw PLAYAREA.
			   ...NOT necessary, BUT we need to refresh playarea when ctype_index is updated. */
			draw_bblock(&workfb, playarea, cubeimg);

			/* M2.2 Draw playblock as position renewed */
			draw_bblock(&workfb, playblock, cubeimg);

			/* M2.3  ***** Refresh workimg/workFB to surfimg. *****
			 * ONLY if surfimg changes:
			 *   1) The bblock position changed.
			 */
			refresh_workimg();

			/* Restr eclock, for shift_dx==true here, For visual effect! --- */
			if(motion_status.motion_bits.shift_dx==true)
				egi_clock_restart(&eclock);

			/* XXX Reset all motion tokens!
			   XXX move to before parsing user commands, we need motions tokens before that.*/
			// motion_status.motions=0;

		} /* End if(motion_status.motions) */
		else	{  /* Makeup with/redraw playblock image as just erased in M7, to keep workfb holding a complete image. */
			draw_bblock(&workfb, playblock, cubeimg);
		}

		/* M3. Check game_pause key */
		if( cmd_pause_game ) {
			/* Backup workimg */
			tmpimg = egi_imgbuf_blockCopy( workimg, 0, 0, workimg->height, workimg->width);

			game_pause();
			refresh_workimg();
			while( cmd_pause_game ) {
				usleep(500000);

				/* If surfuser to exit! */
				if(surfshmem->usersig==1)
					cmd_pause_game=false;

#ifdef DIRECT_KBD_READ
				game_input();
#endif
				if(eclock.status != ECLOCK_STATUS_RUNNING)
					printf("game_pause: eclock.status=%d!\n", eclock.status);
			};

			/* Restore workimg */
			egi_imgbuf_copyBlock( workimg, tmpimg, false, workimg->width, workimg->height, 0,0,0,0);
			egi_imgbuf_free2(&tmpimg);
			refresh_workimg();
		}

		/* M4. Leave a BLANK here...... */

		/* M5A.  If the playblock slips away and starts to fall again during waitrecord,
		 *        ONLY if a motion is just done..., motion tokens cleared at M8.  */
		if( bblock_waitrecord && motion_status.motions && bblock_touchDown(playblock, playarea)==false ) {
			bblock_waitrecord = false;
			printf("Slip to fall...\n");
		}
		/* M5B.  Check if it hits ground or touches other recorded blocks in playarea! */
		else if( bblock_waitrecord || bblock_touchDown(playblock,  playarea) ) {
//			printf("Bblock touchs down!\n");  /* It will keep checking during waitrecord time!!! */
			/* Wait for record to playarea->map! The player has the last chance to move it left/right(UP/DOWN)! */
			if( bblock_waitrecord
			    && egi_clock_peekCostUsec(&eclock) >= ( fspeed>0 ? 1000/fspeed*1000 : 1000/mspeed*1000 ) ) {
				/* Record bblock to PLAYAREA */
				if( record_bblock(playblock, playarea) == -1) {  /* -1 GAME_OVER */
					printf(" --- GAME OVER --- \n");

					/* Give some hint for GAME OVER */
					game_over();
					refresh_workimg(); /* With last score value displaying */
					sleep(3);

					/* Clear whole workimg(playarea) */
					egi_imgbuf_resetColorAlpha(workimg, PLAYAREA_COLOR, -1);
					refresh_workimg();
					/* Clear playarea map */
					memset(playarea->map, 0, playarea->w*playarea->h*sizeof(typeof(*playarea->map)));

					/* Clear score value, and refresh lab_score on surfimg  */
					cnt_scores = 0;
					egi_surfLab_updateText(lab_score, "Scores: 0");
					/* fbdev, lab, face, fw, fh, color, cx0, cy0 */
					egi_surfLab_writeFB(vfbdev, lab_score, egi_sysfonts.regular, 18,18, WEGI_COLOR_BLACK, 0,0);
				}

				/* Destroy the bblock. */
				printf("Record and destroy playblock.\n");
				bblock_destroy(&playblock);

				/* Reset waitrecord */
				bblock_waitrecord=false;

				/* Restart eclock */
				egi_clock_restart(&eclock);

/* <<< -------------- Loop back -------- */
				continue;
			}
			else  if(!bblock_waitrecord)  {  /* Just done checkDown  */
				bblock_waitrecord=true;
				egi_dpstd("BBlock just touchs down!\n");
				//egi_dpstd("Wait for user input.... playblock->x0=%d*cubes\n", playblock->x0/cube_ssize);
			}
			else if(bblock_waitrecord) {
				//egi_dpstd("Bblock waitrecord...\n");
			}
		}

		/* M6. Check playarea->map[] results and remove completed lines.   */
		if( bblock_waitrecord==false ) {

		    if( (ret=check_OKlines(playarea, finish_lines)) >0 ) {  /*  check_OKlines() draw highlighted lines */
			printf("---> %d complete lines <---\n", ret);

			/* Update lab_score */
			cnt_scores += ret;
			//snprintf(lab_score->text, ESURF_LABTEXT_MAX-1, "Scores: %d", cnt_scores);
			egi_surfLab_updateText(lab_score, "Scores: %d", cnt_scores);
			/* fbdev, lab, face, fw, fh, color, cx0, cy0 */
			egi_surfLab_writeFB(vfbdev, lab_score, egi_sysfonts.regular, 18,18, WEGI_COLOR_BLACK, 0,0);

			/* Refresh workimg with highlighted lines. */
			refresh_workimg();
			usleep(500000);

			/* Remove completed lines from playarea */
			remove_OKlines(playarea, finish_lines);

			/* MUST Clear whole workimg(playarea) after remove_OKlines!
			 * draw_bblock() ONLY update cubeimgs where map[] is TURE!
			 */
			egi_imgbuf_resetColorAlpha(workimg, PLAYAREA_COLOR, -1);
		    }
		}

		/* NOW: playblock != NULL */

		/* M7. Erase old playblock image,  position data updated after motions!
		 *     If no motion happens later, then redraw to make it up, see at M2 else{ }.
	 	 */
		erase_bblock(&workfb, playblock);

		/* M8. Here reset all motion tokens, just before game_input() and parsing user commands. */
		motion_status.motions=0;

		/* M9. Read and parse GamePad input */
#ifdef DIRECT_KBD_READ
		game_input();
#endif

		/* M10. Parse user command, move OR Rotate bblock  */
		/* M10.1 Rotate:  Check if space is enough to accommodate rotated bblock...HERE! */
		if(cmd_rotate_c90) {
			//if( bblock_rotate_c90(playblock, NULL)==0 )
			if( bblock_rotateAdjust_c90(&playblock, playarea)==0 )
				motion_status.motion_bits.rotate_ddeg=true;

			cmd_rotate_c90 = false;
		}
		/* M10.2 Move playblock: Check touch_down laster... */
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
			fspeed = 20; /* 10 */
			cmd_fast_down = false;
		}
		/* M10.3 AT LAST, Move bblock forward at specified time interval.  FastSpeed or NormalSpeed */
		usec=egi_clock_peekCostUsec(&eclock);
		if( usec >= ( fspeed>0 ? 1000/fspeed*1000 : 1000/mspeed*1000 ) ) {
			if( !bblock_waitrecord ) {
				if( bblock_shiftDown(playblock, playarea)==0 ) {
					printf(" Set motion shift_dx.\n");
					motion_status.motion_bits.shift_dx=true;
				}
			}

			fspeed=0;
			//egi_clock_restart(&eclock); /* For visual effect, restart eclock after update surimg */
		}
		else if (usec>0) {
			//printf("Wait for shiftDown, usec=%ld\n", usec);
		}
		else { /* usec<0 */
			printf("usec<0! _peekCostUsec error! eclock status=%d, usec=%lld\n", eclock.status, usec);
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
4. Some vars of pmostat->conkeys will NOT auto. reset!
--------------------------------------------------------------------*/
void my_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	int k;
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
	/* NOTE: pmostat->chars[] is read from ssh STDIN !!! NOT APPLICABLE here for GamePad !!! */


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
Create cubeimg_lib[].

Return:
	0	OK
	<0	Fails
---------------------------*/
int  create_cubeimg_lib(void)
{
	int i;
	FBDEV vfb={0};

	/* Create cubeimgs */
	for(i=0; i<CTYPE_MAX; i++) {
		/* Prepare cubeimg with BKG color */
		switch(i) {
			case CTYPE_CLASSIC:
				cubeimg_lib[i]=egi_imgbuf_createWithoutAlpha(cube_ssize, cube_ssize, WEGI_COLOR_BLACK);
				break;
			case CTYPE_ROUND:
				cubeimg_lib[i]=egi_imgbuf_createWithoutAlpha(cube_ssize, cube_ssize, PLAYAREA_COLOR);
				break;
			case CTYPE_FRAME:
				cubeimg_lib[i]=egi_imgbuf_createWithoutAlpha(cube_ssize, cube_ssize, WEGI_COLOR_DARKGREEN);
				break;
			case CTYPE_FRAME2:
				cubeimg_lib[i]=egi_imgbuf_createWithoutAlpha(cube_ssize, cube_ssize, WEGI_COLOR_OCEAN);
				break;
			case CTYPE_FRAME3:
				cubeimg_lib[i]=egi_imgbuf_createWithoutAlpha(cube_ssize, cube_ssize, WEGI_COLOR_PINK);
				break;
		}
		if(cubeimg_lib[i]==NULL)
			return -1;

		/* Init. virt fbdev */
	  	if( init_virt_fbdev(&vfb, cubeimg_lib[i], NULL)!=0 ) {
			egi_imgbuf_free2(&cubeimg_lib[i]);
			return -2;
		}
		vfb.pixcolor_on=true;

		/* Draw cubeimg */
		switch(i) {
			case CTYPE_CLASSIC:	/* A Square Shape */
				fbset_color2(&vfb, PLAYAREA_COLOR);
				draw_rect(&vfb, 0, 0, cube_ssize-1, cube_ssize-1);
				draw_wrect(&vfb, 3, 3, cube_ssize-1-3, cube_ssize-1-3, 2);
				break;
			case CTYPE_ROUND: /* A Round Shape */
				fbset_color2(&vfb, WEGI_COLOR_DARKPURPLE); //LTSKIN);
				draw_filled_circle(&vfb, cube_ssize/2, cube_ssize/2, cube_ssize/2+1);
				break;
			case CTYPE_FRAME: /* A Frame Shape */
				/* dev, type, color, x0,y0, width,height, w */
				draw_button_frame(&vfb, 1, WEGI_COLOR_DARKGREEN, 0,0, cube_ssize, cube_ssize, 3);
				break;
			case CTYPE_FRAME2: /* A Frame Shape */
				/* dev, type, color, x0,y0, width,height, w */
				draw_button_frame(&vfb, 1, WEGI_COLOR_OCEAN, 0,0, cube_ssize, cube_ssize, 3);
				break;
			case CTYPE_FRAME3: /* A Frame Shape */
				/* dev, type, color, x0,y0, width,height, w */
				draw_button_frame(&vfb, 1, WEGI_COLOR_PINK, 0,0, cube_ssize, cube_ssize, 3);
				break;
		}
	}

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
	else {
		/* Assign other members */
		block->type=type;
		block->x0=x0;
		block->y0=y0;
	}

	return block;
}

/*---------------------------------------------
Destroy/Free an BUILD_BLOCK
----------------------------------------------*/
void bblock_destroy(BUILD_BLOCK **block)
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

	/* XXX already checked as above */
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
		/* !!! ONLY if current block grid/map has cubeimg, for all following if()s */
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
	//for(i=block->h; i>=0; i--) {
	for(i=block->h-1; i>=0; i--) {
	   for(j=0; j< block->w; j++) {
		/* !!! ONLY if current block grid/map has cubeimg, for all following if()s */
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
	/* OK, Input params checked in bblock_touchDown() */

	if( bblock_touchDown(block, playarea) )
		return -1;
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
	/* OK, Input params checked in bblock_touchLeft() */

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
	/* OK, Input params checked in bblock_touchRight() */

	if( bblock_touchRight(block, playarea) )
		return -1;
	else
		block->y0 += cube_ssize;

	return 0;
}

/*-----------------------------------------------------------------
Check if bblock interferes with playarea, that their cubeimgs are
located at the same cube grid of playarea.

@block:		Pointer to BUILD_BLOCK
@playarea:	Pointer to BUILD_BLOCK as playarea.

Return:
	True	Interfered
	False	Not interfered
------------------------------------------------------------------*/
bool bblock_interfere(const BUILD_BLOCK *block, const BUILD_BLOCK *playarea)
{
	int i,j;
	int gx,gy;   /* playarea XY in grids/cubes, gx,gy MAY <0! invalid.  */
	int index;

	if(block==NULL || block->map==NULL || playarea==NULL || playarea->map==NULL)
		return false;

	gx = block->x0/cube_ssize;
	gy = block->y0/cube_ssize;

	for(i=block->h-1; i>=0; i--) {
	   for(j=0; j< block->w; j++) {
		/* !!! ONLY if current block grid/map has cubeimg, for all following if()s */
		if( block->map[i*block->w +j] ) {

		   	/* 1. Out of two side lines of playrea. */
	   		if( gy+i <0 || gy+i >playarea->h-1 )
	  			return true;
			/* 2. Out of playarea bottom line!  */
			if( gx+j > playarea->w-1 )
				return true;
			/* 3. Interfere with cubeimg of playarea */
			/* index of  playarea->map[index] */
			index = (gy+i)*playarea->w + gx+j;
			if( playarea->map[index] )
				return true;
		}
	   }
	}

	return false;
}


/*---------------------------------------------
Rotate a building_block 90Deg clockwise.
It auto. adjust left/right(UP/DOWN) a little.


@block:	Pointer to BUILD_BLOCK
//@playarea:	Pointer to BUILD_BLOCK as playarea.

Return:
	0	OK
	<0	Fails
----------------------------------------------*/
int bblock_rotate_c90(BUILD_BLOCK *block)
{
	int i,j;
	bool *tmpmap=NULL;

	if( block==NULL || block->map==NULL)
		return -1;

	/* No need to rotate SHAPE_O */
	if(block->type == SHAPE_O)
		return 0;

	/* Create tmpmap */
	tmpmap=calloc(1, block->w*block->h*sizeof(typeof(*block->map)));
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


/*---------------------------------------------
Rotate a building_block 90Deg clockwise and
check if it interferes with playarea cubeimgs.
If so, cancel rotation operation, otherwise keep
as rotated.

@pblock:	PPointer to BUILD_BLOCK
@playarea:	Pointer to BUILD_BLOCK as playarea.

Return:
	0	OK, block updated.
	<0	Fails
----------------------------------------------*/
int bblock_rotateAdjust_c90(BUILD_BLOCK **pblock, const BUILD_BLOCK *playarea)
{
	int i;

	BUILD_BLOCK *cpyblock;
	BUILD_BLOCK *block;

	if(pblock==NULL || *pblock==NULL)
		return -1;

	block=*pblock;

	if(block==NULL || block->map==NULL || playarea==NULL || playarea->map==NULL)
		return -1;

	/* Create cpyblock and copy data */
	cpyblock=create_bblock( block->type, block->x0, block->y0);
	if(cpyblock==NULL)
		return -1;

	/* Copy w,h, as it may NOT be upright!!! */
	cpyblock->w = block->w;  cpyblock->h = block->h;
	/* Copy block->map[]*/
	memcpy(cpyblock->map, block->map, block->w*block->h*sizeof(typeof(*block->map)));

	/* Rotate block */
	if( bblock_rotate_c90(block)!=0 ) {
		bblock_destroy(&cpyblock);
		return -2;
	}

	/* NOW: block has been rotated c90 */

#if 0 /* ------ ALGRITHM 1:  Give up if interferes -------- */

	/* Check interference for rotated bblock */
	if( bblock_interfere(block, playarea) ) {
		/* Destroy rotated block */
		bblock_destroy(pblock);
		/* Restore to old block data */
		*pblock=cpyblock;
		//egi_dpstd("Bblock interferes ----- xxxxx ------\n");
		return -3;
	}
	else  {  /* Destroy cpyblock */
		bblock_destroy(&cpyblock);
		//egi_dpstd("Bblock NOT interferes ---- OOOO -----.\n");
		return 0;
	}

#else /* ------- ALGRITHM_2:  Try to adjust UP/DOWN for a suitable place. ----- */

	/* Move UP to adjust: 0 -> h */
	for(i=0; i< block->h+1; i++ ) {
		/* Start with i=0 */
		if( bblock_interfere(block, playarea)==false ) {
			bblock_destroy(&cpyblock);
			return 0;
		}
		block->y0 += cube_ssize;
	}
	/* UP_adjust fails! Restore original block->y0 */
	block->y0 -= (block->h+1)*cube_ssize;

	/* Move DOWN to adjust: -1 -> -h,  !!! as i=0 tested before. */
	for(i=1; i< block->h+1; i++ ) {
		/* Start with i=1 */
		block->y0 -= cube_ssize;
		if( bblock_interfere(block, playarea)==false ) {
			bblock_destroy(&cpyblock);
			return 0;
		}
	}
	/* DOWN_adjust ALSO fails! */

	/* Destroy rotated block */
	bblock_destroy(pblock);

	/* Restore old block */
	*pblock=cpyblock;

	return -3;

#endif

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


/*----------------------------------------------------------------------------------------
Read user input from GamePad and set command variables of cmd_xxx_xxx.

Note:
1. The USB GamePad may accidentally disconnect then connect again,
   in such case, conkeys.abskey is NOT reset to ABS_MAX, and the disconnection
   will trigger releasing signal for all ABS_keys, with conkeys.absvalue == 0x7F.
   then ewatch will be stop, see at 3.2.

2.                        --- CAUTION ---
   If you press/release a button too quickly, one round of _read_kbdcode() may return both keycodes!
   Example: keycode[0] as press, and keycode[1] as release...
   if so, the last value of kstat.conkeys.press_lastkey will be FALSE(release) instead of TRUE(press)
   In some cases, it MAY return 3 keycode[] even!!!
   So it's better to keep pressing for a short while till the press_TURE triggers a reaction.

   KEY_GRAVE (41)
	egi_read_kbdcode(): keycode[0]:41
	egi_read_kbdcode(): Release lastkey=41
	egi_read_kbdcode(): keycode[0]:41   <--------- readkbkd returns 2 results, as press and release.
	egi_read_kbdcode(): keycode[1]:41
	egi_read_kbdcode(): keycode[0]:41
	egi_read_kbdcode(): keycode[1]:41

3.		---- ABS_Key Repeat Delay Function  ---
  If you press an ABS key and keep holding down, the dv value will first be read out at point of pressing,
  then it shall wait for a short while before it sets enable_repeat=true and repeats reading out dv values.
  The repeating speed is NOT controlled here.

-----------------------------------------------------------------------------------------*/
void  game_input(void)
{
	static EGI_WATCH ewatch={0};	    /* A time watch */
	static bool	enable_repeat=true; /* Enable repeat reading ABS_KEY status
					     * 1. Init as TRUE, to read out at the first time.
					     * 2. Set to FALSE at pressing the key then, see 3.1. (Just after first read)
					     * 3. Reset to TRUE after keep pressing for a while, see 3.3.
					     * 4. Reset to TRUE at releasing the key, see 3.2.
					     */
	/* KBD GamePad status */
	static EGI_KBD_STATUS kstat={ .conkeys={.abskey=ABS_MAX} };  /* 0-->ABS_X ! */;

	int dv=0;  /* delt value for ABS value */

	/* 1. Read event devs */
 	if( egi_read_kbdcode(&kstat, NULL) )
		return;

	 /* absvalue: 0: ABS_X, 1: ABS_Y, ABS_MAX(0x3F): IDLE */

	/* 2. Convert GamePad ABS_X/Y value to [1 -1] */
	dv=( kstat.conkeys.absvalue -127)>>7; /* NOW: abs value [1 -1] and same as FB Coord. */
	if(dv) {
	   //egi_dpstd("abskey code: %d ( 0:ABS_X, 1:ABS_Y, 63:IDLE) \n", kstat.conkeys.abskey);
	   switch( kstat.conkeys.abskey ) {
        	case ABS_X:
                	printf("ABS_X: %d\n", dv); /* NOW: abs value [1 -1] and same as FB Coord. */
			if( enable_repeat && dv>0 ) {
				cmd_fast_down=true;
			}

                        break;
                case ABS_Y:
                        printf("ABS_Y: %d\n", dv);
			if( enable_repeat && dv<0)
				cmd_shift_left=true;
			if( enable_repeat && dv>0)
				cmd_shift_right=true;
                        break;
                default:
                        break;
	   }
        }

	/* NOTE:			---- ABS_key repeat delay ---
	 * If you press an ABS key and keep holding down, the dv value will be read out at point of pressing (as above),
	 * then it shall wait for a short while, before set enable_repeat as true and repeat reading out dv values.
	 * the repeating speed is NOT controlled here.
	 */

	/* 3. If ABS key is NOT idle. < ABS_MAX means abskey is IDLE > */
	if( kstat.conkeys.abskey !=ABS_MAX ) {
		/* 3.1 At point of pressing the ABS_KEY */
		if( ewatch.status!=ECLOCK_STATUS_RUNNING && kstat.conkeys.press_abskey ) {
			/* Disable repeating press_abskey at first... */
			enable_repeat = false;

			printf(" Restart ewatch...\n");
			egi_clock_restart(&ewatch);
		}
		/* 3.2 At piont of releasing the ABS_KEY */
		else if( kstat.conkeys.absvalue == 0x7F ){  /* absvalue 7F as release sig of a GamePad ABS key. */
			printf(" Stop ewatch...\n");
			egi_clock_stop(&ewatch);

			/* Reset abskey as idle */
			kstat.conkeys.abskey =ABS_MAX;

			/* Enable key repeating, to enable next fisrt read out. */
			enable_repeat=true;
		}
		/* 3.3  After keep holding down for a while */
		else {
			//egi_dpstd("Hold down abskey...\n");
			if( egi_clock_peekCostUsec(&ewatch) > REPEAT_WAITMS*1000 ) {
				/* Enable repeating press_abskey NOW! */
				printf("ABS key value Repeating...\n");
				enable_repeat = true;
			}
		}
	}


	/* 4. GamePad control keys(non_abskey). lastkey MAY NOT cleared yet! Check press_lastkey!! */
	if( kstat.conkeys.press_lastkey ) {
		egi_dpstd("lastkey code: %d\n", kstat.conkeys.lastkey);
		switch(kstat.conkeys.lastkey) {
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

	/* 5. Reset lastkey/abskey keys if necessary, otherwise their values are remained in kstat.conkey. */
	/* 5.1 Reset lastkey: Let every EV_KEY read ONLY once! */
       	kstat.conkeys.lastkey =0;       /* Note: KEY_RESERVED ==0. */

      	/* 5.2 Reset abskey XXX NOPE, we need ABSKEY repeating function. See enbale_repeat. */
	//if( kstat.conkeys.press_abskey )
	//	kstat.conkeys.press_abskey=false;
	//kstat.conkeys.abskey =ABS_MAX; /* NOTE: ABS_X==0!, use ABS_MAX as idle. */

}


/*=====================================
	Surface :: Menu_Help
======================================*/

const char *help_descript = "    < EGI桌面小游戏 >\n\
This program(codes) is under license of GNU GPL v2.\n\
Enjoy!";

/*----------------------------------------------------------
To create a SURFACE for menu help.

@surfcaller:	The caller, as a surfuser.

Note:
1. If it's called in mouse event function, DO NOT forget to
   unlock shmem_mutex.
----------------------------------------------------------*/
void menu_help(EGI_SURFUSER *surfcaller)
{
  /* Surface size */
  int msw=230;
  int msh=160;

  /* Origin coordinate relative to the surfcaller */
  int x0=50;
  int y0=50;


	/* Ref. pointers */
	EGI_SURFUSER     *msurfuser=NULL;
	EGI_SURFSHMEM    *msurfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
	FBDEV            *mvfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
	//EGI_IMGBUF       *msurfimg=NULL;        /* Only a ref. to surfuser->imgbuf */
	SURF_COLOR_TYPE  mcolorType=SURF_RGB565;  /* surfuser->vfbdev color type */
	//EGI_16BIT_COLOR  mbkgcolor;

	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	msurfuser=egi_register_surfuser( ERING_PATH_SURFMAN, NULL,
					 surfcaller->surfshmem->x0+x0, surfcaller->surfshmem->y0+y0,  /* x0,y0 */
					 msw, msh, msw, msh, mcolorType ); /* Fixed size */
	if(msurfuser==NULL) {
		printf("Fail to register surfuser!\n");
		return;
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	mvfbdev=&msurfuser->vfbdev;
	//msurfimg=msurfuser->imgbuf;
	msurfshmem=msurfuser->surfshmem;

        /* 3. Assign surface OP functions, for CLOSE/MIN./MAX. buttons etc.
         *    To be module default ones, OR user defined, OR leave it as NULL.
         */
        //msurfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	//msurfshmem->redraw_surface 	= surfuser_redraw_surface;
	//msurfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	//msurfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        msurfshmem->close_surface 	= surfuser_close_surface;
	//msurfshmem->user_mouse_event	= my_mouse_event;

	/* 4. Name for the surface. */
	strncpy(msurfshmem->surfname, "Help", SURFNAME_MAX-1);

	/* 5. First draw surface, set up TOPBTNs and TOPMENUs. */
	msurfshmem->bkgcolor=egi_color_random(color_light);	//WEGI_COLOR_LTGREEN; /* OR default SURF_MAIN_BKGCOLOR */
	surfuser_firstdraw_surface(msurfuser, TOPBTN_CLOSE, TOPMENU_NONE, NULL); /* Default firstdraw operation */

	/* 6. Start Ering routine */
	printf("start ering routine...\n");
	if( pthread_create(&msurfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, msurfuser) !=0 ) {
		egi_dperr("Fail to launch thread_EringRoutine!");
		if( egi_unregister_surfuser(&msurfuser)!=0 )
			egi_dpstd("Fail to unregister surfuser!\n");
	}

	/* 7. Write content: FTsymbol_uft8strings_writeFB: input FT_Face is NULL!  */
	FTsymbol_uft8strings_writeFB( mvfbdev, egi_sysfonts.bold,   	/* FBdev, fontface */
               	                  16, 16, (UFT8_PCHAR)help_descript,	/* fw,fh, pstr */
                       	          msw-20, 10, 4,         		/* pixpl, lines, fgap */
                               	  10,  40,    		  		/* x0,y0, */
                                  WEGI_COLOR_BLACK, -1, 240,     	/* fontcolor, transcolor,opaque */
       	                          NULL, NULL, NULL, NULL);        	/* int *cnt, int *lnleft, int* penx, int* peny */

	/* 8. Activate image */
	msurfshmem->sync=true;

 	/* ====== Main Loop ====== */
	while( msurfshmem->usersig != 1 ) {
		usleep(100000);
	};


        /* Post_1: Join ering_routine  */
        /* To force eringRoutine to quit , for sockfd MAY be blocked at ering_msg_recv()! */
        tm_delayms(200); /* Let eringRoutine to try to exit by itself, at signal surfshmem->usersig =1 */
        if( msurfshmem->eringRoutine_running ) {
                if( pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) !=0)
                        egi_dperr("Fail to pthread_setcancelstate eringRoutine, it MAY already quit!");
                /* Note:
                 *      " A thread's cancellation type, determined by pthread_setcanceltype(3), may be either asynchronous or
                 *        deferred (the default for new threads). " ---MAN pthread_setcancelstate
                 */
                if( pthread_cancel( msurfshmem->thread_eringRoutine ) !=0)
                        egi_dperr( "Fail to pthread_cancel eringRoutine, it MAY already quit!");
                else
                        egi_dpstd("OK to cancel eringRoutine thread!\n");
        }
        /* Make sure mutex unlocked in pthread if any! */
        egi_dpstd("Joint thread_eringRoutine...\n");
        if( pthread_join( msurfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

        /* Post_2: Unregister and destroy surfuser */
        egi_dpstd("Unregister surfuser...\n");
        if( egi_unregister_surfuser(&msurfuser)!=0 )
                egi_dpstd("Fail to unregister surfuser!\n");


	egi_dpstd("Exit OK!\n");
}


/*=====================================
	Surface :: Menu_Option
======================================*/

ESURF_TICKBOX *TickBox[CTYPE_MAX];
int 	OptionMenu_firstdraw_surface(FBDEV *vfb);
void 	MenuOption_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat);

/*----------------------------------------------------------
To create a SURFACE for menu_option.

@surfcaller:	The caller, as a surfuser.

Note:
1. If it's called in mouse event function, DO NOT forget to
   unlock shmem_mutex.
----------------------------------------------------------*/
void menu_option(EGI_SURFUSER *surfcaller)
{

  /* Surface size */
  int msw=180;
  int msh=140;

  /* Origin coordinate relative to the surfcaller */
  int x0=50;
  int y0=50;

	/* Help content */
	const char *str_option = " 选择基本单元样式:";

	/* Ref. pointers */
	EGI_SURFUSER     *msurfuser=NULL;
	EGI_SURFSHMEM    *msurfshmem=NULL;        /* Only a ref. to surfuser->surfshmem */
	FBDEV            *mvfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
	//EGI_IMGBUF       *msurfimg=NULL;          /* Only a ref. to surfuser->imgbuf */
	SURF_COLOR_TYPE  mcolorType=SURF_RGB565;  /* surfuser->vfbdev color type */
	//EGI_16BIT_COLOR  mbkgcolor;

	/* 1. Register/Create a surfuser */
	printf("Register to create a surfuser...\n");
	msurfuser=egi_register_surfuser( ERING_PATH_SURFMAN, NULL,
					 surfcaller->surfshmem->x0+x0, surfcaller->surfshmem->y0+y0,  /* x0,y0 */
					 msw, msh, msw, msh, mcolorType ); /* Fixed size */
	if(msurfuser==NULL) {
		printf("Fail to register surfuser!\n");
		return;
	}

	/* 2. Get ref. pointers to vfbdev/surfimg/surfshmem */
	mvfbdev=&msurfuser->vfbdev;
	//msurfimg=msurfuser->imgbuf;
	msurfshmem=msurfuser->surfshmem;

        /* 3. Assign surface OP functions, for CLOSE/MIN./MAX. buttons etc.
         *    To be module default ones, OR user defined, OR leave it as NULL.
         */
        //msurfshmem->minimize_surface 	= surfuser_minimize_surface;   	/* Surface module default functions */
	//msurfshmem->redraw_surface 	= surfuser_redraw_surface;
	//msurfshmem->maximize_surface 	= surfuser_maximize_surface;   	/* Need resize */
	//msurfshmem->normalize_surface 	= surfuser_normalize_surface; 	/* Need resize */
        msurfshmem->close_surface 	= surfuser_close_surface;
	msurfshmem->user_mouse_event	= MenuOption_mouse_event;

	/* 4. Name for the surface. */
	strncpy(msurfshmem->surfname, "Option", SURFNAME_MAX-1);

	/* 5. First draw surface, set up TOPBTNs and TOPMENUs. */
	egi_dpstd("Fristdraw surface...\n");
	msurfshmem->bkgcolor=WEGI_COLOR_GRAYA; /* OR default BLACK */
	surfuser_firstdraw_surface(msurfuser, TOPBTN_CLOSE, TOPMENU_NONE, NULL); /* Default firstdraw operation */
	OptionMenu_firstdraw_surface(mvfbdev);


	/* 6. Start Ering routine */
	egi_dpstd("start ering routine...\n");
	if( pthread_create(&msurfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, msurfuser) !=0 ) {
		egi_dperr("Fail to launch thread_EringRoutine!");
		if( egi_unregister_surfuser(&msurfuser)!=0 )
			egi_dpstd("Fail to unregister surfuser!\n");
	}

	/* 7. Write content: FTsymbol_uft8strings_writeFB: input FT_Face is NULL!  */
	FTsymbol_uft8strings_writeFB( mvfbdev, egi_sysfonts.bold,   	/* FBdev, fontface */
               	                  18, 18, (UFT8_PCHAR)str_option,		/* fw,fh, pstr */
                       	          msw-20, 10, 3,         		/* pixpl, lines, fgap */
                               	  5,  35,    		  		/* x0,y0, */
                                  WEGI_COLOR_BLACK, -1, 240,     	/* fontcolor, transcolor,opaque */
       	                          NULL, NULL, NULL, NULL);        	/* int *cnt, int *lnleft, int* penx, int* peny */

	/* 8. Activate image */
	msurfshmem->sync=true;

 	/* ====== Main Loop ====== */
	while( msurfshmem->usersig != 1 ) {
		usleep(100000);
	};

        /* Post_0:      --- CAVEAT! ---
         *  If the program happens to be trapped in a loop when surfshem->usersig==1 is invoked (click on X etc.),
         *  The coder MUST ensure that it can avoid a dead loop under such circumstance (by checking surfshem->usersig in the loop etc.)
         *  OR the SURFMAN may unregister the surface while the SURFUSER still runs and holds resources!
         */

        /* Post_1: Join ering_routine  */
        /* To force eringRoutine to quit , for sockfd MAY be blocked at ering_msg_recv()! */
        tm_delayms(200); /* Let eringRoutine to try to exit by itself, at signal surfshmem->usersig =1 */
        if( msurfshmem->eringRoutine_running ) {
                if( pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) !=0)
                        egi_dperr("Fail to pthread_setcancelstate eringRoutine, it MAY already quit!");
                /* Note:
                 * 	" A thread's cancellation type, determined by pthread_setcanceltype(3), may be either asynchronous or
                 *      deferred (the default for new threads). " ---MAN pthread_setcancelstate
                 */
                if( pthread_cancel( msurfshmem->thread_eringRoutine ) !=0)
                        egi_dperr( "Fail to pthread_cancel eringRoutine, it MAY already quit!");
                else
                        egi_dpstd("OK to cancel eringRoutine thread!\n");
        }
        /* Make sure mutex unlocked in pthread if any! */
        egi_dpstd("Joint thread_eringRoutine...\n");
        if( pthread_join( msurfshmem->thread_eringRoutine, NULL)!=0 )
                egi_dperr("Fail to join eringRoutine");

        /* Post_2: Unregister and destroy surfuser */
        egi_dpstd("Unregister surfuser...\n");
        if( egi_unregister_surfuser(&msurfuser)!=0 )
                egi_dpstd("Fail to unregister surfuser!\n");

	egi_dpstd("Exit OK!\n");
}


/*------------------------------------------
Create TickBoxes on the Menu_Option surface.

@vfb:	Pointer to virtual FBDEV.

Return:
        0       OK
        <0      Fails
------------------------------------------*/
int OptionMenu_firstdraw_surface(FBDEV *vfb)
{
	int i;
        int tw=18;      /* TickBox size */
        int th=18;

	/* Check input VFB */
        if(vfb==NULL || vfb->virt==false) {
                egi_dpstd("Invalid virt. FBDEV!\n");
                return -1;
        }

        /* 1. Create TickBox with unticked imgbuf */
        for(i=0; i<CTYPE_MAX; i++) {
        	/* Draw unticked TickBox */
	        fbset_color2(vfb, WEGI_COLOR_BLACK);
	        draw_rect(vfb, 20+30*i, 100, 20+30*i+tw-1, 100+th-1);
	        fbset_color2(vfb, WEGI_COLOR_GRAYB);
	        draw_filled_rect(vfb, 20+30*i +1, 100 +1, 20+30*i+tw-1 -1, 100+th-1 -1);

//		draw_circle(vfb, 20+30*i+tw/2, 100+tw/2, tw/2);

                TickBox[i]=egi_surfTickBox_create(vfb->virt_fb, 20+30*i, 100, 20+30*i, 100, tw, th);   /* xi,yi, x0, y0, w, h */
                if(TickBox[i]==NULL) {
                        egi_dpstd("Fail to create TickBox[%d].\n",i);
                        return -3;
                }
        }

        /* 2. Draw ticked mark on the first TickBox*/
        fbset_color2(vfb, WEGI_COLOR_BLACK);
        draw_filled_rect(vfb, 20+tw/4, 100+th/4,  20+tw*3/4, 100+th*3/4);

        /* 3. Create TickBox->imgbuf_ticked */
        for(i=0; i<CTYPE_MAX; i++) {
                /* To copy a block from imgbuf as image for unticked box */
                TickBox[i]->imgbuf_ticked=egi_imgbuf_blockCopy(vfb->virt_fb, 20, 100, th, tw); /* fb, xi, yi, h, w */
                if(TickBox[i]->imgbuf_unticked==NULL) {
                        egi_dpstd("Fail to blockCopy TickBox[%d].imgbuf_ticked.\n", i);
                        return -4;
                }
        }

	/* 4. Set current index of TickBox[] to ctype_index accordingly. */
	TickBox[ctype_index]->ticked=true;

	/* 5. Redraw TickBox[] and draw respective cubeimg_lib[] above. */
	for(i=0; i<CTYPE_MAX; i++) {
		egi_dpstd("Draw TickBox[%d]\n", i);
		egi_surfTickBox_writeFB(vfb, TickBox[i],  0, 0);
		/* imgbuf, fb, subnum, subcolor, x0,y0 */
		egi_subimg_writeFB( cubeimg_lib[i], vfb, 0, -1, 20+30*i, 75);
	}

	return 0;
}


/*--------------------------------------------------------------------
                Mouse Event Callback
                (shmem_mutex locked!)

1. It's a callback function called in surfuser_parse_mouse_event().
2. pmostat is for whole desk range.
3. This is for  SURFSHMEM.user_mouse_event() .
4. Some vars of pmostat->conkeys will NOT auto. reset!
--------------------------------------------------------------------*/
void MenuOption_mouse_event(EGI_SURFUSER *surfuser, EGI_MOUSE_STATUS *pmostat)
{
	int i,j;

	EGI_SURFSHMEM 	*surfshmem=surfuser->surfshmem;
	FBDEV 		*vfb=&surfuser->vfbdev;

   /* 1. LeftClick to tick/untick TickBox[]. */
   if(pmostat->LeftKeyDown) {
	egi_dpstd("Click on %d,%d\n", pmostat->mouseX -surfshmem->x0, pmostat->mouseY -surfshmem->y0);
	for(i=0; i<CTYPE_MAX; i++) {                /* X,Y relative to its container. relative to surfimg */
        	if( egi_point_on_surfTickBox( TickBox[i], pmostat->mouseX -surfshmem->x0,
                					  pmostat->mouseY -surfshmem->y0 ) )
		{
			egi_dpstd("Click TickBox[%d]\n", i);

			/* Re-select TickBox[] */
			if(TickBox[i]->ticked==false) {
				/* Select/redraw the TickBox */
	                	TickBox[i]->ticked = true;
				egi_surfTickBox_writeFB(vfb, TickBox[i],  0, 0);

				/* De-select/redraw previous TickBox[] */
				for( j=0; j<CTYPE_MAX; j++ ) {
					if(j!=i && TickBox[j]->ticked==true) {
						TickBox[j]->ticked=false;
						egi_surfTickBox_writeFB(vfb, TickBox[j],  0, 0);
						break;
					}
				}

				/* Update ctype_index accordingly */
				ctype_index = i;

				break;
			}
        	}
	}
   }

}


/*----------------------------------------------------------
To create a SURFACE for menu_we.

@surfcaller:	The caller, as a surfuser.

Note:
1. If it's called in mouse event function, DO NOT forget to
   unlock shmem_mutex.
----------------------------------------------------------*/
void menu_we(EGI_SURFUSER *surfcaller)
{
	egi_crun_stdSurfConfirm( (UFT8_PCHAR)"关于我们", (UFT8_PCHAR)about_we,
			      surfcaller->surfshmem->x0+50, surfcaller->surfshmem->y0+50, 240, 120);

}


/*-------------------------------------------------
User's code in surfuser_close_surface(), as for
surfshmem->user_close_surface().

@surfuser:      Pointer to EGI_SURFUSER.
-------------------------------------------------*/
void my_close_surface(EGI_SURFUSER *surfuer)
{
        /* Unlock to let surfman read flags. */
        pthread_mutex_unlock(&surfuser->surfshmem->shmem_mutex);
/* ------ >>>  Surface shmem Critical Zone  */

        /* Reset MEVENT to let SURFMAN continue to ering mevent. SURFMAN sets MEVENT before ering. --mutex_unlock! */
	surfuser->surfshmem->flags &= (~SURFACE_FLAG_MEVENT);

	/* Confirm to close, surfuser->retval =STDSURFCONFIRM_RET_OK if confirmed. */
        surfuser->retval = egi_crun_stdSurfConfirm( (UFT8_PCHAR)"Caution",
				     (UFT8_PCHAR)"游戏,是入刺地惊猜!\n大佬,你确定要退出了吗?",
                                     surfuser->surfshmem->x0+50, surfuser->surfshmem->y0+50, 220, 100);

/* ------ <<<  Surface shmem Critical Zone  */
        pthread_mutex_lock(&surfuser->surfshmem->shmem_mutex);

}
