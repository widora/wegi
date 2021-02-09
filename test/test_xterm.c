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
HOME(or Fn+LEFT_ARROW!):        Return cursor to the begin of the xterm inputting line.
END(or Fn+RIGHT_ARROW!):        Move cursor to the end of the xterm inputting line.
CTRL+N:         Scroll down 1 line, keep typing cursor position.
CTRL+O:         Scroll up 1 line, keep typing cursor position.
XXX CTRL+H:         Go to the first page.
XXX CTRL+E:         Go to the last page.
CTRL+F:		None.
PG_UP:          Page up
PG_DN:          Page down

		( --- 2. PINYIN Input Method --- )
Refer to: test_editor.c

Note:
1. For Xterm editor, ONLY command lines are editable!
   Editing_cursor is locked to prevent from moving up/down, and only allowed to
   shift left and right.

2. TODO: CSI of ISO/IEC-6429 (ANSI escape sequence)
   It can NOT parse/simulate terminal contorl character sequence, such as
   cursor movement, clear, color and font mode, etc.
   To parse Stander xterm 8 color ---OK

2.1 Command 'ps': Can NOT display a complete line on LOCAL CONSOLE! ---OK, erase '\n'
2.2 Command 'ls(ps..) | more', sh: more: not found!  But | tai,head,grep is OK!
2.3 Command less,more
2.4 Compound command will run at once, Example 'dmesg && date'

TODO:
1. Charmap fontcolor. ---OK
2. Charmap FT-face.

3. For charmap->txtbuff will be reallocated, reference(pointer) to it will be
   INVALID after re_calloc!!!
   So its initial value should be big enough in order to avoid further realloc.
   OR when mem is NOT enough, it MUST shift out old lines one by one.

4. To support:  cd, clear
6. Error display --- OK
7. Cursor shape and it should NOT appear at other place.
8. History --- OK
9. UFT8 cutout position! --- OK
10. Escape sequence parse,
11. obuf/linebuf memgrow().
12. TAB Completion: TAB not allowed for XTERM inputing, instead it's for auto. completing
    your commands.
13. 'clear' command in a shell scprit will erase the first output line from shell_pid.

Jurnal:
2021-10-18:
	1. Add FTcharmap_shrink_dlines().
	2. Add draw_ecursor().

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
#include <pty.h> 	/* -lutil */
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
#define XTERM_PS_COLOR		WEGI_COLOR_GREEN     /* Prompt string */
#define XTERM_SHCMD_COLOR	WEGI_COLOR_LTYELLOW  /* Input shell command */
#define XTERM_STDOUT_COLOR	WEGI_COLOR_WHITE
#define XTERM_STDERR_COLOR	WEGI_COLOR_RED
#define XTERM_BKG_COLOR		COLOR_RGB_TO16BITS(100,5,46)

#define XTERM_MAX_ROWS		50		    /* Max dlines in charmap */

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
char  obuf[EGI_LINE_MAX*4];	/* User stdout buffer */
pid_t sh_pid;			/* child pid for shell command */
bool  lock_mousectrl;		/* 1. If TRUE, LOCK mouse control for editting charmap.
 				 * Example: To lock scroll_down the champ while the main thread is just writing shell output to chmap.
				 */
bool  sustain_cmdout;		/* 1. Set true to signal to continue reading output from the running shell command.
				 * 2. Set false to stop reading output from the running shell command, if it's too tedious.
				 * 3. If it's TRUE, other thread(mousecall) MUST NOT edit charmap at the same time!
				 *    OR it raises a race condition and crash the charmap data!
				 */


/* --- TTY input escape sequences.  USB Keyboard hidraw data is another story...  */
#define		TTY_ESC		"\033"
#define		TTY_UP		"\033[A"
#define		TTY_DOWN	"\033[B"
#define		TTY_RIGHT	"\033[C"
#define		TTY_LEFT	"\033[D"
#define		TTY_HOME	"\033[1~"   /* Mkeyboard HOME */
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

/* --- Special keys --- */
#define		CH_TAB 9   	/* TAB for auto. completion */

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
static void draw_mcursor(int x, int y);			/* Selecting cursor */
static void draw_ecursor(int x, int y, int lndis);	/* Editing/blinking cursor */

void signal_handler(int signal);

/* TODO: Mutex_lock for chmap */
void arrow_down_history(void);  /* In_process function, NO mutex_lock for chmap! */
void arrow_up_history(void);	/* In_process function, NO mutex_lock fir chmap! */
void execute_shellcmd(void);	/* In_process function, NO mutex_lock fir chmap! */
void parse_shellout(char* pstr); /* Escape sequence parsing, NO mutex_lock fir chmap! */


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
	int  i,j,k;
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
			egi_sysfonts.regular, CHMAP_SIZE, tlns, gv_fb_dev.pos_xres-2*smargin, lndis,   /*  typeface, mapsize, lines, pixpl, lndis */
			XTERM_BKG_COLOR, XTERM_STDOUT_COLOR, true, false );      /*  bkgcolor, fontcolor, charColorMap_ON, hlmarkColorMap_ON */
	if(chmap==NULL){ printf("Fail to create char map!\n"); exit(0); };

	/* DIY Blinking cursor */
	chmap->draw_cursor=draw_ecursor;


	/////////////////    TEST: EGI_FONT_BUFF   ///////////////////
	if( argc >1 ) {
		printf("Start to buffer up fonts...\n");
	  	egi_fontbuffer=FTsymbol_create_fontBuffer(egi_sysfonts.regular, fw, fh, 0, 255);  /* face, fw, fh, unistart, size */
	  	//egi_fontbuffer=FTsymbol_create_fontBuffer(egi_sysfonts.regular, fw, fh, 0x4E00, 21000);  /* face, fw, fh, unistart, size */
	}

	/* NO: Load file to chmap */

        /* Init. mouse position */
        mouseX=gv_fb_dev.pos_xres/2;
        mouseY=gv_fb_dev.pos_yres/2;

        /* Init. FB working buffer */
        fb_clear_workBuff(&gv_fb_dev, XTERM_BKG_COLOR);

	/* Title */
	const char *title="EGI XTERM"; //"EGI: 迷你终端"; // "EGI Mini Terminal";
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
	chmap->precolor=XTERM_PS_COLOR;
	FTcharmap_insert_string(chmap,strPrompt,strlen((char*)strPrompt));


#if 1	/* Set termio */
        tcgetattr(STDIN_FILENO, &old_isettings);
        new_isettings=old_isettings;
        new_isettings.c_lflag &= (~ICANON);      /* disable canonical mode, no buffer */
        new_isettings.c_lflag &= (~ECHO);        /* disable echo */
        new_isettings.c_cc[VMIN]=0;		 /* Nonblock */
        new_isettings.c_cc[VTIME]=0;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_isettings);

        tcgetattr(STDOUT_FILENO, &old_osettings);
	new_osettings=old_osettings;
#endif

	/* To fill initial charmap and get penx,peny */
	FTcharmap_writeFB(&gv_fb_dev,  NULL, NULL);
	fb_render(&gv_fb_dev);

#if 0	/* Charmap to the end */
	i=0;
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
#endif

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
						/* Mkeyboard HOME  move cursor to the first of the line */
						if( ch == 126 ) {
							//FTcharmap_goto_lineBegin(chmap);
							/* Move cursor to pos just after PS! */
							FTcharmap_set_pchoff( chmap, pShCmd-chmap->txtbuff, pShCmd-chmap->txtbuff );
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

					case 72: /* HOME : move cursor to the begin of the xterm inputting dline. */
						FTcharmap_set_pchoff( chmap, pShCmd-chmap->txtbuff, pShCmd-chmap->txtbuff );
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
#if 0 ////////////(  NOT FOR XTERM   )///////////////////
			case CTRL_H:
				FTcharmap_goto_firstDline(chmap);
				ch=0;
				break;
			case CTRL_E:
				//FTcharmap_goto_firstDline(chmap);
				FTcharmap_goto_end();
				ch=0;
				break;
#endif /////////////////////////////////////////////////
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

			case CH_TAB:
				/* XTERM TAB Completion */
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
		//else if( ch>31 || ch==9 || ch==10 ) {  //pos>=0 ) {   /* If pos<0, then ... */
		else if( ch>31 || ch==10 ) {  /* TAB(9) not for XTERM inputting! */

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
				/* Input as shell command */
				else {
					chmap->precolor=XTERM_SHCMD_COLOR;
					FTcharmap_insert_char( chmap, &ch );
				}
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
                                           fw, fh,   			   /* fw,fh */
	                                   -1, 255,      		   /* transcolor,opaque */
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

	/* Check ROW limit */
	if(chmap->txtdlncount > XTERM_MAX_ROWS) {
		printf("\033[31m chmap->txtdlncount=%d > MAX_ROWS(%d), to shrink dlines...\n \033[0m", chmap->txtdlncount, XTERM_MAX_ROWS);
		FTcharmap_shrink_dlines(chmap, XTERM_MAX_ROWS/5); //chmap->txtdlncount-XTERM_MAX_ROWS);
		/* NOTE: always charmap after calling shrink_dlines() */
		FTcharmap_writeFB(fbdev, penx, peny);
		printf("\033[31m  Ok..finish shrink dlines chmap->txtdlncount=%d!\n \033[0m", chmap->txtdlncount);
	}

	return ret>0?ret:0;
}

/*-------  NOT For XTERM! -----------------
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
	if( mostatus->mouseDZ > 0 && !lock_mousectrl ) {
		 FTcharmap_scroll_oneline_down(chmap);
	}
	else if ( mostatus->mouseDZ < 0 && !lock_mousectrl ) {
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

/*---------------------------------------
Draw blinking/editting cursor.
----------------------------------------*/
static void draw_ecursor(int x, int y, int lndis )
{
	int w;
        fbset_color(WEGI_COLOR_GRAYB);
	UFT8_CHAR uchar[8]={0};
	int  len;

	if(chmap->pch>=0) {
#if 0	/* NOPE: the last char of a dline will get wrong w, as chmap->pch+1 will be the head of the dline! */
		if( chmap->pch < chmap->chcount-1)
			w=chmap->charX[chmap->pch+1]-chmap->charX[chmap->pch];
		else
			w=fw/2;
#endif
		/* Get width of a uchar */
		if( chmap->pch < chmap->chcount-1) {
			len=cstr_charlen_uft8(chmap->pref+chmap->charPos[chmap->pch]);
			if(len>0) {
				strncpy((char *)uchar,(char *)chmap->pref+chmap->charPos[chmap->pch], len);
			}
			w=FTsymbol_uft8string_getAdvances(chmap->face, fw, fh, uchar);
		}
		else
			w=fw/2;

		/* Get height of a uchar. looks not good for some typeface. */
		// int h=FTsymbol_get_FTface_Height(chmap->face, fw, fh);

		/* Draw a block */
	        draw_filled_rect(&gv_fb_dev, x, y, x+w-1, y+lndis-1); // y+h-1
	}
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
   the cursor should be followed up as set by FTcharmap_insert_string().
2. ASSUME mouse action will NOT edit charmap!! No race condition.
3. Need to charmap to update.
--------------------------------------------------------------*/
void arrow_down_history(void)
{
 	/* Always set hisID==hisCnt after save a history cmd. */
	if( hisCnt>=1 ) {
		hisID++;
		//printf("hisID=%d; hisCnt=%d\n", hisID, hisCnt);

		/* Set hisID Limit then */
		if(hisID>hisCnt)
			hisID=hisCnt;

		/* Set selection, to be deleted by _insert_string() */
		chmap->pchoff=pShCmd-chmap->txtbuff;
		chmap->pchoff2=chmap->txtlen;

		/* Anyway, trigger _insert_string()  to trigger cursor follow_up */
		if( hisID <= hisCnt ) {
			chmap->precolor=XTERM_SHCMD_COLOR;
			/* selection between pchoff/pchoff2 will be deleted first */
		   	FTcharmap_insert_string(chmap, ShCmdHistory+hisID*SHELLCMD_MAX, strlen((char*)ShCmdHistory+hisID*SHELLCMD_MAX));
		}
		//else /* histID == histCnt */
		//	FTcharmap_delete_string(chmap);
	}

}
void arrow_up_history(void)
{
 	/* Always set hisID==hisCnt after save a history cmd. */
	if( hisCnt>0 && hisID>=0 ) {
	    	if( hisID > 0 )
			hisID--;

		/* Set selection, to be deleted by _insert_string() */
		chmap->pchoff=pShCmd-chmap->txtbuff;
		chmap->pchoff2=chmap->txtlen;

		chmap->precolor=XTERM_SHCMD_COLOR;
		FTcharmap_insert_string(chmap, ShCmdHistory+hisID*SHELLCMD_MAX, strlen((char*)ShCmdHistory+hisID*SHELLCMD_MAX));
	}
}


/*-------------------------------------------------------------
	   An In_Process Function
Execute shell command and read output.
Need to charmap to update.

		!!! WARNING !!!
Mouse control for charmap MUST be locked when execute_shellcmd()
Some func has NO mutex_lock/request for the charmap!!!
FTcharmap_goto_end(), parse_shellout(), ....

--------------------------------------------------------------*/
void execute_shellcmd(void)
{
	//int i;
	int status;
	ssize_t nread=0; //nread_out=0, nread_err=0;
	size_t nbuf;	 // bytes of data in obuf
	char linebuf[EGI_LINE_MAX];
	size_t linesize;
	char *pnl;
	//bool err_out, std_out;
	char ptyname[256]={0};
	int ptyfd;
	char **lineptr=NULL;
	//size_t n=0;

	if( ShellCmd[0] ) {
		/* Get rid of NL token '\n' */
		ShellCmd[strlen((char *)ShellCmd)-1]=0;

		/* To lock any MOUSE operation. */
		sustain_cmdout=true;
		lock_mousectrl=true;
		printf("Execute shell cmd: %s\n", ShellCmd);

		/* Set termIO to Canonical mode for shell pid. */
	        new_isettings.c_lflag |= ICANON;      /* enable canonical mode, no buffer */
	        new_isettings.c_lflag |= ECHO;        /* enable echo */
	        tcsetattr(STDIN_FILENO, TCSANOW, &new_isettings);

		status=0;
		sh_pid=forkpty(&ptyfd,ptyname,NULL,NULL);
		/* Child process to execute shell command */
		if( sh_pid==0) {
			/* Execute shell command, the last NL must included in ShellCmd */
			execl("/bin/sh", "sh", "-c", (char *)ShellCmd, (char *)0);
		}
		/* Father process to read child stdout put */
		else if(sh_pid>0) {
			printf("Child pty name: %s\n",ptyname);
#if 0
			ptyfil=fopen(ptyname, "r+");
			if(ptyfil==NULL) {
				printf("Fail to open '%s'\n", ptyname);
				goto END_FUNC;
			}
			// isatty(int fd); isatty()  returns 1 if fd is an open file descriptor referring to a terminal; otherwise 0 is returned
#endif
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
			#if 0 /* TEST --------------- */
			printf("History:\n");
			for(i=0; i<hisCnt; i++)
				printf("%s\n",ShCmdHistory+i*SHELLCMD_MAX);
			printf("\n");
			#endif

			/* Read out child results */
	        	bzero(obuf,sizeof(obuf));
			nbuf=0;
			do {
				/* !!! WARNING !!!
				 * 1. If read an incomplete line here, the next parse_shout() may fail
				 *    in recoginzing UFT-8 wchar OR escape sequence string! so need to read out obuf[] rline by rline into linebuf[].
				 * 2. read(ptyfd) will gulp a chunk of data at one time, and maybe all data for once! then the shell_pid quits!
				 *    We'll process data in obuf[] line by line then, pick out escape sequence string and parse it.
				 * 3. Fget(), getline()..can NOT read out from a tty file stream, if it's valid.
				 * 4. If one line from ptyfd is too big (>EGI_LINE_MAX-1), it will be splited then. usually it's
				 *    from a text file, not from printf, and no ESC sequence there.
				 */
				/* Check remaining space of obuf[] */
				//if(nbuf==sizeof(obuf)-nbuf-1) {
				if(0==sizeof(obuf)-nbuf-1) {
					/* TODO: memgrow obuf */
					printf("Buffer NOT enough! nbuf=%zd! \n", nbuf);
					sustain_cmdout=false;
					break;
				}
				else
					printf("nbuf=%zd \n", nbuf);

			        nread=read(ptyfd, obuf+nbuf, sizeof(obuf)-nbuf-1);
				printf("nread=%zd\n",nread);
				//printf("obuf: \n%s\n",obuf);
				if(nread>0) {
					nbuf += nread;
				}
				else if(nread==0) {
					//printf("nread=0...\n");
				}
				else   { /* nread<0 */
					//printf("nread<0!\n");
				}

				/* Check if has a complete line in obuf. */
				pnl=strstr(obuf,"\n");
				if( pnl!=NULL || nread<0 ) {   /* If nread<0, get remaining data, even without NL. */
					/* Copy to linebuf, TODO: memgrow linebuf */
					if( nread<0 && pnl==NULL ) /* shell pid ends, and remains in obuf without '\n'! */
						linesize=strlen(obuf);
					else  /* A line with '\n' at end. */
						linesize=pnl+1-obuf;

					/* Limit linesize */
					if(linesize > sizeof(linebuf)-1 ) {
						printf("linebuf is NOT enough!, current linesize=%zd\n",linesize);
						linesize=sizeof(linebuf)-1;  /* linesize LIMIT */
						/* TODO: get end of the last complete UFT-8 char in obuf.	 */
					}
					else
						printf("linesize=%zd \n", linesize);

					strncpy(linebuf, obuf, linesize);
					linebuf[linesize]='\0';
					/* Move out from obuf, and update nbuf. */
					memmove(obuf, obuf+linesize, nbuf-linesize+1); /* +1 EOF */
					nbuf -= linesize;
					obuf[nbuf]='\0';

						/* Parse and output the line */
/* TEST: ----- */
// printf("---Befor insert: pchoff=%u, pchoff2=%u, maplines=%d, maplncount=%d, txtlncount=%d\n",
//                                chmap->pchoff, chmap->pchoff2, chmap->maplines, chmap->maplncount,chmap->txtdlncount );

					chmap->precolor=XTERM_STDOUT_COLOR;
					parse_shellout((char *)linebuf);  /* Charmap/fb_render inside */
					chmap->follow_cursor=false;  /* Slow down greatly?! */

#if 0 //////////////////
					/* charmap to update current page, otherwise following FTcharmap_page_down() will fetch error data. */
        		        	FTcharmap_writeFB(&gv_fb_dev, NULL, NULL); /* TODO: self_cooked fw */
					fb_render(&gv_fb_dev);

/* TEST: ----- */
// printf("---Aft insert: maplines=%d, maplncount=%d, txtlncount=%d\n",
//                               chmap->maplines, chmap->maplncount,chmap->txtdlncount );
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
#endif /////////////
				}
				/* ESLE: No complete line in obuf,  AND shell pid not end yet! */
				else if( pnl==NULL && nread>=0 ) {
					linesize=0;
					printf("Continue to buffer for a complete line...\n");
				}

			} while( (nread>=0 || nbuf>0) && sustain_cmdout );

			/* User interrupt to end sh_pid, insert a LN */
		   	if(!sustain_cmdout) {
				FTcharmap_insert_string(chmap,(const UFT8_PCHAR)"\n",1); /* Mouse insert..ignored */
	                	FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
			}

			/* TODO: here, mouse control is allowed! */

			/* Not necessary? Err'Inappropriate ioctl for device' */
		   	if( wait(&status)!=sh_pid ) {
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

#if 1		/* Insert new PS */
		printf("insert PS...\n");
		chmap->precolor=XTERM_PS_COLOR;
		FTcharmap_insert_string(chmap,strPrompt,strlen((char *)strPrompt));
		FTcharmap_writeFB(NULL, NULL, NULL);
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

	/* Unlock mousectrl for champ */
	lock_mousectrl=false;

END_FUNC:
	/* Set termIO to NON_Canonical mode */
	new_isettings.c_lflag &= (~ICANON);      /* disable canonical mode, no buffer */
        new_isettings.c_lflag &= (~ECHO);        /* disable echo */
	new_isettings.c_cc[VMIN]=0;		 /* Nonblock */
        new_isettings.c_cc[VTIME]=0;
	tcsetattr(STDIN_FILENO, TCSANOW, &new_isettings);

	/* Close & Free */
	close(ptyfd);
	if(lineptr)
		free(*lineptr);
}


/*-------------------------------------------------------------------------------
Parse shell output and write to chmap, color attributes are extracted here.

Note:
   		CSI ( Control sequence introducer ) and Display attributes
			< Refer to man console_codes >
   CSI starts with 'ESC[' (or '\033[') and the action of a CSI sequence is determined by
   its final character.
    m -- Set display attributes:
	ESC[ param,param,param,,,,m
   "Several attributes can be set in the same sequence, separated by semicolons.
   An empty parameter (between semicolons or string initiator or terminator) is interpreted as a zero."

	<< parameters for color setting >>
	0       reset all attributes to their defaults
	...
	5       set blink
	...
       30      set black foreground
       31      set red foreground
       32      set green foreground
       33      set brown foreground
       34      set blue foreground
       35      set magenta foreground
       36      set cyan foreground
       37      set white foreground
       38      set underscore on, set default foreground color
       39      set underscore off, set default foreground color
       40      set black background
       41      set red background

       42      set green background
       43      set brown background
       44      set blue background
       45      set magenta background
       46      set cyan background
       47      set white background
       49      set default background color
	....

		((( 8_color_console control sequence )))

CSI Format: "\033[..3X;4X..m"  3X -- foreground color, 4X -- background color

Examples:
	attrGreen: 	"\033[0;32;40m"
	attrRed:	"\033[0;31;40m"


		((( 256_color_console control sequence )))

CSI Format: "\033[..38;5;n..m"  n -- foreground color
	    "\033[..48;5;n..m"  n -- backround color

	     256_color(n):
			0-7:	 Same as 8_color "ESC[30-37m"
			8-15:    Bright version of above colors. "ESC[90-97m"
			16-231:  6x6x6=216 colors ( 16+36*r+6*g+b  where 0<=r,g,b<=5 )
			232-255: 24_grade grays
Examples:
	attrRed:	"\e[38;5;196;48;5;0m"    (forecolor 196, backcolor 0)

Reset to defaults:  "\e[0m"

Params:
@pstr:	Pointer to shell output string.

---------------------------------------------------------------------------*/
void parse_shellout(char* pstr)
{
	int k,j;
	char *ps, *pe;			/* Pointer to start and end of a CSI string */
	const char *delim_ESC="\033"; 	/* Start of CSI seq */
	const char *delim_Param=";";    /* parameter separator */
	char *param;
	char *pins;			/* Pointer to plain string (without CSI) for inserting to Charmap */
	static char *saveptr;
	static char *saveps;

	/* Calloc saveptr/saveps */
	if(saveptr==NULL) {
		saveptr=calloc(1, 2048);
		if(saveptr==NULL) {
			printf("Fail to calloc saveptr!\n");
			return;
		}
	}
	if(saveps==NULL) {
		saveps=calloc(1, 2048);
		if(saveps==NULL) {
			printf("Fail to calloc saveps!\n");
			return;
		}
	}

	printf("---pstr: %s\n", pstr);

	/* Fetch CSI */
 	ps=strtok_r(pstr, delim_ESC,&saveptr);
        for(k=0; ps!=NULL; k++) {
//		printf("ps: %s\n", ps);
		/* Parse CSI color attribute, only if a legal CSI sequence: "ESC[..m" */
		if( ps!=pstr && ps[0]=='[' && (pe=strstr(ps, "m"))!=NULL) {   /* ps!=pstr to rule out "[..m....." case without a leading ESC */
			/* Buffer current line before calling strtok */
//			printf("pe+1: %s\n", pe+1);
			/* Parse parameters of CSI sequence, separated by semicolons ';' */
			param=strtok_r(ps+1, delim_Param, &saveps);
//			printf("param: %s\n", param);
			for(j=0; param!=NULL; j++ ) {
				printf("code %d ",atoi(param));
				/* Only parse 8 font_color NOW. TODO: more... */
				switch(atoi(param)) {
					case 0:
						/* All set to default */
						//if(j==0) {
						chmap->precolor=chmap->fontcolor;
						break;
					case 1:
						/* Set bold */
						break;
					case 2:
						/* set half-bright */
						break;
					case 4:
						/* set underscore */
						break;
					case 5:
						/* set blink */
						break;

					/* Xterm color: 0-7   TOO DARK!!!!
				         *      0-black;  1-red;      2-green;  3-yellow;
				         *      4-blue;   5-magenta;  6-cyan;   7-white
					 * BLACK(0,0,0)
					 * Maroon(128,0,0), Green(0,128,0), Olive(128,128,0),
        	              		 * Navy(0,0,128), Purple(128,0,128), Teal(0,128,128),
					 * Silver(192,192,192)
					 */
					case 30:
						chmap->precolor=WEGI_COLOR_BLACK;
						break;
					case 31: case 32: case 33: case 34: case 35:
					case 36:
					// ---pstr: ^[[32m[    0.000000] ^[[0m^[[1mZone ranges:^[[0m^M  xxxx
						chmap->precolor=egi_256color_code(atoi(param)-30 +8);
						break;
					case 37:
						chmap->precolor=egi_256color_code(7);
						break;
					default:
						break;
				}

				/* Fetch next param */
				param=strtok_r(NULL, delim_Param,&saveps);
			}

			/* Write to charmap immediatly after precolor setting. */
//			printf("insert string:%s\n", pe+1); //tmpbuf);
			FTcharmap_insert_string(chmap,(UFT8_PCHAR)(pe+1),strlen(pe+1));
   			//FTcharmap_writeFB(NULL, NULL, NULL);
			chmap->request=0;

			#if 1 /* Refresh screen after each NL, slow... */
			if( strstr(pe+1, "\n")!=NULL ) {
				chmap->follow_cursor=false;
				FTcharmap_goto_end();
	      			FTcharmap_page_fitBottom(chmap);
       				FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
				fb_render(&gv_fb_dev);
			}
			#endif
		}

		/* Plain string withou CSI */
		else if( ps==pstr || ps[0] !='[' ) {  /* ps==ptr: "[..m....." case without a leading ESC. */
			FTcharmap_insert_string(chmap,(UFT8_PCHAR)(ps),strlen(ps));
   			//FTcharmap_writeFB(NULL, NULL, NULL);
			chmap->request=0;

			if( strstr(ps, "\n")!=NULL ) {
				chmap->follow_cursor=false;
				FTcharmap_goto_end();
	      			FTcharmap_page_fitBottom(chmap);
       				FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
				fb_render(&gv_fb_dev);
			}
		}

		/* Fetch next CSI sequence */
	 	ps=strtok_r(NULL, delim_ESC, &saveptr);
        }
	printf("\n"); /* code printf end */
#if 0
	chmap->follow_cursor=false;
	FTcharmap_goto_end();
	FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
	fb_render(&gv_fb_dev);
#endif

}
