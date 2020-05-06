/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
-------------------------------------------------------------------*/
#ifndef __EGI_FTCHARMAP_H__
#define __EGI_FTCHARMPA_H__

#include "egi_FTsymbol.h"
#include <freetype2/ft2build.h>
#include <freetype2/ftglyph.h>
#include <arpa/inet.h>
#include FT_FREETYPE_H


typedef struct FTsymbol_char_map	EGI_FTCHAR_MAP;		/* Char map for visiable/displayed characters. ---NOW!
								 * A map to relate displayed chars with their positions on LCD.
								 * Call FTsymbol_uft8strings_writeFB() to fill in the struct.
								 */
struct  FTsymbol_char_map {
	pthread_mutex_t mutex;      		/* mutex lock for char map */

/* 1. Global vars for txtbuff.  ( offset position relative to txtbuff ) */
	unsigned char	*txtbuff;		/* txt buffer, from where chars are brought to display on LCD */
	int		txtsize;		/* Size of txtbuff mem space allocated, in bytes */
	int		txtlen;			/* strlen of txtbuff, exclude '\0', need to be updated if content of txtbuff changed.  */

	/* disline/dline: displayed/charmapped line, A line starts/ends at displaying window left/right end side.
	 * retline: A line starts/ends by a new line token '\n'.
	 */
	int		txtdlines;		/* Size of txtdlinePos[]  */
	unsigned int	*txtdlinePos;		/* An array to store offset position(relative to txtbuff) of each txt diplayed lines */
	int		txtdlncount;		/* displayed/charmapped line count, for txtbuff
						 * 			--- NOTE ---
						 * 1. ALWAYS set txtdlncount to the right position before calling FTcharmap_uft8strings_writeFB(),
						 *    so that txtdlinePos[] will be updated properly!
						 * 2. After each FTcharmap_uft8strings_writeFB() calling, reset it to let:
					         *    chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0]
						 *    So it can sustain a stable loop calling.
						 *    !!! However, txtdlinePos[txtdlncount+1],txtdlinePos[txtdlncount+2]... +maplncount-1] still
						 *    hold valid data !!!
						 * 3. Only a line_shift OR page_shift action can change/reset the value of txtdlncount.
						 */


/* 2. Temporary vars for being displayed/charmapped txt.  ( offset position relative to pref )
	 * 1. A map for displayed chars only! A map to tell LCD coordinates of displayed chars and their offset/position in pref[]
	 * 2. Shift pref to change starting char/line for displaying.
	 * 3. Maplines limits the max. number of lines displayed/charmapped each time. It's MAY NOT be the exacte number of actual available
	 *    diplaying lines on LCD, it may be just for charmapping.
	 */
	unsigned char	*pref;			/* A pointer to strings to be displayed, a ref pointer to somewhere of txtbuff[].
						 * Initial pref=txtbuff.
						 */
	int		mapsize;		/* Size of map member arrays(charX,charY,charPos) mem space allocated , Max. number for chcount+1 */
	int		chcount;	 	/* Number_of_displayed_chars +1, '\n' also counted in. NOT index.
					   	 * Note: chcount-1 chars, the last data pref[charPos[chcount-1]] is EOF, and is always
						 * an inserting point.
					   	 */


	unsigned int	mappixpl;		/* pixels per disline */
	int		maplndis;		/* line distance betwee two dlines */

	unsigned int	maplines;		/* Max. displayed lines for current displaying window, size of linePos[] mem space allocatd. */
	int		maplncount;		/* Total number of displayed char lines in current charmap,  NOT index. */
	unsigned int	*maplinePos;		/* Offset position(relative to pref) of the first char of each displayed lines, in bytes */


	int		pch;			/* Index of displayed char as of charX[],charY and charPos[], pch=0 is the first displayed char.
					 	* pch is also used to locate inserting positioin and typing_cursor position.
					 	*/
	int 		*charX;			/* Array, Char start point(left top) FB/LCD coordinates X,Y */
	int 		*charY;
	unsigned int	*charPos;		/* Array, Char offset position relative to pref, in bytes.
					   while in demo codes: pos=charPos[pch],  pch as the index of the array, pos as offset of txtbuff */

	/* Extension: color,size,...*/
};


EGI_FTCHAR_MAP* FTcharmap_create(size_t txtsize,  size_t mapsize, size_t maplines, size_t mappixpl, int maplngap);

void 	FTcharmap_free(EGI_FTCHAR_MAP **chmap);
int 	FTcharmap_set_pref_nextDispLine(EGI_FTCHAR_MAP *chmap);
int  	FTcharmap_uft8strings_writeFB( FBDEV *fb_dev, EGI_FTCHAR_MAP *chmap,			/* mutex_lock */
                                    FT_Face face, int fw, int fh,
                                    int x0, int y0,
                                    int fontcolor, int transpcolor, int opaque,
                                    int *cnt, int *lnleft, int* penx, int* peny );

int 	FTcharmap_page_up(EGI_FTCHAR_MAP *chmap);
int 	FTcharmap_page_down(EGI_FTCHAR_MAP *chmap);
int 	FTcharmap_shift_oneline_up(EGI_FTCHAR_MAP *chmap);
int 	FTcharmap_shift_oneline_down(EGI_FTCHAR_MAP *chmap);

int  	FTcharmap_locate_charPos( EGI_FTCHAR_MAP *chmap, int x, int y);		/* mutex_lock */
int 	FTcharmap_goto_lineBegin( EGI_FTCHAR_MAP *chmap );  			 /* As retline, NOT displine */
int 	FTcharmap_goto_lineEnd( EGI_FTCHAR_MAP *chmap );

#endif
