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
----------------------------------------------------------------------------------*/
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
	EGI_UNIHAN_SET  *tmpset=NULL;
	char pch[4];
	int i;
	struct timeval	tm_start,tm_end;

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


#if 0	/* Strange wcode */
	int m, pos;
	wchar_t wcode;
	//char *stranges="ê̄,ế,éi,ê̌,ěi,ề,èi";   /* ê̄,ế,ê̌,ề   ế =[0x1ebf]  ê̄ =[0xea  0x304]  ê̌ =[0xea  0x30c] ề =[0x1ec1] */
	char *stranges="ńňǹḿ m̀"; /*   0x144  0x148  0x1f9   ḿ =[0x1e3f  0x20]  m̀=[0x6d  0x300] */
	printf("stranges: %s\n", stranges);
	m=0; pos=0;
	while( (m=char_uft8_to_unicode( (const UFT8_PCHAR)(stranges+pos), &wcode)) > 0 )
	{
		pos+=m;
		printf("0x%x  ", wcode);
	}
	printf("\n");
	exit(1);
#endif

	/* Read txt  into EGI_UNIHAN_SET */
	tmpset=UniHan_load_HanyuPinyinTxt(HANYUPINYIN_TXT_PATH);
	uniset=UniHan_load_HanyuPinyinTxt(HANYUPINYIN_TXT_PATH);
	if( tmpset==NULL || uniset==NULL )
		exit(1);

        printf("total size=%d:\n", uniset->size);
        for(i=0; i< uniset->size; i++) {
		bzero(pch,sizeof(pch));
	    //if(uniset->unihans[i].wcode==0x54B9)
        	printf("[%s:%d]%s ",char_unicode_to_uft8(&(uniset->unihans[i].wcode), pch)>0?pch:" ",
								uniset->unihans[i].wcode, uniset->unihans[i].typing);
	}
        printf("\n");

	/* Sort uniset */
	printf("Start insertSort() UNISET typings...\n");
	gettimeofday(&tm_start,NULL);
	UniHan_insertSort(tmpset->unihans, tmpset->size);
	gettimeofday(&tm_end,NULL);
        printf("OK! Finish insert_sorting, total size=%d, cost time=%ldms.\n", uniset->size, tm_diffus(tm_start, tm_end)/1000);

	printf("Start quickSort() UNISET typings...\n");
	gettimeofday(&tm_start,NULL);
	UniHan_quickSort(uniset->unihans, 0, uniset->size-1, 10);
	gettimeofday(&tm_end,NULL);
        printf("OK! Finish quick_sorting, total size=%d, cost time=%ldms.\n", uniset->size, tm_diffus(tm_start, tm_end)/1000);

	/* Check result */
	printf("Check results of quickSort()\n");
	for(i=0; i< uniset->size; i++) {
		#if 0
		bzero(pch,sizeof(pch));
       		printf("[%s:%d]%s",char_unicode_to_uft8(&(uniset->unihans[i].wcode), pch)>0?pch:" ",
								uniset->unihans[i].wcode, uniset->unihans[i].typing);
		#endif
		if( i>0 && UniHan_compare_typing(uniset->unihans+i, uniset->unihans+i-1) == CMPTYPING_IS_AHEAD )
				printf("\n------------- i=%d, uniset[] ERROR ---------------\n",i);

		if( i>0 && UniHan_compare_typing(tmpset->unihans+i, tmpset->unihans+i-1) == CMPTYPING_IS_AHEAD )
				printf("\n------------- i=%d, tmpset[] ERROR ---------------\n",i);

	}
        printf("OK. \n");

	/* Display some */
	printf("============ uniset ===============\n");
	for( i=12000; i<12100; i++ ) {
			bzero(pch,sizeof(pch));
        		printf("unihans[%d]: [%s:%d]%s\n", i, char_unicode_to_uft8(&(uniset->unihans[i].wcode), pch)>0?pch:" ",
								uniset->unihans[i].wcode, uniset->unihans[i].typing);
	}
	printf("============ tmpset ===============\n");
	for( i=12000; i<12100; i++ ) {
			bzero(pch,sizeof(pch));
        		printf("unihans[%d]: [%s:%d]%s\n", i, char_unicode_to_uft8(&(tmpset->unihans[i].wcode), pch)>0?pch:" ",
								tmpset->unihans[i].wcode, tmpset->unihans[i].typing);
	}

	/* Compare insertSort() AND quickSort() */
	printf("Compare results of insertSort() and quickSort()\n");
	for(i=0; i< uniset->size; i++) {
		if( strcmp(tmpset->unihans[i].reading, uniset->unihans[i].reading)!=0 )
		{
			printf("Diff unihans[%d]: uniset ", i);
			bzero(pch,sizeof(pch));
        		printf("[%s:%d]%s ",char_unicode_to_uft8(&(uniset->unihans[i].wcode), pch)>0?pch:" ",
								uniset->unihans[i].wcode, uniset->unihans[i].typing);
			printf(": tmpset ");
			bzero(pch,sizeof(pch));
        		printf("[%s:%d]%s ",char_unicode_to_uft8(&(tmpset->unihans[i].wcode), pch)>0?pch:" ",
								tmpset->unihans[i].wcode, tmpset->unihans[i].typing);
			printf("\n");

		}
	}
	printf("OK\n");

	return 0;
}

