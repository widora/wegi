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


static void write_txt(char *txt, int px, int py, EGI_16BIT_COLOR color);
static int penx;
static int peny;

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

 /* <<<<<  End of EGI general init  >>>>>> */



                  /*-----------------------------------
                   *            Main Program
                   -----------------------------------*/

        struct termios old_settings;
        struct termios new_settings;

	int term_fd;
	char chs[5];
	char ch;
	char buff[1024]={0};
	int  k;
	fd_set rfds;
	int retval;
	struct timeval tmval;
	bool	cursor_blink=false;


        /* Init. mouse position */
//        mouseX=gv_fb_dev.pos_xres/2;
//        mouseY=gv_fb_dev.pos_yres/2;

        /* Init. FB working buffer */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY4);
	fbset_color(WEGI_COLOR_WHITE);
	draw_filled_rect(&gv_fb_dev, 10, 30, 320-1-10,240-1-10);
	write_txt("EGI Editor",120,5,WEGI_COLOR_LTBLUE);
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

	k=0;
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
		//printf("ch=%d\n",ch);

		if( ch==0x7F && k>0 ) { /* backspace */
			printf("backspace\n");
			buff[--k]=0;
		}
		else if(ch>0)
			buff[k++]=ch;

		/* Display txt */
        	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
		write_txt(buff, 20, 40, WEGI_COLOR_BLACK);

		/* Cursor blink */
		cursor_blink = !cursor_blink;
		if(cursor_blink)
			write_txt("|",penx,peny,WEGI_COLOR_RED);
		if(ch<=0)
			tm_delayms(500);

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
			memset(buff,0,sizeof(buff));
                        retval=read(term_fd, buff, sizeof(buff));
			printf("buff:%s",buff);
		}
	}


                  /*-----------------------------------
                   *         End of Main Program
                   -----------------------------------*/

	/* Reset termio */
        tcsetattr(0, TCSANOW,&old_settings);

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
        Write TXT
@txt:   Input text
@px,py: LCD X/Y for start point.
-------------------------------*/
static void write_txt(char *txt, int px, int py, EGI_16BIT_COLOR color)
{
	int fw=18;
	int fh=20;
	int gap=2;

       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,    /* fw,fh, pstr */
                                        320-40, (240-40-10)/(fh+gap), gap,                                  /* pixpl, lines, gap */
                                        px, py,                                    /* x0,y0, */
                                        color, -1, 255,       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, &penx, &peny);      /* int *cnt, int *lnleft, int* penx, int* peny */
}


