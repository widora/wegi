/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to extract information from a PNG file.


Journal:
2022-03-23: Create the file.
2022-03-24/25: egi_parse_pngFile()
2022-03-26:
	1. Test display interlaced PNG.
	2. Move egi_parse_pngFile() to egi_bjp.c

Midas Zhou
知之者不如好之者好之者不如乐之者
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <arpa/inet.h>
#include "egi_debug.h"
#include "egi_cstring.h"
#include "egi_image.h"
#include "egi_bjp.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"

#define TMP_PNG_FPATH  "/tmp/_test_png.png"
void FTsymbol_writeFB(char *txt, int fw, int fh, EGI_16BIT_COLOR color, int px, int py);

/*==========================
          MAIN()
===========================*/
int main(int argc, char **argv)
{
        int i, k;
        int bn=20;//1;                  /* Number of divided blocks for an image.  */
        int bs;                 /* size of a block */
        char *fpath=NULL;       /* Reference only */
        unsigned long fsize=0;
        EGI_PICINFO     *PicInfo;
        FILE *fdest, *fsrc;
        EGI_IMGBUF *imgbuf;
        int ret;
        int tcnt;
        int maxcnt;
        char txtbuff[2048];

        /* Initilize sys FBDEV */
        printf("init_fbdev()...\n");
        if(init_fbdev(&gv_fb_dev))
                return -1;


        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev, false); //true);//false);
        fb_position_rotate(&gv_fb_dev,0);
        //gv_fb_dev.pixcolor_on=true;             /* Pixcolor ON */
        //gv_fb_dev.zbuff_on = true;              /* Zbuff ON */
        //fb_init_zbuff(&gv_fb_dev, INT_MIN);

        /* Load FT fonts */
        if(FTsymbol_load_sysfonts() !=0 ) {
              printf("Fail to load FT appfonts, quit.\n");
              return -2;
	}
	//FTsymbol_disable_SplitWord();

  while(1)
  { ////////////////////////  LOOP TEST  /////////////////////////////////////

        for(i=1; i<argc; i++) {

                fpath=argv[i];
                printf("----------- PNG File: %s ---------\n", fpath);

#if 0  /* Check integrity for original PNG file */
                ret=egi_check_pngfile(fpath);
                if(ret==PNGFILE_COMPLETE)
                        printf(DBG_GREEN"OK, integrity checked!\n"DBG_RESET);
                else if(ret==PNGFILE_INCOMPLETE)
                        printf(DBG_YELLOW"Incomplete PNG file!\n"DBG_RESET);
                else {
                        printf(DBG_RED"Invalid PNG file!\n"DBG_RESET);
                        continue;
                }
#endif

                PicInfo=egi_parse_pngFile(fpath);
                if(PicInfo==NULL)
                        continue;

#if 1           /* Display Picture Info */
                tcnt=0;
                maxcnt=sizeof(txtbuff)-1;
                memset(txtbuff,0,sizeof(txtbuff));

                if(PicInfo->width>0) {
                        ret=snprintf(txtbuff+tcnt, maxcnt-tcnt, "Image: W%dxH%dxcpp%dxbpc%d\n",
                                        PicInfo->width, PicInfo->height, PicInfo->cpp, PicInfo->bpc);
                        if(ret>0)tcnt+=ret;
                }
                if(PicInfo->progressive) {
                        ret=snprintf(txtbuff+tcnt, maxcnt-tcnt, "Interlaced PNG\n");
                        if(ret>0) tcnt+=ret;
                }

                ret=snprintf(txtbuff+tcnt, maxcnt-tcnt, "ColorType: %d\n", PicInfo->colorType);
                if(ret>0)tcnt+=ret;

		if(PicInfo->progressive) {
        	        printf("\n====== Pic Info ======\n%s\n",txtbuff);
                	printf("----------------------\n");
		}
#endif


                /* Copy partial PNG file and display it, demo effect of interlaced PNG. */
                fsrc=fopen(fpath, "rb");
                fsize=egi_get_fileSize(fpath);
                if( fsize==0 || fsrc==NULL)
                        exit(EXIT_FAILURE);
                bs=fsize/bn;

#if 0 /* TEST: ----------------- */
                imgbuf=egi_imgbuf_readfile(fpath);
                if(imgbuf==NULL) {
                	printf("Fail to read imgbuf from '%s'!\n", fpath);
			exit(0);
                }

                /* Display image */
                fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKGRAY);
                egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0,0);   /* imgbuf, fbdev, subnum, subcolor, x0, y0 */
                fb_render(&gv_fb_dev);

/* <----------------- */
		exit(0);
#endif

                /* Display whole image if bn<2 */
                if(bn<2) {
                        /* Load to imgbuf */
                        imgbuf=egi_imgbuf_readfile(fpath);
                        if(imgbuf==NULL) {
                                printf("Fail to read imgbuf from '%s'!\n", TMP_PNG_FPATH);
                                fclose(fdest);
                                continue;
                        }

                        /* Display image */
                        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKGRAY);
                        egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0,0);   /* imgbuf, fbdev, subnum, subcolor, x0, y0 */
                        fb_render(&gv_fb_dev);
                }
                /* Display image progressively .... */
                else {
                   for(k=0; k<bn; k++) {
                        printf("K=%d\n",k);
                        fdest=fopen(TMP_PNG_FPATH, k==0?"wb":"ab");
                        if(fdest==NULL) {
				printf("Fail to open temp. file '%s'.\n", TMP_PNG_FPATH);
				exit(EXIT_FAILURE);
			}

                        /* Copy 1/bn of fsrc to append fdest. */
                        fseek(fsrc, k*bs, SEEK_SET);
                        if( egi_copy_fileBlock(fsrc,fdest, k!=(bn-1) ? bs : (fsize-(bn-1)*bs) )!=0 )
                                printf(DBG_RED"Copy_fileBlock fails at k=%d\n"DBG_RESET, k);

                        /* Fflush or close fdest, OR the last part may be missed when egi_imgbuf_readfile()! */
                        fflush(fdest);        /* force to write user space buffered data to in-core */
                        fsync(fileno(fdest)); /* write to disk/hw */
			fclose(fdest);

#if 1  /* Check integrity for partial PNG file. */
                ret=egi_check_pngfile(TMP_PNG_FPATH); /* <----- Check fdest */
                if(ret==PNGFILE_COMPLETE)
                        printf(DBG_GREEN"OK, integrity checked!\n"DBG_RESET);
                else if(ret==PNGFILE_INCOMPLETE)
                        printf(DBG_YELLOW"Incomplete PNG file!\n"DBG_RESET);
                else {
                        printf(DBG_RED"Invalid PNG file!\n"DBG_RESET);
                        continue;
                }
#endif


                        if( k<12 || k==bn-1 ) { /*  */
                                /* Load to imgbuf */
                                printf("fdest size: %ld of %ld\n", egi_get_fileSize(TMP_PNG_FPATH), fsize);
                                imgbuf=egi_imgbuf_readfile(TMP_PNG_FPATH);
                                if(imgbuf==NULL) {
                                        printf("Fail to read imgbuf from '%s'!\n", TMP_PNG_FPATH);
//                                        fclose(fdest);
                                        continue;
                                }

                                /* Display image */
                                fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_BLACK);

				#if 0  /* LeftTop */
                                egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0,0);   /* imgbuf, fbdev, subnum, subcolor, x0, y0 */

				#else /* Put image to screen center to center */
				int xp=0;
				int yp=0;
				int xw,yw;
				int xres=gv_fb_dev.pos_xres;
				int yres=gv_fb_dev.pos_yres;

		                xw=imgbuf->width>xres ? 0:(xres-imgbuf->width)/2;
                		yw=imgbuf->height>yres ? 0:(yres-imgbuf->height)/2;
				xp=imgbuf->width>xres  ? (imgbuf->width-xres)/2 : 0;
				yp=imgbuf->height>yres  ? (imgbuf->height-yres)/2 : 0;

                		egi_imgbuf_windisplay2( imgbuf, &gv_fb_dev, //-1,
                                        xp, yp, xw, yw,
                                        imgbuf->width, imgbuf->height);
				#endif

                                fb_render(&gv_fb_dev);
                                usleep(500000);
                        }

                        /* Free and release */
                        egi_imgbuf_free2(&imgbuf);
        //                fclose(fdest);
                    } /* END for() */

                } /* END else */
		sleep(1);

                /* Display picture information */
                //draw_blend_filled_rect(&gv_fb_dev, 0, 0, 320-1, 110, WEGI_COLOR_GRAY3, 160);
                draw_blend_filled_rect(&gv_fb_dev, 0, 0, 320-1, 240-1, WEGI_COLOR_GRAY3, 200);
                FTsymbol_writeFB(txtbuff, 18, 18, WEGI_COLOR_WHITE, 5, 10);
                fb_render(&gv_fb_dev);
                usleep(500000);

                fclose(fsrc);
                egi_picinfo_free(&PicInfo);

     } /* END for() */

  } ////////////////////////  END: LOOP TEST  //////////////////////////////////

  exit(0);
}

/*-------------------------------------
        FTsymbol WriteFB TXT
@txt:           Input text
@px,py:         LCD X/Y for start point.
--------------------------------------*/
void FTsymbol_writeFB(char *txt, int fw, int fh, EGI_16BIT_COLOR color, int px, int py)
{
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular,       /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,      /* fw,fh, pstr */
                                        320-10, 240/(fh+fh/5), fh/5,               /* pixpl, lines, fgap */
                                        px, py,                         /* x0,y0, */
                                        color, -1, 250,                 /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
}


