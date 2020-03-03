/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

1. Display wbook through a FB with multiple buffer_pages, and test scrolling
   up and down by touch.

2. FB back buffer is enabled.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_pcm.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"


static char *fp;		/* mmap to txt */

EGI_FILO  *filo_pgOffset;
static unsigned long round_count;
static bool IsScrollUp=true; /* Scroll_Up: to increase page number, Scroll_Dow: to decrease page number */

/* --- Functions --- */
static void * create_pgoffset_table(void *arg);
void writeTxt_to_buffPage(int nbuf, char *pstr, int npg);


/* ---  Paramers for txt_block fonts and typesetting  --- */
static int fw=18, fh=18; 			 /* Fonts width and height */
static int x0=10, y0=10; 			 /* Offset for txt block */
static int margin=20;				 /* x0 + right margin, */
static int lngap=4;				 /* gap between lines, in pixel */
static int pixpl;      //=xres-margin;		 /* Pixels per line of txt block */
static int lines;     //=(yres-10*2)/(fh+lngap); /* lines in the txt block */
static int pixlen;





///////////////////	MAIN    ///////////////////////
int main(int argc, char** argv)
{
	int i,j,k;
	int ret;

	struct timeval tm_start;
	struct timeval tm_end;

EGI_IMGBUF* eimg=NULL;

int 		fd;
int 		fsize;
struct stat 	sb;
unsigned int	txt_pgnum;

int nret=0;
int mark;
unsigned long int 	off=0; 		/* in bytes, offset */
unsigned long int 	xres, yres;	/* X,Y pixles of the FB/Screen */
unsigned long int 	line_length; 	/* in bytes */
static EGI_TOUCH_DATA	touch_data;

int total=0;

pthread_t	thread_create_table;


        /* <<<<<  EGI general init  >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();		   	/* start sys tick */
#if 0
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_test") != 0) {	/* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
#endif

        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {   	/* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        if(FTsymbol_load_appfonts() !=0 ) {  	/* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )		/* init sys FB */
                return -1;

        printf("start touchread thread...\n");
	egi_start_touchread();			/* start touch read thread */

	/* <<<<<  End EGI Init  >>>>> */


	/* Get FB params */
	xres=gv_fb_dev.vinfo.xres;
	yres=gv_fb_dev.vinfo.yres;
	line_length=gv_fb_dev.finfo.line_length;


	/* Set txt block params */
	pixpl=xres-margin;		 /* Pixels per line of txt block */
	lines=(yres-margin)/(fh+lngap); /* lines in the txt block */


#if 0
        if(argc<2) {
                printf("Usage: %s file\n",argv[0]);
                exit(-1);
        }

        /* 1. Load pic to imgbuf */
	eimg=egi_imgbuf_readfile(argv[1]);
	if(eimg==NULL) {
        	EGI_PLOG(LOGLV_ERROR, "%s: Fail to read and load file '%s'!", __func__, argv[1]);
		return -1;
	}
#endif

        /* --- Open wchar book file, and mmap it. --- */
        fd=open("/mmc/xyj_uft8.txt",O_RDONLY);
        if(fd<0) {
                perror("open file");
                return -1;
        }
        /* obtain file stat */
        if( fstat(fd,&sb)<0 ) {
                perror("fstat");
                return -2;
        }
        fsize=sb.st_size;
        /* mmap txt file */
        fp=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
        if(fp==MAP_FAILED) {
                perror("mmap");
                return -3;
        }

	/* --- Start pthread to create page offset table for the wbook --- */
	if(pthread_create(&thread_create_table,NULL, &create_pgoffset_table, NULL) != 0) {
		printf("Fail to create pthread_create_table!\n");
	}
	tm_delayms(500);


#if 0 /*  ------	TEST OFFSET TABLE   ------  */
	for(i=0;;i++)
	{
		/* Check if it gets the end */
		total=egi_filo_itemtotal(filo_pgOffset);
		if(i > total-1 ) {
			printf("Finish displaying wbook. totally %d pages. \n", total );
			round_count++;
			tm_delayms(1000);
			i=0-1;
			continue;
		}
		printf("Display round %ld, Page %d / %d. \n", round_count,i, total);

	        //fb_shift_buffPage(&gv_fb_dev,0); /* switch to FB back buffer page 0 */
		fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_LTYELLOW);
		egi_filo_read(filo_pgOffset,i,&off); /* read filo to get page offset value */
		writeTxt_to_buffPage(0, fp+off, i); /* (nbuf, *pstr, npg) */

		/* Refresh FB page */
		fb_page_refresh(&gv_fb_dev);
		tm_delayms(500);
	}
#endif




do {    ////////////////////////////    LOOP TEST   /////////////////////////////////

	off=0;

#if 0 /* --- write imgbuf to back buffers --- */
	eimg=egi_imgbuf_readfile("/mmc/list.png");
	if(eimg==NULL) {
        	EGI_PLOG(LOGLV_ERROR, "%s: Fail to read and load file '%s'!", __func__, "/mmc/list.png");
		return -1;
	}
	memcpy(gv_fb_dev.map_bk, eimg->imgbuf, FBDEV_BUFFER_PAGES*yres*line_length);
	egi_imgbuf_free(eimg); eimg=NULL;
#endif


	/* Write TXT to FB back buffers to fill all buffer pages */
	for(i=0; i<FBDEV_BUFFER_PAGES; i++)
	{
		/* Check if it gets the end */
		total=egi_filo_itemtotal(filo_pgOffset);
		if(i > total-1 ) {
			printf("Finish displaying wbook. totally %d pages. \n", total );
			break;
		}

	        fb_shift_buffPage(&gv_fb_dev, i); 	/* switch to FB back buffer page i */
		fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_LTYELLOW);
		egi_filo_read(filo_pgOffset, i, &off); 	/* read filo to get page offset value */
		writeTxt_to_buffPage(i, fp+off, i); 	/* (nbuf, *pstr, npg) */
	}


	/* Refresh FB to show first buff page */
	fb_shift_buffPage(&gv_fb_dev,0);
	fb_page_refresh(&gv_fb_dev);


	/* ---- Scrolling up/down  PAGEs by touch sliding ---- */

	i=0; 	  /* Line index of all FB buffer pages */
	mark=0;
	while(1)
      	{
		/*  Get touch data  */
		if(egi_touch_getdata(&touch_data)==false) {
			tm_delayms(10);
			continue;
		}

		/* switch touch status */
		switch(touch_data.status) {
			case released_hold:
				continue;
				break;
			case pressing:
	                        printf("pressing i=%d\n",i);
                        	mark=i;
                        	continue;
				break;
			case releasing:
				printf("releasing\n");
				continue;
				break;

			case pressed_hold:
				break;

			default:
				continue;
		}

                /* Update line index of FB back buffer */
                i=mark-touch_data.dy;
                printf("i=%d\n",i);

                /* Normalize 'i' to: [0  yres*FBDEV_BUFFER_PAGES) */
                if(i < 0 ) {
                        printf("i%%(yres*FBDEV_MAX_BUFF)=%d\n", i%(int)(yres*FBDEV_BUFFER_PAGES));
                        i=(i%(int)(yres*FBDEV_BUFFER_PAGES))+yres*FBDEV_BUFFER_PAGES;
                        printf("renew i=%d\n", i);
                        mark=i+touch_data.dy;
                }
                else if (i > yres*FBDEV_BUFFER_PAGES-1) {
                        i=0;    /* loop back to page 0 */
                        mark=touch_data.dy;
                        //continue;
                }

                /*  Refresh FB with offset line, now 'i' limits to [0  yres*FBDEV_BUFFER_PAGES) */
                fb_slide_refresh(&gv_fb_dev, i);

		tm_delayms(5);
		//usleep(200000); //500);
      	}


} while(1); ///////////////////////////   END LOOP TEST   ///////////////////////////////



#if 0	/* Free eimg */
	egi_imgbuf_free(eimg);
	eimg=NULL;
#endif
	/* close file and munmap */
	if(fp!=MAP_FAILED && fp!=NULL )
		munmap(fp,fsize);
	close(fd);

	egi_free_filo(filo_pgOffset);

	#if 0
        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
	printf9"egi_end_touchread()...\n");
	egi_end_touchread();
        printf("egi_quit_log()...\n");
        egi_quit_log();
        printf("<-------  END  ------>\n");
	#endif

return 0;
}



/*--------------------------------------------
Thread function:
Create page offset table for wbook.
---------------------------------------------*/
static void * create_pgoffset_table(void *arg)
{
	int nret=0;
	unsigned long off=0;
	EGI_FONTS   fonts={.ftname="tmp_fonts",};

	/* As a detached thread */
	pthread_detach(pthread_self());

	/* Create an EGI_FILO to hold the table */
	filo_pgOffset=egi_malloc_filo(1<<4, sizeof(unsigned long), FILO_AUTO_DOUBLE);
	if(filo_pgOffset==NULL) {
		printf("%s: Fail to create filo_pgOffset!\n",__func__);
		return (void *)-1;
	}

	/* To create a FT_Face just same as egi_appfonts.bold! */
	//fonts.bold=FTsymbol_create_newFace(&fonts, "/mmc/fonts/hanserif/SourceHanSerifSC-Bold.otf");// egi_appfonts.fpath_bold);
	fonts.regular=FTsymbol_create_newFace(&fonts, "/mmc/fonts/hanserif/SourceHanSerifSC-Regular.otf");
	if(fonts.regular==NULL) {
		printf("%s: Fail to create new face!\n",__func__);
		return (void *)-1;
	}
	printf("Temp. fonts created!\n");

   /* create page offset table */
   off=0;
   egi_filo_push(filo_pgOffset, &off); /* push first page 0 */

   /* Calculate wbook page index(offset) and Push it to FILO */
   while(1) {
	/* To the NULL fbdev, Use the same params for displaying fbdev. */
	nret=FTsymbol_uft8strings_writeFB(NULL, fonts.regular,      	 /* FBdev, fontface */
      	                          fw, fh, (const unsigned char *)fp+off, /* fw,fh, pstr */
                        	          pixpl, lines, lngap,     	 /* pixpl, lines, gap */
                                          x0, y0,                        /* x0,y0, */
                                     	  WEGI_COLOR_BLACK, -1, -1,     /* fontcolor, transcolor,opaque */
					  NULL, NULL, NULL, NULL);
	if(nret<=0)
		break;

	off += nret;
	egi_filo_push(filo_pgOffset, &off);
   }
	printf("Filo item number: %d\n", egi_filo_itemtotal(filo_pgOffset));

	/* Release the temp. fonts */
	FTsymbol_release_library(&fonts);

	pthread_exit((void *)0);
}


/*---------------------------------------------------------
Write txt to a FB back buffer page, and put page number at
the bottom.

@nbuf:	 Number of FB back buffer page.
@pstr:   Pointer to the txt.
@num:	 Txt page number

----------------------------------------------------------*/
void writeTxt_to_buffPage(int nbuf, char *pstr, int npg)
{
	int  ret;
	char strPgNum[16];
	unsigned int xres=gv_fb_dev.vinfo.xres;
	unsigned int yres=gv_fb_dev.vinfo.yres;

	/* Shift to current working FB back buffer  */
        fb_shift_buffPage(&gv_fb_dev,nbuf);
	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_LTYELLOW);

	/* Write pstr to FB back buffer page */
	ret=FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.regular,     /* FBdev, fontface */
               	                          fw, fh, (const unsigned char *)pstr,  /* fw,fh, pstr */
                       	                  pixpl, lines, lngap,     		/* pixpl, lines, gap */
                               	          x0, y0,                             	/* x0,y0, */
                                  	  WEGI_COLOR_BLACK, -1, -1,    /* fontcolor, transcolor,opaque */
					  NULL, NULL, NULL, NULL);
	if(ret<=0)
		return;

	/* draw a line at bottom of each page */
	//fbset_color(WEGI_COLOR_BLACK);
	//draw_line(&gv_fb_dev,5,yres-6, xres-5, yres-6);

	/* Put page number at bottom of page */
	snprintf(strPgNum,sizeof(strPgNum)-1,"- %d -", npg);
	pixlen=FTsymbol_uft8strings_pixlen(egi_appfonts.bold, 16, 16, (const unsigned char *)strPgNum);
        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.bold,     /* FBdev, fontface */
                                 16, 16, (const unsigned char *)strPgNum,      /* fw,fh, pstr */
                                     pixpl, 1, 0,     			/* pixpl, lines, gap */
                                     (xres-pixlen)/2, yres-20,    	/* x0,y0, */
                                     WEGI_COLOR_BLACK, -1, -1,     /* fontcolor, transcolor,opaque */
				     NULL, NULL, NULL, NULL);

}
