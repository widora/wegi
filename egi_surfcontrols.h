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

/* Extern type definition, as in egi_surface.h */
typedef struct egi_surface_user		EGI_SURFUSER;

/* Type definition */
typedef struct egi_surface_box		ESURF_BOX;
typedef struct egi_surface_label	ESURF_LABEL;
typedef struct egi_surface_button	ESURF_BTN;
typedef struct egi_surface_tickbox	ESURF_TICKBOX;
typedef struct egi_surface_listbox	ESURF_LISTBOX;


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
bool 	egi_point_on_surfBtn(const ESURF_BTN *sbtn, int x, int y);


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
void 	egi_surfTickBox_writeFB(FBDEV *fbdev, const ESURF_TICKBOX *tbox,  int cx0, int cy0);

/***
			--- An EGI_SURFACE_LISTBOX ---
 1. A simple ListBox with vertical scroll bar
 2. ListBoxH == ScrollBarH
*/
struct egi_surface_listbox {
	int		x0;		/* Origin position relative to its container */
	int		y0;
	EGI_IMGBUF	*imgbuf;  	/* Whole listbox area, include vertical scroll bar */
				  	/* (ListboxW + ScrollBarW) * ListBoxH */

	/* List items */
#define ESURF_LISTBOX_ITEM_MAXLEN	256
	char 		(*list)[ESURF_LISTBOX_ITEM_MAXLEN];	/* ALL item array */
					/* !!! WARNING !!!
					 * To ensure space of list[TotalItems].
		 			*/

	size_t		ListCapacity;	/* Capacity of list space, init as 16. */
#define ESURF_LISTBOX_CAPACITY_GROWSIZE	32

	int		TotalItems;	/* Total item in list[] */

	int 		FirstIdx;	/* Index of list[] for the first item displayed in ListBox, <0 Invalid.
					 * 1. Init. as -1 when ListBox created.
					 * 2. ListBox displays items from the FirstIdx item before mouse_event triggers.
					 * 3. To temprarily store first item index.
					 * 4. It changes as mouse_event scroll the ListBox.

					 * Usually FristIdx set as previous SelecteIdx.
					 */

	int		SelectIdx;	/* Index of list[] for currently selected Item, <0 Invalid.
					 * As confirmed selected item index of var_SelectIdx.
					 * 1. Init as -1 when  ListBox created.
					 * 2. When ListBox surface quits, it returns a NON_nagetive value as index of list[]
				  	 *    OR return -1, as cancelling the selection, by clicking on SURFBNT_CLOSE.
					 */
	int 		var_SelectIdx;	/* As an interim variable for listbox_mouse_event function, <0 Invalid.
					 * 1. It will be modified when mouse points to a list item in the ListBox, however
					 *    it will NOT be saved to SelectIdx, until it's clicked to confirm the selection.
					 * 2. Init. as -1 when ListBox created.
					 */
//	bool		init_mevent; 	/* It's token(0) to initlize static variables in listbox_mouse_event */


	/* Font */
	FT_Face 	face;		/* Init as egi_appfonts.regular */
	int 		fh;		/* Font Height and Width, Init as 12 */
	int		fw;

	/* For list box area */
        int 		ListBoxW; 	/* List area size, WITHOUT scroll bar */
	int 		ListBoxH;
	int		ListBarH;	/* Line spacing,  Height of each list bar */

	int		MaxViewItems; 	/* Max. items int the ListBox
					 * MaxViewItems = listbox->ListBoxH/listbox->ListBarH XXX+2! --2 for partial items.
					 */

        EGI_IMGBUF      *ListImgbuf;	/* For ListFB.virt_fb */
        FBDEV           ListFB;		/* virtual FB for list content write */

	/* For scroll bar area */
#define ESURF_LISTBOX_SCROLLBAR_WIDTH	10
        int 		ScrollBarW;	/* Size of vertical scroll area */
        int 		ScrollBarH;     /* == ListBoxH */
        //int SBx0, SBy0;      /* SCroll bar origin relative to its container. (this surface) */

	/* For scroll Guiding Block   */
	int 		pastPos;	/* Height of scroll area above the Scroll slider, stands for passed/viewed content. */
	int 		GuideBlockH;	/* control 	block height */
	int 		GuideBlockW;	/* == ScrollAreaW */
	bool		GuideBlockDownHold;  /* If GuideBlock is hold_down for dragging */

	/* Reactions / Operations */

};
ESURF_LISTBOX 	*egi_surfListBox_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h, int fw, int fh, int ListBarH);
void	egi_surfListBox_free(ESURF_LISTBOX **listbox);
int 	egi_surfListBox_addItem(ESURF_LISTBOX *listbox, const char *pstr);
void 	egi_surfListBox_redraw(EGI_SURFUSER *psurfuser, ESURF_LISTBOX *listbox);
int 	egi_surfListBox_adjustFirstIdx(ESURF_LISTBOX *listbox, int delt); /* FirstIdx driver pastPos */
int 	egi_surfListBox_adjustPastPos(ESURF_LISTBOX *listbox, int delt);  /* pastPos driver FirstIdx */
int 	egi_surfListBox_PxySelectItem(EGI_SURFUSER *surfuser, ESURF_LISTBOX *listbox, int px, int py);

#endif
