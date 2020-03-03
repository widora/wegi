/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A general EGI touch program, start from Home Page.

TODO: Polling touch data/status by a thread is inefficient, turn to
      I/O event driven approach.


Midas Zhou
-------------------------------------------------------------------*/
#include "egi_common.h"
#include "page/egi_pagehome.h"
#include "egi_FTsymbol.h"


int main(int argc, char **argv)
{
	EGI_PAGE *page_home;
        pthread_t   thread_loopread;

        /* <<<<<  EGI general init  >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/egi_log") != 0) {  /* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {       /* load sys ASCII fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        printf("FTsymbol_load_allpages()...\n"); /* load FreeType sys ASCII fonts */
        if( FTsymbol_load_allpages() !=0 ) {
                printf("Fail to load FT symbol pages!\n");
                return -3;
        }
#if 1
        if(FTsymbol_load_sysfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -4;
        }
#endif
#if 1
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -4;
        }
#endif
        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB device */
                return -5;
        /* <<<<<  END EGI general init  >>>>>> */

        /* ---- start touch thread ---- */
        SPI_Open(); /* open SPI device for LCD TOUCH */
        if( pthread_create(&thread_loopread, NULL, (void *)egi_touch_loopread, NULL) !=0 )
        {
                printf(" pthread_create(... egi_touch_loopread() ... ) fails!\n");
                return -6;
        }

        /* ----- start homepage ----- */
        page_home=egi_create_homepage();
        egi_page_activate(page_home);
        page_home->routine(page_home); /* get into routine loop */

        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
        printf("FTsymbol_release_allpages()...\n");
	FTsymbol_release_allpages();
        printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
        printf("egi_quit_log()...\n");
        egi_quit_log();
        printf("<-------  END  ------>\n");

return 0;
}
