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

typedef unsigned char * UFT8_PCHAR;

/* Define P.R.C simple Haizi range */
#define UNICODE_PRC_START     0x4E00
#define UNICODE_PRC_END       0x9FA5


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
        wchar_t                 wcode;          /* Unicode for a Han char */
        unsigned int            freq;           /* Frequency use of the wcode. */

	#define			UNIHAN_TYPING_MAXLEN	8
        char                    typing[UNIHAN_TYPING_MAXLEN];      /* Keyboard input sequence that representing the wcode , in ASCII lowcase.
					 	 * Example: typing of "chuǎng" is "chuang3" or "chuang"
                                                 * 1. Its size depends on input method.
                                                 * 2. For PINYIN typing, 8bytes is enough.
                                                 *    MAX. size example: "chuang3", here '3' indicates the tone.
                                                 */

	#define			UNIHAN_READING_MAXLEN	16
        char                    reading[UNIHAN_READING_MAXLEN];    /* Reading is the pronunciation of an UniHan in written form, with uft-8 encoding.
                                                 * Example: see kMandarin column as in kHanyuPinyin.txt.
						 * 1. Its size depends on type of readings, which may be 'kCantonese' 'kJapaneseKun' etc.
                                                 * 2. For kMandarin reading, 16bytes is enough.
						 *    MAX. size example: "chuǎng" , ǎ-ḿ   is 3-4bytes
                                                 */
} __attribute__((packed));			/* To avoid byte aligment, with consideration of saving structs to a file. */

struct egi_unihan_set
{
        char                    name[16];       /* Short name for the UniHan set */

        uint32_t                size;           /* Size of unihans, total number of EGI_UNIHANs in unihans[]
                                                 * Do not change the type, we'll assembly/disassembly from/into uint8_t when read/write to file.
                                                 */

//      int                     input_method;   /* input method: pinyin, ...  */

        EGI_UNIHAN              *unihans;       /* Array of EGI_UNIHANs */
};

struct egi_unihan_heap
{
	size_t		capacity;	/* Mem. space capacity of unihans, */
	size_t		size; 	 	/* Current size, total number of unihans. MUST < capacity  */
	EGI_UNIHAN	*unihans;	/* Array of EGI_UNIHANs, for a binary heap, index starts from 1, NOT 0. */
};

/* UNIHAN HEAP Funcitons */
EGI_UNIHAN_HEAP* UniHan_create_heap(size_t capacity);
void 		UniHan_free_heap( EGI_UNIHAN_HEAP **heap);

/* Sorting Functions */
int 	UniHan_compare_typing(const EGI_UNIHAN *uhan1, const EGI_UNIHAN *uhan2);
void 	UniHan_insert_sort( EGI_UNIHAN* unihans, int n );

/* UNIHAN SET Functions */
EGI_UNIHAN_SET* UniHan_create_uniset(const char *name, size_t size);
void 		UniHan_free_uniset( EGI_UNIHAN_SET **set);
int 		UniHan_save_uniset(const char *fpath,  const EGI_UNIHAN_SET *set);
EGI_UNIHAN_SET* UniHan_load_uniset(const char *fpath);
EGI_UNIHAN_SET *UniHan_load_HanyuPinyinTxt(const char *fpath);

/* Convert reading to pinyin */
int UniHan_reading_to_pinyin( const UFT8_PCHAR reading, char *pinyin);

#endif
