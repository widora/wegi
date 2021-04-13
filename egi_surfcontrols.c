/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include "egi_surfcontrols.h"


/*---------------------------------------------------------------
Create an ESURF_BOX and blockcopy its imgbuf from input imgbuf.

@imgbuf		An EGI_IMGBUF holds image icons.
@xi,yi		Box image origin relative to its continer.
@x0,y0		Box origin relative to its container.
@w,h		Width and height of the button image.

Return:
	A pointer to ESURF_BOX	ok
	NULL				Fails
----------------------------------------------------------------*/
ESURF_BOX *egi_surfBox_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h)
{
	ESURF_BOX   *box=NULL;

	if(imgbuf==NULL)
		return NULL;

	/* w/h check inside egi_image functions */
	//if(w<1 || h<1)
	//	return NULL;

	/* Calloc SURFBOX */
	box=calloc(1, sizeof(ESURF_BOX));
	if(box==NULL)
		return NULL;

	/* To copy a block from imgbuf as box image */
	box->imgbuf=egi_imgbuf_blockCopy(imgbuf, xi, yi, h, w);
	if(box->imgbuf==NULL) {
		free(box);
		return NULL;
	}

	/* Assign members */
	box->x0=x0;
	box->y0=y0;

	return box;
}

/*---------------------------------------
	Free an ESURF_BOX.
---------------------------------------*/
void egi_surfBox_free(ESURF_BOX **box)
{
	if(box==NULL || *box==NULL)
		return;

	/* Free imgbuf */
	egi_imgbuf_free((*box)->imgbuf);

	/* Free struct */
	free(*box);

	*box=NULL;
}

/*-------------------------------------------------
Check if point (x,y) in on the SURFBOX.

@box	pointer to ESURF_BOX
@x,y	Point coordinate, relative to its contaier!

Return:
	True or False.
-------------------------------------------------*/
inline bool egi_point_on_surfBox(const ESURF_BOX *box, int x, int y)
{
	if( box==NULL || box->imgbuf==NULL )
		return false;

	if( x < box->x0 || x > box->x0 + box->imgbuf->width -1 )
		return false;
	if( y < box->y0 || y > box->y0 + box->imgbuf->height -1 )
		return false;

	return true;
}

/*-----------------------------------------
Display an ESURF_BOX on the FBDEV.

@fbdev:		Pointer to FBDEV
@box:		Pointer to ESURF_BOX
@cx0,cy0:	Its container origin x0,y0
		relative to FBDEV.

-----------------------------------------*/
void egi_surfBox_display(FBDEV *fbdev, const ESURF_BOX *box, int cx0, int cy0) {
	if( box==NULL || fbdev==NULL )
		return;

	egi_subimg_writeFB(box->imgbuf, fbdev, 0, -1, cx0+box->x0, cy0+box->y0); /* imgbuf, fb, subnum, subcolor, x0,y0 */
}


/*---------------------------------------------------------------
Create an ESURF_BTN and blockcopy its imgbuf from input imgbuf.

@imgbuf		An EGI_IMGBUF holds image icons.
@xi,yi		Button image origin relative to its container.
@x0,y0		Button origin relative to its container.
@w,h		Width and height of the button image.

Return:
	A pointer to ESURF_BTN	ok
	NULL				Fails
----------------------------------------------------------------*/
ESURF_BTN *egi_surfbtn_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h)
{
	ESURF_BTN	*sbtn=NULL;

	if(imgbuf==NULL)
		return NULL;

	/* w/h check inside egi_image functions */
	//if(w<1 || h<1)
	//	return NULL;

	/* Calloc SURFBTN */
	sbtn=calloc(1, sizeof(ESURF_BTN));
	if(sbtn==NULL)
		return NULL;

	/* To copy a block from imgbuf as button image */
	sbtn->imgbuf=egi_imgbuf_blockCopy(imgbuf, xi, yi, h, w);
	if(sbtn->imgbuf==NULL) {
		free(sbtn);
		return NULL;
	}

	/* Assign members */
	sbtn->x0=x0;
	sbtn->y0=y0;

	return sbtn;
}


/*---------------------------------------
	Free an ESURF_BTN.
---------------------------------------*/
void egi_surfbtn_free(ESURF_BTN **sbtn)
{
	if(sbtn==NULL || *sbtn==NULL)
		return;

	/* Free imgbuf */
	egi_imgbuf_free((*sbtn)->imgbuf);
	egi_imgbuf_free((*sbtn)->imgbuf_effect);
	egi_imgbuf_free((*sbtn)->imgbuf_pressed);

	/* Free struct */
	free(*sbtn);

	*sbtn=NULL;
}

/*---------------------------------------
Check if point (x,y) in on the SURFBTN.

@sbtn	pointer to ESURF_BTN
@x,y	Point coordinate, relative to its contaier!

Return:
	True or False.
----------------------------------------*/
inline bool egi_point_on_surfbtn(const ESURF_BTN *sbtn, int x, int y)
{
	if( sbtn==NULL || sbtn->imgbuf==NULL )
		return false;

	if( x < sbtn->x0 || x > sbtn->x0 + sbtn->imgbuf->width -1 )
		return false;
	if( y < sbtn->y0 || y > sbtn->y0 + sbtn->imgbuf->height -1 )
		return false;

	return true;
}


/*---------------------------------------------------------------
Create an ESURF_TICKBOX and blockcopy its imgbuf from input imgbuf.
Only  tbox->imgbuf_unticked is filled. imgbuf_ticked to be filled
during  XXX_firstdraw_XXX().

TODO: NOW only square type.

@imgbuf		An EGI_IMGBUF holds image icons.
@xi,yi		TICKBOX image origin relative to its container.
@x0,y0		TICKBOX origin relative to its container.
@w,h		Width and height of the tickbox.

Return:
	A pointer to ESURF_BTN	ok
	NULL				Fails
----------------------------------------------------------------*/
ESURF_TICKBOX *egi_surfTickBox_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h)
{
	ESURF_TICKBOX *tbox=NULL;

	if(imgbuf==NULL)
		return NULL;

	/* w/h check inside egi_image functions */
	//if(w<1 || h<1)
	//	return NULL;

	/* Calloc SURFBTN */
	tbox=calloc(1, sizeof(ESURF_TICKBOX));
	if(tbox==NULL)
		return NULL;

	/* To copy a block from imgbuf as image for unticked box */
	tbox->imgbuf_unticked=egi_imgbuf_blockCopy(imgbuf, xi, yi, h, w);
	if(tbox->imgbuf_unticked==NULL) {
		free(tbox);
		return NULL;
	}

	/* tbox->imgbuf_unticked to be filled in XXX_firstdraw_XXX() */

	/* Assign members */
	tbox->x0=x0;
	tbox->y0=y0;
	tbox->ticked=false;

	return tbox;
}

/*---------------------------------------
	Free an ESURF_TICKBOX.
---------------------------------------*/
void egi_surfTickBox_free(ESURF_TICKBOX **tbox)
{
	if(tbox==NULL || *tbox==NULL)
		return;

	/* Free imgbuf */
	egi_imgbuf_free((*tbox)->imgbuf_unticked);
	egi_imgbuf_free((*tbox)->imgbuf_ticked);

	/* Free struct */
	free(*tbox);

	*tbox=NULL;
}

/*-------------------------------------------
Check whether point (x,y) in on the TICKBOX.

@tbox	pointer to ESURF_TICKBOX
@x,y	Point coordinate, relative to it contianer.

Return:
	True or False.
--------------------------------------------*/
inline bool egi_point_on_surfTickBox(const ESURF_TICKBOX *tbox, int x, int y)
{
	if( tbox==NULL || tbox->imgbuf_unticked==NULL )
		return false;

	if( x < tbox->x0 || x > tbox->x0 + tbox->imgbuf_unticked->width -1 )
		return false;
	if( y < tbox->y0 || y > tbox->y0 + tbox->imgbuf_unticked->height -1 )
		return false;

	return true;
}

/*-----------------------------------------
Display an ESURF_TICKBOX on the FBDEV.

@fbdev:		Pointer to FBDEV
@tbox:		Pointer to ESURF_TICKBOX
@cx0,cy0:	Its container origin x0,y0
		relative to FBDEV.

-----------------------------------------*/
void egi_surfTickBox_display(FBDEV *fbdev, const ESURF_TICKBOX *tbox,  int cx0, int cy0)
{
	if( tbox==NULL || fbdev==NULL )
		return;

	if(tbox->ticked)
							/* imgbuf, fb, subnum, subcolor, x0,y0 */
		egi_subimg_writeFB(tbox->imgbuf_ticked, fbdev, 0, -1, cx0+tbox->x0, cy0+tbox->y0);
	else
		egi_subimg_writeFB(tbox->imgbuf_unticked, fbdev, 0, -1, cx0+tbox->x0, cy0+tbox->y0);
}



