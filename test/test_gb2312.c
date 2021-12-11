/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Reference:  https://www.cnblogs.com/yanye0xff/p/13547150.html

Midas Zhou
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "egi_utils.h"
#include "egi_gb2unicode.h"
#include "egi_cstring.h"

#include "GB2312_table.h"

int main(void)
{
//     int list_size=sizeof(EGI_GB2UNICODE_LIST)/sizeof(EGI_GB2UNICODE_LIST[0]);
//     printf("EGI_GB2UNICODE_LIST has %d uchars!\n", list_size);
//     exit(0);

#if 0 //////////////////////////////////////
	int k;
	EGI_FILEMMAP *fmap=egi_fmap_create("/tmp/gb2unicode.dat",0,PROT_READ,MAP_PRIVATE);
	if(fmap==NULL) exit(1);

	EGI_FILEMMAP *fmap2=egi_fmap_create("/tmp/gb2unicode.dat", fmap->fsize,PROT_WRITE,MAP_PRIVATE);

/* To write as:
   {0x0001,0x000a},
   {0x0002,0x000b},
*/
//
//	EGI_FILEMMAP *fmap3=egi_fmap_create("/tmp/egi_gb2unicode.h", fmap->fsize/4*17, PROT_WRITE,MAP_PRIVATE);
	FILE *fp;
        fp=fopen("/tmp/egi_gb2unicode.h","wb");
        if(fp==NULL) exit(1);


	struct UnicodeToGB2312 {
		uint16_t  gbcode; /* gb2312 code */
		uint16_t  unicode;
	};
	struct UnicodeToGB2312 *U2Glist; /* unicode to gb2312_code list */

	struct UnicodeToGB2312 tmpList[]={ {0x0001,0x000a}, {0x0002,0x000b} };

//	printf("tmpList[]: 0x%04X, 0x%04X\n", tmpList[1].gbcode, tmpList[1].unicode);

	uint16_t *pu;
	char *pc=fmap2->fp;

	pu=(uint16_t *)fmap->fp;


	for(k=0; k < fmap->fsize/4; k++) {
		printf("0x%04X 0x%04X\n", pu[2*k], pu[2*k+1]);

		/* Conver to { gb2312, unicode } */
		pc[k*4]=fmap->fp[k*4+3];  /* Swap here */
		pc[k*4+1]=fmap->fp[k*4+2];
		pc[k*4+2]=fmap->fp[k*4];
		pc[k*4+3]=fmap->fp[k*4+1];
	}


	U2Glist =(struct UnicodeToGB2312 *)fmap2->fp;
	for(k=0; k < fmap2->fsize/4; k++)
		printf("GB2UN: 0x%04X, 0x%04X\n", U2Glist[k].gbcode, U2Glist[k].unicode);

/* To write as:
   {0x0001,0x000a},
   {0x0002,0x000b},
*/
	char strtmp[64];
	for(k=0; k < fmap->fsize/4; k++) {
		memset(strtmp,0,sizeof(strtmp));
		sprintf(strtmp,"{0x%04X, 0x%04X},\n", U2Glist[k].gbcode, U2Glist[k].unicode);
		fwrite(strtmp, 1, strlen(strtmp), fp);
	}

	egi_fmap_free(&fmap);
	egi_fmap_free(&fmap2);
	fclose(fp);
#endif //////////////////////////////////////////////


#if 1 ///////////////////////////////////////////////
	FILE *fp;
        fp=fopen("/tmp/egi_gb2unicode2.h","wb");
        if(fp==NULL) exit(1);

	/*-----------------------------
	struct GB2312
	{
	    unsigned short gb2312code;
	    CString     ChineseCode;
	}GB2312ToChinese[]
	------------------------------*/
	int list_size=sizeof(GB2312ToChinese)/sizeof(GB2312ToChinese[0]);

	/* To write as:
		   {0x0001,0x000a},
		   {0x0002,0x000b},
	*/
	char strtmp[64];
	int k;
	wchar_t dest;
	for(k=0; k <list_size; k++) {
		memset(strtmp,0,sizeof(strtmp));
		char_uft8_to_unicode( (unsigned char *)GB2312ToChinese[k].ChineseCode, &dest);
		sprintf(strtmp,"{0x%04X, 0x%04X},\n", GB2312ToChinese[k].gb2312code, dest);
		fwrite(strtmp, 1, strlen(strtmp), fp);
	}

	fclose(fp);
#endif ///////////////////////////////////////////


#if 0 /////////////////////////////////////////////
	int list_size=UniHan_gb2unicode_listSize(); //sizeof(EGI_GB2UNICODE_LIST)/sizeof(EGI_GB2UNICODE_LIST[0]);
	int i;
	wchar_t wchar[2]={0};
	char dest[4];

        printf("EGI_GB2UNICODE_LIST has %d uchars!\n", list_size);
	sleep(2);
  #if 0
	for(i=0; i<list_size; i++) {
		memset(dest,0,sizeof(dest));
		wchar[0]=EGI_GB2UNICODE_LIST[i].unicode;
		cstr_unicode_to_uft8(wchar, dest);
		printf("GB2UN: 0x%04X, 0x%04X, %s\n", EGI_GB2UNICODE_LIST[i].gbcode, EGI_GB2UNICODE_LIST[i].unicode, dest);
	}
  #endif
#endif //////////////////////////////////////////////

}

