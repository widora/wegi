#include <stdio.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"

int main(void)
{
        /* <<<<<  EGI general init  >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */
#if 0
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_test") != 0) {        /* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }

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

#endif

        printf("start touchread thread...\n");
        egi_start_touchread();                  /* start touch read thread */

        /* <<<<<  End EGI Init  >>>>> */

	printf("Finish EGI init.\n");

 	 while(1) {
		if( egi_touch_timeWait_press(5)==0 ) {
			printf(" Touch down!\n");
		}
		else {
			printf(" Time out!\n");
		}
		tm_delayms(500);
  	}



        /* <<<<<  EGI general release >>>>> */
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
        egi_quit_log();
        printf("<-------  END  ------>\n");


	return 0;
}
