/*------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

			--- Simple Buttons ---

1. Simple buttons are traditional rectangular buttons, or image icon buttons.
  !!! --- LIMITS --- !!!
  Their positions in the screen are fixed, and transparent image icons not
  applied for transfiguration.
  Pressimg and releaseimg can be cut out from a pre_designed scenario picture,
  for they match up its surrounding pixels so well, it looks like that they
  are just transparent icons.

2. A simple button has only two status: released or pressed.
3. There may be some illustrating effects on the button to show that status
   is just be transformed.
4. A simple button may be transfigured by adopting different image icons.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------------*/
#ifndef __EGI_SBTN_H__
#define __EGI_SBTN_H__

#include <egi_color.h>
#include <egi_imgbuf.h>
#include <stdbool.h>

/* Simple rectangular button data */
typedef struct egi_rectbtn EGI_RECTBTN;
struct egi_rectbtn {
        int             id;                             /* button ID */
        EGI_16BIT_COLOR color;				/* Default BLACK */

        int             x0;                             /* left top coordinate */
        int             y0;

        unsigned int    width;                          /* button width and height */
        unsigned int    height;
        unsigned int    sw;                             /* width for bright/shadowy lines */

	EGI_16BIT_COLOR tagcolor;			/* Default BLACK */
	int		fw;				/* tag font size */
	int		fh;
	int		offx;				/* tag Offset postion */
	int		offy;

        bool            pressed;                        /* button status */
        int             type;				/*
							 * 0 - Normal, rectangular button shape.
							 */

        int             effect;                         /* Transforming effect when being touched.
                                                         * 0:   Defalt, normal.
                                                         * 1:   Ink effect.
                                                         */

	/* All imgbufs are references, no need to free. Usually they are subimgs of an EGI_IMGBUF */
	EGI_IMGBUF	*pressimg;			/* imgage icon for pressed status */
	unsigned	pressimg_subnum;
	EGI_IMGBUF	*releaseimg;			/* image icon for released status */
	unsigned	releaseimg_subnum;

        void            (* reaction)(EGI_RECTBTN *);    /* button reaction callback */
};


void egi_sbtn_refresh(EGI_RECTBTN *btn, char *tag);


#endif
