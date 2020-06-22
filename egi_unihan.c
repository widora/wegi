/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Note:
1. See Unicode Character Database in http://www.unicode.org.
	# Unicode Character Database
        # © 2020 Unicode®, Inc.
        # Unicode and the Unicode Logo are registered trademarks of Unicode, Inc. in the U.S. and other countries.
        # For terms of use, see http://www.unicode.org/terms_of_use.html
        # For documentation, see http://www.unicode.org/reports/tr38/

2. With reference to:
  2.1 https://blog.csdn.net/firehood_/article/details/7648625
  2.2 https://blog.csdn.net/FortuneWang/article/details/41575039
  2.3 http://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip (Unihan_Readings.txt)
  2.3 http://www.unicode.org/reports/tr38

3. After purification, the uniset usually contain some empty unihans(wcode==0)
   and those unihans are distributed randomly. However the total size of the uniset does
   NOT include those empty unihans, so if you sort swcode in such case, you MUST take
   uniset->capacity (NOT uniset->size!!!) as total sorting itmes.
   UniHan_quickSort_typing() will rearrange all empty unihans to the end.

Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <ctype.h>
#include <errno.h>
#include "egi_debug.h"
#include "egi_unihan.h"
#include "egi_cstring.h"
#include "egi_math.h"
#include "egi_utils.h"

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
	heap->unihans = calloc(capacity+1, sizeof(EGI_UNIHAN)); /* +1 more as for sentinel, as binary heap index starts from 1, NOT 0. */
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

@name: 	    Short name for the set, MAX len 16-1.
	    If NULL, ignore.
@capacity:  Capacity to total max. number of unihans in the set.

Return:
	A pointer to EGI_UNIHAN_SET	Ok
	NULL				Fail
------------------------------------------------*/
EGI_UNIHAN_SET* UniHan_create_uniset(const char *name, size_t capacity)
{

	EGI_UNIHAN_SET	*unihan_set=NULL;

	if(capacity==0)
		return NULL;

	/* Calloc unihan set */
	unihan_set = calloc(1, sizeof(EGI_UNIHAN_SET));
	if(unihan_set==NULL) {
		printf("%s: Fail to calloc unihan_set.\n",__func__);
		return NULL;
	}

	/* Calloc unihan_set->unihans  */
	unihan_set->unihans = calloc(capacity+1, sizeof(EGI_UNIHAN)); /* +1 as for END token */
	if(unihan_set->unihans==NULL) {
		printf("%s: Fail to calloc unihan_set->unihans.\n",__func__);
		free(unihan_set);
		return NULL;
	}

	/* Assign memebers */
	//printf("%s:sizeof(unihan_set->name)=%d\n", __func__, sizeof(unihan_set->name));
	if(name != NULL)
		strncpy(unihan_set->name, name, sizeof(unihan_set->name)-1);
	unihan_set->capacity=capacity;
	/* unihan_set->size=0 */

	return unihan_set;
}


/*---------------------------------------
	Free an EGI_UNIHAN_SET.
----------------------------------------*/
void UniHan_free_uniset( EGI_UNIHAN_SET **set)
{
	if( set==NULL || *set==NULL )
		return;

	free( (*set)->unihans );
	free(*set);
	*set=NULL;
}


#if 0 ///////////////////////////////////////////////////////
/*----------------------------------------------------------------
Insert an unihan into the heap as per "Priority Binary Heap" Rule.
the inserted member is copied from given unihan.

@heap:		Pointer to an EGI_UNIHAN_HEAP
@unihan:	Pointer to an EGI_UNIHAN

Return:
	0	OK
	<0	Fails
----------------------------------------------------------------*/
int UniHan_heap_insert(EGI_UNIHAN_HEAP* heap, const EGI_UNIHAN* unihan)
{
	int i;

	if( heap==NULL || heap->unihans==NULL )
		return -1;
	if(heap->size >= heap->capacity) {
		printf("%s: No more space for unihans. Please consider to increase capacity.\n",__func__);
		return -2;
	}

	heap->size++;

	/* If the first entry, index==1 */
	if(size==1) {
		heap->unihans[1]=*unihan; /* Direct assign */
		return 0;
	}

	/* Bubble i up, as to find the right sequence for unihan.typing[] */
	for( i=heap->size; UniHan_compare_typing(unihan, heap->unihans[i/2])<0; i /= 2 )
		heap->unihans[i]=unihnas[i/2];	/* binary */



}
#endif  //////////////////////////////



/*---------------------------------------------
Reset freq of all unihans in the uniset to 0.
---------------------------------------------*/
void UniHan_reset_freq( EGI_UNIHAN_SET *uniset )
{
	int i;

	/* Check input */
	if(uniset==NULL || uniset->unihans==NULL || uniset->size==0) /* If empty */
		return;

	for(i=0; i < uniset->size; i++)
		uniset->unihans[i].freq=0;

}


/*-------------------------------------------------------------------------
Compare typings of two unihans and return an integer less than, equal to,
or greater than zero, if unha1 is found to be ahead of, at same order of,
or behind uhans2 in dictionary order respectively.

Note:
1. !!! CAUTION !!!  Here typing refers to PINYIN, other type MAY NOT work.
2. All typing letters MUST be lowercase.
3. It first compares their dictionary order, if NOT in the same order, return
the result; if in the same order, then compare their frequency number to
decide the order.
4. If uhan1->freq is GREATER than uhan2->freq, it returns -1(IS_AHEAD);
   else if uhan1->freq is LESS than uhan2->freq, then return 1(IS_AFTER);
   if EQUAL, it returns 0(IS_SAME).
5. An empty UNIHAN is always 'AFTER' a nonempty UNIHAN.
6. A NULL is always 'AFTER' a non_NULL UNIHAN pointer.

Return:
	Relative Priority Sequence position:
	CMPTYPING_IS_AHEAD    -1 (<0)	unhan1 is  'less than' OR 'ahead of' unhan2
	CMPTYPING_IS_SAME      0 (=0)   unhan1 and unhan2 are at the same place.
	CMPTYPING_IS_AFTER     1 (>0)   unhan1 is  'greater than' OR 'behind' unhan2.

-------------------------------------------------------------------------*/
int UniHan_compare_typing(const EGI_UNIHAN *uhan1, const EGI_UNIHAN *uhan2)
{
	int i;

	/* 1. If uhan1 and/or uhan2 is NULL */
	if( uhan1==NULL && uhan2==NULL )
		return CMPTYPING_IS_SAME;
	else if( uhan1==NULL )
		return CMPTYPING_IS_AFTER;
	else if( uhan2==NULL )
		return CMPTYPING_IS_AHEAD;

	/* 2. Put the empty EGI_UNIHAN to the last, OR check typing[0] ? */
	if(uhan1->wcode==0 && uhan2->wcode!=0)
		return CMPTYPING_IS_AFTER;
	else if(uhan1->wcode!=0 && uhan2->wcode==0)
		return CMPTYPING_IS_AHEAD;
	else if(uhan1->wcode==0 && uhan2->wcode==0)
		return CMPTYPING_IS_SAME;

	/* 3. Compare dictionary oder of EGI_UNIHAN.typing[] */
	for( i=0; i<UNIHAN_TYPING_MAXLEN; i++) {
		if( uhan1->typing[i] > uhan2->typing[i] )
			return CMPTYPING_IS_AFTER;
		else if ( uhan1->typing[i] < uhan2->typing[i])
			return CMPTYPING_IS_AHEAD;
		/* End of typing[] */
		else if(uhan1->typing[i]==0 && uhan2->typing[i]==0)
			break;

		/* else: uhan1->typing[i] == uhan2->typing[i] != 0  */
	}
	/* NOW: CMPTYPING_IS_SAME */

	/* 4. Compare frequencey number: EGI_UNIHAN.freq */
	if( uhan1->freq > uhan2->freq )
		return CMPTYPING_IS_AHEAD;
	else if( uhan1->freq < uhan2->freq )
		return CMPTYPING_IS_AFTER;
	else
		return CMPTYPING_IS_SAME;
}



/*------------------------------------------------------------------------
Sort an EGI_UNIHAN array by Insertion_Sort algorithm, to rearrange unihans
with typing and freq as sorting KEY. (typing:in dictionary order)

Also ref. to mat_insert_sort() in egi_math.c.

Note:
1.The caller MUST ensure unihans array has at least n memebers!
2.Input 'n' MAY includes empty unihans with typing[0]==0, and they will
  be rearranged to the end of the array.

@unihans:       An array of EGI_UNIHANs.
@n:             size of the array. ( NOT capacity )

------------------------------------------------------------------------*/
void UniHan_insertSort_typing( EGI_UNIHAN* unihans, int n )
{
        int i;          /* To tranverse elements in array, 1 --> n-1 */
        int k;          /* To decrease, k --> 1,  */
        EGI_UNIHAN tmp;

	if(unihans==NULL)
		return;

	/* Start sorting ONLY when i>1 */
        for( i=1; i<n; i++) {
                tmp=unihans[i];   /* the inserting integer */

        	/* Slide the inseting integer left, until to the first smaller unihan  */
                for( k=i; k>0 && UniHan_compare_typing(unihans+k-1, &tmp)==CMPTYPING_IS_AFTER; k--)
				unihans[k]=unihans[k-1];   /* swap */

		/* Settle the inserting unihan at last swapped place */
                unihans[k]=tmp;
        }
}


/*--------------------------------------------------------------------
Sort an EGI_UNIHAN array by Quick_Sort algorithm, to rearrange unihans
with typing (as KEY) in dictionary+frequency order.

Refert to mat_quick_sort() in egi_math.h.

 	!!! WARNING !!! This is a recursive function.

Note:
1. The caller MUST ensure start/end are within the legal range.


@start:		start index, as of unihans[start]
@End:		end index, as of unihans[end]
@cutoff:	cutoff value for switch to insert_sort.

Return:
	0	Ok
	<0	Fails
----------------------------------------------------------------------*/
int UniHan_quickSort_typing(EGI_UNIHAN* unihans, unsigned int start, unsigned int end, int cutoff)
{
	int i=start;
	int j=end;
	int mid;
	EGI_UNIHAN	tmp;
	EGI_UNIHAN	pivot;

	/* Check input */
	if( unihans==NULL )
		return -1;

	/* End sorting */
	if( start >= end )
		return 0;

        /* Limit cutoff */
        if(cutoff<3)
                cutoff=3;

/* 1. Implement quicksort */
        if( end-start >= cutoff )
        {
        /* 1.1 Select pivot, by sorting array[start], array[mid], array[end] */
                /* Get mid index */
                mid=(start+end)/2;

                /* Sort [start] and [mid] */
                /* if( array[start] > array[mid] ) */
		if( UniHan_compare_typing(unihans+start, unihans+mid)==CMPTYPING_IS_AFTER ) {
                        tmp=unihans[start];
                        unihans[start]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* Now: [start]<=array[mid] */

                /* IF: [mid] >= [start] > [end] */
                /* if( array[start] > array[end] ) { */
		if( UniHan_compare_typing(unihans+start, unihans+end)==CMPTYPING_IS_AFTER ) {
                        tmp=unihans[start]; /* [start] is center */
                        unihans[start]=unihans[end];
                        unihans[end]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* ELSE:   [start]<=[mid] AND [start]<=[end] */
                /* else if( array[mid] > array[end] ) { */
		if( UniHan_compare_typing(unihans+mid,unihans+end)==CMPTYPING_IS_AFTER ) {
                        /* If: [start]<=[end]<[mid] */
                                tmp=unihans[end];   /* [end] is center */
                                unihans[end]=unihans[mid];
                                unihans[mid]=tmp;
                        /* Else: [start]<=[mid]<=[end] */
                }
                /* Now: array[start] <= array[mid] <= array[end] */

                pivot=unihans[mid];
                //printf("After_3_sort: %d, %d, %d\n", array[start], array[mid], array[end]);

                /* Swap array[mid] and array[end-1], still keep array[start] <= array[mid] <= array[end]!   */
                tmp=unihans[end-1];
                unihans[end-1]=unihans[mid];   /* !NOW, we set memeber [end-1] as pivot */
                unihans[mid]=tmp;

        /* 1.2 Quick sort: array[start] ... array[end-1], array[end]  */
                i=start;
                j=end-1;   /* As already sorted to: array[start]<=pivot<=array[end-1]<=array[end], see 1. above */
                for(;;)
                {
                        /* Stop at array[i]>=pivot: We preset array[end-1]==pivot as sentinel, so i will stop at [end-1]  */
                        /* while( array[++i] < pivot ){ };   Acturally: array[++i] < array[end-1] which is the pivot memeber */
			while( UniHan_compare_typing(unihans+(++i),&pivot)==CMPTYPING_IS_AHEAD ) { };

                        /* Stop at array[j]<=pivot: We preset array[start]<=pivot as sentinel, so j will stop at [start]  */
                        /* while( array[--j] > pivot ){ }; */
			while( UniHan_compare_typing(unihans+(--j),&pivot)==CMPTYPING_IS_AFTER ) { };

                        if( i<j ) {
                                /* Swap array[i] and array[j] */
                                tmp=unihans[i];
                                unihans[i]=unihans[j];
                                unihans[j]=tmp;
                        }
                        else {
                                break;
			}
		}
                /* Swap pivot memeber array[end-1] with array[i], we left this step at last.  */
                unihans[end-1]=unihans[i];
                unihans[i]=pivot; /* Same as array[i]=array[end-1] */

        /* 1.3 Quick sort: recursive call for sorted parts. and leave array[i] as mid of the two parts */
		UniHan_quickSort_typing(unihans, start, i-1, cutoff);
		UniHan_quickSort_typing(unihans, i+1, end, cutoff);

        }
/* 2. Implement insertsort */
        else
		UniHan_insertSort_typing( unihans+start, end-start+1);


	return 0;
}


/*------------------------------------------------------------------------
Sort an EGI_UNIHAN array by Insertion_Sort algorithm, to rearrange unihans
with freq (as KEY) in ascending order.

Also ref. to mat_insert_sort() in egi_math.c.

Note:
1.The caller MUST ensure unihans array has at least n memebers!

@unihans:       An array of EGI_UNIHANs.
@n:             size of the array. ( NOT capacity )
-----------------------------------------------------------------------*/
void UniHan_insertSort_freq( EGI_UNIHAN* unihans, int n )
{
        int i;          /* To tranverse elements in array, 1 --> n-1 */
        int k;          /* To decrease, k --> 1,  */
        EGI_UNIHAN tmp;

	if(unihans==NULL)
		return;

	/* Start sorting ONLY when i>1 */
        for( i=1; i<n; i++) {
                tmp=unihans[i];

        	/* Slide the inseting integer left, until to the first smaller unihan  */
                for( k=i; k>0 && unihans[k-1].freq > tmp.freq; k--)
				unihans[k]=unihans[k-1];   /* swap */

		/* Settle the inserting unihan at last swapped place */
                unihans[k]=tmp;
        }
}


/*--------------------------------------------------------------------
Sort an EGI_UNIHAN array by Quick_Sort algorithm, to rearrange unihans
with freq (as KEY) in ascending order.

Refert to mat_quick_sort() in egi_math.h.

 	!!! WARNING !!! This is a recursive function.

Note:
1. The caller MUST ensure start/end are within the legal range.

@start:		start index, as of unihans[start]
@End:		end index, as of unihans[end]
@cutoff:	cutoff value for switch to insert_sort.

Return:
	0	Ok
	<0	Fails
----------------------------------------------------------------------*/
int UniHan_quickSort_freq(EGI_UNIHAN* unihans, unsigned int start, unsigned int end, int cutoff)
{
	int i=start;
	int j=end;
	int mid;
	EGI_UNIHAN	tmp;
	EGI_UNIHAN	pivot;

	/* Check input */
	if( unihans==NULL )
		return -1;

	/* End sorting */
	if( start >= end )
		return 0;

        /* Limit cutoff */
        if(cutoff<3)
                cutoff=3;

/* 1. Implement quicksort */
        if( end-start >= cutoff )
        {
        /* 1.1 Select pivot, by sorting array[start], array[mid], array[end] */
                /* Get mid index */
                mid=(start+end)/2;

                /* Sort [start] and [mid] */
                /* if( array[start] > array[mid] ) */
		if( unihans[start].freq > unihans[mid].freq ) {
                        tmp=unihans[start];
                        unihans[start]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* Now: [start]<=array[mid] */

                /* IF: [mid] >= [start] > [end] */
                /* if( array[start] > array[end] ) { */
		if( unihans[start].freq > unihans[end].freq ) {
                        tmp=unihans[start]; /* [start] is center */
                        unihans[start]=unihans[end];
                        unihans[end]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* ELSE:   [start]<=[mid] AND [start]<=[end] */
                /* else if( array[mid] > array[end] ) { */
		if( unihans[mid].freq > unihans[end].freq ) {
                        /* If: [start]<=[end]<[mid] */
                                tmp=unihans[end];   /* [end] is center */
                                unihans[end]=unihans[mid];
                                unihans[mid]=tmp;
                        /* Else: [start]<=[mid]<=[end] */
                }
                /* Now: array[start] <= array[mid] <= array[end] */

                pivot=unihans[mid];
                //printf("After_3_sort: %d, %d, %d\n", array[start], array[mid], array[end]);

                /* Swap array[mid] and array[end-1], still keep array[start] <= array[mid] <= array[end]!   */
                tmp=unihans[end-1];
                unihans[end-1]=unihans[mid];   /* !NOW, we set memeber [end-1] as pivot */
                unihans[mid]=tmp;

        /* 1.2 Quick sort: array[start] ... array[end-1], array[end]  */
                i=start;
                j=end-1;   /* As already sorted to: array[start]<=pivot<=array[end-1]<=array[end], see 1. above */
                for(;;)
                {
                        /* Stop at array[i]>=pivot: We preset array[end-1]==pivot as sentinel, so i will stop at [end-1]  */
                        /* while( array[++i] < pivot ){ };   Acturally: array[++i] < array[end-1] which is the pivot memeber */
			while( unihans[++i].freq < pivot.freq ) { };
                        /* Stop at array[j]<=pivot: We preset array[start]<=pivot as sentinel, so j will stop at [start]  */
                        /* while( array[--j] > pivot ){ }; */
			while( unihans[--j].freq > pivot.freq ) { };

                        if( i<j ) {
                                /* Swap array[i] and array[j] */
                                tmp=unihans[i];
                                unihans[i]=unihans[j];
                                unihans[j]=tmp;
                        }
                        else {
                                break;
			}
		}
                /* Swap pivot memeber array[end-1] with array[i], we left this step at last.  */
                unihans[end-1]=unihans[i];
                unihans[i]=pivot; /* Same as array[i]=array[end-1] */

        /* 1.3 Quick sort: recursive call for sorted parts. and leave array[i] as mid of the two parts */
		UniHan_quickSort_freq(unihans, start, i-1, cutoff);
		UniHan_quickSort_freq(unihans, i+1, end, cutoff);

        }
/* 2. Implement insertsort */
        else
		UniHan_insertSort_freq( unihans+start, end-start+1);

	return 0;
}



/*------------------------------------------------------------------------
Sort an EGI_UNIHAN array by Insertion_Sort algorithm, to rearrange unihans
with wcode (as KEY) in ascending order.

Also ref. to mat_insert_sort() in egi_math.c.

Note:
1.The caller MUST ensure unihans array has at least n memebers!
2.Input 'n' should NOT includes empty unihans with wcode==0, OR they will
  be rearranged to the beginning of the array.


@unihans:       An array of EGI_UNIHANs.
@n:             size/capacity of the array. (use capacity if empty unihans eixst)
-----------------------------------------------------------------------*/
void UniHan_insertSort_wcode( EGI_UNIHAN* unihans, int n )
{
        int i;          /* To tranverse elements in array, 1 --> n-1 */
        int k;          /* To decrease, k --> 1,  */
        EGI_UNIHAN tmp;

	if(unihans==NULL)
		return;

	/* Start sorting ONLY when i>1 */
        for( i=1; i<n; i++) {
                tmp=unihans[i];

        	/* Slide the inseting integer left, until to the first smaller unihan  */
                for( k=i; k>0 && unihans[k-1].wcode > tmp.wcode; k--)
				unihans[k]=unihans[k-1];   /* swap */

		/* Settle the inserting unihan at last swapped place */
                unihans[k]=tmp;
        }
}


/*--------------------------------------------------------------------
Sort an EGI_UNIHAN array by Quick_Sort algorithm, to rearrange unihans
with wcode (as KEY) in ascending order.

Refert to mat_quick_sort() in egi_math.h.

 	!!! WARNING !!! This is a recursive function.

Note:
1. The caller MUST ensure start/end are within the legal range.


@start:		start index, as of unihans[start]
@End:		end index, as of unihans[end]
@cutoff:	cutoff value for switch to insert_sort.

Return:
	0	Ok
	<0	Fails
----------------------------------------------------------------------*/
int UniHan_quickSort_wcode(EGI_UNIHAN* unihans, unsigned int start, unsigned int end, int cutoff)
{
	int i=start;
	int j=end;
	int mid;
	EGI_UNIHAN	tmp;
	EGI_UNIHAN	pivot;

	/* Check input */
	if( unihans==NULL )
		return -1;

	/* End sorting */
	if( start >= end )
		return 0;

        /* Limit cutoff */
        if(cutoff<3)
                cutoff=3;

/* 1. Implement quicksort */
        if( end-start >= cutoff )
        {
        /* 1.1 Select pivot, by sorting array[start], array[mid], array[end] */
                /* Get mid index */
                mid=(start+end)/2;

                /* Sort [start] and [mid] */
                /* if( array[start] > array[mid] ) */
		if( unihans[start].wcode > unihans[mid].wcode ) {
                        tmp=unihans[start];
                        unihans[start]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* Now: [start]<=array[mid] */

                /* IF: [mid] >= [start] > [end] */
                /* if( array[start] > array[end] ) { */
		if( unihans[start].wcode > unihans[end].wcode ) {
                        tmp=unihans[start]; /* [start] is center */
                        unihans[start]=unihans[end];
                        unihans[end]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* ELSE:   [start]<=[mid] AND [start]<=[end] */
                /* else if( array[mid] > array[end] ) { */
		if( unihans[mid].wcode > unihans[end].wcode ) {
                        /* If: [start]<=[end]<[mid] */
                                tmp=unihans[end];   /* [end] is center */
                                unihans[end]=unihans[mid];
                                unihans[mid]=tmp;
                        /* Else: [start]<=[mid]<=[end] */
                }
                /* Now: array[start] <= array[mid] <= array[end] */

                pivot=unihans[mid];
                //printf("After_3_sort: %d, %d, %d\n", array[start], array[mid], array[end]);

                /* Swap array[mid] and array[end-1], still keep array[start] <= array[mid] <= array[end]!   */
                tmp=unihans[end-1];
                unihans[end-1]=unihans[mid];   /* !NOW, we set memeber [end-1] as pivot */
                unihans[mid]=tmp;

        /* 1.2 Quick sort: array[start] ... array[end-1], array[end]  */
                i=start;
                j=end-1;   /* As already sorted to: array[start]<=pivot<=array[end-1]<=array[end], see 1. above */
                for(;;)
                {
                        /* Stop at array[i]>=pivot: We preset array[end-1]==pivot as sentinel, so i will stop at [end-1]  */
                        /* while( array[++i] < pivot ){ };   Acturally: array[++i] < array[end-1] which is the pivot memeber */
			while( unihans[++i].wcode < pivot.wcode ) { };
                        /* Stop at array[j]<=pivot: We preset array[start]<=pivot as sentinel, so j will stop at [start]  */
                        /* while( array[--j] > pivot ){ }; */
			while( unihans[--j].wcode > pivot.wcode ) { };

                        if( i<j ) {
                                /* Swap array[i] and array[j] */
                                tmp=unihans[i];
                                unihans[i]=unihans[j];
                                unihans[j]=tmp;
                        }
                        else {
                                break;
			}
		}
                /* Swap pivot memeber array[end-1] with array[i], we left this step at last.  */
                unihans[end-1]=unihans[i];
                unihans[i]=pivot; /* Same as array[i]=array[end-1] */

        /* 1.3 Quick sort: recursive call for sorted parts. and leave array[i] as mid of the two parts */
		UniHan_quickSort_wcode(unihans, start, i-1, cutoff);
		UniHan_quickSort_wcode(unihans, i+1, end, cutoff);

        }
/* 2. Implement insertsort */
        else
		UniHan_insertSort_wcode( unihans+start, end-start+1);

	return 0;
}


/*-------------------------------------------------------------------
Save a EGI_UNIHAN_SET to a file.

NOTE:
1. If the file exists, it will be truncated to zero length first.
2. The EGI_UNIHAN struct MUST be packed type, and MUST NOT include any
   pointers.

@fpath:		File path.
@set: 		Pointer to EGI_UNIHAN_SET.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------*/
int UniHan_save_uniset(const char *fpath,  const EGI_UNIHAN_SET *uniset)
{
	int i;
	int nwrite;
	int nmemb;
	FILE *fil;
	uint8_t nq;
	int ret=0;
	size_t size;		/* Total number of unihans in the uniset */
	EGI_UNIHAN *unihans;

	if( uniset==NULL )
		return -1;

	/* Get uniset memebers */
	unihans=uniset->unihans;
	size=uniset->size;

	/* Open/create file */
        fil=fopen(fpath, "wb");
        if(fil==NULL) {
                printf("%s: Fail to open file '%s' for write. ERR:%s\n", __func__, fpath, strerror(errno));
                return -2;
        }

	/* FWrite total number of EGI_UNIHANs */
	nmemb=1;
	for( i=0; i<4; i++) {
		nq=(size>>(i<<3))&0xFF;			/* Split n to bytes, The least significant byte first. */
		nwrite=fwrite( &nq, 1, nmemb, fil);  	/* 1 byte each time */
	        if(nwrite < nmemb) {
	                if(ferror(fil))
        	                printf("%s: Fail to write number of EGI_UNIHANs to '%s'.\n", __func__, fpath);
                	else
                        	printf("%s: WARNING! fwrite number of EGI_UNIHANs %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
                	ret=-3;
			goto END_FUNC;
		}
        }

	/* FWrite uniset name */
	nmemb=sizeof(uniset->name);
	//printf("nmemb=sizeof((uniset->name))=%d\n",nmemb);
	nwrite=fwrite( uniset->name, 1, nmemb, fil);
        if(nwrite < nmemb) {
        	if(ferror(fil))
                	printf("%s: Fail to write uniset->name '%s'.\n", __func__, uniset->name);
                else
                        printf("%s: WARNING! fwrite number of uniset->name %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
                ret=-4;
                goto END_FUNC;
        }

	/* FWrite all EGI_UNIHANs */
	nmemb=sizeof(EGI_UNIHAN);
	for( i=0; i<size; i++) {
		nwrite=fwrite( unihans+i, 1, nmemb, fil);
	        if(nwrite < nmemb) {
	                if(ferror(fil))
        	                printf("%s: Fail to write EGI_UNIHANs to '%s'.\n", __func__, fpath);
                	else
                        	printf("%s: WARNING! fwrite EGI_UNIHANs  %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
                	ret=-5;
			goto END_FUNC;
		}
	}


END_FUNC:
        /* Close fil */
        if( fclose(fil) !=0 ) {
                printf("%s: Fail to close file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
                ret=-5;
        }

	return ret;
}


/*------------------------------------------------------------------------------
Read EGI_UNIHANs from a file and load them to an EGI_UNIHAN_SET.

@fpath:		File path.
@set: 		A pointer to EGI_UNIHAN_SET.
		The caller MUST ensure enough space to hold all
		readin data.  !!! WARNING !!!

Return:
	A pointer to EGI_UNIHAN_SET	OK, however its data may not be complete
					if fail happens during file reading.
	NULL				Fails
-----------------------------------------------------------------------------*/
EGI_UNIHAN_SET* UniHan_load_uniset(const char *fpath)
{
	EGI_UNIHAN_SET *uniset=NULL;
	int i;
	int nread;
	int nmemb;
	FILE *fil;
	uint8_t nq;  		/* quarter of uint32_t */
	uint32_t total=0;	/* total number of EGI_UNIHAN in the file */

	/* Open/create file */
        fil=fopen(fpath, "rb");
        if(fil==NULL) {
                printf("%s: Fail to open file '%s' for read. ERR:%s\n", __func__, fpath, strerror(errno));
                return NULL;
        }

	/* FRead total number of EGI_UNIHANs */
	nmemb=1;
	for( i=0; i<4; i++) {
		nq=0;
		nread=fread( &nq, 1, nmemb, fil);      /* read 1 byte each time */
	        if(nread < nmemb) {
	                if(ferror(fil))
        	                printf("%s: Fail to read %d_th nq from '%s'.\n", __func__, i, fpath);
                	else
                        	printf("%s: WARNING! read %d_th nq, read %d bytes of total %d bytes.\n", __func__, i, nread, nmemb);

			goto END_FUNC;
		}

		total += nq<<(i<<3);			/* Assembly uint8_t to uint32_t, The least significant byte first. */
        }
	printf("%s: Totally %d unihans in file '%s'.\n",__func__, total, fpath);

	/* Create UNISET */
	uniset=UniHan_create_uniset(NULL, total); /* total is capacity, NOT size! */
	if(uniset==NULL) {
		printf("%s: Fail to create uniset!\n", __func__);
		goto END_FUNC;
	}
	/* Assign size */
	uniset->size=total;

        /* FReadin uniset name */
        nmemb=sizeof(uniset->name);
        nread=fread( uniset->name, 1, nmemb, fil);
        if(nread < nmemb) {
                if(ferror(fil))
                        printf("%s: Fail to read uniset name.\n", __func__);
                else
                        printf("%s: WARNING! fread uniset->name %d bytes of total %d bytes.\n", __func__, nread, nmemb);
		UniHan_free_uniset(&uniset);
                goto END_FUNC;
        }
	/* Whatever, set EOF */
	uniset->name[sizeof(uniset->name)-1]='\0';

	/* FReadin all EGI_UNIHANs */
	nmemb=sizeof(EGI_UNIHAN);
	for( i=0; i<total; i++) {
		nread=fread( uniset->unihans+i, 1, nmemb, fil);
	        if(nread < nmemb) {
	                if(ferror(fil))
        	                printf("%s: Fail to read %d_th EGI_UNIHAN from '%s'.\n", __func__, i, fpath);
                	else
                        	printf("%s: WARNING! fread %d_th EGI_UNIHAN,  %d bytes of total %d bytes.\n", __func__, i, nread, nmemb);

			//UniHan_free_uniset(&uniset);
			//Continue anyway .... //goto END_FUNC;
		}

		/* TEST: --- */
		printf("Load [%d]: wcode:%d, typing:%s, reading:%s \n",
					i, uniset->unihans[i].wcode, uniset->unihans[i].typing, uniset->unihans[i].reading);
	}

END_FUNC:
        /* Close fil */
        if( fclose(fil) !=0 ) {
                printf("%s: Fail to close file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
        }

	return uniset;
}


/*-----------------------------------------------------------------
Examine each UNIHAN in the input text and try to update/poll the
corresponding uniset->unihans[].freq accordingly. Each appearance
of the UNIHAN in the text will add 1 more value to its frequency
weigh. ONLY UNIHANs available in the uniset will be conidered.

Note:
1. The uniset MUST be prearranged in ascending order of wcodes.
2. All ployphonic UNIHANs bearing the same wcode(UNICODE) will be
   updated respectively.
			   Example:
Uniset->unihans[7387]:家, wcode:U+5BB6	reading:gū	typing:gu      freq:3934
Uniset->unihans[7388]:家, wcode:U+5BB6	reading:jia	typing:jia     freq:3934
Uniset->unihans[7389]:家, wcode:U+5BB6	reading:jià	typing:jia     freq:3934
Uniset->unihans[7390]:家, wcode:U+5BB6	reading:jie	typing:jie     freq:3934

@uniset:	A pointer to an EGI_UNIHAN_SET, with unihans in
		ascending order of wcodes.
@fpath:		Full path of a text file.
		The text MUST be UFT-8 encoding.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------*/
int UniHan_poll_freq(EGI_UNIHAN_SET *uniset, const char *fpath)
{
	int		ret;
        int             fd;
        int             fsize=-1;  /* AS token */
	int		chsize;
	struct stat     sb;
        char *          fp=NULL;
	char *		p;
	wchar_t		wcode;

	/* Check input */
	if(uniset==NULL || uniset->unihans==NULL || uniset->size==0) /* If empty */
		return -1;

        /* Open text file */
        fd=open(fpath, O_RDONLY, S_IRUSR);
        if(fd<0) {
                printf("%s: Fail to open input file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
                return -2;
        }

        /* Obtain file stat */
        if( fstat(fd,&sb)<0 ) {
                printf("%s: Fail call fstat for file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
		ret=-3;
		goto END_FUNC;
        }
        fsize=sb.st_size;

        /* MMAP Text file */
        if(fsize >0) {
                fp=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
                if(fp==MAP_FAILED) {
                        printf("%s: Fail to mmap file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
			ret=-4;
			goto END_FUNC;
                }
        }

	/* Count freqency */
	p=fp;
	while(*p) {
                /* Convert UFT-8 encoding to UNICODE, return size of uft-8 code. */
                chsize=char_uft8_to_unicode((const UFT8_PCHAR)p, &wcode);

                if(chsize>0) {
			/* Increase frequency weigh of the UNIHAN */
			if( UniHan_locate_wcode(uniset, wcode)==0 ) {
				/* All ployphonic UNIHANs bearing the same wcode will be updated */
				while( uniset->unihans[uniset->puh].wcode == wcode ) {
					uniset->unihans[uniset->puh].freq +=1;
					uniset->puh++;
				}
			}
			#if 1 /* TEST ---- */
			else {
				char pch[8];
				bzero(pch, sizeof(pch));
				if( UNICODE_PRC_START<=wcode && wcode<=UNICODE_PRC_END) {
					printf("%s: WARNING %s U+%04x NOT found in uniset!\n", __func__,
							char_unicode_to_uft8(&wcode, pch)>0 ? pch : " ", wcode );
				}
			}
			#endif

			/* move on... */
                        p+=chsize;
                }
                else {
                        printf("%s: Unrecoginzable unicode, breakl!\n",__func__);
			ret=-5;
			goto END_FUNC;
                }
	}


END_FUNC:
        /* Munmap file */
        if( fsize>0 && fp!=MAP_FAILED ) {
		if(munmap(fp,fsize) !=0 )
                	printf("%s: Fail to munmap file '%s'. ERR:%s!\n",__func__, fpath, strerror(errno));
	}

	/* Close File */
        if( close(fd) !=0 )
                printf("%s: Fail to close file '%s'. ERR:%s!\n",__func__, fpath, strerror(errno));

        return ret;
}



/*-----------------------------------------------------------------------------
Convert a reading (pronunciation in written form) to a PINYIN typing ( keyboard
input sequence).

@reading:  A pointer to a reading, with uft-8 encoding.
	   example:  "tiě"
@pinyin:   A pointer to a pinyin, in ASCII chars.
	   example:  "tie3"

NOTE:
1. The caller MUST ensure enough space for a pinyin.
2. It will fail to convert following readings with Combining Diacritical Marks:
 	ê̄  [0xea  0x304]  ê̌ [0xea  0x30c]
        ḿ  [0x1e3f  0x20] m̀=[0x6d  0x300]

Return:
	0	OK
	<0	Fails
------------------------------------------------------------------------------*/
int UniHan_reading_to_pinyin( const UFT8_PCHAR reading, char *pinyin)
{
	wchar_t	wcode;
	int k;   	/* index of pinyin[] */
	int size;
	char *p=(char *)reading;
	char tone='\0';	/* Default is NO tone number in unihan.reading, as for SOFT TONE */

	k=0;
	while(*p)
	{
		/* Clear the byte */
		pinyin[k]='\0';


		size=char_uft8_to_unicode( (const UFT8_PCHAR)p, &wcode);

#if 0		/* 1. ----- OPTION: A vowel char: Get pronunciation tone ----- */
		/* NOTE: If option disabled: there will be repeated unihans in the uniset */
		if( size>1 && tone != '\0' ) {   /*  Check tone, to avoid Combining (Diacritical) Marks  */

			switch( wcode )
			{
				case 0x00FC:	/* ü */
					tone='0';
					break;

				case 0x0101:  	/* ā */
				case 0x014D:  	/* ō */
				case 0x0113:  	/* ē */
				case 0x012B:	/* ī */
				case 0x016B:	/* ū */
				case 0x01D6:	/* ǖ */
					tone='1';
					break;

				case 0x00E1:  	/* á */
				case 0x00F3:  	/* ó */
				case 0x00E9:  	/* é */
				case 0x00ED:	/* í */
				case 0x00FA:	/* ú */
				case 0x01D8:	/* ǘ */
				case 0x0144:	/* ń */
				case 0x1E3F:	/* ḿ  */
					tone='2';
					break;

				case 0x01CE:  	/* ǎ */
				case 0x01D2:  	/* ǒ */
				case 0x011B:  	/* ě */
				case 0x01D0:	/* ǐ */
				case 0x01D4:	/* ǔ */
				case 0x01DA:	/* ǚ */
				case 0x0148:	/* ň */
					tone='3';
					break;

				case 0x00E0:  	/* à */
				case 0x00F2:  	/* ò */
				case 0x00E8:  	/* è */
				case 0x00EC:	/* ì */
				case 0x00F9:	/* ù */
				case 0x01DC:	/* ǜ */
				case 0x01F9:	/* ǹ */
				case 0x006D:	/* m̀ [0x6d  0x300] */
					tone='4';
					break;

				case 0x00EA:  	/* ê */
					tone='5';
					break;

				default:
					tone='\0';	/* as EOF */
					break;
			}
		}
#endif

		/* 2. -----A vowel char or ASCII char:  Replace vowel with ASCII char ----- */
		/* NOTE: If NO tone number, then it's   */
		if(size >= 1) {

			switch( wcode )
			{
				case 0x0101:  	/* ā */
				case 0x00E1:  	/* á */
				case 0x01CE:  	/* ǎ */
				case 0x00E0:  	/* à */
					pinyin[k]='a';
					break;

				case 0x014D:  	/* ō */
				case 0x00F3:  	/* ó */
				case 0x01D2:  	/* ǒ */
				case 0x00F2:  	/* ò */
					pinyin[k]='o';
					break;

				case 0x0113:  	/* ē */
				case 0x00E9:  	/* é */
				case 0x011B:  	/* ě */
				case 0x00E8:  	/* è */
				case 0x00EA:  	/* ê */
						/* ê̄ [0xEA  0x304] */
				case 0x1EBF:	/* ế [0x1ebf] */
						/* ê̌ [0xEA  0x30c] */
				case 0x1EC1:	/* ề [0x1ec1] */
					pinyin[k]='e';
					break;

				case 0x012B:	/* ī */
				case 0x00ED:	/* í */
				case 0x01D0:	/* ǐ */
				case 0x00EC:	/* ì */
					pinyin[k]='i';
					break;

				case 0x016B:	/* ū */
				case 0x00FA:	/* ú */
				case 0x01D4:	/* ǔ */
				case 0x00F9:	/* ù */
					pinyin[k]='u';
					break;

				case 0x00FC:	/* ü */
				case 0x01D6:	/* ǖ */
				case 0x01D8:	/* ǘ */
				case 0x01DA:	/* ǚ */
				case 0x01DC:	/* ǜ */
					pinyin[k]='v';
					break;

				case 0x0144:	/* ń */
				case 0x0148:	/* ň */
				case 0x01F9:	/* ǹ */
					pinyin[k]='n';
					break;

				case 0x1E3F:	/* ḿ  [0x1e3f  0x20] */
				case 0x006D:	/* m̀ [0x6d  0x300] */
					pinyin[k]='m';
					break;

				/* Default: An ASCII char OR ingore */
				default:
					if( size==1 && isalpha(wcode) ) {
						//printf("wcode=%d\n", wcode);
						pinyin[k]=wcode;
					}
					/* Else: Ignore some Combining Diacritical Marks */
					break;
			}
		}

		/* 3. ----- Unrecoginzable char ----- */
		else  /* size<1 */
		{
			printf("%s: Unregconizable unicode!\n",__func__);
			return -1;
		}

		/* 4. Move p */
		p+=size;

		/* 5. Increase k as index for next pinyin[] */
		if( pinyin[k] != '\0' ) /* Only if NOT empty */
			k++;
	}

	/* Put tone at the end */
	pinyin[k]=tone;

	return 0;
}


/*-------------------------------------------------------------------
Read a kHanyuPinyin text file, and load data to an EGI_UNIHAN_SET.
Each line in the file presents stricly in the same form as:
        ... ...
        U+92E7  kHanyuPinyin    64207.100:xiàn
        U+92E8  kHanyuPinyin    64208.110:tiě,é
        ... ....

			---- WARNING ----

Some UNIHANs are NOT in kHanyuPinyin category:
Examp: 说这宝贾们见毙...

	Example "毙"
U+6BD9	kCantonese	bai6
U+6BD9	kDefinition	kill; die violent death
U+6BD9	kHanyuPinlu	bì(17)
U+6BD9	kMandarin	bì
U+6BD9	kTGHZ2013	019.020:bì
U+6BD9	kXHC1983	0060.160:bì

Note:
1. The HanyuPinyin text MUST be extracted from Unihan_Readings.txt,
   The Unihan_Readings.txt is included in Unihan.zip:
   http://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip

2. The text file can be generated by follow shell command:
   cat Unihan_Readings.txt | grep kHanyuPinyin > kHanyuPinyin.txt

3. To pick out some unicodes with strange written reading.

   3.1---- Some readings are with Combining Diacritical Marks:
   	ế =[0x1ebf]  ê̄ =[0xea  0x304]  ê̌ =[0xea  0x30c] ề =[0x1ec1]
	ḿ =[0x1e3f  0x20]  m̀=[0x6d  0x300]

   Pick out  ê̄,ế,ê̌,ề  manually first! Or it will bring strange PINYIN.
	U+6B38	kHanyuPinyin	32140.110:āi,ǎi,xiè,ế,éi,ê̌,ěi,ề,èi,ê̄
	U+8A92	kHanyuPinyin	63980.140:xī,yì,ê̄,ế,éi,ê̌,ěi,ề,èi

   Pick out ḿ
	U+5463	kHanyuPinyin	10610.080:móu,ḿ,m̀
	U+5514	kHanyuPinyin	10627.020:wù,wú,ńg,ḿ

   3.2----- Same unihans found in 2 pages: (Keep, but words after SPACE will be discarded!!! ).
   	U+5448	kHanyuPinyin	10585.010:kuáng 10589.110:chéng,chěng
   	U+9848	kHanyuPinyin	53449.060:xuǎn,jiōng 74379.120:jiǒng,xiàn,xuǎn,jiōng
	... ...
	many ... ...

4. The return EGI_UNIHAN_SET usually contains some empty unihans, as
   left unused after malloc.

Return:
	A pointer to EGI_UNIHAN_SET	Ok
	NULL				Fail
----------------------------------------------------------------------*/
EGI_UNIHAN_SET *UniHan_load_HanyuPinyinTxt(const char *fpath)
{

	EGI_UNIHAN_SET	*uniset=NULL;
	size_t  capacity;			/* init. capacity=growsize */
	size_t  growsize=1024;		/* memgrow size for uniset->unihans[], also inital for size */

	FILE *fil;
	char linebuff[1024];			/* Readin line buffer */
	unsigned int  Word_Max_Len=256;		/* Max. length of column words in each line of kHanyuPinyin.txt,  including '\0'. */
	unsigned int  Max_Split_Words=4;  	/* Max number of words in linebuff[] as splitted by delimters */
	/*----------------------------------------------------------------------------------
	   To store words in linebuff[] as splitted by delimter.
	   Example:      linefbuff:  "U+92FA	kHanyuPinyin	64225.040:yuǎn,yuān,wǎn,wān"
			str_words[0]:	U+92FA
			str_words[1]:	kHanyuPinyin
			str_words[2]:	64225.040
			str_words[3]:	yuǎn,yuān,wǎn,wān

	Note: Many execptional cases, words after SPACE to be discarded.
	  Example:
	   	U+5448	kHanyuPinyin	10585.010:kuáng 10589.110:chéng,chěng
					"10589.110:chéng,chěng" will be discarded.
		U+9848  kHanyuPinyin    53449.060:xuǎn,jiōng 74379.120:jiǒng,xiàn,xuǎn,jiōng
					"74379.120:jiǒng,xiàn,xuǎn,jiōng" will be discarded.
	------------------------------------------------------------------------------------*/
	char str_words[Max_Split_Words][Word_Max_Len]; /* See above */
	char *delim="	 :\n"; 		/* delimiters: TAB,SPACE,':','\n'.  for splitting linebuff[] into 4 words. see above. */
	int  m;
	char *pt=NULL;
	int  k;
	wchar_t wcode;
	char pch[4];

	/* Open kHanyuPinyin file */
	fil=fopen(fpath,"r");
	if(fil==NULL) {
		printf("%s: Fail to open '%s', ERR:%s.\n",__func__, fpath,  strerror(errno));
		return NULL;
	}
	else
		printf("%s: Open '%s' to readn and load unihan set...\n", __func__, fpath);

	/* Allocate uniset */
	capacity=growsize;
	uniset=UniHan_create_uniset("kHanyuPinyin", capacity);
	if(uniset==NULL) {
		printf("%s: Fail to create uniset!\n", __func__);
		return NULL;
	}

	/* Loop read in and parse to store to EGI_UNIHAN_SET */
	k=0;
	while ( fgets(linebuff, sizeof(linebuff), fil) != NULL )
	{
		//printf("%s: fgets line '%s'\n", __func__, linebuff);
		/* Separate linebuff[] as per given delimiter
		 * linebuff: "U+92FA     kHanyuPinyin    64225.040:yuǎn,yuān,wǎn,wān"
		 * Note: content in linebuff[] is modified after parsing!
		 */
		pt=strtok(linebuff, delim);
		for(m=0; pt!=NULL && m<Max_Split_Words; m++) {  /* Separate linebuff into 4 words */
			bzero(str_words[m], sizeof(str_words[0]));
			snprintf( str_words[m], Word_Max_Len-1, "%s", pt);
			pt=strtok(NULL, delim);
		}
		if(m<3) {
			printf("Not a complete line!\n");
			continue;
		}
		//printf(" str_words[0]: %s, [1]:%s, [2]:%s \n",str_words[0], str_words[1], str_words[2]);

		/* --- 1. Parse wcode --- */
		/* Convert str_wcode to wcode */
		sscanf(str_words[0]+2, "%x", &wcode); /* +2 bytes to escape 'U+' in "U+3404" */
		/* Assign unihans[k].wcode */
		if( wcode < UNICODE_PRC_START || wcode > UNICODE_PRC_END )
			continue;
		/* --- 2. Parse reading and convert to pinyin --- */
		/* Note: Split str_words[3] into several PINYINs,and store them in separated uniset->unihans
		 * and store them in separated uniset->unihans
		 */
		//printf("str_words[3]: %s\n",str_words[3]);
		pt=strtok(str_words[3], ",");
		/* At lease one pinyin in most case:  " 74801.200:jì " */
		if(pt==NULL)
			pt=str_words[3];

		/* If more than one PINYIN, store them in separated uniset->unihans with same wcode */
		while( pt != NULL ) {
		/* ---- 3. Save unihans[] memebers ---- */

			/* 1. Assign wcode */
			uniset->unihans[k].wcode=wcode;
			/* 2. Assign reading */
			strncpy(uniset->unihans[k].reading, pt, UNIHAN_READING_MAXLEN-1);
			/* 3. Convert reading to PINYIN typing and assign */
			UniHan_reading_to_pinyin( (UFT8_PCHAR)uniset->unihans[k].reading, uniset->unihans[k].typing);

		/* --- DEBUG --- */
		#if 0
		 if( wcode==0x54B9 ) {
			bzero(pch,sizeof(pch));
			EGI_PDEBUG(DBG_UNIHAN,"unihans[%d]:%s, wcode:U+%04x, reading:%s, pinying:%s\n",
						k, char_unicode_to_uft8(&wcode, pch)>0?pch:" ",
						uniset->unihans[k].wcode, uniset->unihans[k].reading, uniset->unihans[k].typing );
		 }
		#endif
			/* 4. Count k as total number of unihans loaded to uniset. */
			k++;

			/* Check space capacity, and memgrow uniset->unihans[] */
			if( k==capacity ) {
				if( egi_mem_grow( (void **)&uniset->unihans, capacity*sizeof(EGI_UNIHAN), growsize*sizeof(EGI_UNIHAN)) != 0 ) {
					printf("%s: Fail to mem grow uniset->unihans!\n", __func__);
					UniHan_free_uniset(&uniset);
					goto END_FUNC;
				}
				capacity += growsize;
				printf("%s: uniset->unihans mem grow from %d to %d.\n", __func__, capacity-growsize, capacity);
			}

			/* Get next split pointer */
			pt=strtok(NULL, ",");

		}  /* End while(pasre line) */

	} /* End while(Read in lines) */

	/* Finally, assign total size */
	uniset->size=k;
	uniset->capacity=capacity;

END_FUNC:
	/* close FILE */
	if(fclose(fil)!=0)
		printf("%s: Fail to close '%s'.\n",__func__, fpath);

	return uniset;
}


/*----------------------------------------------------------------------
Read a kMandarin text file, and load data to an EGI_UNIHAN_SET.
Each line in the file presents stricly in the same form as:
        ... ...
	U+3416	kMandarin	xié
	U+343C	kMandarin	chèng
        ... ....

Note:
1. The Mandarin text MUST be extracted from Unihan_Readings.txt,
   The Unihan_Readings.txt is included in Unihan.zip:
   http://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip

2. The text file can be generated by follow shell command:
   cat Unihan_Readings.txt | grep kMandarin > Mandarin.txt

3. To pick out some unicodes with strange written reading.
   Some readings are with Combining Diacritical Marks:
	ḿ =[0x1e3f  0x20]  m̀=[0x6d  0x300]

   Pick out ḿ
	U+5463	kMandarin	ḿ

4. The return EGI_UNIHAN_SET usually contains some empty unihans, as left
   unused after malloc.


Return:
	A pointer to EGI_UNIHAN_SET	Ok
	NULL				Fail
----------------------------------------------------------------------*/
EGI_UNIHAN_SET *UniHan_load_MandarinTxt(const char *fpath)
{

	EGI_UNIHAN_SET	*uniset=NULL;
	size_t  capacity;			/* init. capacity=growsize */
	size_t  growsize=1024;		/* memgrow size for uniset->unihans[], also inital for size */

	FILE *fil;
	char linebuff[256];			/* Readin line buffer */
	unsigned int  Word_Max_Len=64;		/* Max. length of column words in each line of kMardrin.txt,  including '\0'. */
	unsigned int  Max_Split_Words=3;  	/* Max number of words in linebuff[] as splitted by delimters */
	/*---------------------------------------------------------------
	   To store words in linebuff[] as splitted by delimter.
	   Example:      linefbuff:  "U+342E  kMandarin       xiāng"
			str_words[0]:	U+342E
			str_words[1]:	kMandarin
			str_words[2]:	xiāng
	----------------------------------------------------------------*/
	char str_words[Max_Split_Words][Word_Max_Len]; /* See above */
	char *delim="	 \n"; 		/* delimiters: TAB,SPACE,'\n'.  for splitting linebuff[] into 3 words. see above. */
	int  m;
	char *pt=NULL;
	int  k;
	wchar_t wcode;
	char pch[4];

	/* Open kHanyuPinyin file */
	fil=fopen(fpath,"r");
	if(fil==NULL) {
		printf("%s: Fail to open '%s', ERR:%s.\n",__func__, fpath,  strerror(errno));
		return NULL;
	}
	else
		printf("%s: Open '%s' to readn and load unihan set...\n", __func__, fpath);

	/* Allocate uniset */
	capacity=growsize;
	uniset=UniHan_create_uniset("kMandarin", capacity);
	if(uniset==NULL) {
		printf("%s: Fail to create uniset!\n", __func__);
		return NULL;
	}

	/* Loop read in and parse to store to EGI_UNIHAN_SET */
	k=0;
	while ( fgets(linebuff, sizeof(linebuff), fil) != NULL )
	{
		//printf("%s: fgets line '%s'\n", __func__, linebuff);
		/* Separate linebuff[] as per given delimiter
		 * linefbuff:  "U+342E  kMandarin       xiāng"
		 * Note: content in linebuff[] is modified after parsing!
		 */
		pt=strtok(linebuff, delim);
		for(m=0; pt!=NULL && m<Max_Split_Words; m++) {  /* Separate linebuff into 4 words */
			bzero(str_words[m], sizeof(str_words[0]));
			snprintf( str_words[m], Word_Max_Len-1, "%s", pt);
			pt=strtok(NULL, delim);
		}
		if(m<2) {
			printf("Not a complete line!\n");
			continue;
		}
		//printf(" str_words[0]: %s, [1]:%s, [2]:%s \n",str_words[0], str_words[1], str_words[2]);

		/* --- 1. Parse wcode --- */
		/* Convert str_wcode to wcode */
		sscanf(str_words[0]+2, "%x", &wcode); /* +2 bytes to escape 'U+' in "U+3404" */
		/* Assign unihans[k].wcode */
		if( wcode < UNICODE_PRC_START || wcode > UNICODE_PRC_END )
			continue;

		/* --- 2. Parse reading and convert to pinyin --- */
		/* NOTE:
		 * 1.Split str_words[2] into several PINYINs,and store them in separated uniset->unihans
		 *   and store them in separated uniset->unihans
		 * 2.Currently, Each row in kMandarin has ONLY one typing, so delimiter "," does NOT exist at all!
		 */
		//printf("str_words[2]: %s\n",str_words[2]);
		pt=strtok(str_words[2], ",");

		/* NOW ONLY one typing: "U+8FE2	kMandarin	tiáo"  */
		if(pt==NULL)
			pt=str_words[2];

		/* If more than one PINYIN, store them in separated uniset->unihans with same wcode */
		while( pt != NULL ) {
		/* ---- 3. Save unihans[] memebers ---- */

			/* 1. Assign wcode */
			uniset->unihans[k].wcode=wcode;
			/* 2. Assign reading */
			strncpy(uniset->unihans[k].reading, pt, UNIHAN_READING_MAXLEN-1);
			/* 3. Convert reading to PINYIN typing and assign */
			UniHan_reading_to_pinyin( (UFT8_PCHAR)uniset->unihans[k].reading, uniset->unihans[k].typing);

		/* --- DEBUG --- */
		#if 0
		 if( wcode==0x9E23 ) {
			bzero(pch,sizeof(pch));
			EGI_PDEBUG(DBG_UNIHAN,"unihans[%d]:%s, wcode:U+%04x, reading:%s, pinying:%s\n",
						k, char_unicode_to_uft8(&wcode, pch)>0?pch:" ",
						uniset->unihans[k].wcode, uniset->unihans[k].reading, uniset->unihans[k].typing );
			exit(1);
		 }
		#endif

			/* 4. Count k as total number of unihans loaded to uniset. */
			k++;

			/* Check space capacity, and memgrow uniset->unihans[] */
			if( k==capacity ) {
				if( egi_mem_grow( (void **)&uniset->unihans, capacity*sizeof(EGI_UNIHAN), growsize*sizeof(EGI_UNIHAN)) != 0 ) {
					printf("%s: Fail to mem grow uniset->unihans!\n", __func__);
					UniHan_free_uniset(&uniset);
					goto END_FUNC;
				}
				capacity += growsize;
				printf("%s: uniset->unihans mem grow from %d to %d.\n", __func__, capacity-growsize, capacity);
			}

			/* Get next split pointer */
			pt=strtok(NULL, ",");

		}  /* End while(parse line) */

	} /* End while(Read in lines) */

	/* Finally, assign total size */
	uniset->size=k;
	uniset->capacity=capacity;

END_FUNC:
	/* close FILE */
	if(fclose(fil)!=0)
		printf("%s: Fail to close '%s'.\n",__func__, fpath);

	return uniset;
}



/*-----------------------------------------------------------------------
To locate the index of first unihan[] bearing the given wcode, and set
uniset->puh as the index.

Note:
1. The uniset MUST be prearranged in ascending order of wcodes.
2. Usually there are more than one unihans that have the save wcode, as
one UNIHAN may have several typings, and indexse of those unihans
should be consecutive in the uniset (see 1). Finally the first index will
be located.

@uniset		The target EGI_UNIHAN_SET.
@wcode		The wanted UNICODE.

Return:
	=0		OK, set uniset->puh accordingly.
	<0		Fails, or no result.
-----------------------------------------------------------------------*/
int UniHan_locate_wcode(EGI_UNIHAN_SET* uniset, wchar_t wcode)
{
	unsigned int start,end,mid;
	EGI_UNIHAN *unihans=NULL;

	if(uniset==NULL || uniset->unihans==NULL || uniset->size==0) /* If empty */
		return -1;

	/* Get pointer to unihans */
	unihans=uniset->unihans;

	/* Mark start/end/mid of unihans index */
	start=0;
	end=uniset->capacity-1;  /* NOT uniset->size!! Consider there may be some empty unihans at beginning!  */
	mid=(start+end)/2;

	/* binary search for the wcode  */
	while( unihans[mid].wcode != wcode ) {

		/* check if searched all wcodes */
		if(start==end)
			return -2;
		/* !!! If mid+1 == end: then mid=(n+(n+1))/2 = n, and mid will never increase to n+1! */
		else if( start==mid && mid+1==end ) {	/* start != 0 */
			if( unihans[mid+1].wcode==wcode ) {
				mid += 1;  /* Let mid be the index of unihans, see following... */
				break;
			}
			else
				return -3;
		}

		if( unihans[mid].wcode > wcode ) {   /* then search upper half */
			end=mid;
			mid=(start+end)/2;
			continue;
		}
		else if( unihans[mid].wcode < wcode) {
			start=mid;
			mid=(start+end)/2;
			continue;
		}
	}
	/* Finally unihans[mid].wcode==wcode  */

        /* NOW move up to locate the first unihan bearing the same wcode. */
	while( mid>0 && unihans[mid-1].wcode==wcode ){ mid--; };
	uniset->puh=mid;  /* assign to uniset->puh */

	return 0;
}


/*------------------------------------------------------------------------------
To locate the index of first unihan[] containing the given typing in the
beginning of its typing. and set uniset->puh as the index.

			!!! IMPORTANT !!!
"Containing the give typing" means unihan[].typing is SAME as then given
typing, OR the given typing are contained in beginning of unihan[].typing.

This criteria is ONLY based on PINYIN. Other types of typing may NOT comply.

Note:
1. The uniset MUST be prearranged in dictionary ascending order of typing.
2. Pronunciation tones of PINYIN are neglected.
3. Usually there are more than one unihans that have the save typing,
(same pronuciation, but differenct UNIHANs), and indexse of those unihans
should be consecutive in the uniset (see 1). Finally the firt index will
be located.
4. This algorithm depends on typing type PINYIN, others MAY NOT work.

@uniset		The target EGI_UNIHAN_SET.
@typing		The wanted typing. it will be trimmed to be UNIHAN_TYPING_MAXLEN
		bytes including '\0'.

Return:
	=0		OK, set uniset->puh accordingly.
	<0		Fails, or no result.
------------------------------------------------------------------------------*/
int UniHan_locate_typing(EGI_UNIHAN_SET* uniset, const char* typing)
{
	unsigned int start,end,mid;
	EGI_UNIHAN *unihans=NULL;
	EGI_UNIHAN  tmphan;

	if(uniset==NULL || uniset->unihans==NULL || uniset->size==0) /* If empty */
		return -1;

	/* Get pointer to unihans */
	unihans=uniset->unihans;

	/* Mark start/end/mid of unihans index */
	start=0;
	end=uniset->size-1;
	mid=(start+end)/2;

	/* binary search for the wcode  */
	/* While if NOT containing the typing string at beginning */
	while( strstr(unihans[mid].typing, typing) != unihans[mid].typing ) {

		/* check if searched all wcodes */
		if(start==end)
			return -2;

		/* !!! If mid+1 == end: then mid=(n+(n+1))/2 = n, and mid will never increase to n+1! */
		else if( start==mid && mid+1==end ) {   /* start !=0 */
			//if( unihans[mid+1].wcode==wcode ) {
			/* Check if given typing are contained in beginning of unihan[mid-1].typing. */
			if( strstr(unihans[mid+1].typing, typing) == unihans[mid].typing ) {
				mid += 1;  /* Let mid be the index of unihans, see following... */
				break;
			}
			else
				return -3;
		}

		/* Setup a tmp. UNIHAN */
		tmphan=unihans[mid];
		strncpy(tmphan.typing, typing, UNIHAN_TYPING_MAXLEN-1);
		tmphan.typing[UNIHAN_TYPING_MAXLEN-1]='\0';  /* Set EOF */

		//if( unihans[mid].wcode > wcode ) {   /* then search upper half */
		if ( UniHan_compare_typing(&unihans[mid], &tmphan)==CMPTYPING_IS_AFTER ) {
			end=mid;
			mid=(start+end)/2;
			continue;
		}
		//else if( unihans[mid].wcode < wcode) {
		if ( UniHan_compare_typing(&unihans[mid], &tmphan)==CMPTYPING_IS_AHEAD ) {
			start=mid;
			mid=(start+end)/2;
			continue;
		}
	}
	/* NOW unhihan[mid] has the same typing */

        /* Move up to locate the first unihan bearing the same typing. */
	/* Check if given typing are contained in beginning of unihan[mid-1].typing. */
	while ( mid>0 && (strstr(unihans[mid-1].typing, typing) == unihans[mid-1].typing) ) { mid--; };
	uniset->puh=mid;  /* assign to uniset->puh */

	return 0;
}


/*-------------------------------------------------------------------------
Merge(Copy UNIHANs of) uniset1 into uniset2, each UNIHAN will be checked and
copied to ONLY IF uniset2 does NOT have such an UNIHAN ( with the same wcode
 AND typing ).

Note:
1. uniset1 MUST be streamlized and has NO repeated unihans inside.
   OR they will all be merged into uniset2.
2. UNIHANs are just added to the end of uniset2->unihans, and will NOT be
   sorted!
3. uniset1 will NOT be released/freed after operation!!!
4. uniset2->unihans will mem_grow if capacity is NOT enough.

@uniset1:	An UNIHAN SET to be emerged.
@uniset2:	The receiving UNIHAN SET.

Return:
	0	Ok
	<0	Fail
--------------------------------------------------------------------------*/
int UniHan_merge_uniset(const EGI_UNIHAN_SET* uniset1, EGI_UNIHAN_SET* uniset2)
{
	int i;
	int more=0;	/* more unihans added to uniset2 */

	/* Check input uniset1, check uniset2 in UniHan_quickSort_wcode() */
	if(uniset1==NULL || uniset1->unihans==NULL)
		return -1;

	/* quickSort_wcode uniset2, before calling UniHan_locate_wcode(). */
	if( UniHan_quickSort_wcode(uniset2->unihans, 0, uniset2->size-1, 10) != 0 ) {
		printf("%s: Fail to quickSort_wcode!\n",__func__);
		return -2;
	}

	/* Copy unihans in uniset1 to uniset2 */
	for(i=0; i < uniset1->size; i++)
	{
		/* If found same wocde in uniset2, uniset2->size NOT changed in all for() loops! */
		if( UniHan_locate_wcode(uniset2, uniset1->unihans[i].wcode)==0 ) {
			/* Check if has same typing */
			while( strncmp( uniset2->unihans[uniset2->puh].typing, uniset1->unihans[i].typing, UNIHAN_TYPING_MAXLEN ) != 0 )
			{
				uniset2->puh++;
				if(uniset2->puh == uniset2->size)
					break;
			}
			/* Found same typing, skip to next. */
			if( uniset2->puh < uniset2->size ) {
				printf("wcode %d exists.\n", uniset1->unihans[i].wcode);
				continue;
			}
		}

		/* No same wcode in uniset 2 */
		more++; /* count new ones */

		/* Grow mem for unihans */
		if( uniset2->size+more > uniset2->capacity ) {
			if( egi_mem_grow( (void **)&uniset2->unihans, uniset2->capacity*sizeof(EGI_UNIHAN),
								       UNIHANS_GROW_SIZE*sizeof(EGI_UNIHAN)) != 0 )
			{
                        	printf("%s: Fail to mem grow uniset->unihans!\n", __func__);
				uniset2->size += (more-1); /* update final size */
				return -3;
			}
			uniset2->capacity += UNIHANS_GROW_SIZE;
			printf("%s: Mem grow capacity of uniset2->unihans from %d to %d units.\n",
							__func__, uniset2->capacity-UNIHANS_GROW_SIZE, uniset2->capacity);
		}

		/* copy unihan[i] to uniset2. BUT DO NOT change uniset2->size, as the total number of sorted original data! */
		uniset2->unihans[uniset2->size-1+more] = uniset1->unihans[i];
	}

	/* Finally, change size. */
	uniset2->size += more;
	printf("%s: Totally %d unihans merged.\n",__func__, more);

	return 0;
}


/*----------------------------------------------------------
Check an UNIHAN SET, if unihans found with same wcode
and typing value, then keep only one of them and clear
others.

		!!! --- WARNING --- !!!

1. Reading values are NOT checked here.
2. After purification, the uniset usually contain some empty unihans(wcode==0)
   and those unihans are distributed randomly. However the total size of the uniset does
   NOT include those empty unihans, so if you sort swcode in such case, you MUST take
   uniset->capacity (NOT uniset->size!!!) as total number of sorting itmes.

Return:
	>=0	Ok, total number of repeated unihans removed.
	<0	Fails
-----------------------------------------------------------*/
int UniHan_purify_uniset(EGI_UNIHAN_SET* uniset )
{
	int i;
	int k;
	char pch[4];
	wchar_t wcode;

	if(uniset==NULL)
		return -1;

	/* Print informatin */
	printf("%s: UNISET '%s' size=%d, capacity=%d.\n", __func__,uniset->name, uniset->size, uniset->capacity);

	/* quickSort wcode */
	if( UniHan_quickSort_wcode(uniset->unihans, 0, uniset->size-1, 10) != 0 ) {
		printf("%s: Fai to quickSort wcode!\n", __func__);
		return -2;
	}
	else
		printf("%s: Succeed to quickSort by KEY=wcode!\n", __func__);

	/* Check repeated unihans: with SAME wcode and typing value */
	k=0;
	for(i=1; i< uniset->size; i++) {
		if( uniset->unihans[i].wcode == uniset->unihans[i-1].wcode
		    &&  strncmp( uniset->unihans[i].typing, uniset->unihans[i-1].typing, UNIHAN_TYPING_MAXLEN ) == 0 )
		{
			k++;

			#if 1  /* TEST --- */
			wcode=uniset->unihans[i].wcode;
			bzero(pch,sizeof(pch));
			printf("%s: unihans[%d] AND unihans[%d] is repeated: UNICODE U+%04x '%s' TYPING '%s' \n", __func__, i-1, i,
				        uniset->unihans[i].wcode, char_unicode_to_uft8(&wcode, pch)>0?pch:"#?#", uniset->unihans[i].typing );
			#endif

			/* Clear/empty unihans[i-1] */
			bzero(uniset->unihans+i-1, sizeof(EGI_UNIHAN));
		}
	}
	printf("%s: Totally %d repeated unihans cleared!\n", __func__, k);

	/* Update size */
	uniset->size -= k;

	/* quickSort wcode */
	if( UniHan_quickSort_wcode(uniset->unihans, 0, uniset->capacity-1, 10) != 0 ) {  /* NOTE: capacity NOT size! */
		printf("%s: Fai to quickSort wcode after purification!\n", __func__);
		return -3;
	}

	return k;
}


/*----------------------------------------------
To print wcode info in the uniset.
The uniset MUST be sorted by KEY=wcode before
calling the function.
-----------------------------------------------*/
void UniHan_print_wcode(EGI_UNIHAN_SET *uniset, wchar_t wcode)
{
	int k;
	char pch[4]={0};

        if( UniHan_locate_wcode(uniset,wcode) !=0 ) {
                printf("wcode U+%04x NOT found!\n", wcode);
		return;
	}

	k=uniset->puh;

	/* Print all (polyphonic) unihans with the same unicode/wcode */
	while( uniset->unihans[k].wcode == wcode ) {
		/* Note: readings are in uft8-encoding, so printf format does NOT work! */
		printf("Uniset->unihans[%d]:%s, wcode:U+%04X	reading:%s	typing:%-7.7s freq:%d\n",
			k, char_unicode_to_uft8(&wcode, pch)>0?pch:"#?#",
			uniset->unihans[k].wcode, uniset->unihans[k].reading, uniset->unihans[k].typing, uniset->unihans[k].freq);
		k++;
	}
}
