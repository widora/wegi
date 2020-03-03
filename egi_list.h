/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
-------------------------------------------------------------------*/

#ifndef __EGI_LIST_H__
#define __EGI_LIST_H__

#include "egi.h"
#include "egi_symbol.h"

#define  LIST_ITEM_MAXLINES  10 /* MAX number of txt line for each list item */
#define  LIST_DEFAULT_BKCOLOR	WEGI_COLOR_GRAY /*default list item bkcolor */


/* functions for list */
EGI_EBOX *egi_listbox_new (
        int x0, int y0, /* left top point */
        int inum,       /* item number of a list */
        int nwin,       /* number of items in displaying window */
        int width,      /* H/W for each list item ebox, W/H of the hosting ebox depends on it */
        int height,
        int frame,      /* -1 no frame for ebox, 0 simple .. */
        int nl,         /* number of txt lines for each txt ebox */
        int llen,       /* in byte, length for each txt line */
        EGI_SYMPAGE *font, /* txt font */
        int txtoffx,     /* offset of txt from the ebox, all the same */
        int txtoffy,
        int iconoffx,   /* offset of icon from the ebox, all the same */
        int iconoffy
);



void egi_free_data_list(EGI_DATA_LIST *data_list);
int egi_listbox_activate(EGI_EBOX *ebox);
int egi_listbox_refresh(EGI_EBOX *ebox);
int egi_listbox_updateitem(EGI_EBOX *ebox, int n, int prmcolor, const char **txt);
// static int egi_itembox_decorate(EGI_EBOX *ebox);

#endif
