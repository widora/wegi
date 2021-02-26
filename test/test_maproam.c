/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

To display a png/jpg file on the LCD.

Usage:
	maproam  map_file(jpg/png)

Control key:
	'a' 'A'		turn left
	'd' 'D'		turn right
	'w' 'W'		go ahead
	's' 'S'		go backward
	'z'			zoom up
	'n'			zoom down
	SPACE			to display next picture.
	'q'			quit


TODO: To move image totally out of screen, forth and back, ...then
      the bug appears.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_input.h"

char imd_getchar(void);


int main(int argc, char** argv)
{
	int i,j,k;
	int xres;
	int yres;
	int xp,yp;
	int angle_step=1;	/* angular step */
	int step=10;		/* line step */
	int bigStep=120;
	int Sh,Sw;	/* Show_height, Show_width */
	/* percentage step  1/4 */
	EGI_IMGBUF* eimg=NULL;

	int 	opt;
	bool 	PortraitMode=false;
	bool	TranspMode=false;
	bool	DirectFB_ON=false;

	char	cmdchar=0;		/* A char as for command */
	int	byte_deep=0;

	EGI_IMGBUF *pad=NULL;
	EGI_IMGBUF *compassImg=NULL;
	int compassDiam;  			/* cmopassImg max side length */
	EGI_IMGBUF *roamImg=NULL;	/* roaming img  */

	int	heading=0;		/* heading angle, roamImg -Y to image -Y direction
					 * Compass initial:  map image_X as EAST, image_Y as SOUTH
					 */

	EGI_POINT	points[3];	/* for a small triangle mark */

        /* parse input option */
        while( (opt=getopt(argc,argv,"htdp"))!=-1)
        {
                switch(opt)
                {
                       case 'h':
                           printf("usage:  %s [-h] [-p] [-t] fpath \n", argv[0]);
                           printf("         -h   help \n");
			   printf("         -t   Transparency on\n");
                           printf("         -p   Portrait mode. ( default is Landscape mode ) \n");
                           printf("fpath    file path \n");
                           return 0;
                       case 'p':
                           printf(" Set PortraitMode=true.\n");
                           PortraitMode=true;
                           break;
			case 't':
			   printf(" Set TranspMode=true.\n");
			   TranspMode=true;
			   break;
                       default:
                           break;
                }
        }

        /* Check input files  */
        //printf(" optind=%d, argv[%d]=%s\n", optind, optind, argv[optind] );
	if( optind < argc ) {
		i=optind;
		printf("Follow files found:\n");
		while( i<argc)
			printf("%s\n",argv[i++]);
	} else {
		printf("No input file!\n");
		exit(1);
	}

        /* Load freetype fonts */
        printf("FTsymbol_load_sysfonts()...\n");
        if(FTsymbol_load_sysfonts() !=0) {
                printf("Fail to load FT sysfonts, quit.\n");
                return -1;
        }

        /* Init sys FBDEV  */
        if( init_fbdev(&gv_fb_dev) )
                return -1;

        /* Set FB position mode: LANDSCAPE  or PORTRAIT */
	#ifdef LETS_NOTE
        if(PortraitMode)
                fb_position_rotate(&gv_fb_dev,1);
        else
                fb_position_rotate(&gv_fb_dev,0);
	#else
        if(PortraitMode)
                fb_position_rotate(&gv_fb_dev,0);
        else
                fb_position_rotate(&gv_fb_dev, 3);
	#endif

        xres=gv_fb_dev.pos_xres;
        yres=gv_fb_dev.pos_yres;

	/* create imgbuf for compass */
	#ifdef LETS_NOTE
	compassImg=egi_imgbuf_readfile("/home/midas-zhou/egi/compass.png");
	#else
	compassImg=egi_imgbuf_readfile("/mmc/compass.png");
	#endif

	compassDiam=compassImg->width > compassImg->height ? compassImg->width : compassImg->height;

	pad=egi_imgbuf_create(compassDiam,compassDiam, 180, WEGI_COLOR_LTGREEN); //180, WEGI_COLOR_WHITE);
	egi_imgbuf_fadeOutCircle( pad, 255, compassDiam/2, 0, 0 ); /* eimg, max_alpha, rad, width, itype */

        /* create imgbuf to hold roaming block image */
	roamImg=egi_imgbuf_create(yres|0x1, xres|0x1, 255, WEGI_COLOR_GRAY);
	if(roamImg==NULL)exit(-1);

        /* set FB buffer mode: Direct(no buffer) or Buffer */
        if(DirectFB_ON) {
            gv_fb_dev.map_bk=gv_fb_dev.map_fb; /* Direct map_bk to map_fb */

        } else {
            /* init FB back ground buffer page */
            memcpy(gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.map_fb, gv_fb_dev.screensize);

            /*  init FB working buffer page */
            //fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_BLUE);
            memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_fb, gv_fb_dev.screensize);
        }

	/* ---------- Load image files ----------------- */
for( i=optind; i<argc; i++) {

        /* 1. Load pic to imgbuf */
	eimg=egi_imgbuf_readfile(argv[i]);
	if(eimg==NULL) {
        	printf("Fail to read and load file '%s'!", argv[i]);
		continue;
	}

	/* rotate the imgbuf */

        /* 2. resize the imgbuf  */

        /* 3. Adjust Luminance */
        // egi_imgbuf_avgLuma(eimg, 255/3);

	/* 4. 	------------- Displaying  image ------------ */

	xp=eimg->width/2;
	yp=eimg->height/2;
  do {
		printf("cmdchar=%c\n",cmdchar);
	switch(cmdchar)
	{
		/* ---------------- Parse 'q' ------------------- */
		case 'q':
			goto END_DISPLAY;

#if 0
		/* ---------------- Parse 'z' and 'n' ------------------- */
		case 'z':	/* zoom up */
			xp = (xp+xres/2)*5/4 - xres/2;	/* keep focus on center of LCD */
			yp = (yp+yres/2)*5/4 - yres/2;
 			Sh = Sh*5/4;
			Sw = Sw*5/4;
			break;
		case 'n':	/* zoom down */
			xp = (xp+xres/2)*3/4 - xres/2;	/* keep focus on center of LCD */
			yp = (yp+yres/2)*3/4 - yres/2;
			Sh = Sh*3/4;
			Sw = Sw*3/4;
			tmpimg=egi_imgbuf_resize(eimg, Sw, Sh);
			break;

		/* ---------------- Parse arrow keys -------------------- */
		case '\033':
			byte_deep=1;
			break;
		case '[':
			if(byte_deep!=1) {
				byte_deep=0;
				break;
			}
			byte_deep=2;
			break;
		case 'D':		/* <--- */
			if(byte_deep!=2) {
				byte_deep=0;
				break;
			}
                        xp-=step;
                       //if(xp<0)xp=0;

			byte_deep=0;
                        break;
		case 'C':		/* ---> */
			if(byte_deep!=2) {
				byte_deep=0;
				break;
			}
			xp+=step;
			//if(xp > tmpimg->width-xres )
			//	xp=tmpimg->width-xres>0 ? tmpimg->width-xres:0;

			byte_deep=0;
                        break;
		case 'A':		   /* UP ARROW */
			if(byte_deep!=2) {
				byte_deep=0;
				break;
			}
			yp-=step;
			//if(yp<0)yp=0;

			byte_deep=0;
			break;
		case 'B':		   /* DOWN ARROW */
			if(byte_deep!=2)
				break;
			yp+=step;
			//if(yp > tmpimg->height-yres )
			//	yp=tmpimg->height-yres>0 ? tmpimg->height-yres:0;

			byte_deep=0;
			break;
#endif

		/* ---------------- Parse Key 'a' 'd' 'w' 's' -------------------- */
		case 'a':
			heading-=angle_step;
			break;
		case 'A':  /* big turning angle */
			heading-=angle_step<<2;
			break;

		case 'd':
			heading+=angle_step;
			break;
		case 'D':
			heading+=angle_step<<2;
			break;


		case 'w':  /* go forward */
			xp+=step*sin(MATH_PI*heading/180);
			yp-=step*cos(MATH_PI*heading/180);
			break;

		case 'W':  /* big step */
			xp+=bigStep*sin(MATH_PI*heading/180);
			yp-=bigStep*cos(MATH_PI*heading/180);
			break;

		case 's': /* go backward */
			xp-=step*sin(MATH_PI*heading/180);
			yp+=step*cos(MATH_PI*heading/180);
			break;
		case 'S': /* go backward */
			xp-=bigStep*sin(MATH_PI*heading/180);
			yp+=bigStep*cos(MATH_PI*heading/180);
			break;

		default:
//			printf("cmdchar: %d\n", cmdchar);
			byte_deep=0;
//			continue;
			break;
	}

	 if(!TranspMode)
	 	fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_GRAY); /* for transparent picture */

	 /* Draw 1: rotCopy raomImg and display  */
	 egi_imgbuf_rotBlockCopy(eimg, roamImg, roamImg->height, roamImg->width, xp, yp, heading); /* img, oimg, height, width, px,py, angle */
       	 egi_imgbuf_windisplay( roamImg, &gv_fb_dev, -1,
         	                0, 0, 0, 0,
               		        xres, yres ); //tmpimg->width, tmpimg->height);

	 /* Draw 2: compass */
	 egi_subimg_writeFB( pad, &gv_fb_dev, 0, -1, xres-pad->width, 0); /* imgbuf, fb_dev, subnum, subcolor, x0,y0 */
	 egi_image_rotdisplay( compassImg, &gv_fb_dev, -heading,  		/* egi_imgbuf, FBDEV *fb_dev, int angle */
                               compassImg->width/2, compassImg->height/2,  	/* int xri, int yri, */
	                       xres-compassDiam/2, compassDiam/2); /* int xrl, int yrl */
	 fbset_color(WEGI_COLOR_RED);
	 draw_wline(&gv_fb_dev, xres-compassDiam/2, compassImg->height/2-30, xres-compassDiam/2, compassImg->height/2+20, 3);
	 /* a triangle arrow mark */
	 points[0]=(EGI_POINT){ xres-compassDiam/2,compassImg->height/2-30 };
	 points[1]=(EGI_POINT){ xres-compassDiam/2-5,compassImg->height/2-30+15 };
	 points[2]=(EGI_POINT){ xres-compassDiam/2+5,compassImg->height/2-30+15 };
	 draw_filled_triangle(&gv_fb_dev, points);

	 /* Draw 3: position mark */
	 fbset_color(WEGI_COLOR_BLACK);
	 draw_line(&gv_fb_dev, 0, yres/2, xres-1, yres/2); //120, 320-1, 120);
	 draw_line(&gv_fb_dev, xres/2,0,xres/2, yres-1); //160, 0, 160, 240-1);
	 fbset_color(WEGI_COLOR_RED);
	 draw_filled_circle(&gv_fb_dev, xres/2, yres/2, 6); //320/2, 240/2, 6);


	 #ifdef LETS_NOTE
         FTsymbol_uft8strings_writeFB(  &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        40, 40,(const unsigned char *)"EGI: 游历地图",    /* fw,fh, pstr */
                                        gv_fb_dev.vinfo.xres, 1, 0,                                  /* pixpl, lines, gap */
                                        50,  50,//gv_fb_dev.vinfo.yres/2+250,             /* x0,y0, */
                                        WEGI_COLOR_BLUE, -1, 200,    /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	#else
         FTsymbol_uft8strings_writeFB(  &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        30, 30,(const unsigned char *)"EGI: 游历地图",    /* fw,fh, pstr */
                                        gv_fb_dev.vinfo.xres, 1, 0,                                  /* pixpl, lines, gap */
                                        5,  240-35,//gv_fb_dev.vinfo.yres/2+250,             /* x0,y0, */
                                        WEGI_COLOR_BLUE, -1, 70,    /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	#endif

        /* 4.1 Refresh FB by memcpying back buffer to FB */
	fb_render(&gv_fb_dev);
        //fb_page_refresh(&gv_fb_dev,0);

  } while( (cmdchar=imd_getchar()) != ' ' ); /* input SPACE to end displaying current image */

         fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_BLACK);

	/* 5. Free tmpimg */
	egi_imgbuf_free2(&eimg);
	egi_imgbuf_free2(&roamImg);


} /* End displaying all image files */


END_DISPLAY:
        /* <<<<<  EGI general release >>>>> */
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);

return 0;
}


/*----------------------------------------------------------------------------------------
Immediatly get a char from terminal input without delay.

1. struct termios main members:
           tcflag_t c_iflag;       input modes
           tcflag_t c_oflag;       output modes
           tcflag_t c_cflag;       control modes
           tcflag_t c_lflag;       local modes
           cc_t     c_cc[NCCS];    special characters

2. tcflag_t c_lflag values:
       ... ...
       ICANON Enable canonical mode
       ECHO   Echo input characters.
       ECHOE  If ICANON is also set, the ERASE character erases the preceding input character, and WERASE erases the preceding word.
       ECHOK  If ICANON is also set, the KILL character erases the current line.
       ECHONL If ICANON is also set, echo the NL character even if ECHO is not set.

       ... ...

3. " In noncanonical mode input is available immediately (without the user having to type a
     line-delimiter character), no input processing is performed, and line editing is disabled.
     The settings of MIN (c_cc[VMIN]) and TIME (c_cc[VTIME]) determine the circumstances in which
     which a read(2) completes; there are four distinct cases:
     		VTIME  Timeout in deciseconds for noncanonical read (TIME).
		VMIN   Minimum number of characters for noncanonical read (MIN).
		There are four cases:
	  	MIN == 0, TIME == 0 (polling read)
		MIN > 0, TIME == 0  (blocking read)
		MIN == 0, TIME > 0  (read with timeout)
        	      TIME  specifies  the  limit for a timer in tenths of a second.
		MIN > 0, TIME > 0  (read with interbyte timeout)
        	      TIME specifies the limit for a timer in tenths of a second.  Once an initial byte
		      of input becomes available, the timer is restarted after each further byte is received.
   " --- man tcgetattr / Linux Programmer's Manual

----------------------------------------------------------------------------------------------*/
char imd_getchar(void)
{
	char c;
#if 0
	struct termios old_settings;
	struct termios new_settings;


	tcgetattr(0, &old_settings);
	new_settings=old_settings;
	new_settings.c_lflag &= (~ICANON);      /* disable canonical mode, no buffer */
	new_settings.c_lflag &= (~ECHO);   	/* disable echo */
	new_settings.c_cc[VMIN]=0; //1;
	new_settings.c_cc[VTIME]=0;
	tcsetattr(0, TCSANOW, &new_settings);
#endif
	egi_set_termios();
	c=0;
	read(STDIN_FILENO,&c, 1);
	printf("input c=%d\n",c);

	egi_reset_termios();

//	tcsetattr(0, TCSANOW, &old_settings);

	return c;
}
