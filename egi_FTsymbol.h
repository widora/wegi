/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
-------------------------------------------------------------------*/
#ifndef __EGI_FTSYMBOL_H__
#define __EGI_FTSYMBOL_H__

#include "egi_symbol.h"
#include <freetype2/ft2build.h>
#include <freetype2/ftglyph.h>
#include <arpa/inet.h>
#include FT_FREETYPE_H

typedef struct FTsymbol_library EGI_FONTS;
struct FTsymbol_library {

	char ftname[256];

	/***	www.freetype.org/freetype2/docs
	 * " In multi-threaded applications it is easiest to use one FT_Library
	 *   object per thread, In case this is too cumbersome, a single FT_Library object
	 *   across threads is possible also, as long as a mutex lock is used around FT_New_Face
	 *   and FT_Done_Face. "
	 */
	FT_Library     library;		/* NOTE: typedef struct FT_LibraryRec_  *FT_Library */


	/* Regular type */
        FT_Face         regular;	/* NOTE: typedef struct FT_FaceRec_*  FT_Face */
	char 		*fpath_regular;

	/* Light type */
        FT_Face         light;
	char 		*fpath_light;

	/* Regular type */
        FT_Face         bold;
	char 		*fpath_bold;

	/* Special type */
        FT_Face         special;
	char 		*fpath_special;
};

extern EGI_SYMPAGE sympg_ascii;  /* default font  LiberationMono-Regular */

extern EGI_FONTS   egi_sysfonts; /* system font set */
extern EGI_FONTS   egi_appfonts; /* APP font set */

int 	FTsymbol_load_library( EGI_FONTS *symlib );
FT_Face FTsymbol_create_newFace( EGI_FONTS *symlib, const char *ftpath);
void 	FTsymbol_release_library( EGI_FONTS *symlib );
int	FTsymbol_load_allpages(void);
void 	FTsymbol_release_allpages(void);
int  	FTsymbol_load_sysfonts(void);
int  	FTsymbol_load_appfonts(void);
void	FTsymbol_release_allfonts(void);
int  	FTsymbol_load_asciis_from_fontfile( EGI_SYMPAGE *symfont_page, const char *font_path, int Wp, int Hp );

void 	FTsymbol_unicode_writeFB(FBDEV *fb_dev, FT_Face face, int fw, int fh, wchar_t wcode, int *xleft,
				int x0, int y0, int fontcolor, int transpcolor,int opaque);

int  	FTsymbol_unicstrings_writeFB( FBDEV *fb_dev, FT_Face face, int fw, int fh, const wchar_t *pwchar,
			       unsigned int pixpl,  unsigned int lines,  unsigned int gap,
                               int x0, int y0,
			       int fontcolor, int transpcolor, int opaque );

int  	FTsymbol_uft8strings_writeFB( FBDEV *fb_dev, FT_Face face, int fw, int fh, const unsigned char *pstr,
			       unsigned int pixpl,  unsigned int lines,  unsigned int gap,
                               int x0, int y0,
			       int fontcolor, int transpcolor, int opaque,
 			       int *cnt, int *lnleft, int* penx, int* peny );

int  	FTsymbol_uft8strings_pixlen( FT_Face face, int fw, int fh, const unsigned char *pstr);

#endif
