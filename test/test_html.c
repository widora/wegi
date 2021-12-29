/*---------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License version 2 as published by the Free Software
Foundation.

 		!!! This is for EGI functions TEST only !!!

Example:
	./test_html -c 0 -p 2 -l
	./test_html -c 2 -p 2 -t 5 -l
	./test_html -c 1 -s -l -v 125 http://slide.news.sina.com.cn/w/

TODO:
1. XXX Case 5: aljazeera  quit.
2. XXX Case 0: stock, http get buff error!

Journal:
2021-12-09:
	1. Add display_slide().
2021-12-13/14:
	1. Add SINA stock data chart.
2021-12-17:
	1. Shine video news.

Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "egi_timer.h"
#include "egi_debug.h"
#include "egi_log.h"
#include "egi_https.h"
#include "egi_gb2unicode.h" /* before egi_cstring.h */
#include "egi_cstring.h"
#include "egi_utils.h"
#include "egi_fbdev.h"
#include "egi_image.h"
#include "egi_FTsymbol.h"
#include "egi_math.h"
#include "egi_procman.h"

static char buff[1024*1024];    /* CURL_RETDATA_BUFF_SIZE,  for curl returned data, to be big enough! */
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp);

const char *strhtml="<p> dfsdf sdfig df </p>";
const char *strRequest=NULL;
EGI_FILEMMAP *fmap;
char *pstr, *pstr2;
char *content,*content2;
int  len, len2;

unsigned char  parLines[7][1024];  /* Parsed lines for each block */
wchar_t        unistr[1024*25];	   /* For unicodes */

int fw=20, fh=20;	/* font size */
int gap=6; //fh/3;
int ln=10;		/* lines */
int lnleft;		/* lines left */

int channel;
bool sub_channel;	/* sub_channel, use input URL for strRequest */
char *data_code;	/* */

int PicTs=2;		/* Picture show time */
int TxtTs=2;		/* Text show time */
bool loop_ON;		/* whole loop */
bool video_loop_ON=true;	/* loop playing video */

char videoURL[512];
char imgURL[512];
char dataURL[512];
char strData[256]; 	/* For stock data */
char stockCode[16];	/* For stock code */
EGI_IMGBUF *imgbuf;
unsigned int avgLuma;	/* Avg luma for slide image. */
char imgTXT[1024*32]; //25];
char imgDate[128];
int imgW=320; int imgH=240;

char attrString[1024];          /* To be big enough! */
char value[512];                /* To be big enough! */
bool divIsFound;		/* If the tagged division is found. */


/* Functions */
void display_slide(const char *imgURL, const char *imgTXT,  FT_Face face,
		   const char *logoName, int fnW, int fnH, int x0, int y0);
void display_info(const char *txt,  FT_Face face, int fw, int fh, int x0, int y0);


void print_help(const char *name)
{
        printf("Usage: %s [-hc:p:t:l]\n", name);
        printf("-h     This help\n");
	printf("-l     Loop\n");
	printf("-s     Input URL as sub_channel\n");
	printf("-c n   Channel number.\n");
	printf("   0   SINA stock data.\n");
	printf("   1   SINA\n");
	printf("   2   SHINE\n");
	printf("   3   FOXbusiness\n");
	printf("   4   Jagran\n");
	printf("   5   Aljazeera\n");
	printf("   6   iFeng\n");
	printf("   7   Shine video story\n");
	printf("-p n   Pic show time in second.\n");
	printf("-t n   Text show time in second.\n");
	printf("-v n   Avg luma.\n");
	printf("-d n   Data code for sina stock.\n");
	printf("-f n   A pseudo_HTML script file.\n");
}

/*----------------------------
	     MAIN
-----------------------------*/
int main(int argc, char **argv)
{
	int ret;
	int k=0; /* block line count */
	int i;

        /* Parse input option */
        int opt;
        while( (opt=getopt(argc,argv,"hlsc:p:t:v:d:f:"))!=-1 ) {
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
			case 'd':
				data_code=optarg;
				break;
			case 'f':
				fmap=egi_fmap_create(optarg, 0, PROT_READ, MAP_SHARED);
				if(fmap)
					printf("Script %s is used.\n", optarg);
				break;
			default:
				break;
		}
	}

	//strRequest=argv[optind];
	switch(channel) {
		case 0:
			/* SINA stock, with input HTML file stock.html */
			break;
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
			/* Sub_channel:
			  https://ifinace.ifeng.com
			  https://itech.ifeng.com
			  https://inews.ifeng.com
			  https://ihistory.ifeng.com
			*/
			break;
		case 7:
			strRequest="https://www.shine.cn/video/";
			break;
		default:
			strRequest=argv[optind];
			break;
	}

	/* If sub_channel */
	if(sub_channel)
		strRequest=argv[optind];

	/* If script file for stock */
	if(channel==0 && fmap) {
		strRequest=fmap->fp;
	}
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
//	egi_log_silent(true);

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
	/* Skip http request if channel==0 as for stock image. */
	if(channel==0)
		goto END_REQUEST;

        /* Https GET request */
	//memset(buff,0,sizeof(buff)); /* Clear buff each time before call https_curl_request() */
	buff[0]=0;
	egi_dperr("---->start curl request\n");
//        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 3, 10,
        if( https_curl_request( HTTPS_OPTION_DEFAULT, 3, 10,
				strRequest, buff, NULL, curlget_callback)!=0) {
                 printf("Fail to call https_curl_request() for: %s!\n", strRequest);
		 sleep(10);
		 goto START_REQUEST;
         }
	egi_dperr("---->curl request OK\n");

	printf(" <<< BUFF >>>\n");
//	printf("%s\n", buff);
	printf("Replied http data size=%d\n", strlen(buff));

END_REQUEST:
        printf("   --- Start slide news ---\n");

switch(channel) {
  case 0:  ////////////////////////////////////  SINA_STOCK  ///////////////////////////////////////////

     /* Read script file */
     if( fmap ) {
	/* NO TXT */
	//imgTXT[0]='\0';

        pstr=fmap->fp;
        while( (pstr=cstr_parse_html_tag(pstr, "img", attrString, sizeof(attrString), NULL, &len))!=NULL ) { /* Selfclosed tag, content is same as attrString */
		printf("attrString: %s\n", attrString);

		/* 1.1 Extract image */
		cstr_get_html_attribute(attrString, "src", imgURL);
		printf("imgURL: %s\n", imgURL);

                /* 1.2 Extract dataURL */
		cstr_get_html_attribute(attrString, "data", dataURL);
                printf("dataURL: %s\n", dataURL);

		/* 1.3 Extract text */
		cstr_get_html_attribute(attrString, "code", stockCode);
		printf("code: %s\n", stockCode);

		/* 1.4 Get stock data */
		strData[0]='\0';  /* Clear strData */
		if(dataURL[0]!=0) {
			//memset(buff,0,sizeof(buff)); /* Clear buff each time before call https_curl_request() */
			buff[0]=0;
        		if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 3, 10,
                                dataURL, buff, NULL, curlget_callback)!=0) {
				printf("Fail to request to dataURL!\n");
			}
			else {
				/* To extract data */
//				printf("Reply from dataURL: %s\n", buff);
				char *ps=strstr(buff,",");
				if(ps) {
					ps++;
					char *pe=strstr(ps,"\"");
					if(pe) {
						strncpy(strData, ps, (pe-ps < sizeof(strData)-1) ? pe-ps : sizeof(strData)-1 );
						strData[sizeof(strData)-1]=0;
					}
				}
			}
        	}

		/* 1.5 Get stock/company notice */
		char Request[512];
		Request[0]=0;
		strcat(Request, "http://vip.stock.finance.sina.com.cn/corp/view/vCB_BulletinGather.php?stock_str=");
		strcat(Request, stockCode);
		//strcat(Request, "&gg_data=2021-16&ftype=0");

		imgTXT[0]='\0';
		if(stockCode[0]!=0) {
			//memset(buff,0,sizeof(buff)); /* Clear buff each time before call https_curl_request() */
			buff[0]=0;
        		if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 3, 10,
                                Request, buff, NULL, curlget_callback)!=0) {
				printf("Fail to request for stock notice!\n");
			}
			else {
				printf("\033[0;32;40m ---- Reply from stock bulletin: %s \e[0m\n", buff);
        			/* Extract notice data  */
				cstr_parse_html_tag(buff, "tbody", NULL, 0, &content, &len);
				printf("\033[0;31;40m ------ tbody  ----- \e[0m\n %s\n content_len=%dBytes\n", content, len);
				if(len<1) {
					egi_free_char(&content);
					goto DISPLAY_STOCK_SLIDE;
				}

				/* Squeeze FL */
				//cstr_squeeze_string(content, strlen(content), '\n');

			        /* Extract all text */
				#if 0
			        cstr_extract_html_text(content, imgTXT);
				#else
				cstr_parse_simple_html(content, imgTXT, sizeof(imgTXT));
				#endif
			        printf("imgTXT: %s\n", imgTXT);

                   		/* GBK convert to unicode */
		#if 0    /* GB2312 big-endian To little-endian */
                        char *pt=imgTXT;
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
                                else {                      /* At least 2 bytes */
                                        chtmp=*pt;
                                        *pt=*(pt+1);
                                        *(pt+1)=chtmp;
                                        pt+=2;
                                }
                        }

				printf("start gbk to unicode ..\n");
                        	cstr_gb2312_to_unicode(imgTXT, unistr);
		#else
				cstr_gbk_to_unicode(imgTXT, unistr);
		#endif

                 	        /* Unicode Convert to uft8 */
				printf("start unicode to utf8...\n");
                        	cstr_unicode_to_uft8(unistr, imgTXT);
				printf("imgTXT utf8: %s\n", imgTXT);

				/* Free content */
				egi_free_char(&content);
			}
		}

DISPLAY_STOCK_SLIDE:
		/* Display slide image and text */
		/* url,txt, face, logoname, fw, fh, x0,y0 */
		//display_slide(imgURL, imgTXT, egi_appfonts.regular, "http://finance.sina.com.cn", 13, 13, 10, 240-15);
		printf("Start slide...\n");
		display_slide(imgURL, imgTXT, egi_appfonts.regular, strData, 13, 13, 10, 240-15);
		printf("Finish slide\n");
	}
     }
     /* No script */
     else {
	imgTXT[0]='\0';
	strcat(imgTXT, "No script!");

	/* Display slide image and text */
	/* url,txt, face, logoname, fw, fh, x0,y0 */
	display_slide(imgURL, imgTXT, egi_appfonts.regular, NULL, 24, 24, 320-120, 240-24-8);

     } /* End if(fmap) */

     break;

  case 1:  ////////////////////////////////////  SINA  ////////////////////////////////////////////

	/* Parse HTML */
	pstr=buff;

   /* Parse to extract xxxx from:  <dl>..<dd>xxxx</dd>....<dd>xxxx</dd>... </dl> */
   while( (pstr=cstr_parse_html_tag(pstr, "dl", NULL, 0, &content, &len))!=NULL ) {
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
	while( (pstr2=cstr_parse_html_tag(pstr2, "dd", NULL, 0, &content2, &len2))!=NULL ) {

		/* Check content2 */
		if(content2!=NULL)
			printf("Tag<dd>[%d]: %s\n", k, content2);
		else {
			printf("content2 is NULL!\n");
			continue;
		}

		/* Check content2 size */
		if(len2>sizeof(parLines[0])-1) {
			printf("\033[0;31;40m size_of_content2: %dBytes, it's too big! \e[0m\n", len2); /* situation exists! */
			//exit(0);
			egi_free_char(&content2);
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

   case 2:  ////////////////////////////////  SHINE  /////////////////////////////////////

	/* Parse HTML */
	pstr=buff;


    #if 0 /*  Tag '<div..> ...  </div>'  */
    while( (pstr=cstr_parse_html_tag(pstr, "div", attrString, sizeof(attrString), &content, &len))!=NULL ) {

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

    while( (pstr=cstr_parse_html_tag(pstr, "img", attrString, sizeof(attrString), NULL, &len))!=NULL ) { /* Selfclosed tag, content is same as attrString */
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


  case 3:  ////////////////////////////// FOX ////////////////////////////

	/* Parse HTML */
	pstr=buff;

    while( (pstr=cstr_parse_html_tag(pstr, "img", attrString, sizeof(attrString), NULL, &len))!=NULL ) { /* Selfclosed tag, content is same as attrString */
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
   while( (pstr=cstr_parse_html_tag(pstr, "figure", attrString, sizeof(attrString), &content, &len))!=NULL ) {
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

////////// <article //////////
    while( (pstr=cstr_parse_html_tag(pstr, "article", attrString, sizeof(attrString), &content, &len))!=NULL ) {
//       printf("   --- <article content---\n %s\n", content);
//       printf("attrString: %s\n", attrString);

        /* Confirm attribute value for class */
	fprintf(stderr, "----1\n");
        cstr_get_html_attribute(attrString, "class", value);
//      printf("class=%s\n", value);


        /* Extract image */
	fprintf(stderr, "----2\n");
        cstr_get_html_attribute(content, "src", imgURL);

        /* Add http: before //... */
	fprintf(stderr, "----3\n");
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
	fprintf(stderr, "----4\n");
        cstr_squeeze_string(imgURL, strlen(imgURL), '\n');
        printf("imgURL: %s\n", imgURL);

        /* Extract all text */
	fprintf(stderr, "----5\n");
        cstr_extract_html_text(content, imgTXT, sizeof(imgTXT));
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
	fprintf(stderr, "----6\n");
	display_slide(imgURL, imgTXT, egi_appfonts.bold, "半島新聞 ALJAZEERA", 20, 20, 320-250, 240-25);

   } /* End while() */
  break;

  case 6: //////////////////////////////////  IFENG  ///////////////////////////////////////
	/* Parse HTML */
	pstr=buff;

    //////////  Extract <div class="titleImg-1BvN-wPz"> .... </div>  //////////
    while( (pstr=cstr_parse_html_tag(pstr, "div", attrString, sizeof(attrString), &content, &len))!=NULL ) {

        /* Confirm attribute value for class  */
        cstr_get_html_attribute(attrString, "class", value);
        printf("class=%s\n", value);
        if( strncmp(value,"titleImg", strlen("titleImg"))!=0		/* Title with image */
	    && strncmp(value,"singleTitle", strlen("singleTitle"))!=0) {  /* No image */
		egi_free_char(&content);
                continue;
	}

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
        cstr_extract_html_text(content, imgTXT, sizeof(imgTXT));
        printf("imgTXT: %s\n", imgTXT);

#if 0
        /* Double_decode HTML entity names, in such case as: ....&amp;quot;Hello&amp;quot;... */
        cstr_decode_htmlSpecailChars(imgTXT);
        cstr_decode_htmlSpecailChars(imgTXT);
        printf("imgTXT: %s\n", imgTXT);
#endif

        /* Free content */
	egi_free_char(&content);

	/* Display slide image and text */
	/* url,txt, face, logoname, fw, fh, x0,y0 */
	display_slide(imgURL, imgTXT, egi_appfonts.regular, "鳳凰新聞", 24, 24, 320-120, 240-24-8);

   } /* End while() */
   break;

  case 7: //////////////////////////////////  SHINE VIDEO  ///////////////////////////////////////
	/* Parse HTML */
	pstr=buff;

    while( (pstr=cstr_parse_html_tag(pstr, "video", attrString, sizeof(attrString), &content, &len))!=NULL ) {

        /* Get video address */
        cstr_get_html_attribute(attrString, "src", videoURL);
        printf("Video URL: %s\n", videoURL);

        /* Extract poster image */
        cstr_get_html_attribute(attrString, "poster", imgURL);
        printf("Poster src: %s\n", imgURL);

        /* Extract title */
        cstr_get_html_attribute(attrString, "data-title", imgTXT);
        printf("Title: %s\n", imgTXT);

        /* Free content */
        if(content!=NULL) {
                free(content);
                content=NULL;
        }

	/* Display slide image and text */
	/* url,txt, face, logoname, fw, fh, x0,y0 */
	display_slide(imgURL, imgTXT, egi_appfonts.regular, "ShineNews", 20, 20, 320-120, 240-24-8);

	/* Download video */
   	if(videoURL[0]!=0) {
	   display_info("Downloading video ...", egi_appfonts.regular, 20, 20, 10, 184+20);
	   if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 5, 60,
				videoURL, "/mmc/shine.mp4", NULL, NULL) ==0 ) {

		/* Convert video to MPEG */
	   	display_info("Converting video ...", egi_appfonts.regular, 20, 20, 10, 184+20); //5, 120+25);
		egi_system("ffmpeg -y -i /mmc/shine.mp4 -f mpeg -s 320x184 -r 20 /mmc/shine.avi");

	      for(i=0; i<5||video_loop_ON; i++) {
	   	display_info("Start to play video...", egi_appfonts.regular, 20, 20, 10, 184+20); //5, 120+25*2);
		if( egi_system("/tmp/MPlayer -ac mad -vo fbdev -framedrop /mmc/shine.avi")==0 ) {
	   		display_info("OK! finish playing.", egi_appfonts.regular, 20, 20, 10, 184+20); //5, 120+25*2);
			sleep(5);
		}
		else
	   		display_info("Error playing video.", egi_appfonts.regular, 20, 20, 10, 184+20); //5, 120+25*2);

	      }
	   }
	   else
	   	display_info("Fail to download video!", egi_appfonts.regular, 20, 20, 10, 184+20); //5, 120+20+5);
	}

   } /* End while() */
   break;

  default: break;
} /* End switch() */////////////////////////////////////////


   	if(loop_ON)goto START_REQUEST;

        printf("   --- END ---\n");


	egi_fmap_free(&fmap);
	return 0;
}

/*--------------------------------------------------
 A callback function to deal with http replied data.
--------------------------------------------------*/
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	int blen=strlen(buff);
	int plen=strlen(ptr);
	int xlen=plen+blen;

   	/* MUST check buff[] capacity! */
	if( xlen >= sizeof(buff)-1) {   // OR: CURL_RETDATA_BUFF_SIZE-1-strlen(..)
		/* "If src contains n or more bytes, strncat() writes n+1 bytes to dest
		 * (n from src plus the terminating null byte." ---man strncat
		 */
		fprintf(stderr,"\033[0;31;40m strlen(buff)=%d, strlen(buff)+strlen(ptr)=%d >=sizeof(buff)-1! \e[0m\n",
							blen, xlen);
		//exit(0);  /* YES! */

		strncat(userp, ptr, sizeof(buff)-1-blen); /* 1 for '\0' by strncat */
		//strncat(buff, ptr, sizeof(buff)-1);
	}
	else {
	    #if 1 /* strcat */
	        strcat(userp, ptr);
		//buff[sizeof(buff)-1]='\0';  /* <---------- return data maybe error!!??  */
	    #else /* memcpy */
		memcpy(userp, ptr, size*nmemb); // NOPE for compressed data?!!!
		userp[size*nmemb]='\0';
	    #endif
	}

	return size*nmemb; /* Return size*nmemb OR curl will fails! */
}


/*----------------------------------------------
Display image and text of a slide item.

@imgURL: 	Image URL
@imgTXT:	Slide text.
@face:		Font face for imgTXT,
@logoName:	Logo name
@fnw,fnh:	Font size for logo name.
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
	printf("Fetch the image from imgURL...\n");
   	if(imgURL[0]!=0) {
		ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 5, 20,
				imgURL, "/tmp/slide_news.jpg", NULL, NULL);
	}
	else
		ret=-1;


	/* 2. Fitsize and display slide image */
	if(ret==0 && (imgbuf=egi_imgbuf_readfile("/tmp/slide_news.jpg"))) {
		printf("Ok, succeed to download slide_news.jpg.\n");

	   /* 2a. If for ---- SINA --- stock image */
	   if(channel==0) {
		EGI_IMGBUF *imgcrop=NULL; /* Upper part of original image */
		int k=0;

		/* Get curren time */
		time_t tmp;
		struct tm *tp;
		time(&tmp);
		tp=localtime(&tmp);

		/* Reset alpha */
		egi_imgbuf_resetColorAlpha(imgbuf, -1, 245);

	     #if 0 /* Display as 2 parts */
		for(k=0; k<2; k++) {
		   /* Crop image fore. and aft. half part respectively, then to display... */
		   /* 30  for overlapped part.   (imgbuf ,px,py,h,w) */
		   imgcrop=egi_imgbuf_blockCopy(imgbuf, k*(imgbuf->width/2 -30), 5, imgbuf->height*2/3-10, imgbuf->width/2 +30);

		   /* Resize and display image */
		   imgW=320; imgH=220;
		   egi_imgbuf_resize_update(&imgcrop, false, imgW, imgH);
		   if(avgLuma)
			egi_imgbuf_avgLuma(imgcrop, avgLuma);

		   fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAYC);
		   /* imgbuf, fbdev, subnum, subcolor, x0,y0 */
		   egi_subimg_writeFB(imgcrop, &gv_fb_dev, 0, -1, (320-imgW)/2, (240-imgH)/2 );

		   fb_render(&gv_fb_dev);
		   sleep(PicTs);

		   /* Free imgcrop */
		   egi_imgbuf_free2(&imgcrop);

		} /* End for() */

	     #else /* Crop into two parts */
		//for(k=0; k<2; k++) {
		k=0;
		/* 9:30-11:30 Session */
		if( tp->tm_hour*60+tp->tm_min < 13*60-5 ){ /* start aftnoon session 13:05 */

		   /* Crop percentage labels on right side and paste to mid. */
		   egi_imgbuf_copyBlock(imgbuf, imgbuf, false, 535-493, 183, 272,0, 493,0); /* destimg, srcimg, blendON, bw,bh, xd,yd, xs,ys */
		   /* Crop date and time to mid */
		   egi_imgbuf_copyBlock(imgbuf, imgbuf, false, 495-390, 18, 168,0, 390,0); /* destimg, srcimg, blendON, bw,bh, xd,yd, xs,ys */
		   /* Crop SINA logo to mid */
		   egi_imgbuf_copyBlock(imgbuf, imgbuf, false, 490-455, 25, 455-(490-270),150, 455,150); /* destimg, srcimg, blendON, bw,bh, xd,yd, xs,ys */

		   /* Crop fore. part of the whole image to display... */
		   /* 40  for overlapped part.   (imgbuf ,px,py,h,w) */
		   imgcrop=egi_imgbuf_blockCopy(imgbuf, 0, 5, imgbuf->height*2/3-10, imgbuf->width/2 +40);

		   /* Resize and display image */
		   imgW=320; imgH=220;
		   egi_imgbuf_resize_update(&imgcrop, false, imgW, imgH);
		   if(avgLuma)
			egi_imgbuf_avgLuma(imgcrop, avgLuma);

		   fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAYC);
		   /* imgbuf, fbdev, subnum, subcolor, x0,y0 */
		   egi_subimg_writeFB(imgcrop, &gv_fb_dev, 0, -1, (320-imgW)/2, 5); //(240-imgH)/2 );
		   /* Free imgcrop */
		   egi_imgbuf_free2(&imgcrop);

		   fb_render(&gv_fb_dev);
		   //sleep(PicTs); Aft. putting Logo...

		}
		/* 13:00-15:00 Session */
		else {
		   /* Crop value labels on left side and paste to mid. */
		   egi_imgbuf_copyBlock(imgbuf, imgbuf, false, 50, 183, 270-50,0, 0,0); /* destimg, srcimg, blendON, bw,bh, xd,yd, xs,ys */

		   /* Crop stock name and paste to mid. */
		   egi_imgbuf_copyBlock(imgbuf, imgbuf, false, 170-50,18, 270,0, 50,0); /* destimg, srcimg, blendON, bw,bh, xd,yd, xs,ys */

		   /* Crop aft. part of image to display... */
		   /* 40  for overlapped part.   (imgbuf ,px,py,h,w) */
		   imgcrop=egi_imgbuf_blockCopy(imgbuf, 270-50, 5, imgbuf->height*2/3-10, imgbuf->width/2 +40);

		   /* Resize and display image */
		   imgW=320; imgH=220;
		   egi_imgbuf_resize_update(&imgcrop, false, imgW, imgH);
		   if(avgLuma)
			egi_imgbuf_avgLuma(imgcrop, avgLuma);

		   fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAYC);
		   /* imgbuf, fbdev, subnum, subcolor, x0,y0 */
		   egi_subimg_writeFB(imgcrop, &gv_fb_dev, 0, -1, (320-imgW)/2, 5); //(240-imgH)/2 );
		   /* Free imgcrop */
		   egi_imgbuf_free2(&imgcrop);

		   fb_render(&gv_fb_dev);
		}

	     #endif
	   }

	   /* 2b. Other channel */
	   else {
		/* Resize and display image */
		imgW=320; imgH=240;
		egi_imgbuf_get_fitsize(imgbuf, &imgW, &imgH);
		egi_imgbuf_resize_update(&imgbuf, false, imgW,imgH);
		if(avgLuma)
			egi_imgbuf_avgLuma(imgbuf, avgLuma);
		fb_clear_workBuff(&gv_fb_dev,WEGI_COLOR_DARKPURPLE);
		/* imgbuf, fbdev, subnum, subcolor, x0,y0 */
		egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, (320-imgW)/2, (240-imgH)/2 );
 	   }
	}

	/* 3. Use pure color as background */
	if( imgbuf==NULL)
		fb_clear_workBuff(&gv_fb_dev, COLOR_Aqua);

	/* 4. Write LOGO Name */
FTsymbol_enable_SplitWord();
	FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
					fnW, fnH,(UFT8_PCHAR)logoName,	/* fw, fh, pstr */
					320, 1, 0, x0, y0,        	/* pixpl, lines, gap, x0,y0 */
					WEGI_COLOR_PINK, -1, 240, 	/* fontcolor, transpcolor, opaque */
					NULL, NULL, NULL, NULL ); 	/* *cnt, *lnleft, *penx, *peny */
	fb_render(&gv_fb_dev);
	sleep(PicTs);

	/* 5. Calculate lines needed. */
	if(imgTXT[0]==0)
		goto END_FUNC;

FTsymbol_disable_SplitWord();
	printf("Cal lnleft ...\n");
	FTsymbol_uft8strings_writeFB( &gv_fb_dev, face,  		/* fbdev, face */
					fw, fh, (UFT8_PCHAR)imgTXT,   	/* fw,fh pstr */
					320-5, ln, 4, 5, 5,        	/* pixpl, lines, gap, x0,y0 */
					WEGI_COLOR_BLACK, -1, 240, 	/* fontcolor, transpcolor, opaque */
					NULL, &lnleft, NULL, NULL ); 	/* *cnt, *lnleft, *penx, *peny */
	printf("lnleft=%d\n", lnleft);

	/* 6. Display text */
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


/*----------------------------------------------
Display info text.

@txt:		information text.
@face:		Font face for imgTXT,
@fw,fh:		Font size for logo name.
@x0,y0:		Logo origin position
-----------------------------------------------*/
void display_info(const char *txt,  FT_Face face, int fw, int fh, int x0, int y0)
{
	draw_filled_rect2( &gv_fb_dev, WEGI_COLOR_GRAYC, 0,184, 320-1, 240-1);
	FTsymbol_uft8strings_writeFB( &gv_fb_dev, face,  		/* fbdev, face */
					fw, fh, (UFT8_PCHAR)txt,   	/* fw,fh pstr */
					320-5, 240/fh+1, 4, x0, y0,       /* pixpl, lines, gap, x0,y0 */
					WEGI_COLOR_DARKGRAY, -1, 240, 	/* fontcolor, transpcolor, opaque */
					NULL, NULL, NULL, NULL );	/* *cnt, *lnleft, *penx, *peny */
	fb_render(&gv_fb_dev);
}
