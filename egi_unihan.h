/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

        UNIHAN (Hanzi)          汉字
        UNIHAN_SET              汉字集
        UNIHANGROUP (Cizu)      词组
        UNIHANGROUP_SET         词组集
	PINYIN			Chinese phoneticizing

Midas Zhou
midaszhou@yahoo.com
----------－--------------------------------------------------------*/
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

/* TXT and DATA files */
#define PINYIN3500_TXT_PATH	"/mmc/pinyin3500.txt"		/* 3500 frequently used Haizi TXT */
#define PINYIN3500_DATA_PATH    "/mmc/unihan3500.dat"		/* Saved UNIHAN SET containing above unihans */

#define MANDARIN_TXT_PATH    	"/mmc/kMandarin.txt"		/* kMandarin.txt from Unihan_Readings.txt, see UniHan_load_MandarinTxt() */
#define HANYUPINYIN_TXT_PATH    "/mmc/kHanyuPinyin.txt"		/* kHanyuPinyin.txt from Unihan_Readings.txt, see UniHan_load_HanyuPinyinTxt() */
#define UNIHANS_DATA_PATH       "/mmc/unihans_pinyin.dat"	/* Saved UNIHAN SET containing above unihans */

#define CIZU_TXT_PATH			"/mmc/chinese_cizu.txt"		/* Cizu TXT file path */
#define UNIHANGROUPS_DATA_PATH   	"/mmc/unihangroups_pinyin.dat"	/* Saved UNIHANGROUP set
									 * In practice, it's usually merged with UNIHANs set fir PINYIN IME */
#define UNIHANGROUPS_EXPORT_TXT_PATH	"/mmc/group_test.txt"		/* Text file exported from an UNIHANGROUP set
									 * In practice, it's without UNIHNAs exported.*/

#define PINYIN_NEW_WORDS_FPATH	"/mmc/pinyin_new_words" 	/* For collecting new Cizus/Phrases */
#define PINYIN_UPDATE_FPATH	"/tmp/update_words.txt"		/* For updating typings of some Cizus/Phrases */


/* -----------  TXT and DATA files in EGI editor: test_editor.c  ----------------

		  --- 1. PINYIN DATA LOAD PROCEDURE ---

1.A. For SINGLE_PINYIN_INPUT  (单个汉字输入)
1A.1. Load uniset from data UNIHANS_DATA_PATH.
1A.2. UniHanGroup_quickSort_typings(uniset) for PINYIN IME.

1.B. For MULTI_PINYIN_INPUT   (连词输入，也包括了单个汉字输入)
1B.1. Load uniset from data UNIHANS_DATA_PATH.
1B.2. Try to load group_set from text UNIHANGROUPS_EXPORT_TXT_PATH.
1B.3. If 1 fails, then try to load from data UNIHANGROUPS_DATA_PATH.
1B.4. Add/merge uniset to group_set.
1B.5. Readin new words expend_set from text PINYIN_NEW_WORDS_FPATH.
1B.6. Merge expend_set into group_set and purify it, then assemble
      typings for some groups.
1B.7. Load update_set from text file PINYIN_UPDATE_FPATH.
1B.8. Replace and modify typings in group_set by calling UniHanGroup_update_typings(update_set).
1B.9. UniHanGroup_quickSort_typings(group_set) for PINYIN IME.

		  --- 2. PINYIN DATA SAVE PROCEDURE ---

2.A. For SINGLE_PINYIN_INPUT
2A.1. Save uniset to UNIHANS_DATA_PATH as freq values updated during input.

2.B. For MULTI_PINYIN_INPUT
2B.1. Freq values are updated automatically during PINYIN input.
2B.2. First save group_set to text file UNIHANGROUPS_EXPORT_TXT_PATH.
2B.3. Then save group_set to data file UNIHANGROUPS_DATA_PATH.
      (Note: If system crashes, we hope at lease one file will keep safe.)

3. Note:
3.0. Original UNIHANGROUPS_DATA and UNIHANS_DATA are generated as described
     in "egi_unihan.c".
3.1. PINYIN_NEW_WORDS are merged into UNIHANGROUPS_DATA and typings are replaced
    /updated according to PINYIN_UPDATE_FPATH, so all those new words and
    corrected typings will be saved to both data file and text file.

3.2.  !!! ----- IMPORTANT ----- !!!
  UNIHANGROUPS_EXPORT_TXT_PATH and UNIHANGROUPS_DATA_PATH are as backups
  for each other, if one corrupted, the other can restore/create it.
  see procedure 1B.2 and 1B.3.

3.3. !!! ----- IMPORTANT ----- !!!
  Manually backup following data/text files from time to time, for worst scenario.
  group_test.txtBK  unihangroups_pinyin.datBK  unihans_pinyin.datBK

3.4. Collect new words by selecting and right click menu 'Words' in egi editor,
  ( !!! You'd better select both Cizu and its PINYIN. UniHanGroup_assemble_typings()
    will probably give wrong PINYINs for polyphonic unihans. !!! )
  It will be saved into PINYIN_NEW_WORDS_FPATH, which will be merged into group_set
  next time you load PINYIN data.  See procedure 1B.5.

3.5. If wrong typings are found, then manually create text file PINYIN_UPDATE_FPATH
  and put Cizu and right typings in it. Next time when you start test_editor, it
  will be loaded and used to correct wrong typings in group_set. See procedure 1B.7 and 1B.8.

-------------------------------------------------------------------------------*/
/* File data magic words */
#define UNIHANS_MAGIC_WORDS		"UNIHANS0"		/* 8bytes, unihans data file magic words */
#define UNIHANGROUPS_MAGIC_WORDS	"UNIHANGROUPS0000"	/* 12+4(Ver)=16bytes, unihan_groups data file magic words */

typedef enum UniHanSortOrder
{
	/* Ascending order of keys */
	UNIORDER_NONE			=0,
	UNIORDER_FREQ			=1,	/* For unihans */
	UNIORDER_TYPING_FREQ		=2,	/* For unihans */
	UNIORDER_WCODE_TYPING_FREQ	=3,	/* For unihans */
	UNIORDER_NCH_TYPINGS_FREQ	=4,	/* For unihan_groups. NCH: number of unihans in a group. */
	UNIORDER_NCH_WCODES		=5,	/* For unihan_groups */
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

typedef struct egi_uniHanGroup		EGI_UNIHANGROUP;	/* A struct for Unicode Han Groups (as Cizu/Words/Phrases), nch==1 is also OK */
typedef struct egi_uniHanGroup_set	EGI_UNIHANGROUP_SET;  	/* A set of Unicode Han Groups */

struct egi_unihan
{
        EGI_UNICODE             wcode;          /* Unicode for a Han char */

	#define			UNIHAN_UCHAR_MAXLEN	(4+1)   /* 1 for EOF */
	char			uchar[UNIHAN_UCHAR_MAXLEN];	/* For UFT-8 encoding */

        unsigned int            freq;           /* Frequency of the wcode.  NOW: polyphonic unihans has the same freq value. */

	#define			UNIHAN_TYPING_MAXLEN	8	/* 1 for EOF */
        char                    typing[UNIHAN_TYPING_MAXLEN];      /* Keyboard input sequence that representing the wcode , in ASCII lowercase.
					 	 * Example: PINYIN typing of "chuǎng" is "chuang3" or "chuang"
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
	#define 		UNIHAN_SETNAME_MAX  16
        char                    name[UNIHAN_SETNAME_MAX];  /* Short name for the UniHan set, MUST NOT be a pointer. Will change data file! */

	size_t			capacity;	/* Capacity to hold max number of UNIHANS */
        uint32_t                size;           /* Size of unihans, total number of EGI_UNIHANs in unihans[], exclude empty ones.
                                                 * Do not change the type, we'll assembly/disassembly from/into uint8_t when read/write to file.
                                                 */
        //int                     input_method;   /* input method: pinyin,  ...  */

	unsigned int		puh;		/* Index to an unihans[], usually to store result of loacting/searching.  */

	UNIHAN_SORTORDER	sorder;		/* Sort order ID. */

	#define 		UNIHANS_GROW_SIZE   64   /* Note: All grow_size MUST >=  increment size in functions */
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

struct egi_uniHanGroup		/* UNIHAN Words/Phrasese/Cizus */
{
	#define		UHGROUP_WCODES_MAXSIZE   4   	/* Max. number of unihans contained in a uniHanGroup */
	EGI_UNICODE	wcodes[UHGROUP_WCODES_MAXSIZE];	/* UNICODEs for UNIHANs, NO non_UNIHAN chars!!! */
	unsigned int	freq;		/* Frequency of the unihan group */
	unsigned int 	pos_uchar;	/* Position of utf8-encoding, offset to buffer, as of EGI_UNINHANGROUP_SET.uchars */
	unsigned int 	pos_typing;	/* Position of typing, offset to buffer, as of EGI_UNIHANGROUP_SET.typings */
}__attribute__((packed));               /* To avoid byte aligment, with consideration of saving structs to a file. */

struct egi_uniHanGroup_set
{
   	#define 		UNIHANGROUP_SETNAME_MAX  32
        char                    name[UNIHANGROUP_SETNAME_MAX];       /* Short name for the UniHanGroups set, MUST NOT be a pointer. */
        //int                     input_method;   /* input method: pinyin,  ...  */

	unsigned int		pgrp;		/* Index to an unihgroups[], usually to store result of loacting/searching.  */
						/* Check sorder to confirm pgrp is valid! */
	UNIHAN_SORTORDER	sorder;		/* Sort order ID. */

	size_t			ugroups_capacity;	/* Capacity of unihgroups[], include empty unihgrups[] with unihgrups->wcodes[0]==0 */
	uint32_t		ugroups_size;		/* Current total number of UniHanGroups in ugroups, excluding empty ones.
						  	 * Do not change the type, we'll assembly/disassembly from/into uint8_t.
							 */
        EGI_UNIHANGROUP        *ugroups;    		/* Array of UniHanGroups */
				/* !pitfall : After sorting, the last ugroups[] usually does NOT correspond to the last typings[]/uchars[]! */
	#define 		UHGROUP_UGROUPS_GROW_SIZE   64   /* Note: All grow_size MUST >=  increment size in functions */

	size_t			uchars_capacity;	/* Total mem space allocated for uchars[], in bytes. */
	uint32_t		uchars_size;		/* Current used mem space. in bytes. including all '\0' as dilimiters, keep UINT32_T */
	UFT8_PCHAR		uchars;			/* A buffer to hold all unihans groups in uft8 encoding, with '\0' as dilimiters */
				/* !pitfall : After sorting, the last ugroups[] usually does NOT correspond to the last typings[]/uchars[]! */
	#define 		UHGROUP_UCHARS_GROW_SIZE    512

	size_t			typings_capacity;	/* Total mem space allocated for typings[], in bytes(sizeof(char)) */
	uint32_t		typings_size;		/* Current used mem space, in bytes, including all '\0' as dilimiters, keep UINT32_T */
	char			*typings;		/* A buffer to hold all typings, with '\0' as dilimiters */
							/* Pitfall: Mem space unit is UNIHAN_TYPING_MAXLEN for each wcode! each typing is separated! */
				/* !pitfall : After sorting, the last ugroups[] usually does NOT correspond to the last typings[]/uchars[]! */
	#define 		UHGROUP_TYPINGS_GROW_SIZE   512

	size_t			results_capacity;	/* Total mem allocated for result[], in sizeof(results). Init. as grow_size */
	size_t			results_size;		/* number of available indexes in results[], NOT capacity of results[].   */
	unsigned int		*results; 		/* An array for storing temporary ugroups[] indexes, as results of a search operation
							 * by UniHanGroup_locate_typings(), AND for further sorting operations.
							 */
	#define			UHGROUP_RESULTS_GROW_SIZE    512   /* Also as Result LIMITS for locating/searching typings
								    * See at end of UniHanGroup_locate_typings().
								    */
};


		/* ======= 	1. UniHan functions 	======== */

/* PINYIN Functions */
int  UniHan_divide_pinyin(char *strp, char *pinyin, int n);

/* UNIHAN HEAP Funcitons */
EGI_UNIHAN_HEAP* UniHan_create_heap(size_t capacity);
void 		 UniHan_free_heap( EGI_UNIHAN_HEAP **heap);


/* Order_comparing and Sort Functions */
#define  CMPORDER_IS_AHEAD    -1
#define  CMPORDER_IS_SAME      0
#define  CMPORDER_IS_AFTER     1
int 	UniHan_compare_typing(const EGI_UNIHAN *uhan1, const EGI_UNIHAN *uhan2);

	/* Sort order: typing + freq */
void 	UniHan_insertSort_typing(EGI_UNIHAN* unihans, int n );
int 	UniHan_quickSort_typing(EGI_UNIHAN* unihans, unsigned int start, unsigned int end, int cutoff);

	/* Sort order: freq only */
void 	UniHan_insertSort_freq( EGI_UNIHAN* unihans, int n );
int 	UniHan_quickSort_freq(EGI_UNIHAN* unihans, unsigned int start, unsigned int end, int cutoff);

	/* Sort order: wcode + typing + freq */
void 	UniHan_insertSort_wcode( EGI_UNIHAN* unihans, int n );
int 	UniHan_quickSort_wcode(EGI_UNIHAN* unihans, unsigned int start, unsigned int end, int cutoff);

/* UNISET Sort Function */
int 	UniHan_quickSort_set(EGI_UNIHAN_SET* uniset, UNIHAN_SORTORDER sorder, int cutoff);

/* UNIHAN SET Functions */
EGI_UNIHAN_SET* UniHan_create_set(const char *name, size_t capacity);
void 		UniHan_free_set( EGI_UNIHAN_SET **set);
void 		UniHan_reset_freq( EGI_UNIHAN_SET *uniset );

int 		UniHan_save_set(const EGI_UNIHAN_SET *set, const char *fpath);
EGI_UNIHAN_SET* UniHan_load_set(const char *fpath);

int 		UniHan_locate_wcode(EGI_UNIHAN_SET* uniset, EGI_UNICODE wcode);
int 		UniHan_locate_typing(EGI_UNIHAN_SET* uniset, const char* typing);
int 		UniHan_poll_freq(EGI_UNIHAN_SET *uniset, const char *fpath);
int 		UniHan_increase_freq(EGI_UNIHAN_SET *uniset, const char* typing, EGI_UNICODE wcode, int delt);
/* UNIHAN SET: Merge and purify */
int 		UniHan_merge_set(const EGI_UNIHAN_SET* uniset1, EGI_UNIHAN_SET* uniset2);
int 		UniHan_purify_set(EGI_UNIHAN_SET* uniset );

/* UNIHAN SET: Read formated txt into uniset */
EGI_UNIHAN_SET* UniHan_load_HanyuPinyinTxt(const char *fpath);
EGI_UNIHAN_SET* UniHan_load_MandarinTxt(const char *fpath);

/* Convert reading to pinyin */
int 	UniHan_reading_to_pinyin( const UFT8_PCHAR reading, char *pinyin);

void 	UniHan_print_wcode(EGI_UNIHAN_SET *uniset, EGI_UNICODE wcode);

		/* ======= 	2. UniHan Group functions 	======== */

EGI_UNIHANGROUP_SET* 	UniHanGroup_create_set(const char *name, size_t capacity);
void 		   	UniHanGroup_free_set( EGI_UNIHANGROUP_SET **set);

EGI_UNIHANGROUP_SET* 	UniHanGroup_load_CizuTxt(const char *fpath);
int	UniHanGroup_saveto_CizuTxt(const EGI_UNIHANGROUP_SET *group_set, const char *fpath);
int 	UniHanGroup_add_uniset(EGI_UNIHANGROUP_SET *group_set, const EGI_UNIHAN_SET *uniset);

int 	UniHanGroup_assemble_typings(EGI_UNIHANGROUP_SET *group_set, EGI_UNIHAN_SET *han_set);

int 	UniHanGroup_wcodes_size(const EGI_UNIHANGROUP *group);

void 	UniHanGroup_print_set(const EGI_UNIHANGROUP_SET *group_set, int start, int end);
void 	UniHanGroup_print_group(const EGI_UNIHANGROUP_SET *group_set, int index, bool line_feed);
void 	UniHanGroup_print_results( const EGI_UNIHANGROUP_SET *group_set );

void 	UniHanGroup_search_uchars(const EGI_UNIHANGROUP_SET *group_set, UFT8_PCHAR uchar);
int 	UniHanGroup_locate_uchars(const EGI_UNIHANGROUP_SET *group_set, UFT8_PCHAR uchars);

	/* Sort order: nch + typing + freq */
int 	UniHanGroup_compare_typings( const EGI_UNIHANGROUP *group1, const EGI_UNIHANGROUP *group2, const EGI_UNIHANGROUP_SET *group_set);
int 	UniHanGroup_insertSort_typings(EGI_UNIHANGROUP_SET *group_set, int start, int n);
int 	UniHanGroup_quickSort_typings(EGI_UNIHANGROUP_SET* group_set, unsigned int start, unsigned int end, int cutoff);

//static inline int compare_group_typings(const char *group_typing1, int nch1, const char *group_typing2, int nch2, bool TYPING2_IS_SHORT);
int 	strstr_group_typings(const char *hold_typings, const char* try_typings, int nch);
int 	strcmp_group_typings(const char *hold_typings, const char* try_typings, int nch);
void 	print_group_typings(const char* typings, unsigned int nw);
int  	UniHanGroup_locate_typings(EGI_UNIHANGROUP_SET* group_set, const char* typings);
int 	UniHanGroup_increase_freq(EGI_UNIHANGROUP_SET *group_set, const char* typings, EGI_UNICODE* wcodes, int delt);

	/* Sort order: nch + wcodes */
int 	UniHanGroup_compare_wcodes( const EGI_UNIHANGROUP *group1, const EGI_UNIHANGROUP *group2 );
int     UniHanGroup_insertSort_wcodes(EGI_UNIHANGROUP_SET *group_set, int start, int n);
int 	UniHanGroup_quickSort_wcodes(EGI_UNIHANGROUP_SET* group_set, unsigned int start, unsigned int end, int cutoff);
int  	UniHanGroup_locate_wcodes(EGI_UNIHANGROUP_SET* group_set,  const UFT8_PCHAR uchars, const EGI_UNICODE *wcodes);

/* UNIHANGROUP_SET: Sort Function */
int 	UniHanGroup_quickSort_set(EGI_UNIHANGROUP_SET* group_set, UNIHAN_SORTORDER sorder, int cutoff);

	/* For UniHanGroup_locate_typings() results */
int 	UniHanGroup_compare_LTRIndex(EGI_UNIHANGROUP_SET* group_set, unsigned int n1, unsigned int n2);
void 	UniHanGroup_insertSort_LTRIndex(EGI_UNIHANGROUP_SET *group_set, int start, int n);
int 	UniHanGroup_quickSort_LTRIndex(EGI_UNIHANGROUP_SET* group_set, int start, int end, int cutoff);

/* UNIHANGROUP_SET: Merge and purify */
        /* UniHanGroup_add_uniset(): to merge an EGI_UNIHAN SET to an UNIHANGROUP SET */
int     UniHanGroup_merge_set(const EGI_UNIHANGROUP_SET* group_set1, EGI_UNIHANGROUP_SET* group_set2);
int 	UniHanGroup_purify_set(EGI_UNIHANGROUP_SET* group_set);

int	UniHanGroup_update_typings(EGI_UNIHANGROUP_SET *group_set, const EGI_UNIHANGROUP_SET *update_set);

/* UNIHANGROUP_SET: Save and load */
int 			UniHanGroup_save_set(const EGI_UNIHANGROUP_SET *group_set, const char *fpath);
EGI_UNIHANGROUP_SET* 	UniHanGroup_load_set(const char *fpath);

#endif
