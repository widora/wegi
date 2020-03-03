/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include "egi_common.h"
#include "egi_utils.h"
#include "egi_procman.h"
#include "egi_cstring.h"
#include "egi_FTsymbol.h"
#include "page_avenger.h"

#include <signal.h>
#include <sys/types.h>
#include <malloc.h>

const char *app_name="app_avenger";
static EGI_PAGE *page_avenger=NULL;


/*----------------------------
	     MAIN
----------------------------*/
int main(int argc, char **argv)
{
	int ret=0;

        /* Set memory allocation option */
  //      mallopt(M_MMAP_MAX,0);          /* forbid calling mmap to allocate mem */
  //      mallopt(M_TRIM_THRESHOLD,-1);   /* forbid memory trimming */

        /*  ---  0. assign signal actions  --- */
//	egi_assign_AppSigActions();

        /*  ---  1. EGI General Init Jobs  --- */
        tm_start_egitick();
#ifndef  LETS_NOTE
        if(egi_init_log("/mmc/log_ffmotion") != 0) {
                printf("Fail to init logger for ffmotion,  quit.\n");
                return -1;
        }
#endif
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
#ifndef LETS_NOTE
	if( egi_start_touchread() != 0 ) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to start touch loopread thread!\n", __func__);
		goto FF_FAIL;
	}
#endif

	/* ---  2. EGI PAGE creation  --- */
	printf(" Start creating PAGE for FFmotion ....\n");
        /* create page and load the page */
        page_avenger=create_avengerPage();
        EGI_PLOG(LOGLV_INFO,"%s: [page '%s'] is created.\n", app_name, page_avenger->ebox->tag);

	/* activate and display the page */
        egi_page_activate(page_avenger);
        EGI_PLOG(LOGLV_INFO,"%s: [page '%s'] is activated.\n", app_name, page_avenger->ebox->tag);

        /* trap into page routine loop */
        EGI_PLOG(LOGLV_INFO,"%s: Now trap into routine of [page '%s']...\n", app_name, page_avenger->ebox->tag);
        page_avenger->routine(page_avenger);

        /* get out of routine loop */
        EGI_PLOG(LOGLV_INFO,"%s: Exit routine of [page '%s'], start to free the page...\n",
		                                                app_name,page_avenger->ebox->tag);

	/* --- 3. Free page --- */
	tm_delayms(2000); /* let page log_calling finish */
        egi_page_free(page_avenger);


	ret=pgret_OK;

FF_FAIL:
       	release_fbdev(&gv_fb_dev);
        FTsymbol_release_allpages();
        symbol_release_allpages();
	egi_end_touchread();
        egi_quit_log();

	/* NOTE: APP ends, screen to be refreshed by the parent process!! */
	return ret;

}
