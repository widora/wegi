/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Usage:

Keyboard shortcuts:
	ALT+TAB:	Switch applications.
	ALT+ESC:	Close current TOP surface.
	SUPER+D:	Minimize all surfaces.
	SUPER+W:	Normanlize all surfaces.

Note:
0. EringMSG from SURFMAN's renderThread(After one surface minimized, to pick next TOP surface and ering BRINGTOP)
   and from SURFMAN's main() routine job here(ering mstat to the TOP surface. ) are  NOT syncronized!
   as they are two separated threads.

1. SURFMAN routine:
   1.1 Parse keyboard input:  Shortcut key functions.
   1.2 Parse Mouse event:     LeftKeyDown to pick TOP surface.
   1.3 Ering mostat(packed with mostat+chars[]+conkeys) to the TOP surface.
2. Before access to surfman->surfaces[x], make sure that there
   are surfaces registered in the surfman. If there is NO surface
   available, it will certainly cause segmentation fault!
3. If any SURFUSER do NOT receive ERING MSG, then it MAY cause some
   problem any msg piled up in the kernel space??? --'Resource temporarily unavailable'.
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
6. The SURFMAN requests a surface to close by eringing msg ERING_SURFACE_CLOSE to the surface.

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
8. A mostats MAY be dropped by the surfman before TOP surface clear SURFACE_FLAG_MEVENT!
   Such fail to set surfuser->mevent_suspend and update lastX/Y, which leads to surface mis_movement at next LeftKeyDownHold!
9. A kbdstat is read directly from Keyboard input event, kbdstat.conkeys are control_key_status, sequence of key_pressing
   also recorded. it intends to monitor keyboard shortcut command!

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
2021-04-17:
	1. Need to send LeftKeyDown to the new TOP surface to clear its old stat data, lastX/Y etc.
	   See W2-1-A.2
2021-04-19:
	1. Check whether directory of ERING_PATH_SURFMAN exists, if not, make it.
2021-04-20:
	1. Load a random image file for wallpaper.
2021-05-06:
	1. TEST: Receive msg queue from surfuser.
2021-05-08:
	1. Set permission mode for ERING_PATH_SURFMAN.
2021-05-11:
	1. TEST: surfmsg_send()SURFMSG_REQUEST_REFRESH
2021-05-12:
	1. Set/reset surfman->minibar_ON to display/disappear screen left_side minibar.
2021-05-14/15:
	1. Add a MenuList: surfman->menulist.
	2. Set/reset surfman->menulist_ON to display/disappear MenuList tree.
2021-05-18:
	1. Process SURFMSG_REQUEST_UPDATE_WALLPAPER.
2021-05-21:
	2. W.2.7.2  If the mouse cursor hovers over a NON_TOP_surface, ERING mstat to the surface.
2021-05-30:
	1. W.4  Add waitpid for child process.
2021-05-31:
	1. Interlock surfman->minibar_ON and surfman->menulist_ON
2021-06-02:
	1.Pack keyboard input into pmostat->ch
	2.Modify key_combinations for surface movement/shift.
2021-06-08:
	1.Modify key_combinations for surface movement/shift. CTRL_Q/R/S/T
2021-06-09:
	1. Use bitfield for blooean type memebers of EGI_MOUSE_STATUS.
	2. Add members 'chars[32]' and 'nch' for EGI_MOUSE_STATUS.
	3. Buffer key_inputs in chars[], and later transfer to pmostat->chars[]
	   to ering to the TOP surface. Such to avoid missing any key input.
2021-06-10:
	1. To clear all buffer chars[] if there is no surface displayed on desk.
2021-06-16:
	1. Add:  W.0  Read keyboard shortcut control keys... (CONKEYs)
2021-06-18:
	1. Chars of a TTY Escape Sequence are inseparable, we MUST read out
	   all chars in STDIN buffer at once(at one time), NOT one_by_one!!!
2021-06-24:
	1. Avoid to ering pmostat->chars[] and conkeys to a non_TOP_surface in W.2.7.2.
	2. Add shortcuts:  ALT+TAB, ALT+ESC, SUPER+D, SUPER+W
2021-06-25:
	1. Delete old codes for TEST shortcuts, such as Ctrl+n, Ctrl+o ....
2021-06-30:
	1.  W.2.7.2  Correct surfID for a non_TOP surface:
	    surfID >=0 && surfID!= SURFMAN_MAX_SURFACES && surfID != surfman_get_TopDispSurfaceID(surfman)
2021-07-01:
	1. Delete variable 'surface_downhold', no use now.
	2. W.2.7.2  DO NOT ering LeftKeyDownHold to may non_TOPsurfaces.

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/types.h>

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
	#define WALLPAPER_PATH   "/home/midas-zhou/Pictures/desk"
#else
	#define WALLPAPER_PATH	 "/mmc/desk"
#endif

#ifdef LETS_NOTE
        #define MCURSOR_NORMAL	"/home/midas-zhou/egi/mcursor.png"
        #define MCURSOR_GRAB     "/home/midas-zhou/egi/mgrab.png"
	#define SURFMAN_BKGIMG_PATH	"/home/midas-zhou/egi/egidesk.png"
#else
   	#define MCURSOR_NORMAL 	"/mmc/mcursor.png"
   	#define MCURSOR_GRAB 	"/mmc/mgrab.png"
	#define SURFMAN_BKGIMG_PATH	"/mmc/egidesk.jpg"  // "/mmc/linux.jpg"
#endif

#define SHORTCUT_AFT_DELAYMS	150	/* in ms, delay after a shortcut action.
					 * To avoid repeating actions, let kbdread absort intermediate status.
					 */

/* Inputs: keyboard, mouse, STDIN  */
static EGI_KBD_STATUS 	kbdstat={  .conkeys={.abskey=ABS_MAX} };  /* 0-->ABS_X ! */
static EGI_MOUSE_STATUS mostat;
static EGI_MOUSE_STATUS *pmostat;
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus);
int 	lastX;
int 	lastY;
/* TTY STDIN key buffer */
int 	nch;
char 	chars[32];  	/* W.2.7.1A if NO surface displayed, then clear buffer chars[] */

//bool	surface_downhold; /* NO USE */

EGI_SURFMAN *surfman;
int	zseq;
int	surfID; /* as index of surfman->surfaces[] */
int	userID; /* NOW as surface->csFD */

static int sigQuit;

/* Wait pid */
int wait_pid;
int wait_status;


/* Signal handler for SurfUser */
void signal_handler(int signo)
{
	if(signo==SIGINT) {
		egi_dpstd("SIGINT to quit the process!\n");
		sigQuit=1;
	}
}

/* For SURFMSG_DATA */
int nrcv;
SURFMSG_DATA msgdata;

/* Ering MSG */
ERING_MSG *emsg;

int main(int argc, char **argv)
{
	int j,k;
	char ch;
	int surfID;

	int opt;
	int nread;
	int ret;

	struct timeval  tm_lastkey={0};

        /* <<<<<  EGI general initialization   >>>>>> */

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
	printf("Create an ERING_MSG, sizeof(EGI_MOUSE_STATUS)=%zd ....\n", sizeof(EGI_MOUSE_STATUS));
        emsg=ering_msg_init();
        if(emsg==NULL) exit(EXIT_FAILURE);

	/* 0. Check directory for ERING_PATH_SURFMAN */
	char *strdir=NULL;
	char *pdir=NULL;
	strdir=strdup(ERING_PATH_SURFMAN);
	pdir=dirname(strdir);
	if( access(pdir, F_OK)!=0 ) {
		if(egi_util_mkdir(pdir, S_IRWXG) !=0 ) {
			printf("Fail to mkdir for ering path!\n");
			exit(EXIT_FAILURE);
		}
	}
	free(strdir);

	/* 1. Create an EGI surface manager */
	surfman=egi_create_surfman(ERING_PATH_SURFMAN);
	if(surfman==NULL)
		exit(EXIT_FAILURE);
	else
		printf("Succeed to create surfman at '%s'!\n", ERING_PATH_SURFMAN);

        /* 1.01 Set permission mode for directory '/tmp/run/ering', default: 'd---r-x---' */
        char strtmp[EGI_PATH_MAX+EGI_NAME_MAX];
        strtmp[EGI_PATH_MAX+EGI_NAME_MAX-1]='\0';
        strncpy(strtmp, ERING_PATH_SURFMAN, EGI_PATH_MAX+EGI_NAME_MAX-1);
        if( chmod( dirname(strtmp), 0665)<0 ) {  /* drw-rw-r-x: r-x for Others! x-MUST,   rw- XXX  */
        //if( chmod( "/run/ering", 0666)<0 ) {
                egi_dperr("Fail chmod Dir of ERING_PATH_SURFMAN!");
		egi_destroy_surfman(&surfman);
		exit(EXIT_FAILURE);
        }
        /* 1.02 Set permission mode for the ERING svrpath: srw-rw-rw- */
        if( chmod(ERING_PATH_SURFMAN, 0666)<0 ) {
                egi_dperr("Fail chmod ERING_PATH_SURFMAN!");
		egi_destroy_surfman(&surfman);
		exit(EXIT_FAILURE);
        }

	/* 1.A Load imgbuf for mouse cursor */
	surfman->mcursor=egi_imgbuf_readfile(MCURSOR_NORMAL);
	surfman->mgrab=egi_imgbuf_readfile(MCURSOR_GRAB);

	/* 1.B Load wallpaper */
	//surfman->bkgimg=egi_imgbuf_readfile(SURFMAN_BKGIMG_PATH);
	char **FileList=NULL;
	int  fcount=0;
	FileList=egi_alloc_search_files(WALLPAPER_PATH, "png,jpg", &fcount); // const char* path, const char* fext,  int *pcount
	if( FileList )
		egi_dpstd("Found %d image files for wallpaper.\n", fcount);
	else
		egi_dpstd("No image file found for wallpaper!\n");

//	pthread_mutex_lock(&surfman->surfman_mutex);
/* ------ >>>  Surfman Critical Zone  */

	/* Load a random image file */
	surfman->bkgimg=egi_imgbuf_readfile(FileList[mat_random_range(fcount)]);
	/* Stretch image to fit for the FB */
	egi_imgbuf_resize_update(&surfman->bkgimg, false, surfman->fbdev.pos_xres, surfman->fbdev.pos_yres); /* **pimg, keep_ratio, width, height */

/* ------ <<<  Surfman Critical Zone  */
//	pthread_mutex_unlock(&surfman->surfman_mutex);

        /* 2. Set termI/O 设置终端为直接读取单个字符方式 */
        egi_set_termios();

	/* 3. Set mouse callback function and start mouse readloop */
	/* ! Refere pos_xres/yres for mouse movement limit. !!!WARNING!!! gv_fb_dev MAYBE NOT initilized yet! */
	gv_fb_dev.pos_xres=surfman->fbdev.pos_xres;
	gv_fb_dev.pos_yres=surfman->fbdev.pos_yres;
  	if( egi_start_mouseread(NULL, mouse_callback) <0 )
		exit(EXIT_FAILURE);

        /* 4. Create a MenuList Tree */
        ESURF_MENULIST *mlist_System, *mlist_Program, *mlist_Tools;
        /* 4.1 Create root mlist: ( mode, root, x0, y0, mw, mh, face, fw, fh, capacity) */
        surfman->menulist=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, true, 0, surfman->fbdev.pos_yres-1,
											110, 30, egi_sysfonts.regular, 16, 16, 8 );
        mlist_System=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, false, 0, 0, 130, 30, egi_sysfonts.regular, 16, 16, 8 );
        mlist_Program=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, false, 0, 30*3, 100, 30, egi_sysfonts.regular, 16, 16, 8 );
        mlist_Tools=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, false, 0, 0, 130, 30, egi_sysfonts.regular, 16, 16, 8 );

        /* 4.2 Add items to mlist_Tools */
        egi_surfMenuList_addItem(mlist_Tools, "Security", NULL, "/tmp/test_surfuser -n  系统安全 ");
        egi_surfMenuList_addItem(mlist_Tools, "文件管理", NULL, NULL);
        egi_surfMenuList_addItem(mlist_Tools, "系统监控", NULL, "/tmp/surf_wifiscan");
        egi_surfMenuList_addItem(mlist_Tools, "必应桌面", NULL, "/tmp/surf_wallpaper");

        /* 4.3 Add items to mlist_Program */
        egi_surfMenuList_addItem(mlist_Program, "记事本", NULL, "/tmp/surf_editor");
        egi_surfMenuList_addItem(mlist_Program, "搜索", NULL, NULL);
        egi_surfMenuList_addItem(mlist_Program, "测试", NULL, "/tmp/test_surfuser -n 测试 ");
        egi_surfMenuList_addItem(mlist_Program, "WetRadio", NULL, "/tmp/surf_wetradio");
        egi_surfMenuList_addItem(mlist_Program, "MAD播放器", NULL, "/tmp/surf_madplay /mmc/music/'*.mp3'" );
        egi_surfMenuList_addItem(mlist_Program, "红楼梦", NULL, "/tmp/surf_book");

        /* 4.4 Add items to mlist_System */
        egi_surfMenuList_addItem(mlist_System, "Control Panel", NULL, "/tmp/surfman_guider");
        egi_surfMenuList_addItem(mlist_System, "工具", mlist_Tools, NULL);
        egi_surfMenuList_addItem(mlist_System, "Program", mlist_Program, NULL); /* Link sub_mlist0 to  mlist_System */
        egi_surfMenuList_addItem(mlist_System, "Game", NULL, "/tmp/surf_tetris");

        /* 4.5 Add items to the ROOT menulist */
        egi_surfMenuList_addItem(surfman->menulist, "Shut Down", NULL, NULL);
        egi_surfMenuList_addItem(surfman->menulist, "Log Out", NULL, NULL);
        egi_surfMenuList_addItem(surfman->menulist, "System", mlist_System, NULL);

#if 0	/* 4.5 ------TEST:  Init Assemble the MenuList Selection Path */
        surfman->menulist->path[1] = mlist_System;
        surfman->menulist->path[2] = mlist_Program;
        surfman->menulist->depth=2;     /* 2 levels of sub_MenuList */
#endif


		/* =============== >>>  SURFMAN Routine Job  <<< ============== */

	/* 5. Do SURFMAN routine jobs while no command */
	k=SURFMAN_MAX_SURFACES-1;
	while( surfman->cmd !=1 ) {

/* TEST: ------- SURFMSG ------- */
		/* Receive msg queue _REQUEST_DISPINFO from surfaces */
		nrcv=msgrcv(surfman->msgid, (void *)&msgdata, SURFMSG_TEXT_MAX-1, -SURFMSG_PRVTYPE_MAX, MSG_NOERROR|IPC_NOWAIT);
                if( nrcv<0 && errno == ENOMSG ) {
                }
                else if(nrcv<0)
                        egi_dperr("msgrcv() fails.");
		else {
                	if(nrcv==0)
                        	egi_dpstd("msgrcv nrcv=0!\n");

			switch(msgdata.msg_type) {
				case SURFMSG_REQUEST_DISPINFO:
					egi_dpstd("EMSG from surface: %s!\n", msgdata.msg_text);
					break;
				case SURFMSG_REQUEST_UPDATE_WALLPAPER:
					msgdata.msg_text[nrcv]='\0';
					egi_dpstd("EMSG from surface: Update wallpaper with '%s'.\n", msgdata.msg_text);

					/* Load a image file */
					EGI_IMGBUF *tmpimg=egi_imgbuf_readfile(msgdata.msg_text);
					/* Stretch image to fit for the FB */
					if(tmpimg) {
						egi_imgbuf_resize_update(&tmpimg, false, surfman->fbdev.pos_xres, surfman->fbdev.pos_yres);
						pthread_mutex_lock(&surfman->surfman_mutex);
		/* ------ >>>  Surfman Critical Zone  */

						egi_imgbuf_free2(&surfman->bkgimg);
						printf("Assing surfman->bkgimg = tmpimg!\n");
						surfman->bkgimg = tmpimg;

		/* ------ <<<  Surfman Critical Zone  */
						pthread_mutex_unlock(&surfman->surfman_mutex);
					}
					break;
				default:
					egi_dpstd("Unknown EMSG: %s \n", msgdata.msg_text);
					break;

			}
		}

KEY_INPUT:

		/* W.0  Read keyboard shortcut control keys ...
		 * Note:
		 * 1. Capture keyboard chortcut_control_keys first!!!
		 *    XXX Loop back after control_keys are parsed A.S.A.P, such to avoid key_event missing/omitting
    		 *    XXX due to long time processing/wating in mouse_event.
		 *
		 */
		if( (ret=egi_read_kbdcode(&kbdstat, NULL))!=0 ) {  //"/dev/input/event0") !=0 ) {
			//egi_dpstd("Fail read_kbdcode or no input event, ret=%d!\n", ret);
		}

		/* W.0.1  Parse keyboard shortcut for SURFMAN */
		if( kbdstat.conkeys.nk == 2 && kbdstat.conkeys.asciikey ) {
		    switch(kbdstat.conkeys.conkeyseq[0]) {
			case CONKEYID_ALT:
/*  ALT+TAB ...... Switch surfaces */
				if( kbdstat.conkeys.asciikey == KEY_TAB ) {
					pthread_mutex_lock(&surfman->surfman_mutex);
			/* ------ >>>  Surfman Critical Zone  */
					if(surfman->scnt >0)
				 		surfman_bringtop_surface_nolock(surfman, SURFMAN_MAX_SURFACES-surfman->scnt);
			/* ------ <<<  Surfman Critical Zone  */
					pthread_mutex_unlock(&surfman->surfman_mutex);

					/* Delay and loop back, expecting next read_kbdcode() gets release status. */
					tm_delayms(SHORTCUT_AFT_DELAYMS);
					continue;
				}
/*  ALT+ESC ...... Close current TOP surface.  */
				else if( kbdstat.conkeys.asciikey == KEY_ESC ) {
					pthread_mutex_lock(&surfman->surfman_mutex);
			/* ------ >>>  Surfman Critical Zone  */
					if(surfman->scnt >0) {
					    #if 1 /* unet_sendmsg, emsg type only. */
					        emsg->type=ERING_SURFACE_CLOSE;
			                        if( unet_sendmsg( surfman->surfaces[SURFMAN_MAX_SURFACES-1]->csFD, emsg->msghead) <=0 )
                        		                egi_dpstd("Fail to sednmsg ERING_SURFACE_CLOSE!\n");
					    #else /* ering_msg_send, with payload. */
						if( ering_msg_send( surfman->surfaces[SURFMAN_MAX_SURFACES-1]->csFD,
							emsg, ERING_SURFACE_CLOSE, pmostat, sizeof(EGI_MOUSE_STATUS) ) <=0 ) {
							egi_dpstd("Fail to sendmsg ERING_SURFACE_CLOSE!\n");
						}
					    #endif
					}
			/* ------ <<<  Surfman Critical Zone  */
					pthread_mutex_unlock(&surfman->surfman_mutex);

					/* Delay and loop back */
					tm_delayms(SHORTCUT_AFT_DELAYMS);
					continue;
				}
				break;
			case CONKEYID_SUPER:
/* Super+D ......  Minimize all surface. */
				if( kbdstat.conkeys.asciikey == KEY_D ) {
					pthread_mutex_lock(&surfman->surfman_mutex);
			/* ------ >>>  Surfman Critical Zone  */
					surfman_minimize_allSurfaces(surfman);
			/* ------ <<<  Surfman Critical Zone  */
					pthread_mutex_unlock(&surfman->surfman_mutex);

					/* Delay and loop back */
					tm_delayms(SHORTCUT_AFT_DELAYMS);
					continue;
				}
/* Super+W ......  Normalize all surface. */
				else if( kbdstat.conkeys.asciikey == KEY_W ) {
					pthread_mutex_lock(&surfman->surfman_mutex);
			/* ------ >>>  Surfman Critical Zone  */
					surfman_normalize_allSurfaces(surfman);
			/* ------ <<<  Surfman Critical Zone  */
					pthread_mutex_unlock(&surfman->surfman_mutex);

					/* Delay and loop back */
					tm_delayms(SHORTCUT_AFT_DELAYMS);
					continue;
				}
				break;
			default:
				break;
		   } /* switch */
		}


		/* W.1  Read Keyboard STDIN  */
#if 0	/* Read char ONE BY ONE from TTY stdin. */
                ch=0;
                if( read(STDIN_FILENO, &ch,1) <0 ) {
			egi_dperr("Fail to read STDIN");
			ch=0;
		}
		if(ch>0) {
			/* Push into chars[] */
			if( nch < sizeof(chars)-1 )
				chars[nch++]=ch;
			printf("Keyin: '%c'(%d);  chars[]: %-32.32s\n", ch, ch, chars);
		}

#else	/* Read out all chars from TTY stdin
	 * Note:
	 * 1. Since TTY Escape Sequence is also a SINGLE character/command, they MUST be read out together and
	 *    packed in one ERING session. If a surfuser receives broken Escape_Sequence, it will cause ERROR!
         *
         */
	do {
                if( (nread=read(STDIN_FILENO, chars+nch, sizeof(chars)-1-nch)) <0 ) {
			egi_dperr("Fail to read STDIN");
		}
		if(nread>0) {
			nch +=nread;
			printf("chars[]: %-32.32s\n", chars);
		}

	} while( nread>0 );  /* Break nread<=0 */

#endif


WAIT_REQUEST:
		/* W2. Mouse event process */
	        if( egi_mouse_getRequest(pmostat) ) {  /* If true: pmostat->mutex_lock keeps locked! */

			/* W2.0: Pack STDIN ch into pmostat->ch, while pmostat->mutex_lock MUST keep locked! */
			if(pmostat) { /* To skip init NULL value. */

				/* 1. Transfer nch/chars[] buffer to pmostat->nch/pmotat->ch[] */
				pmostat->nch = nch;
				strncpy(pmostat->chars, chars, nch);
				pmostat->chars[nch]=0; /* EOF */

				/* 2. Transfer kbdstat->conkeys to pmostat->conkeys */
				/* Copy conkeys to pmostat for ERING */
				memcpy(&pmostat->conkeys, &kbdstat.conkeys, sizeof(EGI_CONKEY_STATUS));

				/* TEST: for functional lastkey, check timeval to avoid sending reapting status of press_lastkey.
				 */
				if( pmostat->conkeys.press_lastkey ) {
				    if( timercmp(&tm_lastkey, &kbdstat.tm_lastkey, ==) ) {
					printf("Same tm_lastkey, skip...\n");
					/* !!! NOTE:
					 * 1. Reset 'pmostat'->conkeys.press_lastkey, instead of 'kbdstat'.conkeys.press_lastkey!
					 *    The later will scan in again with different timeval!
					 * 2. If a key is of repeat_event type, it always brings with different tiemval!
					 */
					pmostat->conkeys.press_lastkey=false;
				    }
				    else {
					//printf("New tm_lastkey...\n");
					tm_lastkey = kbdstat.tm_lastkey;
				    }
				}

				/* Clear buffer chars[]:  <---- NOPE! NOT here!!!
				 * Clear AFTER it sends pmostat->chars to the TOP surface!
				 * See at W2.7.1
				 */
				//XXX nch=0;
				//XXX memset(chars,0, sizeof(chars));
			}
			else
				egi_dpstd("pmosta==NULL!\n");


			/* W2.1. If mouse cursor linger at left_bottom corner, then display/activate MenuList tree. */
			if( !surfman->menulist_ON && !pmostat->LeftKeyDownHold ) {  /* to avoid surface_downhold=true */
				if( pmostat->mouseX < 10 && pmostat->mouseY > surfman->fbdev.pos_yres -10 ) {
					surfman->menulist_ON=true;
					surfman->minibar_ON=false; /* interlocking! */
				}
			}
			/* Otherwise update MenuList tree as per pxy */
			else {
				pthread_mutex_lock(&surfman->surfman_mutex);
		 /* ------ >>>  Surfman Critical Zone  */

				egi_surfMenuList_updatePath(surfman->menulist, pmostat->mouseX, pmostat->mouseY);

		 /* ------ <<<  Surfman Critical Zone  */
				pthread_mutex_unlock(&surfman->surfman_mutex);

			}

			/* W2.2. If mouse cursor linger at letf_top corner, then display/activate minibar */
	   if(!surfman->menulist_ON ) {

			if( !surfman->minibar_ON  && !pmostat->LeftKeyDownHold ) {  /* to avoid surface_downhold=true */
				if( pmostat->mouseX < 10 && pmostat->mouseY <10  ) {
				    /* ONLY if mini_surface is NOT zero! */
				    if( surfman->mincnt) {
					surfman->minibar_ON=true;
					surfman->menulist_ON=false; /* interlocking */
				    }
				}
			}
			/* Otherwise, disappear it. */
			else {
				pthread_mutex_lock(&surfman->surfman_mutex);
		 /* ------ >>>  Surfman Critical Zone  */
				/* If mouseXY become out of Minibar */
				if( SURFMAN_MINIBAR_PIXZ != surfman_xyget_Zseq(surfman, pmostat->mouseX, pmostat->mouseY) )
				{
                                        surfman->minibar_ON=false;
//Reset in surfman_render_thread():      surfman->IndexMpMinSurf=-1; /* Refer to nothing */
				}

		 /* ------ <<<  Surfman Critical Zone  */
				pthread_mutex_unlock(&surfman->surfman_mutex);
			}
	    }

	                /* W2.3. LeftKeyDown: To bring the surface to top, to save current mouseXY */
        	        if( pmostat->LeftKeyDown )  {  //&& !surface_downhold ) {

				pthread_mutex_lock(&surfman->surfman_mutex);
		 /* ------ >>>  Surfman Critical Zone  */

                                surfman->mx  = pmostat->mouseX;
                                surfman->my  = pmostat->mouseY;

				/* Check zseq  */
				zseq=surfman_xyget_Zseq(surfman, surfman->mx, surfman->my);
				printf("Picked zseq=%d\n", zseq);

#if 1 /* ------ SurfMan::MenuList ------ */
				/* Check if click on MenuList */
				if( surfman->menulist_ON ) {
					if( zseq != SURFMAN_MENULIST_PIXZ ) {
						surfman->menulist_ON = false;
						/* Reset depth to clear previous selection tree. */
						surfman->menulist->depth=0;
					}
					else
					{
						/* Load MenuItem Linked PROG */
						egi_surfMenuList_runMenuProg(surfman->menulist);
						surfman->menulist_ON = false;
						surfman->menulist->depth=0;
					}

				}
#endif

				/* A. Check if it clicks on any SURFACEs. */
				surfID=surfman_xyget_surfaceID(surfman, surfman->mx, surfman->my );
				printf("surfID=%d\n",surfID);

				/* NOTE: Sending mostat to the top surface MUST be carried out at last!!
				 * For reasons of:
				 * 1. If it clicks on a surface SURFBTN_MIN, the surfuser may have changed surface STATUS before
				 *    executing following codes! AND MAY ALSO before surfman render the updated surface image/minibar!
				 * 2. The surface is displayed on desk while its STAUTS is already MINIMIZED acutually!
				 * 3. A surface may be brought to TOP by resorting zseq, after other surface quits.
				 */

				// xxx if(surfID==SURFMAN_MAX_SURFACES-1) {
				int topID=surfman_get_TopDispSurfaceID(surfman);
				printf("topID=%d\n",topID);

				/* A.1  IF: clicked surface already on TOP layer. ( Minimized surfaces NOT considered! ) */
				/* !!! Exclude surfID <0 && surfman_get_TopDispSurfaceID() <0 */
				//if( surfID >=0 && surfID==surfman_get_TopDispSurfaceID(surfman) ) {
				/* NOTE: See W2.7.2 */
				if( surfID >=0 && surfID!=SURFMAN_MAX_SURFACES && surfID == topID ) {
					//surface_downhold=true;

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
				/* A.2  Else: If clicked surface is NOT current top surface. */
				else if(surfID>=0) { /* <0 as bkground */
					//surface_downhold=true;
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

#if 0				 /* XXX Here, we end mouse event processing without sending mostat to the surface! */
				 /* !!! Need to send LeftKeyDown to the new TOP surface to clear its old stat data, lastX/Y etc.! */
		 /* ------ <<<  Surfman Critical Zone  */
					pthread_mutex_unlock(&surfman->surfman_mutex);
					goto END_MOUSE_EVENT;
#endif
				}

				/* B. surfID<0: Check if it clicks on leftside minibar menu, Check IndexMpMinSurf, which is set by
				      surfman_render_thread() based on surfmap->mx,my
				 */
				else if ( surfman->IndexMpMinSurf >= 0 ) {
					/* Bring mouse clicked minisurface to TOP layer, also restore its status. */
					surfman_bringtop_surface_nolock(surfman, surfman->minsurfaces[surfman->IndexMpMinSurf]->id);

					/* Disappear minibar then */
					surfman->minibar_ON = false;

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

			/* W2.4. */

			/* W2.5. */

			/* W2.6. Update mouseXY to surfman */
			//else {
			else if( pmostat->mouseDX || pmostat->mouseDY ) {

				pthread_mutex_lock(&surfman->surfman_mutex);
		 /* ------ >>>  Surfman Critical Zone  */

                                surfman->mx  = pmostat->mouseX;
                                surfman->my  = pmostat->mouseY;

		 /* ------ <<<  Surfman Critical Zone  */
				pthread_mutex_unlock(&surfman->surfman_mutex);

#if 0 /* TEST-----:  MSG request for SURFMAN to render/refresh.  */
				surfmsg_send(surfman->msgid, SURFMSG_REQUEST_REFRESH, NULL, IPC_NOWAIT);
#endif

			}


#if 1			/* W2.7. At last, send mouse_status to the TOP(focused) surface */
			/*** NOTE:
			 *   1. If current mostat is a LeftKeyDown on minibar menu, then it brings a surface to TOP,
			 *	and here also immediately ering the same mostat to the surface! It may cause unwanted
			 *	effect.
			 *	Example: Only by one click, a surface appears, and at the same time a button of the surface
			 *	is activated.
			 */

			pthread_mutex_lock(&surfman->surfman_mutex);
	/* ------ >>>  Surfman Critical Zone  */

			/* W2.7.1  Only if have surface registered, and MiniBar+MenuList are OFF! */
			if(surfman->scnt && !surfman->minibar_ON && !surfman->menulist_ON ) {
				/*** NOTE:
				 *   1. If current surfaces[SURFMAN_MAX_SURFACES-1] is minimized by SURFUSER! then we have
				 *      to pick next top surface on the desk, NOT in the minibar menu!
				 *      OR the minimized surface will keep receiving mostat !!!!
				 *   2. NOT call surfman_bringtop_surface_nolock(), Let it keeps its zseq!
				 */
				for(j=0; j < surfman->scnt; j++) {  /* SURFMAN_MAX_SURFACES-1-j:  traverse from upper layer. */
					if( surfman->surfaces[SURFMAN_MAX_SURFACES-1-j]->surfshmem->status != SURFACE_STATUS_MINIMIZED ) {
		/* Note: ering_msg_send() is BLOCKING type, if Surfuser DO NOT recv the msg, it will be blocked here!?  OR to kernel buffer. */

					    /* Only if MEVENT is 0! as the SURFUSER finish parsing last MEVENT. */
					    if ( !(surfman->surfaces[SURFMAN_MAX_SURFACES-1-j]->surfshmem->flags & SURFACE_FLAG_MEVENT) ) {
						/* Set MEVENT first */
						surfman->surfaces[SURFMAN_MAX_SURFACES-1-j]->surfshmem->flags |= SURFACE_FLAG_MEVENT;
						/* Send MEVENT then */
//						egi_dpstd("Ering_msg_send ch=%d\n", pmostat->ch);
						//printf("ering msg pmostat  RKDHold=%s\n", pmostat->RightKeyDownHold ? "TRUE" : "FALSE" );
						//if(pmostat->LeftKeyDown) egi_dpstd("Ering mstat LeftKeyDown! \n");
						if( ering_msg_send( surfman->surfaces[SURFMAN_MAX_SURFACES-1-j]->csFD,
							emsg, ERING_MOUSE_STATUS, pmostat, sizeof(EGI_MOUSE_STATUS) ) <=0 ) {
							egi_dpstd("Fail to sendmsg ERING_MOUSE_STATUS!\n");
						}
						egi_dpstd("Ering_msg_send OK! Mxy(%d,%d)\n",pmostat->mouseX, pmostat->mouseY);

	/* TEST: ------ check lastkey */
						if(pmostat->conkeys.press_lastkey)
							egi_dpstd("ering_msg_send  lastkey=%d\n", pmostat->conkeys.lastkey);

						/* !!! NOTE: CONKEYs and chars[] MAY NOT be cleared here(if SURFACE_FLAG_MEVENT)
						 *	So have to clear them again in W 2.7.2 if necessary.
						 */

						/* Reset/clear pmostat->nch and conkeys */
						pmostat->nch=0;
						/* Reset/clear pmostat->conkey */
						memset(&pmostat->conkeys,0,sizeof(EGI_CONKEY_STATUS));

						/* Reset local nch/chars[] */
		                                nch=0;
                		                memset(chars, 0, sizeof(chars));

						/* NOPE! MUST NOT clear kbdstat.conkeys! .press_xxx MUST keeps!  XXX Reset/clear kbdstat->conkey. */
						//memset(&kbdstat.conkeys,0,sizeof(EGI_CONKEY_STATUS));

					    }
					    else {  /* SURFACE_FLAG_MEVENT NOT cleared! Means the TOP surface has NOT finished last mevent yet. */
							egi_dpstd("FLAG_MEVENT NOT cleared!\n");
					    }
					    /* If SURFACE_FLAG_MEVENT NOT cleared by the surfuser, then current mostat will be ignored! */

					    /* Break. Only send to the TOP surface. */
					    break;
					}
				}
			}
			/* W.2.7.1A if NO surface displayed, then clear buffer chars[] */
			if( surfman_get_TopDispSurfaceID(surfman)<0 ) {
				nch=0;
				memset(chars,0, sizeof(chars));
			}


#if 1			/* W.2.7.2  If the mouse cursor hovers over a NON_TOP_surface, ERING mstat to the surface.
			 * Note:
			 * 	1. LeftKeyDownHold will move a NOT_TOP_surface!
			 *      2. 	!!! ----- SEGMENT_FAULT ERROR ----- !!!
			 *	   If current displayed surface has been unregistered, while zbuff[] values of surfman->fb_dev
			 *	   have NOT been updated yet(as they are handled respectively by two threads), then access to
			 *	   surfman->surfaces[surfID] will cause segfault!
			 *	   In this case, surfman_xyget_surfaceID() results in surfID=SURFMAN_MAX_SURFACES!
			 *	3. A NON_TOP_surface receives mstat ONLY WHEN the mouse is within its surface area, and it CAN NOT
			 *	   receive it once the mouse is out of the range.
			 *   	4. If the TOP surface is downhold(RESIZing OR MOVing), it MUST NOT ering mostat to any non_TOPsurfaces.
			 *         any the LeftKeyDownHold will mis_trigger movement....
			 */

	/* ONLY after chars/conkeys cleared by the TOP surface. Send mouse status ONLY! */
	if(pmostat->nch==0 && pmostat->conkeys.nk==0 && !pmostat->LeftKeyDownHold) {
			//egi_dpstd("xyget_surfID\n");
			surfID=surfman_xyget_surfaceID(surfman, surfman->mx, surfman->my );

			int topID=surfman_get_TopDispSurfaceID(surfman);
			/* Note:
			 *  			----- !!! IMPORTANT !!! -----
			 *   1.
			 *      If There is ONLY ONE surface on desktop, and others are MINIMIZED. It's surely the TOP surface,
			 *	BUT not necessary surfID == SURFMAN_MAX_SURFACES-1! Prev. TOP surface is minimized and carry its surfID.
			 *      Call surfman_get_TopDispSurfaceID(surfman) instead.
			 *   2. If surfman_xyget_surfaceID() returns SURFMAN_MAX_SURFACES, it means prev TOP surface just unregistered
		         *      while surfman zbuff[] NOT updated yet!!
			 *	OR surfman_xyget_surfaceID() returns SURFMAN_MAX_SURFACES-1 && surfID!=topID, means prev TOP surface just
			 *	minimized, while zbuff[] NOT updated yet!!
			 *
			 *   3. TODO: Put token in pmostat to inform the receiver that it's NOT TOPsurface!
			 */
			// XXX if( surfID >=0 && surfID != SURFMAN_MAX_SURFACES-1 && surfID != SURFMAN_MAX_SURFACES && surfID !=topID ) {
			//if( surfID >=0 && surfID < SURFMAN_MAX_SURFACES-1 ) {
			if( surfID >=0 && surfID<SURFMAN_MAX_SURFACES-1 && surfID != topID ) {
				egi_dpstd("Touch nonTOP surfID=%d (toPID=%d)\n", surfID, topID);
				//pmostat->LeftKeyDownHold = false; /* XXX This will trigger surface_move. OK, LeftKeyDownHold ruled out! */
				// pmostat->nch = 0;		  /* Ok, cleared in W.2.7.1 */
				if( ering_msg_send( surfman->surfaces[surfID]->csFD,
					emsg, ERING_MOUSE_STATUS, pmostat, sizeof(EGI_MOUSE_STATUS) ) <=0 ) {
					egi_dpstd("Fail to sendmsg ERING_MOUSE_STATUS!\n");
				}
			}
	}
#endif

	/* ------ <<<  Surfman Critical Zone  */
			pthread_mutex_unlock(&surfman->surfman_mutex);

#endif /* END 5. send mostat */

END_MOUSE_EVENT:
			/* Reset lastX/Y, to compare with next mouseX/Y to get deviation. */
               	        lastX=pmostat->mouseX;
                       	lastY=pmostat->mouseY;

			/* Put request */
			egi_mouse_putRequest(pmostat); /* Unlock mutex meanwihle */

                }  /* End of  if( egi_mouse_getRequest(pmostat) ) {...}  */

		/* W3. ELSE: egi_mouse_getRequest(pmostat)==false;  the pmostat MAY be skipped/missed?
		 *     Example: when click on TOPBTN_MIN, it DOESN'T react.
		 */
		else {
			//egi_dpstd("Fail getRequest(pmostat)!\n");
			//goto WAIT_REQUEST;
			goto KEY_INPUT;
		}

		/* W4. Waitpit child */
		if( (wait_pid=waitpid(-1, &wait_status, WNOHANG)) >0 ) {
			egi_dpstd("A child process PID=%d exits %s, wait_status=%d!\n",
					wait_pid, WIFEXITED(wait_status)?"NORMALLY":"ABNORMALLY!",  wait_status);
		}

		tm_delayms(5);

   	} /* End while() */

	/* Free SURFMAN */
	egi_destroy_surfman(&surfman);
	//egi_imgbuf_free(mcursor)(mgrab);

        /* Reset termI/O */
        egi_reset_termios();

        /* <<<<<  EGI general release   >>>>>> */
        // Ignore;

	exit(0);
}


/*------------------------------------------------------------------------------
		Callback for mouse input
Callback runs in MouseRead thread.
1. Just to pass out mouse data.
2. Set request.

--------------------------------------------------------------------------------*/
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus)
{

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

	/* XXX NOTE:
	 * It goes to mouse event select again,  and when mouse_loopread thread reads next mouse BEFORE
	 * main thread locks/pares above passed mostatus, it will MISS it then!
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



