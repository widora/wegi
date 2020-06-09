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

Reading:   The pronunciations for a given character in Mandarin for han unicodes.
	   Example:  chuǎng	yīn
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
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_cstring.h"

#define UNICODE_HAIZI_START	0x4E00
#define UNICODE_HAIZI_END	0x9FA5

#define MAX_UNIHANS		(64*1024)	/* Max. number of unihans[] */

/* A struct to store all haizi unicode
 * and its pinyin.
 * Notice: pinyin[] is the keyword! The same wcode may has several pinyin, each stored in a EGI_UNIHAN.
 */
typedef struct egi_unihan  EGI_UNIHAN;  /* Unicode Han */
struct egi_unihan
{
	wchar_t wcode;   	/* Unicode for a han char */
	char pinyin[8];  	/* KEYWORD:  Keyboard input pinyin,
				 * MAX. size example: "chuang3", here '3' indicates the tone.
				 */
	char reading[16]; 	/* Reading in kMandarin as in kHanyuPinyin.txt
				 * MAX. size example: "chuǎng" , ǎ-ḿ   is 3-4bytes
				 */
};
EGI_UNIHAN unihans[MAX_UNIHANS];  /* Totally 41377 chars */

int convert_reading_to_pinyin( const unsigned char *reading, char *pinyin);


/*===================
	MAIN
===================*/
int main(void)
{
	FILE *fil;
	char linebuff[1024];
	#define  WORD_MAX_LEN 256        /* Max. length of column words in each line of kHanyuPinyin.txt,  including '\0'. */

	/*--------------------------------------------------------
	   To store words in linebuff[] as splitted by delimter.
	   Exampe:      linefbuff:  "U+92FA	kHanyuPinyin	64225.040:yuǎn,yuān,wǎn,wān"
			str_words[0]:	U+92FA
			str_words[1]:	kHanyuPinyin
			str_words[2]:	64225.040
			str_words[3]:	yuǎn,yuān,wǎn,wān
	---------------------------------------------------------*/
	#define  MAX_SPLIT_WORDS 	4  	 /* Max number of words in linebuff[] as splitted by delimte */
	char str_words[MAX_SPLIT_WORDS][WORD_MAX_LEN]; /* See above */
	char strtmp[16]; 	/*  "chuǎng" , ǎ-ḿ   is 3-4bytes */
	char *delim="	:\n"; /* delimiter: for splitting linebuff[] into 4 words */

	int  m;
	char *pt;
	int k;
	wchar_t wcode;

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


	/* Open Han mandrain txt file */
	fil=fopen("/mmc/kHanyuPinyin.txt","r");
	if(fil==NULL) {
		printf("Fail to open han_mandarin txt.\n");
		exit(1);
	}

	/* Read in one line */
	k=0;
	while ( fgets(linebuff, sizeof(linebuff), fil) != NULL )
	{
		printf("	k=%d\n",k);
		printf("Input: %s",linebuff); /* fgets also '/n' */

	        /* Separate linebuff[] as per given delimiter
		 * linebuff: "U+92FA     kHanyuPinyin    64225.040:yuǎn,yuān,wǎn,wān"
		 */
	        pt=strtok(linebuff, delim);
		for(m=0; pt!=NULL && m<MAX_SPLIT_WORDS; m++) {  /* Separate linebuff into 4 words */
			bzero(str_words[m], sizeof(str_words[0]));
			snprintf( str_words[m], WORD_MAX_LEN, "%s", pt);
			pt=strtok(NULL, delim);
		}
		if(m<2) {
			printf("Not a complete line!\n");
			continue;
		}
		//printf(" str_words[0]: %s, [1]:%s, [2]:%s \n",str_words[0], str_words[1], str_words[2]);

/* --- 1. parse wcode --- */
		/* Convert str_wcode to wcode */
		sscanf(str_words[0]+2, "%x", &wcode); /* +2 bytes to escape 'U+' in "U+3404" */

		/* Assign unihans[k].wcode */
		if( wcode < UNICODE_HAIZI_START || wcode > UNICODE_HAIZI_END )
			continue;

/* --- 2. parse reading and convert to pinyin --- */

	        /* Split str_words[3] to several pinyins, Example: "yuǎn,yuān,wǎn,wān" */
		//printf("str_words[3]: %s\n",str_words[3]);
	        pt=strtok(str_words[3], ",");
		/* At lease one pinyin in most case:  " 74801.200:jì " */
		if(pt==NULL)
			pt=str_words[3];

		for(m=0; pt!=NULL; m++) {
/* ---- 3. Save unihans[], notice that pinyin/reading is the keyword! ---- */

			/* 1. wcode */
			unihans[k].wcode=wcode;
			/* 2. reading */
			strncpy(unihans[k].reading, pt, sizeof(unihans[0].reading) );
			/* 3. convert reading to pinyin, as for keyboard input */
			convert_reading_to_pinyin( (const unsigned char *)unihans[k].reading, unihans[k].pinyin);

			printf("unihans[%d]:  wcode:%d, reading:%s, pinying:%s\n",
					k, unihans[k].wcode, unihans[k].reading, unihans[k].pinyin);

			/* Check space capacity */
			k++;
			if(k > MAX_UNIHANS) {
				printf(" K out of range! \n");
				exit(1);
			}

			/* Get next split pointer */
			pt=strtok(NULL, ",");
		}

/* --- 3. convert reading to pinyin --- */



/* ---- Display --- */
                clear_screen(&gv_fb_dev, WEGI_COLOR_BLACK);

                /* Display the han character */
                FTsymbol_unicode_writeFB( &gv_fb_dev, egi_sysfonts.bold,         /* FBdev, fontface */
                                                  50, 50, wcode, NULL,                  /* fw,fh, wcode, *xleft */
                                                  30, 30,                               /* x0,y0, */
                                                  WEGI_COLOR_RED, -1, -20);             /* fontcolor, transcolor,opaque */
    		/* Display wcode */
                bzero(strtmp,sizeof(strtmp));
                sprintf(strtmp,"0x%04x", wcode);
                FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                                30, 30,(const unsigned char *)strtmp,   /* fw,fh, pstr */
                                                320, 1, 0,                      /* pixpl, lines, gap */
                                                120, 30,                                /* x0,y0, */
                                                WEGI_COLOR_WHITE, -1, -50,      /* fontcolor, transcolor,opaque */
                                                NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
		/* Display pinyin */


		usleep(50000);

	}


	/* close FILE */
	if(fclose(fil)!=0)
		printf("Fail to close han_mandrian txt.\n");

	return 0;
}

/*--------------------------------------------------------
Convert reading pronunciation to pinyin, which is suitable
for keyboard input.

@reading:  A pointer to pronunciation string, with uft-8 encoding.
	   example:  "tiě"
@pinyin:   A pointer to pinyin, in ASCII chars.
	   example:  "tie3"

NOTE: The caller MUST ensure enough space for pinyin.

Return:
	0	OK
	<0	Fails
---------------------------------------------------------*/
int convert_reading_to_pinyin( const unsigned char *reading, char *pinyin)
{
	wchar_t	wcode;
	int k;   /* index of pinyin[] */
	int size;
	char *p=(char *)reading;
	char tone='0';

	k=0;
	while(*p)
	{
		size=char_uft8_to_unicode((const unsigned char *)p, &wcode);

		/* 1. ----- A vowel char: Get pronunciation tone ----- */
		if(size>1) {

			switch( wcode )
			{
				case 0x00FC:	/* ü */
					tone='0';
					break;

				case 0x0101:  	/* ā */
				case 0x014D:  	/* ō */
				case 0x0113:  	/* ē */
				case 0x012B:	/* ī */
				case 0x016B:	/* ū */
				case 0x01D6:	/* ǖ */
					tone='1';
					break;

				case 0x00E1:  	/* á */
				case 0x00F3:  	/* ó */
				case 0x00E9:  	/* é */
				case 0x00ED:	/* í */
				case 0x00FA:	/* ú */
				case 0x01D8:	/* ǘ */
				case 0x0155:	/* ń */
				case 0x1E3F:	/* ḿ  */
					tone='2';
					break;

				case 0x01CE:  	/* ǎ */
				case 0x01D2:  	/* ǒ */
				case 0x011B:  	/* ě */
				case 0x01D0:	/* ǐ */
				case 0x01D4:	/* ǔ */
				case 0x01DA:	/* ǚ */
				case 0x0148:	/* ň */
					tone='3';
					break;

				case 0x00E0:  	/* à */
				case 0x00F2:  	/* ò */
				case 0x00E8:  	/* è */
				case 0x00EC:	/* ì */
				case 0x00F9:	/* ù */
				case 0x01DC:	/* ǜ */
				case 0x01F9:	/* ǹ */
					tone='4';
					break;

				case 0x00EA:  	/* ê */
					tone='5';
					break;

				default:
					tone='0';
					break;
			}
		}

		/* 2. -----A vowel char or ASCII char:  Replace vowel with ASCII char ----- */
		if(size >= 1) {

			switch( wcode )
			{
				case 0x0101:  	/* ā */
				case 0x00E1:  	/* á */
				case 0x01CE:  	/* ǎ */
				case 0x00E0:  	/* à */
					pinyin[k]='a';
					break;

				case 0x014D:  	/* ō */
				case 0x00F3:  	/* ó */
				case 0x01D2:  	/* ǒ */
				case 0x00F2:  	/* ò */
					pinyin[k]='o';
					break;

				case 0x0113:  	/* ē */
				case 0x00E9:  	/* é */
				case 0x011B:  	/* ě */
				case 0x00E8:  	/* è */
				case 0x00EA:  	/* ê */
					pinyin[k]='e';
					break;

				case 0x012B:	/* ī */
				case 0x00ED:	/* í */
				case 0x01D0:	/* ǐ */
				case 0x00EC:	/* ì */
					pinyin[k]='i';
					break;

				case 0x016B:	/* ū */
				case 0x00FA:	/* ú */
				case 0x01D4:	/* ǔ */
				case 0x00F9:	/* ù */
					pinyin[k]='u';
					break;

				case 0x00FC:	/* ü */
				case 0x01D6:	/* ǖ */
				case 0x01D8:	/* ǘ */
				case 0x01DA:	/* ǚ */
				case 0x01DC:	/* ǜ */
					pinyin[k]='v';
					break;

				case 0x0155:	/* ń */
				case 0x0148:	/* ň */
				case 0x01F9:	/* ǹ */
					pinyin[k]='n';
					break;

				case 0x1E3F:	/* ḿ  */
					pinyin[k]='m';
					break;

				/* Default: An ASCII char */
				default:
					//printf("wcode=%d\n", wcode);
					pinyin[k]=wcode;
					break;
			}


		}
		/* 3. ----- Unrecoginzable char ----- */
		else  /* size<1 */
		{
			printf("%s: Unregconizable unicode!\n",__func__);
			return -1;
		}

		/* 4. Move p */
		p+=size;

		/* 5. Increase k to parse next char */
		k++;
	}

	/* Put tone at the end */
	pinyin[k]=tone;

	return 0;
}
