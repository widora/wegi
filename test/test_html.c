/*---------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License version 2 as published by the Free Software
Foundation.

 		!!! This is for EGI functions TEST only !!!

Example:
	./test_html -c 1 -s -l -v 125 http://slide.news.sina.com.cn/w/

Journal:
2021-12-09:
	1. Add display_slide().

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
#include "egi_gb2unicode.h" /* before egi_cstring.h */
#include "egi_cstring.h"
#include "egi_utils.h"
#include "egi_fbdev.h"
#include "egi_image.h"
#include "egi_FTsymbol.h"
#include "egi_math.h"

static char buff[1024*1024];      /* for curl returned data, to be big enough! */
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp);

const char *strhtml="<p> dfsdf sdfig df </p>";
const char *strRequest=NULL;
char *pstr, *pstr2;
char *content,*content2;
int  len, len2;

unsigned char  parLines[7][1024];  /* Parsed lines for each block */
wchar_t        unistr[1024];	   /* For unicodes */

int fw=20, fh=20;	/* font size */
int gap=6; //fh/3;
int ln=10;		/* lines */
int lnleft;		/* lines left */

int channel;
bool sub_channel;	/* sub_channel, use input URL for strRequest */
int PicTs=2;		/* Picture show time */
int TxtTs=2;		/* Text show time */
bool loop_ON;

char imgURL[512];
EGI_IMGBUF *imgbuf;
unsigned int avgLuma;	/* Avg luma for slide image. */
char imgTXT[512];
char imgDate[128];
int imgW=320; int imgH=240;

char attrString[1024];          /* To be big enough! */
char value[512];                /* To be big enough! */
bool divIsFound;		/* If the tagged division is found. */


/* Functions */
void display_slide(const char *imgURL, const char *imgTXT,  FT_Face face,
		   const char *logoName, int fnW, int fnH, int x0, int y0);


void print_help(const char *name)
{
        printf("Usage: %s [-hc:p:t:l]\n", name);
        printf("-h     This help\n");
	printf("-l     Loop\n");
	printf("-s     Input URL as sub_channel\n");
	printf("-c n   Channel number.\n");
	printf("   1   SINA\n");
	printf("   2   SHINE\n");
	printf("   3   FOXbusiness\n");
	printf("   4   Jagran\n");
	printf("   5   Aljazeera\n");
	printf("   6   iFeng\n");
	printf("-p n   Pic show time in second.\n");
	printf("-t n   Text show time in second.\n");
	printf("-v n   Avg luma.\n");
}

/*----------------------------
	     MAIN
-----------------------------*/
int main(int argc, char **argv)
{
	int ret;
	int k=0; /* block line count */


        /* Parse input option */
        int opt;
        while( (opt=getopt(argc,argv,"hlsc:p:t:v:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
				exit(0);
                                break;
			case 'l':
				loop_ON=true;
				break;
			case 's':
				sub_channel=true;
				break;
			case 'c':
				channel=atoi(optarg);
				break;
			case 'p':
				PicTs=atoi(optarg);
				break;
			case 't':
				TxtTs=atoi(optarg);
				break;
			case 'v':
				avgLuma=atoi(optarg);
				if(avgLuma>200)
					avgLuma=200;
				else if(avgLuma<100)
					avgLuma=100;
				break;
			default:
				break;
		}
	}

	//strRequest=argv[optind];
	switch(channel) {
		case 1:
			strRequest="https://slide.news.sina.com.cn";
			/* Sub_channel:
			  https://slide.news.sina.com.cn/c/       China
			  https://slide.news.sina.com.cn/w/	  World
			  https://slide.news.sina.com.cn/d/
			  https://slide.news.sina.com.cn/z/
			  https://slide.news.sina.com.cn/weather/  Weather
			 */
			break;
		case 2:
			strRequest="https://www.shine.cn";
			break;
		case 3:
			strRequest="https://www.foxbusiness.com";
			break;
		case 4:
			strRequest="https://english.jagran.com";
			break;
		case 5:
			strRequest="https://chinese.aljazeera.net/news";
			break;
		case 6:
			strRequest="https://ifinance.ifeng.com";
			break;
		default:
			strRequest=argv[optind];
			break;
	}

	/* If sub_channel */
	if(sub_channel)
		strRequest=argv[optind];


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
	FTsymbol_disable_SplitWord();

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


START_REQUEST:
	/* clear buff */
	memset(buff,0,sizeof(buff));

        /* Https GET request */
//        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 3, 10,
        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 3, 10,
				strRequest, buff, NULL, curlget_callback)!=0) {
                 printf("Fail to call https_curl_request() for: %s!\n", strRequest);
		 sleep(10);
		 goto START_REQUEST;
         }
	printf(" <<< BUFF >>>\n%s\n", buff);


        printf("   --- Start slide news ---\n");


switch(channel) {
	case 0: case 2:  ////////////////////////////////////  shine  ///////////////////////////////////////////

	/* Parse HTML */
	pstr=buff;


    #if 0 /*  Tag '<div..> ...  </div>'  */
    while( (pstr=cstr_parse_html_tag(pstr, "div", attrString, &content, &len))!=NULL ) {

        //printf("content: '%s'\n", content);

        /* 1. Get tag attribut value and check */
        cstr_get_html_attribute(attrString, "class", value);
        if(strcmp(value, "top-story")==0) { /* top-story */
                printf("\n        [ ----- DIV top-story ----- ]\n %s\n", content);

		/* 1.1 Extract image */
		cstr_get_html_attribute(content, "data-original", imgURL);
		printf("imgURL: %s\n", imgURL);

                /* 1.2 Extract date */
                cstr_get_html_attribute(content, "data-date", imgDate);
                printf("Date: %s\n", imgDate);

		/* 1.3 Extract text */
		cstr_get_html_attribute(content, "alt", imgTXT);
		printf("imgTXT: %s\n", imgTXT);

		/* 1.4 Put date to imgTxt */
		if(imgDate[0])
			sprintf(imgTXT+strlen(imgTXT), "\n    [%s]", imgDate);

		/* Set token */
		divIsFound=true;
        }
        else {
                //printf("Top story NOT found!\n");
		/* Set token */
		divIsFound=false;
        }

        /* 2. Prepare to search on for the next tag .. */
        //printf("search on ....\n");
        pstr+=strlen("div");
        if( pstr-buff>=strlen(buff) ) {
                printf("Get end of html, break now...\n");
                break;
        }
        /* Free content */
        if(content!=NULL) {
                free(content);
                content=NULL;
        }

	/* 3. Check result */
	if(divIsFound==false)
		continue;

   #else /* Tag '<img ..>' */

    while( (pstr=cstr_parse_html_tag(pstr, "img", attrString, NULL, &len))!=NULL ) { /* Selfclosed tag, content is same as attrString */
		printf("attrString: %s\n", attrString);

		/* 1.1 Extract image */
		cstr_get_html_attribute(attrString, "data-original", imgURL);
		printf("imgURL: %s\n", imgURL);

                /* 1.2 Extract date */

		/* 1.3 Extract text */
		cstr_get_html_attribute(attrString, "alt", imgTXT);
		printf("imgTXT: %s\n", imgTXT);

	        /* Free content */
        	if(content!=NULL) {
                	free(content);
	                content=NULL;
        	}

   #endif

	/* Display slide image and text */
	/* url,txt, face, logoname, fw, fh, x0,y0 */
	display_slide(imgURL, imgTXT, egi_appfonts.bold,
				mat_random_range(2)?"ShanghaiDaily":"上海日報", 18, 18, 320-160, 240-25);

  } /* while */
  break;

  case 1:  ////////////////////////////////////  SINA  ////////////////////////////////////////////

	/* Parse HTML */
	pstr=buff;

   while( (pstr=cstr_parse_html_tag(pstr, "dl", NULL, &content, &len))!=NULL ) {
		/* Check content */
		if(content!=NULL)
			printf("Tag<dl> content : %s\n",content);
		else {
			printf("content is NULL!\n");
			continue;
		}

	/* Reset  k and pstr2 */
	k=0;
	pstr2=content;
	while( (pstr2=cstr_parse_html_tag(pstr2, "dd", NULL, &content2, &len2))!=NULL ) {

		/* Check content2 */
		if(content2!=NULL)
			printf("Tag<dd>[%d]: %s\n", k, content2);
		else {
			printf("content2 is NULL!\n");
			continue;
		}

//		strncpy((char *)&parLines[k%7][0], pstr2, len);
		memset(parLines[k%7],0, sizeof(parLines[0]));
		strncpy( (char *)parLines[k%7], pstr2, len2);
		parLines[k%7][len2]=0;

		/* Free content */
//		egi_free_char(&content2);

		printf("Get partLies[%d]:%s\n", k%7, parLines[k%7]);
		printf("partLines[%d][0]=%u, partLines[%d][1]=%u\n", k%7,parLines[k%7][0],k%7,parLines[k%7][1]);

		k++;

		/* Process block content */
		if(k==7) {
			//k=0; NOT here! k MAY >7

			printf("Title: %s\n", parLines[6]);
			printf("Image: %s\n", parLines[3]);
			printf("Time: %s\n", parLines[4]);

			/* P1. Convert to unicode */
			printf("GB2312 to UNICDOE....\n");

#if 0			/* GB2312 big-endian To little-endian */
			char *pt=(char *)parLines[6];
			char chtmp;
			while(*pt!=0){

				/* ASCII CODES: ??? Necessary ???  Yes! */
				/* ASCII CODEs */
				if( *(unsigned char*)pt < 0x80 )
					pt++;
				/* GB2312 CODEs */
				else if( *(pt+1)=='\0' ) {  /* The last byte */
					pt++;
				}
				else {			    /* At least 2 bytes */
					chtmp=*pt;
					*pt=*(pt+1);
					*(pt+1)=chtmp;
					pt+=2;
				}
			}
			/* GB2312 convert to unicode */
			cstr_gb2312_to_unicode((char *)parLines[6], unistr);

#else			/* GBK convert to unicode */
			cstr_gbk_to_unicode((char *)parLines[6], unistr);
#endif

			/* P2. Convert to uft8 */
			cstr_unicode_to_uft8(unistr, imgTXT);
			/* add time */
			strcat(imgTXT, " [");
			strcat(imgTXT,(char *)parLines[4]);
			strcat(imgTXT, "]");
			printf("imgTXT: %s\n", imgTXT);

			/* Display slide image and text */
			strcpy(imgURL,(char *)parLines[3]);
			/* url,txt, face, logoname, fw, fh, x0,y0 */
			display_slide(imgURL, imgTXT, egi_appfonts.regular, "SINA新浪", 20, 20, 320-110, 240-25);

		} /* END k==7 */

		/* Free content */
		egi_free_char(&content2);

		/* Move pstr2 */
		pstr2 +=len2;

		/* Limit k */
		if(k==7) break;

	} /* END while() for TAG 'dd' */

	/* Free content */
	egi_free_char(&content);

	/* Move pstr */
	pstr +=len;

   } /* END while() for TAG 'dl' */
  break;

  case 3:  ////////////////////////////// FOX ////////////////////////////

	/* Parse HTML */
	pstr=buff;

    while( (pstr=cstr_parse_html_tag(pstr, "img", attrString, NULL, &len))!=NULL ) { /* Selfclosed tag, content is same as attrString */
	printf("   --- <img attrString ---\n %s\n", attrString);

	/* Extract image */
	cstr_get_html_attribute(attrString, "src", imgURL);
	/* Add http: before //... */
	memmove(imgURL+strlen("http:"), imgURL, strlen(imgURL)+1); /* +1 EOF */
	strncpy(imgURL,"http:", strlen("http:"));

	/* Sometimes there maybe '\n' in the URL! */
	cstr_squeeze_string(imgURL, strlen(imgURL), '\n');

	printf("imgURL: %s\n", imgURL);

        /* Extract date */

	/* Extract text */
	cstr_get_html_attribute(attrString, "alt", imgTXT);

	/* Double_decode HTML entity names, in such case as: ....&amp;quot;Hello&amp;quot;... */
	cstr_decode_htmlSpecailChars(imgTXT);
	cstr_decode_htmlSpecailChars(imgTXT);

	printf("imgTXT: %s\n", imgTXT);

	/* Free content */
        if(content!=NULL) {
               	free(content);
	        content=NULL;
        }

	/* Display slide image and text */
	/* url,txt, face, logoname, fw, fh, x0,y0 */
	display_slide(imgURL, imgTXT, egi_appfonts.bold, "Foxbusiness", 18, 18, 320-140, 240-25);

   } /* while */
 break;

 case 4: //////////////////////////////////  jagran  ///////////////////////////////////////////
	/* Parse HTML */
	pstr=buff;

  /* TODO , or use "img"(self_closed tag) as tag */
   while( (pstr=cstr_parse_html_tag(pstr, "figure", attrString, &content, &len))!=NULL ) {
        //printf("content: '%s'\n", content);

        /* 1. Extract image URL. */
        cstr_get_html_attribute(content, "src", imgURL);
	printf("imgURL: %s\n", imgURL);

        /* 2. Extract image Text. */
        cstr_get_html_attribute(content, "alt", imgTXT);
	printf("imgTXT: %s\n", imgTXT);

        /* 3. Prepare to search on for the next tag .. */
        //printf("search on ....\n");
        pstr+=strlen("figure");
        if( pstr-buff>=strlen(buff) ) {
                printf("Get end of html, break now...\n");
                break;
        }
        /* Free content */
        if(content!=NULL) {
                free(content);
                content=NULL;
        }

	/* Display slide image and text */
        /* url,txt, face, logoname, fw, fh, x0,y0 */
	display_slide(imgURL, imgTXT, egi_appfonts.bold, "JAGRAN", 20, 20, 320-120, 240-25);

   } /* End while() */
  break;

  case 5: //////////////////////////////////  AL-JAZEERA  ///////////////////////////////////////
	/* Parse HTML */
	pstr=buff;

#if 0 //////////  <li  //////////
    while( (pstr=cstr_parse_html_tag(pstr, "li", attrString, &content, &len))!=NULL ) {
        printf("   --- <li content---\n %s\n", content);

        /* Confirm attribute value for class */
        cstr_get_html_attribute(attrString, "class", value);
        printf("class=%s\n", value);
        if(strcmp(value,"featured-articles-list__item")!=0)
                continue;
#else ////////// <article //////////
    while( (pstr=cstr_parse_html_tag(pstr, "article", attrString, &content, &len))!=NULL ) {
        printf("   --- <article content---\n %s\n", content);

        /* Confirm attribute value for class */
        cstr_get_html_attribute(attrString, "class", value);
        printf("class=%s\n", value);

#endif

        printf("attrString: %s\n", attrString);

        /* Extract image */
        cstr_get_html_attribute(content, "src", imgURL);

        /* Add http: before //... */
        if(strstr(imgURL,"http:")==NULL && strstr(imgURL,"https:")==NULL) {
		/* IF starts with "//...." */
		if( imgURL[0]=='/' && imgURL[1]=='/' ) {
	                memmove(imgURL+strlen("http:"), imgURL, strlen(imgURL)+1); /* +1 EOF */
        	        strncpy(imgURL,"http:", strlen("http:"));
		}
		/* ELSE starts with "/...." */
		else {
			char  *RootAddr="https://chinese.aljazeera.net";
			memmove(imgURL+strlen(RootAddr), imgURL, strlen(imgURL)+1); /* +1 EOF */
			strncpy(imgURL,RootAddr, strlen(RootAddr));
		}
        }

        /* Sometimes there maybe '\n' in the URL! */
        cstr_squeeze_string(imgURL, strlen(imgURL), '\n');
        printf("imgURL: %s\n", imgURL);

        /* Extract all text */
        cstr_extract_html_text(content, imgTXT);
        printf("imgTXT: %s\n", imgTXT);

#if 0
        /* Double_decode HTML entity names, in such case as: ....&amp;quot;Hello&amp;quot;... */
        cstr_decode_htmlSpecailChars(imgTXT);
        cstr_decode_htmlSpecailChars(imgTXT);
        printf("imgTXT: %s\n", imgTXT);
#endif

        /* Free content */
        if(content!=NULL) {
                free(content);
                content=NULL;
        }

	/* Display slide image and text */
	/* url,txt, face, logoname, fw, fh, x0,y0 */
	display_slide(imgURL, imgTXT, egi_appfonts.bold, "半島新聞 ALJAZEERA", 20, 20, 320-250, 240-25);

   } /* End while() */
  break;

  case 6: //////////////////////////////////  IFENG  ///////////////////////////////////////
	/* Parse HTML */
	pstr=buff;

    //////////  Extract <div class="titleImg-1BvN-wPz"> .... </div>  //////////
    while( (pstr=cstr_parse_html_tag(pstr, "div", attrString, &content, &len))!=NULL ) {

        /* Confirm attribute value for class  */
        cstr_get_html_attribute(attrString, "class", value);
        printf("class=%s\n", value);
        if( strncmp(value,"titleImg", strlen("titleImg"))!=0		/* Title with image */
	    && strncmp(value,"singleTitle", strlen("singleTitle"))!=0) /* No image */
                continue;

        printf("attrString: %s\n", attrString);

        /* Extract image */
        cstr_get_html_attribute(content, "data-lazy-src", imgURL);
        /* Add http: before //... */
        if(strstr(imgURL,"http:")==NULL && strstr(imgURL,"https:")==NULL) {
		/* IF starts with "//...." */
		if( imgURL[0]=='/' && imgURL[1]=='/' ) {
	                memmove(imgURL+strlen("http:"), imgURL, strlen(imgURL)+1); /* +1 EOF */
        	        strncpy(imgURL,"http:", strlen("http:"));
		}
		/* ELSE starts with "/...." */
		else {
			char  *RootAddr="https://chinese.aljazeera.net";
			memmove(imgURL+strlen(RootAddr), imgURL, strlen(imgURL)+1); /* +1 EOF */
			strncpy(imgURL,RootAddr, strlen(RootAddr));
		}
        }

        /* Sometimes there maybe '\n' in the URL! */
        cstr_squeeze_string(imgURL, strlen(imgURL), '\n');
        printf("imgURL: %s\n", imgURL);

        /* Extract all text */
        cstr_extract_html_text(content, imgTXT);
        printf("imgTXT: %s\n", imgTXT);

#if 0
        /* Double_decode HTML entity names, in such case as: ....&amp;quot;Hello&amp;quot;... */
        cstr_decode_htmlSpecailChars(imgTXT);
        cstr_decode_htmlSpecailChars(imgTXT);
        printf("imgTXT: %s\n", imgTXT);
#endif

        /* Free content */
        if(content!=NULL) {
                free(content);
                content=NULL;
        }

	/* Display slide image and text */
	/* url,txt, face, logoname, fw, fh, x0,y0 */
	display_slide(imgURL, imgTXT, egi_appfonts.regular, "鳳凰新聞", 24, 24, 320-120, 240-24-8);

   } /* End while() */
   break;

  default: break;
} /* End switch() */////////////////////////////////////////


   	if(loop_ON)goto START_REQUEST;

        printf("   --- END ---\n");

	return 0;
}

/*-----------------------------------------------
 A callback function to deal with replied data.
------------------------------------------------*/
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
        strcat(userp, ptr);
	//memcpy(userp, ptr, size*nmemb); NOPE for compressed data?!!!
        return size*nmemb;
}


/*----------------------------------------------
Display image and text of a slide item.

@imgURL: 	Image URL
@imgTXT:	Slide text.
@face:		Font face for imgTXT,
@logoName:	Logo name
@fw,fh:		Font size for logo name.
@x0,y0:		Logo origin position
-----------------------------------------------*/
void display_slide(const char *imgURL, const char *imgTXT,  FT_Face face,
		   const char *logoName, int fnW, int fnH, int x0, int y0)
{
	int ret=0;

	if(imgURL==NULL && imgTXT==NULL)
		return;
	if(imgURL[0]==0 && imgTXT[0]==0)
		return;

	/* 1. Download image */
   	if(imgURL[0]!=0) {
		ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 5, 20,
				imgURL, "/tmp/slide_news.jpg", NULL, NULL);
	}

	/* 2. Fitsize and display slide image */
	if(ret==0 && (imgbuf=egi_imgbuf_readfile("/tmp/slide_news.jpg"))) {
		printf("Succeed to download slide_news.jpg\n");

		/* D1. Resize and display image */
		imgW=320; imgH=240;
		egi_imgbuf_get_fitsize(imgbuf, &imgW, &imgH);
		egi_imgbuf_resize_update(&imgbuf, false, imgW,imgH);
		if(avgLuma)
			egi_imgbuf_avgLuma(imgbuf, avgLuma);
//		if(imgbuf) {
		fb_clear_workBuff(&gv_fb_dev,WEGI_COLOR_DARKPURPLE);
		/* imgbuf, fbdev, subnum, subcolor, x0,y0 */
		egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, (320-imgW)/2, (240-imgH)/2 );
//		}
	}

	/* 3. Use pure color as background */
	if( imgbuf==NULL)
		fb_clear_workBuff(&gv_fb_dev, COLOR_Aqua);

	/* 4. Write LOGO Name */
FTsymbol_enable_SplitWord();
	FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
				1.125*fnW, fnH, (UFT8_PCHAR)logoName,  	/* fw,fh pstr */
					320, 1, 0, x0, y0,        	/* pixpl, lines, gap, x0,y0 */
					WEGI_COLOR_PINK, -1, 240, 	/* fontcolor, transpcolor, opaque */
					NULL, NULL, NULL, NULL ); 	/* *cnt, *lnleft, *penx, *peny */
	fb_render(&gv_fb_dev);
	sleep(PicTs);

	/* 5. Calculate lines needed. */
FTsymbol_disable_SplitWord();
	printf("Cal lnleft ...\n");
	FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
					fw, fh, (UFT8_PCHAR)imgTXT,   	/* fw,fh pstr */
					320-5, ln, 4, 5, 5,        	/* pixpl, lines, gap, x0,y0 */
					WEGI_COLOR_BLACK, -1, 240, 	/* fontcolor, transpcolor, opaque */
					NULL, &lnleft, NULL, NULL ); 	/* *cnt, *lnleft, *penx, *peny */
	printf("lnleft=%d\n", lnleft);

	/* 6. Display text */
	if(imgTXT[0]==0)
		goto END_FUNC;

	/* fbdev, x1,y1,x2,y2, color,alpha */
	//draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (18+3)*3+6, WEGI_COLOR_GRAY2, 200);
	draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (fh+4)*(ln-lnleft)+8, WEGI_COLOR_GRAYC, 255);
	FTsymbol_uft8strings_writeFB( &gv_fb_dev, face,  	/* fbdev, face */
					fw, fh, (UFT8_PCHAR)imgTXT,   	/* fw,fh pstr */
					320-5, ln-lnleft, 4, 5, 5,      /* pixpl, lines, gap, x0,y0 */
					WEGI_COLOR_DARKGRAY, -1, 240, 	/* fontcolor, transpcolor, opaque */
					NULL, NULL, NULL, NULL );	/* *cnt, *lnleft, *penx, *peny */

	fb_render(&gv_fb_dev);
	sleep(TxtTs);

END_FUNC:
	/* Free imgbuf */
	egi_imgbuf_free2(&imgbuf);
}
