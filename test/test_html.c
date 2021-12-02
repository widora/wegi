/*---------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License version 2 as published by the Free Software
Foundation.

 		!!! This is for EGI functions TEST only !!!

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

char imgURL[512];
char imgTXT[512];
char imgDate[128];

char attrString[1024];          /* To be big enough! */
char value[512];                /* To be big enough! */
bool divIsFound;		/* If the tagged division is found. */

/*----------------------------
	     MAIN
-----------------------------*/
int main(int argc, char **argv)
{
	int len;
	char *pstr=NULL;
	int ret;
	char *content=NULL;

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


	/* clear buff */
	memset(buff,0,sizeof(buff));

        /* Https GET request */
//        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 3, 10,
        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 3, 10,
				argv[1]!=NULL ? argv[1]:strRequest, buff, NULL, curlget_callback)!=0) {
                 printf("Fail to call https_curl_request() for: %s!\n", argv[1]);
                 exit(-1);
         }
	printf("%s\n", buff);


        printf("   --- Start slide news ---\n");

#if 0 ////////////////////////////////////  sina  ////////////////////////////////////////////
	/* Parse HTML */
	pstr=buff;
	int k=0; /* block line count */
	do {
		/* parse returned data as html */
		pstr=cstr_parse_html_tag(pstr, "dd", NULL, &content, &len);

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

			/* P1. Convert to unicode */
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

			/* P2. Convert to uft8 */
			cstr_unicode_to_uft8(unistr, imgTXT);

			/* P3. Download image */
			ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 5, 5,
					(char *)parLines[3], "/tmp/slide_news.jpg", NULL, NULL);

			/* P4. Display image and imgTXT */
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

				if(argc>2)
					sleep(atoi(argv[2]));
				else
					sleep(2);

				/* Write title */
				//int fw=18, fh=18;

			#if 0  /* Call FTsymbol_unicstrings_writeFB() */
							/* fbdev, x1,y1,x2,y2, color,alpha */
				draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (fh+gap)*4+8, WEGI_COLOR_GRAYC, 200);
     				FTsymbol_unicstrings_writeFB( &gv_fb_dev, egi_appfonts.bold, fw, fh, unistr,   /* fbdev, face, fw,fh pwchar */
							      320-5, gap, 3, 5, 5,	/* pixpl, lines, gap, x0,y0 */
							      WEGI_COLOR_DARKGRAY, -1, 240); /* fontcolor, transpcolor, opaque */
			#else /*  Call FTsymbol_uft8strings_writeFB() */
				printf("Cal lnleft ...\n");
				FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
								fw, fh, (UFT8_PCHAR)imgTXT,   	/* fw,fh pstr */
								320-8, ln, gap, 5, 5,        	/* pixpl, lines, gap, x0,y0 */
								WEGI_COLOR_BLACK, -1, 240, 	/* fontcolor, transpcolor, opaque */
								NULL, &lnleft, NULL, NULL ); 	/* *cnt, *lnleft, *penx, *peny */
				printf("lnleft=%d\n", lnleft);

				/* D3. Display text */
				/* fbdev, x1,y1,x2,y2, color,alpha */
				draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (fh+gap)*(ln-lnleft)+8, WEGI_COLOR_GRAYC, 255);
				FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
								fw, fh, (UFT8_PCHAR)imgTXT,   	/* fw,fh pstr */
								320-8, ln-lnleft, gap, 5, 5,      /* pixpl, lines, gap, x0,y0 */
								WEGI_COLOR_DARKGRAY, -1, 255, 	/* fontcolor, transpcolor, opaque */
								NULL, NULL, NULL, NULL );	/* *cnt, *lnleft, *penx, *peny */
			#endif /* End FTsymbol wirteFB */

				fb_render(&gv_fb_dev);
				if(argc>3)
					sleep(atoi(argv[3]));
				else
					sleep(3);
			}
			else {
				printf("Fail to download news image!\n");
				exit(0);
			}
		}

		/* Free content */
		egi_free_char(&content);

	} while( pstr!=NULL );


#elif 0 //////////////////////////////////  shine  ///////////////////////////////////////////

	/* Parse HTML */
	pstr=buff;

	int k=0; /* block line count */

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

		/* If NO image */
		if(imgURL[0]==0)
			continue;

   #endif

	/* --- 4. Download image and display with Text  --- */
	/* 4.1 Download image */
	ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 5, 20,
			imgURL, "/tmp/slide_news.jpg", NULL, NULL);

	/* 4.2 Display image and text */
	if(ret==0) {
		printf("Succeed to download slide_news.jpg\n");
		imgbuf=egi_imgbuf_readfile("/tmp/slide_news.jpg");

		/* D1. Resize and display image */
		egi_imgbuf_resize_update(&imgbuf, false, 320,240);
		egi_imgbuf_avgLuma(imgbuf, 255/2);
		if(imgbuf) {
			/* imgbuf, fbdev, subnum, subcolor, x0,y0 */
			egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0,0);
		}
		fb_render(&gv_fb_dev);

		if(argc>2)
			sleep(atoi(argv[2]));
		else
			sleep(2);

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
		/* fbdev, x1,y1,x2,y2, color,alpha */
		//draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (18+3)*3+6, WEGI_COLOR_GRAY2, 200);
		draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (fh+4)*(ln-lnleft)+8, WEGI_COLOR_GRAYC, 255);
		FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
						fw, fh, (UFT8_PCHAR)imgTXT,   	/* fw,fh pstr */
						320-5, ln-lnleft, 4, 5, 5,      /* pixpl, lines, gap, x0,y0 */
						WEGI_COLOR_DARKGRAY, -1, 240, 	/* fontcolor, transpcolor, opaque */
						NULL, NULL, NULL, NULL );	/* *cnt, *lnleft, *penx, *peny */

		/* D4. Write ShanghaiDaily */
		FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
						1.125*18, 18, (UFT8_PCHAR)"ShanghaiDaily",  	/* fw,fh pstr */
						200, 1, 0, 320-160, 240-25,        	/* pixpl, lines, gap, x0,y0 */
						WEGI_COLOR_PINK, -1, 240, 	/* fontcolor, transpcolor, opaque */
						NULL, NULL, NULL, NULL ); 	/* *cnt, *lnleft, *penx, *peny */
		fb_render(&gv_fb_dev);
		if(argc>3)
			sleep(atoi(argv[3]));
		else
			sleep(5);

	}
	/* 4.3 Fail to get image */
	else {
		printf("Fail to download news image!\n");
		//exit(0);
	}

  } /* while */

#elif 1 ////////////////////////////// fox ////////////////////////////

	/* Parse HTML */
	pstr=buff;

	int k=0; /* block line count */

    while( (pstr=cstr_parse_html_tag(pstr, "img", attrString, NULL, &len))!=NULL ) { /* Selfclosed tag, content is same as attrString */
	printf("<img > attrString: %s\n", attrString);

	/* Extract image */
	cstr_get_html_attribute(attrString, "src", imgURL);
	/* Add http: before //... */
	memmove(imgURL+strlen("http:"), imgURL, strlen(imgURL));
	strncpy(imgURL,"http:", strlen("http:"));

	printf("imgURL: %s\n", imgURL);

        /* Extract date */

	/* Extract text */
	cstr_get_html_attribute(attrString, "alt", imgTXT);
	/* Decode HTML entity names */
	cstr_decode_htmlSpecailChars(imgTXT);
	printf("imgTXT: %s\n", imgTXT);

	/* Free content */
        if(content!=NULL) {
               	free(content);
	        content=NULL;
        }

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
		egi_imgbuf_resize_update(&imgbuf, false, 320,240);
		egi_imgbuf_avgLuma(imgbuf, 255/2);
		if(imgbuf) {
			/* imgbuf, fbdev, subnum, subcolor, x0,y0 */
			egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0,0);
		}
		fb_render(&gv_fb_dev);

		if(argc>2)
			sleep(atoi(argv[2]));
		else
			sleep(2);

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
		/* fbdev, x1,y1,x2,y2, color,alpha */
		//draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (18+3)*3+6, WEGI_COLOR_GRAY2, 200);
		draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (fh+4)*(ln-lnleft)+8, WEGI_COLOR_GRAYC, 255);
		FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
						fw, fh, (UFT8_PCHAR)imgTXT,   	/* fw,fh pstr */
						320-5, ln-lnleft, 4, 5, 5,      /* pixpl, lines, gap, x0,y0 */
						WEGI_COLOR_DARKGRAY, -1, 240, 	/* fontcolor, transpcolor, opaque */
						NULL, NULL, NULL, NULL );	/* *cnt, *lnleft, *penx, *peny */

		/* D4. Write ShanghaiDaily */
		FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
						1.125*18, 18, (UFT8_PCHAR)"Foxbusiness",  	/* fw,fh pstr */
						200, 1, 0, 320-160, 240-25,        	/* pixpl, lines, gap, x0,y0 */
						WEGI_COLOR_PINK, -1, 240, 	/* fontcolor, transpcolor, opaque */
						NULL, NULL, NULL, NULL ); 	/* *cnt, *lnleft, *penx, *peny */
		fb_render(&gv_fb_dev);
		if(argc>3)
			sleep(atoi(argv[3]));
		else
			sleep(5);

	}
	/* 4.3 Fail to get image */
	else {
		printf("Fail to download news image!\n");
		//exit(0);
	}

  } /* while */


#else //////////////////////////////////  jagran  ///////////////////////////////////////////
	/* Parse HTML */
	pstr=buff;

	int k=0; /* block line count */

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

	/* 4A. Check result */
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
		egi_imgbuf_resize_update(&imgbuf, false, 320,240);
		egi_imgbuf_avgLuma(imgbuf, 255/2);
		if(imgbuf) {
			/* imgbuf, fbdev, subnum, subcolor, x0,y0 */
			egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0,0);
		}
		fb_render(&gv_fb_dev);

		if(argc>2)
			sleep(atoi(argv[2]));
		else
			sleep(2);

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
		/* fbdev, x1,y1,x2,y2, color,alpha */
		//draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (18+3)*3+6, WEGI_COLOR_GRAY2, 200);
		draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (fh+4)*(ln-lnleft)+8, WEGI_COLOR_GRAYC, 255);
		FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
						fw, fh, (UFT8_PCHAR)imgTXT,   	/* fw,fh pstr */
						320-5, ln-lnleft, 4, 5, 5,      /* pixpl, lines, gap, x0,y0 */
						WEGI_COLOR_DARKGRAY, -1, 240, 	/* fontcolor, transpcolor, opaque */
						NULL, NULL, NULL, NULL );	/* *cnt, *lnleft, *penx, *peny */

		/* D4. Write ShanghaiDaily */
		FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
						1.125*18, 18, (UFT8_PCHAR)"JAGRAN",  	/* fw,fh pstr */
						200, 1, 0, 320-160, 240-25,        	/* pixpl, lines, gap, x0,y0 */
						WEGI_COLOR_PINK, -1, 240, 	/* fontcolor, transpcolor, opaque */
						NULL, NULL, NULL, NULL ); 	/* *cnt, *lnleft, *penx, *peny */
		fb_render(&gv_fb_dev);
		if(argc>3)
			sleep(atoi(argv[3]));
		else
			sleep(5);

	}
	/* 4.3 Fail to get image */
	else {
		printf("Fail to download news image!\n");
	}


   } /* End while() */


#endif

        printf("   --- END ---\n");

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


