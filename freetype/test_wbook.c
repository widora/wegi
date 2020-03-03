/*-------------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License version 2 as published by the Free Software
Foundation.
(Note: FreeType 2 is licensed under FTL/GPLv3 or GPLv2 ).

An Example of fonts demonstration based on example1.c in FreeType 2 library docs.


			<<<	--- Glossary ---	>>>

	( Refering to: https://freetype.org/freetype2/docs/tutorial )

character code:		A unique value that defines the character for a given encoding.

charmap:		A table in face object that converts character codes to glyph indices.
			Note:
			1. Most fonts contains a charmap that converts Unicode character codes to
		  	   glyph indices.
			2. FreeType tries to select a Unicode charmp when a new face object
			   is created. and it emulates a Unicode charmap if the font doesn't
			   contain such a charmap.

bitmap_left:		A member of FT_GlyphSlot, the bitmap's left bearing expressed in integer pixels,
			only valid if the format is BITMAP. It's the distance from the origin to the
			leftmost border of the glyph image.

bitmap_top:		A member of FT_GlyphSlot, the bitmap's top bearing expressed in integer pixels.
			It's the distance from the baseline to the top-most glyph scanline.

units_per_EM            The number of font units per EM square for a face, relevant for scalable
		        fonts only. Noticed that all characters have the same height.
		        Typically 1000 for type-1 fonts and 1024 or 2048 for TrueType fonts.

DPI:			Dot per inch, 1 point = 1/72 inch.
PPI:			Pixel per inch, pixel_size = point_size * DPI / 72
26.6 pixel format:	= 1/64th of a pixel per unit, where 64=2^6
			And fixed point type 16.16 is 1/65536 per unit, where 65536=2^16


	  typedef struct  FT_CharMapRec_
	  {
	    FT_Face      face;
	    FT_Encoding  encoding;
	    FT_UShort    platform_id;
	    FT_UShort    encoding_id;
	  } FT_CharMapRec;


CJK:	Chinese, Japanese, Korean
TTF:	True Type Font
TTC:	True Type Collections
OTF:	Open Type Font (Post-script)
SIL:	The SIL Open Font License(OFL) is a free, libre and open source license.

UNICODE HAN DATABASE(UNIHAN): Unicode Standard Annex #38 http://www.unicode.org/reports/tr38/tr38-27.html
Basic Han Unicode range: U+4E00 - U+9FA5

			<<<	--- Some Open/Free Fonts --- 	>>>

Source_Han_San:		A set of OpenType/CFF Pan-CJK fonts. 		( SIL v1.1 )
Source_Han_Serif:	A set of OpenType/CFF Pan-CJK fonts. 		( SIL v1.1 )
Wang_Han_Zong:		A set of traditional/simple chinese fonts.	( GPL v2 )

Liberation-fonts:	A set of western fonts whose layout is compatible to Times New Roman,
			Arial, and Courier New. 			( SIL v1.1 )
Bitstream Vera Fonts	A set of western fonts for GNOME. 	( Bitstream Vera Fonts Copyright )


Notes:

1.  Default encoding environment shall be set to be the same, UFT-8 etc, for both codes compiling
    and running.
2.  Refer to: zenozeng.github.io/Free-Chinese-Fonts for more GPL fonts.
3.  Input text with chinese traditional style when use some chinese fonts, like
    WHZ fonts etc, or it may not be found in its font library.
4.  Source Han Sans support both simple and traditional Chineses text encoding.
5.  FT_Load_Char() calls FT_Get_Char_Index() and FT_Load_Glyph().
6.  "For optional rendering on a screen the bitmap should be used as an alpha channle
    in linear blending with gamma correction." --- FreeType Tutorial 1

7. 				-----  WARNING  -----
    Due to lack of Gamma Correction(?) in color blend macro, there may be some dark/gray
    lines/dots after font bitmap and backgroud blending, especially for two contrasting
    colors/bright colors. For example, white letters and black canvas.
    Select a dark color for font and/or a compatible color for backgroud to weaken the effect.

8.  TODO: Use libiconv to convert between char and wchar_t with different encodings.
9.  This demonstration program is only for scalable fonts! You must make some modification to
    apply for fixed type fonts.
10. Only horizontal text layout is considered.
11. Gap between two characters in horizontal text layout is included/presented in advance.x.

Midas Zhou
midaszhou@yahoo.com
--------------------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <math.h>
#include <wchar.h>
#include <sys/stat.h>
#include <freetype2/ft2build.h>
#include <freetype2/ftglyph.h>
#include "egi_common.h"
#include "egi_image.h"
#include "egi_utils.h"
#include "egi_FTsymbol.h"

#include FT_FREETYPE_H


int main( int  argc,   char**  argv )
{
  EGI_16BIT_COLOR font_color;
  int 		i,j,k;
  int		ret;

  int fd;
  int fsize;
  struct stat sb;
  unsigned char *fp;

  wchar_t *wbook=L"人心生一念，天地尽皆知。善恶若无报，乾坤必有私。\
　　那菩萨闻得此言，满心欢喜，对大圣道：“圣经云：‘出其言善。\
　　则千里之外应之；出其言不善，则千里之外适之。’你既有此心，待我到了东土大唐国寻一个取经的人来，教他救你。你可跟他做个徒弟，秉教伽持，入我佛门。再修正果，如何？”大圣声声道：“愿去！愿去！”菩萨道：“既有善果，我与你起个法名。”大圣道：“我已有名了，叫做孙悟空。”菩萨又喜道：“我前面也有二人归降，正是‘悟’字排行。你今也是‘悟’字，却与他相合，甚好，甚好。这等也不消叮嘱，我去也。”那大圣见性明心归佛教，这菩萨留情在意访神谱。\
　　他与木吒离了此处，一直东来，不一日就到了长安大唐国。敛雾收云，师徒们变作两个疥癫游憎，入长安城里，竟不觉天晚。行至大市街旁，见一座土地庙祠，二人径进，唬得那土地心慌，鬼兵胆战。知是菩萨，叩头接入。那土地又急跑报与城隍社令及满长安城各庙神抵，都来参见，告道：“菩萨，恕众神接迟之罪。”菩萨道：“汝等不可走漏消息。我奉佛旨，特来此处寻访取经人。借你庙宇，权住几日，待访着真僧即回。”众神各归本处，把个土地赶到城隍庙里暂住，他师徒们隐遁真形。\
　　毕竟不知寻出那个取经来，且听下回分解。";

  int fh,fw; /* font Height,Width */
  int lines;
  int pixpl;
  int gap;   /* line gap */
  int offp;
  int xleft;
  int x0,y0;
  int tx0;
  int wtotal;
  int nwrite;

  EGI_IMGBUF *logo_img=NULL;
  EGI_IMGBUF *penguin_img=NULL;



#if 1	/////////////// 	char_unicode_to_uft8( ) 	/////////////
	char char_uft8[4+1];
	whart_t *wp=wbook;

	while(*wp) {
		memset(char_uft8,0,sizeof(char_uft8));

		if(char_unicode_to_utf8(wp, char_uft8)>0)
			printf("%s",char_uft8);
		else
			exit(1);

		wp++;
	}
	printf("\n");
	exit(0);

#endif

        /* EGI general init */
        tm_start_egitick();

while(1) { ///////////////////////  LOOP TEST  ////////////////////////

        if(egi_init_log("/mmc/log_wbook") != 0) {
                printf("Fail to init logger, quit.\n");
                return -1;
        }
        if(symbol_load_allpages() !=0 ) {
                printf("Fail to load symbol pages, quit.\n");
                return -2;
        }
        if(FTsymbol_load_allpages() !=0 ) {  /* and FT fonts LIBS */
                printf("Fail to load FT symbol pages, quit.\n");
                return -2;
        }
        if(FTsymbol_load_appfonts() !=0 ) {  /* and FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }


        init_fbdev(&gv_fb_dev);

	/* load png to imgbuf */
	logo_img=egi_imgbuf_new();
  	if( egi_imgbuf_loadpng("/mmc/tang2.png", logo_img) )
		return -1;
	penguin_img=egi_imgbuf_new();
  	if( egi_imgbuf_loadpng("/mmc/penguin2.png", penguin_img) )
		return -1;


	/* open wchar book file */
	fd=open("/mmc/xyj_uft8.txt",O_RDONLY);
//	fd=open("/home/memo.txt",O_RDONLY);
	if(fd<0) {
		perror("open file");
		return -1;
	}
	/* obtain file stat */
	if( fstat(fd,&sb)<0 ) {
		perror("fstat");
		return -2;
	}
	fsize=sb.st_size;
	/* mmap txt file */
	fp=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
	if(fp==MAP_FAILED) {
		perror("mmap");
		return -3;
	}

        /* --- TITLE --- */
        wchar_t* title =L"大 唐 西 游 记";

	/* get total wchars in the book */
 	wtotal=cstr_strcount_uft8(fp);
	printf(" Total %d characters in the book.\n",wtotal);

   EGI_PLOG(LOGLV_CRITICAL,"---------- ebook START --------\n");

   fw=18; fh=20;
   offp=0;
   lines=10;
   gap=5;
   x0=5;y0=60;
   do {
	/* clear screen */
	clear_screen(&gv_fb_dev, 0xfff9); //0x0679); //COLOR_COMPLEMENT_16BITS(font_color));

        /* Ajust params according to FB pos_rotate */
        fbset_color(WEGI_COLOR_BLACK);
        if(gv_fb_dev.pos_rotate % 2) {
	        /* Clear title zone */
                draw_filled_rect2(&gv_fb_dev, 0x0679, 0, 0, 319, 50 ); /* pos_rotate= 1,3 */
	        /* Draw dividing lines */
                draw_wline_nc(&gv_fb_dev , 0, 50, 319, 50, 1); /* 1 width */
		lines=7;
		pixpl=320;
		tx0=85;
	}
        else {
	        /* Clear title zone */
                draw_filled_rect2(&gv_fb_dev, 0x0679, 0,0, 239, 50 );  /* pos_rotate= 0,2 */
	        /* Draw dividing lines */
                draw_wline_nc(&gv_fb_dev , 0, 50, 239, 50, 1); /* 1 width */
		lines=10;
		pixpl=240;
		tx0=45;
	}

	/* write book title  */
	nwrite=FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,  /* FBdev, fontface */
					   25, 25, title,		/* fw,fh, pstr */
                               		   pixpl-45, 1,  gap,		/* pixpl, lines, gap */
                               		   tx0, 8,			/* x0,y0, */
                               		   WEGI_COLOR_BLACK, -1, -1); 	/* fontcolor, stranscolor,opaque */

	/* write book content: UFT-8 string to FB */
//	nwrite=FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.regular,  /* FBdev, fontface */
	nwrite=FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.regular,  /* FBdev, fontface */
					   fw, fh, wbook+offp, //fp+offp,	/* fw,fh, pstr */
                               		   pixpl-x0, lines,  gap,		/* pixpl, lines, gap */
                               		   x0, y0,			/* x0,y0, */
                               		   WEGI_COLOR_BLACK, -1, -1); 	/* fontcolor, stranscolor,opaque */
	printf("nwrite=%d bytes\n",nwrite);
	offp+=nwrite;

	/* Dispay EGI_IMGBUF */
	egi_imgbuf_windisplay(penguin_img, &gv_fb_dev, -1, 0, 0, 0, 0, penguin_img->width, penguin_img->height);
	egi_imgbuf_windisplay2(logo_img, &gv_fb_dev, 0, 0, 30, 80, logo_img->width, logo_img->height);

	tm_delayms(3000);

        /* reset pos_rotate */
        gv_fb_dev.pos_rotate +=1;
        if(gv_fb_dev.pos_rotate==4)
                gv_fb_dev.pos_rotate=0;

   } while(nwrite>0);

   EGI_PLOG(LOGLV_CRITICAL,"---------- ebook END --------\n");

	/* put a logo */
//	egi_imgbuf_blend_imgbuf(eimg, 0, -10, logo_img);
//	egi_imgbuf_blend_imgbuf(eimg, 240-85, 320-85, pinguin_img);

	/* free EGI_IMGBUF */
	egi_imgbuf_free(logo_img);
	egi_imgbuf_free(penguin_img);

	/* unmap and close fd */
	munmap(fp, fsize);
	close(fd);

FONT_FAILS:
	/* Release EGI fonts */
//	FTsymbol_release_library(&egi_appfonts);

	/* Free EGI */
        release_fbdev(&gv_fb_dev);
        symbol_release_allpages();
	FTsymbol_release_allpages();
	FTsymbol_release_allfonts();
        egi_quit_log();

 }  ///////////////////////  LOOP TEST  END  ////////////////////////

  return 0;
}

/* EOF */
