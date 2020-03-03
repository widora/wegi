/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This module is a wrapper of GIFLIB routines and functions.

The GIFLIB distribution is Copyright (c) 1997  Eric S. Raymond
                  SPDX-License-Identifier: MIT


Midas-Zhou
------------------------------------------------------------------*/
#ifndef __EGI_GIF_H__
#define __EGI_GIF_H__
#include "gif_lib.h"
#include "egi_imgbuf.h"

typedef struct fbdev FBDEV;  /* Just a declaration, referring to definition in egi_fbdev.h */

/*** GIF saved images with uncompressed data   */
typedef struct  {
    bool    	VerGIF89;		 /* Version: GIF89 OR GIF87 */
    int 	SWidth;         	 /* Size of virtual canvas */
    int 	SHeight;

    int 	SColorResolution;        /* How many colors can we generate? */
    int 	SBackGroundColor;        /* Background color for virtual canvas */

    GifByteType 	AspectByte;      /* Used to compute pixel aspect ratio */
    ColorMapObject 	*SColorMap;      /* Global colormap, NULL if nonexistent. */
    int			ImageTotal;	 /* Total number of images */

//  GifImageDesc 	Image;           /* Current image (low-level API) */
    SavedImage 		*SavedImages;         /* Image sequence (high-level API) */
//    int 		ExtensionBlockCount;  /* Count extensions past last image */
//    ExtensionBlock 	*ExtensionBlocks;     /* Extensions past last image */

} EGI_GIF_DATA;


/*** 				--- NOTE ---
 * 1. For big GIF file, be careful to use EGI_GIF, it needs large mem space!
 * 2. No mutex lock applied for EGI_GIF, however EGI_IMGBUF HAS mutex lock imbedded.
 */
typedef struct egi_gif {
    bool    	VerGIF89;		 /* Version: GIF89 OR GIF87 */
    bool	ImgTransp_ON;		 /* Try image transparency */

  /* typedef int GifWord */
    int 	SWidth;         	 /* Size of virtual canvas */
    int 	SHeight;

    int		RWidth;			/* Size for RSimgbuf, Unapplicable if both <=0, */
    int		RHeight;		/* At lease either RWidth or RHeigth to be >0 */

    int 	BWidth;			/* Size of current block image */
    int 	BHeight;
    int		offx;			/* Current block offset relative to canvas origin */
    int		offy;

    /* 	--- For recording last status, 	update only when Disposal_Mode==2 */
    int 	last_BWidth;		/* record last size of current block image */
    int 	last_BHeight;
    int		last_offx;		/* record last block offset relative to canvas origin */
    int		last_offy;
    int		last_Disposal_Mode;	/* ! update for each frame */

    int 	SColorResolution;       /* How many colors can we generate? */
    int 	SBackGroundColor;       /* Background color for virtual canvas, color index */
    EGI_16BIT_COLOR	bkcolor;	/* 16bit color according to SBackGroundColor and SColorMap */

    GifByteType     AspectByte;     	/* Used to compute pixel aspect ratio */


    bool	  Is_DataOwner;		/* Ture:   SColorMap and SavedImages to be freed by egi_gif_free()
					 * 	   When the EGI_GIF is created by egi_gif_slurpFile(), it will auto. set to be true.
					 * False:  To be freed by egi_gifdata_free()
					 *	   Usually the EGI_GIF is created by egi_gif_create() with a given EGI_GIF_DATA.
					 */

   ColorMapObject   *SColorMap;     	/* Global colormap, NULL if nonexistent.
					 * if(!Is_DataOwner): A refrence poiner to  EGI_GIF_DATA.SColorMap, to be freed by egi_gifdata_free()
					 */

   SavedImage	  *SavedImages;   	/*** Image sequence (high-level API)
					 * if(!Is_DataOwner): A refrence poiner to  EGI_GIF_DATA.SavedImages, to be freed by egi_gifdata_free()
					 */



    int			ImageCount;     /* Index of current image (both APIs), starts from 0
					 * Index ready for display!!
					 */
    int			ImageTotal;	/* Total number of images */
    GifImageDesc 	Image;          /* Current image (low-level API) */


//    int 		ExtensionBlockCount;  /* Count extensions past last image */
//    ExtensionBlock 	*ExtensionBlocks;     /* Extensions past last image */

    EGI_IMGBUF		*Simgbuf;	      /* to hold GIF screen/canvas */

    /* To be applied when at lease either RWidth or RHeigth to be >0 */
    EGI_IMGBUF		*RSimgbuf;	      /* resized to RWidthxRHeight */

    /*  Following for one producer and one consumer scenario only! */
    pthread_t		thread_display;	 	/* displaying thread ID */
    bool		thread_running;	 	/* True if thread is running */
    bool		request_quit_display; 	/* True if request to quit egi_gif_displayFrame() */

} EGI_GIF;


/*------------------------------------------
Context for GIF displaying thread.
Parameter definition:
	   Refert to egi_gif_displayFrame( )
-------------------------------------------*/
typedef struct egi_gif_context
{
        FBDEV   *fbdev;
        EGI_GIF *egif;
        int     nloop;
        bool    DirectFB_ON;
        int     User_DisposalMode;	/* <0 disable */
        int     User_TransColor;	/* <0 disable */
        int     User_BkgColor;		/* <0 disable */
        int     xp;
        int     yp;
        int     xw;
        int     yw;
        int     winw, winh;
} EGI_GIF_CONTEXT;



/*** 	----- static functions -----
static void GifQprintf(char *Format, ...);
static void PrintGifError(int ErrorCode);
static void  	egi_gif_FreeSavedImages(SavedImage **psimg, int ImageCount);

inline static void egi_gif_rasterWriteFB( FBDEV *fbdev, EGI_IMGBUF *Simgbuf, int Disposal_Mode,
                                   int xp, int yp, int xw, int yw, int winw, int winh,
                                   int BWidth, int BHeight, int offx, int offy,
                                   ColorMapObject *ColorMap, GifByteType *buffer,
                                   int trans_color, int User_TransColor, int bkg_color,
                                   bool ImgTransp_ON, bool BkgTransp_ON );

static void *egi_gif_threadDisplay(void *argv);

*/

int  	  egi_gif_playFile(const char *fpath, bool Silent_Mode, bool ImgTransp_ON, int *ImageCount, int nloop, bool *sigstop);
EGI_GIF_DATA*  egi_gifdata_readFile(const char *fpath);
EGI_GIF*  egi_gif_create(const EGI_GIF_DATA *gif_data, bool ImgTransp_ON);
EGI_GIF*  egi_gif_slurpFile(const char *fpath, bool ImgTransp_ON);
void 	  egi_gifdata_free(EGI_GIF_DATA **gif_data);
void	  egi_gif_free(EGI_GIF **egif);

//void 	  egi_gif_displayFrame(FBDEV *fbdev, EGI_GIF *egif, int nloop, bool DirectFB_ON,
//                                             int User_DisposalMode, int User_TransColor,int User_BkgColor,
//                                             int xp, int yp, int xw, int yw, int winw, int winh );

void 	  egi_gif_displayGifCtxt( EGI_GIF_CONTEXT *gif_ctxt );

int 	  egi_gif_runDisplayThread(EGI_GIF_CONTEXT *gif_ctxt);

int 	  egi_gif_stopDisplayThread(EGI_GIF *egif);

#endif
