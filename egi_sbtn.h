/*------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------------*/
#ifndef __EGI_SBTN_H__
#define __EGI_SBTN_H__

#include <egi_color.h>
#include <egi_imgbuf.h>
#include <stdbool.h>

//extern struct egi_touch_data;

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

	struct egi_touch_data	*touch_data;			/* pointer to current touch data */
        void            (* reaction)(EGI_RECTBTN *);    /* button reaction callback */
};


void egi_sbtn_refresh(EGI_RECTBTN *btn, char *tag);


#endif
