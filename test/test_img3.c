/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test image rotation functions.

Usage:
	./test_img3  file

Journal:
2021-06-07:
	1. Test egi_imgbuf_scale().


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
  	fb_set_directFB(&gv_fb_dev,false);
  	fb_position_rotate(&gv_fb_dev, 0);

 /* <<<<<  End of EGI general init  >>>>>> */



#if 0 ///////////////////////
fb_set_directFB(&gv_fb_dev,false);
fb_position_rotate(&gv_fb_dev,0);

if(argc<3) exit(1);
EGI_IMGBUF* origimg=egi_imgbuf_readfile(argv[1]);
if(origimg==NULL)exit(1);

EGI_IMGBUF* myimg=egi_imgbuf_resize(origimg, true, 320, 240);
if(myimg==NULL)exit(1);

egi_imgbuf_windisplay2( myimg, &gv_fb_dev,
                        0,0,0,0,320,240 );     /* xp,yp,xw,yw,w,h */
fb_render(&gv_fb_dev);

egi_imgbuf_free2(&origimg);
egi_imgbuf_free2(&myimg);
exit(0);
#endif ////////////////////////




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

//const wchar_t *wstr2=L"奔跑在WIDORA上的小企鹅";
const wchar_t *wstr2=L"转圈缩放的LINUX小企鹅";

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


#if 1    /* ---------     Test egi_imgbuf_scale()    --------- */
	EGI_IMGBUF *tmpimg;
	int sw, sh;
	char strtmp[256];
	s=2;
	delt=1;
do {

	if( s >= 300 && delt >0 )
		delt=-delt;
	else if(s < 2 && delt <0 )
		delt=-delt;

	s+=delt;
	sw=round(1.0*eimg->width*s/200);
	sh=round(1.0*eimg->height*s/200);
	printf("sw=%d, sh=%d\n", sw, sh);
	sprintf(strtmp,"Merge down: %0.2f", 1.0*eimg->width/sw);

	tmpimg=egi_imgbuf_scale( eimg, sw, sh);

	x0=round(1.0*(320-tmpimg->width)/2);
	y0=round(1.0*(240-tmpimg->height)/2);

	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);
	egi_imgbuf_windisplay2( tmpimg, &gv_fb_dev,  		/* imgbuf, fb_dev */
                                0, 0, x0, y0,         		/* xp, yp, xw, yw */
				tmpimg->width, tmpimg->height); /* winw, winh */
        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.bold,       /* FBdev, fontface */
                                        20, 20, (const UFT8_PCHAR)strtmp, /* fw,fh, pstr */
                                        320, 1, 0,                        /* pixpl, lines, gap */
                                        10, 10,                          /* x0,y0, */
                                        WEGI_COLOR_RED, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
	fb_render(&gv_fb_dev);

	egi_imgbuf_free2(&tmpimg);

	if( s<=30 ) {
		tm_delayms(150);
		if(delt==-4)
			delt =-1;
	}
	else if( s<=50 ) {
		tm_delayms(150);
		if( delt==1 )
			delt =4;
		else if(delt==-8)
			delt =-4;
	}
	else if( s<=200 ) {
		tm_delayms(50);
		if(delt==4)
			delt = 8;
	}

} while(1);


#endif



#if 0    /* ---------     Test egi_imgbuf_rotBlockCopy()    --------- */

  	fb_set_directFB(&gv_fb_dev,false);
//	rotimg=egi_imgbuf_createWithoutAlpha(100,150,WEGI_COLOR_BLACK);
//	rotimg=egi_imgbuf_create(101,151,255,0);
	i=0;

  while(1) {
	i+=2;
	if(i>360)i=0;

	fb_clear_backBuff(&gv_fb_dev,WEGI_COLOR_GRAY3);

	rotimg=egi_imgbuf_rotBlockCopy(eimg, NULL, 240, 320, 0,0, i); //zeimg->width/2, eimg->height/2,i); /* img, oimg, height, width, px,py, angle */
	if(rotimg==NULL)printf("fail to rotBlockCopy!\n");
       	egi_imgbuf_windisplay( rotimg, &gv_fb_dev, -1,		 		    /* img, fb, subcolor */
                       	       0, 0, (320-rotimg->width)/2, (240-rotimg->height)/2, /* xp,yp  xw,yw */
               	               rotimg->width, rotimg->height);	 		    /* winw, winh */
	fb_render(&gv_fb_dev);

	tm_delayms(50);
	egi_imgbuf_free2(&rotimg);
  }


#endif

#if 0    /* ---------     Test egi_imgbuf_flipY()    --------- */
	int sk=0;
  	fb_set_directFB(&gv_fb_dev,false);

  while(1) {
	sk=!sk;
	fb_clear_backBuff(&gv_fb_dev,WEGI_COLOR_GRAY3);

   #if 0
	if(sk)
		egi_imgbuf_flipY(eimg);
	else
		egi_imgbuf_centroSymmetry(eimg);
  #endif
	egi_imgbuf_flipX(eimg);

       	egi_imgbuf_windisplay( eimg, &gv_fb_dev, -1,		 		    /* img, fb, subcolor */
                       	       0, 0, (320-eimg->width)/2, (240-eimg->height)/2, /* xp,yp  xw,yw */
               	               eimg->width, eimg->height);	 		    /* winw, winh */
	fb_render(&gv_fb_dev);

	tm_delayms(500);
  }

#endif


#if 0    /* ---------     Test egi_imgbuf_rotate()    --------- */
  	fb_set_directFB(&gv_fb_dev,false);

	EGI_IMGBUF* eimgbk=NULL;
	eimgbk=egi_imgbuf_readfile(argv[2]);
	if(eimgbk!=NULL)
        	egi_imgbuf_windisplay( eimgbk, &gv_fb_dev, -1,		 		    /* img, fb, subcolor */
                        	       0, 0, (320-eimgbk->width)/2, (240-eimgbk->height)/2, /* xp,yp  xw,yw */
                	               eimgbk->width, eimgbk->height);	 		    /* winw, winh */

	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */

	i=-360;
	//i=0;
while(1) {
	i+=5;
	if(i>360) i=-360;
	//if(i>360)i=0;
	printf("i=%d\n",i);

	#if 0
        /* Turn on FB filo and set map pointer */
        fb_filo_on(&gv_fb_dev);
        fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
	#endif

	//fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_DARKRED);
	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */

	/* indicator */
	//egi_image_rotdisplay( eimg, &gv_fb_dev, i, 20, 150, 320/2, 190 );   /* img, fbdev, angle, xri,yri,  xrl,yrl */
	egi_image_rotdisplay( eimg, &gv_fb_dev, i, eimg->width/2, eimg->height/2, 320/2, 240/2 );   /* img, fbdev, angle, xri,yri,  xrl,yrl */
//	egi_image_rotdisplay( eimg, &gv_fb_dev, i, eimg->width/2, 150, 320/2, 240/2 );   /* img, fbdev, angle, xri,yri,  xrl,yrl */

	fb_render(&gv_fb_dev);

	//usleep(120000);
	//egi_sleep(1,0,80);
	tm_delayms(50);

	#if 0
        /* Turn off FB filo and reset map pointer */
        fb_filo_off(&gv_fb_dev);
	#endif
}

	egi_imgbuf_free2(&eimg);
	egi_imgbuf_free2(&eimgbk);
#endif


#if 1    /* ---------     Test egi_imgbuf_rotate(): For 320Wx240H screen    --------- */
do {
	i+=5;

	/* restore FB data */
//	fb_restore_FBimg(&gv_fb_dev, 0, false);
//	clear_screen(&gv_fb_dev, WEGI_COLOR_GRAY3);
	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);

        /* 2. Create rotated imgbuf */
	rotimg=egi_imgbuf_rotate(eimg, i);

	/* 2. Scale the imgbuf (**pimg, width, height) */
	if( s > 200 && delt >0 )
		delt=-1;
	else if(s < 50 && delt <0 )
		delt=1;

	s+=delt;
	egi_imgbuf_resize_update( &rotimg, true, rotimg->width*s/200, rotimg->height*s/200);

	/* 3. Display the image */
	x0=(320-rotimg->width)/2;
	y0=(240-rotimg->height)/2;

	#if 0 /* TEST: egi_imgbuf_windisplay() */
	printf("windisplay...\n");
        egi_imgbuf_windisplay( rotimg, &gv_fb_dev, -1,		 		/* img, fb, subcolor */
                               0, 0, x0, y0,					/* xp,yp  xw,yw */
                               rotimg->width, rotimg->height);	 		/* winw, winh */

	#else /* TEST: egi_imgbuf_windisplay2() */
	egi_imgbuf_windisplay2( rotimg, &gv_fb_dev,  		/* imgbuf, fb_dev */
                                0, 0, x0, y0,         		/* xp, yp, xw, yw */
				rotimg->width, rotimg->height); /* winw, winh */
	#endif


        /* Comments */
#if 0
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          32, 32, wstr1,  		    /* fw,fh, pstr */
                                          320, 2, 10,                    /* pixpl, lines, gap */
                                          0, 200,                      	    /* x0,y0, */
                                          WEGI_COLOR_RED, -1, 255 );   /* fontcolor, transcolor,opaque */
#endif

        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          24, 24, wstr2,  		    /* fw,fh, pstr */
                                          320, 6,  10,                    /* pixpl, lines, gap */
                                          30, 20,                        /* x0,y0, */
                                          WEGI_COLOR_ORANGE, -1, 255 );   /* fontcolor, transcolor,opaque */
	fb_render(&gv_fb_dev);

	/* 4. Free rotimgs */
	egi_imgbuf_free2(&rotimg);

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


