/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

		Usage:  %s [-h] [-t] [-d] [-p] fpath

Test gif file:
1. muyu.gif
2. fu.gif
3. runner.gif
4. jis.gif		!!! with mixed dispose mode !!!
5. cloud.gif

		--- Test Functions ---

1. egi_gif_slurpFile() to load a GIF file to an EGI_GIF struct.
2. egi_gifdata_readFile() to load GIF data to an EGI_GIF_DATA struct.
3. egi_gif_cread() to create an EGI_GIF with the given EGI_GIF_DATA.
4. egi_gif_displayFrame() to display the EGI_GIF.
5. egi_gif_runDisplayThread() to display EGI_GIF by a thread.


Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <getopt.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_gif.h"

int main(int argc, char **argv)
{
	int opt;	/* imput option */
	char *fpath=NULL;
	int i;
    	int xres;
    	int yres;
	EGI_GIF_DATA*  gif_data=NULL;
	EGI_GIF *egif=NULL;
	EGI_GIF_CONTEXT gif_ctxt;

	bool PortraitMode=false; /* LCD display mode: Portrait or Landscape */
	bool ImgTransp_ON=false; /* Suggest: TURE */
	bool DirectFB_ON=false; /* For transparency_off GIF,to speed up,tear line possible. */
	int User_DispMode=-1;   /* <0, disable */
	int User_TransColor=-1; /* <0, disable, if>=0, auto set User_DispMode to 2. */
	int User_BkgColor=-1;   /* <0, disable */

	int xp,yp;
	int xw,yw;


	/* parse input option */
	while( (opt=getopt(argc,argv,"htdp"))!=-1)
	{
    		switch(opt)
    		{
		       case 'h':
		           printf("usage:  %s [-h] [-t] [-d] [-p] fpath \n", argv[0]);
		           printf("         -h   help \n");
		           printf("         -t   Image_transparency ON. ( default is OFF ) \n");
		           printf("         -d   Wirte to FB mem directly, without buffer. ( default use FB working buffer ) \n");
		           printf("         -p   Portrait mode. ( default is Landscape mode ) \n");
			   printf("fpath   Gif file path \n");
		           return 0;
		       case 't':
		           printf(" Set ImgTransp_ON=true.\n");
		           ImgTransp_ON=true;
		           break;
		       case 'd':
		           printf(" Set DirectFB_ON=true.\n");
		           DirectFB_ON=true;
		           break;
		       case 'p':
			   printf(" Set PortraitMode=true.\n");
			   PortraitMode=true;
			   break;
		       default:
		           break;
	    	}
	}
	/* get file path */
	printf(" optind=%d, argv[%d]=%s\n", optind, optind, argv[optind] );
	fpath=argv[optind];
	if(fpath==NULL) {
	           printf("usage:  %s [-h] [-t] [-d] [-p] fpath\n", argv[0]);
	           exit(-1);
	}

        /* <<<<<  EGI general init  >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */

        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_gif") != 0) {        /* start logger */
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

        /* refresh working buffer */
        //clear_screen(&gv_fb_dev, WEGI_COLOR_GRAY);

	/* Set FB mode as LANDSCAPE  */
	if(PortraitMode)
        	fb_position_rotate(&gv_fb_dev,0);
	else
        	fb_position_rotate(&gv_fb_dev,3);

    	xres=gv_fb_dev.pos_xres;
    	yres=gv_fb_dev.pos_yres;

        /* set FB mode */
    	if(DirectFB_ON) {
            gv_fb_dev.map_bk=gv_fb_dev.map_fb; /* Direct map_bk to map_fb */

    	} else {
            /* init FB back ground buffer page */
            memcpy(gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.map_fb, gv_fb_dev.screensize);

	    /*  init FB working buffer page */
            //fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_BLUE);
            memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_fb, gv_fb_dev.screensize);
    	}

        /* <<<<------------------  End FB Setup ----------------->>>> */


#if 0	/* ------------------  TEST:  egi_gif_playFile()  ------------------ */

         // egi_gif_playFile(const char *fpath, bool Silent_Mode, bool ImgTransp_ON, int *ImageCount)
	while(1) {
	 	memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.screensize);
		 egi_gif_playFile(fpath, false, ImgTransp_ON, NULL);
	}
#endif

while(1) {  ////////////////////////////////  ---  LOOP TEST  ---  /////////////////////////////////////

   #if 1  /* OPTION 1:  ------------------  TEST:  Read into by egi_gif_slurpFile()  ------------------ */

	/* read GIF data into an EGI_GIF */
	egif= egi_gif_slurpFile( fpath, ImgTransp_ON); /* fpath, bool ImgTransp_ON */
	if(egif==NULL) {
		printf("Fail to read in gif file!\n");
		exit(-1);
	}
        printf("Finish reading GIF file into EGI_GIF by egi_gif_slurpFile().\n");

   #else /* OPTION 2:  ----------  TEST:  Read into by egi_gifdata_readFile()/egi_gif_create()  --------- */

	printf("Call egi_gifdata_readFile()...\n");
	gif_data=egi_gifdata_readFile( fpath );
	if(gif_data==NULL) {
		printf("Fail to read gif_data by egi_gifdata_readFile()!\n");
		exit(-1);
	}
	egif=egi_gif_create(gif_data, ImgTransp_ON);
        if(egif==NULL) {
               printf("Fail to create egif by egi_gif_create().\n");
               exit(-1);
        }
        printf("Finish reading GIF file into egi_data and creating egif by egi_gif_create().\n");

   #endif

	/* Cal xp,yp xw,yw, to position it to the center of LCD  */
	xp=egif->SWidth>xres ? (egif->SWidth-xres)/2:0;
	yp=egif->SHeight>yres ? (egif->SHeight-yres)/2:0;
	xw=egif->SWidth>xres ? 0:(xres-egif->SWidth)/2;
	yw=egif->SHeight>yres ? 0:(yres-egif->SHeight)/2;

        /* Lump context data */
        gif_ctxt.fbdev=&gv_fb_dev;
        gif_ctxt.egif=egif;
        gif_ctxt.nloop=-1;
        gif_ctxt.DirectFB_ON=DirectFB_ON;
        gif_ctxt.User_DisposalMode=User_DispMode;
        gif_ctxt.User_TransColor=User_TransColor;
        gif_ctxt.User_BkgColor=User_BkgColor;
        gif_ctxt.xp=xp;
        gif_ctxt.yp=yp;
        gif_ctxt.xw=xw;
        gif_ctxt.yw=yw;
        gif_ctxt.winw=egif->SWidth>xres ? xres:egif->SWidth; /* put window at center of LCD */
        gif_ctxt.winh=egif->SHeight>yres ? yres:egif->SHeight;


#if 0  /* ----------------------  TEST:  egi_gif_runDisplayThread( )  ----------------------- */

	while(1) {
		printf("\n\n\n\n----- New Round DisplayThread -----\n\n\n\n");

		/* start display thread */
		if( egi_gif_runDisplayThread(&gif_ctxt) !=0 )
			continue;

		/* Cancel thread after a while */
		//sleep(3);
		EGI_PLOG(LOGLV_CRITICAL,"%s: Start tm_delayms(3*1000) ... ",__func__);
		tm_delayms(3*1000);
		EGI_PLOG(LOGLV_CRITICAL,"%s: Finish tm_delayms(3*1000).", __func__);

		 printf("%s: Call egi_gif_stopDisplayThread...\n",__func__);
        	 if(egi_gif_stopDisplayThread(egif)!=0)
			EGI_PLOG(LOGLV_CRITICAL,"%s: Fail to call egi_gif_stopDisplayThread().",__func__);
	 }
	exit(1);
#endif


#if 1  /* ----------------------  TEST:  egi_gif_displayGifCtxt( )  ----------------------- */

        gif_ctxt.nloop=2; /* 0: set one frame by one frame , <0: forever, else: number of loops */

	/* Loop displaying */
	printf("Call egi_gif_displayGifCtxt()...\n");
//        while(1) {
	   	/* Display one frame/block each time, then refresh FB page.  */
	    	egi_gif_displayGifCtxt( &gif_ctxt );

		#if 0 /* ---  To  apply when .nloop = 0 --- */
        	//gif_ctxt.xw -=1;
		printf("Step %d\n", gif_ctxt.egif->ImageCount);
		if( gif_ctxt.egif->ImageCount > 3 && gif_ctxt.egif->ImageCount < 8 )
		gif_ctxt.xw -=3; /* apply when .nloop = 0 */
		else
		gif_ctxt.xw -=1;
		//getchar();
		#endif
//	}
#endif

    	egi_gif_free(&egif);
	egi_gifdata_free(&gif_data);
	printf("	--- End one round ---\n\n");

}  //////////////////////////////////   --- END LOOP TEST ---   //////////////////////////////////


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
