 /*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to read input from termios and put it on to the screen.


Midas Zhou
-------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>   /* open */
#include <unistd.h>  /* read */
#include <errno.h>
#include <linux/input.h>
#include <termios.h>
#include "egi_common.h"
#include "egi_input.h"
#include "egi_FTsymbol.h"

static EGI_BOX txtbox={ { 10, 30 }, {320-1-10, 240-1-10} };
static char txtbuff[1024];
static int  charX[1024*2];
static EGI_FTCHAR_MAP *chmap;

static int mouseX, mouseY, mouseZ; /* Mouse position */

static int fw=18;	/* Font size */
static int fh=20;
static int fgap=20/5;	/* Text line fgap : TRICKY ^_- */
static int tlns;  //=(txtbox.endxy.y-txtbox.startxy.y)/(fh+fgap); /* Total available txt lines */

static int penx;	/* Pen position */
static int peny;

static void write_txt(char *txt, int px, int py, EGI_16BIT_COLOR color);
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
	char chs[5];
	char ch;

	int  k;
	int  i;
	fd_set rfds;
	int retval;
	struct timeval tmval;
	bool	cursor_blink=false;
	struct timeval tm_blink;
	struct timeval tm_now;


	/* Init. txt and charmap */
	strcat(txtbuff,"歪朵拉的惊奇\nwidora_NEO\n   --- hell world ---\n\n");
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
	write_txt("EGI Editor",120,5,WEGI_COLOR_LTBLUE);
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

	k=cstr_strcount_uft8((const unsigned char *)txtbuff);
	printf("Total %d chars\n", k);
	gettimeofday(&tm_blink,NULL);

	/* ---TEST: EGI_FTCHAR_MAP */
       	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
	write_txt(txtbuff, txtbox.startxy.x, txtbox.startxy.y, WEGI_COLOR_BLACK);
	fbset_color(WEGI_COLOR_RED);
	for(i=0; i<k; i++) {
		printf("k=%d\n",i);
		//penx=charXY[2*i]; peny=charXY[2*i+1];
		penx=chmap->charX[i]; peny=chmap->charY[i];
		draw_filled_rect(&gv_fb_dev, penx, peny, penx+1, peny+fh+fgap-1);
		fb_render(&gv_fb_dev);
		tm_delayms(500);
	}
	exit(0);

	while(1) {
		ch=0;

		read(STDIN_FILENO, &ch, 1);
		if(ch==27) {
			ch=0;
			read(STDIN_FILENO,&ch, 1);
			if(ch==91) {
				ch=0;
				read(STDIN_FILENO,&ch, 1);
				switch ( ch ) {
					case 65:
						printf("UP\n"); break;
					case 66:
						printf("DOWN\n");break;
					case 67:
						printf("RIGHT\n"); break;
					case 68:
						printf("LEFT\n");break;
				}
			}

			/* NOT char */
			ch=0;
		}
		if(ch>0)
			printf("ch=%d\n",ch);

		if( ch==0x7F && k>0 ) { /* backspace */
			//printf("backspace\n");
			txtbuff[--k]=0;
		}
		else if(ch>0)
			txtbuff[k++]=ch;

		/* Display txt */
        	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
		write_txt(txtbuff, txtbox.startxy.x, txtbox.startxy.y, WEGI_COLOR_BLACK);

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
static void write_txt(char *txt, int px, int py, EGI_16BIT_COLOR color)
{
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, //special, //bold,          /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,    	/* fw,fh, pstr */
                                        320-40, tlns, fgap,      /* pixpl, lines, fgap */
                                        px, py,                                 /* x0,y0, */
                                        color, -1, 255,      			/* fontcolor, transcolor,opaque */
                                        chmap, NULL, NULL, &penx, &peny);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}


/*--------------------------------------------
        Callback for mouse input
Just update mouseXYZ
---------------------------------------------*/
static void mouse_callback(unsigned char *mouse_data, int size)
{
        /* 1. Check pressed key */
        if(mouse_data[0]&0x1)
                printf("Leftkey down!\n");

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

