/*------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of a simple editor, read input from termios and put it on to
the screen.


		--- Keys and functions ---

Mouse move: 	Move typing cursor to LeftKeyDown to confimr it.
Mouse scroll: 	Scroll charmap up and down.
ARROW_KEYs: 	Shift typing cursor to locate insert/delete position.
BACKSPACE: 	Delete chars before the cursor.
DEL:		Delete chars following the cursor.
HOME(or Fn+LEFT_ARROW!):	Return cursor to the begin of the retline.
END(or Fn+RIGHT_ARROW!):	Move cursor to the end of the retline.
CTRL+N:		Scroll down 1 line, keep typing cursor position.
CTRL+O:		Scroll up 1 line, keep typing curso position.
PG_UP:		Page up
PG_DN:		Page down


               --- Definition and glossary ---

1. char:    A printable ASCII code OR a local character with UFT-8 encoding.
2. dline:  displayed/charmapped line, A line starts/ends at displaying window left/right end side.
   retline: A line starts/ends by a new line token '\n'.
3. scroll up/down:  scroll up/down charmap by mouse, keep cursor position relative to txtbuff.
	  (UP: decrease chmap->pref (-txtbuff), 	  DOWN: increase chmap->pref (-txtbuff) )

   shift  up/down:  shift typing cursor up/down by ARROW keys.
	  (UP: decrease chmap->pch, 	  DOWN: increase chmap->pch )


TODO:
1. If any control code other than '\n' put in txtbuff[], strange things
   may happen...  \034	due to UFT-8 encoding.   ---Avoid it.
2. Check whether txtbuff[] overflows.  ---OK
3. English words combination.
4. Mutex lock for chmap data. Race condition between FTcharmap_writeFB and
   FTcharmap_locate_charPos(). OR put mouse actions in editing loop. ---OK
5. Remove (lines) in charmap_writeFB(), use chmap->maxlines instead. ---ok
6. The typing cursor can NOT escape out of the displaying window. it always
   remains and blink in visible area.


Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>   /* open */
#include <unistd.h>  /* read */
#include <errno.h>
#include <ctype.h>
#include <linux/input.h>
#include <termios.h>
#include "egi_common.h"
#include "egi_input.h"
#include "egi_FTcharmap.h"
#include "egi_FTsymbol.h"
#include "egi_cstring.h"


#define 	BLINK_INTERVAL_MS	500	/* typing_cursor blink interval in ms */
#define 	CHMAP_TXTBUFF_SIZE	256     /* text buffer size for EGI_FTCHAR_MAP.txtbuff */
#define		CHMAP_SIZE		256 //125	/* NOT less than total number of chars (include '\n's and EOF) displayed in the txtbox */

/* TTY input escape sequences.  USB Keyboard hidraw data is another story...  */
#define		TTY_ESC		"\033"
#define		TTY_UP		"\033[A"
#define		TTY_DOWN	"\033[B"
#define		TTY_RIGHT	"\033[C"
#define		TTY_LEFT	"\033[D"
#define		TTY_HOME	"\033[1~"
#define		TTY_HOME_1	"\033[H"
#define		TTY_INSERT	"\033[2~"
#define		TTY_DEL		"\033[3~"
#define		TTY_END		"\033[4~"
#define		TTY_END_1	"\033[F"
#define		TTY_PAGE_UP	"\033[5~"
#define		TTY_PAGE_DOWN	"\033[6~"


#define 	TEST_INSERT 	1

/* Some ASCII control key */
#define		CTRL_N	14	/* ASCII: Ctrl + N, scroll down  */
#define		CTRL_O	15	/* ASCII: Ctrl + O, scroll up  */

char *strInsert="Widora和小伙伴们";

//static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 30+5+20+5} };	/* Onle line displaying area */
//static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 120-1-10} };	/* Text displaying area */
static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 240-1-10} };	/* Text displaying area */
static int smargin=5; 		/* left and right side margin of text area */
static int tmargin=0;		/* top margin of text area */

/* NOTE: char position, in respect of txtbuff index and data_offset: pos=chmap->charPos[pch] */
static EGI_FTCHAR_MAP *chmap;	/* FTchar map to hold char position data */
static unsigned int pchoff;
static unsigned int chsize;	/* byte size of a char */
static unsigned int chns;	/* total number of chars in a string */

static bool mouseLeftKeyDown;
static int  mouseX, mouseY, mouseZ; /* Mouse point position */
static int  mouseMidX, mouseMidY;   /* Mouse cursor mid position */

static int fw=18;	/* Font size */
static int fh=20;
static int fgap=20/4; //5;	/* Text line fgap : TRICKY ^_- */
static int tlns;  //=(txtbox.endxy.y-txtbox.startxy.y)/(fh+fgap); /* Total available line space for displaying txt */

static int penx;	/* Pen position */
static int peny;

static int FTcharmap_writeFB(FBDEV *fbdev, EGI_16BIT_COLOR color, int *penx, int *peny);
static void FTsymbol_writeFB(char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny);
static void mouse_callback(unsigned char *mouse_data, int size);
static void draw_mcursor(int x, int y);


int main(void)
{

 /* <<<<<<  EGI general init  >>>>>> */

  /* Start sys tick */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();

  #if 0
  /* Start egi log */
  printf("egi_init_log()...\n");
  if(egi_init_log("/mmc/log_scrollinput")!=0) {
        printf("Fail to init egi logger, quit.\n");
        return -1;
  }

  /* Load symbol pages */
  printf("FTsymbol_load_allpages()...\n");
  if(FTsymbol_load_allpages() !=0) /* FT derived sympg_ascii */
        printf("Fail to load FTsym pages! go on anyway...\n");
  printf("symbol_load_allpages()...\n");
  if(symbol_load_allpages() !=0) {
        printf("Fail to load sym pages, quit.\n");
        return -1;
  }
  #endif

  /* Load freetype fonts */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
        printf("Fail to load FT sysfonts, quit.\n");
        return -1;
  }
  FTsymbol_set_SpaceWidth(1);
  FTsymbol_set_TabWidth(2);

  #if 0
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
        printf("Fail to load FT appfonts, quit.\n");
        return -1;
  }

  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
        return -1;
  #endif

  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;

  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,false);
  fb_position_rotate(&gv_fb_dev,0);

  /* Set mouse callback function and start mouse readloop */
  egi_mouse_setCallback(mouse_callback);
  egi_start_mouseread("/dev/input/mice");

 /* <<<<<  End of EGI general init  >>>>>> */



                  /*-----------------------------------
                   *            Main Program
                   -----------------------------------*/

        struct termios old_settings;
        struct termios new_settings;

	int term_fd;
	char ch;
	char chs[4];
	int  k;
	int  nch;
	fd_set rfds;
	int retval;
	struct timeval tmval;
	bool	cursor_blink=false;
	struct timeval tm_blink;
	struct timeval tm_now;

	/* Total available lines of space for displaying chars */
	tlns=(txtbox.endxy.y-txtbox.startxy.y+1)/(fh+fgap);

	/* Init. txt and charmap */
	chmap=FTcharmap_create(CHMAP_TXTBUFF_SIZE, txtbox.startxy.x+smargin, txtbox.startxy.y,		/* buffsize, x0,y0, */
							CHMAP_SIZE, tlns, 320-20-2*smargin, fh+fgap); 	/* mapsize, lines, pixpl, lndis */
	if(chmap==NULL){ printf("Fail to create char map!\n"); exit(0); };

	//strcat(txtbuff,"12345\n歪朵拉的惊奇\nwidora_NEO\n\n"); //   --- hell world ---\n\n");
	//strcat(chmap,"1122\n你既有此心，待我到了东土大唐国寻一个取经的人来，教他救你。你可跟他做个徒弟，秉教伽持，入我佛门。\n"); //   --- hell world ---\n\n");
	strncat((char *)chmap->txtbuff,"1\n2\n3\n4\n5\n你既有此心，\n待我到了东土大唐国\n寻一个取经的人来，\n教他救你。\n\
你可跟他做个徒弟，\n秉教伽持，\n入我佛门。\n -END-", CHMAP_TXTBUFF_SIZE); //   --- hell world ---\n\n");
	//strncat(chmap->txtbuff,"1122\n你既有此心，待我到了东土大唐国寻一个取经的人来，教他救你。", TXTBUFF_SIZE); //   --- hell world ---\n\n");
	chmap->txtlen=strlen((char *)chmap->txtbuff);
	printf("chmap->txtlen=%d\n",chmap->txtlen);

        /* Init. mouse position */
        mouseX=gv_fb_dev.pos_xres/2;
        mouseY=gv_fb_dev.pos_yres/2;

        /* Init. FB working buffer */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY4);
	fbset_color(WEGI_COLOR_WHITE);
	draw_filled_rect(&gv_fb_dev, txtbox.startxy.x, txtbox.startxy.y, txtbox.endxy.x, txtbox.endxy.y );
	FTsymbol_writeFB("EGI Editor",120,5,WEGI_COLOR_LTBLUE, NULL, NULL);
	/* draw grid */
	fbset_color(WEGI_COLOR_GRAYB);
	for(k=0; k<=tlns; k++)
		draw_line(&gv_fb_dev, txtbox.startxy.x, txtbox.startxy.y+k*(fh+fgap), txtbox.endxy.x, txtbox.startxy.y+k*(fh+fgap));

        fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
	fb_render(&gv_fb_dev);

	/* Set termio */
        tcgetattr(0, &old_settings);
        new_settings=old_settings;
        new_settings.c_lflag &= (~ICANON);      /* disable canonical mode, no buffer */
        new_settings.c_lflag &= (~ECHO);        /* disable echo */
        new_settings.c_cc[VMIN]=0;
        new_settings.c_cc[VTIME]=0;
        tcsetattr(0, TCSANOW, &new_settings);

	/* count chars in txtbuff */
	nch=cstr_strcount_uft8((const unsigned char *)chmap->txtbuff);
	printf("Total %d chars\n", nch);
	gettimeofday(&tm_blink,NULL);


#if 0	/* --- TEST ---: EGI_FTCHAR_MAP */
	int i;
	//printf("sizeof(typeof(*chmap->tt))=%d\n",sizeof(typeof(*chmap->tt)));
	//printf("sizeof(typeof(*chmap->charX))=%d\n",sizeof(typeof(*chmap->charX)));
	//exit(1);
       	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
	FTcharmap_writeFB(&gv_fb_dev, txtbox.startxy.x +smargin, txtbox.startxy.y, WEGI_COLOR_BLACK, NULL, NULL);
	fbset_color(WEGI_COLOR_RED);
	printf("chmap->chcount=%d\n", chmap->chcount);
	for(i=0; i<nch+1; i++) {  /* The last one is always the new insterting point */
		printf("i=%d, charPos=%d, charX=%d, charY=%d, char=%s\n", i, chmap->charPos[i], chmap->charX[i], chmap->charY[i], chmap->txtbuff+ chmap->charPos[i] );
		penx=chmap->charX[i]; peny=chmap->charY[i];
		draw_filled_rect(&gv_fb_dev, penx, peny, penx+1, peny+fh+fgap-1);
		fb_render(&gv_fb_dev);
		tm_delayms(100);
	}
	exit(0);
#endif

	/* To fill in FTCHAR map and get penx,peny */
	FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_BLACK, &penx, &peny);
	fb_render(&gv_fb_dev);

	/* Set current(inserting/writing) byte/char position: at the end of displayed text.  */
	//chmap->pch=chmap->chcount-1;  /* Or keep default, as to the beginning of the charmap */
	printf("Init: chmap->chcount=%d, pch=%d, pos=%d\n",chmap->chcount, chmap->pch, chmap->charPos[chmap->pch]);
//	if(pos<0)pos=0;
//	if(pch<0)pch=0;

	/* Loop editing ...   Read from TTY standard input, without funcion keys on keyboard ...... */
	while(1) {

	/* ---- OPTION 1: Read more than N bytes each time, nonblock. !!! NOT good, response slowly, and may cause parse errors !!!  -----*/

	/* ---- OPTION 2: Read in char one by one, nonblock  -----*/
		ch=0;
		read(STDIN_FILENO, &ch, 1);
		if(ch>0) {
		  if(isalpha(ch))
			printf("'%c'\n",ch);
		  else
		  	printf("ch=%d\n",ch);
		}
		//continue;

		/* 1. Parse TTY input escape sequences */
		if(ch==27) {
			ch=0;
			read(STDIN_FILENO,&ch, 1);
			printf("ch=%d\n",ch);
			if(ch==91) {
				ch=0;
				read(STDIN_FILENO,&ch, 1);
				printf("ch=%d\n",ch);
				switch ( ch ) {
					case 49:
						read(STDIN_FILENO,&ch, 1);
						/* mkeyboard HOME  move pch to the first of the line */
						if( ch == 126 ) {
							FTcharmap_goto_lineBegin(chmap);
						}
                                                break;
					case 51: /* DEL */
						read(STDIN_FILENO,&ch, 1);
		                                printf("ch=%d\n",ch);
						if( ch ==126 ) /* Try to delete a char pointed by chmap->pch */
							FTcharmap_delete_char(chmap);
						break;
					case 52: /* END */
						read(STDIN_FILENO,&ch, 1);
						/* mkeyboard END  move pch to the first of the line */
						if( ch == 126 ) {
							FTcharmap_goto_lineEnd(chmap);
						}
                                                break;
					case 53: /* PAGE UP */
						read(STDIN_FILENO,&ch, 1);
						if( ch == 126 ) {
							FTcharmap_page_up(chmap);
						}
                                                break;
					case 54: /* PAGE DOWN */
						read(STDIN_FILENO,&ch, 1);
						if( ch == 126 ) {
							FTcharmap_page_down(chmap);
						}
                                                break;
					case 65:  /* UP arrow : move char pch one display_line up */
						FTcharmap_shift_cursor_up(chmap);
						break;
					case 66:  /* DOWN arrow : move pch one display_line down */
						 /* 1. If already the last dline at page bottom */
						 if( chmap->charY[chmap->pch] > txtbox.startxy.y+tmargin+(fh+fgap)*(chmap->maplines-2)+fh/2 ) {
						     /* If get EOF, then break.  */
						     if(   chmap->pref[chmap->charPos[chmap->chcount-1]] == '\0' )
                                                       		break;
						     //else  FTcharmap_scroll_oneline_down(chmap); NOPE!!! this function move pch(cursor) down also!
						     else if( chmap->maplncount>1 ) {
                				     /* Before/after calling FTcharmap_uft8string_writeFB(), champ->txtdlinePos[txtdlncount]
						      * always pointer to the first dline: champ->txtdlinePos[txtdlncount]==chmap->maplinePos[0]
                 				      * So we need to reset txtdlncount to pointer the next dline
                 				      */
                						chmap->pref=chmap->txtbuff + chmap->txtdlinePos[++chmap->txtdlncount]; /* !++ */
                					//SAME: chmap->pref=chmap->pref+chmap->maplinePos[1];  /* move pref pointer to next dline */
                						printf("%s: Shift down, chmap->txtdlncount=%d \n", __func__, chmap->txtdlncount);
								/* record current pxy */
								int px=chmap->charX[chmap->pch];
								int py=chmap->charY[chmap->pch];
								/* charmap must be updated according to new pref */
                				                FTcharmap_writeFB(NULL, WEGI_COLOR_BLACK, NULL, NULL);
								/* locate pch according to previous pxy */
						        	FTcharmap_locate_charPos (chmap, px, py);
								/* Go on to call charmap again and draw cursors. */
        					    }
						}
						/* 2. Else move down one dline to locate pch */
						else {
						   FTcharmap_locate_charPos (chmap, chmap->charX[chmap->pch], chmap->charY[chmap->pch]+fh/2+fh+fgap);
						}

						break;
					case 67:  /* RIGHT arrow : move pch one char right, Limit to the last [pch] */
						/* TODO: scroll line down if gets to the end of chmap, but not end of txtbuff */
						if( chmap->pch < chmap->chcount-1 ) {
							chmap->pch++;
						}
						break;
					case 68: /* LEFT arrow : move pch one char left */
						/* TODO: scroll line up if gets to the head of chmap, but not head of txtbuff */
						if( chmap->pch > 0 ) {
							chmap->pch--;
						}
						break;
					case 70: /* END : move pch to the end of the return_line */
						FTcharmap_goto_lineEnd(chmap);
						break;

					case 72: /* HOME : move pch to the begin of the return_line */
						/* move pch to the first of the line */
						FTcharmap_goto_lineBegin(chmap);
						break;
				}
			}

			/* reset 0 to ch: NOT a char, just for the following precess to ignore it.  */
			ch=0;
		}

		/* 2. Pan displaying text */
		if( ch==CTRL_N ) {		/* Pan one line up, until the last line. */
			FTcharmap_scroll_oneline_down(chmap);
			//FTcharmap_set_pref_nextDispLine(chmap);
			ch=0; /* to ignore  */
		}
		else if ( ch==CTRL_O) {		/* Pan one line down */
			FTcharmap_scroll_oneline_up(chmap);
			ch=0;
		}

		/* 3. edit text */

		/* --- EDIT:  3.1 Backspace */
		if( ch==0x7F ) { /* backspace */
			FTcharmap_go_backspace( chmap );
			ch=0;
		}

		/* --- EDIT:  3.2 Insert chars, !!! printable chars + '\n' + TAB  !!! */
		else if( (ch>31 || ch==9 || ch==10 ) && chmap->pch>=0 ) {  //pos>=0 ) {   /* If pos<0, then ... */

			/* ---- TEST: insert string */
			if(ch=='5' && TEST_INSERT  ) {
				static int pos=0;

                                /* revise chsize and chns  */
                                chns=cstr_strcount_uft8((const unsigned char *)strInsert);

				FTcharmap_insert_char( chmap, strInsert+pos );

				pos += cstr_charlen_uft8((const unsigned char *)(strInsert+pos));
				if(*(strInsert+pos)=='\0')
					pos=0;

                        }
			else
				FTcharmap_insert_char( chmap, &ch );


	        	/*  Get mutex lock   ----------->  */
        		if(pthread_mutex_lock(&chmap->mutex) !=0){
               			 printf("%s: Fail to lock charmap mutex!", __func__);
                		return -2;
        		}

			/* Already set in FTcharmap_insert_char(), chmap->pchoff will be reset to 0 after charmap, save to pchoff. */
			pchoff=chmap->pchoff;
			chmap->pchoff=0; /* !!!! reset chmap->pchoff, to let chmap->pch unchangd in the following charmapping */

        		/*  <-------- Put mutex lock */
        		pthread_mutex_unlock(&chmap->mutex);


		}  /* END insert ch */


		/* EDIT:  Need to refresh EGI_FTCHAR_MAP after editing !!! .... */

		/* CHARMAP & DISPLAY: Display txt */
        	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
		FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_BLACK, NULL, NULL);

		/* POST: 1. Check pch and charmap again.
		 *	 				    --- NOTE ---
		 * 	1. It is assumed that condition (chmap->pch > chmap->chcount-1) is caused only by inserting a new char
		 *	   and there are no more place in charmap to hold the new char, as pch>chcount-1, which will trigger line scroll hereby.
		 *      2. When
		 */
		if( chmap->pch > chmap->chcount-1 ) {
			printf("chmap->pch > chmap->chcount-1! Set chmap->pchoff and trigger scroll_down. \n");

			/* TODO: mutex lock for chmap elements */
			/* Chmapsize gets limit! and current cursor is NOT charmaped! we need to shift one line down
		         * In order to keep chmap->pch pointing to the typing/inserting cursor after charmapping, we shall
			 * set chmap->pchoff to tell charmap function.
			 */

			/* Already set in FTcharmap_insert_char(), saved to pchoff */
			 chmap->pchoff=pchoff;  /* pchoff is calculated each time inserting a new char, see above. */


			/* Any editing procesure MUST NOT call scroll_oneline_()! race condition, conflict chmap->pchoff */
			// FTcharmap_scroll_oneline_down(chmap); /* shift one line down */
		        /* Change chmap->pref, to change charmap start postion */
      			if( chmap->maplncount>1 ) {
                		/* Before/after calling FTcharmap_uft8string_writeFB(), champ->txtdlinePos[txtdlncount] always pointer to the first
                 	 	*  currently displayed line. champ->txtdlinePos[txtdlncount]==chmap->maplinePos[0]
                 	 	* So we need to reset txtdlncount to pointer the next line
                 		*/
                		chmap->txtdlncount++;
                		chmap->pref=chmap->txtbuff + chmap->txtdlinePos[chmap->txtdlncount];
                		printf("%s: chmap->txtdlncount=%d \n", __func__, chmap->txtdlncount);
			}

			/* We shall charmap immediately, before inserting any new char, just to keep chmap->pchoff consistent with current txtbuff. */
        		fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
			FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_BLACK, NULL, NULL);

			/* TODO: If inserting a string, then it may trigger more than 1 line scroll! */
		}

		/* POST:  check chmap->errbits */
		if( chmap->errbits ) {
			printf("WARNING: chmap->errbits=%#x \n",chmap->errbits);
		}

		/* POST:  2. typing_cursor blink */
		gettimeofday(&tm_now, NULL);
		if( tm_signed_diffms(tm_blink, tm_now) > BLINK_INTERVAL_MS ) {
			tm_blink=tm_now;
			cursor_blink = !cursor_blink;
		}

		if(cursor_blink) {
			/* TODO:
			   1. chmap->pch need to be checked, it may be invalid, and then charXY[] is  meaningless
			   2. Do not draw typing cursor if it is NOT in the charmap range?
			 */
		        penx=chmap->charX[chmap->pch];
			peny=chmap->charY[chmap->pch];

		    #if 0 /* Use char '|', NOT at left most of the char however!  */
			FTsymbol_writeFB("|",penx,peny,WEGI_COLOR_BLACK,NULL,NULL);
		    #else /* Draw geometry,  */
//			printf("txtlen=%d, strlen(txtbuff)=%d,count=%d, pch=%d,  penx,y=%d,%d \n",
//							chmap->txtlen, strlen(chmap->pref), chmap->chcount, pch, penx, peny);
			/* ! peny MAY be out of txt box! see FTcharmpa_writeFb(), a '/n' just before the txtbuff EOF. */
			if( peny < txtbox.endxy.y-(fh+fgap)+fh/2 ) {
				fbset_color(WEGI_COLOR_RED);
				//draw_wline(&gv_fb_dev, penx, peny, penx, peny+fh+fgap-1,2);
				draw_filled_rect(&gv_fb_dev, penx, peny, penx+1, peny+fh+fgap-1);
			}
			else	{ /* RESET_PCH: reset pch to move cursor to the beginning of txtbox */
				//printf(" ***** penx,y out of txtbox, reset pch to 0\n");
				//pch=0;
				printf(" ***** penx,y out of txtbox, reset pch to chcount-2 \n");
				chmap->pch=chmap->chcount-2;
			}
		    #endif
		}

		/* POST: 3. Draw mouse cursor icon */
        	draw_mcursor(mouseX, mouseY);

		/* Render and bring image to screen */
		fb_render(&gv_fb_dev);
	}



	/* <<<<<<<<<<<<<<<<<<<<    Read from USB Keyboard HIDraw   >>>>>>>>>>>>>>>>>>> */
	static char txtbuff[1024];	/* text buffer */

	/* Open term */
	term_fd=open("/dev/hidraw0", O_RDONLY);
	if(term_fd<0) {
		printf("Fail open hidraw0!\n");
		exit(1);
	}

	while(1)
	{
                /* To select fd */
                FD_ZERO(&rfds);
                FD_SET( term_fd, &rfds);
                tmval.tv_sec=0;
                tmval.tv_usec=0;

                retval=select( term_fd+1, &rfds, NULL, NULL, &tmval );
                if(retval<0) {
                        //printf("%s: select event: errno=%d, %s \n",__func__, errno, strerror(errno));
                        continue;
                }
                else if(retval==0) {
                        //printf("Time out\n");
                        continue;
                }
                /* Read input fd and trigger callback */
                if(FD_ISSET(term_fd, &rfds)) {
			memset(txtbuff,0,8); //sizeof(txtbuff));
                        retval=read(term_fd, txtbuff, 8);
			printf("retval=%d: %d,%d,%d,%d, %d,%d,%d,%d\n",retval, txtbuff[0],txtbuff[1],txtbuff[2],txtbuff[3],
										txtbuff[4],txtbuff[5],txtbuff[6],txtbuff[7]);
		}
	}


                  /*-----------------------------------
                   *         End of Main Program
                   -----------------------------------*/

	/* Reset termio */
        tcsetattr(0, TCSANOW,&old_settings);

	/* My release */
	FTcharmap_free(&chmap);


 /* <<<<<  EGI general release   >>>>>> */
 printf("FTsymbol_release_allfonts()...\n");
 FTsymbol_release_allfonts(); /* release sysfonts and appfonts */
 printf("symbol_release_allpages()...\n");
 symbol_release_allpages(); /* release all symbol pages*/
 printf("FTsymbol_release_allpage()...\n");
 FTsymbol_release_allpages(); /* release FT derived symbol pages: sympg_ascii */

 printf("fb_filo_flush() and release_fbdev()...\n");
 fb_filo_flush(&gv_fb_dev);
 release_fbdev(&gv_fb_dev);
 #if 0
 printf("release virtual fbdev...\n");
 release_virt_fbdev(&vfb);
 #endif
 printf("egi_end_touchread()...\n");
 egi_end_touchread();
 #if 0
 printf("egi_quit_log()...\n");
 egi_quit_log();
 #endif
 printf("<-------  END  ------>\n");

return 0;
}


/*-------------------------------------
        FTcharmap WriteFB TXT
@txt:   	Input text
@px,py: 	LCD X/Y for start point.
@color: 	Color for text
@penx, peny:    pointer to pen X.Y.
--------------------------------------*/
static int FTcharmap_writeFB(FBDEV *fbdev, EGI_16BIT_COLOR color, int *penx, int *peny)
{
	int ret;

       	ret=FTcharmap_uft8strings_writeFB( fbdev, chmap,          	/* FBdev, charmap*/
                                        egi_sysfonts.regular, fw, fh,   /* fontface, fw,fh */
                                        color, -1, 255,      		/* fontcolor, transcolor,opaque */
                                        NULL, NULL, penx, peny);        /* int *cnt, int *lnleft, int* penx, int* peny */

        /* Double check! */
        if( chmap->chcount > chmap->mapsize ) {
                printf("WARNING:  chmap.chcount > chmap.mapsize=%d, some displayed chars has NO charX/Y data! \n",
													chmap->mapsize);
        }
        if( chmap->txtdlncount > chmap->txtdlines ) {
                printf("WARNING:  chmap.txtdlncount=%d > chmap.txtdlines=%d, some displayed lines has NO position info. in charmap! \n",
                                                                                	chmap->txtdlncount, chmap->txtdlines);
        }

	return ret>0?ret:0;
}


/*-------------------------------------
        FTsymbol WriteFB TXT
@txt:   	Input text
@px,py: 	LCD X/Y for start point.
@color: 	Color for text
@penx, peny:    pointer to pen X.Y.
--------------------------------------*/
static void FTsymbol_writeFB(char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny)
{
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,    	/* fw,fh, pstr */
                                        320, tlns, fgap,      /* pixpl, lines, fgap */
                                        px, py,                                 /* x0,y0, */
                                        color, -1, 255,      			/* fontcolor, transcolor,opaque */
                                        NULL, NULL, penx, peny);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}


/*--------------------------------------------
        Callback for mouse input
Just update mouseXYZ
---------------------------------------------*/
static void mouse_callback(unsigned char *mouse_data, int size)
{
	int i;
	int mdZ;

        /* 1. Check pressed key */
        if(mouse_data[0]&0x1) {
                printf("Leftkey down!\n");
		mouseLeftKeyDown=true;
	} else
		mouseLeftKeyDown=false;

        if(mouse_data[0]&0x2)
                printf("Right key down!\n");

        if(mouse_data[0]&0x4)
                printf("Mid key down!\n");

        /*  2. Get mouse X */
        mouseX += ( (mouse_data[0]&0x10) ? mouse_data[1]-256 : mouse_data[1] );
        if(mouseX > gv_fb_dev.pos_xres -5)
                mouseX=gv_fb_dev.pos_xres -5;
        else if(mouseX<0)
                mouseX=0;

        /* 3. Get mouse Y: Notice LCD Y direction!  Minus for down movement, Plus for up movement!
         * !!! For eventX: Minus for up movement, Plus for down movement!
         */
        mouseY -= ( (mouse_data[0]&0x20) ? mouse_data[2]-256 : mouse_data[2] );
        if(mouseY > gv_fb_dev.pos_yres -5)
                mouseY=gv_fb_dev.pos_yres -5;
        else if(mouseY<0)
                mouseY=0;

        /* 4. Get mouse Z */
	mdZ= (mouse_data[3]&0x80) ? mouse_data[3]-256 : mouse_data[3] ;
        mouseZ += mdZ;
        //printf("get X=%d, Y=%d, Z=%d \n", mouseX, mouseY, mouseZ);
	if( mdZ > 0 ) {
		 FTcharmap_scroll_oneline_down(chmap);
	}
	else if ( mdZ < 0 ) {
		 FTcharmap_scroll_oneline_up(chmap);
	}

	/* 5. To get current byte/char position  */
	if(mouseLeftKeyDown) {
	   mouseMidY=mouseY+fh/2;
	   mouseMidX=mouseX+fw/2;
	   printf("mouseMidX,Y=%d,%d \n",mouseMidX, mouseMidY);
	   FTcharmap_locate_charPos( chmap, mouseMidX, mouseMidY );
	}

}

/*--------------------------------
 Draw cursor for the mouse movement
1. In txtbox area, use typing cursor.
2. Otherwise, apply mcursor.
----------------------------------*/
static void draw_mcursor(int x, int y)
{
        static EGI_IMGBUF *mcimg=NULL;
        static EGI_IMGBUF *tcimg=NULL;
	EGI_POINT pt;

#ifdef LETS_NOTE
	#define MCURSOR_ICON_PATH "/home/midas-zhou/egi/mcursor.png"
	#define TXTCURSOR_ICON_PATH "/home/midas-zhou/egi/txtcursor.png"
#else
	#define MCURSOR_ICON_PATH "/mmc/mcursor.png"
	#define TXTCURSOR_ICON_PATH "/mmc/txtcursor.png"
#endif
        if(mcimg==NULL)
                mcimg=egi_imgbuf_readfile(MCURSOR_ICON_PATH);
	if(tcimg==NULL) {
		tcimg=egi_imgbuf_readfile(TXTCURSOR_ICON_PATH);
		egi_imgbuf_resize_update(&tcimg, fw, fh );
	}

	pt.x=mouseX;	pt.y=mouseY;

        /* OPTION 1: Use EGI_IMGBUF */
	if( point_inbox(&pt, &txtbox) && tcimg!=NULL ) {
                egi_subimg_writeFB(tcimg, &gv_fb_dev, 0, WEGI_COLOR_RED, mouseX, mouseY ); /* subnum,subcolor,x0,y0 */
	}
	else if(mcimg) {
               	egi_subimg_writeFB(mcimg, &gv_fb_dev, 0, -1, mouseX, mouseY ); /* subnum,subcolor,x0,y0 */
	}

        /* OPTION 2: Draw geometry */

}


