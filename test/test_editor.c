/*------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of a simple editor, read input from termios and put it on to
the screen.

      [ --- Editor Version_B ----:  With A Render_Surface Thread  ]

Thread_1. Main() thread
Thread_2. mouse_callback()
Thread_3. Thread refresh_surface()


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
'+'             Switch ON/OFF hexadecimal Unicode direct input mode.

Note:
  	1. Use '(0x27) as delimiter to separate PINYIN entry. Example: xian --> xi'an.
  	2. Press ESCAPE to clear PINYIN input buffer.
	3. See PINYIN DATA LOAD/SAVE PROCEDURE in "egi_unihan.h".

               ( --- 3. Definition and glossary --- )

1. char:    A printable ASCII code OR a local character with UFT-8 encoding.
2. dline:   A displayed/charmapped line, A line starts/ends at displaying window left/right end side.
   retline: A line starts/ends by a new line token '\n'. OR the very first line.
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
3. When adding a new unihangroup, you'd better provide with correct typing.
   Example: '罪行 zui xing'   Select them together, then RightClick to activate mini menu and click 'Words'
   to add it.
   If ONLY input '罪行' and let it generate typing automatically, it MAY give WRONG typings such as 'zui hang'.
4. Type in ' as separator for typing. Example: ju'e for 巨额．
5. Correct typing for an existed unihangroup: Input the unihangroup and correct typing in file PINYIN_UPDATE_FPATH,
   then call UniHanGroup_load_CizuTxt() and UniHanGroup_update_typings().

	( ----- Following with pthread_render ----- )
6. Three threads operation.
   6.1 Main for stdin input  (MSG requeset for rendering).
   6.2 Mouse callback for mouse input (MSG requeset for rendering).
   6.3 Pthread_render for surface image rendering.
7. Should avoid more than one thread writing/rendering the surface simutaneously!
8. Normally tcsetattr() as blocking mode to save CPU, However in some cases, it needs nonblocking mode,
   Example: when read ESCAPE, see 1.0, 1.3.
9. mutex_render critical zone should encompass egi_sysfonts.lib_mutex critical zone, that can
   avoid mutimutex conflictions.
10. The VSCROLLBAR is active ONLY when the file opens in read_only mode.

TODO:
1. English words combination.
2. IO buffer/continuous key press/ slow writeFB response.
3. If two UniGroups have same wcodes[] and differen typings,  Only one will
   be saved in group_test.txt/unihangroups_pinyin.dat!
   Example:  重重 chong chong   重重 zhong zhong
4. A single unihan may be added into PINYIN_NEW_WORDS_FPATH and loaded to group_set,
   However it will NOT be added into uniset here!
5. If predefined search position soff happens to be WITHIN utf8-encoding of an unihan, then
   the editing cursor will NOT appear, as FTcharmap_uft8strings_writeFB() fails to get chmap->pch!
   soff shall be located somewhere just between two unihans/uchars.
XXX 6. tan Up/Down sefault. SEE Note 9.
XXX 7. MSG squeue auto. clear.
8. After select and click on CompRCMenu (COPY etc.), it will get a LeftKeyDown, and MAYBE also inlucdes
   LeftKeyDownHold(s) if you happen to press the key for a littler too longer, and the LeftKeyDownHold
   will then change current selection unexpectedly!
9. Input 'y' for PINYIN will make group_set->results_size exceed group_set->results_capacity, see UniHanGroup_locate_typings().

Journal
2021-1-9/10:
	1. Revise FTcharmap_writeFB() to get rid of fontcolor, which will now decided by
	   chmap->fontcolor OR chmap->charColorMap.
	2. FTcharmap_create(): Add param 'charColorMap_ON' and 'fontcolor'.
	3. Add RCMENU_COMMAND_COLOR and WBTMENU_COMMAND_MORE.
	4. Add color palette.
2022-05-19:
	1. Add opt to locate search position.
	2. Add save page pos offset for bookmark.
2022-05-20:
	1. Compare charmap result with FBDEV=NULL and FBDEV!=NULL.
2022-05-23:
	1. Add refresh_surface()
2022-05-24:
	1. Add msg queue for requesting render.
2022-06-01:
	1. Enable hexadecimal Unicode input mode.
2022-06-02:
	1. Add mutex_render to sync variables for PINYIN IME, GroupStartIndex, GroupItems ...etc.
	2. render_surface(): Drop all MSG in the queue before rendering surface.
	3. render_surface(): Render surface at regular intervals (without request MSG)
2022-06-03:
	1. mutex_render critical zone should encompass egi_sysfonts.lib_mutex critical zone.
	2. main(): Call FTcharmap_writeFB(NULL...) to update charmap for each input round.
	3. CTRL+V for paste.
2022-06-05:
        1. WBTMenu_execute(): case WBTMENU_COMMAND_HELP: show the UNICODE at current pchoff.
	2. main()::Case PageUp/Down: To avoid incomplete charmap being displayed, replace &gv_fb_dev with NULL.
	3. Add sigQuit, and tcsetattr() also be render_mutex_locked!
2022-06-21:
	1. Add VScrollBar for charmap.
2022-06-21:
	1. To display VScrollBar beside txtbox. --- For read_only mode.
	2. Click on VScrollBar to update vsbar and chmap.
2022-06-27:
	1. Test LETS_NOTE
	2. Add semaphore for render sync.

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
知之者不如好之者好之者不如乐之者
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
#include "egi_surfcontrols.h"

#ifdef LETS_NOTE
	#define DEFAULT_FILE_PATH	"/home/midas-zhou/egi/hlm_all.txt"
	#define DEFAULT_FONTBUFF_PATH   "/home/midas-zhou/egi/fontbuff_regular.dat"
#else
	#define DEFAULT_FILE_PATH	"/mmc/hlm_all.txt"
	#define DEFAULT_FONTBUFF_PATH   "/mmc/fontbuff_regular.dat"
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
#define		CTRL_S  19	/* Ctrl + S: ... CTRL_C .... all occupied by console */
#define         CTRL_V  22	/* Ctrl + v: Paste */
#define		CTRL_E  5	/* Ctrl + e: go to last page */
#define		CTRL_H  8	/* Ctrl + h: go to the first dline, head of the txtbuff */
#define		CTRL_P  16	/* Ctrl + p: PINYIN input ON/OFF toggle */

char *strInsert="Widora和小伙伴们";
char *fpath="/mmc/hello.txt";

struct termios old_settings;
struct termios new_settings;

bool sigQuit;	/* Signal token to quit program */

static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 240-1} };	/* Text displaying area */
/* ----> To adjust later */


/* Input Method: PINYIN */
wchar_t wcode;

EGI_UNIHAN_SET *uniset;
EGI_UNIHANGROUP_SET *group_set;
static bool enable_pinyin=false;
static bool enable_unicode_input=false; /* To input unicode directly for signle Unihan/Emoji,
                                         * To enable ONLY if enable_pinyin is TRUE.
                                         * display_pinyin[] for buffering Unicode.
                                         */

/* Enable input method PINYIN */
char strPinyin[4*8]={0}; /* To store unbroken input pinyin string */
char group_pinyin[UNIHAN_TYPING_MAXLEN*4]; 	/* For divided group pinyins */
EGI_UNICODE group_wcodes[4];			/* group wcodes  */
char display_pinyin[UNIHAN_TYPING_MAXLEN*4];    /* For displaying pinyin, OR buffering Unicode */
unsigned int pos_uchar;
char unihans[512][4*4]; //[256][4]; /* To store UNIHANs/UNIHANGROUPs complied with input pinyin, in UFT8-encoding. 4unihans*4bytes */
EGI_UNICODE unicodes[512]; /* To store UNICODE of unihans[512] MAX for typing 'ji' totally 298 UNIHANs */
unsigned int unindex;
char phan[4]; 		/* temp. */
int  unicount; 		/* Count of unihan(group)s found */
char strHanlist[128]; 	/* List of unihans to be displayed for selecting, 5-10 unihans as a group. */
char strItem[32]; 	/* buffer for each UNIHAN with preceding index, example "1.啊" */
int  GroupStartIndex; 	/* Start index for displayed unihan(group)s
			 * Update in UPP/DOWN arrow, BACKSPACE, ESCAPE, insert_ch operation for PINYIN IME
			 */
int  GroupItems;	/* Current unihan(group)s items in selecting panel, to be updated in render_surface().
			 * See 'Display unihan(group)s for selecting' in render_surface()
			 */
const int  GroupMaxItems=7; 	/* Max. number of unihan(group)s displayed for selecting */
int  HanGroups=0; 	/* Groups with selecting items, each group with Max. GroupMaxItems unihans.
	          	 * Displaying one group of unihans each time on the PINYIN panel
		  	 */
int  HanGroupIndex=0;	/* group index number, < HanGroups */

/* NOTE: char position, in respect of txtbuff index and data_offset: pos=chmap->charPos[pch] */
static EGI_FTCHAR_MAP *chmap;	/* FTchar map to hold char position data */
static bool ReadOnly;		/* Readonly for chmap */
static unsigned int chns;	/* total number of chars in a string */
/* Vertical SCROLLBAR */
EGI_VSCROLLBAR *vsbar;		/* Vertical scroll bar */
/* Note:
 1. NOW ONLY for read_only case.
 2. vsbar->maxLen holds the max numbers of txtdlines charmped, as numbers of valid chmap->txtdlinePos[].
*/

/* Search position, if soff>=0, then spcnt will be ignored */
float spcnt;      /* Search position, percent of total txtlen, ---> abt 0.01 for one page */
off_t soff=-1;    /* Search position, offset value from the beginning */

static bool mouseLeftKeyDown;
static bool mouseLeftKeyUp;
static bool start_pch=false;	      /* True if mouseLeftKeyDown switch from false to true, --- RisingEdge */
static int  mouseX, mouseY; //mouseZ; /* Mouse point position */
static int  mouseMidX, mouseMidY;     /* Mouse cursor mid position */
static int  menuX0, menuY0;

/* For fonts in charmap txtbox */
static int fw=20; //30; //18;	/* Font size */
static int fh=22; //40; //20;
static int fgap=22/3; //5;	/* Text line fgap : TRICKY ^_- */
static int smargin=5; 		/* left and right side margin of text area, If NOT big enough, part of glyph may extend to next line. example ','. */
static int tmargin=3; //fh/4	/* top margin of text area */
static int tlns;  //=(txtbox.endxy.y-txtbox.startxy.y)/(fh+fgap); /* Total available line space for displaying txt */

/* Font size for UI */
int fwUI=18;
int fhUI=20;

//static int penx;	/* Pen position */
//static int peny;


/* Component elements in the APP, Only one of them is active at one point. */
enum CompElem
{
	CompNone=0,
	CompTXTBox,	/* Txtbox */
	CompRCMenu,	/* Right click menu */
	CompWTBMenu,	/* Window top bar menu */
	CompVSBar,	/* VScrollBar */
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
	WBTMENU_COMMAND_HIGHLIGHT   =7, /* mark */
	WBTMENU_COMMAND_SAVEPOS =8,  /* Save current chmap->pref offset to chmap->txtbuff for bookmark */
};
static int WBTMenu_Command_ID=WBTMENU_COMMAND_NONE;

//#define MSG_SYNC_RENDER
#ifdef MSG_SYNC_RENDER  /* Message queue ----- MSG_SYNC_RENDER for Render ----- */

   #include <sys/ipc.h>
   #include <sys/msg.h>
   key_t mqkey;
   int render_msgid, kev_msgid, mev_msgid;

   #define MSG_REFRESH_SURFACE  1

   /*** See 'uapi/linux/msg.h': message buffer for msgsnd and msgrcv calls
    *	struct msgbuf {
    *    	__kernel_long_t mtype;          // type of message
    *    	char mtext[1];                  // message text
    *	};
    */
   typedef struct msgbuf MSG_BUFF;

#else     /* Unnamed semaphore ----- SEM_SYNC_RENDER for Render ----- */
   #include <semaphore.h>
   sem_t render_sem;

#endif

pthread_t  pthread_render;
pthread_mutex_t mutex_render;	/* To lock necessary vars  */
void *render_surface(void* argv);
bool chmap_ready;

/* Functions */
//static int FTcharmap_writeFB(FBDEV *fbdev, EGI_16BIT_COLOR color, int *penx, int *peny);
static int FTcharmap_writeFB(FBDEV *fbdev, int *penx, int *peny);
static int FTcharmap_writeFB2(EGI_FTCHAR_MAP *chmap, FBDEV *fbdev, int *penx, int *peny); /* For TEST */
static void FTcharmap_goto_end(void);
static void FTsymbol_writeFB(const char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny);/* For UI, font is regular type. */
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
void refresh_surface(void);


/* ============================
	    MAIN()
============================ */
int main(int argc, char **argv)
{
 /* <<<<<<  EGI general init  >>>>>> */

  /* Start sys tick */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();

  #if 1  /* Set signal action */
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

  #if 0  /* Load emojis NOPE */
  printf("FTsymbol_load_sysemojis()...\n");
  if(FTsymbol_load_sysemojis() !=0) {
        printf("Fail to load FT sysemojis, quit.\n");
        return -1;
  }
  #endif

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
	char  ch;
	int   i,j,k;
	int   lndis; /* Distance between lines */

        int opt;
        while( (opt=getopt(argc,argv,"hrp:o:"))!=-1 ) {
                switch(opt) {
                        case 'h':
				printf("Usage: %s hrp:o:\n", argv[0]);
				printf(" -h   This help\n");
				printf(" -r   For read_only\n");
				printf(" -p:  Position in percentage\n");
				printf(" -o:  Offset\n");
                                exit(0);
                                break;
			case 'r':
				ReadOnly=true;
				break;
                        case 'p':
                                spcnt=atof(optarg);
                                break;
			case 'o':
				soff=atoi(optarg);
				break;
                }
        }
	if(argv[optind])
		fpath=argv[optind];

	/* Restore bookmark */
	if(soff<0) {
		char strval[65];
		if( egi_get_config_value("BOOKMARK", "offset", strval)==0 ) {
			soff=atoi(strval);
		}
	}

	egi_dpstd(DBG_YELLOW"Input spcnt:%.2f, soff=%jd, fpath: '%s'\n"DBG_RESET, spcnt, soff, fpath);

#if 0	/* S0. Buffer sysfonts.regular */
	printf("Start buffer egi_sysfonts.regular...\n");
        egi_fontbuffer=FTsymbol_create_fontBuffer(egi_sysfonts.regular, fw,fh, 0x4E00, 21000); /*  face, fw,  fh, wchar_t unistart, size_t size */
        if(egi_fontbuffer)
                printf("Font buffer size=%zd!\n", egi_fontbuffer->size);
	else {
		printf("Fail to buffer egi_sysfonts.regular!\n");
		exit(1);
	}
#else  /* S0. Load buffered sysfonts.regular */
	printf("Start loading fontBuffer of egi_sysfonts.regular...\n");
	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY);
	draw_msgbox(&gv_fb_dev, 30, 50, 260, "Loading fontBuffer of egi_sysfonts.regular...");
	fb_render(&gv_fb_dev);
	egi_fontbuffer=FTsymbol_load_fontBuffer(DEFAULT_FONTBUFF_PATH);
        if(egi_fontbuffer) {
                printf("Finish loading egi_fontbuffer, size=%d!, fw=%d, fw=%d\n",
					egi_fontbuffer->size, egi_fontbuffer->fw, egi_fontbuffer->fh);
		/* !!! Have to assign !!! */
		egi_fontbuffer->face=egi_sysfonts.regular;
	}
	else {
		printf("Fail to load fontBuffer for egi_sysfonts.regular!\n");
		exit(1);
	}
#endif

	/* S1. Load necessary PINYIN data, and do some prep. */
	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY);
	draw_msgbox(&gv_fb_dev, 30, 50, 260, "Loading UNIHAN PINYIN data...");
	fb_render(&gv_fb_dev);
	if( load_pinyin_data()!=0 )
		exit(1);

MAIN_START:
	/* S2. Set TextBox size and create FTcharmap */
	/* S2.1 Reset main window size */
	txtbox.startxy.x=0;
	txtbox.startxy.y=30;
	txtbox.endxy.x=gv_fb_dev.pos_xres-1 -(ReadOnly?10:0);
	txtbox.endxy.y=gv_fb_dev.pos_yres-1;

	/* S2.2 Activate main window */
	ActiveComp=CompTXTBox;

	/* S2.3 Total available lines of space for displaying chars */
#if 1
	lndis=fh+fgap;
#else
	lndis=FTsymbol_get_FTface_Height(egi_sysfonts.regular, fw, fh);
#endif
	tlns=(txtbox.endxy.y-txtbox.startxy.y+1 -2*tmargin)/lndis;  // 10 for VScrollBar 
	printf("Blank lines tlns=%d, lndis=%d\n", tlns, lndis);

	/* S2.4 Create charMAP. chmapHeidth/Widht should include margin area. */
	int chmapHeight=2*tmargin+tlns*lndis;//last fgap!  +fh/2;
	int chmapWidth=txtbox.endxy.x-txtbox.startxy.x+1;
	int pixpl=chmapWidth-2*smargin;
	chmap=FTcharmap_create( CHMAP_TXTBUFF_SIZE, txtbox.startxy.x, txtbox.startxy.y,	 /* txtsize,  x0, y0  */
	  		chmapHeight, chmapWidth, smargin, tmargin, /*  height, width, offx, offy */
			egi_sysfonts.regular, fw,fh, CHMAP_SIZE, tlns, pixpl, lndis,   	 /* typeface, fw, fh, mapsize, lines, pixpl, lndis */
			//WEGI_COLOR_WHITE, WEGI_COLOR_BLACK, true, true );  /*  bkgcolor, fontcolor, charColorMap_ON, hlmarkColorMap_ON */
			COLOR_LemonChiffon, WEGI_COLOR_BLACK, true, true );  /*  bkgcolor, fontcolor, charColorMap_ON, hlmarkColorMap_ON */
	if(chmap==NULL){ printf("Fail to create char map!\n"); exit(0); };
	printf("chmap->height: %d\n", chmap->height);

	/* Set READ_ONLY */
	chmap->readOnly=ReadOnly;

	/* S2.4A Create VScrollBar */
	if(chmap->readOnly)
  	     vsbar=egi_VScrollBar_create(txtbox.endxy.x+1, txtbox.startxy.y, chmapHeight, 10);

	/* S2.5 Load file to chmap */
	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY);
	draw_msgbox(&gv_fb_dev, 30, 50, 260, "Loading file to chmap...");
	fb_render(&gv_fb_dev);
	printf("Start to load file to chmap...\n");
	if(fpath) {
		if(FTcharmap_load_file(fpath, chmap, true) !=0 )  /* Load all content */
			printf("Fail to load file to champ!\n");
	}
	else {
		if( FTcharmap_load_file(DEFAULT_FILE_PATH, chmap, true) !=0 )
			printf("Fail to load file to champ!\n");
	}


#if 0  /////////////// TEST: compare charmap result with FBDEV=NULL and FBDEV!=NULL //////////////
	unsigned char uchar[4];
	wchar_t ucode;
	int ulen;
	EGI_FTCHAR_MAP *chmap2;
	chmap2=FTcharmap_create( CHMAP_TXTBUFF_SIZE, txtbox.startxy.x, txtbox.startxy.y,  /* txtsize,  x0, y0  */
			tlns*lndis, txtbox.endxy.x-txtbox.startxy.x+1, smargin, tmargin, /*  height, width, offx, offy */
			egi_sysfonts.regular, CHMAP_SIZE, tlns, gv_fb_dev.pos_xres-2*smargin, lndis,     /* typeface, mapsize, lines, pixpl, lndis */
			WEGI_COLOR_WHITE, WEGI_COLOR_BLACK, true, true );  /*  bkgcolor, fontcolor, charColorMap_ON, hlmarkColorMap_ON */
	if(chmap2==NULL){ printf("Fail to create char map2!\n"); exit(0); };

        /* Load file to chmap2 */
        if(fpath) {
                if(FTcharmap_load_file(fpath, chmap2, CHMAP_TXTBUFF_SIZE) !=0 )
                        printf("Fail to load file to champ2!\n");
        }
        else {
                if( FTcharmap_load_file(DEFAULT_FILE_PATH, chmap2, CHMAP_TXTBUFF_SIZE) !=0 )
                        printf("Fail to load file to champ2!\n");
        }

	/* First charmap */
	FTcharmap_writeFB2(chmap, &gv_fb_dev, NULL, NULL);
	FTcharmap_writeFB2(chmap2, NULL, NULL, NULL);

	/* Compare result */
	for(k=0; k< chmap->chcount; k++) {
		if( chmap->charPos[k] != chmap2->charPos[k] ) {
			printf("chmap->charPos[%d] != chmap2->charPos[%d]\n", k, k);
			exit(0);
		}
	}

	/* Compare next 5 pages */
	for(j=0; j<200; j++) {
	        /* Charmap next page */
        	FTcharmap_page_down(chmap);
        	FTcharmap_page_down(chmap2);

		FTcharmap_writeFB2(chmap, &gv_fb_dev, NULL, NULL);
		FTcharmap_writeFB2(chmap2, NULL, NULL, NULL);

		/* Compare result */
		for(k=0; k< chmap->chcount; k++) {
			if( chmap->charPos[k] != chmap2->charPos[k] || chmap->charX[k] != chmap2->charX[k] ) {
			      printf("chmap->charPos[%d]=%d, chmap2->charPos[%d]=%d\n", k, chmap->charPos[k], k, chmap2->charPos[k]);
			      printf("chmap->charX[%d]=%d, chmap2->charX[%d]=%d\n", k, chmap->charX[k], k, chmap2->charX[k]);
			    	if(k>0) {
					ulen=cstr_charlen_uft8(chmap->pref+chmap->charPos[k-1]);
					if(ulen>0) {
						bzero(uchar, sizeof(uchar));
					 	strncpy((char *)uchar, (char *)chmap->pref+chmap->charPos[k-1], 4);
						char_uft8_to_unicode(uchar, &ucode);
						printf("Ucode: U+%X   ", ucode);  /* U+FF1A full width colon */
						printf("UTF8: ");
						for(i=0;i<ulen;i++)
							printf("%02X", uchar[i]);
						printf("\n");
					}
					printf("chmap: charX[%d]: %d, charX[%d]: %d\n", k-1, chmap->charX[k-1], k, chmap->charX[k]);
					printf("chmap2: charX[%d]: %d, charX[%d]: %d\n", k-1, chmap2->charX[k-1], k, chmap2->charX[k]);
/* Results:
 U+FF1A: full width colon
 chmap: charX[42]: 257, charX[43]: 275   advance=18 =fw
 chmap2: charX[42]: 257, charX[43]: 257  advance=0!
*/


			    	}
			    	exit(0);
			}
		}
	}

	exit(0);
#endif /////////////////////////////////////////////////////////


	/* S3.  Insert colorBandMap for charColorMap and hlmarkColorMap
	 *  TODO: ColorMap ALSO saved:
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


#if MSG_SYNC_RENDER /* OR Semaphore sync */
	/* S4. Create render_msgid.  */
        /* Create System_V message queue */
        mqkey=ftok(".", 5);
	/* Get render_msgid for render */
        render_msgid = msgget(mqkey, IPC_CREAT|0666);
        if(render_msgid<0) {
                egi_dperr("Fail to create render_msgid!");
		exit(1);
        }
#else /* SEM_SYNC_RENDER */
	/* S4. Create/initialize an unnamed semaphore */
	if(sem_init(&render_sem, 0, 0)!=0) {
		egi_dperr("Fail to init render_rem!");
		exit(2);
	}
#endif

        /* S5. Init mutex_render */
        if(pthread_mutex_init(&mutex_render,NULL) != 0) {
                printf("Fail to initiate mutex_render.\n");
		exit(1);
        }
	/* S6. Start thread_render_surface() */
	if(pthread_create(&pthread_render, NULL, render_surface, NULL)<0) {
		printf("Fail to start render_surface()!\n");
		exit(1);
	}

#if MSG_SYNC_RENDER
	/* S7. Get mev_msgid for mouse event */
	mev_msgid= msgget(mqkey, 0);
        if(mev_msgid<0) {
                egi_dperr("Fail to get mev_msgid!");
		exit(1);
	}
	/* S8. Get kev_msgid for keyboard event */
	kev_msgid= msgget(mqkey, 0);
        if(kev_msgid<0) {
                egi_dperr("Fail to get ev_msgid!");
		exit(1);
	}
#else /* SEMAPHORE_SYMC */
#endif

        /* S9. Init. mouse position */
        mouseX=gv_fb_dev.pos_xres/2;
        mouseY=gv_fb_dev.pos_yres/2;

        /* S10. Init. FB working buffer */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY4);
	FTsymbol_writeFB("File  Help",20,5,WEGI_COLOR_WHITE, NULL, NULL);

	/* TEST: getAdvances ---- */
	int penx=140, peny=5;
	FTsymbol_writeFB("EGI笔记本",penx,peny,WEGI_COLOR_LTBLUE, &penx, &peny);
	printf("FTsymbol_writFB get advanceX =%d\n", penx-140);
	printf("FTsymbol_uft8string_getAdvances =%d\n",
		FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fw, fh, (const unsigned char *)"EGI笔记本") );


	/* S11. Draw blank paper + margins */
	fbset_color(WEGI_COLOR_WHITE);
	draw_filled_rect(&gv_fb_dev, txtbox.startxy.x, txtbox.startxy.y, txtbox.endxy.x, txtbox.endxy.y );

        fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
	fb_render(&gv_fb_dev);

	/* S12. Set termio */
        tcgetattr(0, &old_settings);
        new_settings=old_settings;
        new_settings.c_lflag &= (~ICANON);      /* disable canonical mode, no buffer */
        new_settings.c_lflag &= (~ECHO);        /* disable echo */
        new_settings.c_cc[VMIN]=1;//0;		/* 0: return immediately */
        new_settings.c_cc[VTIME]=0;
        tcsetattr(0, TCSANOW, &new_settings);

	/* S13. Render initial charmap. After FTcharmap_load_file(). */
	FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
	fb_render(&gv_fb_dev);


/* TEST: -------- */
//	soff = chmap->txtlen/3; // 4.992/100

	/* S14. Set pchoff */
	if(soff>=0) {
                printf(">>>>> Try to refit soff:  \n");
                cstr_print_byteInBits(chmap->txtbuff[soff], " ----> ");
	  	soff=soff+cstr_charfit_uft8(chmap->txtbuff+soff); /* In case soff is NOT correct */
		cstr_print_byteInBits(chmap->txtbuff[soff], "\n");
		chmap->pchoff=chmap->pchoff2=soff;
	}
	sleep(1);

	/* S15. Charmap to file end, OR to search position soff/spcnt.
	 * Note: If load file with fullmap, then it's NOT necessary!
	 */
	i=0;
	while( chmap->pref[chmap->charPos[chmap->chcount-1]] != '\0' )
	{
		/* If search position is in current page */
		egi_dpstd(DBG_GREEN"Page range offset [%ld, %ld]\n"DBG_RESET,
				(long int)(chmap->pref-chmap->txtbuff),
				(long int)(chmap->pref+chmap->charPos[chmap->chcount-1]-chmap->txtbuff) );

#if 1
		/* spcnt applys */
		if( spcnt>0 ) {
		     if(chmap->pref+chmap->charPos[chmap->chcount-1]-chmap->txtbuff > chmap->txtlen*spcnt/100)
			break;
		}
		/* soff applys */
		else if(soff>=0 ) {
		    if(chmap->pref+chmap->charPos[chmap->chcount-1]-chmap->txtbuff >= soff)
			break;
		}
#endif

		/* Charmap next page */
		FTcharmap_page_down(chmap);

//		printf("FTcharmap_writeFB...\n");
		FTcharmap_writeFB(NULL, NULL, NULL);
		//FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_BLACK, NULL,NULL);

		if( i==20 ) {  /* Every time when i counts to 20 */
		   FTcharmap_writeFB(&gv_fb_dev, NULL,NULL);

		   fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
		   draw_progress_msgbox( &gv_fb_dev, (320-260)/2, (240-120)/2, "文件加载中...",
					  //chmap->pref+chmap->charPos[chmap->chcount-1]-chmap->txtbuff, chmap->txtlen);
					  //chmap->txtdlinePos[chmap->txtdlncount +chmap->maplncount-1],
					  //chmap->txtlen*spcnt/100 );
					  chmap->pref+chmap->charPos[chmap->chcount-1]-chmap->txtbuff, soff);

		   /* Update values for vsbar ( vsbar, barH, maxLen, winLen, pastLen ) */
		   egi_VScrollBar_updateValues(vsbar, 0, chmap->txtdlncount+chmap->maplines, chmap->maplines, chmap->txtdlncount);
		   /* Vertical Scroll Bar */
		   egi_VScrollBar_writeFB(&gv_fb_dev, vsbar);

		   fb_render(&gv_fb_dev);
		   i=0;
		}
		i++;
	}
        fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
	draw_progress_msgbox( &gv_fb_dev, (320-280)/2, (240-140)/2, "文件加载完成", 100,100);
//	goto MAIN_END;

	/* TODO: Scroll to let soff to be in the first line of the charmap */
	egi_dpstd(DBG_YELLOW"Get to bookmarked soff=%jd page: spcnt=%.2f, off=%ld of %d\n"DBG_RESET, soff,
				(float)(chmap->pref-chmap->txtbuff)/chmap->txtlen, (long int)(chmap->pref-chmap->txtbuff), chmap->txtlen);

	/* Update values for vsbar ( vsbar, barH, maxLen, winLen, pastLen ) */
	//egi_VScrollBar_updateValues(vsbar, 0, chmap->txtdlncount+chmap->maplines, chmap->maplines, chmap->txtdlncount);
	egi_VScrollBar_updateValues(vsbar, 0, chmap->txtdlntotal, chmap->maplines, chmap->txtdlncount);
        /* Vertical Scroll Bar */
        egi_VScrollBar_writeFB(&gv_fb_dev, vsbar);
        fb_render(&gv_fb_dev);

/* TEST_TEST_TEST:  ---------------------- */
	/* S16. Set chmap_ready */
	chmap_ready=true;

	printf("Loop editing...\n");

	/* S17. Loop editing ...   Read from TTY standard input, without funcion keys on keyboard ...... */
	while( !sigQuit ) {
	/* ---- OPTION 1: Read more than N bytes each time, nonblock. !!! NOT good, response slowly, and may cause parse errors !!!  -----*/

	/* ---- OPTION 2: Read in char one by one, nonblock  -----*/
		ch=0;
		read(STDIN_FILENO, &ch, 1);
		if(ch>0) {
		  if(isalpha(ch))
			printf("stdin:'%c'\n",ch);
		  else
		  	printf("ch=%d\n",ch);
		}
		//continue;

/* ------ >>>  mutex_render Critical Zone  */
		pthread_mutex_lock(&mutex_render);

		/* 1. Parse TTY input escape sequences */
		if(ch==27) {
			/* 1.0 Setterm Nonblocking type. If ESCAPE,we need a ch=0 to confirm,
			 *     to restore it at 1.3  HK2022-06-01 */
	        	new_settings.c_cc[VMIN]=0;
        		tcsetattr(0, TCSANOW, &new_settings);

			ch=0;
			read(STDIN_FILENO,&ch, 1);
			printf("ch=%d\n",ch);
			/* 1.1  */
			if(ch==91) {
				ch=0;
				read(STDIN_FILENO,&ch, 1);
				printf("ch=%d\n",ch);
				switch ( ch ) {
					case 49: /* HOME */
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
							/* To avoid incomplete charmap displayed by thread refresh_surface(),
							 * as fb_render() is out of render_mutex_lock zone in refresh_surface(). HK2022-06-5
							 */
							//FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
				//			FTcharmap_writeFB(NULL, NULL, NULL);
 	egi_dpstd(DBG_GREEN"Page range offset [%ld, %ld]\n"DBG_RESET, (long int)(chmap->pref-chmap->txtbuff),
								    (long int)(chmap->pref+chmap->charPos[chmap->chcount-1]-chmap->txtbuff) );
						}
                                                break;
					case 54: /* PAGE DOWN */
						read(STDIN_FILENO,&ch, 1);
						if( ch == 126 ) {
							FTcharmap_page_down(chmap);
							/* To avoid incomplete charmap displayed by thread refresh_surface(),
							 * as fb_render() is out of render_mutex_lock zone in refresh_surface().
							 */
							//FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
				//			FTcharmap_writeFB(NULL, NULL, NULL);
 	egi_dpstd(DBG_GREEN"Page range offset [%ld, %ld]\n"DBG_RESET, (long int)(chmap->pref-chmap->txtbuff),
							(long int)(chmap->pref+chmap->charPos[chmap->chcount-1]-chmap->txtbuff) );
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
							/* Try item numbers, to just fit for IME display area. */
							for( i=GroupMaxItems; i>0; i--) { //for( i=7; i>0; i--)
								/* Get GroupItems, for pick starting index. in case itmes < GroupStartIndex */
								if(GroupStartIndex<i) //if(GroupStartIndex-i < 0)
									continue;
								//printf("UP>>>: GroupStartIndex=%d\n",GroupStartIndex);
								/* Clear strHanlist */
								bzero(strHanlist, sizeof(strHanlist));
								/* Fill strHanlist with GroupItems of unihans(groups), start from GroupStartIndex-i. */
								for(j=0; j<i; j++) {
					 			     snprintf(strItem,sizeof(strItem),"%d.%s ",j+1, unihans[GroupStartIndex-i+j]);
								     strcat(strHanlist, strItem);
								}
								/* Check if space is just OK. */
/* Note: Here uses '<=', check at 'Display unihan(group)s for selecting' where uses '>'!   HK2022-06-02  */
								if(FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fwUI, fhUI,
														(UFT8_PCHAR)strHanlist) <= 320-15 )
									break;
							}
							/* Reset new GroupStartIndex and GroupItems */
							GroupStartIndex -= i;
							printf("UP>>>: GroupStartIndex=%d, i=%d, GroupItems=%d\n", GroupStartIndex, i, GroupItems);
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
                                                        if( GroupStartIndex+GroupItems < unicount ) {
                                                                GroupStartIndex += GroupItems;   /* Reset new GroupStartIndex */
								printf("DOWN>>>: GroupStartIndex=%d, GroupItems=%d\n", GroupStartIndex,GroupItems);
							}
							/* Note: GroupItems is updated in 'Display unihan(group)s for selecting'
								 within pthread_render!
								 GroupStartIndex May have updated in above case 65, WHILE GroupItems
								 MAY NOT have been updated yet!
 							 */
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
                        /* 1.2 ESCAP KEY: ch1==27 AND ch2==0 */
                        else if(ch==0) {
				printf(" --- ESCAPE ---\n");
                                if(enable_pinyin) {
                                        /* Clear all pinyin buffer */
                                        bzero(strPinyin,sizeof(strPinyin));
                                        bzero(display_pinyin,sizeof(display_pinyin));
                                        unicount=0; HanGroupIndex=0;

                                        /* Leave 'U+' for Unicode lead */
                                        if(enable_unicode_input)
                                                strcpy(display_pinyin, "U+");
                                }
                        }

			/* ELSE: ch1==27 and (ch2!=91 AND ch2!=0)  */

			/* 1.3 Setterm blocking type... HK2022-06-01 */
	        	new_settings.c_cc[VMIN]=1;
        		tcsetattr(0, TCSANOW, &new_settings);

			/* 1.4 continue while() read stdin HK2022-06-02 */

			//.....GO on to sndmsg for refreshing surface.... ch=0

/* ------ <<<  mutex_render Critical Zone  */
//			pthread_mutex_unlock(&mutex_render);
//			continue;
		}
		/* ELSE: ch1 != 27, go on...  */


		/* 2. TTY Controls  */
		switch( ch )
		{
		    	case CTRL_P:
                                //enable_pinyin=!enable_pinyin;  /* Toggle input method to PINYIN */
                                if(!enable_pinyin) {
                                        enable_pinyin=true;
                                        /* Disable enable_unicode */
                                        enable_unicode_input=false;

                                        /* Shrink maplines, maplinePos[] mem space Ok, increase maplines NOT ok! */
                                        if(chmap->maplines>2) {
                                                chmap->maplines -= (60/lndis); //2;
                                                chmap->height=chmap->maplines*lndis+2*tmargin; /* Reset chmap.height, to limit chmap.fb_render() */
                                        }

                                        /* Clear input pinyin buffer */
                                        bzero(strPinyin,sizeof(strPinyin));
                                        bzero(display_pinyin,sizeof(display_pinyin));
                                }
                                else {
                                        enable_pinyin=false;
                                        /* Default disable unicode input */
                                        enable_unicode_input=false;

                                        /* Recover maplines */
                                        chmap->maplines += (60/lndis); //2;
                                        chmap->height=chmap->maplines*lndis+2*tmargin;  /* restor chmap.height */
                                        unicount=0; HanGroupIndex=0;    /* clear previous unihans */
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
#if 0
			case CTRL_S:
				FTcharmap_copy_to_syspad(chmap);
				ch=0;
				break;
#endif
			case CTRL_V:
				FTcharmap_copy_from_syspad(chmap);
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

		/* 3. Edit text with input ch */
		/* 3.1 BACKSPACE */
		if( ch==0x7F ) { /* backspace */
			/* 3.1.1 Backward for unicode input */
			if(enable_unicode_input && strlen(display_pinyin)>2 ) { /* 'U+' as unicode lead */
				display_pinyin[strlen(display_pinyin)-1]='\0';
                                //printf("display_pinyin: %s\n", display_pinyin);

				/* Convert to UTF-8, single Unihan. */
				unihans[0][0]=0;
				wchar_t wcode=strtol(display_pinyin+2, NULL, 16);/* +2 for "U+" */
				if(char_unicode_to_uft8(&wcode, unihans[0])>0)
					unicount=1;
				else
					unicount=0;
			}
			/* 3.1.2 Backward for pinyin input, When enable_uncidoe_input, strPinyin is empty! */
			else if(enable_pinyin && strlen(strPinyin)>0 ) {
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
			else  { /* strPinyin is empty  OR unicode_input ONLY 'U+' */
				FTcharmap_go_backspace( chmap );
			}
			ch=0;
		}
		/* --- EDIT:  3.2 Insert chars, !!! printable chars + '\n' + TAB  !!! */
		else if( ch>31 || ch==9 || ch==10 ) {  //pos>=0 ) {   /* If pos<0, then ... */
			/* 3.2.1 PINYING Input, put to strPinyin[]  */
	   		if( enable_pinyin ) {  //&& ( ch==32 || isdigit(ch) || isalpha(ch) ) ) {
			  /* 3.2.1.0 Input '+' to disable/enable_unicode_input */
			  if( strPinyin[0]=='\0' && ch=='+' ) {
				if(enable_unicode_input) {
					enable_unicode_input=false;

					/* Clear strPinyin and unihans[]  */
                                        bzero(strPinyin,sizeof(strPinyin));
                                        unicount=0;
                                        //bzero(&unihans[0][0], sizeof(unihans));
                                        bzero(display_pinyin,sizeof(display_pinyin));
                                        GroupStartIndex=0; GroupItems=0; /* reset group items */

				} else {
					enable_unicode_input=true;
					strncpy(display_pinyin,"U+",2);  /* Input Unicode lead 'U+' */
				}

				ch=0;
			  }
			  /* 3.2.1.1 Unicode direct input, for a signle Unihan/Emoji */
			  if( enable_unicode_input ) {
				/* 3.2.1.1.1 Input hexadecimal digits */
				if( isxdigit(ch) ) {
					/* Convert to upper case */
					if(islower(ch)) ch -=32;
					/* TODO: strPinyin space check */
					display_pinyin[strlen(display_pinyin)]=ch;
                                        //printf("display_pinyin: %s\n", display_pinyin);

					/* Convert to UTF-8, single Unihan. */
					unihans[0][0]=0;
					wchar_t wcode=strtol(display_pinyin+2, NULL, 16);/* +2 for "U+" */
					if(char_unicode_to_uft8(&wcode, unihans[0])>0)
						unicount=1;
					else
						unicount=0;
				}
				/* 3.2.1.1.2 SPACE to entry single Unihan/Emoji */
				if( ch==32 ) {
                                        /* If strPinyin is empty, insert SPACE into txt */
                                        if( display_pinyin[0]=='\0' ) {
                                                FTcharmap_insert_char( chmap, &ch);
                                        /* Else, insert the first unihan in the displaying panel  */
                                        } else {
						FTcharmap_insert_char(chmap, &unihans[0][0]);

						/* Clear strPinyin and unihans[]  */
                                                bzero(strPinyin,sizeof(strPinyin));
                                                unicount=0;
                                                //bzero(&unihans[0][0], sizeof(unihans));
                                                bzero(display_pinyin,sizeof(display_pinyin));
                                                GroupStartIndex=0; GroupItems=0; /* reset group items */

						/* Input Unicode lead 'U+' */
						strncpy(display_pinyin,"U+",2);
					}
				}

				ch=0;
			  }
			  /* 3.2.1.2  PINYING Input, put to strPinyin[] */
			  else {
				/* 3.2.1.2.1 SPACE: insert SAPCE */
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
						bzero(&unihans[0][0], sizeof(unihans)); //for Unicode input
						bzero(display_pinyin,sizeof(display_pinyin));
						GroupStartIndex=0; GroupItems=0; /* reset group items */
					}
				}
				/* 3.2.1.2.2 DIGIT: Select unihan OR insert digit */
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
                                                bzero(&unihans[0][0], sizeof(unihans)); //for Unicode input
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
				/* 3.2.1.2.3 ALPHABET: Input as PINYIN */
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
				/* 3.2.1.2.4 ELSE: convert to SBC case(full width) and insert to txt */
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

			  } /* END 3.2.1.2 */

			}

			/* 3.2.2 Keyboard(ch) Direct Input */
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

		}  /* END 3. insert ch */

		/* 4. Update charmap, but not wirte image to FBDEV.
	         * Note:
		 * 1. Update charmap should be in the same thread! to ensure each input operaton will be done.
		 *    If update charmap only in render_surface(), when chmap->request!=0, the input operation
		 *    may be dropped then.
		 *    Example: 2 BACKSPACEs inupt may result only 1 successful FTcharmap_go_backspace().
		*/
		FTcharmap_writeFB(NULL, NULL, NULL);

/* ------ <<<  mutex_render Critical Zone  */
		pthread_mutex_unlock(&mutex_render);

#if MSG_SYNC_RENDER  ///////////////////* 5.0 Msgsnd to request for refreshing surface  *//////////////////
//       refresh_surface();

	/* TEST msg: 2022-05-25: ------------------- */
        MSG_BUFF msgbuff;
        int merr;
        msgbuff.mtype=MSG_REFRESH_SURFACE;
        merr=msgsnd(render_msgid, (void *)&msgbuff, sizeof(msgbuff.mtext), 0);
        if(merr<0) {
        	egi_dperr("msgsnd fails.");
                //if(errno==EIDRM) egi_dpstd("msgid is invalid! Or the queue is removed.\n");
                //else if(errno==EAGAIN) egi_dpstd("Insufficient space in the queue!\n");
        }
#elif 1 ///////////// SEMAPHORE_SYNC_RENDER //////////////
	if(sem_post(&render_sem)!=0)
		egi_dperr("sem_post fails");

#else	/* ------- EDIT:  Need to refresh EGI_FTCHAR_MAP after editing !!! ------  */

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
			fbset_color2(&gv_fb_dev, WEGI_COLOR_WHITE);
			draw_line(&gv_fb_dev, 15, txtbox.endxy.y-60+3+28, 320-15,txtbox.endxy.y-60+3+28);

/* TODO: mutex_lock */
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
					//if(FTsymbol_uft8strings_pixlen(egi_sysfonts.regular, fwUI, fhUI, (UFT8_PCHAR)strHanlist) > 320-15 )
					if(FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fwUI, fhUI, (UFT8_PCHAR)strHanlist) > 320-15 )
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
			start_pch=false; /* HK2022-06-03 */

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

	/* TEST:-----  Wait for a LeftKeyUp: to avoid above click to change/affect txtbox selection range!
        	       If you press down and hold, it surely will change/affect txtbox selection range!
	               mouse_callback() will reset start_pch=false at LeftKeyDownHold.
	 */
                        while(start_pch){usleep(100000);}

			/* Exit Menu handler */
			//start_pch=false;
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

			/* Wait for a LeftKeyUp, TRY to prevent above menu_click from changing txtbox selection range. */
			while(start_pch){usleep(100000);}

			/* Exit Menu handler */
			//start_pch=false;
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
#endif /////////////////////  END : Refresh Surface  ////////////////////////


	} /* End while() */


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
//MAIN_END:

	/* Reset termio */
printf("tcsetattr old...\n");
        tcsetattr(0, TCSANOW, &old_settings);

printf("Free charmap...\n");
	/* My release */
	FTcharmap_free(&chmap);

//goto MAIN_START;

printf("Free uniset and group_set...\n");
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
	struct timeval tms,tme;

	gettimeofday(&tms, NULL);
       	ret=FTcharmap_uft8strings_writeFB( fbdev, chmap,   /* FBdev, charmap*/
                                           //fw, fh,         /* fw,fh */
	                                   -1, 255,      	   	   /* transcolor,opaque */
                                           NULL, NULL, penx, peny);        /* int *cnt, int *lnleft, int* penx, int* peny */
        gettimeofday(&tme, NULL);
	printf("FTcharmap cost time=%luus \n",tm_diffus(tms,tme));

        /* Double check! */
        if( chmap->chcount > chmap->mapsize ) {
                printf("WARNING:  chmap.chcount > chmap.mapsize=%d, some displayed chars has NO charX/Y data! \n",
													chmap->mapsize);
        }
        if( chmap->txtdlncount > chmap->txtdlines ) {
                printf("WARNING:  chmap.txtdlncount=%d > chmap.txtdlines=%d, some displayed lines has NO position info. in charmap! \n",
                                                                                	chmap->txtdlncount, chmap->txtdlines);
        }

	/* Vertical Scroll Bar */
	egi_VScrollBar_writeFB(fbdev, vsbar);

	return ret>0?ret:0;
}

/*-------------------------------------
(Applys if pthread_render NOT used.)

        FTcharmap WriteFB TXT
@txt:   	Input text
@color: 	Color for text
@penx, peny:    pointer to pen X.Y.
--------------------------------------*/
static int FTcharmap_writeFB2(EGI_FTCHAR_MAP *chmap, FBDEV *fbdev, int *penx, int *peny)
{
	int ret;

       	ret=FTcharmap_uft8strings_writeFB( fbdev, chmap,   /* FBdev, charmap*/
                                           //fw, fh,         /* fw,fh */
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


/*----------------------------------------------
FTsymbol WriteFB for UI (NOT for Charmap txtbox)

With fixed fwUI/fhUI/fontface.

@txt:   	Input text
@px,py: 	LCD X/Y for start point.
@color: 	Color for text
@penx, peny:    pointer to pen X.Y.
-----------------------------------------------*/
static void FTsymbol_writeFB(const char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny)
{
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        fwUI, fhUI,(const unsigned char *)txt,    	/* fw,fh, pstr */
                                        //320, tlns, fgap,      /* pixpl, lines, fgap */
					320, 1, 0,
                                        px, py,               /* x0,y0, */
                                        color, -1, 255,       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, penx, peny);      /*  int *cnt, int *lnleft, int* penx, int* peny */
}


/*-----------------------------------------------------------
		Callback for mouse input
1. Just update mouseXYZ
2. Check and set ACTIVE token for menus.
3. If click in TXTBOX, set typing cursor or marks.
4. If click on VSCROLLBAR,  then go to indicated position.
5. The function will be pending until there is a mouse event.
6. Operation to charmap with mutex_lock (+ request).
------------------------------------------------------------*/
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus)
{
	//EGI_POINT  pxy={0,0};
//	printf("mouse_callback\n");

        /* 0. Get mouseLeftKeyDown/Up */
	mouseLeftKeyDown=mostatus->LeftKeyDown; // || mostatus->LeftKeyDownHold;
	mouseLeftKeyUp=mostatus->LeftKeyUp;

        /* 1. Check mouse Left Key status */
	/* 1.1 LeftKeyDown */
	if( mostatus->LeftKeyDown ) {

		/* 1.1.0 Click on VSCROLLBAR */
		//if(vsbar) { //vsbar==NULL is OK for all functions
		egi_VScrollBar_clickUpdateValues(vsbar, mostatus->mouseX, mostatus->mouseY);
		if(egi_VScrollBar_pxyInVSBar(vsbar, mostatus->mouseX, mostatus->mouseY))
			FTcharmap_goto_dline(chmap, vsbar->maxLen*vsbar->GuideTopPos/vsbar->ScrollBarH);
		//}

		/* 1.1.1 Set start_pch */
		if( mouseY > txtbox.startxy.y )
			start_pch=true;

/* Note: start_pch as a sign of pressing for compRCMenu selection, see while( !start_pch ) at refresh_surface() */

                /* 1.1.2 Activate window top bar menu */
	        if( mouseY < txtbox.startxy.y && mouseX <112) //90 )
                   ActiveComp=CompWTBMenu;
		else if( ActiveComp!=CompRCMenu && ActiveComp!=CompWTBMenu ) {
		   if(vsbar && egi_VScrollBar_pxyInVSBar(vsbar, mostatus->mouseX, mostatus->mouseY))
			ActiveComp=CompVSBar;
		   else if(pxy_inbox2(mostatus->mouseX, mostatus->mouseY, &txtbox))
			ActiveComp=CompTXTBox;
		   else
			ActiveComp=CompNone;
		}

                /* 1.1.3 Reset pch2 = pch */
                if( ActiveComp==CompTXTBox )
                       FTcharmap_reset_charPos2(chmap);

	}
	/* 1.2 LeftKeyDownHold */
	else if( mostatus->LeftKeyDownHold ) {
		start_pch=false; /* disable pch mark */

		/* on VSCROLLBAR */
		if(ActiveComp==CompVSBar) {
		   #if 0 /* Mouse drag at head/tail of the GuideBlock */
		   egi_VScrollBar_clickUpdateValues(vsbar, mostatus->mouseX, mostatus->mouseY);
		   if(egi_VScrollBar_pxyInVSBar(vsbar, mostatus->mouseX, mostatus->mouseY))
			FTcharmap_goto_dline(chmap, vsbar->maxLen*vsbar->GuideTopPos/vsbar->ScrollBarH);
		   #else
		   //printf("MouseDY=%d\n", mostatus->mouseDY);
		   //if(egi_VScrollBar_pxyOnGuide(vsbar, mostatus->mouseX, mostatus->mouseY)) {
		   //if(egi_VScrollBar_pxyInVSBar(vsbar, mostatus->mouseX, mostatus->mouseY)) 
		   {
			egi_VScrollBar_dyUpdateValues(vsbar, mostatus->mouseDY);
			FTcharmap_goto_dline(chmap, vsbar->maxLen*vsbar->GuideTopPos/vsbar->ScrollBarH);
		   }
		   #endif
		}
	}
	/* 1.3 LeftKeyUp */
	else if( mostatus->LeftKeyUp ) {
                start_pch=false;
	}

	/* 2. RightKeyDown */
        //if(mouse_data[0]&0x2) {
	if(mostatus->RightKeyDown ) {
                printf("Right key down!\n");
		//pxy.x=mouseX; pxy.y=mouseY;
		/* 2.1 Activate CompRCMenu */
		//if( point_inbox(&pxy, &txtbox ) ) {
		if( pxy_inbox2(mouseX,mouseY, &txtbox) ) {
			ActiveComp=CompRCMenu;
			/* Menu Origin */
			menuX0=mouseX; menuY0=mouseY;
		}
	}

	/* 3. MidkeyDown */
        //if(mouse_data[0]&0x4)
	if(mostatus->MidKeyDown)
                printf("Mid key down!\n");

        /* 4. Get mouseX/Y */
	mouseX = mostatus->mouseX;
	mouseY = mostatus->mouseY;

	/* 5. mouseDZ to Scroll down/up */
	if( mostatus->mouseDZ > 0 ) {
		 FTcharmap_scroll_oneline_down(chmap);
	}
	else if ( mostatus->mouseDZ < 0 ) {
		 FTcharmap_scroll_oneline_up(chmap);
	}

	/* 6. Get mouseMidX/Y */
      	mouseMidY=mostatus->mouseY+fh/2;
       	mouseMidX=mostatus->mouseX+fw/2;
//    	printf("mouseMidX,Y=%d,%d \n",mouseMidX, mouseMidY);

	/* 7. To get current cursor/pchoff position  */
	if( (mostatus->LeftKeyDownHold || mostatus->LeftKeyDown) && ActiveComp==CompTXTBox ) {
		/* 7.1 Set cursor position/Start of selection, LeftKeyDown */
		if(start_pch) {
		        /* 7.1.1 Continue checking request to locate_charPos! */
           		while( FTcharmap_locate_charPos( chmap, mouseMidX, mouseMidY )!=0 ){ tm_delayms(5);};
           		/* 7.1.2 Set random mark color */
           		FTcharmap_set_markcolor( chmap, egi_color_random(color_medium), 80);
//			printf("---pchoff: %d\n", chmap->pchoff);

		}
		/* 7.2 Set end of selection, LeftKeyDownHold. !start_pch implys LeftKeDownHold */
		else {
		   	/* Continue checking request! */
		   	while( FTcharmap_locate_charPos2( chmap, mouseMidX, mouseMidY )!=0 ){ tm_delayms(5); } ;
		}
	}

	/* 8. smgsnd MSG_REFRESH_SURFACE */
#if MSG_SYNC_RENDER /* TEST msg: 2022-05-23: ------------------- */
	MSG_BUFF msgbuff;
	int merr;
	//if( mostatus->mouseDX || mostatus->mouseDY ) {
	if( (!mostatus->KeysIdle || mostatus->mouseDX || mostatus->mouseDY) && mev_msgid>0 ) {
		msgbuff.mtype=MSG_REFRESH_SURFACE;
	        merr=msgsnd(render_msgid, (void *)&msgbuff, sizeof(msgbuff.mtext), 0);
	        if(merr<0) {
            		egi_dperr("msgsnd fails.");
                	//if(errno==EIDRM) egi_dpstd("msgid is invalid! Or the queue is removed.\n");
                	//else if(errno==EAGAIN) egi_dpstd("Insufficient space in the queue!\n");
		}
		//if(chmap)
		//    refresh_surface();
	}
#else  /* SEM_SYNC_RENDER */
	if( (!mostatus->KeysIdle) || mostatus->mouseDX || mostatus->mouseDY ) {
		if(sem_post(&render_sem)!=0)
			egi_dperr("sem_post fails");
	}
#endif

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
	char *mwords[RCMENU_ITEMS]={"Word", "Copy", "Paste", "Cut", "Color"};

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
			fbset_color2(&gv_fb_dev, WEGI_COLOR_ORANGE);
		else
			fbset_color2(&gv_fb_dev,WEGI_COLOR_GRAY4);
		draw_filled_rect(&gv_fb_dev, x0, y0+i*smh, x0+mw, y0+(i+1)*smh);

		/* Menu Words */
#if 1
        	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        18, 20,(const unsigned char *)mwords[i],    /* fw,fh, pstr */
                                        100, 1, 0,      			    /* pixpl, lines, fgap */
                                        x0+10, y0+2+i*smh,                      /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, mp==i?255:100,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
#else //HK2022-05-22
		FTsymbol_writeFB(mwords[i], x0+10, y0+2+i*smh, WEGI_COLOR_WHITE, NULL, NULL);
#endif
	}
	/* Draw rim */
	fbset_color2(&gv_fb_dev, WEGI_COLOR_BLACK);
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

	const char *MFileItems[4]={"Open", "Save", "Exit", "Abort"};
	const char *MHelpItems[5]={"Help", "Renew", "About","Mark", "SavePos" };

        const int MFileNum=4; /* Number of items under tag File */
        const int MHelpNum=5; /* Number of items under tag Help */

	int mw=100;//70;   /* menu width */
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
			fbset_color2(&gv_fb_dev,WEGI_COLOR_ORANGE);
		else
			fbset_color2(&gv_fb_dev,WEGI_COLOR_GRAY4);
		draw_filled_rect(&gv_fb_dev, x0, y0+i*smh, x0+mw, y0+(i+1)*smh);

		/* Menu Words */
#if 0
        	FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        18, 20,(const unsigned char *)MFileItems[i],    /* fw,fh, pstr */
                                        100, 1, 0,      			    /* pixpl, lines, fgap */
                                        x0+10, y0+2+i*smh,                      /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
#else //HK2022-05-22
		FTsymbol_writeFB(MFileItems[i], x0+10, y0+2+i*smh, WEGI_COLOR_WHITE, NULL, NULL);
#endif
	}
	/* Draw rim */
	fbset_color2(&gv_fb_dev,WEGI_COLOR_BLACK);
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

//	printf(" --- WBTMenu_Command_ID:%d ---\n", WBTMenu_Command_ID);

	/* Draw itmes of tag 'Help' */
	for(i=0; i<MHelpNum; i++)
	{
		/* Menu pad */
		if(mp==i)
			fbset_color2(&gv_fb_dev,WEGI_COLOR_ORANGE);
		else
			fbset_color2(&gv_fb_dev,WEGI_COLOR_GRAY4);
		draw_filled_rect(&gv_fb_dev, x0+45, y0+i*smh, x0+45+mw, y0+(i+1)*smh);

		/* Menu Words */
#if 0
        	FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        18, 20,(const unsigned char *)MHelpItems[i],    /* fw,fh, pstr */
                                        100, 1, 0,      			    /* pixpl, lines, fgap */
                                        x0+45+10, y0+2+i*smh,                      /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
#else //HK2022-05-22
		FTsymbol_writeFB(MHelpItems[i], x0+45+10, y0+2+i*smh, WEGI_COLOR_WHITE, NULL, NULL);
#endif
	}
	/* Draw rim */
	fbset_color2(&gv_fb_dev,WEGI_COLOR_BLACK);
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
WriteFB a msg box.

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
	char strval[65];
	char strbuff[1024];
	EGI_16BIT_COLOR color;
	wchar_t unicode;
	char cha;

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
	                while(1) {
				draw_msgbox( &gv_fb_dev, 30, 50, 260, "放弃后修改的数据都将不会被保存！\n　　[N]取消　[Y]确认！" );
                                draw_mcursor();
				fb_render(&gv_fb_dev);

				/* HK2022-05-28 XXXTODO:  NOW two threads race for the stdin, if user
				 * input 'N' to cancel,  it may add char'N' to txtbox. ALSO the first 'Y'
				 * may be caught by main thread.
				 * ----- OK, WBTMenu_execute() be locked by render_mutex in render_surface()
				*/
				//Set as nonblocking
				new_settings.c_cc[VMIN]=0;
				tcsetattr(0, TCSANOW, &new_settings);

				cha=0;
				read(STDIN_FILENO, &cha, 1);
				printf(" abort confirm cha='%c'\n", cha);
				if( cha=='y' || cha== 'Y' ) {
					//exit(10);
					sigQuit=true;
					break;
				}
				else if( cha=='n' || cha== 'N' ) {
					// Reset blocking
					new_settings.c_cc[VMIN]=1;
					tcsetattr(0, TCSANOW, &new_settings);
					break;
				}
			}
			break;
		case WBTMENU_COMMAND_HELP:
			//printf("txtdlinePos/txtlen: %d/%d\n", chmap->txtdlinePos[chmap->txtdlncount], chmap->txtlen);
			//if(soff>=0)
			//sprintf(strbuff, "当前页面位置: %.3f%%\nPoffset: %d/%d\",
			//	    	    	100*(float)(chmap->pref-chmap->txtbuff)/chmap->txtlen,
			//		    	chmap->pref-chmap->txtbuff, chmap->txtlen);

                        unicode=0; /* Init as invalid Unicode */
                        char_uft8_to_unicode(chmap->txtbuff+chmap->pchoff, &unicode);
              sprintf(strbuff, "当前页面位置: %.3f%%\n  Poffset: %ld/%d\n当前光标位置off: %d\n  U+%X\nRlines: %d Dlines: %d\nUchars: %d",
                                                100*(float)(chmap->pref-chmap->txtbuff)/chmap->txtlen,
                                                (long int)(chmap->pref-chmap->txtbuff), chmap->txtlen,
                                                chmap->pchoff<0 ? -1 : chmap->pchoff, unicode, /* 当前光标位置off: %d\  U+%X  */
                                                cstr_count_lines((char *)chmap->txtbuff), /* Total Rlines */
						chmap->txtdlntotal,	/* Total Dlines */
						cstr_strcount_uft8(chmap->txtbuff) ); /* Total uchars */
			draw_msgbox( &gv_fb_dev, 30, 50, 260, strbuff);
			fb_render(&gv_fb_dev);
			sleep(3);
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

		case WBTMENU_COMMAND_SAVEPOS: /* Save current offset for bookmark */
			/* Save current editting cursor position */
			if(chmap->pch>=0)
				sprintf(strval,"%d", chmap->pchoff);
			/* Save start position of current page */
			else
			   	sprintf(strval,"%ld", (long int)(chmap->pref-chmap->txtbuff));
			if( egi_update_config_value("BOOKMARK", "offset", strval)==0 ) {
				sprintf(strbuff, "当前书签位置已经保存: Pos offset=%s\n(请注意先保存文件)", strval);
				draw_msgbox( &gv_fb_dev, 30, 50, 260, strbuff);
				fb_render(&gv_fb_dev);
				sleep(2);
			}
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
	/* 1. Save group_set to txt first, if it fails/corrupts, we still have data file as backup, VICEVERSA */
	draw_msgbox( &gv_fb_dev, 50, 50, 240, "保存group_set.txt..." );
	fb_render(&gv_fb_dev);
	if(UniHanGroup_saveto_CizuTxt(group_set, UNIHANGROUPS_EXPORT_TXT_PATH)==0)
		draw_msgbox( &gv_fb_dev, 50, 50, 240, "group_txt成功保存！" );
	else
		draw_msgbox( &gv_fb_dev, 50, 50, 240, "group_txt保存失败！" );
	fb_render(&gv_fb_dev); tm_delayms(500);

	/* 2. Save group_set to data file */
	draw_msgbox( &gv_fb_dev, 50, 50, 240, "保存group_set..." );
	fb_render(&gv_fb_dev);
	if(UniHanGroup_save_set(group_set, UNIHANGROUPS_DATA_PATH)==0)
		draw_msgbox( &gv_fb_dev, 50, 50, 240, "group_set成功保存！" );
	else
		draw_msgbox( &gv_fb_dev, 50, 50, 240, "group_set保存失败！" );
	fb_render(&gv_fb_dev); tm_delayms(500);
#endif

	/* 3. Save txtbuff to file */
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
	  	/* Load UniHanGroup Set for PINYIN Input, it's assumed already merged with nch==1 UniHans! */
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
	int i;
	int row=6;
	int col=10;
	int side=20; /* color square side, in pixels */
	//int gap=3;  /* Gap between color squares, also as pad side margin */
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



/*-----------------------------------------
Refresh surface items, including:
1. Editor frame.
2. Charmap for editor txtbox content.
3. PINYIN IME panel list.
4. File_Menu and RightClick_Menu.

------------------------------------------*/
void refresh_surface(void)
{
	int i;

		/* EDIT:  Need to refresh EGI_FTCHAR_MAP after editing !!! .... */

		/* Image DivisionS: 1. File top bar  2. Charmap  3. PINYIN IME  4. Sub Menus */

/* ------ >>>  mutex_render Critical Zone  */
	pthread_mutex_lock(&mutex_render);
	/* Note: Page up/down also calls FTcharmap_writeFB(&gv_fb_dev, NULL, NULL); */

	/* --- 1. File top bar */
//printf("Draw editor top....\n");
	//draw_filled_rect2(&gv_fb_dev,WEGI_COLOR_GRAY4, 0, 0, gv_fb_dev.pos_xres-1, gv_fb_dev.pos_yres-1); //txtbox.endxy.x, txtbox.startxy.y-1);
	draw_filled_rect2(&gv_fb_dev,WEGI_COLOR_GRAY4, 0, 0, gv_fb_dev.pos_xres-1, txtbox.startxy.y-1);
	FTsymbol_writeFB("File  Help",20,5,WEGI_COLOR_WHITE, NULL, NULL);
	if(chmap->readOnly)
		FTsymbol_writeFB("EGI笔记本(Read)",140,5,WEGI_COLOR_LTBLUE, NULL, NULL);
	else
		FTsymbol_writeFB("EGI笔记本(Edit)",140,5,WEGI_COLOR_LTBLUE, NULL, NULL);

	/* --- 2. Charmap Display txt */
//printf("FTcharmap_writeFB....\n");
        //fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
	FTcharmap_writeFB(&gv_fb_dev, NULL, NULL);
//printf("FTcharmap_writeFB finish\n");

	/* --- 3. PINYING IME */
	if(enable_pinyin) {
		/* Back pad */
	        //draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYB, 0, txtbox.endxy.y-60, txtbox.endxy.x, txtbox.endxy.y );
		/* Since chmap->height has been adjusted. */
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
//printf("Display PINYIN....\n");
		FTsymbol_writeFB(display_pinyin, 15, txtbox.endxy.y-60+3, WEGI_COLOR_FIREBRICK, NULL, NULL);
		/* Display unihan(group)s for selecting */
		if( unicount>0 ) {
			bzero(strHanlist, sizeof(strHanlist));
			for( i=GroupStartIndex; i<unicount; i++ ) {
				snprintf(strItem,sizeof(strItem),"%d.%s ",i-GroupStartIndex+1,unihans[i]);
				strcat(strHanlist, strItem);
				/* Check if out of range */
				//if(FTsymbol_uft8strings_pixlen(egi_sysfonts.regular, fwUI, fhUI, (UFT8_PCHAR)strHanlist) > 320-15 )
				if(FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fwUI, fhUI, (UFT8_PCHAR)strHanlist) > 320-15 )
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

//printf("Display PINYIN finishes, GroupStartIndex=%d, GroupItems=%d \n", GroupStartIndex, GroupItems);

/* ------ <<<  mutex_render Critical Zone  */
//	pthread_mutex_unlock(&mutex_render);


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

/* TEST:-----  Wait for a LeftKeyUp: to avoid above click to change/affect txtbox selection range!
               If you press down and hold, it surely will change/affect txtbox selection range!
	       mouse_callback() will reset start_pch=false at LeftKeyDownHold.
	       HK2022-06-04
 */
			while(start_pch){usleep(100000);} // try adjust time

			/* Exit Menu handler */
			//start_pch=false;
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

			/* Wait for a LeftKeyUp, TRY to prevent above menu_click from changing txtbox selection range. */
			while(start_pch){usleep(100000);} //try adjust

			/* Exit Menu handler */
			//start_pch=false;
			ActiveComp=CompTXTBox;
			break;
		default:
			break;
	} /* END Switch */

	/* CompWTBMenu ABORT confirmation needs tcsetattr for stdin read, must be mutex_locked! */
/* ------ <<<  mutex_render Critical Zone  */
	pthread_mutex_unlock(&mutex_render);


	/* 5. Draw cursor */
//printf("Draw mcursor......\n");
	draw_mcursor();

	/* 6. Render and bring image to screen */
	fb_render(&gv_fb_dev);
}

/*--------------------------------------
This is a detached thread function.

To render/refresh the surface when
MSG_REFRESH_SURFACE is received.
-------------------------------------*/
void *render_surface(void* argv)
{
	int mlen;
        struct timeval tms,tme;

#if MSG_SYNC_RENDER
	MSG_BUFF msgbuff;
#else /* SEM_SYNC_RENDER */
	struct timespec tmout;
	int sret;
#endif

        /* 1. Detach thread */
        pthread_detach(pthread_self());

	/* 2. Wait for MSG_REFRESH_SURFACE */
	while(!sigQuit) {

 #if MSG_SYNC_RENDER
		/* 2.1 Msgrcv() */
        	//mlen=msgrcv(render_msgid, &msgbuff, sizeof(msgbuff.mtext), MSG_REFRESH_SURFACE, MSG_NOERROR);
		/* MSG_NOERROR: To truncate the message text if longer than msgsz bytes. */
        	mlen=msgrcv(render_msgid, &msgbuff, sizeof(msgbuff.mtext), MSG_REFRESH_SURFACE, MSG_NOERROR|IPC_NOWAIT);
		if(mlen<0) {
			if(errno==EIDRM) {
				egi_dpstd("The message queue was removed!\n");
				break;
			}
			/* At lease 2 times refresh each second. */
			else if(errno==ENOMSG) {
				#ifdef LETS_NOTES
				//usleep(10000);
				#else
				usleep(500000);
				#endif
				egi_dpstd("Refresh at regular intervals.\n");
			}
		}
          #if 0	/* 2.2 Drop all remaining MSGs in the queue! */
		else {
			//msgctl(render_msgid, IPC_RMID, NULL); Erase MSG queue from kernel.
			while( msgrcv(render_msgid, &msgbuff, sizeof(msgbuff.mtext), MSG_REFRESH_SURFACE, IPC_NOWAIT)>=0 )
			{  }
		}
          #endif

#else /* SME_SYNC_RENDER */
		/* 2.1 Wait semaphore */
		tmout.tv_sec=0;
		tmout.tv_nsec=10000000;//500000000;
        gettimeofday(&tms, NULL);
		sret=sem_timedwait(&render_sem, &tmout);
		if(sret<0 ) {
			if(sret==ETIMEDOUT) { /* OK */ }
		}
        gettimeofday(&tme, NULL);
        printf("set_timedwait cost time=%luus \n",tm_diffus(tms,tme));

		/* 2.2 BLANK */
#endif

		/* 2.3 Render surface */
		egi_dpstd("---> Refresh surface!\n");
		if(chmap_ready)  {
		        gettimeofday(&tms, NULL);
			refresh_surface();
		        gettimeofday(&tme, NULL);
		        printf("Refresh surface cost time=%luus \n",tm_diffus(tms,tme));
		}
	}


	/* 3. Return */
	return (void*)0;
}
