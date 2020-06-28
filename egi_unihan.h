/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#ifndef __EGI_UNIHAN_H__
#define __EGI_UNIHAN_H__
#include <stdlib.h>

#include "egi_FTsymbol.h"
/*----- See in egi_FTsymbol.h -----
typedef unsigned char * UFT8_PCHAR;
typedef wchar_t 	EGI_UNICODE;
---------------------------------*/

/* UNICODE range for P.R.C simplified Haizi/UNIHAN */
#define UNICODE_PRC_START     0x4E00
#define UNICODE_PRC_END       0x9FA5

#define MANDARIN_TXT_PATH    	"/mmc/kMandarin.txt"		/* kMandarin.txt from Unihan_Readings.txt, see UniHan_load_MandarinTxt() */
#define HANYUPINYIN_TXT_PATH    "/mmc/kHanyuPinyin.txt"		/* kHanyuPinyin.txt from Unihan_Readings.txt, see UniHan_load_HanyuPinyinTxt() */
#define UNIHANS_DATA_PATH       "/mmc/unihans_pinyin.dat"	/* Saved UNIHAN SET containing above unihans */

#define PINYIN3500_TXT_PATH        "/mmc/pinyin3500.txt"	/* 3500 frequently used Haizi TXT */
#define PINYIN3500_DATA_PATH       "/mmc/unihan3500.dat"	/* Saved UNIHAN SET containing above unihans */


typedef enum UniHanSortOrder
{
	/* Ascending order of keys */
	UNIORDER_NONE			=0,
	UNIORDER_FREQ			=1,
	UNIORDER_TYPING_FREQ		=2,
	UNIORDER_WCODE_TYPING_FREQ	=3,

} UNIHAN_SORTORDER;


/* ------------------   EGI_UNIHAN   ------------------
 * A struct to store a Han Unicode informations.
 * 1. The same wcode may has several pinyin, each stored in a EGI_UNIHAN separately.
 * 2. MUST NOT include any pointer type in EGI_UNIHAN, as the sturct is expected to be saved to a file.
 ----------------------------------------------------*/
typedef struct egi_unihan       EGI_UNIHAN;      /* A struct for Unicode Han */
typedef struct egi_unihan_set   EGI_UNIHAN_SET;  /* A set of Unicode Hans, with common property, such as input_method or locale set etc. */
typedef struct egi_unihan_heap	EGI_UNIHAN_HEAP; /* A heap of Unicode Hans, It's sorted in certain order.
						  * Here refers to Priority Binary Heap
						  */
struct egi_unihan
{
        EGI_UNICODE             wcode;          /* Unicode for a Han char */

	#define			UNIHAN_UCHAR_MAXLEN	(4+1)   /* 1 for EOF */
	char			uchar[UNIHAN_UCHAR_MAXLEN];	/* For UFT-8 encoding */

        unsigned int            freq;           /* Frequency of the wcode. */

	#define			UNIHAN_TYPING_MAXLEN	8	/* 1 for EOF */
        char                    typing[UNIHAN_TYPING_MAXLEN];      /* Keyboard input sequence that representing the wcode , in ASCII lowercase.
					 	 * Example: typing of "chuǎng" is "chuang3" or "chuang"
                                                 * 1. Its size depends on input method.
                                                 * 2. For PINYIN typing, 8bytes is enough.
                                                 *    MAX. size example: "chuang3", here '3' indicates the tone.
						 * 3. If tones are ignored then reading[] will also be no use.
                                                 */

	#define			UNIHAN_READING_MAXLEN	16	/* 1 for EOF */
        char                    reading[UNIHAN_READING_MAXLEN];    /* Reading is the pronunciation of an UniHan in written form, with uft-8 encoding.
                                                 * Example: see kMandarin column as in kHanyuPinyin.txt.
						 * 1. Its size depends on type of readings, which may be 'kCantonese' 'kJapaneseKun' etc.
                                                 * 2. For kMandarin reading, 16bytes is enough.
						 *    MAX. size example: "chuǎng" , ǎ-ḿ   Max 3bytes( with Combining Diacritical Marks)
                                                 */
} __attribute__((packed));			/* To avoid byte aligment, with consideration of saving structs to a file. */

struct egi_unihan_set
{
        char                    name[16];       /* Short name for the UniHan set, MUST NOT be a pointer. */

	size_t			capacity;	/* Capacity to hold max number of UNIHANS */
        uint32_t                size;           /* Size of unihans, total number of EGI_UNIHANs in unihans[], exclude empty ones.
                                                 * Do not change the type, we'll assembly/disassembly from/into uint8_t when read/write to file.
                                                 */
//      int                     input_method;   /* input method: pinyin,  ...  */

	unsigned int		puh;		/* Index to an unihans[], usually to store result of loacting/searching.  */

	UNIHAN_SORTORDER	sorder;		/* Sort order ID. */

	#define 		UNIHANS_GROW_SIZE   64
        EGI_UNIHAN              *unihans;       /* Array of EGI_UNIHANs, NOTE: Always malloc capacity+1, with bottom safe guard! */
     	/* +1 EGI_UNIHAN as bottom safe guard */
};

struct egi_unihan_heap
{
	size_t		capacity;	/* Mem. capacity to hold max number of unihans, */
	size_t		size; 	 	/* Current size, total number of unihans. MUST < capacity  */
	EGI_UNIHAN	*unihans;	/* Array of EGI_UNIHANs, for a binary heap, index starts from 1, NOT 0. */
     	/* +1 EGI_UNIHAN as bottom safe guard */
};

/* UNIHAN HEAP Funcitons */
EGI_UNIHAN_HEAP* UniHan_create_heap(size_t capacity);
void 		 UniHan_free_heap( EGI_UNIHAN_HEAP **heap);


/* Order_comparing and Sort Functions */
#define  CMPORDER_IS_AHEAD    -1
#define  CMPORDER_IS_SAME      0
#define  CMPORDER_IS_AFTER     1
int 	UniHan_compare_typing(const EGI_UNIHAN *uhan1, const EGI_UNIHAN *uhan2);

	/* Sort order: typing + freq */
void 	UniHan_insertSort_typing( EGI_UNIHAN* unihans, int n );
int 	UniHan_quickSort_typing(EGI_UNIHAN* unihans, unsigned int start, unsigned int end, int cutoff);
	/* Sort order: freq only */
void 	UniHan_insertSort_freq( EGI_UNIHAN* unihans, int n );
int 	UniHan_quickSort_freq(EGI_UNIHAN* unihans, unsigned int start, unsigned int end, int cutoff);

	/* Sort order: wcode + typing + freq */
void 	UniHan_insertSort_wcode( EGI_UNIHAN* unihans, int n );
int 	UniHan_quickSort_wcode(EGI_UNIHAN* unihans, unsigned int start, unsigned int end, int cutoff);

/* UNISET Sort Function */
int 	UniHan_quickSort_uniset(EGI_UNIHAN_SET* uniset, UNIHAN_SORTORDER sorder, int cutoff);

/* UNIHAN SET Functions */
EGI_UNIHAN_SET* UniHan_create_uniset(const char *name, size_t capacity);
void 		UniHan_free_uniset( EGI_UNIHAN_SET **set);
void 		UniHan_reset_freq( EGI_UNIHAN_SET *uniset );

int 		UniHan_save_uniset(const char *fpath,  const EGI_UNIHAN_SET *set);
EGI_UNIHAN_SET* UniHan_load_uniset(const char *fpath);

int 		UniHan_locate_wcode(EGI_UNIHAN_SET* uniset, EGI_UNICODE wcode);
int 		UniHan_locate_typing(EGI_UNIHAN_SET* uniset, const char* typing);
int 		UniHan_poll_freq(EGI_UNIHAN_SET *uniset, const char *fpath);
int 		UniHan_increase_freq(EGI_UNIHAN_SET *uniset, const char* typing, EGI_UNICODE wcode, int delt);

int 		UniHan_merge_uniset(const EGI_UNIHAN_SET* uniset1, EGI_UNIHAN_SET* uniset2);
int 		UniHan_purify_uniset(EGI_UNIHAN_SET* uniset );

/* Read formated txt into uniset */
EGI_UNIHAN_SET* UniHan_load_HanyuPinyinTxt(const char *fpath);
EGI_UNIHAN_SET* UniHan_load_MandarinTxt(const char *fpath);

/* Convert reading to pinyin */
int 		UniHan_reading_to_pinyin( const UFT8_PCHAR reading, char *pinyin);

void 		UniHan_print_wcode(EGI_UNIHAN_SET *uniset, EGI_UNICODE wcode);

#endif
