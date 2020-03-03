/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/

#ifndef __PAGE_FFMOTION_H__
#define __PAGE_FFMOTION_H__

#define TIME_SLIDER_ID        100
#define TIME_TXT0_ID          101
#define TIME_TXT1_ID          102

EGI_PAGE *create_ffmotionPage(void);
void free_ffmotionPage(void);
void motpage_update_timingBar(int tm_elapsed, int tm_duration );
void motpage_update_title(const unsigned char *title);
void motpage_rotate(unsigned char pos);
#endif
