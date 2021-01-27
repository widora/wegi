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
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */
#if 0
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_gif") != 0) {        /* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
#endif

#if 0
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

 	printf("FTsymbol_load_sysfonts()...\n");
	if(FTsymbol_load_sysfonts() !=0) {
        	printf("Fail to load FT sysfonts, quit.\n");
        	return -1;
  	}


#if 1 /* ---------- TEST: EGI_FONT_BUFFER  --------------- */
	EGI_FONT_BUFFER *fontbuff;

  while(1) {

//	fontbuff=FTsymbol_create_fontBuffer(egi_sysfonts.regular, 20,20, , 5000); /*  face, fw,  fh, wchar_t unistart, size_t size */
	fontbuff=FTsymbol_create_fontBuffer(egi_sysfonts.regular, 20,20, 0x4E00, 21000); /*  face, fw,  fh, wchar_t unistart, size_t size */
	if(fontbuff) {
		printf("Font buffer size=%zd!\n", fontbuff->size);
	}

	/* You MUST NOT release it in real application, as sysfont is referred by fontbuff! */
//        FTsymbol_release_allfonts();

//        fb_filo_flush(&gv_fb_dev); /* Flush FB filo if necessary */
//        release_fbdev(&gv_fb_dev);

	FTsymbol_free_fontBuffer(&fontbuff);

	sleep(5);
  }

	exit(0);
#endif

        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;

#if 0
        printf("start touchread thread...\n");
        egi_start_touchread();                  /* start touch read thread */
#endif

	/* Set sys FB mode */
  	fb_set_directFB(&gv_fb_dev,true);
  	fb_position_rotate(&gv_fb_dev,0);

        /* <<<<------------------  End EGI Init  ----------------->>>> */


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


#if 0   /* Strange wcode :  with Combining Diacritical Marks  */
        int m, pos;
        wchar_t wcode;
        //char *stranges="ê̄,ế,éi,ê̌,ěi,ề,èi";   /* ê̄,ế,ê̌,ề   ế =[0x1ebf]  ê̄ =[0xea  0x304]  ê̌ =[0xea  0x30c] ề =[0x1ec1] */
        char *stranges="ńňǹḿ m̀"; /*   0x144  0x148  0x1f9   ḿ =[0x1e3f  0x20]  m̀=[0x6d  0x300] */
        printf("stranges: %s\n", stranges);
        m=0; pos=0;
        while( (m=char_uft8_to_unicode( (const UFT8_PCHAR)(stranges+pos), &wcode)) > 0 )
        {
                pos+=m;
                printf("0x%x  ", wcode);
        }
        printf("\n");
        exit(1);
#endif


#if 1   /* All CJK UNICODE */
	int k;
	char strtmp[54];
	wchar_t wcode;
	typedef struct unicode_range { wchar_t start; wchar_t end;  char *name; } unicode_range;

	unicode_range unicode_cjk[]=
{
	{ 0x0000,	0x007F, 	"C0 control and Basic Latin"  },		/* 0 */
	{ 0x0080,	0x00FF,  	"C1 Control and Latin 1 Supplement" },
	{ 0x0100,	0x017F,  	"Latin Extended-A" },
	{ 0x0180,	0x024F, 	"Latin Extended-B" },
	{ 0x02B0,	0x02FF,		"Spacing Modifier" },
	{ 0x0300,	0x036F,		"Combining Diacritical Marks" },		/* 5 */
	{ 0x0370,	0x03FF,		"Greek and Coptic" },
	{ 0x2000,	0x206F,		"General Punctuation" },			/* 7 */
	{ 0x3000,	0x303F,		"CJK Symbols and Punctuation" },		/* 0x3002 。　 */
	{ 0x3040,	0x309F,		"Hiragana" },
	{ 0x30A0,	0x30FF,		"Katakana" },
	{ 0x3100,	0x312F,		"Bopomof" },
	{ 0x3300,	0x33FF,		"CJK Compatibility" },
	{ 0x3400,	0x4DB5,		"CJK Unified Ideographs Extension A" },
	{ 0x4E00,	0x9FFC,		"CJK Unified Ideographs" 	},
	{ 0xF900,	0xFAD9,		"CJK Compatibility Ideographst"  },
	{ 0x20000,	0x2A6DD,	"CJK Unified Ideographs Extension B" },
	{ 0x2A700,	0x2B734,	"CJK Unified Ideographs Extension C" },
	{ 0x2B740,	0x2B81D, 	"CJK Unified Ideographs Extension D"},
	{ 0x2B820,	0x2CEA1,	"CJK Unified Ideographs Extension E" },
	{ 0x2CEB0,	0x2EBE0,	"CJK Unified Ideographs Extension F" },
	{ 0x30000,	0x3134A,	"CJK Unified Ideographs Extension G" },
	{ 0x2F800,	0x2FA1D,	"CJK Compatibility Supplement" },
};

/* =================================================================

wcode 9 has bitmap, and advanceX=50.
wcode 13 has bitmap, and advanceX=50.
wcode 32 has no bitmap, and advanceX=12.
wcode 160 has no bitmap, and advanceX=12.
wcode 12288 has no bitmap, and advanceX=50.
wcode 12330 has bitmap, and zero width.
wcode 12331 has bitmap, and zero width.
wcode 12332 has bitmap, and zero width.
wcode 12333 has bitmap, and zero width.

================================================================== */

#if 0   /* -----TEST: single vowels ----*/
wchar_t alls[]=L"ǎǒěǐǔǚňḿ ";  // ḿ \u1e3f
while(1)
{
	for(k=0; k<sizeof(alls)/sizeof(wchar_t); k++)
	{
		wcode=alls[k];

		bzero(strtmp,sizeof(strtmp));
		sprintf(strtmp,"0x%04x", wcode);
		/* Display UNICODE */
		clear_screen(&gv_fb_dev, WEGI_COLOR_BLACK);
		FTsymbol_uft8strings_writeFB( 	&gv_fb_dev, egi_sysfonts.bold,         	/* FBdev, fontface */
					      	30, 30,(const unsigned char *)strtmp, 	/* fw,fh, pstr */
					      	320, 1, 0,                    	/* pixpl, lines, gap */
						120, 30,                           	/* x0,y0, */
                                     		WEGI_COLOR_WHITE, -1, -50,      /* fontcolor, transcolor,opaque */
	                                     	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

		/* Display font */
		FTsymbol_unicode_writeFB( &gv_fb_dev, egi_sysfonts.bold,         /* FBdev, fontface */
					      	  50, 50, wcode, NULL, 			/* fw,fh, wcode, *xleft */
						  30, 30,                          	/* x0,y0, */
                        	             	  WEGI_COLOR_RED, -1, -20);      	/* fontcolor, transcolor,opaque */

		tm_delayms(1000);
	}
}
#endif


while(1) {
	//for(k=0; k<sizeof(unicode_cjk)/sizeof(unicode_cjk[0]); k++)
	//for(k=2; k<=3; k++) //for(k=2; k<=9; k++)
	k=5;
	{
		printf("		--- %s ---\n",	unicode_cjk[k].name);
		for( wcode=unicode_cjk[k].start; wcode<=unicode_cjk[k].end; wcode++)
		{
			bzero(strtmp,sizeof(strtmp));
			sprintf(strtmp,"0x%04x", wcode);

			/* Display UNICODE */
			clear_screen(&gv_fb_dev, WEGI_COLOR_BLACK);
			FTsymbol_uft8strings_writeFB( 	&gv_fb_dev, egi_sysfonts.bold,         	/* FBdev, fontface */
					      	30, 30,(const unsigned char *)strtmp, 	/* fw,fh, pstr */
					      	320, 1, 0,                    	/* pixpl, lines, gap */
						120, 30,                           	/* x0,y0, */
                                     		WEGI_COLOR_WHITE, -1, -50,      /* fontcolor, transcolor,opaque */
	                                     	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

			/* Display font */
			FTsymbol_unicode_writeFB( &gv_fb_dev, egi_sysfonts.bold,         /* FBdev, fontface */
					      	  50, 50, wcode, NULL, 			/* fw,fh, wcode, *xleft */
						  30, 30,                          	/* x0,y0, */
                        	             	  WEGI_COLOR_RED, -1, -20);      	/* fontcolor, transcolor,opaque */

			tm_delayms(1000);
		}
	}

}
	exit(0);
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
