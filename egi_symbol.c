/*----------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


For test only!

1. Symbols may be icons,fonts, image, a serial motion picture.... etc.
2. One img symbol page size is a 240x320x2Bytes data , 2Bytes for 16bit color.
   each row has 240 pixels. usually it is a pure color data file.
3. One mem symbol page size is MAX.240X320X2Bytes, usually it is smaller because
   its corresponding img symbol page has blank space.
   Mem symbol page is read and converted from img symbol page, mem symbol page
   stores symbol pixels data row by row consecutively for each symbol. and it
   is more efficient for storage and search.
   A mem symbol page may be saved as a file.
5. All symbols in a page MUST have the same height, and each row MUST has the same
   number of symbols.
6. The first symbol in a img page shuld not be used, code '0' usually will be treated
   as and end token for a char string.
7. when you edit a symbol image, don't forget to update:
	7.1 sympg->maxnum;
	7.2 xxx_width[N] >= sympg->maxnu;
	7.3 modify symbol structre in egi_symbol.h accordingly.

TODO:
0.  different type symbol now use same writeFB function !!!!! font and icon different writeFB func????
0.  if image file is not complete.
1.  void symbol_save_pagemem( )
2.  void symbol_writeFB( ) ... copy all data if no transp. pixel applied.
3.  data encode.
4.  symbol linear enlarge and shrink.
5. To read FBDE vinfo to get all screen/fb parameters as in fblines.c, it's improper in other source files.

Journal:
2021-03-10:
	1. symbol_writeFB(): Add check zbuff.
2021-03-25:
	1. symbol_writeFB(): Add zpos for zbuff[].

Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------*/
//#define _GNU_SOURCE	/* for O_CLOEXEC flag */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /*close*/
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "egi_fbgeom.h"
#include "egi_image.h"
#include "egi_symbol.h"
#include "egi_debug.h"
#include "egi_log.h"
#include "egi_timer.h"
#include "egi_math.h"


/*--------------------(  testfont  )------------------------
  1.  ascii 0-127 symbol width,
  2.  5-pixel blank space for nonprintable symbols, though 0-pixel seems also OK.
  3.  Please change the 'space' width according to your purpose.
*/
static int testfont_width[16*8] = /* check with maxnum */
{
	/* give return code a 0 width in a txt display  */
//	5,5,5,5,5,5,5,5,5,5,0,5,5,5,5,5, /* nonprintable symbol, give it 5 pixel wide blank */
//	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, /* nonprintable symbol, give it 5 pixel wide blank */
	0,0,0,0,0,0,0,0,0,30,0,0,0,0,0,0, /* 9-TAB 50pixels,  nonprintable symbol */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	5,7,8,10,11,15,14,5,6,6,10,10,5,6,5,8, /* space,!"#$%&'()*+,-./ */
	11,11,11,11,11,11,11,11,11,11,6,6,10,10,10,10, /* 0123456789:;<=>? */
	19,12,11,11,13,10,10,13,13,5,7,11,9,18,14,14, /* @ABCDEFGHIJKLMNO */
	11,14,11,10,10,13,12,19,11,10,10,6,8,6,10,10, /* PQRSTUVWXYZ[\]^_ */
	6,10,11,9,11,10,6,10,11,5,5,10,5,17,11,10, /* uppoint' abcdefghijklmnop */
	11,11,7,8,7,11,9,15,9,10,8,7,10,7,10,5 /*pqrstuvwxyz{|}~ blank */
};
/* symbole page struct for testfont */
EGI_SYMPAGE sympg_testfont=
{
	.symtype=symtype_font,
	#ifdef LETS_NOTE
	.path="/home/midas-zhou/egi/testfont.img",
	#else
	.path="/home/testfont.img",
	#endif
	.bkcolor=0xffff,
	.data=NULL,
	.maxnum=128-1,
	.sqrow=16,
	.symheight=26,
	.symwidth=testfont_width, /* width list */
};



/*--------------------------(  numbfont  )-----------------------------------
 	big number font 0123456789:
*/
static int numbfont_width[16*8] = /* check with maxnum */
{
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* nonprintable symbol */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	15,15,15,15,15,15,15,15,15,15,15,0,0,0,0,0, /* 0123456789: */
};
/* symbole page struct for numbfont */
EGI_SYMPAGE sympg_numbfont=
{
	.symtype=symtype_font,
	#ifdef LETS_NOTE
	.path="/home/midas-zhou/egi/numbfont.img",
	#else
	.path="/home/numbfont.img",
	#endif
	.bkcolor=0x0000,
	.data=NULL,
	.maxnum=16*4-1,
	.sqrow=16,
	.symheight=20,
	.symwidth=numbfont_width, /* width list */
};


/*--------------------------(  buttons H60 )-----------------------------------*/
static int buttons_width[4*5] =  /* check with maxnum */
{
	60,60,60,60,
	60,60,60,60,
	60,60,60,60,
	60,60,60,60,
	60,60,60,60,
};
EGI_SYMPAGE sympg_buttons=
{
	.symtype=symtype_icon,
	#ifdef LETS_NOTE
	.path="/home/midas-zhou/egi/buttons.img",
	#else
	.path="/home/buttons.img",
	#endif
	.bkcolor=0x0000,
	.data=NULL,
	.maxnum=4*5-1, /* 5 rows of ioncs */
	.sqrow=4, /* 4 icons per row */
	.symheight=60,
	.symwidth=buttons_width, /* width list */
};


/*--------------------------(  small buttons W48H60 )-----------------------------------*/
static int sbuttons_width[5*3] =  /* check with maxnum */
{
	48,48,48,48,
	48,48,48,48,
	48,48,48,48,
};
EGI_SYMPAGE sympg_sbuttons=
{
	.symtype=symtype_icon,
	#ifdef LETS_NOTE
	.path="/home/midas-zhou/egi/sbuttons.img",
	#else
	.path="/home/sbuttons.img",
	#endif
	.bkcolor=0x0000,
	.data=NULL,
	.maxnum=5*3-1, 	/* 5 rows of ioncs */
	.sqrow=5, 	/* 4 icons per row */
	.symheight=48, 	/*60, */
	.symwidth=sbuttons_width, /* width list */
};


/*--------------------------(  30x30 icons for Home Head-Bar )-----------------------------------*/
static int icons_width[8*12] =  /* element number MUST >= maxnum */
{
        30,30,30,30,30,30,30,30,
        30,30,30,30,30,30,30,30,
        30,30,30,30,30,30,30,30,
        30,30,30,30,30,30,30,30,
        30,30,30,30,30,30,30,30,
        30,30,30,30,30,30,30,30,
        30,30,30,30,30,30,30,30,
        30,30,30,30,30,30,30,30,
        30,30,30,30,30,30,30,30,
        30,30,30,30,30,30,30,30,
};
EGI_SYMPAGE sympg_icons=
{
        .symtype=symtype_font,
	#ifdef LETS_NOTE
	.path="/home/midas-zhou/egi/icons.img",
	#else
        .path="/home/icons.img",
	#endif
        .bkcolor=0x0000,
        .data=NULL,
        .maxnum=11*8-1, /* 11 rows of ioncs */
        .sqrow=8, /* 8 icons per row */
        .symheight=30,
        .symwidth=icons_width, /* width list */
};

/* put load motion icon array for CPU load
   !!! put code 0 as end of string */
char symmic_cpuload[6][5]= /* sym for motion icon */
{
	{40,41,42,43,0}, /* very light loadavg 1 */
	{44,45,46,47,0}, /* light  loadavg 2 */
	{48,49,50,51,0}, /* moderate  loadavg 3 */
	{52,53,54,55,0}, /* heavy loadavg 4 */
	{56,57,58,59,0}, /* very heavy loadavg 5 */
	{60,61,62,63,0},  /* red alarm loadavg 6 >5 overload*/
};

/* IoT mmic */
char symmic_iotload[9]=
{ 16,17,18,19,20,21,22,23,0}; /* with end token /0 */


/*------------------(  60x60 icons for PLAYS and ARROWS )-----------------*/
static int icons_2_width[4*5] =  /* element number MUST >= maxnum */
{
	60,60,60,60,
	60,60,60,60,
	60,60,60,60,
	60,60,60,60,
	60,60,60,60,
};
/* symbole page struct for testfont */
EGI_SYMPAGE sympg_icons_2=
{
        .symtype=symtype_icon,
	#ifdef LETS_NOTE
	.path="/home/midas-zhou/egi/icons_2.img",
	#else
        .path="/home/icons_2.img",
	#endif
        .bkcolor=0x0000,
        .data=NULL,
        .maxnum=4*5-1, /* 5 rows of ioncs */
        .sqrow=4, /* 8 icons per row */
        .symheight=60,
        .symwidth=icons_2_width, /* width list */
};


/* for HeWeather Icons */
static int heweather_width[1*1] =
{
	60
};
EGI_SYMPAGE sympg_heweather =
{
        .symtype=symtype_icon,
        .path=NULL,			/* NOT applied */
        .bkcolor=-1,			/* <0, no transpcolor applied */
        .data=NULL,			/* 16bit per pixle image data */
	.alpha=NULL,			/* 8bit per pixle alpha data */
        .maxnum=1*1-1,
        .sqrow=1, 			/* 1 icons per row */
        .symheight=60,
        .symwidth=heweather_width, 	/* width list */
};



/* -----  All static functions ----- */
static uint16_t *symbol_load_page(EGI_SYMPAGE *sym_page);

/* ----------------------------------------------------------------------
TODO: Only for one symbol NOW!!!!!

load a symbol_page struct from a EGI_IMGBUF
@sym_page:   pointer to a symbol_page.
@imgbuf:     a EGI_IMGBUF holding that image data

Return:
	0	ok
	<0	fails
------------------------------------------------------------------------*/
int symbol_load_page_from_imgbuf(EGI_SYMPAGE *sym_page, EGI_IMGBUF *imgbuf)
{
	int i;
	int off;
	int data_size;

	if(imgbuf==NULL || imgbuf->imgbuf==NULL || sym_page==NULL) {
		printf("%s: Invalid input data!\n",__func__);
		return -1;
	}
	data_size=(imgbuf->height)*(imgbuf->width)*2; /* 16bpp color */

        /* malloc mem for symbol_page.symoffset */
        sym_page->symoffset = calloc(1, ((sym_page->maxnum)+1) * sizeof(int) );
        if(sym_page->symoffset == NULL) {
                        printf("%s: fail to malloc sym_page->symoffset!\n",__func__);
                        return -1;
        }

	/* set symoffset */
        off=0;
        for(i=0; i<=sym_page->maxnum; i++)
        {
                sym_page->symoffset[i]=off;
                off += sym_page->symwidth[i] * (sym_page->symheight) ;/* in pixel */
        }

	/* copy color data */
	printf("copy color data...\n");
	sym_page->data=calloc(1,data_size);
	if(sym_page->data==NULL) {
		printf("%s: Fail to alloc sym_page->data!\n",__func__);
		return -2;
	}
	memcpy(sym_page->data, imgbuf->imgbuf, data_size);

	/* copy alpha */
	printf("copy alpha value...\n");
	if(imgbuf->alpha) {
		sym_page->alpha=calloc(1,data_size>>1); /* 8bpp for alpha */
		if(sym_page->alpha==NULL) {
			printf("%s: Fail to alloc sym_page->alpha!\n",__func__);
			free(sym_page->data);
			return -3;
		}
		memcpy(sym_page->alpha, imgbuf->alpha, data_size>>1);
		sym_page->bkcolor=-1; /* use alpha instead of bkcolor */
	}

	return 0;
}


/*---------------------------------------------------------
Load all symbol files into mem pages

! Don't forget to change symbol_free_allpages() accordingly !

return:
	0	OK
	<0	Fail
---------------------------------------------------------*/
int symbol_load_allpages(void)
{
        /* load testfont */
        if(symbol_load_page(&sympg_testfont)==NULL)
                goto FAIL;
        /* load numbfont */
        if(symbol_load_page(&sympg_numbfont)==NULL)
                goto FAIL;
        /* load buttons icons */
        if(symbol_load_page(&sympg_buttons)==NULL)
                goto FAIL;
        /* load small buttons icons */
        if(symbol_load_page(&sympg_sbuttons)==NULL)
                goto FAIL;
        /* load icons for home head-bar*/
        if(symbol_load_page(&sympg_icons)==NULL)
                goto FAIL;
        /* load icons for PLAYERs */
        if(symbol_load_page(&sympg_icons_2)==NULL)
                goto FAIL;

	return 0;
FAIL:
	symbol_release_allpages();
	return -1;
}

/* --------------------------------------
	Free all mem pages
----------------------------------------*/
void symbol_release_allpages(void)
{
	symbol_release_page(&sympg_testfont);
	symbol_release_page(&sympg_numbfont);
	symbol_release_page(&sympg_buttons);
	symbol_release_page(&sympg_sbuttons);
	symbol_release_page(&sympg_icons);
	symbol_release_page(&sympg_icons_2);

}


/*----------------------------------------------------------------
   load an img page file
   1. direct mmap.
      or
   2. load to a mem page. (current implementation)

path: 	path to the symbol image file
num: 	total number of symbols,or MAX code number-1;
height: heigh of all symbols
width:	list for all symbol widthes
sqrow:	symbol number in each row of an img page

------------------------------------------------------------------*/
static uint16_t *symbol_load_page(EGI_SYMPAGE *sym_page)
{
	int fd;
	int datasize; /* memsize for all symbol data */
	int i,j;
	int x0=0,y0=0; /* origin position of a symbol in a image page, left top of a symbol */
	int nr,no;
	int offset=0; /* in pixel, offset of data mem for each symbol, NOT of img file */
	int all_height; /* height for all symbol in a page */
	int width; /* width of a symbol */

	if(sym_page==NULL)
		return NULL;

	/* open symbol image file */
	fd=open(sym_page->path, O_RDONLY|O_CLOEXEC);
	if(fd<0)
	{
		printf("fail to open symbol file %s!\n",sym_page->path);
			perror("open symbol image file");
		return NULL;
	}

#if 1   /*------------  check an ((unloaded)) page structure -----------*/

	/* check for maxnum */
	if(sym_page->maxnum < 0 )
	{
		printf("symbol_load_page(): symbol number less than 1! fail to load page.\n");
		return NULL;
	}
	/* check for data */
	if(sym_page->data != NULL)
	{
		printf("symbol_load_page(): sym_page->data is NOT NULL! symbol page may be already \
				loaded in memory!\n");
		return sym_page->data;
	}
	/* check for symb_index */
	if(sym_page->symwidth == NULL)
	{
		printf("symbol_load_page(): symbol width list is empty! fail to load symbole page.\n");
		return NULL;
	}

#endif

	/* get height for all symbols */
	all_height=sym_page->symheight;

	/* malloc mem for symbol_page.symoffset */
	sym_page->symoffset = malloc( ((sym_page->maxnum)+1) * sizeof(int) );
	if(sym_page->symoffset == NULL) {
			printf("symbol_load_page():fail to malloc sym_page->symoffset!\n");
			return NULL;
	}

	/* calculate symindex->sym_offset for each symbol
           and mem size needed for all symbols */
	datasize=0;
	for(i=0;i<=sym_page->maxnum;i++)
	{
		sym_page->symoffset[i]=datasize;
		datasize += sym_page->symwidth[i] * all_height ;/* in pixel */
	}

	/* malloc mem for symbol_page.data */
	sym_page->data= malloc( datasize*sizeof(uint16_t) ); /* memsize in pixel of 16bit color*/
	{
		if(sym_page->data == NULL)
		{
			EGI_PLOG(LOGLV_ERROR,"symbol_load_page(): fail to malloc sym_page->data! ???Maybe element number of symwidth[] is less than symbol_page.maxnum \n");
			symbol_release_page(sym_page);
			return NULL;
		}
	}


	/* read symbol pixel data from image file to sym_page.data */
        for(i=0; i<=sym_page->maxnum; i++) /* i for each symbol */
        {

		/* a symbol width MUST NOT be zero.!   --- zero also OK, */
/*
		if( sym_page->symwidth[i]==0 )
		{
			printf("symbol_load_page(): sym_page->symwidth[%d]=0!, a symbol width MUST NOT be zero!\n",i);
			symbol_release_page(sym_page);
			return NULL;
		}
*/
		nr=i/(sym_page->sqrow); /* locate row number of the page */
		no=i%(sym_page->sqrow); /* in symbol,locate order number of a symbol in a row */

		if(no==0) /* reset x0 for each row */
			x0=0;
		else
			x0 += sym_page->symwidth[i-1]; /* origin pixel order in a row */

                y0 = nr * all_height; /* origin pixel order in a column */
                //printf("x0=%d, y0=%d \n",x0,y0);

		offset=sym_page->symoffset[i]; /* in pixel, offset of the symbol in mem data */
		width=sym_page->symwidth[i]; /* width of the symbol */
#if 0 /*  for test -----------------------------------*/
		if(i=='M')
			printf(" width of 'M' is %d, offset is %d \n",width,offset);
#endif /*  test end -----------------------------------*/

                for(j=0;j<all_height;j++) /* for each pixel row of a symbol */
                {
                        /* in image file: seek position for pstart of a row, in bytes. 2bytes per pixel */
                        if( lseek(fd,(y0+j)*SYM_IMGPAGE_WIDTH*2+x0*2,SEEK_SET)<0 ) /* in bytes */
                        {
                                perror("lseek symbol image file");
				//EGI_PLOG(LOGLV_ERROR,"lseek symbol image fails.%s \n",strerror(errno));
				symbol_release_page(sym_page);
                                return NULL;
                        }

                        /* in mem data: read each row pixel data form image file to sym_page.data,
			2bytes per pixel, read one row pixel data each time */
                        if( read(fd, (uint8_t *)(sym_page->data+offset+width*j), width*2) < width*2 )
                        {
                                perror("read symbol image file");
	 			symbol_release_page(sym_page);
                                return NULL;
                        }

                }

        }
        printf("finish reading %s.\n",sym_page->path);

#if 0	/* for test ---------------------------------- */
	i='M';
	offset=sym_page->symoffset[i];
	width=sym_page->symwidth[i];
	for(j=0;j<all_height;j++)
	{
		for(k=0;k<width;k++)
		{
			if( *(uint16_t *)(sym_page->data+offset+width*j+k) != 0xFFFF)
				printf("*");
			else
				printf(" ");
		}
		printf("\n");
	}
#endif /*  test end -----------------------------------*/

	close(fd);
	EGI_PLOG(LOGLV_INFO,"symbol_load_page(): succeed to load symbol image file %s!\n", sym_page->path);
	//printf("sym_page->data = %p \n",sym_page->data);
	return (uint16_t *)sym_page->data;
}


/*--------------------------------------------------
	Release data in a symbol page
---------------------------------------------------*/
void symbol_release_page(EGI_SYMPAGE *sym_page)
{
	if(sym_page==NULL)
		return;

	if(sym_page->data != NULL) {
		//printf("%s: free(sym_page->data) ...\n",__func__);
		free(sym_page->data);
		sym_page->data=NULL;
	}

	if(sym_page->alpha != NULL) {
		//printf("%s: free(sym_page->alpah) ...\n",__func__);
		free(sym_page->alpha);
		sym_page->alpha=NULL;
	}

	if(sym_page->symoffset != NULL) {
		//printf("%s: free(sym_page->symoffset) ...\n",__func__);
		free(sym_page->symoffset);
		sym_page->symoffset=NULL;
	}

	/* TODO:  1. For symbol page: symwidth is NOT dynamically allocated
	 *	  2. For FTsymbol page: symwidth is dynamically allocated
	 */
//	sym_page->symwidth=NULL;

//	if(sym_page->symwidth != NULL) {
//		printf("%s: free(sym_page->symwidth) ...\n",__func__);
//		free(sym_page->symwidth);
//		sym_page->symwidth=NULL;
//	}

	/* ? static value */
//	sym_page->path=NULL;


}


/*-----------------------------------------------------------------------
check integrity of a ((loaded)) page structure

sym_page: a loaded page
func:	  function name of the caller

return:
	0	OK
	<0	fails
-----------------------------------------------------------------------*/
int symbol_check_page(const EGI_SYMPAGE *sym_page, char *func)
{

	/* check sym_page */
	if(sym_page==NULL)
        {
                printf("%s(): symbol_page is NULL! .\n",func);
                return -1;
        }
        /* check for maxnum */
        if(sym_page->maxnum < 0 )
        {
                printf("%s(): symbol number less than 1! fail to load page.\n",func);
                return -2;
        }
        /* check for data */
        if(sym_page->data == NULL)
        {
                printf("%s(): sym_page->data is NULL! the symbol page has not been loaded?!\n",func);
	                return -3;
        }
        /* check for symb_index */
        if(sym_page->symwidth == NULL)
        {
                printf("%s(): symbol width list is empty!\n",func);
                return -4;
        }

	return 0;
}



/*--------------------------------------------------------------------------
print all symbol in a mem page.
print '*' if the pixel data is not 0.

sym_page: 	a mem symbol page pointer.
transpcolor:	color treated as transparent.
		black =0x0000; white = 0xffff;
----------------------------------------------------------------------------*/
void symbol_print_symbol( const EGI_SYMPAGE *sym_page, int symbol, uint16_t transpcolor)
{
        int i;
	int j,k;

	/* check page first */
	if(symbol_check_page(sym_page,"symbol_print_symbol") != 0)
		return;

	i=symbol;

#if 1 /* TEST ---------- */
	printf("symheight=%d, symwidth=%d \n", sym_page->symheight, sym_page->symwidth[i]);
#endif

	for(j=0;j<sym_page->symheight;j++) /*for each row of a symbol */
	{
		for(k=0;k<sym_page->symwidth[i];k++)
		{
			/* if not transparent color, then print the pixel */
			if(sym_page->alpha==NULL) {
				if( *(uint16_t *)( sym_page->data+(sym_page->symoffset)[i] \
						+(sym_page->symwidth)[i]*j +k ) != transpcolor ) {
                	                       printf("*");
				}
                        	       else
                                	       printf(" ");
			}
			else {  /* use alpha value */

				if( *(unsigned char *)(sym_page->alpha+(sym_page->symoffset)[i] \
						+(sym_page->symwidth)[i]*j +k ) > 0 )  {

						printf("*");
				}
					else
						printf(" ");
			}
		}
	     	printf("\n"); /* end of each row */
	}
}


/*-----------------------------------------------------
	save mem of a symbol page to a file
-------------------------------------------------------*/
void symbol_save_pagemem(EGI_SYMPAGE *sym_page)
{


}

/*------------------------------------------
	Get string length in pixel.
@str	string
@font	font for the string
-------------------------------------------*/
int symbol_string_pixlen(char *str, const EGI_SYMPAGE *font)
{
	int i;
        int len=strlen(str);
	int pixlen=0;

	if( len==0 || font==NULL)
		return 0;

        for(i=0;i<len;i++)
        {
                /* only if in code range */
                if( str[i] <= font->maxnum )
                           pixlen += font->symwidth[ (unsigned int)str[i] ];
        }

	return pixlen;
}


/*------------------------------------------------------------------------------------------
		write a symbol/font pixel data to FB device

1. Write a symbol data to FB.
2. Alpha data is available in a FT2 symbol page, use WEGI_COLOR_BLACK as default front
   color.
4. Note: put page check in symbol_string_writeFB()!!!
5. Points out of fb_map page(one screen buffer) will be ruled out.

@fbdev: 	FB device
		or Virt FB
@sym_page: 	symbol page, if it has alpha value, then blends with opaque.

@transpcolor: 	>=0 transparent pixel will not be written to FB, so backcolor is shown there.
	     	<0	 no transparent pixel

@fontcolor:	font color(symbol color for a symbol)
		>= 0,  use given font color.
		<0  ,  use default color in img data.

use following COLOR:
#define SYM_NOSUB_COLOR -1  --- no substitute color defined for a symbol or font
#define SYM_NOTRANSP_COLOR -1  --- no transparent color defined for a symbol or font

@x0,y0: 		start position coordinate in screen, left top point of a symbol.
@sym_code: 	symbol code number

@opaque:	Set aplha value (0-255) for all pixels, if sym_page also has alpha value
		then the final alpha value would be: sym_alpha*opaque/255;
		Normally set it as 255.

		<0	Lumiance decrement value;  No effect, or use symbol alpha value.
			alpha =0
		0 	100% back ground color/transparent, alpha=0
		255	100% front color, alpha=255
		>255	alpha=255

-----------------------------------------------------------------------------------------*/
inline void symbol_writeFB(FBDEV *fb_dev, const EGI_SYMPAGE *sym_page, 	\
		int fontcolor, int transpcolor, int x0, int y0, unsigned int sym_code, int opaque)
{
	/* check data */
	if( sym_page==NULL || (sym_page->data==NULL && sym_page->alpha==NULL) ) {
		printf("%s: Input symbol page has no valid data inside.\n",__func__); /* Example: A Freetype char with no bitmap */
		return;
	}

	int i,j;
	FBPIX fpix;
	long int pos; /* offset position in fb map */
	long int zpos; /* for zbuff[] pos */
	int xres;
	int yres;
	int mapx=0,mapy=0; /* if need ROLLBACK effect,then map x0,y0 to LCD coordinate range when they're out of range*/
	uint16_t pcolor;
	uint32_t pargb; /* ARGB, !!! init for non FT type fonts, alpha is 255. */
	uint32_t prgb;  /* RGB */
	unsigned char palpha=0; /* pixel alpha value if applicable */
	unsigned char *map=NULL; /* the pointer to map FB or back buffer */
	uint16_t *data=sym_page->data; /* symbol pixel data in a mem page, for FT2 sympage, it's NULL! */
	int offset;
	long poff;
	int height=sym_page->symheight;
	int width;
	EGI_IMGBUF *virt_fb;
	int sumalpha;
	int lumdev=0;	/* luminance decrement value */

        /* <<<<<<  FB BUFFER SELECT  >>>>>> */
        #if defined(ENABLE_BACK_BUFFER) || defined(LETS_NOTE)
        map=fb_dev->map_bk; /* write to back buffer */
        #else
        map=fb_dev->map_fb; /* write directly to FB map */;
        #endif

	//long int screensize=fb_dev->screensize;

	/* check page */
#if 0  /* not here, put page check in symbol_string_writeFB() */
	if(symbol_check_page(sym_page, "symbol_writeFB") != 0)
		return;
#endif

	/* check sym_code */
	if( sym_code > sym_page->maxnum && sym_page->symtype!=symtype_FT2 ) {
		EGI_PDEBUG(DBG_SYMBOL,"symbole code number out of range! sympg->path: %s\n", sym_page->path);
		return;
	}

        /* set xres and yres */
        virt_fb=fb_dev->virt_fb;
        if(virt_fb) {                   /* for virtual FB */
                xres=virt_fb->width;
                yres=virt_fb->height;
        }
        else {                          /* for FB */
                //xres=fb_dev->vinfo.xres;
		#ifdef LETS_NOTE
		xres=fb_dev->finfo.line_length>>2;
		#else
		xres=fb_dev->finfo.line_length>>1;
		#endif

                yres=fb_dev->vinfo.yres;
        }

	/* get symbol/font width, only 1 character in FT2 symbol page NOW!!! */
	if(sym_page->symtype==symtype_FT2) {
		width=sym_page->ftwidth;
		offset=0;
	}
	else {
		width=sym_page->symwidth[sym_code];
		offset=sym_page->symoffset[sym_code];
	}

	/* check and reset opaque to [0 255] */
	if( opaque < 0 ) {
		lumdev=opaque;	/* As luminance decrement value */
		opaque=255;
	}
	else if( opaque > 255) {
		opaque=255;
	}

	/* Init palpha for non FT symobls */
	if(sym_page->alpha == NULL) {
		palpha=255;
		pargb=255<<24; /* For LETS_NOTE */
	}

	/* get symbol pixel and copy it to FB mem */
	//printf("%s:symbol H=%d, W=%d\n",__func__, height, width);
	for(i=0;i<height;i++)
	{
		for(j=0;j<width;j++)
		{
			/*  skip pixels according to opaque value, skipped pixels
							make trasparent area to the background */
//			if(opaque>0)
//			{
//				if( ((i%2)*(opaque/2)+j)%(opaque) != 0 ) /* make these points transparent */
//					continue;
//			}


#ifdef FB_SYMOUT_ROLLBACK  /* NOTE !!!  FB pos_rotate NOT applied  */

			/*--- map x for every (i,j) symbol pixel---*/
			if( x0+j<0 )
			{
				mapx=xres-(-x0-j)%xres; /* here mapx=1-240, if real FB */
				mapx=mapx%xres;
			}
			else if( x0+j>xres-1 )
				mapx=(x0+j)%xres;
			else
				mapx=x0+j;

			/*--- map Y for every (i,j) symbol pixel---*/
			if ( y0+i<0 )
			{
				/* in case y0=0 and i=0,then mapy=320!!!!, then function will returns
				and abort drawing the symbol., don't forget -1 */
				mapy=yres-(-y0-i)%yres; /* here mapy=1-320 */
				mapy=mapy%yres;
			}
			else if(y0+i>yres-1)
				mapy=(y0+i)%yres;
			else
				mapy=y0+i;


#else /*--- if  NO ROLLBACK ---*/
			/* -----  FB ROTATION POSITION MAPPING -----
			 * IF 90 Deg rotated: Y maps to (xres-1)-FB.X,  X maps to FB.Y
			 */
		       switch(fb_dev->pos_rotate) {
        		        case 0:                 /* FB defaul position */
					mapx=x0+j;
					mapy=y0+i;
	                       		 break;
	        	        case 1:                 /* Clockwise 90 deg */
					mapx=(xres-1)-(y0+i);
					mapy=x0+j;
		                        break;
        		        case 2:                 /* Clockwise 180 deg */
					mapx=(xres-1)-(x0+j);
					mapy=(yres-1)-(y0+i);
        	               		 break;
		                case 3:                 /* Clockwise 270 deg */
					mapx=y0+i;
					mapy=(yres-1)-(x0+j);
		                        break;
		        }


			/* LIMIT CHECK: ignore out ranged points */
			if(mapx>(xres-1) || mapx<0 )
				continue;
			if(mapy>(yres-1) || mapy<0 )
				continue;

#endif

			/*x(i,j),y(i,j) mapped to LCD(xy),
				however, pos may also be out of FB screensize ? */
			pos=mapy*xres+mapx; 	/* in pixel, LCD fb mem position */
			//pos=mapy*(fb_dev->finfo.line_length>>3)+mapx;
			poff=offset+width*i+j; 	/* offset to pixel data */

		        /* Check Z */
   		        if( fb_dev->zbuff_on ) {
		             zpos=mapy*fb_dev->vinfo.xres+mapx;
		             if( fb_dev->zbuff[zpos] > fb_dev->pixz )
		                continue;
		             else {
		             	fb_dev->zbuff[zpos]=fb_dev->pixz;
		             }
		        }

			if(sym_page->alpha)
				palpha=*(sym_page->alpha+poff);  	/*  get pixel alpha */

			/* for FT font sympage, only alpah value, and data is NULL. */
			if( data==NULL ) {
				pcolor=WEGI_COLOR_BLACK; /* pcolor will(may) be re_assigned to fontcolor later ... */
			}
			/* get symbol pixel in page data */
			else {
				pcolor=*(data+poff);
			}

			/* ------- Assign color data one by one,faster than memcpy  --------
			   Write to FB only if:
			   0.  Lumadev<0 (opaque<0), then transparent pixel to be darkened!
			   1.  alpha value exists, then use alpha to blend image instead of transpcolor.
			   2.  OR(no transp. color applied)
			   3.  OR (write only untransparent pixel)
			   otherwise,if pcolor==transpcolor, DO NOT write to FB
			*/
			/* To darken transparent pixel of symbol img page!  */
			if( pcolor == transpcolor && lumdev < 0 && sym_page->alpha==NULL ) {
				#ifdef LETS_NOTE
				#endif
				pos<<=1; /*pixel to byte,  pos=pos*2 */
				/* adjust background pixel luma */
				pcolor=egi_colorLuma_adjust(*(uint16_t *)(map+pos), lumdev);
				*(uint16_t *)(map+pos) = pcolor;
			}

			else if( sym_page->alpha || transpcolor<0 || pcolor!=transpcolor ) /* transpcolor applied befor COLOR FLIP! */
			{
				/* push original fb data to FB FILO, before write new color */
				if(fb_dev->filo_on) {		/* For real FB device */
					#ifdef LETS_NOTE  /*--- 4 bytes per pixel ---*/
					fpix.position=pos<<2; /* pixel to bytes, !!! FAINT !!! */
					fpix.argb=*(uint32_t *)(map+(pos<<2));
					#else		  /*--- 2 bytes per pixel ---*/
					fpix.position=pos<<1; /* pixel to bytes, !!! FAINT !!! */
					fpix.color=*(uint16_t *)(map+(pos<<1));
					//printf("symbol push FILO: pos=%ld.\n",fpix.position);
					#endif
					egi_filo_push(fb_dev->fb_filo,&fpix);
				}

				/* If use complementary color in image page,
				 * It will not apply if alpha value exists.
				 */
				if(TESTFONT_COLOR_FLIP && sym_page->alpha==NULL ) {
					pcolor = ~pcolor;
				}

				/*  if use given symbol/font color  */
				if(fontcolor >= 0)
					pcolor=(uint16_t)fontcolor;

		 	   /* ------------------------ ( for Virtual FB ) ---------------------- */
			   /* Note: Luma decrement NOT applied! */
			   if( virt_fb )
			   {
				if( pos < 0 || pos > fb_dev->screensize - 1 ) /* screensize of imgbuf,
          				                                       * pos in pixels! */
        			{
      	                		printf("symbol_writeFB(): WARNING!!! symbol point reach boundary of FB mem.!\n");
			                return;
			        }

				/* if apply alpha: front pixel, background pixel,alpha value
				 * TODO:  call egi_16bitColor_blend2() to count in bk color alpha value
				 */
				if(sym_page->alpha) {
					if(opaque==255) { /* Speed UP!! */
						/* !!!TO AVOID: blend with backcolor=0(BLACK) and backalpha=0
						 * when call egi_16bitColor_blend() !!
						 */
            	        			//pcolor=COLOR_16BITS_BLEND( pcolor,
						pcolor=egi_16bitColor_blend( pcolor,
								     virt_fb->imgbuf[pos],
								     palpha
								 );
					} else {
						pcolor=egi_16bitColor_blend( pcolor,
								     virt_fb->imgbuf[pos],
								     palpha*opaque/255  /* opaque[0-255] */
								 );
					}
				}
				/* if no alpha data, use opaque instead */
				//else if (opaque > 0) {  /* 0 as 100% transparent */
				else if(opaque!=255) { /* opaque alread reset to [0 255] at beginning */
 	                   			//pcolor=COLOR_16BITS_BLEND( pcolor,
						pcolor=egi_16bitColor_blend( pcolor,
									     virt_fb->imgbuf[pos],
									     opaque
									  );
				}
				/* opaque==255, use original pcolor */

				/* Write to Virt FB */
				virt_fb->imgbuf[pos]=pcolor; /* in pixel */

	   		     	/* Blend alpha, only if VIRT FB has alpha data */
	        		if(virt_fb->alpha) {
        	        		/* sum up alpha value */
					if(sym_page->alpha) {	/* sym_page.alpha prevails */
		        	        	sumalpha=virt_fb->alpha[pos]+palpha;
						/* Rare case, ignored for SPEED UP.
		        	        	 *  sumalpha=virt_fb->alpha[pos]+sym_page->alpha*opaque/255;
						 */
	        		        	if( sumalpha > 255 ) sumalpha=255;
	        	        		virt_fb->alpha[pos]=sumalpha;
					}
					else { 	/* use opaque[0 255], opaque default as 255  */
		        	        	sumalpha=virt_fb->alpha[pos]+opaque;
	        		        	if( sumalpha > 255 ) sumalpha=255;
	        	        		virt_fb->alpha[pos]=sumalpha;
					}
		        	}

			   }

		 	   /* ------------------------ ( for Real FB ) ---------------------- */
			   else
			   {
			   #ifdef LETS_NOTE  /* ------ 4 bytes per pixel ------ */

				/* check available space for a 4bytes pixel color fb memory boundary */
				pos<<=2; /*pixel to byte */
			        if( pos > (fb_dev->screensize-sizeof(uint32_t)) )
        			{
	                		printf("symbol_writeFB(): WARNING!!! symbol point reach boundary of FB mem.!\n");
					printf("pos=%ld, screensize=%ld    mapx=%d,mapy=%d\n",
						 pos, fb_dev->screensize, mapx, mapy);
                			return;
        			}
				/* if apply alpha */
				if(sym_page->alpha) {
					if(opaque==255)  /* Speed UP!! */
					    pargb=COLOR_16TO24BITS(pcolor)+(palpha<<24);
					else
				            pargb=COLOR_16TO24BITS(pcolor)+((palpha*opaque>>8)<<24); //>>8<<24, /255
				}
				//else if (opaque > 0) {  /* 0 as 100% transparent */
				else if(opaque!=255)  /* opaque alread reset to [0 255] at beginning */
					pargb=COLOR_16TO24BITS(pcolor)+(opaque<<24);
				/* else if opaque==255, use original pcolor */

				/* write to FB */
				/* !!! LETS_NOTE: FB only support 1 and 0 for alpha value of ARGB  */
				  // *(uint32_t *)(map+pos)=pargb; /* in pixel, deref. to uint32_t */

				/* Blend manually */
				prgb=(*(uint32_t *)(map+pos))&0xFFFFFF; /* Get back color */
                        	prgb=COLOR_24BITS_BLEND(pargb&0xFFFFFF, prgb, pargb>>24); /* front,back,alpha */

				/* Apply luminance decrement */
				//if(lumdev<0)
				//	pcolor=egi_colorLuma_adjust(xxxx,lumdev);

                               *((uint32_t *)(map+pos))=prgb+(255<<24); /* 1<<24 */


			   #else             /* ----- 2 bytes per pixel ----- */

				/* check available space for a 2bytes pixel color fb memory boundary,
				  !!! though FB has self roll back mechanism.  */
				pos<<=1; /*pixel to byte,  pos=pos*2 */
			        if( pos > (fb_dev->screensize-sizeof(uint16_t)) )
        			{
	                		printf("symbol_writeFB(): WARNING!!! symbol point reach boundary of FB mem.!\n");
					printf("pos=%ld, screensize=%ld    mapx=%d,mapy=%d\n",
						 pos, fb_dev->screensize, mapx, mapy);
                			return;
        			}

				/* if apply alpha: front pixel, background pixel, alpha value */
				if(sym_page->alpha) {
					if(opaque==255) { /* Speed UP!! */
	                    			//pcolor=COLOR_16BITS_BLEND( pcolor,
						pcolor=egi_16bitColor_blend( pcolor,
									    *(uint16_t *)(map+pos),
									    palpha  /* opaque[0-255] */
									 );
					} else {
						pcolor=egi_16bitColor_blend( pcolor,
									    *(uint16_t *)(map+pos),
									    palpha*opaque/255  /* opaque[0-255] */
									 );
					}
				}
				//else if (opaque > 0) {  /* 0 as 100% transparent */
				else if(opaque!=255) { /* opaque alread reset to [0 255] at beginning */
                    			//pcolor=COLOR_16BITS_BLEND( pcolor,
					pcolor=egi_16bitColor_blend( pcolor,
								     *(uint16_t *)(map+pos),
								     opaque
								  );
				}
				/* sym_page->alpha=NULL && opaque==255, use original pcolor */

				/* Apply luminance decrement */
				if(lumdev<0)
					pcolor=egi_colorLuma_adjust(pcolor,lumdev);

				/* write to FB */
				*(uint16_t *)(map+pos)=pcolor; /* in pixel, deref. to uint16_t */

			     #endif  /* END 32/16 bits FB SELECT  */

			   } /* end IF(real FB and virt FB) */

			}
		}
	}
}


/*------------------------------------------------------------------------------
1. write a symbol/font string to FB device.
2. Write them at the same line.
3. If write symbols, just use symbol codes[] for str[].
4. If it's font, then use symbol bkcolor as transparent tunnel.

fbdev: 		FB device
sym_page: 	a font symbol page
fontcolor:	font color (or symbol color for a symbol)
		>= 0, use given font color.
		<0   use default color in img data
transpcolor: 	>=0 transparent pixel will not be written to FB, so backcolor is shown there.
		    for fonts and icons,
	     	<0	 --- no transparent pixel

use following COLOR:
#define SYM_NOSUB_COLOR -1  --- no substitute color defined for a symbol or font
#define SYM_NOTRANSP_COLOR -1 --- no transparent color defined for a symbol or font

x0,y0: 		start position coordinate in screen, left top point of a symbol.
str:		pointer to a char string(or symbol codes[]);

opaque:		set aplha value (0-255)
		<0	No effect, or use symbol alpha value.
		0 	100% back ground color/transparent
		255	100% front color
-------------------------------------------------------------------------------*/
void symbol_string_writeFB(FBDEV *fb_dev, const EGI_SYMPAGE *sym_page, 	\
		int fontcolor, int transpcolor, int x0, int y0, const char* str, int opaque)
{
	const char *p=str;
	int x=x0;

	/* check page data */
	if(symbol_check_page(sym_page, "symbol_writeFB") != 0)
		return;

	/* if the symbol is font then use symbol back color as transparent tunnel */
	//if(tspcolor >0 && sym_page->symtype == symtype_font )

	/* transpcolor applied for both font and icon anyway!!! */
	if(transpcolor>=0 && sym_page->bkcolor>=0 )
		transpcolor=sym_page->bkcolor;

	while(*p) /* code '\0' will be deemed as end token here !!! */
	{
		symbol_writeFB(fb_dev,sym_page,fontcolor,transpcolor,x,y0,*p,opaque);/* at same line, so y=y0 */
		x+=sym_page->symwidth[(int)(*p)]; /* increase current x position */
		p++;
	}
}



/*-----------------------------------------------------------------------------------------
0. Extended ASCII symbols are not supported now!!!
1. write strings to FB device.
2. It will automatically return to next line if current line is used up,
   or if it gets a return code.
3. If write symbols, just use symbol codes[] for str[].
4. If it's font, then use symbol bkcolor as transparent tunnel.
5. Max dent space at each line end is 3 SPACEs, OR modify it.

fbdev: 		FB device
sym_page: 	a font symbol page
pixpl:		pixels per line.
lines:		number of lines available.
gap:		space between two lines, in pixel.
fontcolor:	font color (or symbol color for a symbol)
		>= 0, use given font color.
		<0   use default color in img data
transpcolor: 	>=0 transparent pixel will not be written to FB, so backcolor is shown there.
		    for fonts and icons,
	     	<0	 --- no transparent pixel
use following COLOR:
#define SYM_NOSUB_COLOR -1  --- no substitute color defined for a symbol or font
#define SYM_NOTRANSP_COLOR -1 --- no transparent color defined for a symbol or font
x0,y0: 		start position coordinate in screen, left top point of a symbol.
str:		pointer to a char string(or symbol codes[]);

opaque:		set aplha value (0-255)
		<0	No effect, or use symbol alpha value.
		0 	100% back ground color/transparent
		255	100% front color

TODO: TAB as 8*SPACE.

return:
		>=0   	bytes write to FB
		<0	fails
---------------------------------------------------------------------------------------------*/
int  symbol_strings_writeFB( FBDEV *fb_dev, const EGI_SYMPAGE *sym_page, unsigned int pixpl,
			     unsigned int lines,  unsigned int gap, int fontcolor, int transpcolor,
			     int x0, int y0, const char* str, int opaque )
{
	const char *p=str;
	const char *tmp;
	int x=x0;
	int y=y0;
	unsigned int pxl=pixpl; /* available pixels remainded in current line */
	unsigned int ln=0; /* lines used */
	int cw; 	/* char width, in pixel */
	int ww; 	/* word width, in pixel */
	bool check_word;

	/* check lines */
	if(lines==0)
		return -1;

	/* check page data */
	if(symbol_check_page(sym_page, "symbol_writeFB") != 0)
		return -2;

	/* if the symbol is font then use symbol back color as transparent tunnel */
	//if(tspcolor >0 && sym_page->symtype == symtype_font )

	/* use bkcolor for both font and icon anyway!!! */
	if(transpcolor>=0)
		transpcolor=sym_page->bkcolor;

	check_word=true;


	while(*p) /* code '0' will be deemed as end token here !!! */
	{
		/* skip extended ASCII symbols */
		if( *p > 127 ) {
			p++;
			continue;
		}

#if 0	/////////////  METHOD-1: Check CHARACTER after CHARACTER for necesary space  ////////////

		/* 1. Check whether remained space is enough for the CHARACTER,
  		 * or, if its a return code.
		 * 2. Note: If the first char for a new line is a return code, it returns again,
		 * and it may looks not so good!
		 */
		cw=sym_page->symwidth[(int)(*p)];
		if( pxl < cw || (*p)=='\n' )
		{
			ln++;
			if(ln>=lines) /* no lines available */
				return (p-str);
			y += gap + sym_page->symheight; /* move to next line */
			x = x0;
			pxl=pixpl;
		}

#else 	/////////////  METHOD-2:  Check WORD after WORD for necessary space  ////////////

	if( check_word || *p==' ' || *p=='\n' ) {	/* if a new word begins */

		/* 0. reset tmp and ww */
		tmp=(char *)p;
		ww=0;

		/* 1. If first char is not SPACE: get length of non_space WORD  */
		if(*p != ' ') {
		   	while(*tmp) {
				if( (*tmp != '\n') && (*tmp != ' ') ) {
					ww += sym_page->symwidth[(int)(*tmp)];
					tmp++;
				}
				else {
					break; /* break if SPACE or RETURN, as end of a WORD */
				}
			}
		}
		/* 2. Else if first char is SPACE: each SPACE deemed as one WORD */
		else {  /* ELSE IF *p == ' ' */
				ww += sym_page->symwidth[(int)(*p)];
		}

		/* 3. If not enough space for the WORD, or a RETURN for the first char */
		/* 3.1 if WORD length > pixpl */
		if( ww > pixpl ) {
                        /* This WORD is longer than a line!
			 * Do nothing, do not start a new line.
			 */
		}
		/* 3.2 set MAX pxl limit here for a long WORD at a line end,
		 * It will not start a new line here if pxl is big enough, but check cw by cw later.
		 * Just for good looking!
		 */
		else if( pxl < ww  &&  pxl > 3*sym_page->symwidth[' '] ) {   /* Max dents at line end, 3 SPACE */
			/* Do nothing, do not start a new line */
		}
		/* 3.3 Otherwise start a new line */
		else if( pxl < ww || *p == '\n' ) {
			ln++;
			if(ln>=lines) /* no lines available */
				return (p-str);
			y += gap + sym_page->symheight;
			x = x0;
			pxl=pixpl;
		}

	        /* reset check_word */
		check_word=false;
	}

	/*  if current char is SPACE or NEXT_LINE, we set check_work again for NEXT WORD!!!!
	 *  If current character is not SPACE/control_char, no need to check again.
	 */
	if( *p==' ' || *p=='\n' )
		check_word=true;

	/* process current character */
	cw=sym_page->symwidth[(int)(*p)];

	/* in case we set limit for ww>pxl in above, cw is a char of that long WORD */
	if(cw>pxl) {

		/* shift to next line */
		ln++;
		if(ln>=lines) /* no lines available */
			return (p-str);
		y += gap + sym_page->symheight;
		x = x0;
		pxl=pixpl;

		/* set check_word for next char */
		check_word=true;

		continue;
	}

	/* for control character */
//	if( cw==0 ) {
//		check_word=true;
//		p++;
//		continue;
//	}

#endif  /////////////////////////  END METHOD SELECTION //////////////////////////

		symbol_writeFB(fb_dev,sym_page,fontcolor,transpcolor,x,y,*p,opaque);
		x+=cw;
		pxl-=cw;
		p++;
	}

	return p-str;
}

/*-------------------------------------------------------------------------------
display each symbol in a char string to form a motion picture.

dt:		interval delay time for each symbol in (ms)
sym_page:       a font symbol page
transpcolor:    >=0 transparent pixel will not be written to FB, so backcolor is shown there.
                <0       --- no transparent pixel
use following COLOR:
#define SYM_NOSUB_COLOR -1  --- no substitute color defined for a symbol or font
#define SYM_NOTRANSP_COLOR -1 --- no transparent color defined for a symbol or font

x0,y0:          start position coordinate in screen, left top point of a symbol.
str:            pointer to a char string(or symbol codes[]);
		!!! code /0 as the end token of the string.

-------------------------------------------------------------------------------*/
void symbol_motion_string(FBDEV *fb_dev, int dt, const EGI_SYMPAGE *sym_page,   \
                			int transpcolor, int x0, int y0, const char* str)
{
        const char *p=str;

        /* check page data */
        if(symbol_check_page(sym_page, "symbol_writeFB") != 0)
                return;

        /* use bkcolor for both font and icon anyway!!! */
        if(transpcolor>=0)
                transpcolor=sym_page->bkcolor;

        while(*p) /* code '0' will be deemed as end token here !!! */
       	{
               	symbol_writeFB(fb_dev,sym_page,SYM_NOSUB_COLOR,transpcolor,x0,y0,*p,-1); /* -1, default font color */
		tm_delayms(dt);
           	p++;
       	}
}


/*--------------------------------------------------------------------------
Rotate a symbol, use its bkcolor as transcolor

sym_page: 	symbol page
x0,y0: 		start position coordinate in screen, left top point of a symbol.
sym_code:	symbole code
------------------------------------------------------------------------------*/
void symbol_rotate(const EGI_SYMPAGE *sym_page,
						 int x0, int y0, int sym_code)
{
        /* check page data */
        if(symbol_check_page(sym_page, "symbol_rotate") != 0)
                return;

	int i,j;
        uint16_t *data=sym_page->data; /* symbol pixel data in a mem page */
        int offset=sym_page->symoffset[sym_code];
        int height=sym_page->symheight;
        int width=sym_page->symwidth[sym_code];
	int max= height>width ? height : width;
	int n=((max/2)<<1)+1;/*  as form of 2*m+1  */
	uint16_t *symbuf;

	/* malloc symbuf */
	symbuf=malloc(n*n*sizeof(uint16_t));
	if(symbuf==NULL)
	{
		printf("symbol_rotate(): fail to malloc() symbuf.\n");
		return;
	}
        memset(symbuf,0,sizeof(n*n*sizeof(uint16_t)));

	/* copy data to fill symbuf with pixel number n*n */
        for(i=0;i<height;i++)
        {
                for(j=0;j<width;j++)
                {
			/* for n >= height and widt */
			symbuf[i*n+j]=*(data+offset+width*i+j);
		}
	}

        /* for image rotation matrix */
        struct egi_point_coord  *SQMat; /* the map matrix*/
        SQMat=malloc(n*n*sizeof(struct egi_point_coord));
	if(SQMat==NULL)
	{
		printf("symbol_rotate(): fail to malloc() SQMat.\n");
		return;
	}
        memset(SQMat,0,sizeof(*SQMat));

	/* rotation center */
        struct egi_point_coord  centxy={x0+n/2,y0+n/2}; /* center of rotation */
        struct egi_point_coord  x0y0={x0,y0};

	i=0;
	while(1)
	{
		i++;
              	/* get rotation map */
                mat_pointrotate_SQMap(n, 2*i, centxy, SQMat);/* side,angle,center, map matrix */
                /* draw rotated image */
                fb_drawimg_SQMap(n, x0y0, symbuf, SQMat); /* side,center,image buf, map matrix */
		tm_delayms(20);
	}
}





