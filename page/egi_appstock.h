/*----------------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------------------------------------*/
#ifndef __EGI_APPSTOCK_H__
#define __EIG_APPSTOCK_H__

#include "egi.h"

#define HTTP_REQUEST_INTERVAL	500 /* request interval time in ms */

/* data compression type */
enum stock_compress_type {
	none,		/* when new data comes, shift data_point[] to drop data_point[0]
			 * then save the latest data into data_point[num-1]
                         */
	interpolation,  /* when new data comes, update all data_point[] by interpolation */
	common_avg,	/* when new data comes, update all data_point[] by different weights */
	fold_avg,       /* avg data_point[0], then [1], then[2]....  */
};

void *egi_stockchart(EGI_PAGE *page);


#endif
