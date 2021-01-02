/*---------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of a simple mimic Xterminal.

			----- Control KEYs -----

		( --- 0. Shell command --- )
CTRL+C  To stop currently running shell command
CTRL+D  To exit Xterminal

                ( --- 1. The Editor: Keys and functions --- )
Mouse move:
Mouse scroll:   Scroll charmap up and down.
ARROW_KEYs:     Shift typing cursor to locate insert/delete position.(Only command input line )
BACKSPACE:      Delete chars before the cursor.
DEL:            Delete chars following the cursor.
HOME(or Fn+LEFT_ARROW!):        Return cursor to the begin of the retline.
END(or Fn+RIGHT_ARROW!):        Move cursor to the end of the retline.
CTRL+N:         Scroll down 1 line, keep typing cursor position.
CTRL+O:         Scroll up 1 line, keep typing cursor position.
CTRL+H:         Go to the first page.
CTRL+E:         Go to the last page.
CTRL+F:		None.
PG_UP:          Page up
PG_DN:          Page down

		( --- 2. PINYIN Input Method --- )
Refer to: test_editor.c

Note:
1. For Xterm editor, ONLY command lines are editable!
   Editing_cursor is locked to prevent from moving up/down, and only allowed to
   shift left and right.

2. TODO: It can NOT parse/simulate terminal contorl character sequence, such as
   cursor movement, clear, color and font mode, etc.
2.1 Command 'ps': Can NOT display a complete line on LOCAL CONSOLE! ---OK, erase '\n'
2.2 Command 'ls(ps..) | more', sh: more: not found!  But | tai,head,grep is OK!
2.3 Command less,more

TODO:
1. Charmap fontcolor.
2. Charmap FT-face.

3. For charmap->txtbuff will be reallocated, reference(pointer) to it will be
   INVALID after re_calloc!!!
   So its initial value should be big enough in order to avoid further realloc.
   OR when mem is NOT enough, it MUST shift out old lines one by one.

4. To support:  cd, clear
6. Error display --- OK
7. Cursor shape and it should NOT appear at other place.
8. History --- OK
9. UFT8 cutout position!
10. Escape sequence parse,

Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>   /* open */
#include <unistd.h>  /* read */
#include <errno.h>
#include <ctype.h>
#include <linux/input.h>
#include <termios.h>
#include <sys/wait.h>
#include "egi_common.h"
#include "egi_input.h"
#include "egi_unihan.h"
#include "egi_FTcharmap.h"
#include "egi_FTsymbol.h"
#include "egi_cstring.h"
#include "egi_utils.h"
#include "egi_procman.h"

/* --- TERM settings --- */
struct termios old_isettings;
struct termios old_osettings;
struct termios new_isettings;
struct termios new_osettings;

/* --- Terminal appearance --- */
#define TERM_FONT_COLOR		WEGI_COLOR_GREEN
#define TERM_BKG_COLOR		COLOR_RGB_TO16BITS(100,5,46)

/* --- User Shell command --- */
#define SHELLCMD_MAX	256		/* Max length of a shell command */
#define HISTORY_MAX	10		/* Max. items of history commands saved */

UFT8_CHAR  ShellCmd[SHELLCMD_MAX];	/* The last user input shell command */
UFT8_CHAR  ShCmdHistory[HISTORY_MAX*SHELLCMD_MAX];  /* Shell command history */
unsigned int hisCnt;			/* History items counter */
int hisID;				/* History item index, for history command selection
					 * ALWAYS set hisID=hisCnt (latest ID+1), when a new command saved. */

UFT8_PCHAR pShCmd;			/* Start pointer of shell command string in charmap->txtbuff, after current Prompt_String.
				 	 * WARNING: charmap->txtbuff MUST NOT be reallocated further!!!
				 	*/

UFT8_CHAR strPrompt[64]="Neo# ";  /* Prompt_String, input hint for user. */


/* --- Shell command PID --- */
static UFT8_CHAR  obuf[1024];	/* User stdout buffer */
pid_t sh_pid;			/* child pid for shell command */
bool  sustain_cmdout;		/* 1. Set true to signal to continue reading output from the running shell command.
				 * 2. Set false to stop reading output from the running shell command, if it's too tedious.
				 * 3. If it's TURE, other thread(mousecall) MUST NOT edit charmap at the same time!
 				 * Example: To scroll down the champ while the main thread is just writing shell output to chmap,
				 * it raises a race condition and crash the charmap data!
				 */


/* --- TTY input escape sequences.  USB Keyboard hidraw data is another story...  */
#define		TTY_ESC		"\033"
#define		TTY_UP		"\033[A"
#define		TTY_DOWN	"\033[B"
#define		TTY_RIGHT	"\033[C"
#define		TTY_LEFT	"\033[D"
#define		TTY_HOME	"\033[1~"
#define		TTY_HOME_1	"\033[H"
#define		TTY_INSERT	"\033[2~"
#define		TTY_DEL		"\033[3~"
#define		TTY_END		"\033[4~"
#define		TTY_END_1	"\033[F"
#define		TTY_PAGE_UP	"\033[5~"
#define		TTY_PAGE_DOWN	"\033[6~"

/* --- Some Shell TTY ASCII control key */
#define		CTRL_D	4	/* Ctrl + D: exit  */
#define		CTRL_N	14	/* Ctrl + N: scroll down  */
#define		CTRL_O	15	/* Ctrl + O: scroll up  */
#define 	CTRL_F  6	/* Ctrl + f: save file */
#define		CTRL_S  19	/* Ctrl + S */
#define		CTRL_E  5	/* Ctrl + e: go to last page */
#define		CTRL_H  8	/* Ctrl + h: go to the first dline, head of the txtbuff */
#define		CTRL_P  16	/* Ctrl + p: PINYIN input ON/OFF toggle */


/* --- Input Method: PINYIN --- */
#define SINGLE_PINYIN_INPUT	0       /* !0: Single chinese_phoneticizing input method.
					 *  0: Multiple chinese_phoneticizing input method.
					 */
wchar_t wcode;
EGI_UNIHAN_SET *uniset;
EGI_UNIHANGROUP_SET *group_set;
static bool enable_pinyin=false;  /* Enable input method PINYIN */
char strPinyin[4*8]={0}; 	  /* To store unbroken input pinyin string */
char group_pinyin[UNIHAN_TYPING_MAXLEN*4]; 	/* For divided group pinyins */
EGI_UNICODE group_wcodes[4];			/* group wcodes  */
char display_pinyin[UNIHAN_TYPING_MAXLEN*4];    /* For displaying */
unsigned int pos_uchar;
char unihans[512][4*4]; //[256][4]; /* To store UNIHANs/UNIHANGROUPs complied with input pinyin, in UFT8-encoding. 4unihans*4bytes */
EGI_UNICODE unicodes[512]; /* To store UNICODE of unihans[512] MAX for typing 'ji' totally 298 UNIHANs */
unsigned int unindex;
char phan[4]; 		/* temp. */
int  unicount; 		/* Count of unihan(group)s found */
char strHanlist[128]; 	/* List of unihans to be displayed for selecting, 5-10 unihans as a group. */
char strItem[32]; 	/* buffer for each UNIHAN with preceding index, example "1.啊" */
int  GroupStartIndex; 	/* Start index for displayed unihan(group)s */
int  GroupItems;	/* Current unihan(group)s items in selecting panel */
int  GroupMaxItems=7; 	/* Max. number of unihan(group)s displayed for selecting */
int  HanGroups=0; 	/* Groups with selecting items, each group with Max. GroupMaxItems unihans.
	          	 * Displaying one group of unihans each time on the PINYIN panel
		  	 */
int  HanGroupIndex=0;	/* group index number, < HanGroups */


/* --- CHARMAP --- */
#define 	CHMAP_TXTBUFF_SIZE	1024 /* 256,text buffer size for EGI_FTCHAR_MAP.txtbuff */
#define		CHMAP_SIZE		2048 /* NOT less than Max. total number of chars (include '\n's and EOF) that may displayed in the txtbox */

/* NOTE: char position, in respect of txtbuff index and data_offset: pos=chmap->charPos[pch] */
static EGI_FTCHAR_MAP *chmap;	/* FTchar map to hold char position data */
static unsigned int chns;	/* total number of chars in a string */
static int fw=18;		/* Font size */
static int fh=20;
static int fgap=2; //20/4;	/* Text line fgap : TRICKY ^_- */
static int tlns;  //=(txtbox.endxy.y-txtbox.startxy.y)/(fh+fgap); /* Total available line space for displaying txt */
static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 240-1} };	/* Text displaying area, To adjust later ... */
static int smargin=5; 		/* left and right side margin of text area */
static int tmargin=2;		/* top margin of text area */


/* --- Mouse --- */
static bool mouseLeftKeyDown;
static bool mouseLeftKeyUp;
static bool start_pch=false;	/* True if mouseLeftKeyDown switch from false to true */
static int  mouseX, mouseY, mouseZ; /* Mouse point position */
static int  mouseMidX, mouseMidY;   /* Mouse cursor mid position */
static int  menuX0, menuY0;


/* Component elements in the APP, Only one of them is active at one point. */
enum CompElem
{
	CompNone=0,
	CompTXTBox,	/* Txtbox */
	CompRCMenu,	/* Right click menu */
	CompWTBMenu,	/* Window top bar menu */
};
static enum CompElem ActiveComp;  /* Active component element */

/* Right_Click Menu Command */
enum RCMenu_Command {
	RCMENU_COMMAND_NONE	=-1,
	RCMENU_COMMAND_SAVEWORDS=0,	/* Save new words for PINYIN */
	RCMENU_COMMAND_COPY	=1,
	RCMENU_COMMAND_PASTE	=2,
	RCMENU_COMMAND_CUT	=3,
};
static int RCMenu_Command_ID=RCMENU_COMMAND_NONE;

/* Window_Bar_Top Menu Command */
enum WBTMenu_Command {
	WBTMENU_COMMAND_NONE 	=-1,
	WBTMENU_COMMAND_OPEN 	=0,
	WBTMENU_COMMAND_SAVE 	=1,
	WBTMENU_COMMAND_EXIT 	=2,
	WBTMENU_COMMAND_ABORT 	=3,
	WBTMENU_COMMAND_HELP 	=4,
	WBTMENU_COMMAND_RENEW 	=5,
	WBTMENU_COMMAND_ABOUT 	=6,
};
static int WBTMenu_Command_ID=WBTMENU_COMMAND_NONE;

/* Functions */
static int FTcharmap_writeFB(FBDEV *fbdev, int *penx, int *peny);
static void FTcharmap_goto_end(void);
static void FTsymbol_writeFB(const char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny);
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus);
static void draw_mcursor(int x, int y);
void signal_handler(int signal);
void arrow_down_history(void);  /* In_process function */
void arrow_up_history(void);	/* In_process function */
void execute_shellcmd(void);	/* In_process function */


/* ============================
	    MAIN()
============================ */
int main(int argc, char **argv)
{

 /* <<<<<<  EGI general init  >>>>>> */

  /* Start sys tick */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();

  #if 1 /* Set signal action */
  if(egi_common_sigAction(SIGINT, signal_handler)!=0) {
	printf("Fail to set sigaction!\n");
	return -1;
  }
  #endif

  #if 0
  /* Start egi log */
  printf("egi_init_log()...\n");
  if(egi_init_log("/mmc/log_scrollinput")!=0) {
        printf("Fail to init egi logger, quit.\n");
        return -1;
  }

  /* Load symbol pages */
  printf("FTsymbol_load_allpages()...\n");
  if(FTsymbol_load_allpages() !=0) /* FT derived sympg_ascii */
        printf("Fail to load FTsym pages! go on anyway...\n");
  printf("symbol_load_allpages()...\n");
  if(symbol_load_allpages() !=0) {
        printf("Fail to load sym pages, quit.\n");
        return -1;
  }
  #endif

  /* Load freetype fonts */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
        printf("Fail to load FT sysfonts, quit.\n");
        return -1;
  }
  FTsymbol_set_SpaceWidth(1);
  FTsymbol_set_TabWidth(4.5);


  #if 0
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
        printf("Fail to load FT appfonts, quit.\n");
        return -1;
  }


  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
        return -1;
  #endif

  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;

  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,false);
  fb_position_rotate(&gv_fb_dev,0);

  /* Set mouse callback function and start mouse readloop */
  egi_start_mouseread(NULL, mouse_callback);

 /* <<<<<  End of EGI general init  >>>>>> */



                  /*-----------------------------------
                   *            Main Program
                   -----------------------------------*/


	char ch;
	int  j,k;
	int lndis; /* Distance between lines */

	/* Load unihan group set */
	group_set=UniHanGroup_load_set(UNIHANGROUPS_DATA_PATH);
	if(group_set==NULL)
		exit(EXIT_FAILURE);

        /* QuckSort typing for PINYIN input */
        if( UniHanGroup_quickSort_typings(group_set, 0, group_set->ugroups_size-1, 10) !=0 ) {
                printf("Fail to quickSort typings for group_set!\n");
		exit(EXIT_FAILURE);
        }

MAIN_START:
	/* Reset main window size */
	txtbox.startxy.x=0;
	txtbox.startxy.y=30;
	txtbox.endxy.x=gv_fb_dev.pos_xres-1;
	txtbox.endxy.y=gv_fb_dev.pos_yres-1;

	/* Activate main window */
	ActiveComp=CompTXTBox;

	/* Total available lines of space for displaying chars */
	lndis=fh+fgap;
	//lndis=FTsymbol_get_FTface_Height(egi_sysfonts.regular, fw, fh);
	tlns=(txtbox.endxy.y-txtbox.startxy.y+1)/lndis;
	printf("Blank lines tlns=%d\n", tlns);

	/* Create FTcharmap */
	chmap=FTcharmap_create( CHMAP_TXTBUFF_SIZE, txtbox.startxy.x, txtbox.startxy.y,		/* txtsize,  x0, y0  */
		  		//tlns*lndis, txtbox.endxy.x-txtbox.startxy.x+1, smargin, tmargin,      /*  height, width, offx, offy */
		  	txtbox.endxy.y-txtbox.startxy.y+1, txtbox.endxy.x-txtbox.startxy.x+1, smargin, tmargin,      /*  height, width, offx, offy */
				CHMAP_SIZE, tlns, gv_fb_dev.pos_xres-2*smargin, lndis);   /* mapsize, lines, pixpl, lndis */
	if(chmap==NULL){ printf("Fail to create char map!\n"); exit(0); };
	/* Set color */
	chmap->bkgcolor=TERM_BKG_COLOR;
	chmap->fontcolor=TERM_FONT_COLOR;

	/* NO: Load file to chmap */


        /* Init. mouse position */
        mouseX=gv_fb_dev.pos_xres/2;
        mouseY=gv_fb_dev.pos_yres/2;

        /* Init. FB working buffer */
        fb_clear_workBuff(&gv_fb_dev, TERM_BKG_COLOR);

	/* Title */
	const char *title="EGI: 迷你终端"; // "EGI Mini Terminal";
	int penx=140, peny=3;
	int pixlen;
	pixlen=FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fw, fh, (const UFT8_PCHAR)title);
	FTsymbol_writeFB((char *)title,(320-pixlen)/2,peny,WEGI_COLOR_GRAY2, &penx, &peny);

#if 0 /* ----TEST: Advances  FTcharmap_uft8strings_writeFB() with/withou fb_dev, diff of advances! */
	//const char*Teststr="Linux SEER 3.18.29 #29 Wed Sep 30 14:38:04 CST 2020 mips GNU/Linux\n";
	const char *Teststr="0123456789";

	penx=0; peny=0;
        FTsymbol_writeFB(Teststr,penx,peny,WEGI_COLOR_LTBLUE, &penx, &peny);
        printf("FTsymbol_writFB get advanceX =%d\n", penx);
        printf("FTsymbol_uft8string_getAdvances =%d\n",
                FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fw, fh, (const UFT8_PCHAR)Teststr) );


	//chmap->pchoff=0; chmap->pchoff2=0;
	FTcharmap_insert_string(chmap, (UFT8_PCHAR)Teststr,strlen(Teststr));

	penx=0; peny=0;
	FTcharmap_writeFB(&gv_fb_dev, &penx, &peny);
	printf("fb_dev !=NULL: penx=%d, peny=%d\n", penx, peny);

	penx=0; peny=0;
	FTcharmap_writeFB(NULL, &penx, &peny);
	printf("fb_dev ==NULL: penx=%d, peny=%d\n", penx, peny);
	fb_render(&gv_fb_dev);
	exit(0);
#endif

	/* Set Prompt_String */
	//strncpy(strPrompt, getenv("PS1"), sizeof(strPrompt)-1);
	FTcharmap_insert_string(chmap,strPrompt,strlen((char*)strPrompt));

	/* Set termio */
        tcgetattr(STDIN_FILENO, &old_isettings);
        new_isettings=old_isettings;
        new_isettings.c_lflag &= (~ICANON);      /* disable canonical mode, no buffer */
        new_isettings.c_lflag &= (~ECHO);        /* disable echo */
        new_isettings.c_cc[VMIN]=0;		 /* Nonblock */
        new_isettings.c_cc[VTIME]=0;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_isettings);

        tcgetattr(STDOUT_FILENO, &old_osettings);
	new_osettings=old_osettings;

	/* To fill initial charmap and get penx,peny */
	FTcharmap_writeFB(&gv_fb_dev,  NULL, NULL);
	fb_render(&gv_fb_dev);

	/* Charmap to the end */
	int i=0;
	while( chmap->pref[chmap->charPos[chmap->chcount-1]] != '\0' )
	{
		FTcharmap_page_down(chmap);
		FTcharmap_writeFB(NULL, NULL, NULL);
		//FTcharmap_writeFB(&gv_fb_dev, NULL,NULL);

		if( chmap->txtdlinePos[chmap->txtdlncount] > chmap->txtlen/100 )
			break;

		if( i==20 ) {  /* Every time when i counts to 20 */
		   FTcharmap_writeFB(&gv_fb_dev, NULL,NULL);
		   fb_render(&gv_fb_dev);
		   i=0;
		}
		i++;
	}
        fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
	//goto MAIN_END;

	/* Move cursor to end, just after Prompt_String */
	FTcharmap_goto_lineEnd(chmap);
	/* Set start pointer of shell command */
	pShCmd=chmap->txtbuff+chmap->txtlen;

	/* Loop editing ...   Read from TTY standard input, without funcion keys on keyboard ...... */
	while(1) {

		/* Execute Shell command, record history, and get output */
		execute_shellcmd();

	/* ---- OPTION 1: Read more than N bytes each time, nonblock. !!! NOT good, response slowly, and may cause parse errors !!!  -----*/
	/* ---- OPTION 2: Read in char one by one, nonblock  -----*/
		ch=0;
		read(STDIN_FILENO, &ch, 1);
		if(ch>0) {
		  if(isalpha(ch))
			printf("'%c'\n",ch);
		  else
		  	printf("ch=%d\n",ch);
		}
		//continue;

		/* 1. Parse TTY input escape sequences */
		if(ch==27) {  /* ESCAPE */
			ch=0;
			read(STDIN_FILENO,&ch, 1);
			printf("ch=%d\n",ch);
			if(ch==91) {  /* [ */
				ch=0;
				read(STDIN_FILENO,&ch, 1);
				printf("ch=%d\n",ch);
				switch ( ch ) {
					case 49:
						read(STDIN_FILENO,&ch, 1);
						/* mkeyboard HOME  move cursor to the first of the line */
						if( ch == 126 ) {
							FTcharmap_goto_lineBegin(chmap);
						}
                                                break;
					case 51: /* DEL */
						read(STDIN_FILENO,&ch, 1);
		                                printf("ch=%d\n",ch);
						if( ch ==126 ) /* Try to delete a char pointed by chmap->pch */
							FTcharmap_delete_string(chmap);
						break;
					case 52: /* END */
						read(STDIN_FILENO,&ch, 1);
						/* mkeyboard END  move cursor to the first of the line */
						if( ch == 126 ) {
							FTcharmap_goto_lineEnd(chmap);
						}
                                                break;
					case 53: /* PAGE UP */
						read(STDIN_FILENO,&ch, 1);
						if( ch == 126 ) {
							FTcharmap_page_up(chmap);
						}
                                                break;
					case 54: /* PAGE DOWN */
						read(STDIN_FILENO,&ch, 1);
						if( ch == 126 ) {
							FTcharmap_page_down(chmap);
						}
                                                break;

					case 65:  /* ARROW UP */
						/* Shift displaying UNIHAN group in the PINYIN panel */
						#if SINGLE_PINYIN_INPUT  /* PINYIN Input for single UNIHANs */
						if( enable_pinyin && strPinyin[0]!='\0' ) {
							if(HanGroupIndex > 0)
								HanGroupIndex--;
						}
						#else /* MULTIPLE_PINYIN_INPUT for UNIHANGROUPs (include UNIHANs) */
						if( enable_pinyin && strPinyin[0]!='\0' ) {
							for( i=7; i>0; i--) {
								if(GroupStartIndex-i < 0)
									continue;
								bzero(strHanlist, sizeof(strHanlist));
								for(j=0; j<i; j++) {
					 			     snprintf(strItem,sizeof(strItem),"%d.%s ",j+1, unihans[GroupStartIndex-i+j]);
								     strcat(strHanlist, strItem);
								}
								/* Check if space is OK */
								if(FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fw, fh,
														(UFT8_PCHAR)strHanlist) < 320-15 )
									break;
							}
							/* Reset new GroupStartIndex and GroupItems */
							GroupStartIndex -= i;
                                                }
						#endif

						/* Get history command upward(old) */
						arrow_up_history();


						break;
					case 66:  /* ARROW DOWN */
						/* Shift displaying UNIHAN group in PINYIN panel */
						#if SINGLE_PINYIN_INPUT  /* PINYIN Input for single UNIHANs */
						if( enable_pinyin && strPinyin[0]!='\0' ) {
							if(HanGroupIndex < HanGroups-1)
								HanGroupIndex++;
						}
						#else /* MULTIPLE_PINYIN_INPUT for UNIHANGROUPs (include UNIHANs) */
						if( enable_pinyin && strPinyin[0]!='\0' ) {
                                                        if( GroupStartIndex+GroupItems < unicount )
                                                                GroupStartIndex += GroupItems;
                                                }
						#endif

						/* Get history command downward(new) */
						arrow_down_history();

						break;
					case 67:  /* RIGHT arrow : move cursor one char right, Limit to the last [pch] */
						FTcharmap_shift_cursor_right(chmap);
						break;
					case 68: /* LEFT arrow : move cursor one char left */
						if(pShCmd!=chmap->txtbuff+chmap->pchoff) { /* Stop at Prompt_String */
							FTcharmap_shift_cursor_left(chmap);
						}
						break;
					case 70: /* END : move current cursor to the end of the dline */
						FTcharmap_goto_lineEnd(chmap);
						break;

					case 72: /* HOME : move cursor to the begin of the dline */
						FTcharmap_set_pchoff( chmap,pShCmd-chmap->txtbuff,pShCmd-chmap->txtbuff );
						break;
				}
				/* reset 0 to ch: NOT a char, just for the following precess to ignore it.  */
				ch=0;
			}
			else if(ch==0) { /* ESCAP KEY: ch1==27 AND ch2==0 */
				if(enable_pinyin) {
					/* Clear all pinyin buffer */
					bzero(strPinyin,sizeof(strPinyin));
					bzero(display_pinyin,sizeof(display_pinyin));
					unicount=0; HanGroupIndex=0;
				}
			}
			/* ELSE: ch1==27 and (ch2!=91 AND ch2!=0)  */
		}
		/* ELSE: ch1 != 27, go on...  */

		/* 2. TTY Controls  */
		switch( ch )
		{
			case CTRL_D:
				system("clear");
				exit(0);
		    	case CTRL_P:
				enable_pinyin=!enable_pinyin;  /* Toggle input method to PINYIN */
				if(enable_pinyin) {
					/* Shrink maplines, maplinePos[] mem space Ok, increase maplines NOT ok! */
					if(chmap->maplines>2) {
						chmap->maplines -= 2;
						chmap->height=chmap->maplines*lndis; /* Reset chmap.height, to limit chmap.fb_render() */
					}
					/* Clear input pinyin buffer */
					bzero(strPinyin,sizeof(strPinyin));
					bzero(display_pinyin,sizeof(display_pinyin));
				}
				else {
					/* Recover maplines */
					chmap->maplines+=2;
					//chmap->height=chmap->maplines*lndis;  /* restor chmap.height */
					chmap->height=txtbox.endxy.y-txtbox.startxy.y+1;
					unicount=0; HanGroupIndex=0;	/* clear previous unihans */
				}
				ch=0;
			   	break;
			case CTRL_H:
				FTcharmap_goto_firstDline(chmap);
				ch=0;
				break;
			case CTRL_E:
				//FTcharmap_goto_firstDline(chmap);
				FTcharmap_goto_end();
				ch=0;
				break;
			case CTRL_N:			/* Pan one line up, until the last line. */
				FTcharmap_scroll_oneline_down(chmap);
				//FTcharmap_set_pref_nextDispLine(chmap);
				ch=0; /* to ignore  */
				break;
			case CTRL_O:
				FTcharmap_scroll_oneline_up(chmap);
				ch=0;
				break;
			case CTRL_F:
				/* Save PINYIN data first */
				break;
		}

		/* 3. Edit text */

		/* --- EDIT:  3.1 Backspace */
		if( ch==0x7F ) { /* backspace */
			if(enable_pinyin && strlen(strPinyin)>0 ) {
				strPinyin[strlen(strPinyin)-1]='\0';
				/* Since strPinyin updated: To find out all unihans complying with input pinyin  */
				HanGroupIndex=0;
			        #if SINGLE_PINYIN_INPUT  /* PINYIN Input for single UNIHANs */
				if( UniHan_locate_typing(uniset, strPinyin) == 0 ) {
					unicount=0;
					/* Put all complied UNIHANs to unihans[][4] */
					bzero(&unihans[0][0], sizeof(unihans));
		       			while( strncmp( uniset->unihans[uniset->puh].typing, strPinyin, UNIHAN_TYPING_MAXLEN ) == 0 ) {
						//char_unicode_to_uft8(&(uniset->unihans[uniset->puh].wcode), unihans[unicount]);
						strncpy(unihans[unicount],uniset->unihans[uniset->puh].uchar, UNIHAN_UCHAR_MAXLEN);
						unicount++;
                				uniset->puh++;
					}
					HanGroups=(unicount+GroupMaxItems-1)/GroupMaxItems;
        			}
			        #else /* MULTIPLE_PINYIN_INPUT for UNIHANGROUPs (include UNIHANs) */
			        UniHan_divide_pinyin(strPinyin, group_pinyin, UHGROUP_WCODES_MAXSIZE);
			        bzero(display_pinyin,sizeof(display_pinyin));
				for(i=0; i<UHGROUP_WCODES_MAXSIZE; i++) {
					strcat(display_pinyin, group_pinyin+i*UNIHAN_TYPING_MAXLEN);
					strcat(display_pinyin, " ");
				}
				unicount=0;
				if( UniHanGroup_locate_typings(group_set, group_pinyin)==0 ) {
					//unicount=0;
					k=0; //group_set->results_size;
					while( k < group_set->results_size ) {
						pos_uchar=group_set->ugroups[group_set->results[k]].pos_uchar;
						strcpy(unihans[unicount], (char *)group_set->uchars+pos_uchar);
						unicount++;
						if(unicount > sizeof(unihans)/sizeof(unihans[0]) )
                                                               break;
						k++;
					}
				 }
				 GroupStartIndex=0;GroupItems=0; /* reset group items */
				#endif
			}
			else  { /* strPinyin is empty */
				if(pShCmd!=chmap->txtbuff+chmap->pchoff) /* Stop at Prompt_String */
					FTcharmap_go_backspace( chmap );
			}
			ch=0;
		}
		/* --- EDIT:  3.2 Insert chars, !!! printable chars + '\n' + TAB  !!! */
		else if( ch>31 || ch==9 || ch==10 ) {  //pos>=0 ) {   /* If pos<0, then ... */

			/* 3.2.1 PINYING Input, put to strPinyin[]  */
	   		if( enable_pinyin ) {  //&& ( ch==32 || isdigit(ch) || isalpha(ch) ) ) {
				/* SPACE: insert SAPCE */
				if( ch==32 ) {
					/* If strPinyin is empty, insert SPACE into txt */
					if( strPinyin[0]=='\0' ) {
						FTcharmap_insert_char( chmap, &ch);
					/* Else, insert the first unihan in the displaying panel  */
					} else {
						#if SINGLE_PINYIN_INPUT /* PINYIN Input for single UNIHANs */
						FTcharmap_insert_char( chmap, &unihans[HanGroupIndex*GroupMaxItems][0]);
						#else /* MULTIPLE_PINYIN_INPUT for UNIHANGROUPs (include UNIHANs) */
                                                FTcharmap_insert_string( chmap, (UFT8_PCHAR)unihans[GroupStartIndex], strlen(unihans[0]) );
						#endif
						/* Clear strPinyin and unihans[]  */
						bzero(strPinyin,sizeof(strPinyin));
						unicount=0;
						//bzero(&unihans[0][0], sizeof(unihans));
						bzero(display_pinyin,sizeof(display_pinyin));
						GroupStartIndex=0; GroupItems=0; /* reset group items */
					}
				}
				/* DIGIT: Select unihan OR insert digit */
				else if( isdigit(ch) ) { //&& (ch-48>0 && ch-48 <= GroupMaxItems) ) {   /* Select PINYIN result 48-'0' */
					#if SINGLE_PINYIN_INPUT /* PINYIN Input for single UNIHANs */
					/* Select unihan with index (from 1 to 7) */
					if(strPinyin[0]!='\0' && (ch-48>0 && ch-48 <= GroupMaxItems) ) {   /* unihans[] NOT cleared */
						unindex=HanGroupIndex*GroupMaxItems+ch-48-1;
					   	FTcharmap_insert_char( chmap, &unihans[unindex][0]);

						/* Increase freq weigh value */
						printf("Select unicodes[%d]: U+%X\n", unindex, unicodes[unindex]);
						UniHan_increase_freq(uniset, strPinyin, unicodes[unindex], 10);

						/* Clear strPinyin and unihans[] */
						bzero(strPinyin,sizeof(strPinyin));
						unicount=0;
						//bzero(&unihans[0][0], sizeof(unihans));
					}
					#else   /* MULTIPLE_PINYIN_INPUT for UNIHANGROUPs (include UNIHANs) */
                                        if(strPinyin[0]!='\0' && (ch-48>0 && ch-48 <= GroupItems) ) {   /* unihans[] NOT cleared */
                                                unindex=GroupStartIndex+ch-48-1;  /* for UNIHANGROUPs GroupMaxItems is number of displayed items */
                                                FTcharmap_insert_string( chmap, (UFT8_PCHAR)&unihans[unindex][0], strlen(&unihans[unindex][0]) );

						/* Increase freq weigh value */
						bzero(group_wcodes, sizeof(group_wcodes));
						printf(">> %s\n",(UFT8_PCHAR)&unihans[unindex][0]);
						int nch=0;
						nch=cstr_uft8_to_unicode((UFT8_PCHAR)&unihans[unindex][0], group_wcodes);
						printf("nch=%d\n",nch);
						if(nch>0)
							UniHanGroup_increase_freq(group_set, group_pinyin, group_wcodes, 2);

                                                /* Clear strPinyin and unihans[] */
                                                bzero(strPinyin,sizeof(strPinyin));
                                                unicount=0;
                                                //bzero(&unihans[0][0], sizeof(unihans));
						bzero(display_pinyin,sizeof(display_pinyin));
						GroupStartIndex=0;GroupItems=0; /* reset group items */
                                        }
					#endif
					else  {  /* Insert digit into TXT */
						bzero(phan,sizeof(phan));
						char_DBC2SBC_to_uft8(ch, phan);
						FTcharmap_insert_char(chmap, phan);
					}
				}
				/* ALPHABET: Input as PINYIN */
				else if( (isalpha(ch) && islower(ch)) || ch==0x27 ) {	/* ' as delimiter */
					/* Continue input to strPinyin ONLY IF space is available */
					if( strlen(strPinyin) < sizeof(strPinyin)-1 ) {
					    strPinyin[strlen(strPinyin)]=ch;
					    printf("strPinyin: %s\n", strPinyin);
					    /* Since strPinyin updated: To find out all unihans complying with input pinyin  */
					    HanGroupIndex=0;
					    /* Put all complied UNIHANs to unihans[][4*4] */
                                            bzero(&unihans[0][0], sizeof(unihans));
					    #if SINGLE_PINYIN_INPUT /* PINYIN Input for single UNIHANs  */
					    if( UniHan_locate_typing(uniset,strPinyin) == 0 ) {
						 unicount=0;
						 k=uniset->puh;
						 while( strncmp( uniset->unihans[k].typing, strPinyin, UNIHAN_TYPING_MAXLEN) == 0 ) {
							 //char_unicode_to_uft8(&(uniset->unihans[k].wcode), unihans[unicount]);
							 strncpy(unihans[unicount],uniset->unihans[k].uchar, UNIHAN_UCHAR_MAXLEN);
 							 unicodes[unicount]=uniset->unihans[k].wcode;
							 unicount++;
							 if(unicount > sizeof(unihans)/sizeof(unihans[0]) )
								break;
							 k++;
							 //uniset->puh++;
						}
						HanGroups=(unicount+GroupMaxItems-1)/GroupMaxItems;
					    }
					    #else /* MULTIPLE_PINYIN_INPUT for UNIHANGROUPs (include UNIHANs) */
					    UniHan_divide_pinyin(strPinyin, group_pinyin, UHGROUP_WCODES_MAXSIZE);
					    bzero(display_pinyin,sizeof(display_pinyin));
					    for(i=0; i<UHGROUP_WCODES_MAXSIZE; i++) {
						strcat(display_pinyin, group_pinyin+i*UNIHAN_TYPING_MAXLEN);
						strcat(display_pinyin, " ");
					    }
					    unicount=0; /* counter for unihan_groups  */
					    if( UniHanGroup_locate_typings(group_set, group_pinyin)==0 ) {
						//unicount=0;
						k=0; //group_set->results_size;
						while( k < group_set->results_size ) {
							pos_uchar=group_set->ugroups[group_set->results[k]].pos_uchar;
							strcpy(unihans[unicount], (char *)group_set->uchars+pos_uchar);
							unicount++;
							if(unicount > sizeof(unihans)/sizeof(unihans[0]) )
                                                                break;
							k++;
						}
					    }
					    GroupStartIndex=0;GroupItems=0; /* reset group items */
					    #endif
        				}
				}
				/* ELSE: convert to SBC case(full width) and insert to txt */
				else {
					/*  convert '.' to '。'*/
					if( ch=='.' ) {
						FTcharmap_insert_char(chmap, "。");
					}
					else {
						bzero(phan,sizeof(phan));
						char_DBC2SBC_to_uft8(ch, phan);
						FTcharmap_insert_char(chmap, phan);
					}
				}
				ch=0;
				//continue;
			}

			/* 3.2.2 Keyboard Direct Input */
			else
			{
				/* As end of shell command input */
				if( ch=='\n' ) {
/* TEST: ----- */
 printf("---Before cmd ret: maplines=%d, maplncount=%d, txtlncount=%d\n",
                                chmap->maplines, chmap->maplncount,chmap->txtdlncount );

					/* The ENTER MUST put at end of shell command! Whatever the cursor position.  */
					FTcharmap_goto_lineEnd(chmap); /* This MAY be wrong, if next line is out of dlinePos[] range! */
					FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
					//FTcharmap_insert_string(chmap,(UFT8_PCHAR)&ch,1);  /* follow cursor */
					FTcharmap_insert_char(chmap, &ch);  /* Follow cursor */
					FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
/* TEST: ----- */
 printf("---Aft cmd ret: maplines=%d, maplncount=%d, txtlncount=%d\n",
                                chmap->maplines, chmap->maplncount,chmap->txtdlncount );

					/* If an ENTER: End of shell command, ENTER not included in ShellCmd! */
					strncpy((char *)ShellCmd, (char *)pShCmd, SHELLCMD_MAX-1);
					printf("ShellCmd: %s\n", ShellCmd);
				}
				else
					FTcharmap_insert_char( chmap, &ch );
			}

			ch=0;
		}  /* END insert ch */

		/* EDIT:  Need to refresh EGI_FTCHAR_MAP after editing !!! .... */

		/* Image DivisionS: 1. File top bar  2. Charmap  3. PINYIN IME  4. Sub Menus */
	/* --- 1. File top bar */
		//draw_filled_rect2(&gv_fb_dev,WEGI_COLOR_GRAY4, 0, 0, gv_fb_dev.pos_xres-1, gv_fb_dev.pos_yres-1);
		draw_filled_rect2(&gv_fb_dev,WEGI_COLOR_GRAY4, 0, 0, txtbox.endxy.x, txtbox.startxy.y);
		FTsymbol_writeFB((char *)title,(320-pixlen)/2,peny,WEGI_COLOR_WHITE, &penx, &peny);
	/* --- 2. Charmap Display txt */
        	//fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
		FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
	/* --- 3. PINYING IME */
		if(enable_pinyin) {
			/* Back pad */
			//draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYB, 0, txtbox.endxy.y-60, txtbox.endxy.x, txtbox.endxy.y );
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYB, 0, txtbox.startxy.y+ chmap->height, txtbox.endxy.x, txtbox.endxy.y );

			/* Decoration for PINYIN bar */
			FTsymbol_writeFB("EGI全拼输入", 320-120, txtbox.endxy.y-60+3, WEGI_COLOR_ORANGE, NULL, NULL);
			fbset_color(WEGI_COLOR_WHITE);
			draw_line(&gv_fb_dev, 15, txtbox.endxy.y-60+3+28, 320-15,txtbox.endxy.y-60+3+28);

			#if SINGLE_PINYIN_INPUT /* PINYIN Input for single UNIHANs  */
			/* Display PINYIN */
			FTsymbol_writeFB(strPinyin, 15, txtbox.endxy.y-60+3, WEGI_COLOR_FIREBRICK, NULL, NULL);
			/* Display grouped UNIHANs for selecting  */
			if( unicount>0 ) {
				//printf("unicount=%d, HanGroups=%d, HanGroupIndex=%d\n", unicount, HanGroups,HanGroupIndex);
				bzero(strHanlist, sizeof(strHanlist));
				for( i=0; i < GroupMaxItems && i<unicount; i++ ) {
					unindex=HanGroupIndex*GroupMaxItems+i;
					if(unihans[unindex][0]==0) break;
					snprintf(strItem,sizeof(strItem),"%d.%s ",i+1,unihans[unindex]);
					strcat(strHanlist, strItem);
				}
				FTsymbol_writeFB(strHanlist, 15, txtbox.endxy.y-60+3+30, WEGI_COLOR_BLACK, NULL, NULL);
			}
			#else /* MULTIPLE_PINYIN_INPUT for UNIHANGROUPs (include UNIHANs) */
			/* Display PINYIN */
			FTsymbol_writeFB(display_pinyin, 15, txtbox.endxy.y-60+3, WEGI_COLOR_FIREBRICK, NULL, NULL);
			/* Display unihan(group)s for selecting */
			if( unicount>0 ) {
				bzero(strHanlist, sizeof(strHanlist));
				for( i=GroupStartIndex; i<unicount; i++ ) {
					snprintf(strItem,sizeof(strItem),"%d.%s ",i-GroupStartIndex+1,unihans[i]);
					strcat(strHanlist, strItem);
					/* Check if out of range */
					//if(FTsymbol_uft8strings_pixlen(egi_sysfonts.regular, fw, fh, (UFT8_PCHAR)strHanlist) > 320-15 )
					if(FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fw, fh, (UFT8_PCHAR)strHanlist) > 320-15 )
					{
						strHanlist[strlen(strHanlist)-strlen(strItem)]='\0';
						break;
					}
				}
				GroupItems=i-GroupStartIndex;
				FTsymbol_writeFB(strHanlist, 15, txtbox.endxy.y-60+3+30, WEGI_COLOR_BLACK, NULL, NULL);
			}
			#endif
		}

		/* POST:  check chmap->errbits */
		if( chmap->errbits ) {
			printf("WARNING: chmap->errbits=%#x \n",chmap->errbits);
		}

	   /* --- 4. Sub Menus : Handle Active Menu, active_menu_handle */
	   switch( ActiveComp )
	   {
		default:
			break;
	    } /* END Switch */

		//printf("Draw mcursor......\n");
       		draw_mcursor(mouseMidX, mouseMidY);

		/* Render and bring image to screen */
		fb_render(&gv_fb_dev);
		//tm_delayms(20);
	}


#if 0	/* <<<<<<<<<<<<<<<<<<<<    Read from USB Keyboard HIDraw   >>>>>>>>>>>>>>>>>>> */
	static char txtbuff[1024];	/* text buffer */
	int term_fd;
	int retval;
	struct timeval tmval;
	fd_set rfds;

	/* Open term */
	term_fd=open("/dev/hidraw0", O_RDONLY);
	if(term_fd<0) {
		printf("Fail open hidraw0!\n");
		exit(1);
	}

	while(1)
	{
                /* To select fd */
                FD_ZERO(&rfds);
                FD_SET( term_fd, &rfds);
                tmval.tv_sec=0;
                tmval.tv_usec=0;

                retval=select( term_fd+1, &rfds, NULL, NULL, &tmval );
                if(retval<0) {
                        //printf("%s: select event: errno=%d, %s \n",__func__, errno, strerror(errno));
                        continue;
                }
                else if(retval==0) {
                        //printf("Time out\n");
                        continue;
                }
                /* Read input fd and trigger callback */
                if(FD_ISSET(term_fd, &rfds)) {
			memset(txtbuff,0,8); //sizeof(txtbuff));
                        retval=read(term_fd, txtbuff, 8);
			printf("retval=%d: %d,%d,%d,%d, %d,%d,%d,%d\n",retval, txtbuff[0],txtbuff[1],txtbuff[2],txtbuff[3],
										txtbuff[4],txtbuff[5],txtbuff[6],txtbuff[7]);
		}
	}
#endif   /* <<<<<<<<<< HIDraw  >>>>>>>>>>>>>>> */

                  /*-----------------------------------
                   *         End of Main Program
                   -----------------------------------*/
MAIN_END:

	/* Reset termio */
        tcsetattr(0, TCSANOW,&old_isettings);

	/* My release */
	FTcharmap_free(&chmap);

goto MAIN_START;

	/* Free UNIHAN data */
	UniHan_free_set(&uniset);
	UniHanGroup_free_set(&group_set);


 /* <<<<<  EGI general release   >>>>>> */
 printf("FTsymbol_release_allfonts()...\n");
 FTsymbol_release_allfonts(); /* release sysfonts and appfonts */
 printf("symbol_release_allpages()...\n");
 symbol_release_allpages(); /* release all symbol pages*/
 printf("FTsymbol_release_allpage()...\n");
 FTsymbol_release_allpages(); /* release FT derived symbol pages: sympg_ascii */

 printf("fb_filo_flush() and release_fbdev()...\n");
 fb_filo_flush(&gv_fb_dev);
 release_fbdev(&gv_fb_dev);
 #if 0
 printf("release virtual fbdev...\n");
 release_virt_fbdev(&vfb);
 #endif
 printf("egi_end_touchread()...\n");
 egi_end_touchread();
 #if 0
 printf("egi_quit_log()...\n");
 egi_quit_log();
 #endif
 printf("<-------  END  ------>\n");

return 0;
}


/*-------------------------------------
        FTcharmap WriteFB TXT
@txt:   	Input text
@penx, peny:    pointer to pen X.Y.
--------------------------------------*/
static int FTcharmap_writeFB(FBDEV *fbdev, int *penx, int *peny)
{
	int ret;

       	ret=FTcharmap_uft8strings_writeFB( fbdev, chmap,          	   /* FBdev, charmap*/
                                           egi_sysfonts.regular, fw, fh,   /* fontface, fw,fh */
	                                   chmap->fontcolor, -1, 255,      	   /* fontcolor, transcolor,opaque */
                                           NULL, NULL, penx, peny);        /* int *cnt, int *lnleft, int* penx, int* peny */

        /* Double check! */
        if( chmap->chcount > chmap->mapsize ) {
                printf("WARNING:  chmap.chcount > chmap.mapsize=%d, some displayed chars has NO charX/Y data! \n",
													chmap->mapsize);
        }
        if( chmap->txtdlncount > chmap->txtdlines ) {
                printf("WARNING:  chmap.txtdlncount=%d > chmap.txtdlines=%d, some displayed lines has NO position info. in charmap! \n",
                                                                                	chmap->txtdlncount, chmap->txtdlines);
        }

	return ret>0?ret:0;
}

/*----------------------------------------
    Charmap to the last page of txtbuff.
TODO: mutex_lock + request
----------------------------------------*/
static void FTcharmap_goto_end(void)
{

#if 1   /* OPTION 1: charmap to txtbuff end */
        while( chmap->pref[chmap->charPos[chmap->chcount-1]] != '\0' )
        {
                FTcharmap_page_down(chmap);
                FTcharmap_writeFB(NULL, NULL, NULL);
	}

        /* Reset cursor position: pchoff/pchoff2 accordingly */
        //chmap->pchoff=chmap->pref-chmap->txtbuff+chmap->charPos[chmap->chcount-1];
	chmap->pchoff=chmap->txtlen;
        chmap->pchoff2=chmap->pchoff;
#else   /* OPTION 2:    */

#endif

	/* Display OR let MAIN to handle it */
	#if 0
        FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_BLACK, NULL,NULL);
        fb_render(&gv_fb_dev);
	#endif
}


/*-------------------------------------
        FTsymbol WriteFB TXT
@txt:   	Input text
@px,py: 	LCD X/Y for start point.
@color: 	Color for text
@penx, peny:    pointer to pen X.Y.
--------------------------------------*/
static void FTsymbol_writeFB( const char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny)
{
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        fw, fh,(const UFT8_PCHAR)txt,    	/* fw,fh, pstr */
                                        320, tlns, fgap,      /* pixpl, lines, fgap */
                                        px, py,                                 /* x0,y0, */
                                        color, -1, 255,      			/* fontcolor, transcolor,opaque */
                                        NULL, NULL, penx, peny);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}


/*-----------------------------------------------------------
		Callback for mouse input
Callback runs in MouseRead thread.
1. Just update mouseXYZ
2. Check and set ACTIVE token for menus.
3. If click in TXTBOX, set typing cursor or marks.
4. The function will be pending until there is a mouse event.
------------------------------------------------------------*/
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus)
{
	EGI_POINT  pxy={0,0};


        /* 1. Check mouse Left Key status */
	mouseLeftKeyDown=mostatus->LeftKeyDown; // || mostatus->LeftKeyDownHold;
	mouseLeftKeyUp=mostatus->LeftKeyUp;

	if( mostatus->LeftKeyDown ) {
		/* Set start_pch */
		if( mouseY > txtbox.startxy.y )
			start_pch=true;

                #if 0 /* Disable for TERM: (Activate window top bar menu)  */
                if( mouseY < txtbox.startxy.y && mouseX <112) //90 )
                      ActiveComp=CompWTBMenu;
		#endif

                /* Reset pch2 = pch */
                if( ActiveComp==CompTXTBox )
                       FTcharmap_reset_charPos2(chmap);
	}
	else if( mostatus->LeftKeyDownHold ) {
		start_pch=false; /* disable pch mark */
	}
	else if( mostatus->LeftKeyUp ) {
                start_pch=false;
	}

	/* Right click down */
       //if(mouse_data[0]&0x2) {
	if(mostatus->RightKeyDown ) {
                printf("Right key down!\n");
		pxy.x=mouseX; pxy.y=mouseY;
		if( point_inbox(&pxy, &txtbox ) ) {
			ActiveComp=CompRCMenu;
			menuX0=mouseX;
			menuY0=mouseY;
		}
	}

	/* Midkey down */
       //if(mouse_data[0]&0x4)
	if(mostatus->MidKeyDown)
                printf("Mid key down!\n");

        /*  2. Get mouse X,Y,DZ,Z */
	mouseX = mostatus->mouseX;
	mouseY = mostatus->mouseY;

	/* Scroll down/up */
	if( mostatus->mouseDZ > 0 && !sustain_cmdout ) {
		 FTcharmap_scroll_oneline_down(chmap);
	}
	else if ( mostatus->mouseDZ < 0 && !sustain_cmdout ) {
		 FTcharmap_scroll_oneline_up(chmap);
	}

      	mouseMidY=mostatus->mouseY+fh/2;
       	mouseMidX=mostatus->mouseX+fw/2;
//    	printf("mouseMidX,Y=%d,%d \n",mouseMidX, mouseMidY);

	#if 0 /* Disable for TERM/sustain_cmdout: (3. To get current cursor/pchoff position)  */
	if( (mostatus->LeftKeyDownHold || mostatus->LeftKeyDown) && ActiveComp==CompTXTBox ) {
		/* Set cursor position/Start of selection, LeftKeyDown */
		if(start_pch) {
		        /* continue checking request! */
           		while( FTcharmap_locate_charPos( chmap, mouseMidX, mouseMidY )!=0 ){ tm_delayms(5);};
           		/* set random mark color */
           		FTcharmap_set_markcolor( chmap, egi_color_random(color_medium), 80);
		}
		/* Set end of selection, LeftKeyDownHold */
		else {
		   	/* Continue checking request! */
		   	while( FTcharmap_locate_charPos2( chmap, mouseMidX, mouseMidY )!=0 ){ tm_delayms(5); } ;
		}
	}
	#endif
}

/*--------------------------------
 Draw cursor for the mouse movement
1. In txtbox area, use typing cursor.
2. Otherwise, apply mcursor.
----------------------------------*/
static void draw_mcursor(int x, int y)
{
//	int mode;
        static EGI_IMGBUF *mcimg=NULL;
        static EGI_IMGBUF *tcimg=NULL;
	EGI_POINT pt;

#ifdef LETS_NOTE
	#define MCURSOR_ICON_PATH 	"/home/midas-zhou/egi/mcursor.png"
	#define TXTCURSOR_ICON_PATH 	"/home/midas-zhou/egi/txtcursor.png"
#else
	#define MCURSOR_ICON_PATH 	"/mmc/mcursor.png"
	#define TXTCURSOR_ICON_PATH 	"/mmc/txtcursor.png"
#endif

        if(mcimg==NULL)
                mcimg=egi_imgbuf_readfile(MCURSOR_ICON_PATH);
	if(tcimg==NULL) {
		tcimg=egi_imgbuf_readfile(TXTCURSOR_ICON_PATH);
		egi_imgbuf_resize_update(&tcimg, fw, fh );
	}

	pt.x=mouseX;	pt.y=mouseY;  /* Mid */

	/* Always draw mouse in directFB mode */
	/* directFB and Non_directFB mixed mode will disrupt FB synch, and make the screen flash?? */
	//mode=fb_get_FBmode(&gv_fb_dev);
	//fb_set_directFB(&gv_fb_dev,true);

        /* OPTION 1: Use EGI_IMGBUF */

	/* If txtbox is NOT active */
	if( ActiveComp != CompTXTBox )
               	egi_subimg_writeFB(mcimg, &gv_fb_dev, 0, -1, mouseX, mouseY ); /* subnum,subcolor,x0,y0 */
	/* Editor box is active */
	else {
	    if( point_inbox(&pt, &txtbox) && tcimg!=NULL )
                egi_subimg_writeFB(tcimg, &gv_fb_dev, 0, WEGI_COLOR_RED, mouseX, mouseY ); /* subnum,subcolor,x0,y0 */

	   else if(mcimg)
             	egi_subimg_writeFB(mcimg, &gv_fb_dev, 0, -1, mouseX, mouseY ); /* subnum,subcolor,x0,y0 */
	}

        /* OPTION 2: Draw geometry */

	/* Restore FBmode */
	//fb_set_directFB(&gv_fb_dev, mode);
}


void signal_handler(int signal) {
	printf("signal=%d\n",signal);
	if(signal==SIGINT) {
		printf("--- SIGINT ----\n");
		sustain_cmdout=false;
		kill(sh_pid, SIGINT);
	}
}


/*-----------------------------------------------------------
		   An In_Process Function
Select history command Downward/Upward.
Note:
1. Delet current input and inster history command
2. ASSUE mouse action will NOT edit charmap!! No race condition.
3. Need to charmap to update.
-----------------------------------------------------------*/
void arrow_down_history(void)
{
 	/* Always set hisID==hisCnt after save a history cmd. */
	if( hisCnt>1 && hisID < hisCnt ) {
		hisID++;
		//printf("hisID=%d; hisCnt=%d\n", hisID, hisCnt);
		chmap->pchoff=pShCmd-chmap->txtbuff;
		chmap->pchoff2=chmap->txtlen;
		if( hisID < hisCnt )
		   	FTcharmap_insert_string(chmap, ShCmdHistory+hisID*SHELLCMD_MAX, strlen((char*)ShCmdHistory+hisID*SHELLCMD_MAX));
		else /* histID == histCnt */
			FTcharmap_delete_string(chmap);
	}
}
void arrow_up_history(void)
{
 	/* Always set hisID==hisCnt after save a history cmd. */
	if( hisCnt>0 && hisID > 0 ) {
		hisID--;
		chmap->pchoff=pShCmd-chmap->txtbuff;
		chmap->pchoff2=chmap->txtlen;
		FTcharmap_insert_string(chmap, ShCmdHistory+hisID*SHELLCMD_MAX, strlen((char*)ShCmdHistory+hisID*SHELLCMD_MAX));
	}
}


/*---------------------------------------
	   An In_Process Function
Execute shell command and read output.
Need to charmap to update.
----------------------------------------*/
void execute_shellcmd(void)
{
	int i;
	int stdout_pipe[2]={0}; /* For stdout io */
	int stderr_pipe[2]={0}; /* For stderr io */
	int status;
	ssize_t nread=0; //nread_out=0, nread_err=0;

	if( ShellCmd[0] ) {
		/* Get rid of NL token '\n' */
		ShellCmd[strlen((char *)ShellCmd)-1]=0;

		/* To lock any MOUSE operation. */
		sustain_cmdout=TRUE;
		printf("Execute shell cmd: %s\n", ShellCmd);

		/* Set termIO to Canonical mode for shell pid. */
	        new_isettings.c_lflag |= ICANON;      /* enable canonical mode, no buffer */
	        new_isettings.c_lflag |= ECHO;        /* enable echo */
	        tcsetattr(STDIN_FILENO, TCSANOW, &new_isettings);

		/* Create pipe */
		if(pipe(stdout_pipe)<0)
			printf("Fail to create stdout_pipe!\n");
		if(pipe(stderr_pipe)<0)
			printf("Fail to create stderr_pipe!\n");

		status=0;
		sh_pid=vfork();

		/* Child process to execute shell command */
		if( sh_pid==0) {
			//printf("Child process\n");

			/* Connect to stdout and stderr write_end */
			close(stdout_pipe[0]);  /* Close read_end */
			if(dup2(stdout_pipe[1],STDOUT_FILENO)<0)
				printf("Fail to call dup2 for stdout!\n");
			close(stderr_pipe[0]);  /* Close read_end */
			if(dup2(stderr_pipe[1],STDERR_FILENO)<0)
				printf("Fail to call dup2 for stderr!\n");

			/* Execute shell command, the last NL must included in ShellCmd */
			//system((char *)ShellCmd);
			execl("/bin/sh", "sh", "-c", (char *)ShellCmd, (char *)0);
		}
		/* Father process to read child stdout put */
		else if(sh_pid>0) {
			/* Save command to history, get rid of the last '\n' */
			if( hisCnt < HISTORY_MAX ) {
				/* Ingore repeated command, save command. */
				if( hisCnt==0 || strcmp((char *)ShCmdHistory+(hisCnt-1)*SHELLCMD_MAX, (char *)ShellCmd )!=0 ) {
					memset((char *)ShCmdHistory+hisCnt*SHELLCMD_MAX,0,SHELLCMD_MAX);
					strncpy( (char *)ShCmdHistory+hisCnt*SHELLCMD_MAX, (char *)ShellCmd, strlen((char *)ShellCmd)); /* -1 \n */
					hisCnt++;
					hisID=hisCnt; /* lastest history ID +1 */
				}
			}
			else {
				/* Discard the oldest history item, and save command to the last. */
				memmove((char *)ShCmdHistory, (char *)ShCmdHistory+SHELLCMD_MAX, SHELLCMD_MAX*(HISTORY_MAX-1));
				hisCnt--;
				if( strcmp((char *)ShCmdHistory+(hisCnt-1)*SHELLCMD_MAX, (char *)ShellCmd )!=0 ) {
					memset((char *)ShCmdHistory+hisCnt*SHELLCMD_MAX,0,SHELLCMD_MAX);
					strncpy( (char *)ShCmdHistory+hisCnt*SHELLCMD_MAX, (char *)ShellCmd, strlen((char *)ShellCmd)); /* -1 \n */
				}
				hisCnt++;
				hisID=hisCnt;  /* lastest history ID +1*/
			}
			/* TEST --------------- */
			printf("History:\n");
			for(i=0; i<hisCnt; i++)
				printf("%s\n",ShCmdHistory+i*SHELLCMD_MAX);
			printf("\n");

			/* Close pipe write_end, which is for the child */
			close(stdout_pipe[1]);
			close(stderr_pipe[1]);

			do {
		        	bzero(obuf,sizeof(obuf));
			        nread=read(stdout_pipe[0], obuf, sizeof(obuf)-1);
			        //printf("strlen=%d, nread=%d\n",strlen((char *)obuf), nread);
				//printf("obuf[0]=0x%02x obuf: %s\n",obuf[0],obuf);
				//printf("obuf[last-1]=0x%02x \n",obuf[nread-2]);
				//printf("obuf[last]=0x%02x \n",obuf[nread-1]);

				if(nread<=0)
				        nread=read(stderr_pipe[0], obuf, sizeof(obuf)-1);
				if(nread<=0)
					break;

/* TEST: ----- */
 printf("---Befor insert: pchoff=%u, pchoff2=%u, maplines=%d, maplncount=%d, txtlncount=%d\n",
                                chmap->pchoff, chmap->pchoff2, chmap->maplines, chmap->maplncount,chmap->txtdlncount );

				/* Copy results into chamap->txtbuff */
			       #if 0 /* TEST: ---------- */
				//const char *Hello="Sat_Dec_26_17_58_35_CST_2020\n";
				const char *Hello="Linux SEER2 3.18.29 #29 Wed Sep 30 14:38:04 CST 2020 mips GNU/Linux\n";
				FTcharmap_insert_string(chmap, (UFT8_PCHAR)Hello,strlen(Hello));
			       #else
				FTcharmap_insert_string(chmap,obuf,strlen((char *)obuf));
				chmap->follow_cursor=false;
			       #endif
				/* charmap to update current page, otherwise following FTcharmap_page_down() will fetch error data. */
        	        	FTcharmap_writeFB(&gv_fb_dev, NULL, NULL); /* TODO: self_cooked fw */

/* TEST: ----- */
 printf("---Aft insert: maplines=%d, maplncount=%d, txtlncount=%d\n",
                                chmap->maplines, chmap->maplncount,chmap->txtdlncount );

				/* Page_down to the last page, sustain_cmdout locked MOUSE operation! */
       				while( chmap->pref[chmap->charPos[chmap->chcount-1]] != '\0' )
        			{
					//printf("scroll oneline down!\n");
			 		//FTcharmap_scroll_oneline_down(chmap);
               				FTcharmap_page_down(chmap);
               				FTcharmap_writeFB(NULL, NULL, NULL);
					//fb_render(&gv_fb_dev);
				}

			    	#if 1 /* Adjust last page to a full page, let PS at bottom. */
       	      			FTcharmap_page_fitBottom(chmap);
       	       			FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
				fb_render(&gv_fb_dev);
			    	#endif

			} while( nread>0 && sustain_cmdout );

			/* User interrupt to end sh_pid, insert a LN */
		   	if(!sustain_cmdout) {
				FTcharmap_insert_string(chmap,(const UFT8_PCHAR)"\n",1); /* Mouse insert..ignored */
	                	FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
			}

			/* Not necessary? Err'Inappropriate ioctl for device' */
		   	if( wait(&status)!=0 ) {
				printf("Fail to wait() sh_pid! Err'%s'\n", strerror(errno) );
		   	}
		   	printf("sh_pid return with status:0x%04X\n",status);
		}
		/* sh_pid<0 */
		else {
			printf("Fail to call vfork!\n");
		}

		/* Release sustain_cmdout */
		sustain_cmdout=false;

		close(stdout_pipe[0]);
		close(stdout_pipe[1]);

#if 1		/* Insert new PS */
		printf("insert PS...\n");
		FTcharmap_insert_string(chmap,strPrompt,strlen((char *)strPrompt));
		//chmap->follow_cursor=false;
		FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
		printf("Ok\n");
#endif

      		FTcharmap_page_fitBottom(chmap);
       		FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
		fb_render(&gv_fb_dev);

		/* ReSet start pointer of shell command */
		pShCmd=chmap->txtbuff+chmap->txtlen;

		/* Clear ShellCmd */
		bzero(ShellCmd, sizeof(ShellCmd));

	}

	/* Set termIO to NON_Canonical mode */
	new_isettings.c_lflag &= (~ICANON);      /* disable canonical mode, no buffer */
        new_isettings.c_lflag &= (~ECHO);        /* disable echo */
	new_isettings.c_cc[VMIN]=0;		 /* Nonblock */
        new_isettings.c_cc[VTIME]=0;
	tcsetattr(STDIN_FILENO, TCSANOW, &new_isettings);
}
