/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


                        --- Definition and glossary ---

0. cursor:  A blinking cursor is an editing point, corresponding with chmap->pchoff pointing to some where of
            chmap->txtbuff, to/from which you may add/delete chars.

                                        --- NOTICE ---
            1. A cursor usually can move to the left side of the last char of a dline, NOT the right side.
            If a cursor can move to the right side of the last char of a dline, or say end of the dline, that
            means it is a newline char ('\n'), OR it's the EOF.
            2. Sometimes it may needs more than one press/operation ( delete, backspace, shift etc.)
            to make the cursor move, this is because there is/are unprintable chars with zero width.
            3. If the cursor(pchoff) in NOT shown in current charmap, just press any key to scroll to locate it.
                                        !!! WARNING !!!
            If you press the deleting key, it will execute anyway, even the cursor(pchoff) is NOT in the current charmap!
            though it will scroll to cursor position after deletion, you may NOT be aware of anything that have been deleted.
            So always bring the cursor within your sight before editing!

1. char:    A printable ASCII code OR a local character with UFT-8 encoding.
2. charmap: A EGI_FTCHAR_MAP struct that holds data of currently displayed chars,
            of both their corresponding coordinates in displaying window and offset position in memory.
	    FAINT WARNING: Do NOT confuse with Freetype charmap concept!
3. dline:  displayed/charmapped line, A line starts/ends at displaying window left/right end side.
   retline: A line starts/ends by a new line token '\n'.

4. scroll_up/down:
                scroll up/down charmap for one dline
                keep cursor position unchanged(relative to txtbuff)
                Functions: FTcharmap_scroll_oneline_up(),  FTcharmap_scroll_oneline_down()

5. shift_up/down/left/right:
                shift typing cursor up/down/left/right.
                charmap follow the cursor to scroll if it gets out of current charmap.
                Functions: FTcharmap_shift_cursor_up(),  FTcharmap_shift_cursor_down()
                            FTcharmap_shift_cursor_left(),  FTcharmap_shift_cursor_right()

6. page_up/down:
                scroll whole charmap up/down totally out of current displayed charmap,
                keep cursor position unchanged(relative to txtbuff)
                Functions: FTcharmap_page_up(),  FTcharmap_page_down()

7. EOF:         For txtbuff:      A '\0' token to signal as end of the buffered string.
                For saved file:   A '\n' to comply with UNIX/POSIX file end standard.

8. Selection mark:
                Two offsets of txtbuff, pchoff and pchoff2, which defines selected content/chars btween them.
                if(pchoff==pchoff2), then no selection.


                        --- PRE_Charmap Actions ---

PRE_1:  Set chmap->txtdlncount          ( txtdlinePos[] index,a link between txtbuff and pref
                                          as txtdlinePos[txtdlncount]=pref-txtbuff
                                          and chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0] )
PRE_2:  Set chmap->pref                 ( set starting point for charmapping )
PRE_3:  Set chmap->pchoff/pchoff2       ( chmap->pch/pch2: to be derived from pchoff/pchoff2 in charmapping! )
PRE_4:  Set chmap->fix_cursor           ( option: keep cursor position unchanged on screen  )
PRE_5:  Set chmap->follow_cursor        ( option: auto. scroll to keep cursor always in current charmap )
PRE_6:  Set chmap->request              ( request charmapping exclusively and immediately )

                        ---  DO_Charmap FTcharmap_uft8strings_writeFB() ---

charmap_1:      Update chmap->chcount
Charmap_2:      Update chmap->charX[], charY[],charPos[]

charmap_3:      Update chmap->maplncount
charmap_4:      Update chmap->maplinePos[]

charmap_5:      Update chmap->txtdlcount   ( NOTE: chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0] )
charmap_6:      Update chmap->txtdlinePos[]

charmap_7:      Check chmap->follow_cursor
charmap_8:      Check chmap->fix_cursor

charmap_9:      Draw selection mark

                        --- POST_Charmap Actions ---

POST_1: Check chmap->errbits
POST_2: Redraw cursor


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#ifndef __EGI_FTCHARMAP_H__
#define __EGI_FTCHARMPA_H__

#include "egi_color.h"
#include "egi_FTsymbol.h"
#include <freetype2/ft2build.h>
#include <freetype2/ftglyph.h>
#include <arpa/inet.h>
#include FT_FREETYPE_H

#ifdef __cplusplus
 extern "C" {
#endif

typedef enum FTcharmap_errcode FTCHARMAP_ERRCODE;
enum FTcharmap_errcode {
	CHMAPERR_NO_ERR=0,

	/* Hit limit with chmap.txtsize > chmap.txtlen+1 */
	CHMAPERR_TXTSIZE_LIMIT =1,

	/* Hit limit with chmap.chcount > chmap.mapsize, some displayed chars have NO charX/charY/charPos data!
	 * It'll fail to locate those chars later! AND cause unexpected behaviors/results.
	 */
	CHMAPERR_MAPSIZE_LIMIT =1<<1,

	/* Hit limit with chmap.txtdlncount > chmap.txtdlines, some displayed lines have NO txtdlinesPos data!
	 * It'll fail to locate those lines later! AND cause unexpected behaviors/results.
	 */
	CHMAPERR_TXTDLINES_LIMIT =1<<2,
};

/* FAINT WARNING: Do NOT confuse with Freetype charmap concept! */
typedef struct FTsymbol_char_map	EGI_FTCHAR_MAP;		/* Char map for visiable/displayed characters. ---NOW!
								 * A map to relate displayed chars with their positions on LCD.
								 * Call FTsymbol_uft8strings_writeFB() to fill in the struct.
								 */

/* 		!!! WARNING !!!
   Reference to any recallocable/memgrowable memeber is dangerous! Offset position is recommended instead.
*/

struct  FTsymbol_char_map {

/* 1. Global vars for txtbuff.  ( offset position relative to txtbuff ) */
	int		request;		/* ==0: charmap is clear, charmap data are consistent with each other.
						 * !=0: Some params are reset, and is requested to do charmap immediately and exclusively
					 	 *	to make pch/pchoff, txtdlncount/pref and other data consistent!
						 *
						 * 			--- IMPORTANT ---
						 * After an operation which needs an immediate re_charmap afterwards, the 'request' must be set.
						 * It will then acts as an rejection semaphore to other thread functions which are also trying to
						 * modify charmap parameters before the urgent re_charmap is completed.
						 * TODO:  not applied for all functions yet!
						 * TODO:  Charmap functions will try to check request only once, and the function will return
						 * if request in nonzero.  -- Keep trying/waiting ?
						 */
	unsigned int	errbits;		/* to record types of errs that have gone through.  */
	pthread_mutex_t mutex;      		/* mutex lock for charmap */

	FT_Face		face;			/* Freetype font typeface, A handle to a given typographic face object. */

	int		txtsize;		/* Size of txtbuff mem space allocated, in bytes */
	//unsigned int	txtsize;		/* Size of txtbuff mem space allocated, in bytes */
	unsigned char	*txtbuff;		/* txt buffer, auto mem_grow. */
	#define TXTBUFF_GROW_SIZE       1024    //64      /* Auto. mem_grow size for chmap->txtbuff[] */

	int		txtlen;			/* string length of txtbuff,  txtbuff EOF '\0' is NOT counted in!
						 * To be updated if content of txtbuff changed.
						 */

	/* disline/dline: displayed/charmapped line, A line starts/ends at displaying window left/right end side.
	 * retline/rline: A line starts/ends by a new line token '\n'.
	 */
	int		txtrlines;		/* LIMIT: Size of txtdlinePos[], auto mem_grow.  */
	unsigned int	*txtrlinePos;		/* An array to store offset position(relative to txtbuff) of each txt dlines, which
						 * have already been charmapped, and MAYBE not displayed in the current charmap.
					         */
	#define TXTRLINES_GROW_SIZE     1024    /* Auto. mem_grow size for chmap->txtdlines and chmap->txtdlinePos[] */

	int		txtrlncount;		/* Counter for retlines */


	int		txtdlines;		/* LIMIT: Size of txtdlinePos[], auto mem_grow.  */
	unsigned int	*txtdlinePos;		/* An array to store offset position(relative to txtbuff) of each txt dlines, which
						 * have already been charmapped, and MAYBE not displayed in the current charmap.
						 * 1. In FTcharmap_get_txtdlIndex(), txtdlinePos[] after current charmap are considered as
						 *    VALID if they are >0!
					         */
	#define TXTDLINES_GROW_SIZE     1024    /* Auto. mem_grow size for chmap->txtdlines and chmap->txtdlinePos[] */

	int		txtdlncount;		/* displayed/charmapped line count, for all txtbuff
						 * 			--- NOTE ---
						 * 1. ALWAYS set txtdlncount to the start/right position before calling FTcharmap_uft8strings_writeFB(),
						 *     			--- IMPORTANT ---
						 *    so that txtdlinePos[] will be updated properly! Its index will start from txtdlncount
						 * 2. After each FTcharmap_uft8strings_writeFB() calling, reset it to let:
					         *    chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0]
						 *    So it can sustain a stable loop calling.
						 *    !!! However, txtdlinePos[txtdlncount+1],txtdlinePos[txtdlncount+2]... +maplncount-1] still
						 *    hold valid data !!!
						 * 3. Only a line_shift OR page_shift action can change/reset the value of txtdlncount.
						 */

	int	 	bkgcolor;		 /* background/paper color.
						  * If bkgcolor<0, ignore. no bkg color,
						  * Else if bkgcolor>=0 set as EGI_16BIT_COLOR
						  */
	int		precolor;		 /* Preset as fontcolor before each inserting(string/char) operation.
						  * MUST reset to -1 after insertion. means functions only once.
						  * It prevails fontcolor AND charColorMap
						  * If <0, then it will be invalid.
						  */
	EGI_16BIT_COLOR fontcolor;		 /* font color,charColorMap will prevail it. */
	EGI_COLOR_BANDMAP *charColorMap;	 /* color map for chars: bands[]->pos corresponds to pos(offset) to txtbuff.
						  * If NULL, then apply fontcolor
						  */
	EGI_COLOR_BANDMAP *hlmarkColorMap;	 /* color map for Highlight mark: bands[]->pos corresponds to pos(offset) to txtbuff.
						  * If NULL, then do not apply.
						  */

	EGI_16BIT_COLOR	markcolor;		 /* Selection mark color */
	EGI_8BIT_ALPHA	markalpha;		 /* Selection mark alpha */


	/* --- Geometry parameters and fixed vars --- */
	int		mapx0;			/* charmap area left top point  */
	int		mapy0;

	int		height;			/* Height and width of displaying area, including margins */
	int		width;

	int 		offx;			/* Left side margin for blank paper */
	int		offy;			/* Top margin for blank paper */

	unsigned int	mappixpl;		/* pixels per disline */
	int		maplndis;		/* line distance betwee two dlines */

	unsigned int	maplines;		/*  LIMIT: Max. number of displayed lines for current displaying window,
					         *  and also size of maplinePos[] mem space allocatd.
						 */
	int		mapsize;		/* Size of map member arrays(charX,charY,charPos) mem space allocated , Max. number for chcount+1 */
	#define MAPSIZE_GROW_SIZE       1024    /* Auto. mem_grow size for charX[] ,charY[],charPos[]  */

/* 2. Temporary vars for being displayed/charmapped txt.  ( offset position relative to pref )
	 * 1. A map for displayed chars only! A map to tell LCD coordinates of displayed chars and their offset/position in pref[].
	 *    Each char(and typing cursor) displayed will then have its charX/Y and charPos data in current charmap struct, by which
	 *    each pixel on the screen may be mapped to its corresponding char in mem.
	 * 2. Change .pref to display different blocks in txtbuff. ( pref is starting char/line for displaying).
	 * 3. Change .pchoff AND .pch to locate/keep typing(deleting,inserting) cursor position.
	 *    .pchoff --- in bytes, position in txtbuff[]
	 *    .pch    --- in chars, position inc charX[] charY[] charPos[]
	 * 4. Maplines limits the max. number of lines displayed/charmapped each time. It's MAY NOT be the exacte number of actual available
	 *    diplaying lines on LCD, it may be just for charmapping.
	 */
	unsigned char	*pref;			/* A pointer to strings to be displayed, a ref pointer to somewhere of txtbuff[].
					         * content pointed by pref will be charmapped and displayed.
						 * Initial pref=txtbuff.
						 */
	wchar_t		maskchar;		/* If maskchar !=0 , then use maskchar for displaying ( NOT charmapping ) */
	int		chcount;	 	/* Total number of displayed/charmapped chars.
					         * (EOF also include if charmapped, '\n' also counted in. NOT as index.
					   	 * Note: xxxchcount-1 charsxx, the last data pref[charPos[chcount-1]] is EOF, and is always
						 * an inserting point. ( If EOF is charmapped.  )
					   	 */


	int		maplncount;		/* Total number of displayed char lines in current charmap,  NOT index. */
	unsigned int	*maplinePos;		/* Offset position(relative to pref) of the first char of each displayed lines, in bytes
						 * redundant with: txtdlinePos[txtdlncount],...[txtdlncount+1],...[txtdlncount+2]... +maplncount-1].
						 * ....relative to txtbuff thought. 	TODO: cancel it.
						 */

	unsigned int	pchoff;			/* Offset postion to txtbuff !!!, OnlyIf pchoff>0, it will be used to relocate pch after charmapping!
						 *			--- MOST IMPORTANT ---
						 * pchoff/pch points to the CURRENT/IMMEDIATE typing/inserting position.
						 * In case that after inserting a new char, chmapsize reaches LIMIT and need to shift one line down,
                           			 * then we'll use pchoff to locate the typing position in charmapping. so after inserting a new char
						 * into txtbuff, ALWAYS update pchoff to keep track of typing/inserting cursor position.
						 *			--- pchoff V.S. pchoff2 ---
					 	 * NOTE: pchoff is the master of pchoff2, if pchoff changes, pchoff2 changes with it,
						 * 	 to keep as pchoff2==pchoff (say interlocked).
						 * Only when selecting action is triggered, then will pchoff2 diffs from pchoff.
						 */

	unsigned int    pchoff2;		/* OnlyIf pchoff2>0, it will be used to relocate pch2. see pch2. */

	bool		fix_cursor;		/* If true: chmap->pch/pch2 and pchoff/pchoff2 will all be updated by charPos_nolock() in charmapping.
						 * After charmapping, set pch as to re_locate cursor nearest to its previous (x,y) postion.
						 * set as true when shift cursor up/down to cross top/bottom dline.
						 */
	bool		follow_cursor;		/* If true: charmapping will continue to shift dline by dline until pchoff/pch cursor gets into
					  	 * current charmap,and results in pch>=0.
						 * Charmapped image will be rendered/displayed after shifting each dline.
						 */
	int		pch;			/* Index of displayed char as of charX[],charY and charPos[], pch=0 is the first displayed char.
						 * chmap->pch/pch2 is derived from chmap->pchoff/pchoff2 in charmapping!
						 *   			--- MOST IMPORTANT ---
						 * pchoff/pch points to the CURRENT/IMMEDIATE typing/inserting position.
						 * If pchoff NOT in current charmap, then pch<0.
						 *			--- NOTE pch v.s. pch2 ---
					 	 * NOTE: pch is the master of pch2, if pch changes, pch2 changes with it, to keep as pch2==pch.
						 * Only when selecting action is triggered, then will pch2 diffs from pch.
						 */
	int		pch2;			/* Applied as second pch to mark beginning OR ending of a selection.
						 * chmap->pch/pch2 is derived from chmap->pchoff/pchoff2 in charmapping!
						 * If pchoff2 NOT in current charmap, then pch2<0.
						 */
	int 		*charX;			/* Array, Char start point(left top) FB/LCD coordinates X,Y  auto_grow */
	int 		*charY;			/*  auto_grow */
	unsigned int	*charPos;		/* Array, Char offset position relative to pref, in bytes. auto_grow */
	//unsigned int 	*charW;			/* Array,Widths of chars */

	/* Extension: color,size,... */

	#define CURSOR_BLINK_INTERVAL_MS 500 	/* typing_cursor blink interval in ms, cursor ON and OFF duration time, same.*/
	bool		cursor_on;		/* To turn ON cursor */
	struct timeval tm_blink;		/* For cursor blinking timimg */
	void (*draw_cursor)(int x, int y, int lndis); /* Draw blinking editing cursor,
						       * default: FTcharmap_draw_cursor(int x, int y, int lndis )
						       */
};


EGI_FTCHAR_MAP* FTcharmap_create(size_t txtsize,  int x0, int y0,  int height, int width, int offx, int offy,
                                 FT_Face face, size_t mapsize, size_t maplines, size_t mappixpl, int maplndis,
				 int bkgcolor, EGI_16BIT_COLOR fontcolor, bool charColorMap_ON, bool hlmarkColorMap_ON);
				 //bool charColorMap_ON, EGI_16BIT_COLOR fontcolor );

void 	FTcharmap_set_markcolor(EGI_FTCHAR_MAP *chmap, EGI_16BIT_COLOR color, EGI_8BIT_ALPHA alpha);
int 	FTcharmap_memGrow_txtbuff(EGI_FTCHAR_MAP *chmap, size_t more_size);	/* NO lock */
int 	FTcharmap_memGrow_txtdlinePos(EGI_FTCHAR_MAP *chmap, size_t more_size); /* No lock */
int 	FTcharmap_memGrow_mapsize(EGI_FTCHAR_MAP *chmap, size_t more_size);   	/* NO lock */
int 	FTcharmap_load_file(const char *fpath, EGI_FTCHAR_MAP *chmap, size_t txtsize);
int 	FTcharmap_save_file(const char *fpath, EGI_FTCHAR_MAP *chmap);	/* mutex_lock */

void 	FTcharmap_free(EGI_FTCHAR_MAP **chmap);
int 	FTcharmap_set_pref_nextDispLine(EGI_FTCHAR_MAP *chmap);
int  	FTcharmap_uft8strings_writeFB( FBDEV *fb_dev, EGI_FTCHAR_MAP *chmap,			/* mutex_lock, request_clear */
                                    //FT_Face face, int fw, int fh,
                                    int fw, int fh,
                                    //int fontcolor, int transpcolor, int opaque,
                                    int transpcolor, int opaque,
                                    int *cnt, int *lnleft, int* penx, int* peny );
//static void FTcharmap_mark_selection(FBDEV *fb_dev, EGI_FTCHAR_MAP *chmap); /*without mutex_lock */

/* Note: Following functions assume that current/previous pages have been charmapped already! */
int 	FTcharmap_page_up(EGI_FTCHAR_MAP *chmap);			/* mutex_lock + request */
int 	FTcharmap_page_down(EGI_FTCHAR_MAP *chmap);			/* mutex_lock + request */
int 	FTcharmap_page_fitBottom(EGI_FTCHAR_MAP *chmap);		/* mutex_lock + request */
int 	FTcharmap_scroll_oneline_up(EGI_FTCHAR_MAP *chmap);		/* mutex_lock + request */
int 	FTcharmap_scroll_oneline_down(EGI_FTCHAR_MAP *chmap);		/* mutex_lock + request */

/* To locate chmap->pch */
int  	FTcharmap_locate_charPos( EGI_FTCHAR_MAP *chmap, int x, int y);		/* mutex_lock + request_check */

//static int FTcharmap_locate_charPos_nolock( EGI_FTCHAR_MAP *chmap, int x, int y);  /* without mutex_lock, no request_check */

/* To locate chmap->pch2 */
int  	FTcharmap_locate_charPos2( EGI_FTCHAR_MAP *chmap, int x, int y);		/* mutex_lock */
int 	FTcharmap_reset_charPos2( EGI_FTCHAR_MAP *chmap );  /* reset pch2=pch */        /* mutex_lock */

int 	FTcharmap_shift_cursor_up(EGI_FTCHAR_MAP *chmap);		/* mutex_lock + request */
int 	FTcharmap_shift_cursor_down(EGI_FTCHAR_MAP *chmap);		/* mutex_lock + request */
int 	FTcharmap_shift_cursor_right(EGI_FTCHAR_MAP *chmap);		/* mutex_lock + request */
int 	FTcharmap_shift_cursor_left(EGI_FTCHAR_MAP *chmap);		/* mutex_lock + request */

int 	FTcharmap_set_pchoff( EGI_FTCHAR_MAP *chmap, unsigned int pchoff, unsigned int pchoff2 );   /* mutex_lock */
int 	FTcharmap_goto_lineBegin( EGI_FTCHAR_MAP *chmap );  	/* mutex_lock + request */
int 	FTcharmap_goto_lineEnd( EGI_FTCHAR_MAP *chmap );	/* mutex_lock + request */

int     FTcharmap_goto_firstDline ( EGI_FTCHAR_MAP *chmap );    /* mutex_lock + request */
//int     FTcharmap_goto_lastDline ( EGI_FTCHAR_MAP *chmap );    /* mutex_lock + request */

int 	FTcharmap_getPos_lastCharOfDline(EGI_FTCHAR_MAP *chmap,  int dln); /* ret pos is relative to txtdlinePos[] */
int 	FTcharmap_get_txtdlIndex(EGI_FTCHAR_MAP *chmap,  unsigned int pchoff);

int 	FTcharmap_go_backspace( EGI_FTCHAR_MAP *chmap );		/* mutex_lock + request +charColorMap|hlmarkColorMap */

int 	FTcharmap_insert_string_nolock( EGI_FTCHAR_MAP *chmap, const unsigned char *pstr, size_t strsize ); /* +charColorMap|hlmarkColorMap */
int 	FTcharmap_insert_string( EGI_FTCHAR_MAP *chmap, const unsigned char *pstr, size_t strsize );  /* mutex_lock + request */


int 	FTcharmap_insert_char( EGI_FTCHAR_MAP *chmap, const char *ch );	/* mutex_lock + request  +charColorMap|hlmarkColorMap */

/* Delete a char preceded by cursor OR chars selected between pchoff2 and pchoff */
int 	FTcharmap_delete_string_nolock( EGI_FTCHAR_MAP *chmap );	/*  +charColorMap|hlmarkColorMap */
int 	FTcharmap_delete_string( EGI_FTCHAR_MAP *chmap );		/* mutex_lock + request */

/* Modify color band associated with char/highlight */
int     FTcharmap_modify_charColor( EGI_FTCHAR_MAP *chmap, EGI_16BIT_COLOR color, bool request);	/* mutex_lock + request + charColorMap */
int  	FTcharmap_modify_hlmarkColor( EGI_FTCHAR_MAP *chmap, EGI_16BIT_COLOR color, bool request);	/* mutex_lock + request + hlmarkColorMap */

/* For XTERM */
int  FTcharmap_shrink_dlines( EGI_FTCHAR_MAP *chmap, size_t dlns);	/* mutex_lock + request  +charColorMap|hlmarkColorMap */

/* To/from EGI_SYSPAD */
int 	FTcharmap_copy_from_syspad( EGI_FTCHAR_MAP *chmap );		/* mutex_lock + request */
int 	FTcharmap_copy_to_syspad( EGI_FTCHAR_MAP *chmap );
int 	FTcharmap_cut_to_syspad( EGI_FTCHAR_MAP *chmap );		/* mutex_lock + request */

int 	FTcharmap_save_words( EGI_FTCHAR_MAP *chmap, const char *fpath );

#ifdef __cplusplus
 }
#endif

#endif
