/*---------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License version 2 as published by the Free Software
Foundation.

Journal:
2021-11-24:
	1. Test download web page picture/titles as slide news.

Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "egi_timer.h"
#include "egi_log.h"
#include "egi_https.h"
#include "egi_cstring.h"
#include "egi_utils.h"
#include "egi_fbdev.h"
#include "egi_image.h"
#include "egi_gb2unicode.h"
#include "egi_FTsymbol.h"
//#include "egi_color.h"

int   cstr_gb2312_to_unicode(const char *src,  wchar_t *dest);

static char buff[256*1024];      /* for curl returned data, to be big enough! */
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp);

const char *strhtml="<p> dfsdf sdfig df </p>";
//const char *strRequest="http://mini.eastday.com/mobile/191123112758979.html";
//const char *strRequest="http://mini.eastday.com/mobile/191123190527243.html";

const char *strRequest="http://slide.news.sina.com.cn";

unsigned char  parLines[7][1024];  /* Parsed lines for each block */
wchar_t  unistr[2048];			/* For unicodes */

int main(int argc, char **argv)
{
	char *content=NULL;
	int len;
	char *pstr=NULL;
	int ret;

	EGI_IMGBUF *imgbuf=NULL;

#if 0
	printf("%s\n", strhtml);
	pstr=cstr_parse_html_tag(strhtml, "p", &content, &len);
	printf("pstr: %s\n",pstr);
	printf("Get %d bytes content: %s\n", len, content);
	egi_free_char(&content);
#endif

#if 0   /* Start egi log */
        if(egi_init_log("/mmc/test_http.log") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
        }
        EGI_PLOG(LOGLV_INFO,"%s: Start logging...", argv[0]);
#endif

#if 0   /* For http,  conflict with curl?? */
        printf("start egitick...\n");
        tm_start_egitick();
#endif

        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

        /* Initilize sys FBDEV */
        printf("init_fbdev()...\n");
        if(init_fbdev(&gv_fb_dev))
                return -1;

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev,false);
        fb_position_rotate(&gv_fb_dev,0);
        //gv_fb_dev.pixcolor_on=true;             /* Pixcolor ON */
        //gv_fb_dev.zbuff_on = true;              /* Zbuff ON */
        //fb_init_zbuff(&gv_fb_dev, INT_MIN);


	/* clear buff */
	memset(buff,0,sizeof(buff));

        /* Https GET request */
        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 3,5,
				argv[1]!=NULL ? argv[1]:strRequest, buff, NULL, curlget_callback)!=0) {
                 printf("Fail to call https_curl_request() for: %s!\n", argv[1]);
                 exit(-1);
         }
	printf("%s\n", buff);

	/* Parse HTML */
	pstr=buff;
	int k=0; /* block line count */
	do {
		/* parse returned data as html */
		pstr=cstr_parse_html_tag(pstr, "dd", &content, &len);

		/* Check content */
		if(content!=NULL)
			printf("Html line: %s\n",content);
		else
			break;

//		strncpy((char *)&parLines[k%7][0], pstr, len);
		memset(parLines[k%7],0, sizeof(parLines[0]));
		strncpy( (char *)parLines[k%7], pstr, len);
		parLines[k%7][len]=0;
		egi_free_char(&content);

		printf("Get partLies[%d]:%s\n", k%7, parLines[k%7]);
		printf("partLines[%d][0]=%u, partLines[%d][1]=%u\n", k%7,parLines[k%7][0],k%7,parLines[k%7][1]);

		k++;

		/* Process block content */
		if(k==7) {
			k=0;
			printf("Title: %s\n", parLines[6]);
			printf("Image: %s\n", parLines[3]);
			printf("Time: %s\n", parLines[4]);

			/* Convert to unicode */
			printf("GB2312 to UNICDOE....\n");
			/* To little endia */
			char *pt=(char *)parLines[6];
			char chtmp;
			while(*pt!=0){
				/* ASCII */
				if( *(unsigned char*)pt < 0x80 )
					pt++;
				/* GB2312 */
				else {
					chtmp=*pt;
					*pt=*(pt+1);
					*(pt+1)=chtmp;
					pt+=2;
				}
			}
			cstr_gb2312_to_unicode((char *)parLines[6], unistr);

			/* Download image */
			ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 5, 5,
					(char *)parLines[3], "/tmp/slide_news.jpg", NULL, NULL);

			if(ret==0) {
				printf("Succeed to download slide_news.jpg\n");
				imgbuf=egi_imgbuf_readfile("/tmp/slide_news.jpg");
				egi_imgbuf_resize_update(&imgbuf, false, 320,240);
				egi_imgbuf_avgLuma(imgbuf, 255/2);
				if(imgbuf) {
					/* imgbuf, fbdev, subnum, subcolor, x0,y0 */
					egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0,0);
				}
				fb_render(&gv_fb_dev);
				sleep(2);

				/* Write title */
							/* fbdev, x1,y1,x2,y2, color,alpha */
				draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (15+3)*3+6, WEGI_COLOR_GRAY3, 200);
     				FTsymbol_unicstrings_writeFB( &gv_fb_dev, egi_appfonts.bold, 15, 15, unistr,   /* fbdev, face, fw,fh pwchar */
							      320-5, 5, 3, 5, 5,	/* pixpl, lines, gap, x0,y0 */
							      WEGI_COLOR_WHITE, -1, 240); /* fontcolor, transpcolor, opaque */

				fb_render(&gv_fb_dev);
				sleep(3);
			}
			else {
				printf("Fail to download news image!\n");
				exit(0);
			}
		}

	} while( pstr!=NULL );

	egi_free_char(&content);
	return 0;
}

/*-----------------------------------------------
 A callback function to deal with replied data.
------------------------------------------------*/
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
        strcat(userp, ptr);
        return size*nmemb;
}


/*-------------------------------------------------------------
Convert string in GB2312 coding into Unicodes.

@src:	String in GB2312.
@dest:  Pointer to space for unicodes.
	and END token L'\0' will be set at last.
Return:
        >0      OK, number of converted characters(wchars).
        <=0     Fail, or unrecognizable.
-------------------------------------------------------------*/
int   cstr_gb2312_to_unicode(const char *src,  wchar_t *dest)
{
	int list_size=sizeof(EGI_GB2UNICODE_LIST)/sizeof(EGI_GB2UNICODE_LIST[0]);

	char *ps=(char *)src;
//        uint16_t *ps=(uint16_t *)src;
	bool search_OK;
        int size=0;     /* in bytes, size of the returned dest in UFT-8 encoding*/

	/* Look table Binary Search*/
	int low,mid,high;

        if(src==NULL || dest==NULL )
                return -1;

        while( *ps !='\0' ) {  /* wchar t end token */

	  /* ASCII CODES */
	  if( *(unsigned char *)ps < 0x80 ) {
		*dest=*ps;
		/* Next ps */
		ps+=1;
	  }
	  /* GB2312 CODES */
	  else {

		/* Look table Binary Search*/
		low=0;
		high=list_size-1;
		search_OK=false;
		while(low<=high) {
			mid=(low+high)>>1;
			printf("mid=%d\n",mid);
		   	if( EGI_GB2UNICODE_LIST[mid].gbcode ==*(uint16_t*)ps ) {
				*dest=EGI_GB2UNICODE_LIST[mid].unicode;
/* TEST: -------- */
			printf("GB2312: 0x%04x -> UNICODE: 0x%04X \n",
				EGI_GB2UNICODE_LIST[mid].gbcode, EGI_GB2UNICODE_LIST[mid].unicode);

				search_OK=true;
				break;
			}
			else if( EGI_GB2UNICODE_LIST[mid].gbcode < *(uint16_t*)ps)
				low=mid+1;
			else
				high=mid-1;
		}

		/* Check search result */
		if(!search_OK)
			break;

		/* Next ps */
		ps+=2;
	   }

	   /* Increment */
           size +=1;
	   dest++;
	   //ps++;

        }

	printf("unicodes: size=%d\n", size);

	/* Set last dest as END */
	*dest=0;

        return size;
}

