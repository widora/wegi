/*------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

mouseXAn example of a simple editor, read input from termios and put it on to
the screen.


		--- Keys and functions ---

Mouse move: 	Move typing cursor to LeftKeyDown to confimr it.
Mouse scroll: 	Scroll charmap up and down.
ARROW_KEYs: 	Shift typing cursor to locate insert/delete position.
BACKSPACE: 	Delete chars before the cursor.
DEL:		Delete chars following the cursor.
HOME(or Fn+LEFT_ARROW!):	Return cursor to the begin of the retline.
END(or Fn+RIGHT_ARROW!):	Move cursor to the end of the retline.
CTRL+N:		Scroll down 1 line, keep typing cursor position.
CTRL+O:		Scroll up 1 line, keep typing curso position.
CTRL+E:		Go to the last page.
CTRL+F:		Save current chmap->txtbuff to a file.
PG_UP:		Page up
PG_DN:		Page down

		--- PINYIN Input ---

CTRL+E:		Switch ON/OFF PINYIN input panel.
ARROW_DOWN:	Show next group of Hanzi.
ARROW_UP:	Show previous group of Hanzi.
BACKSPACE:	Roll back pinyin.
1-7:		Use index to select Haizi in the displaying panel.
SPACE:		Select the first Haizi in the displaying panel


               --- Definition and glossary ---

1. char:    A printable ASCII code OR a local character with UFT-8 encoding.
2. dline:  displayed/charmapped line, A line starts/ends at displaying window left/right end side.
   retline: A line starts/ends by a new line token '\n'.
3. scroll up/down:  scroll up/down charmap by mouse, keep cursor position relative to txtbuff.
	  (UP: decrease chmap->pref (-txtbuff), 	  DOWN: increase chmap->pref (-txtbuff) )

   shift  up/down:  shift typing cursor up/down by ARROW keys.
	  (UP: decrease chmap->pch, 	  DOWN: increase chmap->pch )


Note:
1. Mouse_events and and key_events run in two independent threads.
   When you input keys while moving/scrolling mouse to change inserting position, it can
   happen simutaneously!  TODO: To avoid?


TODO:
1. If any control code other than '\n' put in txtbuff[], strange things
   may happen...  \034	due to UFT-8 encoding.   ---Avoid it.
2. Check whether txtbuff[] overflows.  ---OK
3. English words combination.
4. Mutex lock for chmap data. Race condition between FTcharmap_writeFB and
   FTcharmap_locate_charPos(). OR put mouse actions in editing loop. ---OK
5. Remove (lines) in charmap_writeFB(), use chmap->maxlines instead. ---ok
6. The typing cursor can NOT escape out of the displaying window. it always
   remains and blink in visible area.	---OK
7. IO buffer/continuous key press/ slow writeFB response.


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


#ifdef LETS_NOTE
	#define DEFAULT_FILE_PATH	"/home/midas-zhou/egi/hlm_all.txt"
#else
	#define DEFAULT_FILE_PATH	"/mmc/hlm_all.txt"
#endif

#define 	CHMAP_TXTBUFF_SIZE	64//1024     /* 256,text buffer size for EGI_FTCHAR_MAP.txtbuff */
#define		CHMAP_SIZE		2048 //256  /* NOT less than Max. total number of chars (include '\n's and EOF) that may displayed in the txtbox */

/* TTY input escape sequences.  USB Keyboard hidraw data is another story...  */
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


#define 	TEST_INSERT 	1

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
wchar_t		wcode;
EGI_UNIHAN_SET	*uniset;
static bool enable_pinyin=false;  /* Enable input method PINYIN */
char strPinyin[64]={0};		  /* To store input pinyin string */
char unihans[512][4]; //[256][4]; /* To store UNIHANs complied with input pinyin, [4] in UFT8-encoding.  */
EGI_UNICODE unicodes[512];		  /* To store UNICODE of unihans[512]  MAX for typing 'ji' totally 298 UNIHANs */
unsigned int unindex;
char phan[4];			  /* temp. */
int  unicount;			  /* Count of unihans found */
char strHanlist[128];		  /* List of unihans to be displayed for selecting, 5-10 unihans as a group. */
char strItem[32];		  /* buffer for each UNIHAN with preceding index, example "1.啊"  */
int  GroupMaxItems=7;		  /* Max. number of unihans displayed each time for selecting */
int  HanGroups=0;		  /* Groups with selecting items, each group with Max. GroupMaxItems unihans.
				   * Displaying one group of unihans each time on the PINYIN panel
				   */
int  HanGroupIndex=0;		 /* group index number, < HanGroups */

/* NOTE: char position, in respect of txtbuff index and data_offset: pos=chmap->charPos[pch] */
static EGI_FTCHAR_MAP *chmap;	/* FTchar map to hold char position data */
static unsigned int chns;	/* total number of chars in a string */

static bool mouseLeftKeyDown;
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
	RCMENU_COMMAND_COPY	=0,
	RCMENU_COMMAND_PASTE	=1,
	RCMENU_COMMAND_CUT	=2,
};
static int RCMenu_Command_ID=RCMENU_COMMAND_NONE;

/* Window_Bar_Top Menu Command */
enum WBTMenu_Command {
	WBTMENU_COMMAND_NONE 	=-1,
	WBTMENU_COMMAND_OPEN 	=0,
	WBTMENU_COMMAND_SAVE 	=1,
	WBTMENU_COMMAND_EXIT 	=2,
	WBTMENU_COMMAND_HELP 	=3,
	WBTMENU_COMMAND_ABOUT 	=4,
};
static int WBTMenu_Command_ID=WBTMENU_COMMAND_NONE;

/* Functions */
static int FTcharmap_writeFB(FBDEV *fbdev, EGI_16BIT_COLOR color, int *penx, int *peny);
static void FTcharmap_goto_end(void);
static void FTsymbol_writeFB(char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny);
static void mouse_callback(unsigned char *mouse_data, int size);
static void draw_mcursor(int x, int y);
static void draw_RCMenu(int x0, int y0);  /* Draw right_click menu */
static void draw_WTBMenu(int x0, int y0); /* Draw window_top_bar menu */
static void draw_msgbox( FBDEV *fb_dev, int x0, int y0, int width, const char *msg );
static void draw_progress_msgbox( FBDEV *fb_dev, int x0, int y0, const char *msg, int pv, int pev);
static void RCMenu_execute(enum RCMenu_Command Command_ID);
static void WBTMenu_execute(enum WBTMenu_Command Command_ID);


/* ============================
	    MAIN()
============================ */
int main(int argc, char **argv)
{

 /* <<<<<<  EGI general init  >>>>>> */

  /* Start sys tick */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();

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
  FTsymbol_set_TabWidth(2);


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
  //egi_mouse_setCallback(mouse_callback);
  egi_start_mouseread("/dev/input/mice", mouse_callback);

 /* <<<<<  End of EGI general init  >>>>>> */



                  /*-----------------------------------
                   *            Main Program
                   -----------------------------------*/

        struct termios old_settings;
        struct termios new_settings;

	char ch;
	int  k;
	int lndis; /* Distance between lines */

	if( argc > 1 )
		fpath=argv[1];

  	/* Load UniHan Set for PINYIN Input */
  	uniset=UniHan_load_set(UNIHANS_DATA_PATH);
  	if( uniset==NULL ) exit(1);
	if( UniHan_quickSort_set(uniset, UNIORDER_TYPING_FREQ, 10) !=0 ) exit(2);
	getchar();

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

	chmap=FTcharmap_create( CHMAP_TXTBUFF_SIZE, txtbox.startxy.x, txtbox.startxy.y,		/* txtsize,  x0, y0  */
		  txtbox.endxy.y-txtbox.startxy.y+1, txtbox.endxy.x-txtbox.startxy.x+1, smargin, tmargin,      /*  height, width, offx, offy */
				CHMAP_SIZE, tlns, gv_fb_dev.pos_xres-2*smargin, lndis);   /* mapsize, lines, pixpl, lndis */
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

        /* Init. mouse position */
        mouseX=gv_fb_dev.pos_xres/2;
        mouseY=gv_fb_dev.pos_yres/2;

        /* Init. FB working buffer */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY4);
	FTsymbol_writeFB("File  Help",20,5,WEGI_COLOR_WHITE, NULL, NULL);

	/* TEST: getAdvances ---- */
	int penx=140, peny=5;
	FTsymbol_writeFB("记事本",penx,peny,WEGI_COLOR_LTBLUE, &penx, &peny);
	printf("FTsymbol_writFB get advanceX =%d\n", penx-140);
	printf("FTsymbol_uft8string_getAdvances =%d\n",
		FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fw, fh, (const unsigned char *)"记事本") );

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
	FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_BLACK, NULL, NULL);
	fb_render(&gv_fb_dev);

	/* Charmap to the end */
	system("date");
	int i=0;
	while( chmap->pref[chmap->charPos[chmap->chcount-1]] != '\0' )
	{
		FTcharmap_page_down(chmap);
		FTcharmap_writeFB(NULL, 0, NULL, NULL);
		//FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_BLACK, NULL,NULL);

		if( chmap->txtdlinePos[chmap->txtdlncount] > chmap->txtlen/100 )
			break;

		if( i==20 ) {  /* Every time when i counts to 20 */
		   FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_BLACK, NULL,NULL);
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
	system("date");
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
						if( enable_pinyin && strPinyin[0]!='\0' ) {
							if(HanGroupIndex > 0)
								HanGroupIndex--;
						}
						else
							FTcharmap_shift_cursor_up(chmap);
						break;
					case 66:  /* DOWN arrow : move cursor one display_line down */
						/* Shift displaying UNIHAN group */
						if( enable_pinyin && strPinyin[0]!='\0' ) {
							if(HanGroupIndex < HanGroups-1)
								HanGroupIndex++;
						}
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
			}

			/* reset 0 to ch: NOT a char, just for the following precess to ignore it.  */
			ch=0;
		}

		/* 2. TTY Controls  */
		switch( ch )
		{
		    	case CTRL_P:
				enable_pinyin=!enable_pinyin;  /* Toggle input method to PINYIN */
				if(enable_pinyin)
					bzero(strPinyin,sizeof(strPinyin));
				else {
					unicount=0;		/* clear previous unihans */
					HanGroupIndex=0;
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
				printf("Saving chmap->txtbuff to a file...");
				if(FTcharmap_save_file(fpath, chmap)==0)
	                                draw_msgbox( &gv_fb_dev, 50, 50, 240, "文件已经保存！" );
				else
	                                draw_msgbox( &gv_fb_dev, 50, 50, 240, "文件保存失败！" );
                                fb_render(&gv_fb_dev);
				tm_delayms(300);
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
				if( UniHan_locate_typing(uniset, strPinyin) == 0 ) {
					unicount=0;
					/* Put all complied UNIHANs to unihans[][4] */
					bzero(&unihans[0][0], sizeof(unihans));
		       			while( strncmp( uniset->unihans[uniset->puh].typing, strPinyin, UNIHAN_TYPING_MAXLEN ) == 0 ) {
						char_unicode_to_uft8(&(uniset->unihans[uniset->puh].wcode), unihans[unicount]);
						unicount++;
                				uniset->puh++;
					}
					HanGroups=(unicount+GroupMaxItems-1)/GroupMaxItems;
        			}
			}
			else
				FTcharmap_go_backspace( chmap );
			ch=0;
		}
		/* --- EDIT:  3.2 Insert chars, !!! printable chars + '\n' + TAB  !!! */
		else if( ch>31 || ch==9 || ch==10 ) {  //pos>=0 ) {   /* If pos<0, then ... */

			/* 3.2.1 PINYING Input, put to strPinyin[]  */
	   		if( enable_pinyin ) {  //&& ( ch==32 || isdigit(ch) || isalpha(ch) ) ) {
				/* SPACE: insert SAPCE */
				if( ch==32 ) {
					/* If no strPinyin is empty, insert SPACE into txt */
					if( strPinyin[0]=='\0' ) {
						FTcharmap_insert_char( chmap, &ch);
					/* Else, insert the first unihan in the displaying panel  */
					} else {
						FTcharmap_insert_char( chmap, &unihans[HanGroupIndex*GroupMaxItems][0]);
						/* Clear strPinyin and unihans[]  */
						bzero(strPinyin,sizeof(strPinyin));
						unicount=0;
						//bzero(&unihans[0][0], sizeof(unihans));
					}
				}
				/* DIGIT: Select unihan OR insert digit */
				else if( isdigit(ch) ) { //&& (ch-48>0 && ch-48 <= GroupMaxItems) ) {   /* Select PINYIN result 48-'0' */
					/* Select unihan with index (from 1 to 7) */
					if(strPinyin[0]!='\0' && (ch-48>0 && ch-48 <= GroupMaxItems) ) {   /* unihans[] NOT cleared */
						unindex=HanGroupIndex*GroupMaxItems+ch-48-1;
					   	FTcharmap_insert_char( chmap, &unihans[unindex][0]);

						/* Increase freq weigh value */
						printf("unicodes[%d]: U+%X\n", unindex, unicodes[unindex]);
						UniHan_increase_freq(uniset, strPinyin, unicodes[unindex], 10);

						/* Clear strPinyin and unihans[] */
						bzero(strPinyin,sizeof(strPinyin));
						unicount=0;
						//bzero(&unihans[0][0], sizeof(unihans));
					}
					else  {  /* Insert digit into TXT */
						bzero(phan,sizeof(phan));
						char_DBC2SBC_to_uft8(ch, phan);
						FTcharmap_insert_char(chmap, phan);
					}
				}
				/* ALPHABET: Input as PINYIN */
				else if( isalpha(ch) && islower(ch) ) {
					/* Continue input to strPinyin ONLY IF space is available */
					if( strlen(strPinyin) < sizeof(strPinyin)-1 ) {
					    strPinyin[strlen(strPinyin)]=ch;
					    printf("strPinyin: %s\n", strPinyin);
					    /* Since strPinyin updated: To find out all unihans complying with input pinyin  */
					    HanGroupIndex=0;
					    /* Put all complied UNIHANs to unihans[][4] */
                                            bzero(&unihans[0][0], sizeof(unihans));
					    if( UniHan_locate_typing(uniset,strPinyin) == 0 ) {
						 unicount=0;
						 k=uniset->puh;
						 while( strncmp( uniset->unihans[k].typing, strPinyin, UNIHAN_TYPING_MAXLEN) == 0 ) {
							 char_unicode_to_uft8(&(uniset->unihans[k].wcode), unihans[unicount]);
							 unicodes[unicount]=uniset->unihans[k].wcode;
							 unicount++;
							 if(unicount > sizeof(unihans)/sizeof(unihans[0]) )
								break;
							 k++;
							 //uniset->puh++;
						}
						HanGroups=(unicount+GroupMaxItems-1)/GroupMaxItems;
					    }
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

		/* CHARMAP & DISPLAY: Display txt */
        	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
		FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_BLACK, NULL, NULL);

		/* PINYING Input Display */
		if(enable_pinyin) {
			/* Back pad */
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYB, 0, txtbox.endxy.y-60, txtbox.endxy.x, txtbox.endxy.y );

			/* Decoration for PINYIN bar */
			FTsymbol_writeFB("EGI全拼输入", 320-120, txtbox.endxy.y-60+3, WEGI_COLOR_ORANGE, NULL, NULL);
			fbset_color(WEGI_COLOR_WHITE);
			draw_line(&gv_fb_dev, 15, txtbox.endxy.y-60+3+28, 320-15,txtbox.endxy.y-60+3+28);

			/* Display PINYIN */
			FTsymbol_writeFB(strPinyin, 15, txtbox.endxy.y-60+3, WEGI_COLOR_FIREBRICK, NULL, NULL);

			/* Display grouped UNIHANs for selecting  */
			if( unicount>0 ) {
				//printf("unicount=%d, HanGroups=%d, HanGroupIndex=%d\n", unicount, HanGroups,HanGroupIndex);
				bzero(strHanlist, sizeof(strHanlist));
				for( i=0; i < GroupMaxItems && i<unicount; i++ ) {
					snprintf(strItem,sizeof(strItem),"%d.%s ",i+1,unihans[HanGroupIndex*GroupMaxItems+i]);
					strcat(strHanlist, strItem);
				}
				FTsymbol_writeFB(strHanlist, 15, txtbox.endxy.y-60+3+30, WEGI_COLOR_BLACK, NULL, NULL);
			}
		}

		/* POST:  check chmap->errbits */
		if( chmap->errbits ) {
			printf("WARNING: chmap->errbits=%#x \n",chmap->errbits);
		}

	   /* --- Handle Active Menu: active_menu_handle --- */
	   switch( ActiveComp )
	   {
		/* Righ_click menu */
		case CompRCMenu:
			/* Backup desktop bkg */
	        	fb_copy_FBbuffer(&gv_fb_dev,FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */

			while( !start_pch ) {  /* Wait for click */
        			fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
				draw_RCMenu(menuX0, menuY0);


       				draw_mcursor(mouseMidX, mouseMidY);
				fb_render(&gv_fb_dev);
			}
			/* Execute Menu Command */
			RCMenu_execute(RCMenu_Command_ID);

			/* Exit Menu handler */
			ActiveComp=CompTXTBox;
			break;

		/* Window Bar menu */
		case CompWTBMenu:
			/* In case that start_pch MAY have NOT reset yet */
			start_pch=false;

			printf("WTBmenu Open\n");
			/* Backup desktop bkg */
	        	fb_copy_FBbuffer(&gv_fb_dev,FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */

			while( !start_pch || mouseLeftKeyDown==false ) {  /* Wait for next click */
        			fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
				draw_WTBMenu( txtbox.startxy.x, txtbox.startxy.y );

       				draw_mcursor(mouseMidX, mouseMidY);
				fb_render(&gv_fb_dev);
			}
			printf("WTBmenu Close\n");
			/* Execute Menu Command */
			WBTMenu_execute(WBTMenu_Command_ID);

			/* Exit Menu handler */
			ActiveComp=CompTXTBox;
			break;
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
        tcsetattr(0, TCSANOW,&old_settings);

	/* My release */
	FTcharmap_free(&chmap);

goto MAIN_START;


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
@px,py: 	LCD X/Y for start point.
@color: 	Color for text
@penx, peny:    pointer to pen X.Y.
--------------------------------------*/
static int FTcharmap_writeFB(FBDEV *fbdev, EGI_16BIT_COLOR color, int *penx, int *peny)
{
	int ret;

       	ret=FTcharmap_uft8strings_writeFB( fbdev, chmap,          	/* FBdev, charmap*/
                                        egi_sysfonts.regular, fw, fh,   /* fontface, fw,fh */
	                                        color, -1, 255,      		/* fontcolor, transcolor,opaque */
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
                FTcharmap_writeFB(NULL, 0, NULL, NULL);
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
static void FTsymbol_writeFB(char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny)
{
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,    	/* fw,fh, pstr */
                                        320, tlns, fgap,      /* pixpl, lines, fgap */
                                        px, py,                                 /* x0,y0, */
                                        color, -1, 255,      			/* fontcolor, transcolor,opaque */
                                        NULL, NULL, penx, peny);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}


/*--------------------------------------------
        Callback for mouse input
1. Just update mouseXYZ
2. Check and set ACTIVE token for menus.
---------------------------------------------*/
static void mouse_callback(unsigned char *mouse_data, int size)
{
//	int i;
	int mdZ;
	EGI_POINT  pxy={0,0};

        /* 1. Check pressed key */
        if(mouse_data[0]&0x1) {
		/* Start selecting */
		if( mouseLeftKeyDown==false ) {  /* check old status */
                	printf("Leftkey press!\n");
			start_pch=true;

			/* Activate window top bar menu */
			if( mouseY < txtbox.startxy.y && mouseX <90 )
				ActiveComp=CompWTBMenu;

			/* reset pch2 = pch */
			if( ActiveComp==CompTXTBox )
				FTcharmap_reset_charPos2(chmap);
		}
		else {
			/* Leftkey press hold */
			start_pch=false;
		}
		/* ! Set at last */
		mouseLeftKeyDown=true;
	}
	else {
		/* End selecting */
		if(mouseLeftKeyDown==true) {
			printf("LeftKey release!\n");
		}
		start_pch=false;
		/* ! Set at last */
		mouseLeftKeyDown=false;
	}

	/* Right click down */
        if(mouse_data[0]&0x2) {
                printf("Right key down!\n");
		pxy.x=mouseX; pxy.y=mouseY;
		if( point_inbox(&pxy, &txtbox ) ) {
			ActiveComp=CompRCMenu;
			menuX0=mouseX;
			menuY0=mouseY;
		}
	}

        if(mouse_data[0]&0x4)
                printf("Mid key down!\n");

        /*  2. Get mouse X */
        mouseX += ( (mouse_data[0]&0x10) ? mouse_data[1]-256 : mouse_data[1] );
        if(mouseX > gv_fb_dev.pos_xres -5)
                mouseX=gv_fb_dev.pos_xres -5;
        else if(mouseX<0)
                mouseX=0;

        /* 3. Get mouse Y: Notice LCD Y direction!  Minus for down movement, Plus for up movement!
         * !!! For eventX: Minus for up movement, Plus for down movement!
         */
        mouseY -= ( (mouse_data[0]&0x20) ? mouse_data[2]-256 : mouse_data[2] );
        if(mouseY > gv_fb_dev.pos_yres -5)
                mouseY=gv_fb_dev.pos_yres -5;
        else if(mouseY<0)
                mouseY=0;

      	mouseMidY=mouseY+fh/2;
       	mouseMidX=mouseX+fw/2;
//       	printf("mouseMidX,Y=%d,%d \n",mouseMidX, mouseMidY);

        /* 4. Get mouse Z */
	mdZ= (mouse_data[3]&0x80) ? mouse_data[3]-256 : mouse_data[3] ;
        mouseZ += mdZ;
        //printf("get X=%d, Y=%d, Z=%d, dz=%d\n", mouseX, mouseY, mouseZ, mdZ);
	if( mdZ > 0 ) {
		 FTcharmap_scroll_oneline_down(chmap);
	}
	else if ( mdZ < 0 ) {
		 FTcharmap_scroll_oneline_up(chmap);
	}

	/* 5. To get current cursor/pchoff position  */
	if(mouseLeftKeyDown && ActiveComp==CompTXTBox ) {
		/* Cursor position/Start of selection */
		if(start_pch) {
		        /* continue checking request! */
           		while( FTcharmap_locate_charPos( chmap, mouseMidX, mouseMidY )!=0 ){ tm_delayms(5);};
           		/* set random mark color */
           		FTcharmap_set_markcolor( chmap, egi_color_random(color_medium), 80);
		}
		/* End of selection */
		else {
		   	/* Continue checking request! */
		   	while( FTcharmap_locate_charPos2( chmap, mouseMidX, mouseMidY )!=0){ tm_delayms(5); } ;

		}
	}
}

/*--------------------------------
 Draw cursor for the mouse movement
1. In txtbox area, use typing cursor.
2. Otherwise, apply mcursor.
----------------------------------*/
static void draw_mcursor(int x, int y)
{
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

}


/*-------------------------------------
Draw right_click menu.
@x0,y0:	Left top origin of the menu.
--------------------------------------*/
static void draw_RCMenu(int x0, int y0)
{
	char *mwords[3]={"Copy", "Paste", "Cut" };

	int mw=70;   /* menu width */
	int smh=33;  /* sub menu height */
	int mh=smh*3;  /* menu height */

	int i;
	int mp;	/* 0,1,2 OR <0 Non,  Mouse pointed menu item */

	/* Decide submenu items as per mouse position on the right_click menu */
	if( mouseX>x0 && mouseX<x0+mw && mouseY>y0 && mouseY<y0+mh )
		mp=(mouseY-y0)/smh;
	else
		mp=-1;

	/* Assign command ID */
	RCMenu_Command_ID=mp;

	/* Draw submenus */
	for(i=0; i<3; i++)
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

/*-------------------------------------
Draw window top bar menu.
@x0,y0:	Left top origin of the menu.
--------------------------------------*/
static void draw_WTBMenu(int x0, int y0)
{
	static int  mtag=-1;	/* 0-File,  1-Help */

	char *MFileItems[3]={"Open", "Save", "Exit"};
	char *MHelpItems[2]={"Help", "About" };

	int mw=70;   /* menu width */
	int smh=33;  /* menu item height */
	int mh=smh*3;  /* menu box height */

	int i;
	int mp;	/* 0,1,2 OR <0 Non,  Mouse pointed menu item */

	/* On 'File' or 'Help': Only to select it. NOT deselect! */
	if( mouseX<90 && mouseY<txtbox.startxy.y ) {
		mtag=mouseX/45;
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
	for(i=0; i<3; i++)
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
	if( mouseX>x0+45 && mouseX<x0+45+mw && mouseY>y0 && mouseY<y0+mh )
		mp=(mouseY-y0)/smh;
	else
		mp=-1;  /* Not on the menu */

	/* Assig WBTMenu_Command_ID */
	if( mp > -1 )
		WBTMenu_Command_ID=3+mp;
	else
		WBTMenu_Command_ID=-1;

	/* Draw itmes of tag 'Help' */
	for(i=0; i<2; i++) /* 2 itmes */
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
	draw_rect(&gv_fb_dev, x0+45, y0, x0+45+mw, y0+2*smh);
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
	int tmargin=30; /* Top maring, including dot marks */
	int bmargin=15; /* Bottom maring */
	int smargin=15;	/* Side margin */
	int lndis;	/* Distance between lines */
	int peny;

        EGI_16BIT_COLOR boxcolor=WEGI_COLOR_GRAY4; 	/* Color of the msgbox. */
	EGI_8BIT_ALPHA  boxalpha=255;			/* Alpha value of the msgbox. */
        EGI_16BIT_COLOR fontcolor=WEGI_COLOR_WHITE; 	/* Font color. */

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
        draw_blend_filled_roundcorner_rect( fb_dev, x0,y0, x0+width-1, y0+height-1, 9,    /* fbdev, x1, y1, x2, y2, r */
                                            boxcolor, boxalpha);                           /* color, alpha */

	/* Draw top mark */
	fbset_color2(fb_dev, WEGI_COLOR_ORANGE);
	draw_filled_circle( fb_dev, x0+width-15, y0+15, 10 );
	fbset_color2(fb_dev, WEGI_COLOR_LTGREEN);
	draw_filled_circle( fb_dev, x0+width-15-25, y0+15-1, 10 ); /* -1, to balance visual error */
	//fbset_color2(fb_dev, WEGI_COLOR_LTBLUE);
	//draw_filled_circle( fb_dev, x0+width-15-25-25, y0+15, 10 );


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
	if(RCMenu_Command_ID<0)
		return;

	switch(RCMenu_Command_ID) {
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
		default:
			printf("RCMENU_COMMAND_NONE\n");
			break;
	}
}


/*----------------------------------------------------
Exectue command for selected right_click menu

@Command_ID: WBTMenu Command ID, <0 ingnore.
-----------------------------------------------------*/
static void WBTMenu_execute(enum WBTMenu_Command Command_ID)
{
	if(Command_ID<0)
		return;

	switch(Command_ID) {
		case WBTMENU_COMMAND_OPEN:
			printf("WBTMENU_COMMAND_OPEN\n");
			break;
		case WBTMENU_COMMAND_SAVE:
			printf("WBTMENU_COMMAND_SAVE\n");
			FTcharmap_save_file(fpath, chmap);

			start_pch=false;
                        while( !start_pch ) {  /* Wait for click */
                                fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
				draw_msgbox( &gv_fb_dev, 50, 50, 240, "文件已经保存！" );
                                draw_mcursor(mouseMidX, mouseMidY);
                                fb_render(&gv_fb_dev);
                        }

			break;
		case WBTMENU_COMMAND_EXIT:
			printf("WBTMENU_COMMAND_EXIT\n");
			break;
		case WBTMENU_COMMAND_HELP:
			printf("WBTMENU_COMMAND_HELP\n");
			break;
		case WBTMENU_COMMAND_ABOUT:
			printf("WBTMENU_COMMAND_ABOUT\n");

			start_pch=false;
                        while( !start_pch ) {  /* Wait for click */
                                fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
				draw_msgbox( &gv_fb_dev, 50, 50, 240, "　　这是用EGI编制的一个简单的记事本. Widora和他的小伙伴们" );
                                draw_mcursor(mouseMidX, mouseMidY);
                                fb_render(&gv_fb_dev);
                        }

			break;
		default:
			printf("WBTMENU_COMMAND_NONE\n");
			break;
	}
	printf("WBTMENU_COMMAND_NONE\n");

}
