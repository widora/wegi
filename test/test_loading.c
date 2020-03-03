/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_gif.h"
#include "egi_shmem.h"

int main(int argc, char **argv)
{
	int opt;	/* input option */
	char *fpath=NULL;
	bool	sigstop=false;

	bool PortraitMode=false; /* LCD display mode: Portrait or Landscape */
	bool ImgTransp_ON=true; /* Suggest: TURE */
	bool DirectFB_ON=false; /* For transparency_off GIF,to speed up,tear line possible. */

        /* parse input option */
        while( (opt=getopt(argc,argv,"hq"))!=-1)
        {
                switch(opt)
                {
                       case 'h':
                           printf("usage:  %s [-h] [-q] fpath \n", argv[0]);
                           printf("         -h   help \n");
                           printf("         -q   signal to stop gif playing through shm_msg.\n");
                           printf("	    fpath   Gif file path \n");
                           return 0;
                       case 'q':
                           printf(" Signal to stop gif playing...\n");
                           sigstop=true;
                           break;
                       default:
                           break;
                }
        }


	/* ------------ SHM_MSG communication ---------- */
	EGI_SHMEM shm_msg= {
	  .shm_name="loading_ctrl",
	  .shm_size=4*1024,
	};

	if( egi_shmem_open(&shm_msg) !=0 ) {
		printf("Fail to open shm_msg!\n");
		exit(1);
	}

	/* shm_msg communictate */
        if( shm_msg.msg_data->active) {		/* If msg_data is ACTIVE */
                if(shm_msg.msg_data->msg[0] != '\0')
                        printf("Msg in shared mem '%s' is: %s\n", shm_msg.shm_name, shm_msg.msg_data->msg);
                if(argv[1] != NULL ) {
                        strncpy(shm_msg.msg_data->msg,argv[1],64-1);
			printf("Pushed message '%s' into shared mem '%s'.\n",shm_msg.msg_data->msg, shm_msg.shm_name);
		}

		/* ---- signal to stop ----- */
		if( sigstop ) {
			printf("Signal to stop and wait....\n");
			while( shm_msg.msg_data->active) {
				shm_msg.msg_data->signum=atoi(argv[1]);
				shm_msg.msg_data->sigstop=true;
				tm_delayms(100);
			}
		}
		egi_shmem_close(&shm_msg);
		exit(1);
        }
	else {					/* Not active, activate it.  */
                shm_msg.msg_data->active=true;
                printf("You are the first to handle this mem!\n");
        }


        /* <<<<< ----------------  EGI general init --------------- >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */
#if 0
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_gif") != 0) {        /* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
	}
#endif

#if 1
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


	/* Set FB mode as LANDSCAPE  */
	if(PortraitMode)
        	fb_position_rotate(&gv_fb_dev,0);
	else
        	fb_position_rotate(&gv_fb_dev,3);

//    	xres=gv_fb_dev.pos_xres;
//    	yres=gv_fb_dev.pos_yres;

        /* set FB mode */
	fb_set_directFB(&gv_fb_dev, DirectFB_ON);
	if(!DirectFB_ON) {
		fb_init_FBbuffers(&gv_fb_dev); /* if NOT direct FB */
		/* save scene to the 2nd back ground buffer */
		memcpy(gv_fb_dev.map_bk+2*gv_fb_dev.screensize, gv_fb_dev.map_fb, gv_fb_dev.screensize);
	}

        /* refresh working buffer */
        //clear_screen(&gv_fb_dev, WEGI_COLOR_GRAY);


        /* <<<<------------------  End FB Setup ----------------->>>> */

        #if 1
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          30, 30, (wchar_t *)L"OCR检测中...",	 //"图像识别中...",     /* fw,fh, pstr */
                                          320, 1, 0,                    /* pixpl, lines, gap */
                                          75,  10,                           /* x0,y0, */
                                          COLOR_24TO16BITS(0xFF6600), -1, 255 );   /* fontcolor, transcolor,opaque */
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          30, 30, (wchar_t *)L"OCR检测中...",	//"图像识别中...",                    /* fw,fh, pstr */
                                          320, 1, 0,                    /* pixpl, lines, gap */
                                          75,  240-50,                           /* x0,y0, */
                                          COLOR_24TO16BITS(0x0099FF), -1, 255 );   /* fontcolor, transcolor,opaque */
        #else
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold,          /* FBdev, fontface */
                                        60, 60,(const unsigned char *)argv[1],  /* fw,fh, pstr */
                                        320, 3, 15,                             /* pixpl, lines, gap */
                                        20, 20,                                 /* x0,y0, */
                                        WEGI_COLOR_RED, -1, -1,      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	#endif
	fb_page_refresh(&gv_fb_dev,0);
	if(!DirectFB_ON)
		fb_init_FBbuffers(&gv_fb_dev); /* if NOT direct FB */



#if 1	/* ------------------  TEST:  egi_gif_playFile()  ------------------ */
	fpath=argv[1];
	while(1) {
	 	 memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.screensize);
		 egi_gif_playFile(fpath, false, ImgTransp_ON, NULL, 1, &shm_msg.msg_data->sigstop); /* 1 round */

		/* check EGI_SHM_MSG */
		if(shm_msg.msg_data->sigstop ) {
			printf(" ---------- shm_msg: sigstop received! \n");
			/* reset data */
			shm_msg.msg_data->signum=0;
			shm_msg.msg_data->active=false;
			shm_msg.msg_data->sigstop=false;
			break;
		}
	}
#endif

	if(!DirectFB_ON) {
		/* Restore scene from the 2nd back ground buffer */
		memcpy(gv_fb_dev.map_fb, gv_fb_dev.map_bk+2*gv_fb_dev.screensize, gv_fb_dev.screensize);
	}

RELEASE_EGI:
	egi_shmem_close(&shm_msg);

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
