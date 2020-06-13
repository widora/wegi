/*----------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test for converting Pinyin to Unicode han.

With reference to:
  1. https://blog.csdn.net/firehood_/article/details/7648625
  2. https://blog.csdn.net/FortuneWang/article/details/41575039
  3. http://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip (Unihan_Readings.txt)
  4. http://www.unicode.org/reports/tr38


			--- Definition and glossary ---

Reading:   The pronunciations for a given character in written form.
	   Example:  chuǎng	yīn  (kMandarin)
Pinyin:    Modified reading for keyboard input.
	   Example:  chuang3   yin1  ( With a digital number to indicate the tone )


Note:
1. Prepare 'han_mandarin.txt':
	cat Unihan_Readings.txt | grep kMandarin > han_mandarin.txt

   File 'han_mandarin.txt' contains unicode and mandarin reading(original pinyin):
	... ...
	U+2CBB1	kMandarin	yīn
	U+2CBBF	kMandarin	gài
	... ....

1. Prepare kHanyuPinyin.txt
	cat Unihan_Readings.txt | grep kHanyuPinyin > kHanyuPinyin.txt

   File 'kHanyuPinyin.txt' contains unicode and mandarin reading(original pinyin):
	... ...
	U+92E7	kHanyuPinyin	64207.100:xiàn
	U+92E8	kHanyuPinyin	64208.110:tiě,é
	... ....


2. Extract unicodes and reading to put to an EGI_UNIHAN.


Midas Zhou
-------------------------------------------------------------------------------*/
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_cstring.h"
#include "egi_unihan.h"
#include <errno.h>

#define HANYUPINYIN_TXT_PATH	"/mmc/kHanyuPinyin.txt"
#define UNIHANS_DATA_PATH	"/tmp/unihans_pinyin.dat"

/*===================
	MAIN
===================*/
int main(void)
{
	EGI_UNIHAN_SET	*uniset=NULL;
	char pch[4];
	int i;

	/* Load sysfont */
        printf("FTsymbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0) {
                printf("Fail to load FT sysfonts, quit.\n");
                return -1;
        }

	/* Init FBDEV */
        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev,true);
        fb_position_rotate(&gv_fb_dev,0);


	/* Read txt  into EGI_UNIHAN_SET */
	uniset=UniHan_load_HanyuPinyinTxt(HANYUPINYIN_TXT_PATH);
	if(uniset==NULL)
		exit(1);

        printf("total size=%d:\n", uniset->size);
        for(i=0; i< uniset->size; i++) {
		bzero(pch,sizeof(pch));
        	printf("[%s:%d]%s ",char_unicode_to_uft8(&(uniset->unihans[i].wcode), pch)>0?pch:" ",
								uniset->unihans[i].wcode, uniset->unihans[i].typing);
	}
        printf("\n");
	exit(0);

	/* Sort uniset */
	UniHan_insert_sort(uniset->unihans, uniset->size);
        printf("Sorted typing, total size=%d:\n", uniset->size);
        for(i=0; i< uniset->size; i++) {
		bzero(pch,sizeof(pch));
        	printf("[%s:%d]%s ",char_unicode_to_uft8(&(uniset->unihans[i].wcode), pch)>0?pch:" ",
								uniset->unihans[i].wcode, uniset->unihans[i].typing);
	}
        printf("\n");

	return 0;
}

