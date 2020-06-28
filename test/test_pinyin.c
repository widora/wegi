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
1. Prepare kHanyuPinyin.txt
	cat Unihan_Readings.txt | grep kHanyuPinyin > kHanyuPinyin.txt

   File 'kHanyuPinyin.txt' contains unicode, dict page number and mandarin reading(original pinyin):
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
#include <termios.h>

/*===================
	MAIN
===================*/
int main(void)
{
	EGI_UNIHAN_SET	*uniset=NULL;
	EGI_UNIHAN_SET  *uniset3500=NULL;
	EGI_UNIHAN_SET  *unisetMandarin=NULL;
	EGI_UNIHAN_SET  *tmpset=NULL;
	char pch[4];
	int i,j,k;
        struct termios old_settings;
        struct termios new_settings;

	struct timeval	tm_start,tm_end;
	wchar_t  	swcode;
        char pinyin[UNIHAN_TYPING_MAXLEN];
        char strinput[UNIHAN_TYPING_MAXLEN];

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

        /* Set termio */
        tcgetattr(0, &old_settings);
        new_settings=old_settings;
        new_settings.c_lflag |= (ICANON);      /* enable canonical mode */
        new_settings.c_lflag |= (ECHO);        /* enable echo */
        tcsetattr(0, TCSANOW, &new_settings);


#if 0	/* ------------- Strange wcode -------------- */
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

#if 0 /*----------- Load uniset and test pinyin ---------- */
	uniset=UniHan_load_uniset(UNIHANS_DATA_PATH);
	if( uniset==NULL )
		exit(1);
        printf("Uniset total size=%d:\n", uniset->size);

  while(1) {
        /* Input pinyin */
        printf("Input pinyin:"); fflush(stdout);
        fgets(strinput, UNIHAN_TYPING_MAXLEN, stdin);
        if( strlen(strinput)>0 )
                strinput[strlen(strinput)-1]='\0';
        else
                continue;

        /* Locate typing */
	uniset->sorder=UNIORDER_TYPING_FREQ;
        UniHan_locate_typing(uniset, strinput);
	i=0; k=uniset->puh;
	while( strncmp( uniset->unihans[k].typing, strinput, UNIHAN_TYPING_MAXLEN ) == 0 ) {
		i++;
                bzero(pch,sizeof(pch));
                printf("%d.%s(%d) ", i, char_unicode_to_uft8(&(uniset->unihans[k].wcode), pch)>0 ? pch : " ", uniset->unihans[k].freq);
		k++;
        }
        printf("\n");
  }

#endif



#if 0 /*-------------  1. test INSERT_SORT tmpset  ------------------*/
	/* Read txt  into EGI_UNIHAN_SET */
	tmpset=UniHan_load_HanyuPinyinTxt(HANYUPINYIN_TXT_PATH);
	if( tmpset==NULL )
		exit(1);
	getchar();

        for(i=0; i< tmpset->size; i++)
        	printf("%d_[%s:%d]%s ", i, tmpset->unihans[i].uchar,tmpset->unihans[i].wcode, tmpset->unihans[i].typing);
        printf("\n");
        printf("tmpset total size=%d, capacity=%d:\n", tmpset->size, tmpset->capacity);
	getchar();

	#if 0	/* Insert Sort Typing */
	printf("Start insertSort typings for tmpset...\n");
	gettimeofday(&tm_start,NULL);
	UniHan_insertSort_typing(tmpset->unihans, tmpset->size);
	gettimeofday(&tm_end, NULL);
        printf("OK! Finish insert_sorting, total size=%d, cost time=%ldms.\n", tmpset->size, tm_diffus(tm_start, tm_end)/1000);
	#else	/* Quick Sort Typing */
	printf("Start quickSort typings for tmpset...\n");
	gettimeofday(&tm_start,NULL);
	UniHan_quickSort_uniset(tmpset, UNIORDER_TYPING_FREQ, 9);
	gettimeofday(&tm_end,NULL);
        printf("OK! Finish quick_sorting, total size=%d, capacity=%d, cost time=%ldms.\n",
						tmpset->size, tmpset->capacity, tm_diffus(tm_start, tm_end)/1000);
	#endif
	getchar();

	/* Check UNIORDER_TYPING_FREQ */
	printf("Check results of  UNIORDER_TYPING_FREQ.\n");
	for(i=0; i< tmpset->size; i++) {
		#if 1
       		printf("[%s:%d]%s",tmpset->unihans[i].uchar,tmpset->unihans[i].wcode, tmpset->unihans[i].typing);
		#endif
		if( i>0 && UniHan_compare_typing(tmpset->unihans+i, tmpset->unihans+i-1) == CMPORDER_IS_AHEAD )
				printf("\n------------- i=%d, tmpset[] ERROR ---------------\n",i);

	}
        printf("\n Finish checking! \n");
	exit(1);
#endif  /*-------- END Insert_sort tmpset -------- */






#if 1 /*-------------- 2. test MERGE, PURIFY, QUICK_SORT，LOAD, SAVE uniset ------------------*/


#if 0	/* OPTION 1: Read kHanyuPinyin txt into uniset */
	uniset=UniHan_load_HanyuPinyinTxt(HANYUPINYIN_TXT_PATH);
	if( uniset==NULL )
		exit(1);
        printf("Load kHanyuPinyin txt to uniset, total size=%d:\n", uniset->size);
	getchar();

	/* Load saved uniset3500 */
        uniset3500=UniHan_load_uniset(PINYIN3500_DATA_PATH);
        if(uniset3500==NULL)
                exit(2);
        else
                printf("Load %s to uniset, name='%s', size=%d\n",PINYIN3500_DATA_PATH, uniset->name, uniset->size);

	/* merge uniset3500 into uniset */
	UniHan_merge_uniset(uniset3500, uniset);
	getchar();

#else   /* OPTION 2: Load saved uniset3500 to uniset */
        uniset=UniHan_load_uniset(PINYIN3500_DATA_PATH);
        if(uniset==NULL)
                exit(2);
        else
                printf("Load %s to uniset, name='%s', size=%d\n",PINYIN3500_DATA_PATH, uniset->name, uniset->size);

	getchar();
#endif

	/* Read kMandarin txt into uniset */
	unisetMandarin=UniHan_load_MandarinTxt(MANDARIN_TXT_PATH);
	if( unisetMandarin==NULL )
		exit(1);
        printf("Load kMandarin txt to unisetMandarin, total size=%d:\n", unisetMandarin->size);
	UniHan_quickSort_uniset(unisetMandarin, UNIORDER_WCODE_TYPING_FREQ, 10);
	UniHan_print_wcode(unisetMandarin, 0x9E23);
	getchar();

	/* Merge unisetMandarin into uniset */
	UniHan_merge_uniset(unisetMandarin, uniset);
        /* quickSort_wcode uniset2, before calling UniHan_locate_wcode(). */
	UniHan_quickSort_uniset(uniset, UNIORDER_WCODE_TYPING_FREQ, 10);
        //UniHan_quickSort_wcode(uniset->unihans, 0, uniset->size-1, 10);
	UniHan_print_wcode(uniset, 0x9E23);
	getchar();

	/* Purify merged uniset, clear redundant unihans */
	UniHan_purify_uniset(uniset);
	UniHan_print_wcode(uniset, 0x8272); //0x9E23);
	getchar();

	/* TEST: Locate a wcode */
	#if 1
	swcode=0x9E23; //0x4E00; //0x9FA4;
	if( UniHan_locate_wcode(uniset,swcode) !=0 ) //9FA5); //x54B9);
		printf("wcode U+%04x NOT found!\n", swcode);
	while( uniset->unihans[uniset->puh].wcode==swcode ) {
        	printf("[%s:%d]%s\n",uniset->unihans[uniset->puh].uchar,
							uniset->unihans[uniset->puh].wcode, uniset->unihans[uniset->puh].typing);

		uniset->puh++;
	}
	getchar();
	#endif

        /*  Quick_Sort with KEY==wcode before poll freq  */
        printf("Start quick_sorting by wcode..."); fflush(stdout);
        //UniHan_quickSort_wcode( uniset->unihans, 0, uniset->size-1, 10);
	UniHan_quickSort_uniset(uniset, UNIORDER_WCODE_TYPING_FREQ, 10);
        printf("...OK\n");

        /* Poll frequence by text, BEFORE quickSort_typing() */
        UniHan_reset_freq( uniset);

	#if 1
        printf("开始统计<红楼梦>字符频率 ..."); fflush(stdout);
        UniHan_poll_freq(uniset, "/mmc/hlm_all.txt");
	#endif
	#if 1
        printf("开始统计<西游记>字符频率 ..."); fflush(stdout);
        UniHan_poll_freq(uniset, "/mmc/xyj_all.txt");
	#endif
	#if 1
        printf("开始统计<三国演义>字符频率 ..."); fflush(stdout);
        UniHan_poll_freq(uniset, "/mmc/sgyy_all.txt");
	#endif
	#if 1
        printf("开始统计<水浒传>字符频率 ..."); fflush(stdout);
        UniHan_poll_freq(uniset, "/mmc/shz_all.txt");
	#endif
        printf("...OK\n");

        /* Quick Sort uniset with KEY==frequency */
        printf("Start quick_sorting by freq..."); fflush(stdout);
        //UniHan_quickSort_freq( uniset->unihans, 0, uniset->size-1, 10);
	UniHan_quickSort_uniset(uniset, UNIORDER_FREQ, 10);
        printf("...OK\n");

	#if 1 /* Check repeated case */
	swcode=0x513f;
        for(i=0; i< uniset->size; i++) {
	    if(uniset->unihans[i].wcode==swcode) //0x4E00) //0x9FA4) //0x9FA5)
        	printf("i=%d, [%s:0x%04x]%s ",i,uniset->unihans[i].uchar,
								uniset->unihans[i].wcode, uniset->unihans[i].typing);
	}
        printf("\n");
	getchar();
	#endif

        /* Print 20 most frequently used UNIHANs in the text */
        i=0; k=0;
        while( k<30 )
        {
                /* Skip polyphonic unihans ()() still exeption: 儿[0x513f](6079) 玉[0x7389](6079) 儿[0x513f](6079)  */
                if(  uniset->size < 30
                     || uniset->unihans[uniset->size-i-1].wcode != uniset->unihans[uniset->size-i].wcode )
                {
                        printf("%d: %s[0x%04x](%d)\n", k+1, uniset->unihans[uniset->size-i-1].uchar,
                                                	uniset->unihans[uniset->size-i-1].wcode, uniset->unihans[uniset->size-i-1].freq );
                        k++;
                }
                i++;
        }
        getchar();

	/* Print out a wcode */
        //UniHan_quickSort_wcode( uniset->unihans, 0, uniset->size-1, 10); //  uniset->capacity-1, 10);
	UniHan_quickSort_uniset(uniset, UNIORDER_WCODE_TYPING_FREQ, 10);
	UniHan_print_wcode(uniset, 0x5BB6);
	UniHan_print_wcode(uniset, 0x96BE);
	getchar();

	/* Checking redundancy: with SAME wcode and typing */
	printf("Start checking redundancy...\n");
	for(i=0; i< uniset->capacity-1; i++) {
		printf("\r i=%d/%d		", i, uniset->capacity-1-1);fflush(stdout);
		for(j=i+1; j< uniset->capacity; j++) {
			if( uniset->unihans[i].wcode!=0
			   && uniset->unihans[i].wcode==uniset->unihans[j].wcode ) {
				if( strncmp( uniset->unihans[i].typing, uniset->unihans[j].typing, UNIHAN_TYPING_MAXLEN ) ==0 ) {
					/* print redundant unihans */
					printf("\n");
        				printf("unihans[%d]: [%s:U+%X] %s ",i, uniset->unihans[i].uchar,
									       uniset->unihans[i].wcode, uniset->unihans[i].typing);
        				printf("unihans[%d]: [%s:U+%X] %s ",j, uniset->unihans[j].uchar,
										uniset->unihans[j].wcode, uniset->unihans[j].typing);
					printf("\n");
				}
			}
		}
	}
        printf("\n Finish checking redundancy of wcode+typing. \n");
	getchar();

	/* Quick sort uniset with KEY=typing */
	printf("Start quickSort() UNISET typings..."); fflush(stdout);
	gettimeofday(&tm_start,NULL);
	//UniHan_quickSort_typing(uniset->unihans, 0, uniset->capacity-1, 10);
	UniHan_quickSort_uniset(uniset, UNIORDER_TYPING_FREQ, 10);
	gettimeofday(&tm_end,NULL);
        printf("OK! Finish quick_sorting, total size=%d, cost time=%ldms.\n", uniset->size, tm_diffus(tm_start, tm_end)/1000);

	/* Check typing order of two nearby unihans */
	printf("Check results of quickSort() uniset..."); fflush(stdout);
	for(i=0; i< uniset->capacity; i++) {
		#if 1
       		printf("[%s:U+%X]%s",uniset->unihans[i].uchar,uniset->unihans[i].wcode, uniset->unihans[i].typing);
		#endif
		if( i>0 && UniHan_compare_typing(uniset->unihans+i, uniset->unihans+i-1) == CMPORDER_IS_AHEAD )
				printf("\n------------- i=%d, uniset[] ERROR ---------------\n",i);

	}
        printf("\n Finish checking typing order of two nearby unihans, OK. \n");
	getchar();

	/* Save sorted uniset UNIORDER_TYPING_FREQ */
	if( UniHan_save_uniset(UNIHANS_DATA_PATH, uniset) == 0 )
		printf("Uniset is saved to '%s'.\n",UNIHANS_DATA_PATH);
	getchar();

        /* Print all unihans, grouped by PINYIN */
        bzero(pinyin,sizeof(pinyin));
        for( i=0; i< uniset->capacity; i++) {
                if( strcmp(pinyin, uniset->unihans[i].typing) !=0 )
                {
                        strncpy(pinyin, uniset->unihans[i].typing, sizeof(pinyin)-1);
                        printf("\n[%s]: ",pinyin);
                }
                bzero(pch,sizeof(pch));
                printf("%s(U+%X)",char_unicode_to_uft8(&(uniset->unihans[i].wcode), pch)>0 ? pch : " ", uniset->unihans[i].freq );
        }
        printf("\n\n\n");



#if 1  /* ---- TEST: pinyin Input.  To locate a typing, the uniset must alread have been sorted by typing. */
  while(1) {
        /* Input pinyin */
        printf("Input pinyin:"); fflush(stdout);
        fgets(strinput, UNIHAN_TYPING_MAXLEN, stdin);
        if( strlen(strinput)>0 )
                strinput[strlen(strinput)-1]='\0';
        else
                continue;

        /* Locate typing */
        UniHan_locate_typing(uniset, strinput);
	i=0; k=uniset->puh;
	while( strncmp( uniset->unihans[k].typing, strinput, UNIHAN_TYPING_MAXLEN ) == 0 ) {
		i++;
                bzero(pch,sizeof(pch));
                printf("%d.%s(%d) ", i, char_unicode_to_uft8(&(uniset->unihans[k].wcode), pch)>0 ? pch : " ", uniset->unihans[k].freq);
		k++;
        }
        printf("\n");
  }
#endif


#endif  /*-------- END quick_sort uniset -------- */




#if 0  /* ---------  3. Compare tmpset and uniset as results of insertSort() AND quickSort() ------- */
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
#endif

	return 0;
}

