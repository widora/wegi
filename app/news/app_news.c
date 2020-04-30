/*--------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example for www.juhe.com https news interface.


Note:
1. HTTPS requests to get JUHE news list, then display item news one by one.
2. History will be saved as html text files.
3. Touch on news picture area to view the contents.
   Touch on news titles area to return to news picures.

Usage:	./test_juhe

Midas Zhou
--------------------------------------------------------------------------*/
#include <stdio.h>
#include <curl/curl.h>
#include <string.h>
#include <libgen.h>
#include <json-c/json.h>
#include <json-c/json_object.h>
#include <fcntl.h>
#include "egi_common.h"
#include "egi_https.h"
#include "egi_cstring.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"
#include "egi_gif.h"
#include "page_minipanel.h"
#include "juhe_news.h"
#include "app_news.h"


static char strkey[256];			/* for name of json_obj key */
static char buff[CURL_RETDATA_BUFF_SIZE]; 	/* for curl returned data, text/html expected */

#define THUMBNAIL_DISPLAY_SECONDS       5
#define CONTENT_DISPLAY_SECONDS         10
#define TOUCH_SLIDE_THRESHOLD           25	/* threshold of dx or dy value */

static bool Is_Saved_Html;                      /* If read from saved html, versus Is_Live_Html */

/* ZONE divisions
	     A 320x240 LCD in Landscape Mode

<------------------------ 320 -------------------------->|  <---------
 |			   |					    |
 |			   |				slide box   60
100	  Left Box     Mini Panel     Right Box			    |
 |			   |			<---------------------
 |			   |
<----------- 160 --------->|<----------- 160 ----------->|
 |			   |
 |			   |
 95	  Left Box	   |           Right Box
 |			   |
 |			   |
---------------------------------------------------------|
 |
 45			Title Box
 |
---------------------------------------------------------|
*/
/*   !!!---   In LCD touch pad coord.   ---!!!    */
EGI_BOX text_box={ {0, 0}, {240-45-1, 320-1} };	  	/* Text display zone  */
EGI_BOX right_box={ {0, 0}, {195, 160} };		/* Text display zone  */
EGI_BOX left_box={ {0, 160}, {195, 320-1} };	  	/* Text display zone  */
EGI_BOX title_box={ {240-45, 0}, {240-1, 320-1} }; 	/* Title display zone */
EGI_BOX minipanel_box={ {0, 0}, {100, 320-1} }; 	/* Mini panel zone */
EGI_BOX	slide_box={ {0, 0}, {60, 320-1} };		/* Sliding touch zone, for mini_panel activating */

/* Function */
static void display_newsFilo(EGI_FILO *filo);
static void free_newsFilo(EGI_FILO** filo);

void press_mark(EGI_POINT touch_pxy, int rad, uint8_t alpha, EGI_16BIT_COLOR color);
void ripple_mark(EGI_POINT touch_pxy, uint8_t alpha, EGI_16BIT_COLOR color);


/*------------------    www.juhe.cn  News Types     -----------------
   top(头条，默认),shehui(社会),guonei(国内),guoji(国际),yule(娱乐),
   tiyu(体育), junshi(军事),keji(科技),caijing(财经),shishang(时尚)
--------------------------------------------------------------------*/
static char* news_type[]=
{
   "top", "caijing", "keji", "guoji", "yule"
};


/*----------------------------
	    MAIN
-----------------------------*/
int main(int argc, char **argv)
{
	int i;
	int k;
	int err;
	int len;
	char *pstr=NULL;
	char *purl=NULL;		   /* Item news URL */
        static char strRequest[256+64];

	char *thumb_path="/tmp/thumb.jpg"; /* temprary thumbnail pic file */
	char pngNews_path[32];		   /* png file name for saving */
	char html_path[32];		   /* html file name for saving */
	int  error_code;		   /* error_code in returned JUHE json string */
	int  totalItems;		   /* total news items returned in one curl GET session */
	char attrMark[128];		   /* JUHE Mark string, to be shown on top of screen  */
	EGI_IMGBUF *imgbuf=NULL;
	EGI_IMGBUF *pad=NULL;

	EGI_TOUCH_DATA touch_data;	  /*  touch_data */
	EGI_TOUCH_DATA press_touch;	  /*  touch_data */
	EGI_FILO *news_filo=NULL;

#if 0
	juhe_fill_charBuff("/tmp/juhe_keji.html", buff, CURL_RETDATA_BUFF_SIZE);
	len=cstr_clear_unprintChars(buff, sizeof(buff));
// 	len=cstr_squeeze_string(buff, sizeof(buff), '\n');
	printf("pick out %d spots\n", len);
	juhe_save_charBuff("/tmp/juhe_keji.html",buff);
	exit(0);
#endif


#if 0 //////////////////
	char strbuff[64]="1234567890098765432155333322211";
	char spot;


	if( argc >1 )
		spot=argv[1][0];
	else
		spot='\0';

	printf("spot=%d\n",spot);
	printf("Original strbuff: %s\n",strbuff);

	strbuff[5]=spot;
	strbuff[8]=spot;
	strbuff[10]=spot;
	strbuff[11]=spot;
	strbuff[18]=spot;
	strbuff[28]=spot;
	strbuff[9]=spot;
	strbuff[13]=spot;
	strbuff[15]=spot;

	printf("sizeof strbuff=%zd\n", sizeof(strbuff));
	printf("Spotted strbuff: ");
	for(i=0; i<sizeof(strbuff); i++) {
		if(strbuff[i]==spot)
			printf("*");
		else
			printf("%c",strbuff[i]);
	}
	printf("\n");

 	len=cstr_squeeze_string(strbuff, sizeof(strbuff), spot);

	printf("Squeezed strbuff: %s\n",strbuff);
	printf("Pick out %d chars\n", len);

	exit(0);
#endif //////////////////


        /* <<<<< 	 EGI general init 	 >>>>>> */
#if 0
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */
#endif

#if 1
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/juhe.log") != 0) {        /* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
#endif

#if 0
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {       /* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
#endif
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;
#if 1
        printf("start touchread thread...\n");
        egi_start_touchread();                  /* start touch read thread */
#endif
        /* <<<<< 	 End EGI Init 	 >>>>> */

	/* Set screen view type as LANDSCAPE mode */
	fb_position_rotate(&gv_fb_dev, 0); //3

        /* Create a pad (int height, int width, unsigned char alpha, EGI_16BIT_COLOR color) */
        pad=egi_imgbuf_create(50, 320, 175, WEGI_COLOR_GYBLUE);

        /* read key from EGI config file */
        egi_get_config_value("JUHE_NEWS", "key", strkey);

	/* --- FOR TEST --- */
//	juhe_get_newsContent("http://mini.eastday.com/mobile/191125161256109.html");
//	exit(0);
//	char *picURL="http://03imgmini.eastday.com/mobile/20191126/20191126155447_532a561cb24ea6199e7809a77ea8c655_2_mwpm_03200403.jpg";
//      https_easy_download(picURL, "/tmp/easy_pic.jpg", NULL, download_callback);
//	exit(0);

while(1) { /////////////////////////	  LOOP TEST      ////////////////////////////

 for(k=0; k< sizeof(news_type)/sizeof(char *); k++ ) {
	/* Clear data buffer */
       	memset(buff,0,sizeof(buff));
	totalItems=0;

	/* Check whether type_0.png exists, from which to deduce that this type of news already downloaded */
	memset(pngNews_path,0,sizeof(pngNews_path));
	snprintf(pngNews_path, sizeof(pngNews_path),"/tmp/%s_0.png",news_type[k]);

	/* Prepare html_path for saving news_type[k] */
	memset(html_path,0,sizeof(html_path));
	snprintf(html_path, sizeof(html_path),"/tmp/juhe_%s.html",news_type[k]);

	/* IF:  history html file does NOT exist, then start Https GET request. */
	if( access(html_path, F_OK) !=0 )
	{
		/* prepare GET request string */
        	memset(strRequest,0,sizeof(strRequest));
	        strcat(strRequest,"https://v.juhe.cn/toutiao/index?type=");
		strcat(strRequest, news_type[k]);
	        strcat(strRequest,"&key=");
        	strcat(strRequest,strkey);

		printf("\n\n ------- [%s] News API powered by www.juhe.cn  ------ \n\n", news_type[k]);
        	printf("Request:%s\n", strRequest);

	        /* Https GET request */
	        if( (err=https_curl_request(strRequest, buff, NULL, curlget_callback)) != 0 ) {
        	        EGI_PLOG(LOGLV_ERROR,"%s: Fail to call https_curl_request()!",__func__);
			if(err==-3) /* content length < 0 */
			  EGI_PLOG(LOGLV_ERROR,"%s: Content length<0 for: %s",__func__, strRequest);
                	//return -1;  Go on....
	        }
        	printf("	--- Http GET Reply ---\n %s\n",buff);

		/* Squeeze buff */
 		len=cstr_squeeze_string(buff, sizeof(buff), '\n');
		if(len>0)
			EGI_PLOG(LOGLV_CRITICAL,"%s: squeeze out %d '\\n's in buff. \n",__func__,len);
 		len=cstr_squeeze_string(buff, sizeof(buff), '\r');
		if(len>0)
			EGI_PLOG(LOGLV_CRITICAL,"%s: squeeze out %d '\\r's in buff.",__func__,len);

		//len=cstr_clear_controlChars(buff,sizeof(buff)); /* This will damage UFT8 coding! */

		/* Checked return json string */
		error_code=juhe_get_errorCode(buff);
		if(error_code!=0) {
              	EGI_PLOG(LOGLV_ERROR,"%s: JUHE returned json string with error_code=%d for '%s'.",
									 __func__, error_code, news_type[k]);
			continue;
		}

		/* Get total number of news items */
		totalItems=juhe_get_totalItemNum(buff);
		EGI_PLOG(LOGLV_INFO,"%s: Http GET return total %d news items for '%s'",
								         __func__, totalItems, news_type[k]);
		if(totalItems <= 0) {
		        EGI_PLOG(LOGLV_ERROR,"%s: Http GET return total %d news items for '%s'",
							                 __func__, totalItems, news_type[k]);
			continue; /* continue for(k) */
		}

		/* save buff to a html file */
		if( juhe_save_charBuff(html_path, buff) <= 0 ) {
			EGI_PLOG(LOGLV_ERROR,"%s: Fail to save buff to '%s'.",__func__, html_path);
			continue; /* continue for(k) */
		}

		/* Set flag */
		Is_Saved_Html=false;

	/* ELSE:  history html file already exists, then read it to buff. */
	} else {
		printf("\n\n ------- News type [%s] already downloaded  -----\n ", news_type[k]);

		/* Load saved html to buff */
		EGI_PLOG(LOGLV_INFO,"%s: Reload html file %s to buff ...", __func__, news_type[k]);
	 	if( juhe_fill_charBuff(html_path, buff, sizeof(buff)) <=0 ) {
			EGI_PLOG(LOGLV_ERROR,"%s: Fail to read '%s' to fill buff.",
										__func__, html_path);
			continue; /* continue for(k) */
		}

		totalItems=juhe_get_totalItemNum(buff);
		EGI_PLOG(LOGLV_INFO,"%s -----  Html file reload total %d news items ----- ",
										__func__, totalItems);
		if(totalItems <= 0) {
			continue; /* continue for(k) */
		}

		/* Set flag */
		Is_Saved_Html=true;
	}

	//  totalItems==0 have been ruled out!
   	/* Get top N items for each type of news */
//	if(totalItems==0) {	/* if 0, then try to use saved images */
//		totalItems=30;
//		EGI_PLOG(LOGLV_WARN,"%s: TotalItems is 0!",__func__);
//	}

	/* Get news thumbnai picture and content item by item */
	for(i=0; i<totalItems; i++) {
		/* clear FB back buff for rewrite */
		fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_BLACK);

		/* Set PNG news picture name string */
		memset(pngNews_path,0,sizeof(pngNews_path));
		snprintf(pngNews_path, sizeof(pngNews_path),"/tmp/%s_%d.png",news_type[k],i);

		/* --- Get thumbnail pic URL and download it --- */
		/* Get thumbnail URL */
		pstr=juhe_dupGet_elemValue(buff,i,"thumbnail_pic_s");
		if(pstr == NULL) {
		    #if 1
		    printf("News type [%s] item[%d]: thumbnail URL not found, try next item...\n",
											news_type[k], i );
		    continue;	/* continue for(i) */
		    #else
		    printf("News type [%s] item[%d]: thumbnail URL not found, skip to next news type...\n",
											news_type[k], i );
		    break;	/* Jump out of for(i) */
		    #endif
		}

		/* Download thumbnail pic
		 * !!!WARNING!!!: https_easy_download may fail, if thumb_path is NOT correct.
		 */
		EGI_PLOG(LOGLV_INFO,"%s:  --- Start https easy download thumbnail pic --- URL: %s",
											__func__, pstr);
		if( https_easy_download(pstr, thumb_path, NULL, download_callback) !=0 ) {
	                EGI_PLOG(LOGLV_ERROR,"%s: Fail to easy_download %s[%d] '%s'.\n",
									 __func__, news_type[k], i, pstr);
			egi_free_char(&pstr);
               		continue; /* Continue for(i) */

			/* Note: If fails, it will slip to next item, and for next item, it will
			 *       always stuck if you press/touch to move back.
			 */

		}
		egi_free_char(&pstr);

        	/* read in the thumbnail pic file */
		EGI_PLOG(LOGLV_INFO,"%s: Read thumb_path into imgbuf...",__func__);
        	imgbuf=egi_imgbuf_readfile(thumb_path);
       		if(imgbuf==NULL) {
	                printf("Fail to read image file '%s'.\n", thumb_path);
               		continue; /* Continue for(i) */
	        }


		#if 0 /* Not necessary for Landscape mode */
		/* rotate the imgbuf */
		egi_imgbuf_rotate_update(&imgbuf, 90);
		/* resize */
		egi_imgbuf_resize_update(&imgbuf, 240,320);
		#endif

	        /* display the thumbnail  */
	        egi_imgbuf_windisplay( imgbuf, &gv_fb_dev, -1,         /* img, FB, subcolor */
               		                0, 0,                            /* int xp, int yp */
                       			0, 0, imgbuf->width, imgbuf->height   /* xw, yw, winw,  winh */
                       		      );

		/* display pad for words writing */
	        egi_imgbuf_windisplay(  pad, &gv_fb_dev, -1,         /* img, FB, subcolor */
               		                0, 0,                            /* int xp, int yp */
                       			0, 240-45, imgbuf->width, imgbuf->height   /* xw, yw, winw,  winh */
                       		      );

		/* draw a red line at top of pad */
		#if 0
		fbset_color(WEGI_COLOR_ORANGE);
		draw_wline_nc(&gv_fb_dev, 0, 240-45, 320-1, 240-45, 1);
		#endif

		/* Free the imgbuf */
	        egi_imgbuf_free(imgbuf);imgbuf=NULL;

		/* --- Get news title and display it --- */
		pstr=juhe_dupGet_elemValue(buff, i, "title");
		if(pstr==NULL) {
			#if 1
			printf("News type [%s] item[%d]: Title not found, try next item...\n",
											news_type[k], i );
			continue;	/* continue for(i) */
			/* Note: If NOT found, it will slip to next item, and for next item, it will
			 *       always stuck if you press/touch to move back.
			 */

			#else
			printf("News type [%s] item[%d]: Title not found, skip to next news type...\n",
											news_type[k], i );
			break;  	/* Jump out of for(i) */
			#endif
		}

		/* Prepare attribute mark string */
		memset(attrMark, 0, sizeof(attrMark));
		sprintf(attrMark, "%s_%d:  ",news_type[k], i);
		strcat(attrMark,"Powered by www.juhe.cn");

        	FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.regular,     /* FBdev, fontface */
                                     16, 16, (const unsigned char *)attrMark, 	   /* fw,fh, pstr */
                                     320-10, 1, 5,                       /* pixpl, lines, gap */
                                     5, 0,          /* x0,y0, */
                                     WEGI_COLOR_GRAY, -1, 255,  /* fontcolor, transcolor,opaque */
				     NULL, NULL, NULL, NULL);

        	FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.bold,     /* FBdev, fontface */
                                     15, 15, (const unsigned char *)pstr,      /* fw,fh, pstr */
                                     320-10, 3, 5,   //240-10,3,5          /* pixpl, lines, gap */
                                     5, 240-45+5,      //5,320-75,          /* x0,y0, */
                                     WEGI_COLOR_WHITE, -1, 255,  /* fontcolor, transcolor,opaque */
				     NULL, NULL, NULL, NULL);

		printf(" ------From %s:  %s News, Item %d ----- \n",
				 	(Is_Saved_Html==true)?"saved html":"live html", news_type[k], i);
		printf(" Title:	 %s\n", pstr);
		egi_free_char(&pstr);

		/* Refresh FB page */
		printf("FB page refresh ...\n");
		//fb_page_refresh(&gv_fb_dev);
		fb_page_refresh_flyin(&gv_fb_dev, 40);

		/* save FB data to a PNG file */
		//egi_save_FBpng(&gv_fb_dev, pngNews_path);

#if 0
if(i==5) { //////////////////////////// INSERT GIF  //////////////////////

        int xres=gv_fb_dev.pos_xres;
        int yres=gv_fb_dev.pos_yres;

        /* init FB back ground buffer page */
        memcpy(gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.map_fb, gv_fb_dev.screensize);

        /*  init FB working buffer page */
        memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_fb, gv_fb_dev.screensize);

        /* read in GIF data to EGI_GIF */
        EGI_GIF *egif= egi_gif_slurpFile( "/mmc/happyny.gif", true); /* fpath, bool ImgTransp_ON */
        if(egif==NULL) {
                printf("Fail to read in gif file!\n");
                exit(-1);
        }
        printf("Finishing read GIF.\n");

        /* Cal xp,yp xw,yw */
        int xp=egif->SWidth>xres ? (egif->SWidth-xres)/2:0;
        int yp=egif->SHeight>yres ? (egif->SHeight-yres)/2:0;
        int xw=egif->SWidth>xres ? 0:(xres-egif->SWidth)/2;
        int yw=egif->SHeight>yres ? 0:(yres-egif->SHeight)/2;


        /* Loop displaying */
//        while(1) {

            /* Display one frame/block each time, then refresh FB page.  */
            egi_gif_displayFrame( &gv_fb_dev, egif, -1,  /* *fbdev, EGI_GIF *egif, nloop */
                                 /* DirectFB_ON, User_DispMode, User_TransColor, User_BkgColor
                                  * If User_TransColor>=0, set User_DispMode to 2 as for transparency.
                                  */
				 false,-1,-1,-1,
                                 /* to put center of IMGBUF to the center of LCD */
                                 xp,yp,xw,yw,                   /* xp,yp, xw, yw */
                                egif->SWidth>xres ? xres:egif->SWidth,          /* winw */
                                egif->SHeight>yres ? yres:egif->SHeight         /* winh */
                        );
//        }
        egi_gif_free(&egif);

}  ////////////////////////////////////////////////////////////
#endif

		/*** 		Hold on and wait .....
		 *  If the reader touch the screen within xs, then display the item content,
		 *  otherwise skip to next title show when time is out.
		 */
		tm_start_egitick();
		if( egi_touch_timeWait_press(THUMBNAIL_DISPLAY_SECONDS,0, &press_touch)==0 ) {
			printf(" >>>>>  Press touching pxy(%d,%d)\n",
						press_touch.coord.x, press_touch.coord.y);

			/* 1. If press/touch title_box */
			if( point_inbox(&press_touch.coord, &title_box) ) {
				press_mark(press_touch.coord,40,135,WEGI_COLOR_ORANGE);
				purl=juhe_dupGet_elemValue(buff, i, "url");
				printf("New item[%d] URL: %s\n", i, purl);
				EGI_PLOG(LOGLV_INFO,"%s: Start juhe_get_newsContent()...",__func__);

				/* Get news content and display it in text zone */
				news_filo=juhe_get_newsFilo(purl);
				display_newsFilo(news_filo);
				free_newsFilo(&news_filo);

				/*free duped char */
				egi_free_char(&purl);
				continue;
			}

                        /* 2. If press/touch slide_box area -- AND -- sliding to pull down mini panel */
			else if( point_inbox(&press_touch.coord, &slide_box) )  //minipanel_box) )
			{
				press_mark(press_touch.coord,40,135,WEGI_COLOR_ORANGE);
				if( egi_touch_timeWait_release(3,0,&touch_data)==0
				    		&& touch_data.dx > TOUCH_SLIDE_THRESHOLD )
				{
					printf("--- dx=%d, dy=%d --- \n", touch_data.dx, touch_data.dy);
					/* trap into mini panel routine */
	                                create_miniPanel();
					fb_render(&gv_fb_dev);
					miniPanel_routine();
                        	        free_miniPanel();

					/*free duped char */
					//egi_free_char(&purl);

					continue;
				}
				printf("%s: <<<<<  timewait release fails!	>>>> \n",__func__);
                        }

			/* 1. If press/touch left_box. Move back 1 item... */
			if( point_inbox(&press_touch.coord, &left_box) )  /* Move back */
			{
				press_mark(press_touch.coord, 40, 180, WEGI_COLOR_RED);
				if(i > 0)   	/* move back 1 item */
					i-=2;
				else if(i==0)
					i=-1;
			}
			/* 2. Else if press/touch right_box,  Move forward... */
			else // ( point_inbox(&press_touch.coord, &right_box) )  /* Move forward */
				press_mark(press_touch.coord, 40, 180, WEGI_COLOR_RED);

			/*free duped char */
			// egi_free_char(&purl);
			/* ---> GO TO NEXT NEWS ITEM[i] */
		} /* END touch timeWait */
	} /* END for(i) */
 } /* END for(k) */


} //////////////////////////      END LOOP  TEST      ///////////////////////////

	/* free imgbuf */
	egi_imgbuf_free(pad);
	egi_imgbuf_free(imgbuf);

        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
        printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev); /* Flush FB filo if necessary */
        release_fbdev(&gv_fb_dev);
        printf("egi_end_touchread()...\n");
        egi_end_touchread();
#if 0
        printf("egi_quit_log()...\n");
        egi_quit_log();
#endif
        printf("<-------  END  ------>\n");

	return 0;
}





/*--------------------------------------------------
Display news paragraph contents hold in address
pushed in the FILO.

@filo:	An EGI_FILO holds addresses of all contents
	item by item.


-------------------------------------------------*/
static void display_newsFilo(EGI_FILO *filo)
{
	int i;

        int len;
        char *content=NULL;		   /* Pointer to content of a piece of news */
        char *pstr=NULL;		   /* Pointer to file OR buff */
	int nitems;			   /* items in filo */
	int nwritten=0;

	EGI_TOUCH_DATA	touch_data;	   /* touch data. */

	/* For UFT-8 writeFB func. */
	int fw=18, fh=18;  	/* font width and height */
	int lngap=6;		/* line gap */
	int pixpl=320-10;  	/* pixels per line */
	int wlines=(240-45)/(fh+lngap);	/* total lines for writing */
	int cnt=0;		/*  character count */
	int lnleft;		/* lines unwritten */
	int penx0=5, peny0=4;	/* Starting pen positio */
	int penx, peny;		/* Curretn pen position */

	/* init */
	penx=penx0;
	peny=peny0;
	lnleft=wlines;


	if(filo==NULL)
		return;

	/* Clear displaying zone */
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYC, 0, 0, 320-1, 240-45-1);

	/* Read FILO and get pointer to paragraph content, then display it. */
	nitems=egi_filo_itemtotal(filo);
	for(i=0; egi_filo_read(filo, i, &content)==0; i++ )
	{
		printf("%s: ----From %s: writeFB paragraph %d -----\n",
				 	   __func__, (Is_Saved_Html==true)?"saved html":"live html", i);
		if(content==NULL)
			printf("%s:---------- content is NULL ---------\n",__func__);

		printf("content[%d/%d]: %s\n", i, nitems-1, content);

		/* Pointer to content and length */
		pstr=content;
		len=strlen(content);

		/* Write paragraph content to FB, it may need several displaying pages */
		do {
			/* write to FB back buff */
			printf("FTsymbol_uft8strings_writenFB...\n");
        		nwritten=FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.bold,       /* FBdev, fontface */
                        	             fw, fh, (const unsigned char *)pstr,   /* fw,fh, pstr */
                                	     pixpl, lnleft, lngap,     	    	    /* pixpl, lines, gap */
	                                     penx, peny,      //5,320-75,     	    /* x0,y0, */
        	                             WEGI_COLOR_BLACK, -1, 255,  /* fontcolor, transcolor, opaque */
					     &cnt, &lnleft, &penx, &peny);   /* &cnt, &lnleft, &penx, &peny */

			printf("UFT8 strings writeFB: cnt=%d, lnleft=%d, penx=%d, peny=%d\n",
										cnt,lnleft,penx,peny);
			if(nwritten>0)
				pstr+=nwritten; /* move pointer forward */

			/* If text box if filled up OR all FILO is empty, refresh it */
			if( lnleft==0 ||  i==nitems-1 ) {  //&& len-nwritten<=0 ) {
				/* refresh FB */
				printf("%s:  written=%d, refresh fb...\n", __func__, nwritten);
				fb_render(&gv_fb_dev);

				/* refresh text box */
				printf("\n	---->>>  Refresh Text BOX  <<<----- \n");
				lnleft=wlines;
				penx=penx0; peny=peny0;

				/* If touch screen within Xs, then end the function */
				tm_start_egitick();
				if( egi_touch_timeWait_press(CONTENT_DISPLAY_SECONDS, 0,&touch_data)==0 ) {
					//press_mark(touch_data.coord, 40, 135, WEGI_COLOR_ORANGE);
					ripple_mark(touch_data.coord, 135, WEGI_COLOR_ORANGE);
					printf("Touch tpxy(%d,%d)\n", touch_data.coord.x, touch_data.coord.y);
					/* If hit title box, then return */
					if( point_inbox(&touch_data.coord, &title_box) )
						return;

					/* else, continue news content displaying */

				}

				/* Clear displaying zone, only */
				draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAYC, 0, 0, 320-1, 240-45-1);
			}
		} while( nwritten>0 && (len -= nwritten)>0 );

	} /* End read filo and display content */

}


/*------------------------------------
Free all contents pointed by addresses
pushed in the filo, then free the
EGI_FILO struct.
------------------------------------*/
static void free_newsFilo(EGI_FILO** filo)
{
	char* content=NULL;

	if( filo==NULL || *filo==NULL )
		return;

	/* Free all content pointers in FILO  */
	while( egi_filo_pop(*filo, &content)==0 )
		free(content);

	/* Free FILO */
	egi_free_filo(*filo);

	*filo=NULL;
}


/*----------------------------------------------------
Draw a filled circle as a  water mark.

@rad:		radius of the mark
@alpha:		Alpha value for color blend
@touch_pxy:	Point coordinate
------------------------------------------------------*/
void press_mark(EGI_POINT touch_pxy, int rad, uint8_t alpha, EGI_16BIT_COLOR color)
{
	/* Turn on FB filo and set map pointer */
        fb_filo_on(&gv_fb_dev);
	gv_fb_dev.map_bk=gv_fb_dev.map_fb;

	draw_blend_filled_circle( &gv_fb_dev,
				  320-touch_pxy.y, touch_pxy.x,
				  rad, color, alpha); /* r,color,alpha  */

	tm_delayms(100);
	fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */

	/* Turn off FB filo and reset map pointer */
	gv_fb_dev.map_bk=gv_fb_dev.map_buff;
	fb_filo_off(&gv_fb_dev);
}



/* ----------------------------------------------------------------------
Draw a water ripple mark.

@alpha:		Alpha value for color blend
@touch_pxy:	Point coordinate
---------------------------------------------------------------------- */
void ripple_mark(EGI_POINT touch_pxy, uint8_t alpha, EGI_16BIT_COLOR color)
{
	int i;
	int rad[]={ 20, 30, 40, 50, 60, 70, 80 };
	int wid[]={ 9, 7, 7, 5, 5, 3, 3 };

   for(i=0; i<sizeof(rad)/sizeof(rad[0]); i++)
   {
	/* Turn on FB filo and set map pointer */
        fb_filo_on(&gv_fb_dev);
	gv_fb_dev.map_bk=gv_fb_dev.map_fb;

   	draw_blend_filled_annulus(&gv_fb_dev,
			    320-touch_pxy.y, touch_pxy.x,
			    rad[i], wid[i], color, alpha );	/* radius, width, color, alpha */
	tm_delayms(65);
	fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */

	/* Turn off FB filo and reset map pointer */
	gv_fb_dev.map_bk=gv_fb_dev.map_buff;
	fb_filo_off(&gv_fb_dev);
   }

}
