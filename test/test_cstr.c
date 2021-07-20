/*---------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License version 2 as published by the Free Software
Foundation.

Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <freetype2/ft2build.h>
#include <freetype2/ftglyph.h>
#include <ctype.h>
#include "egi_common.h"
#include "egi_utils.h"
#include "egi_FTsymbol.h"
#include "egi_cstring.h"

int main( int  argc,   char**  argv )
{
  int 		i,j,k;
  wchar_t *wbook=L"assd人心生一念，天地尽皆知。善恶若无报，乾坤必有私。\
　　那菩萨闻得此言，满心欢喜，对大圣道：“圣经云：‘出其言善。\
　　则千里之外应之；出其言不善，则千里之外适之。’你既有此心，待我到了东土大唐国寻一个取经的人来，教他救你。你可跟他做个徒弟，秉教伽持，入我佛门。再修正果，如何？”大圣声声道：“愿去！愿去！”菩萨道：“既有善果，我与你起个法名。”大圣道：“我已有名了，叫做孙悟空。”菩萨又喜道：“我前面也有二人归降，正是‘悟’字排行。你今也是‘悟’字，却与他相合，甚好，甚好。这等也不消叮嘱，我去也。”那大圣见性明心归佛教，这菩萨留情在意访神谱。\
　　他与木吒离了此处，一直东来，不一日就到了长安大唐国。敛雾收云，师徒们变作两个疥癫游憎，入长安城里，竟不觉天晚。行至大市街旁，见一座土地庙祠，二人径进，唬得那土地心慌，鬼兵胆战。知是菩萨，叩头接入。那土地又急跑报与城隍社令及满长安城各庙神抵，都来参见，告道：“菩萨，恕众神接迟之罪。”菩萨道：“汝等不可走漏消息。我奉佛旨，特来此处寻访取经人。借你庙宇，权住几日，待访着真僧即回。”众神各归本处，把个土地赶到城隍庙里暂住，他师徒们隐遁真形。\
　　毕竟不知寻出那个取经来，且听下回分解。";


#if 1 /////////  cstr_get_peword(), cstr_strlen_eword()  //////////
    const char *pstr="人心生一念，天地尽皆知HELLOOO WORLD! this is really very,,,, god 徒们变nice ssooooo yes! --++12283NDD";

/* --- Result -----
len=7 Word: HELLOOO
len=5 Word: WORLD
len=4 Word: this
len=2 Word: is
len=6 Word: really
len=4 Word: very
len=3 Word: god
len=4 Word: nice
len=7 Word: ssooooo
len=3 Word: yes
len=8 Word: 12283NDD

*/
    char *pt;
    int len;

    /* Print out all alphanumeric characters */
    for(i=0; i<128; i++) {
	    if( isalnum(i) )
		printf("%c is an alphanumeric character!\n", i);
    }
    exit(0);


    pt =(char *)pstr;
    while( (pt=cstr_get_peword((UFT8_PCHAR)pt)) ) {
	  len=cstr_strlen_eword((UFT8_PCHAR)pt);
	  printf("Word: ");
	  for(i=0; i<len; i++)
		printf("%c", pt[i]);
	  printf("   (len=%d) \n", len);
	  pt +=len;
    }

    exit(0);
#endif

#if 1 /////////  cstr_hash_string() //////////
char buff[1024];

while( fgets(buff, sizeof(buff)-1, stdin)!=NULL ) {
	printf("String HASH: %lld\n", cstr_hash_string((UFT8_PCHAR)buff,0));
}

exit(0);
#endif


#if 0  //////////  egi_count_file_lines(const char *fpath) ///////////

if(argc<2)
  exit(-1);

printf("Lines of egi.conf: %d\n", egi_count_file_lines(argv[1]) );

exit(0);
#endif


#if 0  //////////  int cstr_copy_line(const char *src, char *dest, size_t size)  ///////////

char src[]="0123456";
src[0]='\0';
char dest[16];
int ret;


 ret=cstr_copy_line( src, dest, 16 );

 printf("ret=%d,  dest[max 16]='%s'  \n",ret, dest);

exit(0);
#endif


#if 1 	//////////    int egi_update_config_value(char *sect, char *key, char* pvalue)   //////////

	egi_update_config_value("TOUCH_PAD", "xpt_facts", "HELLO");
	egi_update_config_value("TOUCH_PAD", "xpt_factX", "12.1");
	egi_update_config_value("TOUCH_PAD", "xpt_factY", "20.1");
	egi_update_config_value("TOUCH_PAD_", "xpt_facts", "HELLO");

exit(0);
#endif


#if 0  ////////// cstr_extract_ChnUft8TimeStr()    cstr_getSecFrom_ChnUft8TimeStr()   ////////////

/*
十五小时一百零七分钟七百零零九秒以后提醒
五分钟后提醒
35分钟后提醒
一小时48分钟后提醒。
半个小时后提醒。
12点58分提醒。
95分钟以后提醒。
12小时107分钟以后提醒。
108小时78分钟以后提醒。
三刻钟以后提醒
11点零六十二秒
862小时58分钟，72秒
862小时58分钟72秒
*/

   char strwords[]="请在上午一千六百十七时六分一千六百十七秒提醒我";
   char strtm[128]={0};
   int	secs;

   if(argc<2)exit(1);

   if( cstr_extract_ChnUft8TimeStr( (const char *)argv[1], strtm, sizeof(strtm))==0 )
	printf("Time str: %s\n",strtm);

   secs=cstr_getSecFrom_ChnUft8TimeStr(strtm,NULL);
   printf("Total %d seconds.\n",secs);

   exit(0);
#endif

  uint32_t a=0x12345678;
  char *pa=(char *)&a;
  printf(" *pa=0x%x \n", *(pa+3));


#if 0	///////////////  cstr/char_unicode_to_uft8( )  /////////////

	char char_uft8[4+1];
	char buff[2048];
	wchar_t *wp=wbook;

	char *pbuf=buff;

	#if  1 /* ------- char_unicode_to_uft8() ------- */
	memset(buff,0,sizeof(buff));
	while(*wp) {
		//memset(char_uft8,0,sizeof(char_uft8));
		k=char_unicode_to_uft8(wp, pbuf);
		if(k<=0)
			break;

		pbuf+=k;
		wp++;
	}
	printf("%s\n",buff);
	#endif

	#if  1 /* ------- cstr_unicode_to_uft8() ------- */
	memset(buff,0,sizeof(buff));
	cstr_unicode_to_uft8(wbook,buff);
	printf("%s\n",buff);
	#endif


	exit(0);

#endif

  return 0;
}

