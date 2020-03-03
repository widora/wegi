/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/

#ifndef __EGI_FILO_H__
#define __EGI_FILO_H__

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define FILO_AUTO_DOUBLE	0b01
#define FILO_AUTO_HALVE		0b10

/*  1.  WARNING: No mutex lock applied, It is supposed there is only one pusher and one puller,
 *      and they should NOT write/read the buff at the same time.
 *  2.  When you set auto double/halve flag, make sure that buff_size is
 *      N power of 2,or buff_size=1<<N.
 */
typedef struct
{
        unsigned int    item_size;      /* size of each item data, in byte.*/
        unsigned int    buff_size;      /* total item number that the buff is capable of holding,
					 * actual item number is less than that. */
        unsigned char   **buff;         /* data buffer [buff_size][item_size] */

	unsigned int	pt;		/* current data position,
					 * pt++ AFTER push; pt-- BEFORE pop
					 */

//        pthread_mutex_t lock;         /* thread mutex lock */

	int		auto_realloc;	/* 1:0b01  double buff size each time when it's full.
					 * 2:0b10  release unused space when it's half empty.
					 * 3:0b11  auto double and halve 1+2
					 */
} EGI_FILO;


EGI_FILO * egi_malloc_filo(int buff_size, int item_size, int realloc);
void 	egi_free_filo(EGI_FILO *efilo );
int 	egi_filo_push(EGI_FILO *filo, const void* data);
int 	egi_filo_pop(EGI_FILO *filo, void* data);
int 	egi_filo_read(const EGI_FILO *filo, int pn, void* data);
int 	egi_filo_itemtotal(const EGI_FILO *filo);
void* 	egi_filo_get_flatData(const EGI_FILO *filo);

#endif
