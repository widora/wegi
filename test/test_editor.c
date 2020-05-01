/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of a simple editor, read input from termios and put it on to
the screen.


		--- Usage and Notes ---

1. Use MOUSE to move typing cursor to locate insert/delete position.
2. Use ARROW_KEYs to move typing cursor to locate insert/delete position.
3. Use BACKSPACE to delete chars before the cursor.
4. Use DEL to delete chars following the cursor.
5. Use HOME to return cursor to the begin of the line.
6. Use END to move cursor to the end of the line.

TODO:
1. If any control code other than '\n' put in txtbuff[], strange things
   may happen...	due to UFT-8 encoding.   ---Avoid it.
2. Check whether txtbuff[] overflows.

Midas Zhou
midaszhou@yahoo.com
---------------------------------------------------------------------*/
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
#include "egi_FTsymbol.h"
#include "egi_cstring.h"


#define 	BLINK_INTERVAL_MS	500	/* typing_cursor blink interval in ms */

/* TTY additional control KEY definition.  USB Keyboard hidraw data is another story...  */
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


static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 240-1-10} };	/* Txt zone */
static int smargin=5; 		/* left and right side margin of text area */
static int tmargin=0;		/* top margin of text area */
static char txtbuff[1024];	/* text buffer */

//static int pos;    		/* current byte position of txtbuff[], in bytes, from 0 */
static int pch;    		/* current char(displayed in txtbox) index position of txtbuff[], from 0, --->to locate typing cursor*/
/* NOTE: char position, in respect of txtbuff index and data_offset: pos=chmap->charPos[pch] */
static EGI_FTCHAR_MAP *chmap;	/* FTchar map to hold char position data */

static int mouseX, mouseY, mouseZ; /* Mouse point position */
static int mouseMidX, mouseMidY;   /* Mouse cursor mid position */

static int fw=18;	/* Font size */
static int fh=20;
static int fgap=20/5;	/* Text line fgap : TRICKY ^_- */
static int tlns;  //=(txtbox.endxy.y-txtbox.startxy.y)/(fh+fgap); /* Total available line space for displaying txt */

static int penx;	/* Pen position */
static int peny;

static void FTcharmap_writeFB(char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny);
static void FTsymbol_writeFB(char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny);
static void mouse_callback(unsigned char *mouse_data, int size);
static void draw_mcursor(int x, int y);

static int FTcharmap_locate_charPos( const EGI_FTCHAR_MAP *chmap, int x, int y,  int lndis, int *pch);
static int FTcharmap_goto_lineBegin( const EGI_FTCHAR_MAP *chmap, int *pch);
static int FTcharmap_goto_lineEnd( const EGI_FTCHAR_MAP *chmap, int *pch);



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


	/* Init. txt and charmap */
	strcat(txtbuff,"12345\n歪朵拉的惊奇\nwidora_NEO\n\n"); //   --- hell world ---\n\n");
	strcat(txtbuff,"1122\n你既有此心，待我到了东土大唐国寻一个取经的人来，教他救你。你可跟他做个徒弟，秉教伽持，入我佛门。\n"); //   --- hell world ---\n\n");
	chmap=FTcharmap_create(sizeof(txtbuff));
	if(chmap==NULL){ printf("Fail to create char map!\n"); exit(0); };
	chmap->txtlen=strlen(txtbuff);

	tlns=(txtbox.endxy.y-txtbox.startxy.y)/(fh+fgap); /* Total available txt lines */

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
	nch=cstr_strcount_uft8((const unsigned char *)txtbuff);
	printf("Total %d chars\n", nch);
	gettimeofday(&tm_blink,NULL);


#if 0	/* --- TEST ---: EGI_FTCHAR_MAP */
	int i;
	//printf("sizeof(typeof(*chmap->tt))=%d\n",sizeof(typeof(*chmap->tt)));
	//printf("sizeof(typeof(*chmap->charX))=%d\n",sizeof(typeof(*chmap->charX)));
	//exit(1);
       	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
	FTcharmap_writeFB(txtbuff, txtbox.startxy.x +smargin, txtbox.startxy.y, WEGI_COLOR_BLACK, NULL, NULL);
	fbset_color(WEGI_COLOR_RED);
	printf("chmap->chcount=%d\n", chmap->chcount);
	for(i=0; i<nch+1; i++) {  /* The last one is always the new insterting point */
		printf("i=%d, charPos=%d, charX=%d, charY=%d, char=%s\n", i, chmap->charPos[i], chmap->charX[i], chmap->charY[i], txtbuff+ chmap->charPos[i] );
		penx=chmap->charX[i]; peny=chmap->charY[i];
		draw_filled_rect(&gv_fb_dev, penx, peny, penx+1, peny+fh+fgap-1);
		fb_render(&gv_fb_dev);
		tm_delayms(100);
	}
	exit(0);
#endif

	/* To fill in FTCHAR map and get penx,peny */
	FTcharmap_writeFB(txtbuff, txtbox.startxy.x +smargin, txtbox.startxy.y, WEGI_COLOR_BLACK, &penx, &peny);
	fb_render(&gv_fb_dev);

	/* Set current(inserting/writing) byte/char position: at the end of displayed text.  */
	pch=chmap->chcount-1;
	printf("chmap->chcount=%d, pch=%d, pos=%d\n",chmap->chcount, pch, chmap->charPos[pch]);
//	if(pos<0)pos=0;
//	if(pch<0)pch=0;

	/* Loop editing ...   Read from TTY standard input, without funcion keys on keyboard ...... */
	while(1) {

#if 0	/* ---- OPTION 1: Read more than N bytes each time, nonblock. !!! NOT good, response slowly, and may cause parse errors !!!  -----*/
		bzero(chs,sizeof(chs));
		retval=read(STDIN_FILENO,chs,sizeof(chs));
		if(retval>0)
			printf("ret=%d: %d,%d,%d,%d\n",retval,chs[0],chs[1],chs[2],chs[3]);

		/* Parse additional TTY control key */
		if(retval>2 && strncmp(chs,"\033[",2)==0 ) {
			switch( chs[2] ) {
				case  	'1':	/* TTY_HOME */
				case	'H':
					/* mkeyboard HOME  move pch to the first of the line */
	                              	FTcharmap_goto_lineBegin(chmap, &pch);
					break;
				case	'3':	/* TTY_DEL */
					if( chmap->txtlen >0 ) {
						memmove( txtbuff+chmap->charPos[pch], txtbuff+chmap->charPos[pch+1],
								strlen(txtbuff+chmap->charPos[pch]) +1); /* move backward */
						/* keep pos and pch unchanged */
						printf("DEL: pos=%d, pch=%d\n", chmap->charPos[pch], pch);
					}
					break;
				case	'4':	/* TTY_END */
				case	'F':
					/* mkeyboard END  move pch to the end of the line */
        	                      	FTcharmap_goto_lineEnd(chmap, &pch);
					break;
				case	'A':	/* TTY_UP arrwo */
					/* move char pch one display_line up */
					/* refresh char pos/pch */
	   				FTcharmap_locate_charPos( chmap, chmap->charX[pch], chmap->charY[pch]+fh/2-fh-fgap, fh+fgap, &pch);
					break;
				case 	'B':  /* TTY_DOWN arrow : move pch one display_line down */
	   					FTcharmap_locate_charPos( chmap, chmap->charX[pch], chmap->charY[pch]+fh/2+fh+fgap, fh+fgap, &pch);
					break;
				case 	'C':  /* TTY_RIGHT arrow : move pch one char right */
					if(pch < chmap->chcount-1 ) {
						pch ++;
					}
					break;
				case 	'D': /* TTY_LEFT arrow : move pch one char left */
					if(pch>0) {
						pch --;
					}
					break;
			} /* end switch() */

			chs[0]=0; /* reset chs[0], since it's not single ASCII key. */
		}
		else if(retval>1) {  /* Usually wrong, ignore it */
			printf("WARNING: read in more than 1 ASCII keys!\n");
			chs[0]=0;
		}

		/* Otherwise assume only one ASCII read in */
		ch=chs[0];

#else	/* ---- OPTION 2: Read in char one by one, nonblock  -----*/
		ch=0;
		read(STDIN_FILENO, &ch, 1);
		if(ch>0)
		  printf("ch=%d\n",ch);
		//continue;

		/* Parse arrow movements  */
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
                                                	FTcharmap_goto_lineBegin(chmap, &pch);
						}
                                                break;
					case 51:
						read(STDIN_FILENO,&ch, 1);
		                                printf("ch=%d\n",ch);
						/*  DEL */
						if( ch == 126 && chmap->txtlen>0 ) {
							memmove( txtbuff+chmap->charPos[pch], txtbuff+chmap->charPos[pch+1],
											strlen(txtbuff+chmap->charPos[pch]) +1); /* move backward */
							/* charPos[] NOT updated, before calling charmap_writeFB ! */
							chmap->txtlen -= chmap->charPos[pch+1]-chmap->charPos[pch];
							/* keep pos and pch unchanged */
							printf("DEL: pos=%d, pch=%d\n", chmap->charPos[pch], pch);
						}
						break;
					case 52:
						read(STDIN_FILENO,&ch, 1);
						/* mkeyboard END  move pch to the first of the line */
						if( ch == 126 ) {
                                                	FTcharmap_goto_lineEnd(chmap, &pch);
						}
                                                break;
					case 65:  /* UP arrow : move char pch one display_line up */
						/* refresh char pos/pch */
	   					FTcharmap_locate_charPos( chmap, chmap->charX[pch], chmap->charY[pch]+fh/2-fh-fgap, fh+fgap, &pch);
						break;
					case 66:  /* DOWN arrow : move pch one display_line down */
	   					FTcharmap_locate_charPos( chmap, chmap->charX[pch], chmap->charY[pch]+fh/2+fh+fgap, fh+fgap, &pch);
						break;
					case 67:  /* RIGHT arrow : move pch one char right */
						if(pch < chmap->chcount-1 ) {
							pch ++;
						}
						break;
					case 68: /* LEFT arrow : move pch one char left */
						if(pch>0) {
							pch --;
						}
						break;
					case 70: /* END : move pch to the end of the return_line */
						FTcharmap_goto_lineEnd(chmap, &pch);
						break;
					case 72: /* HOME : move pch to the begin of the return_line */
						/* move pch to the first of the line */
						FTcharmap_goto_lineBegin(chmap, &pch);
						break;
				}
			}

			/* reset 0 to ch: NOT a char, just for the following precess to ignore it.  */
			ch=0;
		}
#endif /*---- End of OPTIONs ----*/


		/* --- EDIT:  1. Backspace */
		if( ch==0x7F ) { /* backspace */
		   //if( pos>0 ) {
		   if( pch > 0 ) {
			/* ! move whole string forward.. */
			memmove(txtbuff+chmap->charPos[pch-1], txtbuff+chmap->charPos[pch],  strlen(txtbuff+chmap->charPos[pch-1]) +1);
			/* charPos[] NOT updated, before calling charmap_writeFB ! */
			chmap->txtlen -= chmap->charPos[pch]-chmap->charPos[pch-1];
			pch--;
		  }
		}

		/* --- EDIT:  2. Insert chars, !!! printable chars + '\n' + TAB  !!! */
		else if( (ch>31 || ch==9 || ch==10 ) && pch>=0 ) {  //pos>=0 ) {   /* If pos<0, then ... */
			/* Insert at txt end */
			if(txtbuff[ chmap->charPos[pch] ]==0) {
				printf("insert at end, ch=%d \n",ch);
				txtbuff[ chmap->charPos[pch] ]=ch;
				/* move pch forward */
				pch++;
				chmap->charPos[pch]=chmap->charPos[pch-1]+sizeof(char);
				/* Warning! now pch MAY out of displaying txtbox range! charXY is undefined 0,0!
				 * search/see RESET_PCH
				 */
				chmap->txtlen +=sizeof(char);
			}
			/* Insert not at end */
			else {
				memmove(txtbuff+chmap->charPos[pch]+sizeof(char), txtbuff+chmap->charPos[pch],
									strlen(txtbuff+chmap->charPos[pch] ) +1);
				txtbuff[ chmap->charPos[pch] ]=ch;
				/* move pch forward */
				pch++;
				chmap->charPos[pch]=chmap->charPos[pch-1]+sizeof(char);
				chmap->txtlen +=sizeof(char);
			}
		}


		/* EDIT:  Need to refresh EGI_FTCHAR_MAP after editing !!! .... */

		/* Display txt */
        	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
		FTcharmap_writeFB(txtbuff, txtbox.startxy.x+smargin, txtbox.startxy.y, WEGI_COLOR_BLACK, NULL, NULL);

		/* Check if pch exceeds displaying count */
		//printf("pch=%d, chmap->chcount=%d \n", pch, chmap->chcount);
		if( pch > chmap->chcount-1 )
			pch=0;

		/* Cursor blink */
		gettimeofday(&tm_now, NULL);
		if( tm_signed_diffms(tm_blink, tm_now) > BLINK_INTERVAL_MS ) {
			tm_blink=tm_now;
			cursor_blink = !cursor_blink;
		}
		if(cursor_blink) {
		    #if 0 /* Use char '|' */
			FTsymbol_writeFB("|",penx,peny,WEGI_COLOR_BLACK,NULL,NULL);
		    #else /* Draw geometry */
			penx=chmap->charX[pch];
			peny=chmap->charY[pch];
			printf("txtlen=%d, strlen(txtbuff)=%d,count=%d, pch=%d,  penx,y=%d,%d \n",
							chmap->txtlen, strlen(txtbuff), chmap->chcount, pch, penx, peny);
			/* ! peny MAY be out of txt box! see FTcharmpa_writeFb() */
			if( peny < txtbox.endxy.y-(fh+fgap) ) {
				fbset_color(WEGI_COLOR_RED);
				draw_filled_rect(&gv_fb_dev, penx, peny, penx+1, peny+fh+fgap-1);
			}
			else	{ /* RESET_PCH: reset pch to move cursor to the beginning of txtbox */
				printf("penx,y out of txtbox, reset pch to 0\n");
				pch=0;
			}
		    #endif
		}

		/* 5. Draw mouse cursor icon */
        	draw_mcursor(mouseX, mouseY);

		/* Render and bring image to screen */
		fb_render(&gv_fb_dev);
	}



	/* <<<<<<<<<<<<<<<<<<<<    Read from USB Keyboard HIDraw   >>>>>>>>>>>>>>>>>>> */
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
static void FTcharmap_writeFB(char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny)
{
       	FTcharmap_uft8strings_writeFB(   &gv_fb_dev, chmap, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,    	/* fw,fh, pstr */
                                        320-20-2*smargin, tlns, fgap,      /* pixpl, lines, fgap */
                                        px, py,                                 /* x0,y0, */
                                        color, -1, 255,      			/* fontcolor, transcolor,opaque */
                                        NULL, NULL, penx, peny);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
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
	bool LeftKey_down;

        /* 1. Check pressed key */
        if(mouse_data[0]&0x1) {
                printf("Leftkey down!\n");
		LeftKey_down=true;
	} else
		LeftKey_down=false;


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
        mouseZ += ( (mouse_data[3]&0x80) ? mouse_data[3]-256 : mouse_data[3] );
        //printf("get X=%d, Y=%d, Z=%d \n", mouseX, mouseY, mouseZ);

	/* 5. To get current byte/char position  */

	if(LeftKey_down) {
	   mouseMidY=mouseY+fh/2;
	   mouseMidX=mouseX+fw/2;
	   FTcharmap_locate_charPos( chmap, mouseMidX, mouseMidY, fh+fgap, &pch);
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



/*-----------------------------------------------------------------
To locate pos/pch from FTCHAR map according to given x,y.

@chmap:		The FTCHAR map.
@x,y:		A B/LCD coordinate pointed to a char.
@lndis:		Distance between two char lines.
@pch:		A pointer to char index in chmap, in chars.
		If NULL, ignore.
@pos:		A pointer to char postion in chmap, in bytes.
		If NULL, ignore.
Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------*/
static int FTcharmap_locate_charPos( const EGI_FTCHAR_MAP *chmap, int x, int y,  int lndis, int *pch)
{
	int i;

	if( chmap==NULL || chmap->chcount < 1 ) {
		printf("%s: Input FTCHAR map is empty!\n", __func__);
		return -1;
	}

	/* Search char map to find pch/pos for the x,y  */
        for(i=0; i < chmap->chcount; i++) {
        	//printf("i=%d/(%d-1)\n", i,chmap->chcount);
                if( chmap->charY[i] < y && chmap->charY[i]+lndis > y ) {
                        if( chmap->charX[i] <= x		/*  == ALSO is the case ! */
			    && (     i==chmap->chcount-1		/* Last char/insert */
				 || ( chmap->charX[i+1] > x || chmap->charY[i] != chmap->charY[i+1] )  /* || or END of the line */
				)
			   )
                        {
				if(pch!=NULL)
                                	*pch=i;
                        }
		}
	}

	return 0;
}

/*-----------------------------------------------------------
Move pch/pos to point to the first char of the displayed line.

@chmap:	an EGI_FTCHAR_MAP
@pch:	pointer to current char index
@pos:   pointer to current char pos, pos=chmap->charPos[pch];

Return:
	0	OK,    *pch, *pos modifed accordingly.
	<0	Fail   *pch, *pos unchanged.
------------------------------------------------------------*/
static int FTcharmap_goto_lineBegin( const EGI_FTCHAR_MAP *chmap, int *pch)
{
	int index;

	if( chmap==NULL || chmap->chcount < 1 ) {
		printf("%s: Input FTCHAR map is empty!\n", __func__);
		return -1;
	}
	if( pch==NULL || *pch<0  )
		return -2;

	index=*pch;
	while( index>0 && txtbuff[ chmap->charPos[index-1] ] != '\n' ) {
		//printf("index=%d\n",index);
		index--;
	}
	/* If no '\n' found, then index will be 0 */

	*pch=index;

	return 0;
}


/*-----------------------------------------------------------
Move pch/pos to point to the end char of the displayed line.

@chmap:	an EGI_FTCHAR_MAP
@pch:	pointer to current char index
@pos:   pointer to current char pos, pos=chmap->charPos[pch];

Return:
	0	OK,    *pch, *pos modifed accordingly.
	<0	Fail   *pch, *pos unchanged.
------------------------------------------------------------*/
static int FTcharmap_goto_lineEnd( const EGI_FTCHAR_MAP *chmap, int *pch)
{
	int index;

	if( chmap==NULL || chmap->chcount < 1 ) {
		printf("%s: Input FTCHAR map is empty!\n", __func__);
		return -1;
	}
	if( pch==NULL || *pch<0 )
		return -2;

	index=*pch;
	while( index < chmap->chcount-1 && txtbuff[ chmap->charPos[index] ] != '\n' ) {
		printf("index=%d\n",index);
		index++;
	}
	/* If no '\n' found, then index will be chmap-count-1, as end  */

	*pch=index;
	 //printf("%s: pch=%d, pos=%d\n",__func__,*pch, *pos);

	return 0;
}

