/*-----------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

        UNIHAN (Hanzi)          汉字
        UNIHAN_SET              汉字集
        UNIHANGROUP (Cizu)      词组
        UNIHANGROUP_SET         词组集
        PINYIN                  Chinese phoneticizing

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

3. Procedure for generating UniHan data file for EGI_PINYIN IME:
   3.1 	Run test_pinyin3500.c to generate and save PINYIN3500 UinHan set.
	fil=fopen(PINYIN3500_TXT_PATH,"r");
	read line to uniset....
   	UniHan_save_set(uniset, PINYIN3500_DATA_PATH)
   3.2 	Load unisetMandarin from text MANDARIN_TXT_PATH. (test_pinyin.c)
	unisetMandarin=UniHan_load_MandarinTxt(MANDARIN_TXT_PATH)
   3.3 	Merge unisetMandarin into uniset.
	UniHan_merge_set(unisetMandarin, uniset)

   	PINYIN3500_TXT_PATH ----> uniset   ------merge to --->  uniset
					     |
   MANDARIN_TXT_PATH ----> unisetMandarin ---|

   3.4 	Sort and purify uniset.
	UniHan_quickSort_set(uniset, UNIORDER_WCODE_TYPING_FREQ, 10)
        UniHan_purify_set(uniset)
   3.5 	Generate frequency values for UniHans in uniset.
	UniHan_poll_freq(uniset, "/mmc/xyj_all.txt") ....
   3.6 	Sort UniHan set.
	UniHan_quickSort_set(uniset, UNIORDER_TYPING_FREQ, 10)
   3.7 	Save to UniHan data.
	UniHan_save_set(uniset, UNIHANS_DATA_PATH)
   3.8. MANDARIN_TXT lacks PINYIN/readings for some polyphonic UNIHANs, example: 单(chan). If PINYIN3500_TXT
   	also misses that PINYIN/reading, manually add it in PINYIN3500_TXT, and then run above procedure again.

4. Procedure for generating UniHanGroup data file for EGI_PINYIN IME:
   4.1  Load UinHan data.
	han_set=UniHan_load_set(UNIHANS_DATA_PATH);
   4.2	Load UniHanGroup data from text. (  <----- OR text fed back from 4.7 -----
	Text line format: Cizu+PINYIN.
		Example:  词语　ci yu  (PINYIN may be omitted, and leave it to UniHanGroup_assemble_typings() ).
   	group_set=UniHanGroup_load_CizuTxt(CIZU_TXT_PATH);
   4.3	Assemble typings for UniHanGroup ( For text fed back from 4.7, MAY NOT necessary!  )
   	UniHanGroup_assemble_typings(group_set, han_set)
   4.4  Merge UniHan to UniHanGroup set. nch==1 unihan_groups!
	UniHanGroup_load_uniset(group_set, han_set)
   4.5  Sort UniHanGroup set.
   	UniHanGroup_quickSort_typings(group_set ...);
   4.6  Save to UniHanGroup data. It also contains all UniHans(as nch=1 unihan_groups)!
	UniHanGroup_save_set(group_set, UNIHANGROUPS_DATA_PATH).
   4.7  Save UniHanGroup to a text. 	( --------> for further manual PINYIN modification, then -------> Feed back to 4.2
	UniHanGroup_saveto_CizuTxt(group_set, "/tmp/cizu+pinyin.txt"), ONLY Cizu(nch>1 groups) are saved to text!
	Note: Since some assembled PINYINs are incorrect, we may modify them manually in the text, then reload
        to group_set by calling UniHanGroup_load_CizuTxt().
	!!! This text file will be improved iteratively !!!

   x.x  TODO: poll freq.

5. Expend UniHanGroup data by merging. 
   5.1  Collect new Words/Cizu/Phrases and put them into a text file.
	Text line fromat: Cizu+PINYIN.
		Example:  词语　ci yu  (PINYIN may be omitted)
       ( TODO: Also you may add new polyphonic UniHan in the text file and merged into group set,
               but it needs to export to unihan set later.)
   5.2  Read above text into a UniHanGroup_Set:
	group_expend=UniHanGroup_load_CizuTxt(PINYIN_NEW_WORDS_FPATH);
   5.3  Merge group_expend into group_set and purify to clear redundancy:
	UniHanGroup_merge_set(group_expend, group_set);
	UniHanGroup_purify_set(group_set);  Mem holes exists in uchar/typings after purification.
   5.4  Follow 4.5, 4.6 and 4.7 to save to UniHanGroup data for EGI_PINYIN IME.

6. Call UniHanGroup_update_typings() to modify typings.
	Example to correct Cizus containing '行':
	1.  Export group_test to a text file:
	    UniHanGroup_saveto_CizuTxt(group_set,"/mmc/group_test.txt").
	2.  Extract all Cizus/Phrases containing the same polyphoinc UNIHAN '行':
    	    cat /mmc/group_test.txt | grep 行 > /mmc/update_words.txt
	3.  Modify all typings in update_words.txt manually.
	4.  Then reload update_words.txt into update_set by calling UniHanGroup_load_CizuTxt().
	5.  Replace typings in group_set by calling UniHanGroup_update_typings(group_set, update_set);


Journal:
2021-06-15:
	1. Add polyphonic UNIHAN 盛(cheng) to unihans_pinyin.dat

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------------------*/
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
static inline int compare_group_typings( const char *group_typing1, int nch1, const char *group_typing2, int nch2);


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
1. The function will treat all Yunmus as Zero Shengmu PINTYINs!
2. Some Shengmu and Yunmu are NOT compitable:
   Example: xa＃,xo＃,xe＃,xv＃
	    be bia biu....
	    zhi zhing chin ching shin shing ---- OK.

@strp:		An unbroken string of alphabet chars in lowercases,
		terminated with a '\0'.
		Note:
		1. It SHOULD occupy UNIHAN_TYPING_MAXLEN*n bytes mem space,
		as the function will try to read the content in above range
		withou checking position of the string terminal NULL!
		2. A terminal NULL '\0' will be written to strp as end of
	        dividing n groups!

@pinyin:	To store parsed groups of PINYINs.
		Each group takes UNIHAN_TYPING_MAXLEN bytes, including '\0's.
		So it MUST hold at least UNIHAN_TYPING_MAXLEN*n bytes mem space.
@n:		Max. groups of PINYINs expected.

Return:
	>0	OK, total number of PINYIN typings as divided into.
	<=0	Fails
----------------------------------------------------------------------------*/
int  UniHan_divide_pinyin(char *strp, char *pinyin, int n)
{
	int i;
	int np;			/* count total number of PINYIN typings */
	const char *ps=NULL;		/* Start of a complete PINYIN typing (Shengmu+Yunmu OR Yunmu) */
	const char *pc=strp;
	bool	zero_shengmu; 		/* True if has NO Shengmu */
	bool Shengmu_Is_ZCS=false; 	/* z,zh,c,ch,s,sh */

	if( strp==NULL || pinyin ==NULL || n<1 )
		return -1;

	/* Clear data */
	memset(pinyin, 0, n*UNIHAN_TYPING_MAXLEN);
	np=0;

	for( i=0; i<n+1; i++) {
		while( *pc != '\0' ) {   /* pc always points to the start of a new Shengmu or Yunmu */

			/* If i==4: break. as a full end of 4 groups. See below explanation. */
			if( i==n )
				break;

			ps=pc;  /* mark start of a PINYIN typing  */

			zero_shengmu=false; /* Assume it has shengmu */

			Shengmu_Is_ZCS=false; /* z,zh,c,ch,s,sh */

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
					Shengmu_Is_ZCS=true;
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
					else if( *(pc+1)=='n') 		/* To appear on ! */
						pc +=2;
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
							/* To avoid incompitable combinations:
							 *   cin,cing,sin,sing,zin,zing; chin,ching,shin,shing,zhin,zhing
							 */
							if(Shengmu_Is_ZCS) {
								pc +=1;
								break;
							}

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
							else if( *(pc+2)=='n' )
								pc+=3;		/* Just to appear ion! */
							else if( *(pc+2)=='n' )  /* To appear ion !! */
								pc+=3;
							else
								pc+=2;	/* To appear 'io' */	/* i */
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
			break;; // break while()

		} /* while() */

	} /* for() */

	/* If i==n+1: full end of n groups. put terminal to pstr then!!
	 * Example: 'ling dian xing dong', 'jin yan jiao xiong'.
	 * Here 'ong' and 'iong' are special vowels, as 'on' and 'ion' are NOT vowels!
	 * Others 'iang'-'ia','ian', 'uang'-'ua','uan',... are all vowels!
	 */
	if(i==n+1)
		*(char *)pc='\0';  /* ps as start pointer of redundant chars. */

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
		if( UniHan_compare_wcode(unihans+start, unihans+mid)==CMPORDER_IS_AFTER ) {
                        tmp=unihans[start];
                        unihans[start]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* Now: [start]<=array[mid] */

                /* IF: [mid] >= [start] > [end] */
                /* if( array[start] > array[end] ) { */
		if( UniHan_compare_wcode(unihans+start, unihans+end)==CMPORDER_IS_AFTER ) {
                        tmp=unihans[start]; /* [start] is center */
                        unihans[start]=unihans[end];
                        unihans[end]=unihans[mid];
                        unihans[mid]=tmp;
                }
                /* ELSE:   [start]<=[mid] AND [start]<=[end] */
                /* else if( array[mid] > array[end] ) { */
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
			while( UniHan_compare_wcode(unihans+(++i), &pivot)==CMPORDER_IS_AHEAD ) { };
                        /* Stop at array[j]<=pivot: We preset array[start]<=pivot as sentinel, so j will stop at [start]  */
                        /* while( array[--j] > pivot ){ }; */
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

        /* Reset sorder before sorting */
        uniset->sorder=UNIORDER_NONE;

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
int UniHan_save_set(const EGI_UNIHAN_SET *uniset, const char *fpath)
{
	int i;
	int nwrite;
	int nmemb;
	FILE *fil;
	uint8_t nq;
	int ret=0;
	uint32_t size;		/* Total number of unihans in the uniset */
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
Read EGI_UNIHANs from a data file and load them to an EGI_UNIHAN_SET.

@fpath:		File path.

Return:
	A pointer to EGI_UNIHAN_SET	OK, however its data may not be complete
					if error occurs during file reading.
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
	char setname[UNIHAN_SETNAME_MAX];

	/* Open file */
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

        /* FReadin uniset name */
        nmemb=UNIHAN_SETNAME_MAX;
        nread=fread( setname, 1, nmemb, fil);
        if(nread < nmemb) {
                if(ferror(fil))
                        printf("%s: Fail to read uniset name.\n", __func__);
                else
                        printf("%s: WARNING! fread uniset name %d bytes of total %d bytes.\n", __func__, nread, nmemb);
		//UniHan_free_set(&uniset);
                goto END_FUNC;
        }
	/* Whatever, set EOF */
	setname[UNIHAN_SETNAME_MAX-1]='\0';

	/* Create UNISET */
	uniset=UniHan_create_set(setname, total); /* total is capacity, NOT size! */
	if(uniset==NULL) {
		printf("%s: Fail to create uniset!\n", __func__);
		goto END_FUNC;
	}
	/* Assign size */
	uniset->size=total;

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
		At least UNIHAN_TYPING_MAXLEN bytes of space with EOF!
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
			if( uniset->unihans[i].freq > (unsigned)(UINT_MAX-delt) )
				return -4;
			#endif

			/* Increase freq */
			uniset->unihans[i].freq += delt;

			/* Need to resort TYPING_FREQ for the given typing. */
			/* TODO: Test, responding time,... seems OK */
			uniset->sorder=UNIORDER_NONE;
			if( UniHan_quickSort_typing(uniset->unihans, start, end, 5) !=0 ) {
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


NOTE:
1. The caller MUST ensure enough space for a pinyin.
2. It will fail to convert following readings with Combining Diacritical Marks:
 	ê̄  [0xea  0x304]  ê̌ [0xea  0x30c]
        ḿ  [0x1e3f  0x20] m̀=[0x6d  0x300]

@reading:  A pointer to a reading, with uft-8 encoding.
	   example:  "tiě"
@pinyin:   A pointer to a pinyin, in ASCII chars.
	   example:  "tie3"

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
				printf("%s: uniset->unihans mem grow from %zu to %zu.\n", __func__, capacity-growsize, capacity);
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

2. The Mandarin text lacks PINYIN/readings for some polyphonic UNIHANs
   Example: 单(chan).

3. The text file can be generated by follow shell command:
   cat Unihan_Readings.txt | grep kMandarin > Mandarin.txt

4. To pick out some unicodes with strange written reading.
   Some readings are with Combining Diacritical Marks:
	ḿ =[0x1e3f  0x20]  m̀=[0x6d  0x300]

   Pick out ḿ
	U+5463	kMandarin	ḿ

5. The return EGI_UNIHAN_SET usually contains some empty unihans, as left
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
				printf("%s: uniset->unihans mem grow from %zu to %zu.\n", __func__, capacity-growsize, capacity);
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
		At least UNIHAN_TYPING_MAXLEN bytes of space with EOF!
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
			printf("%s: Mem grow capacity of uniset2->unihans from %zu to %zu units.\n",
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

	if(uniset==NULL || uniset->unihans==NULL)
		return -1;

	/* Print informatin */
	printf("%s: UNISET '%s' size=%u, capacity=%zu.\n", __func__,uniset->name, uniset->size, uniset->capacity);

	/* quickSort for UNIORDER_WCODE_TYPING_FREQ */
	if( uniset->sorder != UNIORDER_WCODE_TYPING_FREQ )
	{
		if( UniHan_quickSort_set(uniset, UNIORDER_WCODE_TYPING_FREQ, 10) != 0 ) {
			printf("%s: Fail to quickSort wcode!\n", __func__);
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

	/* Update size, Note: wcodes_sort range [0 capacity]. so it will NOT affect wcodes_sorting. */
	uniset->size -= k;

	/* Reset sorder */
	uniset->sorder=UNIORDER_NONE;

	/* quickSort wcode:  sort in wcode+typing+freq order, sorting_wcode range [0 capacity] */
	if( UniHan_quickSort_set(uniset, UNIORDER_WCODE_TYPING_FREQ, 10) != 0 ) {
		printf("%s: Fail to quickSort wcode after purification!\n", __func__);
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

	/* Calloc group_set->resutls */
	group_set->results_capacity=UHGROUP_RESULTS_GROW_SIZE;
        group_set->results = calloc(group_set->results_capacity, sizeof(typeof(*group_set->results)));
        if(group_set->results==NULL) {
                printf("%s: Fail to calloc group_set->results.\n",__func__);
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
	free( (*set)->results );
	free(*set);
	*set=NULL;
}


/*-----------------------------------------------------------------------
Read an UFT-8 text file containing unihan_groups(words/phrases/Cizus)
and load them to an EGI_UNIHANGROUP_SET struct.

Note:
1. Get rid of Byte_Order_Mark(U+FEFF) in the text file before calling
   this function.

2. Each unihan_group(words/phrases/Cizu) MUST occupy one line in the
text file, such as:
...
笑　	xiao
joke	xiao hua    ( 2.1 Non_UNIHAN: Ignored! )
笑话	xiao hua  9 ( 9 --- freq value of the group )
普通    	    ( 2.2 No typings: It still will reserve 2*UNIHAN_TYPING_MAXLEN bytes!)
武则天	　wu ze	    ( 2.3 Incomplete typings: Leave typings as blank.)
日新月异  ri xin yue yi
...

3. Even NO typing in text file, it will still reserve nch*UNIHAN_TYPING_MAXLEN
   mem space for the unihan_group.
   Example: '普通' as above.


@fpath:		Full path to the text file.

Return:
	A pointer to EGI_UNIHANGROUP_SET	OK
	Null					Fails
------------------------------------------------------------------------*/
EGI_UNIHANGROUP_SET*    UniHanGroup_load_CizuTxt(const char *fpath)
{
	EGI_UNIHANGROUP_SET* group_set=NULL;

	FILE *fil;
	char linebuff[256];
	unsigned int  Word_Max_Len=32;		/* Max. length of column words in each line,  including '\0'. */
	unsigned int  Max_Split_Words=6;  	/* Max number of words in linebuff[] as splitted by delimters, 1group+4typings+1freq=6 */
	char str_words[Max_Split_Words][Word_Max_Len]; /* See above */
	char *delim=" 	\r\n"; 			/* delimiters: TAB,SPACE.\r\n  for splitting linebuff[] into 5 words. see above. */
	int  m;
	char *pt=NULL;

	int i;
	int k; 			 	/* For counting unihan groups */
	int nch;		  	/* Number of UNICODEs */
	unsigned int pos_uchar;
	unsigned int pos_typing;
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
	pos_uchar=0;
	pos_typing=0;
	while ( fgets(linebuff, sizeof(linebuff), fil) != NULL  )  /* fgets(): a terminating null byte ALWAYS is stored at the end! */
	{
		/* Get rid of 0xFEFF Byte_Order_Mark if any */

		#if  0 ///// NOT NECESSARY NOW ///// /* Get rid of NON_UNIHAN encoding chars at end. Example: '\n','\r',SPACE at end of each line */
		for( len=strlen(linebuff);
		     //linebuff[len-1] == '\n' || linebuff[len-1] == '\r' || linebuff[len-1] == ' ';
		     len>0 && (unsigned int)linebuff[len-1] < 0x80;	/* 0b10XXXXXX as end of UNIHAN UFT8 encoding */
		     len=strlen(linebuff) )
		{
			linebuff[len-1]='\0';
		}
		#endif

		/* Readin words: uinhangroups + (0-4)typings */
		pt=strtok(linebuff, delim);
		bzero(&str_words[0][0], sizeof(str_words[0])*Max_Split_Words); /* Clear all! as nch is NOT available here. */
		for(m=0; pt!=NULL && m<Max_Split_Words; m++) {  /* Separate linebuff into 4 words */
			snprintf( str_words[m], Word_Max_Len-1, "%s", pt); /* pad EOF */
			pt=strtok(NULL, delim);
		}
		/* Check m, if NO word at all */
		if(m<1)continue;

		/* Check first byte to see if UFT8 encoding for an UNIHAN */
		if( str_words[0][0] < (unsigned int)0b11000000 ) {
			printf("%s: '%s' is Not an UNIHAN with UFT8 encoding!\n",__func__, str_words[0]);
			continue;
		}

		/* Check total number of unihans in the group */
		nch=cstr_strcount_uft8((const UFT8_PCHAR)str_words[0]);
		if( nch < 1 ) {
			/* TEST ---- */
			#if 0
			printf("%s: nch=cstr_strcount_uft8()=%d \n",__func__, nch);
			getchar();
			#endif
			continue;
		}
		else if( nch > UHGROUP_WCODES_MAXSIZE ) {
			//printf("%s:%s nch=%d > UHGROUP_WCODES_MAXSIZE=%d!\n", __func__, str_words[0], nch, UHGROUP_WCODES_MAXSIZE);
			continue;
		}

		/* Check number of typings: str_words 1: groups, m-1: typings */
		if( m-1 < nch ) {
		    	if( m-1 > 0 )
				printf("%s: Incomplete typings for '%s' ( ntypings=%d < nch=%d ). \n", __func__, str_words[0], m-1, nch);

			/* Clear incomplete typings */
			bzero(&str_words[1][0], sizeof(str_words[0])*(Max_Split_Words-1));
		}

#if 0	/* TEST--- */
		printf("%s: ",str_words[0]);
		for(i=1; i<m; i++)
			printf("%s ",str_words[i]);
		printf("\n");
		continue;
#endif

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
			chsize= char_uft8_to_unicode((const UFT8_PCHAR)str_words[0]+len, group_set->ugroups[k].wcodes+i);
			if(chsize<0) {
				UniHanGroup_free_set(&group_set);
				goto END_FUNC;
			}
			len+=chsize;
		}

		/* 1.3 Assign freq */
		if( m > nch+1 )
			group_set->ugroups[k].freq=atoi(str_words[nch+1]);

		/* NOW: len==strlen(str_words[0]) */

	/* 2. --- uchars --- */
		/* 2.0 Store pos_uchar */
		group_set->ugroups[k].pos_uchar=pos_uchar;

                /* 2.1 Check uchars mem */
                if( group_set->uchars_size+len+1 > group_set->uchars_capacity ) {		/* +1 '\0' delimiter  */
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
		strncpy((char *)group_set->uchars+pos_uchar, str_words[0], len+1); /* fget() ensure a '\0' at last of linebuff */

		/* 2.3 Update pos_uchar for next group */
		pos_uchar += len+1;	/* Add a '\0' as delimiter */

		/* 2.4: update uchars_size */
		group_set->uchars_size = pos_uchar; /* SAME AS += len+1, as uchar_size starts from 0 */

	/* 3. --- typings --- */
		/* 3.1 Check typings mem */
                if( group_set->typings_size + nch*UNIHAN_TYPING_MAXLEN > group_set->typings_capacity ) {
                        if( egi_mem_grow( (void **)&group_set->typings,
                                                group_set->typings_capacity*1, 				//sizeof(typeof(*group_set->typings)),
                                                      UHGROUP_TYPINGS_GROW_SIZE ) != 0 )
                        {
                                        printf("%s: Fail to mem grow group_set->typings!\n", __func__);
                                        UniHanGroup_free_set(&group_set);
                                        goto END_FUNC;
                        }
                        /* Update capacity */
                        group_set->typings_capacity += UHGROUP_TYPINGS_GROW_SIZE;
                }
		/* 3.2 Store typings[]. Note: Even NO typing text, we still reserve mem space for it.  */
		for(i=0; i<nch; i++) /* strcpy one by one, each str_words[] padded with a EOF */
			strncpy( group_set->typings+pos_typing+i*UNIHAN_TYPING_MAXLEN, str_words[1+i], UNIHAN_TYPING_MAXLEN);
		/* 3.3 Assign pos_typing for ugroups[i] */
		group_set->ugroups[k].pos_typing=pos_typing;
		/* 3.4 Update pos_typing for NEXT UNIHAN_GROUP */
		pos_typing += nch*UNIHAN_TYPING_MAXLEN;
		/* 3.5 Update typings_size */
		group_set->typings_size = pos_typing; /* SAME AS: += nch*UNIHAN_TYPING_MAXLE, as typings_size starts from 0 */

	/* 4. --- Count on --- */
		/* Count number of unihan groups */
		k++;
		/* Update ugroups_size */
		group_set->ugroups_size +=1;
	}
	printf("%s: Finish reading %d unihan groups into an unihan_group set!\n",__func__, k);

END_FUNC:
        /* Close fil */
        if( fclose(fil) !=0 )
                printf("%s: Fail to close file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));

	return group_set;
}


/*---------------------------------------------------------------
Save a group set to a Cizu text file. ONLY UNIHANGROUPs will be
saved and single UNIHANs will be screened out.

Note:
1. Only uchars and typings will be written to the text file,
   which is also compatible for UniHanGroup_load_CizuTxt() to
   read in.
...
(小　 single UNIHAN to be ignored )
普通  pu tong
武则天  wu ze tian
日新月异  ri xin yue yi
....

@group_set:	A pinter to an EGI_UNIHANGROUP_SET
@fpath:		Full file path of saved file.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------*/
int  UniHanGroup_saveto_CizuTxt(const EGI_UNIHANGROUP_SET *group_set, const char *fpath)
{
	int i,j;
	int nwrite;
	int len;
	FILE *fil;
	int ret=0;
	unsigned int off;
	int nch;

	/* Check group_set */
	if(group_set==NULL || group_set->ugroups_size<1 )
		return -1;

	/* Open/create file */
        fil=fopen(fpath, "w");
        if(fil==NULL) {
                printf("%s: Fail to open file '%s' for write. ERR:%s\n", __func__, fpath, strerror(errno));
                return -2;
        }

	/* Write comment at firt line. It will deemed as invalid line by UniHanGroup_load_CizuTxt(fpath)  */
	fprintf(fil, "### This text is generated from UniHanGroup set '%s', only nch>1 groups are exported.\n", group_set->name);

	/* Write all groups with nch>1 */
	for(i=0; i < group_set->ugroups_size; i++) {
	        /* Get number of chars of each group */
	        nch=UHGROUP_WCODES_MAXSIZE;
       		while( nch>0 && group_set->ugroups[i].wcodes[nch-1] == 0 ) { nch--; };

		/* Rule out single UNIHANs, which are treated separately. */
		if(nch<2)
			continue;

		/* write uchar */
		off=group_set->ugroups[i].pos_uchar;
		len=strlen((char *)group_set->uchars+off);
		nwrite=fprintf(fil,"%s ",group_set->uchars+off);
		if(len+1 != nwrite) /* +1 SPACE */
			printf("%s: WARNING! fprintf writes uchars %d of total %d bytes!\n", __func__, nwrite, len);

		/* write typings */
		off=group_set->ugroups[i].pos_typing;
		for(j=0; j<nch; j++) {
			len=strlen(group_set->typings+off+j*UNIHAN_TYPING_MAXLEN);
			nwrite=fprintf(fil,"%s ",group_set->typings+off+j*UNIHAN_TYPING_MAXLEN);
			if(len+1 != nwrite)
				printf("%s: WARNING! fprintf writes typings %d of total %d bytes!\n", __func__, nwrite, len);
		}

		/* write freq if NOT 0, with field width=10 */
		if(group_set->ugroups[i].freq !=0 ) {
			nwrite=fprintf(fil,"%10d", group_set->ugroups[i].freq); /* typings leaves a SPACE */
			if(nwrite!=10)
				printf("%s: WARNING! fprintf writes %d of total %d bytes!\n", __func__, nwrite, 10);
		}

		/* Feed line */
		fprintf(fil,"\n");
	}

        /* Close fil */
        if( fclose(fil) !=0 ) {
                printf("%s: Fail to close file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
                ret=-5;
        }

	return ret;
}


/*----------------------------------------------------------
Add an UNIHAN set into an UNIHANGROUP set, by adding all
unihans at the end of the group_set.

@group_set:	Pointer to an UNIHANGROUP_SET.
@uniset:	Pointer to an UINIHAN_SET.

Return:
	0	OK
	<0	Fails
----------------------------------------------------------*/
int UniHanGroup_add_uniset(EGI_UNIHANGROUP_SET *group_set, const EGI_UNIHAN_SET *uniset)
{
	int i;
	int k;
	int charlen;
	unsigned int pos_uchar=0;
	unsigned int pos_typing=0;

	if( group_set==NULL || group_set->ugroups==NULL || uniset==NULL )
		return -1;

	/* Get pos_uchar/pos_typing for new unihan(group) */
	/* !pitfall : After sorting, the last ugroups[] usually does NOT correspond to the last typings[] and uchars[]! */
	pos_uchar = group_set->uchars_size;
	pos_typing = group_set->typings_size;

	/* K as for NEW index of group_set->ugroups[], for the new unihan(group) */
	k=group_set->ugroups_size;

	for(i=0; i< uniset->size; i++)
	{

	/* 1. --- Append ugroups[] --- */
		/* 1.1 Check ugroups mem */
		if( group_set->ugroups_size == group_set->ugroups_capacity ) {
			if( egi_mem_grow( (void **)&group_set->ugroups,
						group_set->ugroups_capacity*sizeof(EGI_UNIHANGROUP),
						      UHGROUP_UGROUPS_GROW_SIZE*sizeof(EGI_UNIHANGROUP) ) != 0 )
			{
                                        printf("%s: Fail to mem grow group_set->ugroups!\n", __func__);
                                        return -3;
                        }
			/* Update capacity */
			group_set->ugroups_capacity += UHGROUP_UGROUPS_GROW_SIZE;
		}

		/* 1.2 Store wcodes[], nch==1, append ugroups at the end */
		group_set->ugroups[k].wcodes[0]=uniset->unihans[i].wcode;

	/* 2. --- Append uchars[] --- */
		/* 2.1 Store pos_uchar */
		group_set->ugroups[k].pos_uchar=pos_uchar;

		/* 2.2 Get new uchar len */
		charlen=cstr_charlen_uft8((UFT8_PCHAR)uniset->unihans[i].uchar);
		if(charlen<0) {
			printf("%s: uniset->unihans[%d].uchar data error!\n", __func__, i);
			return -4;
		}

                /* 2.3 Check uchars mem */
                if( group_set->uchars_size+charlen+1 > group_set->uchars_capacity ) {		/* +1 '\0' delimiter */
                        if( egi_mem_grow( (void **)&group_set->uchars,
                                                group_set->uchars_capacity*1, 				//sizeof(typeof(*group_set->uchars)),
                                                      UHGROUP_UCHARS_GROW_SIZE*1 ) != 0 )
                        {
                                        printf("%s: Fail to mem grow group_set->uchars!\n", __func__);
                                        return -5;
                        }
                        /* Update capacity */
                        group_set->uchars_capacity += UHGROUP_UCHARS_GROW_SIZE;
                }
		/* 2.4 Store uchars[] */
		strncpy((char *)group_set->uchars+pos_uchar, (char *)uniset->unihans[i].uchar, charlen+1); /*  */
		/* 2.5 Update pos_uchar for next group */
		pos_uchar += charlen+1;	/* Add a '\0' as delimiter */
		/* 2.6 update uchars_size */
		group_set->uchars_size = pos_uchar; /* SAME AS += len+1, as uchar_size starts from 0 */

	/* 3. --- Append typings[] --- */
		/* 3.1 Check typings mem */
                if( group_set->typings_size + UNIHAN_TYPING_MAXLEN > group_set->typings_capacity ) {
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
		/* 3.2 Store typings[]  */
		strncpy( group_set->typings+pos_typing, uniset->unihans[i].typing, UNIHAN_TYPING_MAXLEN);
		/* 3.3 Assign pos_typing for ugroups[i] */
		group_set->ugroups[k].pos_typing=pos_typing;
		/* 3.4 Update pos_typing for NEXT UNIHAN_GROUP */
		pos_typing += UNIHAN_TYPING_MAXLEN;
		/* 3.5 Update typings_size */
		group_set->typings_size = pos_typing; /* SAME AS: += nch*UNIHAN_TYPING_MAXLE, as typings_size starts from 0 */

	/* 4. --- Assign freq --- */
		group_set->ugroups[k].freq=uniset->unihans[i].freq;

	/* 5. --- Update size and k --- */
		/* 4.1 Update ugroups_size at last */
		group_set->ugroups_size +=1;
		/* 4.2 Increase index for next one */
		k++;
	}

        /* Reset sort order */
        group_set->sorder=UNIORDER_NONE;

	printf("%s: Finish loading, final ugroups_size=%d\n",__func__, group_set->ugroups_size);
	return 0;
}


/*-----------------------------------------------------------------------
Assemble typings for each unihan_group in ugroup_set, by copying
typing of each UNIHAN from the input uhan_set.
If the UNIHAN is polyphonic, ONLY the first typing will be used!
so it MAY NOT be correct for some unihan_groups/phrases!

Note:
1.Group_set->typings MUST already have been allocated for each UNIHAN!
2.If char of Unihan Group is NOT included in the han_set, it will
  fail to assemble typing!
  	ＡＢ公司  Fail to UniHan_locate_wcode
3.TODO: If the UNIHAN is polyphonic, ONLY the first typing will be used!
  so it MAY NOT be correct for some unihan_groups/phrases!
	22511: 谋求 mo qiu   (x)
	22512: 谋取 mo qu    (x)
	地图: de tu
4. If some group typings already exits, then it keeps intact.

@ugroup_set:	Unihan Group Set without typings.
@uhan_set:	Unihan Set with typings.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------------*/
int UniHanGroup_assemble_typings(EGI_UNIHANGROUP_SET *group_set, EGI_UNIHAN_SET *han_set)
{
	int i,j;
	int nch;
	unsigned off;

	if( group_set==NULL || group_set->typings==NULL || group_set->ugroups_size < 1 )
		return -1;

	if( han_set==NULL || han_set->size < 1 )
		return -2;

	/* Sort han_set to UNIORDER_WCODE_TYPING_FREQ */
	if( UniHan_quickSort_set(han_set, UNIORDER_WCODE_TYPING_FREQ, 10) !=0 ) {
		printf("%s: Fail to quickSort han_set to UNIORDER_WCODE_TYPING_FREQ!\n",__func__);
		return -4;
	}

	/* Traverse UNIHAN_GROUPs */
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

		/* TEST --- */
//		printf("%s  ", group_set->uchars+group_set->ugroups[i].pos_uchar);

		/* 3. Get typing of each UNIHAN */
		off=group_set->ugroups[i].pos_typing;
		if(*(group_set->typings+off)=='\0') {   /* ONLY IF its typings does NOT exist. */
			for(j=0; j<nch; j++) {
				if( UniHan_locate_wcode(han_set, group_set->ugroups[i].wcodes[j]) ==0 ) {
				    	strncpy( group_set->typings+off+j*UNIHAN_TYPING_MAXLEN,
							han_set->unihans[han_set->puh].typing, UNIHAN_TYPING_MAXLEN);
					/* TEST --- */
//					printf("%s ", han_set->unihans[han_set->puh].typing);
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
		}
	}

	return 0;
}


/*-----------------------------------------------------
Get the number of unihans that making up a UNIHANGROUP.
Return:
	>=0	The nmber of unihans in a group.
	<0	Fail
------------------------------------------------------*/
inline int UniHanGroup_wcodes_size(const EGI_UNIHANGROUP *group)
{
	if(group==NULL)
		return -1;

        /* Count number of UNICODES in wcodes[] */
        int nch=UHGROUP_WCODES_MAXSIZE;
        while( nch>0 && group->wcodes[nch-1] == 0 ) { nch--; };

	return nch;
}


/*---------------------------------------------------
Print out  unihan_goups in a group set.

@group_set	Unihan Group Set
@start		Start index of ugroups[]
@end		End index of ugroups[]
---------------------------------------------------*/
void UniHanGroup_print_set(const EGI_UNIHANGROUP_SET *group_set, int start, int end)
{
	int i,j;
	int nch;

	if(group_set==NULL || group_set->ugroups_size==0 )
		return;

	/* Limit start, end */
	if(start<0)start=0;
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
			for(j=0; j<nch; j++) {
				printf("%s%s",group_set->typings+group_set->ugroups[i].pos_typing+j*UNIHAN_TYPING_MAXLEN,
					      j==nch-1?")":" ");
			}
			for(j=0; j<UHGROUP_WCODES_MAXSIZE; j++)
				printf(" U+%d",group_set->ugroups[i].wcodes[j]);
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
@line_feed:	If true, add line feed at end.
---------------------------------------------------------------*/
void UniHanGroup_print_group(const EGI_UNIHANGROUP_SET *group_set, int index, bool line_feed)
{
	int k;
	int nch;

	if(group_set==NULL)
		return;

	/* Limit index */
	if( index<0 || index > group_set->ugroups_size-1)
		return;

        /* Count number of UNICODES in wcodes[] */
        nch=UHGROUP_WCODES_MAXSIZE;
        while( nch>0 && group_set->ugroups[index].wcodes[nch-1] == 0 ) { nch--; };

	/* Print unihans and typings */
	printf("%s: ",group_set->uchars+group_set->ugroups[index].pos_uchar);
	for(k=0; k<nch; k++) {
		printf("%s ",group_set->typings+group_set->ugroups[index].pos_typing+k*UNIHAN_TYPING_MAXLEN);
	}
        for(k=0; k<UHGROUP_WCODES_MAXSIZE; k++)
       	        printf(" U+%d",group_set->ugroups[index].wcodes[k]);

	if(line_feed)
		printf("\n");
}


/*-----------------------------------------------------------------
Print out UCHAR and TYPINGS of all unihan groups in the result set.
@group_set:     Unihan Group Set
------------------------------------------------------------------*/
void UniHanGroup_print_results(	const EGI_UNIHANGROUP_SET *group_set )
{
	int i;

	if(group_set==NULL)
		return;

	for(i=0; i < group_set->results_size; i++)
		UniHanGroup_print_group(group_set, group_set->results[i], true);

	printf("Group set results size=%zu\n",group_set->results_size);
}


/*-------------------------------------------------------
Search all unihan groups in a group set to find out
the input uchar, then print the unihan_group.

@group_set:	Unihan Group Set
@uchar:		Pointer to an UCHAR in UFT-8 enconding.
--------------------------------------------------------*/
void UniHanGroup_search_uchars(const EGI_UNIHANGROUP_SET *group_set, UFT8_PCHAR uchar)
{
	int k;
	char *pc;

	if(group_set==NULL || uchar==NULL)
		return;

	for(k=0; k < group_set->ugroups_size; k++) {
		pc=(char *)group_set->uchars+group_set->ugroups[k].pos_uchar;
		if( strstr( pc, (char *)uchar)==pc ) {
			printf("ugroups[%d]: ",k);
			UniHanGroup_print_group(group_set, k, true);
		}
	}
}


/*-----------------------------------------------------------------
Locate a string of uchars with UFT8 encoding in the given group_set.

@uchar:	A pointer to uchars in UFT8 encoding.
@group_set: A UNIHAN GROUP SET that contains the uchars.

Return:
	>=0	Index of group_set->ugroups[]
	<0	Fail
------------------------------------------------------------------*/
inline int UniHanGroup_locate_uchars(const EGI_UNIHANGROUP_SET *group_set, UFT8_PCHAR uchars)
{
	int i;
	unsigned int off;

	if( group_set==NULL || group_set->ugroups==NULL )
		return -1;

	for(i=0; i < group_set->ugroups_size; i++) {
		off=group_set->ugroups[i].pos_uchar;
		if( strcmp((char *)group_set->uchars+off,(char *)uchars)==0 )
			return i;
	}

	return -2;
}




/*-----------------------------------------------------------------------------
Compare nch+typing+freq ORDER of two unihan_groupss and return an integer less
than, equal to, or greater than zero, if group1 is found to be ahead of,
at same order of, or behind group2 in dictionary order respectively.

Comparing/sorting order (OPTION 1or2) MUST be the same as in compare_group_typings()!

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
7. Pending: groups with typings[0]==EOF.

@group1, group2:	Unihan groups for comparision.
@group_set:		Group Set that holds above uinhangroups.
//@enable_short_typing:
    If TRUE, then compare with byte order + typing order.
	Example: 'dong xia nan bei' ---> comparing order: d x n b o i a e n a n i g.
    Else see all typings as one string, and compare byte one by one.
    	Example: 'dong xia nan bei' ---> comparing order: d o n g x i a n a n b e i.

Return:
	Relative Priority Sequence position:
	CMPORDER_IS_AHEAD    -1 (<0)	group1 is  'less than' OR 'ahead of' group2
	CMPORDER_IS_SAME      0 (=0)    group1 and group2 are at the same order.
	CMPORDER_IS_AFTER     1 (>0)    group1 is  'greater than' OR 'behind' group2.
-------------------------------------------------------------------------------------*/
int UniHanGroup_compare_typings( const EGI_UNIHANGROUP *group1, const EGI_UNIHANGROUP *group2, const EGI_UNIHANGROUP_SET *group_set )
				//bool enable_short_typing )
{
	int k,i;
	int nch1, nch2;
	char c1,c2;

	/* If group_set is NULL */
	if(group_set==NULL || group_set->ugroups==NULL)
		return CMPORDER_IS_SAME;

	/* 1. If group1 and/or group2 is NULL */
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
	if( nch1==0 && nch2!=0 )
		return CMPORDER_IS_AFTER;
	else if( nch1!=0 && nch2==0 )
		return CMPORDER_IS_AHEAD;
	else if( nch1==0 && nch2==0 )
		return CMPORDER_IS_SAME;

	/* 4. NOW nch1>0 and nch2>0: Compare nch of group */
	if( nch1 > nch2 )
		return CMPORDER_IS_AFTER;
	else if( nch1 < nch2 )
		return CMPORDER_IS_AHEAD;
	/* ELSE: nch1==nch2 */

	/* 5. Compare order of group typing[] */

#if 1   /* OPTION 1 (OK): Compare each byte in typing, with byte order + typing order.
	This is for short_typing locating and sorting!
	typing[0][0], typing[1][0], typing[2][0], typiong[3][0], ...  ( 1. compare 0th byte, from 0-Nth typing )
	typing[0][1], typing[1][1], typing[2][1], typiong[3][1], ...  ( 2. compare 1th byte,...)
	typing[0][2], typing[1][2], typing[2][2], typiong[3][2], ...  ( 3. compare 2th byte,...)
	typing[0][3], typing[1][3], typing[2][3], typiong[3][3], ...  ( 3. compare 3th byte,...)
		... ...
	Example: 'dong xia nan bei' ---> comparing order: d x n b o i a e n a n i g
   */
//   if(enable_short_typing)
   {        /* Compare typing bytes */
        for( i=0; i<UNIHAN_TYPING_MAXLEN; i++) {		/* tranverse typing byte order */
		for( k=0; k<nch1; k++) {			/* tranverse wcode/typing order, nch1==nch1  */
			c1=group_set->typings[group1->pos_typing+k*UNIHAN_TYPING_MAXLEN+i];
			c2=group_set->typings[group2->pos_typing+k*UNIHAN_TYPING_MAXLEN+i];
                        if( c1 > c2 )
                                return CMPORDER_IS_AFTER;
			else if( c1 < c2 )
				return CMPORDER_IS_AHEAD;
		}
	}
   }
#else   /* OPTION 2:  see all typings as one string, and compare byte one by one.
	typing[0][0], typing[0][1], typing[0][2], typiong[0][3], ...  ( 1. compare 0th typing, from 0-Nth byte )
	typing[1][0], typing[1][1], typing[1][2], typiong[1][3], ...  ( 2. compare 1th typing,...)
	typing[2][0], typing[2][1], typing[2][2], typiong[2][3], ...  ( 3. compare 2th typing,... )
	typing[3][0], typing[3][1], typing[3][2], typiong[3][3], ...  ( 3. compare 3th typing,... )
		... ...
	Example: 'dong xia nan bei' ---> comparing order: d o n g x i a n a n b e i.
    */
   //else  /* disable_short_typing */
   {
	for( k=0; k<nch1; k++)
	{
		/* Compare typing bytes */
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
   }
#endif

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
Compare two group_typings only in nch+typing order, without considering freq.

Comparing/sorting order (OPTION 1or2) MUST be the same as in UniHanGroup_compare_typings()!

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

	!!! --------- WRONG -----------!!!

	'z c s k' will be AHEAD of 'zun yi hui yi' if enable_short_typing=false , while ALSO  AFTER 'zuo bi shang guan'.
	 ( sort order: nch + typing + )
	zun xun yuan ze
	zun yi hui yi
	zun zhao zhi hang
	zuo bang you bet
	zuo bi shang guan
	zuo chi shan kong
	zuo chu gui ding
	zuo chu jue ding


@group_typing1:	Pointer to group1 typings
@nch1:		Number of UNIHANs in group1

@group_typing2:	Pointer to group2 typings  ( MAYBE  short_typing )
@nch2:		Number of UNIHANs in group2

@enable_short_typing:
    If TRUE, then compare with byte order + typing order.
	Example: 'dong xia nan bei' ---> comparing order: d x n b o i a e n a n i g.
    Else see all typings as one string, and compare byte one by one.
    	Example: 'dong xia nan bei' ---> comparing order: d o n g x i a n a n b e i.

Return:
	Relative Priority Sequence position:
	CMPORDER_IS_AHEAD    -1 (<0)   typing1 is  'less than' OR 'ahead of' typing2
	CMPORDER_IS_SAME      0 (=0)   typing1 and typing2 are at the same order.
	CMPORDER_IS_AFTER     1 (>0)   typing1 is  'greater than' OR 'behind' typing2.
------------------------------------------------------------------------------------*/
static inline int compare_group_typings( const char *group_typing1, int nch1, const char *group_typing2, int nch2 )
					 //bool enable_short_typing )
{
	int k,i;
	char c1,c2;

	/* Compare nch */
    	if( nch1 > nch2 )
		return  CMPORDER_IS_AFTER;
    	else if( nch1 < nch2 )
		return CMPORDER_IS_AHEAD;
	/* NOW: nch1==nch2 */

#if 1 /* OPTION 1: (OK)  Interval(short_typing) comparing,  group_typing2 as short_typing!  */
    //if(enable_short_typing) //&& nch1==nch2 )
    {
        /* Compare typing bytes  in  byte order + typing order. See in UniHanGroup_compare_typings() */
        for( i=0; i<UNIHAN_TYPING_MAXLEN; i++) {		/* tranverse typing byte order */
		for( k=0; k<nch1; k++) {			/* tranverse wcode order, nch1==nch1  */
			c1=group_typing1[k*UNIHAN_TYPING_MAXLEN+i];
			c2=group_typing2[k*UNIHAN_TYPING_MAXLEN+i];
                        if( c1 > c2 )
                                return CMPORDER_IS_AFTER;
			else if( c1 < c2 )
				return CMPORDER_IS_AHEAD;
		}
	}
	return CMPORDER_IS_SAME;  /* For k==UHGROUP_WCODES_MAXSIZE or k=nch1(nch2) */
    }

#else    /* OPTION 2:  Consecutive(complete) comparing */
    //else
    {
        /* Compare dictionary order of group typing[] */
        for( k=0; k<nch1; k++)
        {
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
	return CMPORDER_IS_SAME;  /* For k==UHGROUP_WCODES_MAXSIZE or k=nch1(nch2) */
    }
#endif

}


/*------------------------------------------------------------------------
Sort an unihan_groups set by Insertion_Sort algorithm, to rearrange unihan
groups in ascending order of typing+freq. (typing:in dictionary order)

Also ref. to mat_insert_sort() in egi_math.c.

Note:
1.The caller MUST ensure ugroups has at least [start+n-1] memebers!
2.Input 'n' MAY includes empty unihans with typing[0]==0, and they will
  be rearranged to the end of the array.

@group_set:	Group Set.
@start:		Start index of group_set->ugroups[]
@n:             size of the array to be sorted, from [start] to [start+n-1].

Return:
	0 	Ok
	<0 	Fails
------------------------------------------------------------------------*/
int UniHanGroup_insertSort_typings(EGI_UNIHANGROUP_SET *group_set, int start, int n)
{
        int i;          /* To tranverse elements in array, 1 --> n-1 */
        int k;          /* To decrease, k --> 1,  */
        EGI_UNIHANGROUP tmp;

	if( group_set==NULL || group_set->ugroups==NULL)
		return -1;

	/* Start sorting ONLY when i>1 */
        for( i=start+1; i< start+n; i++) {
                tmp=group_set->ugroups[i];   /* the inserting integer */

        	/* Slide the inseting integer left, until to the first smaller unihan  */
                //for( k=i; k>0 && UniHanGroup_compare_typings(unihans+k-1, &tmp)==CMPORDER_IS_AFTER; k--) {
		for( k=i; k>0 && UniHanGroup_compare_typings(group_set->ugroups+k-1, &tmp, group_set)==CMPORDER_IS_AFTER; k--) {
				//unihans[k]=unihans[k-1];   /* swap */
				group_set->ugroups[k]=group_set->ugroups[k-1];  /* swap */
		}

		/* Settle the inserting unihan at last swapped place */
                group_set->ugroups[k]=tmp;
        }

	return 0;
}


/*---------------------------------------------------------------------
Sort an unihan_groups set by Quick_Sort algorithm, to rearrange unihan
groups in ascending order of nch+typing+freq.
To see UniHanGroup_compare_typings() for the details of order comparing.

Refert to mat_quick_sort() in egi_math.h.

 	!!! WARNING !!! This is a recursive function.

Note:
1. The caller MUST ensure start/end are within the valid range.

@group_set:	Group set that hold unihan_groups.
@start:		start index, as of group_set->ugroups[start]
@End:		end index, as of group_set->ugroups[end]
@cutoff:	cutoff value for switch to insert_sort.


Return:
	0	Ok
	<0	Fails
----------------------------------------------------------------------*/
int UniHanGroup_quickSort_typings(EGI_UNIHANGROUP_SET* group_set, unsigned int start, unsigned int end, int cutoff)
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
		if( UniHanGroup_compare_typings(group_set->ugroups+start, group_set->ugroups+mid, group_set)==CMPORDER_IS_AFTER ) {
			tmp=group_set->ugroups[start];
			group_set->ugroups[start]=group_set->ugroups[mid];
			group_set->ugroups[mid]=tmp;
                }
                /* Now: [start]<=array[mid] */

                /* IF: [mid] >= [start] > [end] */
                /* if( array[start] > array[end] ) { */
		if( UniHanGroup_compare_typings(group_set->ugroups+start, group_set->ugroups+end, group_set)==CMPORDER_IS_AFTER ) {
			tmp=group_set->ugroups[start]; /* [start] is center */
			group_set->ugroups[start]=group_set->ugroups[end];
			group_set->ugroups[end]=group_set->ugroups[mid];
			group_set->ugroups[mid]=tmp;
                }
                /* ELSE:   [start]<=[mid] AND [start]<=[end] */
                /* else if( array[mid] > array[end] ) { */
		if( UniHanGroup_compare_typings(group_set->ugroups+mid, group_set->ugroups+end, group_set)==CMPORDER_IS_AFTER ) {
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
		        while ( UniHanGroup_compare_typings(group_set->ugroups+(++i), &pivot, group_set) ==CMPORDER_IS_AHEAD ) { };
                        /* Stop at array[j]<=pivot: We preset array[start]<=pivot as sentinel, so j will stop at [start]  */
                        /* while( array[--j] > pivot ){ }; */
		        while ( UniHanGroup_compare_typings(group_set->ugroups+(--j), &pivot, group_set) ==CMPORDER_IS_AFTER ) { };

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
		UniHanGroup_quickSort_typings(group_set, start, i-1, cutoff);
		UniHanGroup_quickSort_typings(group_set, i+1, end, cutoff);

        }
/* 2. Implement insertsort */
        else
		UniHanGroup_insertSort_typings(group_set, start, end-start+1);

        /* Reset sort order */
        group_set->sorder=UNIORDER_NCH_TYPINGS_FREQ;

	return 0;
}


/*-----------------------------------------------------------------------------
Compare nch+wcodes ORDER of two unihan_groups and return an integer less
than, equal to, or greater than zero, if group1 is found to be ahead of,
at same order of, or behind group2 respectively.

Note:
1. It frist compares number of unihans in the group, the greater one is
   AFTER the smaller one.
2. Then it compares their wcodes(UNICODEs IDs), the bigger the later.
3. An empty unihan group(nch==0) is always 'AFTER' a nonempty one.
4. Assume groups with same wcodes(UNICODEs) should bear same frequency value and typings.
   So freqs and typings will NOT be compared.
5. A NULL is always 'AFTER' a non_NULL one.

@group1		A pointer to EGI_UNIHANGGROUP
@gruop2

Return:
	Relative Priority Sequence position:
	CMPORDER_IS_AHEAD    -1 (<0)	group1 is  'less than' OR 'ahead of' group2
	CMPORDER_IS_SAME      0 (=0)    group1 and group2 are at the same order.
	CMPORDER_IS_AFTER     1 (>0)    group1 is  'greater than' OR 'behind' group2.
-------------------------------------------------------------------------------------*/
inline int UniHanGroup_compare_wcodes( const EGI_UNIHANGROUP *group1, const EGI_UNIHANGROUP *group2 )
{
	int i;
	int nch1, nch2;

	/* If group_set is NULL */
	if(group1==NULL || group2==NULL)
	{
		if(group1==NULL && group2==NULL)
			return CMPORDER_IS_SAME;
		else if(group1==NULL)
			return CMPORDER_IS_AFTER;
		else if(group2==NULL)
			return CMPORDER_IS_AHEAD;
	}

	/* 1. Get number of chars of each group */
        nch1=UHGROUP_WCODES_MAXSIZE;
        while( nch1>0 && group1->wcodes[nch1-1] == 0 ) { nch1--; };
        nch2=UHGROUP_WCODES_MAXSIZE;
        while( nch2>0 && group2->wcodes[nch2-1] == 0 ) { nch2--; };

	/* 2. Put the empty ugroup to the last,
	 *    It also screens out nch1==0 AND/OR nch2==0 !!
	 */
	if( nch1==0 || nch2==0 )
	{
		if( nch1==0 && nch2!=0 )
			return CMPORDER_IS_AFTER;
		else if( nch1!=0 && nch2==0 )
			return CMPORDER_IS_AHEAD;
		else if( nch1==0 && nch2==0 )
			return CMPORDER_IS_SAME;
	}

	/* 4. NOW nch1>0 and nch2>0: Compare nch of group */
	if( nch1 > nch2 )
		return CMPORDER_IS_AFTER;
	else if( nch1 < nch2 )
		return CMPORDER_IS_AHEAD;
	/* ELSE: nch1==nch2 */

	/* 5. Compare wcodes[] of groups */
	for(i=0; i<UHGROUP_WCODES_MAXSIZE; i++) {
		if( (uint32_t)(group1->wcodes[i]) > (uint32_t)(group2->wcodes[i]) )
			return CMPORDER_IS_AFTER;
		else if ( (uint32_t)(group1->wcodes[i]) < (uint32_t)(group2->wcodes[i]) )
			return CMPORDER_IS_AHEAD;
	}

	/* XXXX 6. Put groups with typings[0]==EOF later .... need group_set ...*/

	/* Assume groups with same wcodes(UNICODEs) should bear same frequency value and typings. */

	return CMPORDER_IS_SAME;
}


/*----------------------------------------------------------------------------
Sort an unihan groups set by Insertion_Sort algorithm, to rearrange unihan
groups in ascending order of nch+wcodes.

Also ref. to mat_insert_sort() in egi_math.c.

Note:
1.The caller MUST ensure ugroups has at least [start+n-1] memebers!
2.Input 'n' MAY includes empty unihan_groups with wcodes[all]==0, and they will
  be rearranged to the end of the array.


@group_set:	Group Set.
@start:		Start index of group_set->ugroups[]
@n:             size of the array to be sorted, from [start] to [start+n-1].

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------------------*/
int  UniHanGroup_insertSort_wcodes(EGI_UNIHANGROUP_SET *group_set, int start, int n)
{
        int i;          /* To tranverse elements in array, 1 --> n-1 */
        int k;          /* To decrease, k --> 1,  */
        EGI_UNIHANGROUP tmp;

	if( group_set==NULL || group_set->ugroups==NULL)
		return -1;

//	if( start<0 || n<0 || (unsigned int)(start+n) > group_set->ugroups_size )
//		return -2;

	/* Start sorting */
        for( i=start+1; i< start+n; i++) {
                tmp=group_set->ugroups[i];   /* the inserting integer */

        	/* Slide the inseting integer left, until to the first smaller unihan  */
		//for( k=i; k>0 && array[k-1]>tmp; k--)
		//for( k=i; k>0 && UniHanGroup_compare_wcodes(group_set, k-1, i)==CMPORDER_IS_AFTER; k--) { /* NOPE i, as ugroups[i] will change !*/
		for( k=i; k>0 && UniHanGroup_compare_wcodes(group_set->ugroups+k-1, &tmp)==CMPORDER_IS_AFTER; k--) {
				//printf("Is_AFTER\n");
				group_set->ugroups[k]=group_set->ugroups[k-1];  /* swap */
		}

		/* Settle the inserting unihan at last swapped place */
                group_set->ugroups[k]=tmp;
        }

	return 0;
}


/*---------------------------------------------------------------------
Sort an unihan_groups set by Quick_Sort algorithm, to rearrange unihan
groups in ascending order of nch+wcodes.
To see UniHanGroup_compare_wcodes() for the details of order comparing.

Refert to mat_quick_sort() in egi_math.h.

 	!!! WARNING !!! This is a recursive function.

Note:
1. The caller MUST ensure start/end are within valid range.

@group_set:	Group set that hold unihan_groups.
@start:		start index, as of group_set->ugroups[start]
@End:		end index, as of group_set->ugroups[end]
@cutoff:	cutoff value for switch to insert_sort.

Return:
	0	Ok
	<0	Fails
----------------------------------------------------------------------*/
int UniHanGroup_quickSort_wcodes(EGI_UNIHANGROUP_SET* group_set, unsigned int start, unsigned int end, int cutoff)
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
		if( UniHanGroup_compare_wcodes(group_set->ugroups+start, group_set->ugroups+mid)==CMPORDER_IS_AFTER ) {
			tmp=group_set->ugroups[start];
			group_set->ugroups[start]=group_set->ugroups[mid];
			group_set->ugroups[mid]=tmp;
                }
                /* Now: [start]<=array[mid] */

                /* IF: [mid] >= [start] > [end] */
                /* if( array[start] > array[end] ) { */
		if( UniHanGroup_compare_wcodes(group_set->ugroups+start, group_set->ugroups+end)==CMPORDER_IS_AFTER ) {
			tmp=group_set->ugroups[start]; /* [start] is center */
			group_set->ugroups[start]=group_set->ugroups[end];
			group_set->ugroups[end]=group_set->ugroups[mid];
			group_set->ugroups[mid]=tmp;
                }
                /* ELSE:   [start]<=[mid] AND [start]<=[end] */
                /* else if( array[mid] > array[end] ) { */
		if( UniHanGroup_compare_wcodes(group_set->ugroups+mid, group_set->ugroups+end)==CMPORDER_IS_AFTER ) {
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
		        while ( UniHanGroup_compare_wcodes(group_set->ugroups+(++i), &pivot) ==CMPORDER_IS_AHEAD ) { };
                        /* Stop at array[j]<=pivot: We preset array[start]<=pivot as sentinel, so j will stop at [start]  */
                        /* while( array[--j] > pivot ){ }; */
		        while ( UniHanGroup_compare_wcodes(group_set->ugroups+(--j), &pivot) ==CMPORDER_IS_AFTER ) { };

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
		UniHanGroup_quickSort_wcodes(group_set, start, i-1, cutoff);
		UniHanGroup_quickSort_wcodes(group_set, i+1, end, cutoff);

        }
/* 2. Implement insertsort */
        else
		UniHanGroup_insertSort_wcodes(group_set, start, end-start+1);

        /* Reset sort order */
        group_set->sorder=UNIORDER_NCH_WCODES;

	return 0;
}


/*-------------------------------------------------------------------------------
To locate the index of first group_set->ugroups[] bearing the given wcodes[]( and
same nch), and set group_set->pgrp as the index.

Note:
1. The group_set MUST be prearranged in ascending order of wcodes. If NOT, it then
   calls UniHanGroup_quickSort_set() to sort the group_set first.
2. There MAY be more than one ugroups[] that have the save wcodes[], and indexse
   of those unihans should be consecutive (see 1). Finally the first index will
   be located.
3. If nch==1, then an UNIHAN to be located.

@group_set		The target EGI_UNIHAN_SET.
@uchars			UFT8 string as of wcodes[].
			if NULL, read wcodes instead.
@wcodes			Pointer to wcodes[UHGROUP_WCODES_MAXSIZE]
			The caller MUST ensure at least UHGROUP_WCODES_MAXSIZE.
			Effective ONLY if uchars==NULL.
Return:
	=0		OK, set uniset->puh accordingly.
	<0		Fails, or no result.
--------------------------------------------------------------------------------*/
int  UniHanGroup_locate_wcodes(EGI_UNIHANGROUP_SET* group_set,  const UFT8_PCHAR uchars, const EGI_UNICODE *wcodes)
{
	int i;
	unsigned int start,end,mid;
	EGI_UNIHANGROUP	*ugroups=NULL;
	EGI_UNIHANGROUP	tmp_group={0};
	int chsize;
	int len;
	int nch;

	if(group_set==NULL || group_set->ugroups_size==0 || (uchars==NULL && wcodes==NULL) )
		return -1;

	/* Check sort order */
	if( group_set->sorder != UNIORDER_NCH_WCODES ) {
		printf("%s: Start to quickSort set to UNIORDER_NCH_WCODES!\n",__func__);
		if( UniHanGroup_quickSort_set(group_set, UNIORDER_NCH_WCODES, 10) !=0 ) {
			printf("%s: Fail to quickSort set to UNIORDER_NCH_WCODES!\n", __func__);
			return -2;
		}
	}

	/* Get pointer to ugroups */
	ugroups=group_set->ugroups;

	/* Fake a tmp_group */
	memset(tmp_group.wcodes,0,UHGROUP_WCODES_MAXSIZE*sizeof(EGI_UNICODE));
	if(uchars) {
		/* Convert uchars to tmp_group.wcodes[] */
		for( len=0, nch=0, i=0; i<UHGROUP_WCODES_MAXSIZE; i++ ) {
		 	chsize=char_uft8_to_unicode(uchars+len, tmp_group.wcodes+i);
			if(chsize<1)
				break;
			else {
				nch++;
				len+=chsize;
			}
		}
	}
	else {  /* Take wcodes */
		memcpy(tmp_group.wcodes, wcodes,UHGROUP_WCODES_MAXSIZE*sizeof(EGI_UNICODE));
	}


#if 0	/* Check nch */
	if(nch<1)
		return -3;
#endif

	/* Mark start/end/mid of ugroups[] index */
	start=0;
	end=group_set->ugroups_size-1;
	mid=(start+end)/2;

	/* binary search for the wcodes in all ugroups[]  */
	//while( unihans[mid].wcode != wcode ) {
	while( UniHanGroup_compare_wcodes( ugroups+mid, &tmp_group ) != CMPORDER_IS_SAME )
	{
		/* check if searched all ugroups */
		if(start==end)
			return -2;
		/* !!! If mid+1 == end: then mid=(n+(n+1))/2 = n, and mid will never increase to n+1! */
		else if( start==mid && mid+1==end ) {	/* start != 0 */
			//if( unihans[mid+1].wcode==wcode ) {
			if( UniHanGroup_compare_wcodes( ugroups+mid+1, &tmp_group ) == CMPORDER_IS_SAME ) {
				mid += 1;  /* Let mid be the index of unihans, see following... */
				break;
			}
			else
				return -3;
		}

		//if( unihans[mid].wcode > wcode ) {   /* then search upper half */
		if( UniHanGroup_compare_wcodes( ugroups+mid, &tmp_group ) == CMPORDER_IS_AFTER ) {
			end=mid;
			mid=(start+end)/2;
			continue;
		}
		//else if( unihans[mid].wcode < wcode) {
		else if( UniHanGroup_compare_wcodes( ugroups+mid, &tmp_group ) == CMPORDER_IS_AHEAD ) {
			start=mid;
			mid=(start+end)/2;
			continue;
		}
	}
	/* Finally ugroups[mid].wcode[]==wcodes[]  */

        /* NOW move up to locate the first ugroups[] bearing the same wcodes[]. */
	//while( mid>0 && unihans[mid-1].wcode==wcode ){ mid--; };
	while( mid>0 && UniHanGroup_compare_wcodes( ugroups+mid-1, &tmp_group ) == CMPORDER_IS_SAME ) { mid--; };
	group_set->pgrp=mid;  /* assign to uniset->puh */

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
The caller MUST ensure at least nch*UNIHAN_TYPING_MAXLEN bytes mem space for both
hold_typings AND try_typings!


@hold_typings: 	Typings expected to contain the try_typings.
@try_typings:   Typings expected to be contained in the hold_typings.
		And it MAY be short_typings.
@nch: 		number of wcodes for comparion.

Return:
	0	Ok,
	<0      Fails, or NOT contained
---------------------------------------------------------------------------------*/
inline int strstr_group_typings(const char *hold_typings, const char* try_typings, int nch)
{
	int i;

	/* Limit nch */
	if(nch>UHGROUP_WCODES_MAXSIZE)
		nch=UHGROUP_WCODES_MAXSIZE;

	for(i=0; i<nch; i++) {
		if( strstr(hold_typings+UNIHAN_TYPING_MAXLEN*i, try_typings+UNIHAN_TYPING_MAXLEN*i)
		    != hold_typings+UNIHAN_TYPING_MAXLEN*i )
		{
			return -1;
		}
	}

	return 0;
}


/*-------------------------------------------------------------------------------
Compare hold_tyingps and try_typings, return 0 if they are the same.

			!!! WARNING !!!
The caller MUST ensure at least nch*UNIHAN_TYPING_MAXLEN bytes mem space for both
hold_typings AND try_typings!

@hold_tyings, try_typings: typings for comparision.
			   Note, nch of two typins MUST be the same!
@nch: 			   number of wcodes for comparion.

Return:
	0	Ok,same string.
	<0      Fails or different.
---------------------------------------------------------------------------------*/
inline int strcmp_group_typings(const char *hold_typings, const char* try_typings, int nch)
{
	int i;

	/* Limit nch */
	if(nch>UHGROUP_WCODES_MAXSIZE)
		nch=UHGROUP_WCODES_MAXSIZE;

	/* Compare typings for each wcode/unihan */
	for(i=0; i<nch; i++) {
		if( strcmp(hold_typings+UNIHAN_TYPING_MAXLEN*i, try_typings+UNIHAN_TYPING_MAXLEN*i) != 0 )
			return -1;
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
in ugroups->typings[ugroup->pos_typing] ) of the wcode are the SAME as the given
typings, OR the given (4) typings are contained in beginning of
ugroups->typings[ugroup->pos_typing] respectively.

This criteria is ONLY based on PINYIN. Other types of typing may NOT comply.

Note:
1. The group_set MUST be prearranged in dictionary ascending order of nch+typing+freq.
   (UNIORDER_NCH_TYPINGS_FREQ)
2. And all uniset->unhihans[0-size-1] are assumed to be valid/normal, with
   wcode >0 AND typing[0]!='\0'.
3. Pronunciation tones of PINYIN are neglected.
4. If unihan_group(cizu) can NOT be located for the input typing, it will TRY to
   search it at longer groups.
   Example:
	 typings 'zuo fa z'  ---> NO nch=3 unihan_groups ----> then locate at '作法自毙'

   !!! However, it MAY fail to locate!!! As their sorting method diffs
   	Example: for nch=3: 1->2->3(->1) sorting. for nch=4: 1->2->3->4(->1) sorting.
   The bigger nch has more chance to locate a result!

5. This algorithm depends on typing type PINYIN, others MAY NOT work.
6. Some polyphonic UNIHANs will fail to locate, as UniHanGroup_assemble_typings()
   does NOT know how to pick the right typings for an unihan group!
   Call UniHanGroup_update_typings() to correct it, see dscription in the function!
	 ...
	 地球: de qiu
	 出差: chu cha
	 锒铛: lang cheng
	 占便宜: zhan bian yi
	 朴素大方: piao su da fang
	 朝三暮四: chao san mu si
	 现身说法: xian shen shui fa
	 畅行无阻: chang hang wu zu
	 ...
6. Resolved! TO_DO: For short_typing, two intials shall be nearly the same number, otherwise it may fail!
   (To improve: First locate by initals, then search it in result collections. Example: 'huah' --> first 'hh' --> 'huah' )

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
	int i,k;
//	int step;
	unsigned int start,end,mid;
	EGI_UNIHANGROUP *ugroups=NULL;
	int nch1,nch2;   /* nch2 as input typings */
	char init_typings[UNIHAN_TYPING_MAXLEN*UHGROUP_WCODES_MAXSIZE];  /* with first inits of input typings only */

	if(typings==NULL || typings[0] ==0 )
		return -1;

	if(group_set==NULL || group_set->ugroups==NULL || group_set->ugroups_size==0) /* If empty */
		return -1;

	/* Check sort order */
	if( group_set->sorder!=UNIORDER_NCH_TYPINGS_FREQ ) {
                printf("%s: group_set sort order is NOT UNIORDER_NCH_TYPINGS_FREQ!\n", __func__);
                return -1;
	}

	/* Get pointer to unihans */
	ugroups=group_set->ugroups;

	/* Reset results size */
	group_set->results_size=0;

	/* Get total number of unihans/unicodes presented in input typings */
       	nch2=UHGROUP_WCODES_MAXSIZE;
        while( nch2>0 && typings[(nch2-1)*UNIHAN_TYPING_MAXLEN]=='\0' ) { nch2--; };

	/* Prepare init_typings[], with first initial of typings! */
	bzero(init_typings,sizeof(init_typings));
	init_typings[0]=typings[0];
	init_typings[1]=typings[1];	/* Tricky */
	for(i=1; i<UHGROUP_WCODES_MAXSIZE; i++)
		init_typings[i*UNIHAN_TYPING_MAXLEN]=typings[i*UNIHAN_TYPING_MAXLEN];

START_SEARCH:
	/* Mark start/end/mid of ugroups[] index */
	start=0;
	end=group_set->ugroups_size-1;
	mid=(start+end)/2;

	/* binary search for the typings  */
	/* While if NOT containing the typing string at beginning */
	//while( nch1!=nch2 || strstr_group_typings( group_set->typings+ugroups[mid].pos_typing, typings, ch2) !=0 ) {
	do {
		/* Get total number of unihans/unicodes presented in ugroups[mid].wcodes[] */
        	nch1=UHGROUP_WCODES_MAXSIZE;
	        while( nch1>0 && ugroups[mid].wcodes[nch1-1] == 0 ) { nch1--; };

		/* check if searched all wcodes */
		if(start==end) {
			/* Expand search range to longer groups */
			if( nch2 < UHGROUP_WCODES_MAXSIZE ) {
				nch2++;
				init_typings[1]=0;     /* Tricky */
				goto START_SEARCH;
			}
			else
				return -2;
		}

		/* !!! If mid+1 == end: then mid=(n+(n+1))/2 = n, and mid will never increase to n+1! */
		else if( start==mid && mid+1==end ) {   /* start !=0 */
			//if（ unihans[mid+1].wcode==wcode ) {
			/* Check if given typing are contained in beginning of unihan[mid+1].typing. */
			if( nch1==nch2 && strstr_group_typings( group_set->typings+ugroups[mid+1].pos_typing,
						  						init_typings, nch2 ) ==0 ) {
						  //group_set->typings+ugroups[mid].pos_typing ) ==0 ) {
				mid += 1;  /* Let mid be the index of group_set, see following... */
				break;
			}
			else {
				/* Expand search range to longer groups */
				if( nch2 < UHGROUP_WCODES_MAXSIZE ) {
					nch2++;
					init_typings[1]=0;     /* Tricky */
					goto START_SEARCH;
				}
				else
					return -3;
			}
		}

		//if( unihans[mid].wcode > wcode ) {   /* then search upper half */
		if( compare_group_typings(group_set->typings+ugroups[mid].pos_typing, nch1, init_typings, nch2)==CMPORDER_IS_AFTER ) {
			end=mid;
			mid=(start+end)/2;
			continue;
		}
		//else if( unihans[mid].wcode < wcode) {
		if( compare_group_typings(group_set->typings+ugroups[mid].pos_typing, nch1, init_typings, nch2)==CMPORDER_IS_AHEAD ) {
			start=mid;
			mid=(start+end)/2;
			continue;
		}

	        /* NOW CMPORDER_IS_SAME: To rule out abnormal unihan with wcode==0!
		 * UniHan_compare_typing() will return CMPORDER_IS_SAME if two unihans both with wocde==0 */
		//if( unihans[mid].wcode==0 )
		if( ugroups[mid].wcodes[0]==0 )
			return -3;
	} while( nch1 != nch2 || strstr_group_typings( group_set->typings+ugroups[mid].pos_typing, init_typings, nch1) !=0 );
	/* NOW unhihan[mid] has the same typing */

	/* Reset group_set->results_size at beginning of the function */

#if 0  /* OPTION 1:  Check results by Moving k UP and DOWN  from mid.
	* 				!!! --- WARNING --- !!!
	* The group_set->resutls[] from this way can NOT ensure Shortest_Typing_First principle! as k may NOT
	* move up to the very beginning before group_set->results_size gets to LIMIT.
        */
	/* Move UP: to locate the first unihan group that bearing the same typings. */
	k=mid; /* reset k to mid, k is init_typing index! */
	while( k>0 && strstr_group_typings(group_set->typings+ugroups[k-1].pos_typing, init_typings, nch1) ==0 )
	{
		/* Check input typings and put index to group_set->results */
		if( strstr_group_typings(group_set->typings+ugroups[k-1].pos_typing, typings, nch1) ==0 ) {
			if( group_set->results_size < group_set->results_capacity ) /* TODO memgrow group_set->results */
				group_set->results[group_set->results_size++]=k-1;
			else
                		printf("%s: group_set->results_size LIMIT!\n", __func__);
		}
		k--;
	};
	group_set->pgrp=k;  /* Assign uniset->pgrp */

	/* Move DOWN: to locate the last unihan group that bearing the same typings. */
	k=mid; /* reset k to mid */
	while( k < group_set->ugroups_size && strstr_group_typings(group_set->typings+ugroups[k].pos_typing, init_typings, nch1) ==0 )
	{
		/* Check input typings and put index to group_set->results */
		if( strstr_group_typings(group_set->typings+ugroups[k].pos_typing, typings, nch1) ==0 ) {
			if( group_set->results_size < group_set->results_capacity )  /* TODO memgrow group_set->results */
				group_set->results[group_set->results_size++]=k;
			else
                		printf("%s: group_set->results_size LIMIT!\n", __func__);
		}
		k++;
	};

#else  /* OPTION 2: Move to the top(beginning) of the valid typings, then move down and check */
	k=mid; /* reset k to mid, k is init_typing index! */

	#if 1 /* --- METHOD_1 ---: Move k to top, STEP=1 */
	while( k>0 && strstr_group_typings(group_set->typings+ugroups[k-1].pos_typing, init_typings, nch1) ==0 ) { k--; };
	#else /* --- METHOD_2 ---: Move k to top, STEP=4 */
	while(k>0) {
		if(k>8) step=8; else step=1;
		k -= step;
		if( strstr_group_typings(group_set->typings+ugroups[k].pos_typing, init_typings, nch1) !=0 )
			break;
	}
	if(k<0)k=0;
	/* Now ugroups[k] MAY not contain init_tyings! Move down to the first one. */
	while( strstr_group_typings(group_set->typings+ugroups[k].pos_typing, init_typings, nch1) !=0 ) { k++; };
	#endif

	group_set->pgrp=k;  /* Assign uniset->pgrp */

	/* Move k down and Check input typings and put index to group_set->results */
	while( strstr_group_typings(group_set->typings+ugroups[k].pos_typing, init_typings, nch1) ==0 ) {
		if( strstr_group_typings(group_set->typings+ugroups[k].pos_typing, typings, nch1) ==0 ) {
	        	if( group_set->results_size < group_set->results_capacity )  /* TODO memgrow group_set->results */
        	        	group_set->results[group_set->results_size++]=k;
                	else {
                		printf("%s: group_set->results_size LIMIT!\n", __func__);
				break;
			}
		}
       		k++;  /* k is init_typing index! */
	}

#endif

	/* Sort result in order of TypingLen+Freq */
	#if 0
	UniHanGroup_insertSort_LTRIndex(group_set, 0, group_set->results_size);
	#else
	UniHanGroup_quickSort_LTRIndex(group_set, 0, group_set->results_size-1, 10);
	#endif

	return 0;
}


/*-----------------------------------------------------------------------------------------------
	    ( ---- This function is for UniHanGroup_insertSort_LTRIndex() ---- )

Compare TypingLen+freq ORDER of two unihan_groups as represented by index of group_set->ugroups[],
the indexes MUST be from group_set->results! so all unihan_groups in the results have same nch value!

Note:
1. The caller MUST ensure that n1,n2 are within range [0 group_set->ugroups_size], and they are
   picked from group_set->results[].
2. It first compares total length of their typings. the smaller one is ahead the bigger one.
3. Then compare frequency values, if group1->freq is GREATER than group2->freq, it returns -1(IS_AHEAD);
   else if group1->freq is LESS than group2->freq, then return 1(IS_AFTER);
   if EQUAL, it returns 0(IS_SAME).

@n1, n2:	Index of group_set->ugroups[]
@group_set:	Group Set that holds above uinhangroups.

Return:
	Relative Priority Sequence position:
	CMPORDER_IS_AHEAD    -1 (<0)   group1 is  'less than' OR 'ahead of' group2
	CMPORDER_IS_SAME      0 (=0)   group1 and group2 are at the same order.
	CMPORDER_IS_AFTER     1 (>0)   group1 is  'greater than' OR 'behind' group2.
-----------------------------------------------------------------------------------------------*/
inline int UniHanGroup_compare_LTRIndex(EGI_UNIHANGROUP_SET* group_set, unsigned int n1, unsigned int n2)
{
	int i;
	int nch;
	int chlen1[UHGROUP_WCODES_MAXSIZE], chlen2[UHGROUP_WCODES_MAXSIZE];
	int len1, len2;
	unsigned int off1, off2;

	/* check group_set */
	if( group_set==NULL )
		return CMPORDER_IS_SAME;

	/* 1. Get number of chars of each group, Assume nch==nch1==nch2. */
        nch=UHGROUP_WCODES_MAXSIZE;
        while( nch>0 && group_set->ugroups[n1].wcodes[nch-1] == 0 ) { nch--; };

	/* 2. Compare total length of typings */
	len1=0;
	len2=0;
	off1=group_set->ugroups[n1].pos_typing;
	off2=group_set->ugroups[n2].pos_typing;

	/* Count charlength and strlength */
	for(i=0; i<nch; i++) {
		chlen1[i]=strlen(group_set->typings+off1+i*UNIHAN_TYPING_MAXLEN);
		len1 += chlen1[i];
		chlen2[i]=strlen(group_set->typings+off2+i*UNIHAN_TYPING_MAXLEN);
		len2 += chlen2[i];
	}

	#if 0 /* !!! NOPE !!!: Suppose NO typing in results[] is empty! 3. Put empty typing at end. */
	if( len1 == 0 && len2 ==0 )
		return CMPORDER_IS_SAME;
	else if( len1==0)
		return CMPORDER_IS_AFTER;
	else if( len2==0)
		return CMPORDER_IS_AHEAD;
	#endif /////////////////////////////////////////////////

// #if 0 /* Compare lenght of typings first */
if(nch==1) {  /* For UNIHAN, compare length first */
	/* 4. Compare length of typings */
	if( len1 > len2 )
		return CMPORDER_IS_AFTER;
	else if( len1 < len2 )
		return CMPORDER_IS_AHEAD;
	/* ELSE: Compare length of each typing */
	for(i=0; i<nch; i++) {
		if( chlen1[i]>chlen2[i] )
			return CMPORDER_IS_AFTER;
		else if( chlen1[i]<chlen2[i] )
			return CMPORDER_IS_AHEAD;
	}
	/* NOW: each typing has same length */

	/* 5. Compare frequency */
	if( group_set->ugroups[n1].freq > group_set->ugroups[n2].freq )
		return CMPORDER_IS_AHEAD;
	if( group_set->ugroups[n1].freq < group_set->ugroups[n2].freq )
		return CMPORDER_IS_AFTER;

}
// #else /* Compare frequency first, How ever this MAY put unwant word first: Example: yu -- 1.曰 2.与 3.玉 4.于 5.云 6.原 */
else {   /* For UNIHANGROUPs, compare freq first:
		1. As the results are vague if pinyin_inputs are Shengmus only.
	  */
	/* 4. Compare frequency */
	if( group_set->ugroups[n1].freq > group_set->ugroups[n2].freq )
		return CMPORDER_IS_AHEAD;
	if( group_set->ugroups[n1].freq < group_set->ugroups[n2].freq )
		return CMPORDER_IS_AFTER;

	/* 5. Compare length of typings */
	if( len1 > len2 )
		return CMPORDER_IS_AFTER;
	else if( len1 < len2 )
		return CMPORDER_IS_AHEAD;
	/* ELSE: Compare length of each typing */
	for(i=0; i<nch; i++) {
		if( chlen1[i]>chlen2[i] )
			return CMPORDER_IS_AFTER;
		else if( chlen1[i]<chlen2[i] )
			return CMPORDER_IS_AHEAD;
	}
}
//#endif


	return CMPORDER_IS_SAME;
}


/*-----------------------------------------------------------------------
Sort group_set->results[] by Insertion_Sort algorithm, to rearrange those
index so as to present their corresponding unihan_groups in ascending order
of TypingLen+freq.

Also ref. to mat_insert_sort() in egi_math.c.

Note:
1. Before calling this function, group_set->results[] MUST hold a set of
   indexes of group_set->ugroups[], as a result locating/searching operation
   (usually by UniHanGroup_locate_typings()).
2. The caller MUST ensure group_set->results[] has at least [start+n-1] memebers!
3. Input 'n' MAY includes empty unihans with typing[0]==0, and they will
   be rearranged to the end of the array. 	--- NOPE, rule out.

@group_set:	Group Set.
@start:		Start index of group_set->result[]
@n:             size of the array to be sorted, from [start] to [start+n-1].
------------------------------------------------------------------------*/
void UniHanGroup_insertSort_LTRIndex(EGI_UNIHANGROUP_SET *group_set, int start, int n)
{
        int i;          /* To tranverse elements in array, 1 --> n-1 */
        int k;          /* To decrease, k --> 1,  */
	int tmp_index;

	if( group_set==NULL || group_set->ugroups==NULL || group_set->results_size ==0 )
		return;

	/* Start sorting ONLY when m>start+1 */
        for( i=start+1; i< start+n; i++) {
		tmp_index=group_set->results[i];

        	/* Slide the inseting integer left, until to the first smaller unihan  */
		//for( k=i; k>0 && array[k-1]>tmp; k--)
		for( k=i; k>0 && UniHanGroup_compare_LTRIndex(group_set, group_set->results[k-1], tmp_index)==CMPORDER_IS_AFTER; k--) {
				//unihans[k]=unihans[k-1];   /* swap */
				group_set->results[k]=group_set->results[k-1]; /* swap */
		}

		/* Settle the inserting unihan at last swapped place */
                group_set->results[k]=tmp_index;
        }
}


/*--------------------------------------------------------------------------
Sort group_set->results[] by Quick_Sort algorithm, to rearrange those
index so as to present their corresponding unihan_groups in ascending
order of TypingLen+freq.

Also ref. to mat_quick_sort() in egi_math.c.

Note:
1. Before calling this function, group_set->results[] MUST hold a set of
   indexes of group_set->ugroups[], as a result locating/searching operation
   (usually by UniHanGroup_locate_typings()).
2. The caller MUST ensure start/end are within the valid range.

@group_set:	Group set that hold unihan_groups.
@start:		start index, as of group_set->ugroups[start]
@End:		end index, as of group_set->ugroups[end]
@cutoff:	cutoff value for switch to insert_sort.

Return:
	0	Ok
	<0	Fails
---------------------------------------------------------------------------*/
int UniHanGroup_quickSort_LTRIndex(EGI_UNIHANGROUP_SET* group_set, int start, int end, int cutoff)
{
	int i=start;
	int j=end;
	int mid;
	int tmp;
	int pivot;

	/* Check input */
	if( group_set==NULL )
		return -1;

	/* Rule out invalid params, rule out (end < start+1) in 1. */
	if( start<0 || end<0 || end > group_set->results_size-1 )
		return 0;

	/* End sorting */
	if( start >= end )
		return 0;

        /* Limit cutoff */
        if(cutoff<3)
                cutoff=3;

/* 1. Implement quicksort */
        if( end-start >= cutoff )  /* This will rule out end < start+1 */
        {
        /* 1.1 Select pivot, by sorting array[start], array[mid], array[end] */
                /* Get mid index */
                mid=(start+end)/2;

                /* Sort [start] and [mid] */
                /* if( array[start] > array[mid] ) */
		if( UniHanGroup_compare_LTRIndex(group_set, group_set->results[start], group_set->results[mid])==CMPORDER_IS_AFTER ) {
			tmp=group_set->results[start];
			group_set->results[start]=group_set->results[mid];
			group_set->results[mid]=tmp;
                }
                /* Now: [start]<=array[mid] */

                /* IF: [mid] >= [start] > [end] */
                /* if( array[start] > array[end] ) { */
		if( UniHanGroup_compare_LTRIndex(group_set, group_set->results[start], group_set->results[end])==CMPORDER_IS_AFTER ) {
			tmp=group_set->results[start]; /* [start] is center */
			group_set->results[start]=group_set->results[end];
			group_set->results[end]=group_set->results[mid];
			group_set->results[mid]=tmp;
                }
                /* ELSE:   [start]<=[mid] AND [start]<=[end] */
                /* else if( array[mid] > array[end] ) { */
		if( UniHanGroup_compare_LTRIndex(group_set, group_set->results[mid], group_set->results[end])==CMPORDER_IS_AFTER ) {
                        /* If: [start]<=[end]<[mid] */
			tmp=group_set->results[end]; /* [start] is center */
			group_set->results[end]=group_set->results[mid];
			group_set->results[mid]=tmp;
                        /* Else: [start]<=[mid]<=[end] */
                }
                /* Now: array[start] <= array[mid] <= array[end] */

		pivot=group_set->results[mid];

                /* Swap array[mid] and array[end-1], still keep array[start] <= array[mid] <= array[end]!   */
		tmp=group_set->results[end-1]; /* [start] is center */
		group_set->results[end-1]=group_set->results[mid];
		group_set->results[mid]=tmp;

        /* 1.2 Quick sort: array[start] ... array[end-1], array[end]  */
                i=start;
                j=end-1;   /* As already sorted to: array[start]<=pivot<=array[end-1]<=array[end], see 1. above */
                for(;;)
                {
                        /* Stop at array[i]>=pivot: We preset array[end-1]==pivot as sentinel, so i will stop at [end-1]  */
                        /* while( array[++i] < pivot ){ };   Acturally: array[++i] < array[end-1] which is the pivot memeber */
			while ( UniHanGroup_compare_LTRIndex(group_set, group_set->results[++i], pivot) ==CMPORDER_IS_AHEAD ) { };
                        /* Stop at array[j]<=pivot: We preset array[start]<=pivot as sentinel, so j will stop at [start]  */
                        /* while( array[--j] > pivot ){ }; */
			while ( UniHanGroup_compare_LTRIndex(group_set, group_set->results[--j], pivot) ==CMPORDER_IS_AFTER ) { };

                        if( i<j ) {
                                /* Swap array[i] and array[j] */
				tmp=group_set->results[i];
				group_set->results[i]=group_set->results[j];
				group_set->results[j]=tmp;
                        }
                        else {
                                break;
			}
		}
                /* Swap pivot memeber array[end-1] with array[i], we left this step at last.  */
		group_set->results[end-1]=group_set->results[i];
		group_set->results[i]=pivot; /* Same as array[i]=array[end-1] */

        /* 1.3 Quick sort: recursive call for sorted parts. and leave array[i] as mid of the two parts */
		UniHanGroup_quickSort_LTRIndex(group_set, start, i-1, cutoff);
		UniHanGroup_quickSort_LTRIndex(group_set, i+1, end, cutoff);

        }
/* 2. Implement insertsort */
        else
		UniHanGroup_insertSort_LTRIndex(group_set, start, end-start+1);

        /* Do NOT reset group_set->sorder. it's for group_set->results only! */

	return 0;
}


/*---------------------------------------------------------
Quick sort an UNIHANGROUP SET according to the required sort order.

		   --- WARNING!!! ---

Be cautious about sorting range of groups! Since there MAY
be empty groups(with wcode[all]==0) in the unihan groups.

For UNIORDER_NCH_TYPINGS_FREQ, it's assumed that all empty groups
are located at end of the ugroups[] BEFORE quickSort!
and sorting range is [0, size-1].

For UNIORDER_NCH_WCODE: the range is [0, capacity-1].
and empty groups will be put to the end of the array AFTER
quickSort.

@group_set	An pointer to an UNIHANGROUP_SET.
@sorder		Required sort order.
@cutoff		Cutoff value for quicksort algorithm.

Return:
	<0	Fails
	=0	OK
----------------------------------------------------------*/
int UniHanGroup_quickSort_set(EGI_UNIHANGROUP_SET* group_set, UNIHAN_SORTORDER sorder, int cutoff)
{
	int ret=0;

	if(group_set==NULL || group_set->ugroups==NULL || group_set->ugroups_size==0) /* If empty */
		return -1;

	/* Reset sorder before sorting */
	group_set->sorder=UNIORDER_NONE;

	/* Select sort functions */
	switch( sorder )
	{
		case	UNIORDER_NCH_WCODES:		/* Range[0, capacity-1] */
			ret=UniHanGroup_quickSort_wcodes(group_set, 0, group_set->ugroups_capacity-1, cutoff);
			break;
		case	UNIORDER_NCH_TYPINGS_FREQ:	/* Range[0, size-1] */
			ret=UniHanGroup_quickSort_typings(group_set, 0, group_set->ugroups_size-1, cutoff);
			break;
		default:
			printf("%s: Unsupported sort order!\n",__func__);
			ret=-1;
			break;
	}

	/* Put sort order ID */
	if(ret==0)
		group_set->sorder=sorder;

	return ret;
}


/*------------------------------------------------------------------------
Save an EGI_UNIHANGROUP_SET to a file.

NOTE:
1. If the file exists, it will be truncated to zero length first.
2. The struct egi_uniHanGroup MUST be packed type, and MUST NOT include any
   pointers.

@fpath:		File path.
@set: 		Pointer to an EGI_UNIHANGROUP_SET.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------------*/
int UniHanGroup_save_set(const EGI_UNIHANGROUP_SET *group_set, const char *fpath)
{
	int i;
	int nwrite;
	int nmemb;
	FILE *fil;
	int ret=0;
	char magic_words[16];
	uint32_t ugroups_size;		/* Total number of unihan_groups in the group_set */
	uint32_t uchars_size;		/* Total size of group_set->uchars */
	uint32_t typings_size;		/* Total size of group_set->typings */
	uint8_t size_bts[4];		/* size divided into 4 bytes */

	if( group_set==NULL || group_set->ugroups==NULL )
		return -1;

	/* Get group_set memebers */
	ugroups_size=group_set->ugroups_size;
	uchars_size=group_set->uchars_size;
	typings_size=group_set->typings_size;

	/* Open/create file */
        fil=fopen(fpath, "wb");
        if(fil==NULL) {
                printf("%s: Fail to open file '%s' for write. ERR:%s\n", __func__, fpath, strerror(errno));
                return -2;
        }

	/* Write UNIHANGROUP magic words, Totally 16bytes. */
	strncpy(magic_words,UNIHANGROUPS_MAGIC_WORDS,16);
	nmemb=16;
	nwrite=fwrite( magic_words, 1, nmemb, fil);     /* 1 byte each time */
        if(nwrite < 16 ) {
                if(ferror(fil))
       	                printf("%s: Fail to write magic words to '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fwrite magic words %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
               	ret=-3;
		goto END_FUNC;
	}

	/* Write setname */
	nmemb=UNIHANGROUP_SETNAME_MAX;
	nwrite=fwrite(group_set->name, 1, nmemb, fil);  	/* 1 byte each time */
        if(nwrite < nmemb) {
                if(ferror(fil))
       	                printf("%s: Fail to fwrite group_set name to '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fwrite group_set name %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
               	ret=-4;
		goto END_FUNC;
	}

	/* FWrite ugroups_size of EGI_UNIHANGROUPs */
	for(i=0; i<4; i++)
		size_bts[i]=(ugroups_size>>(i<<3))&0xFF; /* Split size to bytes, The least significant byte first. */
	nmemb=4;
	nwrite=fwrite( size_bts, 1, nmemb, fil);  	/* 1 byte each time */
        if(nwrite < nmemb) {
                if(ferror(fil))
       	                printf("%s: Fail to fwrite ugroups_size of EGI_UNIHANGROUPs to '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fwrite ugroups_size of EGI_UNIHANGROUPs %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
               	ret=-5;
		goto END_FUNC;
	}

	/* FWrite uchars_size of EGI_UNIHANGROUPs */
	for(i=0; i<4; i++)
		size_bts[i]=(uchars_size>>(i<<3))&0xFF;	/* Split size to bytes, The least significant byte first. */
	nmemb=4;
	nwrite=fwrite( size_bts, 1, nmemb, fil);  	/* 1 byte each time */
        if(nwrite < nmemb) {
                if(ferror(fil))
       	                printf("%s: Fail to fwrite uchars_size to '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fwrite uchars_size %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
               	ret=-6;
		goto END_FUNC;
	}

	/* FWrite typings_size of EGI_UNIHANGROUPs */
	for(i=0; i<4; i++)
		size_bts[i]=(typings_size>>(i<<3))&0xFF; /* Split size to bytes, The least significant byte first. */
	nmemb=4;
	nwrite=fwrite( size_bts, 1, nmemb, fil);  	/* 1 byte each time */
        if(nwrite < nmemb) {
                if(ferror(fil))
       	                printf("%s: Fail to fwrite typings_size to '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fwrite typings_size %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
               	ret=-7;
		goto END_FUNC;
	}

	/* TEST ---- */
	printf("%s: ugroups_size=%d, uchars_size=%d, typings_size=%d.\n",__func__, ugroups_size, uchars_size, typings_size);

	/* 2. Fwrite ugroups */
	nmemb=ugroups_size*sizeof(EGI_UNIHANGROUP);
	nwrite=fwrite(group_set->ugroups, 1, nmemb, fil);  	/* 1 byte each time */
        if(nwrite != nmemb) {
                if(ferror(fil))
       	                printf("%s: Fail to fwrite group_set->ugroups to '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fwrite group_set->ugroups %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
               	ret=-8;
		goto END_FUNC;
	}

	/* 3. Fwrite uchars */
	nmemb=uchars_size*sizeof(typeof(*group_set->uchars));
	nwrite=fwrite(group_set->uchars, 1, nmemb, fil);  	/* 1 byte each time */
        if(nwrite != nmemb) {
                if(ferror(fil))
       	                printf("%s: Fail to fwrite group_set->uchars to '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fwrite group_set->uchars %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
               	ret=-9;
		goto END_FUNC;
	}

	/* 4. Fwrite typings */
	nmemb=typings_size*sizeof(typeof(*group_set->typings));
	nwrite=fwrite(group_set->typings, 1, nmemb, fil);  	/* 1 byte each time */
        if(nwrite != nmemb) {
                if(ferror(fil))
       	                printf("%s: Fail to fwrite group_set->typings to '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fwrite group_set->typings %d bytes of total %d bytes.\n", __func__, nwrite, nmemb);
               	ret=-10;
		goto END_FUNC;
	}


END_FUNC:
        /* Close fil */
        if( fclose(fil) !=0 ) {
                printf("%s: Fail to close file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
                ret=-11;
        }

	return ret;
}


/*--------------------------------------------------------------------------------
Read EGI_UNIHANGROUPs from a data file and load them to an EGI_UNIHANGROUP_SET.

@fpath:		File path.
@set: 		A pointer to EGI_UNIHANGROUP_SET.
		The caller MUST ensure enough space to hold all
		readin data.  !!! WARNING !!!

Return:
   A pointer to EGI_UNIHANGROUP_SET	OK, however its data may not be complete
                                    	    	if error occurs during file reading.
   NULL					Fails
---------------------------------------------------------------------------------*/
EGI_UNIHANGROUP_SET* UniHanGroup_load_set(const char *fpath)
{
	EGI_UNIHANGROUP_SET *group_set=NULL;
	int i;
	int nread;
	int nmemb;
	FILE *fil;
	char magic_words[16+1]={0};
	uint32_t ugroups_size;		/* Total number of unihan_groups in the group_set */
	uint32_t uchars_size;		/* Total size of group_set->uchars */
	uint32_t typings_size;		/* Total size of group_set->typings */
	uint8_t size_bts[4];		/* size divided into 4 bytes */
	char setname[UNIHANGROUP_SETNAME_MAX];
	void *ptmp;

	/* Open/create file */
        fil=fopen(fpath, "rb");
        if(fil==NULL) {
                printf("%s: Fail to open file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
		return NULL;
	}

	/* Read UNIHANGROUP magic words, Totally 16bytes. */
	nmemb=16;
	nread=fread(magic_words, 1, nmemb, fil);     /* 1 byte each time */
        if(nread < nmemb ) {
                if(ferror(fil))
       	                printf("%s: Fail to read magic words from '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fread magic words %d bytes of total %d bytes.\n", __func__, nread, nmemb);
		goto END_FUNC;
	}
	//printf("%s: magic_words: %s\n",__func__, magic_words);
	/* Check magic words */
	if( strncmp(magic_words,UNIHANGROUPS_MAGIC_WORDS,12) != 0 ) {
		printf("%s: It's NOT an UNIHANGROUP data file!\n",__func__);
		goto END_FUNC;
	}

	/* Read set name */
	nmemb=UNIHANGROUP_SETNAME_MAX;
	nread=fread(setname, 1, nmemb, fil);  	/* 1 byte each time */
        if(nread < nmemb) {
                if(ferror(fil))
       	                printf("%s: Fail to fread group_set name to '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fread group_set name %d bytes of total %d bytes.\n", __func__, nread, nmemb);
		goto END_FUNC;
	}
	/* Whatever, set EOF */
	setname[UNIHANGROUP_SETNAME_MAX-1]='\0';

	/* Read ugrops_size of unihan groups set */
	nmemb=4;
	nread=fread(size_bts, 1, nmemb, fil);
        if(nread < nmemb ) {
                if(ferror(fil))
       	                printf("%s: Fail to read ugroups_size_bts from '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fread ugroups_size_bts %d bytes of total %d bytes.\n", __func__, nread, nmemb);
		goto END_FUNC;
	}
	for(ugroups_size=0,i=0; i<4; i++)	/* Assemble to ugroups_size */
		ugroups_size += size_bts[i]<<(i<<3);

	/* Read uchars_size of unihan groups set */
	nmemb=4;
	nread=fread(size_bts, 1, nmemb, fil);
        if(nread < nmemb ) {
                if(ferror(fil))
       	                printf("%s: Fail to read uchars_size_bts from '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fread uchars_size_bts %d bytes of total %d bytes.\n", __func__, nread, nmemb);
		goto END_FUNC;
	}
	for(uchars_size=0,i=0; i<4; i++)	/* Assemble to ugroups_size */
		uchars_size += size_bts[i]<<(i<<3);

	/* Read typings_size of unihan groups set */
	nmemb=4;
	nread=fread(size_bts, 1, nmemb, fil);
        if(nread < nmemb ) {
                if(ferror(fil))
       	                printf("%s: Fail to read typings_size_bts from '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fread typings_size_bts %d bytes of total %d bytes.\n", __func__, nread, nmemb);
		goto END_FUNC;
	}
	for(typings_size=0,i=0; i<4; i++)	/* Assemble to ugroups_size */
		typings_size += size_bts[i]<<(i<<3);

	/* TEST ---- */
	printf("%s: ugroups_size=%d, uchars_size=%d, typings_size=%d.\n",__func__, ugroups_size, uchars_size, typings_size);

	/* 1. Create group set */
	group_set=UniHanGroup_create_set(setname, ugroups_size); /* ugroups_size as capacity */
	if(group_set==NULL)
		goto END_FUNC;

	/* Assign memebers */
	group_set->ugroups_size=ugroups_size;
	group_set->ugroups_capacity=ugroups_size;

	group_set->uchars_size=uchars_size;
	group_set->uchars_capacity=uchars_size;

	group_set->typings_size=typings_size;
	group_set->typings_capacity=typings_size;

	/* 2. Fread ugroups */
	nmemb=ugroups_size*sizeof(EGI_UNIHANGROUP);
	nread=fread(group_set->ugroups, 1, nmemb, fil);
	if( nread != nmemb ) {
                if(ferror(fil))
       	                printf("%s: Fail to read ugroups from '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fread ugroups %d bytes of total %d bytes.\n", __func__, nread, nmemb);

		UniHanGroup_free_set(&group_set);
		goto END_FUNC;
	}

	/* 3. Fread uchars */
	/* 3.1 realloc group_set->uchars */
	ptmp=realloc(group_set->uchars, uchars_size);
	if(ptmp==NULL) {
		printf("%s: Fail to realloc group_set->uchars!\n",__func__);
		UniHanGroup_free_set(&group_set);
		goto END_FUNC;
	}
	group_set->uchars=ptmp;

	/* 3.2 Fread group_set->uchars */
	nmemb=uchars_size*sizeof(typeof(*group_set->uchars));
	nread=fread(group_set->uchars, 1, nmemb, fil);
	if( nread != nmemb ) {
                if(ferror(fil))
       	                printf("%s: Fail to read uchars from '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fread uchars %d bytes of total %d bytes.\n", __func__, nread, nmemb);
		UniHanGroup_free_set(&group_set);
		goto END_FUNC;
	}

	/* 4. Fread typings */
	/* 4.1 realloc group_set->typings */
	ptmp=realloc(group_set->typings, typings_size);
	if(ptmp==NULL) {
		printf("%s: Fail to realloc group_set->typings!\n",__func__);
		UniHanGroup_free_set(&group_set);
		goto END_FUNC;
	}
	group_set->typings=ptmp;
	/* 4.2 Fread group_set->typings */
	nmemb=typings_size*sizeof(typeof(*group_set->typings));
	nread=fread(group_set->typings, 1, nmemb, fil);
	if( nread != nmemb ) {
                if(ferror(fil))
       	                printf("%s: Fail to read typings from '%s'.\n", __func__, fpath);
               	else
                       	printf("%s: Error, fread typings %d bytes of total %d bytes.\n", __func__, nread, nmemb);
		UniHanGroup_free_set(&group_set);
		goto END_FUNC;
	}


END_FUNC:
        /* Close fil */
        if( fclose(fil) !=0 ) {
                printf("%s: Fail to close file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
        }

	return group_set;
}


/*---------------------------------------------------------------------------
Merge(Copy UNIHANGROUPs of) group_set1 into group_set2, without checking
redundancy.

Note:
1. groups are just added to the end of group_set2->ugroups[], and will NOT be
   sorted after operation!
2. group_set1 will NOT be released/freed after operation!
3. group_set2->ugruops/typings/uchars will mem_grow if capacity is NOT enough.
4. If it fails, successflly merged groups keeps.

@group_set1:	An UNIHANGROUP SET to be emerged.
@group_set2:	The receiving UNIHANGRUOP SET.

Return:
	0	Ok
	<0	Fail
-----------------------------------------------------------------------------*/
int UniHanGroup_merge_set(const EGI_UNIHANGROUP_SET* group_set1, EGI_UNIHANGROUP_SET* group_set2)
{
	int ret=0;
	int i;
	int more=0;		/* more groupss added to uniset2 */
	int len;		/* uchar length, including 0 as EOF */
	int nch;
	unsigned int off_uchar;   /* for group_set 1 */
	unsigned int off_typing; /* for group_set 1 */
	unsigned int pos_uchar;  /* for group_set 2 */
	unsigned int pos_typing; /* for group_set 2 */
	unsigned int index;

	/* Check input */
	if( group_set1==NULL || group_set1->ugroups==NULL || group_set2==NULL || group_set2->ugroups==NULL )
		return -1;

	/* Get index for new groups */
	index=group_set2->ugroups_size;

	/* Get pos_uchar/pos_typing for new groups */
	pos_uchar=group_set2->uchars_size;
	pos_typing=group_set2->typings_size;

	/* Copy groups in group_set1 to group_set2 */
	for(i=0; i < group_set1->ugroups_size; i++)
	{
		/* Get nch for the merging group */
                nch=UHGROUP_WCODES_MAXSIZE;
       	        while( nch>0 && group_set1->ugroups[i].wcodes[nch-1] == 0 ) { nch--; };
		if(nch==0)continue;

		/* Get offset of group_set1 uchar/typing */
		off_uchar=group_set1->ugroups[i].pos_uchar;
		off_typing=group_set1->ugroups[i].pos_typing;

		/* Get length of group_set1 uchars, excluding '\0'  */
		len=cstr_strlen_uft8(group_set1->uchars+off_uchar);
		len+=1; /* +1 EOF */

	/* 1. --- ugroup --- */
		/* 1.1 Check ugroups mem */
		if( group_set2->ugroups_size == group_set2->ugroups_capacity ) {
			if( egi_mem_grow( (void **)&group_set2->ugroups,
						group_set2->ugroups_capacity*sizeof(EGI_UNIHANGROUP),
						      UHGROUP_UGROUPS_GROW_SIZE*sizeof(EGI_UNIHANGROUP) ) != 0 )
			{
                                        printf("%s: Fail to mem grow group_set2->ugroups!\n", __func__);
					ret=-2;
                                        goto END_FUNC;
                        }
			/* Update capacity */
			group_set2->ugroups_capacity += UHGROUP_UGROUPS_GROW_SIZE;
		}

		/* 1.2 Copy ugroups[]: wcodes and freq */
		group_set2->ugroups[index]=group_set1->ugroups[i];

	/* 2. --- uchars --- */
                /* 2.1 Check uchars mem. Here len including '\0' */
                if( group_set2->uchars_size+len > group_set2->uchars_capacity ) {
                        if( egi_mem_grow( (void **)&group_set2->uchars,
                                                group_set2->uchars_capacity*1, 				//sizeof(typeof(*group_set->uchars)),
                                                      UHGROUP_UCHARS_GROW_SIZE*1 ) != 0 )
                        {
                                        printf("%s: Fail to mem grow group_set2->uchars!\n", __func__);
					ret=-3;
                                        goto END_FUNC;
                        }
                        /* Update capacity */
                        group_set2->uchars_capacity += UHGROUP_UCHARS_GROW_SIZE;
                }

		/* 2.2 Copy uchars */
		strncpy((char *)group_set2->uchars+pos_uchar, (char *)group_set1->uchars+off_uchar, len); /*  here len including EOF */

		/* 2.3 Assign pos_uchar */
		group_set2->ugroups[index].pos_uchar=pos_uchar;

		/* 2.4 update pos_uchar for next ugroups. */
		pos_uchar +=len;  /* here len including +1 '\0' */

		/* 2.5 update uchars_size here  */
		group_set2->uchars_size = pos_uchar;  /* SAME AS, as uchar_size starts from 0 */

	/* 3. --- typings --- */
		/* 3.1 Check typings mem */
                if( group_set2->typings_size + nch*UNIHAN_TYPING_MAXLEN > group_set2->typings_capacity ) {
                        if( egi_mem_grow( (void **)&group_set2->typings,
                                                group_set2->typings_capacity*1,			//sizeof(typeof(*group_set->typings)),
                                                      UHGROUP_TYPINGS_GROW_SIZE ) != 0 )
                        {
                                        printf("%s: Fail to mem grow group_set2->typings!\n", __func__);
					ret=-4;
                                        goto END_FUNC;
                        }
                        /* Update capacity */
                        group_set2->typings_capacity += UHGROUP_TYPINGS_GROW_SIZE;
                }

		/* 3.2 Copy typings */
		memcpy(group_set2->typings+pos_typing, group_set1->typings+off_typing, nch*UNIHAN_TYPING_MAXLEN);

		/* 3.3 Assign pos_typing */
		group_set2->ugroups[index].pos_typing=pos_typing;

		/* 3.4 Update pos_typing for next group */
		pos_typing += nch*UNIHAN_TYPING_MAXLEN;

		/* 3.5 Update typings_size */
		group_set2->typings_size = pos_typing;  /* SAME AS, typings_size starts from 0 */

	/* 4. --- Count on --- */
		/* Update index */
		index++;

		/* Update ugroups_size, add one by one, in case func exists abruptly we can still keep data of merged groups. */
		group_set2->ugroups_size +=1;
		more++;
	}

END_FUNC:
	printf("%s: Merged %d unihan groups into group_set2!\n",__func__, more);

	/* Reset sort order */
	group_set2->sorder=UNIORDER_NONE;

	return ret;
}


/*-------------------------------------------------------------------------------
Check an UNIHANGROUP SET, if more than one cases are found with the same wcodes[]
,keep only one and clear others.
	!!! --- ONLY redundant Cizus(nch>1) will be purified --- !!!
Note:
1.  Only unihangroups with nch>1 will be purified!
2.  Typings of the unihan_group will be remained if it exists, but it will select
    the last available one.

TODO:
1. Useless bytes exist as mem hole in group_set->typings[]/uchars[] after purification.
   Call UniHanGroup_save_set() and UniHanGroup_load_set() to eliminate negative effects
   of those mem holes.

2. If two UniGroups have same wcodes[] and differen typings,  Only one will be saved
   in data.
   Example:  重重 chong chong   重重 zhong zhong


Return:
	>=0	Ok, total number of repeated unihans removed.
	<0	Fails
------------------------------------------------------------------------------*/
int UniHanGroup_purify_set(EGI_UNIHANGROUP_SET* group_set)
{
	int i,j;
	int k;
	unsigned int off;
	int nch=0;
	EGI_UNIHANGROUP *tmp_group=NULL;   /* As checking group */
	bool		tmp_group_renewed;
	char 		tmp_typings[UNIHAN_TYPING_MAXLEN*UHGROUP_WCODES_MAXSIZE];

	if(group_set==NULL || group_set->ugroups==NULL )
		return -1;

	/* Print informatin */
	printf("%s: UNIHANGROUP '%s' size=%u, capacity=%zu.\n", __func__, group_set->name, group_set->ugroups_size, group_set->ugroups_capacity);

	/* quickSort for UNIORDER_NCH_WCODES */
	if( group_set->sorder != UNIORDER_NCH_WCODES )
	{
		if( UniHanGroup_quickSort_set(group_set, UNIORDER_NCH_WCODES, 10) != 0 ) {
			printf("%s: Fail to quickSort wcode!\n", __func__);
			return -2;
		}
		else
			printf("%s: Succeed to quickSort by KEY=wcode!\n", __func__);
	}

	/* Check repeated unihan_groups: with SAME wcodes[] */
	k=0;
	bzero(tmp_typings,sizeof(tmp_typings));
	tmp_group=group_set->ugroups+0; /* Set first checking wcodes */
	tmp_group_renewed=true;
	for(i=1; i< group_set->ugroups_size; i++) {

		/* Check tmp_group and get nch */
		if(tmp_group_renewed) {
			/* Get total number of unihans in the tmp_group */
	                nch=UHGROUP_WCODES_MAXSIZE;
        	        while( nch>0 && tmp_group->wcodes[nch-1] == 0 ) { nch--; };

			/* Only check nch>1, tmp_group is before current ugroups[i] */
			if( nch<2 ) {
				tmp_group=group_set->ugroups+i; /* Reset tmp_group */
				tmp_group_renewed=true;
				continue;
			}
		}

		/* Check all wcodes[] */
        	for(j=0; j<UHGROUP_WCODES_MAXSIZE; j++) {
                	if( tmp_group->wcodes[j] != group_set->ugroups[i].wcodes[j] )
                        	break;
                }
                /* Redundant groups/wcodes found: tmp_group and ugroups[i] ... */
                if( j==UHGROUP_WCODES_MAXSIZE ) {
			/* Backup typings if NOT zero */
			off=group_set->ugroups[i].pos_typing;
			if( group_set->typings[off] != '\0' )  {
				memcpy(tmp_typings, group_set->typings+off, nch*UNIHAN_TYPING_MAXLEN);
			}

	/* TEST --- */
			UniHanGroup_print_group(group_set, i, true);

			/* Clear it */
			memset(group_set->ugroups+i, 0, sizeof(EGI_UNIHANGROUP));

			/* !!!WARNING!!!: corresponding group_set->typings[] and uchars[] NOT cleared!!!  */

			/* Count cleared groups, and continue checking redundancy with the same wcodes  */
			k++;
			continue;
                }
		/* No redundancy, renew tmp_group */
		else {
			/* Copy typings to tmp_group, ASSUME mem space for typings already allocated! */
			/* Usually there is ONE unihan_group holding PINYIN typings, others are empty. */
			if(tmp_typings[0] != '\0') {
				off=tmp_group->pos_typing;
				memcpy(group_set->typings+off, tmp_typings, nch*UNIHAN_TYPING_MAXLEN);
				bzero(tmp_typings,sizeof(tmp_typings));
			}
			/* Renew tmp_group for next round check */
			tmp_group=group_set->ugroups+i;
			tmp_group_renewed=true;
		}

	}
	printf("%s: Totally %d repeated unihan_groups cleared!\n", __func__, k);

	/* Update size, Note: wcodes_sort range [0 capacity]. so it will NOT affect wcodes_sorting. */
	group_set->ugroups_size -= k;

	/* Reset sorder before sorting */
	group_set->sorder=UNIORDER_NONE;

	/* quickSort wcode:  sort in wcode+typing+freq order. wcodes_sort range [0 capacity] */
	if( UniHanGroup_quickSort_set(group_set, UNIORDER_NCH_WCODES, 10) != 0 ) {
		printf("%s: Fail to quickSort wcode after purification!\n", __func__);
		return -3;
	}

	return k;
}


/*----------------------------------------------------------------------------
Increase/decrease weigh value of frequency to an UNIHANGROUP.

NOTE:
1. The group_set MUST be sorted in ascending order of UNIORDER_NCH_TYPINGS_FREQ.
   ( Call UniHan_quickSort_typing() to achieve. )

2. After increasing freq for the unihan_group, you MAY see position of the group
   in gourp_set->resutls[] sorted by UniHanGroup_quickSort_LTRIndex() NOT changed,
   this is because UniHanGroup_compare_LTRIndex() compares length of PINYIN first.
   see codes in UniHanGroup_compare_LTRIndex().

@group_set:	Pointer to an EGI_UNIHANGROUP_SET.
@typings:	Typing of the target UNIHANGROUP.
		At least nch*UNIHAN_TYPING_MAXLEN bytes of space with EOF!
@wcodes:	Pointer to UNICODEs of the target unihan_group.
@delt:		Variation to the freq value.

Return:
        0       OK
        <0      Fails
------------------------------------------------------------------------------*/
int UniHanGroup_increase_freq(EGI_UNIHANGROUP_SET *group_set, const char* typings, EGI_UNICODE* wcodes, int delt)
{
	unsigned int i,j,k;
	unsigned int start, end;
	unsigned int index;
	int nch;

	if( typings==NULL || wcodes==NULL )
		return -1;
	if( group_set==NULL || group_set->ugroups==NULL || group_set->ugroups_size==0) /* If empty */
		return -1;

	/* Check sort order */
	if( group_set->sorder != UNIORDER_NCH_TYPINGS_FREQ ) {
		printf("%s: Group_set sort order is NOT UNIORDER_NCH_TYPINGS_FREQ!\n", __func__);
		return -2;
	}

        /* Get total number of unihans in the tmp_group */
        nch=UHGROUP_WCODES_MAXSIZE;
        while( nch>0 && wcodes[nch-1] == 0 ) { nch--; };
	if(nch<1)
		return -2;

	/* Check consistency for typings[] and wcodes[] */
	for(i=0; i<nch; i++) {
	   if( typings[(nch-1)*UNIHAN_TYPING_MAXLEN]=='\0' )
		return -2;
	}

	/* Locate typings, results index stored in group_set->resutls[] after locating. */
	if( UniHanGroup_locate_typings(group_set, typings) !=0 ) {
		printf("%s: Fail to locate typing!\n",__func__);
		return -3;
	}

	/* If NO results */
	if( group_set->results_size<1 )
		return -4;

	/* Locate the wcodes */
	for(i=0; i< group_set->results_size; i++) {
		/* Get group_set->ugours[] index */
		index=group_set->results[i];
		if( index > group_set->ugroups_size-1 )
			return -5;

		/* Check all wcodes[], continue loop if NOT find. */
                for(j=0; j<nch; j++) {
                        if( (uint32_t)wcodes[j] != (uint32_t)(group_set->ugroups[index].wcodes[j]) )
                            break;
                }
		if(j<nch)continue;
		printf("nch=%d,j=%d\n",nch, j);
		/* NOW: Find the wcodes! */

		/* Freq overflow check  */
		#if 0
		if( group_set->ugroups[index].freq > (unsigned int)(UINT_MAX-delt) )
			return -6;
		#endif

		/* Increase freq */
		group_set->ugroups[index].freq += delt;
		printf("%s: UNIGHANROUP '%s' freq updated to %d\n",
					__func__, group_set->uchars+group_set->ugroups[index].pos_uchar, group_set->ugroups[index].freq);

	        /* Remind that group_set->results[] has been resorted by UniHanGroup_quickSort_LTRIndex() to order of TypingLen+Freq
	         * at end of UniHanGroup_locate_typings(), so need to restore start/end as of index of group_set->ugroups[].
		 * As we need [start end] to redo quickSort_typings, to narrow sorting range.
	         */
		start=group_set->results[0];
		end=group_set->results[0];
		for(k=0; k < group_set->results_size; k++) {
			if(group_set->results[k]<start)		/* Min. of resutls[] */
				start=group_set->results[k];
			else if(group_set->results[k]>end)	/* Max. of results[] */
				end=group_set->results[k];
		}

		/* Need to resort TYPING_FREQ for the given typing */
		/* TODO: Test, responding time, seems OK.. */
		group_set->sorder=UNIORDER_NONE;
		if( UniHanGroup_quickSort_typings(group_set, start, end, 5) !=0 ) {
		//if( UniHanGroup_quickSort_typings(group_set, 0, group_set->ugroups_size-1, 9) !=0 ) { /* Sort all range, Slow */
			printf("%s: Fail to resort NCH_TYPINGS_FREQ after frequency incremental!\n",__func__);
			return -5;
		}
		else
			group_set->sorder=UNIORDER_NCH_TYPINGS_FREQ;

		return 0;  /* OK */

        }
	/* END for() NO results! */

       return -6;
}

/*-------------------------------------------------------------------------------------
Replace typings of some unihan_groups in the group_set by that of the same unihan_groups
in the update_set.
This is a way to modify incorrect typings of some unihan_groups, particularly for
polyphonic unihans.

Example to correct Cizus of '行':
1.  Export group_test to a text file by calling UniHanGroup_saveto_CizuTxt(group_set,"/mmc/group_test.txt").
2.  Extract all Cizus/Phrases containing the same polyphoinc UNIHAN '行':
    cat /mmc/group_test.txt | grep 行 > /mmc/update_words.txt
3.  Modify all typings in update_words.txt manually.
4.  Then reload update_words.txt into update_set by calling UniHanGroup_load_CizuTxt().
5.  Call this function to replace all typings.

@group_set	Pointer to an EGI_UNIHANGROUP_SET that need modification.
@update_set	Pointer to an EGI_UNIHANGROUP_SET holding the correct typings.

Return:
	>=0	Numbers of unihan_groups updated.
	<0	Fails
-------------------------------------------------------------------------------------*/
int UniHanGroup_update_typings(EGI_UNIHANGROUP_SET *group_set, const EGI_UNIHANGROUP_SET *update_set)
{
	int k;
	int nch;
	unsigned int off;
	unsigned int off2;
	int count=0;

	/* check input */
	if(group_set==NULL || update_set==NULL)
		return -1;

	for(k=0; k < update_set->ugroups_size; k++)
	{
		/* Locate wcodes, with same nch+wcodes */
		if( UniHanGroup_locate_wcodes(group_set, NULL, update_set->ugroups[k].wcodes) ==0 )
		{
			/* Get total number of unihans of the group */
		        nch=UHGROUP_WCODES_MAXSIZE;
		        while( nch>0 &&  update_set->ugroups[k].wcodes[nch-1] == 0 ) { nch--; };

			/* group_set->pgrp, only for the first result group. */
			off=group_set->ugroups[group_set->pgrp].pos_typing;
			off2=update_set->ugroups[k].pos_typing;

			/* Replace typings */
			memcpy(group_set->typings+off,update_set->typings+off2, nch*UNIHAN_TYPING_MAXLEN);

			/* TEST ---- */
			#if 1
			printf("%s: typings updated as \n", __func__);
			UniHanGroup_print_group(update_set, k, true);
			UniHanGroup_print_group(group_set, group_set->pgrp, true);
			#endif

			count++;
		}
		/* Not found in group_set */
		else {
			off2=update_set->ugroups[k].pos_uchar;
			printf("%s: Fail to locate '%s'. \n",__func__, update_set->uchars+off2);
		}
	}

	printf("%s: Totally %d typings updated!\n",__func__,count);
	return count;
}
