/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Example:
1. Call egi_gif_slurpFile() to load a GIF file to an EGI_GIF struct
2. Call egi_gif_displayFrame() to display the EGI_GIF.
3. Call egi_gif_runDisplayThread() to display EGI_GIF by a thread.

Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_gif.h"

#define FLOWER_MAX	6
#define SHOOTER_MAX	2
#define ZOMBIE_MAX	12

static void display_gifCharacter( EGI_GIF_CONTEXT *gif_ctxt);


int main(int argc, char **argv)
{

        /* <<<<<  EGI general init  >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */

        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_zombie") != 0) {        /* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
	}

#if 0
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {       /* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }
#endif

        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;

#if 0
        printf("start touchread thread...\n");
        egi_start_touchread();                  /* start touch read thread */
#endif

        /* <<<<------------------  End EGI Init  ----------------->>>> */


	int i;
    	int xres;
    	int yres;
	EGI_GIF_DATA *gifdata_flower=NULL;
	EGI_GIF_DATA *gifdata_peashooter=NULL;
	EGI_GIF_DATA *gifdata_zombie=NULL;

	EGI_GIF *gif_flower[FLOWER_MAX]={NULL};
	EGI_GIF *gif_peashooter[SHOOTER_MAX]={NULL};
	EGI_GIF *gif_zombie[ZOMBIE_MAX]={NULL};

	EGI_GIF_CONTEXT ctxt_flower[FLOWER_MAX];
	EGI_GIF_CONTEXT ctxt_peashooter[SHOOTER_MAX];
	EGI_GIF_CONTEXT ctxt_zombie[ZOMBIE_MAX];

	/* Set FB mode as LANDSCAPE  */
        fb_position_rotate(&gv_fb_dev, 0);
    	xres=gv_fb_dev.pos_xres;
    	yres=gv_fb_dev.pos_yres;

	/* Load game bkg image and display it */
	EGI_IMGBUF* imgbuf_garden=egi_imgbuf_readfile("/mmc/zombie/pz_scene.png");
	if(imgbuf_garden==NULL) {
		printf("Fail to load imgbuf_garden!\n");
		exit(1);
	}
        egi_imgbuf_windisplay( imgbuf_garden, &gv_fb_dev, -1,                   /* img, fb, subcolor */
                               0, 0, 0, 0,                                      /* xp,yp  xw,yw */
                               imgbuf_garden->width, imgbuf_garden->height);    /* winw, winh */
	fb_render(&gv_fb_dev);

        /* refresh working buffer */
        //clear_screen(&gv_fb_dev, WEGI_COLOR_GRAY);

        /* init FB back ground buffer page */
        memcpy(gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.map_fb, gv_fb_dev.screensize);
        /*  init FB working buffer page */
        //fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_BLUE);
        memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_fb, gv_fb_dev.screensize);


	/* Read GIF file into EGI_GIF_DATAs */
	gifdata_flower=egi_gifdata_readFile("/mmc/zombie/sunflower.gif");
	if(gifdata_flower==NULL) {
		printf("Fail to read in gif_data for sunflowers!\n");
		exit(-1);
	}
	gifdata_peashooter=egi_gifdata_readFile("/mmc/zombie/peashooter.gif");
	if(gifdata_peashooter==NULL) {
		printf("Fail to read in gif_data for peashooter!\n");
		exit(-1);
	}
	gifdata_zombie=egi_gifdata_readFile("/mmc/zombie/coneheader.gif");
	if(gifdata_zombie==NULL) {
		printf("Fail to read in gif_data for zombie!\n");
		exit(-1);
	}
        printf("Finishing read GIF file into EGI_GIF_DATAs.\n");

	/* Create EGI_GIF with given EGI_GIF_DATA */
	for(i=0; i<FLOWER_MAX; i++) {
		gif_flower[i]= egi_gif_create(gifdata_flower, true); /* fpath, bool ImgTransp_ON */
		if(gif_flower[i]==NULL) {
			printf("Fail to create EGI_GIF for sunflowers!\n");
			exit(-1);
		}
	}
	for(i=0; i<SHOOTER_MAX; i++) {
		gif_peashooter[i]= egi_gif_create(gifdata_peashooter, true); /* fpath, bool ImgTransp_ON */
		if(gif_peashooter[i]==NULL) {
			printf("Fail to create EGI_GIF for peashooters!\n");
			exit(-1);
		}
	}
	for(i=0; i<ZOMBIE_MAX; i++) {
		gif_zombie[i]= egi_gif_create(gifdata_zombie, true); /* fpath, bool ImgTransp_ON */
		if(gif_zombie[i]==NULL) {
			printf("Fail to create EGI_GIF for cone zombies!\n");
			exit(-1);
		}
	}
        printf("Finishing creating all EGI_GIFs.\n");


	/* ------------ Plants EGI_GIF contxt -----------*/

        ctxt_flower[0]=(EGI_GIF_CONTEXT) {
  			.fbdev=NULL, //&gv_fb_dev;
        		.egif=gif_flower[0],
        		.nloop=-1,
        		.DirectFB_ON=false,
        		.User_DisposalMode=-1,
        		.User_TransColor=-1,
        		.User_BkgColor=-1,
        		.xp=gif_flower[0]->SWidth>xres ? (gif_flower[0]->SWidth-xres)/2:0,
        		.yp=gif_flower[0]->SHeight>yres ? (gif_flower[0]->SHeight-yres)/2:0,
        		.xw=10,
        		.yw=55,
        		.winw=gif_flower[0]->SWidth>xres ? xres:gif_flower[0]->SWidth,
        		.winh=gif_flower[0]->SHeight>yres ? yres:gif_flower[0]->SHeight
	};
	/* assign to other flowers */
	for(i=1; i<FLOWER_MAX; i++)
	{
		ctxt_flower[i]=ctxt_flower[0];
	        ctxt_flower[i].egif=gif_flower[i];
        	ctxt_flower[i].xw=10+60*(i/2);
        	ctxt_flower[i].yw=55+90*(i%2);
		ctxt_flower[i].egif->ImageCount=egi_random_max(ctxt_flower[i].egif->ImageTotal);
	}

	/* assign to peashooter */
        ctxt_peashooter[0]=(EGI_GIF_CONTEXT) {
  			.fbdev=NULL, //&gv_fb_dev;
        		.egif=gif_peashooter[0],
        		.nloop=-1,
        		.DirectFB_ON=false,
        		.User_DisposalMode=-1,
        		.User_TransColor=-1,
        		.User_BkgColor=-1,
			.xp=gif_peashooter[0]->SWidth>xres ? (gif_peashooter[0]->SWidth-xres)/2:0,
			.yp=gif_peashooter[0]->SHeight>yres ? (gif_peashooter[0]->SHeight-yres)/2:0,
        		.xw= 180,
        		.yw= 40,
			.winw=gif_peashooter[0]->SWidth>xres ? xres:gif_peashooter[0]->SWidth,
			.winh=gif_peashooter[0]->SHeight>yres ? yres:gif_peashooter[0]->SHeight
	};
	/* for other peashooters */
	for(i=1; i<SHOOTER_MAX; i++) {
		ctxt_peashooter[i]=ctxt_peashooter[0];
		ctxt_peashooter[i].egif=gif_peashooter[i];
		ctxt_peashooter[i].xw= 180+60*(i/2);
		ctxt_peashooter[i].yw= 40+90*(i%2);
		ctxt_peashooter[i].egif->ImageCount=egi_random_max(ctxt_peashooter[i].egif->ImageTotal);
	}

	/* ------------ Zombie EGI_GIF contxt -----------*/

        ctxt_zombie[0]=(EGI_GIF_CONTEXT) {
  			.fbdev=NULL, //&gv_fb_dev;
        		.egif=gif_zombie[0],
        		.nloop=-1,
        		.DirectFB_ON=false,
        		.User_DisposalMode=-1,
        		.User_TransColor=-1,
        		.User_BkgColor=-1,
        		.xp=gif_zombie[0]->SWidth>xres ? (gif_zombie[0]->SWidth-xres)/2:0,
        		.yp=gif_zombie[0]->SHeight>yres ? (gif_zombie[0]->SHeight-yres)/2:0,
        		.xw=200,//300,
        		.yw=0,//60,
        		.winw=gif_zombie[0]->SWidth>xres ? xres:gif_zombie[0]->SWidth,
        		.winh=gif_zombie[0]->SHeight>yres ? yres:gif_zombie[0]->SHeight
	};
	/* for other zombies */
	for(i=1; i<ZOMBIE_MAX; i++) {
		ctxt_zombie[i]=ctxt_zombie[0];
	        ctxt_zombie[i].egif=gif_zombie[i];
       	 	ctxt_zombie[i].xw=200+30+egi_random_max(80)*(i%(ZOMBIE_MAX/2));
        	ctxt_zombie[i].yw=0+80*(i/(ZOMBIE_MAX/2));
		ctxt_zombie[i].egif->ImageCount=egi_random_max(ctxt_zombie[i].egif->ImageTotal);
	}

	/* --- Start thread: frame refresh thread for zombies --- */
	for(i=0; i<ZOMBIE_MAX; i++) {
		if( egi_gif_runDisplayThread(&ctxt_zombie[i]) !=0 ) {
			printf("Fail to run thread for ctxt_zombie[%d]!\n",i);
			exit(1);
		} else {
			printf("Finish loading thread for ctxt_zombie[%d]!\n",i);
		}
		tm_delayms(150); /* for some random factors */
	}

	/* --- Start thread: frame refresh thread for plants --- */
	for(i=0; i<FLOWER_MAX; i++) {	  					/* ----- Sun flowers ----- */
		if( egi_gif_runDisplayThread(&ctxt_flower[i]) !=0 ) {
			printf("Fail to run thread for ctxt_flower[%d]!\n",i);
			exit(1);
		} else {
			printf("Finish loading thread for ctxt_flower[%d]\n",i);
		}
		//tm_delayms(150); /* for some random factors */
	}
	for(i=0; i<SHOOTER_MAX; i++) {						/* ----- peashooters ----- */
		if( egi_gif_runDisplayThread(&ctxt_peashooter[i]) !=0 ) {
			printf("Fail to run thread for ctxt_peashooter[%d]\n",i);
			exit(1);
		} else {
			printf("Finish loading thread for ctxt_peashooter[%d]\n",i);
		}
		//tm_delayms(150); /* for some random factors */
	}


	EGI_PLOG(LOGLV_INFO,"%s: Start game loop...", argv[0]);
	/* ============================    GAME LOOP   =============================== */
while(1) {

        /* refresh working buffer */
        //fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_GRAY);
	memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.screensize);

	/* Beware of displaying sequence !*/

	/* ------------ Display Plants -------------- */
	/* display sunflowers */
	for(i=0; i<FLOWER_MAX; i++) {
		display_gifCharacter(&ctxt_flower[i]);
	}
	/* display peashooter */
	for(i=0; i<SHOOTER_MAX; i++) {
		display_gifCharacter(&ctxt_peashooter[i]);
	}

	/* ------------ Display Zombies -------------- */
	/* display cone zombies */
	for(i=0; i<ZOMBIE_MAX; i++) {
		display_gifCharacter(&ctxt_zombie[i]);

		/* refresh position */
		ctxt_zombie[i].xw -= 1;
		if(ctxt_zombie[i].xw < -120 )
			ctxt_zombie[i].xw=300;
	}

	/* Refresh FB */
	fb_render(&gv_fb_dev);
	tm_delayms(100);
        //usleep(100000);
}



#if 0 ///////////////////////////////////////////////////////////
        gif_ctxt.nloop=0; /* set one frame by one frame */
	/* Loop displaying */
        while(1) {

	    /* Display one frame/block each time, then refresh FB page.  */
	    egi_gif_displayFrame( &gif_ctxt );

        	gif_ctxt.xw -=2;

	}
    	egi_gif_free(&egif);
#endif /////////////////////////////////////////////////////////

	/* free EGI_GIF */
	for(i=0;i<FLOWER_MAX;i++)
		egi_gif_free(&gif_flower[i]);
	for(i=0;i<SHOOTER_MAX;i++)
		egi_gif_free(&gif_peashooter[i]);
	for(i=0;i<SHOOTER_MAX;i++)
		egi_gif_free(&gif_zombie[i]);

	/* free EGI_GIF_DATA */
	egi_gifdata_free(&gifdata_flower);
	egi_gifdata_free(&gifdata_peashooter);
	egi_gifdata_free(&gifdata_zombie);


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

#if 1
        egi_quit_log();
        printf("<-------  END  ------>\n");
#endif

	return 0;
}

/* -----------------------------------------------
     Display GIF actor with its context
-------------------------------------------------*/
static void display_gifCharacter( EGI_GIF_CONTEXT *gif_ctxt)
{

        egi_imgbuf_windisplay( gif_ctxt->egif->Simgbuf, &gv_fb_dev, -1,    /* img, fb, subcolor */
                               gif_ctxt->xp, gif_ctxt->yp,            /* xp, yp */
                               gif_ctxt->xw, gif_ctxt->yw,            /* xw, yw */
                               gif_ctxt->winw, gif_ctxt->winh /* winw, winh */
                              );
}
