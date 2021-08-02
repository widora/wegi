/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A program to test EGI C libs, and a simple ETriangle class operation.


Reference:
include/sys/cdefs.h:# define __BEGIN_NAMESPACE_STD	namespace std {
include/sys/cdefs.h:# define __END_NAMESPACE_STD	}
include/sys/cdefs.h:# define __USING_NAMESPACE_STD(name) using std::name;
include/sys/cdefs.h:# define __BEGIN_NAMESPACE_STD
include/sys/cdefs.h:# define __END_NAMESPACE_STD
include/sys/cdefs.h:# define __USING_NAMESPACE_STD(name)

include/sys/cdefs.h: #ifdef	__cplusplus
lib/gcc/mipsel-openwrt-linux-uclibc/4.8.3/include/stddef.h:  || (defined(__cplusplus) && __cplusplus >= 201103L)



Journal:
2021-07-27:
	1. Create test_cpp.cpp

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include "egi_debug.h"
#include "egi_fbdev.h"
#include "egi_math.h"
#include "egi_FTsymbol.h"
#include "egi_vector3D.h"

#include <iostream>
using namespace std;


/*-------------------------------
	Class Definition
-------------------------------*/
class ETriangle
{
	public:
		ETriangle()
		{
			pts[0]=(EGI_POINT){160,80};
			pts[1]=(EGI_POINT){80, 160};
			pts[2]=(EGI_POINT){240,160};
		}

		void draw()
		{
			draw_filled_triangle(&gv_fb_dev, pts);
		}

		void random()
		{
			egi_randp_inbox(pts+0, &gv_fb_box);
			egi_randp_inbox(pts+1, &gv_fb_box);
			egi_randp_inbox(pts+2, &gv_fb_box);
		}

	private:
		EGI_POINT pts[3];
};


/*--------------------
	MAIN
--------------------*/
int main(void)
{
	/* <<<<<  EGI general init  >>>>>> */

#if 0
	/* Start sys tick */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();

	/* Start EGI log */
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_wifiscan") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
        }

	/* Load symbol pages */
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
	/* Load freetype sysfonts */
        printf("FTsymbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0) {
                printf("Fail to load FT sysfonts, quit.\n");
                return -1;
        }
#endif
	/* Load freetype appfonts */
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

        //FTsymbol_set_SpaceWidth(1);

        /* Initilize sys FBDEV */
        printf("init_fbdev()...\n");
        if(init_fbdev(&gv_fb_dev))
                return -1;

        /* Start touch read thread */
#if 0
        printf("Start touchread thread...\n");
        if(egi_start_touchread() !=0)
                return -1;
#endif

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev,false);
        fb_position_rotate(&gv_fb_dev,0);

	/* <<<<<  End of EGI general init  >>>>>> */


	/* ===== MAIN PROG ===== */

	egi_dpstd("Hello, this is C++!\n");
	cout << "Hello, this is C++!\n" << endl;

        int i,k;
        int r=200;//75,180
        int xc=160,yc=120;
        int x1,x2,y1,y2;
        int tmp[4]={3, 180-3, 93,180-93};
        bool antialias_on=false;

        /* Check whether lookup table fp16_cos[] and fp16_sin[] is generated */
        if( fp16_sin[30] == 0) {
                printf("%s: Start to create fixed point trigonometric table...\n",__func__);
                mat_create_fpTrigonTab();
        }


while(1) {   /////////    LOOP TEST   //////////

/* ----- 1. TEST:   Call EGI C libs to write/draw  --------------------- */
   for(k=0; k<2; k++) {
	if(!antialias_on) {
	        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY);

        	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                	                        28, 28,                         /* fw,fh */
                        	                (UFT8_PCHAR)"EGI Demo", /* pstr */
                                	        300, 1, 0,                        /* pixpl, lines, fgap */
                                        	20, 85,                            /* x0,y0, */
	                                        WEGI_COLOR_BLACK, -1, 255,        /* fontcolor, transcolor,opaque */
        	                                NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
        	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                	                        28, 28,                         /* fw,fh */
                        	                (UFT8_PCHAR)"C/C++混合编程", /* pstr */
                                	        300, 1, 0,                        /* pixpl, lines, fgap */
                                        	20, 125,                            /* x0,y0, */
	                                        WEGI_COLOR_BLACK, -1, 255,        /* fontcolor, transcolor,opaque */
        	                                NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
		fb_render(&gv_fb_dev);
		sleep(2);
	}

        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY);

#if 1   /* Tip */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
                                        24, 24,                           /* fw,fh */
                                        antialias_on?(UFT8_PCHAR)"Antialias_ON":(UFT8_PCHAR)"Antialias_OFF", /* pstr */
                                        300, 1, 0,                        /* pixpl, lines, fgap */
                                        10, 6,                            /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
#endif


        /* Draw lines with/without anti_aliasing effect */
        fbset_color(egi_color_random(color_deep));
        for(i=0; i<180; i+=3) {
                x1=xc+(r*fp16_cos[i]>>16);
                y1=yc-(r*fp16_sin[i]>>16);
                x2=xc+(r*fp16_cos[i+180]>>16);
                y2=yc-(r*fp16_sin[i+180]>>16);

                printf("i=%d: line (%d,%d)-(%d,%d) \n", i,x1,y1,x2,y2);

                gv_fb_dev.antialias_on=antialias_on;
                //fbset_color(egi_color_random(color_deep));
                if(antialias_on) {
                        draw_line_antialias(&gv_fb_dev, x1,y1, x2,y2);  /* Note: LCD coord sys! */
                }
                else
                        draw_line_simple(&gv_fb_dev, x1,y1, x2,y2);    /* Note: LCD coord sys! */

                fb_render(&gv_fb_dev);
        }

#if 1    /* Tip */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
                                        24, 24,                           /* fw,fh */
                                        antialias_on?(UFT8_PCHAR)"Antialias_ON":(UFT8_PCHAR)"Antialias_OFF", /* pstr */
                                        300, 1, 0,                        /* pixpl, lines, fgap */
                                        10, 6,                    /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
#endif

        fb_render(&gv_fb_dev);

        /* Hold on */
        sleep(1);

        /* Toggle */
        antialias_on=!antialias_on;
  }


/* ----- 2. TEST:  C++ Class ETriangle --------------------- */
	ETriangle trian;

        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY);
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
               	                        28, 28,                         /* fw,fh */
                       	                (UFT8_PCHAR)"EGI Triangle类对象", /* pstr */
                               	        300, 1, 0,                        /* pixpl, lines, fgap */
                                       	20, 85,                            /* x0,y0, */
                                        WEGI_COLOR_BLACK, -1, 255,        /* fontcolor, transcolor,opaque */
       	                                NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
               	                        28, 28,                         /* fw,fh */
                       	                (UFT8_PCHAR)"C/C++混合编程", /* pstr */
                               	        300, 1, 0,                        /* pixpl, lines, fgap */
                                       	20, 125,                            /* x0,y0, */
                                        WEGI_COLOR_BLACK, -1, 255,        /* fontcolor, transcolor,opaque */
       	                                NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	fb_render(&gv_fb_dev);
	sleep(2);

	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_BLACK);

	for(k=0; k<500; k++) {

		fbset_color2(&gv_fb_dev, egi_color_random(color_all));

		trian.draw();
		trian.random();

	        fb_render(&gv_fb_dev);
		//sleep(1);
	}

} /////////   END LOOP   /////////

	/* <<<<<  EGI general release >>>>> */
        //printf("FTsymbol_release_allfonts()...\n");
        //FTsymbol_release_allfonts();
        //printf("symbol_release_allpages()...\n");
        //symbol_release_allpages();
        printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
        //printf("egi_quit_log()...\n");
        //egi_quit_log();
        printf("<-------  END  ------>\n");

	return 0;
}


