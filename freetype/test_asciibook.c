/*-----------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License version 2 as published by the Free Software
Foundation.
(Note: FreeType 2 is licensed under FTL/GPLv3 or GPLv2 ).

An Example to load font file to an ASCII symbol page, then display an ASCII TXT book
with this symbol page.


Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <math.h>
#include <wchar.h>
#include "egi_common.h"
#include "egi_image.h"
#include "egi_utils.h"
#include <sys/stat.h>


int main( int  argc,   char**  argv )
{
	int ret;
	int i;

	int fd;
	int fsize;
	struct stat sb;
	char *fp;

	int nwrite;
	int gap; 	/* line gap */
	int pos_rotate=0;    /* FB rotation position 0-3: Default,90,180,270 degree */

  	EGI_16BIT_COLOR font_color, bk_color;

	EGI_IMGBUF *logo_img=egi_imgbuf_new();
  	if( egi_imgbuf_loadpng("/mmc/oldman.png", logo_img) )
                return -1;
  	EGI_IMGBUF *penguin_img=egi_imgbuf_new();
  	if( egi_imgbuf_loadpng("/mmc/penguin.png", penguin_img) )
                return -1;

        char *strtest="jjgggiii}||  and he has certain parcels... \n\r  	\
you are from God and have overcome them,	\
for he who is in you is greater than 		\
he who is in the world.";

	char *strp=NULL;

	/* open and mmap txt book */
	fd=open("/home/book.txt",O_RDONLY);
	if(fd<0) {
		perror("open file");
		return -1;
	}
	/* obtain file stat */
	if ( fstat(fd,&sb)<0 ) {
		perror("fstat");
		return -2;
	}

	fsize=sb.st_size;

	/* mmap the file */
	fp=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
	if(fp==MAP_FAILED) {
		perror("mmap");
		return -3;
	}
	printf("%s\n",fp);

        /* <<<<  EGI general init >>>> */
        tm_start_egitick();
        if(egi_init_log("/mmc/log_freetype") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
        }
        if(symbol_load_allpages() !=0 ) {
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        init_fbdev(&gv_fb_dev);


while(1) {  ////////////////////////////////   LOOP TEST   ///////////////////////////////

	/* load symbol page of ASCII */
	struct symbol_page sympg_ascii={0};
	//ret=symbol_load_asciis_from_fontfile(&sympg_ascii, argv[1], 18, 18);
	//ret=symbol_load_asciis_from_fontfile(&sympg_ascii, "/mmc/fonts/vera/Vera.ttf", 18, 18);
  //ret=symbol_load_asciis_from_fontfile( &sympg_ascii, "/mmc/fonts/hansans/SourceHanSansSC-Regular.otf", 18, 18);
  ret=FTsymbol_load_asciis_from_fontfile( &sympg_ascii, "/mmc/fonts/liber/LiberationMono-Regular.ttf", 18, 18);
	if(ret) {
		exit(1);
	}

/*---------------------------------------------------------------------------------------------------
void symbol_strings_writeFB( FBDEV *fb_dev, const struct symbol_page *sym_page, unsigned int pixpl,  \
               		     unsigned int lines,  unsigned int gap, int fontcolor, int transpcolor,  \
                             int x0, int y0, const char* str, int opaque);
-----------------------------------------------------------------------------------------------------*/

  	/* set font color */
  	font_color= WEGI_COLOR_BLACK;//egi_color_random(deep);

	nwrite=0;
	strp=fp;

	/* clear screen */
 	clear_screen(&gv_fb_dev, 0x0679); //WEGI_COLOR_GRAYA); //COLOR_COMPLEMENT_16BITS(font_color));

	gap=3;

	/* set rotate FB */
	gv_fb_dev.pos_rotate=1;

  /* -------  Display txt book  ------ */
  do {

	printf("start compare...\n");

	/* --- TITLE --- */
	char * title ="The Old Man and the Sea";

	/* Clear title zone */
	if( gv_fb_dev.pos_rotate % 2)
		draw_filled_rect2(&gv_fb_dev, 0x0679, 0, 0, 319, 40-10 ); /* pos_rotate= 1,3 */
	else
		draw_filled_rect2(&gv_fb_dev, 0x0679, 0,0, 239, 60-10 );  /* pos_rotate= 0,2 */

	//int symbol_strings_writeFB( FBDEV *fb_dev, const struct symbol_page *sym_page, unsigned int pixpl,
	//                            unsigned int lines,  unsigned int gap, int fontcolor, int transpcolor,
	//                            int x0, int y0, const char* str, int opaque);
	if(gv_fb_dev.pos_rotate % 2) {
		symbol_strings_writeFB(&gv_fb_dev, &sympg_ascii, 320, 2, gap, WEGI_COLOR_WHITE,
                                                                              -1, 5, 5, title, -1);
	} else {

		symbol_strings_writeFB(&gv_fb_dev, &sympg_ascii, 240, 2, gap, WEGI_COLOR_WHITE,
                                                                              -1, 5, 5, title, -1);
	}

	/* Clear text board */
	bk_color=WEGI_COLOR_GRAY; //GRAY5;//GRAYB;
	if(gv_fb_dev.pos_rotate % 2)				  /* Landscape Display, 9 lines */
		draw_filled_rect2(&gv_fb_dev, bk_color, 0, 40-10, 319, 60+5 + 9*(sympg_ascii.symheight+gap) );
	else						    	  /* Portrait Display, 12 lines */
		draw_filled_rect2(&gv_fb_dev, bk_color, 0, 60-10, 239, 60+5 + 12*(sympg_ascii.symheight+gap) );

	/* Write strings to FB */
	printf("start strings writeFB......");
	if(gv_fb_dev.pos_rotate % 2) {
							   /* line_pix=320, lines=7, gap, (x0,y0):(5,40) */
		nwrite=symbol_strings_writeFB(&gv_fb_dev, &sympg_ascii, 320-5, 9, gap, font_color,
                                                                              -1, 5, 50-10, strp, -1);
	} else {
							   /* line_pix=240, lines=12, gap, (x0,y0):(5,0) */
		nwrite=symbol_strings_writeFB(&gv_fb_dev, &sympg_ascii, 240-5, 12, gap, font_color,
                                                                              -1, 5, 60, strp, -1);
	}

	/* Draw dividing lines */
	fbset_color(WEGI_COLOR_WHITE); //BLACK);
	if(gv_fb_dev.pos_rotate % 2)
		draw_wline_nc(&gv_fb_dev , 0, 40-10, 319, 40-10, 1); /* 1 width */
	else
		draw_wline_nc(&gv_fb_dev , 0, 60-10, 239, 60-10, 1); /* 1 width */

//	draw_wline_nc(&gv_fb_dev , 0,    60+5 + 10*(sympg_ascii.symheight+5),
//				   239,  60+5 + 10*(sympg_ascii.symheight+5),  2);

	printf("nwrite=%d bytes.\n",nwrite);

	/* Display logo oldman H200xW168 */
	if(gv_fb_dev.pos_rotate % 2) {		/* Landscape Display */
	        egi_imgbuf_windisplay(logo_img, &gv_fb_dev,-1, 0, 0, (320-168)/2, 30+(240-30-200)/2,
							logo_img->width, logo_img->height);
	} else {				/* Portrait Display */
	        egi_imgbuf_windisplay(logo_img, &gv_fb_dev,-1, 0, 0, (240-168)/2, 50+(320-50-200)/2,
							logo_img->width, logo_img->height);
	}

        /* Display EGI_IMGBUF, alway in default display mode */
//        egi_imgbuf_windisplay2(penguin_img, &gv_fb_dev, 0, 0, 0, 0, //80, 100,
//							penguin_img->width, penguin_img->height);

	tm_delayms(2000);
	strp +=nwrite;

	gv_fb_dev.pos_rotate +=1;

	/* reset pos_rotate */
	if(gv_fb_dev.pos_rotate==4)
		gv_fb_dev.pos_rotate=0;

   } while(nwrite>0);


	/* print symbol */
#if 0
	for(i=0;i<128;i++)
		symbol_print_symbol(&sympg_ascii, i,-1);
#endif

	/* release the sympage */
	free(sympg_ascii.symwidth); /* !!! free symwidth separately */
	symbol_release_page(&sympg_ascii);

	tm_delayms(1000);


}  ////////////////////////////////   END Loop test  ///////////////////////////////


	/* unmap and close file */
	munmap(fp,fsize);
	close(fd);

	/* free EGI_IMGBUF */
	egi_imgbuf_free(logo_img);
	egi_imgbuf_free(penguin_img);

	/* Free EGI */
        release_fbdev(&gv_fb_dev);
        symbol_release_allpages();
        egi_quit_log();
}
