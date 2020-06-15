/*------------------------------------------------------------------
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
  1. https://blog.csdn.net/firehood_/article/details/7648625
  2. https://blog.csdn.net/FortuneWang/article/details/41575039
  3. http://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip (Unihan_Readings.txt)
  4. http://www.unicode.org/reports/tr38


TODO:
1. Sort out frequently_used UniHans.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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


/*-------------------------------------------------------------------------
Compare typings of two unihans and return an integer less than, equal to,
or greater than zero, if unha1 is found to be ahead of, at same order of,
or behind uhans2 in dictionary order respectively.

Note:
1. It first compares their dictionary order, if NOT in the same place, return
the result, if in the same place, then compare their frequency number.

2. If uhan1->freq is GREATER than uhan2->freq, it returns -1(IS_AHEAD);
   else if uhan1->fre is LESS than uhan2->freq, then return 1(IS_AFTER);
   if EQUAL, it returns 0(IS_SAME).

3. A NULL is always 'AFTER' an EGI_UNIHAN pointer.

Return:
	Relative Priority Sequence position.
	<0	unhan1 is  'less than' OR 'ahead of' unhan2
	=0	unhan1 and unhan2 are at the same place.
	>0	unhan1 is  'greater than' OR 'behind' unhan2.
-------------------------------------------------------------------------*/
int UniHan_compare_typing(const EGI_UNIHAN *uhan1, const EGI_UNIHAN *uhan2)
{
	int i;

	/* 1. If uhan1 and/or uhan2 is NULL */
	if( uhan1==NULL && uhan2==NULL )
		return 0;
	else if( uhan1==NULL )
		return 1;
	else if( uhan2==NULL )
		return -1;

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
			return 1;
		else if ( uhan1->typing[i] < uhan2->typing[i])
			return -1;

		/* End of typing[] */
		if(uhan1->typing[i]==0 && uhan2->typing[i]==0)
			break;
	}

	/* 4. Compare frequencey number: EGI_UNIHAN.freq */
	if( uhan1->freq > uhan2->freq )
		return -1;
	else if( uhan1->freq < uhan2->freq )
		return 1;
	else
		return 0;

}


/*--------------------------------------------------------------------
Sort EGI_UNIHAN array(dictionary order) with Insertion_Sort algorithm.
Also ref. to mat_insert_sort() in egi_math.c.

Note: The caller MUST ensure unihans has at least n memebers!

@unihans:       An array of EGI_UNIHANs.
@n:             size of the array.

--------------------------------------------------------------------*/
void UniHan_insertSort( EGI_UNIHAN* unihans, int n )
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


/*---------------------------------------------------------------------------
Sort a part of EGI_UNIHAN array in dictionary+freqency order.
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
----------------------------------------------------------------------------*/
int UniHan_quickSort(EGI_UNIHAN* unihans, unsigned int start, unsigned int end, int cutoff)
{
	int i=start;
	int j=end;
	int mid;
	EGI_UNIHAN	tmp;
	EGI_UNIHAN	pivot;

	/* End sorting */
	if( start >= end )
		return 0;

        /* Limit cutoff */
        if(cutoff<3)
                cutoff=3;

/* 1. Implement quicksort */
        if( end-start >= cutoff ) //QUICK_SORT_CUTOFF )
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
		UniHan_quickSort(unihans, start, i-1, cutoff);
		UniHan_quickSort(unihans, i+1, end, cutoff);

        }
/* 2. Implement insertsort */
        else
		UniHan_insertSort( unihans+start, end-start+1);


	return 0;
}


/*------------------------------------------------
Create a set of unihan, with empty data.

@name: 	Short name for the set, MAX len 16-1.
@size:  Total number of unihans in the set.

Return:
	A pointer to EGI_UNIHAN_SET	Ok
	NULL				Fail
------------------------------------------------*/
EGI_UNIHAN_SET* UniHan_create_uniset(const char *name, size_t size)
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
	printf("%s:sizeof(unihan_set->name)=%d\n", __func__, sizeof(unihan_set->name));
	strncpy(unihan_set->name, name, sizeof(unihan_set->name)-1);
	unihan_set->size=size;

	return unihan_set;
}

/*---------------------------------------
	Free an EGI_UNIHAN_SET.
----------------------------------------*/
void UniHan_free_uniset( EGI_UNIHAN_SET **set)
{
	if(set==NULL || *set==NULL)
		return

	free( (*set)->unihans );
	free(*set);
	*set=NULL;
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
int UniHan_save_uniset(const char *fpath,  const EGI_UNIHAN_SET *set)
{
	int i;
	int nwrite;
	int nmemb;
	FILE *fil;
	uint8_t nq;
	int ret=0;
	size_t size;		/* Total number of unihans in the uniset */
	EGI_UNIHAN *unihans;

	if( set==NULL )
		return -1;

	/* Get uniset memebers */
	unihans=set->unihans;
	size=set->size;

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
	/* FWrite all EGI_UNIHANs */
	nmemb=sizeof(EGI_UNIHAN);
	for( i=0; i<size; i++) {
		nwrite=fwrite( unihans+i, 1, nmemb, fil);
	        if(nwrite < nmemb) {
	                if(ferror(fil))
        	                printf("%s: Fail to write EGI_UNIHANs to '%s'.\n", __func__, fpath);
                	else
                        	printf("%s: WARNING! fwrite EGI_UNIHANs  %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
                	ret=-4;
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


/*----------------------------------------------------------------------------
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
	EGI_UNIHAN_SET *set=NULL;
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
	set=UniHan_create_uniset("chpinyin", total);
	if(set==NULL) {
		printf("%s: Fail to create uniset!\n", __func__);
		goto END_FUNC;
	}

	/* FReadin all EGI_UNIHANs */
	nmemb=sizeof(EGI_UNIHAN);
	for( i=0; i<total; i++) {
		nread=fread( set->unihans+i, 1, nmemb, fil);
	        if(nread < nmemb) {
	                if(ferror(fil))
        	                printf("%s: Fail to read %d_th EGI_UNIHAN from '%s'.\n", __func__, i, fpath);
                	else
                        	printf("%s: WARNING! fread %d_th EGI_UNIHAN,  %d bytes of total %d bytes.\n", __func__, i, nread, nmemb);

			//Continue anyway .... //goto END_FUNC;
		}

		/* TEST: --- */
		printf("[%d]: wcode:%d, typing:%s, reading:%s \n",
					i, set->unihans[i].wcode, set->unihans[i].typing, set->unihans[i].reading);
	}


END_FUNC:
        /* Close fil */
        if( fclose(fil) !=0 ) {
                printf("%s: Fail to close file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
        }

	return set;
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
	char tone='0';

	k=0;
	while(*p)
	{
		size=char_uft8_to_unicode( (const UFT8_PCHAR)p, &wcode);

		/* 1. ----- A vowel char: Get pronunciation tone ----- */
		if(size>1) {

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
					tone='4';
					break;

				case 0x00EA:  	/* ê */
					tone='5';
					break;

				default:
					tone='0';
					break;
			}
		}

		/* 2. -----A vowel char or ASCII char:  Replace vowel with ASCII char ----- */
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
						/* ê̄  */  /* pick out */
						/* ế */
						/* ê̌ */
						/* ề */
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

				case 0x1E3F:	/* ḿ  */
				 		/* m̀ */
					pinyin[k]='m';
					break;

				/* Default: An ASCII char */
				default:
					//printf("wcode=%d\n", wcode);
					pinyin[k]=wcode;
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

Note:
1. The HanyuPinyin text MUST be extracted from Unihan_Readings.txt,
   The Unihan_Readings.txt is included in Unihan.zip:
   http://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip

2. The text file can be generated by follow shell command:
   cat Unihan_Readings.txt | grep kHanyuPinyin > kHanyuPinyin.txt

3. To pick out some strange form....

   3.1---- Some readings are with Combining Diacritical Marks:
   	ế =[0x1ebf]  ê̄ =[0xea  0x304]  ê̌ =[0xea  0x30c] ề =[0x1ec1]
	ḿ =[0x1e3f  0x20]  m̀=[0x6d  0x300]

   Pick out  ê̄,ế,ê̌,ề  manually first! Or it will bring strange PINYIN.
	U+6B38	kHanyuPinyin	32140.110:āi,ǎi,xiè,ế,éi,ê̌,ěi,ề,èi,ê̄
	U+8A92	kHanyuPinyin	63980.140:xī,yì,ê̄,ế,éi,ê̌,ěi,ề,èi

   Pick out ḿ
	U+5463	kHanyuPinyin	10610.080:móu,ḿ,m̀
	U+5514	kHanyuPinyin	10627.020:wù,wú,ńg,ḿ

   3.2----- Same unihan in 2 pages: (Keep, but words after SPACE will be discarded!!! ).
   	U+5448	kHanyuPinyin	10585.010:kuáng 10589.110:chéng,chěng
   	U+9848	kHanyuPinyin	53449.060:xuǎn,jiōng 74379.120:jiǒng,xiàn,xuǎn,jiōng
	... ...
	many ... ...


4. The return EGI_UNIHAN_SET usually contains empty unihans, as
   left unused after malloc.

Return:
	A pointer to EGI_UNIHAN_SET	Ok
	NULL				Fail
----------------------------------------------------------------------*/
EGI_UNIHAN_SET *UniHan_load_HanyuPinyinTxt(const char *fpath)
{

	EGI_UNIHAN_SET	*uniset=NULL;
	size_t  size;			/* init. size=growsize */
	size_t  growsize=1024;		/* memgrow size for uniset->unihans[], also inital for size */

	FILE *fil;
	char linebuff[1024];			/* Readin line buffer */
	#define  WORD_MAX_LEN 		256	/* Max. length of column words in each line of kHanyuPinyin.txt,  including '\0'. */
	#define  MAX_SPLIT_WORDS 	4  	/* Max number of words in linebuff[] as splitted by delimters */
	/*--------------------------------------------------------
	   To store words in linebuff[] as splitted by delimter.
	   Exampe:      linefbuff:  "U+92FA	kHanyuPinyin	64225.040:yuǎn,yuān,wǎn,wān"
			str_words[0]:	U+92FA
			str_words[1]:	kHanyuPinyin
			str_words[2]:	64225.040
			str_words[3]:	yuǎn,yuān,wǎn,wān
	---------------------------------------------------------*/
	char str_words[MAX_SPLIT_WORDS][WORD_MAX_LEN]; /* See above */
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
	size=growsize;
	uniset=UniHan_create_uniset("PRC_PINYIN", size);
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
		for(m=0; pt!=NULL && m<MAX_SPLIT_WORDS; m++) {  /* Separate linebuff into 4 words */
			bzero(str_words[m], sizeof(str_words[0]));
			snprintf( str_words[m], WORD_MAX_LEN, "%s", pt);
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
			strncpy(uniset->unihans[k].reading, pt, UNIHAN_READING_MAXLEN);
			/* 3. Convert reading to PINYIN typing and assign */
			UniHan_reading_to_pinyin( (UFT8_PCHAR)uniset->unihans[k].reading, uniset->unihans[k].typing);

		/* --- DEBUG --- */
		#if 0
		 if( wcode==0x54B9 ) {
			bzero(pch,sizeof(pch));
			EGI_PDEBUG(DBG_UNIHAN,"unihans[%d]:%s, wcode:%d, reading:%s, pinying:%s\n",
						k, char_unicode_to_uft8(&wcode, pch)>0?pch:" ",
						uniset->unihans[k].wcode, uniset->unihans[k].reading, uniset->unihans[k].typing );
		 }
		#endif
			/* 4. Count k as total number of unihans loaded to uniset. */
			k++;

			/* Check space capacity, and memgrow uniset->unihans[] */
			if( k==size ) {
				if( egi_mem_grow( (void **)&uniset->unihans, size*sizeof(EGI_UNIHAN), growsize*sizeof(EGI_UNIHAN)) != 0 ) {
					printf("%s: Fail to mem grow uniset->unihans!\n", __func__);
					UniHan_free_uniset(&uniset);
					goto END_FUNC;
				}
				size += growsize;
				printf("%s: uniset->unihans mem grow from %d to %d.\n", __func__, size-growsize, size);
			}

			/* Get next split pointer */
			pt=strtok(NULL, ",");

		}  /* End while(pare line) */

	} /* End while(Read in lines) */

	/* Finally, assign total size */
	uniset->size=size;

END_FUNC:
	/* close FILE */
	if(fclose(fil)!=0)
		printf("Fail to close han_mandrian txt.\n");

	return uniset;
}
