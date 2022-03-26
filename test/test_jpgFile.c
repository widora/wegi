/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to extract information from exif data in a JPEG file.


Journal:
2022-03-15: Create the file.
2022-03-16: Parse Exif IFD directory
2022-03-17: Parse IFD0 tags 0x9c9b-0x9c9f (for Windows Explorer)
2022-03-18: Parse SOFn for image size.
2022-03-19: Add EGI_PICINFO and egi_picinfo_calloc() egi_picinfo_free()
2022-03-21:
	1. Move EGI_PICINFO and functions to egi_bjp.h egi_bjp.c
	2. Test effect of progressive JPEG.
2022-03-23: Test egi_check_jpgfile()

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

#define TMP_JPEG_NAME  "/tmp/._test_jpg.jpeg"

void FTsymbol_writeFB(char *txt, int fw, int fh, EGI_16BIT_COLOR color, int px, int py);

/*==========================
	  MAIN()
===========================*/
int main(int argc, char **argv)
{
	int i, k;
	int bn=20;//1;  		/* Number of divided blocks for an image.  */
	int bs;    		/* size of a block */
	char *fpath=NULL;  	/* Reference only */
	unsigned long fsize=0;
	EGI_PICINFO	*PicInfo;
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
		printf("----------- JPEG File: %s ---------\n", fpath);
		ret=egi_check_jpgfile(fpath);
		if(ret==JPEGFILE_COMPLETE)
			printf("OK, integraty checked!\n");
		else if(ret==JPEGFILE_INCOMPLETE)
			printf(DBG_YELLOW"Incomplete JPEG file!\n"DBG_RESET);
		else {
			printf(DBG_RED"Invalid JPEG file!\n"DBG_RESET);
			continue;
		}

		PicInfo=egi_parse_jpegFile(fpath);
		if(PicInfo==NULL)
			continue;

		/* Display Picture Info */
		tcnt=0;
		maxcnt=sizeof(txtbuff)-1;
		memset(txtbuff,0,sizeof(txtbuff));

		if(PicInfo->width>0) {
//			printf("ImageSize: W%dxH%dxCPP%dxBPC%d\n",
//				PicInfo->width, PicInfo->height, PicInfo->cpp, PicInfo->bpc);
			ret=snprintf(txtbuff, maxcnt-tcnt, "Image: W%dxH%dxcpp%dxbpc%d\n",
					PicInfo->width, PicInfo->height, PicInfo->cpp, PicInfo->bpc);
			if(ret>0)tcnt+=ret;
		}

		if(PicInfo->progressive) {
//			printf(DBG_MAGENTA"Progressive DCT\n"DBG_RESET);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt, "Progressive DCT\n");
			if(ret>0) tcnt+=ret;
		}

		if(PicInfo->CameraMaker) {
//			printf("CameraMaker: %s\n", PicInfo->CameraMaker);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt, "CameraMaker: %s\n", PicInfo->CameraMaker);
			if(ret>0) tcnt+=ret;
		}
		if(PicInfo->CameraModel) {
//			printf("CameraModel: %s\n", PicInfo->CameraModel);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt,"CameraModel: %s\n", PicInfo->CameraModel);
			if(ret>0) tcnt+=ret;
		}
		if(PicInfo->DateTaken) {
//			printf("DateTaken: %s\n", PicInfo->DateTaken);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt,"DateTaken: %s\n", PicInfo->DateTaken);
			 if(ret>0) tcnt+=ret;
		}
		if(PicInfo->software) {
//			printf("Software: %s\n", PicInfo->software);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt,"Software: %s\n", PicInfo->software);
			if(ret>0) tcnt+=ret;
		}
		if(PicInfo->title) {
//			printf("Title: %s\n", PicInfo->title);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt,"Title: %s\n", PicInfo->title);
			if(ret>0) tcnt+=ret;
		}
		if(PicInfo->author) {
//                      printf("Author: %s\n", PicInfo->author);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt,"Author: %s\n", PicInfo->author);
			if(ret>0) tcnt+=ret;
		}
		if(PicInfo->keywords) {
//                      printf("Keywords: %s\n", PicInfo->keywords);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt,"Keywords: %s\n", PicInfo->keywords);
			if(ret>0) tcnt+=ret;
		}
		if(PicInfo->subject) {
//                      printf("Subject: %s\n", PicInfo->subject);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt,"Subject: %s\n", PicInfo->subject);
			if(ret>0) tcnt+=ret;
		}
		if(PicInfo->ImageDescription) {
//                      printf("Description: %s\n", PicInfo->ImageDescription);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt, "Description: %s\n", PicInfo->ImageDescription);
			if(ret>0) tcnt+=ret;
		}
		if(PicInfo->comment) {
//                      printf("Comment: %s\n", PicInfo->comment);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt,"Comment: %s\n", PicInfo->comment);
			if(ret>0) tcnt+=ret;
		}
		if(PicInfo->xcomment) {
//                      printf("XComment: %s\n", PicInfo->xcomment);
			ret=snprintf(txtbuff+tcnt, maxcnt-tcnt,"XComment: %s\n", PicInfo->xcomment);
			if(ret>0) tcnt+=ret;
		}

		printf("\n====== Pic Info ======\n%s\n",txtbuff);
		printf("----------------------\n");

#if 0 /* TEST: --------- */
		continue;
#endif

		/* Copy partial JPEG file and display it, demo effect of baseline_DCT and progressive_DCT */
		fsrc=fopen(fpath, "rb");
		fsize=egi_get_fileSize(fpath);
		if( fsize==0 || fsrc==NULL)
			exit(EXIT_FAILURE);
		bs=fsize/bn;

		/* Display whole image */
		if(bn<2) {
			/* Load to imgbuf */
			imgbuf=egi_imgbuf_readfile(fpath);
			if(imgbuf==NULL) {
				printf("Fail to read imgbuf from '%s'!\n", TMP_JPEG_NAME);
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
			fdest=fopen(TMP_JPEG_NAME, k==0?"wb":"ab");
			if(fdest==NULL) exit(EXIT_FAILURE);

			/* Copy 1/bn of fsrc to append fdest. */
			fseek(fsrc, k*bs, SEEK_SET);
			if( egi_copy_fileBlock(fsrc,fdest, k!=(bn-1) ? bs : (fsize-(bn-1)*bs) )!=0 )
				printf("Copy_fileBlock fails at k=%d\n", k);

			/* Fflush or close fdest, OR the last part may be missed when egi_imgbuf_readfile()! */
			fflush(fdest);        /* force to write user space buffered data to in-core */
			fsync(fileno(fdest)); /* write to disk/hw */

		    	if( k<4 || k==bn-1 ) { // || !PicInfo->progressive ) {
				/* Load to imgbuf */
				printf("fdest size: %ld of %ld\n", egi_get_fileSize(TMP_JPEG_NAME), fsize);
				imgbuf=egi_imgbuf_readfile(TMP_JPEG_NAME);
				if(imgbuf==NULL) {
					printf("Fail to read imgbuf from '%s'!\n", TMP_JPEG_NAME);
					fclose(fdest);
					continue;
				}

				/* Display image */
	                	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKGRAY);
	                        egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0,0);   /* imgbuf, fbdev, subnum, subcolor, x0, y0 */

   		             	fb_render(&gv_fb_dev);
        		        usleep(300000);
		   	}

			/* Free and release */
			egi_imgbuf_free2(&imgbuf);
			fclose(fdest);
		    } /* END for() */

	        } /* END else */

		/* Display picture information */
		//draw_blend_filled_rect(&gv_fb_dev, 0, 0, 320-1, 110, WEGI_COLOR_GRAY3, 160);
		draw_blend_filled_rect(&gv_fb_dev, 0, 0, 320-1, 240-1, WEGI_COLOR_GRAY3, 160);
		FTsymbol_writeFB(txtbuff, 18, 18, WEGI_COLOR_BLACK, 5, 10);
		fb_render(&gv_fb_dev);
		sleep(2);

		fclose(fsrc);
		egi_picinfo_free(&PicInfo);

	} /* End for() */

} ////////////////////////  END: LOOP TEST  /////////////////////////////////////

	return 0;
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
                                        color, -1, 240,                 /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
}

