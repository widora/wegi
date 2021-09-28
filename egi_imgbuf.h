/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Jorunal:
2021-09-10:
	1. Add EGI_IMGFRAME and EGI_IMGMOTION

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#ifndef __EGI_IMGBUF_H__
#define __EGI_IMGBUF_H__

#include "egi_color.h"
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
//#include <freetype2/ftglyph.h>

#ifdef __cplusplus
 extern "C" {
#endif

typedef	struct {
		int x0;		/* Subimage left top starting point coordinates relative to image origin */
		int y0;
		int w;		/* Size of subimage */
		int h;
}EGI_IMGBOX;

typedef struct {
		EGI_16BIT_COLOR color;
		unsigned char 	alpha;
}EGI_16BIT_PIXEL;			/* also see PIXEL in egi_bjp.h */

typedef struct
{
	/* TODO NOTE: mutex only applied to several functions now....
	 */
	pthread_mutex_t	img_mutex;	/* mutex lock for imgbuf */
	// int refcnt;

        int 		height;	 	/* image height */
        int 		width;	 	/* image width */

	/* for normal image data storage */
        EGI_16BIT_COLOR *imgbuf; 	/* color data, for RGB565 format */
	EGI_IMGBOX 	*subimgs; 	/* sub_image boxes, subimgs[0] - subimgs[submax] */
	int 		submax;	 	/* max index of subimg,
					 * >=0, Max. index as for subimgs[index].
					 */
	EGI_8BIT_ALPHA 	*alpha;    	/* 8bit, alpha channel value, if applicable: alpha=0,100%backcolor, alpha=1, 100% frontcolor */

#if 0   /* Now it is applied in EGI_GIF */
    	bool            imgbuf_ready;       /* To indicate that imgbuf data is ready!
                                               * In some case imgbuf may be emptied before it is updated, so all alpha values may be
                                               * reset to 0 at that moment, if a thread read and display just at the point, it will
                                               * leave a blank on the screen and cause flickering.
                                               */
#endif

	/* DelayMs */
	unsigned int delayms;		/* Apply for serial imgbufs */

	/* For image data processing
	 * Note:For image processing, it will be better to define
	 *	multi_dimension arrays, which will improve index sorting speed???
	 * !! test !!
	 */
        EGI_16BIT_COLOR **pcolors; 	/* pcolors[height][width]  */
        unsigned char   **palphas;	/* palphas[height][width]  */

	void *data; 		 	/* color data, for pixel format other than RGB565 */
	//EGI_16BIT_PIXEL **pixels;     /* pixels[height][width] */

} EGI_IMGBUF;



/* EGI image motion pictures:  Header
 * Note:
 *  	1. TODO: current functions DO NOT consider machine arch and byte order!
 */
typedef struct {
	size_t		datasize;	/* Size of compressed frame image data, as follows. */
	char		data[];		/* Compressed frame data */
} EGI_IMGFRAME;
typedef struct {
	size_t		headsize;	/* To make it version compatible */

        int		height;	 	/* image height */
        int 		width;	 	/* image width */
	int		frames;		/* frames as in data[] */
	int 		delayms;	/* ms for each image */
	int		compress;	/* 0: Uncompressed,
					 * 1: Intra_Frame compression applied.
					 * 2: Inter_Frame compression applied.
					 **/

	char		blob[];		/* 1. If Uncompressed:  EGI_16BIT_COLOR []
					 * 2. If Compressed:    EGI_IMGFRAME []
					 */
//        EGI_16BIT_COLOR imgbuf[]; 	/* color data, for RGB565 format */
} EGI_IMGMOTION;


#ifdef __cplusplus
 }
#endif

#endif
