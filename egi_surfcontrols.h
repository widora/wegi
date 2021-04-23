/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
---------------------------------------------------------------------*/
#ifndef __EGI_SURFCONTROLS_H__
#define __EGI_SURFCONTROLS_H__

#include "egi_fbdev.h"
#include "egi_image.h"
#include "egi_FTsymbol.h"

typedef struct egi_surface_box		ESURF_BOX;    // ESURF_LABEL
typedef struct egi_surface_label	ESURF_LABEL;
typedef struct egi_surface_button	ESURF_BTN;
typedef struct egi_surface_tickbox	ESURF_TICKBOX;


/***
			--- An EGI_SURFACE_BOX ---
 1. A simple box on surface.
*/
struct egi_surface_box {
	int		x0;	/* Origin position relative to its container */
	int		y0;
	EGI_IMGBUF	*imgbuf;

	/* Reactions / Operations */
	// On_Touch:
	// On_Click:
	// On_dblClick:

	unsigned char data[];
};
ESURF_BOX 	*egi_surfBox_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h);
void 	egi_surfBox_free(ESURF_BOX **box);
bool 	egi_point_on_surfBox(const ESURF_BOX *box, int x, int y);
void 	egi_surfBox_writeFB(FBDEV *fbdev, const ESURF_BOX *box,  int cx0, int cy0);


/***
			--- An EGI_SURFACE_LABEL ---
 1. A simple label on surface.
*/
struct egi_surface_label {
	int		x0;	/* Origin position relative to its container */
	int		y0;
	EGI_IMGBUF	*imgbuf;	/* As bk image */

	int		w;	/* Width and heigh for text, in case imgbuf==NULL. */
	int		h;
#define ESURF_LABTEXT_MAX	512
	char		text[ESURF_LABTEXT_MAX];	/* content */

	/* Reactions / Operations */
	// On_Touch:
	// On_Click:
	// On_dblClick:
};
ESURF_LABEL 	*egi_surfLab_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h);
void 	egi_surfLad_free(ESURF_LABEL **lab);
void 	egi_surfLab_writeFB(FBDEV *fbdev, const ESURF_LABEL *lab, FT_Face face, int fw, int fh, EGI_16BIT_COLOR color, int cx0, int cy0);
void 	egi_surfLab_updateText( ESURF_LABEL *lab, const char *text );

/***
			--- An EGI_SURFACE_BUTTON ---
 1. A simple button on surface.
*/
enum {
	SURFBTN_IMGBUF_NORMAL  =0,
	SURFBTN_IMGBUF_EFFECT  =1,
	SURFBTN_IMGBUF_PRESSED =2,
};
struct egi_surface_button {
	int		x0;	/* Origin position relative to its container */
	int		y0;
	EGI_IMGBUF	*imgbuf; /* To hold normal button image */
				 /* OR: Use its subimg to hold effect/pressed/... images */

	EGI_IMGBUF	*imgbuf_effect;  /* Partial OR whole size */
	EGI_IMGBUF	*imgbuf_pressed;
	bool		pressed;
	/* Reactions / Operations */
	// On_Touch:
	// On_Click:
	// On_dblClick:
};
ESURF_BTN *egi_surfBtn_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h);
void 	egi_surfBtn_free(ESURF_BTN **sbtn);
void    egi_surfBtn_writeFB(FBDEV *fbdev, const ESURF_BTN *btn, int type, int cx0, int cy0);
bool 	egi_point_on_surfbtn(const ESURF_BTN *sbtn, int x, int y);


/***
			--- An EGI_SURFACE_TICKBOX ---
 1. A simple TickBox on surface.
*/
struct egi_surface_tickbox {
	int		x0;	/* Origin position relative to its container */
	int		y0;
	EGI_IMGBUF	*imgbuf_unticked;  /* BOX size included */

	EGI_IMGBUF	*imgbuf_ticked;	   /* To create based on imgbuf_unticked */
	bool		ticked;

	/* Reactions / Operations */
	// On_Click:
};
ESURF_TICKBOX *egi_surfTickBox_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h);
void 	egi_surfTickBox_free(ESURF_TICKBOX **tbox);
bool 	egi_point_on_surfTickBox(const ESURF_TICKBOX *tbox, int x, int y);
void 	egi_surfTickBox_display(FBDEV *fbdev, const ESURF_TICKBOX *tbox,  int cx0, int cy0);


#endif
