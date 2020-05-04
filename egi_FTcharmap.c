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
@maplines:	 Max. displayed lines

Return:
	A pointer to char map	OK
	NULL			Fails
-------------------------------------------------*/
EGI_FTCHAR_MAP* FTcharmap_create(size_t txtsize,  size_t mapsize, size_t maplines)
{
	EGI_FTCHAR_MAP  *chmap=calloc(1, sizeof(EGI_FTCHAR_MAP));
	if(chmap==NULL) {
		printf("%s: Fail to calloc chmap!\n",__func__);
		return NULL;
	}

        /* Init charmap mutex */
        if(pthread_mutex_init(&chmap->mutex,NULL) != 0)
        {
                printf("%s: fail to initiate charmap mutex.\n",__func__);
		FTcharmap_free(&chmap);
                return NULL;
        }

	/* To allocate txtbuff */
	chmap->txtbuff=calloc(1,sizeof(typeof(*chmap->txtbuff))*(txtsize+1) );
	if( chmap->txtbuff == NULL) {
		printf("%s: Fail to calloc chmap txtbuff!\n",__func__);
		FTcharmap_free(&chmap);
		return NULL;
	}
	chmap->txtsize=txtsize;
	chmap->pref=chmap->txtbuff; /* Init. pref pointing to txtbuff */

	/* To allocate linePos */
	chmap->linePos=calloc(1,sizeof(typeof(*chmap->linePos))*maplines );
	if( chmap->linePos == NULL) {
		printf("%s: Fail to calloc chmap linePos!\n",__func__);
		FTcharmap_free(&chmap);
		return NULL;
	}
	chmap->maplines=maplines;

	/* To allocate just once!???? */
	chmap->charX=calloc(1, sizeof(typeof(*chmap->charX))*(mapsize+1) );
	chmap->charY=calloc(1, sizeof(typeof(*chmap->charY))*(mapsize+1) );
	chmap->charPos=calloc(1, sizeof(typeof(*chmap->charPos))*(mapsize+1) );
	if(chmap->charX==NULL || chmap->charY==NULL || chmap->charPos==NULL ) {
		printf("%s: Fail to calloc chmap members!\n",__func__);
		FTcharmap_free(&chmap);
		return NULL;
	}
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

        /* Hope there is no other user.. */
        if(pthread_mutex_lock(&((*chmap)->mutex)) !=0 )
                printf("%s: Fail to lock charmap mutex!\n",__func__);

	/* free(ptr): If ptr is NULL, no operation is performed */
	free( (*chmap)->txtbuff );
	free( (*chmap)->linePos );
	free( (*chmap)->charX );
	free( (*chmap)->charY );
	free( (*chmap)->charPos );

        /*  ??? necesssary ??? */
        pthread_mutex_unlock(&((*chmap)->mutex));

        /* Destroy thread mutex lock for page resource access */
        if(pthread_mutex_destroy(&((*chmap)->mutex)) !=0 )
                printf("%s: Fail to destroy charmap mutex!\n",__func__);

	free( *chmap );
	*chmap=NULL;
}


/*---------  NEGATIVE:  use charmap->linePos[] instead ---------
Set chmap->pref to position of the begin char of the next
displayed line.

@chmap:		pointer to an EGI_FTCHAR_MAP

Return:
	0	OK, chmap->pref changed.
	<0	Fails, chmap->pref unchanged.
	1	There's only one disp line. chmpa->pref unchagned.
--------------------------------------------------------*/
int FTcharmap_set_pref_nextDispLine(EGI_FTCHAR_MAP *chmap)
{
	int i;

	if( chmap==NULL || chmap->txtbuff==NULL )
		return -1;

	/* If only 1 displine */
	if( chmap->charY[0] == chmap->charY[chmap->chcount-1] )
		return 1;

	for(i=0; i < chmap->chcount; i++) {
		if( chmap->charY[i] > chmap->charY[0] )  /* Find the first char NOT in the first displine. */
			break;
	}

	/* Reset pref */
	chmap->pref=chmap->pref+chmap->charPos[i];

	return 0;
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
int  FTcharmap_uft8strings_writeFB( FBDEV *fb_dev, EGI_FTCHAR_MAP *chmap,
			       	    FT_Face face, int fw, int fh,
			            unsigned int pixpl,  unsigned int lines,  unsigned int gap,
                                    int x0, int y0,
			            int fontcolor, int transpcolor, int opaque,
			            int *cnt, int *lnleft, int* penx, int* peny )
{
	int size;
	int count;			/* number of character written to FB*/
	int px,py;			/* bitmap insertion origin(BBOX left top), relative to FB */
	const unsigned char *pstr=NULL;	/* == chmap->pref */
	const unsigned char *p=NULL;  	/* variable pointer, to each char */
        int xleft; 			/* available pixels remainded in current line */
        int ln; 			/* lines used */
 	wchar_t wcstr[1];

	/* Check input */
	if(face==NULL) {
		printf("%s: Input FT_Face is NULL!\n",__func__);
		return -1;
	}
	// if( pixpl==0 || lines==0 ) return -1; /* move to just after initiate vars */

	/* Check chmap */
	if(chmap==NULL || chmap->txtbuff==NULL) {
		printf("%s: Input chmap or its data is invalid!\n", __func__);
		return -2;
	}

	/* Init pstr and p */
	pstr=(unsigned char *)chmap->pref;  /* However pref may be NULL */
	p=pstr;

	/*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -3;
        }

	/* Init tmp. vars */
	px=x0;
	py=y0;
	xleft=pixpl;
	count=0;
	ln=0;		/* Line index from 0 */

	if(pixpl==0 || lines==0)
		goto FUNC_END;

	/* Must reset linePos[]!  */
	memset(chmap->linePos, 0, chmap->maplines*sizeof(typeof(*chmap->linePos)) );

	#if 0 /* Mutext lock ?? race condition --> mouse action */
	memset(chmap->charX, 0, chmap->mapsize*sizeof(typeof(*chmap->charX)) );
	memset(chmap->charY, 0, chmap->mapsize*sizeof(typeof(*chmap->charY)) );
	memset(chmap->charPos, 0, chmap->mapsize*sizeof(typeof(*chmap->charPos)));
	#endif

	/* Init. first linePos */
	chmap->chcount=0;

	chmap->lncount=0;
	chmap->linePos[chmap->lncount]=0; //chmap->pref;
	chmap->lncount++;


	while( *p ) {

		/* --- check whether lines are used up --- */
		if( lines < ln+1) {  /* ln index from 0, lines size_t */
			//printf("%s: ln=%d, Lines not enough! finish only %d chars.\n", __func__, ln, count);
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

		/* NOTE: Shift offset to next wchar, notice that following p is for the next char!!! */
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
			if( chmap->chcount < chmap->mapsize ) {
				chmap->charX[chmap->chcount]=x0+pixpl-xleft;  /* line end */
				chmap->charY[chmap->chcount]=py;
				chmap->charPos[chmap->chcount]=p-pstr-size;	/* reduce size */
				chmap->chcount++;

				/* Get start postion of next displaying line  */
				if(chmap->lncount < chmap->maplines-1 ) {
					chmap->linePos[chmap->lncount]=p-pstr;  /* keep +size */
					chmap->lncount++;
				}
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
		if(xleft<=0) {  /* ! xleft==0: for unprintable char, xleft unchaged */

			/* Get start position of next displaying line
			 * NOTE: for xleft==0: end line position is also the beginning of the next line!
			 *       if leave it until next xleft<0, then the position moves 1 more char away!
			 */
			if(chmap->lncount < chmap->maplines-1 ) {
				if(xleft<0)
					chmap->linePos[chmap->lncount]=p-pstr-size;  /* deduce size, display char to the next line */
				else /* xleft==0 */
					chmap->linePos[chmap->lncount]=p-pstr;  /* keep +size, xleft=0, as start of a new line */
				chmap->lncount++;  /* Increment after */
			}

			/* Fail to  writeFB. reel back pointer p, only if xleft<0 */
			if(xleft<0) {
				p-=size;
				count--;
			}

			/* Set new line, even xleft==0 and next char is an unprintable char, as we already start a new linePos[] as above. */
			/* Following move to the last if(xleft<=0){...} */
                        // ln++;
                        // xleft=pixpl;
                        // py+= fh+gap;
		}
		/* Fillin char map */
		if(xleft>=0) {  /* xleft > = 0, writeFB ok */
			if( chmap->chcount < chmap->mapsize) {
				chmap->charX[chmap->chcount]=px;  //!x0+pixpl-xleft;  /* char start point! */
				chmap->charY[chmap->chcount]=py;
				chmap->charPos[chmap->chcount]=p-pstr-size;	/* deduce size.  only when xleft<0, p will reel back size, see above. */
				chmap->chcount++;
			}
		}
		/* NOTE: At last, reset ln/xleft/py for new line. Just after charXY updated!!! */
		if(xleft<=0) {
			/* Set new line, even xleft==0 and next char is an unprintable char, as we already start a new linePos[] as above. */
                  	ln++;
                       	xleft=pixpl;
                        py+= fh+gap;
		}


	} /* end while() */

	/* If finishing writing whole strings, ln++ to get written lines, as ln index from 0 */
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
	if( (*p)=='\0' ) {
		chmap->charX[chmap->chcount]=x0+pixpl-xleft;  /* line end */
		chmap->charY[chmap->chcount]=py;	      /* ! py MAY be out of displaying box range, for it's already +fh+gap after '\n'.  */
		chmap->charPos[chmap->chcount]=p-pstr;	      /* ! mapindex MAY be out of displaying box range, for it's already self incremented ++ */
		chmap->chcount++;
	}

	/* Total number of lncount, after increment, Ok */
	/* Total number of chcount, after increment, Ok */

	/* Double check! */
	if( chmap->chcount > chmap->mapsize )
		printf("%s: WARNING:  chmap.chcount > chmap.mapsize=%d! \n", __func__,chmap->mapsize);

  	/*  <-------- Put mutex lock */
  	pthread_mutex_unlock(&chmap->mutex);

	return p-pstr;
}

