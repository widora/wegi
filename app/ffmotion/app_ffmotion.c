/*-------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An EGI APP program for FFmotion player.

   [  EGI_UI ]
	|
	|____ [ BUTTON ]
		  |
		  |
           <<<  signal  >>>
		  |
		  |
	      [ SUBPROCESS ] app_ffmotion.c
				|
	        		|____ [ UI_PAGE ] page_ffmotion.c
							|
							|_____ [ extern OBJ FILE ] ffmotion.c


Midas Zhou
midaszhou@yahoo.com
---------------------------------------------------------------------------------------*/
#include "egi_common.h"
#include "egi_procman.h"
#include "egi_utils.h"
#include "egi_cstring.h"
#include "ffmotion.h"
#include "egi_FTsymbol.h"
#include "page_ffmotion.h"
#include "ffmotion_utils.h"
#include <signal.h>
#include <sys/types.h>
#include <malloc.h>

const char *app_name="app_ffmotion";

static EGI_PAGE *page_ffmotion=NULL;

/*----------------------------
	     MAIN
----------------------------*/
int main(int argc, char **argv)
{
	int ret=0;
	char video_dir[EGI_PATH_MAX]={0};
	char url_addr[EGI_URL_MAX]={0};

        /* Set memory allocation option */
        mallopt(M_MMAP_MAX,0);          /* forbid calling mmap to allocate mem */
        mallopt(M_TRIM_THRESHOLD,-1);   /* forbid memory trimming */

        /*  ---  0. assign signal actions  --- */
	egi_assign_AppSigActions();

        /*  ---  1. EGI General Init Jobs  --- */
        tm_start_egitick();
        if(egi_init_log("/mmc/log_ffmotion") != 0) {
                printf("Fail to init logger for ffmotion,  quit.\n");
                return -1;
        }
        if(symbol_load_allpages() !=0 ) {
                printf("Fail to load sym pages,quit.\n");
                ret=-2;
		goto FF_FAIL;
        }
        if(FTsymbol_load_allpages() !=0 ) {
                printf("Fail to load sym pages,quit.\n");
                ret=-2;
		goto FF_FAIL;
        }
	/* FT fonts needs more memory, disable it if not necessary */
	FTsymbol_load_appfonts();

	/* Init FB device,for PAGE displaying */
        init_fbdev(&gv_fb_dev);

        /* start touch_read thread */
	if( egi_start_touchread() != 0 ) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to start touch loopread thread!\n", __func__);
		goto FF_FAIL;
	}

	/*  --- 2. set FFmotion Context --- */
	if( init_ffmotionCtx("avi, mp4, mp3") ) {
	        EGI_PLOG(LOGLV_INFO,"%s: fail to init FFmotion_Ctx.\n", __func__);
		goto FF_FAIL;
	}

	/*  ---  3. EGI PAGE creation  ---  */
	printf(" Start creating PAGE for FFmotion ....\n");
        /* 3.1 create page and load the page */
        page_ffmotion=create_ffmotionPage();
        EGI_PLOG(LOGLV_INFO,"%s: [page '%s'] is created.\n", app_name, page_ffmotion->ebox->tag);

	/* 3.2 activate and display the page */
        egi_page_activate(page_ffmotion);
        EGI_PLOG(LOGLV_INFO,"%s: [page '%s'] is activated.\n", app_name, page_ffmotion->ebox->tag);

        /* 3.3 trap into page routine loop */
        EGI_PLOG(LOGLV_INFO,"%s: Now trap into routine of [page '%s']...\n", app_name, page_ffmotion->ebox->tag);
        page_ffmotion->routine(page_ffmotion);

        /* 3.4 get out of routine loop */
        EGI_PLOG(LOGLV_INFO,"%s: Exit routine of [page '%s'], start to free the page...\n",
		                                                app_name,page_ffmotion->ebox->tag);

	tm_delayms(200); /* let page log_calling finish */

        egi_page_free(page_ffmotion);
	ret=pgret_OK;

FF_FAIL:
	free_ffmotionCtx();
       	release_fbdev(&gv_fb_dev);
        FTsymbol_release_allpages();
        symbol_release_allpages();
	egi_end_touchread();
        egi_quit_log();


	/* NOTE: APP ends, screen to be refreshed by the parent process!! */
	return ret;
}
