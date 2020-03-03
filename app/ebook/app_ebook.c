/*-------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An EGI APP program for EBOOK reading.

   [  EGI_UI HOME_PAGE ]
	|
	|____ [ BUTTON ]
		  |
		  |
           <<<  signal  >>>
		  |
		  |
	      [ SUBPROCESS ] app_ebook.c
				|
	        		|____ [ UI_PAGE ] page_ebook.c
							|
							|_____ [ extern OBJ_FUNC ] xxxxx.c


Midas Zhou
midaszhou@yahoo.com
---------------------------------------------------------------------------------------*/
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_procman.h"
#include "page_ebook.h"
#include <signal.h>
#include <sys/types.h>

const char *app_name="app_ebook";
static EGI_PAGE *page_ebook=NULL;

/*----------------------------
	     MAIN
----------------------------*/
int main(int argc, char **argv)
{
	int ret=0;
	pthread_t thread_loopread;

        /*  ---  0. assign signal actions  --- */
	printf("APP_EBOOK:  -------- assign signal action --------\n");
	egi_assign_AppSigActions();

        /*  ---  1. EGI General Init Jobs  --- */
        tm_start_egitick();
        if(egi_init_log("/mmc/ebook_log") != 0 ) {
                EGI_PLOG(LOGLV_ERROR,"Fail to init logger,quit.\n");
                return -1;
        }
        if(symbol_load_allpages() !=0 ) {
                EGI_PLOG(LOGLV_ERROR,"Fail to load sym pages,quit.\n");
                ret=-2;
		goto FF_FAIL;
        }
	/* FTsymbol needs more memory, so just disable it if not necessary */
        if(FTsymbol_load_allpages() !=0 ) {
                EGI_PLOG(LOGLV_ERROR,"Fail to load FTsymbol pages,quit.\n");
                ret=-2;
		goto FF_FAIL;
        }
	/* FTsymbol needs more memory, so just disable it if not necessary */
        if(FTsymbol_load_appfonts() !=0 ) {
                EGI_PLOG(LOGLV_ERROR,"Fail to load FT sys fonts,quit.\n");
                ret=-2;
		goto FF_FAIL;
        }


	/* Init FB device */
        init_fbdev(&gv_fb_dev);
        /* start touch_read thread */
        SPI_Open();/* for touch_spi dev  */
        if( pthread_create(&thread_loopread, NULL, (void *)egi_touch_loopread, NULL) !=0 )
        {
                EGI_PLOG(LOGLV_ERROR, "%s: pthread_create(... egi_touch_loopread() ... ) fails!\n",app_name);
                ret=-3;
		goto FF_FAIL;
        }

	/*  ---  2. EGI PAGE creation  ---  */
	printf(" start page ebook creation....\n");
        /* create page and load the page */
        page_ebook=create_ebook_page();
        EGI_PLOG(LOGLV_INFO,"%s: [page '%s'] is created.\n", app_name, page_ebook->ebox->tag);

	/* activate and display the page */
        egi_page_activate(page_ebook);
        EGI_PLOG(LOGLV_INFO,"%s: [page '%s'] is activated.\n", app_name, page_ebook->ebox->tag);

        /* trap into page routine loop */
        EGI_PLOG(LOGLV_INFO,"%s: Now trap into routine of [page '%s']...\n", app_name, page_ebook->ebox->tag);
        page_ebook->routine(page_ebook);
        /* get out of routine loop */
        EGI_PLOG(LOGLV_INFO,"%s: Exit routine of [page '%s'], start to free the page...\n",
		                                                app_name,page_ebook->ebox->tag);

	tm_delayms(200); /* let page log_calling finish */

	/* free PAGE and other resources */
        egi_page_free(page_ebook);
	free_ebook_page();

	ret=pgret_OK;

FF_FAIL:
       	release_fbdev(&gv_fb_dev);
	FTsymbol_release_allfonts();
        FTsymbol_release_allpages();
        symbol_release_allpages();
	SPI_Close();
        egi_quit_log();

	/* NOTE: APP ends, screen to be refreshed by the parent process!! */
	return ret;

}
