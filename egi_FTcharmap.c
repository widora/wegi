/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Note:
1. FreeType 2 is licensed under FTL/GPLv3 or GPLv2.
   Based on freetype-2.5.5

2. If there are any selection marks, but not displayed in current charmap,
   To press 'DEL' or 'BACKSPACE':  it first deletes those selected chars,
   then scrolls charmap to display the typing cursor.
   To press any other key: it just scrolls charmap to display selection marks.

3. Sometimes it may needs more than one press/opertion ( delete, backspace, shift etc.)
   to make the cursor move, this is because there is/are unprintable chars
   with zero width.

			--- Definition and glossary ---

1. char:    A printable ASCII code OR a local character with UFT-8 encoding.
2. charmap: A EGI_FTCHAR_MAP struct that holds data of currently displayed chars,
	    of both their corresponding coordinates in displaying window and offset position in memory.
3. dlines:  displayed/charmapped line, A line starts/ends at displaying window left/right end side.
   retline: A line starts/ends by a new line token '\n'.

4. scroll_up/down:
		scroll up/down charmap for one dline
		keep cursor position unchanged(relative to txtbuff)
	   	Functions: FTcharmap_scroll_oneline_up(),  FTcharmap_scroll_oneline_down()

5. shift_up/down/left/right:
		shift typing cursor up/down/left/right.
		charmap follow the cursor to scroll if it gets out of current charmap.
	 	Functions: FTcharmap_shift_cursor_up(),  FTcharmap_shift_cursor_down()
	  	     	    FTcharmap_shift_cursor_left(),  FTcharmap_shift_cursor_right()

6. page_up/down:
		scroll whole charmap up/down totally out of current displayed charmap,
		keep cursor position unchanged(relative to txtbuff)
	  	Functions: FTcharmap_page_up(),  FTcharmap_page_down()


                        --- PRE_Charmap Actions ---

PRE_1:  Set chmap->txtdlncount
PRE_2:  Set chmap->pref
PRE_3:  Set chmap->pchoff/pchoff2  ( chmap->pch/pch2: to be derived from pchoff/pchoff2 in charmapping! )
PRE_4:  Set chmap->fix_cursor (option)
PRE_5:  Set chmap->request

			---  Charmap ---

charmap_1:	Update chmap->chcount
Charmap_2:	Update chmap->charX[], charY[],charPos[]

charmap_3:	Update chmap->maplncount
charmap_4:	Update chmap->maplinePos[]

charmap_5:	Update chmap->txtdlcount   ( NOTE: chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0] )
charmap_6:	Update chmap->txtdlinePos[]

                        --- POST_Charmap Actions ---

POST_1: Check chmap->errbits
POST_2: Redraw cursor


TODO:
1. A faster way to memmove...
2. A faster way to locate pch...
3. Auto grow mem. for chmap members.
4. Delete/insert/.... editing functions will fail if chmap->request!=0. ( loop trying/waiting ??)

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include "egi_log.h"
#include "egi_FTcharmap.h"
#include "egi_cstring.h"
#include "egi_utils.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>

static int FTcharmap_locate_charPos_nolock( EGI_FTCHAR_MAP *chmap, int x, int y);	/* No mutex_lock */
static void FTcharmap_mark_selection(FBDEV *fb_dev, EGI_FTCHAR_MAP *chmap);		/* No mutex_lock */


/*------------------------------------------------
To create a char map with given size.

@txtsize: Size of txtbuff[], exclude '\0'.
	 !txtsize+1 units to be allocated.
@x0,y0:  charmap left top point.
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
EGI_FTCHAR_MAP* FTcharmap_create(size_t txtsize,  int x0, int y0, size_t mapsize, size_t maplines, size_t mappixpl, int maplndis )
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

	chmap->mapx0=x0;
	chmap->mapy0=y0;
	chmap->mapsize=mapsize;

	/* Set default selection color/alpha */
	chmap->markcolor=WEGI_COLOR_YELLOW;
	chmap->markalpha=50;

	return chmap;
}


/*------------------------------------------------------------
		Set selection mark color/alpha

@chmap:	An EGI_FTCHAR_MAP struct
@colro, alpha:  color and alpha value for selection mark.

-------------------------------------------------------------*/
void FTcharmap_set_markcolor(EGI_FTCHAR_MAP *chmap, EGI_16BIT_COLOR color, EGI_8BIT_ALPHA alpha)
{
	if(chmap==NULL)
		return;

	chmap->markcolor=color;
	chmap->markalpha=alpha;
}


/*-------------------------------------------------------------------------------
Load a file into chmap->txtbuff, original data will be lost.
chmap->txtbuff will be realloced to (fsize+txtsize+1) bytes before copying file
data to chmap->txtbuff.
If create a new file, then chmap->txtbuff will be realloced to (txtsize+1) bytes.

@fpath:		file path
@chmap:		An EGI_FTCHAR_MAP
@txtsize:	If new file, the txtsize is chmap->txtbuff->txtsize=txtsize+1.
		Else	chmap->txtbuff=fsize+txtsize+1

Return:
	0	Succeed
	<0	Fail
-------------------------------------------------------------------------------*/
int FTcharmap_load_file(const char *fpath, EGI_FTCHAR_MAP *chmap, size_t txtsize)
{

        int             fd;
        int             fsize;
        struct stat     sb;
        char *          fp=NULL;

	/* Check input */
	if(chmap==NULL)
		return -1;

	/* Mmap input file */
        fd=open(fpath, O_CREAT|O_RDWR|O_CLOEXEC);
        if(fd<0) {
                printf("%s: Fail to open input file '%s': %s\n", __func__, fpath, strerror(errno));
                return -1;
        }
        /* Obtain file stat */
        if( fstat(fd,&sb)<0 ) {
                printf("%s: Fail call fstat for file '%s': %s\n", __func__, fpath, strerror(errno));
                return -2;
        }
        fsize=sb.st_size;

        /* If not new file: mmap txt file */
	if(fsize >0) {
	        fp=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
        	if(fp==MAP_FAILED) {
                	printf("%s: Fail to mmap file '%s': %s\n", __func__, fpath, strerror(errno));
			return -3;
		}
	}

	/* Realloc chmap->txtbuff */
	free(chmap->txtbuff); chmap->txtbuff=NULL;
        chmap->txtbuff=calloc(1,sizeof(typeof(*chmap->txtbuff))*(fsize+txtsize+1) );  /* +1 EOF */
        if( chmap->txtbuff == NULL) {
                printf("%s: Fail to calloc chmap txtbuff!\n",__func__);
		munmap(fp,fsize);
		close(fd);
		return -3;
        }
        chmap->txtsize=fsize+txtsize+1;     /* txtsize is Appendable size */
        chmap->pref=chmap->txtbuff; /* Init. pref pointing to txtbuff */

	/* If not new file: Copy file content to txtbfuff */
	if(fsize>0)
		memcpy(chmap->txtbuff, fp, fsize);

        /* Set txtlen */
        chmap->txtlen=strlen((char *)chmap->txtbuff);
        printf("%s: Copy from '%s' chmap->txtlen=%d bytes, totally %d characters.\n", __func__, fpath, chmap->txtlen,
	        					cstr_strcount_uft8((const unsigned char *)chmap->txtbuff) );

	/* munmap file */
	if( fsize>0 && munmap(fp,fsize) !=0 )
		printf("%s: Fail to munmap file '%s': %s!\n",__func__, fpath, strerror(errno));

	if( close(fd) !=0 )
		printf("%s: Fail to close file '%s': %s!\n",__func__, fpath, strerror(errno));

	return 0;
}


/*-------------------------------------------------------------
Save chmap->txtbuff to a file.

@fpath:		file path
@chmap:		An EGI_FTCHAR_MAP

Return:
	0	Succeed
	<0	Fail
---------------------------------------------------------------*/
int FTcharmap_save_file(const char *fpath, EGI_FTCHAR_MAP *chmap)
{
	FILE *fil;
	int ret=0;
	int nwrite=0;

	/* Check input */
	if(chmap==NULL)
		return -1;

	fil=fopen(fpath, "w");
	if(fil==NULL) {
                printf("%s: Fail to open file '%s' for write: %s\n", __func__, fpath, strerror(errno));
		return -2;
	}

	/*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -3;
        }

	/* Write txtbuff to fil */
	nwrite=fwrite(chmap->txtbuff, 1, chmap->txtlen+1, fil); /* +1 EOF */
	if(nwrite < chmap->txtlen) {
		printf("%s: WARNING! fwrite %d bytes of total %d bytes.\n", __func__, nwrite, chmap->txtlen);
		ret=-4;
	}

	/* Close fil */
	if( fclose(fil) !=0 ) {
                printf("%s: Fail to close file '%s': %s\n", __func__, fpath, strerror(errno));
		ret=-5;
	}


  	/*  <-------- Put mutex lock */
  	pthread_mutex_unlock(&chmap->mutex);

	return ret;
}


/*----------------------------------------------
	Free an EGI_FTCHAR_MAP.
-----------------------------------------------*/
void FTcharmap_free(EGI_FTCHAR_MAP **chmap)
{
	if(  chmap==NULL || *chmap==NULL)
		return;

        /* Hope there is no other user.. */
        /*  Get mutex lock   ----------->  */
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
        /*  <-------- Put mutex lock */
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
      3. !!! WARNING !!!
	 Any ASCII codes that may be preseted in the txtbuff MUST all be included in charmapping.
	 MUST NOT skip any char in charmapping, OR it will cause charmap data error in further operation,
	 Example: when shift chmap->pchoff, it MAY points to a char that has NO charmap data at all!
	 example: TAB(9) and CR(13)!
	 see 'EXCLUDE WARNING' in the function.

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
	int x0;				/* charmap left top point */
	int y0;
	int pchx=0;			/* if(chmap->fix_cursor), then save charXY[pch] */
	int pchy=0;

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

	/* Check chmap->request : NOT yet applied for all functions. */
	#if 0
	if(!chmap->request) {
  	/*  <-------- Put mutex lock */
	  	pthread_mutex_unlock(&chmap->mutex);
		return 1;
	}
	#endif

	/* TODO.  charmap->maplines */
	x0=chmap->mapx0;
	y0=chmap->mapy0;
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


	/* if(chmap->fix_cursor), save charXY[pch] to  */
	if(chmap->fix_cursor) {
		pchx=chmap->charX[chmap->pch];
		pchy=chmap->charY[chmap->pch];
	}

	/* Must reset maplinePos[] and clear old data */
	memset(chmap->maplinePos, 0, chmap->maplines*sizeof(typeof(*chmap->maplinePos)) );
	memset(chmap->charPos, 0, chmap->mapsize*sizeof(typeof(*chmap->charPos)));
	memset(chmap->charX, 0, chmap->mapsize*sizeof(typeof(*chmap->charX)) );
	memset(chmap->charY, 0, chmap->mapsize*sizeof(typeof(*chmap->charY)) );

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
		if(*wcstr=='\n') {  /* ASCII Newline 10 */
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

		#if 0  /*  !!! EXCLUDE WARNING,  MUST NOT SKIP !!!
			*   It will cause charmap data error in further operation, Example: when shift chmap->pchoff, it MAY
			*   points to a char that has NO charmap data at all!!!
		 	*/
		/* If other control codes or DEL, skip it. To avoid print some strange icons. */
		else if( (*wcstr < 32 || *wcstr==127) && *wcstr!=9 && *wcstr!=13 ) {	/* EXCLUDE WARNING:  except TAB(9), '\r' */
			//printf("%s: ASCII control code: %d\n", __func__, *wcstr);     /* ! ASCII Return=CR=13 */
			continue;
		}
		#endif

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

	/* Update chmap->pch,
	 * to let it point to the same char before charmapping, as chmap->txtbuff[chmap->pchoff]
	 * TODO: Any better solution??
	 *  	How about if chmap->pchoff points outside of current charmap? Do not draw the typing cursor ???
	 */

	/* First set to <0, NOT in current charmap */
	chmap->pch=-1;
	chmap->pch2=-1;
	off=chmap->pref-chmap->txtbuff;

	/* Ca. chmap->pch : If  pchoff is NOT align with charPos[], then it will fail to match, usually it's uft-8 coding error. */
	if( chmap->pchoff >= off+chmap->charPos[0] && chmap->pchoff <= off+chmap->charPos[chmap->chcount-1] ) {
		for( i=0; i < chmap->chcount; i++) {
			if( off+chmap->charPos[i] == chmap->pchoff ) {
				chmap->pch=i;
				break;
			}
		}
	}
	/* Cal. chmap->pch2 : If  pchoff is NOT align with charPos[], then it will fail to match, usually it's uft-8 coding error. */
	if( chmap->pchoff2 == chmap->pchoff)
		chmap->pch2=chmap->pch;

	else if( chmap->pchoff2 >= off+chmap->charPos[0] && chmap->pchoff2 <= off+chmap->charPos[chmap->chcount-1] ) {
		for( i=0; i < chmap->chcount; i++) {
			if( off+chmap->charPos[i] == chmap->pchoff2 ) {
				chmap->pch2=i;
				break;
			}
		}
	}
	//printf("%s: pchoff=%d, pchoff2=%d \n",__func__, chmap->pchoff, chmap->pchoff2);
	//printf("%s: pch=%d, pch2=%d \n",__func__, chmap->pch, chmap->pch2);

	/* After pchoff, check chmap->fix_cursor */
	if(chmap->fix_cursor==true) {
		/* Relocate pch/pch2 as per pchx,pchy:  selection will be canclled!
		 * chmap->pch/pch2, pchoff/pchoff2 all updated!
		 */
		FTcharmap_locate_charPos_nolock(chmap, pchx, pchy);
		/* reset */
		chmap->fix_cursor=false;
	}

	/* Check pch and pch2, see if selection activated.*/
	if( chmap->pchoff != chmap->pchoff2 )
		FTcharmap_mark_selection(fb_dev, chmap);

	/* Reset chmap->request */
	chmap->request=0;

  	/*  <-------- Put mutex lock */
  	pthread_mutex_unlock(&chmap->mutex);

	return p-pstr;
}

/*------------------------------------------------------------------------
Mark selected chars between chmap->pchoff and chmap->pchoff2.

Note:
1. chmap->pch AND chmap->pch2 MUST be updated before calling this function.

------------------------------------------------------------------------*/
inline static void FTcharmap_mark_selection(FBDEV *fb_dev, EGI_FTCHAR_MAP *chmap)
{
	int k;
	int startX,startY;	/* Two cursor top points, between which are selected chars. */
	int endX,endY;
	int mdls;		/* lines between start/end line, only if selected dlines >=3 */
	int pch=0,pch2=0;		/* Temp. pch/pch2 only for mark, not for cursor position */
	int off=0;

	if(fb_dev==NULL || chmap==NULL)
		return;

	/* Cal. off */
	off=chmap->pref-chmap->txtbuff;

	/* Check pchoff and pchoff2, if NO selection */
	if (chmap->pchoff == chmap->pchoff2 )
		return;

	/* if(chmap->pch == chmap->pch2) return; NOPE, if all =-1, they are just out of range , but may have selection */

	/* 1. Determine pch */
	if( chmap->pchoff <= off+chmap->charPos[0] )
			pch=0;
	else if ( chmap->pchoff > off+chmap->charPos[chmap->chcount-1])
			pch=chmap->chcount-1;
	else
			pch=chmap->pch;

	/* 2. Determine pch2 */
	if( chmap->pchoff2 <= off+chmap->charPos[0] )
			pch2=0;
	else if ( chmap->pchoff2 > off+chmap->charPos[chmap->chcount-1])
			pch2=chmap->chcount-1;
	else
			pch2=chmap->pch2;

	/* If NO selection */
	//printf("Chmap: pch=%d,pchoff=%d, pch2=%d, pchoff2=%d\n", chmap->pch,chmap->pchoff, chmap->pch2, chmap->pchoff2);
	//printf("Drawmark: pch=%d, pch2=%d \n", pch, pch2);
	if( pch==pch2 )
		return;

	if( pch <0 || pch2 <0 ) {
		printf("%s: WARNING! Error in current charmap! chmap->pch=%d, chmap->pch2=%d\n",__func__, chmap->pch, chmap->pch2);
		return;
	}

	/* 2. Start/end cursors are in the same dline */
	if(chmap->charY[pch] == chmap->charY[pch2] ) {
		/* Determind start/end XY */
		if(chmap->charX[pch] >= chmap->charX[pch2]) {
			startX=chmap->charX[pch2];
			endX=chmap->charX[pch];
		} else {
			startX=chmap->charX[pch];
			endX=chmap->charX[pch2];
		}
		startY=chmap->charY[pch2];
		endY=startY;

		/*  Draw blend area */
		draw_blend_filled_rect(fb_dev, startX, startY, endX, endY+chmap->maplndis-1,
								chmap->markcolor, chmap->markalpha); /* dev, x1,y1,x2,y2,color,alpha */

	}
	/* 3. Start/end cursors are NOT in the same dline */
	else {
		/* Determind start/end XY */
		if( chmap->charY[pch] > chmap->charY[pch2] ) {
			startY=chmap->charY[pch2];  	startX=chmap->charX[pch2];
			endY=chmap->charY[pch];	   	endX=chmap->charX[pch];
		}
		else {
			startY=chmap->charY[pch]; 	startX=chmap->charX[pch];
			endY=chmap->charY[pch2];	endX=chmap->charX[pch2];
		}

		/* 2.1 Mark start dline area */
		draw_blend_filled_rect(fb_dev, startX, startY, chmap->mapx0+chmap->mappixpl-1, startY+chmap->maplndis-1,
                                                                      chmap->markcolor, chmap->markalpha); /* dev, x1,y1,x2,y2,color,alpha */
		/* 2.2 Mark mid dlines areas */
		mdls=(endY-startY)/chmap->maplndis-1;
		if(mdls>0) {
			for( k=0; k<mdls; k++ ) {
				draw_blend_filled_rect(fb_dev, chmap->mapx0, startY+(k+1)*(chmap->maplndis) -1,
							chmap->mapx0+chmap->mappixpl-1, startY+(k+2)*(chmap->maplndis) -1,
                                                                      chmap->markcolor, chmap->markalpha); /* dev, x1,y1,x2,y2,color,alpha */
			}
		}

		/* 2.3 Mark end dline area */
		draw_blend_filled_rect(fb_dev, chmap->mapx0, endY, endX, endY+chmap->maplndis-1,
                                                                      chmap->markcolor, chmap->markalpha); /* dev, x1,y1,x2,y2,color,alpha */
	}

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

	/* Only if charmap cleared, charmap finish last session. */
	if( chmap->request != 0) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* Keep selection marks, as chmap->pchoff/pchoff2 unchanged */

	/* PRE_: Set chmap->txtdlncount */
	if( chmap->txtdlncount > chmap->maplines )
        	chmap->txtdlncount -= chmap->maplines;
        else  { /* It reaches the first page */
        	chmap->txtdlncount=0;
	}

	/* PRE_: Set pref to previous txtdlines position */
        chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];

        printf("%s: page up to chmap->txtdlncount=%d \n", __func__, chmap->txtdlncount);

        /* set chmap->request */
        chmap->request=1;

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

	/* Only if charmap cleared, charmap finish last session. */
	if( chmap->request != 0) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* Keep selection marks, as chmap->pchoff/pchoff2 unchanged */

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

        /* set chmap->request */
        chmap->request=1;

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
	0	OK, chmap->pref and pchoff changed.
	<0	Fails, chmap->pref and pchoff unchanged.
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
	/* Only if charmap cleared, finish last session. */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* Do not change chmap->pchoff and pchoff2 */

        /* MUST_2. chmap->txtdlncount--
	 * MUST_3. Change chmap->pref, to change charmap start postion */
        if( chmap->txtdlncount > 0 ) {
        	/* reset txtdlncount to pointer the line BEFORE first dline in current charmap page */
                chmap->pref=chmap->txtbuff + chmap->txtdlinePos[--chmap->txtdlncount];    /* Enusre txtdlinePos[] is valid */
                printf("%s: chmap->txtdlncount=%d \n", __func__, chmap->txtdlncount);
        }

        /* set chmap->request */
        chmap->request=1;

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
1. 		--- WARNING!!! ---
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
	0	OK, chmap->pref and pchoff changed.
	<0	Fails, chmap->pref and pch off unchanged.
-----------------------------------------------------*/
int FTcharmap_scroll_oneline_down(EGI_FTCHAR_MAP *chmap)
{
	int charlen=0;

	if( chmap==NULL || chmap->txtbuff==NULL)
		return -1;

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Only if charmap cleared, finish last session. */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* Do not change chmap->pchoff and pchoff2 */

	 /* Before/after calling FTcharmap_uft8string_writeFB(), champ->txtdlinePos[txtdlncount] always point to the first
          * dline: champ->txtdlinePos[txtdlncount]==chmap->maplinePos[0]
          * So we need to reset txtdlncount to pointer the next line
          */
	/* PRE_1: Update txtdlncount */
	chmap->txtdlncount++;
	/* PRE_2. Update chmap->pref */
        if(chmap->maplines>1) {
        	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount]; /* !++ */
        }
        else {  /* If chmap->maplines==1, then txtdlinePos[+1] may has no data, its value is 0 */
		/* To the end of the dline's last char */
        	charlen=cstr_charlen_uft8((const unsigned char *)(chmap->pref+chmap->charPos[chmap->chcount-1]));
                if(charlen<0) {
                	printf("%s: WARNING: cstr_charlen_uft8() negative! \n",__func__);
                        charlen=0;
                }
                chmap->pref += chmap->charPos[chmap->chcount-1]+charlen;
        }

        printf("%s: chmap->txtdlncount=%d \n", __func__, chmap->txtdlncount);

	/* set chmap->request */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*-------------------------------------------------------------
To locate chmap->pch(chmap->pch2) and pchoff/pchoff2  according
to given x,y,
chmap->pch2/pchoff2 is interlocked with chmap->pch/pchoff here!

1. !!! WARN !!!
   It will fail when chmap->request is set! So the caller
   MAY need to continue checking the result.

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

	/* Only if charmap cleared. */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* Search char map to find pch/pos for the x,y  */
	//printf("input x,y=(%d,%d), chmap->chcount=%d \n", x,y, chmap->chcount);
	/* TODO: a better way to locate i */
        for(i=0; i < chmap->chcount; i++) {
             	//printf("i=%d/(%d-1)\n", i, chmap->chcount);
             	if( chmap->charY[i] <= y && chmap->charY[i]+ chmap->maplndis > y ) {	/* locate Y notice <= to left: A <= Y < B  */
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
				/* Find the nearest charX[] */
				if( x-chmap->charX[i] > (chmap->mapx0+chmap->mappixpl-chmap->charX[i])/2 )
					chmap->pch=i+1;
				else
					chmap->pch=i;
				break;
			}
	     	}
	}

	/* interlock pch/pch2 : MUST NOT CANCEL THIS!  to cancel selection marks!*/
	chmap->pch2=chmap->pch;

	/* Set pchoff/pchoff2 accordingly */
	chmap->pchoff=chmap->pref-chmap->txtbuff+chmap->charPos[chmap->pch];
	chmap->pchoff2=chmap->pchoff; /* interlockded */

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}
/*-----------------------------------------------------------------------------
Refer to FTcharmap_locate_charPos_nolock( EGI_FTCHAR_MAP *chmap, int x, int y)
Without mutx_lock.

Note:
1. chmap->pch/pch2, pchoff/pchoff2 all updated!

------------------------------------------------------------------------------*/
static int FTcharmap_locate_charPos_nolock( EGI_FTCHAR_MAP *chmap, int x, int y)
{
	int i;

	if( chmap==NULL ) {
		printf("%s: Input FTCHAR map is empty!\n", __func__);
		return -1;
	}

	/* Search char map to find pch/pos for the x,y  */
        for(i=0; i < chmap->chcount; i++) {
             	//printf("i=%d/(%d-1)\n", i, chmap->chcount);
		/* METHOD 2: Pick the nearst charX[] to x */
             	if( chmap->charY[i] <= y && chmap->charY[i]+ chmap->maplndis > y ) {	/* locate Y, notice <= to left: A <= Y < B */
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
				/* Find the nearest charX[] */
				if( x-chmap->charX[i] > (chmap->mapx0+chmap->mappixpl-chmap->charX[i])/2 )
					chmap->pch=i+1;
				else
					chmap->pch=i;
				break;
			}
	     	}
	}

	/* interlock pch/pch2: MUST NOT CANCLE THIS, to cancel selection marks! */
	chmap->pch2=chmap->pch;

	/* set pchoff/pchoff2 accordingly */
	chmap->pchoff=chmap->pref-chmap->txtbuff+chmap->charPos[chmap->pch];
	chmap->pchoff2=chmap->pchoff;

	return 0;
}


/*---------------------------------------------------------------
To locate chmap->pch2 ONLY according to given x,y.
Keep chmap->pch unchanged.

NOTE:
1. !!! WARN !!!
   It will fail when chmap->request is set!. So the caller
   MAY need to continue checking the result.

@chmap:		The FTCHAR map.
@x,y:		A B/LCD coordinate pointed to a char.

Return:
	0	OK
	<0	Fails
----------------------------------------------------------------*/
int FTcharmap_locate_charPos2( EGI_FTCHAR_MAP *chmap, int x, int y)
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

	/* Only if charmap cleared. */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* Search char map to find pch/pos for the x,y  */
	//printf("input x,y=(%d,%d), chmap->chcount=%d \n", x,y, chmap->chcount);
        for(i=0; i < chmap->chcount; i++) {
             	//printf("i=%d/(%d-1)\n", i, chmap->chcount);
             	if( chmap->charY[i] <= y && chmap->charY[i]+ chmap->maplndis > y ) {	/* locate Y notice <= to left: A <= Y < B  */
		/* METHOD 2: Pick the nearst charX[] to x */
			/* 1. Not found OR ==chcount-1),  set to the end of the chmap. */
			if( i==chmap->chcount-1 ) {  // && chmap->charX[i] <= x ) {  /* Last char of the chmap */
				chmap->pch2=i;
				break;
			}
			/* 2. Not the last char of dline, so [i+1] is valid always! */
			else if( chmap->charX[i] <= x && chmap->charX[i+1] > x ) {
				/* find the nearest charX[] */
				if( x-chmap->charX[i] > chmap->charX[i+1]-x )
					chmap->pch2=i+1;
				else
					chmap->pch2=i;
				break;
			}
			/* 3. The last char of the dline.  */
			else if( chmap->charX[i] <= x && chmap->charY[i] != chmap->charY[i+1] ) {
				/* Find the nearest charX[] */
				if( x-chmap->charX[i] > (chmap->mapx0+chmap->mappixpl-chmap->charX[i])/2 )
					chmap->pch2=i+1;
				else
					chmap->pch2=i;
				break;
			}
	     	}
	}

	/* set pchoff/pchoff2 accordingly */
	chmap->pchoff2=chmap->pref-chmap->txtbuff+chmap->charPos[chmap->pch2];

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*------------------------------------------------------------
Reset chmap->pch2 to chmap->pch. cancel selection.

@chmap:		The FTCHAR map.
@x,y:		A B/LCD coordinate pointed to a char.

Return:
	0	OK
	<0	Fails
----------------------------------------------------------*/
int FTcharmap_reset_charPos2( EGI_FTCHAR_MAP *chmap )
{
	if( chmap==NULL ) {
		printf("%s: Input FTCHAR map is empty!\n", __func__);
		return -1;
	}

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Reset pchoff2/pch2 same as pchoff/pch */
	chmap->pchoff2=chmap->pchoff;
	chmap->pch2=chmap->pch;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*--------------------------------------------------------------------
If NOT in current charmap: then scroll to it.
Else if at upper most dline: reset chmap->pref to previous dline, and
set chmap->fix_cursor.
Else: just call locate_charPos(x,y) to update chmap->pch/pchoff
and pch2/pchoff2.

Note:
1. If in current charmap, then chmap->pchoff and pchoff2 will be
   interlocked, and any selection mark will be canclled.
   Else: not in current charmap, then selection mark keeps.

@chmap: an EGI_FTCHAR_MAP

Return:
        0       OK,    chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
-------------------------------------------------------------------*/
int FTcharmap_shift_cursor_up(EGI_FTCHAR_MAP *chmap)
{
	int off=0;
	int dln=0;

	if( chmap==NULL || chmap->txtbuff==NULL)
		return -1;

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Check request ? OR wait ??? */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* offset of pref */
	off=chmap->pref-chmap->txtbuff;

	/* 1. If chmap->pch Not in current charmap page, scroll to  */
	if( chmap->pch<0 ) {
		/* If pchoff not in current charmap page, then scroll to */
		if( chmap->pchoff < off  || chmap->pchoff > off+chmap->charPos[chmap->chcount-1] ) {
			/* Get txtdlncount corresponding to pchoff */
			dln=FTcharmap_get_txtdlIndex(chmap, chmap->pchoff);
			if(dln<0) {
				printf("%s: Fail to find index of chmap->txtdlinePos[] for pchoff!\n", __func__);
				/* --- Do nothing! --- */
			}
			else {
	        	        /* PRE_1. Update chmap->txtdlncount */
				chmap->txtdlncount=dln;
        		        /* PRE_2. Update chmap->pref */
	                	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount]; /* ensure txtdlinePos[] has valid data */
			}
		}
	}
        /* 2. If chmap->pch in current charmap */
	else if( chmap->pch >= 0 && chmap->pch < chmap->chcount )
	{
           /* 2.1 If already gets to the upper most dline */
	   if( chmap->charY[chmap->pch] == chmap->charY[0] )
	   {
        	/* Change chmap->pref, to change charmap start postion */
                if( chmap->txtdlncount > 0 ) {   /* Till to the first dline */
                	/* Before/after calling FTcharmap_uft8string_writeFB(), champ->txtdlinePos[txtdlncount]
                        * always pointer to the first dline: champ->txtdlinePos[txtdlncount]==chmap->maplinePos[0]
                        * So we need to reset txtdlncount to pointer the previous dline.
                        */
			/* PRE_1. Updated chmap->dlncount */
			/* PRE_2. Update champ->pref */
                        chmap->pref=chmap->txtbuff + chmap->txtdlinePos[--chmap->txtdlncount]; /* !--, ensure txtdlinePos[] has valid data */
                        printf("%s: Shift up,  chmap->txtdlncount=%d \n", __func__, chmap->txtdlncount);
			/* PRE_3 Set chmap->fix_cursor, then will call _get_charPos_nolock() in charmapping.
			 * chmap->pch/pch2, pchoff/pchoff2 will all be updated by charPos_nolock() */
			chmap->fix_cursor=true;
		}
	   }
	   /* 2.2 Else, move up to previous dline */
	   else {
		/* chmap->pch/pch2, pchoff/pchoff2 will all be updated by charPos_nolock() */
       		FTcharmap_locate_charPos_nolock( chmap, chmap->charX[chmap->pch],  chmap->charY[chmap->pch]-chmap->maplndis); /* exact Y */
	   }
	}

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

        return 0;
}

/*-----------------------------------------------------------
If NOT in current charmap:  then scroll to it.
Else if at bottom dline: reset chmap->pref to next dline, and
set chmap->fix_cursor.
Else if not at bottom dline: just call locate_charPos(x,y) to
update chmap->pch/pchoff and pch2/pchoff2.

Note:
1. If in current charmap, then chmap->pchoff and pchoff2 will be
   interlocked, and any selection mark will be canclled.
   Else: not in current charmap, then selection mark keeps.

@chmap: an EGI_FTCHAR_MAP

Return:
        0       OK,    chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
----------------------------------------------------------*/
int FTcharmap_shift_cursor_down(EGI_FTCHAR_MAP *chmap)
{
	int dln=0;
	int off=0;
	int charlen=0;

	if( chmap==NULL || chmap->txtbuff==NULL)
		return -1;

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Check request ? OR wait ??? */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* offset of pref */
	off=chmap->pref-chmap->txtbuff;

	/* 1. If chmap->pch Not in current charmap page, scroll to  */
	if( chmap->pch<0 ) {
		/* If pchoff not in current charmap page, then scroll to */
		/* If chmap->maplines==1, then FTcharmap_get_txtdlIndex() will fail! */
		if( chmap->pchoff < off  || chmap->pchoff > off+chmap->charPos[chmap->chcount-1] ) {
			/* Get txtdlncount corresponding to pchoff */
			dln=FTcharmap_get_txtdlIndex(chmap, chmap->pchoff);
			if(dln<0) {
				printf("%s: Fail to find index of chmap->txtdlinePos[] for pchoff!\n", __func__);
				/* --- Do nothing! --- */
			}
			else {
	        	        /* PRE_1. Update chmap->txtdlncount */
				chmap->txtdlncount=dln;
        		        /* PRE_2. Update chmap->pref */
	                	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount]; /* ensure txtdlinePos[] has valid data */
			}
		}
	}
        /* 2. If chmap->pch in current charmap */
	else if( chmap->pch >= 0 && chmap->pch < chmap->chcount )
	{
	    printf("%s: in current charmap.\n",__func__);
	    /*  2.1 If already gets to the last dline of current charmap page */
	    if( chmap->charY[chmap->pch] >= chmap->mapy0+chmap->maplndis*(chmap->maplines-1) ) {
	    // NOPE, Not canvas bottom!! if( chmap->charY[chmap->pch] == chmap->charY[chmap->chcount-1] ) {
		printf("%s: in the last dline of current charmap.\n", __func__);
		/* If EOF in current charmap page, then do nothing. */
		if( chmap->pref[chmap->charPos[chmap->chcount-1]] != '\0' ) {
			/* Only if charmap more than 1 dlines ---NOPE! */
 			// else if( chmap->maplncount>1 ) {
                	/* Before/after calling FTcharmap_uft8string_writeFB(), champ->txtdlinePos[txtdlncount]
	                 * always pointer to the first dline: champ->txtdlinePos[txtdlncount]==chmap->maplinePos[0]
        	         * So we need to reset txtdlncount to pointer the next dline
                	 */
			/* PRE_1. Update chmap->txtdlncount */
			chmap->txtdlncount++;
			/* PRE_2. Update chmap->pref */
			if(chmap->maplines>1) {  /* If chmap->maplines==1, then txtdlinePos[+1] may has no data, its value is 0 */
	                	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount]; /* !++ */
			}
			else { /* To the end of the dline's last char */
				charlen=cstr_charlen_uft8((const unsigned char *)(chmap->pref+chmap->charPos[chmap->chcount-1]));
	                	if(charlen<0) {
        	                	printf("%s: WARNING: cstr_charlen_uft8() negative! \n",__func__);
					charlen=0;
                		}
				chmap->pref += chmap->charPos[chmap->chcount-1]+charlen;
			}
			/* PRE_3. Set chmap->fix_cursor
			 * chmap->pch/pch2, pchoff/pchoff2 will all be updated by charPos_nolock() in charmapping */
			chmap->fix_cursor=true;
		}
	    }
            /* 2.2 Else move down to next dline */
            else {
		/* chmap->pch/pch2, pchoff/pchoff2 will all be updated by charPos_nolock() */
        	FTcharmap_locate_charPos_nolock(chmap, chmap->charX[chmap->pch], chmap->charY[chmap->pch]+chmap->maplndis); /* exact Y */
            }
	}

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

        return 0;
}


/*---------------------------------------------------------------------
1. If pchoff not in current charmap page, then scroll to.
2. If in current charmap:
   Increase chmap->pch, if it gets to the end of current charmap
   then scroll one dline down.

If selection mark exits, then it will changes as cursor position moves,
until pchoff and pchoff2 becomes the same!

@chmap: an EGI_FTCHAR_MAP

Return:
        0       OK,    chmap->pchoff or chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
--------------------------------------------------------------------*/
int FTcharmap_shift_cursor_right(EGI_FTCHAR_MAP *chmap)
{
	int off;
	int dln;
	int charlen;

	if( chmap==NULL || chmap->txtbuff==NULL)
		return -1;

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Check request ? OR wait ??? */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* offset of pref */
	off=chmap->pref-chmap->txtbuff;

	/* 1. If chmap->pch Not in current charmap page */
	if( chmap->pch < 0 && chmap->txtbuff[ chmap->pchoff ] != '\0' ) {

	        #if 0 /* Move pchoff forward first */
		charlen=cstr_charlen_uft8((const unsigned char *)(chmap->txtbuff+chmap->pchoff));
		if(charlen<0) {
			printf("%s: WARNING: cstr_charlen_uft8() negative! \n",__func__);
			charlen=0;
		}

		/* PRE_: Update pchoff/pchoff2  */
		if(chmap->pchoff==chmap->pchoff2) {
			chmap->pchoff += charlen;
			chmap->pchoff2=chmap->pchoff;
		}
		else   /* If pchoff/pchoff2 in selection, then do NOT interlock */
			chmap->pchoff += charlen;
		#endif /* END Move pchoff forward */

		/* If pchoff not in current charmap page, then scroll to */
		if( chmap->pchoff < off  || chmap->pchoff > off+chmap->charPos[chmap->chcount-1] ) {
			/* Get txtdlncount corresponding to pchoff */
			dln=FTcharmap_get_txtdlIndex(chmap, chmap->pchoff);
			if(dln<0) {
				printf("%s: Fail to find index of chmap->txtdlinePos[] for pchoff!\n", __func__);
				/* --- Do nothing! --- */
			}
			else {
	        	        /* PRE_1. Update chmap->txtdlncount */
				chmap->txtdlncount=dln;
        		        /* PRE_2. Update chmap->pref */
	                	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];
			}
		}
	}
        /* 2. Else Increase chmap->pch, in the current charmap page */
	else if( chmap->pch >= 0 && chmap->pch < chmap->chcount-1 ) {
		chmap->pch++;
		/* PRE_: Update pchoff/pchoff2 */
		if(chmap->pchoff==chmap->pchoff2)
			chmap->pchoff2=chmap->pchoff = off+chmap->charPos[chmap->pch];
		else	/* If in selection, then do NOT interlock with pchoff2 */
			chmap->pchoff = off+chmap->charPos[chmap->pch];
	}
	/* 3. If in the last of charmap. but NOT EOF */
	else if( chmap->pch == chmap->chcount-1 && chmap->pref[ chmap->charPos[chmap->pch] ] != '\0'  ) {
		/* PRE: Set chmap->pchoff */
		charlen=cstr_charlen_uft8((const unsigned char *)(chmap->pref+chmap->charPos[chmap->pch]));
		if(charlen<0) {
			printf("%s: WARNING: cstr_charlen_uft8() negative! \n",__func__);
			charlen=0;
		}

		/* PRE_: Set pchoff/pchoff2 relative to txtbuff */
		if(chmap->pchoff==chmap->pchoff2) {
			chmap->pchoff = off+chmap->charPos[chmap->pch]+charlen;
			chmap->pchoff2=chmap->pchoff;
		}
		else	/* If in selection, then do NOT interlock with pchoff2 */
			chmap->pchoff = off+chmap->charPos[chmap->pch]+charlen;

		/* Need to scroll down one line */
		/* PRE_. Update chmap->txtdlncount */
		chmap->txtdlncount++;
		/* PRE_. Update chmap->pref */
		if(chmap->maplines>1) {  /* If chmap->maplines==1, then txtdlinePos[+1] may has no data, its value is 0 */
                	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount]; /* !++ */
		}
		else { /* To the end of the dline's last char */
			charlen=cstr_charlen_uft8((const unsigned char *)(chmap->pref+chmap->charPos[chmap->chcount-1]));
                	if(charlen<0) {
       	                	printf("%s: WARNING: cstr_charlen_uft8() negative! \n",__func__);
				charlen=0;
               		}
			chmap->pref += chmap->charPos[chmap->chcount-1]+charlen;
		}

	}

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

        return 0;
}


/*---------------------------------------------------------------------
Decrease chmap->pch, if it gets to the start of current
charmap then scroll one dline up.

If selection mark exits, then it will changes as cursor position moves,
until pchoff and pchoff2 becomes the same!

@chmap: an EGI_FTCHAR_MAP

Return:
        0       OK,    chmap->pchoff or chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
--------------------------------------------------------------------*/
int FTcharmap_shift_cursor_left(EGI_FTCHAR_MAP *chmap)
{
	int off;
	int dln;
	int doff;
	int charlen;


	if( chmap==NULL || chmap->txtbuff==NULL)
		return -1;

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Check request ? OR wait ??? */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* offset of pref */
	off=chmap->pref-chmap->txtbuff;

	/* 1. If chmap->pch Not in current charmap page */
	if( chmap->pch < 0 ) {

		/* Get to the very beginning */
		if(chmap->pchoff==0 )
			charlen=0;
		else {
			#if 1
			charlen=0;
			#else
			/* TODO: Move pchoff backward 1 char  */
			charlen=cstr_charlen_uft8((const unsigned char *)(chmap->txtbuff+chmap->pchoff));
			if(charlen<0) {
				printf("%s: WARNING: cstr_charlen_uft8() negative! \n",__func__);
			}
			#endif
		}

		/* PRE_: Update pchoff/pchoff2  */
		if(chmap->pchoff==chmap->pchoff2) { /* If no selection, interlock with pchoff2 */
			chmap->pchoff -= charlen;
			chmap->pchoff2=chmap->pchoff;
		}
		else 				/* If pchoff/pchoff2 in selection, then do NOT interlock */
			chmap->pchoff -= charlen;

		/* If pchoff still not in current charmap page, then scroll to its dline*/
		if( chmap->pchoff < off  || chmap->pchoff > off+chmap->charPos[chmap->chcount-1] ) {
			/* Get txtdlncount corresponding to pchoff */
			dln=FTcharmap_get_txtdlIndex(chmap, chmap->pchoff);
			if(dln<0) {
				printf("%s: Fail to find index of chmap->txtdlinePos[] for pchoff!\n", __func__);
				/* --- Do nothing! --- */
			}
			else {
	        	        /* PRE_: Update chmap->txtdlncount */
				chmap->txtdlncount=dln;
        		        /* PRE_: Update chmap->pref */
	                	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];
				/* In charmapping, chmap->pch will be updated according to chmap->pchoff */
			}
		}
	}

        /* 2. Else Increase chmap->pch, in the current charmap page */
	else if( chmap->pch > 0 ) {
		chmap->pch--;
		/* PRE_: Update pchoff/pchoff2 */
		if(chmap->pchoff==chmap->pchoff2)
			chmap->pchoff2=chmap->pchoff = off+chmap->charPos[chmap->pch];
		else	/* If in selection, then do NOT interlock with pchoff2 */
			chmap->pchoff = off+chmap->charPos[chmap->pch];
	}
	/* 3. If the first of charmap, but NOT first txtdline. need to  scroll up one dline  */
	else if( chmap->pch == 0 && chmap->txtdlncount>0 ) {
		/* PRE_: Update chmap->txtdlncount */
		chmap->txtdlncount--;
		/* PRE_: Set chmap->pchoff,to relocate pch */
		doff = FTcharmap_getPos_lastCharOfDline(chmap, chmap->txtdlncount);
		if( doff<0) {
			printf("%s: getPos_lastCharOfDline() fails!\n",__func__);
			doff=0;
		}
		printf("%s: getPos_lastCharOfDline() doff=%d \n",__func__, doff);
		/* PRE_: Set pchoff/pchoff2 relative to txtbuff */
		if(chmap->pchoff==chmap->pchoff2) {
			chmap->pchoff= chmap->txtdlinePos[chmap->txtdlncount] + doff;
			chmap->pchoff2=chmap->pchoff;
		}
		else	/* If in selection, then do NOT interlock with pchoff2 */
			chmap->pchoff += chmap->txtdlinePos[chmap->txtdlncount];

		/* PRE_: Update chmap->pref */
	        chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];
	}

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

        return 0;
}


/*--------------------------------------------------------
If cursor not in current charmap, then scroll to.
Else set pchoff/pchoff2 to point the head char of its dline.

@chmap: an EGI_FTCHAR_MAP

Return:
        0       OK,    chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
-------------------------------------------------------*/
int FTcharmap_goto_lineBegin( EGI_FTCHAR_MAP *chmap )
{
	int off;
	int dln;

        if( chmap==NULL ) {
                printf("%s: Input FTCHAR map is empty!\n", __func__);
                return -1;
        }

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Check request ? OR wait ??? */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* offset of pref */
	off=chmap->pref-chmap->txtbuff;

	/* 1. If chmap->pch Not in current charmap page */
	if( chmap->pch < 0 ) {
		/* If pchoff not in current charmap page, then scroll to */
		if( chmap->pchoff < off  || chmap->pchoff > off+chmap->charPos[chmap->chcount-1] ) {  /* NOT necessary ? */
			/* Get txtdlncount corresponding to pchoff */
			dln=FTcharmap_get_txtdlIndex(chmap, chmap->pchoff);
			if(dln<0) {
				printf("%s: Fail to find index of chmap->txtdlinePos[] for pchoff!\n", __func__);
				/* --- Do nothing! --- */
			}
			else {
	        	        /* PRE_1. Update chmap->txtdlncount */
				chmap->txtdlncount=dln;
        		        /* PRE_2. Update chmap->pref */
	                	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];
			}
		}
	}
	/* 2. In current charmap */
	else {
		 /* Get txtdlncount corresponding to pchoff */
                  dln=FTcharmap_get_txtdlIndex(chmap, chmap->pchoff);
		  if(dln<0) {
                                printf("%s: Fail to find index of chmap->txtdlinePos[] for pchoff!\n", __func__);
                                /* --- Do nothing! --- */
                  }
                  else {
                                /* PRE_1. Update pchoff/pchoff2 */
                                chmap->pchoff=chmap->txtdlinePos[dln];
				chmap->pchoff2=chmap->pchoff; /* interlocked */
                 }
	}

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

        return 0;
}


/*---------------------------------------------------------
If cursor not in current charmap, then scroll to.
Else set pchoff/pchoff2 to point the end char of its dline.

@chmap: an EGI_FTCHAR_MAP

Return:
        0       OK,    chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
----------------------------------------------------------*/
int FTcharmap_goto_lineEnd( EGI_FTCHAR_MAP *chmap )
{
	int off;
	int dln;

        if( chmap==NULL ) {
                printf("%s: Input FTCHAR map is empty!\n", __func__);
                return -1;
        }

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }
	/* Check request ? OR wait ??? */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* offset of pref */
	off=chmap->pref-chmap->txtbuff;

	/* 1. If chmap->pch Not in current charmap page */
	if( chmap->pch < 0 ) {
		/* If pchoff not in current charmap page, then scroll to */
//		if( chmap->pchoff < off  || chmap->pchoff > off+chmap->charPos[chmap->chcount-1] ) {  /* NOT necessary ? */
			/* Get txtdlncount corresponding to pchoff */
			dln=FTcharmap_get_txtdlIndex(chmap, chmap->pchoff);
			if(dln<0) {
				printf("%s: Fail to find index of chmap->txtdlinePos[] for pchoff!\n", __func__);
				/* --- Do nothing! --- */
			}
			else {
	        	        /* PRE_1. Update chmap->txtdlncount */
				chmap->txtdlncount=dln;
        		        /* PRE_2. Update chmap->pref */
	                	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];
			}
//		}
	}
	/* 2. In current charmap */
	else {
		/* Move pch to the last of its dline */
		while(  chmap->pch < chmap->chcount-1
			&& chmap->charY[chmap->pch] == chmap->charY[chmap->pch+1] )  /* pch increment if at same dline */
		{
			chmap->pch++;
		}
                /* PRE_1. Update pchoff/pchoff2 */
                chmap->pchoff=off+chmap->charPos[chmap->pch];
		chmap->pchoff2=chmap->pchoff; /* interlocked */
	}

	/* Set chmap->request for charmapping */
	chmap->request=1;

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
	int charlen=0;

	/* Check input */
        if( chmap==NULL ) {
                printf("%s: Input FTCHAR map is empty!\n", __func__);
                return -1;
        }
	if( dln<0 || dln > chmap->txtdlines-1 ) {
                printf("%s: Input index of txtdlnPos[] out of range!\n", __func__);
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


/*-----------------------------------------------------------------
TODO: TEST
Get index of chmap->txtdlinePos[], the input chmap->pchoff
is located somewhere of the txtdline.

Note:
1. !!! WARNING !!!
Forward search may return incorrect data in chmap->txtdlinePos[]!
in case those txtlines have NOT been charmapped or updated.
Avoid to call this function in such case. example: when inserting
words at the end of current charmap, and want to scroll down withou
updating charmap.


@chmap: 	an EGI_FTCHAR_MAP
@pchoff:	chmap->pchoff pointing to the cursor position.

Return:
        >0       OK,   return index of txtdlinePos[]
        <0      Fails
------------------------------------------------------------------*/
int FTcharmap_get_txtdlIndex(EGI_FTCHAR_MAP *chmap,  int pchoff)
{
	int i;
	int off;

	/* Check input */
        if( chmap==NULL ) {
                printf("%s: Input FTCHAR map is empty!\n", __func__);
                return -1;
        }

	/* Always the first dline */
	if( pchoff==0 ) {
		return 0;
	}
	/* Out of range */
	else if( pchoff<0 || pchoff > chmap->txtlen ) {   /* chmap->txtlen not includes EOF */
                printf("%s: Input pchoff out of range!\n", __func__);
                return -2;
	}

	/* Execption: when chmap->maplines==1, and pchoff at current dline
	 * example:  FTcharmap_go_lineBegin(), when chmap->maplines==1.
	 */
	if( chmap->maplines==1 ) {
		printf("%s: maplines==1\n",__func__);
		off=chmap->pref-chmap->txtbuff;
		if( pchoff >= off && pchoff<= off+chmap->charPos[chmap->chcount-1] ) /* ONLY one dline */
			return	chmap->txtdlncount;  /* Only one dline, chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0] */
	}

	/* Search index:
	 *  NOTE:  1. chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0],
	 *  	  2. txtdlinePos[txtdlncount+1],txtdlinePos[txtdlncount+2]... +maplncount-1] still holds valid data.
	 */
	//for(i=1; i < chmap->txtdlncount + chmap->maplncount; i++) {
  	for(i=1; i < chmap->txtdlines; i++) {  /* WARNING: may be un_charmapped or old data */
  		if( chmap->txtdlinePos[i] == 0 ) /* i>0;  0 as un_charmapped data */
 			break;
		else if( chmap->txtdlinePos[i] > pchoff )
			return	i-1;
	}


	/* Not found */
	return -3;
}


/*-------------------------------------------------------------
1. If selection marks(pchoff!=pchoff2):
   Delete all selected chars, and if pch<0, set scroll to dline
   where pchoff located.

   ( For pchoff==pchoff2, no selection marks)
2. If pch NOT in current charmap:
   set scroll to dline where pchoff located.
3. Else if pch in current charmap:
   3.1. If chmap->pch points to the leftmost of the current top
     displine then scroll one dline up after moving backward.
     and reset chmap->pchoff.
   3.2 Else move backward and change chmap->pchoff.

@chmap: 	Pointer to the EGI_FTCHAR_MAP.

 Return:
        0       OK,    chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
-------------------------------------------------------------*/
int FTcharmap_go_backspace( EGI_FTCHAR_MAP *chmap )
{
	int off;
	int dln;
	int pos;
	int startPos, endPos;

	/* Check input */
        if( chmap==NULL ) {
                printf("%s: Input FTCHAR map is empty!\n", __func__);
                return -1;
        }

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Check request ? OR wait ??? */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* offset of pref */
	off=chmap->pref-chmap->txtbuff;

	printf("%s: backspace chmap->pch=%d \n", __func__, chmap->pch);

	/* 1. If selection marks, delete all selected chars. */
	if( chmap->pchoff != chmap->pchoff2 ) {
		if(chmap->pchoff > chmap->pchoff2) {
			startPos=chmap->pchoff2;
			endPos=chmap->pchoff;
		}
		else {
			startPos=chmap->pchoff;
			endPos=chmap->pchoff2;
		}
		/* Move followed chars backward to cover selected chars. */
		memmove( chmap->txtbuff+startPos, chmap->txtbuff+endPos, strlen((char *)(chmap->txtbuff+endPos)) +1);  /* +1 EOF */
		/* PRE_: Update txtlen */
		chmap->txtlen -= (endPos-startPos);
		/* PRE_: Set pchoff2=pchoff=startPos */
		chmap->pchoff2=startPos;
		chmap->pchoff=startPos;

		/* If startPos NOT in current charmap */
		if(  startPos < off+chmap->charPos[0] || startPos > off+chmap->charPos[chmap->chcount-1] ) {
			/* 3. Reset chmap->pref for re_charmapping */
			dln=FTcharmap_get_txtdlIndex(chmap,  chmap->pchoff);
			if(dln<0) {
				printf("%s: dln is nagtive, FTcharmap_get_txtdlIndex() fails!\n",__func__);
				/* --- Do nothing! --- */
			}
			/* PRE_:  Set chmap->txtdlncount */
			chmap->txtdlncount=dln;
			/* PRE_:  Set chmap->pref */
			chmap->pref=chmap->txtbuff+chmap->txtdlinePos[dln];
	  	}
	}
	/* 2. pchoff==pchoff2: If chmap->pch Not in current charmap page, set to scroll to dline */
	else if( chmap->pch < 0 ) {
		printf("%s: pch not in current charmap!\n", __func__);
		/* If pchoff still not in current charmap page, then scroll to its dline*/
		if( chmap->pchoff < off  || chmap->pchoff > off+chmap->charPos[chmap->chcount-1] ) {
			/* Get txtdlncount corresponding to pchoff */
			dln=FTcharmap_get_txtdlIndex(chmap, chmap->pchoff);
			if(dln<0) {
				printf("%s: Fail to find index of chmap->txtdlinePos[] for pchoff!\n", __func__);
				/* --- Do nothing! --- */
			}
			else {
	        	        /* PRE_: Update chmap->txtdlncount */
				chmap->txtdlncount=dln;
        		        /* PRE_: Update chmap->pref */
	                	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];
				/* In charmapping, chmap->pch will be updated according to chmap->pchoff */
			}
		}
	}
        /* 3. pchoff==pchoff2: If chmap->pch in current charmap */
        else if( chmap->pch >= 0 && chmap->pch < chmap->chcount )
        {

	   /* 3.1 If current pch gets leftmost of the top dline. But NOT to the beginning of txtbuff, then scroll one line up after moving string! */
	   if( chmap->pch==0 && chmap->pref != chmap->txtbuff ) {
		printf("%s: chmap->txtdlncount=%d \n", __func__, chmap->txtdlncount);

		/* Get pos offset of the last char in previous dline */
		pos=FTcharmap_getPos_lastCharOfDline(chmap, chmap->txtdlncount-1); /* relative to pline */
		pos=chmap->txtdlinePos[chmap->txtdlncount-1]+pos;  		   /* relative to txtbuff */

		/* Move following string backward to delete previous char */
	   	memmove( chmap->txtbuff+pos, chmap->pref+chmap->charPos[chmap->pch],  /* pch==0 */
						    strlen((char *)(chmap->pref+chmap->charPos[chmap->pch])) +1); /* +1 for string end */

		/* MUST_1. Updated chmap->txtlen */
		chmap->txtlen -= cstr_charlen_uft8((const unsigned char *)(chmap->txtbuff+pos));

				/* Scroll one dline up */
		/* MUST_2. Set chmap->pchoff immediately for charmap */
		chmap->pchoff=pos; /*: set cursor position relative to txtbuff */
		chmap->pchoff2=chmap->pchoff; /* interlocked */
		/* MUST_3. Set chmap->txtdlncount */
		chmap->txtdlncount--;
		/* MUST_4. Set pref to previous txtdline before charmap and set txtdlncount also */
                chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];
	   }

	   else if( chmap->pch >0 ) {  /* To rule out chmap->pref == chmap->txtbuff!! */
	   /* ! move whole string forward..
            * NOTE: DEL/BACKSPACE operation acturally copys string including only ONE '\0' at end.
	    *  After deleting/moving a more_than_1_byte_width uft-8 char, there are still several bytes
	    *  need to be cleared. and EOF token MUST reset when insert char at the end of txtbuff[] laster.
	    */
		memmove(chmap->pref+chmap->charPos[chmap->pch-1], chmap->pref+chmap->charPos[chmap->pch],
							    strlen((char *)(chmap->pref+chmap->charPos[chmap->pch])) +1); /* +1 for string end */

		/* charPos[] NOT updated, before calling charmap_writeFB ! */
		pos=chmap->pref-chmap->txtbuff +chmap->charPos[chmap->pch-1]; /* relative to txtbuff */

		/* MUST_1. Update chmap->txtlen */
		chmap->txtlen -= chmap->charPos[chmap->pch]-chmap->charPos[chmap->pch-1];
		/* MUST_2. Update chmap->pchoff/pchoff2 */
		chmap->pchoff=pos;
		chmap->pchoff2=chmap->pchoff; /* interlocked */
	   }

	}

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*---------------------------------------------------------------
If NOT in current charmap:
	Set to scroll to dline where pchoff located.
Else:
	Insert a char(ASCII or UFT-8 encoding) into the chmap->txtbuff.

			!!! --- WARNING --- !!!

All chars MUST be printable, OR '\n', OR TAB, OR legal UFT-8 encoded.
It MAY crash chmapping if any illegal char exists in champ->txtbuff.??

@chmap:         Pointer to the EGI_FTCHAR_MAP.
@ch:		Pointer to inserting char.

Return:
        0       OK,    chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
------------------------------------------------------------------*/
int FTcharmap_insert_char( EGI_FTCHAR_MAP *chmap, const char *ch )
{
	int chsize;  /* Size of inserted char */
	int off;
	int dln;
	//int chns;    /* Total number of inserted chars, ==1 NOW */

	/* Check input */
        if( chmap==NULL ) {
                printf("%s: Input FTCHAR map is empty!\n", __func__);
                return -1;
        }
	/* Verify pch */
	chsize=cstr_charlen_uft8((const unsigned char *)ch);
	if(chsize<0) {
                printf("%s: unrecognizable UFT-8 char!\n", __func__);
		return -2;
	}
	/* ASCII printable chars + '\n' + TAB */
	else if( chsize==1 && !(*ch>31 || *ch==9 || *ch==10) ){
                printf("%s: ASCII '%d' not printable!\n", __func__, *ch);
		return -3;
	}

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Check request ? OR wait ??? */
	if( chmap->request !=0 ) {
		printf("%s: chmap->request !=0 \n",__func__);
        	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* offset of pref */
	off=chmap->pref-chmap->txtbuff;

	/* 0. Get totabl number of inserting char */
	//chns=1;			/* 1 char only NOW */

	/* 1. Check txtbuff space */
	if( chmap->txtlen +chsize > chmap->txtsize-1 ) {
		printf("%s: chmap->txtbuff is full! Fail to insert char. chmap->txtlen=%d, chmap->txtsize-1=%d \n",
										 __func__, chmap->txtlen, chmap->txtsize-1);
		chmap->errbits |= CHMAPERR_TXTSIZE_LIMIT;
		/* TODO: txtsize auto. grow */

        /*  <-------- Put mutex lock */
        	pthread_mutex_unlock(&chmap->mutex);
		return -4;
	}

	printf("current chmap->pch=%d\n", chmap->pch);

	/* 1. If chmap->pch Not in current charmap page, set to scroll to dline */
	if( chmap->pch < 0 ) {
		printf("%s: pch not in current charmap!\n", __func__);
		/* If pchoff still not in current charmap page, then scroll to its dline*/
		if( chmap->pchoff < off  || chmap->pchoff > off+chmap->charPos[chmap->chcount-1] ) {
			/* Get txtdlncount corresponding to pchoff  */
			dln=FTcharmap_get_txtdlIndex(chmap, chmap->pchoff);
			if(dln<0) {
				printf("%s: Fail to find index of chmap->txtdlinePos[] for pchoff!\n", __func__);
				/* --- Do nothing! --- */
			}
			else {
	        	        /* PRE_: Update chmap->txtdlncount */
				chmap->txtdlncount=dln;
        		        /* PRE_: Update chmap->pref */
	                	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];
				/* In charmapping, chmap->pch will be updated according to chmap->pchoff */
			}
		}
	}
        /* 2. If chmap->pch in current charmap */
        else if( chmap->pch >= 0 && chmap->pch < chmap->chcount )
        {
	   /* 2.1  Insert char at EOF of chmap->txtbuff, OR start of an empty txtbuff */
	   if(chmap->pref[ chmap->charPos[ chmap->pch] ]=='\0') {  /* Need to reset charPos[] at charmapping */
		printf("%s: Insert a char at EOF.\n", __func__);

		/* 1 Insert char to the EOF, notice that charPos[] have not been updated yet, it holds just insterting pos. */
		strncpy((char *)chmap->pref+chmap->charPos[chmap->pch], ch, chsize);

		/* TEST: Confirm txtbuff EOF */
		#if 0
		if(chmap->pref[chmap->charPos[chmap->pch]] != 0 ) {
				printf("Error occurs as txtbuff EOF=%d!\n",*(unsigned char *)(chmap->pref+chmap->charPos[chmap->pch]) );
		}
		#endif

		/* 2 --- Always reset txtbuff EOF here! ----
		 * NOTE: DEL/BACKSPACE operation acturally copys string including only ONE '\0' at end, NOT just move and completely clear !
		 * So after deleting/moving a more_than_1_byte_width uft-8 char, there are still several bytes need to be cleared.
		 * For the case, we insert the char just at the fore-mentioned position of '\0', and the next byte may be NOT '\0'!
		 */
		chmap->pref[chmap->charPos[chmap->pch]]='\0';

		/* 3 Update chmap->txtlen: size check at beginning. */
		chmap->txtlen +=chsize;

		/* !!!Warning: now chmap->pch MAY out of displaying txtbox range! charXY is undefined as initial value (0,0)!
		 * champ->pchoff/pchoff2 to be modified accordingly for pch/pch2, see below.
		 */
	   }
	   /* 2.2 Insert char not at EOF */
	   else {
		printf("%s: Insert at chmap->pch=%d of chcount=%d\n", __func__,chmap->pch, chmap->chcount);

		/* 1 Move string forward to leave space for the inserting char */
		memmove(chmap->pref+chmap->charPos[chmap->pch]+chsize, chmap->pref+chmap->charPos[chmap->pch],
							strlen((char *)(chmap->pref+chmap->charPos[chmap->pch] )) +1);
		/* 2 Insert the char, notice that charPos[] have not been updated yet, it holds just  insterting pos. */
		strncpy((char *)chmap->pref+chmap->charPos[chmap->pch], ch, chsize);

		/* 3 Update chmap->txtlen: size check at beginning. */
		chmap->txtlen +=chsize;
	     }

	     /* In case that inserting a new char just at the end of the charmap, and AFTER charmapping: chmap->pch > chmap->chcount-1
	      *  (here chmap->pch is preset as above ), then we need to scroll one line down to update the cursor.
	      *  we'll need pchoff to locate the typing position in charmapping function.
	      */
	     chmap->pchoff=off+chmap->charPos[chmap->pch]+chsize; /* : NOW typing/inserting cursor position relative to txtbuff */
	     chmap->pchoff2 = chmap->pchoff;  /* interlocked with pchoff */
	     printf("%s: pchoff2=phoff=%d \n",__func__, chmap->pchoff);

	     /* TODO: ONLY if pchoff out of current charmap, or newline '\n',  scroll down one dline then.... */
	     /* If at the end of charmap(may still in current charmap), then scroll down,
	      * if chmap->maplines==1!, then txtdlinePos[+1] MAY have NO data!!
	      */
	     if(chmap->pch == chmap->chcount-1 && chmap->maplines >1 ) {
        	        /* PRE_: Update chmap->txtdlncount */
			chmap->txtdlncount +=1;
       		        /* PRE_: Update chmap->pref */
                	chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];
	     }
	     /* ELSE if (chmap->maplines==1), then after insertion chmap->pch will not in current charmap page,
	      * and it will scroll to dline first, see above in if(chmap->pch<0) { ,,,,}  */

	}

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*----------------------------------------------------------------------------
If ( pchoff==pchoff2) delete a char preceded by cursor.
If ( pchoff!=pchoff1) delete all chars between pchoff and pchoff2.

Note:
1. If NOT in current charmap, reset chmap->txtdlncount,pref after deleting.
   Point chmap->pref to the dline where chmap->pchoff located.
2. Otherwise deletes the char pointed by pch, and chmap->pch/pchoff keeps unchanged!
3. The EOF will NOT be deleted!

@chmap:         Pointer to the EGI_FTCHAR_MAP.

Return:
        0       OK
        <0      Fail
---------------------------------------------------------------------------*/
int FTcharmap_delete_char( EGI_FTCHAR_MAP *chmap )
{
	int charlen=0;
	int startPos=0;
	int endPos=0;
	int dlindex=0;
	int off=0;

	/* Check input */
        if( chmap==NULL || chmap->txtbuff==NULL ) {
                printf("%s: Input FTCHAR map is empty!\n", __func__);
                return -1;
        }

        /*  Get mutex lock   ----------->  */
        if(pthread_mutex_lock(&chmap->mutex) !=0){
                printf("%s: Fail to lock charmap mutex!", __func__);
                return -2;
        }

	/* Check request ? OR wait ??? */
	if( chmap->request !=0 ) {
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	#if 0 /* -----TEST:  */
	wchar_t wcstr;
	if(char_uft8_to_unicode(chmap->pref+chmap->charPos[chmap->pch], &wcstr) >0) {
		printf("Del wchar = %d\n", wcstr);
	}
	#endif

	/* NOTE: DEL/BACKSPACE operation acturally copys string including only ONE '\0' at end.
         *  After deleting/moving a more_than_1_byte_width uft-8 char, there are still several bytes
         *  need to be cleared. and EOF token MUST reset when insert char at the end of txtbuff[] later.
         */


        /* Only if txtbuff is NOT empty  */
        if( chmap->txtlen > 0 ) {

		/* 1. If selection marks, delete all selected chars. */
		if( chmap->pchoff != chmap->pchoff2 ) {
			if(chmap->pchoff > chmap->pchoff2) {
				startPos=chmap->pchoff2;
				endPos=chmap->pchoff;
			}
			else {
				startPos=chmap->pchoff;
				endPos=chmap->pchoff2;
			}
			/* Move followed chars backward to cover selected chars. */
			memmove( chmap->txtbuff+startPos, chmap->txtbuff+endPos, strlen((char *)(chmap->txtbuff+endPos)) +1);  /* +1 EOF */
			/* PRE_: Update txtlen */
			chmap->txtlen -= (endPos-startPos);
			/* 2. PRE_: Set pchoff2=pchoff=startPos */
			chmap->pchoff2=startPos;
			chmap->pchoff=startPos;

			/* If startPos NOT in current charmap */
			off=chmap->pref-chmap->txtbuff;
			if(  startPos < off+chmap->charPos[0] || startPos > off+chmap->charPos[chmap->chcount-1] ) {
				/* 3. Reset chmap->pref for re_charmapping */
				dlindex=FTcharmap_get_txtdlIndex(chmap,  chmap->pchoff);
				if(dlindex<0) {
					printf("%s: dlindex is nagtive, FTcharmap_get_txtdlIndex() fails!\n",__func__);
					dlindex=0;
				}
				/* PRE_:  Set chmap->txtdlncount */
				chmap->txtdlncount=dlindex;
				/* PRE_:  Set chmap->pref */
				chmap->pref=chmap->txtbuff+chmap->txtdlinePos[dlindex];
		  	}
		}

		/* 2. No selection marks, and chmap->pchoff(pchoff2) NOT in current charmpa */
		else  if( chmap->pchoff == chmap->pchoff2 && chmap->pch < 0) {

			/* Get charlen and start/endPos*/
                        charlen=cstr_charlen_uft8(chmap->txtbuff+chmap->pchoff);
			startPos=chmap->pchoff;
			endPos=chmap->pchoff+charlen;

			/* Move followed chars backward to cover selected chars. */
			memmove( chmap->txtbuff+startPos, chmap->txtbuff+endPos, strlen((char *)(chmap->txtbuff+endPos)) +1);  /* +1 EOF */
			/* 1. Update txtlen */
			chmap->txtlen -= (endPos-startPos);


			/* 2. PRE_: keep pchoff and pchoff2 */
			/* 3. Reset chmap->pref for re_charmapping */
			dlindex=FTcharmap_get_txtdlIndex(chmap,  chmap->pchoff);
			if(dlindex<0) {
				printf("%s: dlindex is nagtive, FTcharmap_get_txtdlIndex() fails!\n",__func__);
				dlindex=0;
			}
			/* PRE_1:  Set chmap->txtdlncount */
			chmap->txtdlncount=dlindex;
			/* PRE_2:  Set chmap->pref */
			chmap->pref=chmap->txtbuff+chmap->txtdlinePos[dlindex];
		}

		/* 3. NO selection marks, chmap->pch in current charmap, But NOT the end */
                else if( chmap->pch < chmap->chcount-1 ) {
			/* Move followed chars backward to cover the char pointed by pch. */
                	memmove( chmap->pref+chmap->charPos[chmap->pch], chmap->pref+chmap->charPos[chmap->pch+1],
                                                                       strlen((char *)(chmap->pref+chmap->charPos[chmap->pch+1])) +1); /* +1 EOF */
                	/* 1. Update txtlen: charPos[] not updated yet. (before calling charmap_writeFB ) */
                        chmap->txtlen -= chmap->charPos[chmap->pch+1]-chmap->charPos[chmap->pch];
                       /* If reset EOF here, it must clear length of the deleted uft8 char. so we'd rather reset EOF when
			* inserting char at the end of txtbuff. */
                 }

                 /* 4. NO selection marks, chmap-pch is the last pch of current charmap (but NOT the EOF), then [pch+1] is NOT available! */
                 else if( chmap->pch == chmap->chcount-1 && chmap->pref[chmap->charPos[chmap->pch]] != '\0' ) {
                 	printf("%s: --- DEL last pch --- \n", __func__);
                        /* if chmap->pref[chmap->charPos[pch]] == '\0' then charlen=... will be -1, */
                        charlen=cstr_charlen_uft8(chmap->pref+chmap->charPos[chmap->pch]);
                        //printf("%s: char=%d,  charlen=%d \n",chmap->pref[chmap->charPos[chmap->pch]], charlen);
			/* Move followed chars backward to cover the char pointed by pch. */
                        memmove( chmap->pref+chmap->charPos[chmap->pch],
                                                                  chmap->pref+chmap->charPos[chmap->pch]+charlen,
                                                                  strlen((char *)(chmap->pref+chmap->charPos[chmap->pch]+charlen)) +1 ); /* +1 EOF */
                        /* Update txtlen */
                        chmap->txtlen -= charlen;
                       /* If reset EOF here, it must clear length of the deleted uft8 char. so we'd rather reset EOF when
			* inserting char at the end of txtbuff. */
                 }
	}
	/* ELSE: txtlen==0, chmap->txtbuff is empty! */

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}



