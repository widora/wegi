/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#ifndef __EGI_IMAGE_H__
#define __EGI_IMAGE_H__

#include "egi_imgbuf.h"
#include "egi_fbdev.h"
#include "egi_color.h"
#include <freetype2/ft2build.h>
#include <freetype2/ftglyph.h>

enum imgframe_type {
	frame_simple=0,		/* <100 , outline frame defined by ebox type, check in egi_btn.c, egi_txt.c...  */
	frame_dbline=1,
	frame_inner=2,

	frame_none=100,		/* incoporate with ebox->frame and ebox->frame_img, <100  */
	frame_round_rect,
	frame_circle,
	frame_oval,
	frame_updown_mist,
};

/* TODO NOTE!!!
	1. some 'const' qualifiers may damage pthread_mutex !??!?
	2. Necessary to use pthread mutex for EGI_IMGBUF???
	3. Too many mutex operations will slow down system??!?
*/

EGI_IMGBUF*	egi_imgbuf_alloc(void);
EGI_IMGBOX*	egi_imgboxes_alloc(int n);
void 		egi_imgboxes_free(EGI_IMGBOX *imboxes);
int 		egi_imgbuf_setSubImgs(EGI_IMGBUF *eimg, int num);
void 		egi_imgbuf_cleardata(EGI_IMGBUF *egi_imgbuf); /* free data inside */
void 		egi_imgbuf_free(EGI_IMGBUF *egi_imgbuf);      /* mutex_lock */
void 		egi_imgbuf_free2(EGI_IMGBUF **pimg);	      /* mutex_lock */
int 		egi_imgbuf_init(EGI_IMGBUF *egi_imgbuf, int height, int width,  bool AlphaON);
int 		egi_imgbuf_addBoundaryBox(EGI_IMGBUF *ineimg, EGI_16BIT_COLOR color, int lw);
EGI_IMGBUF*	egi_imgbuf_create( int height, int width, unsigned char alpha, EGI_16BIT_COLOR color );
EGI_IMGBUF*	egi_imgbuf_createWithoutAlpha( int height, int width, EGI_16BIT_COLOR color);
EGI_IMGBUF*	egi_imgbuf_readfile(const char* fpath);
EGI_IMGBUF*	egi_imgbuf_blockCopy( const EGI_IMGBUF *ineimg, int px, int py, int height, int width );
EGI_IMGBUF* 	egi_imgbuf_rotBlockCopy( EGI_IMGBUF *eimg, EGI_IMGBUF *oimg, int height, int width,	/* mutex_lock eimg only! */
                                  	 int px, int py, int angle);
int  		egi_imgbuf_copyBlock( EGI_IMGBUF *destimg, const EGI_IMGBUF *srcimg, bool blendON,
							 	int bw, int bh, int xd, int yd, int xs, int ys );
EGI_IMGBUF*	egi_imgbuf_subImgCopy( const EGI_IMGBUF *eimg, int index );
int 		egi_imgbuf_setFrame( EGI_IMGBUF *eimg, enum imgframe_type type,
                         	     int alpha, int pn, const int *param );
unsigned char 	get_alpha_mapCurve( EGI_8BIT_ALPHA max_alpha, int range, int type, int x);

#define 	FADEOUT_EDGE_TOP	(1<<0)
#define 	FADEOUT_EDGE_RIGHT	(1<<1)
#define 	FADEOUT_EDGE_BOTTOM	(1<<2)
#define 	FADEOUT_EDGE_LEFT	(1<<3)
int 		egi_imgbuf_fadeOutEdges(EGI_IMGBUF *eimg, EGI_8BIT_ALPHA max_alpha, int width, unsigned int ssmode, int type);
int 		egi_imgbuf_fadeOutCircle(EGI_IMGBUF *eimg, EGI_8BIT_ALPHA max_alpha, int rad, int width, int type);

EGI_IMGBUF*	egi_imgbuf_newFrameImg( int height, int width,
                	             unsigned char alpha, EGI_16BIT_COLOR color,
                        	     enum imgframe_type type,
                                     int pn, const int *param );

 /* use 2D array ineimg->pcolors(palphas) */
EGI_IMGBUF  *egi_imgbuf_avgsoft( EGI_IMGBUF *ineimg, int size, bool alpha_on, bool hold_on);
/***
 * Function same as egi_imgbuf_avgsoft(), but without allocating  additional
 * 2D array for color/alpha data processsing.
 */
EGI_IMGBUF  *egi_imgbuf_avgsoft2(const EGI_IMGBUF *ineimg, int size, bool alpha_on); /* use 1D array data */

//EGI_IMGBUF  *egi_imgbuf_resize(const EGI_IMGBUF *ineimg, unsigned int width, unsigned int height);
EGI_IMGBUF  *egi_imgbuf_resize(EGI_IMGBUF *ineimg, bool keep_ratio, int width, int height);     /* mutex_lock */
EGI_IMGBUF  *egi_imgbuf_resize_nolock( EGI_IMGBUF *ineimg, bool keep_ratio, int width, int height );
EGI_IMGBUF  *egi_imgbuf_scale( EGI_IMGBUF *ineimg, int width, int height );

int 	egi_imgbuf_resize_update(EGI_IMGBUF **pimg, bool keep_ratio, unsigned int width, unsigned int height);
int 	egi_imgbuf_scale_update(EGI_IMGBUF **pimg, int width, int height);

int 	egi_imgbuf_blur_update(EGI_IMGBUF **pimg, int size, bool alpha_on);
int	egi_imgbuf_blend_imgbuf(EGI_IMGBUF *eimg, int xb, int yb, const EGI_IMGBUF *addimg );

EGI_IMGBUF* egi_imgbuf_rotate(EGI_IMGBUF *eimg, int angle);	/* mutex_lock */
int 	egi_imgbuf_rotate_update(EGI_IMGBUF **eimg, int angle);

int	egi_imgbuf_windisplay( EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev, int subcolor,	/* mutex_lock */
                	                        int xp, int yp, int xw, int yw, int winw, int winh);
/* no subcolor, no FB filo */
int egi_imgbuf_windisplay2( EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev,			/* mutex_lock */
                                        int xp, int yp, int xw, int yw, int winw, int winh);

int egi_image_rotdisplay( EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev, int angle,		/* mutex_lock for input egi_imgbuf */
                                                int xri, int yri, int xrl, int yrl);

/* display sub_image of an EGI_IMAGBUF */
int egi_subimg_writeFB(EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev, int subnum,		/* mutex_lock */
                                                        int subcolor, int x0,   int y0);

/* For whole imgbuf: rest color and alpha */
int egi_imgbuf_resetColorAlpha(EGI_IMGBUF *egi_imgbuf, int color, int alpha );		/* NOPE! mutex_lock */

/* For subimgbuf: reset color and alpha for all pixels */
int egi_imgbuf_reset(EGI_IMGBUF *egi_imgbuf, int subnum, int color, int alpha);		/* mutex_lock */
//RENAME: egi_subimg_reset()

/* blend an EGI_IMGBUF with a FT_Bitmap */
int egi_imgbuf_blend_FTbitmap(EGI_IMGBUF* eimg, int xb, int yb, FT_Bitmap *bitmap,
								EGI_16BIT_COLOR subcolor);

/* Set average luminance/birghtness Y for an image */
int egi_imgbuf_avgLuma( EGI_IMGBUF *eimg, unsigned char luma );

/* Flip image */
int egi_imgbuf_flipY( EGI_IMGBUF *eimg );		/* mutex_lock */
int egi_imgbuf_centroSymmetry( EGI_IMGBUF *eimg );	/* mutex_lock */
int egi_imgbuf_flipX( EGI_IMGBUF *eimg );		/* mutex_lock inside */

int  egi_imgbuf_showRBG888( const unsigned char *rgb, int width, int height,
                            FBDEV *fb_dev, int x0, int y0 );


#endif
