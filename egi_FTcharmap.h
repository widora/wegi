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
	char		*txtbuff;		/* txt buffer, from where chars are brought to display on LCD */
	size_t		txtsize;		/* Size of txtbuff, in bytes */
	int		txtlen;			/* strlen of txtbuff, exclude '\0', need to be updated if content of txtbuff changed.  */

	/* A map for displayed chars only! A map to tell LCD coordinates of displayed chars and their offset/position in txtbuff[]   */
	char		*pref;			/* A pointer to strings to be displayed, a ref pointer to txtbuff */
	size_t		mapsize;		/* Size of map member arrays(charX,charY,charPos), Max. number for chcount */
	int		chcount;	 	/* Number_of_displayed_chars +1, '\n' also counted in.
					   	 * Note: count-1 for chars, the last data is always for the new inserting point
					   	 */

	int		pch;			/* Index of displayed char as of charX[],charY and charPos[], pch=0 is the first displayed char.
					 	* pch is also used to locate inserting positioin and typing_cursor position.
					 	*/
	int 		*charX;			/* Array, Char start point(left top) FB/LCD coordinates X,Y */
	int 		*charY;
	unsigned int	*charPos;		/* Array, Char offset position values relative to pdistr, in bytes.
					   while in demo codes: pos=charPos[pch],  pch as the index of the array, pos as offset of txtbuff */

	/* Extension: color,size,...*/
};


EGI_FTCHAR_MAP* FTcharmap_create(size_t txtsize,  size_t mapsize);
void 	FTcharmap_free(EGI_FTCHAR_MAP **chmap);
int  	FTcharmap_uft8strings_writeFB( FBDEV *fb_dev, EGI_FTCHAR_MAP *chmap, FT_Face face,
				    int fw, int fh, const unsigned char *pstr,
                                    unsigned int pixpl,  unsigned int lines,  unsigned int gap,
                                    int x0, int y0,
                                    int fontcolor, int transpcolor, int opaque,
                                    int *cnt, int *lnleft, int* penx, int* peny );

#endif
