/*----------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


A EGI_FILO buffer

1. WARNING: No mutex lock applied, It is supposed there is only one pusher and one puller,
   and they should NOT write/read the buff at the same time.

2. When you set auto double/halve flag, make sure that buff_size is N power of 2,
   or buff_size=1<<N. !!! --- IF NOT, DATA WILL BE LOST AFTER REALLOC --- !!!

Midas Zhou
----------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "egi_log.h"
#include "egi_utils.h"
#include "egi_filo.h"


/*-----------------------------------------------------------------
Calloc and init a FILO buffer
buff_size:	as for buff[buff_size][item_size]
item_size:
realloc:	if !=0, enable auto realloc for buff.

Return:
	Pointer to filo		Ok
	NULL			fails
-------------------------------------------------------------------*/
EGI_FILO * egi_malloc_filo(int buff_size, int item_size, int realloc)
{
	EGI_FILO *efilo=NULL;

        if( buff_size<=0 || item_size<=0 ) {
                EGI_PLOG(LOGLV_ERROR, "%s: input buff_size or item_size is invalid.",__func__);
                return NULL;
        }
        /* calloc efilo */
        efilo=(EGI_FILO *)calloc(1, sizeof(EGI_FILO));
        if(efilo==NULL) {
                EGI_PLOG(LOGLV_ERROR,"%s: Fail to malloc efilo.",__func__);
                return NULL;
        }

        /* NO LOCK APPLIED !!!! buff mutex lock */
//        if(pthread_mutex_init(&efilo->lock,NULL) != 0) {
//                printf("%s: fail to initiate EGI_FILO mutex lock.\n",__func__);
//                free(efilo);
//                return NULL;
//        }

        /* malloc buff, and memset inside... */
        efilo->buff=egi_malloc_buff2D(buff_size, item_size);
        if(efilo->buff==NULL) {
                EGI_PLOG(LOGLV_ERROR,"%s: Fail to malloc efilo->buff.", __func__);
                free(efilo);
                efilo=NULL;
                return NULL;
        }
	efilo->buff_size=buff_size;
	efilo->item_size=item_size;
	efilo->pt=0;
	efilo->auto_realloc=realloc;

	return efilo;
}


/*---------------------------------------
Free a EGI_FILO structure and its buff.
----------------------------------------*/
void egi_free_filo(EGI_FILO *efilo )
{
        if( efilo != NULL)
        {
                /* free buff */
                if( efilo->buff != NULL ) {
                        egi_free_buff2D(efilo->buff, efilo->buff_size);
                        //free(efifo->buff) and set efifo->buff=NULL in egi_free_buff2D() already
                }
                /* free itself */
                free(efilo);

                efilo=NULL;   /* ineffective though...*/
        }
}


/*-----------------------------------------------
Push data into FILO buffer
The caller shall confirm data size as per filo.
filo:	FILO struct
data:	pointer to data for push

Return:
	>0	filo buff is full.
	0	OK
	<0	fails
-------------------------------------------------*/
int egi_filo_push(EGI_FILO *filo, const void* data)
{
	/* verifyi input data */
	if( filo==NULL || filo->buff==NULL ) {
                EGI_PLOG(LOGLV_ERROR, "%s: input filo is invalid.",__func__);
                return -1;
        }
	if( data==NULL ) {
                EGI_PLOG(LOGLV_ERROR, "%s: input data is invalid.",__func__);
                return -2;
        }
	/* check buff space */
	if(filo->pt==filo->buff_size) {
		/* if enable auto double reaclloc, double buff size*/
		if((filo->auto_realloc)&0b01) {
			printf("%s: realloc buff size from %d to %d\n",
							__func__, filo->buff_size, (filo->buff_size)<<1);
		 	if( egi_realloc_buff2D( &filo->buff, filo->buff_size,
					    		(filo->buff_size)<<1, filo->item_size ) !=0 )
			{
				EGI_PLOG(LOGLV_ERROR,"%s: fail to realloc filo buff to double size.", __func__);
				return -3;
			}
			filo->buff_size <<= 1;
			EGI_PLOG(LOGLV_INFO,"%s: FILO buff_size is doubled to be %d.",
									__func__, filo->buff_size);
		}
		else {
			EGI_PLOG(LOGLV_ERROR,"%s: FILO buff is full. no more data to push in.", __func__);
			return 1;
		}
	}
	/* push data into buff */
	//printf("%s: memcpy\n",__func__);
	memcpy( (void *)(filo->buff)[filo->pt], data, filo->item_size );

	filo->pt++;

	return 0;
}


/*-------------------------------------------
Pop data from FILO buffer
filo:	FILO struct
data:	pointer to pass the data

Return:
	>0	filo buff is empty. data not
		changed.
	0	OK
	<0	fails
--------------------------------------------*/
int egi_filo_pop(EGI_FILO *filo, void* data)
{

	/* verifyi input data */
	if( filo==NULL || filo->buff==NULL ) {
                EGI_PLOG(LOGLV_ERROR, "%s: input filo is invalid.",__func__);
                return -1;
        }

	/* check buff point */
	if( filo->pt==0 ) {
//		EGI_PLOG(LOGLV_ERROR, "%s: egi_filo_pop(): FILO buff is empty, no more data to pop out.",
//												__func__);
		return 1;
	}
	/* if enbale auto halve realloc, halve buff size */
	if( (filo->auto_realloc)&0b10 )  {
		/* check buff space, if half empty */
		if( filo->pt == (filo->buff_size)>>1 )  {
			if( egi_realloc_buff2D( &filo->buff, filo->buff_size,
				    		(filo->buff_size)>>1, filo->item_size ) !=0 )
			{
				EGI_PLOG(LOGLV_ERROR,"%s: fail to realloc filo buff to half size.", __func__);
				return -3;
			}
			filo->buff_size >>= 1;
			EGI_PLOG(LOGLV_INFO,"%s: FILO buff_size is halved to be %d.",
									__func__, filo->buff_size);
		}
	}
	/* shift pt and copy data */
	filo->pt--;

	if(data != NULL)
		memcpy( data, (void *)(filo->buff)[filo->pt], filo->item_size);

	return 0;
}


/*----------------------------------------------------------------
Read data from FILO buffer, data in FILO keep intact after read.
filo:	FILO struct
pn:	index/position of the data in the filo buff, start from 0.
data:	pointer to pass the data

Return:
	>0	filo buff is empty.
	0	OK
	<0	fails
-----------------------------------------------------------------*/
int egi_filo_read(const EGI_FILO *filo,  int pn, void* data)
{
	/* verifyi input data */
	if( filo==NULL || filo->buff==NULL || data==NULL ) {
                EGI_PLOG(LOGLV_ERROR, "%s: input filo is invalid.",__func__);
                return -1;
        }
	/* check buff point */
	if( filo->pt==0 ) {
//		EGI_PLOG(LOGLV_ERROR, "%s: egi_filo_pop(): FILO buff is empty, no more data to pop out.",
//												__func__);
		return 1;
	}
	if( pn < 0 || pn > filo->pt-1 ) {
		EGI_PLOG(LOGLV_WARN,"%s: input pn is invalid.",__func__);
		return -2;
	}

	/* if data==NULL, Do not pass data */
	if(data != NULL)
		memcpy(data, (void *)(filo->buff)[pn], filo->item_size);

	return 0;
}


/*----------------------------------------
Get total number of items in the filo buff.
filo:	FILO struct

Return:
	number of data in the buff.
----------------------------------------*/
int egi_filo_itemtotal(const EGI_FILO *filo)
{
	/* verifyi input data */
	if( filo==NULL || filo->buff==NULL ) {
                EGI_PLOG(LOGLV_ERROR, "%s: input filo is invalid.",__func__);
                return 0;
        }

	return filo->pt;
}

/*---------------------------------------------------
Get 1D array from an EGI_FILO by deflating filo->buff.

Return:
	NULL	fails
	Others	OK
----------------------------------------------------*/
void* egi_filo_get_flatData(const EGI_FILO *filo)
{
	int i;
	unsigned char *data=NULL;

	/* verifyi input data */
	if( filo==NULL || filo->buff==NULL ) {
                EGI_PLOG(LOGLV_ERROR, "%s: input filo is invalid.",__func__);
                return 0;
        }

	data=calloc(filo->buff_size, filo->item_size);
	if(data==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s:Fail to calloc data!", __func__);
		return NULL;
	}

	for(i=0; i < filo->pt; i++)
		memcpy(data+i*filo->item_size, filo->buff[i], filo->item_size);

	return (void *)data;
}
