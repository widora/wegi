/*--------------------------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License version 2 as published by the Free Software
Foundation.
(Note: FreeType 2 is licensed under FTL/GPLv3 or GPLv2 ).

An Example of fonts demonstration based on example1.c in FreeType 2 library docs.

1. This program is ONLY for horizontal text layout !!!

2. To get the MAX boudary box heigh: symheight, which is capable of containing each character in a font set.
   (symheight as for struct symbol_page)

   1.1  base_uppmost + base_lowmost +1    --->  symheight  (same height for all chars)
        base_uppmost = MAX( slot->bitmap_top )                      ( pen.y set to 0 as for baseline )
        base_lowmost = MAX( slot->bitmap.rows - slot->bitmap_top )  ( pen.y set to 0 as for baseline )

        !!! Note: Though  W*H in pixels as in FT_Set_Pixel_Sizes(face,W,H) is capable to hold each
        char images, but it is NOT aligned with the common baseline !!!

   1.2 MAX(slot->advance.x/64, slot->bitmap.width)      --->  symwidth  ( for each charachter )

       Each ascii symbol has different value of its FT bitmap.width.

3. 0-31 ASCII control chars, 32-126 ASCII printable symbols.

4. For ASCII charaters, then height defined in FT_Set_Pixel_Sizes() or FT_Set_Char_Size() is NOT the true
   hight of a character bitmap, it is a norminal value including upper and/or low bearing gaps, depending
   on differenct font glyphs.

5. Following parameters all affect the final position of a character.
   5.1 slot->advance.x ,.y:     Defines the current cursor/pen shift vector after loading/rendering a character.
                                !!! This will affect the position of the next character. !!!
   5.2 HBearX, HBearY:          position adjust for a character, relative to the current cursor position(local
                                origin).
   5.3 slot->bitmap_left, slot->bitmap_top:  Defines the left top point coordinate of a character bitmap,
                                             relative to the global drawing system origin.


Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <math.h>
#include <wchar.h>
#include <freetype2/ft2build.h>
#include <freetype2/ftglyph.h>
#include "egi_common.h"
#include "egi_image.h"
#include "egi_utils.h"

#include FT_FREETYPE_H


int main( int  argc,   char**  argv )
{
	FT_Library    	library;
	FT_Face       	face;		/* NOTE: typedef struct FT_FaceRec_*  FT_Face */

	FT_GlyphSlot  	slot;
	FT_Matrix     	matrix;         /* transformation matrix */
	FT_Vector     	pen;            /* untransformed origin  */
	FT_Vector	origin;
	int		line_starty;    /* starting pen Y postion for rotated lines */
	FT_BBox		bbox;		/* Boundary box */
	int		bbox_W, bbox_H; /* Width and Height of the boundary box */
	FT_Error      	error;

	/* size in pixels */
	int		nsize=8;
	int		size_pix[][2]= {

	16,16,	/* W*H */
	18,18,
	18,20,
	24,24,
	32,32,
	36,36,
	48,48,
	64,64
	};

	int base_uppmost; 	/* in pixels, from baseline to the uppermost scanline */
	int base_lowmost; 	/* in pixels, from baseline to the lowermost scanline */
	int symheight;	  	/* in pixels, =base_uppmost+base_lowmost+1, symbol height for struct symbol_page */
	int symwidth_sum; 	/* sum of all charachter widths, as all symheight are the same,
			      	 * so symwidth_total*symheight is required mem space */
	int sympix_total;	/* total pixel number of all symbols in a sympage */

	struct symbol_page	symfont_page={0};
	int hi,wi;
	int pos_symdata, pos_bitmap;


	int HBearX,HBearY; /* horiBearingX and horiBearingY */
	int x1,y1,x2,y2; /* 2 points to define a boundary box */

 	FT_CharMap*   pcharmaps=NULL;
  	char          tag_charmap[6]={0};
  	uint32_t      tag_num;


  	char*         font_path;
//  	char*         text;

  	char**	fpaths;
  	int		count;
  	char**	picpaths;
  	int		pcount;

  	EGI_16BIT_COLOR font_color;
  	int		glyph_index;
  	int		deg; /* angle in degree */
  	double          angle;
  	int             num_chars;
  	int 		i,j,k,m,n;
	int 		test_k;
  	int		np;
  	float		step;
	int		ret;

	EGI_IMGBUF  *eimg=NULL;
	eimg=egi_imgbuf_new();

        /* EGI general init */
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

	/* get input args */
  	if ( argc <2 )
  	{
    	fprintf ( stderr, "usage: %s font_path  [a_letter]\n", argv[0] );
   	 	exit( 1 );
  	}

  	font_path      = argv[1];                           /* first argument     */
//  	text          = argv[2];                           /* second argument    */
//  	num_chars     = strlen( text );


   	/* TODO: convert char to wchar_t */
     	char    *cstr="abcefghijJBHKMS_%$^#_{}|:>?";


   /* buff all png file path */
   picpaths=egi_alloc_search_files("/mmc/pic", ".png", &pcount); /* NOTE: path for fonts */
   printf("Totally %d png files are found.\n",pcount);
   if(pcount==0) exit(1);

   /* buff all ttf,otf font file path */
   fpaths=egi_alloc_search_files(font_path, ".ttf, .otf", &count); /* NOTE: path for fonts */
   printf("Totally %d ttf font files are found.\n",count);
   if(count==0) exit(1);
   for(i=0; i<count; i++)
        printf("%s\n",fpaths[i]);



/* >>>>>>>>>>>>>>>>>>>>  START FONTS DEMON TEST  >>>>>>>>>>>>>>>>>>>> */
j=0;
for(j=0; j<=count; j++) /* transverse font type files */
{
	if(j==count)j=0;

 for(m=0; m<10; m++)   /* ascii chars shift */
 {
	/* 1. initialize FT library */
	error = FT_Init_FreeType( &library );
	if(error) {
		printf("%s: An error occured during FreeType library initialization.\n",__func__);
		return -1;
	}

	/* 2. create face object, face_index=0 */
 	//error = FT_New_Face( library, font_path, 0, &face );
 	error = FT_New_Face( library, fpaths[j], 0, &face );
	if(error==FT_Err_Unknown_File_Format) {
		printf("%s: Font file opens, but its font format is unsupported!\n",__func__);
		FT_Done_FreeType( library );
		return -2;
	}
	else if ( error ) {
		printf("%s: Fail to open or read font '%s'.\n",__func__, fpaths[j]);
		FT_Done_FreeType( library );
		return -3;
	}
  	/* get pointer to the glyph slot */
  	slot = face->glyph;

	printf("-------- Load font '%s', index[0] --------\n", fpaths[j]);
	printf("   num_faces:		%d\n",	face->num_faces);
	printf("   face_index:		%d\n",	face->face_index);
	printf("   family name:		%s\n",	face->family_name);
	printf("   style name:		%s\n",	face->style_name);
	printf("   num_glyphs:		%d\n",	face->num_glyphs);
	printf("   face_flags: 		0x%08X\n",face->face_flags);
	printf("   units_per_EM:	%d\n",	face->units_per_EM);
	printf("   num_fixed_sizes:	%d\n",	face->num_fixed_sizes);
	if(face->num_fixed_sizes !=0 ) {
		for(i=0; i< face->num_fixed_sizes; i++) {
			printf("[%d]: H%d x W%d\n",i, face->available_sizes[i].height,
							    face->available_sizes[i].width);
		}
	}
	/* print charmaps */
	printf("   num_charmaps:	%d\n",	face->num_charmaps);
	for(i=0; i< face->num_charmaps; i++) { /* print all charmap tags */
		pcharmaps=face->charmaps;
		tag_num=htonl((uint32_t)(*pcharmaps)->encoding );
		memcpy( tag_charmap, &tag_num, 4); /* 4 bytes TAG */
		printf("      			[%d] %s\n", i,tag_charmap ); /* 'unic' as for Unicode */
		pcharmaps++;
	}
	tag_num=htonl((uint32_t)( face->charmap->encoding));
	memcpy( tag_charmap, &tag_num, 4); /* 4 bytes TAG */
	printf("   charmap in use:	%s\n", tag_charmap ); /* 'unic' as for Unicode */

	/* vertical distance between two consective lines */
	printf("   height(V dist, in font units):	%d\n",	face->height);

	/* set angle */
	deg=0.0;

  	/* 3. set up matrix for transformation */
  	angle     = ( deg / 360.0 ) * 3.14159 * 2;
  	printf(" ------------- angle: %d, %f ----------\n", deg, angle);
  	matrix.xx = (FT_Fixed)( cos( angle ) * 0x10000L );
  	matrix.xy = (FT_Fixed)(-sin( angle ) * 0x10000L );
	matrix.yx = (FT_Fixed)( sin( angle ) * 0x10000L );
	matrix.yy = (FT_Fixed)( cos( angle ) * 0x10000L );

	/* 4. re_set pen position
   	 * the pen position in 26.6 cartesian space coordinates
     	 * 64 units per pixel for pen.x and pen.y
 	 */
  	pen.x = 5*64;
  	pen.y = 5*64;
  	line_starty=pen.y;

  	/* clear screen */
  	clear_screen(&gv_fb_dev, WEGI_COLOR_BLACK);

	/* init imgbuf */
	egi_imgbuf_init(eimg, 320, 240);

  	/* set font color */
  	//font_color= egi_color_random(all);
	font_color = WEGI_COLOR_WHITE;

/* select differenct size */
for(k=0; k< nsize-1; k++)
{
   	/* 5. set character size in pixels */
   	error = FT_Set_Pixel_Sizes(face, size_pix[k][0], size_pix[k][1]); /* width, height */
	/* OR set character size in 26.6 fractional points, and resolution in dpi */
   	//error = FT_Set_Char_Size( face, 32*32, 0, 100, 0 );


#if 1	////////////////////////////   Get symbol_page SYMHEIGHT    ///////////////////////////

   	//error = FT_Set_Pixel_Sizes(face, 32, 32); /* size in pixels: width, height */

	/* ------- Get MAX. base_uppmost, base_lowmost ------- */
	base_uppmost=0;
	base_lowmost=0;
	/* set origin here, so that bitmap_top is aligned */
	origin.x=0;
	origin.y=0;
    	FT_Set_Transform( face, &matrix, &origin);

	//test_k=7;
	//FT_Set_Pixel_Sizes(face, size_pix[test_k][0], size_pix[test_k][1]);
	test_k=5;

	symheight=0;
	symwidth_sum=0;

	/* Tranvser all chars to get MAX height for the boundary box, base line all aligned. */
	for( n=0; n<128; n++) {  /* 0-NULL, 32-SPACE, 127-DEL */

	    	error = FT_Load_Char( face, n, FT_LOAD_RENDER );
	    	if ( error ) {
			printf("%s: FT_Load_Char() error!\n");
			exit(1);
		}

		/* base_uppmost: dist from base_line to top */
		if(base_uppmost < slot->bitmap_top) {
			base_uppmost=slot->bitmap_top;
			printf("upp -------- %c -------\n", n);
		}

		/* base_lowmost: dist from base_line to bottom */
		// if(base_lowmost < size_pix[k][1]-slot->bitmap_top ) {  /* too big!!! for '_'  */
		/* !!!! unsigned int - signed int */
		if( base_lowmost < (int)(slot->bitmap.rows) - (int)(slot->bitmap_top) ) {
				base_lowmost=(int)slot->bitmap.rows-(int)slot->bitmap_top;

		#if 0
			printf(" slot->bitmap.rows=%d, slot->bitmap_top=%d \n",
							slot->bitmap.rows, slot->bitmap_top );
			printf(" %d < %d \n", base_lowmost, slot->bitmap.rows - slot->bitmap_top );
			if( slot->bitmap_top < 0) {   /* for '_' especially */
				if(base_lowmost < slot->bitmap.rows-slot->bitmap_top) {
					base_lowmost=slot->bitmap.rows-slot->bitmap_top;
					printf("low<0 ---- %c ---- base_lowmost=%d\n", n,base_lowmost);
				}
			}
			else {
				//base_lowmost=size_pix[k][1]-slot->bitmap_top;
				base_lowmost=slot->bitmap.rows-slot->bitmap_top;
				printf("low>0 ---- %c ---- base_lowmost=%d\n", n,base_lowmost);
			}
		#endif

		}

		/* For sympage:  Get symwidth, and sum it up */
		bbox_W=slot->advance.x>>6;
		if(bbox_W < slot->bitmap.width) /* adjust bbox_W */
			bbox_W =slot->bitmap.width;

		symwidth_sum += bbox_W;
	}

	/* For sympage:  Get symheight at last */
	symheight=base_uppmost+base_lowmost+1;
	printf("size_pix[] height = %d\n",size_pix[test_k][1]);
	printf("symheight=%d, base_uppmost=%d; base_lowmost=%d\n", symheight, base_uppmost, base_lowmost);
	symfont_page.symheight=symheight;

	/* For sympage: Set Maxnum */
	symfont_page.maxnum=128-1; /* 0-127 */
	/* TODO: 0-31 unprintable ascii characters */
	/* .width, .data, .alpha  .symoffset */

	sympix_total=symheight*symwidth_sum;

	/* For sympage: allcate .data, .alpha, .symwidth, .symoffset */
	symfont_page.data=calloc(1, sympix_total*sizeof(uint16_t));
	if(symfont_page.data==NULL) goto FT_FAILS;

	symfont_page.alpha=calloc(1, sympix_total*sizeof(unsigned char));
	if(symfont_page.alpha==NULL) goto FT_FAILS;

	symfont_page.symwidth=calloc(1, (symfont_page.maxnum+1)*sizeof(int) );
	if(symfont_page.symwidth==NULL) goto FT_FAILS;

	symfont_page.symoffset=calloc(1, (symfont_page.maxnum+1)*sizeof(int));
	if(symfont_page.symoffset==NULL) goto FT_FAILS;

#if 1
	/* Tranvser all chars to get alpha data for sympage , ensure that all base_lines are aligned. */
	/* NOTE:
	 *	1. TODO: Unprintable ASCII characters will be presented by FT as a box with a cross inside.
	 *	   We just need to replace with SPACE.
	 *	2. For font sympage, only .aplha is available, .data is useless!!!
	 *	3. Symbol font boundary box [ symheight*bbox_W ] is bigger than each slot->bitmap box !
	 */

	symfont_page.symoffset[0]=0; /* default */
	for( n=0; n<128; n++) {	/* 0-NULL, 32-SPACE, 127-DEL */

		/* load N to ASCII symbol bitmap */
	    	error = FT_Load_Char( face, n, FT_LOAD_RENDER );
	    	if ( error ) {
			printf("%s: Fail to FT_Load_Char() n=%d as ASCII symbol!\n", n);
			exit(1);
		}

		/* Get symwidth */
		bbox_W=slot->advance.x>>6;
		if(bbox_W < slot->bitmap.width) /* adjust bbox_W */
			bbox_W =slot->bitmap.width;

		symfont_page.symwidth[n]=bbox_W;

#if 1 /* ----- TEST ----- */
		if( argc>2 && n==argv[2][0]) {
			printf("-------- %c -------\n",n);
			printf("base_uppmost-slot->bitmap_top:  %d\n", base_uppmost-slot->bitmap_top);
			printf("slot->bitmap.rows:		%d\n", slot->bitmap.rows);
			printf("symheight:			%d\n", symheight);
		}
#endif /* TEST end */

		/* hi: 0-symheight,  wi: 0-bbox_W */
		/* ASSUME: bitmap start from hi=0 !!! */

		/* 1. From base_uppmost to bitmap_top:
		 *    All blank lines, just set alpha to 0.
		 */
		for( hi=0; hi < base_uppmost-slot->bitmap_top; hi++) {
			for( wi=0; wi < bbox_W; wi++ ) {
				pos_symdata=symfont_page.symoffset[n] + hi*bbox_W + wi;
				/* all default value */
				symfont_page.alpha[pos_symdata] = 0;
			}
		}

		/* 2. From bitmap_top to bitmap bottom,
		 *    Actual data here.
		 */
	     for( hi=base_uppmost-slot->bitmap_top; hi < (base_uppmost-slot->bitmap_top) + slot->bitmap.rows; \
							 hi++ )
		{

			/* ROW: within bitmap.width, !!! ASSUME bitmap start from wi=0 !!! */
			for( wi=0; wi < slot->bitmap.width; wi++ ) {
				pos_symdata=symfont_page.symoffset[n] + hi*bbox_W +wi;
				pos_bitmap=(hi-(base_uppmost-slot->bitmap_top))*slot->bitmap.width+wi;
				symfont_page.alpha[pos_symdata]= slot->bitmap.buffer[pos_bitmap];

#if 1 /* ------ TEST:  print alpha ------ */
	  if( argc>2 && n==argv[2][0]) {

		if(symfont_page.alpha[pos_symdata]>0)
			printf("*");
		else
			printf(" ");
		if( wi==slot->bitmap.width-1 )
			printf("\n");
	   }
#endif

			}
			/* ROW: blank area in BBOX */
			for( wi=slot->bitmap.width; wi<bbox_W; wi++ ) {
				pos_symdata=symfont_page.symoffset[n] + hi*bbox_W +wi;
				symfont_page.alpha[pos_symdata]=0;
			}
		}

		/* 3. From bitmap bottom to symheight( bottom),
		 *    All blank lines.
		 */
		for( hi=(base_uppmost-slot->bitmap_top) + slot->bitmap.rows; hi < symheight; hi++ )
		{
			for( wi=0; wi < bbox_W; wi++ ) {
				pos_symdata=symfont_page.symoffset[n] + hi*bbox_W + wi;
				/* set only alpha value */
				symfont_page.alpha[pos_symdata] = 0;
			}
		}

		/* move symoffset to next one */
		if(n<127)
			symfont_page.symoffset[n+1] = symfont_page.symoffset[n] + bbox_W * symheight;

	} /* end of all ASCII chars */
#endif

	/* test it */
	printf("----------- PRINT SYMBOL ---------\n");
	symbol_print_symbol(&symfont_page, argv[2][0],-1);

	/* release the sympage */
	symbol_release_page(&symfont_page);


//	exit(1);
#endif	///////////////////////////   END GET SYMpage   //////////////////////////////////



  	pen.y=line_starty;

	/* load chars in string one by one and process it */
//	for ( n = 0; n < strlen(cstr); n++ )
	for ( n = 32 + 10*m; n<128; n++ ) /* ASCII */
	{
		   	/* 6. re_set transformation before loading each wchar,
			 *    as pen postion and transfer_matrix may changed.
		         */
		    	FT_Set_Transform( face, &matrix, &pen );

			/* 7. load char and render to bitmap  */

			// glyph_index = FT_Get_Char_Index( face, wcstr[n] ); /*  */
			// printf("charcode[%X] --> glyph index[%d] \n", wcstr[n], glyph_index);
#if 1
			/*** Option 1:  FT_Load_Char( FT_LOAD_RENDER )
		    	 *   load glyph image into the slot (erase previous one)
			 */
		    	//error = FT_Load_Char( face, cstr[n], FT_LOAD_RENDER );
		    	error = FT_Load_Char( face, n, FT_LOAD_RENDER );
		    	if ( error ) {
				printf("%s: FT_Load_Char() error!\n");
      				continue;                 /* ignore errors */
			}
#else
			/*** Option 2:  FT_Load_Char(FT_LOAD_DEFAULT) + FT_Render_Glyph()
		         *    Call FT_Load_Glyph() with default flag and load the glyph slot
			 *    in its native format, then call FT_Render_Glyph() to convert
			 *    it to bitmap.
		         */
		    	//error = FT_Load_Char( face, cstr[n], FT_LOAD_DEFAULT );
		    	error = FT_Load_Char( face, n, FT_LOAD_DEFAULT );
		    	if ( error ) {
				printf("%s: FT_Load_Char() error!\n");
		      		continue;                 /* ignore errors */
			}

		        FT_Render_Glyph( slot, FT_RENDER_MODE_NORMAL );
			/* ALSO_Ok: _LIGHT, _NORMAL; 	BlUR: _MONO	LIMIT: _LCD */
#endif

	/* NOTE:
         * 	1. Define a boundary box with bbox_W and bbox_H to contain each character.
	 *	2. The BBoxes are NOT the same size, and neither top_lines nor bottom_lines
	 *	   are aligned at the same level.
	 *	3. The symbol_page boundary boxes are with the same height, and their top_lines
	 *	   and bottum_lines are aligned at the same level!
	 *	4. Only after adjust, the result of bbox_W is equal either slot->advance.x>>6 or
	 *	   slot->bitmap.width is applied.
	 */
	bbox_W=slot->advance.x>>6;   /* -OK-, ONLY after adjust,result is same as bitmap.width */
	//bbox_W=slot->bitmap.width; /* -OK-, ONLY after adjust,result is same as slot->advance.x>>6 */

	//bbox_W=size_pix[k][0]; 	     		  /* not appropriate with bitmap width !!! */
	//bbox_W=(face->bbox.xMax - face->bbox.xMin)>>6;  /* too big !!! */
	bbox_H=slot->bitmap.rows;    /* -Ok- */
	//bbox_H=size_pix[k][1];			  /* not aligned  */
	//bbox_H=(face->bbox.yMax - face->bbox.yMin)>>6;  /* too big */

	HBearX=slot->metrics.horiBearingX/64; /* May be minus, so do not use (>>6)!!! */;
	HBearY=slot->metrics.horiBearingY/64;

	/* Adjust bbox_W */
	/* advance.x/64 < bitmap.width, Example: which shitfs the charachter more to the right. */
	if(bbox_W < slot->bitmap.width)
		bbox_W =slot->bitmap.width;
#if 0
	/* HBearX<0, Example: 'j' HBearX=-4, which shitfs the character more to the left.??? */
	if(HBearX<0)
		bbox_W += -HBearX;
#endif

	/* -------  Draw BBOX boundary box  ------- */
#if 0
	x1=slot->bitmap_left;
	y1=320-slot->bitmap_top;
	x2=slot->bitmap_left+bbox_W-1;
	y2=320-slot->bitmap_top +bbox_H;
	//printf("x1=%d, y1=%d;  x2=%d, y2=%d\n", x1,y1,x2,y2);
	fbset_color(WEGI_COLOR_RED);
	draw_rect( &gv_fb_dev, x1, y1, x2, y2);
#endif

	/* -------  Draw symbol_page boundary box  ------- */
	x1=slot->bitmap_left; // + (HBearX<0 ? -HBearX : 0);			// -HBearX;
	y1=320-(pen.y/64+base_uppmost); //slot->bitmap_top ;
	x2=slot->bitmap_left+bbox_W-1; // + (HBearX<0 ? -HBearX : 0);		// -HBearX;
	y2=320-(pen.y/64-base_lowmost); //slot->bitmap_top +bbox_H;
	fbset_color(WEGI_COLOR_GREEN);
	draw_rect( &gv_fb_dev, x1, y1, x2, y2);


	/* --------  Print Out Parameters for Comparison ------- */
//if( slot->metrics.horiBearingX < 0 )
{
	//printf(" '%c'--- W*H=%d*%d \n", cstr[n], size_pix[k][0], size_pix[k][1]);
	printf(" '%c' --- W*H=%d*%d \n", n, size_pix[k][0], size_pix[k][1]);

	printf("User defined face boundary box: bitmap.rows=%d, ( bbox_W=%d, bbox_H=%d ) symheight=%d .\n",
							slot->bitmap.rows, bbox_W, bbox_H, symheight );

	/* Note that even when the glyph image is transformed, the metrics are NOT ! */
	printf("glyph metrics[in 26.6 format]: metrics.width=%d,	 metrics.height=%d\n",
					       slot->metrics.width, slot->metrics.height );
	printf("			       metrics.horiBearingX=%d,  metrics.horiBearingY=%d\n",
					       slot->metrics.horiBearingX, slot->metrics.horiBearingY );
	printf("			       metrics.HBearX=%d, 	 metrics.HBearY=%d\n",
					       HBearX, HBearY );

	printf("bitmap.rows=%d, bitmap.width=%d\n", slot->bitmap.rows, slot->bitmap.width);
	printf("bitmap_left=%d, bitmap_top=%d\n", slot->bitmap_left, slot->bitmap_top);

  	/* Note that 'slot->bitmap_left' and 'slot->bitmap_top' are also used
	 * to specify the position of the bitmap relative to the current pen
	 * position (e.g., coordinates (0,0) on the baseline).  Of course,
	 * 'slot->format' is also changed to @FT_GLYPH_FORMAT_BITMAP.
	 */
	printf("advance.x=%d  metrics.horiAdvance=%d\n",slot->advance.x, slot->metrics.horiAdvance);
	printf("advance.y=%d\n",slot->advance.y);
}

	/* 8. Draw to EGI_IMGBUF */
	error=egi_imgbuf_blend_FTbitmap(eimg, slot->bitmap_left, 320-slot->bitmap_top,
	//error=egi_imgbuf_blend_FTbitmap(eimg, slot->bitmap_left, slot->bitmap_top,
						&slot->bitmap, font_color); //egi_color_random(deep) );
	if(error) {
		printf(" Fail to fetch Font type '%s' char index [%d]\n", fpaths[j], n);
	}

    	/* 9. increment pen position */
	pen.x += (bbox_W<<6) +(8<<6);
    	//pen.x += slot->advance.x + (8<<6);
    	pen.y += slot->advance.y; /* same in a line for the same char  */

  } /* end for() load and process chars */

	/* 10. skip pen to next line, in font units. */
	pen.x = 3<<6; /* start X in a new line */
	line_starty += ((int)(size_pix[k][1])*3/2 )<<6; /* LINE GAP,  +15 */
	/* 1 pixel= 64 units glyph metrics */

} /* end of k size */


/*-------------------------------------------------------------------------------------------
int egi_imgbuf_windisplay(EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev, int subcolor,
                                        int xp, int yp, int xw, int yw, int winw, int winh);
// no subcolor, no FB filo
int egi_imgbuf_windisplay2(EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev,
                                        int xp, int yp, int xw, int yw, int winw, int winh);
----------------------------------------------------------------------------------------------*/

	/* Dispay EGI_IMGBUF */
	egi_imgbuf_windisplay2(eimg, &gv_fb_dev, 0, 0, 0, 0, eimg->width, eimg->height);

	tm_delayms(2000);


FT_FAILS:
  FT_Done_Face    ( face );
  FT_Done_FreeType( library );

 } /* end for()  ascii char shift */

} /* end fpaths[j] */


/* >>>>>>>>>>>>>>>>>  END FONTS DEMON TEST  >>>>>>>>>>>>>>>>>>>> */

 	/* free fpaths buffer */
        egi_free_buff2D((unsigned char **) fpaths, count);
        egi_free_buff2D((unsigned char **) picpaths, pcount);

	/* free EGI_IMGBUF */
	egi_imgbuf_free(eimg);

	/* release sympage */
	symbol_release_page(&symfont_page);

	/* Free EGI */
        release_fbdev(&gv_fb_dev);
        symbol_release_allpages();
        egi_quit_log();

  return 0;
}

/* EOF */
