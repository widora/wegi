/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test image rotation functions.

Usage:
	./test_img3  file

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_pcm.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"

int main(int argc, char** argv)
{
	int i,j,k;
	int ret;

	struct timeval tm_start;
	struct timeval tm_end;

        /* <<<<<  EGI general init  >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();		   	/* start sys tick */
#if 0
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_test") != 0) {	/* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {   	/* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
#endif
        if(FTsymbol_load_appfonts() !=0 ) {  	/* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

  	/* Initilize sys FBDEV */
  	printf("init_fbdev()...\n");
  	if(init_fbdev(&gv_fb_dev))
        	return -1;

  	/* Start touch read thread */
#if 0
  	printf("Start touchread thread...\n");
  	if(egi_start_touchread() !=0)
        	return -1;
#endif
  	/* Set sys FB mode */
  	fb_set_directFB(&gv_fb_dev,true);
  	fb_position_rotate(&gv_fb_dev,3);

 /* <<<<<  End of EGI general init  >>>>>> */




#if 0 ///////////////////////////////////////////////////////
EGI_IMGBUF *test_img=egi_imgbuf_create(50, 180, 0, WEGI_COLOR_RED);/* H,W, alpha ,color */
printf("(-30)%%360=%d;  (-390)%%360=%d;  30%%(-360)=%d;  390%%(-360)=%d \n",
					(-30)%360, (-390)%360, 30%(-360), 390%(-360) );
for(i=0; i<361; i+=10)
{
	egi_imgbuf_rotate(test_img, i);
}
exit(0);
#endif ///////////////////////////////////////////////////////



EGI_IMGBUF* eimg=NULL;
EGI_IMGBUF* rotimg=NULL;
int x0,y0;

char**	fpaths=NULL;	/* File paths */
int	ftotal=0; 	/* File counts */
int	num=0;		/* File index */

int	s=50;		/* size */
int 	delt=1;		/* incremental delta */

const wchar_t *wstr1=L"   a mini. EGI";

const wchar_t *wstr2=L"奔跑在WIDORA上的\n	\
             小企鹅";

        if(argc<2) {
                printf("Usage: %s file\n",argv[0]);
                exit(-1);
        }

        /* 1. Load pic to imgbuf */
	eimg=egi_imgbuf_readfile(argv[1]);
	if(eimg==NULL) {
        	EGI_PLOG(LOGLV_ERROR, "%s: Fail to read and load file '%s'!", __func__, argv[1]);
		return -1;
	}


#if 1    /* ---------     Test egi_imgbuf_rotate()    --------- */
  	fb_set_directFB(&gv_fb_dev,false);

while(1) {
	i+=1;
	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);
	egi_image_rotdisplay( eimg, &gv_fb_dev, i, 0, eimg->height, 320/2, 240/2 );   /* img, fbdev, angle, xri,yri,  xrl,yrl */
	fb_render(&gv_fb_dev);
	//usleep(120000);
	//egi_sleep(1,0,80);
	tm_delayms(60);
}

#endif



#if 1    /* ---------     Test egi_imgbuf_rotate()    --------- */

do {
	i+=10;

	/* restore FB data */
//	fb_restore_FBimg(&gv_fb_dev, 0, false);
	clear_screen(&gv_fb_dev, WEGI_COLOR_GRAY3);
//	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);

        /* 2. Create rotated imgbuf */
	rotimg=egi_imgbuf_rotate(eimg, i);

	/* 2. Scale the imgbuf (**pimg, width, height) */
	if( s > 150 && delt >0 )
		delt=-1;
	else if(s < 50 && delt <0 )
		delt=1;

	s+=delt;
	egi_imgbuf_resize_update( &rotimg, rotimg->width*s/200, rotimg->height*s/200);


	/* 3. Display the image */
	x0=(240-rotimg->width)/2;
	y0=(320-rotimg->height)/2;

	#if 0 /* TEST: egi_imgbuf_windisplay() */
        egi_imgbuf_windisplay( rotimg, &gv_fb_dev, -1,		 		/* img, fb, subcolor */
                               0, 0, x0, y0,					/* xp,yp  xw,yw */
                               rotimg->width, rotimg->height);	 		/* winw, winh */

	#else /* TEST: egi_imgbuf_windisplay2() */
	egi_imgbuf_windisplay2( rotimg, &gv_fb_dev,  		/* imgbuf, fb_dev */
                                0, 0, x0, y0,         		/* xp, yp, xw, yw */
				rotimg->width, rotimg->height); /* winw, winh */
	#endif


        /* Comments */
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          32, 32, wstr1,  		    /* fw,fh, pstr */
                                          240, 6, 10,                    /* pixpl, lines, gap */
                                          0, 200,                      	    /* x0,y0, */
                                          WEGI_COLOR_BLACK, -1, 255 );   /* fontcolor, transcolor,opaque */

        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          24, 24, wstr2,  		    /* fw,fh, pstr */
                                          240, 6,  10,                    /* pixpl, lines, gap */
                                          0, 50,                        /* x0,y0, */
                                          WEGI_COLOR_WHITE, -1, 255 );   /* fontcolor, transcolor,opaque */


	/* 4. Free rotimgs */
	egi_imgbuf_free(rotimg);
	rotimg=NULL;

	/* 5. Refresh FB by memcpying back buffer to FB */
//	fb_page_refresh(&gv_fb_dev,0);

	/* 6. Clear screen and increase num */
	usleep(55000);
	//tm_delayms(100);
	//clear_screen(&gv_fb_dev, WEGI_COLOR_BLACK);


} while(1); ///////////////////////////   END LOOP TEST   ///////////////////////////////

	/* Free eimg */
	egi_imgbuf_free(eimg);
	eimg=NULL;

#endif  /* ---------  END test  egi_imgbuf_rotate()    --------- */






	#if 0
        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
        printf("egi_quit_log()...\n");
        egi_quit_log();
        printf("<-------  END  ------>\n");
	#endif

return 0;
}


