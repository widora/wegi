/*---------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License version 2 as published by the Free Software
Foundation.

 		!!! This is for EGI functions TEST only !!!

Journal:
	2021-12-06: Create the file.

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

static char buff[1024*1024];      /* for curl returned data, to be big enough! */
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp);

const char *strhtml="<p> dfsdf sdfig df </p>";
const char *strRequest=NULL;

unsigned char  parLines[7][1024];  /* Parsed lines for each block */
wchar_t        unistr[1024];	   /* For unicodes */

int fw=20, fh=20;	/* font size */
int gap=6; //fh/3;
int ln=10;		/* lines */
int lnleft;		/* lines left */

int channel;
int PicTs=2;		/* Picture show time */
int TxtTs=2;		/* Text show time */
bool loop_ON;
unsigned int avgLuma;   /* Avg luma for slide image. */

char imgURL[512];
char imgTXT[512];
char imgDate[128];

char attrString[1024];          /* To be big enough! */
char value[512];                /* To be big enough! */
bool divIsFound;		/* If the tagged division is found. */



void print_help(const char *name)
{
        printf("Usage: %s [-hc:p:t:l]\n", name);
        printf("-h     This help\n");
	printf("-c n   Channel number.\n");
	printf("-p n   Pic show time in second.\n");
	printf("-t n   Text show time in second.\n");
        printf("-v n   Avg luma.\n");
	printf("-l   Loop\n");
}

/*----------------------------
	     MAIN
-----------------------------*/
int main(int argc, char **argv)
{
	int len;
	char *pstr=NULL;
	int ret;
	char *content=NULL;
//	int k=0; /* block line count */

	printf("pstr: %s\n", pstr);


	EGI_IMGBUF *imgbuf=NULL;

        /* Parse input option */
        int opt;
        while( (opt=getopt(argc,argv,"hc:p:t:v:l"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
				exit(0);
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
			case 'l':
				loop_ON=true;
				break;
			default:
				break;
		}
	}


	switch(channel) {
		case 1:
			strRequest="https://www.caixinglobal.com/finance/";
			break;
		case 2:
			strRequest="https://www.caixinglobal.com/business-and-tech/";
			break;
		case 3:
			strRequest="https://www.caixinglobal.com/economy/";
			break;
		case 4:
			strRequest="https://www.caixinglobal.com/china/";
			break;
		case 5:
			strRequest="https://www.caixinglobal.com/world/";
			break;
		case 6:
			strRequest="https://www.caixinglobal.com/caixin-must-read/";
			break;
		default: /* 0 ... */
			/* Chinese: https://finance.caixin.com/m/ >/dev/null */
			strRequest=argv[optind];
			break;
	}

#if 0   /* Start egi log */
        if(egi_init_log("/mmc/test_http.log") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
        }
        EGI_PLOG(LOGLV_INFO,"%s: Start logging...", argv[0]);
#endif
	egi_log_silent(true);

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
        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 3, 20,
				strRequest, buff, NULL, curlget_callback)!=0) {
                 printf("Fail to call https_curl_request() for: %s!\n", strRequest);
		 sleep(10);
		 goto START_REQUEST;
         }
//	printf("%s\n", buff);


        printf("   --- Start slide news ---\n");


   ////////////////////////////// finance ////////////////////////////

	/* Parse HTML */
	pstr=buff;

#if 0 ///////////////////  English ///////////////////////////////
    while( (pstr=cstr_parse_html_tag(pstr, "li", attrString, sizeof(attrString), &content, &len))!=NULL ) {

//	printf("   --- <li content---\n %s\n", content);

	/* Confirm attribute value for class */
	cstr_get_html_attribute(attrString, "class", value);
	printf("attrString: %s\n", attrString);
	printf("class=%s\n", value);
	if(strcmp(value,"has-img cf")!=0) {
		egi_free_char(&content);
		continue;
	}

	/* Extract image */
	cstr_get_html_attribute(content, "src", imgURL);
	/* Add http: before //... */
	if(strstr(imgURL,"http:")==NULL && strstr(imgURL,"https:")==NULL) {
		memmove(imgURL+strlen("http:"), imgURL, strlen(imgURL)+1); /* +1 EOF */
		strncpy(imgURL,"http:", strlen("http:"));
	}

#else  ///////////////////  Chinese ///////////////////////////////
    char *strClassID="ListItemOfMobSame_news";
    while( (pstr=cstr_parse_html_tag(pstr, "div", attrString, sizeof(attrString), &content, &len))!=NULL ) {
//	printf("   --- <div content---\n %s\n", content);

	/* Confirm attribute value for class */
	fprintf(stderr,"get attribute class..\n");
	cstr_get_html_attribute(attrString, "class", value);
	printf("attrString: %s\n", attrString);
	printf("class=%s\n", value);
	if(strncmp(value, strClassID, strlen(strClassID))!=0) {
		egi_free_char(&content);
		continue;
	}

	/* Extract image */
	fprintf(stderr,"get attribute imgURL..\n");
	cstr_get_html_attribute(content, "src", imgURL);
#endif /////////////////

	/* Sometimes there maybe '\n' in the URL! */
	cstr_squeeze_string(imgURL, strlen(imgURL), '\n');
	printf("imgURL: %s\n", imgURL);

	/* Extract all text */
	fprintf(stderr,"extract html text..\n"); /* TODO: segmentation fault here */
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

	/* If NO image */
	if(imgURL[0]==0)
		continue;

	/* --- 4. Download image and display with Text  --- */
	/* 4.1 Download image */
	ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 5, 20,
			imgURL, "/tmp/slide_news.jpg", NULL, NULL);

	/* 4.2 Display image and text */
	if(ret==0) {
		printf("Succeed to download slide_news.jpg\n");
		imgbuf=egi_imgbuf_readfile("/tmp/slide_news.jpg");

		/* D1. Resize and display image */
		int imgW=320; int imgH=240;
		egi_imgbuf_get_fitsize(imgbuf, &imgW, &imgH);
		egi_imgbuf_resize_update(&imgbuf, false, imgW,imgH);
		if(avgLuma)
			egi_imgbuf_avgLuma(imgbuf, avgLuma);
		if(imgbuf) {
			fb_clear_workBuff(&gv_fb_dev,WEGI_COLOR_DARKPURPLE);
			/* imgbuf, fbdev, subnum, subcolor, x0,y0 */
			egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, (320-imgW)/2, (240-imgH)/2 );
		}
		fb_render(&gv_fb_dev);
		sleep(PicTs);

                /* Free imgbuf */
                egi_imgbuf_free2(&imgbuf);

		/* Write TEXT */

		/* D2. Calculate lines needed. */
		printf("Cal lnleft ...\n");
		FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
						fw, fh, (UFT8_PCHAR)imgTXT,   	/* fw,fh pstr */
						320-5, ln, 4, 5, 5,        	/* pixpl, lines, gap, x0,y0 */
						WEGI_COLOR_BLACK, -1, 240, 	/* fontcolor, transpcolor, opaque */
						NULL, &lnleft, NULL, NULL ); 	/* *cnt, *lnleft, *penx, *peny */
		printf("lnleft=%d\n", lnleft);

		/* D3. Display text */
		if(imgTXT[0]) {
		   /* fbdev, x1,y1,x2,y2, color,alpha */
		   //draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (18+3)*3+6, WEGI_COLOR_GRAY2, 200);
		   draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (fh+4)*(ln-lnleft)+8, WEGI_COLOR_GRAYC, 255);
		   FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
						fw, fh, (UFT8_PCHAR)imgTXT,   	/* fw,fh pstr */
						320-5, ln-lnleft, 4, 5, 5,      /* pixpl, lines, gap, x0,y0 */
						WEGI_COLOR_DARKGRAY, -1, 240, 	/* fontcolor, transpcolor, opaque */
						NULL, NULL, NULL, NULL );	/* *cnt, *lnleft, *penx, *peny */
		}

		/* D4. Write LOGO */
		//draw_blend_filled_rect( &gv_fb_dev, 320-100, 240-32, 320-1, 240-1, WEGI_COLOR_GRAYC, 180);
		draw_blend_filled_roundcorner_rect(&gv_fb_dev, 320-100, 240-32, 320-1, 240-1, 8, WEGI_COLOR_GRAYC, 180);
		FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold, /* fbdev, face */
						1.125*24, 24, (UFT8_PCHAR)"财新网",  	/* fw,fh pstr */
						200, 1, 0, 320-90, 240-30,        	/* pixpl, lines, gap, x0,y0 */
						WEGI_COLOR_DARKBLUE, -1, 240, 	/* fontcolor, transpcolor, opaque */
						NULL, NULL, NULL, NULL ); 	/* *cnt, *lnleft, *penx, *peny */
		fb_render(&gv_fb_dev);
		sleep(TxtTs);
	}
	/* 4.3 Fail to get image */
	else {
		printf("Fail to download news image!\n");
		//exit(0);
	}

   } /* while */


   	if(loop_ON)goto START_REQUEST;

        printf("   --- END ---\n");

	return 0;
}

/*-----------------------------------------------
 A callback function to deal with replied data.
------------------------------------------------*/
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
        int blen=strlen(buff);
        int plen=strlen(ptr);
        int xlen=plen+blen;

        /* MUST check buff[] capacity! */
        if( xlen > sizeof(buff)-1) {   // OR: CURL_RETDATA_BUFF_SIZE-1-strlen(..)
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


