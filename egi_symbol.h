/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


For test only!

Midas Zhou
------------------------------------------------------------------------*/
#ifndef __EGI_SYMBOL_H__
#define __EGI_SYMBOL_H__

#include "egi_image.h"
#include "egi_fbgeom.h"
#include "egi.h"


/* symbol image size */
#define SYM_IMGPAGE_WIDTH 240
#define SYM_IMGPAGE_HEIGHT 320

#define TESTFONT_COLOR_FLIP 0 /* 1 use complementary color */

/*for symbol_writeFB(), roll back to start if symbol point reach boundary of FB mem */

#define no_FB_SYMOUT_ROLLBACK /* also check FB_DOTOUT_ROLLBACK in fblines.h */

#define SYM_NOSUB_COLOR -1 /* no substitute color defined for a symbol or font */
#define SYM_NOTRANSP_COLOR -1 /* no transparent color defined for a symbol or font */
#define SYM_FONT_DEFAULT_TRANSPCOLOR 1 /* >0 Ok, for symbol_string_writeFB(), if the symbol is font then
					use symbol back color as transparent tunnel */


/* APP ICON Index of EGI_SYMPAGE sympg_icon  */
#define ICON_DISKPLAY_INDEX	4
#define ICON_NETRADIO_INDEX	8
#define ICON_MEMOTXT_INDEX	5
#define ICON_DIAGRAM_INDEX	6
#define ICON_IOT_INDEX		1



/* symbol type */
enum symbol_type
{
        symtype_font,
        symtype_icon,
	symtype_FT2	/* freetype2 */
};


/*
 * symbol page struct
 * TODO: Cache FT2 characters if possible.
 * NOTE:
 *       1. For FT(FreeType2) wchar page, a symbl_page holds only one character. and many memebers
 *	    are not applicable then.
 */
typedef struct symbol_page EGI_SYMPAGE;
struct symbol_page
{
	/* symbol type, NOT used yet */
	enum symbol_type symtype;

	/* symbol image file path */
	/* for a 320x240x2Byte page, 2Bytes for 16bit color */
	char *path;
	/* back color of the image, a font symbol may use bkcolor as transparent channel */
	uint16_t  bkcolor;
	/* page symbol mem data, store each symbol data consecutively, no hole. while img page file may have hole or blank row*/
	uint16_t *data;		/* NOTE: for FT sympage, .data is useless */

	/* alpha data */
	unsigned char *alpha;	/* NOTE: for FT sympage, .data is useless, only .alpha is available */

	/* maximum number of symbols in this page, start from 0 */
	int  maxnum; /* maxnum+1 = total number,
		      * NOT applicable for FT page.
 		      */

	/* total symbol number for  each row, each row has 240 pixels in a raw img page. */
	int sqrow; /* for raw img page */

	/*same height for all symbols in a page */
	int symheight; /* For FT page, taken set_font height as in FT_Set_Pixel_Sizes(),
                          rather than bitmap.rows */

	/* use symb_index to locate a symbol in mem, and get its width */
	//struct symbol_index *symindex; /* symb_index[x], x is the corresponding code number of the symbol, like ASCII code */

	/* use symoffset[] to locate a symbole in a row */
	int *symoffset; /* in pixel, offset from uint16_t *data for each smybol!!
			 * Not applicable for FT page
			 */
	int *symwidth; /* in pixel, symbol width may be different, while height MUST be the same
			* Not applicable for FT page
			*/
	int ftwidth;	/* For FT page only, which holds only one character
			 * taken as slot->advance.x;
			 */

	/* !!!!! following not used, if symbol encoded from 0, you can use array index number as code number.
	 Each row has same number of symbols, so you can use code number to locate a row in a img page
	 Code is a unique encoding number for each symbol in a page, like ASCII number.
	*/
	int *symb_code; /* default NULL, if not applicable
			 * MAYBE: applicable for FT page
			 */
};


/*--------------  shared symbol data  -------------*/
extern EGI_SYMPAGE sympg_testfont;
extern EGI_SYMPAGE sympg_numbfont;
extern EGI_SYMPAGE sympg_buttons;
extern EGI_SYMPAGE sympg_sbuttons;
extern EGI_SYMPAGE sympg_icons;
extern EGI_SYMPAGE sympg_icons_2;
extern EGI_SYMPAGE sympg_heweather;

extern EGI_SYMPAGE sympg_ascii;

extern char symmic_cpuload[6][5];
extern char symmic_iotload[9];


/*------------------- functions ---------------------*/
int 	symbol_load_allpages(void); //__attribute__((constructor))
void 	symbol_release_allpages(void);
//static uint16_t *symbol_load_page(EGI_SYMPAGE *sym_page);
void 	symbol_release_page(EGI_SYMPAGE *sym_page);
int 	symbol_check_page(const EGI_SYMPAGE *sym_page, char *func);
void 	symbol_save_pagemem(EGI_SYMPAGE *sym_page);
int 	symbol_load_page_from_imgbuf(EGI_SYMPAGE *sym_page, EGI_IMGBUF *imgbuf);

/*--------------------------------------------------------------------------------------------
transpcolor:    >=0 transparent pixel will not be written to FB, so backcolor is shown there.
                <0       no transparent pixel

fontcolor:      font color (or symbol color for a symbol)
                >= 0, use given font color.
                <0   use color in img data

use following COLOR:
#define SYM_NOSUB_COLOR -1  --- no substitute color defined for a symbol or font
#define SYM_NOTRANSP_COLOR -1 --- no transparent color defined for a symbol or font

----------------------------------------------------------------------------------------------*/
void symbol_print_symbol(const EGI_SYMPAGE *sym_page, int symbol, uint16_t transpcolor);

int symbol_string_pixlen(char *str, const EGI_SYMPAGE *font);

void symbol_writeFB(FBDEV *fb_dev, const EGI_SYMPAGE *sym_page,  \
                int fontcolor, int transpcolor, int x0, int y0, unsigned int sym_code, int opaque);

void symbol_string_writeFB(FBDEV *fb_dev, const EGI_SYMPAGE *sym_page,   \
                int fontcolor, int transpcolor, int x0, int y0, const char* str, int opaque);

int symbol_strings_writeFB( FBDEV *fb_dev, const EGI_SYMPAGE *sym_page, unsigned int pixpl,	\
                             unsigned int lines,  unsigned int gap, int fontcolor, int transpcolor,	\
                             int x0, int y0, const char* str, int opaque);

void symbol_motion_string(FBDEV *fb_dev, int dt, const EGI_SYMPAGE *sym_page,   \
                	 		int transpcolor, int x0, int y0, const char* str);

void symbol_rotate(const EGI_SYMPAGE *sym_page,	\
                                                 int x0, int y0, int sym_code);



#endif
