/*------------------ eig_txt.h --------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
--------------------------------------------------------------------*/
#ifndef __EGI_TXT_H__
#define __EGI_TXT_H__

#include "egi.h"
#include "egi_symbol.h"
#include <stdio.h>
#include <stdbool.h>

/* for txt ebox */
EGI_DATA_TXT *egi_init_data_txt(EGI_DATA_TXT *data_txt, 	/* ----- OBSOLETE ----- */
                 int offx, int offy, int nl, int llen, EGI_SYMPAGE *font, uint16_t color);

/* For non_FTsymbols */
EGI_DATA_TXT *egi_txtdata_new(  int offx, int offy, 		/* create new txt data */
			        int nl,				/* lines */
			        int llen,			/* chars per line */
			        EGI_SYMPAGE *font,	/* font */
			        uint16_t color			/* txt color */
			      );

/* For FTsymbols */
EGI_DATA_TXT *egi_utxtdata_new( int offx, int offy,     /* offset from ebox left top */
                                int nl,                 /* lines */
                                int pixpl,              /* pixels per line */
                                FT_Face font_face,      /* font face type */
                                int fw, int fh,         /* font width and height, in pixels */
                                int gap,                /* adjust gap between lines */
                                uint16_t color		/* txt color */
			       );

EGI_EBOX * egi_txtbox_new( char *tag,/* create new txt ebox */
			   EGI_DATA_TXT *egi_data,
		           bool movable,
		           int x0, int y0,
		           int width, int height,
		           int frame,
		           int prmcolor
			 );

int 	egi_txtbox_activate(EGI_EBOX *ebox);
int 	egi_txtbox_refresh(EGI_EBOX *ebox);
//static int egi_txtbox_decorate(EGI_EBOX *ebox);
int 	egi_txtbox_sleep(EGI_EBOX *ebox);
int 	egi_txtbox_hide(EGI_EBOX *ebox);
int 	egi_txtbox_unhide(EGI_EBOX *ebox);
EGI_DATA_TXT *egi_txtbox_getdata(EGI_EBOX *ebox);
void 	egi_free_data_txt(EGI_DATA_TXT *data_txt);

/* For non_FTsymbols only */
int 	egi_txtbox_readfile(EGI_EBOX *ebox, char *path);
void 	egi_txtbox_settitle(EGI_EBOX *ebox, char *title);
int 	egi_push_datatxt(EGI_EBOX *ebox, char *buf, int *pnl);
int 	egi_txtbox_set_direct(EGI_EBOX *ebox, int direct);

#endif
