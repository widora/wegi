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

 --->	typedef struct SavedImage {
	    GifImageDesc ImageDesc;
	    GifByteType *RasterBits;         	// on malloc(3) heap
	    int ExtensionBlockCount;         	// Count of extensions before image
	    ExtensionBlock *ExtensionBlocks; 	// Extensions before image
	} SavedImage;

 --->	typedef struct GifFileType {
		    GifWord SWidth, SHeight;         // Size of virtual canvas
		    GifWord SColorResolution;        // How many colors can we generate?
		    GifWord SBackGroundColor;        // Background color for virtual canvas
		    GifByteType AspectByte;          // Used to compute pixel aspect ratio
		    ColorMapObject *SColorMap;       // Global colormap, NULL if nonexistent
		    int ImageCount;                  // Number of current image (both APIs)
		    GifImageDesc Image;              // Current image (low-level API)
		    SavedImage *SavedImages;         // Image sequence (high-level API)
		    int ExtensionBlockCount;         // Count extensions past last image
		    ExtensionBlock *ExtensionBlocks; // Extensions past last image
		    int Error;                       // Last error condition reported
		    void *UserData;                  // hook to attach user data (TVT)
    		void *Private;                   // Don't mess with this!
	} GifFileType;


 --->	typedef struct GraphicsControlBlock {
	    	int DisposalMode;
		#define DISPOSAL_UNSPECIFIED     0       // No disposal specified.
		#define DISPOSE_DO_NOT           1       // Leave image in place
		#define DISPOSE_BACKGROUND       2       // Set area too background color
		#define DISPOSE_PREVIOUS         3       // Restore to previous content
		bool UserInputFlag;      	// User confirmation required before disposal
		int DelayTime;           	// pre-display delay in 0.01sec units
	    	int TransparentColor;    	// Palette index for transparency, -1 if none
		#define NO_TRANSPARENT_COLOR    -1
	} GraphicsControlBlock;

 2. Parameters relating to transparent settings:
    2.0  Set ImgAlpha_ON TRUE to turn on transparency settings!
    2.1  Defined in gif control block:gcb.TransparentColor, trans_color=gcb.TransparentColor.
    2.2  User can define GifFile->SBackGroundColor as transparent color too,
         by setting Bkg_Transp as TRUE.
    2.3  User may define user_trans_color also as transparent color.
    2.4  When Disposal_Mode(gcb.DisposalMode) is 2, screen will be restored
	 to original bkg image, implemented by FB FILO or FB buff page copy.

   				 --- WARN ---
   	All transparency switches MUST correspondes to GIF designer's intent!

 3. Usually the first GIF image is in screen/canvas size with defined bkgcolor,
    and following images are smaller in size and only areas with changing pixels.

 4. For big GIF file, DO NOT try to use slurp method, it needs large mem space!

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
void display_image( int Width, int Height, int offx, int offy,
		  ColorMapObject *ColorMap, GifByteType *buffer);
void display_slogan(void);

static EGI_IMGBUF *Simgbuf=NULL; 	/* an IMGBUF to hold gif image canvas/screen imgbuf */

static bool ImgAlpha_ON=true;    	/* User define:
				  	 * True: apply imgbuf alpha
				  	 * Note: !!! Turn off ONLY when the GIF file has no transparent
				         * color at all, then it will speed up IMGBUF displaying.
				  	*/
static bool Is_ImgColorMap=false;  	/* TRUE If color map is local image color map, NOT global screen color map
				    	 * Defined in gif file.
				    	*/
static int  Disposal_Mode=0;	   	/* Defined in gif file:
						0. No disposal specified
					 	1. Leave image in place
						2. Set area to background color(image)
						3. Restore to previous content
					 */
static int  trans_color=-1;     	/* Palette index for transparency, -1 if none, or NO_TRANSPARENT_COLOR
					  * Defined in gif file.
					 */
static int  user_trans_color=-1;       /* -- FOR TEST ONLY --  Usually disable it with -1.
					 * User defined palette index for the transparency
					 * When ENABLE, you shall use FB FILO or FB buff page copy! or moving
					 * trails will overlapped! BUT, BUT!!! If image Disposal_Mode != 2,then
					 * the image will mess up!!!
					 * To check image[0] for user_trans_color index !!!, for following
					 * image[] MAY contains trans_color only !!!
					 */
static bool Bkg_Transp=false;      	/* Usually it's FALSE!  as trans_color will cause the same effect.
					 * If true, make backgroud color transparent. User define.
					 * WARN!: Original GIF should has separated background color from image,
					 * otherwise the same color in image will be transparent also!
					 */
static int  bkg_color=0;	  	/* Back ground color index, defined in gif file. */
static int  SWidth, SHeight;      	/* screen(gif canvas) width and height, defined in gif file */
static int  offx, offy;	          	/* gif block image width and height, defined in gif file */
static  SavedImage *ImageData;     	/* pointer to savedimages in gifFile after DGifSlurp() */
static  ExtensionBlock  *ExtBlock; 	/* Pointer to extension block in savedimage */

/*------------------------------
	   MAIN()
------------------------------*/
int main(int argc, char ** argv)
{
    int NumFiles=1;
    char *FileName=argv[1];

    int	i;		    /* as for GIF image index */
    int j;
    int Width=0, Height=0;  /* image block size */
    int DelayMs;	    /* delay time in ms */
    int DelayFact;	    /* adjust delay time according to block image size */

    GifFileType *GifFile;
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

    /* Slurp reads an entire GIF into core, hanging all its state info off
     *  the GifFileType pointer, it may take a short of time...
     */
    printf("%s: Slurping GIF file and put images to memory...\n", __func__);
    if (DGifSlurp(GifFile) == GIF_ERROR) {
	printf("Fails to slurp the gif file!\n");
        PrintGifError(GifFile->Error);
        exit(EXIT_FAILURE);
    }

    if (GifFile->SHeight == 0 || GifFile->SWidth == 0) {
	fprintf(stderr, "Image of width or height 0\n");
	exit(EXIT_FAILURE);
    }

    /* Get GIF version*/
    printf("Gif Verion: %s\n", DGifGetGifVersion(GifFile));
    printf("Total ImageCount=%d\n", GifFile->ImageCount); /* after slurp, imagecount is total number! */
    if( GifFile->ImageCount < 1 )
	exit(-1);

    /* Get back ground color */
    bkg_color=GifFile->SBackGroundColor;
    printf("Background color index: %d\n", bkg_color);
    printf("SColorResolution: %d\n", (int)GifFile->SColorResolution);

    /* get SWidth and SHeight */
    SWidth=GifFile->SWidth;
    SHeight=GifFile->SHeight;
    printf("GifScreen size: %dx%d ---\n", SWidth, SHeight);

    /* Get global color map */
    Is_ImgColorMap=false;
    ColorMap=GifFile->SColorMap;
    printf("Global Colorcount=%d\n", ColorMap->ColorCount);
    /* check that the background color isn't garbage (SF bug #87) */
    if (GifFile->SBackGroundColor < 0 || GifFile->SBackGroundColor >= ColorMap->ColorCount) {
        fprintf(stderr, "Background color out of range for colormap\n");
        exit(EXIT_FAILURE);
    }

    /* Display images in GifFileType->SavedImages, as put by DGifSlurp() already.*/

while(1) {	/////////////////////    LOOP TEST    ///////////////////////

    /*  init FB: Copy background buffer page to working buffer page */
    memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.screensize);

    for( i=0; i< GifFile->ImageCount; i++ )
    {
	printf("\nGet ImageData[%d] ...\n", i);
        ImageData=&GifFile->SavedImages[i];
	printf("ExtensionBlockCount=%d\n", ImageData->ExtensionBlockCount);

        /* Get local image colorMap */
        if(ImageData->ImageDesc.ColorMap) {
        	Is_ImgColorMap=true;
	        ColorMap=ImageData->ImageDesc.ColorMap;
		printf("Local image Colorcount=%d\n", ColorMap->ColorCount);
        }
        else {
	        Is_ImgColorMap=false;
		ColorMap=GifFile->SColorMap;
	        //printf("Global Colorcount=%d\n", ColorMap->ColorCount);
        }
        //printf("ColorMap.BitsPerPixel=%d\n", ColorMap->BitsPerPixel);

        /* get Block image offset and size */
        offx=ImageData->ImageDesc.Left;
        offy=ImageData->ImageDesc.Top;
        Width=ImageData->ImageDesc.Width;
        Height=ImageData->ImageDesc.Height;
        printf("image: off(%d,%d) size(%dx%d) \n", offx,offy,Width, Height);

        /* Get extension block */
        if( ImageData->ExtensionBlockCount > 0 )
	{
		/* Search for control block */
		for( j=0; j< ImageData->ExtensionBlockCount; j++ )
		{
		    ExtBlock=&ImageData->ExtensionBlocks[j];
		    if( ExtBlock->Function == GRAPHICS_EXT_FUNC_CODE )
		    {
               		if (DGifExtensionToGCB(ExtBlock->ByteCount, ExtBlock->Bytes, &gcb) == GIF_ERROR) {
                           printf("%s: DGifExtensionToGCB() Fails!\n",__func__);
		           continue;
			 }

                        /* ----- for test ---- */
                        #if 1
                        printf("\tTransparency on: %s\n",
           			gcb.TransparentColor != -1 ? "yes" : "no");
                        printf("\tTransparent Index: %d\n", gcb.TransparentColor);
                        printf("\tDelayTime: %d\n", gcb.DelayTime);
	                printf("\tDisposalMode: %d\n", gcb.DisposalMode);
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
	                #if 0
        	        DelayMs=gcb.DelayTime*10*DelayFact/40000;
        	        sleep(DelayMs/1000);
                	usleep((DelayMs%1000)*1000);
			if(DelayMs==0)
	                        usleep(25000);
        	        #else
                        DelayMs=gcb.DelayTime*10;
                	usleep(DelayMs*1000);
	                #endif

		     }
		} /* END for(j):  read extension block*/
	}

	else {	/* NO extension block */
		ExtBlock=NULL;
	}

	/* <<< Display image >> */
//	printf(" RasterBits[10]=%d\n", ImageData->RasterBits[10]);
	/* display gif image, ALWAYS after a control block! */
       	display_image(Width, Height, offx, offy, ColorMap, ImageData->RasterBits); //ScreenBuffer);

    } /* END for( i ): read/display image squeces */


}	/////////////////////    END : LOOP TEST    ///////////////////////

    /* Close file */
    if (DGifCloseFile(GifFile, &Error) == GIF_ERROR) {
	PrintGifError(Error);
	exit(EXIT_FAILURE);
    }

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


void display_slogan(void)
{
	const wchar_t *wstr1=L"  EGI: miniUI Powered by Openwrt";
	const wchar_t *wstr2=L"                 小巧而丽致\n\
    奔跑在WIDORA_NEO上";


        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          17, 17, wstr1,                    /* fw,fh, pstr */
                                          320, 6, 6,                        /* pixpl, lines, gap */
                                          0, 240-30,                        /* x0,y0, */
                                          COLOR_RGB_TO16BITS(0,151,169), -1, 255 );   /* fontcolor, transcolor,opaque */

        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          24, 24, wstr2,                    /* fw,fh, pstr */
                                          320, 6,  3,                       /* pixpl, lines, gap */
                                          0, 150,                           /* x0,y0, */
                                          COLOR_RGB_TO16BITS(224,60,49), -1, 255 );   /* fontcolor, transcolor,opaque */
}


/*------------------------------------------------------------------
Writing current gif image block data to the Simgbuf and then display
the Simgbuf.

@Width, Height:  Width and Heigh for the current gif image size.
@offx, offy:	 offset of current image block relative to full gif
	         image screen/canvas.
@ColorMap:	 Color map for current image block.
@buffer:         Gif image rasterbits.

-------------------------------------------------------------------*/
void display_image( int Width, int Height, int offx, int offy,
		  ColorMapObject *ColorMap, GifByteType *buffer)
{
    int i,j;
    int pos=0;
    int spos=0;
    GifColorType *ColorMapEntry;
    int xres=gv_fb_dev.pos_xres;
    int yres=gv_fb_dev.pos_yres;
    EGI_16BIT_COLOR img_bkcolor;

    /* Create Simgbuf */
    if(Simgbuf==NULL) {
	     /* get bkg color */
             if(Is_ImgColorMap) /* TODO:  not tested! */
                      ColorMapEntry = &ColorMap->Colors[bkg_color];
             else             /* Is global color map. */
                      ColorMapEntry = &ColorMap->Colors[bkg_color];
             img_bkcolor=COLOR_RGB_TO16BITS( ColorMapEntry->Red,
                                             ColorMapEntry->Green,
                                             ColorMapEntry->Blue);
	    /* create imgbuf, with bkg alpha == 0!!! */
	    Simgbuf=egi_imgbuf_create(SHeight, SWidth, 0, img_bkcolor); //height, width, alpha, color
	    if(Simgbuf==NULL)
		return;

	    /* NOTE: if applay Simgbuf->alpha, then all initial pixels are invisible */
	    if(!ImgAlpha_ON) {
		    free(Simgbuf->alpha);
		    Simgbuf->alpha=NULL;
	   }
    }

   /* Limit Screen width x heigh, necessary? */
   if(Width==SWidth)offx=0;
   if(Height==SHeight)offy=0;
   //printf("%s: input Height=%d, Width=%d offx=%d offy=%d\n", __func__, Height, Width, offx, offy);

    //printf(" buffer[0]=%d\n", buffer[0]);

    for(i=0; i<Height; i++)
    {
	 /* update block of Simgbuf */
	 for(j=0; j<Width; j++ ) {
	      pos=i*Width+j;
              spos=(offy+i)*SWidth+(offx+j);
	      /* Nontransparent color: set color and set alpha to 255 */
	      if( trans_color < 0 || trans_color != buffer[pos]  )
              {
		  if(Is_ImgColorMap) /* TODO:  not tested! */
		         ColorMapEntry = &ColorMap->Colors[buffer[pos]];
		  else 		   /* Is global color map. */
		         ColorMapEntry = &ColorMap->Colors[buffer[pos]];

                  Simgbuf->imgbuf[spos]=COLOR_RGB_TO16BITS( ColorMapEntry->Red,
							    ColorMapEntry->Green,
							    ColorMapEntry->Blue);
		  /* Only if imgbuf alpha is ON */
		  if(ImgAlpha_ON)
		  	Simgbuf->alpha[spos]=255;
	      }
	      /* Just after above!, If IMGBUF alpha applys, Transprent color: set alpha to 0 */
	      if(ImgAlpha_ON) {
		  /* Make background color transparent */
	          if( Bkg_Transp && ( buffer[pos] == bkg_color) ) {  /* bkg_color meaningful ONLY when global color table exists */
		      Simgbuf->alpha[spos]=0;
	          }
	          if( buffer[pos] == trans_color || buffer[pos] == user_trans_color) {   /* ???? if trans_color == bkg_color */
		      Simgbuf->alpha[spos]=0;
	          }
	      }
	      /*  ELSE : Keep old color and alpha value in Simgbuf->imgbuf */
          }
    }

    //newimg=egi_imgbuf_resize(&Simgbuf, 240, 240);  // EGI_IMGBUF **pimg, width, height
    if( Disposal_Mode==2 ) /* Set area to background color/image */
    {
   	  /* Display imgbuf, with FB FILO ON, OR fill background with FB bkg buffer */
	  fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
	  fb_filo_on(&gv_fb_dev); /* start collecting old FB pixel data */
    }

    /* display the whole Gif screen/canvas */
    egi_imgbuf_windisplay( Simgbuf, &gv_fb_dev, -1,             /* img, fb, subcolor */
			   SWidth>xres ? (SWidth-xres)/2:0,     /* xp */
			   SHeight>yres ? (SHeight-yres)/2:0,   /* yp */
			   SWidth>xres ? 0:(xres-SWidth)/2,	/* xw */
			   SHeight>yres ? 0:(yres-SHeight)/2,	/* yw */
                           Simgbuf->width, Simgbuf->height    /* winw, winh */
			);

//    display_slogan();

    if( Disposal_Mode==2  )  /* Set area to background color/image */
      	fb_filo_off(&gv_fb_dev); /* start collecting old FB pixel data */


    /* Write BLOCK image to FB */
//    if(DirectFB_ON) {
//
//

    /* refresh FB page */
    fb_page_refresh(&gv_fb_dev);


    /* reset trans color index */
//    trans_color=-1;
}

