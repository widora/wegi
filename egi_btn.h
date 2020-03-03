/*---------------- egi_btn.h--------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

egi btn tyep ebox functions

Midas Zhou
------------------------------------------------------*/
#ifndef __EGI_BTN_H__
#define __EGI_BTN_H__


#include "egi.h"
#include "egi_symbol.h"

EGI_DATA_BTN *egi_btndata_new(int id, enum egi_btn_type shape,
                                        EGI_SYMPAGE *icon, int icon_code,
					EGI_SYMPAGE *font
			        );

EGI_EBOX * egi_btnbox_new( char *tag,
        EGI_DATA_BTN *egi_data,
        bool movable,
        int x0, int y0,
        int width, int height,
        int frame,
        int prmcolor
);

int 	egi_btnbox_activate(EGI_EBOX *ebox);
int 	egi_btnbox_refresh(EGI_EBOX *ebox);
void 	egi_btngroup_refresh(EGI_EBOX **ebox_group, int num);
int 	egi_btnbox_hide(EGI_EBOX *ebox);
int 	egi_btnbox_unhide(EGI_EBOX *ebox);
void 	egi_free_data_btn(EGI_DATA_BTN *data_btn);
int 	egi_btnbox_setsubcolor(EGI_EBOX *ebox, EGI_16BIT_COLOR subcolor);
EGI_EBOX *egi_copy_btn_ebox(EGI_EBOX *ebox); /* Wait to be tested! */
//static void egi_btn_touch_effect(EGI_EBOX *ebox, EGI_TOUCH_DATA *touch_data);

#endif
