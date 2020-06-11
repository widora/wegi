/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "egi_unihan.h"

/*-------------------------------------------------
Create an UNIHAN_HEAP with empty data.

@capacity:	Max. number of EGI_UNIHANs to be
		hold in the heap.

Return:
	Pointer to an EGI_UNIHAN_HEAP	Ok
	NULL				Fails
--------------------------------------------------*/
EGI_UNIHAN_HEAP* UniHan_create_heap(size_t capacity)
{
	EGI_UNIHAN_HEAP	*heap=NULL;

	if(capacity==0)
		return NULL;

	/* Calloc heap */
	heap = calloc(1, sizeof(EGI_UNIHAN_HEAP));
	if(heap==NULL) {
		printf("%s: Fail to calloc unihan heap.\n",__func__);
		return NULL;
	}

	/* Calloc heap->unihans */
	heap->unihans = calloc(capacity+1, sizeof(EGI_UNIHAN)); /* +1 more as for sentinel */
	if(heap->unihans==NULL) {
		printf("%s: Fail to calloc heap->unihans.\n",__func__);
		free(heap);
		return NULL;
	}

	/* Assign memebers */
	heap->capacity=capacity;
	heap->size=0;

	return heap;
}

/*---------------------------------------
	Free an EGI_UNIHAN_HEAP.
----------------------------------------*/
void UniHan_free_heap( EGI_UNIHAN_HEAP **heap)
{
	if(heap==NULL || *heap==NULL)
		return

	free( (*heap)->unihans );
	free(*heap);
	*heap=NULL;
}



/*------------------------------------------------
Create a set of unihan, with empty data.

@name: 	Short name for the set, MAX len 16-1.
@size:  Total number of unihans in the set.

Return:
	A pointer to EGI_UNIHAN_SET	Ok
	NULL				Fail
------------------------------------------------*/
EGI_UNIHAN_SET* UniHan_create_set(const char *name, size_t size)
{

	EGI_UNIHAN_SET	*unihan_set=NULL;

	if(size==0)
		return NULL;

	/* Calloc unihan set */
	unihan_set = calloc(1, sizeof(EGI_UNIHAN_SET));
	if(unihan_set==NULL) {
		printf("%s: Fail to calloc unihan_set.\n",__func__);
		return NULL;
	}

	/* Calloc unihan_set->unihans */
	unihan_set->unihans = calloc(size, sizeof(EGI_UNIHAN));
	if(unihan_set->unihans==NULL) {
		printf("%s: Fail to calloc unihan_set->unihans.\n",__func__);
		free(unihan_set);
		return NULL;
	}

	/* Assign memebers */
	strncpy(unihan_set->name, name, sizeof(unihan_set->name)-1);
	unihan_set->size=size;

	return unihan_set;
}

/*---------------------------------------
	Free an EGI_UNIHAN_SET.
----------------------------------------*/
void UniHan_free_set( EGI_UNIHAN_SET **set)
{
	if(set==NULL || *set==NULL)
		return

	free( (*set)->unihans );
	free(*set);
	*set=NULL;
}




