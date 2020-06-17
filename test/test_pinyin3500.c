/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

With reference to: http://a.dgqjj.com/rpinyin_3500ge.html
Load frequently-used Haizi(pinyin3500) to an EGI_UNIHAN_SET.


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_cstring.h"
#include "egi_unihan.h"
#include <errno.h>

#define PINYIN3500_TXT_PATH    	   "/mmc/pinyin3500.txt"
#define PINYIN3500_DATA_PATH       "/mmc/unihan3500.dat"


int main(void)
{
	int i,k;
        EGI_UNIHAN_SET  *uniset=NULL;
        char pch[4];
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
        char *delim="	 \n";          /* delimiters: TAB,SPACE,'\n'.  for splitting linebuff[] into 4 words. see above. */
        char *pt=NULL;
	int m;


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
        uniset=UniHan_create_uniset("PRC_PINYIN",  10*1024-1); //   (3500/1024+1)*1024 -1 );
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

	                /* --- 2. Assign PINYING typing --- */
			strncpy( uniset->unihans[k].typing, str_words[1], sizeof(str_words[0])-1);

			/* --- 3. Assign freq ---- */
			uniset->unihans[k].freq=1;

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
	UniHan_save_uniset( PINYIN3500_DATA_PATH, uniset);

	/* Free uniset */
	UniHan_free_uniset(&uniset);

	/* ---- 2. Load saved uniset ------- */
	uniset=UniHan_load_uniset(PINYIN3500_DATA_PATH);
	if(uniset==NULL)
		exit(2);
	else
		printf("Load %s to uniset, size=%d\n",PINYIN3500_DATA_PATH, uniset->size);

#if 0	/*  Insert_Sort by wcode */
	printf("Start insert_sorting by wcode ..."); fflush(stdout);
	UniHan_insertSort_wcode( uniset->unihans, uniset->size );
	printf("...OK\n");
#else	/*  Quick_Sort by wcode */
	printf("Start quick_sorting by wcode..."); fflush(stdout);
	UniHan_quickSort_wcode( uniset->unihans, 0, uniset->size-1, 5);
	printf("...OK\n");
#endif

	//printf("Enter a key to continue.\n");
	//getchar();

	/* Print all unihans */
	for( i=0; i< uniset->size; i++) {
	    /* Print polyphonic unihans only */
	    if( (i>0 && uniset->unihans[i].wcode==uniset->unihans[i-1].wcode )
		|| uniset->unihans[i].wcode==uniset->unihans[i+1].wcode )
	    {
		bzero(pch,sizeof(pch));
                printf("[%s:%d]%s ",char_unicode_to_uft8(&(uniset->unihans[i].wcode), pch)>0?pch:" ",
	                                                            uniset->unihans[i].wcode, uniset->unihans[i].typing);
	    }
	}
	printf("\n");
	printf("Finally uniset->size=%d\n",uniset->size);

	/* Quick_Sort by typing */
	printf("Start insert_sorting by wcode ...\n");
	UniHan_quickSort_typing(uniset->unihans, 0, uniset->size-1, 5);

        /* Print all unihans, grouped by PINYIN */
	bzero(pinyin,sizeof(pinyin));
        for( i=0; i< uniset->size; i++) {
		if( strcmp(pinyin, uniset->unihans[i].typing) !=0 )
		{
			strncpy(pinyin, uniset->unihans[i].typing, sizeof(pinyin)-1);
			printf("\n[%s]: ",pinyin);
		}
                bzero(pch,sizeof(pch));
                printf("%s",char_unicode_to_uft8(&(uniset->unihans[i].wcode), pch)>0 ? pch : " " );
        }
        printf("\n");


	/* Free */
	UniHan_free_uniset(&uniset);

        /* Close fil */
        if( fclose(fil) !=0 ) {
                printf("Fail to close file. ERR:%s\n", strerror(errno));
        }

	usleep(600000);

 } /* ---------- END LOOP TEST -------------- */

	return 0;
}


