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
        char strinput[UNIHAN_TYPING_MAXLEN*4];

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

#if 0 /* ---------- Test: UniHan_divide_pinyin() ----------- */
	char group_pinyin[UNIHAN_TYPING_MAXLEN*4];
	while(1) {
		printf("Input unbroken pinyins:"); fflush(stdout);
		fgets(strinput, UNIHAN_TYPING_MAXLEN*4, stdin);
	        if( strlen(strinput)>0 )
        	        strinput[strlen(strinput)-1]='\0';
        	else
                	continue;

		/* Parse pinyin */
		//bzero(group_pinyin,UNIHAN_TYPING_MAXLEN*4);
		UniHan_divide_pinyin(strinput, group_pinyin, 4);
		printf("Parsed pinyin: ");
		for(i=0; i<4; i++)
			printf("%s ",group_pinyin+UNIHAN_TYPING_MAXLEN*i);
		printf("\n");
	}
#endif

#if 0 /*----------- Load uniset and test pinyin ---------- */
	uniset=UniHan_load_set(UNIHANS_DATA_PATH);
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
	/* Test increase freq for the second unihan */
	if(k>=uniset->puh+1) {
		UniHan_increase_freq(uniset, strinput, uniset->unihans[uniset->puh+1].wcode, 10);
		//UniHan_quickSort_uniset(uniset, UNIORDER_TYPING_FREQ, 10);
	}
  }
  exit(1);
#endif

#if 0 /*-------------  test UniHanGroup Functions  ------------------*/
		EGI_UNIHAN_SET *han_set=NULL;
		EGI_UNIHANGROUP_SET *group_set=NULL;

 while(1) {  /////////////////////       LOOP TEST 	   //////////////////////////
		han_set=UniHan_load_set(UNIHANS_DATA_PATH);
		if(han_set==NULL) exit(2);
	        printf("Uniset total size=%d:\n", han_set->size);
		getchar();

		group_set=UniHanGroup_load_CizuTxt(CIZU_TXT_PATH);
		if(group_set==NULL) exit(1);
		printf("Finish load CizuTxt!\n");
		UniHanGroup_print_set(group_set, group_set->ugroups_size-50, group_set->ugroups_size-1);
//		getchar();

	#if 1	/* TEST: --- sort wcodes ---- */
		int sorder;

		printf("Start to sort wcodes...\n");
		gettimeofday(&tm_start,NULL);
		//UniHanGroup_insertSort_wcodes(group_set, 0, group_set->ugroups_size);
		UniHanGroup_quickSort_wcodes(group_set, 0, group_set->ugroups_size-1, 9);
		gettimeofday(&tm_end,NULL);
		printf("Finish quickSort wcodes. cost time: %ldms\n", tm_diffus(tm_start, tm_end)/1000);
		//UniHanGroup_print_set(group_set, 0, group_set->ugroups_size-1);
		printf("Start to check redundant wcodes...\n");
		int rwcount=0;
		for(i=1; i< group_set->ugroups_size; i++) {
			for(j=0; j<UHGROUP_WCODES_MAXSIZE; j++) {
				if( group_set->ugroups[i-1].wcodes[j] != group_set->ugroups[i].wcodes[j] )
					break;
			}
			/* Redundant wcodes found ... */
			if( j==UHGROUP_WCODES_MAXSIZE ) {
				UniHanGroup_print_group(group_set,i,true);
				rwcount++;
			}
		}
		printf("Finish checking redundant wcodes, %d redundant groups found!\n", rwcount);
		getchar();

	#endif

		#if 0 /* NOT HERE! .... Assemble typings just AFTER merge/purify operation!  */
		printf("Start to assemble typings...\n");
		if( UniHanGroup_assemble_typings(group_set, han_set) != 0) {
			printf("Fail to assmeble typings!\n");
			exit(1);
		}
		printf("Finish assmebling typings.\n");
		getchar();
		#endif

		printf("Start to load/merge han_set to group_set...\n");
		if( UniHanGroup_add_uniset(group_set, han_set) !=0 ) {
			printf("Fail to load han_set!\n");
			exit(1);
		}
		printf("Finish add han_set into group_set.\n");
		getchar();

	#if 1 /* ---TEST:  UniHanGroup merge and purify functions */
		EGI_UNIHANGROUP_SET*  expend_set=UniHanGroup_load_CizuTxt(PINYIN_NEW_WORDS_FPATH);
		if(expend_set==NULL) exit(1);
		UniHanGroup_print_set(expend_set,0,expend_set->ugroups_size-1);
		printf("Start to merge addon_set ...\n");
		UniHanGroup_merge_set(expend_set, group_set);
		UniHanGroup_free_set(&expend_set);
		printf("Finish merging addon_set for new words!\n");
		getchar();

	  #if 1 /* Test --- UniHanGroup_loate_wcodes[] */
		char strUFT8[4*4+1];
		EGI_UNICODE wcodes[UHGROUP_WCODES_MAXSIZE];
		UniHanGroup_quickSort_set(group_set, UNIORDER_NCH_WCODES, 10);
		while(1) {
			bzero(strUFT8,sizeof(strUFT8));
			bzero(wcodes,sizeof(EGI_UNICODE)*UHGROUP_WCODES_MAXSIZE);
			printf("Input uchars:");
			scanf("%s",strUFT8);
			printf("Start to locate wcodes[] ...\n");
		  #if 0 /* Use uchars */
			gettimeofday(&tm_start,NULL);
			if( UniHanGroup_locate_wcodes(group_set, (UFT8_PCHAR)strUFT8, NULL )==0 ) {
		  #else /* Use wcodes */
			cstr_uft8_to_unicode((UFT8_PCHAR)strUFT8, wcodes);
			gettimeofday(&tm_start,NULL);
			if( UniHanGroup_locate_wcodes(group_set, NULL, wcodes)==0 ) {
		  #endif
				gettimeofday(&tm_end,NULL);
				UniHanGroup_print_group(group_set, group_set->pgrp, true);
			} else {
				gettimeofday(&tm_end,NULL);
				printf("Fail to locate wcodes[]:%s\n", strUFT8);
			}
			printf("Finish locate wcodes. cost time: %ldms\n", tm_diffus(tm_start, tm_end)/1000);
			getchar();
		}
	  #endif

		printf("Start to purify group set and clear redundant ones ...\n");
		UniHanGroup_purify_set(group_set);
		printf("Finish purifying group_set!\n");
		getchar();

		printf("Start to assemble typings AFTER merge/purify ...\n");
		if( UniHanGroup_assemble_typings(group_set, han_set) != 0) {
			printf("Fail to assmeble typings!\n");
			exit(1);
		}
		printf("Finish assmebling typings.\n");
		getchar();

	#endif
		printf("Start to quick sort typings...\n");
		gettimeofday(&tm_start,NULL);
		//UniHanGroup_insertSort_typings(group_set, 0, group_set->ugroups_size-1); // 500);
		UniHanGroup_quickSort_typings(group_set, 0, group_set->ugroups_size-1, 9);
		gettimeofday(&tm_end,NULL);
		printf("Finish quickSort typings. cost time: %ldms\n", tm_diffus(tm_start, tm_end)/1000);
		getchar();

		#if 0 /* Print first 50 and last 50 groups */
		UniHanGroup_print_set(group_set, 0, 50-1);
		UniHanGroup_print_set(group_set, group_set->ugroups_size-1-50, group_set->ugroups_size-1);
		getchar();
		#endif

		/* Test: Save to data file */
	 //while(1) {  ///// --------- LOOP TEST UniHanGroup_save/load_set() -------- /////

		printf("Start to save group_set to data file..."); fflush(stdout);
		if( UniHanGroup_save_set(group_set, UNIHANGROUPS_DATA_PATH)==0 )
			printf("Save group_set to '%s' successfully!\n", UNIHANGROUPS_DATA_PATH);
		getchar();
		/* ------ UNIHANGROUPS_DATA saved! ------ */

		EGI_UNIHANGROUP_SET* tmpGroupSet=UniHanGroup_load_set(UNIHANGROUPS_DATA_PATH);
		if(tmpGroupSet!=NULL) {
			printf("Load set tmpGroupSet, setname:%s, size=%d\n",tmpGroupSet->name, tmpGroupSet->ugroups_size);
			//getchar();
			UniHanGroup_print_set(group_set, tmpGroupSet->ugroups_size-1-1000, tmpGroupSet->ugroups_size-1);
			UniHanGroup_free_set(&tmpGroupSet);
			//getchar();
		}
	 //}

		#if 1 /* Save to txt */
		printf("Start to save group_set to CizuTxt..."); fflush(stdout);
		if( UniHanGroup_saveto_CizuTxt(group_set, "/tmp/cizu+pinyin.txt")==0 )
			printf("Save group_set to cizu+pinyin.txt successfully!\n");
		getchar();
		#endif

		#if 1  /* TEST search */
		UniHanGroup_search_uchars(group_set, (UFT8_PCHAR)"老有所乐");
		getchar();
		//if( (k=UniHanGroup_locate_uchars(group_set,(UFT8_PCHAR)"紧俏")) >=0 )
		//UniHanGroup_print_set(group_set, k-10, k+10);
		//getchar();
		#endif

	#if 1	/* TEST ----Locate group_typing */
	char group_pinyin[UNIHAN_TYPING_MAXLEN*4];
	int np;
	while(1) {
		/* Input unbroken pinyins */
		printf("Input unbroken pinyins:"); fflush(stdout);
		fgets(strinput, UNIHAN_TYPING_MAXLEN*4, stdin);
	        if( strlen(strinput)>0 )
        	        strinput[strlen(strinput)-1]='\0';
        	else
                	continue;

		/* Divide pinyin */
		np=UniHan_divide_pinyin(strinput, group_pinyin, UHGROUP_WCODES_MAXSIZE); /* bzero group_pinyin inside function */
		for(i=0; i<UHGROUP_WCODES_MAXSIZE; i++)
			printf("%s ",group_pinyin+UNIHAN_TYPING_MAXLEN*i);
		printf(":\n");

		/* Locate typing in the group set */
		gettimeofday(&tm_start,NULL);
		if( UniHanGroup_locate_typings(group_set, group_pinyin)==0 ) {
			gettimeofday(&tm_end,NULL);

			/* print all results */
			UniHanGroup_print_results(group_set);

			printf("Finish locating typings. cost time: %ldms\n", tm_diffus(tm_start, tm_end)/1000);

		} else
			printf(": No result!\n");
	}
	#endif

		UniHan_free_set(&han_set);
		UniHanGroup_free_set(&group_set);


 }  ////////////////////////	END  LOOP TEST	  //////////////////////////

exit(1);
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
        uniset=UniHan_load_set(PINYIN3500_DATA_PATH);
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
	UniHan_quickSort_set(unisetMandarin, UNIORDER_WCODE_TYPING_FREQ, 10);
	UniHan_print_wcode(unisetMandarin, 0x81C2);
	getchar();

	printf("Start to merge unisetMandarin into uniset...\n");
	/* Merge unisetMandarin into uniset */
	UniHan_merge_set(unisetMandarin, uniset);
	printf("Finish merging unisetMandarin into uniset!\n");
	printf("Start to quickSort wcode for uniset....\n");
        /* quickSort_wcode uniset2, before calling UniHan_locate_wcode(). */
	UniHan_quickSort_set(uniset, UNIORDER_WCODE_TYPING_FREQ, 10);
        //UniHan_quickSort_wcode(uniset->unihans, 0, uniset->size-1, 10);
	printf("Finish quickSort wcode for uniset....\n");
	UniHan_print_wcode(uniset, 0x81C2);
	getchar();

	/* Purify merged uniset, clear redundant unihans (with same wcode and tying) */
	printf("Start to purify uniset...\n");
	UniHan_purify_set(uniset);
	printf("Finish purifying uniset!\n");
	UniHan_print_wcode(uniset, 0x81C2); //0x9E23);
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
	UniHan_quickSort_set(uniset, UNIORDER_WCODE_TYPING_FREQ, 10);
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
	UniHan_quickSort_set(uniset, UNIORDER_FREQ, 10);
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
	UniHan_quickSort_set(uniset, UNIORDER_WCODE_TYPING_FREQ, 10);
	UniHan_print_wcode(uniset, 0x5BB6);
	UniHan_print_wcode(uniset, 0x96BE);
	getchar();

#if 0	/* Checking redundancy: with SAME wcode and typing */
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
#endif

	/* Quick sort uniset with KEY=typing */
	printf("Start quickSort() UNISET typings..."); fflush(stdout);
	gettimeofday(&tm_start,NULL);
	//UniHan_quickSort_typing(uniset->unihans, 0, uniset->capacity-1, 10);
	UniHan_quickSort_set(uniset, UNIORDER_TYPING_FREQ, 10);
	gettimeofday(&tm_end,NULL);
        printf("OK! Finish quick_sorting, total size=%d, cost time=%ldms.\n", uniset->size, tm_diffus(tm_start, tm_end)/1000);
	getchar();

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
	if( UniHan_save_set(uniset, UNIHANS_DATA_PATH) == 0 )
		printf("--- Uniset is saved to '%s'.\n",UNIHANS_DATA_PATH);
	getchar();

        /* Print all unihans, grouped by PINYIN */
        bzero(pinyin,sizeof(pinyin));
        for( i=0; i< uniset->capacity; i++) {
                if( strcmp(pinyin, uniset->unihans[i].typing) !=0 )
                {
                        strncpy(pinyin, uniset->unihans[i].typing, UNIHAN_TYPING_MAXLEN);
                        printf("\n[%s]: ",pinyin);
                }
                printf("%s(U+%X)",uniset->unihans[i].uchar, uniset->unihans[i].freq );
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
	/* Test increase freq for the second unihan */
	if(k>=uniset->puh+1) {
		UniHan_increase_freq(uniset, strinput, uniset->unihans[uniset->puh+1].wcode, 10);
		UniHan_quickSort_set(uniset, UNIORDER_TYPING_FREQ, 10);
	}
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


