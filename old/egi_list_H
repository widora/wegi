#ifndef __EGI_LIST_H__
#define __EGI_LIST_H__

#include "egi.h"


#define  LIST_ITEM_MAXLINES  10 /* MAX number of txt line for each list item */
#define  LIST_DEFAULT_BKCOLOR	WEGI_COLOR_GRAY /*default list item bkcolor */

typedef struct egi_list EGI_LIST;

struct egi_list
{
	/* left point coordinates */
	int x0;
	int y0;

	/* total number of items in a list */
	int inum;

	/* ----- for all txt ebox --------
	  in pixel,
	  width and height for each list item
	  the height also may be adjusted according to number of txt lines and icon later
	*/
//	int width;
//	int height;

	/* ----- for all txt ebox --------
	in byte,  length of each txt line */
//	int llen;

	/* a list of type_txt ebox */
	EGI_EBOX **txt_boxes;

	/* ----- for all txt ebox --------
	 frame type, to see egi_ebox.c
	 -1  no frame
	  0  simple
	 ....
	*/
//	int frame;

	/* ----- for all txt ebox --------
	  sympg font for the txt ebox */
//	struct symbol_page *font;

	/* ----- for all txt ebox --------
	  offset of txt from the ebox */
//	int txtoffx;
//	int txtoffy;

	/*
	  sympg icon for each list item
	  NULL means no icon
	*/
	struct symbol_page **icons;
	int *icon_code;

	/*  offset of icon from the ebox */
	int iconoffx;
	int iconoffy;

	/*
	   prime_color/back_color for each list item,
	   or use first bkcolor as default color
	  init default color in egi_list_new()
	*/
	uint16_t *prmcolor;

	/*
	   color for each txt line in a list item.
	   if null, use default as black 0.
	   ALL BLACK now !!!!!
	*/
	//uint16_t (*txtcolor)[LIST_ITEM_MAXLINES];

};


/* functions for list */
EGI_LIST *egi_list_new (
        int x0, int y0, /* left top point */
        int inum,       /* item number of a list */
        int width,      /* h/w for each list item - ebox */
        int height,
        int frame,      /* -1 no frame for ebox, 0 simple .. */
        int nl,         /* number of txt lines for each txt ebox */
        int llen,       /* in byte, length for each txt line */
        struct symbol_page *font, /* txt font */
        int txtoffx,     /* offset of txt from the ebox, all the same */
        int txtoffy,
        int iconoffx,   /* offset of icon from the ebox, all the same */
        int iconoffy
);
int egi_list_free(EGI_LIST *list);
int egi_list_activate(EGI_LIST *list);
int egi_list_refresh(EGI_LIST *list);
int egi_list_updateitem(EGI_LIST *list, int n, uint16_t prmcolor, char **data);


#endif
