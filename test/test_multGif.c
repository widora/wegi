/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to operate and display EGI_GIF with mutli_threads.

Note:
1. The base layer GIF for displaying shall be nontransparent type or
   set ImgTransp_ON to false.

Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <pthread.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_gif.h"


const wchar_t *wslogan=L"EGI: A mini UI Powered by Openwrt";


typedef struct gif_thread_params
{
	const char *fname;
	EGI_GIF *egif;
	bool  DirectFB_ON; /* This will NOT affect gif.Simgbuf */
	bool ImgTransp_ON;
	int  x0,y0;	   /* gif->Simgbuf origin coordinates relating to LCD */
	int  xp,yp;

} GIF_PARAM;


static GIF_PARAM gif_param[4] =
{
//	{ .fname="/mmc/cloud.gif", .DirectFB_ON=false, .ImgTransp_ON=true, .x0=-100, .y0=-80 }, /* 507x436 */
//	{ .fname="/mmc/chuying2.gif", .DirectFB_ON=false, .ImgTransp_ON=true, .x0=0, .y0=-40 },
	{ .fname="/mmc/runner.gif", .DirectFB_ON=false, .ImgTransp_ON=false, .x0=-180, .y0=-80 },
//	{ .fname="/mmc/nian.gif", .DirectFB_ON=false, .ImgTransp_ON=false, .x0=20,  .y0=240-60 }, /* 50x50 */
//	{ .fname="/mmc/jis.gif", .DirectFB_ON=false, .ImgTransp_ON=true, .x0=10,  .y0=-30 }, /* 300x300 */
//	{ .fname="/mmc/3monk.gif", .DirectFB_ON=false, .ImgTransp_ON=false, .x0=110, .y0=240-60 },/* 150x50 */
	{ .fname="/mmc/muyu.gif", .DirectFB_ON=false, .ImgTransp_ON=true, .x0=90, .y0=80 }, //.x0=10, .y0=20  }, /* 140x123 */
};
//static EGI_GIF *egif[4];
static pthread_t thread_gif[4];
static void *thread_GifImgbuf_update(void *arg);

int main(int argc, char **argv)
{
        /* <<<<<  EGI general init  >>>>>> */
#if 0
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */

        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_test") != 0) {        /* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }

#endif

        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {       /* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;

#if 0
        printf("start touchread thread...\n");
        egi_start_touchread();                  /* start touch read thread */
#endif

        /* <<<<------------------  End EGI Init  ----------------->>>> */

	int i;
	int k;
    	int xres;
    	int yres;
	bool DirectFB_ON=false; /* For transparency_off GIF,to speed up,tear line possible. */
	bool ImgTransp_ON=true;

	/* Set FB mode as LANDSCAPE  */
        fb_position_rotate(&gv_fb_dev, 3);

    	xres=gv_fb_dev.pos_xres;
    	yres=gv_fb_dev.pos_yres;

        /* set FB mode */
    	if(DirectFB_ON) {

            gv_fb_dev.map_bk=gv_fb_dev.map_fb; /* Direct map_bk to map_fb */

    	} else {
            /* init FB back ground buffer page */
            memcpy(gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.map_fb, gv_fb_dev.screensize);

	    /*  init FB working buffer page */
            fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_BLUE);
            //memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_fb, gv_fb_dev.screensize);
    	}

	/* Start GIF updating threads */
	printf("Creating gif threads...\n");
	for(i=0; i<4; i++) {
		if( pthread_create(&thread_gif[i],NULL, thread_GifImgbuf_update, (void *)&gif_param[i]) != 0 )
		{
			printf("Fail to start thread_gif[%d]!\n",i);
			exit(-1);
		}
	}

	/* Loop displaying */
	printf("loop displaying...\n");
	k=320;
        while(1) {

	   /* refresh working buffer */
           fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_GRAY);
 	   //memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.screensize);

	   for(i=0;i<4; i++)
	   {
		if(gif_param[i].egif==NULL)
			continue;

		egi_imgbuf_windisplay( gif_param[i].egif->Simgbuf, &gv_fb_dev, -1,    /* img, fb, subcolor */
                          		gif_param[i].xp, gif_param[i].yp,       	/* xp, yp */
                           		gif_param[i].x0, gif_param[i].y0,  		/* xw, yw */
                           		gif_param[i].egif->SWidth, gif_param[i].egif->SHeight /* winw, winh */
                        	);

           	/* draw outline frame */
		#if 0
		fbset_color2(&gv_fb_dev, WEGI_COLOR_YELLOW);
		draw_wrect(&gv_fb_dev, gif_param[i].x0, gif_param[i].y0,
                           gif_param[i].x0+gif_param[i].egif->SWidth-1,
                           gif_param[i].y0+gif_param[i].egif->SHeight-1,3);

		#endif
	    }

	    /* write slogan */
	    k-=3;
	    if(k<-500)k=320;
            FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          24, 24, wslogan,                    	/* fw,fh, pstr */
                                          500, 1, 6,                   		 /* pixpl, lines, gap */
                                          k, 10,                           /* x0,y0, */
                              COLOR_RGB_TO16BITS(224,60,49), -1, 255 );   /* fontcolor, transcolor, opaque */

	    printf(" page refresh \n");
	    fb_page_refresh(&gv_fb_dev);
	    usleep(10000);
	}

	/* Free EGI_GIF */
        for(i=0; i<4; i++)
	    	egi_gif_free(&(gif_param[i].egif));

        /* <<<<<-----------------  EGI general release  ----------------->>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
        printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev); /* Flush FB filo if necessary */
        release_fbdev(&gv_fb_dev);
        printf("egi_end_touchread()...\n");
        egi_end_touchread();
        printf("egi_quit_log()...\n");

#if 0
        egi_quit_log();
        printf("<-------  END  ------>\n");
#endif

	return 0;
}


/*--------------------------------------
A thread function: updating GIF Simgbuf
---------------------------------------*/
static void *thread_GifImgbuf_update(void *arg)
{
	EGI_GIF *egif;
	GIF_PARAM *param=(GIF_PARAM *)arg;

	/* Slurp GIF data to EGI_GIF */
	egif= egi_gif_slurpFile( param->fname, param->ImgTransp_ON); /* fpath, bool ImgTransp_ON */
	if(egif==NULL) {
		printf("%s: Fail to read in gif file '%s'! \n",__func__, param->fname);
		return (void *)-1;
	}
	printf("%s: Finish slurping '%s' to EGI_GIF.\n", __func__, param->fname);

	/* assign EGI_GIF */
	param->egif=egif;

	/* Loop updating Simgbuf of egif */
        while(1) {
	    /* FBDEV is NULL, update egif->Simgbuf ONLY */
            egi_gif_displayFrame( NULL, egif, 		/* *fbdev, EGI_GIF *egif */
				  100, false,		/*  nloop, bool DirectFB_ON */
		  -1,-1, -1,		/*  int User_DisposalMode, int User_TransColor, int User_BkgColor, */
				  0,0,  		/* xp, yp */
				  0,0, 			/* xw, yw */
				  0,0			/* winw, winh */
			);
	}

    	egi_gif_free(&egif);


	return (void *)0;
}
