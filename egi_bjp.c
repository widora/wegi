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
2021-09-08:
	1. Add: egi_save_FBjpg()
2022-02-22:
	1. egi_imgbuf_loadjpg(): Consider color space case: RGBA(components=4).
2022-03-13:
	1. open_jpgImg(): Precalc image size, and give up if >EGI_IMAGE_SIZE_LIMIT.
2022-03-14:
	1. egi_imgbuf_loadpng(): Check size for mem allocation limit.
2022-03-21:
	1. Add struct EGI_PICINFO and its functions:
	   egi_picinfo_calloc() egi_picinfo_free()  egi_parse_jpegExif()--->egi_parse_jpegFile()
2022-03-22:
	1. open_jpgImg(): Set my_error_exit handler to avoid abrupt exit.
	2. Add egi_check_jpgfile(), egi_simplecheck_jpgfile()
2022-03-26:
	1.egi_imgbuf_loadpng(): Check jstep to force function to continue when
	  png_read_png() throws error. specially for partial (interlaced) PNG file.

Modified and appended by Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
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


/*------------------------------------------------------------------------
Check whether the input file is a complete JPEG file, which starts with
SOI(0xFF, 0xD8) and ends with EOI(0xFF, 0xD9), and all segments are in
complete size.

Note:
1. JPEG file structure:
    segment + segment + segment + .....
    Where: Segment = Marker + Payload(Data)
           Marker =  0xFF + MakerCode(0xXX)
  1.1 A JPEG file starts with segement SOI(Start of image) and ends
      segment EOI(End of image).
  1.2 Structure of a segment:
     0xFF+MakerCode(1 byte)+TagDataSize(2 bytes)+TagData(n bytes)
     ( !!!--- Execption ---!!! Segment SOI and EOI have NO TagDataSize and TagData )
  1.3 Structure of SOS(StartOfLine) segment:
	SOS_TAG  0xFF+0xDA+TagDataSize(2 bytes)+TagData  !!! TagDataSize NOT count compressed image data !!!
	//then followed by compressed image data...

		  !!! ---- CAUTION ----- !!!
   In compressed image data, there are ALSO some available Markers!
   and should be treated specially..
	0xFF00:     Ignore 00
	0xFFD9:     EOI
	0xFFD0-D7   RSTn
	..

2. Check invalid JPEG structure (TODO more)
  2.1 No SOI or EOI marker.
  2.2 More than 1 SOI marker.
  ...
  TODO: 

Return:
	0	A valid and complete JPEG file
	>0	An incomplete JPEG file (For most case)
		It may be a pending image during transmission.
	<0	An invalid or corrupted JPEG file
		see NOTE 2.
----------------------------------------------------------------------------*/
int egi_check_jpgfile(const char* fpath)
{
	FILE *fil;
	int err=0;
	unsigned char marker[2];
	unsigned char tmpbuf[4];
	unsigned int seglen;    /* Segment length, including 2Byte data length value
				 * excluding 2 bytes of the mark, 0xFFFE etc.
				 */
	int ret;
	unsigned long off=0;
	bool found_SOS=false;    /* Marker SatrtOfScan is found, and fposs set to
				  * beginning of compressed image data
			          */
	bool found_EOI=false;
	long pos_EOI;		/* File position at end of EOI */

        /* 1. Open JPEG file */
        fil=fopen(fpath, "rb");
        if(fil==NULL)
		return JPEGFILE_INVALID;

	/* 2. Check Start Marker SOI(0xFF 0xD8) */
	if( fseek(fil, 0, SEEK_SET)!=0 ) {
		err=JPEGFILE_INCOMPLETE; goto END_FUNC;
	}
	if( fread(marker, 1, 2, fil)!=2 ) {
		err=JPEGFILE_INCOMPLETE; goto END_FUNC;
	}
	if( marker[0]!=0xFF || marker[1]!=0xD8 ) {
		egi_dpstd(DBG_YELLOW"No marker SOI!\n"DBG_RESET);
		err=JPEGFILE_INVALID; goto END_FUNC;
	}

	/* 3. Check End Marker EOI(0xFF, 0xD9) */
	if( fseek(fil, -2, SEEK_END)!=0 ) {
		err=JPEGFILE_INCOMPLETE; goto END_FUNC;
	}
	if( fread(marker, 1, 2, fil)!=2 ) {
		err=JPEGFILE_INCOMPLETE; goto END_FUNC;
	}
	if( marker[0]!=0xFF || marker[1]!=0xD9 ) {
		egi_dpstd(DBG_YELLOW"No marker EOI!\n"DBG_RESET);
		err=JPEGFILE_INCOMPLETE;
		//goto END_FUNC;  go on to check if invalid
	}
	else if ( marker[0]==0xFF || marker[1]==0xD9 ) {
		found_EOI=true;
		pos_EOI=ftell(fil);
	}

	/* 4. Check other segments */
	/* fseek to end of SOI */
	if( fseek(fil, 2, SEEK_SET)!=0 ) {
                err=JPEGFILE_INCOMPLETE; goto END_FUNC;
        }
	//off=2;
	while(1) {
		ret=fread(marker, 1, 2, fil);
		if(ret<2) {
			if(!feof(fil)) { /* Maybe the last 1 byte. OK */
				egi_dperr("Fail to read markers");
				err=JPEGFILE_INCOMPLETE;
			}
			//if(ret>0) off +=ret;
			goto END_FUNC;
		}
		//off +=2;

		/* M1. Pass non_marker data. */
		if(marker[0]!=0xFF && marker[1]!=0xFF) {
//			printf("M1: 0x%02X%02X\n", marker[0], marker[1]);

			/* ONLY possible in compressed data after SOS marker */
			if(!found_SOS) {
				egi_dpstd(DBG_RED"M1: Invalid JPEG structure, data error!\n"DBG_RESET);
				err=JPEGFILE_INVALID; goto END_FUNC;
			}
			//off +=2;
			continue;
		}
		/* M2. Find  0xFF at marker[1] */
		else if( marker[0]!=0xFF && marker[1]==0xFF ) {
//			printf("M2: 0x%02X%02X\n", marker[0], marker[1]);

			/* ONLY possible in compressed data after SOS marker */
			if(!found_SOS) {
				egi_dpstd(DBG_RED"M2: Invalid JPEG structure, data error!\n"DBG_RESET);
				err=JPEGFILE_INVALID; goto END_FUNC;
			}

			/* rewind back one byte */
			if( fseek(fil, -1, SEEK_CUR)!=0 ) {
				err=JPEGFILE_INCOMPLETE; goto END_FUNC;
			}
			//off -=1;
			continue;
		}
		/* M3. Find a mark */
		else if(marker[0]==0xFF && marker[1]!=0xFF) {
//			egi_dpstd("Marker Code: 0xFF%02X\n", marker[1]);

			/* SOS: Start Of Scan */
		        /* Not here, to set JUST after skip seglen to start point of image data */
			//if(marker[1]==0xDA) {
			//    found_SOS=true;
			//}

			/* If the second SOI marker */
			if( marker[1]== 0xD8 ) {
				egi_dpstd(DBG_RED"Invalid JPEG structure, it has more than one SOI markers!\n"DBG_RESET);
				err=JPEGFILE_INVALID; goto END_FUNC;
			}

			/* EOI found in compressed data, not at eof. */
			if( marker[1]== 0xD9 && found_SOS && pos_EOI!=ftell(fil) ) {  /* NOT feof() */
                                egi_dpstd(DBG_YELLOW"EOI is found in compressed data! 2 pics jointed?\n"DBG_RESET);
                                //err=JPEGFILE_INVALID; goto END_FUNC;
                        }

			/* Get segment data length */
			ret=fread(tmpbuf, 1, 2, fil);
			if(ret<2) {
				/* OK, maybe EOF!  */
				if(!feof(fil)) {
				    egi_dperr(DBG_RED"Fail to read length of segment, ret=%d/2."DBG_RESET, ret);
				    err=JPEGFILE_INCOMPLETE;
				}
				//if(ret>0) off +=ret;
				goto END_FUNC;
			}
			//off +=2;

			/* If found SOS,which is the LAST segment before EOI, then the compressed image data is followed,
			 * In compressed image data, 0xFFxx MAY have NO TagData at all! Example: 0xFF00
			 * in this case data in tmpbuf[] is meaningless, so DO NOT skip aft found SOS!
			 */
			if(!found_SOS) {
				/* Bigendian here */
				seglen = (tmpbuf[0]<<8)+tmpbuf[1];  /* !!! including 2bytes of lenght value !!! */

				/* Seek to end of the segment */
				if( fseek(fil, seglen-2, SEEK_CUR)!=0 ) {
					err=JPEGFILE_INCOMPLETE; goto END_FUNC;
				}
				//off +=seglen-2;
			}

			/* Set found_SOS at end of case M1.  */
			if(marker[1]==0xDA) /* StartOfScan */
				found_SOS=true;

			continue;
		}
		/*  M4. Padding 0xFFFF  */
		else {   /* (marker[0]==0xFF && marker[1]==0xFF) */
			/* ????? rewind back one byte, in case NEXT bytes is NOT 0xFF */
			if( fseek(fil, -1, SEEK_CUR)!=0 ) {
				err=JPEGFILE_INCOMPLETE; goto END_FUNC;
			}

			off -=1;
			continue;
		}
	}

END_FUNC:
	fclose(fil);
	return err;
}


/*-----------------------------------------------------------
Check ONLY marker SOI(StartOfImage) and EOI(EndOfImage) to
see whether the JPEG file is invalid/valid/complete.

Return:
	0	A valid and complete JPEG file
	>0	An incomplete JPEG file (For most case)
		It may be a pending image during transmission.
	<0	An invalid or corrupted JPEG file
-------------------------------------------------------------*/
int egi_simpleCheck_jpgfile(const char* fpath)
{
	FILE *fil;
	int err=JPEGFILE_COMPLETE;
	unsigned char marker[2];

        /* 1. Open JPEG file */
        fil=fopen(fpath, "rb");
        if(fil==NULL)
		return JPEGFILE_INVALID;

	/* 2. Check Start Marker SOI(0xFF 0xD8) */
	if( fseek(fil, 0, SEEK_SET)!=0 ) {
		err=JPEGFILE_INCOMPLETE; goto END_FUNC;
	}
	if( fread(marker, 1, 2, fil)!=2 ) {
		err=JPEGFILE_INCOMPLETE; goto END_FUNC;
	}
	/*  JPEG SOI Mark */
	if( marker[0]!=0xFF || marker[1]!=0xD8 ) {
		egi_dpstd(DBG_YELLOW"No marker SOI!\n"DBG_RESET);
		err=JPEGFILE_INVALID; goto END_FUNC;
	}

	/* 3. Check End Marker EOI(0xFF, 0xD9) */
	if( fseek(fil, -2, SEEK_END)!=0 ) {
		err=JPEGFILE_INCOMPLETE; goto END_FUNC;
	}
	if( fread(marker, 1, 2, fil)!=2 ) {
		err=JPEGFILE_INCOMPLETE; goto END_FUNC;
	}
	/* JPEG EOI Mark*/
	if( marker[0]!=0xFF || marker[1]!=0xD9 ) {
		egi_dpstd(DBG_YELLOW"No marker EOI!\n"DBG_RESET);
		err=JPEGFILE_INCOMPLETE; goto END_FUNC;
	}

END_FUNC:
	fclose(fil);
	return err;
}


/*----------------------------------------------------------------
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
----------------------------------------------------------------*/
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

TODO:
1. XXX If too big size file! Check cinfo for memory requirement then...
   OR it will abort abruptly!
      OK. Check EGI_IMAGE_SIZE_LIMIT
2. XXX If JPEG datastream contains no image, it will abort abruptly!
      Ok, set my_error_exit handler.

return:
	=NULL fail
	>0 decompressed image buffer pointer
--------------------------------------------------------------*/
//#include <setjmp.h>
/** 1. Here's the extended error handler struct */
struct my_error_mgr {
  struct jpeg_error_mgr pub;    /* "public" fields */
  jmp_buf setjmp_buffer;        /* for return to caller */
};
typedef struct my_error_mgr * my_error_ptr;

/** 2. Here's the routine that will replace the standard error_exit method */
METHODDEF(void) my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

/* 3. open_jpgImg */
unsigned char * open_jpgImg(const char * filename, int *w, int *h, int *components )
{
	unsigned char header[2];
	struct jpeg_decompress_struct cinfo;
        //struct jpeg_error_mgr jerr;
	struct my_error_mgr jerr;
        FILE *infile;
        unsigned char *buffer=NULL;
        unsigned char *pt=NULL;
	//int ret;
	//unsigned int cnt_scanline;  /* Counter scanle */

        if (( infile = fopen(filename, "rbe")) == NULL) {
                egi_dperr("open %s failed", filename);
                return NULL;
        }

	/* To confirm JPEG/JPG type, simple way. */
	 /* start of image 0xFF D8 */
	if( fread(header,1,2,infile)<2 ) {
		fclose(infile);
		return NULL;
	}
	if(header[0] != 0xFF || header[1] != 0xD8) {
		fprintf(stderr, "SOI(FF D8): 0x%x 0x%x, '%s' is NOT a recognizable JPG/JPEG file!\n",
										header[0], header[1],filename);
		fclose(infile);
		return NULL;
	}
#if 0 /* Omit for abbreviated jpeg file */
	fseek(infile,-2,SEEK_END); /* end of image 0xFF D9 */
	fread(header,1,2,infile);
	if(header[0] != 0xFF || header[1] != 0xD9) {
		fprintf(stderr, "EOI(FF D9): 0x%x 0x%x, '%s' is NOT a recognizable JPG/JPEG file!\n",
									header[0], header[1], filename);
		fclose(infile);
		return NULL;
	}
#endif
	/* Must reset seek for jpeg decompressor! */
	fseek(infile,0,SEEK_SET);

	/* Fill in the standard error-handling methods */
        cinfo.err = jpeg_std_error(&jerr.pub);

#if 1  /* Set MY error handler to avoid abrupt exit */
	jerr.pub.error_exit = my_error_exit;
	/* Establish the setjmp return context for my_erro_exit ot user. */
	if( setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return NULL;
	}
#endif

	/* Create decompressor */
        jpeg_create_decompress(&cinfo);

	/* Prepare data source */
        jpeg_stdio_src(&cinfo, infile);

//	egi_dpstd("Jpeg read header...\n");
        jpeg_read_header(&cinfo, TRUE);

	/* Calc dimensioins first MidasHK_2022_03_13 */
	jpeg_calc_output_dimensions(&cinfo);
//	egi_dpstd("Pre_calc to get output image dimension: W%dxH%d\n", cinfo.output_width, cinfo.output_height);
	if( cinfo.output_width*cinfo.output_height > EGI_IMAGE_SIZE_LIMIT ) {
		egi_dpstd(DBG_RED"Image size too big W%dxH%d (>EGI_IMAGE_SIZE_LIMIT), give up!\n"DBG_RESET,
							cinfo.output_width, cinfo.output_height);
		fclose(infile);
		jpeg_destroy_decompress(&cinfo);
		return NULL;
	}

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
//	egi_dpstd("Start decompress...\n");
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
	//cnt_scanline=0;
	        while (cinfo.output_scanline < cinfo.output_height) {
                jpeg_read_scanlines(&cinfo, &pt, 1);
                //ret=jpeg_read_scanlines(&cinfo, &pt, 1);
		//if(ret>0)
		//    cnt_scanline +=ret;
                pt += cinfo.output_width * cinfo.output_components;
        }
	//egi_dpstd("cnt_scanline=%d v.s. output_height=%d\n", cnt_scanline, cinfo.output_height);

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

        fclose(infile);

	return buffer;
}


/*   release mem for decompressed jpeg image buffer */
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
	egi_dpstd("Open a jpg file with size W%dxH%d, components=%d \n", width, height, components);


	/* NOW: Only support component number 1,3,4 as GRAY, RGB, RGBA */
	if(components!=1 && components!=3 && components!=4) {
		egi_dpstd("Only supports component number:1,3,4. Sorry.\n");
		close_jpgImg(imgbuf);
		return -1;
	}

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
	/* Allocate imgbuf */
	egi_imgbuf->imgbuf=calloc(1, width*height*bytpp);
	if(egi_imgbuf->imgbuf==NULL)
	{
		egi_dperr("fail to malloc imgbuf.");
	        pthread_mutex_unlock(&egi_imgbuf->img_mutex);
		close_jpgImg(imgbuf);
		return -3;
	}

	/* Allocate alpha: MidasHK_2022_02_22 */
	if( components==4 ) {
		egi_imgbuf->alpha=calloc(1, width*height);
		if(egi_imgbuf->alpha==NULL)
		{
			egi_dperr("fail to malloc alpha.");
			free(egi_imgbuf->imgbuf);
		        pthread_mutex_unlock(&egi_imgbuf->img_mutex);
			close_jpgImg(imgbuf);
			return -3;
		}
	}


	/* Conver/assign color data to imgbuf */
	dat=imgbuf;
	for(i=height-1;i>=0;i--) /* row */
	{
		for(j=0;j<width;j++)
		{
//			location= (height-i-1)*width*bytpp + j*bytpp;
			location= (height-i-1)*width + j;

			/* Gray */
			if(components==1) {
				color=COLOR_RGB_TO16BITS(*dat,*dat,*dat);
				*(uint16_t *)(egi_imgbuf->imgbuf+location )=color;
			}
			/* RGB or RGBA */
			else if(components==3 || components==4) {
				color=COLOR_RGB_TO16BITS(*dat,*(dat+1),*(dat+2));
				*(uint16_t *)(egi_imgbuf->imgbuf+location )=color;
			}

			/* ALPHA */
			if(components==4) /* ALPHA for RGBA */
				*(egi_imgbuf->alpha+location)=*(dat+3);

			dat +=components; //dat +=3;
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

5. XXXTODO: For interlaced PNG file.  ----OK, check jstep after png_read_png() throws error

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
	static	int jstep=0;
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
	bool	check_size=false;

PNG_INIT:  /* Return here aft check_size  */

        /* open PNG file */
        fil=fopen(fpath,"rbe");
        if(fil==NULL) {
                egi_dperr("Fail to open png file:%s", fpath);
                return -1;
        }

        /* to confirm it's a PNG file */
        if( fread(header,1, 8, fil)<8 ) {
		fclose(fil);
		return -2;
	}
        if(png_sig_cmp((png_bytep)header,0,8)) {
                printf("Input file %s is NOT a recognizable PNG file!\n", fpath);
                ret=-2;
                goto INIT_FAIL;
        }

//INIT_PNG: NOT HERE!  OR setjmp() will fail aft check_size.
        /* Initiate/prepare png srtuct for read */
        png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
        if(png_ptr==NULL) {
                egi_dperr("png_create_read_struct failed!");
                ret=-3;
                goto INIT_FAIL;
        }

	/* Note: If not setjmp, function will abort at error. */
	//if(!check_size) { --- NOPE, MUST re_setjmp here.
//	egi_dpstd(" --------> setjmp \n");
        if( setjmp(png_jmpbuf(png_ptr)) != 0) {
		/* If error happens afterward, it will longjmp here! and values of local vars also reset. */
//                egi_dperr("setjmp(png_jmpbuf(png_ptr)) failed! jstep=%d\n",jstep);
		if(jstep==1)
		   goto AFT_READ_PNG;
                ret=-5;
                goto READ_FAIL;
        }

        info_ptr=png_create_info_struct(png_ptr);
        if(info_ptr==NULL) {
                egi_dperr("png_create_info_struct failed!");
                ret=-4;
                goto READ_FAIL; //INIT_FAIL;
        }

        /* assign IO pointer: png_ptr->io_ptr = (png_voidp)fp */
        png_init_io(png_ptr,fil);
        /* Tells libpng that we have already handled the first 8 bytes
         * of the PNG file signature.
         */
        png_set_sig_bytes(png_ptr, 8); /* 8 is Max */

	/* Here get image size  =0 !! */
	//width=png_get_image_width(png_ptr, info_ptr);
        //height=png_get_image_height(png_ptr,info_ptr);

#if 1	///////////* Size checking for mem allocation LIMIT. --- MidasHK_2022_03_14 *////////

	/* This is NOT suitable for mem allocation LIMIT purpose! */
	//png_set_user_limits(png_ptr, 2048, EGI_IMAGE_SIZE_LIMIT/2048); /* user_width_max, user_height_max */

        if( !check_size ) {
	   /* png_read_info() gives us all of the information from the
    	    * PNG file before the first IDAT (image data chunk).
            */
	   png_read_info(png_ptr, info_ptr);
           egi_dpstd(DBG_GREEN"png_read_info get image size W%uxH%u\n"DBG_RESET,
                                                 (unsigned int)info_ptr->width, (unsigned int)info_ptr->height);
           if (info_ptr->height > PNG_UINT_32_MAX/png_sizeof(png_bytep)) {
                  png_error(png_ptr, "Image is too high to process with png_read_png()");
		  ret=-5;
		  goto READ_FAIL;
	   }
	   if(info_ptr->width*info_ptr->height > EGI_IMAGE_SIZE_LIMIT ) {
                egi_dpstd(DBG_RED"Image size too big W%uxH%u (>EGI_IMAGE_SIZE_LIMIT), give up!\n"DBG_RESET,
                                                 (unsigned int)info_ptr->width, (unsigned int)info_ptr->height);
		ret=-5;
		goto READ_FAIL;
	   }

	   /* Finish size checking */
           png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
	   fclose(fil);

	   check_size=true;
	   goto PNG_INIT;
	}
#endif  /////////////////////////////////////////////////

        /*  try more options for transforms...
         *  PNG_TRANSFORM_GRAY_TO_RGB, PNG_TRANSFORM_INVERT_ALPHA, PNG_TRANSFORM_BGR
         *  PNG_TRANSFORM_EXPAND: transform to RGB 24bpp.
         */
//	egi_dpstd("----->png_read_png\n");
	jstep=1;
        png_read_png(png_ptr,info_ptr, PNG_TRANSFORM_EXPAND|PNG_TRANSFORM_GRAY_TO_RGB, 0);

AFT_READ_PNG:
	jstep=2;

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


	/* TODO: Get comments in the picture */
        //if (png_get_text(read_ptr, end_info_ptr, &text_ptr, &num_text) > 0)


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
        png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
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
	info_header.ciWidth=fb_dev->pos_xres; //240;  MidasHK_2022_03_08 */
	info_header.ciHeight=fb_dev->pos_yres; //320;
	info_header.ciPlanes[0]=1;
	info_header.ciBitCount=24;
//	*(long *)info_header.ciSizeImage=fb_dev->screensize/2*3; /* to be multiple of 4 */
	*info_header.ciSizeImage=fb_dev->screensize/2*3; /* to be multiple of 4 */
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


/*---------------------------------------------------
Save FB data to a JPEG file.

@fb_dev:	Pointer to an FBDEV
@fpath:		Input path to the file.
@quality:	Quality of compression.

Return:
	0	OK
	<0	Fail
---------------------------------------------------*/
int egi_save_FBjpg(FBDEV *fb_dev, const char *fpath, int quality)
{
	int ret=0;

	if(fb_dev==NULL || fb_dev->map_fb==NULL || fpath==NULL)
		return -1;

        EGI_IMGBUF *imgbuf=egi_imgbuf_alloc();
        if(imgbuf==NULL)
                return -2;

	/* Assign width and height as per FB POSITION ROTATION */
	imgbuf->width=fb_dev->pos_xres;
	imgbuf->height=fb_dev->pos_yres;

	/* Get refer to map_fb */
        imgbuf->imgbuf=(EGI_16BIT_COLOR *)fb_dev->map_fb; /* !!! WARNING: temporary refrence !!! */
	/* imgbuf->alpha keeps NULL, as FB buffer has no alpha. */

	/* Rotate imgbuf as per pos_rotate, imgbuf as copied. */
	imgbuf=egi_imgbuf_rotate(imgbuf, 90*fb_dev->pos_rotate);
	if(imgbuf==NULL) {
		printf("%s: Fail to call egi_imgbuf_rotate()!\n",__func__);
		return -3;
	}

	/* Save to jpg */
        if(egi_imgbuf_savejpg(fpath, imgbuf, quality) != 0 )
		ret=-4;

	/* Free imgbuf */
	egi_imgbuf_free(imgbuf);

	return ret;
}


////////////////////////  Struct EGI_PICINFO and Functions //////////////////////////////////////
#include "egi_cstring.h"

/*-----------------------------------
Callocate an empty EGI_PICINFO.
Return:
        !NULL   OK
        NULL    Fails
------------------------------------*/
EGI_PICINFO *egi_picinfo_calloc(void)
{
        return calloc(1, sizeof(EGI_PICINFO));
}

/*-----------------------------------
Free an EGI_PICINFO.
------------------------------------*/
void egi_picinfo_free(EGI_PICINFO **info)
{
        if(info==NULL || *info==NULL)
                return;

        free( (*info)->comment );
        free( (*info)->app14com );

        free( (*info)->CameraMaker );
        free( (*info)->CameraModel );
	free( (*info)->software );
        free( (*info)->ImageDescription );
	free( (*info)->DateTaken );
	free( (*info)->DateLastModify );
        free( (*info)->title );
        free( (*info)->xcomment );
        free( (*info)->author );
        free( (*info)->keywords );
        free( (*info)->subject );
	free( (*info)->copyright );

        egi_imgbuf_free2(&(*info)->thumbnail);

        free((*info));
        *info=NULL;
}

/*-------------------------------------------------------
Read and parse Exif data(image information) in a jpeg file.
Also read Comment segment of JPEG.

Note:
0.  Marker: Starting 2Bs token of a jpeg segement as 0xFF**.
    Tag:  Starting 2Bs token of an Exif IFD directory entry.

1. While(1) search for Markers starting with 0xFF token,
   if found, then switch() to parse the segment, after
   break/end of the case, it skips to the end of the segment
   and start to search next Marker...

2. Exif IFD parse sequence: IFD0 --> SubIFD --> IFD1
   See E1.5.3~E1.5.7 for skipping/shifting to IFD directory.


Return:
	0	OK
	<0 	Fails


	============  JPEG and EXIF  ==============

Reference:
1. https://www.media.mit.edu/pia/Research/deepview/exif.html
2. https:/www.exiftool.org/TagNames/EXIF.html
3. https://www.disktuna.com/list-of-jpeg-markers/

Note:
1. JPEG file structure:
    segment + segment + segment + .....
    Where: Segment = Marker + Payload(Data)
           Marker =  0xFF + MakerCode(0xXX)

  1.1 A JPEG file starts with segement SOI(Start of image) and ends
      segment EOI(End of image).

  1.2 Structure of a segment:
     0xFF+MakerCode(1 byte)+TagDataSize(2 bytes)+TagData(n bytes)
     ( !!!--- Execption ---!!! Segment SOI and EOI have NO TagDataSize and TagData )
  1.3 Structure of SOS(StartOfLine) segment:
        SOS_TAG  0xFF+0xDA+TagDataSize(2 bytes)+TagData  !!! TagDataSize NOT count compressed image data !!!
        //then followed by compressed image data...

                  !!! ---- CAUTION ----- !!!
   In compressed image data, there are ALSO some available Markers!
   and should be treated specially..
        0xFF00:     Ignore 00
        0xFFD9:     EOI
        0xFFD0-D7   RSTn
        ....

  1.4 0xFF padding: there maybe 0xFFs between segments.

  1.5 Exif uses Marker APP1(0xFFE1) for application.

2. Typical JPEG markers:

Code     Marker        Name
-----------------------------------------
SOI      0xFF, 0xD8    Start of image
SOF0     0xFF, 0xC0    Start of frame N,N indicates which compression process
SOF2     0xFF, 0xC2
SOFn     ...
DHT	 0xFF, 0xC4    Define Huffman Tables
DQT      0xFF, 0XDB    Define Quantization Table(s)
DRI	 0xFF, 0xDD    Define Restart Interval
SOS      0xFF, 0xDA    Start of Scan
DNL      0xFF, 0xDC    Define number of lines (Not common)
APPn     0xFF, 0xEn    Application specific (n=0-15)
--------
APP0	 0xFF, 0xE0    JFIF-JFIF JPEG image, AVI1-Motion JPEG(MJPG)
APP1     0xFF, 0xE1    EXIF Metadata, TIFF IFD format, JPEG Thumbnail(160x120), Adobe XMP
APP2     0xFF, 0xE2    ICC color profile, FlashPix
APP14    0xFF, 0xEC    Picture info(older digicams), Photoshop save for web:Ducky
--------
JPG7	 0xFF, 0xF7    Lossless JPEG
JPG8	 0xFF, 0xF8    Lossless JPEG Extension Parameters
COM      0xFF, 0xFE    Comments
EOI      0xFF, 0xD9    End of image
(Note: the marker 0xFFE0 - 0xFFEF is named "Application Marker", theyr are
  used for user application, and not necessary for JPEG image.)

2A. Frame Header Structure(SOFn)
    Marker: 0xFFC0-3,5-7,9-B,D-F   	2Bytes
    Frame header length		   	2Bytes
    Sample precision               	1Byte
    Number of lines(Height Y)      	2Bytes
    Number of samples per line(WidthX)  2Bytes
    Number of components in frame       1Byte
    Frame component specification...

3. Exif uses Marker APP1(0xFFE1) for application, and usually starts as:
   Segment_SOI + Segment_APP1 + other_segments ....
   (Exif data: inserts image/digicam information and thumbnail image to JPEG
   while in conformatiy with JPEG specification.)

4. JFIF Exif data structure      (IFD: ImageFileDirectory)
   DATA		       MEANING
   -------------------------------
   FFE1                APP1 Marker
   SSSS                APP1 Data Size (Start of data)
   45786966 0000       Exif Header
   49492A00 08000000   TIFF Header (Tag Image File Format)
   ------------------------------- (IFD: ImageFileDirectory = list of EXIF Tags )
   XXXX....               IFD0(main image) | Directory  ----> Tag 0x8769 link to SubIFD
   LLLLLLL		    		   | Link to IFD1 ----> IFD1
   XXXX....               Data area of IFD0

   XXXX....               Exif SubIFD      | Directory
   00000000                                | End of Link
   XXXX....               Data area Exif SubIFD

   XXXX....               IFD1(thumbnai)   | Directory
   00000000                                | End of Link
   XXXX....               Data area of IFD1
   ------------------------------
   FFD8XXXX...XXXXFFD9    Thumbnail JPEG image(ALSO starts with 0xFFD8, end with 0xFFD9)

  4.1 IFD directory structure
    ---------------------------------------
    E(2Bs) : Number of directory entry
    T(2Bs)+F(2Bs)+N(4Bs)+D(4Bs)   Entry0  (T-Tag, F-Format, N-NumberOfComponent, D-DataValue)
    T(2Bs)+F(2Bs)+N(4Bs)+D(4Bs)   Entry1
	    ... ...
    T(2Bs)+F(2Bs)+N(4Bs)+D(4Bs)   Entry2
    L(4Bs)   Offset to next IFD

Journal:
2022-03-15: Create the file.
2022-03-16: Parse Exif IFD directory
2022-03-17: Parse IFD0 tags 0x9c9b-0x9c9f (for Windows Explorer)
2022-03-18: Parse SOFn for image size.
2022-03-19: Add EGI_PICINFO and egi_picinfo_calloc() egi_picinfo_free()
2022-03-21: Move EGI_PICINFO and functions to egi_bjp.h egi_bjp.c

		==========================

--------------------------------------------------------*/
//EGI_PICINFO* egi_parse_jpegExif(const char *fpath)
EGI_PICINFO* egi_parse_jpegFile(const char *fpath)
{
	FILE *fil=NULL;
	unsigned char marker[2];
	unsigned char tmpbuf[16];
	bool IsHostByteOrder; /* TiffData byte order */
	char buff[1024*4];
        uint16_t uniTmp;           /* 16-bits unicode 2.0 */
        wchar_t uniText[1024*4];   /* 32-bits Unicode */
	char strText[1024*4*4];      /* Encoded in UTF-8 */

	unsigned int imgWidth=0, imgHeight=0; /* Image size, read from frame of SOFn */
	unsigned int imgBPC=0;   /* Bits per color component */
	unsigned int imgCPP=0;   /* color components per pixel */


	long SegHeadPos;	/* File offset to start of a segment, at beginning of the Marker */
	unsigned int seglen;    /* Segment length, including 2Byte data length value
				 * excluding 2 bytes of the mark, 0xFFFE etc.
				 */
	unsigned int sdatasize; /* =seglen-1, segment data size, excluding 2byte data lenght value.*/
	unsigned long off=0;
	int k;
	bool	found_SOS=false; /* Whether the Marker SOS(StartOfScan) is found */

	long TiffHeadPos;       /* File offset to Start of TiffHeader */
	long SaveCurPos;	/* For save current file position */
	unsigned char TiffHeader[8];
	long offIFD0=0;		/* Offset to next IFD0, from TiffHeadPos */
	long offIFD1=0;		/* Offset to next IFD1, from TiffHeadPos */
	long offSubIFD=0;  	/* Offset to SubIFD, from TiffHeadPos */
	long offNextIFD; 	/* Offset to next IFD, from TiffHeadPos */
	unsigned int  ndent;  	/* number of directory entry */
	unsigned char dentry[2+2+4+4]; /* Directory entry content: TagCode(2)+Format(2)+Components(4)+value(4) */
	unsigned short tagcode; /* Tag code of diretory entry, dentry[0-1] */
	unsigned int  entcomp;  /* Entry components, dentry[4-7], len=sizeof(data_format)*entcomp */
	unsigned int  entval;   /* Entry value, dentry[8-11] */
#if 0
	enum data_format {
		for_ubyte   	    =1,   //unsigned byte   *1Bs
		for_ascii_string    =2,   //		    *1Bs
		for_ushort          =3,   //unsigned short  *2Bs
		for_ulong           =4,   //unsigned long   *4Bs
		for_urational	    =5,   //unsigned rational *8Bs
		for_sbyte	    =6,   //signed byte     *1Bs
		for_undefined	    =7,	  //		    *1Bs
		for_signed_short    =8,   //signed short    *2Bs
		for_signed_long     =9,   //signed long     *4Bs
		for_signed_rational =10,  //signed rational *8Bs
		for_single_float    =11,  //single float    *4Bs
		for_double_float    =12,  //double float    *8Bs
	} ;
#endif

	enum {
		stage_IFD0   =0,
		stage_IFD1   =1,
		stage_SubIFD =2,
	};
	int  IFDstage;  /* Image File Directory stage */

	int err=0;
	int ret;

	/*  Special tags for Windows Explore
                 * 0x9C9B:      XPTitle,   (ignored by Windows Explorer if ImageDescription exists)
                 * 0x9c9c       XPComment
                 * 0x9c9d       XPAuthor   (ignored by Windows Explorer if Artist exists)
                 * 0x9c9e       XPKeywords
                 * 0x9c9f       XPSubject
	*/
	const char *TagXPnames[] ={    /* For debug print purpose */
		"Title", "Comment", "Author", "Keywords", "Subject" };

	/* 0. Create PicInfo */
	EGI_PICINFO *PicInfo=egi_picinfo_calloc();
	if(PicInfo==NULL) {
		egi_dperr("Fail to calloc PicInfo");
		return NULL;
	}

        /* 1. Open JPEG file */
        fil=fopen(fpath, "rb");
        if(fil==NULL) {
                egi_dperr("Fail to open '%s'", fpath);
		//return -1;
		err=-1; goto END_FUNC;
        }

	/* 2. Read marker */
	ret=fread(marker, 1, 2, fil);
	if(ret<2) {
		egi_dperr("Fail to read file head marker");
		//fclose(fil);
		//return -2;
		err=-2; goto END_FUNC;
	}
	off +=2;

	/* 3. Check JPEG file header as SOI segment, no length data.  */
	if(marker[0]!=0xFF || marker[1]!=0xD8) {
		egi_dpstd("'%s' is NOT a JPEG file!\n", fpath);
		//fclose(fil);
		//return -3;
		err=-3; goto END_FUNC;
	}
	printf("SOI of a JPEG file!\n");
	/* Notice SOI semgent has seglen=0 */

	/* 4. Read other segments and parse content */
	while(1) {
		ret=fread(marker, 1, 2, fil);
		if(ret<2) {
			if(!feof(fil)) {
				egi_dperr("Fail to read markers");
				err=-1;
			}
			goto END_FUNC;
		}
		off +=2;

		/* M. Check and Parse Markers */
		/* M1. Pass non_marker data. */
		if(marker[0]!=0xFF && marker[1]!=0xFF) {
			/* ONLY possible in compressed data after SOS marker */
			if(!found_SOS) {
				egi_dpstd(DBG_RED"M1. Invalid JPEG structure, data error!\n"DBG_RESET);
				err=JPEGFILE_INVALID; goto END_FUNC;
			}
			continue;
		}
		/* M2. Find  0xFF at marker[1] */
		else if( marker[0]!=0xFF && marker[1]==0xFF ) {
			/* ONLY possible in compressed data after SOS marker */
			if(!found_SOS) {
				egi_dpstd(DBG_RED"M2. Invalid JPEG structure, data error!\n"DBG_RESET);
				err=JPEGFILE_INVALID; goto END_FUNC;
			}

			/* rewind back one byte */
			fseek(fil, -1, SEEK_CUR);
			off -=1;
			continue;
		}
		/* M3. Find a mark */
		else if(marker[0]==0xFF && marker[1]!=0xFF) {
			/* M3.1  0xFF00 exits in compressed image data */
			if(marker[1]!=0x00) { /* ??? FF/00 sequences in the compressed image data. */
//			     egi_dpstd("Marker Code: 0x%02X\n", marker[1]);
			}
			else {
				//egi_dpstd("<<< Marker code:0xFF00 >>>!\n");
				continue;
			}

			/* M3.2 Set segHeadPos */
			SegHeadPos=ftell(fil)-2; /* At beginning of the Marker */

			/* M3.3 Get segment data length
                         * NOTE: In compressed image data, 0xFFxx MAY have NO TagData at all! Example: 0xFF00
                         * in this case data in tmpbuf[] is meaningless!  See above M3.1
                         */
			ret=fread(tmpbuf, 1, 2, fil);
			if(ret<2) {
				/* OK, maybe EOF!  */
				if(!feof(fil)) {
				    egi_dperr(DBG_RED"Fail to read length of segment, ret=%d/2."DBG_RESET, ret);
				    err=-1;
				}
				goto END_FUNC;
			}
			off +=2;

			/* M3.4 Bigendian here */
			seglen = (tmpbuf[0]<<8)+tmpbuf[1];  /* !!! including 2bytes of lenght value !!! */
			sdatasize = seglen-2;

			/* M3.5 Parse Segment */
			switch(marker[1]) {
			   /* If SOS (Start Of Scan) segment:
			    * Compressed image data is followed immediately, till to the EOI.
			    */
			   case 0xDA:
				//found_SOS=true;  NOT here, after switch(marker[1])! see M3.7
				break;

			   /* Get image size from segment Start_Of_Frame(SOFn)  */
			   /* NOTE:
			    * 1. SOF0-3(0xC0-0XC3) non-hierarchincal Huffman coding
			    *    SOF0(0xC0): --- Baseline DCT (Discrete Cosine Transform)
			    *    SOF2(0xC2): --- Progresive DCT
			    * X. (DHT 0xFFC4: Huffman table specification) <---- ignore here.
			    * 2. SOF5-7(0xC5-0xC7): hierarchincal Huffman coding
			    * X. (JPG 0xFFC8: Reserved for JPEG extension) <---- ignore here.
			    * 3. SOF9-11(0xC9-0xCB): non-hierarchincal arithmetic coding
			    * X. (DAC 0xFFCC: Arithmetic coding conditioning specification) <---- ignore here.
			    * 4. SOF13-15(0xCD-0xCF): hierarchincal arithmetic coding
			    */
			   case 0xC0:
			   case 0xC1: case 0xC2: case 0xC3:
			   case 0xC5: case 0xC6: case 0xC7:
			   case 0xC9: case 0xCA: case 0xCB:
			   case 0xCD: case 0xCE: case 0xCF:

				/* Check progressive DCT */
				if(marker[1]==0xC2)
					PicInfo->progressive=true;

				/* Structure of Segment SOFn:
				    Marker: 0xFFC0-3,5-7,9-B,D-F   	2Bytes
				    Frame header length			2Bytes
				    Sample precision(bits per comp.) 	1Byte
				    Number of lines(HeightY)      	2Bytes
				    Number of samples per line(WidthX)  2Bytes
				    Number of components in frame       1Byte
				*/
				/* Read precision + Height + Width + Components = 6Bytes */
				ret=fread(tmpbuf, 1, 6, fil);
				if(ret<6) {
					egi_dperr("Fail to read bpc,height,width,cpp.");
					//fclose(fil);
					//return -3;
					err=-3; goto END_FUNC;
				}
				off +=6;

				/* Image size */
				imgBPC=tmpbuf[0];
				imgHeight=(tmpbuf[1]<<8) | tmpbuf[2];  /* bigenidan here */
				imgWidth=(tmpbuf[3]<<8) | tmpbuf[4];
				imgCPP=tmpbuf[5];
				egi_dpstd("Marker:0x%02X, Image size: W%dxH%dxCPP%dxBPC%d\n", marker[1], imgWidth, imgHeight, imgCPP, imgBPC);
			   	PicInfo->width=imgWidth;
				PicInfo->height=imgHeight;
				PicInfo->bpc=imgBPC;
				PicInfo->cpp=imgCPP;

				break;
			   /* Define Number of lines, NOT common */
			   case 0xDC:
				ret=fread(tmpbuf, 1, 2, fil);
				if(ret<2) {
					egi_dperr("Fail to read number of lines.");
					//fclose(fil);
					//return -3;
					err=-3; goto END_FUNC;
				}
				off +=2;

				egi_dpstd(DBG_YELLOW"Marker DNL, Get number of lines: %d\n"DBG_RESET, (tmpbuf[1]<<8) + tmpbuf[0]);
				break;
			   /* Comment Segment */
			   case 0xFE:  /* Comment */
			   case 0xEC:  /* APP14 0xEC Picture info(older digicams), Photoshop save for web:Ducky  */
#if 1 /* TEST: APPx-------------- */
			   case 0xE0: case 0xE2: case 0xE3: case 0xE4: case 0xE5: case 0xE6: case 0xE7:
			   case 0xE8: case 0xE9: case 0xEA: case 0xEB: case 0xED: case 0xEF:
#endif

				/* Read comment to buff */
				if( sizeof(buff)-1 < sdatasize ) {
					/* Maybe NOT comment! OR printable string! */
					egi_dpstd(" sdatasize=%u > sizeof(buff)-1!\n", sdatasize);
					//fclose(fil);
					//return -4;

					ret=fread(buff, 1, sizeof(buff)-1, fil);
                                	if(ret<sizeof(buff)-1) {
                                        	egi_dperr("Fail to read COMMENT data.");
                                       	 	//fclose(fil);
                                        	//return -4;
						err=-4; goto END_FUNC;
					}
					buff[sizeof(buff)-1]='\0';
					off += sizeof(buff)-1;
				}
				else {
					ret=fread(buff, 1, sdatasize, fil);
					if(ret<sdatasize) {
						egi_dperr("Fail to read COMMENT data.");
						//fclose(fil);
						//return -4;
						err=-4; goto END_FUNC;
					}
					buff[sdatasize]='\0';
					off +=sdatasize;
				}

				/* Print comment */
				if(marker[1]==0xFE) {
					egi_dpstd("Comment(FE): %s\n", buff);
					PicInfo->comment = strdup(buff);
				}
				else if(marker[1]==0xEC)
					egi_dpstd("Comment(EC): %s\n", buff);
				else
					egi_dpstd("Data(MK 0x%02X, datasize=%dBs): %s\n", marker[1], sdatasize, buff);

			 	break;

//////////////////////  Start Exif data (case 0xE1)  /////////////////////
			   case 0xE1:  /*  Exif APP1 */
				/* ASCII character "Exif" and 2bytes of 0x00 are used. */
                                ret=fread(buff, 1, 6, fil);
                                if(ret<6) {
                                        egi_dperr("Fail to read 6bytes Exif_Header.");
                                        //fclose(fil);  return -5;
					err=-5; goto END_FUNC;
                                }
                                off +=6;
				buff[6]='\0'; /* in case wrong header token */
				egi_dpstd("Exif Header token: %s\n", buff);

				/* Note: Traverse sequence: IFD0->SubIFD->IFD1 */

				if(strcmp(buff,"Exif")!=0) {
					egi_dpstd(DBG_RED"NOT Exif data, ignore.\n"DBG_RESET);
					//fclose(fil);
					//return -5;
					break;  /*  break case 0xE1  */
				}


				/* Save file offset to Start of TIFF Header */
				TiffHeadPos=ftell(fil);

				/* E: Read and parse IFD directory  */
				/* E1. TIFF Header:  49492A00 08000000 */
				ret=fread(TiffHeader, 1, 8, fil);
				if(ret<8) {
                                        egi_dperr("Fail to read 8bytes TIFF_Header.");
                                        //fclose(fil);  return -5;
					err=-5; goto END_FUNC;
                                }
                                off +=8;

				/* E1.1 TiffHeader[0-1]: endianness */
				if(TiffHeader[0]==0x49 && TiffHeader[1]==0x49) {
					IsHostByteOrder=true; /* LittleEndian */
					egi_dpstd(DBG_GREEN"Tiff data byte order: LittleEndian\n"DBG_RESET);
				} else {
					/* TODO: Leave a pit here for BigEndian host. >>> */
					IsHostByteOrder=false;
					egi_dpstd(DBG_YELLOW"Tiff data byte order: BigEndian\n"DBG_RESET);
				}


				/* E1.2 TiffHeader[2-3]: Next 2bytes are always 2bytes-length value of 0x002A(big) OR 0x2A00(little) */

				/* E1.3 TiffHeader[4-7]: Offset to first IFD, from start of TIFF_Header, so at least 8 */
				if(IsHostByteOrder==true) /* LittleEndian */
					offIFD0 = TiffHeader[4]+(TiffHeader[5]<<8)+(TiffHeader[6]<<16)+(TiffHeader[7]<<24);
				else
					offIFD0 = TiffHeader[7]+(TiffHeader[6]<<8)+(TiffHeader[5]<<16)+(TiffHeader[4]<<24);

				/* Current seek offset 8bytes, so MUST offIFD>=8!  */
				if(offIFD0<8) {
				     	egi_dpstd("Data error! offIFD<8!\n");
					err=-5; goto END_FUNC;
				}
				egi_dpstd("Offset to first IFD: %lu\n", offIFD0);

				/* E1.4 Goto first IFD0, as already passed 8bytes. */
				if( fseek(fil, offIFD0-8, SEEK_CUR)!=0 ){
					egi_dperr("fseek to offIFD-8 fails");
					err=-5; goto END_FUNC;
				}
				off += offIFD0-8;

				/* E1.5 Start form IFD0 */
				IFDstage=stage_IFD0;

START_IFD_DIRECTORY:		/* Start of IFD0 OR SubIFD OR IFD1  */
				/* IFD directory
				    E(2Bs) : Number of directory entry
				    T(2Bs)+F(2Bs)+N(4Bs)+D(4Bs)   Entry0   (T-Tag, F-Format, N-NumberOfComponent, D-DataValue)
				    T(2Bs)+F(2Bs)+N(4Bs)+D(4Bs)   Entry1
				    ... ...
				    T(2Bs)+F(2Bs)+N(4Bs)+D(4Bs)   Entry2
				    L(4Bs)   Offset to next IFD
				*/


				/* E1.5.1 Read number of directory entrys */
                                ret=fread(buff, 1, 2, fil);
                                if(ret<2) {
                                        egi_dperr("Fail to read 2bytes number of directory entry.");
					err=-5; goto END_FUNC;
                                }
                                off +=2;

			        if(IsHostByteOrder==true) /* LittleEndian */
					ndent=buff[0]+(buff[1]<<8);
				else
					ndent=buff[1]+(buff[0]<<8);

				egi_dpstd(">>>>>>  %s directory has %d entrys  <<<<<< \n",
					IFDstage==stage_IFD0?"IFD0":(IFDstage==stage_IFD1?"IFD1":"SubIFD"),  ndent);

				/* E1.5.2. Parse entrys */
				for(k=0; k<ndent; k++) {
	                                /* Read Entry content 12bytes, TagCode(2)+Format(2)+Components(4)+value(4) */
        	                        ret=fread(dentry, 1, 12, fil); /* 2+2+4+4 */
                	                if(ret<12) {
                        	                egi_dperr("Fail to read 12bytes directory entry content.");
						err=-5; goto END_FUNC;
                                	}
                                	off +=12;

					/* Get tag, components, value */
					if(IsHostByteOrder==true) { /* LittleEndian */
						tagcode = dentry[0] + (dentry[1]<<8);
						entcomp = dentry[4]+(dentry[5]<<8)+(dentry[6]<<16)+(dentry[7]<<24);
						entval  = dentry[8]+(dentry[9]<<8)+(dentry[10]<<16)+(dentry[11]<<24);
					}
					else {
						tagcode = dentry[1] + (dentry[0]<<8);
						entcomp = dentry[7]+(dentry[6]<<8)+(dentry[5]<<16)+(dentry[4]<<24);
						entval  = dentry[11]+(dentry[10]<<8)+(dentry[9]<<16)+(dentry[8]<<24);
					}

//					egi_dpstd("Exif Tag: 0x%04X  entcomp=%d\n", tagcode, entcomp);

        //////////// Exif data:: Parse IFD Entrys ///////////////////////////////////////////////
	switch(tagcode) {
		case 0x010e:  /* 0x010e ImageDescription, ascii string */
		    /* Then entval is offset value */
		    if( 1*entcomp >4 ) {
			SaveCurPos=ftell(fil);
			if(fseek(fil, TiffHeadPos+entval, SEEK_SET)!=0) {
				egi_dperr("Fail to fseek");
				err=-5; goto END_FUNC;
		    	}

			/* Read ascii string */
			ret=fread(buff, 1, 1*entcomp>(sizeof(buff)-1)?(sizeof(buff)-1):(1*entcomp), fil);
			if(ret<1*entcomp) {
                        	egi_dperr("Fail to read ImageDescription string.");
                                err=-5;  goto END_FUNC;
                        }
			buff[sizeof(buff)-1]='\0';
			egi_dpstd("ImageDescription: %s\n", buff);
			PicInfo->ImageDescription=strdup(buff);

			/* Restore SEEK position */
			fseek(fil, SaveCurPos, SEEK_SET);
		    }
		    break;
		case 0x010f:  /* 0x010f Make Manufacturer of digicam, ascii string */
		    /* Then entval is offset value */
		    if( 1*entcomp >4 ) {
			SaveCurPos=ftell(fil);
			if(fseek(fil, TiffHeadPos+entval, SEEK_SET)!=0) {
				egi_dperr("Fail to fseek");
				err=-5; goto END_FUNC;
		    	}

			/* Read ascii string */
			ret=fread(buff, 1, 1*entcomp>(sizeof(buff)-1)?(sizeof(buff)-1):(1*entcomp), fil);
			if(ret<1*entcomp) {
                        	egi_dperr("Fail to read Maker of digicam.");
                                err=-5;  goto END_FUNC;
                        }
			buff[sizeof(buff)-1]='\0';
			egi_dpstd("Camera Maker: %s\n", buff);
			PicInfo->CameraMaker=strdup(buff);

			/* Restore SEEK position */
			fseek(fil, SaveCurPos, SEEK_SET);
		    }
		    break;
		case 0x0110:  /* 0x0110 Model Model number of digicam, ascii string */
		    /* Then entval is offset value */
		    if( 1*entcomp >4 ) {
			SaveCurPos=ftell(fil);
			if(fseek(fil, TiffHeadPos+entval, SEEK_SET)!=0) {
				egi_dperr("Fail to fseek");
				err=-5; goto END_FUNC;
		    	}

			/* Read ascii string */
			ret=fread(buff, 1, 1*entcomp>(sizeof(buff)-1)?(sizeof(buff)-1):(1*entcomp), fil);
			if(ret<1*entcomp) {
                        	egi_dperr("Fail to read Model of digicam.");
                                err=-5;  goto END_FUNC;
                        }
			buff[sizeof(buff)-1]='\0';
			egi_dpstd("Camera Model: %s\n", buff);
			PicInfo->CameraModel=strdup(buff);

			/* Restore SEEK position */
			fseek(fil, SaveCurPos, SEEK_SET);
		    }
		    break;
		case 0x0112:  /* 0x0112 Orientation, The orientation of the camera relative to the scene, ushort */
		    break;
		case 0x011a:  /* 0x011a XResolution, unsigned_rational */
		    break;
		case 0x011b:  /* 0x011b YResolution, unsigned_rational */
		    break;
		case 0x0128:  /* 0x0128 ResolutonUnit, unsigned_short */
		    break;
		case 0x0131:  /* 0x0131 Software, ascii string */
		    /* Then entval is offset value */
		    if( 1*entcomp >4 ) {
			SaveCurPos=ftell(fil);
			if(fseek(fil, TiffHeadPos+entval, SEEK_SET)!=0) {
				egi_dperr("Fail to fseek");
				err=-5; goto END_FUNC;
		    	}

			/* Read ascii string */
			ret=fread(buff, 1, 1*entcomp>(sizeof(buff)-1)?(sizeof(buff)-1):(1*entcomp), fil);
			if(ret<1*entcomp) {
                        	egi_dperr("Fail to read Software.");
                                err=-5;  goto END_FUNC;
                        }
			buff[sizeof(buff)-1]='\0';
			egi_dpstd("Software: %s\n", buff);
			PicInfo->software=strdup(buff);

			/* Restore SEEK position */
			fseek(fil, SaveCurPos, SEEK_SET);
		    }

		    break;
		case 0x0132:  /* 0x0132 Last DataTime, ascii string,  "YYYY:MM:DD HH:MM:SS"+0x00, 20Bytes */
		    /* Then entval is offset value */
		    if( 1*entcomp >4 ) {
			SaveCurPos=ftell(fil);
			if(fseek(fil, TiffHeadPos+entval, SEEK_SET)!=0) {
				egi_dperr("Fail to fseek");
				err=-5; goto END_FUNC;
		    	}
			/* Read ascii string */
			ret=fread(buff, 1, 1*entcomp>(sizeof(buff)-1)?(sizeof(buff)-1):(1*entcomp), fil);
			if(ret<1*entcomp) {
                        	egi_dperr("Fail to read DateTime.");
                                err=-5;  goto END_FUNC;
                        }
			buff[sizeof(buff)-1]='\0';
			egi_dpstd("Last Date: %s\n", buff);
			PicInfo->DateLastModify=strdup(buff);

			/* Restore SEEK position */
			fseek(fil, SaveCurPos, SEEK_SET);
		    }
		    break;
		case 0x013e:  /* 0x013e WhitePoint, 2 unsigned_rational */
		    break;
		case 0x013f:  /* 0x013f PrimaryChromatictities, 6 unsigned rational */
		    break;
		case 0x0201:  /* IFD1, ThumbnailOffset in IFD1 of JPEG and some TIFF-based images */
		    egi_dpstd("IFD1 TAG for Thumbnail Offset\n");
		    break;
		case 0x0202:  /* IFD1, ThumbnailLength in IFD1 of JPEG and some TIFF-based images */
		    egi_dpstd("IFD1 TAG for Thumbnail Length\n");
		    break;
		case 0x0211:  /* 0x0211 YCbCrCoefficients, 3 unsigned rational */
		    break;
		case 0x0213:  /* 0x0213 YCbCrPositioning, 1 unsigned short */
		    break;
		case 0x0214:  /* 0x0214 ReferenceBlackWhite 6 unsigned rational */
		    break;
		case 0x8298: /* 0x8298 Copyright information, ascii string */
		    /* Then entval is offset value */
		    if( 1*entcomp >4 ) {
			SaveCurPos=ftell(fil);
			if(fseek(fil, TiffHeadPos+entval, SEEK_SET)!=0) {
				egi_dperr("Fail to fseek");
				err=-5; goto END_FUNC;
		    	}
			/* Read ascii string */
			ret=fread(buff, 1, 1*entcomp>(sizeof(buff)-1)?(sizeof(buff)-1):(1*entcomp), fil);
			if(ret<1*entcomp) {
                        	egi_dperr("Fail to read Copyright Info.");
                                err=-5;  goto END_FUNC;
                        }
			buff[sizeof(buff)-1]='\0';
			egi_dpstd("Copyright: %s\n", buff);
			PicInfo->copyright=strdup(buff);

			/* Restore SEEK position */
			fseek(fil, SaveCurPos, SEEK_SET);
		    }
		    break;

/* ----- Exif data:: Offset to Sub IFD ----- */
		case 0x8769: /* 0x8769 ExifOffset 1 unsigned long Offset to Exif Sub IFD */

		#if 0
		    /* Entval is the offset value, to Exif SubIFD */
		    if(fseek(fil, TiffHeadPos+entval, SEEK_SET)!=0) {
			egi_dperr("Fail to fseek to Exif SubIFD");
			err=-5; goto END_FUNC;
		    }

		    IFDstage=stage_SubIFD;
		    goto START_IFD_DIRECTORY;
		#endif

		    /* Save offSubIFD */
		    offSubIFD=entval;

		    break;

/* ---- For SubIFD Directory ---- */
		case 0x9003:  /* DateTime Original, ascii string 20bytes with '\0' */
		    /* Then entval is offset value */
		    if( 1*entcomp >4 ) {
			SaveCurPos=ftell(fil);
			if(fseek(fil, TiffHeadPos+entval, SEEK_SET)!=0) {
				egi_dperr("Fail to fseek");
				err=-5; goto END_FUNC;
		    	}
			/* Read ascii string */
			ret=fread(buff, 1, 1*entcomp>(sizeof(buff)-1)?(sizeof(buff)-1):(1*entcomp), fil);
			if(ret<1*entcomp) {
                        	egi_dperr("Fail to read DateTimeOriginal.");
                                err=-5;  goto END_FUNC;
                        }
			buff[sizeof(buff)-1]='\0';
			egi_dpstd("Date Taken: %s\n", buff);
			PicInfo->DateTaken=strdup(buff);

			/* Restore SEEK position */
			fseek(fil, SaveCurPos, SEEK_SET);
		    }
		    break;

		/* IFD0 tags 0x9c9b-0x9c9f are used by Windows Explorer;  there are in 16bit unicode
		 * special characters in these values are converted to UTF-8 by default
		 * 0x9C9B:      XPTitle,   (ignored by Windows Explorer if ImageDescription exists)
		 * 0x9c9c	XPComment
		 * 0x9c9d	XPAuthor   (ignored by Windows Explorer if Artist exists)
		 * 0x9c9e	XPKeywords
		 * 0x9c9f	XPSubject
		 */
		case 0x9C9B: case 0x9c9c: case 0x9c9d: case 0x9c9e: case 0x9c9f:
                    /* Then entval is offset value */
                    if( 1*entcomp >4 ) {
                        SaveCurPos=ftell(fil);
                        if(fseek(fil, TiffHeadPos+entval, SEEK_SET)!=0) {
                                egi_dperr("Fail to fseek");
                                err=-5; goto END_FUNC;
                        }

			/*  Check size for uniText */
			if( sizeof(uniText) < 1*entcomp ) {
				egi_dpstd("sizeof(uniText) < 1*entcomp!\n");
				err=-5; goto END_FUNC;
			}

                        /*   Read all 16-bits Unicodes */
			int j;
                        memset(uniText, 0, sizeof(uniText));
                        for(j=0; j<1*entcomp/2; j++) {
                        	if( fread(&uniTmp,1,2,fil)!=2 ) {
                                           err=-5; goto END_FUNC;
				}

                                /* Endianess conversion */
                                if(!IsHostByteOrder)
                                	uniText[j]=(wchar_t)uniTmp;
                                else
                                        uniText[j]=(wchar_t)ntohl(uniTmp);
                         }

			/* Convert to UTF-8 */
			int len=cstr_unicode_to_uft8(uniText, strText);
			if(len>0) {
				strText[len]='\0';
				printf("%s: %s\n", TagXPnames[tagcode-0x9c9b], strText);
			}

			/* Write to PicInfo */
			switch(tagcode) {
				case 0x9c9b:
					PicInfo->title=strdup(strText); break;
				case 0x9c9c:
					PicInfo->xcomment=strdup(strText); break;
				case 0x9c9d:
					PicInfo->author=strdup(strText); break;
				case 0x9c9e:
					PicInfo->keywords=strdup(strText); break;
				case 0x9c9f:
					PicInfo->subject=strdup(strText); break;
			}

                        /* Restore SEEK position */
                        fseek(fil, SaveCurPos, SEEK_SET);
                    }

		    break;
		case 0xEA1C:  /*  Also padding */
		    break;

		default:
		    break;
	}
        //////////// Exif data:: END Parse IFD Entrys ///////////////////////////////////////////////

				} /* End for(k) pase entrys */

				/* NOW: At end of the file directory(TAG lists) */

				/* E1.5.3 At end of the file directory, read Offset to next IFD */
        	                ret=fread(tmpbuf, 1, 4, fil);
                	        if(ret<4) {
                        	       egi_dperr("Fail to read 4bytes offset to next IFD.");
                                       err=-5;  goto END_FUNC;
                                }
                                off +=4;

				/* E1.5.4 Get offset value */
				if(IsHostByteOrder==true) /* LittleEndian */
					offNextIFD=tmpbuf[0]+(tmpbuf[1]<<8)+(tmpbuf[2]<<16)+(tmpbuf[3]<<24);
				else
					offNextIFD=tmpbuf[3]+(tmpbuf[2]<<8)+(tmpbuf[1]<<16)+(tmpbuf[0]<<24);

				/* E1.5.5 Check current IFDstage, then Update/Shift IFD stage */
				if(IFDstage==stage_IFD0) {
				     offIFD0=0; /* Reset */
				     egi_dpstd("End of IFD0 directory, offNextIFD=%ld\n", offNextIFD);
				     offIFD1=offNextIFD;
				}
				else if(IFDstage==stage_IFD1) {
				     offIFD1=0; /* Reset! */
				     egi_dpstd("End of IFD1 directory, offNextIFD=%ld. --- BREAK ---\n", offNextIFD);
				     break;  /* Break Exif_data case (case 0xE1) */
				}
				else if(IFDstage==stage_SubIFD) {
				        egi_dpstd("End of SubIFD directory, offNextIFD=%ld\n", offNextIFD);
					offSubIFD=0; /* Reset */
				}
				/* Sequence: IFD0-->SubIFD-->IFD1, MUST break when IFDstage==stage_SubIFD, or it loops. */

				/* E1.5.6 Skip to SubIFD */
				if(offSubIFD>0 && IFDstage!=stage_SubIFD) {
		    			if(fseek(fil, TiffHeadPos+offSubIFD, SEEK_SET)!=0) {
						egi_dperr("Fail to fseek to SubIFD");
						err=-5; goto END_FUNC;
					}

		    			IFDstage=stage_SubIFD;
		    			goto START_IFD_DIRECTORY;
		    		}

				/* E1.5.7 Skip to IFD1 */
				if(offIFD1!=0) {
		    			if(fseek(fil, TiffHeadPos+offIFD1, SEEK_SET)!=0) {
						egi_dperr("Fail to fseek to IFD1");
						err=-5; goto END_FUNC;
		    			}

					IFDstage=stage_IFD1;
					goto START_IFD_DIRECTORY;
				}

				/* E1.5.8 Skip to IFDx if offNextIFD>0 ??? XXX ONLY IFD1 */

				break;
//////////////////////  END Exif data (case 0xE1)  /////////////////////

			   case 0xD9:
				printf("EOI: End of JPEG file!\n");
				goto END_FUNC;
				break;

			   default:

			   	break;
			} /* END M3.5 switch (marker[1]) */


			/* M3.6 Get to the END of the segment */
			if(!found_SOS) {
			    if(fseek(fil, SegHeadPos+2+seglen, SEEK_SET)!=0) {
				egi_dperr("Fail to fseek the END of segment.");
                                err=-5; goto END_FUNC;
			    }
			}

                        /* NOTE: If found SOS,which is the LAST segment before EOI, then the compressed image data is
			 * followed immediately, till to the EOI.
                         * In compressed image data, 0xFFxx MAY have NO TagData at all! Example: 0xFF00
                         * in this case data in tmpbuf[] is meaningless, so DO NOT fseek/skip in this case.
                         */

			/* M3.7 To set found_SOS=true just AFTER it fseeks to the END of SOS segment! So Markers in compressed
			 * image data WILL NOT fseek/skip, see M3.6.
			 */
			if(marker[1]==0xDA)  /* SOS */
				found_SOS=true;

			continue;
		} /* End find a marker */

		/*  M4. Padding 0xFFF  */
		else { /* (marker[0]==0xFF && marker[1]==0xFF) */
//			printf("0xFFFF\n");
			/* rewind back one byte, in case NEXT bytes is NOT 0xFF */
			fseek(fil, -1, SEEK_CUR);
			off -=1;
			continue;
		}

	} /* End while() */

	/* 5. END JOB */
END_FUNC:
	fclose(fil);

	/* Check error */
	if(err)	egi_picinfo_free(&PicInfo);

	return PicInfo;
}

