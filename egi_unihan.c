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


//static inline int strstr_group_typings(const char *hold_typings, const char* try_typings);
static inline int compare_group_typings( const char *group_typing1, int nch1, const char *group_typing2, int nch2, bool TYPING2_IS_SHORT);


/*----------------------------------------------------------------------------
Parse an unbroken string into several groups of PINYIN typings.
A PINYIN typing typically consists of a Shengmu(initial consonant) and a
Yunmu(vowel/vowels). In some cases, a legitimat PINYIN MAY have NO Shengmu at
all (Zero Shengmu)．
The result is in form of UNIHAN.typing, NOT UNIHAN.reading!
(See difinitions of 'typing' AND 'reading' in egi_unihan.h)

Note:
 1. Zero Shengmu PINYINs: ( However, the function will treat all Yunmus as Zero Shengmu PINTYINs! )
 a(啊) ai(爱) ao an ang
 e(额）er(而) en
 o(喔) ou

 2. Shengmu(inital consonant) list(23):
 (But 'y,w' here are treated as Shengmu)
	 b,p,m,f,d,t,n,l,g,k,h,j,q,x,zh,ch,sh,r,z,c,s,y,w

 3. Yunmu(vowel/vowels) list:
	a,o,e,
	i,u,ü		(v)
        ai,ei,ui,
	ao,ou,iu,
	ie,üe,er	(ve)  [ yüe -> yue ]
	an,en,in,un,ün	(vn)  [ yün -> yun ]
	ang,eng,ing,ong

	(other compounds)
	ua, uo, iou, uei
	iao, uai, uei, uen, ueng
	ian, uan, üan, iang, uang, iong
	Notice that 'iou','uei','uen' 'ueng' are to be written as 'iu','ui','un','ong' respectively when preceded by a Shengmu.

			( ---  Yunmus in Typings as Classified --- )
	a,ai,ao,an,ang
	o,ou,ong,
	e,ei,er,en,eng,
	i,iu,ie,in,ia, iao, ian, ing, iang, iong
	u,ui,uo,ue,un,ua,uai,uan,uang
	v,ve,vn,　　		（van ?)
	(as ü replaced by v in typings )


  ( All 26 alphabets will be the initial char of a Shenmu/Yunmu except 'v' )

 4. If there is ambiguous for parsing an unbroken pingyin string, such as "fangan", use '(0x27) to separate.
	"xian"  	is parsed to:  xian
	"xi'an" 	is parsed to:  xi an
	"haobangai"	is parsed to:  hao bang ai
	"ha'o'ban'gai"	is parsed to:  ha o ban gai

TODO:
	The function will treat all Yunmus as Zero Shengmu PINTYINs!

@strp:		An unbroken string of alphabet chars in lowercases,
		terminated with a '\0'.
		It SHOULD occupy UNIHAN_TYPING_MAXLEN*n bytes mem space,
		as the function will try to read the content in above range
		withou checking position of the string terminal NULL!

@pinyin:	To store parsed groups of PINYINs.
		Each group takes UNIHAN_TYPING_MAXLEN bytes, including '\0's.
		So it MUST hold at least UNIHAN_TYPING_MAXLEN*n bytes mem space.
@n:		Max. groups of PINYINs expected.

Return:
	>0	OK, total number of PINYIN typings as divided into.
	<=0	Fails
----------------------------------------------------------------------------*/
int  UniHan_divide_pinyin(const char *strp, char *pinyin, int n)
{
	int i;
	int np;			/* count total number of PINYIN typings */
	const char *ps;		/* Start of a complete PINYIN typing (Shengmu+Yunmu OR Yunmu) */
	const char *pc=strp;
	bool	zero_shengmu; /* True if has NO Shengmu */

	if( strp==NULL || pinyin ==NULL || n<1 )
		return -1;

	/* Clear data */
	memset(pinyin, 0, n*UNIHAN_TYPING_MAXLEN);
	np=0;

	for( i=0; i<n; i++) {
		while( *pc != '\0' ) {   /* pc always points to the start of a new Shengmu or Yunmu */

			ps=pc;  /* mark start of a PINYIN typing  */

			zero_shengmu=false; /* Assume it has shengmu */

			/* 1. Get Shengmu(initial consonant) */
			switch(*pc)
			{
				/* b,p,m,f,d,t,n,l,g,k,h,j,q,x,zh,ch,sh,r,z,c,s,y,w */
				case 'b': case 'p' : case 'm' : case 'f' : case 'd' : case 't' :
				case 'n': case 'l' : case 'g' : case 'k' : case 'h' : case 'j' :
				case 'q': case 'x' : case 'r' : case 'y' : case 'w' :
					pc++;
					break;
				case 'z':
				case 'c':
				case 's':
					switch( *(pc+1) ) {
						case 'h':		/* zh, ch, sh */
							pc +=2;
							break;
						default:		/* z,c,s */
							pc++;
							break;
					}
					break;

				/* Other case */
				case 'v':		/* A 'v' will never be an initial char of a Shengmu or Yunmu */
				case  0x27:		/* ' As separator */
					pc++;		/* get rid of it */
					continue;
					//break;

				/* unrecognizable Shengmu, OR start of a Yunmu */
				default:
					/* Get rid of other chars ?? */
					if( !isalpha(*pc) || !islower(*pc) ) {
						pc++;
						continue;
					}
					/* ELSE: a zero_Shengmu PINYIN, go on to parse Yunmu... */
					zero_shengmu=true;
					/* keep pc unchanged */
					break;
			}

			/* 2. Get Yunmu(vowel/vowels) */
			switch (*pc)
			{
				/* a, ai, ao, an, ang */
				case 'a':
					switch(*(pc+1)) {
						case 'i':			/* ai */
						case 'o':			/* ao */
							pc+=2;
							break;
						case 'n':
							if(*(pc+2)=='g')  	/* ang */
								pc+=3;
							else		  	/* an */
								pc+=2;
							break;
						default:  			/* a */
							pc ++;
							break;
					}
					break;

				/* o,ou,ong */
				case 'o':
					if( *(pc+1)=='u' )			/* ou */
						pc +=2;
					else if( *(pc+1)=='n' && *(pc+2)=='g' ) /* ong */
						pc +=3;
					else					/* o */
						pc++;
					break;

				/* e,ei,er,en,eng */
				case 'e':
					switch(*(pc+1)) {
						case 'i':			/* ei */
							pc+=2;
							break;
						case 'r':			/* er */
							if(zero_shengmu)
								pc+=2;
							else
								pc++;
							break;
						case 'n':
							if(*(pc+2)=='g')	/* eng */
								pc+=3;
							else			/* en */
								pc+=2;
							break;
						default:			/* e */
							pc++;
							break;
					}
					break;

				/* i,iu,ie,in,ing, ia, iao, ian, iang, iong */
				case 'i':
					switch(*(pc+1)) {
						case 'u':			/* iu */
						case 'e':			/* ie */
							pc+=2;
							break;
						case 'n':
							if(*(pc+2)=='g')	/* ing */
								pc+=3;
							else			/* in */
								pc+=2;
							break;
						case 'a':
							switch(*(pc+2)) {
								case 'o':
									pc+=3;			/* iao */
									break;
								case 'n':
									if(*(pc+3)=='g')	/* iang */
										pc+=4;
									else			/* ian */
										pc+=3;
									break;
								default:  /* takes only 'i'  */
									pc+=2;			/* ia */
									break;
							}
							break;
						case 'o':
							if( *(pc+2)=='n' && *(pc+3)=='g' )	/* iong */
								pc+=4;
							else
								pc++;	/* takes only 'i' */	/* i */
							break;

						default:
							pc++;
							break;
					}
					break;

				/* u,ui,uo,ue,un,ua,uai,uan,uang */
				case 'u':
					switch(*(pc+1)) {
						case 'i':			/* ui */
						case 'o':			/* ua */
						case 'e':			/* ue */
						case 'n':			/* un */
							pc+=2;
							break;
						case 'a':
							switch(*(pc+2)) {
								case 'i':
									pc+=3;			/* uai */
									break;
								case 'n':
									if(*(pc+3)=='g')        /* uang */
										pc+=4;
									else			/* uan */
										pc+=3;
									break;
								default:			/* ua */
									pc+=2;	/* taks only ua */
									break;
							}
							break;

						default:
							pc++;			/* u */
							break;
					}
					break;

				/* v,ve,vn*/
				case 'v':
					switch(*(pc+1)) {
						case 'e':			/* ve */
						case 'n':			/* vn */
							pc+=2;
							break;
						default:
							pc++;			/* u */
							break;
					}
					break;

				/* Unrecognizable vowel/vowels, OR start of a Shengmu */
				default:
					/* keep pc unchanged */
					break;
			}

			/*  All 26 alphabets will be the initial char of a Shenmu or a Yunmu, except 'v'!  */

			/* 3. Copy a complete PINYIN typing(Shengmu+Yunmu or Yunmu) to pinyin[] */
			strncpy(pinyin+i*UNIHAN_TYPING_MAXLEN, ps, pc-ps);
			np++;

			/* 4. For the next PINYIN */
			i++;

		} /* while() */

	} /* for() */

	return np;
}

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
EGI_UNIHAN_SET* UniHan_create_set(const char *name, size_t capacity)
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
void UniHan_free_set( EGI_UNIHAN_SET **set)
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
Compare typing+freq ORDER of two unihans and return an integer less than, equal to,
or greater than zero, if unha1 is found to be ahead of, at same order of,
or behind uhans2 in dictionary order respectively.

Note:
1. !!! CAUTION !!!  Here typing refers to PINYIN, other type MAY NOT work.
2. All typing letters MUST be lowercase.
3. It first compares their dictionary order, if NOT in the same order, return
the result; if in the same order, then compare their frequency number to
decide the order as in 4.
4. If uhan1->freq is GREATER than uhan2->freq, it returns -1(IS_AHEAD);
   else if uhan1->freq is LESS than uhan2->freq, then return 1(IS_AFTER);
   if EQUAL, it returns 0(IS_SAME).
5. An empty UNIHAN(wcode==0) is always 'AFTER' a nonempty UNIHAN.
6. A NULL is always 'AFTER' a non_NULL UNIHAN pointer.

Return:
	Relative Priority Sequence position:
	CMPORDER_IS_AHEAD    -1 (<0)	unhan1 is  'less than' OR 'ahead of' unhan2
	CMPORDER_IS_SAME      0 (=0)   unhan1 and unhan2 are at the same order.
	CMPORDER_IS_AFTER     1 (>0)   unhan1 is  'greater than' OR 'behind' unhan2.

-------------------------------------------------------------------------*/
int UniHan_compare_typing(const EGI_UNIHAN *uhan1, const EGI_UNIHAN *uhan2)
{
	int i;

	/* 1. If uhan1 and/or uhan2 is NULL */
	if( uhan1==NULL && uhan2==NULL )
		return CMPORDER_IS_SAME;
	else if( uhan1==NULL )
		return CMPORDER_IS_AFTER;
	else if( uhan2==NULL )
		return CMPORDER_IS_AHEAD;

	/* 2. Put the empty EGI_UNIHAN to the last, OR check typing[0] ? */
	if(uhan1->wcode==0 && uhan2->wcode!=0)
		return CMPORDER_IS_AFTER;
	else if(uhan1->wcode!=0 && uhan2->wcode==0)
		return CMPORDER_IS_AHEAD;
	else if(uhan1->wcode==0 && uhan2->wcode==0)
		return CMPORDER_IS_SAME;

	/* 3. Compare dictionary oder of EGI_UNIHAN.typing[] */
	for( i=0; i<UNIHAN_TYPING_MAXLEN; i++) {
		if( uhan1->typing[i] > uhan2->typing[i] )
			return CMPORDER_IS_AFTER;
		else if ( uhan1->typing[i] < uhan2->typing[i])
			return CMPORDER_IS_AHEAD;
		/* End of both typings, EOF */
		else if(uhan1->typing[i]==0 && uhan2->typing[i]==0)
			break;

		/* else: uhan1->typing[i] == uhan2->typing[i] != 0  */
	}
	/* NOW: CMPORDER_IS_SAME */

	/* 4. Compare frequencey number: EGI_UNIHAN.freq */
	if( uhan1->freq > uhan2->freq )
		return CMPORDER_IS_AHEAD;
	else if( uhan1->freq < uhan2->freq )
		return CMPORDER_IS_AFTER;
	else
		return CMPORDER_IS_SAME;
}


/*-------------------------------------------------------------------------
Compare wcode+typing(+freq) ODER of two unihans and return an integer less than, equal to,
or greater than zero, if unha1 is found to be ahead of, at same order of,
or behind uhans2 in dictionary order respectively.

Note:
1. !!! CAUTION !!!  Here typing refers to PINYIN, other type MAY NOT work.
2. All typing letters MUST be lowercase.
3. It first compares their wcode value, if NOT in the same order, return
the result; if in the same order, then compare their typing value to
decide the order.
4. An empty UNIHAN(wcode==0) is always 'AFTER' a nonempty UNIHAN !!!
5. A NULL is always 'AFTER' a non_NULL UNIHAN pointer.

Return:
	Relative Priority Sequence position:
	CMPORDER_IS_AHEAD    -1 (<0)	unhan1 is  'less than' OR 'ahead of' unhan2
	CMPORDER_IS_SAME      0 (=0)   unhan1 and unhan2 are at the same order.
	CMPORDER_IS_AFTER     1 (>0)   unhan1 is  'greater than' OR 'behind' unhan2.

-------------------------------------------------------------------------*/
int UniHan_compare_wcode(const EGI_UNIHAN *uhan1, const EGI_UNIHAN *uhan2)
{

	/* 1. If uhan1 and/or uhan2 is NULL */
	if( uhan1==NULL && uhan2==NULL )
		return CMPORDER_IS_SAME;
	else if( uhan1==NULL )
		return CMPORDER_IS_AFTER;
	else if( uhan2==NULL )
		return CMPORDER_IS_AHEAD;

	/* 2. Put the empty EGI_UNIHAN to the last  */
	if(uhan1->wcode==0 && uhan2->wcode!=0)
		return CMPORDER_IS_AFTER;
	else if(uhan1->wcode!=0 && uhan2->wcode==0)
		return CMPORDER_IS_AHEAD;
	else if(uhan1->wcode==0 && uhan2->wcode==0)
		return CMPORDER_IS_SAME;

	/* 3. Compare wcode value */
	if(uhan1->wcode > uhan2->wcode)
		return CMPORDER_IS_AFTER;
	else if(uhan1->wcode < uhan2->wcode)
		return CMPORDER_IS_AHEAD;

	/* 4. If wcodes are the SAME, compare typing then */
	return UniHan_compare_typing(uhan1, uhan2);
}


/*------------------------------------------------------------------------
Sort an EGI_UNIHAN array by Insertion_Sort algorithm, to rearrange unihans
in ascending order of typing+freq. (typing:in dictionary order)

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
                for( k=i; k>0 && UniHan_compare_typing(unihans+k-1, &tmp)==CMPORDER_IS_AFTER; k--) {
				unihans[k]=unihans[k-1];   /* swap */
		}

		/* Settle the inserting unihan at last swapped place */
                unihans[k]=tmp;
        }
}


/*------------------------------------------------------------------
Sort an EGI_UNIHAN array by Quick_Sort algorithm, to rearrange unihans
in ascending order of typing+freq.
To see UniHan_compare_typing() for the details of order comparing.

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
		if( UniHan_compare_typing(unihans+start, unihans+mid)==CMPORDER_IS_AFTER ) {
                        tmp=unihans[start];
                        unihans[start]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* Now: [start]<=array[mid] */

                /* IF: [mid] >= [start] > [end] */
                /* if( array[start] > array[end] ) { */
		if( UniHan_compare_typing(unihans+start, unihans+end)==CMPORDER_IS_AFTER ) {
                        tmp=unihans[start]; /* [start] is center */
                        unihans[start]=unihans[end];
                        unihans[end]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* ELSE:   [start]<=[mid] AND [start]<=[end] */
                /* else if( array[mid] > array[end] ) { */
		if( UniHan_compare_typing(unihans+mid,unihans+end)==CMPORDER_IS_AFTER ) {
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
			while( UniHan_compare_typing(unihans+(++i),&pivot)==CMPORDER_IS_AHEAD ) { };

                        /* Stop at array[j]<=pivot: We preset array[start]<=pivot as sentinel, so j will stop at [start]  */
                        /* while( array[--j] > pivot ){ }; */
			while( UniHan_compare_typing(unihans+(--j),&pivot)==CMPORDER_IS_AFTER ) { };

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


#if 0 ////////////////////  Replaced!  //////////////////////////////
/*--------------- wcode is the ONLY sorting KEY!!! ---------------------------
Sort an EGI_UNIHAN array by Insertion_Sort algorithm, to rearrange unihans
with wcode (as KEY) in ascending order.

Also ref. to mat_insert_sort() in egi_math.c.

Note:
1.The caller MUST ensure unihans array has at least n memebers!
2.Input 'n' should NOT includes empty unihans with wcode==0, OR they will
  be rearranged to the beginning of the array.


@unihans:       An array of EGI_UNIHANs.
@n:             size/capacity of the array. (use capacity if empty unihans eixst)
------------------------------------------------------------------------------*/
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
#endif ///////////////////////////////////////////////////////////////

/*------------------------------------------------------------------------
Sort an EGI_UNIHAN array by Insertion_Sort algorithm, to rearrange unihans
in ascending order of wcode+typing(+freq). (typing:in dictionary order)

Also ref. to mat_insert_sort() in egi_math.c.

Note:
1.The caller MUST ensure unihans array has at least n memebers!
2.Input 'n' MAY includes empty unihans with wcode==0, and they will
  be rearranged to the end of the array.

@unihans:       An array of EGI_UNIHANs.
@n:             size of the array. ( NOT capacity )

------------------------------------------------------------------------*/
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
                //for( k=i; k>0 && unihans[k-1].wcode > tmp.wcode; k--)
		for( k=i; k>0 && UniHan_compare_wcode(unihans+k-1, &tmp)==CMPORDER_IS_AFTER; k--)
				unihans[k]=unihans[k-1];   /* swap */

		/* Settle the inserting unihan at last swapped place */
                unihans[k]=tmp;
        }
}

#if 0 //////////////////  Replaced! ////////////////////////////
/*----------------- wcode is the ONLY sorting KEY!!!--------------------
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
#endif /////////////////////////////////////////////////////////////////////////////////////


/*--------------------------------------------------------------------
Sort an EGI_UNIHAN array by Quick_Sort algorithm, to rearrange unihans
in ascending order of wcode+typing(+freq). (typing:in dictionary order)


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
		// if( unihans[start].wcode > unihans[mid].wcode ) {
		if( UniHan_compare_wcode(unihans+start, unihans+mid)==CMPORDER_IS_AFTER ) {
                        tmp=unihans[start];
                        unihans[start]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* Now: [start]<=array[mid] */

                /* IF: [mid] >= [start] > [end] */
                /* if( array[start] > array[end] ) { */
		// if( unihans[start].wcode > unihans[end].wcode ) {
		if( UniHan_compare_wcode(unihans+start, unihans+end)==CMPORDER_IS_AFTER ) {
                        tmp=unihans[start]; /* [start] is center */
                        unihans[start]=unihans[end];
                        unihans[end]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* ELSE:   [start]<=[mid] AND [start]<=[end] */
                /* else if( array[mid] > array[end] ) { */
		// if( unihans[mid].wcode > unihans[end].wcode ) {
		if( UniHan_compare_wcode(unihans+mid, unihans+end)==CMPORDER_IS_AFTER ) {
                        /* If: [start]<=[end]<[mid] */
                                tmp=unihans[end];   /* [end] is center */
                                unihans[end]=unihans[mid];
                                unihans[mid]=tmp;
                        /* Else: [start]<=[mid]<=[end] */
                }
                /* Now: array[start] <= array[mid] <= array[end] */

                pivot=unihans[mid];

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
			// while( unihans[++i].wcode < pivot.wcode ) { };
			while( UniHan_compare_wcode(unihans+(++i), &pivot)==CMPORDER_IS_AHEAD ) { };
                        /* Stop at array[j]<=pivot: We preset array[start]<=pivot as sentinel, so j will stop at [start]  */
                        /* while( array[--j] > pivot ){ }; */
			// while( unihans[--j].wcode > pivot.wcode ) { };
			while( UniHan_compare_wcode(unihans+(--j), &pivot)==CMPORDER_IS_AFTER ) { };

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


/*---------------------------------------------------------
Quick sort an uniset according to the required sort order.

		   --- WARNING!!! ---

Be cautious about sorting range of unihans! Since there MAY
be empty unihans(with wcode==0) in the uniset.

For  UNIORDER_FREQ and UNIORDER_TYPING_FREQ, it's assumed
that all empty unihans are located at end of the unihan array
BEFORE quickSort! and sorting range is [0, size-1].

For UNIORDER_WCODE_TYPING_FREQ: the range is [0,capacity-1].
and empty unihans will be put to the end of the array AFTER
quickSort.

@uniset		An pointer to an UNIHAN_SET.
@sorder		Required sort order.
@cutoff		Cutoff value for quicksort algorithm.

Return:
	<0	Fails
	=0	OK
----------------------------------------------------------*/
int UniHan_quickSort_set(EGI_UNIHAN_SET* uniset, UNIHAN_SORTORDER sorder, int cutoff)
{
	int ret=0;

	if(uniset==NULL || uniset->unihans==NULL || uniset->size==0) /* If empty */
		return -1;

	/* Select sort functions */
	switch( sorder )
	{
		case	UNIORDER_FREQ:		/* Range[0, size-1] */
			ret=UniHan_quickSort_freq(uniset->unihans, 0, uniset->size-1, cutoff);
			break;
		case	UNIORDER_TYPING_FREQ:	/* Range[0, size-1] */
			ret=UniHan_quickSort_typing(uniset->unihans, 0, uniset->size-1, cutoff);
			break;
		case	UNIORDER_WCODE_TYPING_FREQ:  /* Range[0,capacity-1]: unihans with wcode==0 will be put to the end of the array */
			ret=UniHan_quickSort_wcode(uniset->unihans, 0, uniset->capacity-1, cutoff);
			break;
		default:
			printf("%s: Unsupported sort order!\n",__func__);
			ret=-1;
			break;
	}

	/* Put sort order ID */
	if(ret==0)
		uniset->sorder=sorder;

	return ret;
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
int UniHan_save_set(const char *fpath,  const EGI_UNIHAN_SET *uniset)
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

	printf("%s: Write uniset '%s' with %d UNIHANs to file '%s' \n",__func__, uniset->name, size, fpath);

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
EGI_UNIHAN_SET* UniHan_load_set(const char *fpath)
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
	uniset=UniHan_create_set(NULL, total); /* total is capacity, NOT size! */
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
		UniHan_free_set(&uniset);
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

			//UniHan_free_set(&uniset);
			//Continue anyway .... //goto END_FUNC;
		}

		#if 0 /* TEST: --- */
		printf("Load [%d]: wcode:%d, typing:%s, reading:%s \n",
					i, uniset->unihans[i].wcode, uniset->unihans[i].typing, uniset->unihans[i].reading);
		#endif
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
weigh. ONLY UNIHANs available in the uniset will be considered.

Note:
1. The uniset MUST be prearranged in ascending order of wcodes+typing+freq!
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

	/* Check sort order */
	if( uniset->sorder != UNIORDER_WCODE_TYPING_FREQ ) {
		printf("%s: Uniset sort order is NOT UNIORDER_WCODE_TYPING_FREQ!\n", __func__);
		return -2;
	}

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
					printf("%s: WARNING %s U+%X NOT found in uniset!\n", __func__,
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


/*----------------------------------------------------------------------------
Increase/decrease weigh value of freqency to an UNIHAN.

NOTE:
1. The uniset MUST be sorted in ascending order of typing+freq.
   ( Call UniHan_quickSort_typing() to achieve. )

@uniset:	A pointer to an EGI_UNIHAN_SET.
@typing:	Typing of the target unihan.
@wcode:		UNICODE of the target unihan.
@delt:		Variation to the freq value.

Return:
        0       OK
        <0      Fails
------------------------------------------------------------------------------*/
int UniHan_increase_freq(EGI_UNIHAN_SET *uniset, const char* typing, EGI_UNICODE wcode, int delt)
{
	unsigned i;
	unsigned int  start, end;

	if(uniset==NULL || uniset->unihans==NULL || uniset->size==0) /* If empty */
		return -1;

	/* Check sort order */
	if( uniset->sorder != UNIORDER_TYPING_FREQ ) {
		printf("%s: Uniset sort order is NOT UNIORDER_TYPING_FREQ!\n", __func__);
		return -2;
	}

	/* Locate first typing */
	if( UniHan_locate_typing(uniset, typing) !=0 ) {
		printf("%s: Fail to locate typing!\n",__func__);
		return -3;
	}

	/* Start/end index of unihans[] that bearing the given typing */
	start=uniset->puh;
	end=start;
	while( strncmp( uniset->unihans[end+1].typing, typing, UNIHAN_TYPING_MAXLEN ) == 0 )
		{ end++; };

	/* Locate the wcode */
	for(i=start; i<=end; i++) {
		if( uniset->unihans[i].wcode == wcode ) {
			/* Freq overflow check: check type of unihan.freq! */
			#if 0
			if( uniset->unihans[i].freq > UINT_MAX-delt )
				return -4;
			#endif

			/* Increase freq */
			uniset->unihans[i].freq += delt;
			uniset->sorder=UNIORDER_NONE;

			/* Need to resort TYPING_FREQ for the given typing */
			if( UniHan_quickSort_typing(uniset->unihans, start, i, 5) !=0 ) {
				printf("%s: Fail to resort TYPING_FREQ after frequency incremental!\n",__func__);
				return -5;
			}
			else
				uniset->sorder=UNIORDER_TYPING_FREQ;

			return 0;
		}
       }

       return -6;
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
Read a kHanyuPinyin text file (with UFT8 encoding), and load data to
an EGI_UNIHAN_SET.
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
	char uchar[UNIHAN_UCHAR_MAXLEN];

	/* Open kHanyuPinyin file */
	fil=fopen(fpath,"r");
	if(fil==NULL) {
		printf("%s: Fail to open '%s', ERR:%s.\n",__func__, fpath,  strerror(errno));
		return NULL;
	}
	else
		printf("%s: Open '%s' to read in and load unihan set...\n", __func__, fpath);

	/* Allocate uniset */
	capacity=growsize;
	uniset=UniHan_create_set("kHanyuPinyin", capacity);
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

		/* Convert to uft-8 encoding */
		bzero(uchar,sizeof(uchar));
		if( char_unicode_to_uft8(&wcode, uchar) <=0 )
			printf("%s: Fail to convert wcode=U+%X to UFT-8 encoding unihan.\n",__func__, wcode);

		/* --- 2. Parse reading and convert to pinyin --- */
		/* Note: Split str_words[3] into several PINYINs,and store them in separated uniset->unihans
		 * and store them in separated uniset->unihans
		 */
		//printf("str_words[3]: %s\n",str_words[3]);
		pt=strtok(str_words[3], ",");
		/* At lease one pinyin, for most case:  " 74801.200:jì " */
		if(pt==NULL)
			pt=str_words[3];

		/* If more than one PINYIN, store them in separated uniset->unihans with same wcode */
		while( pt != NULL ) {
		/* ---- 3. Save unihans[] memebers ---- */
			/* 1. Assign wcode */
			uniset->unihans[k].wcode=wcode;
			/* 2. Assign uchar */
			strncpy(uniset->unihans[k].uchar, uchar, UNIHAN_UCHAR_MAXLEN-1);
			/* 3. Assign reading */
			strncpy(uniset->unihans[k].reading, pt, UNIHAN_READING_MAXLEN-1);
			/* 4. Convert reading to PINYIN typing and assign */
			UniHan_reading_to_pinyin( (UFT8_PCHAR)uniset->unihans[k].reading, uniset->unihans[k].typing);

		/* --- DEBUG --- */
		#if 1
		 if( wcode==0x54B9 ) {
			EGI_PDEBUG(DBG_UNIHAN,"unihans[%d]:%s, wcode:U+%04X, reading:%s, pinying:%s\n",
							k, uniset->unihans[k].uchar, uniset->unihans[k].wcode,
								uniset->unihans[k].reading, uniset->unihans[k].typing );
		 }
		#endif
			/* 4. Count k as total number of unihans loaded to uniset. */
			k++;

			/* Check space capacity, and memgrow uniset->unihans[] */
			if( k==capacity ) {
				if( egi_mem_grow( (void **)&uniset->unihans, capacity*sizeof(EGI_UNIHAN), growsize*sizeof(EGI_UNIHAN)) != 0 ) {
					printf("%s: Fail to mem grow uniset->unihans!\n", __func__);
					UniHan_free_set(&uniset);
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
Read a kMandarin text file (with UFT8-encoding), and load data to an
EGI_UNIHAN_SET struct.
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
	char uchar[UNIHAN_UCHAR_MAXLEN];

	/* Open kHanyuPinyin file */
	fil=fopen(fpath,"r");
	if(fil==NULL) {
		printf("%s: Fail to open '%s', ERR:%s.\n",__func__, fpath,  strerror(errno));
		return NULL;
	}
	else
		printf("%s: Open '%s' to read in and load unihan set...\n", __func__, fpath);

	/* Allocate uniset */
	capacity=growsize;
	uniset=UniHan_create_set("kMandarin", capacity);
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

		/* Convert to uft-8 encoding */
		bzero(uchar,sizeof(uchar));
		if( char_unicode_to_uft8(&wcode, uchar) <=0 )
			printf("%s: Fail to convert wcode=U+%X to UFT-8 encoding unihan.\n",__func__, wcode);

		/* --- 2. Parse reading and convert to pinyin --- */
		/* NOTE:
		 * 1.Split str_words[2] into several PINYINs,and store them in separated uniset->unihans
		 *   and store them in separated uniset->unihans
		 * 2.Currently, Each row in kMandarin has ONLY one typing, so delimiter "," does NOT exist at all!
		 */
		//printf("str_words[2]: %s\n",str_words[2]);
		pt=strtok(str_words[2], ",");

		/* NOW ONLY one typing: "U+8FE2	kMandarin	tiáo", pt==NULL  */
		if(pt==NULL)
			pt=str_words[2];

		/* If more than one PINYIN, store them in separated uniset->unihans with same wcode */
		while( pt != NULL ) {
		/* ---- 3. Save unihans[] memebers ---- */

			/* 1. Assign wcode */
			uniset->unihans[k].wcode=wcode;
			/* 2. Assign uchar */
			strncpy(uniset->unihans[k].uchar, uchar, UNIHAN_UCHAR_MAXLEN-1);
			/* 3. Assign reading */
			strncpy(uniset->unihans[k].reading, pt, UNIHAN_READING_MAXLEN-1);
			/* 4. Convert reading to PINYIN typing and assign */
			UniHan_reading_to_pinyin( (UFT8_PCHAR)uniset->unihans[k].reading, uniset->unihans[k].typing);

		/* --- DEBUG --- */
		#if 0
		 if( wcode==0x54B9 ) {
			EGI_PDEBUG(DBG_UNIHAN,"unihans[%d]:%s, wcode:U+%04x, reading:%s, pinying:%s\n",
							k, uniset->unihans[k].uchar, uniset->unihans[k].wcode,
								uniset->unihans[k].reading, uniset->unihans[k].typing );
		 }
		#endif

			/* 4. Count k as total number of unihans loaded to uniset. */
			k++;

			/* Check space capacity, and memgrow uniset->unihans[] */
			if( k==capacity ) {
				if( egi_mem_grow( (void **)&uniset->unihans, capacity*sizeof(EGI_UNIHAN), growsize*sizeof(EGI_UNIHAN)) != 0 ) {
					printf("%s: Fail to mem grow uniset->unihans!\n", __func__);
					UniHan_free_set(&uniset);
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

	/* Check sort order */
	if( uniset->sorder != UNIORDER_WCODE_TYPING_FREQ ) {
		printf("%s: Uniset sort order is NOT UNIORDER_WCODE_TYPING_FREQ!\n", __func__);
		return -2;
	}

	/* Get pointer to unihans */
	unihans=uniset->unihans;

	/* Mark start/end/mid of unihans index */
	start=0;
	end=uniset->size-1;  /* NOPE!! XXXXX NOT uniset->size!! Consider there may be some empty unihans at beginning!  */
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
"Containing the given typing" means unihan[].typing is SAME as the given
typing, OR the given typing are contained in beginning of unihan[].typing.

This criterion is ONLY based on PINYIN. Other types of typing may NOT comply.

Note:
1. The uniset MUST be prearranged in dictionary ascending order of typing+freq.
2. And all uuniset->unhihans[0 size-1] are assumed to be valid/normal, with
   wcode >0 AND typing[0]!='\0'.
3. Pronunciation tones of PINYIN are neglected.
4. Usually there are more than one unihans that have the save typing,
(same pronuciation, but different UNIHANs), and indexse of those unihans
should be consecutive in the uniset (see 1). Finally the firt index will
be located.
5. This algorithm depends on typing type PINYIN, others MAY NOT work.

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

	/* Check sort order */
	if( uniset->sorder != UNIORDER_TYPING_FREQ ) {
		printf("%s: Uniset sort order is NOT UNIORDER_TYPING_FREQ!\n", __func__);
		return -2;
	}

	/* Get pointer to unihans */
	unihans=uniset->unihans;

	/* Mark start/end/mid of unihans index */
	start=0;
	end=uniset->size-1;
	mid=(start+end)/2;

	/* binary search for the typing */
	/* While if NOT containing the typing string at beginning */
	while( strstr(unihans[mid].typing, typing) != unihans[mid].typing ) {

		//printf("[mid].typing=%s, start=%d,mid=%d, end=%d\n",unihans[mid].typing, start,mid,end);

		/* check if searched all wcodes */
		if(start==end) {
			return -2;
		}

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
		if ( UniHan_compare_typing(&unihans[mid], &tmphan)==CMPORDER_IS_AFTER ) {
			end=mid;
			mid=(start+end)/2;
			continue;
		}
		//else if( unihans[mid].wcode < wcode) {
		if ( UniHan_compare_typing(&unihans[mid], &tmphan)==CMPORDER_IS_AHEAD ) {
			start=mid;
			mid=(start+end)/2;
			continue;
		}

	        /* NOW CMPORDER_IS_SAME: To rule out abnormal unihan with wcode==0!
		 * UniHan_compare_typing() will return CMPORDER_IS_SAME if two unihans both with wocde==0 */
		if( unihans[mid].wcode==0 )
			return -3;
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
   sorted after operation!
3. uniset1 will NOT be released/freed after operation!
4. uniset2->unihans will mem_grow if capacity is NOT enough.

@uniset1:	An UNIHAN SET to be emerged.
@uniset2:	The receiving UNIHAN SET.

Return:
	0	Ok
	<0	Fail
--------------------------------------------------------------------------*/
int UniHan_merge_set(const EGI_UNIHAN_SET* uniset1, EGI_UNIHAN_SET* uniset2)
{
	int i;
	int more=0;	/* more unihans added to uniset2 */

	/* Check input uniset1, check uniset2 in UniHan_quickSort_wcode() */
	if(uniset1==NULL || uniset1->unihans==NULL)
		return -1;

	/* quickSort_wcode uniset2, before calling UniHan_locate_wcode(). */
	#if 1
	if( UniHan_quickSort_set(uniset2, UNIORDER_WCODE_TYPING_FREQ, 10) != 0 ) {
		printf("%s: Fail to quickSort_wcode!\n",__func__);
		return -2;
	}
	#else
	if( UniHan_quickSort_wcode(uniset2->unihans, 0, uniset2->size-1, 10) != 0 ) {
		printf("%s: Fail to quickSort_wcode!\n",__func__);
		return -2;
	} else
		uniset2->sorder=UNIORDER_WCODE_TYPING_FREQ;
	#endif

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
				printf("Merge: [%s:U+%X] already exists.\n",uniset1->unihans[i].uchar, uniset1->unihans[i].wcode);
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

	/* Change size. */
	uniset2->size += more;
	printf("%s: Totally %d unihans merged.\n",__func__, more);

	/* Reset sort order */
	uniset2->sorder=UNIORDER_NONE;

	return 0;
}


/*---------------------------------------------------------------------------
Check an UNIHAN SET, if unihans found with same wcode and typing value, then
keep only one of them and clear others. At last, call UniHan_quickSort_wcode()
to sort unihans in ascending order of wcode+typing+freq.

		!!! --- WARNING --- !!!

1. Reading values are NOT checked here.

xxxxxxxxxxx  OK, Solved! as UniHan_quickSort_wcode() improved.  xxxxxxxxxxxx
x
x  2. After purification, the uniset usually contain some empty unihans(wcode==0)
x  and those unihans are distributed randomly. However the total size of the
x  uniset does NOT include those empty unihans, so if you sort swcode in such
x  case, you MUST take uniset->capacity (NOT uniset->size!!!) as total number
x  of sorting itmes.
x
x  3. TODO Follow case will NOT be purified. ...OK!
x  Uniset->unihans[16131]:色, wcode:U+8272  reading:sè	typing:se     freq:0
x  Uniset->unihans[16132]:色, wcode:U+8272  reading:	typing:shai   freq:1
x  Uniset->unihans[16133]:色, wcode:U+8272  reading:	typing:se     freq:1
x
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

Return:
	>=0	Ok, total number of repeated unihans removed.
	<0	Fails
---------------------------------------------------------------------------*/
int UniHan_purify_set(EGI_UNIHAN_SET* uniset )
{
	int i;
	int k;
	//char pch[4];
	//wchar_t wcode;

	if(uniset==NULL)
		return -1;

	/* Print informatin */
	printf("%s: UNISET '%s' size=%d, capacity=%d.\n", __func__,uniset->name, uniset->size, uniset->capacity);

	/* quickSort for UNIORDER_WCODE_TYPING_FREQ */
	if( uniset->sorder != UNIORDER_WCODE_TYPING_FREQ )
	{
		if( UniHan_quickSort_set(uniset, UNIORDER_WCODE_TYPING_FREQ, 10) != 0 ) {
			printf("%s: Fai to quickSort wcode!\n", __func__);
			return -2;
		}
		else
			printf("%s: Succeed to quickSort by KEY=wcode!\n", __func__);
	}

	/* Check repeated unihans: with SAME wcode and typing value */
	k=0;
	for(i=1; i< uniset->size; i++) {
		if( uniset->unihans[i].wcode == uniset->unihans[i-1].wcode
		    &&  strncmp( uniset->unihans[i].typing, uniset->unihans[i-1].typing, UNIHAN_TYPING_MAXLEN ) == 0 )
		{
			k++;
			#if 1  /* TEST --- */
			printf("%s: unihans[%d] AND unihans[%d] is repeated: [%s:U+%X:'%s'] \n", __func__,
				    i-1,i, uniset->unihans[i].uchar, uniset->unihans[i].wcode, uniset->unihans[i].typing );
			#endif

			/* Clear/empty unihans[i-1] */
			bzero(uniset->unihans+i-1, sizeof(EGI_UNIHAN));
		}
	}
	printf("%s: Totally %d repeated unihans cleared!\n", __func__, k);

	/* Update size */
	uniset->size -= k;

	/* Reset sorder */
	uniset->sorder=UNIORDER_NONE;

	/* quickSort wcode:  sort in wcode+typing+freq order */
	if( UniHan_quickSort_set(uniset, UNIORDER_WCODE_TYPING_FREQ, 10) != 0 ) {
		printf("%s: Fai to quickSort wcode after purification!\n", __func__);
		return -3;
	}

	return k;
}


/*-------------------------------------------------------------
To find unihans with given wcode in the uniset and print
out all of them.
The uniset MUST be sorted by UniHan_quickSort_wcode() before
calling this function.
--------------------------------------------------------------*/
void UniHan_print_wcode(EGI_UNIHAN_SET *uniset, EGI_UNICODE wcode)
{
	int k;

	/* UniHan_locate_wcode(): Input check also inside. */
        if( UniHan_locate_wcode(uniset,wcode) !=0 ) {		/* sort: wcode+typing+freq */
                printf("wcode U+%04x NOT found!\n", wcode);
		return;
	}

	k=uniset->puh;

	/* Print all (polyphonic) unihans with the same unicode/wcode */
	while( uniset->unihans[k].wcode == wcode ) {
		/* Note: readings are in uft8-encoding, so printf format does NOT work! */
		printf("Uniset->unihans[%d]:%s, wcode:U+%04X	reading:%s	typing:%-7.7s freq:%d\n",
			k, uniset->unihans[k].uchar[0]=='\0'?"#?#":uniset->unihans[k].uchar,
			uniset->unihans[k].wcode, uniset->unihans[k].reading, uniset->unihans[k].typing, uniset->unihans[k].freq);
		k++;
	}
}



/* <<<<<<<<<<<<<<<<<<<<<<    Functions for UNIHAN_GROUPS (Words/Phrases/Cizu) >>>>>>>>>>>>>>>>>>>>>>>>>>*/

/*------------------------------------------------------------------------
Create a set of unihan_groups(phrases), with empty data.

@name: 	    Short name for the set.
	    If NULL, ignore.
@capacity:  Capacity for total max. number of unihan_groups in the set.

Return:
	A pointer to EGI_UNIHANGROUP_SET	Ok
	NULL				Fail
-------------------------------------------------------------------------*/
EGI_UNIHANGROUP_SET* UniHanGroup_create_set(const char *name, size_t capacity)
{

	EGI_UNIHANGROUP_SET	*group_set=NULL;

	if(capacity==0)
		return NULL;

	/* Calloc unihan set */
	group_set = calloc(1, sizeof(EGI_UNIHANGROUP_SET));
	if(group_set==NULL) {
		printf("%s: Fail to calloc group_set.\n",__func__);
		return NULL;
	}

	/* Calloc group_set->ugroups  */
	group_set->ugroups_capacity=capacity;
	group_set->ugroups = calloc(group_set->ugroups_capacity, sizeof(EGI_UNIHANGROUP));
	if(group_set->ugroups==NULL) {
		printf("%s: Fail to calloc group_set->ugroups.\n",__func__);
		free(group_set);
		return NULL;
	}

	/* Calloc group_set->uchars */
	group_set->uchars_capacity=UHGROUP_UCHARS_GROW_SIZE;
        group_set->uchars = calloc(1, group_set->uchars_capacity);
        if(group_set->uchars==NULL) {
                printf("%s: Fail to calloc group_set->uchars.\n",__func__);
		UniHanGroup_free_set(&group_set);
                return NULL;
        }

	/* Calloc group_set->typings */
	group_set->typings_capacity=UHGROUP_TYPINGS_GROW_SIZE;
        group_set->typings = calloc(1, group_set->typings_capacity);
        if(group_set->typings==NULL) {
                printf("%s: Fail to calloc group_set->typings.\n",__func__);
		UniHanGroup_free_set(&group_set);
                return NULL;
        }

	/* Assign other members */
	group_set->sorder=UNIORDER_NONE;
	if(name != NULL)
		strncpy(group_set->name, name, sizeof(group_set->name)-1);

	return group_set;
}


/*---------------------------------------
	Free an EGI_UNIHANGROUP_SET
----------------------------------------*/
void UniHanGroup_free_set( EGI_UNIHANGROUP_SET **set)
{
	if( set==NULL || *set==NULL )
		return;

	free( (*set)->ugroups );
	free( (*set)->uchars );
	free( (*set)->typings );
	free(*set);
	*set=NULL;
}


/*----------------------------------------------------------------
Read an UFT-8 text file containing unihan_groups(words/phrases/Cizus)
and load them to an EGI_UNIHANGROUP_SET struct.

Note:
1. Get rid of Byte_Order_Mark(U+FEFF) in the text file before calling
   this function.

2. Each unihan_group(words/phrases/Cizu) MUST occupy one line in the
text file, such as:
...
普通
武则天
日新月异
...

@fpath:		Full path to the text file.

Return:
	A pointer to EGI_UNIHANGROUP_SET	OK
	Null					Fails
-----------------------------------------------------------------*/
EGI_UNIHANGROUP_SET*    UniHanGroup_load_CizuTxt(const char *fpath)
{
	EGI_UNIHANGROUP_SET* group_set=NULL;

	FILE *fil;
	char linebuff[128];
	int i;
	int k; 			 	/* For counting unihan groups */
	int nch;		  	/* Number of UNICODEs */
	unsigned int pos_uchar=0;
	//unsigned int pos_typing=0;
	int len;
	int chsize;

	/* Open file */
	fil=fopen(fpath,"r");
	if(fil==NULL) {
		printf("%s: Fail to open '%s', ERR:%s.\n",__func__, fpath,  strerror(errno));
		return NULL;
	}
	else
		printf("%s: Open '%s' to read in and load unihan group set...\n", __func__, fpath);

	/* Allocate unihan group set */
	group_set=UniHanGroup_create_set("中文词组", UHGROUP_UGROUPS_GROW_SIZE);
	if(group_set==NULL) {
		printf("%s: Fail to create unihan group set!\n", __func__);
		return NULL;
	}

	/* Loop read in and parse to store to EGI_UNIHANGROUP_SET */
	k=0;
	while ( fgets(linebuff, sizeof(linebuff), fil) != NULL  )  /* fgets(): a terminating null byte ALWAYS is stored at last! */
	{
		/* Get rid of 0xFEFF Byte_Order_Mark if any */

		/* Get rid of NON_UNIHAN encoding chars at end. Example: '\n','\r',SPACE at end of each line */
		for( len=strlen(linebuff);
		     //linebuff[len-1] == '\n' || linebuff[len-1] == '\r' || linebuff[len-1] == ' ';
		     len>0 && (unsigned int)linebuff[len-1] < 0x80;	/* 0b10XXXXXX as end of UNIHAN UFT8 encoding */
		     len=strlen(linebuff) )
		{
			linebuff[len-1]='\0';
		}

		/* Check total number of unihans in the group */
		nch=cstr_strcount_uft8((const UFT8_PCHAR)linebuff);
		if( nch < 1 ) {
			/* TEST ---- */
			#if 0
			printf("%s: nch=cstr_strcount_uft8()=%d \n",__func__, nch);
			getchar();
			#endif
			continue;
		}
		else if( nch > UHGROUP_WCODES_MAXSIZE ) {
			printf("%s:%s nch=%d, Too many wcodes for a UNIHAN_GROUP!\n", __func__, linebuff, nch);
			continue;
		}

	/* 1. --- ugroup --- */
		/* 1.1 Check ugroups mem */
		if( group_set->ugroups_size == group_set->ugroups_capacity ) {
			if( egi_mem_grow( (void **)&group_set->ugroups,
						group_set->ugroups_capacity*sizeof(EGI_UNIHANGROUP),
						      UHGROUP_UGROUPS_GROW_SIZE*sizeof(EGI_UNIHANGROUP) ) != 0 )
			{
                                        printf("%s: Fail to mem grow group_set->ugroups!\n", __func__);
					UniHanGroup_free_set(&group_set);
                                        goto END_FUNC;
                        }
			/* Update capacity */
			group_set->ugroups_capacity += UHGROUP_UGROUPS_GROW_SIZE;
		}

		/* 1.2 Store wcodes[] */
		len=0; chsize=0;
		for(i=0; i<nch; i++) {
			chsize= char_uft8_to_unicode((const UFT8_PCHAR)linebuff+len, group_set->ugroups[k].wcodes+i);
			if(chsize<0) {
				UniHanGroup_free_set(&group_set);
				goto END_FUNC;
			}
			len+=chsize;
		}

		/* NOW: len==strlen(linebuff) */
		/* 1.3 Update ugroups_size */
		group_set->ugroups_size +=1;
		/* 1.4 Store pos_uchar */
		group_set->ugroups[k].pos_uchar=pos_uchar;
		//group_set->ugroups[k].pos_typing=pos_typing;

	/* 2. --- uchars --- */
                /* 2.1 Check uchars mem */
                if( group_set->uchars_size+len+1 > group_set->uchars_capacity ) {		/* +1 EOF */
                        if( egi_mem_grow( (void **)&group_set->uchars,
                                                group_set->uchars_capacity*1, 				//sizeof(typeof(*group_set->uchars)),
                                                      UHGROUP_UCHARS_GROW_SIZE*1 ) != 0 )
                        {
                                        printf("%s: Fail to mem grow group_set->uchars!\n", __func__);
                                        UniHanGroup_free_set(&group_set);
                                        goto END_FUNC;
                        }
                        /* Update capacity */
                        group_set->uchars_capacity += UHGROUP_UCHARS_GROW_SIZE;
                }

		/* 2.2 Store uchars[] */
		strncpy((char *)group_set->uchars+pos_uchar, linebuff, len+1); /* fget() ensure a '\0' at last of linebuff */

                /* Test --- */
		#if 1
                if( nch<5 && k<3 ) {
			printf("nch=%d [%s: ",nch,group_set->uchars+pos_uchar);
               		for(i=0; i<nch; i++) {
                                printf("U+%X ", group_set->ugroups[k].wcodes[i]);
                	}
                	printf("] \n");
		}
		#endif

		/* 2.3 Update pos_uchar for next group */
		pos_uchar += len+1;	/* Add a '\0' as delimiter */

		/* 2.4: update uchars_size */
		group_set->uchars_size = pos_uchar; /* SAME AS += len+1, as uchar_size starts from 0 */

		/* Count number of unihan groups */
		k++;  	/* group_set->ugroups_size updated in 1.3 */
	}

//	printf(" group_set->ugroups[803].wcodes[0]: U+%X \n", group_set->ugroups[803].wcodes[0]);
	printf("%s: Finish reading %d unihan groups into group_set!\n",__func__, k);

	/* 3. --- typings --- */

END_FUNC:
        /* Close fil */
        if( fclose(fil) !=0 )
                printf("%s: Fail to close file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));

	return group_set;
}


/*----------------------------------------------------------------
Assemble typings for each unihan_group in ugroup_set, by copying
typing of each UNIHAN from the uhan_set.
If the UNIHAN is polyphonic, ONLY the first typing will be used!
so it MAY NOT be correct for some unihan_groups/phrases!

Note:
1.If char of Unihan Group is NOT included in the han_set, it will
  fail to assemble typing!
  	ＡＢ公司  Fail to UniHan_locate_wcode

2.If the UNIHAN is polyphonic, ONLY the first typing will be used!
  so it MAY NOT be correct for some unihan_groups/phrases!
	22511: 谋求 mo qiu   (x)
	22512: 谋取 mo qu    (x)
	地图: de tu

@ugroup_set:	Unihan Group Set without typings.
@uhan_set:	Unihan Set with typings.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------*/
int UniHanGroup_assemble_typings(EGI_UNIHANGROUP_SET *group_set, EGI_UNIHAN_SET *han_set)
{
	int i,j;
	int nch;
	unsigned int pos_typing=0;

	if( group_set==NULL || group_set->ugroups_size < 1 )
		return -1;

	if( group_set->typings_size != 0 )
		return -2;

	if( han_set==NULL || han_set->size < 1 )
		return -3;

	/* Sort han_set to UNIORDER_WCODE_TYPING_FREQ */
	if( UniHan_quickSort_set(han_set, UNIORDER_WCODE_TYPING_FREQ, 10) !=0 ) {
		printf("%s: Fail to quickSort han_set to UNIORDER_WCODE_TYPING_FREQ!\n",__func__);
		return -4;
	}

	/* Tranverse UNIHAN_GROUPs */
	pos_typing=0;
	for(i=0; i < group_set->ugroups_size; i++)
	{
		/* 1. Count number of UNICODES in wcodes[] */
		nch=UHGROUP_WCODES_MAXSIZE;
		while( nch>0 && group_set->ugroups[i].wcodes[nch-1] == 0 ) { nch--; };

		/* ?? necessary ?? */
		if(nch==0) {
			printf("%s: group_set->ugroups[%d].wcodes[0] is EMPTY!\n", __func__, i);
			return -5;
		}

		/* 2. Check typings mem */
                if( group_set->typings_size + nch*UNIHAN_TYPING_MAXLEN > group_set->typings_capacity ) {
                        if( egi_mem_grow( (void **)&group_set->typings,
                                                group_set->typings_capacity*1, 				//sizeof(typeof(*group_set->typings)),
                                                      UHGROUP_TYPINGS_GROW_SIZE*1 ) != 0 )
                        {
                                        printf("%s: Fail to mem grow group_set->typings!\n", __func__);
					return -6;
                        }
                        /* Update capacity */
                        group_set->typings_capacity += UHGROUP_TYPINGS_GROW_SIZE;
                }

		/* TEST --- */
//		printf("%s  ", group_set->uchars+group_set->ugroups[i].pos_uchar);

		/* 3. Get typing of each UNIHAN */
		for(j=0; j<nch; j++)
		{
			if( UniHan_locate_wcode(han_set, group_set->ugroups[i].wcodes[j]) ==0 ) {
			    	strncpy( group_set->typings+pos_typing+j*UNIHAN_TYPING_MAXLEN,
						han_set->unihans[han_set->puh].typing, UNIHAN_TYPING_MAXLEN);
				/* TEST --- */
//				printf("%s ", han_set->unihans[han_set->puh].typing);
			}
			#if 0  /* TEST --- */
			else {
				printf("%s: Fail to UniHan_locate_wcode U+%X (%s), press a key to continue.\n",
						__func__, group_set->ugroups[i].wcodes[j], group_set->uchars+ group_set->ugroups[i].pos_uchar );
				getchar();
				/* Though empty typing for this UNIHAN, go on to assign pos_typing and keep space. */
			}
			#endif
		}

		/* 4. Assign pos_typing for ugroups[i] */
		group_set->ugroups[i].pos_typing=pos_typing;

		/* 5. Update pos_typing for NEXT UNIHAN_GROUP */
		pos_typing += nch*UNIHAN_TYPING_MAXLEN;

		/* 6. Update typings_size */
		group_set->typings_size = pos_typing; /* SAME AS: += nch*UNIHAN_TYPING_MAXLE, as typings_size starts from 0 */
	}

	return 0;
}

/*-----------------------------------
Get number of unihans in a UNIHANGROUP

------------------------------------*/
int UniHanGroup_wcodes_size(const EGI_UNIHANGROUP *group)
{


}

/*---------------------------------------------------
Print out  unihan_goups in a group set.

@group_set	Unihan Group Set
@start		Start index of ugroups[]
@end		End index of ugroups[]
---------------------------------------------------*/
void UniHanGroup_print_set(const EGI_UNIHANGROUP_SET *group_set, unsigned int start, unsigned int end)
{
	int i,j;
	int nch;

	if(group_set==NULL)
		return;

	/* Limit start, end */
	if( end > group_set->ugroups_size-1 )
		end=group_set->ugroups_size-1;

	for(i=start; i<end+1; i++)
	{
                /* Count number of UNICODES in wcodes[] */
                nch=UHGROUP_WCODES_MAXSIZE;
                while( nch>0 && group_set->ugroups[i].wcodes[nch-1] == 0 ) { nch--; };

		/* printt typings */
//		if( group_set->typings[0] != '\0' ) // && nch==1 )
		{
			printf("%d: %s (",i, group_set->uchars+group_set->ugroups[i].pos_uchar);
			for(j=0; j<nch; j++)
				printf("%s%s",group_set->typings+group_set->ugroups[i].pos_typing+j*UNIHAN_TYPING_MAXLEN,
					      j==nch-1?")":" ");
			printf("\n");
		}
	}
	printf("Group set '%s': Contains %d unihan groups.\n", group_set->name, group_set->ugroups_size);
}


/*--------------------------------------------------------------
Print out UCHAR and TYPINGS of an unihan_group(cizu/phrase/words)
from a group set.

@group_set:	Unihan Group Set
@index:		index of group_set->ugroups[].
---------------------------------------------------------------*/
void UniHanGroup_print_group(const EGI_UNIHANGROUP_SET *group_set, unsigned int index)
{
	int k;
	int nch;

	if(group_set==NULL)
		return;

	/* Limit index */
	if( index > group_set->ugroups_size-1 )
		return;

        /* Count number of UNICODES in wcodes[] */
        nch=UHGROUP_WCODES_MAXSIZE;
        while( nch>0 && group_set->ugroups[index].wcodes[nch-1] == 0 ) { nch--; };

	/* Print unihans and typings */
	printf("%s: ",group_set->uchars+group_set->ugroups[index].pos_uchar);
	for(k=0; k<nch; k++) {
		printf("%s ",group_set->typings+group_set->ugroups[index].pos_typing+k*UNIHAN_TYPING_MAXLEN);
	}
	printf("\n");
}


/*---------------------------------------------------
Search all unihan groups in a grou set to find out
the input uchar, then print the unihan_group.

@group_set:	Unihan Group Set
@uchar:		Pointer to an UCHAR in UFT-8 enconding.
---------------------------------------------------*/
void UniHanGroup_search_uchar(const EGI_UNIHANGROUP_SET *group_set, UFT8_PCHAR uchar)
{
	int k;
	char *pc;

	if(group_set==NULL || uchar==NULL)
		return;

	for(k=0; k < group_set->ugroups_size; k++) {
		pc=(char *)group_set->uchars+group_set->ugroups[k].pos_uchar;
		if( strstr( pc, (char *)uchar)==pc ) {
			printf("ugroups[%d]: ",k);
			UniHanGroup_print_group(group_set, k);
		}
	}
}


/*----------------------------------------------------------------------------
Compare typing+freq ORDER of two unihan_groupss and return an integer less
than, equal to, or greater than zero, if unha1 is found to be ahead of,
at same order of, or behind uhans2 in dictionary order respectively.

Note:
1. !!! CAUTION !!!  Here typing refers to PINYIN, other type MAY NOT work.
2. All typing letters MUST be lowercase.
2. It frist compares number of unihans in the group, the greater one is
   AFTER the smaller one.
3. Then it compares their dictionary order, if NOT in the same order, return
the result; if in the same order, then compare their frequency number to
decide the order.
4. If group1->freq is GREATER than group2->freq, it returns -1(IS_AHEAD);
   else if group1->freq is LESS than group2->freq, then return 1(IS_AFTER);
   if EQUAL, it returns 0(IS_SAME).
5. An empty unihan group(nch==0) is always 'AFTER' a nonempty one.
6. A NULL is always 'AFTER' a non_NULL one.

@group1, group2:	Unihan groups for comparision.
@group_set:		Group Set that holds above uinhangroups.

Return:
	Relative Priority Sequence position:
	CMPORDER_IS_AHEAD    -1 (<0)	group1 is  'less than' OR 'ahead of' group2
	CMPORDER_IS_SAME      0 (=0)    group1 and group2 are at the same order.
	CMPORDER_IS_AFTER     1 (>0)    group1 is  'greater than' OR 'behind' group2.
-----------------------------------------------------------------------------------*/
int UniHanGroup_compare_typing( const EGI_UNIHANGROUP *group1, const EGI_UNIHANGROUP *group2, const EGI_UNIHANGROUP_SET *group_set)
{
	int k,i;
	int nch1, nch2;
	char c1,c2;

	/* If group_set is NULL */
	if(group_set==NULL || group_set->ugroups==NULL)
		return CMPORDER_IS_SAME;

	/* 1. If uhan1 and/or uhan2 is NULL */
	if( group1==NULL && group2==NULL )
		return CMPORDER_IS_SAME;
	else if( group1==NULL )
		return CMPORDER_IS_AFTER;
	else if( group2==NULL )
		return CMPORDER_IS_AHEAD;

	/* 2. Get number of chars of each group */
        nch1=UHGROUP_WCODES_MAXSIZE;
        while( nch1>0 && group1->wcodes[nch1-1] == 0 ) { nch1--; };
        nch2=UHGROUP_WCODES_MAXSIZE;
        while( nch2>0 && group2->wcodes[nch2-1] == 0 ) { nch2--; };

	/* 3. Put the empty ugroup to the last, OR check typing[0] ?
	 *     It also screens out nch1==0 AND/OR nch2==0 !!
	 */
	//if(group1->wcodes[0]==0 && group2->wcodes[0] !=0 )
	if( nch1==0 && nch1!=0 )
		return CMPORDER_IS_AFTER;
	//else if(group1->wcodes[0]!=0 && group2->wcodes[0]==0)
	else if( nch1!=0 && nch2==0 )
		return CMPORDER_IS_AHEAD;
	//else if(group1->wcodes[0]==0 && group2->wcodes[0]==0)
	else if( nch1==0 && nch2==0 )
		return CMPORDER_IS_SAME;

	/* 4. NOW nch1>0 and nch2>0: Compare nch of group */
	if( nch1 > nch2 )
		return CMPORDER_IS_AFTER;
	else if( nch1 < nch2 )
		return CMPORDER_IS_AHEAD;
	/* ELSE: ch1==ch2 */

	/* 5. Compare dictionary oder of group typing[] */
	for( k=0; k<UHGROUP_WCODES_MAXSIZE; k++)
	{
		/* Check nch1, nch2 whichever gets first */
		if( nch1==k || nch2==k ) {
			if(nch1==k && nch2==k)
				return CMPORDER_IS_SAME;
			else if(nch1==k)
				return CMPORDER_IS_AHEAD;
			else if(nch2==k)
				return CMPORDER_IS_AFTER;
		}

		/* Compare typing for each wcode */
		for( i=0; i<UNIHAN_TYPING_MAXLEN; i++) {
			c1=group_set->typings[group1->pos_typing+k*UNIHAN_TYPING_MAXLEN+i];
			c2=group_set->typings[group2->pos_typing+k*UNIHAN_TYPING_MAXLEN+i];
			if( c1 > c2 )
				return CMPORDER_IS_AFTER;
			else if( c1 < c2 )
				return CMPORDER_IS_AHEAD;
			/* End of both typings, EOF */
			else if( c1==0 && c2==0 )
				break;
		}
	}
	/* NOW: CMPORDER_IS_SAME */

	/* 6. Compare frequencey number: EGI_UNIHAN.freq */
	if( group1->freq > group2->freq )
		return CMPORDER_IS_AHEAD;
	else if( group1->freq < group2->freq )
		return CMPORDER_IS_AFTER;
	else
		return CMPORDER_IS_SAME;

}

/*----------------------------------------------------------------------------------
		( This function is for UniHanGroup_locate_typings()! )
Compare two group_typings only, without considering freq.

                        !!! WARNING !!!
1. The caller MUST ensure UHGROUP_WCODES_MAXSIZE*UNIHAN_TYPING_MAXLEN bytes
   mem space for both hold_typings AND try_typings!
2. And they MUST NOT be NULL.
3. If they are both empty(all 0s), it will return CMPORDER_IS_SAME.
4. Consecutive Comparing AND Interval/short_typing Comparing
   'short_typing' means the typings is represeted by intial XINPIN alphabets.
    Example:  'xiao mi' --> 'x m'	'qian qi bai guai' -->  'q q b g'

   compare short_typing (group_typing2) "c j d q" with group_typing1
		...
	"chang hong dian qi"
	"chang jia da qiao"
	"cheng ji duo qu"
		...
   Consecutive comparing:  "c j d q" IS_AHEAD of "chang hong dian qi".
   Interval comparing:     "c j d q" IS_AFTER  "chang hong dian qi".

5. For shortyping comparing. Size of group MUST be same! as original grout_set
   are sorted in ascending alphabetic order.
   Example 'y f f s' will be compared to be AFTER 'yi fang'
	....
   	yi fan
	yi fan feng shun
   	yi fang
	....

@group_typing1:	Pointer to group1 typings
@nch1:		Number of UNIHANs in group1

@group_typing2:	Pointer to group2 typings  ( MAYBE  short_typing )
@nch2:		Number of UNIHANs in group2


Return:
	Relative Priority Sequence position:
	CMPORDER_IS_AHEAD    -1 (<0)   typing1 is  'less than' OR 'ahead of' typing2
	CMPORDER_IS_SAME      0 (=0)   typing1 and unhan2 are at the same order.
	CMPORDER_IS_AFTER     1 (>0)   typing1 is  'greater than' OR 'behind' typing2.
------------------------------------------------------------------------------------*/
static inline int compare_group_typings( const char *group_typing1, int nch1, const char *group_typing2, int nch2,
					 bool TYPING2_IS_SHORT )
{
	int k,i;
	char c1,c2;


	/* Compare nch */
    	if( nch1 > nch2 )
		return  CMPORDER_IS_AFTER;
    	else if( nch1 < nch2 )
		return CMPORDER_IS_AHEAD;
	/* NOW: nch1==nch2 */

    /* OPTION 1:	Interval(short_typing) comparing,  group_typing2 as short_typing!  */
    if(TYPING2_IS_SHORT) //&& nch1==nch2 )
    {
        /* Compare dictionary order of group typing[] */
        for( k=0; k<UHGROUP_WCODES_MAXSIZE; k++)
        {
		/* Check nch1, nch2 whichever gets first, here k means number of compared typings */
		if( nch1==k || nch2==k ) {			/* Also limit input nch1,ch2 */
			if(nch1==k && nch2==k)
				return CMPORDER_IS_SAME;
			else if(nch1==k)
				return CMPORDER_IS_AHEAD;
			else if(nch2==k)
				return CMPORDER_IS_SAME; 	/* For short_typing, NOT AFTER */
		}

                /* Compare typing for each wcode */
                for( i=0; i<UNIHAN_TYPING_MAXLEN; i++) {
                        c1=group_typing1[k*UNIHAN_TYPING_MAXLEN+i];
                        c2=group_typing2[k*UNIHAN_TYPING_MAXLEN+i];
                        if( c1 > c2 ) {
				if( c2 !=0 )
	                                return CMPORDER_IS_AFTER;
				else
					break;			/* Fort short_typing: c1>c2 && c2==0, IS_SAME */
			}
                        else if( c1 < c2 )
                                return CMPORDER_IS_AHEAD;
                        /* Gets to the end of both typings, EOF */
                        else if( c1==0 && c2==0 )
                                break;

			/* Continue when c1==c2 */
                }
        }
	return CMPORDER_IS_SAME;
    }

    /* OPTION 2:  Consecutive(complete) comparing */
    else
    {
        /* Compare dictionary order of group typing[] */
        for( k=0; k<UHGROUP_WCODES_MAXSIZE; k++)
        {
		/* Check nch1, nch2 whichever gets first, here k means number of compared typings */
		if( nch1==k || nch2==k ) {			/* Also limit input nch1,ch2 */
			if(nch1==k && nch2==k)
				return CMPORDER_IS_SAME;
			else if(nch1==k)
				return CMPORDER_IS_AHEAD;
			else if(nch2==k)
				return CMPORDER_IS_AFTER;
		}

                /* Compare typing for each wcode */
                for( i=0; i<UNIHAN_TYPING_MAXLEN; i++) {
                        c1=group_typing1[k*UNIHAN_TYPING_MAXLEN+i];
                        c2=group_typing2[k*UNIHAN_TYPING_MAXLEN+i];
                        if( c1 > c2 )
                                return CMPORDER_IS_AFTER;
                        else if( c1 < c2 )
                                return CMPORDER_IS_AHEAD;
                        /* End of both typings, EOF */
                        else if( c1==0 && c2==0 )
                                break;
                }
        }
	return CMPORDER_IS_SAME;
    }
}


/*------------------------------------------------------------------------
Sort an EGI_UNIHAN array by Insertion_Sort algorithm, to rearrange unihans
in ascending order of typing+freq. (typing:in dictionary order)

Also ref. to mat_insert_sort() in egi_math.c.

Note:
1.The caller MUST ensure ugroups has at least [start+n] memebers!
2.Input 'n' MAY includes empty unihans with typing[0]==0, and they will
  be rearranged to the end of the array.

@group_set:	Group Set.
@start:		Start index of group_set->ugroups[]
@n:             size of the array to be sorted, from [start] to [start+n-1].

------------------------------------------------------------------------*/
void UniHanGroup_insertSort_typing(EGI_UNIHANGROUP_SET *group_set, int start, int n)
{
        int i;          /* To tranverse elements in array, 1 --> n-1 */
        int k;          /* To decrease, k --> 1,  */
        EGI_UNIHANGROUP tmp;

	if( group_set==NULL || group_set->ugroups==NULL)
		return;

	/* Start sorting ONLY when i>1 */
        for( i=start+1; i< start+n; i++) {
                tmp=group_set->ugroups[i];   /* the inserting integer */

        	/* Slide the inseting integer left, until to the first smaller unihan  */
                //for( k=i; k>0 && UniHanGroup_compare_typing(unihans+k-1, &tmp)==CMPORDER_IS_AFTER; k--) {
		for( k=i; k>0 && UniHanGroup_compare_typing(group_set->ugroups+k-1, &tmp, group_set)==CMPORDER_IS_AFTER; k--) {
				//unihans[k]=unihans[k-1];   /* swap */
				group_set->ugroups[k]=group_set->ugroups[k-1];  /* swap */
		}

		/* Settle the inserting unihan at last swapped place */
                group_set->ugroups[k]=tmp;
        }
}

/*---------------------------------------------------------------------
Sort unihan groups by Quick_Sort algorithm, to rearrange unihan_groups
in ascending order of typing+freq.
To see UniHanGroup_compare_typing() for the details of order comparing.

Refert to mat_quick_sort() in egi_math.h.

 	!!! WARNING !!! This is a recursive function.

Note:
1. The caller MUST ensure start/end are within the legal range.

@group_set:	Group set that hold unihan_groups.
@start:		start index, as of group_set->ugroups[start]
@End:		end index, as of group_set->ugroups[end]
@cutoff:	cutoff value for switch to insert_sort.

Return:
	0	Ok
	<0	Fails
----------------------------------------------------------------------*/
int UniHanGroup_quickSort_typing(EGI_UNIHANGROUP_SET* group_set, unsigned int start, unsigned int end, int cutoff)
{
	int i=start;
	int j=end;
	int mid;
	EGI_UNIHANGROUP	tmp;
	EGI_UNIHANGROUP	pivot;

	/* Check input */
	if( group_set==NULL )
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
		if( UniHanGroup_compare_typing(group_set->ugroups+start, group_set->ugroups+mid, group_set)==CMPORDER_IS_AFTER ) {
			tmp=group_set->ugroups[start];
			group_set->ugroups[start]=group_set->ugroups[mid];
			group_set->ugroups[mid]=tmp;
                }
                /* Now: [start]<=array[mid] */

                /* IF: [mid] >= [start] > [end] */
                /* if( array[start] > array[end] ) { */
		if( UniHanGroup_compare_typing(group_set->ugroups+start, group_set->ugroups+end, group_set)==CMPORDER_IS_AFTER ) {
			tmp=group_set->ugroups[start]; /* [start] is center */
			group_set->ugroups[start]=group_set->ugroups[end];
			group_set->ugroups[end]=group_set->ugroups[mid];
			group_set->ugroups[mid]=tmp;
                }
                /* ELSE:   [start]<=[mid] AND [start]<=[end] */
                /* else if( array[mid] > array[end] ) { */
		if( UniHanGroup_compare_typing(group_set->ugroups+mid, group_set->ugroups+end, group_set)==CMPORDER_IS_AFTER ) {
                        /* If: [start]<=[end]<[mid] */
			tmp=group_set->ugroups[end]; /* [start] is center */
			group_set->ugroups[end]=group_set->ugroups[mid];
			group_set->ugroups[mid]=tmp;
                        /* Else: [start]<=[mid]<=[end] */
                }
                /* Now: array[start] <= array[mid] <= array[end] */

		pivot=group_set->ugroups[mid];

                /* Swap array[mid] and array[end-1], still keep array[start] <= array[mid] <= array[end]!   */
		tmp=group_set->ugroups[end-1]; /* [start] is center */
		group_set->ugroups[end-1]=group_set->ugroups[mid];
		group_set->ugroups[mid]=tmp;

        /* 1.2 Quick sort: array[start] ... array[end-1], array[end]  */
                i=start;
                j=end-1;   /* As already sorted to: array[start]<=pivot<=array[end-1]<=array[end], see 1. above */
                for(;;)
                {
                        /* Stop at array[i]>=pivot: We preset array[end-1]==pivot as sentinel, so i will stop at [end-1]  */
                        /* while( array[++i] < pivot ){ };   Acturally: array[++i] < array[end-1] which is the pivot memeber */
		        while ( UniHanGroup_compare_typing(group_set->ugroups+(++i), &pivot, group_set) ==CMPORDER_IS_AHEAD ) { };
                        /* Stop at array[j]<=pivot: We preset array[start]<=pivot as sentinel, so j will stop at [start]  */
                        /* while( array[--j] > pivot ){ }; */
		        while ( UniHanGroup_compare_typing(group_set->ugroups+(--j), &pivot, group_set) ==CMPORDER_IS_AFTER ) { };

                        if( i<j ) {
                                /* Swap array[i] and array[j] */
				tmp=group_set->ugroups[i];
				group_set->ugroups[i]=group_set->ugroups[j];
				group_set->ugroups[j]=tmp;
                        }
                        else {
                                break;
			}
		}
                /* Swap pivot memeber array[end-1] with array[i], we left this step at last.  */
		group_set->ugroups[end-1]=group_set->ugroups[i];
		group_set->ugroups[i]=pivot; /* Same as array[i]=array[end-1] */

        /* 1.3 Quick sort: recursive call for sorted parts. and leave array[i] as mid of the two parts */
		UniHanGroup_quickSort_typing(group_set, start, i-1, cutoff);
		UniHanGroup_quickSort_typing(group_set, i+1, end, cutoff);

        }
/* 2. Implement insertsort */
        else
		UniHanGroup_insertSort_typing(group_set, start, end-start+1);

	return 0;
}

/*-------------------------------------------------------------------------------
	( This function is for UniHanGroup_locate_typings()! )
If each segment of try_typings is located/contained from the beginning of hold_typings'
segments respectively, then it returns 0.

Note:
1. This function multiply calls strstr() to compare two UNIHANGROUP.typing[], each
with UHGROUP_WCODES_MAXSIZE segments of typings, and each segment occupys
UNIHAN_TYPING_MAXLEN bytes. Totally UHGROUP_WCODES_MAXSIZE*UNIHAN_TYPING_MAXLEN bytes.

2. Try_typings MAY be a short_typing.
   Examples:
        short_typing "ch h d q" are contained in "chang hong dian qi"

			!!! WARNING !!!
The caller MUST ensure UHGROUP_WCODES_MAXSIZE*UNIHAN_TYPING_MAXLEN bytes
mem space for both hold_typings AND try_typings!


@hold_typings: 	Typings expected to contain the try_typings.
@try_typings:   Typings expected to be contained in the hold_typings.
		And it MAY be short_typings.

Return:
	0	Ok,
	<0      Fails, or NOT contained
---------------------------------------------------------------------------------*/
inline int strstr_group_typings(const char *hold_typings, const char* try_typings)
{
	int i;

	for(i=0; i<UHGROUP_WCODES_MAXSIZE; i++) {
		if( strstr(hold_typings+UNIHAN_TYPING_MAXLEN*i, try_typings+UNIHAN_TYPING_MAXLEN*i)
		    != hold_typings+UNIHAN_TYPING_MAXLEN*i )
		{
			return -1;
		}
	}

	return 0;
}


/*-----------------------------------------------------------------------------------
To locate the index of first ugroups[] that contains the given typings in the
beginning of its typings(UNIHANGROUP.typings[]=ugroups[].typings[ugroup->pos_typing]).
and set group_set->pgrp as the index.
Notice that each wcode of UNIHANGROUP has one typing, and each UNIHANGROUP(cizu)
occupys UHGROUP_WCODES_MAXSIZE*UNIHAN_TYPING_MAXLEN bytes.

                        !!! IMPORTANT !!!
"Containing the given typing" means UHGROUP_WCODES_MAXSIZE(4) typings (
in ugroups->typings[ugroup->pos_typing] ) of the wcode are the SAME as teh give
typings, OR the given (4) typings are contained in beginning of
ugroups->typings[ugroup->pos_typing] respectively.

This criteria is ONLY based on PINYIN. Other types of typing may NOT comply.

Note:
1. The group_set MUST be prearranged in dictionary ascending order of typing+freq.
2. And all uuniset->unhihans[0-size-1] are assumed to be valid/normal, with
   wcode >0 AND typing[0]!='\0'.
3. Pronunciation tones of PINYIN are neglected.
4. Usually there are more than one unihans that have the save typing,
(same pronuciation, but different UNIHANs), and indexse of those unihans
should be consecutive in the uniset (see 1). Finally the firt index will
be located.
5. This algorithm depends on typing type PINYIN, others MAY NOT work.
6. TODO: Some polyphonic UNIHANs will fail to locate, as UniHanGroup_assemble_typings()
   does NOT know how to pick the right typings for an unihan group!
	 ...
	 地球: de qiu
	 朝三暮四: chao san mu si
	 出差: chu cha
	 锒铛: lang cheng
	 ...

@group_set      The target EGI_UNIHAN_SET.
@typings  	The wanted PINYIN typings, consists of UHGROUP_WCODES_MAXSIZE typing.
		It MUST occupys UHGROUP_WCODES_MAXSIZE*UNIHAN_TYPING_MAXLEN bytes,
		with each wcode corresponding to one typing of UNIHAN_TYPING_MAXLEN bytes.

Return:
        =0              OK, and set group_set->pgrp accordingly.
        <0              Fails, or no result.
---------------------------------------------------------------------------------------*/
int  UniHanGroup_locate_typings(EGI_UNIHANGROUP_SET* group_set, const char* typings)
{
	unsigned int start,end,mid;
	EGI_UNIHANGROUP *ugroups=NULL;
	int nch1,nch2;

	if(typings==NULL || typings[0] ==0 )
		return -1;

	if(group_set==NULL || group_set->ugroups==NULL || group_set->ugroups_size==0) /* If empty */
		return -1;

	/* TODO:  Check sort order */

	/* Get pointer to unihans */
	ugroups=group_set->ugroups;

	/* Mark start/end/mid of ugroups[] index */
	start=0;
	end=group_set->ugroups_size-1;
	mid=(start+end)/2;

	/* Get total number of unihans/unicodes presented in input typings */
       	nch2=UHGROUP_WCODES_MAXSIZE;
        while( nch2>0 && typings[(nch2-1)*UNIHAN_TYPING_MAXLEN]=='\0' ) { nch2--; };

	/* binary search for the typings  */
	/* While if NOT containing the typing string at beginning */
	//while( strstr(unihans[mid].typing, typing) != unihans[mid].typing ) {
	while( strstr_group_typings( group_set->typings+ugroups[mid].pos_typing, typings ) !=0 ) {

		/* check if searched all wcodes */
		if(start==end) {
			return -2;
		}

		/* !!! If mid+1 == end: then mid=(n+(n+1))/2 = n, and mid will never increase to n+1! */
		else if( start==mid && mid+1==end ) {   /* start !=0 */
			//if( unihans[mid+1].wcode==wcode ) {
			/* Check if given typing are contained in beginning of unihan[mid+1].typing. */
			//if( strstr(unihans[mid+1].typing, typing) == unihans[mid].typing ) {
			if(strstr_group_typings( group_set->typings+ugroups[mid+1].pos_typing,
						  				typings ) ==0 ) {
						  //group_set->typings+ugroups[mid].pos_typing ) ==0 ) {
				mid += 1;  /* Let mid be the index of group_set, see following... */
				break;
			}
			else
				return -3;
		}

		/* 2. Get total number of unihans/unicodes presented in ugroups[mid].wcodes[] */
        	nch1=UHGROUP_WCODES_MAXSIZE;
	        while( nch1>0 && ugroups[mid].wcodes[nch1-1] == 0 ) { nch1--; };

		//if( unihans[mid].wcode > wcode ) {   /* then search upper half */
		//if( UniHan_compare_typing(&unihans[mid], &tmphan)==CMPORDER_IS_AFTER ) {
		if( compare_group_typings(group_set->typings+ugroups[mid].pos_typing, nch1, typings, nch2, false)==CMPORDER_IS_AFTER ) {
			end=mid;
			mid=(start+end)/2;
			continue;
		}
		//else if( unihans[mid].wcode < wcode) {
		//if ( UniHan_compare_typing(&unihans[mid], &tmphan)==CMPORDER_IS_AHEAD ) {
		if( compare_group_typings(group_set->typings+ugroups[mid].pos_typing, nch1, typings, nch2, false)==CMPORDER_IS_AHEAD ) {
			start=mid;
			mid=(start+end)/2;
			continue;
		}

	        /* NOW CMPORDER_IS_SAME: To rule out abnormal unihan with wcode==0!
		 * UniHan_compare_typing() will return CMPORDER_IS_SAME if two unihans both with wocde==0 */
		//if( unihans[mid].wcode==0 )
		if( ugroups[mid].wcodes[0]==0 )
			return -3;
	}
	/* NOW unhihan[mid] has the same typing */

	/* Move up to locate the first unihan group that bearing the same typings. */
	/* Check if given typing are contained in beginning of unihan[mid-1].typing. */
	//while ( mid>0 && (strstr(unihans[mid-1].typing, typing) == unihans[mid-1].typing) ) { mid--; };
	while( mid>0 && strstr_group_typings(group_set->typings+ugroups[mid-1].pos_typing, typings) ==0 ) { mid--; };
	group_set->pgrp=mid;  /* assign to uniset->opuh */

	return 0;
}
