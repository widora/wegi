/*------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of a simple editor, read input from termios and put it on to
the screen.

		( --- 1. The Editor: Keys and functions --- )

Mouse move: 	Move typing cursor to, LeftKeyDown to locate typing postion.
Mouse scroll: 	Scroll charmap up and down.
ARROW_KEYs: 	Shift typing cursor to locate insert/delete position.
BACKSPACE: 	Delete chars before the cursor.
DEL:		Delete chars following the cursor.
HOME(or Fn+LEFT_ARROW!):	Return cursor to the begin of the retline.
END(or Fn+RIGHT_ARROW!):	Move cursor to the end of the retline.
CTRL+N:		Scroll down 1 line, keep typing cursor position.
CTRL+O:		Scroll up 1 line, keep typing cursor position.
CTRL+H:		Go to the first page.
CTRL+E:		Go to the last page.
CTRL+F:		Save current chmap->txtbuff to a file. PINYIN DATA also saved
		with update frequency weigh values.
PG_UP:		Page up
PG_DN:		Page down


		( --- 2. PINYIN Input Method --- )

CTRL+P:		Switch ON/OFF PINYIN input panel.
ARROW_DOWN:	Show next group of Hanzi/Cizu.
ARROW_UP:	Show previous group of Hanzi/Cizu.
BACKSPACE:	Roll back pinyin input buffer.
1-7:		Use as index to select Haizi/Cizu in the displaying panel.
SPACE:		Select the first Haizi/Cizu in the displaying panel

Note:
  	1. Use '(0x27) as delimiter to separate PINYIN entry. Example: xian --> xi'an.
  	2. Press ESCAPE to clear PINYIN input buffer.
	3. See PINYIN DATA LOAD/SAVE PROCEDURE in "egi_unihan.h".

               ( --- 3. Definition and glossary --- )

1. char:    A printable ASCII code OR a local character with UFT-8 encoding.
2. dline:   A displayed/charmapped line, A line starts/ends at displaying window left/right end side.
   retline: A line starts/ends by a new line token '\n'.
3. scroll up/down:  scroll up/down charmap by mouse, keep cursor position relative to txtbuff.
	  (UP: decrease chmap->pref (-txtbuff), 	  DOWN: increase chmap->pref (-txtbuff) )

   shift  up/down:  shift typing cursor up/down by ARROW keys.
	  (UP: decrease chmap->pch, 	  DOWN: increase chmap->pch )


Note:
1. You must have PINYIN_UPDATE_FPATH("/tmp/update_words.txt") created, otherwise it will fail.
   The purpose of this program is for accumulating/updating unihan groups!
2. Mouse_events and and key_events run in two independent threads.
   When you input keys while moving/scrolling mouse to change inserting position, it can
   happen simutaneously!  TODO: To avoid?

TODO:
1. English words combination.
2. IO buffer/continuous key press/ slow writeFB response.
3. If two UniGroups have same wcodes[] and differen typings,  Only one will
   be saved in group_test.txt/unihangroups_pinyin.dat!
   Example:  重重 chong chong   重重 zhong zhong

Journal
2021-1-9/10:
	1. Revise FTcharmap_writeFB() to get rid of fontcolor, which will now decided by
	   chmap->fontcolor OR chmap->charColorMap.
	2. FTcharmap_create(): Add param 'charColorMap_ON' and 'fontcolor'.
	3. Add RCMENU_COMMAND_COLOR and WBTMENU_COMMAND_MORE.
	4. Add color palette.


Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>   /* open */
#include <unistd.h>  /* read */
#include <errno.h>
#include <ctype.h>
#include <linux/input.h>
#include <termios.h>
#include "egi_common.h"
#include "egi_input.h"
#include "egi_unihan.h"
#include "egi_FTcharmap.h"
#include "egi_FTsymbol.h"
#include "egi_cstring.h"
#include "egi_utils.h"
#include "egi_procman.h"

#ifdef LETS_NOTE
	#define DEFAULT_FILE_PATH	"/home/midas-zhou/egi/hlm_all.txt"
#else
	#define DEFAULT_FILE_PATH	"/mmc/hlm_all.txt"
#endif

#define SINGLE_PINYIN_INPUT	0       /* !0: Single chinese_phoneticizing input method.
					 *  0: Multiple chinese_phoneticizing input method.
					 */

#define 	CHMAP_TXTBUFF_SIZE	64//1024     /* 256,text buffer size for EGI_FTCHAR_MAP.txtbuff */
#define		CHMAP_SIZE		2048 //256  /* NOT less than Max. total number of chars (include '\n's and EOF) that may displayed in the txtbox */

/* TTY input escape sequences.  USB Keyboard hidraw data is another story...
 * An escape sequece is regarded as a single nonprintable character. the ESC character (\033) is used as the first char. for a control key.
 */
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


#define 	TEST_INSERT 	0

/* Some Shell TTY ASCII control key */
#define		CTRL_N	14	/* Ctrl + N: scroll down  */
#define		CTRL_O	15	/* Ctrl + O: scroll up  */
#define 	CTRL_F  6	/* Ctrl + f: save file */
#define		CTRL_S  19	/* Ctrl + S */
#define		CTRL_E  5	/* Ctrl + e: go to last page */
#define		CTRL_H  8	/* Ctrl + h: go to the first dline, head of the txtbuff */
#define		CTRL_P  16	/* Ctrl + p: PINYIN input ON/OFF toggle */

char *strInsert="Widora和小伙伴们";
char *fpath="/mmc/hello.txt";

//static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 30+5+20+5} };	/* Onle line displaying area */
//static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 120-1-10} };	/* Text displaying area */
//static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 240-1-10} };	/* Text displaying area */
static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 240-1} };	/* Text displaying area */
/* To adjust later */

static int smargin=5; 		/* left and right side margin of text area */
static int tmargin=2;		/* top margin of text area */

/* Input Method: PINYIN */
wchar_t wcode;
EGI_UNIHAN_SET *uniset;
EGI_UNIHANGROUP_SET *group_set;
static bool enable_pinyin=false;
/* Enable input method PINYIN */
char strPinyin[4*8]={0}; /* To store unbroken input pinyin string */
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
int  GroupItems;		/* Current unihan(group)s items in selecting panel */
int  GroupMaxItems=7; 	/* Max. number of unihan(group)s displayed for selecting */
int  HanGroups=0; 	/* Groups with selecting items, each group with Max. GroupMaxItems unihans.
	          	 * Displaying one group of unihans each time on the PINYIN panel
		  	 */
int  HanGroupIndex=0;	/* group index number, < HanGroups */

/* NOTE: char position, in respect of txtbuff index and data_offset: pos=chmap->charPos[pch] */
static EGI_FTCHAR_MAP *chmap;	/* FTchar map to hold char position data */
static unsigned int chns;	/* total number of chars in a string */

static bool mouseLeftKeyDown;
static bool mouseLeftKeyUp;
static bool start_pch=false;	/* True if mouseLeftKeyDown switch from false to true */
static int  mouseX, mouseY, mouseZ; /* Mouse point position */
static int  mouseMidX, mouseMidY;   /* Mouse cursor mid position */
static int  menuX0, menuY0;

static int fw=18;	/* Font size */
static int fh=20;
static int fgap=20/4; //5;	/* Text line fgap : TRICKY ^_- */
static int tlns;  //=(txtbox.endxy.y-txtbox.startxy.y)/(fh+fgap); /* Total available line space for displaying txt */

//static int penx;	/* Pen position */
//static int peny;

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
	RCMENU_COMMAND_COLOR	=4,
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
	WBTMENU_COMMAND_HIGHLIGHT   =7,
};
static int WBTMenu_Command_ID=WBTMENU_COMMAND_NONE;

/* Functions */
//static int FTcharmap_writeFB(FBDEV *fbdev, EGI_16BIT_COLOR color, int *penx, int *peny);
static int FTcharmap_writeFB(FBDEV *fbdev, int *penx, int *peny);
static void FTcharmap_goto_end(void);
static void FTsymbol_writeFB(const char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny);
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus);
//static void draw_mcursor(int x, int y);
static void draw_mcursor(void);
static void draw_RCMenu(int x0, int y0);  /* Draw right_click menu */
static void draw_WTBMenu(int x0, int y0); /* Draw window_top_bar menu */
static void draw_msgbox( FBDEV *fb_dev, int x0, int y0, int width, const char *msg );
static void draw_progress_msgbox( FBDEV *fb_dev, int x0, int y0, const char *msg, int pv, int pev);
static void RCMenu_execute(enum RCMenu_Command Command_ID);
static void WBTMenu_execute(enum WBTMenu_Command Command_ID);
static int  load_pinyin_data(void);
static void save_all_data(void);
void signal_handler(int signal);
void draw_palette(int x0, int y0);


/* ============================
	    MAIN()
============================ */
int main(int argc, char **argv)
{
 /* <<<<<<  EGI general init  >>>>>> */

  /* Start sys tick */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();

  /* Set signal action */
  if(egi_common_sigAction(SIGINT, signal_handler)!=0) {
	printf("Fail to set sigaction!\n");
	return -1;
  }

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

        struct termios old_settings;
        struct termios new_settings;

	char ch;
	int  j,k;
	int lndis; /* Distance between lines */

	if( argc > 1 )
		fpath=argv[1];

	/* Load necessary PINYIN data, and do some prep. */
	if( load_pinyin_data()!=0 )
		exit(1);


MAIN_START:
	/* Reset main window size */
	txtbox.startxy.x=0;
	txtbox.startxy.y=30;
	txtbox.endxy.x=gv_fb_dev.pos_xres-1;
	txtbox.endxy.y=gv_fb_dev.pos_yres-1;

	/* Activate main window */
	ActiveComp=CompTXTBox;

	/* Total available lines of space for displaying chars */
	//lndis=fh+fgap;
	lndis=FTsymbol_get_FTface_Height(egi_sysfonts.regular, fw, fh);
	tlns=(txtbox.endxy.y-txtbox.startxy.y+1)/lndis;
	printf("Blank lines tlns=%d\n", tlns);

	/* Create charMAP */
	chmap=FTcharmap_create( CHMAP_TXTBUFF_SIZE, txtbox.startxy.x, txtbox.startxy.y,	 /* txtsize,  x0, y0  */
//		  txtbox.endxy.y-txtbox.startxy.y+1, txtbox.endxy.x-txtbox.startxy.x+1, smargin, tmargin,      /*  height, width, offx, offy */
	  		tlns*lndis, txtbox.endxy.x-txtbox.startxy.x+1, smargin, tmargin, /*  height, width, offx, offy */
			egi_sysfonts.regular, CHMAP_SIZE, tlns, gv_fb_dev.pos_xres-2*smargin, lndis,   	 /* typeface, mapsize, lines, pixpl, lndis */
			WEGI_COLOR_WHITE, WEGI_COLOR_BLACK, true, true );  /*  bkgcolor, fontcolor, charColorMap_ON, hlmarkColorMap_ON */
	if(chmap==NULL){ printf("Fail to create char map!\n"); exit(0); };


	/* Load file to chmap */
	if( argc>1 ) {
		if(FTcharmap_load_file(argv[1], chmap, CHMAP_TXTBUFF_SIZE) !=0 )
			printf("Fail to load file to champ!\n");
	}
	else {
		if( FTcharmap_load_file(DEFAULT_FILE_PATH, chmap, CHMAP_TXTBUFF_SIZE) !=0 )
			printf("Fail to load file to champ!\n");
	}
	/* TODO: ColorMap ALSO saved:
	*  Before that, we manually create monocolor map for loaded txt first.
	*/
	if( egi_colorBandMap_insertBand(chmap->charColorMap, 0, chmap->txtlen, chmap->fontcolor)!=0 ) {  /* map, pos, len, color */
		printf("Fail to inser char color band for loaded file!\n");
	}
	if( egi_colorBandMap_insertBand(chmap->hlmarkColorMap, 0, chmap->txtlen, chmap->bkgcolor)!=0 ) {  /* map, pos, len, color */
		printf("Fail to inser highlight color band for loaded file!\n");
	}


#if 0	/* -----TEST:  Set char color band map */
	if(chmap->charColorMap) {
		egi_colorBandMap_combineBands(chmap->charColorMap, 0, 16, WEGI_COLOR_RED);
		egi_colorBandMap_combineBands(chmap->charColorMap, 16, 16, WEGI_COLOR_GREEN);
		egi_colorBandMap_combineBands(chmap->charColorMap, 32, 16, WEGI_COLOR_BLUE);
	}
#endif
#if 0	/* -----TEST:  Set Highlight mark band map */
	if(chmap->hlmarkColorMap) {
		egi_colorBandMap_combineBands(chmap->hlmarkColorMap, 0, 16, WEGI_COLOR_RED);
		egi_colorBandMap_combineBands(chmap->hlmarkColorMap, 16, 16, WEGI_COLOR_GREEN);
		egi_colorBandMap_combineBands(chmap->hlmarkColorMap, 32, 16, WEGI_COLOR_BLUE);
	}
#endif

        /* Init. mouse position */
        mouseX=gv_fb_dev.pos_xres/2;
        mouseY=gv_fb_dev.pos_yres/2;

        /* Init. FB working buffer */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY4);
	FTsymbol_writeFB("File  Help",20,5,WEGI_COLOR_WHITE, NULL, NULL);

	/* TEST: getAdvances ---- */
	int penx=140, peny=5;
	FTsymbol_writeFB("EGI笔记本",penx,peny,WEGI_COLOR_LTBLUE, &penx, &peny);
	printf("FTsymbol_writFB get advanceX =%d\n", penx-140);
	printf("FTsymbol_uft8string_getAdvances =%d\n",
		FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fw, fh, (const unsigned char *)"EGI笔记本") );


	/* Draw blank paper + margins */
	fbset_color(WEGI_COLOR_WHITE);
	draw_filled_rect(&gv_fb_dev, txtbox.startxy.x, txtbox.startxy.y, txtbox.endxy.x, txtbox.endxy.y );

        fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
	fb_render(&gv_fb_dev);

	/* Set termio */
        tcgetattr(0, &old_settings);
        new_settings=old_settings;
        new_settings.c_lflag &= (~ICANON);      /* disable canonical mode, no buffer */
        new_settings.c_lflag &= (~ECHO);        /* disable echo */
        new_settings.c_cc[VMIN]=0;
        new_settings.c_cc[VTIME]=0;
        tcsetattr(0, TCSANOW, &new_settings);

	/* To fill initial charmap and get penx,peny */
	FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
	fb_render(&gv_fb_dev);

	/* Charmap to the end */
	int i=0;
	while( chmap->pref[chmap->charPos[chmap->chcount-1]] != '\0' )
	{
		FTcharmap_page_down(chmap);
		FTcharmap_writeFB(NULL, NULL, NULL);
		//FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_BLACK, NULL,NULL);

		if( chmap->txtdlinePos[chmap->txtdlncount] > chmap->txtlen/100 )
			break;

		if( i==20 ) {  /* Every time when i counts to 20 */
		   FTcharmap_writeFB(&gv_fb_dev, NULL,NULL);
		   fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
		   draw_progress_msgbox( &gv_fb_dev, (320-260)/2, (240-120)/2, "文件加载中...",
								chmap->txtdlinePos[chmap->txtdlncount], chmap->txtlen/100);
		   fb_render(&gv_fb_dev);
		   i=0;
		}
		i++;
	}
        fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
	draw_progress_msgbox( &gv_fb_dev, (320-280)/2, (240-140)/2, "文件加载完成", 100,100);
//	goto MAIN_END;

	printf("Loop editing...\n");

	/* Loop editing ...   Read from TTY standard input, without funcion keys on keyboard ...... */
	while(1) {
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
		if(ch==27) {
			ch=0;
			read(STDIN_FILENO,&ch, 1);
			printf("ch=%d\n",ch);
			if(ch==91) {
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
					case 65:  /* UP arrow : move cursor one display_line up  */
						/* Shift displaying UNIHAN group */
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
						else
							FTcharmap_shift_cursor_up(chmap);
						break;
					case 66:  /* DOWN arrow : move cursor one display_line down */
						/* Shift displaying UNIHAN group */
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
						else
							FTcharmap_shift_cursor_down(chmap);
						break;
					case 67:  /* RIGHT arrow : move cursor one char right, Limit to the last [pch] */
						FTcharmap_shift_cursor_right(chmap);
						break;
					case 68: /* LEFT arrow : move cursor one char left */
						FTcharmap_shift_cursor_left(chmap);
						break;
					case 70: /* END : move cursor to the end of the return_line */
						FTcharmap_goto_lineEnd(chmap);
						break;

					case 72: /* HOME : move cursor to the begin of the return_line */
						/* move cursor to the first of the line */
						FTcharmap_goto_lineBegin(chmap);
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
					chmap->height=chmap->maplines*lndis;  /* restor chmap.height */
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
				save_all_data();
				/* Save txtbuff to file */
				printf("Saving chmap->txtbuff to a file...");
				if(FTcharmap_save_file(fpath, chmap)==0)
	                                draw_msgbox( &gv_fb_dev, 50, 50, 240, "文件已经保存！" );
				else
	                                draw_msgbox( &gv_fb_dev, 50, 50, 240, "文件保存失败！" );
                                fb_render(&gv_fb_dev);
				tm_delayms(500);

				ch=0;
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
			else  /* strPinyin is empty */
				FTcharmap_go_backspace( chmap );
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
				/* ---- TEST: insert string */
				if(ch=='5' && TEST_INSERT  ) {
					static int pos=0;

	                                /* revise chsize and chns  */
        	                        chns=cstr_strcount_uft8((const unsigned char *)strInsert);
					FTcharmap_insert_char( chmap, strInsert+pos );

					pos += cstr_charlen_uft8((const unsigned char *)(strInsert+pos));
					if(*(strInsert+pos)=='\0')
						pos=0;
	                        }
				else {
					FTcharmap_insert_char( chmap, &ch );
				}

			}

			ch=0;
		}  /* END insert ch */

		/* EDIT:  Need to refresh EGI_FTCHAR_MAP after editing !!! .... */

		/* Image DivisionS: 1. File top bar  2. Charmap  3. PINYIN IME  4. Sub Menus */
	/* --- 1. File top bar */
		draw_filled_rect2(&gv_fb_dev,WEGI_COLOR_GRAY4, 0, 0, gv_fb_dev.pos_xres-1, gv_fb_dev.pos_yres-1); //txtbox.endxy.x, txtbox.startxy.y-1);
		FTsymbol_writeFB("File  Help",20,5,WEGI_COLOR_WHITE, NULL, NULL);
		FTsymbol_writeFB("EGI笔记本",140,5,WEGI_COLOR_LTBLUE, NULL, NULL);
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
		/* Righ_click menu */
		case CompRCMenu:
			/* Backup desktop bkg */
	        	fb_copy_FBbuffer(&gv_fb_dev,FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */

			while( !start_pch ) {  /* Wait for click */
        			fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
				draw_RCMenu(menuX0, menuY0);
       				draw_mcursor();
				fb_render(&gv_fb_dev);
			}
			/* Execute Menu Command */
			RCMenu_execute(RCMenu_Command_ID);

			/* Exit Menu handler */
			start_pch=false;
			ActiveComp=CompTXTBox;
			break;

		/* Window Bar menu */
		case CompWTBMenu:
			/* In case that start_pch MAY have NOT reset yet */
			start_pch=false;

			printf("WTBmenu Open\n");
			/* Backup desktop bkg */
	        	fb_copy_FBbuffer(&gv_fb_dev,FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */

			/* Wait for next click ... */
			while( !start_pch || mouseLeftKeyDown==false ) {
        			fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
				draw_WTBMenu( txtbox.startxy.x, txtbox.startxy.y );
       				draw_mcursor();
				fb_render(&gv_fb_dev);
			}

			printf("WTBmenu Close\n");
			/* Execute Menu Command */
			WBTMenu_execute(WBTMenu_Command_ID);

			/* Exit Menu handler */
			start_pch=false;
			ActiveComp=CompTXTBox;
			break;
		default:
			break;
	    } /* END Switch */

		//printf("Draw mcursor......\n");
       		draw_mcursor();

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
        tcsetattr(0, TCSANOW,&old_settings);

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
@color: 	Color for text
@penx, peny:    pointer to pen X.Y.
--------------------------------------*/
static int FTcharmap_writeFB(FBDEV *fbdev, int *penx, int *peny)
{
	int ret;

       	ret=FTcharmap_uft8strings_writeFB( fbdev, chmap,          	   /* FBdev, charmap*/
                                           fw, fh,   /* fontface, fw,fh */
	                                   -1, 255,      	   	   /* transcolor,opaque */
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
        FTcharmap_writeFB(&gv_fb_dev, NULL,NULL);
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
static void FTsymbol_writeFB(const char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny)
{
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,    	/* fw,fh, pstr */
                                        320, tlns, fgap,      /* pixpl, lines, fgap */
                                        px, py,                                 /* x0,y0, */
                                        color, -1, 255,      			/* fontcolor, transcolor,opaque */
                                        NULL, NULL, penx, peny);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}


/*-----------------------------------------------------------
		Callback for mouse input
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

                /* Activate window top bar menu */
                if( mouseY < txtbox.startxy.y && mouseX <112) //90 )
                        ActiveComp=CompWTBMenu;

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
	if( mostatus->mouseDZ > 0 ) {
		 FTcharmap_scroll_oneline_down(chmap);
	}
	else if ( mostatus->mouseDZ < 0 ) {
		 FTcharmap_scroll_oneline_up(chmap);
	}

      	mouseMidY=mostatus->mouseY+fh/2;
       	mouseMidX=mostatus->mouseX+fw/2;
//    	printf("mouseMidX,Y=%d,%d \n",mouseMidX, mouseMidY);

	/* 3. To get current cursor/pchoff position  */
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
}

/*--------------------------------
 Draw cursor for the mouse movement
1. In txtbox area, use typing cursor.
2. Otherwise, apply mcursor.
----------------------------------*/
static void draw_mcursor(void)
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
		egi_imgbuf_resize_update(&tcimg, true, fw, fh );
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


/*-------------------------------------
Draw right_click menu.
@x0,y0:	Left top origin of the menu.
--------------------------------------*/
static void draw_RCMenu(int x0, int y0)
{
	#define RCMENU_ITEMS	5
	char *mwords[RCMENU_ITEMS]={"Words", "Copy", "Paste", "Cut", "Color"};

	int mw=70;   /* menu width */
	int smh=33;  /* sub menu height */
	int mh=smh*RCMENU_ITEMS;  /* menu height */

	int i;
	int mp;	/* 0,1,2 OR <0 Non,  Mouse pointed menu item */

	/* Limit x0,y0 */
	if(x0 > 320-mw) x0=320-mw;
	if(y0 > 240-mh) y0=240-mh;

	/* Decide submenu items as per mouse position on the right_click menu */
	if( mouseX>x0 && mouseX<x0+mw && mouseY>y0 && mouseY<y0+mh )
		mp=(mouseY-y0)/smh;
	else
		mp=-1;

	/* Assign command ID */
	RCMenu_Command_ID=mp;

	/* Draw submenus */
	for(i=0; i<RCMENU_ITEMS; i++)
	{
		/* Menu pad */
		if(mp==i)
			fbset_color(WEGI_COLOR_ORANGE);
		else
			fbset_color(WEGI_COLOR_GRAY4);
		draw_filled_rect(&gv_fb_dev, x0, y0+i*smh, x0+mw, y0+(i+1)*smh);

		/* Menu Words */
        	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)mwords[i],    /* fw,fh, pstr */
                                        100, 1, 0,      			    /* pixpl, lines, fgap */
                                        x0+10, y0+2+i*smh,                      /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, mp==i?255:100,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}
	/* Draw rim */
	fbset_color(WEGI_COLOR_BLACK);
	draw_rect(&gv_fb_dev, x0, y0, x0+mw, y0+mh);
}


/*-----------------------------------------
1. Cal. WBTMenu_Command_ID
2. Draw sub_items of window top bar menu.
@x0,y0:	Left top origin of the menu.
-----------------------------------------*/
static void draw_WTBMenu(int x0, int y0)
{
	static int  mtag=-1;	/* [0]-File,  [1]-Help */

        const int MFileNum=4; /* Number of items under tag File */
        const int MHelpNum=4; /* Number of items under tag Help */

	const char *MFileItems[4]={"Open", "Save", "Exit", "Abort"};
	const char *MHelpItems[4]={"Help", "Renew", "About","Mark" };

	int mw=70;   /* menu width */
	int smh=33;  /* menu item height */
	int mh=smh*MFileNum;  /* menu box height for MFile */

	int i;
	int mp;	/* 0,1,2 OR <0 Non,  Mouse pointed menu item */

	/* On 'File' or 'Help': Only to select it. NOT deselect! */
	//if( mouseX<90 && mouseY<txtbox.startxy.y ) {
	if( mouseX<112 && mouseY<txtbox.startxy.y ) {
		mtag=mouseX/(112/2);
	}
	//else
	//	mtag=-1; /* No tag selected */


   /* 1. On Tag_File */
   if(mtag==0)
   {
	/* Check mouse position on the menu */
	if( mouseX>x0 && mouseX<x0+mw && mouseY>y0 && mouseY<y0+mh )
		mp=(mouseY-y0)/smh;
	else
		mp=-1;  /* Not on the menu */

	/* Assig WBTMenu_Command_ID */
	WBTMenu_Command_ID=mp;

	/* Draw itmes of tag 'File' */
	for(i=0; i<MFileNum; i++)
	{
		/* Menu pad */
		if(mp==i)
			fbset_color(WEGI_COLOR_ORANGE);
		else
			fbset_color(WEGI_COLOR_GRAY4);
		draw_filled_rect(&gv_fb_dev, x0, y0+i*smh, x0+mw, y0+(i+1)*smh);

		/* Menu Words */
        	FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)MFileItems[i],    /* fw,fh, pstr */
                                        100, 1, 0,      			    /* pixpl, lines, fgap */
                                        x0+10, y0+2+i*smh,                      /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}
	/* Draw rim */
	fbset_color(WEGI_COLOR_BLACK);
	draw_rect(&gv_fb_dev, x0, y0, x0+mw, y0+mh);
    }

   /* 2. On Tag_Help */
   else if(mtag==1)
   {
	/* Check mouse position on the menu */
	if( mouseX>x0+45 && mouseX<x0+45+mw && mouseY>y0 && mouseY<y0+smh*MHelpNum )
		mp=(mouseY-y0)/smh;
	else
		mp=-1;  /* Not on the menu */

	/* Assig WBTMenu_Command_ID */
	if( mp > -1 )
		WBTMenu_Command_ID=MFileNum+mp;
	else
		WBTMenu_Command_ID=-1;

	/* Draw itmes of tag 'Help' */
	for(i=0; i<MHelpNum; i++)
	{
		/* Menu pad */
		if(mp==i)
			fbset_color(WEGI_COLOR_ORANGE);
		else
			fbset_color(WEGI_COLOR_GRAY4);
		draw_filled_rect(&gv_fb_dev, x0+45, y0+i*smh, x0+45+mw, y0+(i+1)*smh);

		/* Menu Words */
        	FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)MHelpItems[i],    /* fw,fh, pstr */
                                        100, 1, 0,      			    /* pixpl, lines, fgap */
                                        x0+45+10, y0+2+i*smh,                      /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}
	/* Draw rim */
	fbset_color(WEGI_COLOR_BLACK);
	draw_rect(&gv_fb_dev, x0+45, y0, x0+45+mw, y0+MHelpNum*smh);
    }
}


/*-------------------------------------------------------
WriteFB a progressing msg box

@x0,y0:  	Left top coordinate of the msgbox.
@msg:	 	Message string in UFT-8 encoding.
@pv:	 	Progress value.
@pev:		Progress end value.

-------------------------------------------------------------*/
static void draw_progress_msgbox( FBDEV *fb_dev, int x0, int y0, const char *msg, int pv, int pev)
{
	int width=260;	/* Width and height of the msgbox */
	int height=120;
	int fh=16;	/* Font height and width */
	int fw=16;
	int smargin=20;	/* Side margin */
	int bw=9;		/* width of progressing bar */
	int barY;	/* Y coordinate of the bar */
	int barEndX;	/* End point X coordinate of the bar */
	char strPercent[16];
	int pixlen;

        EGI_16BIT_COLOR color=WEGI_COLOR_GRAY4; 	/* Color of the msgbox. */
	EGI_8BIT_ALPHA  alpha=255;			/* Alpha value of the msgbox. */

	/* Check input */
	if(fb_dev==NULL)
		return;

	/* Set limit */
	if(pev<1)pev=1;
	if(pv<0)pv=0;
	if(pv>pev)pv=pev;

	/* Initiate vars */
	barY=y0+height/2; //y0+15+fh+15+(bw+1)/2;	/* in mid of height */
	bzero(strPercent,sizeof(strPercent));
	sprintf(strPercent,"%d%%",100*pv/pev);

	/* 1. Draw msgbox pad */
        draw_blend_filled_roundcorner_rect( fb_dev, x0,y0, x0+width-1, y0+height-1, 5,    /* fbdev, x1, y1, x2, y2, r */
                                            color, alpha);                           /* color, alpha */

	/* 2. Put message */
       	FTsymbol_uft8strings_writeFB( fb_dev, egi_sysfonts.regular,         /* FBdev, fontface */
                                       fw, fh,(const unsigned char *)msg,     	/* fw,fh, pstr */
                                       width-2*smargin, 5, 5, 		    	/* pixpl, lines, fgap */
                                       x0+smargin, y0+15,                     	/* x0,y0, */
                                       WEGI_COLOR_WHITE, -1, 255,        	/* fontcolor, transcolor,opaque */
                                       NULL, NULL, NULL, NULL);      		/* int *cnt, int *lnleft, int* penx, int* peny */

	/* 3. Draw bkg empty progressing bar */
	fbset_color2(&gv_fb_dev, WEGI_COLOR_GRAY);
	draw_wline( &gv_fb_dev, x0+smargin, barY, x0+width-smargin, barY, bw);

	/* 4. Draw progressing bar */
	barEndX=(x0+smargin)+(width-2*smargin)*pv/pev;
	fbset_color2(fb_dev, WEGI_COLOR_GREEN);
	draw_wline(fb_dev, x0+smargin, barY, barEndX, barY, bw);

	/* 6. Put percentage */
	pixlen=FTsymbol_uft8strings_pixlen( egi_sysfonts.regular, fw, fh, (const unsigned char *)strPercent);
       	FTsymbol_uft8strings_writeFB( fb_dev, egi_sysfonts.regular,         /* FBdev, fontface */
                                       fw, fh,(const unsigned char *)strPercent,   	/* fw,fh, pstr */
                                       width-2*smargin, 1, 0, 		    	/* pixpl, lines, fgap */
                                       x0+(width-pixlen)/2, barY+20,              	/* x0,y0, */
                                       WEGI_COLOR_WHITE, -1, 255,        	/* fontcolor, transcolor,opaque */
                                       NULL, NULL, NULL, NULL);      		/* int *cnt, int *lnleft, int* penx, int* peny */
}


/*-----------------------------------------------------------
WriteFB a progressing msg box

Limit: Max. 100 line text.

@x0,y0:  	Left top coordinate of the msgbox.
@msg:	 	Message string in UFT-8 encoding.
@width:	 	width of the msgbox.
-------------------------------------------------------------*/
static void draw_msgbox( FBDEV *fb_dev, int x0, int y0, int width, const char *msg)
{
	int height;	//=120;
	int fh=18;	/* Font height and width */
	int fw=18;
	int fgap;
	int tmargin=35; /* Top maring, including dot marks */
	int bmargin=15; /* Bottom maring */
	int smargin=15;	/* Side margin */
	int lndis;	/* Distance between lines */
	int peny;

        EGI_16BIT_COLOR boxcolor=WEGI_COLOR_GRAY2; 	/* Color of the msgbox. */
	EGI_8BIT_ALPHA  boxalpha=255;			/* Alpha value of the msgbox. */
        EGI_16BIT_COLOR fontcolor=WEGI_COLOR_BLACK; 	/* Font color. */

	/* Check input */
	if(fb_dev==NULL)
		return;

	/* Set Width limit */
	if(width<100)
		width=100;

	/* Cal. line gap */
	lndis=FTsymbol_get_FTface_Height(egi_sysfonts.regular, fw, fh);
	fgap=lndis-fh;  /* May adjust here */

	/* Cal. msgbox height, writeFB NULL to get final peny.  */
        FTsymbol_uft8strings_writeFB(  NULL, egi_sysfonts.regular,         	/* FBdev, fontface */
                                       fw, fh,(const unsigned char *)msg,       /* fw,fh, pstr */
                                       width-2*smargin, 100, fgap,           	/* pixpl, lines, fgap */
                                       x0+smargin, y0+tmargin,                  /* x0,y0, */
                                       0, -1, 255,               		/* fontcolor, transcolor,opaque */
                                       NULL, NULL, NULL, &peny);                /* int *cnt, int *lnleft, int* penx, int* peny */
	height=(peny+lndis)-(y0-1)+bmargin;

	/* Set Height limit */


	/* Draw msgbox pad */
        draw_blend_filled_roundcorner_rect( fb_dev, x0,y0, x0+width-1, y0+height-1, 5,//9,    /* fbdev, x1, y1, x2, y2, r */
                                            boxcolor, boxalpha);                           /* color, alpha */

	/* Draw top mark */
	fbset_color2(fb_dev, WEGI_COLOR_ORANGE);
	draw_filled_circle( fb_dev, x0+width-15, y0+15, 10 );
	fbset_color2(fb_dev, WEGI_COLOR_LTGREEN);
	draw_filled_circle( fb_dev, x0+width-15-25, y0+15-1, 10 ); /* -1, to balance visual error */
	//fbset_color2(fb_dev, WEGI_COLOR_LTBLUE);
	//draw_filled_circle( fb_dev, x0+width-15-25-25, y0+15, 10 );

	/* Draw msgbox txt area */
        draw_blend_filled_roundcorner_rect( fb_dev, x0+5, y0+30, x0+width-5-1, y0+height-5-1, 3,//5,    /* fbdev, x1, y1, x2, y2, r */
                                            WEGI_COLOR_GRAYB, boxalpha);                           /* color, alpha */

	/* Put message */
       	FTsymbol_uft8strings_writeFB( fb_dev, egi_sysfonts.regular,         /* FBdev, fontface */
                                      fw, fh,(const unsigned char *)msg,     	/* fw,fh, pstr */
                                      width-2*smargin, 100, fgap,	    	/* pixpl, lines, fgap */
                                      x0+smargin, y0+tmargin,                	/* x0,y0, */
                                      fontcolor, -1, 255,        	/* fontcolor, transcolor,opaque */
                                      NULL, NULL, NULL, NULL);      		/* int *cnt, int *lnleft, int* penx, int* peny */
}


/*----------------------------------------------
Exectue command for selected right_click menu

@RCMenu_Command_ID: Command ID, <0 ingnore.

------------------------------------------------*/
static void RCMenu_execute(enum RCMenu_Command RCMenu_Command_ID)
{
	int ret;
	EGI_16BIT_COLOR color;

	if(RCMenu_Command_ID<0)
		return;

	switch(RCMenu_Command_ID) {
		case RCMENU_COMMAND_SAVEWORDS:
			printf("RCMENU_COMMAND_SAVEWORDS\n");
			ret=FTcharmap_save_words(chmap, PINYIN_NEW_WORDS_FPATH);
			if(ret==1)
				draw_msgbox(&gv_fb_dev, 50, 50, 240, "这个词组已经保存过了！" );
			else if(ret==0)
				draw_msgbox(&gv_fb_dev, 50, 50, 240, "词组已经保存！需要重新加载后方可加入到EGI输入法" );
			else
				draw_msgbox(&gv_fb_dev, 50, 50, 240, "词组保存失败！" );
			fb_render(&gv_fb_dev);
			sleep(1);
			break;
		case RCMENU_COMMAND_COPY:
			printf("RCMENU_COMMAND_COPY\n");
			FTcharmap_copy_to_syspad(chmap);
			break;
		case RCMENU_COMMAND_PASTE:
			printf("RCMENU_COMMAND_PASTE\n");
			FTcharmap_copy_from_syspad(chmap);
			break;
		case RCMENU_COMMAND_CUT:
			printf("RCMENU_COMMAND_CUT\n");
			FTcharmap_cut_to_syspad(chmap);
			break;
		case RCMENU_COMMAND_COLOR:
			printf("RCMENU_COMMAND_COLOR\n");
			mouseLeftKeyDown=false; /* reset it first */
			while( !mouseLeftKeyDown ) {  /* Wait for click */
				fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
				draw_palette(50, 40);
				/* Draw pick square */
				color=fbget_pixColor(&gv_fb_dev, mouseX, mouseY);
				draw_filled_rect2(&gv_fb_dev, color, mouseX+10, mouseY+10, mouseX+49, mouseY+49);
				draw_mcursor();
				fb_render(&gv_fb_dev);
			}
			/* Pick color and dye selections, DO NOT pick color here! the mouse thread already updated position after above. */
			FTcharmap_modify_charColor(chmap, color, true);
			break;
		default:
			printf("RCMENU_COMMAND_NONE\n");
			break;
	}
}


/*----------------------------------------------------
Exectue command for selected Window_Bar top menu

@Command_ID: WBTMenu Command ID, <0 ingnore.
-----------------------------------------------------*/
static void WBTMenu_execute(enum WBTMenu_Command Command_ID)
{
	EGI_16BIT_COLOR color;

	if(Command_ID<0)
		return;

	switch(Command_ID) {
		case WBTMENU_COMMAND_OPEN:
			printf("WBTMENU_COMMAND_OPEN\n");
			break;
		case WBTMENU_COMMAND_SAVE:
			printf("WBTMENU_COMMAND_SAVE\n");
			save_all_data();

			start_pch=false;
                        while( !start_pch ) {  /* Wait for click */
                                fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
				draw_msgbox( &gv_fb_dev, 50, 50, 240, "文件已经保存！" );
                                draw_mcursor();
                                fb_render(&gv_fb_dev);
                        }

			break;
		case WBTMENU_COMMAND_EXIT:
			printf("WBTMENU_COMMAND_EXIT\n");
			save_all_data();
			exit(0);
			break;
		case WBTMENU_COMMAND_ABORT:
			printf("WBTMENU_COMMAND_ABORT\n");
			char ch;
	                while(1) {
				draw_msgbox( &gv_fb_dev, 30, 50, 260, "放弃后修改的数据都将不会被保存！\n　　[N]取消　[Y]确认！" );
                                draw_mcursor();
				fb_render(&gv_fb_dev);

				read(STDIN_FILENO, &ch, 1);
				printf(" abort confirm ch='%c'\n", ch);
				if( ch=='y' || ch== 'Y' )
					exit(10);
				else if( ch=='n' || ch== 'N' )
					break;
			}
			break;
		case WBTMENU_COMMAND_HELP:
			printf("WBTMENU_COMMAND_HELP\n");
			break;
		case WBTMENU_COMMAND_RENEW:
			printf("WBTMENU_COMMAND_RENEW\n");

			save_all_data();
			draw_msgbox( &gv_fb_dev, 50, 50, 240, "开始重新加载拼音数据..." );
			fb_render(&gv_fb_dev);
			if( load_pinyin_data()==0 ) {
				draw_msgbox( &gv_fb_dev, 50, 50, 240, "拼音数据成功加载！" );
				fb_render(&gv_fb_dev); tm_delayms(200);
			}
			else {
				draw_msgbox( &gv_fb_dev, 50, 50, 240, "拼音数据加载失败！" );
				fb_render(&gv_fb_dev); tm_delayms(500);
				exit(1);
			}
			break;
		case WBTMENU_COMMAND_ABOUT:
			printf("WBTMENU_COMMAND_ABOUT\n");
			mouseLeftKeyDown=false; /* reset it first */
                        while( !mouseLeftKeyDown ) {  /* Wait for click */
                                fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
				draw_msgbox( &gv_fb_dev, 50, 50, 240, "　　这是用EGI编制的一个简单的记事本. Widora和他的小伙伴们" );
                                draw_mcursor();
                                fb_render(&gv_fb_dev);
                        }

			break;
		case WBTMENU_COMMAND_HIGHLIGHT:
			printf("WBTMENU_COMMAND_HIGHLIGHT\n");
			mouseLeftKeyDown=false; /* reset it first */
			while( !mouseLeftKeyDown ) {  /* Wait for click */
				fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
				draw_palette(50, 50);
				/* Draw pick square */
				color=fbget_pixColor(&gv_fb_dev, mouseX, mouseY);
				draw_filled_rect2(&gv_fb_dev, color, mouseX+10, mouseY+10, mouseX+49, mouseY+49);
				draw_mcursor();
				fb_render(&gv_fb_dev);
			}
			/* Pick color and dye selections, DO NOT pick color here! the mouse thread already updated position after above. */
			FTcharmap_modify_hlmarkColor(chmap, color, true);
			break;

		default:
			printf("WBTMENU_COMMAND_NONE\n");
			break;
	}

}

/*------------------------------------------------------------
1. Save uniset/group_set, as freq weigh values updated!
2. Save group_set to a text file.
3. Save group_set to a text file first, if it fails/corrupts,
   we still have data file as backup, VICEVERSA
4. Save txtbuff to a text file.
-------------------------------------------------------------*/
void save_all_data(void)
{

/* Save uniset/group_set, as freq weigh values updated! */
#if SINGLE_PINYIN_INPUT
	draw_msgbox( &gv_fb_dev, 50, 50, 240, "保存uniset..." );
	fb_render(&gv_fb_dev);
	if(UniHan_save_set(uniset, UNIHANS_DATA_PATH)==0)
		draw_msgbox( &gv_fb_dev, 50, 50, 240, "uniset成功保存！" );
	else
		draw_msgbox( &gv_fb_dev, 50, 50, 240, "uniset保存失败！" );
	fb_render(&gv_fb_dev); tm_delayms(500);
#else /* MULTI_PINYIN_INPUT */
	/* Save group_set to txt first, if it fails/corrupts, we still have data file as backup, VICEVERSA */
	draw_msgbox( &gv_fb_dev, 50, 50, 240, "保存group_set.txt..." );
	fb_render(&gv_fb_dev);
	if(UniHanGroup_saveto_CizuTxt(group_set, UNIHANGROUPS_EXPORT_TXT_PATH)==0)
		draw_msgbox( &gv_fb_dev, 50, 50, 240, "group_txt成功保存！" );
	else
		draw_msgbox( &gv_fb_dev, 50, 50, 240, "group_txt保存失败！" );
	fb_render(&gv_fb_dev); tm_delayms(500);
	/* Save group_set to data file */
	draw_msgbox( &gv_fb_dev, 50, 50, 240, "保存group_set..." );
	fb_render(&gv_fb_dev);
	if(UniHanGroup_save_set(group_set, UNIHANGROUPS_DATA_PATH)==0)
		draw_msgbox( &gv_fb_dev, 50, 50, 240, "group_set成功保存！" );
	else
		draw_msgbox( &gv_fb_dev, 50, 50, 240, "group_set保存失败！" );
	fb_render(&gv_fb_dev); tm_delayms(500);
#endif

	/* Save txtbuff to file */
	printf("Saving chmap->txtbuff to a file...");
	if(FTcharmap_save_file(fpath, chmap)==0)
        	draw_msgbox( &gv_fb_dev, 50, 50, 240, "文件已经保存！" );
	else
        	draw_msgbox( &gv_fb_dev, 50, 50, 240, "文件保存失败！" );
        fb_render(&gv_fb_dev);
	tm_delayms(500);
}

/*-----------------------------------
Warning: race condition with main()
FT_Load_Char()...
------------------------------------*/
void signal_handler(int signal)
{
	printf("signal=%d\n",signal);
	if(signal==SIGINT) {
		printf("--- SIGINT ----\n");

//		save_all_data();
//		exit(0);
	}
}


/*-----------------------------------------------------------------
1. Free old data of uniset and group_set.
2. Load PINYIN data of uniset and group_set.
3. Load new_words to expend group_set.
4. Load update_words to fix some pingyings. Prepare pinyin sorting.

Return:
	0	OK
	<0	Fails
------------------------------------------------------------------*/
int load_pinyin_data(void)
{
	int ret=0;

	/* 0. Free old data */
	UniHan_free_set(&uniset);
	UniHanGroup_free_set(&group_set);

  	/* 1. Load UniHan Set for PINYIN Assembly and Input */
  	uniset=UniHan_load_set(UNIHANS_DATA_PATH);
  	if( uniset==NULL ) {
		printf("Fail to load uniset from %s!\n",UNIHANS_DATA_PATH);
		return -1;
	}
	if( UniHan_quickSort_set(uniset, UNIORDER_TYPING_FREQ, 10) !=0 ) {
		printf("Fail to quickSort uniset!\n");
		ret=-2; goto FUNC_END;
	}

	/* 2. Load from text first, if it fails, then try data file. TODO: group_set MAY be incompelet if it failed during saving! */
	group_set=UniHanGroup_load_CizuTxt(UNIHANGROUPS_EXPORT_TXT_PATH);
	if( group_set==NULL) {
                printf("Fail to load group_set from %s, try to load from %s...\n", UNIHANGROUPS_EXPORT_TXT_PATH,UNIHANGROUPS_DATA_PATH);
	  	/* Load UniHanGroup Set Set for PINYIN Input, it's assumed already merged with nch==1 UniHans! */
	  	group_set=UniHanGroup_load_set(UNIHANGROUPS_DATA_PATH);
		if(group_set==NULL) {
	                printf("Fail to load group_set from %s!\n",UNIHANGROUPS_DATA_PATH);
                	ret=-3; goto FUNC_END;
		}
	}
	else {
		/* Need to add uniset to grou_set, as a text group_set has NO UniHans inside. */
        	if( UniHanGroup_add_uniset(group_set, uniset) !=0 ) {
	                printf("Fail to add uniset to  group_set!\n");
                	ret=-3; goto FUNC_END;
        	}
	}

	/* 3. Readin new words and merge into group_set */
        EGI_UNIHANGROUP_SET*  expend_set=UniHanGroup_load_CizuTxt(PINYIN_NEW_WORDS_FPATH);
        if(expend_set != NULL) {
		if( UniHanGroup_merge_set(expend_set, group_set)!=0 ) {
			printf("Fail to merge expend_set into group_set !\n");
			ret=-5; goto FUNC_END;
		}
		if( UniHanGroup_purify_set(group_set)<0 ) {
			printf("Fail to purify group_set!\n");
			ret=-5; goto FUNC_END;
		}
		if( UniHanGroup_assemble_typings(group_set, uniset) != 0) {
                        printf("Fail to assmeble typings for group_set by uniset!\n");
			ret=-5; goto FUNC_END;
                }
	} else {
		printf("Fail to load %s into group_set!\n", PINYIN_NEW_WORDS_FPATH);
		ret=-6;
		goto FUNC_END;
	}

	/* 4. Modify typings */
	EGI_UNIHANGROUP_SET *update_set=UniHanGroup_load_CizuTxt(PINYIN_UPDATE_FPATH);
	if( update_set==NULL) {
		printf("Fail to load %s!\n",PINYIN_UPDATE_FPATH);
		ret=-7;	goto FUNC_END;
	}
	if( UniHanGroup_update_typings(group_set, update_set) < 0 ) {
		printf("Fail to update typings for group_set!\n");
		ret=-7;	goto FUNC_END;
	}

	/* 5. quckSort typing for PINYIN input */
	if( UniHanGroup_quickSort_typings(group_set, 0, group_set->ugroups_size-1, 10) !=0 ) {
		printf("Fail to quickSort typings for group_set!\n");
		ret=-8;
		goto FUNC_END;
	}

FUNC_END:
	/* Free temp. expend_set AND update_set */
	UniHanGroup_free_set(&expend_set);
	UniHanGroup_free_set(&update_set);

	/* If fails, free uniset AND group_set */
	if(ret!=0) {
		UniHan_free_set(&uniset);
		UniHanGroup_free_set(&group_set);
	}

	return ret;
}

/*---------------------
Draw a simple palette.
---------------------*/
void draw_palette(int x0, int y0)
{
	int i,j;
	int row=6;
	int col=10;
	int side=20; /* color square side, in pixels */
	int gap=3;  /* Gap between color squares, also as pad side margin */
	int x,y;
	EGI_16BIT_COLOR color;

#if 0	/* Draw pad */
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY, x0,y0, x0+gap+(side+gap)*col, y0+gap+(side+gap)*row);
	fbset_color(WEGI_COLOR_BLACK);
	draw_rect(&gv_fb_dev, x0-1,y0-1, x0+gap+(side+gap)*col+1, y0+gap+(side+gap)*row+1);

	/* Draw RGB color squares */
	for( i=0; i<row; i++) {
		for(j=0; j<col; j++) {
			/* Complex color */
			//if(j<6)
			//	 color= COLOR_RGB_TO16BITS(0x33*i,0x33*j, 0xFF); //0xFF-0x11*j); //0x99

			/* R G B */
			if(j==6)
				color=COLOR_RGB_TO16BITS(0x55+0x22*i, 0, 0);
			else if(j==7)
				color=COLOR_RGB_TO16BITS(0,0x55+0x22*i, 0);
			else if(j==8)
				color=COLOR_RGB_TO16BITS(0,0,0x55+0x22*i);
			/* Gray */
			else
				color=COLOR_RGB_TO16BITS(0x55+0x22*i,0x55+0x22*i,0x55+0x22*i);

			/* Draw squares */
			draw_filled_rect2(&gv_fb_dev, color, x0+side*j+gap*(j+1), y0+side*i+gap*(i+1),
							    x0+side*j+gap*(j+1) +side-1, y0+side*i+gap*(i+1)+side-1 );
		}
	}
	/* Draw Douglas.R.Jacobs' RGB Hex Triplet Color Chart */
	row=36;
	col=6;
	for( i=0; i<row; i++) {
		for(j=0; j<col; j++) {
			color= COLOR_RGB_TO16BITS(0xFF-0x33*(i/6), 0xFF-0x33*j, 0xFF-0x33*(i%6));
			draw_filled_rect2(&gv_fb_dev, color, x0+23*j, y0+4*i,  x0+23*(j+1)-1, y0+4*(i+1)-1);
		}
	}

#else  /* ANSI control sequence 256 color, for xterm. */

	int margin=3; /* pad side margin, gap=0 */
	side=12;
	col=18;
	row=15;

	/* Draw pad */
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY, x0,y0, x0+2*margin+side*col, y0+2*margin+side*row);
	fbset_color(WEGI_COLOR_BLACK);
	draw_rect(&gv_fb_dev, x0-1,y0-1, x0+2*margin+side*col+1, y0+2*margin+side*row+1);

	/* Code 16-231 */
	for( i=16; i<232; i++ ) {
		color=egi_256color_code(i);
		x=x0+margin+( (((i-16)%6) + (i-16)/72*6)*side);  /* 6 square as a group, vertically 12 group as a colomn.  */
		y=y0+margin+((((i-16)/6)%12)*side);
		draw_filled_rect2(&gv_fb_dev, color, x, y, x+side-1, y+side-1);
	}
	/* Code 0-15 */
	for( i=0; i<16; i++ ) {
		color=egi_256color_code(i);
		x=x0+margin+i*side;
		y=y0+margin+12*side;
		draw_filled_rect2(&gv_fb_dev, color, x, y, x+side-1, y+side-1);
	}
	/* Code 232-255: 24_grade grays */
	for( i=232; i<255; i++ ) {
		color=egi_256color_code(i);
		x=x0+margin+((i-232)%18)*side;
		y=y0+margin+13*side+(i-232)/18*side;
		draw_filled_rect2(&gv_fb_dev, color, x, y, x+side-1, y+side-1);
	}


#endif

}
