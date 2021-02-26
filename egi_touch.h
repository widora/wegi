/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/
#ifndef __EGI_TOUCH_H__
#define __EGI_TOUCH_H__

#include "egi.h"
#include "egi_sbtn.h"
#include <stdbool.h>

/* typedef struct egi_touch_data EGI_TOUCH_DATA in "egi.h" */

bool 		egi_touch_on_rectBTN(EGI_TOUCH_DATA *touch_data, EGI_RECTBTN *btn);
int 		egi_touch_timeWait_press( int s, unsigned int ms, EGI_TOUCH_DATA *touch_data);
int 		egi_touch_timeWait_release( int s, unsigned int ms, EGI_TOUCH_DATA *touch_data);
void 		egi_touchread_nowait(bool nowait);
int 		egi_start_touchread(void);
int 		egi_end_touchread(void);
bool 		egi_touchread_is_running(void);
bool 		egi_touch_getdata(EGI_TOUCH_DATA *data);
int 		egi_touch_fbpos_data(FBDEV *fbdev, EGI_TOUCH_DATA *touch_data, int rpos);
EGI_TOUCH_DATA 	egi_touch_peekdata(void);
int 		egi_touch_peekdx(void);
int 		egi_touch_peekdy(void);
void 		egi_touch_peekdxdy(int *dx, int *dy);
enum 		egi_touch_status egi_touch_peekstatus(void);
void* 		egi_touch_loopread(void);


#endif
