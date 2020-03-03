/* -------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

1. Display wbook through a FB with multiple buffer_pages, and test scrolling
   up and down by touch.

2. FB back buffer is enabled.


		-----  FAST SLIDING MOVEMENT PROBLEs -----
pressing i=333
releasing
pressing i=458
releasing
pressing i=566
i=643 -- 5 up to 6 ---
get_txtpg_offset: offset[2924] for txt page[7]
i=649 -- 6 up to 7 ---
get_txtpg_offset: offset[3392] for txt page[8]
i=649 -- 7 up to 8 ---
get_txtpg_offset: offset[3860] for txt page[9]
i=672 -- 8 up to 9 ---
get_txtpg_offset: offset[4328] for txt page[10]
i=688 -- 9 up to 10 --
get_txtpg_offset: offset[4513] for txt page[11]
releasing



Midas Zhou
midaszhou@yahoo.com
---------------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_pcm.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"

static char *fp;			/* mmap to txt */
static unsigned int total_txt_pages;	/* total pages of txt file */

EGI_FILO  *filo_pgOffset;
static unsigned long round_count;
static bool Is_ScrollUp=true; 		/* Scroll_Up: to increase page number,
					 * Scroll_Dow: to decrease page number */
static bool preup=true;			/* Previous scroll up */


/* --- Functions --- */
static void *create_pgOffset_table(void *arg);
static void writeTxt_to_buffPage(int nbufpg, char *pstr, int ntxtpg);
unsigned int get_txtpg_offset(int npg);


/* ---  Paramers for txt_block fonts and typesettings  --- */
static int fw=18, fh=18; 			 /* Fonts width and height */
static int x0=10, y0=10; 			 /* Offset for txt block */
static int margin=20;				 /* x0 + right margin, */
static int lngap=4;				 /* gap between lines, in pixel */
static int pixpl;      //=xres-margin;		 /* Pixels per line of txt block */
static int lines;     //=(yres-10*2)/(fh+lngap); /* lines in the txt block */
static int pixlen;

/* --- Easter Egge --- */
EGI_IMGBUF* eimg=NULL;
static int egg_pgnum=-1;


///////////////////	MAIN    ///////////////////////
int main(int argc, char** argv)
{
	int i,j,k;
	int ret;

	struct timeval tm_start;
	struct timeval tm_end;


int 		fd;
int 		fsize;
struct stat 	sb;


int	cur_txt_pgnum;   /* Current focusing txt page number:
			  * if Is_Scroll_Up, it's the lowest displaying txt page.
			  * if Is_Scroll_Down, it's the uppermost displaying txt page.
			  */
int     cur_buff_pgnum;  /* Current focusing FB buff page number:
			  * if Is_Scroll_Up, it's the lowest displaying FB buff page.
			  * if Is_Scroll_Down, it's the uppermost displaying FB buff page.
			  */
int     prep_txt_pgnum;  /* The number of TXT page which is to be prepared for displaying */
int	prep_buff_pgnum; /* The number FB buff page which is to be prepared for displaying */


int nret=0;
int mark;				/* current line index of FB back buffers at time of touch 'pressing' */
unsigned long int 	off=0; 		/* in bytes, offset */
unsigned long int 	xres, yres;	/* X,Y pixles of the FB/Screen */
unsigned long int 	line_length; 	/* in bytes */
static EGI_TOUCH_DATA	touch_data;
int                     predy;//devy;   /* previous touch_data.dy value */
int			preold;		/* previous index of back buffer */

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
//        fd=open("/mmc/xyj_uft8.txt",O_RDONLY);
        fd=open("/mmc/xyj_uft8_part1.txt",O_RDONLY);
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
	if(pthread_create(&thread_create_table,NULL, &create_pgOffset_table, NULL) != 0) {
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
		writeTxt_to_buffPage(0, fp+off, i); /* (nbufpg, *pstr, ntxtpg) */

		/* Refresh FB page */
		fb_page_refresh(&gv_fb_dev);
		tm_delayms(500);
	}
#endif



#if 1 /* --- write imgbuf to back buffers --- */
	char *fpath="/mmc/zombie1.png";
	eimg=egi_imgbuf_readfile(fpath);
	if(eimg==NULL) {
        	EGI_PLOG(LOGLV_ERROR, "%s: Fail to read and load file '%s'!", __func__, fpath);
		return -1;
	}
//	memcpy(gv_fb_dev.map_bk, eimg->imgbuf, FBDEV_BUFFER_PAGES*yres*line_length);
//	egi_imgbuf_free(eimg); eimg=NULL;
#endif


do {    ////////////////////////////    LOOP TEST   /////////////////////////////////

	off=0;

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
		writeTxt_to_buffPage(i, fp+off, i); 	/* (nbufpg, *pstr, ntxtpg) */
	}

	/* Refresh FB to show first buff page */
	fb_shift_buffPage(&gv_fb_dev,0);
	fb_page_refresh(&gv_fb_dev);


	/* ---- Scrolling up/down  PAGEs by touch sliding ---- */
	for( i=0, mark=0, predy=0,	/* i as line index of all FB buffer pages */
		 cur_txt_pgnum=0, cur_buff_pgnum=0 ; ; )
      	{
		/*  Get touch data  */
		if(egi_touch_getdata(&touch_data)==false) {
			tm_delayms(10);
			continue;
		}

		/* Switch touch status */
		switch(touch_data.status) {
			case released_hold:
				continue;
				break;
			case pressing:
	                        printf("pressing i=%d\n",i);
                        	mark=i;	/* Rest mark and predy */
				predy=0;
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

                /* Check instantanous sliding direction */
                if( touch_data.dy - predy > 0 ) { 	/* Scroll DOWN, sliding in Screen +Y direction */
                        Is_ScrollUp = false;
			printf("Down\n");
                }
                else if ( touch_data.dy - predy < 0 ) {  /* Scroll UP, sliding in Screen -Y direction */
                        Is_ScrollUp = true;
			printf("UP\n");
                }
		else					/* Idel */
			Is_ScrollUp = preup;

               //predy=touch_data.dy;  /* To update at last, we need old value before. */

                /* Update i (line index of all FB back buffers) */
                i=mark-touch_data.dy;  /* DY>0 Scroll DOWN;  DY<0 Scroll UP */
                //printf("i=%d  %s\n", i, Is_ScrollUp==true ? "Up":"Down");

                /* Normalize 'i' to: [0  yres*FBDEV_BUFFER_PAGES) */
		/*** Branching accroding to updated line index i */

		/* IF_1.  From HEAD to TAIL (traverse from the first buffer page to the last buffer page) */
		/* 1. This occurs only when scrolls down
		 * 2. Map and modulo i to [0  yres*FBDEV_BUFFER_PAGES)
		 * 3. As i<0, screen uppermost 0 line changes, new page appears, so we need to update
		 *    pgnums and buffpage!
	 	 */
                if(i < 0 ) {
			/* Stop scrolling down from 0, as gets to the beginning of the txt page */
			if(cur_txt_pgnum==0) {
				printf("Get to the HEAD!\n");
				i=0;
				// continue;  Just go down to call fb_slide_refresh(&gv_fb_dev, i)
			}
			/* If Scroll down from 1 to 0, cur_txt_pgnum is still 1 now!  */
			//else if( !Is_ScrollUp && cur_txt_pgnum==1 ) {
			else if( cur_txt_pgnum==1 ) {
				printf("    i<0: 1 down to 0\n");
				cur_txt_pgnum--;
				cur_buff_pgnum--;
				i=0;
			}
		 	/* As i<0, screen uppermost 0 line changes, new page appears!
			 * So we MUST update pgnums and buffpage here!, Not in IF_3.1
			 */
			else {
				printf("HEAD to TAIL i=%d\n", i);
				/* Loop to the last buffer page */
        	                //printf("i%%(yres*FBDEV_MAX_BUFF)=%d\n", i%(int)(yres*FBDEV_BUFFER_PAGES));
				preold=i;
	                        i=(i%(int)(yres*FBDEV_BUFFER_PAGES))+yres*FBDEV_BUFFER_PAGES;
                        	printf("renew i=%d\n", i);
				mark += i - preold;

				printf(" --- %d down to %d ---\n", cur_txt_pgnum, cur_txt_pgnum-1 );
				cur_txt_pgnum--;

				cur_buff_pgnum--;
				if(cur_buff_pgnum < 0)
					cur_buff_pgnum=FBDEV_BUFFER_PAGES-1;

				/* (Scroll down) Update next buff page
				 * cur_txt_pgnum already decremented.
				 * Only if current txt page is NOT the FIRST page */
				if( cur_txt_pgnum > 0 )  {
					prep_buff_pgnum=cur_buff_pgnum-1;
					if(prep_buff_pgnum < 0)
						prep_buff_pgnum=FBDEV_BUFFER_PAGES-1;

				   /* Update next buff page */
				   writeTxt_to_buffPage( prep_buff_pgnum,
					fp+get_txtpg_offset(cur_txt_pgnum-1), cur_txt_pgnum-1 );
		    	        }
			}
                } /* END IF_1 */

		/* IF_2.  From TAIL to HEAD  (Traverse from the last buffer page to the first buffer page)
		 *	  This occurs only when scrolls up !  */
                else if (i > yres*FBDEV_BUFFER_PAGES-1) {
		    /* 1. For scrolling up, we only update the uppermost line index i,
		     * 2. Let IF_3.2 to check the bottom line [i+yres-1] to see if new page appears,
		     */
			printf("TAIL to HEAD i=%d\n",i);
			/* Loop back to the first buffer page 0 */
			preold=i;
                        i=0;
			mark += i-preold;

			/* --- For scrolling up, let IF_3.2 to check the bottom line if new page appears --- */
			//cur_txt_pgnum++;
			//cur_buff_pgnum++;
			// ... ...

                } /* END IF_2 */

		/* IF_3.  Does NOT cross FB buffer HEAD/TAIL boundary
		 * 	  Check scroll diretion and prepare next buffer page
		 */
		else {
			/* IF_3.1 */
			if( Is_ScrollUp ) {
				/*** Only if scroll up to another page ( get page boundary line )
				 *   	 OR: scrolling direction changes !!!!
				 * Note: i is already added with -touch_data.dy
				 *   	  i=mark-touch_data.dy
				 *  --- Check the screen bottom yres-1 line, see if new page appears --- */
	  	printf(" -- UP (i+yres-1)/yres=%lu  (mark+yres-1)/yres=%lu  (mark-predy+yres-1)/yres=%lu --\n",
						(i+yres-1)/yres, (mark+yres-1)/yres, (mark-predy+yres-1)/yres);

				/* WRONG!!!, It may move back and forth, so check last time predy instead !!!! */
				//if ( (i+yres-1)/yres > (mark+yres-1)/yres )
				//{
				    /* ! touch_data.dy is negative, check if last time i is NOT cross - */

				if( ( (mark-predy+yres-1)/yres != (i+yres-1)/yres || preup==false )
				     &&  ( cur_txt_pgnum < total_txt_pages-1 ) ) /* txt page num from 0 */
				{
				      printf("        i=%d -- %d up to ", i, cur_txt_pgnum );
						cur_txt_pgnum++;
						cur_buff_pgnum++;
					 	/* If change both */
						if( ( (mark-predy+yres-1)/yres != (i+yres-1)/yres )
											&& preup==false ){
							cur_txt_pgnum++;
							cur_buff_pgnum++;
					 	}
					printf(" %d -- \n", cur_txt_pgnum);

						if(cur_buff_pgnum > FBDEV_BUFFER_PAGES-1)
							cur_buff_pgnum=0;

						/* Update next buff page
						 * cur_txt_pgnum already incremented.
						 * Only if current txt page is NOT the END page. */
						if( cur_txt_pgnum < total_txt_pages-1 )  {
							prep_buff_pgnum=cur_buff_pgnum+1;
							if(prep_buff_pgnum > FBDEV_BUFFER_PAGES-1)
								prep_buff_pgnum=0;

						    writeTxt_to_buffPage( prep_buff_pgnum,
						        fp+get_txtpg_offset(cur_txt_pgnum+1), cur_txt_pgnum+1);
						}
						else if ( i > cur_buff_pgnum*yres )
                                                     i=cur_buff_pgnum*yres;
				  }
				  /* If get to the end of txt page, stop and hold on */
				  else if ( cur_txt_pgnum == total_txt_pages-1 ) {
					  printf(" --- %d end .\n", cur_txt_pgnum);
					  if( i > cur_buff_pgnum*yres )
						  i=cur_buff_pgnum*yres;
				  }
				//}
			}
			/* IF_3.2 */
			else  { /* ( IsScrollDown ) */
				/*** Only if scroll down to another page ( get page boundary line )
				 *   	 OR: scrolling direction changes !!!!
				 * Note: i is already added with -touch_data.dy
				 *	 as i=mark-touch_data.dy
				 *   --- Check the screen uppermost 0 line, see if new page appears! --- */
			  	printf(" -- DOWN i/yres=%lu mark/yres=%lu  (mark-predy)/yres=%lu --\n",
								i/yres, mark/yres, (mark-predy)/yres);

				/* WRONG!!!, It may move back and forth, so check last time predy instead !! */
				//xxxx if( i/yres < mark/yres ) /* Check crossed */

				    /* Confirm last time i is NOT cross */
				if( ( (mark-predy)/yres != i/yres || preup==true )
				  	             &&  ( cur_txt_pgnum > 0 )  )   /* txt page num from 0 */
                                    {    /* ! touch_data.dy is positive - */

					printf("        i=%d -- %d down to ",i, cur_txt_pgnum);
						cur_txt_pgnum--;
						cur_buff_pgnum--;
					  	/* If both change */
						if( ( (mark-predy)/yres != i/yres )
									&& preup==true ) {
							cur_txt_pgnum--;
							cur_buff_pgnum--;
						}
					 printf(" %d -- \n", cur_txt_pgnum);

						if(cur_buff_pgnum < 0)
							cur_buff_pgnum=FBDEV_BUFFER_PAGES-1;

						/* (Scroll down) Update next buff page
						 * cur_txt_pgnum already decremented.
						 * Only if current txt page is NOT the FIRST page */
						if( cur_txt_pgnum > 0 )  {
							prep_buff_pgnum=cur_buff_pgnum-1;
							if(prep_buff_pgnum < 0)
								prep_buff_pgnum=FBDEV_BUFFER_PAGES-1;

						   /* Update next buff page */
						   writeTxt_to_buffPage( prep_buff_pgnum,
							fp+get_txtpg_offset(cur_txt_pgnum-1), cur_txt_pgnum-1 );
				    	        }
				      }
				     /* If get to the first txt page, stop and hold on */
				     else if ( cur_txt_pgnum == 0 && i < cur_buff_pgnum*yres ) {
					  printf(" --- %d HEAD .\n", cur_txt_pgnum);
					  i=cur_buff_pgnum*yres;
				     }

			      //xxxx } /* END  scroll down to another page  */

			 } /* END IF_3.2 ( IsScrollDown ) */

		} /* END IF_3 */

		/* Record dy at last */
                predy=touch_data.dy;
		preup=Is_ScrollUp;

                /*  Refresh FB with offset line, now 'i' limits to [0  yres*FBDEV_BUFFER_PAGES) */
                fb_slide_refresh(&gv_fb_dev, i);
		tm_delayms(5);
      	} /* for() touch parse END */


} while(1); ///////////////////////////   END LOOP TEST   ///////////////////////////////


	egi_imgbuf_free(eimg);
	eimg=NULL;

	egi_free_filo(filo_pgOffset);

        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev); /* Flush FB filo if necessary */
        release_fbdev(&gv_fb_dev);
	printf("egi_end_touchread()...\n");
	egi_end_touchread();
        printf("egi_quit_log()...\n");
        egi_quit_log();
        printf("<-------  END  ------>\n");

return 0;

}



/*--------------------------------------------
Thread function:
Create page offset table for wbook.
---------------------------------------------*/
static void * create_pgOffset_table(void *arg)
{
	int nret=0;
	unsigned long off=0;
	EGI_FONTS   fonts={.ftname="tmp_fonts",};  /* a temporary EGI_FONTS */

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

   /* Create page offset table
    * Calculate wbook page index(offset) and Push it to FILO
    */
   for( off=0,
        	egi_filo_push(filo_pgOffset, &off),             /* Init off and push first page 0 */
        				total_txt_pages=0; ; )	/* Init total txt pages */
   {
	/* To the NULL fbdev, Use the same params for displaying fbdev. */
	nret=FTsymbol_uft8strings_writeFB(NULL, fonts.regular,      	 /* FBdev, fontface */
      	                          fw, fh, (const unsigned char *)fp+off, /* fw,fh, pstr */
                        	          pixpl, lines, lngap,     	 /* pixpl, lines, gap */
                                          x0, y0,                        /* x0,y0, */
                                     	  WEGI_COLOR_BLACK, -1, -1,	 /* fontcolor, transcolor,opaque */
					  NULL, NULL, NULL, NULL);
	if(nret <= 0)
		break;

	off += nret;
	egi_filo_push(filo_pgOffset, &off);

	total_txt_pages++;

	printf("Filo item number: %d off: %ld\n", egi_filo_itemtotal(filo_pgOffset)-1, off);
   }
	printf("Total txt pages: %d \n", total_txt_pages);
	printf("Filo item number: %d pointer to the bottom!", egi_filo_itemtotal(filo_pgOffset)-1);

	/* create easter egg page number */
	egg_pgnum=egi_random_max(total_txt_pages)-1;

	/* Release the temp. fonts */
	FTsymbol_release_library(&fonts);

	pthread_exit((void *)0);
}


/*---------------------------------------------------------
Write txt to a FB back buffer page, and put page number at
the bottom.

@nbufpg:	 Number of FB back buffer page.
@pstr:   	 Pointer to the txt.
@ntxtpg:	 Txt page number

----------------------------------------------------------*/
void writeTxt_to_buffPage(int nbufpg, char *pstr, int ntxtpg)
{
	int  ret;
	char strPgNum[16];
	unsigned int xres=gv_fb_dev.vinfo.xres;
	unsigned int yres=gv_fb_dev.vinfo.yres;
	unsigned int line_length=gv_fb_dev.finfo.line_length;

	/* Shift to current working FB back buffer  */
        fb_shift_buffPage(&gv_fb_dev,nbufpg);
	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_LTYELLOW);

	/* Write pstr to FB back buffer page */
	ret=FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.regular,     /* FBdev, fontface */
               	                          fw, fh, (const unsigned char *)pstr,  /* fw,fh, pstr */
                       	                  pixpl, lines, lngap,     		/* pixpl, lines, gap */
                               	          x0, y0,                             	/* x0,y0, */
                                  	  WEGI_COLOR_BLACK, -1, -1,   /* fontcolor, transcolor,opaque */
					  NULL, NULL, NULL, NULL);

	if(ret<0)
		return;

	/* draw a line at bottom of each page */
	//fbset_color(WEGI_COLOR_BLACK);
	//draw_line(&gv_fb_dev,5,yres-6, xres-5, yres-6);

	/* Put page number at bottom of page */
	snprintf(strPgNum,sizeof(strPgNum)-1,"- %d -", ntxtpg);
	pixlen=FTsymbol_uft8strings_pixlen(egi_appfonts.bold, 16, 16, (const unsigned char *)strPgNum);
        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.bold,     /* FBdev, fontface */
                                 16, 16, (const unsigned char *)strPgNum,      /* fw,fh, pstr */
                                     pixpl, 1, 0,     			/* pixpl, lines, gap */
                                     (xres-pixlen)/2, yres-20,    	/* x0,y0, */
                                     WEGI_COLOR_BLACK, -1, -1,  /* fontcolor, transcolor,opaque */
				     NULL, NULL, NULL, NULL );


	/* Put an Easter Egg */
	if( egg_pgnum>=0 && ntxtpg==egg_pgnum ) {
		//memcpy(gv_fb_dev.map_bk, eimg->imgbuf, eimg->height*line_length);
		egi_imgbuf_windisplay2( eimg, &gv_fb_dev,  		/* *imgbuf, *fbdev */
					0,0, 0,20,			/* xp,yp,  xw,yw  */
					eimg->width, eimg->height ); 	/* winw, winh */
	}

}


/*----------------------------------------------------------
Get offset value correspoinding to the input txt page number
Use the offset value to seek the txt block.

@npg:	txt page numer

Note: If

Return:
	offset value
----------------------------------------------------------*/
unsigned int get_txtpg_offset(int npg)
{
	unsigned int offset=0;

	egi_filo_read(filo_pgOffset, npg, &offset);

	printf("%s: offset[%d] for txt page[%d] \n",__func__, offset, npg);
	return offset;
}
