/* -----------------------------------------------------------------------
A routine derived from GIFLIB examples: convert GIF to 24-bit RGB pixel triples.

     The GIFLIB distribution is Copyright (c) 1997  Eric S. Raymond
			SPDX-License-Identifier: MIT

	== Authors of Giflib ==
Gershon Elber <gershon[AT]cs.technion.sc.il>
        original giflib code
Toshio Kuratomi <toshio[AT]tiki-lounge.com>
        uncompressed gif writing code
        former maintainer
Eric Raymond <esr[AT]snark.thyrsus.com>
        current as well as long time former maintainer of giflib code
others ...


Note:
TODO:
 1. Is_ImgColorMap=TRUE NOT tested yet!

	typedef struct GraphicsControlBlock {
	    	int DisposalMode;
		#define DISPOSAL_UNSPECIFIED      0       // No disposal specified.
		#define DISPOSE_DO_NOT            1       // Leave image in place
		#define DISPOSE_BACKGROUND        2       // Set area too background color
		#define DISPOSE_PREVIOUS          3       // Restore to previous content
		bool UserInputFlag;      // User confirmation required before disposal
		int DelayTime;           // pre-display delay in 0.01sec units
	    	int TransparentColor;    // Palette index for transparency, -1 if none
		#define NO_TRANSPARENT_COLOR    -1
	} GraphicsControlBlock;

 2. Parameters relating to transparent settings:
    2.0  Set ImgAlpha_ON TURE to turn ontransparency settings!
    2.1  Defined in gif control block, trans_color=gcb.TransparentColor.
    2.2  User can define GifFile->SBackGroundColor as transparent color too,
         by setting Bkg_Transp TURE.
    2.3  User may define user_trans_color also as transparent color.
    2.4  When Disposal_Mode(gcb.DisposalMode) is 2, screen will be restored
	 to original bkg image, implemented by FB FILO or FB buff page copy.

Midas Zhou
---------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <fcntl.h>

#include "gif_lib.h"
#include "egi_common.h"
#include "egi_FTsymbol.h"

#define PROGRAM_NAME    "gif2rgb"
#define GIF_MESSAGE(Msg) fprintf(stderr, "\n%s: %s\n", "gif2rgb", Msg)
#define GIF_EXIT(Msg)    { GIF_MESSAGE(Msg); exit(-3); }
#define UNSIGNED_LITTLE_ENDIAN(lo, hi)	((lo) | ((hi) << 8))

void GifQprintf(char *Format, ...);
void PrintGifError(int ErrorCode);
void display_gif( int Width, int Height, int offx, int offy,
		  ColorMapObject *ColorMap, GifRowType *ScreenBuffer);
void display_slogan(void);

static EGI_IMGBUF *Simgbuf=NULL; /* an IMGBUF to hold gif image canvas/screen imgbuf */

static bool ImgAlpha_ON=true;    /* User define:
				  * True: apply imgbuf alpha
				  * Note: !!! Turn off ONLY when the GIF file has no transparent color at all.
				  * than it will speed up IMGBUF displaying.
				  */
static bool Is_ImgColorMap=false;  /* TRUE If color map is image color map, NOT global screen color map
				    * Defined in gif file.
				    */
static int  Disposal_Mode=0;	   /* Defined in gif file:
					0. No disposal specified
				 	1. Leave image in place
					2. Set area to background color(image)
					3. Restore to previous content
				    */
static int  trans_color=-1;       /* Palette index for transparency, -1 if none, or NO_TRANSPARENT_COLOR
				   * Defined in gif file.
				   */
static int  user_trans_color=-1;   /* -1, User defined palette index for the transparency */
static bool Bkg_Transp=false;     /* If true, make backgroud transparent. User define. */

static int  bkg_color=0;	  /* Back ground color index, defined in gif file. */

static int  SWidth, SHeight;      /* screen(gif canvas) width and height, defined in gif file */
static int  offx, offy;	          /* gif block image width and height, defined in gif file */


/*------------------------------
	   MAIN()
------------------------------*/
int main(int argc, char ** argv)
{
    int NumFiles=1;
    char *FileName=argv[1];

    int	i, j, Size;
    int Width=0, Height=0;  /* image block size */
    int Row=0, Col=0;	    /* init. as image block offset relative to screen/canvvas */
    int ExtCode, Count;
    int DelayMs;	    /* delay time in ms */
    int DelayFact;	    /* adjust delay time according to block image size */

    GifRecordType RecordType;
    GifByteType *Extension;
    GifRowType *ScreenBuffer;  /* typedef unsigned char *GifRowType */
    GifFileType *GifFile;
    int
	InterlacedOffset[] = { 0, 4, 2, 1 }, /* The way Interlaced image should. */
	InterlacedJumps[] = { 8, 8, 4, 2 };    /* be read - offsets and jumps... */
    int ImageNum = 0;
    ColorMapObject *ColorMap;
    GraphicsControlBlock gcb;

    int Error;

    /* --- Init FB DEV --- */
    printf("init_fbdev()...\n");
    if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;
    /* Set screen view type as LANDSCAPE mode */
    fb_position_rotate(&gv_fb_dev, 3);

    //fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_BLUE);
    /* init FB back ground buffer page */
    memcpy(gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.map_fb, gv_fb_dev.screensize);
//    memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.screensize);

     /* --- Load fonts --- */
    printf("symbol_load_allpages()...\n");
    if(symbol_load_allpages() !=0 ) {       /* load sys fonts */
             printf("Fail to load sym pages,quit.\n");
             return -2;
     }
    if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
             printf("Fail to load FT appfonts, quit.\n");
            return -2;
    }


while(1) {	/////////////////////    LOOP TEST    ///////////////////////

    /*  init FB: Copy background buffer page to working buffer page */
    memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.screensize);

    /* Open gif file */
    if (NumFiles == 1) {
	if ((GifFile = DGifOpenFileName(FileName, &Error)) == NULL) {
	    PrintGifError(Error);
	    exit(EXIT_FAILURE);
	}
    }
    else {
	/* Use stdin instead: */
	if ((GifFile = DGifOpenFileHandle(0, &Error)) == NULL) {
	    PrintGifError(Error);
	    exit(EXIT_FAILURE);
	}
    }

    if (GifFile->SHeight == 0 || GifFile->SWidth == 0) {
	fprintf(stderr, "Image of width or height 0\n");
	exit(EXIT_FAILURE);
    }

    /* Get GIF version*/
    printf("Gif Verion: %s\n", DGifGetGifVersion(GifFile));

    /* Get color map */
#if 1
    if(GifFile->Image.ColorMap) {
	Is_ImgColorMap=true;
	ColorMap=GifFile->Image.ColorMap;
		printf("Image Colorcount=%d\n", GifFile->Image.ColorMap->ColorCount);
    }
    else {
	Is_ImgColorMap=false;
		ColorMap=GifFile->SColorMap;
	printf("Global Colorcount=%d\n", GifFile->SColorMap->ColorCount);
    }
    printf("ColorMap.BitsPerPixel=%d\n", ColorMap->BitsPerPixel);
    /* check that the background color isn't garbage (SF bug #87) */
    if (GifFile->SBackGroundColor < 0 || GifFile->SBackGroundColor >= ColorMap->ColorCount) {
        fprintf(stderr, "Background color out of range for colormap\n");
        exit(EXIT_FAILURE);
    }
#endif

    /* Get back ground color */
    bkg_color=GifFile->SBackGroundColor;
    printf("Background color index=%d\n", bkg_color);
    printf("SColorResolution = %d\n", (int)GifFile->SColorResolution);

    /* get SWidth and SHeight */
    SWidth=GifFile->SWidth;
    SHeight=GifFile->SHeight;
    printf("\n--- SWxSH=%dx%d ---\n", SWidth, SHeight);

    printf("ImageCount=%d\n", GifFile->ImageCount);

    /***   ---   Allocate screen as vector of column and rows   ---
     * Note this screen is device independent - it's the screen defined by the
     * GIF file parameters.
     */
    if ((ScreenBuffer = (GifRowType *)
	malloc(GifFile->SHeight * sizeof(GifRowType))) == NULL)
	    GIF_EXIT("Failed to allocate memory required, aborted.");

    Size = GifFile->SWidth * sizeof(GifPixelType);/* Size in bytes one row.*/
    if ((ScreenBuffer[0] = (GifRowType) malloc(Size)) == NULL) /* First row. */
	GIF_EXIT("Failed to allocate memory required, aborted.");

    for (i = 0; i < GifFile->SWidth; i++)  /* Set its color to BackGround. */
	ScreenBuffer[0][i] = GifFile->SBackGroundColor;

    for (i = 1; i < GifFile->SHeight; i++) {
	/* Allocate the other rows, and set their color to background too: */
	if ((ScreenBuffer[i] = (GifRowType) malloc(Size)) == NULL)
	    GIF_EXIT("Failed to allocate memory required, aborted.");

	memcpy(ScreenBuffer[i], ScreenBuffer[0], Size);
    }

    ImageNum = 0;
    /* Scan the content of the GIF file and load the image(s) in: */
    do {

	if (DGifGetRecordType(GifFile, &RecordType) == GIF_ERROR) {
	    PrintGifError(GifFile->Error);
	    exit(EXIT_FAILURE);
	}

	switch (RecordType) {
	    case IMAGE_DESC_RECORD_TYPE:
		if (DGifGetImageDesc(GifFile) == GIF_ERROR) {
		    PrintGifError(GifFile->Error);
		    exit(EXIT_FAILURE);
		}
		Row = GifFile->Image.Top; /* Image Position relative to Screen. */
		Col = GifFile->Image.Left;

		/* Get offx, offy, Original Row AND Col will increment later!!!  */
		offx = Col;
		offy = Row;
		Width = GifFile->Image.Width;
		Height = GifFile->Image.Height;

		/* check block image size and position */
    		printf("\nImageCount=%d\n", GifFile->ImageCount);
		GifQprintf("%s: Image %d at (%d, %d) [%dx%d]:     ",
				    PROGRAM_NAME, ++ImageNum, Col, Row, Width, Height );

		if (GifFile->Image.Left + GifFile->Image.Width > GifFile->SWidth ||
			   GifFile->Image.Top + GifFile->Image.Height > GifFile->SHeight) {
		    fprintf(stderr, "Image %d is not confined to screen dimension, aborted.\n",ImageNum);
		    exit(EXIT_FAILURE);
		}

		/* Get color (index) data */
		if (GifFile->Image.Interlace) {
		    printf(" Interlaced image data ...\n");
		    /* Need to perform 4 passes on the images: */
		    for (Count = i = 0; i < 4; i++)
			for (j = Row + InterlacedOffset[i]; j < Row + Height;
						 j += InterlacedJumps[i]  )
			{
			    GifQprintf("\b\b\b\b%-4d", Count++);
			    if ( DGifGetLine(GifFile, &ScreenBuffer[j][Col],
				 Width) == GIF_ERROR )
			    {
				 PrintGifError(GifFile->Error);
				 exit(EXIT_FAILURE);
			    }
			}
		}
		else {
		    printf(" Noninterlaced image data ...\n");
		    for (i = 0; i < Height; i++) {
			GifQprintf("\b\b\b\b%-4d", i);
			if (DGifGetLine(GifFile, &ScreenBuffer[Row++][Col],
				Width) == GIF_ERROR) {
			    PrintGifError(GifFile->Error);
			    exit(EXIT_FAILURE);
			}
		    }
		}
		printf("\n");

	       /* Get color map, colormap may be updated/changed here! */
	       if(GifFile->Image.ColorMap) {
			Is_ImgColorMap=true;
			ColorMap=GifFile->Image.ColorMap;
			printf("Image Colorcount=%d\n", GifFile->Image.ColorMap->ColorCount);
    	       }
	       else {
			Is_ImgColorMap=false;
			ColorMap=GifFile->SColorMap;
			printf("Global Colorcount=%d\n", GifFile->SColorMap->ColorCount);
	       }
	       printf("ColorMap.BitsPerPixel=%d\n", ColorMap->BitsPerPixel);

		/* display */
   		display_gif(Width, Height, offx, offy, ColorMap, ScreenBuffer);
		/* hold on ... see displaying delay in case EXTENSION_RECORD_TYPE */
		//usleep(55000);
		break;

	    case EXTENSION_RECORD_TYPE:
		/*  extension blocks in file: */
		if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR) {
		    PrintGifError(GifFile->Error);
		    exit(EXIT_FAILURE);
		}

		/*   --- parse extension code ---  */
                if (ExtCode == GRAPHICS_EXT_FUNC_CODE) {
                     if (Extension == NULL) {
                            printf("Invalid extension block\n");
                            GifFile->Error = D_GIF_ERR_IMAGE_DEFECT;
                            PrintGifError(GifFile->Error);
                            exit(EXIT_FAILURE);
                     }
                     if (DGifExtensionToGCB(Extension[0], Extension+1, &gcb) == GIF_ERROR) {
                            PrintGifError(GifFile->Error);
                            exit(EXIT_FAILURE);
                      }
		      /* ----- for test ---- */
		      #if 1
                      printf("Transparency on: %s\n",
                      			gcb.TransparentColor != -1 ? "yes" : "no");
                      printf("Transparent Index: %d\n", gcb.TransparentColor);
                      printf("DelayTime: %d\n", gcb.DelayTime);
		      printf("DisposalMode: %d\n", gcb.DisposalMode);
		      #endif

		      /* Get disposal mode */
		      Disposal_Mode=gcb.DisposalMode;
      		      /* Get trans_color */
		      trans_color=gcb.TransparentColor;

		      /* >200x200 no delay, 0 full delay */
		      if(Width*Height > 40000)
			DelayFact=0;
		      else
		      	DelayFact= (200*200-Width*Height);

		      /* Get delay time in ms, and delay */
		      #if 1
		        DelayMs=gcb.DelayTime*10*DelayFact/40000;
		        sleep(DelayMs/1000);
		        usleep((DelayMs%1000)*1000);
		      #else
			usleep(50000);
		      #endif

		      /* start delay counting... */

		 }

		/* Read out next extension and discard, TODO: Not useful information????  */
		while (Extension!=NULL) {
                    if (DGifGetExtensionNext(GifFile, &Extension) == GIF_ERROR) {
                        PrintGifError(GifFile->Error);
                        exit(EXIT_FAILURE);
                    }
                    if (Extension == NULL) {
			//printf("Extension is NULL\n");
                        break;
		    }
		    printf(" --- No use? ----- \n");
		    if (Extension[0] & 0x01) {
        		   trans_color = Extension[3];
                    	   printf("---Transparent Index: %d\n", trans_color);
		    }
		    Disposal_Mode = (Extension[0] >> 2) & 0x07;
		    printf("---Disposal Mode: %d\n", Disposal_Mode);
                }
		break;

	    case TERMINATE_RECORD_TYPE:
		break;

	    default:		    /* Should be trapped by DGifGetRecordType. */
		break;
	}

    } while (RecordType != TERMINATE_RECORD_TYPE);

    /* Free scree buffers */
    for (i = 0; i < SHeight; i++)
	free(ScreenBuffer[i]);
    free(ScreenBuffer);

    /* Close file */
    if (DGifCloseFile(GifFile, &Error) == GIF_ERROR) {
	PrintGifError(Error);
	exit(EXIT_FAILURE);
    }


}	/////////////////////    END : LOOP TEST    ///////////////////////


    /* free Simgbuf */
    egi_imgbuf_free(Simgbuf);
    Simgbuf=NULL;

    /* release FB dev */
    fb_filo_flush(&gv_fb_dev);
    release_fbdev(&gv_fb_dev);

  return 0;
}



/*****************************************************************************
 Same as fprintf to stderr but with optional print.
******************************************************************************/
void GifQprintf(char *Format, ...)
{
bool GifNoisyPrint = true;

    va_list ArgPtr;

    va_start(ArgPtr, Format);

    if (GifNoisyPrint) {
        char Line[128];
        (void)vsnprintf(Line, sizeof(Line), Format, ArgPtr);
        (void)fputs(Line, stderr);
    }

    va_end(ArgPtr);
}

void PrintGifError(int ErrorCode)
{
    const char *Err = GifErrorString(ErrorCode);

    if (Err != NULL)
        fprintf(stderr, "GIF-LIB error: %s.\n", Err);
    else
        fprintf(stderr, "GIF-LIB undefined error %d.\n", ErrorCode);
}


/*------------------------------------------------------------------
Writing current gif image block data to the Simgbuf and then display
the Simgbuf.

@Width, Height:  Width and Heigh for the current gif image size.
@offx, offy:	 offset of current image block relative to full gif
	         image screen/canvas.
@ColorMap:	 Color map for current image block.
@ScreenBuffer:   Gif image screen/canvas buffer for color indices.

-------------------------------------------------------------------*/
void display_gif( int Width, int Height, int offx, int offy,
		  ColorMapObject *ColorMap, GifRowType *ScreenBuffer)
{
    int i,j;
    int pos=0;
    GifRowType GifRow;
    GifColorType *ColorMapEntry;

    int xres=gv_fb_dev.pos_xres;
    int yres=gv_fb_dev.pos_yres;
    printf("xres=%d, yres=%d\n", xres, yres);

    /* fill to Simgbuf */
    if(Simgbuf==NULL) {
	    Simgbuf=egi_imgbuf_create(SHeight, SWidth, 0, 0);//255, 0); //height, width, alpha, color
	    if(Simgbuf==NULL)
		return;

	    /* NOTE: if applay Simgbuf->alpha, then all initial pixels are invisible */
	    if(!ImgAlpha_ON) {
		    free(Simgbuf->alpha);
		    Simgbuf->alpha=NULL;
	   }
    }

   /* Limit Screen width x heigh */
   if(Width==SWidth)offx=0;
   if(Height==SHeight)offy=0;
   //printf("%s: input Height=%d, Width=%d offx=%d offy=%d\n", __func__, Height, Width, offx, offy);

   printf(" ScreenBuffer[2][2]=%d \n", ScreenBuffer[2][2]);

    for(i=offy; i<offy+Height; i++)
    {
 	 if(Is_ImgColorMap) {
	    GifRow = ScreenBuffer[i]; //[i-offy];
	 } else {
	    GifRow = ScreenBuffer[i];
	 }

	 /* update block of Simgbuf */
	 for(j=offx; j<offx+Width; j++ ) {
	      pos=i*SWidth+j;

//	     if( GifRow[j] > ColorMap->ColorCount || GifRow[j]<0 )
//		printf("GifRow[%d]=%d invalid! \n", j,GifRow[j]);

	      /* Nontransparent color: set color and set alpha to 255 */
	      if( trans_color < 0 || trans_color != GifRow[j]  )
              {
		  if(Is_ImgColorMap) /* TODO:  not tested! */
		         ColorMapEntry = &ColorMap->Colors[GifRow[j]]; // [GifRow[j-offx]];
		  else 		   /* Is global color map. */
		         ColorMapEntry = &ColorMap->Colors[GifRow[j]];

                  Simgbuf->imgbuf[pos]=COLOR_RGB_TO16BITS(  ColorMapEntry->Red,
							    ColorMapEntry->Green,
							    ColorMapEntry->Blue);
		  /* Only if imgbuf alpha is ON */
		  if(ImgAlpha_ON)
		  	Simgbuf->alpha[pos]=255;
	      }
	      /* Just after above!, If IMGBUF alpha applys, Transprent color: set alpha to 0 */
	      if(ImgAlpha_ON) {
		  /* Make background color transparent */
	          if( Bkg_Transp && ( GifRow[j] == bkg_color) ) {  /* bkg_color meaningful ONLY when global color table exists */
		      Simgbuf->alpha[pos]=0;
	          }
	          if( GifRow[j] == trans_color || GifRow[j] == user_trans_color) {   /* ???? if trans_color == bkg_color */
		      Simgbuf->alpha[pos]=0;
	          }
	      }
	      /*  ELSE : Keep old color and alpha value in Simgbuf->imgbuf */
	      //else printf(" ======   trans_color=%d GirRow[j]=%d  ====== \n", trans_color,GifRow[j]);
          }
    }

    //newimg=egi_imgbuf_resize(&Simgbuf, 240, 240);  // EGI_IMGBUF **pimg, width, height

    if( Disposal_Mode==2 ) /* Set area to background color/image */
    {
   	  /* Display imgbuf, with FB FILO ON, OR fill background with FB bkg buffer */
	  fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
	  fb_filo_on(&gv_fb_dev); /* start collecting old FB pixel data */
    }

    egi_imgbuf_windisplay( Simgbuf, &gv_fb_dev, -1,            /* img, fb, subcolor */
			   SWidth>xres ? (SWidth-xres)/2:0,       /* xp */
			   SHeight>yres ? (SHeight-yres)/2:0,       /* yp */
			   SWidth>xres ? 0:(xres-SWidth)/2,	/* xw */
			   SHeight>yres ? 0:(yres-SHeight)/2,	/* yw */
                           Simgbuf->width, Simgbuf->height    /* winw, winh */
			);

//    display_slogan();


    if( Disposal_Mode==2 )  /* Set area to background color/image */
      	fb_filo_off(&gv_fb_dev); /* start collecting old FB pixel data */

    /* refresh FB page */
    fb_page_refresh(&gv_fb_dev);

    /* reset trans color index, NOT necessary!*/
//  trans_color=-1;

}


void display_slogan(void)
{
	const wchar_t *wstr1=L"  EGI: miniUI Powered by Openwrt";
	const wchar_t *wstr2=L"                 小巧而丽致\n\
    奔跑在WIDORA_NEO上";


        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          17, 17, wstr1,                    /* fw,fh, pstr */
                                          320, 6, 6,                    /* pixpl, lines, gap */
                                          0, 240-30,                           /* x0,y0, */
                                          COLOR_RGB_TO16BITS(0,151,169), -1, 255 );   /* fontcolor, transcolor,opaque */

        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          24, 24, wstr2,                    /* fw,fh, pstr */
                                          320, 6,  3,                    /* pixpl, lines, gap */
                                          0, 150,                        /* x0,y0, */
                                          COLOR_RGB_TO16BITS(224,60,49), -1, 255 );   /* fontcolor, transcolor,opaque */
}


