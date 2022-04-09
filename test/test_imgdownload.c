/*---------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License version 2 as published by the Free Software
Foundation.

 		!!! This is for EGI functions TEST only !!!

Journal:
2022-04-01: Create the file.
2022-04-04: Test cstr_replace_string().

Midas Zhou
知之者不如好之者好之者不如乐之者
----------------------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "egi_debug.h"
#include "egi_timer.h"
#include "egi_log.h"
#include "egi_https.h"
#include "egi_gb2unicode.h" /* before egi_cstring.h */
#include "egi_cstring.h"
#include "egi_utils.h"
#include "egi_fbdev.h"
#include "egi_image.h"
#include "egi_FTsymbol.h"
#include "egi_bjp.h"

#define TMP_IMAGE_FPATH 	"/tmp/_temp_imgdown_.bjp"
//#define PROGRESSIVE_JPG_URL     "https://tse1-mm.cn.bing.net/th/id/R-C.ed0516530f608d6a912e86b3d9c23b27?rik=IALRZpH%2fmW%2b8IA&riu=http%3a%2f%2fwww.globalgardenfriends.com%2fwp-content%2fuploads%2f2018%2f08%2fHow-to-Successfully-Grow-Fruit-Trees-in-Your-Backyard.jpeg.jpg&ehk=YxLq1%2b0Jr3qXqV11mARJIF%2b9hASX5UpiRUhL4c4OjGA%3d&risl=&pid=ImgRaw&r=0"
#define PROGRESSIVE_JPG_URL "http://www.reasoft.com/tutorials/web/img/progress.jpg"
pthread_t  thread_preview;
void *preview_image(void *argv);
bool download_complete;

static char buff[1024*1024];      /* for curl returned data, to be big enough! */
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp);

const char *strHtml="https://bing.ioliu.cn";
//const char *strRequest=NULL;
char strRequest[1024];
int k;

int fw=16, fh=16;	/* font size */
int gap=4; //fh/3;
int ln=10;		/* lines */
int lnleft;		/* lines left */

int OrgTs=1;		/* Original image show time */
int PicTs=2;		/* Resized image show time */
int TxtTs=2;		/* Text show time */
bool loop_ON;
unsigned int avgLuma;   /* Avg luma for slide image. */
bool verbose_ON;

const char* channelName="Bing必应";
char imgURL[512];
char imgTXT[512];
char imgDate[128];

char attrString[1024];          /* To be big enough! */
char value[512];                /* To be big enough! */
bool divIsFound;		/* If the tagged division is found. */

void print_help(const char *name)
{
        printf("Usage: %s [-hVls:p:t:v:] url\n", name);
        printf("-h     This help\n");
	printf("-V     Verbose on\n");
	printf("-l     Loop\n");
	printf("-s n   Start item number k\n");
	printf("-p n   Pic show time in second.\n");
	printf("-t n   Text show time in second.\n");
        printf("-v n   Avg luma.\n");
}

/*----------------------------
	     MAIN
-----------------------------*/
int main(int argc, char **argv)
{
	int len;
	int ret;
	char *pstr=NULL;
	char *content=NULL;
	EGI_IMGBUF *imgbuf=NULL;
	EGI_PICINFO *picInfo=NULL;

        /* Parse input option */
        int opt;
        while( (opt=getopt(argc,argv,"hVls:p:t:v:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
				exit(0);
                                break;
			case 's':
				k=atoi(optarg);
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
			case 'V':
				verbose_ON=true;
				break;
			case 'l':
				loop_ON=true;
				break;
			default:
				break;
		}
	}
#if 0
	/* Input URL */
	if(optind<argc)
		strRequest=argv[optind];

	/* Check URL */
	if(strRequest==NULL) {
		printf("no URL found!\n");
		exit(-1);
	}
#endif

#if 0   /* Start egi log */
        if(egi_init_log("/mmc/test_http.log") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
        }
        EGI_PLOG(LOGLV_INFO,"%s: Start logging...", argv[0]);
#endif
	if(!verbose_ON)
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



	/* Request string */
	if(k==185)k=0;
	sprintf(strRequest,"%s?p=%d", strHtml, k++);
	printf(DBG_MAGENTA"URL: %s\n"DBG_RESET, strRequest);

	/* clear buff */
	memset(buff,0,sizeof(buff));

        /* Https GET request */
        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 3, 60,
				strRequest, buff, NULL, curlget_callback)!=0) {
                 printf("Fail to call https_curl_request() for: %s!\n", strRequest);
		 sleep(10);
		 goto START_REQUEST;
         }
//	printf("%s\n", buff);

        printf("   --- Start slide images ---\n");

	/* Parse HTML */
	pstr=buff;

        char *strClassID="card progressive";

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
	printf("Orig imgURL: %s\n", imgURL);

#if 0	/* Replace to 1902x1080 */
	//char *pt=&imgURL[0];
	char *pt=(char *)imgURL;
	if(cstr_replace_string(&pt, sizeof(imgURL)-1-strlen(imgURL),"640x480", "1920x1080")==NULL)
		printf(DBG_RED"cstr_replace_string fails!\n"DBG_RESET);
#endif

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
	remove(TMP_IMAGE_FPATH);

#if 1   /* 4.1 Start thread preview */
	download_complete=false;
	printf(DBG_GREEN"Create thread_preview...\n"DBG_GREEN);
	if(pthread_create(&thread_preview, NULL, preview_image, NULL)!=0) {
		printf("Fail to create thread_preview!\n");
		exit(-1);
	}
	printf(DBG_GREEN"thread_preview created!\n"DBG_GREEN);
	sleep(1);
#endif


#ifdef PROGRESSIVE_JPG_URL
	imgURL[0]=0;
	strcpy(imgURL, PROGRESSIVE_JPG_URL);
#endif

	/* 4.2 Easy download image */
	//printf("Download %s...\n", imgURL);
	ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 5, 60,
			imgURL, TMP_IMAGE_FPATH, NULL, NULL);
	download_complete=true;

	/* Printf PICINFO */
	picInfo=egi_parse_jpegFile(TMP_IMAGE_FPATH);
	if(picInfo)
		egi_picinfo_print(picInfo);
	egi_picinfo_free(&picInfo);

#if 1	/* 4.3 wait thread preview */
	printf("Try to join thread_preview...\n");
	pthread_join(thread_preview, NULL);
#endif

	/* 4.2 Display image and text */
	if(ret==0) {
		printf("Succeed to download image from: k=%d, %s\n", k, imgURL);

		imgbuf=egi_imgbuf_readfile(TMP_IMAGE_FPATH);
		if(imgbuf==NULL) {
			printf(DBG_RED"Image NOT found!\n"DBG_RESET);
			goto START_REQUEST;
		}

		/* D0. Display with original image size, center_to_center to screen */
                int xp=0;
                int yp=0;
                int xw,yw;
	        int xres=gv_fb_dev.pos_xres;
        	int yres=gv_fb_dev.pos_yres;
                xw=imgbuf->width>xres ? 0:(xres-imgbuf->width)/2;
                yw=imgbuf->height>yres ? 0:(yres-imgbuf->height)/2;
                xp=imgbuf->width>xres  ? (imgbuf->width-xres)/2 : 0;
                yp=imgbuf->height>yres  ? (imgbuf->height-yres)/2 : 0;
                egi_imgbuf_windisplay2( imgbuf, &gv_fb_dev, xp, yp, xw, yw,
                                        imgbuf->width, imgbuf->height);
		fb_render(&gv_fb_dev);
		sleep(OrgTs);

		/* D1. Resize and display the image */
		int mw,mh,mtp;
		int step;
		float scale;
		int imgW=320; int imgH=240;

		egi_imgbuf_get_fitsize(imgbuf, &imgW, &imgH);

		/* Write fitup image to BKG_BUFF */
		EGI_IMGBUF *tmpimg=egi_imgbuf_resize(imgbuf, false, imgW,imgH);
		egi_subimg_writeFB(tmpimg, &gv_fb_dev, 0, -1, (320-imgW)/2, (240-imgH)/2 );
		fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);
		egi_imgbuf_free2(&tmpimg);

#if 1  /* Scale down step by step */
	  if(imgbuf->width>320) {
	     if(imgW==320) {  /* scale width */
		for(mw=imgbuf->width, mh=imgbuf->height, step=(mw-320)/10;
		    mw>=320;
		    mtp=mw, mw-=step, scale=1.0*mw/mtp, mh=scale*mh )
		{
			egi_imgbuf_resize_update(&imgbuf, false, mw, mh);
			//fb_clear_workBuff(&gv_fb_dev,WEGI_COLOR_DARKPURPLE); /* OK, for RGBA image as bkgcolor.*/
			/* imgbuf, fbdev, subnum, subcolor, x0,y0 */
			egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, (320-mw)/2, (240-mh)/2 );
			fb_render(&gv_fb_dev);
		}
	     }
	    //else if(imgH==240) /* scale height */
	  }
	  	/* Restore fitup image to WORKING_BUFF */
		fb_copy_FBbuffer(&gv_fb_dev,  FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);
		fb_render(&gv_fb_dev);

#else /* To fit with screen size */
		egi_imgbuf_resize_update(&imgbuf, false, imgW,imgH);
		if(avgLuma)
			egi_imgbuf_avgLuma(imgbuf, avgLuma);
		if(imgbuf) {
			fb_clear_workBuff(&gv_fb_dev,WEGI_COLOR_DARKPURPLE); /* OK, for RGBA image as bkgcolor.*/
			/* imgbuf, fbdev, subnum, subcolor, x0,y0 */
			egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, (320-imgW)/2, (240-imgH)/2 );
		}
		fb_render(&gv_fb_dev);
#endif
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
		   draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, (fh+4)*(ln-lnleft)+8, WEGI_COLOR_GRAYC, 160);
		   FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  	/* fbdev, face */
						fw, fh, (UFT8_PCHAR)imgTXT,   	/* fw,fh pstr */
						320-5, ln-lnleft, 4, 5, 5,      /* pixpl, lines, gap, x0,y0 */
						WEGI_COLOR_BLACK, -1, 250, 	/* fontcolor, transpcolor, opaque */
						NULL, NULL, NULL, NULL );	/* *cnt, *lnleft, *penx, *peny */
		}

		/* D4. Write Channel */
		if(channelName) {
		      int pixlen= FTsymbol_uft8strings_pixlen( egi_appfonts.bold, 20, 20, (UFT8_PCHAR)channelName); /* face,fw,fh,pstr */
		      draw_blend_filled_roundcorner_rect(&gv_fb_dev, 5, 240-5-30, 5+(pixlen+10), 240-5, 4, WEGI_COLOR_GRAYC, 180);
		      FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold, /* fbdev, face */
						20, 20, (UFT8_PCHAR)channelName,  /* fw,fh pstr */
						200, 1, 0, 5+10/2, 240-30-2, /* pixpl, lines, gap, x0,y0 */
						WEGI_COLOR_DARKBLUE, -1, 240, 	/* fontcolor, transpcolor, opaque */
						NULL, NULL, NULL, NULL ); 	/* *cnt, *lnleft, *penx, *peny */
		}
		fb_render(&gv_fb_dev);
		sleep(TxtTs);
	}
	/* 4.3 Fail to get image */
	else {
		printf("Fail to download image!\n");
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

/*-------------------------------------------
This is a thread function.
Try to parse and preview a (progressive)image
during downloading.
-------------------------------------------*/
void *preview_image(void *argv)
{
        int xp=0, yp=0;
        int xw=0, yw=0;
        int xres=gv_fb_dev.pos_xres;
        int yres=gv_fb_dev.pos_yres;
	EGI_IMGBUF *imgbuf2;

	egi_dpstd(DBG_GREEN"In thread ...\n"DBG_RESET);

	while(!download_complete)  {

		egi_dpstd(DBG_GREEN"read file ...\n"DBG_RESET);
		imgbuf2=egi_imgbuf_readfile(TMP_IMAGE_FPATH);
	        if(imgbuf2==NULL) {
			usleep(200000);
			continue;
        	}

		egi_dpstd(DBG_GREEN"Try to windisplay...\n"DBG_RESET);

		// #ifdef PROGRESSIVE_JPG_URL
	        /* Display with original image size, center_to_center to screen */
        	xw=imgbuf2->width>xres ? 0:(xres-imgbuf2->width)/2;
	        yw=imgbuf2->height>yres ? 0:(yres-imgbuf2->height)/2;
        	xp=imgbuf2->width>xres  ? (imgbuf2->width-xres)/2 : 0;
	        yp=imgbuf2->height>yres  ? (imgbuf2->height-yres)/2 : 0;
        	egi_imgbuf_windisplay2(imgbuf2, &gv_fb_dev, xp, yp, xw, yw,
                                    imgbuf2->width, imgbuf2->height);

	        fb_render(&gv_fb_dev);
		egi_imgbuf_free2(&imgbuf2);
	}

	return NULL;
}
