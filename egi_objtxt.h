/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

All EGI OBJ Initializations


Midas Zhou
-----------------------------------------------------------------*/
#ifndef __EGI_OBJTXT_H__
#define __EGI_OBJTXT_H__

#include <stdint.h>


/*  ------------ MEMO: txt ebox  ----------- */
EGI_EBOX *create_ebox_memo(void);
EGI_EBOX *create_ebox_clock(void);
EGI_EBOX *create_ebox_note(void);
int egi_txtbox_demo(EGI_EBOX *ebox, EGI_TOUCH_DATA * touc_data);

/*  -------- egi pattern  ----- */
EGI_EBOX *create_ebox_titlebar(
        int x0, int y0,
        int offx, int offy,
        uint16_t bkcolor,
        char *title
);

EGI_EBOX *egi_msgbox_create(char *msg, long ms, uint16_t bkcolor);
void egi_msgbox_pvupdate(EGI_EBOX *msgbox, int pv);
void egi_msgbox_destroy(EGI_EBOX *msgbox);

#endif
