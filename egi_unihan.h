/*-------------------------------------------------------------------------------------------------------------
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


Midas Zhou
midaszhou@yahoo.com
---------------------------------------------------------------------------------------------------------------*/
#ifndef __EGI_UNIHAN_H__
#define __EGI_UNIHAN_H__

#include <stdlib.h>

/* ------------------   EGI_UNIHAN   ------------------
 * A struct to store a Han Unicode informations.
 * 1. The same wcode may has several pinyin, each stored in a EGI_UNIHAN separately.
 * 2. MUST NOT include any pointer type, as the sturct is expected to be saved to a file.
 ----------------------------------------------------*/
typedef struct egi_unihan       EGI_UNIHAN;      /* A struct for Unicode Han */
typedef struct egi_unihan_set   EGI_UNIHAN_SET;  /* A set of Unicode Hans, with common property, such as input_method or locale set etc. */
typedef struct egi_unihan_heap	EGI_UNIHAN_HEAP; /* A heap of Unicode Hans, It's sorted in certain order. */
struct egi_unihan
{
        wchar_t                 wcode;          /* Unicode for a Han char */
        unsigned int            freq;           /* Frequency use of the wcode. */

        char                    typing[8];      /* Keyboard input sequence that representing the wcode , in ASCII.
					 	 * Example: typing of "chuǎng" is "chuang3"
                                                 * 1. Its size depends on input method.
                                                 * 2. For PINYIN typing, 8bytes is enough.
                                                 *    MAX. size example: "chuang3", here '3' indicates the tone.
                                                 */

        char                    reading[16];    /* Reading is the pronunciation of an UniHan in written form.
                                                 * Example: see kMandarin column as in kHanyuPinyin.txt.
						 * 1. Its size depends on type of readings, which may be 'kCantonese' 'kJapaneseKun' etc.
                                                 * 2. For kMandarin reading, 16bytes is enough.
						 *    MAX. size example: "chuǎng" , ǎ-ḿ   is 3-4bytes
                                                 */
} __attribute__((packed));

struct egi_unihan_set
{
        char                    name[16];       /* Short name for the UniHan set */

        uint32_t                size;           /* Size of unihans, total number of EGI_UNIHANs in unihans[]
                                                 * Do not change the type, we'll assembly/disassembly from/into uint8_t when read/write to file.
                                                 */

//      int                     input_method;   /* input method: pinyin, ...  */

        EGI_UNIHAN              *unihans;       /* Pointer to EGI_UNIHANs */
};

struct egi_unihan_heap
{
	size_t		capacity;	/* Mem. space capacity of unihans, */
	size_t		size; 	 	/* Current size, total number of unihans. MUST < capacity  */
	EGI_UNIHAN	*unihans;	/*  */
};



/* UNIHAN HEAP Operation:  sorting algorithm */
EGI_UNIHAN_HEAP* UniHan_create_heap(size_t capacity);
void 		UniHan_free_heap( EGI_UNIHAN_HEAP **heap);



/* UNIHAN SET Functions */
EGI_UNIHAN_SET* UniHan_create_set(const char *name, size_t size);
void 		UniHan_free_set( EGI_UNIHAN_SET **set);


#endif
