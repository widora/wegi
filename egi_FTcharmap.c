/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Note:
1. FreeType 2 is licensed under FTL/GPLv3 or GPLv2.
   Based on freetype-2.5.5

Midas Zhou
-------------------------------------------------------------------*/
#include "egi_log.h"
#include "egi_FTcharmap.h"
#include "egi_cstring.h"
#include "egi_utils.h"


/*------------------------------------------------
To create a char map with given size.

@txtsize: Size of txtbuff[], exclude '\0'.
	 !txtsize+1 units to be allocated.
@size:	 How many chars hold in the displaying map.
	 or Max. number of chars to be displayed once.
	 !!!NOTE, size-1 for char, and the last data is
	 always for new insterting point.
	 !size+1 units to be allocated.

Return:
	A pointer to char map	OK
	NULL			Fails
-------------------------------------------------*/
EGI_FTCHAR_MAP* FTcharmap_create(size_t txtsize,  size_t mapsize)
{
	EGI_FTCHAR_MAP  *chmap=calloc(1, sizeof(EGI_FTCHAR_MAP));
	if(chmap==NULL) {
		printf("%s: Fail to calloc chmap!\n",__func__);
		return NULL;
	}

	/* To allocate txtbuff */
	chmap->txtbuff=calloc(1,sizeof(typeof(*chmap->txtbuff))*(txtsize+1) );
	if( chmap->txtbuff == NULL) {
		printf("%s: Fail to calloc chmap txtbuff!\n",__func__);
		FTcharmap_free(&chmap);
	}
	else
		chmap->txtsize=txtsize;

	/* To allocate just once!???? */
	chmap->charX=calloc(1, sizeof(typeof(*chmap->charX))*(mapsize+1) );
	chmap->charY=calloc(1, sizeof(typeof(*chmap->charY))*(mapsize+1) );
	chmap->charPos=calloc(1, sizeof(typeof(*chmap->charPos))*(mapsize+1) );
	if(chmap->charX==NULL || chmap->charY==NULL || chmap->charPos==NULL ) {
		printf("%s: Fail to calloc chmap members!\n",__func__);
		FTcharmap_free(&chmap);
	}
	else
		chmap->mapsize=mapsize;

	return chmap;
}

/*----------------------------------------------
	Free an EGI_FTCHAR_MAP.
-----------------------------------------------*/
void FTcharmap_free(EGI_FTCHAR_MAP **chmap)
{
	if(  chmap==NULL || *chmap==NULL)
		return;

	/* free(ptr): If ptr is NULL, no operation is performed */
	free( (*chmap)->txtbuff );
	free( (*chmap)->charX );
	free( (*chmap)->charY );
	free( (*chmap)->charPos );
	free( *chmap );

	*chmap=NULL;
}


/*-----------------------------------------------------------------------------------------
Write a string of charaters with UFT-8 encoding to FB.

TODO: 1. Alphabetic words are treated letter by letter, and they may be separated at the end of
         a line, so it looks not good.
      2. Apply character functions in <ctype.h> to rule out chars, with consideration of locale setting?

@fbdev:         FB device
@chmap:		pointer to an EGI_FTCHAR_MAP
@face:          A face object in FreeType2 library.
@fh,fw:		Height and width of the wchar.
@pstr:          pointer to a string with UTF-8 encoding.
@xleft:		Pixel space left in FB X direction (horizontal writing)
		Xleft will be subtrated by slot->advance.x first anyway, If xleft<0 then, it aborts.
@pixpl:         pixels per line.
@lines:         number of lines available.
@gap:           space between two lines, in pixel. may be minus.
@x0,y0:		Left top coordinate of the character bitmap,relative to FB coord system.

@transpcolor:   >=0 transparent pixel will not be written to FB, so backcolor is shown there.
                <0       no transparent pixel

@fontcolor:     font color(symbol color for a symbol)
                >= 0,  use given font color.
                <0  ,  use default color in img data.

use following COLOR:
#define SYM_NOSUB_COLOR -1  --- no substitute color defined for a symbol or font
#define SYM_NOTRANSP_COLOR -1  --- no transparent color defined for a symbol or font

@opaque:        >=0     set aplha value (0-255) for all pixels, and alpha data in sympage
			will be ignored.
                <0      Use symbol alpha data, or none.
                0       100% back ground color/transparent
                255     100% front color

@cnt:		Total printable characters written.
@lnleft:	Lines left unwritten.
@penx:		The last pen position X
@peny:		The last pen position Y.
		Note: Whatever pstr is finished or not
		      penx,peny will be reset to starting position of next line!!!

return:
                >=0     bytes write to FB
                <0      fails
------------------------------------------------------------------------------------------------*/
int  FTcharmap_uft8strings_writeFB( FBDEV *fb_dev, EGI_FTCHAR_MAP *chmap, FT_Face face,
			       	    int fw, int fh, const unsigned char *pstr,
			            unsigned int pixpl,  unsigned int lines,  unsigned int gap,
                                    int x0, int y0,
			            int fontcolor, int transpcolor, int opaque,
			            int *cnt, int *lnleft, int* penx, int* peny )
{
	int size;
	int count;		/* number of character written to FB*/
	int mapindex;		/* include RETURN */
	int px,py;		/* bitmap insertion origin(BBOX left top), relative to FB */
	const unsigned char *p=pstr;
        int xleft; 		/* available pixels remainded in current line */
        unsigned int ln; 	/* lines used */
 	wchar_t wcstr[1];

	/* check input data */
	if(face==NULL) {
		printf("%s: input FT_Face is NULL!\n",__func__);
		return -1;
	}

	if( pixpl==0 || lines==0 || pstr==NULL )
		return -1;

	px=x0;
	py=y0;
	xleft=pixpl;
	count=0;
	ln=0;		/* Line index from 0 */

	/* init char map */
	mapindex=0;
	memset(chmap->charX, 0, chmap->mapsize*sizeof(typeof(*chmap->charX)) );
	memset(chmap->charY, 0, chmap->mapsize*sizeof(typeof(*chmap->charY)) );
	memset(chmap->charPos, 0, chmap->mapsize*sizeof(typeof(*chmap->charPos)));

	while( *p ) {

		/* --- check whether lines are used up --- */
		if( ln > lines-1) {  /* ln index from 0 */
			//printf("%s: ln=%d, Lines not enough! finish only %d chars.\n", __func__, ln, count);
			//return p-pstr;

			#if 0
			/* Fillin CHAR MAP */
			if(chmap && mapindex < chmap->mapsize ) {
				chmap->charX[mapindex]=0;  /* line start */
				chmap->charY[mapindex]=py;
				chmap->charPos[mapindex]=p-pstr;
				mapindex++;
			}
			#endif

			/* here ln is the written line number,not index number */
			goto FUNC_END;
		}

		/* convert one character to unicode, return size of utf-8 code */
		size=char_uft8_to_unicode(p, wcstr);

#if 0 /* ----TEST: print ASCII code */
		if(size==1) {
			printf("%s: ASCII code: %d \n",__func__,*wcstr);
		}
		//if( !iswprint(*wcstr) )  ---Need to setlocale(), not supported yet!
		//	printf("Not printalbe wchar code: 0x%x \n",*wcstr);

#endif /*-----TEST END----*/

		/* shift offset to next wchar */
		if(size>0) {
			p+=size;
			count++;
		}
		else {	/* If fails, try to step 1 byte forward to locate next recognizable unicode wchar */
			p++;
			continue;
		}

		/* CONTROL ASCII CODE:
		 * If return to next line
		 */
		if(*wcstr=='\n') {
			//printf(" ... ASCII code: Next Line ...\n ");

			/* Fillin CHAR MAP before start a new line */
			if(chmap && mapindex < chmap->mapsize ) {
				chmap->charX[mapindex]=x0+pixpl-xleft;  /* line end */
				chmap->charY[mapindex]=py;
				chmap->charPos[mapindex]=p-pstr-size;	/* reduce size */
				mapindex++;
			}

			/* change to next line, +gap */
			ln++;
			xleft=pixpl;
			py+= fh+gap;

			continue;
		}
		/* If other control codes or DEL, skip it. To avoid print some strange icons.  */
		else if( (*wcstr < 32 || *wcstr==127) && *wcstr!=9 ) {	/* Exclude TAB(9) */
			printf("%s: ASCII control code: %d\n", __func__, *wcstr);
			continue;
		}

		/* write unicode bitmap to FB, and get xleft renewed. */
		px=x0+pixpl-xleft;
		//printf("%s:unicode_writeFB...\n", __func__);
		FTsymbol_unicode_writeFB(fb_dev, face, fw, fh, wcstr[0], &xleft,
							 px, py, fontcolor, transpcolor, opaque );

		/* --- check line space --- */
		if(xleft<0) { /* NOT writeFB, reel back pointer p */
			p-=size;
			count--;
			/* change to next line, +gap */
			ln++;
			xleft=pixpl;
			py+= fh+gap;
		}
		else {
			/* Fillin CHAR MAP */
			if(chmap && mapindex < chmap->mapsize) {
				chmap->charX[mapindex]=px;  //!x0+pixpl-xleft;  /* char start point! */
				chmap->charY[mapindex]=py;
				chmap->charPos[mapindex]=p-pstr-size;	/* deduce size */
				mapindex++;
			}
		}

	} /* end while() */

	/* if finishing writing whole strings, ln++ to get written lines, as ln index from 0 */
	if(*pstr)   /* To rule out NULL input */
		ln++;

FUNC_END:
	//printf("%s: %d characters written to FB.\n", __func__, count);
	if(cnt!=NULL)
		*cnt=count;
	if(lnleft != NULL)
		*lnleft=lines-ln; /* here ln is written lines, not index number */
	if(penx != NULL)
		*penx=x0+pixpl-xleft;
	if(peny != NULL)
		*peny=py;

	/* If all chars written to FB, then EOF is the last data in the charmap, also as the last insterting point */
	if(chmap != NULL && (*p)=='\0' ) {
		chmap->charX[mapindex]=x0+pixpl-xleft;  /* line end */
		chmap->charY[mapindex]=py;	      /* ! py MAY be out of displaying box range, for it's already +fh+gap after '\n'.  */
		chmap->charPos[mapindex]=p-pstr;	      /* ! mapindex MAY be out of displaying box range, for it's already self incremented ++ */
		chmap->chcount=mapindex+1;
	}
	else if(chmap != NULL) {
		chmap->chcount=mapindex;
	}

	/* Double check! */
	if( chmap->chcount > chmap->mapsize )
		printf("%s: WARNING:  chmap.chcount > chmap.mapsize=%d! \n", __func__,chmap->mapsize);


	return p-pstr;
}

