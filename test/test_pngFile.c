/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to extract information from a PNG file.

Refrence:
1  https://www.w3.org/TR/PNG/
2. http://www.libpng.org/pub/png/spec/

Note:
0. All integer values in chunks are big-endian.
1. Structe of a PNG file:
    PNG_Signature + Chunk(IHDR) + Chunk + Chunk + Chunk... + Chunk(IEND)

2. PNG_Signature:  137 80 78 71 13 10 26 10  (decimal)

3. Structure of a chunk:
    DataLength(4Bs) + ChunkType(4Bs) + ChunkData + CRC(4Bs)

    3.1 DataLength: It counts ONLY the ChunkData! (EXCLUDING Lenght, ChunkType and CRC)
    3.2 Chunk Type: 4-bytes type code restricted to: A-Z and a-z (65-90 and 97-122 decimal)
    3.3 Chunk Data: It can be of zero length.
    3.4 CRC: 4-bytes Cyclic Redundancy Check for ChunkType+ChunkData, always present.
    3.5 Chunks can appear in any order, except IHDR and IEND.
4. Chunk properties
    bit 5 of each byte of ChunkType are used to convey chunk properties.
    4.1 Ancillary bit:    bit 5 of the 1st Byte
	0 (uppercase) = critical
	1 (lowercase) = ancillary
    4.2 Private bit:      bit 5 of the 2nd Byte
	0 (uppercase) = public
	1 (lowercase) = private
    4.3 Reserved bit:     bit 5 of the 3rd Byte
	0 (uppercase) = in this version PNG
	1 (reserved)  = datastream does NOT conform to this version of PNG
    4.4 Safe-to-copy bit: bit 5 of the 4th Byte
        0 (uppercase) = this chunk is unsafe to copy
        1 (lowercase) = this chunk is safe to copy

    Example:  chunk type "cHNk"
	c(0x63)---0b01100011  bit 5 is '1' (Ancillary bit)
	H(0x48)---0b01001000  bit 5 is '0' (Private bit)
	N(0x4E)---0b01001110  bit 5 is '0' (Reserved bit)
	k(0x6B)---0b01101011  bit 5 is '1' (Safe-to-copy bit)

5. Chunks and ordering rules
   5.1  Critical chunks
 	IHDR	   Shall be the FIRST
	PLTE       Before first IDAT
	IDAT       --- Multiple IDATs allowed, and shall be consecutive ---
	IEDN	   Shall be the LAST
   5.2  Ancillary chunks
	cHRM       Before PLTE and IDAT
	....       (See more in standard)
	sPLT       Before IDAT
	tITME      None
	iTXt       None
	tEXt	   None
	zTxt       None
6. Chunk IHDR: Image header
   The IHDR chunk shall be the first chunk in the PNG datastream.
         --- ChunkData ---
   Width (in pixels)	4 bytes
   Height (in pixels)	4 bytes
   Bit depth		1 byte
   Colour type		1 byte
   Compression method	1 byte
   Filter method	1 byte
   Interlace method	1 byte


Journal:
2022-03-23: Create the file.
2022-03-24/25: egi_parse_pngFile()
2022-03-26: Test display interlaced PNG.

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

#define TMP_PNG_NAME  "/tmp/_test_png.png"
EGI_PICINFO* egi_parse_pngFile(const char *fpath);
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
                printf("----------- JPEG File: %s ---------\n", fpath);

#if 0
                ret=egi_check_jpgfile(fpath);
                if(ret==JPEGFILE_COMPLETE)
                        printf("OK, integraty checked!\n");
                else if(ret==JPEGFILE_INCOMPLETE)
                        printf(DBG_YELLOW"Incomplete JPEG file!\n"DBG_RESET);
                else {
                        printf(DBG_RED"Invalid JPEG file!\n"DBG_RESET);
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


                /* Copy partial JPEG file and display it, demo effect of interlaced PNG. */
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
		exit(0);
#endif

                /* Display whole image if bn<2 */
                if(bn<2) {
                        /* Load to imgbuf */
                        imgbuf=egi_imgbuf_readfile(fpath);
                        if(imgbuf==NULL) {
                                printf("Fail to read imgbuf from '%s'!\n", TMP_PNG_NAME);
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
                        fdest=fopen(TMP_PNG_NAME, k==0?"wb":"ab");
                        if(fdest==NULL) {
				printf("Fail to open temp. file '%s'.\n", TMP_PNG_NAME);
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

                        if( k<12 || k==bn-1 ) { /*  */
                                /* Load to imgbuf */
                                printf("fdest size: %ld of %ld\n", egi_get_fileSize(TMP_PNG_NAME), fsize);
                                imgbuf=egi_imgbuf_readfile(TMP_PNG_NAME);
                                if(imgbuf==NULL) {
                                        printf("Fail to read imgbuf from '%s'!\n", TMP_PNG_NAME);
//                                        fclose(fdest);
                                        continue;
                                }

                                /* Display image */
                                fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_BLACK);

				#if 0  /* LeftTop */
                                egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0,0);   /* imgbuf, fbdev, subnum, subcolor, x0, y0 */

				#else /* Put image in middle of screen */
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
                                //usleep(10000);
                        }

                        /* Free and release */
                        egi_imgbuf_free2(&imgbuf);
        //                fclose(fdest);
                    } /* END for() */

                } /* END else */

                /* Display picture information */
                //draw_blend_filled_rect(&gv_fb_dev, 0, 0, 320-1, 110, WEGI_COLOR_GRAY3, 160);
                draw_blend_filled_rect(&gv_fb_dev, 0, 0, 320-1, 240-1, WEGI_COLOR_GRAY3, 200);
                FTsymbol_writeFB(txtbuff, 18, 18, WEGI_COLOR_WHITE, 5, 10);
                fb_render(&gv_fb_dev);
                sleep(1);

                fclose(fsrc);
                egi_picinfo_free(&PicInfo);

     } /* END for() */

  } ////////////////////////  END: LOOP TEST  //////////////////////////////////

  exit(0);
}


/*------------------------------------------------
Read and parse image information in a jpeg file.
Also read Comment segment of JPEG.

Return:
	!NULL	OK
	NULL	Fails
------------------------------------------------*/
EGI_PICINFO* egi_parse_pngFile(const char *fpath)
{
	FILE *fil=NULL;
	unsigned char tmpbuf[16];
	char chunkTypeCode[5];
	unsigned int  cdatalen;   /* chunk data length, counts ONLY ChunkData! excluding ChunkLengh, ChunkType and CRC */

	unsigned char bitDepth;
	unsigned char colorType;

	/* Critical chunks */
	bool found_IHDR=false;
	bool found_PLTE=false;
	bool found_IDAT=false;
	bool found_IEND=false;

	/* Image size, read from IHDR */
        unsigned int imgWidth=0, imgHeight=0;

	int err=PNGFILE_COMPLETE; //=0;
	int ret;

        /* 0. Create PicInfo */
        EGI_PICINFO *PicInfo=egi_picinfo_calloc();
        if(PicInfo==NULL) {
                egi_dperr("Fail to calloc PicInfo");
                return NULL;
        }

        /* 1. Open PNG file */
        fil=fopen(fpath, "rb");
        if(fil==NULL) {
                egi_dperr("Fail to open '%s'", fpath);
                err=PNGFILE_INVALID; goto END_FUNC;
        }

        /* 2. Read PNG signature:  137 80 78 71 13 10 26 10  (decimal) */
        ret=fread(tmpbuf, 1, 8, fil);
        if(ret<8) {
                egi_dperr("Fail to read signature.");
                err=PNGFILE_INVALID; goto END_FUNC;
        }

	/* 3. Check signature */
	if( tmpbuf[0]!=137 || tmpbuf[1]!=80 || tmpbuf[2]!=78 || tmpbuf[3]!=71
	   || tmpbuf[4]!=13 || tmpbuf[5]!=10 || tmpbuf[6]!=26 || tmpbuf[7]!=10 )
	{
		egi_dpstd("Signature error, it's NOT a legitimage PNG file!\n");
		err=PNGFILE_INVALID; goto END_FUNC;
	}

	/* 4. Read IHDR chunck
	 *   ChunkDataLength(4Bs) + ChunkType(4Bs) + ChunkData + CRC(4Bs)
    	 *   Note: 1. ChunkDataLength counts ONLY the ChunkData! (EXCLUDING Lengh, ChunkType and CRC)
		   2. For IHDR, ChunkDataLenght shall be 13Bytes
			Width  (in pixels)	4 bytes
			Height  (in pixels)     4 bytes
			Bit depth		1 byte
			Colour type		1 byte
			Compression method	1 byte
			Filter method		1 byte
			Interlace method	1 byte
	 */
	/* 4.0 Fread  ChunkDataLength(4Bs) + ChunkType(4Bs) */
        ret=fread(tmpbuf, 1, 8, fil);
        if(ret<8) {
                egi_dperr("Fail to read IHDR hearder.");
                err=PNGFILE_INCOMPLETE; goto END_FUNC;
        }
	/* 4.1 Check TYPE */
	strncpy(chunkTypeCode,(char*)tmpbuf+4, 4); chunkTypeCode[4]='\0';
//	egi_dpstd("ChunkType: %s\n", chunkTypeCode);
	if( strncmp(chunkTypeCode, "IHDR",4)!=0 ) {
		egi_dpstd("First chunk is NOT IDHR, illegitimate!\n");
		err=PNGFILE_INVALID; goto END_FUNC;
	}
	found_IHDR=true;

	/* 4.2 ChunkData length */
	cdatalen=(tmpbuf[0]<<24)+(tmpbuf[1]<<16)+(tmpbuf[2]<<8)+tmpbuf[3]; /* BIG_ENDIAN */
	//printf("cdatasize: %d\n", cdatalen);
	if(cdatalen!=13) {
		egi_dpstd("IHDR ChunkDataLength: %d, it should be 13!\n", cdatalen);
	}

	/* 4.3 Read IHDR ChunkData */
        ret=fread(tmpbuf, 1, 13, fil); /* Read ChunkDataLength(13Bs) */
        if(ret<13) {
                egi_dperr("Fail to read IHDR ChunkData.");
                err=PNGFILE_INCOMPLETE; goto END_FUNC;
        }
	/* 4.3.1 Width and Height */
	imgWidth = (tmpbuf[0]<<24)+(tmpbuf[1]<<16)+(tmpbuf[2]<<8)+tmpbuf[3]; /* BIG_ENDIAN */
	imgHeight = (tmpbuf[4]<<24)+(tmpbuf[5]<<16)+(tmpbuf[6]<<8)+tmpbuf[7]; /* BIG_ENDIAN */
//	egi_dpstd("Image: W%dxH%d\n", imgWidth, imgHeight);
	PicInfo->width=imgWidth;
	PicInfo->height=imgHeight;

	/* 4.3.2 Color type and bit depth */
	/* Allowed combinations of colour type and bit depth
	   PNG image type	Colour type	Allowed bit depths	Interpretation
	   ---------------------------------------------------------------------------
	   Greyscale	           0              1, 2, 4, 8, 16      Each pixel is a greyscale sample
	   Truecolour	           2                  8, 16           Each pixel is an R,G,B triple
	   Indexed-colour	   3               1, 2, 4, 8         Each pixel is a palette index; a PLTE chunk shall appear.
	   Greyscale with alpha	   4	              8, 16	      Each pixel is a greyscale sample followed by an alpha sample.
	   Truecolour with alpha   6                  8, 16           Each pixel is an R,G,B triple followed by an alpha sample.

	*/
	bitDepth = tmpbuf[8];
	colorType = tmpbuf[9];
	/* int cpp;  --- color components(samples) per pixel
         * int bpc;  --- Bits per color component
	 */
	PicInfo->colorType=colorType;
	switch(colorType) {
		case 0:  PicInfo->cpp=1; break;  /* A greyscale sample*/
		case 2:  PicInfo->cpp=3; break;  /* An R,G,B triple */
		case 3:  PicInfo->cpp=1; break;  /* A palette index */
		case 4:  PicInfo->cpp=2; break;  /* A greyscale sample + an alpha sample */
		case 6:  PicInfo->cpp=4; break;  /* An R,G,B triple + an alpha sample */
	}
	PicInfo->bpc=bitDepth;

	/* 4.4 The last bytes for: Interlacing/Progressive PNG */
	PicInfo->progressive = tmpbuf[12];
//	egi_dpstd("Interlace method: %d\n",tmpbuf[12]);

	/* 4.5 Fseek to chunkEnd, pass 4Bs CRC */
	if(fseek(fil, 4, SEEK_CUR)!=0 ) {
        	egi_dperr("Fail to fseek to end of IHDR chunk.");
                err=PNGFILE_INCOMPLETE; goto END_FUNC;
         }

#if 1	/* 5. Read other chunks and parse content
	 *   ChunkDataLength(4Bs) + ChunkType(4Bs) + ChunkData + CRC(4Bs)
	 */
	while(1) {
		/* 5.0 Fread  ChunkDataLength(4Bs) + ChunkType(4Bs) */
	        ret=fread(tmpbuf, 1, 8, fil);
        	if(ret<8) {
			if(!feof(fil)) {
                		egi_dperr("Fail to read chunk hearder for '%s'", fpath);
				err=PNGFILE_INCOMPLETE;
			}
			//else  egi_dpstd("END of file.\n");
                	goto END_FUNC;
        	}

		/* 5.1 Check TYPE */
		strncpy(chunkTypeCode,(char*)tmpbuf+4, 4); chunkTypeCode[4]='\0';
//		egi_dpstd("ChunkType: %s\n", chunkTypeCode);
		if( strncmp(chunkTypeCode, "PLTE",4)==0 ) {
			found_PLTE=true;
		}
		else if( strncmp(chunkTypeCode, "IDAT",4)==0 ) {
			found_IDAT=true;
		}
		else if( strncmp(chunkTypeCode, "IEND",4)==0 ) {
			found_IEND=true;
		}

		/* 5.2 ChunkData length */
		cdatalen=(tmpbuf[0]<<24)+(tmpbuf[1]<<16)+(tmpbuf[2]<<8)+tmpbuf[3]; /* BIG_ENDIAN */
		//printf("cdatasize: %d\n", cdatalen);

		/* 5.3 Fseek to the end of the chunk, skipping ChunkData + CRC(4Bs) */
		if(fseek(fil, cdatalen+4, SEEK_CUR)!=0 ) {
        		egi_dperr("Fail to fseek to end of IHDR chunk.");
                	err=PNGFILE_INCOMPLETE; goto END_FUNC;
         	}
	}
#endif

	/* 6. Check critical chunks */
	if(!found_IDAT)
		err=PNGFILE_INCOMPLETE;
	if(!found_IEND)
		err=PNGFILE_INCOMPLETE;


        /* 7. END JOB */
END_FUNC:
        fclose(fil);

        /* Check error */
        if(err) egi_picinfo_free(&PicInfo);

        return PicInfo;
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
