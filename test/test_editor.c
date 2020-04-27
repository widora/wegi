 /*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of a simple editor, read input from termios and put it on to
the screen.


		--- Usage and Notes ---

1. Use MOUSE to move txt_cursor to locate intert/delete position.
2. Use ARROW_KEYs to move txt_cursor to locate intert/delete position.
3. Use BACKSPACE to delete chars before the cursor.
4. Use DEL to delete chars following the cursor.


TODO:
1. If any control code other than '\n' put in txtbuff[], strange things
   may happen...


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
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

static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 240-1-10} };	/* Txt zone */

static char txtbuff[1024];	/* text buffer */

static int pos;    		/* current byte position in txtbox[], in bytes, from 0 */
static int pch;    		/* current char index positiion in txtbox[], from 0 */
/* NOTE: char position, in respect of txtbuff index and data_offset: pos=chmap->charPos[pch] */
static EGI_FTCHAR_MAP *chmap;	/* FTchar map to hold char position data */

static int mouseX, mouseY, mouseZ; /* Mouse point position */
static int mouseMidX, mouseMidY;   /* txt cursor mid position */

static int fw=18;	/* Font size */
static int fh=20;
static int fgap=20/5;	/* Text line fgap : TRICKY ^_- */
static int tlns;  //=(txtbox.endxy.y-txtbox.startxy.y)/(fh+fgap); /* Total available txt lines */

static int penx;	/* Pen position */
static int peny;

static void write_txt(char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny);
static void mouse_callback(unsigned char *mouse_data, int size);
static void draw_mcursor(int x, int y);
static int get_CharPos( const EGI_FTCHAR_MAP *chmap, int x, int y,  int lndis, int *pch, int *pos);



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
	char chs[5];
	char ch;

	int  k;
	int  i;
	int  nch;
	fd_set rfds;
	int retval;
	struct timeval tmval;
	bool	cursor_blink=false;
	struct timeval tm_blink;
	struct timeval tm_now;


	/* Init. txt and charmap */
//	strcat(txtbuff,"12345\n歪朵拉的惊奇\nwidora_NEO\n\n"); //   --- hell world ---\n\n");
	strcat(txtbuff,"\n这里个个都是人才\nabcdefghijklm\n"); //   --- hell world ---\n\n");
	chmap=FTsymbol_create_charMap(sizeof(txtbuff));
	if(chmap==NULL){ printf("Fail to create char map!\n"); exit(0); };
	tlns=(txtbox.endxy.y-txtbox.startxy.y)/(fh+fgap); /* Total available txt lines */

        /* Init. mouse position */
        mouseX=gv_fb_dev.pos_xres/2;
        mouseY=gv_fb_dev.pos_yres/2;

        /* Init. FB working buffer */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY4);
	fbset_color(WEGI_COLOR_WHITE);
	//draw_filled_rect(&gv_fb_dev, 10, 30, 320-1-10,240-1-10);
	draw_filled_rect(&gv_fb_dev, txtbox.startxy.x, txtbox.startxy.y, txtbox.endxy.x, txtbox.endxy.y );
	write_txt("EGI Editor",120,5,WEGI_COLOR_LTBLUE, NULL, NULL);
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

	nch=cstr_strcount_uft8((const unsigned char *)txtbuff);
	printf("Total %d chars\n", nch);
	gettimeofday(&tm_blink,NULL);

#if 0	/* --- TEST ---: EGI_FTCHAR_MAP */
	//printf("sizeof(typeof(*chmap->tt))=%d\n",sizeof(typeof(*chmap->tt)));
	//printf("sizeof(typeof(*chmap->charX))=%d\n",sizeof(typeof(*chmap->charX)));
	//exit(1);
       	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
	write_txt(txtbuff, txtbox.startxy.x, txtbox.startxy.y, WEGI_COLOR_BLACK, NULL, NULL);
	fbset_color(WEGI_COLOR_RED);
	printf("chmap->count=%d\n", chmap->count);
	for(i=0; i<nch+1; i++) {  /* The last one is always the new insterting point */
		printf("i=%d, charPos=%d, charX=%d, charY=%d, char=%s\n", i, chmap->charPos[i], chmap->charX[i], chmap->charY[i], txtbuff+ chmap->charPos[i] );
		penx=chmap->charX[i]; peny=chmap->charY[i];
		draw_filled_rect(&gv_fb_dev, penx, peny, penx+1, peny+fh+fgap-1);
		fb_render(&gv_fb_dev);
		tm_delayms(100);
	}
	exit(0);
#endif

	/* To fill in chmap and get penx,peny */
	write_txt(txtbuff, txtbox.startxy.x, txtbox.startxy.y, WEGI_COLOR_BLACK, &penx, &peny);

	/* Set current(inserting/writing) byte/char position: at the end of txt string.  */
	pos=chmap->charPos[chmap->count-1];
	pch=chmap->count-1;
	printf("chmap->count=%d, pos=%d, pch=%d\n",chmap->count, pos, pch);
	if(pos<0)pos=0;
	if(pch<0)pch=0;

	/* Loop editing ... */
	while(1) {
		ch=0;

		/* Read in char one by one, nonblock  */
		read(STDIN_FILENO, &ch, 1);
		//if(ch>0)
		   //printf("ch=%d\n",ch);

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
					case 51:
						read(STDIN_FILENO,&ch, 1);
		                                printf("ch=%d\n",ch);
						/*  DEL */
						if( ch == 126 ) {
							memmove( txtbuff+pos, txtbuff+chmap->charPos[pch+1],
								 chmap->charPos[chmap->count-1]-chmap->charPos[pch+1] +1);  /* move backward */
							/* keep pos and pch unchanged */
							printf("DEL: pos=%d, pch=%d\n", pos, pch);
						}
						break;

					case 65:  /* UP */
						printf("UP\n");
						/* refresh char pos/pch */
	   					get_CharPos( chmap, chmap->charX[pch], chmap->charY[pch]+fh/2-fh-fgap, fh+fgap, &pch, &pos);
						break;
					case 66:  /* DOWN */
						printf("DOWN\n");
						/* refresh char pos/pch */
	   					get_CharPos( chmap, chmap->charX[pch], chmap->charY[pch]+fh/2+fh+fgap, fh+fgap, &pch, &pos);
						break;
					case 67:  /* RIGHT */
						printf("RIGHT\n");
						if(pch < chmap->count-1 ) {
							pos += chmap->charPos[pch+1]-chmap->charPos[pch];
							pch ++;
						}
						break;
					case 68: /* LEFT */
						printf("LEFT\n");
						if(pch>0) {
							pos -= chmap->charPos[pch]-chmap->charPos[pch-1];
							pch --;
						}
						break;
				}
			}

			/* reset 0 to ch: NOT a char, just for the following precess to ignore it.  */
			ch=0;
		}

		/* --- EDIT:  1. Backspace */
		if( ch==0x7F ) { /* backspace */
		   if( pos>0 ) {
			//printf("chmap:[count-1]-pch=%d, strlen(txtbuf+pos)=%d\n",chmap->charPos[chmap->count-1]-chmap->charPos[pch], strlen(txtbuff+pos));
			//memmove(txtbuff+pos-(chmap->charPos[pch]-chmap->charPos[pch-1]), txtbuff+pos,
			memmove(txtbuff+chmap->charPos[pch-1], txtbuff+pos,
						chmap->charPos[chmap->count-1]-chmap->charPos[pch] +1);  //strlen(txtbuff+pos));  /* move backward */
			pos -= (chmap->charPos[pch]-chmap->charPos[pch-1]);
			pch--;
			printf("Backspace: pos=%d, pch=%d\n", pos, pch);
		  }
		}

		/* --- EDIT:  2. Insert chars, !!! ONLY printable chars and '\n' !!! */
		else if( (ch>31 || ch==10) && pos>=0 ) {   /* If pos<0, then ... */
			/* insert at txt end */
			if(txtbuff[pos+1]==0) {
				txtbuff[pos++]=ch;
				pch++;
			}
			/* insert not at  end */
			else {
				memmove(txtbuff+pos+sizeof(char), txtbuff+pos, strlen(txtbuff+pos) +1);
				txtbuff[pos]=ch;
				pos += sizeof(char);  /* pos in bytes */
				pch++;  /* pos in chars */
			}
		}

		/* EDIT:  TO refresh EGI_FTCHAR_MAP !!! .... */

		/* Display txt */
        	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
		write_txt(txtbuff, txtbox.startxy.x, txtbox.startxy.y, WEGI_COLOR_BLACK, NULL, NULL);

		/* Cursor blink */
		gettimeofday(&tm_now, NULL);
		if( tm_signed_diffms(tm_blink, tm_now) > 500	 ) {
			tm_blink=tm_now;
			cursor_blink = !cursor_blink;
		}
		if(cursor_blink) {
			#if 0 /* Use char '|' */
			write_txt("|",penx,peny,WEGI_COLOR_BLACK);
			#else /* Draw geometry */
			penx=chmap->charX[pch];
			peny=chmap->charY[pch];
			fbset_color(WEGI_COLOR_RED);
			draw_filled_rect(&gv_fb_dev, penx, peny, penx+1, peny+fh+fgap-1);
			#endif
		}

		/* 5. Draw mouse cursor icon */
        	draw_mcursor(mouseX, mouseY);

		fb_render(&gv_fb_dev);
	}



	/* <<<<<<<<<<<<<<<<<<<<    Read from tty, buffered! echo until enter a return.   >>>>>>>>>>>>>>>>>>> */
	/* Open term */
	term_fd=open("/dev/tty1", O_RDONLY);
	if(term_fd<0) {
		printf("Fail open tty1!\n");
		exit(1);
	}


	while(1)
	{
                /* To select fd */
                FD_ZERO(&rfds);
                FD_SET( term_fd, &rfds);
                tmval.tv_sec=1;
                tmval.tv_usec=0;

                retval=select( term_fd+1, &rfds, NULL, NULL, &tmval );
                if(retval<0) {
                        //printf("%s: select event: errno=%d, %s \n",__func__, errno, strerror(errno));
                        continue;
                }
                else if(retval==0) {
                        printf("Time out\n");
                        continue;
                }
                /* Read input fd and trigger callback */
                if(FD_ISSET(term_fd, &rfds)) {
			memset(txtbuff,0,sizeof(txtbuff));
                        retval=read(term_fd, txtbuff, sizeof(txtbuff));
			printf("txtbuff:%s",txtbuff);
		}
	}


                  /*-----------------------------------
                   *         End of Main Program
                   -----------------------------------*/

	/* Reset termio */
        tcsetattr(0, TCSANOW,&old_settings);

	/* My release */
	FTsymbol_free_charMap(&chmap);


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


/*-------------------------------
        WriteFB TXT
@txt:   Input text
@px,py: LCD X/Y for start point.
-------------------------------*/
static void write_txt(char *txt, int px, int py, EGI_16BIT_COLOR color, int *penx, int *peny)
{
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,    	/* fw,fh, pstr */
                                        320-40, tlns, fgap,      /* pixpl, lines, fgap */
                                        px, py,                                 /* x0,y0, */
                                        color, -1, 255,      			/* fontcolor, transcolor,opaque */
                                        chmap, NULL, NULL, penx, peny);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
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
	   get_CharPos( chmap, mouseMidX, mouseMidY, fh+fgap, &pch, &pos);
	}

}

/*-----------------------------------------------------------------
To get pos and pch of the pointed char from FTCHAR map.

@chmap:		The FTCHAR map.
@x,y:		A B/LCD coordinate pointed to a char.
@lndis:		Distanche between two char lines.
@pch:		A pointer to char index in chmap, in chars.
@pos:		A pointer to char postion in chmap, in bytes.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------*/
static int get_CharPos( const EGI_FTCHAR_MAP *chmap, int x, int y,  int lndis, int *pch, int *pos)
{
	int i;

	if( chmap==NULL || chmap->count < 1 ) {
		printf("%s: Input FTCHAR map is empty!\n", __func__);
		return -1;
	}

	/* Search char map to find pch/pos for the x,y  */
        for(i=0; i < chmap->count; i++) {
        	//printf("i=%d/(%d-1)\n", i,chmap->count);
                if( chmap->charY[i] < y && chmap->charY[i]+lndis > y ) {
                        if( chmap->charX[i] <= x		/*  == ALSO is the case ! */
			    && (     i==chmap->count-1		/* Last char/insert */
				 || ( chmap->charX[i+1] > x || chmap->charY[i] != chmap->charY[i+1] )  /* || or END of the line */
				)
			   )
                        {
				if(pch!=NULL)
                                	*pch=i;
				if(pos!=NULL)
                                	*pos=chmap->charPos[i];
                        }
		}
	}

	return 0;
}


/*--------------------------------
 Draw cursor for the mouse movement
1. In txtbox area, use txtcursor.
2. Otherwise, apply mcursor.
----------------------------------*/
static void draw_mcursor(int x, int y)
{
        static EGI_IMGBUF *mcimg=NULL;
        static EGI_IMGBUF *tcimg=NULL;
	EGI_POINT pt;
        if(mcimg==NULL)
                mcimg=egi_imgbuf_readfile("/mmc/mcursor.png");
	if(tcimg==NULL) {
		tcimg=egi_imgbuf_readfile("/mmc/txtcursor.png");
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

