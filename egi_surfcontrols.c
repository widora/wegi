/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Journal:
2021-4-22:
	1. Add ESURF_LABEL and its functions.
	2. Modify 'egi_surfbtn_' to 'egi_surfBtn_'
	3. Add egi_surfBtn_writeFB().
	4. Modify 'egi_surfXXX_display' to egi_surfXXX_writeFB'

2021-4-25:
	1. Add ESURF_LISTBOX and its functions.
2021-4-29:
	1. ESURF_LISTBOX: add memeber 'var_SelectIdx'
2021-5-01:
	1. egi_surfListBox_create(): Add params 'fw' and 'fh'.
2021-5-02:
	1. Add: egi_surfListBox_addItem(), egi_surfListBox_redraw()
	        egi_surfListBox_adjustFirstIdx(), egi_surfListBox_PxySelectItem()
2021-5-05:
	1. ESURF_LISTBOX: add memeber 'GuideBlockDownHold'.
	2. Add: egi_surfListBox_adjustPastPos()

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include "egi_surfcontrols.h"
#include "egi_surface.h"
#include "egi_debug.h"
#include "egi_utils.h"

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
Display an ESURF_BOX onto the FBDEV.

@fbdev:		Pointer to FBDEV
@box:		Pointer to ESURF_BOX
@cx0,cy0:	Its container origin x0,y0
		relative to FBDEV.

-----------------------------------------*/
void egi_surfBox_writeFB(FBDEV *fbdev, const ESURF_BOX *box, int cx0, int cy0) {
	if( box==NULL || fbdev==NULL )
		return;

	egi_subimg_writeFB(box->imgbuf, fbdev, 0, -1, cx0+box->x0, cy0+box->y0); /* imgbuf, fb, subnum, subcolor, x0,y0 */
}


/*---------------------------------------------------------------
Create an ESURF_LABEL and blockcopy its imgbuf from input imgbuf.

@imgbuf		An EGI_IMGBUF holds image.
@xi,yi		Label bkg image origin relative to its container.
@x0,y0		Label origin relative to its container.
@w,h		Width and height of the label.

Return:
	A pointer to ESURF_LABEL	ok
	NULL				Fails
----------------------------------------------------------------*/
ESURF_LABEL *egi_surfLab_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h)
{
	ESURF_LABEL   *lab=NULL;

	if(imgbuf==NULL)
		return NULL;

	/* w/h check inside egi_image functions */
	//if(w<1 || h<1)
	//	return NULL;

	/* Calloc ESURF_LAB */
	lab=calloc(1, sizeof(ESURF_LABEL));
	if(lab==NULL)
		return NULL;

	/* To copy a block from imgbuf as box image */
	lab->imgbuf=egi_imgbuf_blockCopy(imgbuf, xi, yi, h, w);
	if(lab->imgbuf==NULL) {
		free(lab);
		return NULL;
	}

	/* Assign members */
	lab->x0=x0;
	lab->y0=y0;
	lab->w=w;
	lab->h=h;

	return lab;
}

/*---------------------------------------
	Free an ESURF_LABEL.
---------------------------------------*/
void egi_surfLab_free(ESURF_LABEL **lab)
{
	if(lab==NULL || *lab==NULL)
		return;

	/* Free imgbuf */
	egi_imgbuf_free((*lab)->imgbuf);

	/* Free struct */
	free(*lab);

	*lab=NULL;
}

/*-----------------------------------------
Display an ESURF_LABEL onto the FBDEV.

@fbdev:		Pointer to FBDEV
@lab:		Pointer to ESURF_BOX
@face:		Font face
@fw,fh:		Font size
@color:		Text color
@cx0,cy0:	Its container origin x0,y0
		relative to FBDEV.

-----------------------------------------*/
void egi_surfLab_writeFB(FBDEV *fbdev, const ESURF_LABEL *lab, FT_Face face, int fw, int fh, EGI_16BIT_COLOR color, int cx0, int cy0)
{
	if( lab==NULL || fbdev==NULL )
		return;

	/* Bkg image */
	egi_subimg_writeFB(lab->imgbuf, fbdev, 0, -1, cx0+lab->x0, cy0+lab->y0); /* imgbuf, fb, subnum, subcolor, x0,y0 */

	/* Write label text on bkg */
        FTsymbol_uft8strings_writeFB(  fbdev, face,             		/* FBdev, fontface */
                                        fw, fh, (UFT8_PCHAR)lab->text,   	/* fw,fh, pstr */
                                        lab->w, 1, 0,                           /* pixpl, lines, fgap */
                                        lab->x0, lab->y0,            		/* x0,y0, */
                                        color, -1, 255,              		/* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);                /* int *cnt, int *lnleft, int* penx, int* peny */

}

/*-------------------------------------
Update Label text.

@lab:	Pointer to ESUR_LABEL
@text:	Text for label.
-------------------------------------*/
void egi_surfLab_updateText( ESURF_LABEL *lab, const char *text )
{
	if(lab==NULL || text==NULL)
		return;

	strncpy( lab->text, text, ESURF_LABTEXT_MAX-1 );
	lab->text[ESURF_LABTEXT_MAX-1]='\0';
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
ESURF_BTN *egi_surfBtn_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h)
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
		egi_dpstd("Fail to blockCopy imgbuf!\n");
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
void egi_surfBtn_free(ESURF_BTN **sbtn)
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

/*-----------------------------------------
Display an ESURF_BTN onto the FBDEV.

@fbdev:		Pointer to FBDEV
@btn:		Pointer to ESURF_BTN
@type:		Type of imgbuf.
		SURFBTN_IMGBUF_NORMAL/EFFECT/PRESSED.
@cx0,cy0:	Its container origin x0,y0
		relative to FBDEV.

-----------------------------------------*/
void egi_surfBtn_writeFB(FBDEV *fbdev, const ESURF_BTN *btn, int type, int cx0, int cy0)
{
	if( btn==NULL || fbdev==NULL )
		return;

	switch(type) {
		case SURFBTN_IMGBUF_NORMAL:
			egi_subimg_writeFB(btn->imgbuf, fbdev, 0, -1, cx0+btn->x0, cy0+btn->y0); /* imgbuf, fb, subnum, subcolor, x0,y0 */
			break;
		case SURFBTN_IMGBUF_EFFECT:
			egi_subimg_writeFB(btn->imgbuf_effect, fbdev, 0, -1, cx0+btn->x0, cy0+btn->y0);
			break;
		case SURFBTN_IMGBUF_PRESSED:
			egi_subimg_writeFB(btn->imgbuf_pressed, fbdev, 0, -1, cx0+btn->x0, cy0+btn->y0);
			break;
	}
}


/*---------------------------------------
Check if point (x,y) in on the SURFBTN.

@sbtn	pointer to ESURF_BTN
@x,y	Point coordinate, relative to its contaier!

Return:
	True or False.
----------------------------------------*/
inline bool egi_point_on_surfBtn(const ESURF_BTN *sbtn, int x, int y)
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
@xi,yi		TICKBOX.imgbuf origin relative to its container(input imgbuf).
@x0,y0		TICKBOX origin relative to its container.
@w,h		Width and height of the tickbox.

Return:
	A pointer to ESURF_TICKBOX	ok
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
void egi_surfTickBox_writeFB(FBDEV *fbdev, const ESURF_TICKBOX *tbox,  int cx0, int cy0)
{
	if( tbox==NULL || fbdev==NULL )
		return;

	if(tbox->ticked)
							/* imgbuf, fb, subnum, subcolor, x0,y0 */
		egi_subimg_writeFB(tbox->imgbuf_ticked, fbdev, 0, -1, cx0+tbox->x0, cy0+tbox->y0);
	else
		egi_subimg_writeFB(tbox->imgbuf_unticked, fbdev, 0, -1, cx0+tbox->x0, cy0+tbox->y0);
}


/*---------------------------------------------------------------
Create an ESURF_LISTBOX and blockcopy its imgbuf from input imgbuf.

@imgbuf		An EGI_IMGBUF holds backgroud image for the ListBox.
@xi,yi		LISTBOX.imgbuf origin relative to its holding image(input imgbuf).
@x0,y0		LISTBOX origin relative to its container.
@w,h		Width and height of the whole ListBox area.(including
		vertical scroll bar).
@fw,fh		Font size.
@ListBarH	Height for each list bar, line_spacing.

Return:
	A pointer to ESURF_LISTBOX	Ok
	NULL				Fails
----------------------------------------------------------------*/
ESURF_LISTBOX   *egi_surfListBox_create(EGI_IMGBUF *imgbuf, int xi, int yi, int x0, int y0, int w, int h,
							    int fw, int fh, int ListBarH )
{
	ESURF_LISTBOX	*listbox=NULL;

	if(imgbuf==NULL)
		return NULL;

	/* w/h check inside egi_image functions */
	//if(w<1 || h<1)
	//	return NULL;

	/* Check fw/fh */
	if( fw<1 ) fw=5;
	if( fh<1 ) fh=5;

	/* Calloc LISTBOX */
	listbox=calloc(1, sizeof(ESURF_LISTBOX));
	if(listbox==NULL)
		return NULL;

	/* To copy a block from imgbuf as whole listbox bkgimg. */
	listbox->imgbuf=egi_imgbuf_blockCopy(imgbuf, xi, yi, h, w);
	if(listbox->imgbuf==NULL) {
		egi_dpstd("Fail to blockCopy imgbuf!\n");
		free(listbox);
		return NULL;
	}

	/* Assign members */
	listbox->x0=x0;
	listbox->y0=y0;

        listbox->FirstIdx=-1;    /* Start item index in ListBox */
        listbox->SelectIdx=-1;   /* Selected Item index, <0 Invalid.  */
	listbox->var_SelectIdx=-1;  /* As interim var. for  mouse event function */

	listbox->face = egi_appfonts.regular;
	listbox->fh=fh;
	listbox->fw=fw;

	listbox->ListBoxW = w - ESURF_LISTBOX_SCROLLBAR_WIDTH;
	listbox->ListBoxH = h;
	listbox->ListBarH = ListBarH;
	listbox->MaxViewItems = (listbox->ListBoxH +listbox->ListBarH/2) /listbox->ListBarH; /* Partially show */

	listbox->ScrollBarW = ESURF_LISTBOX_SCROLLBAR_WIDTH;
	listbox->ScrollBarH = h;

	listbox->pastPos = 0;
	listbox->GuideBlockH = h;  /* Init. as full range */
	listbox->GuideBlockW = ESURF_LISTBOX_SCROLLBAR_WIDTH;

	/* Create list space */
	listbox->ListCapacity=16;
	listbox->list = calloc(listbox->ListCapacity, ESURF_LISTBOX_ITEM_MAXLEN*sizeof(char));
	if(listbox->list==NULL) {
                egi_dpstd("Fail to calloc listbox->list!\n");
                egi_imgbuf_free(listbox->imgbuf);
                free(listbox);
                return NULL;
	}

	/* Create ListImgBox for List area */
        listbox->ListImgbuf=egi_imgbuf_blockCopy(listbox->imgbuf, 0, 0, listbox->ListBoxH, listbox->ListBoxW);
	if(listbox->ListImgbuf==NULL) {
		egi_dpstd("Fail to blockCopy ListImgbuf!\n");
		free(listbox->list);
		egi_imgbuf_free(listbox->imgbuf);
		free(listbox);
		return NULL;
	}

	/* Init. ListFB */
        if( init_virt_fbdev(&listbox->ListFB, listbox->ListImgbuf, NULL)!= 0 ) {
                egi_dpstd("Fail to initialize listFB!\n");
		egi_imgbuf_free(listbox->ListImgbuf);
		free(listbox->list);
                egi_imgbuf_free(listbox->imgbuf);
                free(listbox);
                return NULL;
        }


	return listbox;
}


/*---------------------------------------
	Free an ESURF_LISTBOX.
---------------------------------------*/
void egi_surfListBox_free(ESURF_LISTBOX **listbox)
{
	if(listbox==NULL || *listbox==NULL)
		return;

	/* Release ListFB */
	release_virt_fbdev(&(*listbox)->ListFB);

	/* Free imgbuf */
	egi_imgbuf_free((*listbox)->ListImgbuf);
	egi_imgbuf_free((*listbox)->imgbuf);

	/* Free list */
	free((*listbox)->list);

	/* Free struct */
	free(*listbox);

	*listbox=NULL;
}


/*--------------------------------------------------
Add/copy item content to listbox->list[], and update
GuideBlokH, pastPost,..etc accordingly.

@listbox	Pointer to ESURF_LISTBOX
@pstr		Pointer to item content, as string

Return:
	0	OK
	<0	Fails
--------------------------------------------------*/
int egi_surfListBox_addItem(ESURF_LISTBOX *listbox, const char *pstr)
{
	if( listbox == NULL || pstr == NULL )
		return -1;

	/* Check ListCapacity and growsize for listbox->list */
	if( listbox->TotalItems >= listbox->ListCapacity ) {

		if( egi_mem_grow((void **)&listbox->list, ESURF_LISTBOX_ITEM_MAXLEN*listbox->ListCapacity,	/* oldsize */
						     ESURF_LISTBOX_ITEM_MAXLEN*sizeof(char)*ESURF_LISTBOX_CAPACITY_GROWSIZE)  /* moresize */
		    <0 ) {
			egi_dpstd("Fail to memgrow for listbox->list!\n");
			return -2;
		}

		listbox->ListCapacity += ESURF_LISTBOX_CAPACITY_GROWSIZE;
	}

	/* Copy content into list */
	strncpy(listbox->list[listbox->TotalItems], pstr, ESURF_LISTBOX_ITEM_MAXLEN-1);

	/* Update TotalItems */
	listbox->TotalItems++;

	/* Update listbox->pastPos */
	listbox->pastPos = listbox->ScrollBarH*listbox->FirstIdx/listbox->TotalItems;

	/* Update GuideBlokH */
        listbox->GuideBlockH = listbox->ListBoxH * ( listbox->MaxViewItems < listbox->TotalItems ?
						    listbox->MaxViewItems : listbox->TotalItems  ) /listbox->TotalItems;
	/* Set Limit for GuideBlokH */
        if(listbox->GuideBlockH<1)
        	listbox->GuideBlockH=1;

	return 0;
}


/*--------------------------------------------------------------------
Redraw listbox and its view content to the surface( surfuser->imgbuf).
( ListBox content + ScrollBar + GuideBlock )

@psurfuser:	Pointer to EGI_SURFUSER.
@listbox:	Pointer to ESURF_LISTBOX.

---------------------------------------------------------------------*/
void egi_surfListBox_redraw(EGI_SURFUSER *psurfuser, ESURF_LISTBOX *listbox)
{
	int i;

	if( psurfuser==NULL || listbox==NULL )
		return;

	if( listbox->list==NULL ) //|| listbox->list[0]==NULL )
		return;

	/* Get ref. pointers to vfbdev/surfimg/surfshmem */
	//EGI_SURFSHMEM   *psurfshmem=NULL;      /* Only a ref. to surfuser->surfshmem */
	FBDEV           *pvfbdev=NULL;           /* Only a ref. to &surfuser->vfbdev  */
	EGI_IMGBUF	*psurfimg=NULL;          /* Only a ref. to surfuser->imgbuf */

	//psurfshmem=surfuser->surfshmem;
	pvfbdev=(FBDEV *)&psurfuser->vfbdev;
	psurfimg=psurfuser->imgbuf;

	/* 1. Refresh bkgIMG. ONLY for scrollbar area here. */
        //egi_surfBox_writeFB(mvfbdev, (ESURF_BOX *)listbox, 0, 0);
        egi_imgbuf_copyBlock( psurfimg, listbox->imgbuf, false, 			/* destimg, srcimg, blendON */
        			listbox->ScrollBarW, listbox->ScrollBarH, 	/* bw,bh */
                                listbox->x0+listbox->ListBoxW, listbox->y0, 	/* xd,yd */
                                listbox->ListBoxW, 0); 				/* xs,ys */



	/* 2. Draw scroll bar GUIDEBLOCK */
	draw_filled_rect2(pvfbdev, WEGI_COLOR_ORANGE, listbox->x0 +listbox->ListBoxW, listbox->y0 +listbox->pastPos,
			listbox->x0 +listbox->ListBoxW +listbox->GuideBlockW-1, listbox->y0 +listbox->pastPos +listbox->GuideBlockH-1 );

      	/* 3. Refresh bkimg for ListImgbuf */
        egi_imgbuf_copyBlock( listbox->ListImgbuf, listbox->imgbuf, false, 	/* destimg, srcimg, blendON */
                               listbox->ListBoxW, listbox->ListBoxH, 0,0,  0,0 ); /* bw,bh, xd,yd, xs,ys */

      	/* 4. Update/write ListBox items to ListImgbuf */
	if(listbox->MaxViewItems<1) {
	      	egi_subimg_writeFB(listbox->ListImgbuf, pvfbdev, 0, -1, listbox->x0, listbox->y0);
		return;
	}
        for(i=0; i< listbox->MaxViewItems && i+listbox->FirstIdx < listbox->TotalItems; i++) {
        	FTsymbol_uft8strings_writeFB( &listbox->ListFB, listbox->face,          /* FBdev, fontface */
               	                                  listbox->fw, listbox->fh,		/* fw,fh */
						  (UFT8_PCHAR)listbox->list[i+listbox->FirstIdx],   /* pstr */
                       	                          listbox->ListBoxW, 1, 0,              /* pixpl, lines, fgap */
                               	                  5, i*listbox->ListBarH +2,            /* x0,y0, */
                                       	          WEGI_COLOR_BLACK, -1, 255,            /* fontcolor, transcolor,opaque */
                                               	  NULL, NULL, NULL, NULL);              /* int *cnt, int *lnleft, int* penx, int* peny */
       	}

        /* 5. Paste ListImgbuf to surface */
      	egi_subimg_writeFB(listbox->ListImgbuf, pvfbdev, 0, -1, listbox->x0, listbox->y0);
}


/*--------------------------------------------------------------------
Adjust listbox->FirstIdx, and change listbox->pastPos accordingly.

NOTE: Use FirstIdx to driver pastPos (guiding block position).

@listbox:	Pointer to ESURF_LISTBOX.
@delt:		Adjusting value for listbox->FirstIdx. ( + or - )

Return:
	0	OK
	<0	Fails
---------------------------------------------------------------------*/
int egi_surfListBox_adjustFirstIdx(ESURF_LISTBOX *listbox, int delt)
{
	if(listbox==NULL)
		return -1;

	/* If empty content */
	if( listbox->TotalItems <1 ) { /* Empty listbox */
		listbox->FirstIdx=0;
		return 0;
	}

	/* Update FirstIdx  */
        listbox->FirstIdx += delt;
        if(listbox->FirstIdx < 0) {
        	listbox->FirstIdx=0;

		listbox->pastPos=0;
		return 0;
	}
	/* At lease one item in ListBox */
        else if( listbox->FirstIdx > listbox->TotalItems-1 ) {
        	listbox->FirstIdx = listbox->TotalItems-1;

		listbox->pastPos = listbox->ScrollBarH - listbox->GuideBlockH;
		return 0;
	}

        /* Update listbox->pastPos */
        listbox->pastPos = listbox->ScrollBarH* listbox->FirstIdx/listbox->TotalItems; /* TotalItems<0 ruled out */
       	if(listbox->pastPos<0)
        	listbox->pastPos=0;
        else if( listbox->pastPos > listbox->ScrollBarH - listbox->GuideBlockH)
                listbox->pastPos=listbox->ScrollBarH - listbox->GuideBlockH;

	return 0;
}


/*--------------------------------------------------------------------
Adjust listbox->pastPos, and change listbox->FirstIdx accordingly.
NOTE: Use pastPos to driver FirstIdx (listbox view content).

@listbox:	Pointer to ESURF_LISTBOX.
@delt:		Adjusting value for listbox->pastPos. ( + or - )

Return:
	0	OK
	<0	Fails
---------------------------------------------------------------------*/
int egi_surfListBox_adjustPastPos(ESURF_LISTBOX *listbox, int delt)
{
	if(listbox==NULL)
		return -1;

	/* If empty content */
	if( listbox->TotalItems <1 ) { /* Empty listbox */
		listbox->pastPos=0;
		listbox->FirstIdx=0;
		return 0;
	}

        /* Update listbox->pastPos */
	listbox->pastPos += delt;
       	if(listbox->pastPos<0) {
        	listbox->pastPos=0;
		listbox->FirstIdx=0;
		return 0;
	}
        else if( listbox->pastPos > listbox->ScrollBarH - listbox->GuideBlockH) {

                listbox->pastPos=listbox->ScrollBarH - listbox->GuideBlockH;
		/* Update FirstIdx as follows... */
	}

	/* Update FirstIdx  */
	listbox->FirstIdx = listbox->pastPos*listbox->TotalItems/listbox->ScrollBarH;

	return 0;
}



/*----------------------------------------------------------------------------
Check if point (px,py) is on ViewItemBar of ListBox, change listbox->SeletIdx
and Hightlight the selected item then.
Here selected item is of listbox->var_SelectIdx, NOT listbox->SelectIdx!

@psurfuer:	Pointer to EGI_SURFUSER, as container of the listbox.
@listbox:	Pointer to ESURF_LISTBOX.
@px, py:	Cooridnate relative to surfuser origin.

mouse_hover = mouse_enter + mouse_over +mouse_leave ??

Return:
	>=0	OK, as listbox->var_SelectIdx.
	<0	Fails
----------------------------------------------------------------------------*/
int egi_surfListBox_PxySelectItem(EGI_SURFUSER *surfuser, ESURF_LISTBOX *listbox, int px, int py)
{
	FBDEV *vfb=NULL;
	int index;  /* newly selected item index, as of listbox->list[] */

	if(surfuser == NULL || listbox == NULL )
		return -1;

	if( listbox->list==NULL ) // || listbox->list[0]==NULL )
		return -1;

	if( listbox->TotalItems <1 )
		return -1;

	/* Get ref. pointers to vfbdev/surfimg/surfshmem */
	vfb=&surfuser->vfbdev;

	/* Check if px,py located in ListBox area. (NOT in scroll bar area.) */
	if( pxy_inbox( px,py, listbox->x0, listbox->y0,
		       listbox->x0 + listbox->ListBoxW-1, listbox->y0 + listbox->ListBoxH-1) == false  )
		return -2;

	/* Cal. current moused pointed item index */
	index = listbox->FirstIdx + (py-SURF_TOPBAR_HEIGHT)/listbox->ListBarH;

	/* Limit index, in case BLANK items. */
	if( index > listbox->TotalItems-1 )
		return -3;

	/* NOW: listbox->var_SelectIdx is still old value */

	/* If selected item changes */
	if( listbox->var_SelectIdx != index ) {
	   	/* 1. Restor/De_highlight previous selected ListBar: redraw bkgimg and content */
	   	if( listbox->var_SelectIdx > -1 ) {
			/* Refresh item bkimg */
			egi_imgbuf_copyBlock( listbox->ListImgbuf, listbox->imgbuf, false, /*  destimg, srcimg, blendON */
					listbox->ListBoxW, listbox->ListBarH, 	      /* bw, bh */
					0, listbox->ListBarH*(listbox->var_SelectIdx-listbox->FirstIdx),    /* xd, yd */
					0, listbox->ListBarH*(listbox->var_SelectIdx-listbox->FirstIdx) );  /*xs,ys */

			/* Restore item content to ListImgbuf */
			FTsymbol_uft8strings_writeFB( &listbox->ListFB, listbox->face,   	/* FBdev, fontface */
      			             	  listbox->fw, listbox->fh, (UFT8_PCHAR)listbox->list[listbox->var_SelectIdx],	/* fw,fh, pstr */
               	       			          listbox->ListBoxW, 1, 0,         		/* pixpl, lines, fgap */
                       	       			  5, (listbox->var_SelectIdx-listbox->FirstIdx)*listbox->ListBarH +2,  /* x0,y0, */
	                               		  WEGI_COLOR_BLACK, -1, 255,     	/* fontcolor, transcolor,opaque */
		       	                          NULL, NULL, NULL, NULL);        	/* int *cnt, int *lnleft, int* penx, int* peny */
	   	}

		/* 2. Draw/highlight newly selected items */
		#if 0 /* Mask newly selected item */
		draw_blend_filled_rect( vfb, listbox->x0, listbox->y0+(index-listbox->FirstIdx)*listbox->ListBarH,
					 listbox->x0+listbox->ListBoxW -1, listbox->y0+(index-listbox->FirstIdx+1)*listbox->ListBarH-1,
       					 WEGI_COLOR_GRAY1, 180);
		#else /* Change newly selected ListBar bkgimg */
		draw_filled_rect2(&listbox->ListFB,  WEGI_COLOR_CYAN, 0, listbox->ListBarH*(index-listbox->FirstIdx),
						listbox->ListBoxW-1, listbox->ListBarH*(index-listbox->FirstIdx+1)-1 );

		FTsymbol_uft8strings_writeFB( &listbox->ListFB, egi_appfonts.regular,   /* FBdev, fontface */
      	       			               listbox->fw, listbox->fh, (UFT8_PCHAR)listbox->list[index],	/* fw,fh, pstr */
            	       			   	listbox->ListBoxW, 1, 0,         		/* pixpl, lines, fgap */
                       	       			5, (index-listbox->FirstIdx)*listbox->ListBarH +2,  	/* x0,y0, */
                                		WEGI_COLOR_BLACK, -1, 255,     	/* fontcolor, transcolor,opaque */
		       	                        NULL, NULL, NULL, NULL);        	/* int *cnt, int *lnleft, int* penx, int* peny */

		#endif

		/* 3. Paste modified ListImgbuf to surface */
		egi_subimg_writeFB(listbox->ListImgbuf, vfb, 0, -1, listbox->x0, listbox->y0);

		/* 4. Upate SelectIdx at last */
		listbox->var_SelectIdx = index;
	}

	return listbox->var_SelectIdx;
}
