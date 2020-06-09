/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Note:
1. FreeType 2 is licensed under FTL/GPLv3 or GPLv2.
   Based on freetype-2.5.5

2. If there are any selection marks, but not displayed in current charmap,
   To press 'DEL' or 'BACKSPACE':  it first deletes those selected chars,
   then scrolls charmap to display/locate the typing cursor.
   To press any other key: it just scrolls charmap to display/locate selection marks.

3. Sometimes it may needs more than one press/operation ( delete, backspace, shift etc.)
   to make the cursor move, this is because there is/are unprintable chars
   with zero width.


			--- Definition and glossary ---

0. cursor:  A blinking cursor is an editing point, corresponding with chmap->pchoff pointing to some where of
	    chmap->txtbuff, to/from which you may add/delete chars.

					--- NOTICE ---
	    1. A cursor usually can move to the left side of the last char of a dline, NOT the right side.
	    If a cursor can move to the right side of the last char of a dline, or say end of the dline, that
	    means it is a newline char ('\n'), OR it's the EOF.
	    2. Sometimes it may needs more than one press/operation ( delete, backspace, shift etc.)
   	    to make the cursor move, this is because there is/are unprintable chars with zero width.
	    3. If the cursor(pchoff) in NOT shown in current charmap, just press any key to scroll to locate it.
	    				!!! WARNING !!!
	    If you press the deleting key, it will execute anyway, even the cursor(pchoff) is NOT in the current charmap!
	    though it will scroll to cursor position after deletion, you may NOT be aware of anything that have been deleted.
	    So always bring the cursor within your sight before editing!

1. char:    A printable ASCII code OR a local character with UFT-8 encoding.
2. charmap: A EGI_FTCHAR_MAP struct that holds data of currently displayed chars,
	    of both their corresponding coordinates in displaying window and offset position in memory.
	    FAINT WARNING: Do NOT confuse with Freetype charmap concept!
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

7. EOF:		For txtbuff: 	  A '\0' token to signal as end of the buffered string.
		For saved file:   A '\n' to comply with UNIX/POSIX file end standard.

8. Selection mark:
		Two offsets of txtbuff, pchoff and pchoff2, which defines selected content/chars btween them.
		if(pchoff==pchoff2), then no selection.

                        	<--- PRE_Charmap Actions --->

PRE_1:  Set chmap->txtdlncount       	( txtdlinePos[] index,a link of txtbuff->charmap
					  as txtdlinePos[txtdlncount]=pref-txtbuff
					  and chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0] )
PRE_2:  Set chmap->pref		     	( starting point for charmapping )
PRE_3:  Set chmap->pchoff/pchoff2     	( chmap->pch/pch2: to be derived from pchoff/pchoff2 in charmapping! )
PRE_4:  Set chmap->fix_cursor 	    	( option: keep cursor position unchanged on screen  )
PRE_5:	Set chmap->follow_cursor 	( option: auto. scroll to keep cursor always in current charmap )
PRE_6:  Set chmap->request		( request charmapping exclusively and immediately )


			<---  DO_Charmap: FTcharmap_uft8strings_writeFB() --->

charmap_1:	Update chmap->chcount
Charmap_2:	Update chmap->charX[], charY[],charPos[]

charmap_3:	Update chmap->maplncount
charmap_4:	Update chmap->maplinePos[]

charmap_5:	Update chmap->txtdlcount   ( NOTE: chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0] )
charmap_6:	Update chmap->txtdlinePos[]

charmap_7:	Check chmap->follow_cursor
charmap_8:	Check chmap->fix_cursor

charmap_9:	Draw selection mark


                <--- POST_Charmap Actions: Also see FTcharmap_uft8strings_writeFB() --->

POST_1: Redraw cursor
POST_2: Check chmap->errbits


TODO:
1. A faster way to memmove...
2. A faster way to locate pch...
3. Delete/insert/.... editing functions will fail/abort if chmap->request!=0. ( loop trying/waiting ??)

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include "egi_log.h"
#include "egi_debug.h"
#include "egi_FTcharmap.h"
#include "egi_cstring.h"
#include "egi_utils.h"
#include "egi_timer.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>

static int FTcharmap_locate_charPos_nolock( EGI_FTCHAR_MAP *chmap, int x, int y);	/* No mutex_lock */
static void FTcharmap_mark_selection(FBDEV *fb_dev, EGI_FTCHAR_MAP *chmap);		/* No mutex_lock */


/*-------------------------------------------------
Drawing cursor as of EGI_FTCHAR_MAP.draw_cursor
@x,y:		origin point.
@lndis:		distance between two lines.
--------------------------------------------------*/
static void FTcharmap_draw_cursor(int x, int y, int lndis )
{
	fbset_color(WEGI_COLOR_RED);
        //draw_wline(&gv_fb_dev, x, y, x+1, y+lndis-1,2);
        draw_filled_rect(&gv_fb_dev, x, y, x+1, y+lndis-1);
}


/*------------------------------------------------
To create a char map with given params.

@txtsize: Size of txtbuff[], exclude '\0'.
	 ! txtsize+1(EOF) bytes to be allocated.
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
EGI_FTCHAR_MAP* FTcharmap_create(size_t txtsize,  int x0, int y0,  int height, int width, int offx, int offy,
				 size_t mapsize, size_t maplines, size_t mappixpl, int maplndis )
{
	if( height<=0 || width <=0 ) {
		printf("%s: Input height and/or width is invalid!\n",__func__);
		return NULL;
	}

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
	chmap->txtsize=txtsize+1;   /* +1 EOF */
	chmap->pref=chmap->txtbuff; /* Init. pref pointing to txtbuff */

	/* TEST */
	printf(" --- TEST ---: sizeof(typeof(*chmap->txtdlinePos))=%d\n", sizeof(typeof(*chmap->txtdlinePos)) );

	/* To allocate txtdlinePos */
	chmap->txtdlines=1024;	/* Initial value, Min>2, see mem_grow codes in FTcharmap_uft8strings_writeFB().*/
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

	/* Set default  color/alpha */
	chmap->bkgcolor=WEGI_COLOR_WHITE;
	chmap->fontcolor=WEGI_COLOR_BLACK;
	chmap->markcolor=WEGI_COLOR_YELLOW;
	chmap->markalpha=50;

	/* Assign other params */
	chmap->mapx0=x0;
	chmap->mapy0=y0;
	chmap->width=width;
	chmap->height=height;
	chmap->offx=offx;
	chmap->offy=offy;
	chmap->mapsize=mapsize;

	/* Set default draw_cursor function */
	chmap->draw_cursor=FTcharmap_draw_cursor;

	/* Set blink timer */
	if(gettimeofday(&chmap->tm_blink, NULL)!=0) {
		printf("%s: Fail to set chmap->tm_blink!\n",__func__);
		FTcharmap_free(&chmap);
		return NULL;
	}

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


/*--------------------------------------------------------------
Grow/realloc chmap->txtbuff more_size units.

Return:
	0	Ok
	<0	Fails.
---------------------------------------------------------------*/
inline int FTcharmap_memGrow_txtbuff(EGI_FTCHAR_MAP *chmap, size_t more_size)
{
	int off;

	if( chmap==NULL )
		return -1;

	/* Save current offset of pref */
	off=chmap->pref-chmap->txtbuff;

	/* Grow and init. to 0 in egi_mem_grow() */
        if( egi_mem_grow( (void **)&chmap->txtbuff,
                        sizeof(typeof(*chmap->txtbuff))*chmap->txtsize, sizeof(typeof(*chmap->txtbuff))*more_size ) !=0 )
		return -2;
        else {
        	chmap->txtsize += more_size; /* size in units */

		/*  --- Critical ---
		 * Since chmap->pref is a pointer to chmap->txtbuff, so as chmap->txtbuff changes, It MUST adjust accordingly!
		 */
		chmap->pref = chmap->txtbuff+off;
        }

	return 0;
}


/*--------------------------------------------------------
Grow/realloc charmap-txtdlinePos[] more_size units.

Return:
	0	Ok
	<0	Fails.
----------------------------------------------------------*/
inline int FTcharmap_memGrow_txtdlinePos(EGI_FTCHAR_MAP *chmap, size_t more_size)
{
	if( chmap==NULL )
		return -1;

	/* Grow chmap->txtdlinePos[], initial reset to 0 in egi_mem_grow() */
	if( egi_mem_grow( (void **)&chmap->txtdlinePos,
	       sizeof(typeof(*chmap->txtdlinePos))*chmap->txtdlines, sizeof(typeof(*chmap->txtdlinePos))*more_size ) !=0 )
		return -2;
	else
		chmap->txtdlines += TXTDLINES_GROW_SIZE;  /* size in units */

	return 0;
}

/*------------------------------------------------------------------
Grow/realloc charmap members: charX[], charY[], charPos[]
 more_size units.

Return:
	0	Ok
	<0	Fails.
------------------------------------------------------------------*/
inline int FTcharmap_memGrow_mapsize(EGI_FTCHAR_MAP *chmap, size_t more_size)
{

	if( chmap==NULL )
		return -1;

	/* Grow and init. to 0 in egi_mem_grow() */
        if( egi_mem_grow( (void **)&chmap->charX,
                        sizeof(typeof(*chmap->charX))*chmap->mapsize, sizeof(typeof(*chmap->charX))*more_size ) !=0 )
		return -2;
        if( egi_mem_grow( (void **)&chmap->charY,
                        sizeof(typeof(*chmap->charY))*chmap->mapsize, sizeof(typeof(*chmap->charY))*more_size ) !=0 )
		return -3;
        if( egi_mem_grow( (void **)&chmap->charPos,
                        sizeof(typeof(*chmap->charPos))*chmap->mapsize, sizeof(typeof(*chmap->charPos))*more_size ) !=0 )
		return -4;

	/* Adjust mapsize */
       	chmap->mapsize += more_size; /* size in units */

	return 0;
}



/*-------------------------------------------------------------------------------
Load content of a file into chmap->txtbuff, and original data in txtbuff will be
lost. if the file dose NOT exist, then the file will be created(for future save
operation).

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
        fd=open(fpath, O_CREAT|O_RDWR|O_CLOEXEC, S_IRWXU|S_IRWXG );
        if(fd<0) {
                printf("%s: Fail to open input file '%s': %s\n", __func__, fpath, strerror(errno));
                return -1;
        }
        /* Obtain file stat */
        if( fstat(fd,&sb)<0 ) {
                printf("%s: Fail call fstat for file '%s': %s\n", __func__, fpath, strerror(errno));
		if( close(fd) !=0 )
			printf("%s: Fail to close file '%s': %s!\n",__func__, fpath, strerror(errno));
                return -2;
        }
        fsize=sb.st_size;

        /* If not new file: mmap txt file */
	if(fsize >0) {
	        fp=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
        	if(fp==MAP_FAILED) {
                	printf("%s: Fail to mmap file '%s': %s\n", __func__, fpath, strerror(errno));
			if( close(fd) !=0 )
				printf("%s: Fail to close file '%s': %s!\n",__func__, fpath, strerror(errno));
			return -3;
		}
	}

	/* Free and realloc chmap->txtbuff */
	free(chmap->txtbuff); chmap->txtbuff=NULL;
        chmap->txtbuff=calloc(1,sizeof(typeof(*chmap->txtbuff))*(fsize+txtsize+1) );  /* +1 EOF */
        if( chmap->txtbuff == NULL) {
                printf("%s: Fail to calloc chmap txtbuff!\n",__func__);
		munmap(fp,fsize);
		close(fd);
		return -3;
        }
        chmap->txtsize=fsize+txtsize+1;     /* txtsize is appendable size */
        chmap->pref=chmap->txtbuff; 	    /* Init. pref pointing to txtbuff */

	/* If not new file: Copy file content to txtbfuff */
	if(fsize>0)
		memcpy(chmap->txtbuff, fp, fsize);

        /* Set txtlen */
        chmap->txtlen=strlen((char *)chmap->txtbuff);
        EGI_PDEBUG(DBG_CHARMAP,"Copy from '%s' chmap->txtlen=%d bytes, totally %d characters.\n", fpath, chmap->txtlen,
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
	if(chmap==NULL || chmap->txtsize<1 )
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
	/* Min LEN.  In case it's a new empty file. */
	if(chmap->txtlen==0) {
		chmap->txtbuff[0]='\n';
		chmap->txtlen=1;
	}

	#if 0   /* NOPE!!! This will increase chmap->txtlen, instead to put EOF after fwriting txtbuff !!! */
	if(chmap->txtbuff[chmap->txtlen-1]!='\n') {	/* txtbuff[txtlen] is ALWAYS '\0', as txtbuff EOF */
		chmap->txtbuff[chmap->txtlen]='\n';	/* UNIX File EOF standard */
		chmap->txtlen++;
	}
	#endif

	nwrite=fwrite(chmap->txtbuff, 1, chmap->txtlen, fil);
	if(nwrite < chmap->txtlen) {
		if(ferror(fil))
			printf("%s: Fail to write to '%s'.\n", __func__, fpath);
		else
			printf("%s: WARNING! fwrite %d bytes of total %d bytes.\n", __func__, nwrite, chmap->txtlen);
		ret=-4;
	}
	EGI_PDEBUG(DBG_CHARMAP,"%d bytes written.\n", nwrite);

	/* End_Of_File: UNIX/POSIX standard */
	if(chmap->txtbuff[chmap->txtlen-1]!='\n')	/* txtbuff[txtlen] is ALWAYS '\0', as txtbuff EOF */
		fwrite("\n", 1, 1, fil);		/* No error check here!!! */

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
	 Any ASCII codes that may be preseted in the txtbuff MUST all be charmapped into charX/Y/Pos[].
	 MUST NOT skip any char,event it has zero width, OR it will cause charmap data error in further operation,
	 Example: when shift chmap->pchoff, it MAY point to a char that has NO charmap data at all!
	 example: TAB(9) and CR(13)!
	 see 'EXCLUDE WARNING' in the function.
      4. To investigate:
	 If you try to insert a samll size char( like '.') in the beginning of a dline, and it happens
	 that there is just enough space left in previous dline, then after charmapping the char
	 will be displayed at the end of the last dline!  ---- Any further problems ???

@fbdev:         FB device
		If NULL: Ignore to redraw bkgcolor and curosr!
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
			will be ignored
                <0      Use symbol alpha data, or none.
                0       100% back ground color/transparent
                255     100% front color

@cnt:		Total recognizable UFT-8 characters written.
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
	int i,k;
	int size;
	int count;			/* number of character written to FB*/
	int px,py;			/* bitmap insertion origin(BBOX left top), relative to FB */
	const unsigned char *pstr=NULL;	/* == chmap->pref */
	const unsigned char *p=NULL;  	/* variable pointer, to tranverse each char in pstr[] */
        int xleft; 			/* available pixels remainded in current dline */
        int ln; 			/* dlines used */
	unsigned int lines;		/* Total dlines, =chmap->maplines */
	unsigned int pixpl;		/* pixels per dline, =chmap->mappixpl */
	unsigned off=0;
	int x0;				/* charmap left top point, exclude margin */
	int y0;
	int pchx=0;			/* if(chmap->fix_cursor), then save charXY[pch] */
	int pchy=0;
	int charlen;
	struct timeval tm_now;
 	wchar_t wcstr[1];
        FT_Fixed   advance; 		/* typedef signed long  FT_Fixed;   to store 16.16 fixed-point values */
	FT_Error   error;
	int sdw;			/* Self_defined width for some unicodes */

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

	/* if(chmap->fix_cursor), save charXY[pch] to  */
	if(chmap->fix_cursor) {
		pchx=chmap->charX[chmap->pch];
		pchy=chmap->charY[chmap->pch];
	}

	/* Init charmap canvas params */
	x0=chmap->mapx0+chmap->offx;
	y0=chmap->mapy0+chmap->offy;
	lines=chmap->maplines;
	pixpl=chmap->mappixpl;
	//gap=chmap->maplngap;

START_CHARMAP:	/* If follow_cursor, loopback here */

	/* Draw/clear blank paper/canvas, not only when follow_cursor  */
	if( chmap->bkgcolor >= 0 && fb_dev != NULL ) {
	       	fbset_color(chmap->bkgcolor);
       		draw_filled_rect(&gv_fb_dev, chmap->mapx0, chmap->mapy0,  chmap->mapx0+chmap->width, chmap->mapy0+chmap->height );
		#if 1 /* Draw grids */
	        fbset_color(WEGI_COLOR_GRAYB);
        	for(k=0; k<=chmap->maplines; k++)
                	draw_line(&gv_fb_dev, x0, y0+k*chmap->maplndis-1, x0+chmap->mappixpl-1, y0+k*chmap->maplndis-1);
		#endif
	}

	/* Init pstr and p */
	pstr=chmap->pref;  /* However, pref may be NULL */
	p=pstr;

	/* Init tmp. vars */
	px=x0;
	py=y0;
	xleft=pixpl;
	count=0;
	ln=0;		/* Line index from 0 */

	/* Check input pixpl and lines, */
	if(pixpl==0 || lines==0)
		return -4; //goto CHARMAP_END;

	/* Must reset temp charmap vars and clear old data */
	memset(chmap->maplinePos, 0, chmap->maplines*sizeof(typeof(*chmap->maplinePos)) );
	memset(chmap->charPos, 0, chmap->mapsize*sizeof(typeof(*chmap->charPos)));
	memset(chmap->charX, 0, chmap->mapsize*sizeof(typeof(*chmap->charX)) );
	memset(chmap->charY, 0, chmap->mapsize*sizeof(typeof(*chmap->charY)) );

	/* Reset/Init. charmap data, first maplinePos */
	chmap->chcount=0;
	chmap->maplncount=0;
	chmap->maplinePos[chmap->maplncount]=0; /* relative to chmap->pref */
	chmap->maplncount++;	/* MAPLNCOUNT_PRE_INCREMENT */

	/* Init.charmap global data */
	/* NOTE: !!! chmap->txtdlncount=xxx, MUST preset before calling this function */
	chmap->txtdlinePos[chmap->txtdlncount++]=chmap->pref-chmap->txtbuff;

	while( *p ) {

		/* Check txtdlncount and mem_grow txtdlinePos[] if necessary. */
		 if(chmap->txtdlncount >= chmap->txtdlines-2) {  /* ==chmap->txtdlines-1-1, -1 for allowance. */
			EGI_PDEBUG(DBG_CHARMAP,"txtdlncount=%d, txtdlines=%d, maplncount=%d, mem_grow chmap->txtdlinePos...\n",
										chmap->txtdlncount, chmap->txtdlines,chmap->maplncount);

			if( FTcharmap_memGrow_txtdlinePos(chmap, TXTDLINES_GROW_SIZE ) !=0 ) {
				chmap->errbits |= CHMAPERR_TXTDLINES_LIMIT;
				printf("%s: Fail to mem_grow chmap->txtdlinePos from %d to %d units more!\n",
											   __func__, chmap->txtdlines, TXTDLINES_GROW_SIZE);
	  	/*  <-------- Put mutex lock */
	  			pthread_mutex_unlock(&chmap->mutex);
				return -5;
			}
		}

		/* --- check whether lines are used up --- */
		if( lines < ln+1) {  /* ln index from 0, lines size_t */
			//printf("%s: ln=%d, Lines not enough! finish only %d chars.\n", __func__, ln, count);
			goto CHARMAP_END;
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
			printf("%s: WARNING! char_size < 0!\n",__func__);
			p++;
			continue;
		}

		/* CONTROL ASCII CODE:
		 * If return to next line
		 */
		if(*wcstr=='\n') {  /* ASCII Newline 10 */
			//printf(" ... ASCII code: Next Line ...\n ");

			/* Check Size */
			if ( chmap->chcount >= chmap->mapsize ) {
				EGI_PDEBUG(DBG_CHARMAP,"mem_grow chmap->mapsize charXYPOS from %d to %d units more...\n",
												chmap->mapsize, MAPSIZE_GROW_SIZE);
				if( FTcharmap_memGrow_mapsize(chmap, MAPSIZE_GROW_SIZE) !=0 ) {
					chmap->errbits |= CHMAPERR_MAPSIZE_LIMIT;
					printf("%s: Fail to mem_grow chmap->mapsize, charXYPOS from %d to %d units more!\n",
											   __func__, chmap->mapsize, MAPSIZE_GROW_SIZE);
				}
			}
			/* Fillin CHAR MAP before start a new line */
			else {  /* ( chmap->chcount < chmap->mapsize ) */
				chmap->charX[chmap->chcount]=x0+pixpl-xleft;  /* line end */
				chmap->charY[chmap->chcount]=py;
				chmap->charPos[chmap->chcount]=p-pstr-size;	/* deduce size, as p now points to end of '\n'  */
				chmap->chcount++;

				/* Get start postion of next displaying line.  */
				if(chmap->maplncount < chmap->maplines ) { /* LIMIT_MAPLNCOUNT */
					/* set maplinePos[] */
					chmap->maplinePos[chmap->maplncount++]=p-pstr;  /* keep p+size */
					/* set txtdlinePos[] */
					if(chmap->txtdlncount < chmap->txtdlines-1 )
						chmap->txtdlinePos[chmap->txtdlncount++]=p-(chmap->txtbuff); /* keep p+size */
				}
			}

			/* change to next line, +gap */
			ln++;
			xleft=pixpl;
			py += chmap->maplndis; //fh+gap;

			continue;
		}

		#if 0  /*  !!! EXCLUDE WARNING,  MUST NOT SKIP ANY UNPRITABLE CHARS HERE !!!
			*   It will cause charmap data error in further operation, Example: when shift chmap->pchoff, it MAY
			*   points to a char that has NO charmap data at all!!!
		 	*/
		/* If other control codes or DEL, skip it. To avoid print some strange icons. */
		else if( (*wcstr < 32 || *wcstr==127) && *wcstr!=9 && *wcstr!=13 ) {	/* EXCLUDE WARNING:  except TAB(9), '\r' */
			//printf("%s: ASCII control code: %d\n", __func__, *wcstr);     /* ! ASCII Return=CR=13 */
			continue;
		}
		#endif

		/* Reset pen X position */
		px=x0+pixpl-xleft;

		/* Write unicode bitmap to FB, and get xleft renewed. */

		/* If fb_dev==NULL: To get xleft in a fast way */
		if( fb_dev == NULL) {
		    	/* If has self_cooked width for some special unicodes, bitmap ignored. */
		    	sdw=FTsymbol_cooked_charWidth(wcstr[0], fw);
		    	if(sdw>=0)
				xleft -= sdw;
		    	/* No self_cooked width, With bitmap */
		    	else {
		        	/* set character size in pixels before calling FT_Get_Advances()! */
        			error = FT_Set_Pixel_Sizes(face, fw, fh);
			        if(error)
			                printf("%s: FT_Set_Pixel_Sizes() fails!\n",__func__);
				/* !!! WARNING !!! load_flags must be the same as in FT_Load_Char( face, wcode, flags ) when writeFB
	                         * the same ptr string.
				 * Strange!!! a little faster than FT_Get_Advance()
			 	 */
				error= FT_Get_Advances(face, *wcstr, 1, FT_LOAD_RENDER, &advance);
        	        	if(error)
                			printf("%s: Fail to call FT_Get_Advances().\n",__func__);
	                	else
        	                	xleft -= advance>>16;
		  	}
		}
		else /* fb_dev is NOT NULL */
		{	/* Use marskchar to replace/cover original chars, such as asterisk. */
			if( chmap->maskchar !=0 ) {
				FTsymbol_unicode_writeFB(fb_dev, face, fw, fh, chmap->maskchar, &xleft,
								 px, py, fontcolor, transpcolor, opaque );
			}
			else
				FTsymbol_unicode_writeFB(fb_dev, face, fw, fh, wcstr[0], &xleft,
								 px, py, fontcolor, transpcolor, opaque );
		}

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
				}
				else /* xleft==0 */ {
					/* set maplinePos[] */
					chmap->maplinePos[chmap->maplncount++]=p-pstr;  /* keep +size, xleft=0, as start of a new line */
                                        /* set txtdlinePos[] */
					if(chmap->txtdlncount < chmap->txtdlines-1)
                                        	chmap->txtdlinePos[chmap->txtdlncount++]=p-(chmap->txtbuff);
				}
			}

			/* Fail to  writeFB. reel back pointer p, only if xleft<0 */
			if(xleft<0) {
				p-=size;
				count--;
			}

		}
		/* Fillin char map */
		if(xleft>=0) {  /* xleft > = 0, writeFB ok */
			/* Check Size */
			if ( chmap->chcount >= chmap->mapsize ) {
				EGI_PDEBUG(DBG_CHARMAP,"mem_grow chmap->mapsize charXYPOS from %d to %d units more...\n",
												chmap->mapsize, MAPSIZE_GROW_SIZE);
				if( FTcharmap_memGrow_mapsize(chmap, MAPSIZE_GROW_SIZE) !=0 ) {
					chmap->errbits |= CHMAPERR_MAPSIZE_LIMIT;
					printf("%s: Fail to mem_grow chmap->mapsize, charXYPOS from %d to %d units more!\n",
											   __func__, chmap->mapsize, MAPSIZE_GROW_SIZE);
				}
			}
			else { /* ( chmap->chcount < chmap->mapsize) */
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

	/* LN_INCREMENT: If finishing writing whole strings, ln++ to get written lines, as ln index from 0 */
	if(*pstr)   /* To rule out NULL input */
		ln++;

CHARMAP_END:

	/* 1. Update output params */
	//printf("%s: %d characters written to FB.\n", __func__, count);
	if(cnt!=NULL)
		*cnt=count;
	if(lnleft != NULL)
		*lnleft=lines-ln; /* here ln is written lines, not index number */
	if(penx != NULL)
		*penx=x0+pixpl-xleft;
	if(peny != NULL)
		*peny=py;

	/* 2. Treat EOF */
	/* If all chars written to FB, then EOF is the last data in the charmap, also as the last insterting point
	 * We MUST also rule out the case that current writting line space used up.
	 * NOTE: You MUST NOT use (chmap->maplncount <= chmap->maplines), for chmap->maplncount is pre_incremented!
	 *       see lable MAPLNCOUNT_PRE_INCREMENT  LIMIT_MAPLNCOUNT
	 */
	if( (*p)=='\0' && lines >= ln )  /* NOW ln is NOT index !!! see label LN_INCREMENT */
	{
		//printf("%s: put EOF. plncount=%d, maplines=%d \n",__func__, chmap->maplncount, chmap->maplines);
		chmap->charX[chmap->chcount]=x0+pixpl-xleft;  /* line end */
		chmap->charY[chmap->chcount]=py;	      /* to avoid py out of range */
		chmap->charPos[chmap->chcount]=p-pstr;
		chmap->chcount++;
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

	/* 3. Reset chmap->txtdlncount as headline of current charmap */
	/* NOTE!!! Need to reset chmap->txtdlncount to let  chmape->txtdlinePos[txtdlncount] pointer to the first displaying line of current charmap
	 * Otherwise in a loop charmap wirteFB calling, it will increment itself forever...
	 * So let chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0] here;
	 * !!! However, txtdlinePos[txtdlncount+1],txtdlinePos[txtdlncount+2]... +maplncount-1] still hold valid data !!!
	 */
	chmap->txtdlncount -= chmap->maplncount;

	/* 4. Update chmap->pch and pch2 */
	/* First set to <0, to assume the cursor is NOT in current charmap. */
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
	//printf("%s: pch=%d, pch2=%d \n",__func__, chmap->pch, chmap->pch2);

	/* 5. If follow_cursor set: continue charmapping until pch>=0.*/
	if( chmap->follow_cursor && chmap->pch<0 ) {
		EGI_PDEBUG(DBG_CHARMAP,"Continue charmapping to follow cursor...\n");

		/* PRE_1: set txtdlncount: noticed that now chmap->txtdlinePos[txtdlncount]==chmap->maplinePos[0] */
		if( chmap->pchoff > off+chmap->charPos[chmap->chcount-1] ) {
			#if 1
			EGI_PDEBUG(DBG_CHARMAP,"Scroll down one dline to follow cursor...\n");
			chmap->txtdlncount++;	/* Scroll down one dline each time. */
			#else
			EGI_PDEBUG(DBG_CHARMAP,"Scroll down one page to follow cursor...\n");
			chmap->txtdlncount += chmap->maplncount-1; /* Scroll down one page eatch time. */
			/* Note: When inserting a large block, it's faster. but when we typing in at the end of current page,
			 * we just want to scroll one dline each time and keep our sight focus from jumping over too much.
			 */
			#endif
		}
		else if ( chmap->pchoff < off+chmap->charPos[0] ) {
			EGI_PDEBUG(DBG_CHARMAP,"Scroll up to follow cursor...\n");
			chmap->txtdlncount--;	/* Scroll up one dline each time.*/
		}
		else
			printf("%s: chmap->pch ERROR!\n", __func__);

		/* PRE_2: set chmap->pref */
		if( chmap->maplines >1 ) {
			chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];
		}
		/* If mapline==1 ONLY 1 dline, then scroll down to next dline */
		else { // ( chmap->maplines==1 )	/* If chmap->maplines==1, then txtdlinePos[+1] may has no data, its value is 0 */
                	charlen=cstr_charlen_uft8((const unsigned char *)(chmap->pref+chmap->charPos[chmap->chcount-1]));
                	if(charlen<0) {
                        	printf("%s: WARNING: cstr_charlen_uft8() negative! \n",__func__);
                        	charlen=0;
                	}
			chmap->pref=chmap->pref+chmap->charPos[chmap->chcount-1]+charlen;
		}

		/* Before repeating, render charmapped image */
		fb_render(fb_dev);

		goto  START_CHARMAP;
	}
	else  {  /* Reset follow_cursor */
		chmap->follow_cursor=false;
	}

	/* 6. After updating pchoff, IF chmap->fix_cursor set, reset pchoff. */
	if( chmap->fix_cursor==true ) {
		/* Relocate pch/pch2 as per pchx,pchy:  selection will be canclled!
		 * chmap->pch/pch2, pchoff/pchoff2 all updated!
		 */
		FTcharmap_locate_charPos_nolock(chmap, pchx, pchy);
		/* reset */
		chmap->fix_cursor=false;
	}

	/* 7. Draw selection Mark: Check pch and pch2, see if selection activated. */
	if( chmap->pchoff != chmap->pchoff2 )
		FTcharmap_mark_selection(fb_dev, chmap);

	/* 8. Draw blinking cursor */
	if( chmap->draw_cursor != NULL && fb_dev!=NULL )
	{
		/* Draw cursor first, so you may manually turn on cursor_on. */
		if( chmap->cursor_on && chmap->pch>=0 )
			chmap->draw_cursor(chmap->charX[chmap->pch],chmap->charY[chmap->pch],chmap->maplndis);

		/* Get time */
	        gettimeofday(&tm_now, NULL);  /* If it fails, just let it fail. */
       		if( tm_signed_diffms(chmap->tm_blink, tm_now) > CURSOR_BLINK_INTERVAL_MS ) {
			chmap->cursor_on=!chmap->cursor_on;
	       		chmap->tm_blink=tm_now;
		}
	}

	/* Clear chmap->request */
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

		/* 2.1 Mark start dline area */				/* chmap->withd != chmap->mappixpl+chmap->offx */
		draw_blend_filled_rect(fb_dev, startX, startY, chmap->mapx0+chmap->offx+chmap->mappixpl-1, startY+chmap->maplndis-1,
                                                                      chmap->markcolor, chmap->markalpha); /* dev, x1,y1,x2,y2,color,alpha */
		/* 2.2 Mark mid dlines areas */
		mdls=(endY-startY)/chmap->maplndis-1;
		if(mdls>0) {
			for( k=0; k<mdls; k++ ) {		/* dev, x1,y1,x2,y2,color,alpha */
			      draw_blend_filled_rect(fb_dev, chmap->mapx0+chmap->offx, startY+(k+1)*(chmap->maplndis), /* -1 to create a line */
						    chmap->mapx0+chmap->offx+chmap->mappixpl-1, startY+(k+2)*(chmap->maplndis) -1,  /* x2,y2 */
                                                                      chmap->markcolor, chmap->markalpha); 		    /* color,alpha */
			}
		}

		/* 2.3 Mark end dline area */
		draw_blend_filled_rect(fb_dev, chmap->mapx0+chmap->offx, endY, endX, endY+chmap->maplndis-1,
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

	if( chmap->txtdlncount != 0 ) {
		/* PRE_: Set chmap->txtdlncount */
		if( chmap->txtdlncount > chmap->maplines )
        		chmap->txtdlncount -= chmap->maplines;
	        else  { /* It reaches the first page */
        		chmap->txtdlncount=0;
		}

		/* PRE_: Set pref to previous txtdlines position */
	        chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];

        	EGI_PDEBUG(DBG_CHARMAP,"page up to chmap->txtdlncount=%d \n",chmap->txtdlncount);
	}
	else
        	EGI_PDEBUG(DBG_CHARMAP, "Reach the top page: chmap->maplncount=%d, chmap->maplines=%d, chmap->txtdlncount=%d, \n",
									chmap->maplncount, chmap->maplines, chmap->txtdlncount);

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

	/* 1. Current IS a full page/window AND not the EOF page */
 	if( chmap->maplncount == chmap->maplines  && chmap->pref[ chmap->charPos[chmap->chcount-1] ] != '\0' )
	{
		/* Get to the start point of the next page */
		pos=chmap->charPos[chmap->chcount-1]+cstr_charlen_uft8(chmap->pref+chmap->charPos[chmap->chcount-1]);

		/* reset pref and txtdlncount */
       		chmap->pref=chmap->pref+pos;
		chmap->txtdlncount += chmap->maplines;

       		EGI_PDEBUG(DBG_CHARMAP, "Page down to: chmap->txtdlncount=%d \n", chmap->txtdlncount);
        }
	else
        	EGI_PDEBUG(DBG_CHARMAP, "Reach the last page: chmap->chcount=%d, chmap->maplncount=%d, chmap->maplines=%d, chmap->txtdlncount=%d, \n",
								chmap->chcount, chmap->maplncount, chmap->maplines, chmap->txtdlncount);

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
             EGI_PDEBUG(DBG_CHARMAP,"chmap->txtdlncount=%d \n", chmap->txtdlncount);
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

	/* Only if first dline is NOT the last dline: more then 1 dline OR no EOF for the only dline case */
	if( chmap->maplncount >1 || ( chmap->maplines==1 && chmap->pref[chmap->chcount-1]!='\0' ) )
	{
		 /* Before/after calling FTcharmap_uft8string_writeFB(), champ->txtdlinePos[txtdlncount] always point to the first
        	  * dline: champ->txtdlinePos[txtdlncount]==chmap->maplinePos[0]
	          * So we need to reset txtdlncount to pointer the next line
        	  */
		/* PRE_1: Update txtdlncount */
		chmap->txtdlncount++;
	        if(chmap->maplines>1) {
			/* PRE_2. Update chmap->pref */
        		chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount]; /* !++ */
	        }
        	else {  /* If chmap->maplines==1, then txtdlinePos[+1] may has no data, its value is 0 */
			/* To the end of the dline's last char */
        		charlen=cstr_charlen_uft8((const unsigned char *)(chmap->pref+chmap->charPos[chmap->chcount-1]));
                	if(charlen<0) {
                		printf("%s: WARNING: cstr_charlen_uft8() negative! \n",__func__);
	                        charlen=0;
        	        }
			/* PRE_2. Update chmap->pref */
	                chmap->pref += chmap->charPos[chmap->chcount-1]+charlen;
        	}
	        EGI_PDEBUG(DBG_CHARMAP,"chmap->txtdlncount=%d \n", chmap->txtdlncount);
	}

	/* set chmap->request */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*-------------------------------------------------------------
Refer to FTcharmap_locate_charPos_nolock(). with mutex_lock.

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
	int ret=0;

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

	/* Call  _charPos_nolock() */
	ret=FTcharmap_locate_charPos_nolock(chmap,x,y);

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return ret;
}

/*--------------------------------------------------------------------------------
To locate chmap->pch/chmap->pch2) and pchoff/pchoff2  according
to given x,y,
chmap->pch2/pchoff2 is interlocked with chmap->pch/pchoff here!

Note:
1. chmap->pch/pch2, pchoff/pchoff2 all updated!
2. If (x,y) falls into remaing blank space of a dline: The check point is middle of the
   blank space,  If (x,y) is before the mid. point, the cursor goes to the last char
   of the dline, else it goes to the first char of the NEXT dline.
   See "3. The last char of the dline."

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------------------------*/
inline static int FTcharmap_locate_charPos_nolock( EGI_FTCHAR_MAP *chmap, int x, int y)
{
	int i;

	if( chmap==NULL ) {
		printf("%s: Input FTCHAR map is empty!\n", __func__);
		return -1;
	}

	/* 1. XY above charmap window */
	if( y < chmap->charY[0] )
		chmap->pch=0;
	/* 2. XY below charmap window */
	else if( y > chmap->charY[chmap->chcount-1]+chmap->maplndis-1 )
		chmap->pch=chmap->chcount-1;
	/* 3. XY in charmap windown */
	else {
	   /* Search char map to find pch/pos for the x,y  */
           for(i=0; i < chmap->chcount; i++) {
             	//printf("i=%d/(%d-1)\n", i, chmap->chcount);
		/* Pick the nearst charX[] to x */
		/* Locate Y, notice <= to left: A <= Y < B. */
             	if( chmap->charY[i] <= y && chmap->charY[i]+ chmap->maplndis > y ) {  /* in same dline */
			/* 0. If get to left margin */
			if( x < chmap->mapx0+chmap->offx ) {
				chmap->pch=i;
				break;
			}
			/* 1. Not found OR ==chcount-1),  set to the end of the chmap. */
			else if( i==chmap->chcount-1 ) {  // && chmap->charX[i] <= x ) {  /* Last char of the chmap */
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
				/* Check point: mid. of dline's remaing blank space:  its right (i+1), its left (i); */
				if( x-chmap->charX[i] > (chmap->mapx0+chmap->mappixpl-chmap->charX[i])/2 )
					chmap->pch=i+1;
				else
					chmap->pch=i;
				break;
			}

	     	}
	   } /* END for() */
	}

	/* interlock pch/pch2: MUST NOT CANCLE THIS, to cancel selection marks! */
	chmap->pch2=chmap->pch;

	/* set pchoff/pchoff2 accordingly */
	chmap->pchoff=chmap->pref-chmap->txtbuff+chmap->charPos[chmap->pch];
	chmap->pchoff2=chmap->pchoff;

	return 0;
}


/*---------------------------------------------------------------------
Refer to FTcharmap_locate_charPos( EGI_FTCHAR_MAP *chmap, int x, int y)
Without mutx_lock.

To locate chmap->pch2 according to given x,y.
Keep chmap->pch unchanged!

NOTE:
1. !!! WARN !!!
   It will fail when chmap->request is set!. So the caller
   MAY need to continue checking the result.

@chmap:		The FTCHAR map.
@x,y:		A B/LCD coordinate pointed to a char.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------*/
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

	/* !!! --- WARNING --- !!!  following operation for chmap->pch2 ONLY, NOT pch! */

	/* 1. XY above charmap window */
	if( y < chmap->charY[0] )
		chmap->pch2=0;    /* !!! pch2 !!! */
	/* 2. XY below charmap window */
	else if( y > chmap->charY[chmap->chcount-1]+chmap->maplndis-1 )
		chmap->pch2=chmap->chcount-1;   /* !!! pch2 !!! */
	/* 3. XY in charmap windown */
	else {
	   /* Search char map to find pch/pos for the x,y  */
           for(i=0; i < chmap->chcount; i++) {
             	//printf("i=%d/(%d-1)\n", i, chmap->chcount);
		/* Pick the nearst charX[] to x */
		/* Locate Y notice <= to left: A <= Y < B  */
             	if( chmap->charY[i] <= y && chmap->charY[i]+ chmap->maplndis > y ) {
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
				/* Check point: mid. of dline's remaing blank space:  its right (i+1), its left (i); */
				if( x-chmap->charX[i] > (chmap->mapx0+chmap->mappixpl-chmap->charX[i])/2 )
					chmap->pch2=i+1;
				else
					chmap->pch2=i;
				break;
			}
	     	}
	   } /* END for() */
	}

	/* set pchoff2 ONLY! */
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
		else if(chmap->txtdlncount==0)
			EGI_PDEBUG(DBG_CHARMAP,"Get to the TOP dline.\n");
	   }
	   /* 2.2 Else, move up to previous dline */
	   else {
		/* chmap->pch/pch2, pchoff/pchoff2 will all be updated by charPos_nolock() */
       		FTcharmap_locate_charPos_nolock( chmap, chmap->charX[chmap->pch],  chmap->charY[chmap->pch]-chmap->maplndis); /* exact Y */
	   }
	}

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

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
	    /*  2.1 If already gets to the last dline of current charmap page */
	    if( chmap->charY[chmap->pch] >= chmap->mapy0+chmap->maplndis*(chmap->maplines-1) ) {
	    	// NOPE, Not canvas bottom!! if( chmap->charY[chmap->pch] == chmap->charY[chmap->chcount-1] ) {
		EGI_PDEBUG(DBG_CHARMAP,"in the last dline of current charmap.\n");
		/* If NOT EOF dline*/
		if( chmap->pch!=chmap->chcount-1 || chmap->pref[chmap->charPos[chmap->chcount-1]] != '\0' ) {
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
		/* If EOF, then do nothing. */
		else
			EGI_PDEBUG(DBG_CHARMAP,"Get to EOF dline.\n");
	    }
            /* 2.2 Else move down to next dline */
            else {
		/* chmap->pch/pch2, pchoff/pchoff2 will all be updated by charPos_nolock() */
        	FTcharmap_locate_charPos_nolock(chmap, chmap->charX[chmap->pch], chmap->charY[chmap->pch]+chmap->maplndis); /* exact Y */
            }
	}

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

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
		printf("%s: pch<0 \n",__func__);
	        #if 0 /* Move pchoff/cursor forward */
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
	else if( chmap->pch >= 0 && (chmap->pch < chmap->chcount-1) ) {
		printf("%s: pch=%d, chcount=%d \n",__func__, chmap->pch, chmap->chcount);
		chmap->pch++;
		/* PRE_: Update pchoff/pchoff2 */
		if(chmap->pchoff==chmap->pchoff2)
			chmap->pchoff2=chmap->pchoff = off+chmap->charPos[chmap->pch];
		else	/* If in selection, then do NOT interlock with pchoff2 */
			chmap->pchoff = off+chmap->charPos[chmap->pch];
	}
	/* 3. If in the last of charmap. but NOT EOF */
	else if( chmap->pch == chmap->chcount-1 && chmap->pref[ chmap->charPos[chmap->pch] ] != '\0'  ) {
		printf("%s: in last dline, pch=%d\n",__func__, chmap->pch);
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
	/* 4. If EOF */
	else if( chmap->pref[ chmap->charPos[chmap->pch] ] == '\0' )
		EGI_PDEBUG(DBG_CHARMAP,"Get to the EOF.\n");

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

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
		#if 0 /* Move pchoff/cusor backward */
		/* Get to the very beginning */
		if(chmap->pchoff==0 )
			charlen=0;   /* skip no char */
		else {
			/*  Need to move pchoff backward 1 char */
			charlen=cstr_prevcharlen_uft8((const unsigned char *)(chmap->txtbuff+chmap->pchoff));
			if(charlen<0) {
				printf("%s: WARNING: cstr_prevcharlen_uft8() negative! \n",__func__);
			}
		}

		/* PRE_: Update pchoff/pchoff2  */
		if(chmap->pchoff==chmap->pchoff2) { /* If no selection, interlock with pchoff2 */
			chmap->pchoff -= charlen;
			chmap->pchoff2=chmap->pchoff;
		}
		else 				/* If pchoff/pchoff2 in selection, then do NOT interlock */
			chmap->pchoff -= charlen;
		#endif

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
	/* 4. If head of txtbuff */
	else if( chmap->pch == 0 && chmap->txtdlncount==0 )
		EGI_PDEBUG(DBG_CHARMAP,"Get to the HEAD of txtbuff.\n");

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

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

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

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

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

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
in case those txtlinePos[] have NOT been charmapped or updated after
some editing, though it's rarely to happen.
Avoid to call this function in such case. example: when inserting
words at the end of current charmap, and go to scroll down immediatley
withou updating charmap at first.


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
	if( pchoff<0 || pchoff > chmap->txtlen ) {   /*  pchoff==chmap->txtlen is EOF */
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
	#if 0  ////// OBSOLETE ////////
  	for(i=1; i < chmap->txtdlines; i++) {  /* WARNING: may be un_charmapped or old data */
  		if( chmap->txtdlinePos[i] == 0 ) /* i>0;  0 as un_charmapped data */
 			break;
		else if( pchoff < chmap->txtdlinePos[i] )
			return	i-1;
		else if( pchoff >= chmap->txtdlinePos[i] && )
				return i;  /* The last txtdline, EOF */
	}
	#endif ////////////////////
	/* 1. Check txtdline 0 -> txtdlines-2 */
  	for(i=0; i < chmap->txtdlines-1; i++) {  /* !!! WARNING !!!: txtdlinPos[] may hold un_charmapped or old data */
		if( pchoff >= chmap->txtdlinePos[i]   /* txtdlinePos[0] === 0, as we always start with chmap->pref = chmap->txtbuff  */
		    && ( pchoff < chmap->txtdlinePos[i+1] || chmap->txtdlinePos[i+1]==0) )   /* || Or end dline */
			return	i;

		/* End data, no data anymore... */
		if( chmap->txtdlinePos[i+1] ==0 )
			return -2;
	}
	/* 2. If txtdlines all filled data, check the last txtdlinePos[] */
	if( pchoff >= chmap->txtdlinePos[chmap->txtdlines-1] ) // RULE OUT && pchoff <= chmap->txtlen, see above )  /*  pchoff==chmap->txtlen is EOF */
		return chmap->txtdlines-1;

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

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*----------------------------------------------------------------------------
Insert a string into chmap->txtbuff+pchoff. without mutex_lock and request!

Note:
1. If the inserting string is very big, then it may take a while charmapping
   and scrolling to follow cursor to updated pchoff.

@chmap:         Pointer to the EGI_FTCHAR_MAP.
@pstr:		Pointer to inserting string.
@size:		Size of the inserting string.

Return:
        0       OK,    chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
--------------------------------------------------------------------------*/
inline int FTcharmap_insert_string_nolock( EGI_FTCHAR_MAP *chmap, const unsigned char *pstr, size_t strsize )
{
	int dln;
	int startPos, endPos;

	/* Check input */
        if( chmap==NULL || pstr==NULL ) {
                return -1;
        }

	/* 1. Check txtbuff space, and auto mem_grow if necessary. */
	if( chmap->txtlen +strsize > chmap->txtsize-1 ) {  /* ?-1 */

                EGI_PDEBUG(DBG_CHARMAP,"chmap->txtbuff is full! txtlen=%d + strsize=%d > txtsize-1=%d, mem_grow chmap->txtbuff...\n",
                                                                                	chmap->txtlen, strsize,chmap->txtsize-1);
		if( FTcharmap_memGrow_txtbuff(chmap, TXTBUFF_GROW_SIZE+strsize) !=0 )
		{
			chmap->errbits |= CHMAPERR_TXTSIZE_LIMIT;
                	printf("%s: Fail to mem_grow chmap->txtbuff from %d to %d units more!\n",
									__func__, chmap->txtsize, TXTBUFF_GROW_SIZE+strsize); /* !!! */
			return -2;
		}
                EGI_PDEBUG(DBG_CHARMAP,"OK, mem_grow champ->txtbuff to %d units.\n", chmap->txtsize);
	}

	/* If head of selection (pchoff or pchoff2) Not in current charmap page, set to scroll to ? Just refert to FTcharmap_insert_char() */

    	/* 2.0 If selection marks, delete all selected chars first and reset pchoff/pchoff2 */
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
		/* Update txtlen */
		chmap->txtlen -= (endPos-startPos);
		/* Set pchoff2=pchoff=startPos */
		chmap->pchoff=startPos;
		chmap->pchoff2=startPos;

		/* We MUST reset chmap->txtdlncount here, in case current chmap->txtdlncount will be INVALID after deletion,
		 * as previous txtdlinePos[txtdlncount] pointed data may be also deleted!
		 */
		if(startPos==0) {
			/* PRE_: */
			chmap->txtdlncount=0;
			chmap->pref=0;
		}
		else {
			dln=FTcharmap_get_txtdlIndex(chmap, startPos-1);
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
	/* ELSE pchoff==pchoff2 */

	/*  Following: chmap->pch/pch2 is invalid, as it has NOT been charmapped/updated */

	/* 2.1  Insert char at EOF of chmap->txtbuff, OR start of an empty txtbuff */
	if(chmap->txtbuff[chmap->pchoff]=='\0') {  /* Need to reset charPos[] at charmapping */
		printf("%s: Insert string at EOF.\n", __func__);

		/* 1 Insert char to the EOF, pchoff updated. */
		strncpy((char *)chmap->txtbuff+chmap->pchoff, (char *)pstr, strsize);

		/* 2 --- Always reset txtbuff EOF here! ----
		 * NOTE: DEL/BACKSPACE operation acturally copys string including only ONE '\0' at end, NOT just move and completely clear !
		 * So after deleting/moving a more_than_1_byte_width uft-8 char, there are still several bytes need to be cleared.
		 * For the case, we insert the char just at the fore-mentioned position of '\0', and the next byte may be NOT '\0'!
		 */
		chmap->txtbuff[chmap->pchoff+strsize]='\0';

		/* 3 Update chmap->txtlen: size check at beginning. */
		chmap->txtlen +=strsize;
	}
	/* 2.2 Insert char not at EOF */
	else {
		printf("%s: Insert string at chmap->pch=%d pchoff=%d\n", __func__,chmap->pch, chmap->pchoff);

		/* 1 Move string forward to leave space for the inserting char */
		memmove(chmap->txtbuff+chmap->pchoff+strsize, chmap->txtbuff+chmap->pchoff,
							strlen((char *)(chmap->txtbuff+chmap->pchoff)) +1 ); /* +1 EOF */

		/* 2 Insert the string. */
		strncpy((char *)chmap->txtbuff+chmap->pchoff, (char *)pstr, strsize);

		/* 3 Update chmap->txtlen: size check at beginning. */
		chmap->txtlen +=strsize;
	}

	/* Adjust pchoff/pchoff2 */
     	chmap->pchoff += strsize; 	       /* : NOW typing/inserting cursor position relative to txtbuff */
     	chmap->pchoff2 = chmap->pchoff;  /* interlocked with pchoff */

	/* If pchoff out of current charmap, or newline '\n',  scroll down one dline then....
	 * if chmap->maplines==1!, then txtdlinePos[+1] MAY have NO data!
	 * Set follow_cursor to force to scroll in charmapping until cursor/pchoff get into current charmap.
	 */
	chmap->follow_cursor=true;

	return 0;
}


/*------------------------------------------------------------------------
Insert a string into chmap->txtbuff+pchoff with mutex_lock and request.

@chmap:         Pointer to the EGI_FTCHAR_MAP.
@pstr:		Pointer to inserting string.
@size:		Size of the inserting string.

Return:
        0       OK,    chmap->pch modifed accordingly.
        <0      Fail   chmap->pch, unchanged.
------------------------------------------------------------------------*/
int FTcharmap_insert_string( EGI_FTCHAR_MAP *chmap, const unsigned char *pstr, size_t strsize )
{
	/* Check input */
        if( chmap==NULL || pstr==NULL ) {
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
		printf("%s: chmap->request !=0 \n",__func__);
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* Inster string to chmap->txtbuff, follow_cursor is set in the function. */
	if( FTcharmap_insert_string_nolock( chmap, pstr, strsize ) != 0 ) {
		printf("%s: Fail to call FTcharmap_insert_string_nolock()!\n",__func__);
	       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -3;
	}

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}



/*---------------------------------------------------------------
Insert a character into charmap->txtbuff.

1. If cursor OR head of selection NOT in current charmap,
   Set to scroll to dline where it located.
2. If selection marked, delete all selected chars first.
3. Then insert a char(ASCII or UFT-8 encoding) into the chmap->txtbuff.

			!!! --- WARNING --- !!!

All chars MUST be printable, OR '\n', OR TAB, OR legal UFT-8 encoded.
It MAY crash chmapping if any illegal char exists in champ->txtbuff.??

@chmap:         Pointer to the EGI_FTCHAR_MAP.
@ch:		Pointer to inserting char.

Return:
        0       OK
        <0      Fail
------------------------------------------------------------------*/
int FTcharmap_insert_char( EGI_FTCHAR_MAP *chmap, const char *ch )
{
	int chsize;  /* Size of inserted char */
//	int off;
	int dln;
	int startPos, endPos;
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
//	off=chmap->pref-chmap->txtbuff;

	/* 0. Get totabl number of inserting char */
	//chns=1;			/* 1 char only NOW */

	/* 1. Check txtbuff space, and auto mem_grow if necessary. */
	if( chmap->txtlen +chsize > chmap->txtsize-1 -1) {

		//chmap->errbits |= CHMAPERR_TXTSIZE_LIMIT;
                EGI_PDEBUG(DBG_CHARMAP,"chmap->txtbuff is full! txtlen=%d + chsize=%d > txtsize-1=%d, mem_grow chmap->txtbuff...\n",
                                                                                	chmap->txtlen, chsize,chmap->txtsize-1);
		if( FTcharmap_memGrow_txtbuff(chmap, TXTBUFF_GROW_SIZE) !=0 )  /* If string, GROW_SIZE+strlen */
		{
			chmap->errbits |= CHMAPERR_TXTSIZE_LIMIT;
                	printf("%s: Fail to mem_grow chmap->txtbuff from %d to %d units more!\n",
							__func__, chmap->txtsize, TXTBUFF_GROW_SIZE); /* If string, GROW_SIZE+strlen */

       		/*  <-------- Put mutex lock */
		        	pthread_mutex_unlock(&chmap->mutex);
				return -4;
		}
                EGI_PDEBUG(DBG_CHARMAP,"OK, mem_grow champ->txtbuff to %d units.\n", chmap->txtsize);
	}
	//printf("current chmap->pch=%d\n", chmap->pch);

	/* 1. If head of selection (pchoff or pchoff2) Not in current charmap page, set to scroll to */
	/* Get head of selection, consider pchoff==pchoff2 also as a selection. */
	if( chmap->pch < 0 && chmap->pchoff <= chmap->pchoff2 )
	{
		printf("%s: pch not in current charmap!\n", __func__);
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
	else if( chmap->pch2 < 0 && chmap->pchoff2 < chmap->pchoff )
	{
		printf("%s: pch2 not in current charmap!\n", __func__);
		/* Get txtdlncount corresponding to pchoff  */
		dln=FTcharmap_get_txtdlIndex(chmap, chmap->pchoff2);
		if(dln<0) {
			printf("%s: Fail to find index of chmap->txtdlinePos[] for pchoff2!\n", __func__);
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

        /* 2. If head of selection in current charmap */
      // else if( chmap->pch >= 0 && chmap->pch < chmap->chcount )
	else if( chmap->pch >=0 || chmap->pch2 >=0 )
        {
	    /* 2.0 If selection marsk, delete all selected chars first */
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
		/* Update txtlen */
		chmap->txtlen -= (endPos-startPos);
		/* Set pchoff2=pchoff=startPos */
		chmap->pchoff=startPos;
		chmap->pchoff2=startPos;
	    }

	   /* 2.1  Insert char at EOF of chmap->txtbuff, OR start of an empty txtbuff */
	   //if(chmap->pref[ chmap->charPos[ chmap->pch] ]=='\0') {  /* Need to reset charPos[] at charmapping */
	   if(chmap->txtbuff[chmap->pchoff]=='\0') {  /* Need to reset charPos[] at charmapping */
		printf("%s: Insert a char at EOF.\n", __func__);

		/* 1 Insert char to the EOF, notice that charPos[] have not been updated yet, it holds just insterting pos. */
		strncpy((char *)chmap->txtbuff+chmap->pchoff, ch, chsize);

		/* 2 --- Always reset txtbuff EOF here! ----
		 * NOTE: DEL/BACKSPACE operation acturally copys string including only ONE '\0' at end, NOT just move and completely clear !
		 * So after deleting/moving a more_than_1_byte_width uft-8 char, there are still several bytes need to be cleared.
		 * For the case, we insert the char just at the fore-mentioned position of '\0', and the next byte may be NOT '\0'!
		 */
		chmap->txtbuff[chmap->pchoff+chsize]='\0';

		/* 3 Update chmap->txtlen: size check at beginning. */
		chmap->txtlen +=chsize;
		/* !!!Warning: now chmap->pch MAY out of displaying txtbox range! charXY is undefined as initial value (0,0)!
		 * champ->pchoff/pchoff2 to be modified accordingly for pch/pch2, see below.
		 */
	   }
	   /* 2.2 Insert char not at EOF */
	   else {
		//printf("%s: Insert at chmap->pch=%d of chcount=%d\n", __func__,chmap->pch, chmap->chcount);

		/* 1 Move string forward to leave space for the inserting char */
		memmove(chmap->txtbuff+chmap->pchoff+chsize, chmap->txtbuff+chmap->pchoff,
							strlen((char *)(chmap->txtbuff+chmap->pchoff)) +1);

		/* 2 Insert the char, notice that charPos[] have not been updated yet, it holds just  insterting pos. */
		strncpy((char *)chmap->txtbuff+chmap->pchoff, ch, chsize);

		/* 3 Update chmap->txtlen: size check at beginning. */
		chmap->txtlen +=chsize;
	     }

	     /* In case that inserting a new char just at the end of the charmap, and AFTER charmapping: chmap->pch > chmap->chcount-1
	      *  (here chmap->pch is preset as above ), then we need to scroll one line down to update the cursor.
	      *  we'll need pchoff to locate the typing position in charmapping function.
	      */
	     chmap->pchoff += chsize; 	       /* : NOW typing/inserting cursor position relative to txtbuff */
	     chmap->pchoff2 = chmap->pchoff;  /* interlocked with pchoff */
	     //printf("%s: pchoff2=phoff=%d \n",__func__, chmap->pchoff);

	     /* If pchoff out of current charmap, or newline '\n',  scroll down one dline then....
	      * if chmap->maplines==1!, then txtdlinePos[+1] MAY have NO data!
	      * Set follow_cursor to force to scroll in charmapping until cursor/pchoff get into current charmap.
	      */
	     chmap->follow_cursor=true;
	}

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*----------------------------------------------------------------
Without mutex_lock and request!
If ( pchoff==pchoff2) delete a char preceded by cursor.
If ( pchoff!=pchoff1) delete all chars between pchoff and pchoff2

Note:
1. No matter where is the editing cursor(pchoff), it will execute deletion anyway,
   even the cursor(pchoff) is NOT in the current charmap.
2. If NOT in current charmap, reset chmap->txtdlncount,pref after deleting.
   Point chmap->pref to the dline where chmap->pchoff located.
3. Otherwise deletes the char pointed by pch, and chmap->pch/pchoff keeps unchanged!
4. The EOF will NOT be deleted!

@chmap:         Pointer to the EGI_FTCHAR_MAP.

Return:
        0       OK
        <0      Fail
----------------------------------------------------------------*/
inline int FTcharmap_delete_string_nolock( EGI_FTCHAR_MAP *chmap )
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

	#if 0 /* -----TEST:  */
	wchar_t wcstr;
	if(char_uft8_to_unicode(chmap->pref+chmap->charPos[chmap->pch], &wcstr) >0) {
		printf("Del wchar = %d\n", wcstr);
	}
	#endif

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
			#if 0 /* TEST:. ...*/
			if(chmap->txtlen != strlen((char *)chmap->txtbuff) )
				printf("%s: ERROR txtlen=%d, pchoff=%d \n", __func__, chmap->txtlen, chmap->pchoff);
			#endif

			/* 2. PRE_: Set pchoff2=pchoff=startPos */
			chmap->pchoff2=startPos;
			chmap->pchoff=startPos;

			/* If startPos NOT in current charmap */
			off=chmap->pref-chmap->txtbuff;
			if(  startPos < off+chmap->charPos[0] || startPos > off+chmap->charPos[chmap->chcount-1] ) {
				/* 3. Reset chmap->pref for re_charmapping */
				dlindex=FTcharmap_get_txtdlIndex(chmap,  chmap->pchoff);
				if(dlindex<0) {
					printf("%s: pchoff=%d, dlindex is nagtive, FTcharmap_get_txtdlIndex() fails!\n",__func__,chmap->pchoff);
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

	return 0;
}


/*----------------------------------------------------------------------------
If ( pchoff==pchoff2) delete a char preceded by cursor.
If ( pchoff!=pchoff1) delete all chars between pchoff and pchoff2

Note:
1. No matter where is the editing cursor(pchoff), it will execute deletion anyway,
   even the cursor(pchoff) is NOT in the current charmap.
2. If NOT in current charmap, reset chmap->txtdlncount,pref after deleting.
   Point chmap->pref to the dline where chmap->pchoff located.
3. Otherwise deletes the char pointed by pch, and chmap->pch/pchoff keeps unchanged!
4. The EOF will NOT be deleted!

@chmap:         Pointer to the EGI_FTCHAR_MAP.

Return:
        0       OK
        <0      Fail
---------------------------------------------------------------------------*/
int FTcharmap_delete_string( EGI_FTCHAR_MAP *chmap )
{
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

	if( FTcharmap_delete_string_nolock(chmap) != 0 ) {
		printf("%s: Fail to call FTcharmap_delete_string_nolock()!\n",__func__);
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -4;
	}

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


/*----------------------------------------------------
Copy content in EGI SYSPAD to chmap->txtbuff.

Return:
	0	OK
	<0	Fail
-----------------------------------------------------*/
int FTcharmap_copy_from_syspad( EGI_FTCHAR_MAP *chmap )
{
	int ret=0;

	if( chmap==NULL || chmap->txtbuff==NULL )
		return -1;

	/* Get buffer from EGI SYSPAD */
	EGI_SYSPAD_BUFF *padbuff=egi_buffer_from_syspad();
	if(padbuff==NULL)
		return -2;

	/* Insert to charmap, with mutex_lock */
	printf("%s: insert string: '%s'\n",__func__, (char *)padbuff->data);
	ret=FTcharmap_insert_string( chmap, padbuff->data, padbuff->size );

	/* Free pad buffer */
	egi_sysPadBuff_free(&padbuff);

	return ret;
}


/*----------------------------------------------------
Copy selected content in charmap to EGI SYSPAD PATH.

No mutex_lock applied!

Return:
	0	OK
	<0	Fail
-----------------------------------------------------*/
int FTcharmap_copy_to_syspad( EGI_FTCHAR_MAP *chmap )
{
	int startPos, endPos;

	if( chmap==NULL || chmap->txtbuff==NULL )
		return -1;

	/* If NO selection */
	if( chmap->pchoff == chmap->pchoff2 )
		return -2;

	/* Get selection */
	if(chmap->pchoff > chmap->pchoff2) {
		startPos=chmap->pchoff2;
		endPos=chmap->pchoff;
	}
	else {
		startPos=chmap->pchoff;
		endPos=chmap->pchoff2;
	}

	/* Save to EGI_SYSPAD file */
	if( egi_copy_to_syspad( chmap->txtbuff+startPos, endPos-startPos) != 0 )
		return -3;

	return 0;
}


/*--------------------------------------------------
No mutex_lock and request!

Delete selected content in charmap and put it to
EGI SYSPAD PATH.

Return:
	0	OK
	<0	Fail
--------------------------------------------------*/
int FTcharmap_cut_to_syspad( EGI_FTCHAR_MAP *chmap )
{
	int startPos, endPos;

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

	/* If NO selection */
	if( chmap->pchoff == chmap->pchoff2 )
		return -4;

	/* Get selection */
	if(chmap->pchoff > chmap->pchoff2) {
		startPos=chmap->pchoff2;
		endPos=chmap->pchoff;
	}
	else {
		startPos=chmap->pchoff;
		endPos=chmap->pchoff2;
	}

	/* Save selected content to EGI_SYSPAD file */
	if( egi_copy_to_syspad( chmap->txtbuff+startPos, endPos-startPos) != 0 )
		return -5;

	if( FTcharmap_delete_string_nolock(chmap) != 0 ) {
		printf("%s: Fail to call FTcharmap_delete_string_nolock()!\n",__func__);
       	/*  <-------- Put mutex lock */
	        pthread_mutex_unlock(&chmap->mutex);
		return -6;
	}

	/* Keep cursor on */
	gettimeofday(&chmap->tm_blink,NULL);
	chmap->cursor_on=true;

	/* Set chmap->request for charmapping */
	chmap->request=1;

        /*  <-------- Put mutex lock */
        pthread_mutex_unlock(&chmap->mutex);

	return 0;
}


