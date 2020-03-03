/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <getopt.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_gif.h"

int main(int argc, char **argv)
{
	int ts=0;

	if(argc < 1)
		return 2;

	if(argc > 2)
		ts=atoi(argv[2]);
	else
	 	ts=1;

        /* <<<<<  EGI general init  >>>>>> */
#if 0
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */

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

	/* Direct FB */
	fb_set_directFB(&gv_fb_dev, true);
	/* set pos_rotation */
	gv_fb_dev.pos_rotate=3;

	/* backup current screen */
       fb_page_saveToBuff(&gv_fb_dev, 0);

#if 0
	/* draw box */
	int offx=atoi(argv[1]);
	int offy=atoi(argv[2]);
	int width=atoi(argv[3]);
	int height=atoi(argv[4]);

	fbset_color(WEGI_COLOR_RED);
	draw_wrect(&gv_fb_dev, offx, offy, offx+width, offy+height,3);
#endif



#if 1

        #if 0
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          17, 17, wstr1,                    /* fw,fh, pstr */
                                          320, 6, 6,                    /* pixpl, lines, gap */
                                          0, 240-30,                           /* x0,y0, */
                                          COLOR_RGB_TO16BITS(0,151,169), -1, 255 );   /* fontcolor, transcolor,opaque */
	#endif

	//printf("%s\n", argv[1]);
	FTsymbol_uft8strings_writeFB( 	&gv_fb_dev, egi_appfonts.bold,         	/* FBdev, fontface */
				      	//60, 60,(const unsigned char *)argv[1], 	/* fw,fh, pstr */
				      	//320, 3, 15,                    	    	/* pixpl, lines, gap */
					//20, 20,                           	/* x0,y0, */
				      	30, 30,(const unsigned char *)argv[1], 	/* fw,fh, pstr */
				      	320-10, 5, 4,                    	/* pixpl, lines, gap */
					10, 10,                           	/* x0,y0, */
                                     	WEGI_COLOR_RED, -1, -1,      /* fontcolor, transcolor,opaque */
                                     	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
#endif


	/* restore current screen */
	sleep(ts);
	fb_page_restoreFromBuff(&gv_fb_dev, 0);

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
