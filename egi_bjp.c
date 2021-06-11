/*------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Original source: https://blog.csdn.net/luxiaoxun/article/details/7622988

libjpeg.txt:
  * "Interchange" datastreams contain an image and all tables needed to decode
     the image.  These are the usual kind of JPEG file.
  * "Abbreviated image" datastreams contain an image, but are missing some or
    all of the tables needed to decode that image.
  * "Abbreviated table specification" (henceforth "tables-only") datastreams
    contain only table specifications.


1. Based on libpng-1.2.56 and jpeg_9a of 19-Jan-2014.
2. The width of the displaying picture must be a multiple of 4.

TODO:
    1. to pad width to a multiple of 4 for bmp file.
    2. jpgshow() picture flips --OK
    3. in show_jpg(), just force 8bit color data to be 24bit, need to improve.!!!

Journal:
2021-05-19:
	1.egi_imgbuf_loadjpg(): close_jpgImg(imgbuf) before abort/return.

2021-06-06:
	1. Add: egi_imgbuf_savejpg()

Modified and appended by Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------------*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <linux/fb.h> //u
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <jpeglib.h>
#include <jerror.h>
#include <dirent.h>
#include <png.h>
#include "egi_image.h"
#include "egi_debug.h"
#include "egi_bjp.h"
#include "egi_color.h"
#include "egi_timer.h"

//static BITMAPFILEHEADER FileHead;
//static BITMAPINFOHEADER InfoHead;

/*--------------------------------------------------------------
Compress RGB_24bit image to an JPEG file.

@filename:	File path to save target JPEG file.
@qulity:	Jpeg compression quality(0-100).
@width,height:  Image size.
@rgb24:		Pointer to RGB_24bit image data.
@cspace:	Color space of input data.
		Example:  JCS_RGB, JCS_YCbCr, JCS_CMYK, JCS_YCCK,
			  JCS_BG_RGB, JCS_BG_YCC

return:
	0	Ok
	<0	Fails
--------------------------------------------------------------*/
int compress_to_jpgFile(const char * filename, int quality, int width, int height,
			unsigned char *rgb24, J_COLOR_SPACE inspace)
{
  	FILE * fil;

	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

  	JSAMPROW row_pointer[1];      /* pointer to JSAMPLE row[s] */
  	int bytes_per_row;            /* row width  */

	/* Check input */
	if(rgb24==NULL || width<1 || height<1)
		return -1;

	/* Step 1: allocate and initialize JPEG compression object */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	/* Step 2: specify data destination (eg, a file) */
	if ((fil= fopen(filename, "wb")) == NULL) {
    		fprintf(stderr, "%s: Fail to open '%s' for write JPEG.", __func__, filename);
		return -1;
  	}
  	jpeg_stdio_dest(&cinfo, fil);

	/* Step 3: set parameters for compression */
	cinfo.image_width = width;
  	cinfo.image_height = height;
  	cinfo.input_components = 3;
  	cinfo.in_color_space = inspace; //JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

	/* Step 4: Start compressor */
	jpeg_start_compress(&cinfo, TRUE);

	/* Step 5: while (scan lines remain to be written) */
	/*           jpeg_write_scanlines(...); */
	bytes_per_row = width * 3;  /* R8G8B8 */
	while (cinfo.next_scanline < cinfo.image_height) {
		/* One row each time, maybe more. */
    		row_pointer[0] = &rgb24[cinfo.next_scanline * bytes_per_row];
    		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  	}

  	/* Step 6: Finish compression */
  	jpeg_finish_compress(&cinfo);
  	fclose(fil);

	/* Step 7: release JPEG compression object */
  	jpeg_destroy_compress(&cinfo);

	return 0;
}


/*--------------------------------------------------------------
Compress RGB_24bit image to JPEG, and save to outbuffer.

			!!! WARNING !!!
The caller MUST ensure that mem space of outbuffer is big enough,
AND always reset outsize to the actually allocated size before
EACH CALL!
If the input outsize is NOT big enough(include if NULL), the
function will allocate outbuffer again! and it triggers memory
leakage!
OR you just have the function to allocate outbuffer, and free it
after EACH CALL!

@outbuff:	Buffer to store compressed jpeg data.
@outsize:	Jpeg data size.
@qulity:	Jpeg compression quality.
@width,height:  Image size.
@rgb24:		Pointer to RGB_24bit image data.
@cspace:	Color space of input data.
		Example:  JCS_RGB, JCS_YCbCr, JCS_CMYK, JCS_YCCK,
			  JCS_BG_RGB, JCS_BG_YCC


return:
	0	Ok
	<0	Fails
--------------------------------------------------------------*/
int compress_to_jpgBuffer(unsigned char ** outbuffer, unsigned long * outsize,
				int quality, int width, int height, unsigned char *rgb24, J_COLOR_SPACE inspace)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

  	JSAMPROW row_pointer[1];      /* pointer to JSAMPLE row[s] */
  	int bytes_per_row;            /* row width  */

	/* Check input */
	if(rgb24==NULL || width<1 || height<1)
		return -1;

	/* Step 1: allocate and initialize JPEG compression object */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	/* Step 2: specify data destination */
	jpeg_mem_dest(&cinfo, outbuffer, outsize);

	/* Step 3: set parameters for compression */
	cinfo.image_width = width;
  	cinfo.image_height = height;
  	cinfo.input_components = 3;
  	cinfo.in_color_space = inspace; //JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

	/* Step 4: Start compressor */
	jpeg_start_compress(&cinfo, TRUE);

	/* Step 5: while (scan lines remain to be written) */
	/*           jpeg_write_scanlines(...); */
	bytes_per_row = width * 3;  /* R8G8B8 */
	while (cinfo.next_scanline < cinfo.image_height) {
		/* One row each time, maybe more. */
    		row_pointer[0] = &rgb24[cinfo.next_scanline * bytes_per_row];
    		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  	}

  	/* Step 6: Finish compression */
  	jpeg_finish_compress(&cinfo);

	/* Step 7: release JPEG compression object */
  	jpeg_destroy_compress(&cinfo);

	return 0;
}


/*--------------------------------------------------------------
Open jpg file and return decompressed image buffer pointer,
Call close_jgpImg() to release image buffer.

@filename	File name.
@w,h:   	Pointer to pass out withd/height of the image
@components:  	out color components

return:
	=NULL fail
	>0 decompressed image buffer pointer
--------------------------------------------------------------*/
unsigned char * open_jpgImg(const char * filename, int *w, int *h, int *components )
{
	unsigned char header[2];
	struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;
        FILE *infile;
        unsigned char *buffer=NULL;
        unsigned char *pt=NULL;

        if (( infile = fopen(filename, "rbe")) == NULL) {
                fprintf(stderr, "open %s failed\n", filename);
                return NULL;
        }

	/* To confirm JPEG/JPG type, simple way. */
	fread(header,1,2,infile); /* start of image 0xFF D8 */
	if(header[0] != 0xFF || header[1] != 0xD8) {
		fprintf(stderr, "SOI(FF D8): 0x%x 0x%x, '%s is NOT a recognizable JPG/JPEG file!\n",
										header[0], header[1],filename);
		fclose(infile);
		return NULL;
	}
#if 0 /* Omit for abbreviated jpeg file */
	fseek(infile,-2,SEEK_END); /* end of image 0xFF D9 */
	fread(header,1,2,infile);
	if(header[0] != 0xFF || header[1] != 0xD9) {
		fprintf(stderr, "EOI(FF D9): 0x%x 0x%x, '%s is NOT a recognizable JPG/JPEG file!\n",
									header[0], header[1], filename);
		fclose(infile);
		return NULL;
	}
#endif
	/* Must reset seek for jpeg decompressor! */
	fseek(infile,0,SEEK_SET);

	/* Fill in the standard error-handling methods */
        cinfo.err = jpeg_std_error(&jerr);

	/* Create decompressor */
        jpeg_create_decompress(&cinfo);

	/* Prepare data source */
        jpeg_stdio_src(&cinfo, infile);

        jpeg_read_header(&cinfo, TRUE);

#if 0	/* Check huffman table */
	int i;
	printf("Check dc_huff tables: ");
	for(i=0; i<NUM_HUFF_TBLS; i++)
		printf("[%d] %s ",i, (cinfo.dc_huff_tbl_ptrs[i] == NULL)?"NULL":"OK");
	printf("Check ac_huff tables: ");
	for(i=0; i<NUM_HUFF_TBLS; i++)
		printf("[%d] %s ",i, (cinfo.ac_huff_tbl_ptrs[i] == NULL)?"NULL":"OK");
	printf("\n");
#endif

#if  0    /* TODO: Code to load a fixed Huffman table if necessary */
	JHUFF_TBL* huff_ptr=NULL;

    if (cinfo.ac_huff_tbl_ptrs[n] == NULL)
      cinfo.ac_huff_tbl_ptrs[n] = jpeg_alloc_huff_table((j_common_ptr) &cinfo);
    huff_ptr = cinfo.ac_huff_tbl_ptrs[n];       /* huff_ptr is JHUFF_TBL* */
    for (i = 1; i <= 16; i++) {
      /* counts[i] is number of Huffman codes of length i bits, i=1..16 */
      huff_ptr->bits[i] = counts[i];
    }
    for (i = 0; i < 256; i++) {
      /* symbols[] is the list of Huffman symbols, in code-length order */
      huff_ptr->huffval[i] = symbols[i];
    }
#endif

	/* Start decompress */
        jpeg_start_decompress(&cinfo);

	/* Get image info. */
        *w = cinfo.output_width;
        *h = cinfo.output_height;
	*components=cinfo.out_color_components;

	/* Allocate buffer to hold decompressed data */
        buffer = (unsigned char *) malloc(cinfo.output_width *
                        cinfo.output_components * cinfo.output_height);
  	pt = buffer;

	/* Decompressing... */
        while (cinfo.output_scanline < cinfo.output_height) {
                jpeg_read_scanlines(&cinfo, &pt, 1);
                pt += cinfo.output_width * cinfo.output_components;
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

        fclose(infile);

	return buffer;

}


/*    release mem for decompressed jpeg image buffer */
void close_jpgImg(unsigned char *imgbuf)
{
	if(imgbuf != NULL)
		free(imgbuf);
}


/*---------------------------------------------------------------
Open a buffer holding a complete JPG file and return decompressed
image buffer pointer. Call close_jgpImg() to release image buffer.

@inbuff		Buffer holding a complete JPG image.
@insize		Size of inbuff.
@w,h:   	Pointer to pass out  withd/height of the image
@components:  	out color components

return:
	=NULL fail
	>0 decompressed image buffer pointer
--------------------------------------------------------------*/
unsigned char * open_buffer_jpgImg( const unsigned char *inbuff, unsigned long insize,
						int *w, int *h, int *components )
{
	struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;
        unsigned char *buffer=NULL;
        unsigned char *pt=NULL;

	if(inbuff==NULL)
		return NULL;

	/* To confirm JPEG/JPG type, simple way. */
	if( inbuff[0] != 0xFF || inbuff[1] != 0xD8) {
		fprintf(stderr, "SOI(FF D8): 0x%x 0x%x, inbuffer is NOT a recognizable JPG/JPEG file!\n",inbuff[0], inbuff[1]);
		return NULL;
	}

	/* Fill in the standard error-handling methods */
        cinfo.err = jpeg_std_error(&jerr);

	/* Create decompressor */
        jpeg_create_decompress(&cinfo);

	/* Prepare data source */
	jpeg_mem_src(&cinfo, (unsigned char *)inbuff, insize);

        jpeg_read_header(&cinfo, TRUE);

	/* Start decompress */
        jpeg_start_decompress(&cinfo);

	/* Get image info. */
        *w = cinfo.output_width;
        *h = cinfo.output_height;
	*components=cinfo.out_color_components;

	/* Allocate buffer to hold decompressed data */
        buffer = (unsigned char *) malloc(cinfo.output_width *
                        cinfo.output_components * cinfo.output_height);
  	pt = buffer;

	/* Decompressing... */
        while (cinfo.output_scanline < cinfo.output_height) {
                jpeg_read_scanlines(&cinfo, &pt, 1);
                pt += cinfo.output_width * cinfo.output_components;
        }

	/* Finish */
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

	return buffer;
}


/*------------------------------------------------------------------
open a 24bit BMP file and write image data to FB.
image size limit: 320x240

Note:
    1. Read color pixel one by one, is rather slower.
    2. For BGR type onley, BGRA is not supported.
    3. FB.pos_rotate is NOT supported.

blackoff:   1	 Do NOT wirte black pixels to FB.
		 (keep original data in FB)

	    0	 Wrtie  black pixels to FB.
x0,y0:		start point in LCD coordinate system



Return:
	    1   file size too big
	    0	OK
	   <0   fails
--------------------------------------------------------------------*/
int show_bmp(const char* fpath, FBDEV *fb_dev, int blackoff, int x0, int y0)
{
	BITMAPFILEHEADER FileHead;
	BITMAPINFOHEADER InfoHead;
	unsigned char *buff;
	int j,k;
	int pad; /*in byte, pad to multiple of 4bytes */

	FILE *fp;
	int xres=fb_dev->vinfo.xres;
	int bits_per_pixel=fb_dev->vinfo.bits_per_pixel;
	int rc;
//	int line_x, line_y;
	long int location = 0;
	uint16_t color;
	PIXEL pix;

	/* get fb map */
	unsigned char *fbp =fb_dev->map_fb;

	printf("fpath=%s\n",fpath);
	fp = fopen( fpath, "rbe" );
	if (fp == NULL)
	{
		return -1;
	}

	rc = fread( &FileHead, sizeof(BITMAPFILEHEADER),1, fp );
	if ( rc != 1)
	{
		printf("Read FileHead error!\n");
		fclose( fp );
		return -2;
	}

	//检测是否是bmp图像
	if (memcmp(FileHead.cfType, "BM", 2) != 0)
	{
		printf("It's not a BMP file\n");
		fclose( fp );
		return -3;
	}

	rc = fread((char *)&InfoHead, sizeof(BITMAPINFOHEADER), 1, fp);
	if ( rc != 1)
	{
		printf("Read InfoHead error!\n");
		fclose( fp );
		return -4;
	}

	printf("BMP width=%d,height=%d  ciBitCount=%d\n",(int)InfoHead.ciWidth, (int)InfoHead.ciHeight, (int)InfoHead.ciBitCount);
	//检查是否24bit色
	if(InfoHead.ciBitCount != 24 ){
		printf("It's not 24bit_color BMP!\n");
		return -5;
	}

#if 0 /* check picture size */
	//检查宽度是否240x320
	if(InfoHead.ciWidth > 240 ){
		printf("The width is great than 240!\n");
		return -1;
	}
	if(InfoHead.ciHeight > 320 ){
		printf("The height is great than 320!\n");
		return -1;
	}

	if( InfoHead.ciHeight+y0 > 320 || InfoHead.ciWidth+x0 > 240 )
	{
		printf("The origin of picture (x0,y0) is too big for a 240x320 LCD.\n");
		return -1;
	}
#endif

//	line_x = line_y = 0;

	fseek(fp, FileHead.cfoffBits, SEEK_SET);

	/* if ciWidth_bitcount is not multiple of 4bytes=32, then we need skip padded 0,
	 * skip=0, if ciWidth_bitcount is multiple of 4bytes=32.
	 *  if line=7*3byte=21bytes, 21+3_pad=24yptes  skip=3bytes.
	 */
	pad=( 4-(((InfoHead.ciWidth*InfoHead.ciBitCount)>>3)&3) ) &3; /* bytes */
	printf("Pad byte for each line: %d \n",pad);

	/* calloc buff for one line image */
	buff=calloc(1, InfoHead.ciWidth*sizeof(PIXEL)+pad );
	if(buff==NULL) {
		printf("%s: Fail to alloc buff for color data.\n",__func__);
		return -6;
	}

	/* Read one line each time, read more to speed up process */
	for(j=0;j<InfoHead.ciHeight;j++)
	{
		/* Read one line with pad */
		rc=fread(buff,InfoHead.ciWidth*sizeof(PIXEL)+pad,1,fp);
		if( rc < 1 ) {
			printf("%s: fread error.%s\n",__func__,strerror(errno));
			return -7;
		}

		/* write to FB */
		for(k=0; k<InfoHead.ciWidth; k++) /* iterate pixel data in one line */
		{
			pix=*(PIXEL *)(buff+k*sizeof(PIXEL));

			location = k * bits_per_pixel / 8 +x0 +
					(InfoHead.ciHeight - j - 1 +y0) * xres * bits_per_pixel / 8;
	        	/* NOT necessary ???  check if no space left for a 16bit_pixel in FB mem */
        		if( location<0 || location>(fb_dev->screensize-bits_per_pixel/8) )
	        	{
               			 printf("show_bmp(): WARNING: point location out of fb mem.!\n");
				 free(buff);
  		                 return 1;
        		}
			/* converting to format R5G6B5(as for framebuffer) */
			color=COLOR_RGB_TO16BITS(pix.red,pix.green,pix.blue);
			/*if blockoff>0, don't write black to fb, so make it transparent to back color */
			if(  !blackoff || color )
			{
				*(uint16_t *)(fbp+location)=color;
			}
		}
	 }

	free(buff);
	fclose( fp );
	return( 0 );
}


/*------------------------  Direct FB wirte -------------------------------
Open a jpg file and write image data to FB.
The orginal (x0,y0) and JGP size MUST be appropriate so as to ensure that the
image is totally contained within the screen! Or it will abort.

@fpath:		JPG file path
(inbuff and insize will be valid only if fpath==NULL )
@inbuff:	Buffer holding a compelte jpg file.
@insize:	Size of inbuff.
blackoff:   1   Do NOT wirte black pixels to FB.
		 (keep original data in FB,make black a transparent tunnel)
	    0	 Wrtie  black pixels to FB.
(x0,y0): 	original coordinate of picture in LCD
		MUST >=0  NOW!

NOTE:
1. FB.pos_rotate is not suppored.
2. For Little_endian only.

Return:
	    0	OK
	    <0  fails
-------------------------------------------------------------------------*/
int show_jpg( const char* fpath, const unsigned char *inbuff, unsigned long insize,
	      FBDEV *fb_dev, int blackoff, int x0, int y0)
{
	unsigned char *fbp;
	int xres;
	int yres;
	int bytes_per_pixel;
	int width,height;
	int components;
	unsigned char *imgbuf;  /* 24bits color data */
	unsigned char *dat;	/* color data position */
	uint16_t color;
	uint32_t rgb;  		/* For LETS_NOTE */
	long int location = 0;  /* of FB */
	int x,y;
	int ltx=0, lty=0, rbx=0, rby=0; /* LeftTop and RightBottom tip coords of image, within and relative to the screen */

	if(fb_dev==NULL)
		return -1;
	if(fpath==NULL && inbuff==NULL)
		return -1;

        /* Get fb map pointer and params */
        if(fb_dev->map_bk)
                fbp =fb_dev->map_bk;
        else
                fbp =fb_dev->map_fb;
	xres=fb_dev->finfo.line_length/(fb_dev->vinfo.bits_per_pixel>>3);
	yres=fb_dev->vinfo.yres;
	bytes_per_pixel=(fb_dev->vinfo.bits_per_pixel)>>3;

	/* Open jpg and get data pointer */
	if(fpath) {
		imgbuf=open_jpgImg(fpath, &width, &height, &components );
		if(imgbuf==NULL)
			return -1;
	}
	else {
		imgbuf=open_buffer_jpgImg(inbuff, insize, &width, &height, &components );
		if(imgbuf==NULL)
			return -1;
	}
	//printf("JPG %dx%dx%d\n", width, height, components);

	/* WARNING: need to be improve here: converting 8bit to 24bit color*/
	if(components==1) /* 8bit color */
	{
		height=height/3; /* force to be 24bit pic, however shorten the height */
	}

#if  0  /////////////////////////  Image MUST totally contained in the screen  /////////////////////

	dat=imgbuf;

	/* Check image size and position */
	if( x0<0 || y0<0 || x0+width > xres || y0+height > yres ) {
		printf("Image cannot fit into the screen!\n");
		return -2;
	}

	/* Flip picture to be same data sequence of BMP file */
	for( y=0; y<height; y++) {	/* Bottom to Top */
		for(x=0; x<width; x++) {
		   // #ifdef LETS_NOTE /* ----- FOR 32BITS COLOR (ARGB) FBDEV ----- */
		   if( bytes_per_pixel==4 ) {
			location = (x+x0) * bytes_per_pixel + (y +y0) * xres * bytes_per_pixel;
			rgb=(*(uint32_t*)dat)>>8;
			if(  blackoff<=0 || rgb ) {
				*(fbp+location+2)=*dat++;
				*(fbp+location+1)=*dat++;
				*(fbp+location)=*dat++;
				*(fbp+location+3)=255;		/* Alpha */
			}
		   }
		   else if( bytes_per_pixel==2 ) {
			location = (x+x0) * bytes_per_pixel + (y +y0) * xres * bytes_per_pixel;
			//显示每一个像素, in ili9431 node of dts, color sequence is defined as 'bgr'(as for ili9431) .
   	        	/* dat(R8G8B8) converting to format R5G6B5(as for framebuffer) */
			color=COLOR_RGB_TO16BITS(*dat,*(dat+1),*(dat+2));
			/* if blockoff>0, don't write black to fb, so make it transparent to back color */
			if(  blackoff<=0 || color ) {
				*(uint16_t *)(fbp+location)=color;
			}
			dat+=3;
		   }
		}
	}

#else  //////////////////////////////////////////////

	/* If totally out of range */
	if(x0 < -width+1 || x0 > xres-1)
		return 0;
	if(y0 < -height+1 || y0 > yres-1)
		return 0;

	/* Image LeftTop point (x,y), within and relatvie to screen */
	if(x0<0) ltx=0;
	else	 ltx=x0;
	if(y0<0) lty=0;
	else	 lty=y0;

	/* Image RightBottom point (x,y), within and relatvie to screen */
	if(x0+width-1 > xres-1)	 rbx=xres-1;
	else			 rbx=x0+width-1;
	if(y0+height-1 > yres-1) rby=yres-1;
	else			 rby=y0+height-1;

	//printf("LT(%d,%d), RB(%d,%d)\n", ltx,lty, rbx, rby);

   	/* Here (x,y) are within screen coord */
	for( y=lty; y<rby+1; y++) {
		for(x=ltx; x<rbx+1; x++) {
		   location = x*bytes_per_pixel + y*xres*bytes_per_pixel;  /* (x,y) within screen */
		   dat = imgbuf + (x-x0)*3 + (y-y0)*width*3;		/* Of image */
		   // #ifdef LETS_NOTE /* ----- FOR 32BITS COLOR (ARGB) FBDEV ----- */
		   if( bytes_per_pixel==4 ) {
			rgb=(*(uint32_t*)dat)>>8;
			if(  blackoff<=0 || rgb ) {
				*(fbp+location+2)=*dat++;
				*(fbp+location+1)=*dat++;
				*(fbp+location)=*dat++;
				*(fbp+location+3)=255;		/* Alpha */
			}
		   }
		   else if( bytes_per_pixel==2 ) {
   	        	/* dat(R8G8B8) converting to format R5G6B5(as for framebuffer) */
			color=COLOR_RGB_TO16BITS(*dat,*(dat+1),*(dat+2));
			/* if blockoff>0, don't write black to fb, so make it transparent to back color */
			if(  blackoff<=0 || color ) {
				*(uint16_t *)(fbp+location)=color;
			}
		   }
		}
	}
#endif

	/* Free decompressed data buffer */
	close_jpgImg(imgbuf);
	return 0;
}


/*------------------------------------------------------------------------
Read JPG image data and load to an EGI_IMGBUF.
Clear data and realloc if any old data exists in egi_imgbuf before loading.

fpath:		JPG file path
egi_imgbuf:	EGI_IMGBUF  to hold the image data, in 16bits color

Note:
	1. only for case components=3.
	2. No alpha data for EGI_IMGBUF.

Return
		0	OK
		<0	fails
-------------------------------------------------------------------------*/
int egi_imgbuf_loadjpg(const char* fpath,  EGI_IMGBUF *egi_imgbuf)
{
	//int xres=fb_dev->vinfo.xres;
	//int bits_per_pixel=fb_dev->vinfo.bits_per_pixel;
	int width,height;
	int components;
	unsigned char *imgbuf;
	unsigned char *dat;
	uint16_t color;
	long int location = 0;
	int bytpp=2; /* bytes per pixel, 16bits color only */
	int i,j;

	if( egi_imgbuf==NULL || fpath==NULL ) {
		printf("%s: Input egi_imgbuf or fpath is NULL!\n",__func__);
		return -1;
	}

	/* open jpg and get parameters */
	imgbuf=open_jpgImg(fpath, &width, &height, &components);
	if(imgbuf==NULL) {
		printf("egi_imgbuf_loadjpg(): open_jpgImg() fails!\n");
		return -1;
	}
	egi_dpstd("Open a jpg file with size W%dxH%d \n", width, height);

        /* ------------> Get mutex lock */
#if 1
	// printf("%s: try mutex lock...\n",__func__);
        if(pthread_mutex_lock(&egi_imgbuf->img_mutex) != 0)
        {
                egi_dperr("Fail to get mutex lock.\n");
		close_jpgImg(imgbuf);
                return -2;
        }
#endif
	/* clear old data if any, and reset params */
	egi_imgbuf_cleardata(egi_imgbuf);

	egi_imgbuf->height=height;
	egi_imgbuf->width=width;
	EGI_PDEBUG(DBG_BJP,"egi_imgbuf_loadjpg():succeed to open jpg file %s, width=%d, height=%d\n",
								fpath,egi_imgbuf->width,egi_imgbuf->height);
	/* alloc imgbuf */
	egi_imgbuf->imgbuf=calloc(1, width*height*bytpp);
	if(egi_imgbuf->imgbuf==NULL)
	{
		printf("egi_imgbuf_loadjpg(): fail to malloc imgbuf.\n");
	        pthread_mutex_unlock(&egi_imgbuf->img_mutex);
		close_jpgImg(imgbuf);
		return -3;
	}

	/* TODO: WARNING: need to be improve here: converting 8bit to 24bit color*/
	if(components==1) /* 8bit color */
	{
		printf(" egi_imgbuf_loadjpg(): WARNING!!!! components is 1. \n");
		height=height/3; /* force to be 24bit pic, however shorten the height */
	}

	/* flip picture to be same data sequence of BMP file */
	dat=imgbuf;
	for(i=height-1;i>=0;i--) /* row */
	{
		for(j=0;j<width;j++)
		{
//			location= (height-i-1)*width*bytpp + j*bytpp;
			location= (height-i-1)*width + j;

			color=COLOR_RGB_TO16BITS(*dat,*(dat+1),*(dat+2));
			*(uint16_t *)(egi_imgbuf->imgbuf+location )=color;
			dat +=3;
		}
	}

	/* <----------  put image mutex lock */
        pthread_mutex_unlock(&egi_imgbuf->img_mutex);

	close_jpgImg(imgbuf);

	return 0;
}


/*------------------------------------------------------------------------
Read PNG image data and load to an EGI_IMGBUF.
Clear data and realloc if any old data exists in egi_imgbuf before loading.

fpath:		PNG file path
eg_imgbuf:	EGI_IMGBUF  to hold the image data, in 16bits color

Note:
1. color_type,  Bit depth,       Description
   0            1,2,4,8,16       each pixel is a grayscale sample
   2            8,16             each pixel is an RGB triple.
   3            1,2,4,8          each pixel is a platte index
   4            8,16             a grayscale pixel followed by an alpha sample
   6            8,16             a RGB triple pixel followed by an alpha sample
   Referring to: https//www.w3.org/TR/PNG/

2. Input image is transformed to RGB 24bpp type by setting option PNG_TRANSFORM_EXPAND
   in png_read_png().

3. Data in egi_imgbuf will be cleared by egi_imgbuf_cleardata() before load data;

4. TODO: After transformation, it only deals with type_2(real_color PNG_COLOR_TYPE_RGB)
   and type_6(real_color with alpha channel, PNG_COLOR_TYPE_RGB_ALPHA) PNG file,
   and bit_depth MUST be 8!

Return
		0	OK
		<0	fails
------------------------------------------------------------------------------------*/
int egi_imgbuf_loadpng(const char* fpath,  EGI_IMGBUF *egi_imgbuf)
{
	if( egi_imgbuf==NULL || fpath==NULL ) {
		printf("%s: Input egi_imgbuf or fpath is NULL!\n",__func__);
		return -1;
	}

	int ret=0;
        int i,j;
        char header[8];
        FILE *fil;
        png_structp 	png_ptr;
        png_infop   	info_ptr;
        png_byte 	color_type;
        png_byte 	bit_depth;
        png_byte   	channels;
        png_byte   	pixel_depth;
        png_bytep 	*row_ptr;

        int bytpp=3; /* now only support 3 or 4 bpp, */
        int     width;
        int     height;
	long	pos;

        /* open PNG file */
        fil=fopen(fpath,"rbe");
        if(fil==NULL) {
                printf("Fail to open png file:%s.\n", fpath);
                return -1;
        }

        /* to confirm it's a PNG file */
        fread(header,1, 8, fil);
        if(png_sig_cmp((png_bytep)header,0,8)) {
                printf("Input file %s is NOT a recognizable PNG file!\n", fpath);
                ret=-2;
                goto INIT_FAIL;
        }

        /* Initiate/prepare png srtuct for read */
        png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
        if(png_ptr==NULL) {
                printf("png_create_read_struct failed!\n");
                ret=-3;
                goto INIT_FAIL;
        }
        info_ptr=png_create_info_struct(png_ptr);
        if(info_ptr==NULL) {
                printf("png_create_info_struct failed!\n");
                ret=-4;
                goto INIT_FAIL;
        }
        if( setjmp(png_jmpbuf(png_ptr)) != 0) {
                printf("setjmp(png_jmpbuf(png_ptr)) failed!\n");
                ret=-5;
                goto INIT_FAIL;
        }
        /* assign IO pointer: png_ptr->io_ptr = (png_voidp)fp */
        png_init_io(png_ptr,fil);
        /* Tells libpng that we have already handled the first 8 bytes
         * of the PNG file signature.
         */
        png_set_sig_bytes(png_ptr, 8); /* 8 is Max */

        /*  try more options for transforms...
         *  PNG_TRANSFORM_GRAY_TO_RGB, PNG_TRANSFORM_INVERT_ALPHA, PNG_TRANSFORM_BGR
         *  PNG_TRANSFORM_EXPAND: transform to RGB 24bpp.
         */
        png_read_png(png_ptr,info_ptr, PNG_TRANSFORM_EXPAND|PNG_TRANSFORM_GRAY_TO_RGB, 0);
        /* read png info: size, type, bit_depth
         * Contents of row_info:
         *  png_uint_32 width      width of row
         *  png_uint_32 rowbytes   number of bytes in row
         *  png_byte color_type    color type of pixels
         *  png_byte bit_depth     bit depth of samples
         *  png_byte channels      number of channels (1-4)
         *  png_byte pixel_depth   bits per pixel (depth*channels)
         */
        width=png_get_image_width(png_ptr, info_ptr);
        height=png_get_image_height(png_ptr,info_ptr);
        color_type=png_get_color_type(png_ptr, info_ptr);
        bit_depth=png_get_bit_depth(png_ptr, info_ptr);
        pixel_depth=info_ptr->pixel_depth;
        channels=png_get_channels(png_ptr, info_ptr);
        EGI_PDEBUG(DBG_BJP,"PNG file '%s', Image data info after transform_expand:\n", fpath);
        EGI_PDEBUG(DBG_BJP,"Width=%d, Height=%d, color_type=%d \n", width, height, color_type);
        EGI_PDEBUG(DBG_BJP,"bit_depth=%d, channels=%d,  pixel_depth=%d \n", bit_depth, channels, pixel_depth);


        /*   Now, we only deal with type_2(real_color, PNG_COLOR_TYPE_RGB)
	 *	  and type_6(real_color with alpha channel, PNG_COLOR_TYPE_RGB_ALPHA) PNG file,
         *       and bit_depth must be 8.
         */
      if(bit_depth!=8 || (color_type !=PNG_COLOR_TYPE_RGB && color_type !=PNG_COLOR_TYPE_RGB_ALPHA) ){
                printf(" Only support PNG color_type=2(real color) \n");
                printf(" and color_type=6 (real color with alpha channel), bit_depth must be 8.\n");
                ret=-6;
                goto READ_FAIL;
        }
	printf("%s: Open '%s' with size W%dxH%d, color_type %s\n", __func__, fpath, width, height,
					(color_type==PNG_COLOR_TYPE_RGB) ? "RGB" : "RGBA" );

        /* get mutex lock */
//	printf("%s: start pthread_mutex_lock() egi_imgbuf....\n",__func__);
        if(pthread_mutex_lock(&egi_imgbuf->img_mutex) != 0)
        {
                printf("%s: fail to get mutex lock.\n",__func__);
		ret=-7;
		goto READ_FAIL;
        }

	/* free data and reset egi_imgbuf */
//	printf("%s: start egi_imgbuf_cleardata(egi_imgbuf)....\n",__func__);
	egi_imgbuf_cleardata(egi_imgbuf);

        /* alloc RBG image buffer */
        egi_imgbuf->height=height;
        egi_imgbuf->width=width;
        egi_imgbuf->imgbuf=calloc(1, width*height*2);
        if(egi_imgbuf->imgbuf==NULL) {
                printf("Fail to calloc egi_imgbuf->imgbuf!\n");
		pthread_mutex_unlock(&egi_imgbuf->img_mutex);
                ret=-7;
                goto READ_FAIL;
        }
       /* alloc Alpha channel data */
        if(color_type==6) {
                egi_imgbuf->alpha=calloc(1,width*height);
                if(egi_imgbuf->alpha==NULL) {
			free(egi_imgbuf->imgbuf); /* free imgbuf */
                        printf("Fail to calloc egi_imgbuf->alpha!\n");
			pthread_mutex_unlock(&egi_imgbuf->img_mutex);
                        ret=-8;
                        goto READ_FAIL;
                }
        }


//	printf("%s: start read RGB/alpha data into egi_imgbuf....\n",__func__);
        /* read RGB data into image buffer */
        row_ptr=png_get_rows(png_ptr,info_ptr);
        if(color_type==2) /*data: RGB*/
                bytpp=3;
        else if(color_type==6) /*data: RGBA*/
                bytpp=4;

        pos=0;
        for(i=0; i<height; i++) {
                for(j=0; j<bytpp*width; j+=bytpp ) {
                        /* row_ptr[i][j];    Red
                           row_ptr[i][j+1];  Green
                           row_ptr[i][j+2];  Blue
                           row_ptr[i][j+3];  Alpha */
                    *(egi_imgbuf->imgbuf+pos)=COLOR_RGB_TO16BITS(row_ptr[i][j], row_ptr[i][j+1],row_ptr[i][j+2]);
                    /* get alpha value(0-255):
		     * 0--transparent,100% bk imgae,
		     * 255--100% png image
   		     */
                    if(color_type==6) {
                        *(egi_imgbuf->alpha+pos)=row_ptr[i][j+3];
                    }

                    pos++;
                }
        }

	/* put image mutex */
//	printf("%s: start pthread_mutex_unlock() egi_imgbuf....\n",__func__);
	pthread_mutex_unlock(&egi_imgbuf->img_mutex);

READ_FAIL:
        png_destroy_read_struct(&png_ptr, &info_ptr,0);
INIT_FAIL:
        fclose(fil);

	return ret;
}

/*--------------------------------------------------------------------------
Save an EGI_IMGBUF to a JPG file by calling libjpeg.

@fpath:		PNG file path
@eg_imgbuf:	EGI_IMGBUF holding the image data, in 16bits color
@quality:	Quality of compression.

Return:
	0	OK
	<0	Fails
---------------------------------------------------------------*/
int egi_imgbuf_savejpg(const char* fpath,  EGI_IMGBUF *eimg, int quality)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	/* typedef char JSAMPLE; typedef JSAMPLE FAR *JSAMPROW;  */
	JSAMPROW row_buff=NULL; /* SMAE as: char *row_buff; */
	FILE *outfile;
	int i;
	unsigned int k,off;
	EGI_16BIT_COLOR color;

	if(eimg==NULL || eimg->imgbuf==NULL)
		return -1;

        if (( outfile = fopen(fpath, "wbe")) == NULL) {
                egi_dperr("open '%s' fails.", fpath);
                return -2;
        }

	/* Calloc row_buff */
	row_buff=calloc(1, eimg->width*3);  /* RGB 24bit */
	if(row_buff==NULL) {
		fclose(outfile);
		return -3;
	}

	/* Allocate and init. JPEG compression object */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	/* Specify data destination */
	jpeg_stdio_dest(&cinfo, outfile);

	/* Set parameters for compression */
	cinfo.image_width = eimg->width;
	cinfo.image_height = eimg->height;
	cinfo.input_components = 3;  /* Color components per pixel: RGB  */
	cinfo.in_color_space =JCS_RGB;

	/* Set default compression parameters */
	jpeg_set_defaults(&cinfo);

	/* Set any non-default parameters */
	/* Set quality */
	jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */ );

	/* Start compressing */
	/* Write a complete interchang-JPEG file */
	jpeg_start_compress(&cinfo, TRUE);

	while( cinfo.next_scanline < cinfo.image_height ) {
		/* Read eimg line to row_buff */
		k=0;
		for(i=0; i < eimg->width; i++) {
			off=cinfo.next_scanline*eimg->width + i;
			color = eimg->imgbuf[off];
			row_buff[k++]=(color&0xF800)>>8;  //R
			row_buff[k++]=(color&0x7E0)>>3;	  //G
			row_buff[k++]=(color&0x1F)<<3;	  //B
		}

		(void) jpeg_write_scanlines(&cinfo, &row_buff, 1);
	}

	/* Finish compresson */
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	/* Close file */
	fclose(outfile);

	/* Free row_buff */
	free(row_buff);

	return 0;
}


/*--------------------------------------------------------------------------
Save an EGI_IMGBUF to an PNG file by calling libpng.

@fpath:		PNG file path
@eg_imgbuf:	EGI_IMGBUF holding the image data, in 16bits color

TODO: Now we only deal with PNG_COLOR_TYPE_RGB and PNG_COLOR_TYPE_RGB_ALPHA type.
      and bit_depth set to be 8!

Return:
	0	OK
	<0	Fails
---------------------------------------------------------------*/
int egi_imgbuf_savepng(const char* fpath,  EGI_IMGBUF *egi_imgbuf)
{
	FILE *fp;
	int i,j;
	png_structp 	png_ptr;
	png_infop	info_ptr;
	png_byte 	bit_depth=8;  /* bit depth MUST be 8 */
        png_byte 	color_type;
        png_byte   	bytes_perpix; /* bytes per pixel */
	png_uint_32	height;
	png_uint_32	width;
	png_byte	*row_buff;	/* buffer for one row of image data */
	png_bytep 	pt;

	/* check input imgbuf */
	if(egi_imgbuf==NULL || egi_imgbuf->imgbuf==NULL
			    || egi_imgbuf->width <=0 || egi_imgbuf->height <=0 ) {
		printf("%s: Input EGI_IMGBUF data is invalid!\n",__func__);
		return -1;
	}

	/* get image params */
	height=egi_imgbuf->height;
	width=egi_imgbuf->width;
 	if (height > PNG_UINT_32_MAX/png_sizeof(png_bytep)) {
                printf("%s: File '%s' is too tall to process in memory.\n", __func__, fpath);
		return -1;
	}

	if(egi_imgbuf->alpha != NULL) {
		color_type=PNG_COLOR_TYPE_RGB_ALPHA;
		bytes_perpix=4;
	}
	else {
		color_type=PNG_COLOR_TYPE_RGB;
		bytes_perpix=3;
	}

	/* allocate row_buff */
	row_buff=calloc(1, width*bytes_perpix);
	if(row_buff==NULL) {
		printf("%s: Fail to callo row_buff!\n",__func__);
		return -1;
	}

	/* open file for write */
	fp=fopen(fpath, "wbe");
	if(fp==NULL) {
                printf("%s: Fail to open file %s for write.\n", __func__, fpath);
		free(row_buff);
		return -1;
	}

	/* create and initialize the png_struct for write */
	//printf("png_create_write_struct...\n");
	png_ptr=png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL,NULL);
	if(png_ptr==NULL) {
		printf("%s: png_create_write_struct() fails!\n",__func__);
		free(row_buff);
		fclose(fp);
		return -2;
	}

	/* allocate/initialize the image information data */
	//printf("png_create_info_struct...\n");
	info_ptr=png_create_info_struct(png_ptr);
	if(info_ptr==NULL) {
		printf("%s: png_create_info_struct() fails!\n",__func__ );
		free(row_buff);
		fclose(fp);
		png_destroy_write_struct(&png_ptr, png_infopp_NULL);
		return -3;
	}

	/* set default error handling */
	//printf("setjmp..\n");
	if( setjmp(png_jmpbuf(png_ptr)) ) {
		free(row_buff);
		fclose(fp);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return -4;
	}

	/* I/O initialization using standard C streams */
	//printf("png_init_io...\n");
	png_init_io(png_ptr, fp);

	/* set image format */
	//printf("png_set_IHDR..\n");
	png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, color_type, PNG_INTERLACE_NONE,
		 	PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	/* Optional:  gamma chunk */
	/* Optional:  comments txt */

	/* write file header information */
	//printf("png_write_info...\n");
	png_write_info(png_ptr, info_ptr);

	/* Optional:  set up transformations */


	/* ----- wrtie to file one row at a time ---- */
	pt=row_buff;
	for(i=0; i< height; i++ ) {
		/* transform image data from EGI_IMGBUF to row_buff */
		for(j=0; j< width; j++) {
		   	*pt = ((egi_imgbuf->imgbuf[i*width+j])>>11)<<3;		/* R */
		   	*(pt+1) = ((egi_imgbuf->imgbuf[i*width+j])&0x7E0)>>3;	/* G */
		   	*(pt+2) = ((egi_imgbuf->imgbuf[i*width+j])&0x1F)<<3;	/* B */

 		   	if(egi_imgbuf->alpha) {
				*(pt+3)= egi_imgbuf->alpha[i*width+j];		/* Alpha */
				pt += 4;
		  	 } else {
				pt += 3;
		   	}
		}
		/* write to file */
		png_write_rows(png_ptr, &row_buff, 1);
		/* reset pt */
		pt=row_buff;
	}

	/* finish writing */
	//printf("png_write_end...\n");
	png_write_end(png_ptr, info_ptr);
	free(row_buff);

	/* clean up */
	png_destroy_write_struct(&png_ptr,&info_ptr);
	fclose(fp);

	return 0;
}


/*--------------------------------------------------------------------------------
Roam a JPG or PNG picture in a displaying window

path:		jpg file path
step:		roaming step length, in pixel
ntrip:		number of trips for roaming.
(xw,yw):	displaying window origin, relate to the LCD coord system.
winw,winh:		width and height of the displaying window.
---------------------------------------------------------------------------------*/
int egi_roampic_inwin(const char *path, FBDEV *fb_dev, int step, int ntrip,
						int xw, int yw, int winw, int winh)
{
	int i,k;
        int stepnum;

        EGI_POINT pa,pb; /* 2 points define a picture image box */
        EGI_POINT pn; /* origin point of displaying window */
        EGI_IMGBUF *imgbuf=NULL; /*private var, no need for mutex operation */

	/* new EGI_IMGBUF */
	imgbuf=egi_imgbuf_alloc(); //new();
	if(imgbuf==NULL) {
		printf("%s: Fail to run egi_imgbuf_alloc().\n",__func__);
		return -1;
	}

	/* load jpg image to the image buffer */
        if( egi_imgbuf_loadjpg(path, imgbuf) !=0 ) {
		if( egi_imgbuf_loadpng(path,imgbuf)!=0 ) {
			printf("%s: Input file is NOT a recognizable JPG or PNG type.\n",__func__);
			egi_imgbuf_free(imgbuf);
		}
	}

        /* define left_top and right_bottom point of the picture */
        pa.x=0;
        pa.y=0;
        pb.x=imgbuf->width-1;
        pb.y=imgbuf->height-1;

        /* define a box, within which the displaying origin(xp,yp) related to the picture is limited */
        EGI_BOX box={ pa, {pb.x-winw,pb.y-winh}};

	/* set  start point */
        egi_randp_inbox(&pb, &box);

        for(k=0;k<ntrip;k++)
        {
                /* METHOD 1:  pick a random point in box for pn, as end point of this trip */
                //egi_randp_inbox(&pn, &box);

                /* METHOD 2: pick a random point on box sides for pn, as end point of this trip */
                egi_randp_boxsides(&pn, &box);
                printf("random point: pn.x=%d, pn.y=%d\n",pn.x,pn.y);

                /* shift pa pb */
                pa=pb; /* now pb is starting point */
                pb=pn;

                /* count steps from pa to pb */
                stepnum=egi_numstep_btw2p(step,&pa,&pb);
                /* walk through those steps, displaying each step */
                for(i=0;i<stepnum;i++)
                {
                        /* get interpolate point */
                        egi_getpoit_interpol2p(&pn, step*i, &pa, &pb);
			/* display in the window */
                        egi_imgbuf_windisplay( imgbuf, fb_dev, -1, pn.x, pn.y, xw, yw, winw, winh ); /* use window */
			#ifdef ENABLE_BACK_BUFFER
			fb_page_refresh(fb_dev,0);
			usleep(125000);
			#else
                        tm_delayms(55);
			#endif
                }
        }

        egi_imgbuf_free(imgbuf);

	return 0;
}


/* --------------  OBSELETE!!! see egi_alloc_search_files() in egi_utils.c ----------------
find out all jpg files in a specified directory

path:	 	path for file searching
count:	 	total number of jpg files found
fpaths:  	file path list
maxfnum:	max items of fpaths
maxflen:	max file name length

return value:
         0 --- OK
        <0 --- fails
------------------------------------------------------------------------------------------*/
int egi_find_jpgfiles(const char* path, int *count, char **fpaths, int maxfnum, int maxflen)
{
        DIR *dir;
        struct dirent *file;
        int fn_len;
	int num=0;

        /* open dir */
        if(!(dir=opendir(path)))
        {
                printf("egi_find_jpgfs(): error open dir: %s !\n",path);
                return -1;
        }

	/* get jpg files */
        while((file=readdir(dir))!=NULL)
        {
                /* find out all jpg files */
                fn_len=strlen(file->d_name);
		if(fn_len>maxflen-10)/* file name length limit, 10 as for extend file name */
			continue;
                if(strncmp(file->d_name+fn_len-4,".jpg",4)!=0 )
                         continue;
		sprintf(fpaths[num],"%s/%s",path,file->d_name);
                //strncpy(fpaths[num++],file->d_name,fn_len);
		num++;
		if(num==maxfnum)/* break if fpaths is full */
			break;
        }

	*count=num; /* return count */

         closedir(dir);
         return 0;
}


/*---------------------------------------------------
Save FB data to a BMP file.
TODO: RowSize=4*[ BPP*Width/32], otherwise padded with 0.

@fb_dev:	Pointer to an FBDEV
@fpath:		Input path to the file.

Return:
	0	OK
	<0	Fail
---------------------------------------------------*/
int egi_save_FBbmp(FBDEV *fb_dev, const char *fpath)
{
	FILE *fil;
	int rc;
	int i,j;
	int xres=fb_dev->vinfo.xres; /* line pixel number */
	int yres=fb_dev->vinfo.yres;
	EGI_16BIT_COLOR	color16b;
	PIXEL bgr;
	BITMAPFILEHEADER file_header; /* 14 bytes */
	BITMAPINFOHEADER info_header; /* 40 bytes */

	/* check input data */
	if(fb_dev==NULL || fb_dev->map_fb==NULL || fpath==NULL)
		return -1;

   printf("file header size:%zd, info header size:%zd\n",sizeof(BITMAPFILEHEADER),sizeof(BITMAPINFOHEADER));

	/* fill in file header */
	memset(&file_header,0,sizeof(BITMAPFILEHEADER));
	file_header.cfType[0]='B';
	file_header.cfType[1]='M';
	file_header.cfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)
					+fb_dev->screensize/2*3;  /* 16bits -> 24bits */
	file_header.cfReserved=0;
	file_header.cfoffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
/*
        char cfType[2];//文件类型，"BM"(0x4D42)
        long cfSize;//文件大小（字节）
        long cfReserved;//保留，值为0
        long cfoffBits;//数据区相对于文件头的偏移量（字节）
*/

	/* Fill in info header */
	memset(&info_header,0,sizeof(BITMAPINFOHEADER));
	info_header.ciSize[0]=40;
	info_header.ciWidth=240;
	info_header.ciHeight=320;
	info_header.ciPlanes[0]=1;
	info_header.ciBitCount=24;
	*(long *)info_header.ciSizeImage=fb_dev->screensize/2*3; /* to be multiple of 4 */
/*
        char ciSize[4];//BITMAPFILEHEADER所占的字节数
        long ciWidth;//宽度
        long ciHeight;//高度
        char ciPlanes[2];//目标设备的位平面数，值为1
        int ciBitCount;//每个像素的位数
        char ciCompress[4];//压缩说明
        char ciSizeImage[4];//用字节表示的图像大小，该数据必须是4的倍数
        char ciXPelsPerMeter[4];//目标设备的水平像素数/米
        char ciYPelsPerMeter[4];//目标设备的垂直像素数/米
        char ciClrUsed[4]; //位图使用调色板的颜色数
        char ciClrImportant[4]; //
*/

	/* open file for write */
	fil=fopen(fpath,"we");
	if(fil==NULL) {
		printf("%s: Fail to open %s for write.\n",__func__,fpath);
		return -1;
	}

	/* write file header */
        rc = fwrite((char *)&file_header, 1, sizeof(BITMAPFILEHEADER), fil);
        if (rc != sizeof(BITMAPFILEHEADER)) {
		printf("%s: Fail to write file header to %s.\n",__func__,fpath);
		fclose(fil);
		return -2;
         }
	/* write info header */
        rc = fwrite((char *)&info_header, 1, sizeof(BITMAPINFOHEADER), fil);
        if (rc != sizeof(BITMAPINFOHEADER)) {
		printf("%s: Fail to write info header to %s.\n",__func__,fpath);
		fclose(fil);
		return -3;
         }

	/* RGB color saved in order of BGR in BMP file
	 * From bottom to up
	 */
	for(i=0;i<yres;i++) /* iterate line number */
	{
		for(j=0;j<xres;j++) /* iterate pixels in a line, xres to be multiple of 4 */
		{
			color16b=*(uint16_t *)(fb_dev->map_fb+fb_dev->screensize-(i+1)*xres*2 + j*2); /* bpp=2 */
			bgr.red=(color16b>>11)<<3;
			bgr.green=(color16b&0b11111100000)>>3;
			bgr.blue=(color16b << 11)>>8;

		        rc = fwrite((char *)&bgr, 1, sizeof(PIXEL), fil);
       			if (rc != sizeof(PIXEL)) {
				printf("%s: Fail to write BGR data to %s, i=%d. \n",__func__,fpath,i);
				fclose(fil);
				return -4;
			}
         	}
	}

	fclose(fil);
	return 0;
}


/*---------------------------------------------------
Save FB data to a PNG file.

@fb_dev:	Pointer to an FBDEV
@fpath:		Input path to the file.

Return:
	0	OK
	<0	Fail
---------------------------------------------------*/
int egi_save_FBpng(FBDEV *fb_dev, const char *fpath)
{
	int ret=0;
//	EGI_IMGBUF* imgbuf=NULL;

	if(fb_dev==NULL || fb_dev->map_fb==NULL || fpath==NULL)
		return -1;

        EGI_IMGBUF *imgbuf=egi_imgbuf_alloc();
        if(imgbuf==NULL)
                return -2;

	/* create an EGI_IMG with same size as FB */
//        imgbuf=egi_imgbuf_create(fb_dev->vinfo.yres, fb_dev->vinfo.xres, 255, 0);

	/* assign width and height as per FB POSITION ROTATION */
	imgbuf->width=fb_dev->pos_xres;
	imgbuf->height=fb_dev->pos_yres;

	/* save imgbuf to PNG file */
//	free(imgbuf->imgbuf);
        imgbuf->imgbuf=(EGI_16BIT_COLOR *)fb_dev->map_fb; /* !!! WARNING: temporary refrence !!! */
	/* imgbuf->alpha keeps NULL, as FB buffer has no alpha. */

	/* rotate imgbuf */
	imgbuf=egi_imgbuf_rotate(imgbuf, 90*fb_dev->pos_rotate);
	if(imgbuf==NULL) {
		printf("%s: Fail to call egi_imgbuf_rotate()!\n",__func__);
		return -3;
	}

	/* save to png */
        if(egi_imgbuf_savepng(fpath, imgbuf) != 0 )
		ret=-2;

	//imgbuf->imgbuf=NULL;	/* !!! Dereference, return ownership to fb_dev */
	egi_imgbuf_free(imgbuf);

	return ret;
}

