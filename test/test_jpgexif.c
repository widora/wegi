/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to extract information from exif data in a JPEG file.

Reference:
1. https://www.media.mit.edu/pia/Research/deepview/exif.html
2. https:/www.exiftool.org/TagNames/EXIF.html

Note:
1. JPEG file structure:
    segment + segment + segment + .....
    Where: Segment = Marker + Payload(Data)
           Marker =  0xFF + MakerCode(0xXX)

  1.1 A JPEG file starts with segement SOI(Start of image) and ends
      segment EOI(End of image).

  1.2 Structre of a segment:
     0xFF+MakerCode(1 byte)+DataSize(2 bytes)+Data(n bytes)
     ( !!!--- Execption ---!!! Segment SOI and EOI have NO DataSize and Data )

  1.3. 0xFF padding: there maybe 0xFFs between segments.

  1.4 Exif uses Marker APP1(0xFFE1) for application.

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
   ------------------------------- (IFD: ImageFileDirectory)
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
   FFD8XXXX...XXXXFFD9    Thumbnail image

Journal:
2022-03-15: Create the file.
2022-03-16: Parse Exif IFD directory
2022-03-17: Parse IFD0 tags 0x9c9b-0x9c9f (for Windows Explorer)
2022-03-18: Parse SOFn for image size.

Midas Zhou
知之者不如好之者好之者不如乐之者
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include "egi_debug.h"
#include "egi_cstring.h"
#include <arpa/inet.h>


int egi_parse_jpegExif(const char *fpath);

int main(int argc, char **argv)
{
	int i;

	for(i=1; i<argc; i++) {
		printf("----------- JPEG File: %s ---------\n",argv[i]);
		egi_parse_jpegExif(argv[i]);
	}

	return 0;
}


/*-------------------------------------------------------
Read and parse Exif data in a jpeg file.

Also read Comment segment of JPEG.

Note:
1. While(1) search for Markers starting with 0xFF token,
   if found, then switch() to parse the segment, after
   break/end of the case, it skips to the end of the segment
   and start to search next Marker...

2. Exif IFD parse sequence: IFD0 --> SubIFD --> IFD1
   See E1.5.3~E1.5.7 for skipping/shifting to IFD directory.



Return:
	0	OK
	<0 	Fails
--------------------------------------------------------*/
int egi_parse_jpegExif(const char *fpath)
{
	FILE *fil=NULL;
	unsigned char marker[2];
	unsigned char tmpbuf[16];
	bool IsHostByteOrder; /* TiffData byte order */
	char buff[1024*4];
        uint16_t uniTmp;           /* 16-bits unicode 2.o */
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


	long TiffHeadPos;       /* File offset to Start of TiffHeader */
	long SaveCurPos;	/* For save current file position */
	unsigned char TiffHeader[8];
	//long offIFD; 		/* Offset for IFD */
	long offIFD0=0;		/* Offset to next IFD0, from TiffHeadPos */
	long offIFD1=0;		/* Offset to next IFD1, from TiffHeadPos */
	long offSubIFD=0;  	/* Offset to SubIFD, from TiffHeadPos */
	long offNextIFD; 	/* Offset to next IFD, from TiffHeadPos */
	unsigned int  ndent;  	/* number of directory entry */
	unsigned char dentry[2+2+4+4]; /* Directory entry content: TagCode(2)+Format(2)+Components(4)+value(4) */
	unsigned short tagcode; /* Tag code of diretory entry, dentry[0-1] */
	unsigned int  entcomp;  /* Entry components, dentry[4-7], len=sizeof(data_format)*entcomp */
	unsigned int  entval;   /* Entry value, dentry[8-11] */
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
	const char *TagXPnames[] ={
		"Title", "Comment", "Author", "Keywords", "Subject" };

        /* 1. Open JPEG file */
        fil=fopen(fpath, "rb");
        if(fil==NULL) {
                egi_dperr("Fail to open '%s'", fpath);
		return -1;
        }

	/* 2. Read marker */
	ret=fread(marker, 1, 2, fil);
	if(ret<2) {
		egi_dperr("Fail to read file head marker");
		fclose(fil);
		return -2;
	}
	off +=2;

	/* 3. Check JPEG file header as SOI segment, no length data.  */
	if(marker[0]!=0xFF || marker[1]!=0xD8) {
		egi_dpstd("'%s' is NOT a JPEG file!\n", fpath);
		fclose(fil);
		return -3;
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
			continue;
		}
		/* M2. Find  0xFF at marker[1] */
		else if( marker[0]!=0xFF && marker[1]==0xFF ) {
			/* rewind back one byte */
			fseek(fil, -1, SEEK_CUR);
			off -=1;
			continue;
		}
		/* M3. Find a mark */
		else if(marker[0]==0xFF && marker[1]!=0xFF) {

			if(marker[1]!=0x00) { /* ??? FF/00 sequences in the compressed image data. */
//			     egi_dpstd("Marker Code: 0x%02X\n", marker[1]);
			}
			else {
				continue;
				egi_dpstd("<<< Marker code:0xFF00 >>>!\n");
			}

			/* Set segHeadPos */
			SegHeadPos=ftell(fil)-2; /* At beginning of the Marker */

			/* Get segment data length */
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

			/* Bigendian here */
			seglen = (tmpbuf[0]<<8)+tmpbuf[1];  /* !!! including 2bytes of lenght value !!! */
			sdatasize = seglen-2;

			/* Parse Segment */
			switch(marker[1]) {
			   /* Get image size from Segment SOF0  */
			   case 0xC0: case 0xC1: case 0xC2: case 0xC3:
			   case 0xC5: case 0xC6: case 0xC7:
			   case 0xC9: case 0xCA: case 0xCB:
			   case 0xCD: case 0xCE: case 0xCF:
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
					fclose(fil);
					return -3;
				}
				off +=6;

				/* Image size */
				imgBPC=tmpbuf[0];
				imgHeight=(tmpbuf[1]<<8) | tmpbuf[2];  /* bigenidan here */
				imgWidth=(tmpbuf[3]<<8) | tmpbuf[4];
				imgCPP=tmpbuf[5];
				egi_dpstd("Image size: W%dxH%dxCPP%dxBPC%d\n", imgWidth, imgHeight, imgCPP, imgBPC);
			   	break;
			   /* Define Number of lines, NOT common */
			   case 0xDC:
				ret=fread(tmpbuf, 1, 2, fil);
				if(ret<2) {
					egi_dperr("Fail to read number of lines.");
					fclose(fil);
					return -3;
				}
				off +=2;

				egi_dpstd(DBG_YELLOW"Marker DNL, Get number of lines: %d\n"DBG_RESET, (tmpbuf[1]<<8) + tmpbuf[0]);
				break;
			   /* Comment Segment */
			   case 0xFE:
			   case 0xEC:  /* APP12 0xEC  Also comments  */
#if 1 /* TEST: -------------- */
			   case 0xE0: case 0xE2: case 0xE3: case 0xE4: case 0xE5: case 0xE6: case 0xE7:
			   case 0xE8: case 0xE9: case 0xEA: case 0xEB: case 0xED: case 0xEF:
#endif

#if 0	/* TEST: --------------------------------*/
				if( marker[1]==0xE0 ) {
					printf("Marker 0xE0 seglen=%d\n", seglen);
				}
#endif

				/* Read comment to buff */
				if( sizeof(buff)-1 < sdatasize ) {
					/* Maybe NOT comment! OR printable string! */
					egi_dpstd(" comment len=%u > sizeof buff[]!\n", sdatasize);
					//fclose(fil);
					//return -4;

					ret=fread(buff, 1, sizeof(buff)-1, fil);
                                	if(ret<sizeof(buff)-1) {
                                        	egi_dperr("Fail to read COMMENT data.");
                                       	 	fclose(fil);
                                        	return -4;
					}
					buff[sizeof(buff)-1]='\0';
					off += sizeof(buff)-1;
				}
				else {
					ret=fread(buff, 1, sdatasize, fil);
					if(ret<sdatasize) {
						egi_dperr("Fail to read COMMENT data.");
						fclose(fil);
						return -4;
					}
					buff[sdatasize]='\0';
					off +=sdatasize;
				}

				/* Print comment */
				if(marker[1]==0xFE || marker[1]==0xEC)
					printf("Comment(FE,EC): %s\n", buff);
				else
					printf("Data(E0~EF): %s\n", buff);

			 	break;

//////////////////////  Start Exif data (case 0xE1)  /////////////////////
			   case 0xE1:  /*  Exif APP1 */
				/* ASCII character "Exif" and 2bytes of 0x00 are used. */
                                ret=fread(buff, 1, 6, fil);
                                if(ret<6) {
                                        egi_dperr("Fail to read 6bytes Exif_Header.");
                                        fclose(fil);  return -5;
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
                                        fclose(fil);  return -5;
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
				    T(2Bs)+F(2Bs)+N(4Bs)+D(4Bs)   Entry0
				    T(2Bs)+F(2Bs)+N(4Bs)+D(4Bs)   Entry1
				    ... ...
				    T(2Bs)+F(2Bs)+N(4Bs)+D(4Bs)   Entry2
				    L(4Bs)   Offset to next IFD
				*/

				//if(IFDstage==stage_IFD0)
				//	IFDstage=stage_IFD1;


				/* E1.5.1 Read number of directory entrys */
                                ret=fread(buff, 1, 2, fil);
                                if(ret<2) {
                                        egi_dperr("Fail to read 2bytes number of directory entry.");
                                        err=-5;  goto END_FUNC;
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
                                	        err=-5;  goto END_FUNC;
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

			/* Restore SEEK position */
			fseek(fil, SaveCurPos, SEEK_SET);
		    }

		    break;
		case 0x0132:  /* 0x0132 DataTime, ascii string,  "YYYY:MM:DD HH:MM:SS"+0x00, 20Bytes */
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

			/* Restore SEEK position */
			fseek(fil, SaveCurPos, SEEK_SET);
		    }
		    break;
		case 0x013e:  /* 0x013e WhitePoint, 2 unsigned_rational */
		    break;
		case 0x013f:  /* 0x013f PrimaryChromatictities, 6 unsigned rational */
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

				/* E1.5.5 Update/Shift IFD stage */
				if(IFDstage==stage_IFD0) {
				     offIFD0=0; /* Reset */
				     printf("End of IFD0 directory, offNextIFD=%ld\n", offNextIFD);
				     offIFD1=offNextIFD;
				}
				else if(IFDstage==stage_IFD1) {
				     printf("End of IFD1 directory, offNextIFD=%ld\n", offNextIFD);
				     offIFD1=0; /* Reset! */
				}
				else if(IFDstage==stage_SubIFD) {
				        printf("End of SubIFD directory, offNextIFD=%ld, ---- BREAK ANYWAY-----\n", offNextIFD);
					break; /* MUST break here, it goes to IFD1 AGAIN~ LOOP... */
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

				/* E1.5.8 Skip to IFDx if offNextIFD>0 ??? */

				break;
//////////////////////  END Exif data (case 0xE1)  /////////////////////

			   case 0xD9:
				printf("EOI: End of JPEG file!\n");
				goto END_FUNC;
				break;

			   default:

			   	break;
			} /* END switch (marker[1]) */

			/* Get to the END of the segment */
			if(fseek(fil, SegHeadPos+2+seglen, SEEK_SET)!=0) {
				egi_dperr("Fail to fseek the END of segment.");
                                err=-5; goto END_FUNC;
			}

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

	return err;
}


