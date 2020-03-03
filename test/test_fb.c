/*-----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test FBDEV functions


Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "utils/egi_utils.h"

int main(int argc, char **argv)
{
int i;
int px,py;
EGI_POINT   ept;
EGI_IMGBUF *eimg=NULL;
EGI_IMGBUF *logo1=NULL;
EGI_IMGBUF *logo2=NULL;
FBDEV 	   vfb;

//wchar_t *prophesy=L"人心生一念，天地尽皆知。善恶若无报，乾坤必有私。";
wchar_t *prophesy=L"人心生一念, 天地盡皆知. 善惡若無報, 乾坤必有私. ";
wchar_t *ptemp=L"温度 36";


while(1) {    ////////////////////////////    LOOP TEST    ////////////////////////////

        /* EGI general init */
	printf("tm_start_egitick()...\n");
        tm_start_egitick();
	printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_fb") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
        }
	printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        if(FTsymbol_load_appfonts() !=0 ) {  /* and FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

	/* init a FB */
	printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )
		return -1;


#if 0 /* <<<<<<<<<<< TEST: virtual FB functions (1)  <<<<<<<<<<<<< */
	/* init a virtal FB */
	printf("load PNG to imgbuf...\n");
        eimg=egi_imgbuf_alloc();
        if( egi_imgbuf_loadpng("/mmc/mice.png", eimg) ) //icerock.png
                return -1;

	printf("init_virt_fbdev()...\n");
	if(init_virt_fbdev(&vfb, eimg))
		return -1;

	/* draw on VIRT FB */
	printf("draw pcircle on virt FB...\n");
	for(i=0;i<15; i++) {
		egi_randp_inbox(&ept, &gv_fb_box);
		draw_filled_annulus2(&vfb, ept.x, ept.y,
				20+egi_random_max(150), 2+egi_random_max(15), egi_color_random(color_all));
	}
	/* write on VIRT FB */
	printf("write on virt FB...\n");
        FTsymbol_unicstrings_writeFB(&vfb, egi_appfonts.special,   /* FBdev, fontface */
                                           36, 36, prophesy,    /* fw,fh, pstr */
                                           240, 5, 5,           /* pixpl, lines, gap */
                                           20, 80,                      /* x0,y0, */
                                           WEGI_COLOR_WHITE, -1, -1);   /* fontcolor, stranscolor,opaque */

	printf("windisplay...\n");
	egi_imgbuf_windisplay(eimg, &gv_fb_dev, -1, 0, 0, 0, 0, 240, 320 );
	//egi_imgbuf_windisplay2(eimg, &gv_fb_dev, 0, 0, 0, 0, 240,320);

//	tm_delayms(200);


#elif 1 /* <<<<<<<<<<< TEST: virtual FB functions (2)  <<<<<<<<<<<<< */

	/* init a virtal FB */
	printf("init an empty imgbuf...\n");
        eimg=egi_imgbuf_alloc();
	egi_imgbuf_init(eimg, 60, 240*2);
        //if( egi_imgbuf_loadpng("/mmc/lll.png", eimg) )
        //        return -1;
	printf("init_virt_fbdev()...\n");
	if( init_virt_fbdev(&vfb, eimg) )
		return -1;


 ///////<<<<<  logo1  >>>>>///////
	/* origin of png, relative to virt FB coord */
	px=60; py=0;
	/* blend a png onto virt FB */
	logo1=egi_imgbuf_alloc();
        if( egi_imgbuf_loadpng("/mmc/shanghai_today.png",logo1) )
                return -1;
	egi_imgbuf_windisplay(logo1, &vfb, WEGI_COLOR_WHITE, 0, 0, px, py, logo1->width, logo1->height );

	/* write on VIRT FB */
	printf("write on virt FB...\n");
        FTsymbol_unicstrings_writeFB(&vfb, egi_appfonts.regular,   /* FBdev, fontface */
                                           18, 18, L"温度36",    /* fw,fh, pstr */
                                           200, 1, 0,           /* pixpl, lines, gap */
                                           px+60, 10,                      /* x0,y0, */
                                           WEGI_COLOR_ORANGE, -1, -1);   /* fontcolor, stranscolor,opaque */
        FTsymbol_unicstrings_writeFB(&vfb, egi_appfonts.regular,   /* FBdev, fontface */
                                           18, 18, L"上海",    /* fw,fh, pstr */
                                           200, 1, 0,           /* pixpl, lines, gap */
                                           px+60, 30,                      /* x0,y0, */
                                           WEGI_COLOR_YELLOW, -1, -1);   /* fontcolor, stranscolor,opaque */

 ///////<<<<<  logo2  >>>>>///////
	/* origin of png, relative to virt FB coord */
	px=60+240; py=0;
	/* blend a png onto virt FB */
	logo2=egi_imgbuf_alloc();
        if( egi_imgbuf_loadpng("/mmc/zhoushan_today.png",logo2) )
                return -1;
	egi_imgbuf_windisplay(logo2, &vfb, WEGI_COLOR_WHITE, 0, 0, px, py, logo2->width, logo2->height );

	/* write on VIRT FB */
	printf("write on virt FB...\n");
        FTsymbol_unicstrings_writeFB(&vfb, egi_appfonts.regular,   /* FBdev, fontface */
                                           18, 18, L"温度36",    /* fw,fh, pstr */
                                           200, 1, 0,           /* pixpl, lines, gap */
                                           px+60, 10,                      /* x0,y0, */
                                           WEGI_COLOR_ORANGE, -1, -1);   /* fontcolor, stranscolor,opaque */
        FTsymbol_unicstrings_writeFB(&vfb, egi_appfonts.regular,   /* FBdev, fontface */
                                           18, 18, L"舟山",    /* fw,fh, pstr */
                                           200, 1, 0,           /* pixpl, lines, gap */
                                           px+60, 30,                      /* x0,y0, */
                                           WEGI_COLOR_BLUE, -1, -1);   /* fontcolor, stranscolor,opaque */


  /* ----- motion display ----- */
   for(i=0; i<180; i++) {
        /* <<<<<<<    Flush FB and Turn on FILO  >>>>>>> */
        //printf("Flush pixel data in FILO, start  ---> ");
        fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
        fb_filo_on(&gv_fb_dev); /* start collecting old FB pixel data */

	/* write to FB */
	//printf("windisplay...\n");
	egi_imgbuf_windisplay(eimg, &gv_fb_dev, -1, i<<1, 0, 0, 240, eimg->width, eimg->height);

        fb_filo_off(&gv_fb_dev); /* turn off filo */

	tm_delayms(120);
   }

#endif /* END TEST Virt FB (2) */


#if 0 /* <<<<<<<<<<< TEST: fb_buffer_FBimg() and fb_restore_FBimg() <<<<<<<<<<<<< */

	printf("fb_buffer_FBimg(NB=0)...\n");
	fb_buffer_FBimg(&gv_fb_dev,0);

	tm_delayms(500);

	printf("fb_restore_FBimg(NB=0, clear=TRUE)...\n");
	fb_restore_FBimg(&gv_fb_dev,0,true);

#endif /* END TEST fb FBimg */


	/* ----- Release and free sources ----- */
	printf("egi_imgbuf_free()...\n");
	egi_imgbuf_free(eimg);
	egi_imgbuf_free(logo1);
	egi_imgbuf_free(logo2);

	printf("release_virt_fbdev()...\n");
	release_virt_fbdev(&vfb);

	printf("release_fbdev()...\n");
    	fb_filo_flush(&gv_fb_dev); /* flush FILO at last */
	release_fbdev(&gv_fb_dev);

	printf("FTsymbol_release_allfonts()...\n");
	FTsymbol_release_allfonts();

	printf("symbol_release_allpages()...\n");
	symbol_release_allpages();

	printf("egi_quit_log()...\n");
	egi_quit_log();
	printf("---------- END -----------\n");

}   ///////////////////////////   END LOOP TEST   ////////////////////////////


	return 0;
}
