/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A fake login page just for testing FTcharmap functions.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>   	/* Open */
#include <unistd.h>  	/* Read */
#include <errno.h>
#include <ctype.h>
#include <linux/input.h>
#include <termios.h>
#include "egi_common.h"
#include "egi_input.h"
#include "egi_FTcharmap.h"
#include "egi_FTsymbol.h"
#include "egi_cstring.h"

#define 	PASSWORD_MAX_SIZE	64  /* in bytes, text buffer size for EGI_FTCHAR_MAP.txtbuff */
#define		CHMAP_SIZE		256 /* NOT less than Max. total number of chars (include '\n's and EOF) that may displayed in the txtbox */

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
#define 	CTRL_F  6	/* ASCII: Ctrl + f */

char *strPasswd="Openwrt_Bingo";

/* Charmap displaying area */
//static EGI_BOX txtbox={ { 40, 120 }, {320-1-40, 120+5+20+2 } };	/* Onle line displaying area */
static EGI_BOX txtbox={ { 25, 170 }, {320-1-35, 170+5+20+2 } };	/* Onle line displaying area */
static int smargin=10; 		/* left and right side margin of text area */
static int tmargin=2;		/* top margin of text area */

/* NOTE: char position, in respect of txtbuff index and data_offset: pos=chmap->charPos[pch] */
static EGI_FTCHAR_MAP *chmap;	/* FTcharmap */
//static unsigned int pchoff;
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

static int  FTcharmap_writeFB(FBDEV *fbdev, EGI_16BIT_COLOR color, int *penx, int *peny);
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus);
static void draw_mcursor(int x, int y);


static void login_layout()
{
	EGI_IMGBUF *wallimg=egi_imgbuf_readfile("/mmc/login.jpg");
	if(wallimg==NULL)
		wallimg=egi_imgbuf_readfile("/mmc/login.png");
	if(wallimg==NULL)
		exit(1);

        /* Wallpaper */
        egi_subimg_writeFB(wallimg, &gv_fb_dev, 0, -1, 0, 0); /* subnum,subcolor,x0,y0 */

	/* Outer pad */
   	draw_blend_filled_roundcorner_rect( &gv_fb_dev, txtbox.startxy.x-10, txtbox.startxy.y-40,   /* fbdev, x1, y1, x2, y2, r */
							txtbox.endxy.x+smargin+10, txtbox.endxy.y+35, 10,
							//WEGI_COLOR_GRAY5, 140);  		    /* color, alpha */
							WEGI_COLOR_LTBLUE, 140);  		    /* color, alpha */
							//WEGI_COLOR_GRAY3, 240);  		    /* color, alpha */
	fbset_color(WEGI_COLOR_BLACK);
	draw_roundcorner_wrect(&gv_fb_dev,txtbox.startxy.x-10, txtbox.startxy.y-40, txtbox.endxy.x+smargin+10, txtbox.endxy.y+35,10,1);

	/* Txtbox pad */
	draw_blend_filled_roundcorner_rect(&gv_fb_dev, txtbox.startxy.x, txtbox.startxy.y-5, /*  fbdev, x1, y1, x2, y2, r, color, alpha */
						txtbox.endxy.x+smargin, txtbox.endxy.y+5,5,
						WEGI_COLOR_GRAY5, 160); //WEGI_COLOR_DARKGRAY, 120);
	/* Txtbox frame */
	fbset_color(WEGI_COLOR_WHITE);
	draw_roundcorner_wrect( &gv_fb_dev, txtbox.startxy.x, txtbox.startxy.y-5,     /*  fbdev, x1, y1, x2, y2, r, w */
						txtbox.endxy.x+smargin, txtbox.endxy.y+5,5,1);
}

static void login_hint(void)
{
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold, //regular,  //bold,          /* FBdev, fontface */
                                        18, 18,(const unsigned char *)"Openwrt", //\nPassword:",      /* fw,fh, pstr */
	                                        200, 2, 8,      				/* pixpl, lines, fgap */
                                        txtbox.startxy.x, txtbox.startxy.y-fh-15,  	/* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,                      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}
static bool invalid_passwd;
static void login_errmsg(void)
{
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.special, //regular,  //bold,          /* FBdev, fontface */
                                        17, 18,(const unsigned char *)"Invalid password, Pls retry.",      /* fw,fh, pstr */
                                        300, 1, 8,      				/* pixpl, lines, fgap */
                                        txtbox.startxy.x, txtbox.endxy.y+10,  		/* x0,y0, */
                                        WEGI_COLOR_ORANGE, -1, 255,                      /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}


int main(int argc, char **argv)
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
        return  -1;
  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,false);
  fb_position_rotate(&gv_fb_dev,0);

  /* Set mouse callback function and start mouse readloop */
  egi_start_mouseread("/dev/input/mice", mouse_callback);


 /* <<<<<  End of EGI general init  >>>>>> */



                  /*-----------------------------------
                   *            Main Program
                   -----------------------------------*/


        struct termios old_settings;
        struct termios new_settings;

	int term_fd;
	char ch;
	//char chs[4];
	int  nch;
	fd_set rfds;
	int retval;
	struct timeval tmval;
	EGI_IMGBUF *homepage=egi_imgbuf_readfile("/mmc/homepage.jpg");

	/* Total available lines of space for displaying chars */
	tlns=(txtbox.endxy.y-txtbox.startxy.y+1)/(fh+fgap);
	printf("Blank lines tlns=%d\n", tlns);

	/* Create and init FTcharmap */
	chmap=FTcharmap_create( PASSWORD_MAX_SIZE, txtbox.startxy.x, txtbox.startxy.y,		/* txtsize,  x0, y0  */
		  txtbox.endxy.y-txtbox.startxy.y+1, txtbox.endxy.x-txtbox.startxy.x+1, smargin, tmargin,      /*  height, width, offx, offy */
		  egi_sysfonts.regular, CHMAP_SIZE, tlns, txtbox.endxy.x-txtbox.startxy.x-smargin, fh+fgap,   /* face, mapsize, lines, pixpl, lndis */
		  -1, WEGI_COLOR_WHITE, true, false );  /* bkgcolor, fontcolor, charColorMap_ON, hlmarkColorMap_ON */
	if(chmap==NULL){ printf("Fail to create char map!\n"); exit(0); };
	chmap->bkgcolor=-1;			/* no bkgcolor */
	chmap->markcolor=WEGI_COLOR_GREEN; 	/* Selection mark color */
	chmap->markalpha=180;
	//chmap->maskchar=0x2605;    // 0x2663;solid club //0x2661(2665); empty(solid) heart //0x2605; solid start //0x2103; DEGREE

        /* Init. mouse position */
        mouseX=gv_fb_dev.pos_xres/2;
        mouseY=gv_fb_dev.pos_yres/2;

	/* Draw login page face */
	login_layout();
	login_hint();

	/* Buffer to FB */
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

	/* To fill initial charmap and get penx,peny */
	FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_BLACK, &penx, &peny);
	fb_render(&gv_fb_dev);


	/* Loop editing ...   Read from TTY standard input, without funcion keys on keyboard ...... */
	while(1) {
		tcdrain(0);

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
						/* mkeyboard HOME  move cursor to the first of the line */
						if( ch == 126 ) {
							FTcharmap_goto_lineBegin(chmap);
						}
                                                break;
					case 51: /* DEL */
						read(STDIN_FILENO,&ch, 1);
		                                printf("ch=%d\n",ch);
						if( ch ==126 ) /* Try to delete a char pointed by chmap->pch */
							//FTcharmap_delete_char(chmap);
							FTcharmap_delete_string(chmap);
						break;
					case 52: /* END */
						read(STDIN_FILENO,&ch, 1);
						/* mkeyboard END  move cursor to the first of the line */
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
					case 66:  /* DOWN arrow : move cursor one display_line down */
						FTcharmap_shift_cursor_down(chmap);
						break;
					case 67:  /* RIGHT arrow : move cursor one char right, Limit to the last [pch] */
						FTcharmap_shift_cursor_right(chmap);
						break;
					case 68: /* LEFT arrow : move cursor one char left */
						FTcharmap_shift_cursor_left(chmap);
						break;
					case 70: /* END : move cursor to the end of the return_line */
						FTcharmap_goto_lineEnd(chmap);
						break;

					case 72: /* HOME : move cursor to the begin of the return_line */
						/* move cursor to the first of the line */
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
		else if ( ch==CTRL_F) {		/* Save chmap->txtbuff */
			printf("Saving chmap->txtbuff to a file...");
			if( argc > 1)
				FTcharmap_save_file(argv[1], chmap);
			else
				FTcharmap_save_file("/tmp/hello.txt", chmap);
			ch=0;
		}

		/* 3. edit text */
		/* --- '\n' as Enter / Confirm --- */
		if( ch==10 ) {
			if( strcmp((char *)chmap->txtbuff, strPasswd) == 0 ) {
				invalid_passwd=false;
				printf("Bingo! Welcome! \n");
				system("screen -dmS WAV aplay -M /mmc/philo.wav");
				while(1) {
				        egi_subimg_writeFB(homepage, &gv_fb_dev, 0, -1, 0, 0); /* subnum,subcolor,x0,y0 */
		                	draw_mcursor(mouseMidX, mouseMidY);
		                	fb_render(&gv_fb_dev);
				}
			}
			else {
				invalid_passwd=true;
				printf("Invalid password!\n");
			}

			continue;
		}

		/* --- EDIT:  3.1 Backspace */
		if( ch==0x7F ) { /* backspace */
			FTcharmap_go_backspace( chmap );
			ch=0;
		}

		/* --- EDIT:  3.2 Insert chars, !!! printable chars + '\n' + TAB  !!! */
		else if( ch>31 || ch==9 || ch==10 ) {  //pos>=0 ) {   /* If pos<0, then ... */

			FTcharmap_insert_char( chmap, &ch );
		}  /* END insert ch */


		/* EDIT:  Need to refresh EGI_FTCHAR_MAP after editing !!! .... */

		/* CHARMAP & DISPLAY: Display txt */
        	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */
		FTcharmap_writeFB(&gv_fb_dev, WEGI_COLOR_WHITE, NULL, NULL);
		if(invalid_passwd)
			login_errmsg();

		/* POST:  check chmap->errbits */
		if( chmap->errbits ) {
			printf("WARNING: chmap->errbits=%#x \n",chmap->errbits);
		}

		/* POST: Draw mouse cursor icon */
        	draw_mcursor(mouseMidX, mouseMidY);

		/* Render and bring image to screen */
		fb_render(&gv_fb_dev);
		//tm_delayms(30);
	}


#if 0////////////
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

#endif ////////////////////

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
                                        fw, fh,   			/* fw,fh */
                                        -1, 255,      			/* transcolor,opaque */
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


/*--------------------------------------------
        Callback for mouse input
Just update mouseXYZ
---------------------------------------------*/
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus)
{
	int mdZ;
	bool	start_pch=false;	/* True if mouseLeftKeyDown switch from false to true */

        /* 1. Check pressed key */
        if(mouse_data[0]&0x1) {

		/* Start selecting */
		if(mouseLeftKeyDown==false) {
                	printf("Leftkey press!\n");
			start_pch=true;
			/* reset pch2 = pch */
			FTcharmap_reset_charPos2(chmap);
		}
		else {
			start_pch=false;
		}

		mouseLeftKeyDown=true;
	}
	else {
		/* End selecting */
		if(mouseLeftKeyDown==true) {
			printf("LeftKey release!\n");
		}
		start_pch=false;
		mouseLeftKeyDown=false;
	}

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
	if(mouseLeftKeyDown && start_pch ) {		/* champ->pch and pch2: To mark start of selection */
	   mouseMidY=mouseY+fh/2;
	   mouseMidX=mouseX+fw/2;
	   printf("mouseMidX,Y=%d,%d \n",mouseMidX, mouseMidY);
	   /* continue checking */
	   while( FTcharmap_locate_charPos( chmap, mouseMidX, mouseMidY )!=0 ){ tm_delayms(5);};

	   /* set random mark color */
	 //  FTcharmap_set_markcolor( chmap, egi_color_random(color_medium), 60);

	}
	else if ( mouseLeftKeyDown && !start_pch ) {  /* chmap->pch2: To mark end of selection */
	   mouseMidY=mouseY+fh/2;
	   mouseMidX=mouseX+fw/2;
	   printf("2mouseMidX,Y=%d,%d \n",mouseMidX, mouseMidY);
	   /* Continue checking */
	   while( FTcharmap_locate_charPos2( chmap, mouseMidX, mouseMidY )!=0){ tm_delayms(5); } ;
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
	#define MCURSOR_ICON_PATH 	"/home/midas-zhou/egi/mcursor.png"
	#define TXTCURSOR_ICON_PATH 	"/home/midas-zhou/egi/txtcursor.png"
#else
	#define MCURSOR_ICON_PATH 	"/mmc/mcursor.png"
	#define TXTCURSOR_ICON_PATH 	"/mmc/txtcursor.png"
#endif
        if(mcimg==NULL)
                mcimg=egi_imgbuf_readfile(MCURSOR_ICON_PATH);
	if(tcimg==NULL) {
		tcimg=egi_imgbuf_readfile(TXTCURSOR_ICON_PATH);
		egi_imgbuf_resize_update(&tcimg, true, fw, fh );
	}

	pt.x=mouseX;	pt.y=mouseY;  /* Mid */

        /* OPTION 1: Use EGI_IMGBUF */
	if( point_inbox(&pt, &txtbox) && tcimg!=NULL ) {
                egi_subimg_writeFB(tcimg, &gv_fb_dev, 0, WEGI_COLOR_RED, mouseX, mouseY ); /* subnum,subcolor,x0,y0 */
	}
	else if(mcimg) {
               	egi_subimg_writeFB(mcimg, &gv_fb_dev, 0, -1, mouseX, mouseY ); /* subnum,subcolor,x0,y0 */
	}

        /* OPTION 2: Draw geometry */

}


