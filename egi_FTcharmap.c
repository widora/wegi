/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Note:
1. FreeType 2 is licensed under FTL/GPLv3 or GPLv2.
   Based on freetype-2.5.5


			--- Definition and glossary ---

1. dlines:  displayed/charmapped line, A line starts/ends at displaying window left/right end side.
   retline: A line starts/ends by a new line token '\n'.

2. scroll up/down:  scroll up/down charmap by mouse, keep cursor position relative to txtbuff.
          (UP: decrease chmap->pref,      DOWN: increase chmap->pref )

   shift  up/down:  shift typing cursor up/down by ARROW keys.
          (UP: decrease chmap->pch,       DOWN: increase chmap->pch )


Midas Zhou
midaszhou@yahoo.com
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
@mappixpl:	 Max pixels per dline.
@maplndis:	 Distance between two dlines, in pixels.

Return:
	A pointer to char map	OK
	NULL			Fails
-------------------------------------------------*/
EGI_FTCHAR_MAP* FTcharmap_create(size_t txtsize,  size_t mapsize, size_t maplines, size_t mappixpl, int maplndis )
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

	/* To allocate txtdlinePos */
	chmap->txtdlines=1024;	/* Initial value,  to grow later ...*/
	chmap->txtdlinePos=calloc(1, sizeof(typeof(*chmap->txtdlinePos))*(chmap->txtdlines) );
	if( chmap->txtdlinePos == NULL) {
		printf("%s: Fail to calloc chmap txtdlinePos!\n",__func__);
		FTcharmap_free(&chmap);
		return NULL;
	}

	/* To allocate maplinePos */
	chmap->maplinePos=calloc(1,sizeof(typeof(*chmap->maplinePos))*maplines );
	if( chmap->maplinePos == NULL) {
		printf("%s: Fail to calloc chmap maplinePos!\n",__func__);
		FTcharmap_free(&chmap);
		return NULL;
	}
	chmap->maplines=maplines;
	chmap->mappixpl=mappixpl;
	chmap->maplndis=maplndis;

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
	free( (*chmap)->txtdlinePos );
	free( (*chmap)->maplinePos );
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


/*---------  NEGATIVE:  use charmap->maplinePos[] instead ---------
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
Charmap a string of charaters with UFT-8 encoding.

TODO: 1. Alphabetic words are treated letter by letter, and they may be separated at the end of
         a line, so it looks not good.
      2. Apply character functions in <ctype.h> to rule out chars, with consideration of locale setting?

@fbdev:         FB device
@chmap:		pointer to an EGI_FTCHAR_MAP
@face:          A face object in FreeType2 library.
@fh,fw:		Height and width of the wchar.
//@pstr:          pointer to a string with UTF-8 encoding.
@xleft:		Pixel space left in FB X direction (horizontal writing)
		Xleft will be subtrated by slot->advance.x first anyway, If xleft<0 then, it aborts.
//@pixpl:         pixels per line.
//@lines:         number of lines available.
//@gap:           space between two lines, in pixel. may be minus.
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
                                    int x0, int y0,
			            int fontcolor, int transpcolor, int opaque,
			            int *cnt, int *lnleft, int* penx, int* peny )
{
	int i;
	int size;
	int count;			/* number of character written to FB*/
	int px,py;			/* bitmap insertion origin(BBOX left top), relative to FB */
	const unsigned char *pstr=NULL;	/* == chmap->pref */
	const unsigned char *p=NULL;  	/* variable pointer, to each char in pstr[] */
        int xleft; 			/* available pixels remainded in current line */
        int ln; 			/* lines used */
	unsigned int lines;		/* Total lines, =chmap->maplines */
	unsigned int pixpl;		/* pixels per line, =chmap->mappixpl */
	//int gap;			/* line gap, =chmap->maplngap */
	unsigned off=0;

 	wchar_t wcstr[1];


	/* Check input font face */
	if(face==NULL) {
		printf("%s: Input FT_Face is NULL!\n",__func__);
		return -1;
	}
	// if( pixpl==0 || lines==0 ) return -1; /* move to just after initiate vars */

	/* Check input charmap */
	if(chmap==NULL || chmap->txtbuff==NULL) {
		printf("%s: Input chmap or its data is invalid!\n", __func__);
		return -2;
	}

	/*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -3;
        }

	/* TODO.  charmap->maplines */
	lines=chmap->maplines;
	pixpl=chmap->mappixpl;
	//gap=chmap->maplngap;

	/* Init pstr and p */
	pstr=chmap->pref;  /* However, pref may be NULL */
	p=pstr;

	/* Init tmp. vars */
	px=x0;
	py=y0;
	xleft=pixpl;
	count=0;
	ln=0;		/* Line index from 0 */

	/* Check input pixpl and lines, TODO lines=chmap->champlines */
	if(pixpl==0 || lines==0)
		goto FUNC_END;

	/* Must reset maplinePos[] and clear old data */
	memset(chmap->maplinePos, 0, chmap->maplines*sizeof(typeof(*chmap->maplinePos)) );
	memset(chmap->charPos, 0, chmap->mapsize*sizeof(typeof(*chmap->charPos)));
	#if 1 /* Mutext lock ?? race condition --> mouse action */
	memset(chmap->charX, 0, chmap->mapsize*sizeof(typeof(*chmap->charX)) );
	memset(chmap->charY, 0, chmap->mapsize*sizeof(typeof(*chmap->charY)) );
	#endif


	/* Reset/Init. charmap data, first maplinePos */
	chmap->chcount=0;

	chmap->maplncount=0;
	chmap->maplinePos[chmap->maplncount]=0; //chmap->pref;
	chmap->maplncount++;

	/* Init.charmap global data */
	/* NOTE: !!! chmap->txtdlncount=xxx, MUST preset before calling this function */
	chmap->txtdlinePos[chmap->txtdlncount++]=chmap->pref-chmap->txtbuff;



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
				chmap->charPos[chmap->chcount]=p-pstr-size;	/* deduce size, as p now points to end of '\n'  */
				chmap->chcount++;

				/* Get start postion of next displaying line  */
				if(chmap->maplncount < chmap->maplines ) { //(chmap->maplncount < chmap->maplines-1 ) {
					/* set maplinePos[] */
					chmap->maplinePos[chmap->maplncount++]=p-pstr;  /* keep +size */
					//chmap->maplncount++;
					/* set txtdlinePos[] */
					if(chmap->txtdlncount < chmap->txtdlines-1 )
						chmap->txtdlinePos[chmap->txtdlncount++]=p-(chmap->txtbuff);
					//else grow space ...
				}
			}

			/* change to next line, +gap */
			ln++;
			xleft=pixpl;
			py += chmap->maplndis; //fh+gap;

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
			if(chmap->maplncount < chmap->maplines ) {   //(chmap->maplncount < chmap->maplines-1) {
				if(xleft<0) {
					/* set maplinePos[] */
					chmap->maplinePos[chmap->maplncount++]=p-pstr-size;  /* deduce size, display char to the next line */
                                        /* set txtdlinePos[] */
					if(chmap->txtdlncount < chmap->txtdlines-1)
	                                        chmap->txtdlinePos[chmap->txtdlncount++]=p-(chmap->txtbuff)-size;
					//else grow space ....
				}
				else /* xleft==0 */ {
					/* set maplinePos[] */
					chmap->maplinePos[chmap->maplncount++]=p-pstr;  /* keep +size, xleft=0, as start of a new line */
                                        /* set txtdlinePos[] */
					if(chmap->txtdlncount < chmap->txtdlines-1)
                                        	chmap->txtdlinePos[chmap->txtdlncount++]=p-(chmap->txtbuff);
				}
				//chmap->maplncount++;  /* Increment after */
			}

			/* Fail to  writeFB. reel back pointer p, only if xleft<0 */
			if(xleft<0) {
				p-=size;
				count--;
			}

			/* Set new line, even xleft==0 and next char is an unprintable char, as we already start a new maplinePos[] as above. */
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
			/* Set new line, even xleft==0 and next char is an unprintable char, as we already start a new maplinePos[] as above. */
                  	ln++;
                       	xleft=pixpl;
                        py += chmap->maplndis; //fh+gap;
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

// Right, NOT necessary: chmap->maplncount++; 	/* Also need to increment maplncount, as it only triggered by '\n' and linepix_overflower as above. */
	}

	/* Total number of chcount, after increment, Ok */
	/* Total number of maplncount, after increment, Ok */
	/* Total number of txtlncount, after increment, Ok */

	/* Double check! */
	if( chmap->chcount > chmap->mapsize ) {
		chmap->errbits |= CHMAPERR_MAPSIZE_LIMIT;
		printf("%s: WARNING:  chmap.chcount > chmap.mapsize=%d, some displayed chars has NO charX/Y data! \n", __func__,chmap->mapsize);
	}
	if( chmap->txtdlncount > chmap->txtdlines ) {
		chmap->errbits |= CHMAPERR_TXTDLINES_LIMIT;
		printf("%s: WARNING:  chmap.txtdlncount=%d > chmap.txtdlines=%d, some displayed lines has NO position info. in charmap! \n",
										__func__, chmap->txtdlncount, chmap->txtdlines);
	}

	/* NOTE!!! Need to reset chmap->txtdlncount to let  chmape->txtdlinePos[txtdlncount] pointer to the first displaying line of current charmap
	 * Otherwise in a loop charmap wirteFB calling, it will increment itself forever...
	 * So let chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0] here;
	 * !!! However, txtdlinePos[txtdlncount+1],txtdlinePos[txtdlncount+2]... +maplncount-1] still hold valid data !!!
	 */
	chmap->txtdlncount -= chmap->maplncount;

	/* Update chmap->pch, to let it point to the same char before charmapping, as chmap->txtbuff[chmap->pchoff] */
	/* TODO: Any better solution??
	 *  	How about if chmap->pchoff points outside of current charmap? Do not draw the typing cursor ???
	 */
	if(chmap->pchoff > 0) {
		off=chmap->pref-chmap->txtbuff;
		chmap->pch=0; /* If no match */
		for( i=0; i < chmap->chcount; i++) {
			if( off+chmap->charPos[i] == chmap->pchoff ) {
				chmap->pch=i;
				break;
			}
		}

		/* reset pchoff */
		chmap->pchoff=0;
	}
	/* If no match, then chmap->pch = 0
	 *  xxxx then chmap->pch = chmap->chcount-1, indicate to the last char/EOF in charmap
	 */


  	/*  <-------- Put mutex lock */
  	pthread_mutex_unlock(&chmap->mutex);

	return p-pstr;
}


/*----------------------------------------------
Move chmap->pref backward to pointer to the the
start of previous displaying page.

@chmap:		pointer to an EGI_FTCHAR_MAP
Return:
	0	OK, chmap->pref changed.
	<0	Fails, chmap->pref unchanged.
-----------------------------------------------*/
int FTcharmap_page_up(EGI_FTCHAR_MAP *chmap)
{
	if( chmap==NULL || chmap->txtbuff==NULL)
		return -1;

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Only if charmap cleared chmap->pchoff, charmap finish last session. */
	if( chmap->pchoff != 0) {
        	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}
        /* Just before changing chmap->pref: Remember current typing/insertion cursor byte position in chmap->txtbuff */
        chmap->pchoff=chmap->pref-chmap->txtbuff+chmap->charPos[chmap->pch];


	/* Set chmap->txtdlncount */
	if( chmap->txtdlncount > chmap->maplines )
        	chmap->txtdlncount -= chmap->maplines;
        else  { /* It reaches the first page */
        	chmap->txtdlncount=0;
	}

	/* Set pref to previous txtdlines position */
        chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];

        printf("%s: page up to chmap->txtdlncount=%d \n", __func__, chmap->txtdlncount);

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*----------------------------------------------
Move chmap->pref forward to pointer to the the
start of next displaying page.

@chmap:		pointer to an EGI_FTCHAR_MAP
Return:
	0	OK, chmap->pref changed.
	<0	Fails, chmap->pref unchanged.
-----------------------------------------------*/
int FTcharmap_page_down(EGI_FTCHAR_MAP *chmap)
{
	int pos;

	if( chmap==NULL || chmap->txtbuff==NULL)
		return -1;

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Only if charmap cleared chmap->pchoff, charmap finish last session. */
	if( chmap->pchoff != 0) {
        	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}
        /* Just before changing chmap->pref: Remember current typing/insertion cursor byte position in chmap->txtbuff */
        chmap->pchoff=chmap->pref-chmap->txtbuff+chmap->charPos[chmap->pch];


 	if( chmap->maplncount == chmap->maplines )              /* 1. Current IS a full page/window */
	{
		/* Get to the start point of the next page */
		pos=chmap->charPos[chmap->chcount-1]+cstr_charlen_uft8(chmap->pref+chmap->charPos[chmap->chcount-1]);

		if( chmap->pref[pos] != '\0' ) {		/* AND 2. current is NOT the last page (Next char is not EOF) */

			/* reset pref and txtdlncount */
        		chmap->pref=chmap->pref+pos;
			chmap->txtdlncount += chmap->maplines;

        		printf("%s: page down to: chmap->txtdlncount=%d \n", __func__, chmap->txtdlncount);
		}
        }
	else
        	printf("%s: reach the last page: chmap->maplncount=%d, chmap->maplines=%d, chmap->txtdlncount=%d, \n",
							__func__,  chmap->maplncount, chmap->maplines, chmap->txtdlncount);

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*----------------------------------------------------------
Move chmap->pref backward to pointer to the the start
of the previous displaying line.
scroll_oneline_up until the first dline (of txtbuff) gets
to the top of the displaying window.

chmap->pch unchangd!

Note:
1. --- WARNING!!! ---
   Any editing (delet/insert) function MUST NOT call this
   function. because editing will also modify chmap->pchoff,
   which will cause conflict!
2. Set chmap->pchoff in order to keep cursor position
   unchanged.  !Race conditon with main loop
3. Charmap immediately after calling this function,
   and before any insert/delete operations, which MAY
   forget to change chmap->pchoff, or modified by other
   thread!

@chmap:		pointer to an EGI_FTCHAR_MAP
Return:
	0	OK, chmap->pref changed.
	<0	Fails, chmap->pref unchanged.
----------------------------------------------------------*/
int FTcharmap_scroll_oneline_up(EGI_FTCHAR_MAP *chmap)
{
	if( chmap==NULL || chmap->txtbuff==NULL)
		return -1;

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }
	/* Only if charmap cleared chmap->pchoff, finish last session. */
	if( chmap->pchoff != 0) {
        	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

        /* Just before changing chmap->pref: Remember current typing/insertion cursor byte position in chmap->txtbuff */
        chmap->pchoff=chmap->pref-chmap->txtbuff+chmap->charPos[chmap->pch];

	/* Change chmap->pref, to change charmap start postion */
        if( chmap->txtdlncount > 0 ) {
        	/* reset txtdlncount to pointer the line BEFORE first dline in current charmap page */
                chmap->pref=chmap->txtbuff + chmap->txtdlinePos[--chmap->txtdlncount];
                printf("%s: chmap->txtdlncount=%d \n", __func__, chmap->txtdlncount);
        }

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*----------------------------------------------------
Move chmap->pref forward to pointer to the the start
of the next displaying line.
scroll_oneline_down until the last dline (of txtbuff)
gets to top of the displaying window.

Note:
1. --- WARNING!!! ---
   Any editing (delet/insert) function MUST NOT call this
   function. because editing will also modify chmap->pchoff,
   which will cause conflict!
2. Set chmap->pchoff in order to keep cursor position
   unchanged.  ! Race condition with main loop.
3. Charmap immediately after calling this function,
   and before any insert/delete operations, which MAY
   forget to change chmap->pchoff. or modified by other
   thread!


@chmap:		pointer to an EGI_FTCHAR_MAP
Return:
	0	OK, chmap->pref changed.
	<0	Fails, chmap->pref unchanged.
-----------------------------------------------------*/
int FTcharmap_scroll_oneline_down(EGI_FTCHAR_MAP *chmap)
{
	if( chmap==NULL || chmap->txtbuff==NULL)
		return -1;

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Only if charmap cleared chmap->pchoff, finish last session. */
	if( chmap->pchoff != 0) {
        	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

        /* Just before changing chmap->pref: Remember current typing/insertion cursor byte position in chmap->txtbuff */
        chmap->pchoff=chmap->pref-chmap->txtbuff+chmap->charPos[chmap->pch];

	if( chmap->maplncount>1 ) {
       	 	/* Before/after calling FTcharmap_uft8string_writeFB(), champ->txtdlinePos[txtdlncount] always point to the first
                 * dline: champ->txtdlinePos[txtdlncount]==chmap->maplinePos[0]
                 * So we need to reset txtdlncount to pointer the next line
                 */
                chmap->pref=chmap->txtbuff + chmap->txtdlinePos[++chmap->txtdlncount];
                //SAME: chmap->pref=chmap->pref+chmap->maplinePos[1];  /* move pref pointer to next dline */
                printf("%s: chmap->txtdlncount=%d \n", __func__, chmap->txtdlncount);
	}

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*---------------------------------------------------------------
To locate chmap->pch according to given x,y.

@chmap:		The FTCHAR map.
@x,y:		A B/LCD coordinate pointed to a char.

Return:
	0	OK
	<0	Fails
----------------------------------------------------------------*/
int FTcharmap_locate_charPos( EGI_FTCHAR_MAP *chmap, int x, int y)
{
	int i;

	if( chmap==NULL ) {
		printf("%s: Input FTCHAR map is empty!\n", __func__);
		return -1;
	}

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Search char map to find pch/pos for the x,y  */
	//printf("input x,y=(%d,%d), chmap->chcount=%d \n", x,y, chmap->chcount);
	/* TODO: a better way to locate i */
        for(i=0; i < chmap->chcount; i++) {
             	//printf("i=%d/(%d-1)\n", i, chmap->chcount);
             	if( chmap->charY[i] <= y && chmap->charY[i]+ chmap->maplndis > y ) {	/* locate Y */
		#if 0  /* METHOD 1: Pick the first charX[] */
            		if( chmap->charX[i] <= x		/*  == ALSO is the case ! */
			    && (     i==chmap->chcount-1	/* Last char/insert of chmap, and next [i+1] will be ingored. */
				 || ( chmap->charX[i+1] > x || chmap->charY[i] != chmap->charY[i+1] )  /* || or END of the dline */
				)
			   )
                        {
				chmap->pch=i;
				break;
				//printf("Locate charXY[%d]: (%d,%d)\n", i, chmap->charX[i],chmap->charY[i]);
                        }

		#else /* METHOD 2: Pick the nearst charX[] to x */
			/* 1. Not found OR ==chcount-1),  set to the end of the chmap. */
			if( i==chmap->chcount-1 ) {  // && chmap->charX[i] <= x ) {  /* Last char of the chmap */
				chmap->pch=i;
				break;
			}
			/* 2. Not the last char of dline, so [i+1] is valid always! */
			else if( chmap->charX[i] <= x && chmap->charX[i+1] > x ) {
				/* find the nearest charX[] */
				if( x-chmap->charX[i] > chmap->charX[i+1]-x )
					chmap->pch=i+1;
				else
					chmap->pch=i;
				break;
			}
			/* 3. The last char of the dline.  */
			else if( chmap->charX[i] <= x && chmap->charY[i] != chmap->charY[i+1] ) {
				chmap->pch=i;
				break;
			}
		#endif
	     	}
	}


        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*--------------------------------------------------------
Move chmap->pch to point to the first char of current retline.
NOT displine!

@chmap: an EGI_FTCHAR_MAP

Return:
        0       OK,    chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
-------------------------------------------------------*/
int FTcharmap_goto_lineBegin( EGI_FTCHAR_MAP *chmap )
{
        int index=0;

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

        if( chmap==NULL ) {
                printf("%s: Input FTCHAR map is empty!\n", __func__);
                return -1;
        }

        index=chmap->pch;
        while( index>0 && chmap->pref[ chmap->charPos[index-1] ] != '\n' ) {
                //printf("index=%d\n",index);
                index--;
        }

        /* If no '\n' found, then index will be 0 */
        chmap->pch=index;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

        return 0;
}


/*---------------------------------------------------------
Move chmap->pch to point to the end char of current retline. ('\n')
NOT displine!

@chmap: an EGI_FTCHAR_MAP

Return:
        0       OK,    chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
----------------------------------------------------------*/
int FTcharmap_goto_lineEnd( EGI_FTCHAR_MAP *chmap )
{
        int index;

        if( chmap==NULL ) {
                printf("%s: Input FTCHAR map is empty!\n", __func__);
                return -1;
        }

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

        index=chmap->pch;
        while( index < chmap->chcount-1 && chmap->pref[ chmap->charPos[index] ] != '\n' ) {
                //printf("index=%d\n",index);
                index++;
        }

        /* If no '\n' found, then index will be chmap->count-1, as end  */
        chmap->pch=index;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

        return 0;
}


/*---------------------------------------------------------
Get the offset position (relative to txtdlinePos ) of the
last char in the dline.

@chmap: 	an EGI_FTCHAR_MAP
@dln: 		index of chmap->txtdlinePos[]

Return:
        >0       OK,   return offset of char, relative to its dline.
        <0      Fails
----------------------------------------------------------*/
int FTcharmap_getPos_lastCharOfDline(EGI_FTCHAR_MAP *chmap,  int dln)
{
	unsigned int pos;
	int charlen;

	/* Check input */
        if( chmap==NULL ) {
                printf("%s: Input FTCHAR map is empty!\n", __func__);
                return -1;
        }
	if( dln<0 || dln > chmap->txtdlines-1 ) {
                printf("%s: Input index of txtdlnPos[] out of ragne!\n", __func__);
                return -2;
	}
	if( chmap->txtdlinePos[dln] == 0 && dln != 0 ) {  /* No data, in NOT first dline */
                printf("%s: No data in txtdlnPos[dln] yet, try charmap first!\n", __func__);
		return -3;
	}

	/* Search one by one to calculate pos */
	pos=0;
	while(  chmap->txtbuff[chmap->txtdlinePos[dln]+pos] != '\0'			/* Get EOF */
		&&  (  chmap->txtdlinePos[dln+1]!=0		      /* Next dline MUST be mapped!,  dln+1 is alway valid if not EOF */
		       && chmap->txtdlinePos[dln]+pos != chmap->txtdlinePos[dln+1]  ) 	/* And not get to the next dline! */
	)
	{
		charlen=cstr_charlen_uft8(chmap->txtbuff+chmap->txtdlinePos[dln]+pos);
		if(charlen<1) {
			printf("%s: Unrecognizable uft-8 char found!\n",__func__);
			return -4;
		}
		pos += charlen;
	};

	/* Retreat charlen to get offset */
	return  pos-charlen;
}
