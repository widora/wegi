/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/

#ifndef __PAGE_AVENGER_H__
#define __PAGE_AVENGER_H__

EGI_PAGE *create_avengerPage(void);
void 	avenpage_update_creditTxt(int score, int level);
void 	free_avengerPage(void);

#endif
