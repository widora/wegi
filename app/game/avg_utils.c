/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-------------------------------------------------------------------*/
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "page_avenger.h"
#include "avg_mvobj.h"



/*---------------------------
Absolute value of an integer.
---------------------------*/
int avg_intAbs(int a)
{
	return a > 0 ? a : -a;
}


/*--------------------------
        Game README
--------------------------*/
void game_readme(void)
{
	int rad=16; /* radius for pad */

        /* title */
        const wchar_t *title=L"AVENGER V1.0";

        /* README */
        const wchar_t *readme=L"    复仇之箭 made by EGI\n \
 运行平台 WIDORA_NEO\n \
   更猛烈的一波攻击来袭! \n \n \
             READY?!!";

	/* Create a pad */
#if 1
		/* (int height, int width, unsigned char alpha, EGI_16BIT_COLOR color) */
 	EGI_IMGBUF *pad=egi_imgbuf_create(200, 220, 200, WEGI_COLOR_LTBLUE);
		/* ( EGI_IMGBUF *eimg, enum imgframe_type type, int alpha, int pn, const int *param );  */
	egi_imgbuf_setFrame( pad, frame_round_rect, -1, 1, &rad);
        egi_imgbuf_windisplay(  pad, &gv_fb_dev, -1,	   /* EGI_IMGBUF*, FBDEV*, int subcolor */
                                0, 0, 10, 50,              /* int xp, int yp, int xw, int yw */
				pad->width, pad->height    /*	int winw, int winh */
			     );
	/* free */
	egi_imgbuf_free(pad);

#endif

        /*  title  */
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,     /* FBdev, fontface */
                                          24, 24, title,                /* fw,fh, pstr */
                                          240, 1,  6,                   /* pixpl, lines, gap */
                                          30, 60,                      /* x0,y0, */
                                          WEGI_COLOR_DARKRED, -1, -1 );     /* fontcolor, transcolor,opaque */

        /* README */
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,   /* FBdev, fontface */
                                          18, 18, readme,                /* fw,fh, pstr */
                                          240, 6,  7,                    /* pixpl, lines, gap */
                                          0, 100,                        /* x0,y0, */
                                          WEGI_COLOR_DARKRED, -1, -1 );      /* fontcolor, transcolor,opaque */

	/* free */
}


/*----------------------------------
	Game leve up info.
----------------------------------*/
void  game_levelUp(bool levelup, int level)
{
	int rad=16; /* radius for pad */

        const wchar_t *strWin=L"  成功击退一波敌人!\n \
\n参加佐大OpenWrt培训班可以学习更多技能哦! \
\n  AVENGER V1.0";

        const wchar_t *strLose=L"  基地炸毁,任务失败!\n	\n 参加佐大OpenWrt培训班可以学习更多技能哦! \n \
  AVENGER V1.0";


	/* Create a pad */
		/* (int height, int width, unsigned char alpha, EGI_16BIT_COLOR color) */
 	EGI_IMGBUF *pad=egi_imgbuf_create(180, 200, 170, WEGI_COLOR_GRAYA);
		/* ( EGI_IMGBUF *eimg, enum imgframe_type type, int alpha, int pn, const int *param );  */
	egi_imgbuf_setFrame( pad, frame_round_rect, -1, 1, &rad);
        egi_imgbuf_windisplay(  pad, &gv_fb_dev, -1,	   /* EGI_IMGBUF*, FBDEV*, int subcolor */
                                0, 0, 20, 50,              /* int xp, int yp, int xw, int yw */
				pad->width, pad->height    /*	int winw, int winh */
			     );

        /* Game Level Info */
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,   /* FBdev, fontface */
                                          18, 18, (levelup==true) ? strWin : strLose,  /* fw,fh, pstr */
                                          200-10, 6,  7,                    /* pixpl, lines, gap */
                                          20+5, 50+20,                        /* x0,y0, */
                                          WEGI_COLOR_FIREBRICK, -1, -1 );      /* fontcolor, transcolor,opaque */


	/* Show level up information */


	/* free */
	egi_imgbuf_free(pad);
}
