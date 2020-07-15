/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

With reference to: http://a.dgqjj.com/rpinyin_3500ge.html
Load frequently-used Haizi(pinyin3500) to an EGI_UNIHAN_SET.

Note:
1. Vowel 'ü' in pinyin3500 marked as 'uu', need to be replaced with 'v'
   in the txt file. ( kMandarin.txt has those UNIHANs with right typings! )

	...
	NUU nuu 女恧衄
	NUUE nuue 疟虐
	LUU luu 驴吕侣铝旅偻屡缕履律虑滤率绿
	LUUE luue 掠略
	...

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_cstring.h"
#include "egi_unihan.h"
#include <errno.h>


int main(void)
{
	int i,k,m,n;
        EGI_UNIHAN_SET  *uniset=NULL;
        char pch[4];
	char strinput[UNIHAN_TYPING_MAXLEN];
	char pinyin[UNIHAN_TYPING_MAXLEN];

        size_t  size;
//        size_t  growsize=1024;          /* memgrow size for uniset->unihans[], also inital for size */
	wchar_t wcode;

        FILE *fil;
        char linebuff[1024];            /* Readin line buffer */
        #define  WORD_MAX_LEN           1024     /* Max. length of column words in each line of kHanyuPinyin.txt,  including '\0'. */
        #define  MAX_SPLIT_WORDS        3       /* Max number of words in linebuff[] as splitted by delimters */
        /*----------------------------------------------------------------------------------
           To store words in linebuff[] as splitted by delimter.
           Example:      ZA     za    拶杂砸咋扎匝咂
                        str_words[0]:   ZA
                        str_words[1]:   za
                        str_words[2]:   拶杂砸咋扎匝咂
        ------------------------------------------------------------------------------------*/
        char str_words[MAX_SPLIT_WORDS][WORD_MAX_LEN]; /* See above */
	char *pstr=NULL;
        char *delim="	 \n";          /* delimiters: TAB,SPACE,'\n'.  for splitting linebuff[] into 3 words. see above. */
        char *pt=NULL;


while(1) { /* ---------- LOOP TEST -------------- */

        /* Open 3500pinyin file */
        fil=fopen(PINYIN3500_TXT_PATH,"r");
        if(fil==NULL) {
                printf("%s: Fail to open '%s', ERR:%s.\n",__func__, PINYIN3500_TXT_PATH,  strerror(errno));
                exit(-1);
        }
        else
                printf("Succeed to open '%s'.\n",PINYIN3500_TXT_PATH);

        /* Allocate uniset : ensure enough capacity.
	 * 				--- NOTE ---
	 * 3500 pinyins need 4761 EGI_UNIHANs, as some of them are polyphonic chars with several different typings(PINYIN), and
	 * each typing occupys an EGI_UNIHAN.
	 */
        uniset=UniHan_create_set("PRC_PINYIN",  10*1024-1); //   (3500/1024+1)*1024 -1 );
        if(uniset==NULL) {
                printf("Fail to create uniset!\n");
                exit(-1);
        }
	printf("OK,uniset created!\n");

        /* Loop read in and parse to store to EGI_UNIHAN_SET */
        k=0;
        while ( fgets(linebuff, sizeof(linebuff), fil) != NULL )
        {
		printf("linebuff: %s\n", linebuff);

		/* Read each line */
		pt=strtok(linebuff, delim);
        	for(m=0; pt!=NULL && m<3; m++) {  /* Separate linebuff into 3 words */
        		bzero(str_words[m], sizeof(str_words[0]));
	                snprintf( str_words[m], WORD_MAX_LEN-1, "%s", pt);
        	        pt=strtok(NULL, delim);
	        }
        	if(m<2) {
                	printf("Not a complete line!\n");
	                continue;
        	}

		/* Extract UNIHAN one by one */
		pstr=str_words[2];
		while( (size=char_uft8_to_unicode((const unsigned char *)pstr, &wcode)) >0 )
		{
			/* --- 1. Assign wcode --- */
       	        	if( wcode < UNICODE_PRC_START || wcode > UNICODE_PRC_END )
                	        continue;
			else
				uniset->unihans[k].wcode=wcode;

			/* --- 2. Assign uchar --- */
			strncpy(uniset->unihans[k].uchar, pstr, size);

	                /* --- 3. Assign PINYING typing --- */
			strncpy( uniset->unihans[k].typing, str_words[1], sizeof(str_words[0])-1);

			/* --- 4. Assign freq ---- */
			uniset->unihans[k].freq=0;  	/* Keep 0, same as UniHan_load_HanyuPinyinTxt() AND UniHan_load_MandarinTxt() */

			/* increase count */
			k++;
			pstr += size;

			if(k>=uniset->capacity)
				printf("ERROR no more space in uniset!\n" );

		} /* End while( uft8_to_unicode ) */

	} /* End while( get line ) */

	/* Assign size */
	uniset->size=k;

	/* ----- 1. Save uniset to a file -----*/
	UniHan_save_set(uniset, PINYIN3500_DATA_PATH);
	printf("Finish saving uniset to '%s'.\n",PINYIN3500_DATA_PATH);
	getchar();

	/* Free uniset */
	UniHan_free_set(&uniset);

	/* ---- 2. Load saved uniset ------- */
	uniset=UniHan_load_set(PINYIN3500_DATA_PATH);
	if(uniset==NULL)
		exit(2);
	else
		printf("Load %s to uniset, name='%s', size=%d\n",PINYIN3500_DATA_PATH, uniset->name, uniset->size);
	getchar();


#if 0	/*  Insert_Sort by wcode */
	printf("Start insert_sorting by wcode ..."); fflush(stdout);
	UniHan_insertSort_wcode( uniset->unihans, uniset->size );
	printf("...OK\n");
#else	/*  Quick_Sort by wcode */
	printf("Start quick_sorting by wcode..."); fflush(stdout);
	//UniHan_quickSort_wcode( uniset->unihans, 0, uniset->size-1, 5);
	UniHan_quickSort_set(uniset, UNIORDER_WCODE_TYPING_FREQ, 5);
	printf("...OK\n");
#endif

	//printf("Enter a key to continue.\n");
	//getchar();

#if 1	/* Print all unihans */
	for( i=0; i< uniset->size; i++) {
	    /* Check wcode sorting */
	    if( i>0 && uniset->unihans[i].wcode < uniset->unihans[i-1].wcode ) {
			printf("ERROR: uniset->unihans[%d].wcode > uniset->unihans[%d].wcode\n",i, i-1);
			exit(1);
	    }

	    /* Print polyphonic unihans only */
	    if( (i>0 && uniset->unihans[i].wcode==uniset->unihans[i-1].wcode )
		|| uniset->unihans[i].wcode==uniset->unihans[i+1].wcode )
	    {
		printf("[%s:U+%d] %s   	", uniset->unihans[i].uchar, uniset->unihans[i].wcode, uniset->unihans[i].typing);

	        if( uniset->unihans[i].wcode == uniset->unihans[i-1].wcode &&  uniset->unihans[i].wcode != uniset->unihans[i+1].wcode )
			printf("\n");
	    }
	}
	printf("\n");
#endif
	printf("Finally uniset->size=%d\n",uniset->size);
	getchar();


for(m=0; m<4; m++) {
	/*  Quick_Sort with KEY==wcode before poll freq  */
	printf("Start quick_sorting by wcode..."); fflush(stdout);
	//UniHan_quickSort_wcode( uniset->unihans, 0, uniset->size-1, 5);
	UniHan_quickSort_set(uniset, UNIORDER_WCODE_TYPING_FREQ, 5);
	printf("...OK\n");

	/* Reset frequence */
	UniHan_reset_freq( uniset);

	/* Poll frequence by text, BEFORE quickSort_typing() */
   if(m==0) {
	printf("开始统计<红楼梦>字符频率 ..."); fflush(stdout);
	UniHan_poll_freq(uniset, "/mmc/hlm_all.txt");
	printf("...OK\n");
   }
   else if(m==1) {
	printf("开始统计<西游记>字符频率 ..."); fflush(stdout);
	UniHan_poll_freq(uniset, "/mmc/xyj_all.txt");
	printf("...OK\n");
   }
   else if(m==2) {
	printf("开始统计<三国演义>字符频率 ..."); fflush(stdout);
	UniHan_poll_freq(uniset, "/mmc/sgyy_all.txt");
	printf("...OK\n");
   }
   else if(m==3) {
	printf("开始统计<水浒传>字符频率 ..."); fflush(stdout);
	UniHan_poll_freq(uniset, "/mmc/shz_all.txt");
	printf("...OK\n");
   }

	/* Sort frequency */
	printf("Start quick_sorting by freq..."); fflush(stdout);
	#if 0
	UniHan_insertSort_freq( uniset->unihans, uniset->size-1 );
	#else
	//UniHan_quickSort_freq( uniset->unihans, 0, uniset->size-1, 10);
	UniHan_quickSort_set(uniset, UNIORDER_FREQ, 5);
	#endif
	printf("...OK\n");

	/* Print 20 most frequently used UNIHANs in the text */
	i=0; k=0;
	while( k<30 )
	{
		/* Skip polyphonic unihans */
		if(  uniset->size < 30
		     || uniset->unihans[uniset->size-i-1].wcode != uniset->unihans[uniset->size-i].wcode )
		{
	                printf("%d: %s(%d)\n", k+1, uniset->unihans[uniset->size-i-1].uchar,uniset->unihans[uniset->size-i-1].freq );
			k++;
		}
		i++;
	}
	getchar();
}


	/* Quick_Sort by typing */
	printf("Start quick_sorting by typing ...\n");
	//UniHan_quickSort_typing(uniset->unihans, 0, uniset->size-1, 5);
	UniHan_quickSort_set(uniset, UNIORDER_TYPING_FREQ, 5);

	/* Print all unihans, grouped by PINYIN */
	bzero(pinyin,sizeof(pinyin));
        for( i=0; i< uniset->size; i++) {
		if( strcmp(pinyin, uniset->unihans[i].typing) !=0 )
		{
			strncpy(pinyin, uniset->unihans[i].typing, sizeof(pinyin)-1);
			printf("\n[%s]: ",pinyin);
		}
                printf("%s(%d)",uniset->unihans[i].uchar, uniset->unihans[i].freq );
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
	i=1;
	while( strncmp(uniset->unihans[uniset->puh].typing, strinput, UNIHAN_TYPING_MAXLEN )==0 ) {
                printf("%d.%s ", i, uniset->unihans[uniset->puh].uchar);
		uniset->puh++;
		i++;
		if(uniset->puh > uniset->size-1)
			break;
	}
	printf("\n");
  }
#endif

	/* Free */
	UniHan_free_set(&uniset);

        /* Close fil */
        if( fclose(fil) !=0 ) {
                printf("Fail to close file. ERR:%s\n", strerror(errno));
        }

	usleep(600000);

 } /* ---------- END LOOP TEST -------------- */

	return 0;
}


