/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/

#ifndef __PAGE_FFMUSIC_H__
#define __PAGE_FFMUSIC_H__

#include "egi.h"

#define TIME_SLIDER_ID        100
#define TIME_TXT0_ID          101
#define TIME_TXT1_ID          102

extern EGI_PAGE  *create_ffmuzPage(void);
extern void 	   free_ffmuzPage(void);
extern void 	muzpage_update_title(const unsigned char *title)__attribute__((weak));
extern void 	muzpage_update_timingBar(int tm_elapsed, int tm_duration )__attribute__((weak));


#endif
