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
2021-5-13:
2021-5-14:
2021-5-15:
	1. Add ESURF_MENULIST and its functions.
2021-5-16:
	1. Add egi_surfMenuList_runMenuProg()
2021-5-18:
	1. egi_surfMenuList_updatePath(): Apply sub_MenuList.(x0,y0) to offset
	   its normal alignment position.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include "egi_surfcontrols.h"
#include "egi_surface.h"
#include "egi_utils.h"
#include "egi_debug.h"
#include "egi_log.h"
#include <unistd.h>

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
                                        cx0+lab->x0, cy0+lab->y0,     		/* x0,y0, */
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


/*-----------------------------------------------------------
Create a MenuList.

@mode:		Root MenuList position type.
	        ROOT_LEFT_BOTTOM	ROOT_RIGHT_BOTTO
		ROOT_LEFT_TOP		ROOT_RIGHT_TOP

@x0,y0:  	For root MenuList: Origin relative to its container
		Other sub_MenuList: Offset to normal aligment postion.
                Usually just take (0,0), unless you don't keep normal
		alignment of MenuList to MenuList.

@mw,mh:	 	Size for each menu item/piece.
@FT_Face:	Font face.
@fw,fh:		Font size.
@capacity:	MAX./capable number of menu itmes in the list.
@root:		TRUE: If a root MenuList.

Return:
	An pointer to ESURF_MENULIST	Ok
	NULL				Fails
------------------------------------------------------------*/
ESURF_MENULIST *egi_surfMenuList_create( int mode, bool root, int x0, int y0, int mw, int mh,
	                                         FT_Face face, int fw, int fh, unsigned int capacity)
{
	if( mw <1 || mh <1 )
		return NULL;
	if( capacity < 1 )
		return NULL;

	/* 1. Calloc mlist */
	ESURF_MENULIST *mlist = calloc(1, sizeof(ESURF_MENULIST));
	if(mlist==NULL)
		return NULL;

	/* 2. Calloc mitems */
	mlist->mitems = calloc(capacity, sizeof(EGI_MENUITEM));
	if(mlist->mitems==NULL) {
		free(mlist);
		return NULL;
	}

	/* 3. Calloc path for root MenuList */
	if(root) {
		mlist->path= calloc(ESURF_MENULIST_DEPTH_MAX, sizeof(ESURF_MENULIST*));
		if(mlist->path==NULL) {
			free(mlist->mitems);
			free(mlist);
			return NULL;
		}

		/* Assign path[0] to itself */
		mlist->path[0]=mlist;
	}

	/* 4. Assign memebers */
	mlist->mode=mode;
	mlist->root=root;

	mlist->node=-1;	/* Default NOT NodeItem */

	mlist->x0=x0;
	mlist->y0=y0;

	mlist->mw=mw;
	mlist->mh=mh;

	mlist->face=face;

	if(fw<1 || fh<1) {
		fw=12;
		fh=12;
	}
	mlist->fw=fw;
	mlist->fh=fh;

	mlist->capacity=capacity;

	/* Set default color, User can revise it. */
	mlist->bkgcolor=ESURF_MENULIST_BKGCOLOR;
	mlist->sidecolor=ESURF_MENULIST_SIDECOLOR;
	mlist->hltcolor=ESURF_MENULIST_HLTCOLOR;
	mlist->fontcolor=ESURF_MENULIST_FONTCOLOR;

	/* If only a root MenuList, without any sub_MenuList, then mlist->depth==0 */

	/* Final selected MenuItem index: For root sub_MenuList Only. */
	mlist->fidx = -1;

	return mlist;
}

/*-----------------------------------------------------------
Free an MenuList.

	!!! --- This is a recursive function --- !!!

mlist:	An pointer to ESURF_MENULIST
------------------------------------------------------------*/
void    egi_surfMenuList_free(ESURF_MENULIST **mlist)
{
	int i;

	if( mlist==NULL || *mlist==NULL )
		return;

	/* Free itemm menus */
	for(i=0; i < (*mlist)->mcnt; i++) {
		/* Free name */
		free( (*mlist)->mitems[i].descript);
		/* Free linked MenuList */
		egi_surfMenuList_free(&((*mlist)->mitems[i].mlist));
	}

	/* If ROOT */
	if( (*mlist)->path )
		free( (*mlist)->path );

	free(*mlist);
	*mlist=NULL;
}


/*---------------------------------------------------------------------
Draw a root MenuList with expanded/visible sub_MenuLists.

		!!! --- This is a recursive function --- !!!

@FBDEV:		Pointer to FBDEV.
@mlist:		A pointer to a ESURF_MENULIST.
@offx, offy:	Offset to mlist->(x0,y0).
		For a root MenuList usually: 0,0.
		For a sub_MenuList, offsetXY from its upper level MenuList origin.
@select_idx:    Selected Menu index, as of mlist->mitems[].
		If mlist is root MenuList, then select_idx will be checked/calcluated.
		<0, Ignore.
--------------------------------------------------------------------*/
void egi_surfMenuList_writeFB(FBDEV *fbdev, const ESURF_MENULIST *mlist, int offx, int offy, int select_idx)
{
	int i;

	if(fbdev==NULL || mlist==NULL)
		return;

	/* Check select_idx */
	if( mlist->root ) {
		if(mlist->depth >0)
			select_idx = mlist->path[1]->node;  /* Node number of next sub MenuList */
		else /* ==0 */
			select_idx = mlist->fidx;    	     /* As index of selected item */
	}

	switch(mlist->mode) {
	    case MENULIST_ROOTMODE_LEFT_BOTTOM:

		/* 1. Draw MenuList bkgcolor */
		draw_filled_rect2(fbdev, mlist->bkgcolor, mlist->x0 +offx, mlist->y0 +offy,
					mlist->x0 +offx +mlist->mw-1, mlist->y0 +offy -(mlist->mh*mlist->mcnt)-1 );
		/* Rim/Edge Lines */
		//fbset_color2(fbdev, mlist->sidecolor);
		fbset_color2(fbdev, COLOR_COMPLEMENT_16BITS(mlist->bkgcolor));
		//draw_rect(fbdev,  mlist->x0 +offx, mlist->y0 +offy,
		//			mlist->x0 +offx +mlist->mw-1, mlist->y0 +offy -(mlist->mh*mlist->mcnt)-1 );  /* width 1pix */
		draw_wrect(fbdev,  mlist->x0 +offx, mlist->y0 +offy,
					mlist->x0 +offx +mlist->mw-1, mlist->y0 +offy -(mlist->mh*mlist->mcnt)-1, 2); /* width 2pix */


		/* 2. Draw description and mark for each MenuItem */
		for(i=0; i < mlist->mcnt; i++) {

			/* Highlight color on selected MenuItem */
			/* Ignore select_idx <0 */
			if( select_idx  == i ) // xxx  -- fidx MAYBE not cleared as -1 !!!
				//draw_filled_rect2( fbdev, mlist->hltcolor, x1,y1, x2,y2 );
				draw_blend_filled_rect( fbdev,
						   mlist->x0 +offx +1, 		     mlist->y0 +offy -i*mlist->mh -1,		/* x1,y1 */
						   mlist->x0 +offx +mlist->mw -1 -1, mlist->y0 +offy -(i+1)*mlist->mh -1 +1,    /* x2,y2 */
						   mlist->hltcolor, 220 );

			/* Write Description */
		        FTsymbol_uft8strings_writeFB( fbdev, mlist->face, 	/* FBdev, fontface */
                                    mlist->fw, mlist->fh, (const UFT8_PCHAR)mlist->mitems[i].descript,  /* fw,fh, pstr */
                                    mlist->mw -15, 1, 0,	 	/* pixpl, lines, fgap */
                                    mlist->x0 +offx -ESURF_MENULIST_OVERLAP +15, mlist->y0 +offy -(i+1)*mlist->mh +5,       /* x0,y0, */
                                    mlist->fontcolor, -1, 220,       /* fontcolor, transcolor,opaque */
                                    NULL, NULL, NULL, NULL );        /* int *cnt, int *lnleft, int* penx, int* peny */

			/* Draw NodeItem mark '>' */
			if( mlist->mitems[i].mlist !=NULL) {
				const char* nodemark=">";
				int pixlen=FTsymbol_uft8strings_pixlen( mlist->face, mlist->fw, mlist->fh, (const UFT8_PCHAR)nodemark);
				if(pixlen>0)
		                       FTsymbol_uft8strings_writeFB( fbdev, mlist->face,       /* FBdev, fontface */
                	                      mlist->fw, mlist->fh, (const UFT8_PCHAR)nodemark,  /* fw,fh, pstr */
                        	              mlist->mw, 1, 0,                  /* pixpl, lines, fgap */
                                	      mlist->x0 +offx +mlist->mw -pixlen-ESURF_MENULIST_OVERLAP-5, /* x0, y0 */
					      mlist->y0 +offy -(i+1)*mlist->mh +5,
                                      	      mlist->fontcolor, -1, 240,       /* fontcolor, transcolor,opaque */
                                      	      NULL, NULL, NULL, NULL );        /* int *cnt, int *lnleft, int* penx, int* peny */
			}
		}

		/* 3. If root_MenuList:  Draw all sub_MenuLists in path[].  */
		if( mlist->root ) {
			i=1; /* i=0, root_MenuList */

			/* Sub_MenList offsetX/Y are all (0,0)s
			 * Convert to origin relative to root_MenuList container.
			 */
			offx = mlist->x0 +offx;
			offy = mlist->y0 +offy;

			/* TODO:  mlist->path[] MAY NOT cleared,  MUST check depth value! */
			while( mlist->path[i] && i< mlist->depth+1 ) {
				/* Cal. offset, relative to origin of the root MenuList */
				if(i==1) {  /* OK, mlist->path[0] pointed to root mlist. */
					offx += mlist->mw  -ESURF_MENULIST_OVERLAP; /* --- overlap effect */
					offy -= mlist->mh * (mlist->path[1])->node;
				} else {
					offx += (mlist->path[i-1])->mw -ESURF_MENULIST_OVERLAP;  /* --- overlap effect */
					offy -= (mlist->path[i-1])->mh * (mlist->path[i])->node;
				}

				/* Cal. select_index */
				/* Recursive calling */
				//egi_dpstd("Draw mlist->path[%d]...\n", i);
				egi_surfMenuList_writeFB(fbdev, mlist->path[i], offx, offy,
						      i==mlist->depth  ? mlist->path[i]->fidx : mlist->path[i+1]->node );  /* Selected menu index */
						      /* If ROOT or END menulist, take as fidx. */
				i++;
			}
		}

		break;

	    case MENULIST_ROOTMODE_RIGHT_BOTTOM :
		break;
	    case MENULIST_ROOTMODE_LEFT_TOP:
		break;
	    case MENULIST_ROOTMODE_RIGHT_TOP:
		break;
	}
}


/*---------------------------------------------------------------------
Add a new item to the mlist.
Note:
1. Ownership of submlist is transfered to mlist, and it will be released
   when free mlist.

@mlist:		An pointer to ESURF_MENULIST
@descript:	Description of the new menu item.
@submlist:	!NULL, A new menu item links to the sub_MenuList.
		Ownership transfered to mlist!
		It's node number also updated!
@run_script:	Valid ONLY when submlist==NULL!
		Set as MenuItem.run_script.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------*/
int  egi_surfMenuList_addItem(ESURF_MENULIST *mlist, const char *descript,  ESURF_MENULIST *submlist,  const char *run_script)
{
	if( mlist==NULL || descript==NULL )
		return -1;

	/* Check/Grow capacity */
	if( mlist->mcnt == mlist->capacity ) {
		if( egi_mem_grow( (void **)&mlist->mitems, mlist->capacity*sizeof(EGI_MENUITEM),
							   ESURF_MENUILIST_MITEMS_GROWSIZE*sizeof(EGI_MENUITEM)) != 0 ) {
			egi_dperr("Fail to memgrow mitems.");
			return -2;
		}

		mlist->capacity += ESURF_MENUILIST_MITEMS_GROWSIZE;
	}

	/* Description */
	mlist->mitems[mlist->mcnt].descript=strndup(descript, EGI_MENU_DESCRIPT_MAXLEN);
	if(mlist->mitems[mlist->mcnt].descript==NULL) {
		egi_dperr("Fail assign descript");
		return -3;
	}

	/* Add sub menulist*/
	if( submlist != NULL ) {
		mlist->mitems[mlist->mcnt].mlist=submlist;

		/* Update its node number */
		submlist->node=mlist->mcnt;

		/* Update depth */
		/* NOPE! depth to be updated in realtime.  */
	}
	else {
		mlist->mitems[mlist->mcnt].run_script=run_script;
	}

	/* Update Counter */
	mlist->mcnt +=1;

	return 0;
}


/*------------------------------------------------------------------
Update menu selection tree path and midx.
If the cursor is on a MenuItem that links to a sub_MenuList, then
update mlist->path[].
Else if the cursor in on a regular MenuItem, then update mlist->fidx.


@mlist:	A pointer to ESURF_MENULIST, It MUST be a root MenuList.
@px,py:	As cursor point. It's MUST under the same coord. system as
        of mlist->x0,y0 !

Return:
	0	OK
	<0	Fails
-------------------------------------------------------------------*/
int  egi_surfMenuList_updatePath(ESURF_MENULIST *mlist, int px, int py)
{
	int i;
	int x0,y0;	/* Origin point of a MenuList box */
	int x1, y1; 	/* Diagonal point to x0,y0 */
	int index;

	if(mlist==NULL || mlist->root==false)
		return -1;
	if(mlist->mcnt<1)  /* At lease on item */
		return -2;

	/* Init. x0,y0,  NOTE: mlist is the ROOT MenuList. */
	x0=mlist->x0;
	y0=mlist->y0;

	/* Reset find first */
	mlist->fidx = -1;

	/* Check if Pxy is on current MenuList tree map. */
	for(i=0; i < mlist-> depth +1; i++) {

		/* Cal. MenuList Box points */
		switch(mlist->mode) {
    		   case MENULIST_ROOTMODE_LEFT_BOTTOM:
			/* Cal. origin of MenuList: mlist->path[i] */
			if(i>0) {
				x0 += mlist->path[i-1]->mw -ESURF_MENULIST_OVERLAP  /* --- overlap effect */
				      +mlist->path[i]->x0;  /* Offset from normal alignment */
				y0 += -(mlist->path[i]->node)*(mlist->path[i-1]->mh)
				      +mlist->path[i]->y0;  /* Offset from normal alignment */
			}
			/* Cal. diagonal corner point of the MenuList box. */
			x1 = x0 +mlist->path[i]->mw -ESURF_MENULIST_OVERLAP; /* --- Deduce overlap */
			y1 = y0 -mlist->path[i]->mh*mlist->path[i]->mcnt;

			break;

    		   case MENULIST_ROOTMODE_RIGHT_BOTTOM :
			break;
    		   case MENULIST_ROOTMODE_LEFT_TOP:
			break;
    		   case MENULIST_ROOTMODE_RIGHT_TOP:
			break;
		}

		/* Check if Pxy located within the MenuList Box */
		if( pxy_inbox(px, py, x0, y0, x1, y1) ) {
			/* Cal. index of MenuItem as pxy loacted on */
			index = ( abs(y0-py) -2) / (mlist->path[i]->mh); /* -2: in case index == path[i]->mcnt ? NO WAY! */

			/* If current item links to a sub_MenuList, then depth +1 */
			if( mlist->path[i]->mitems[index].mlist !=NULL ) {
				/* Update depth as i+1 */
				mlist->depth = i+1;
				/* Expand tree path, and  */
				mlist->path[mlist->depth]=mlist->path[i]->mitems[index].mlist;
				/* Node number for new added sub_MenuList */
				mlist->path[mlist->depth]->node = index;
				/* Marked as no item selected ??? */
				mlist->path[mlist->depth]->fidx = -1;
			}
			else {
				mlist->depth = i;
				/* Update fidx for both root and end menuList!! */
				mlist->fidx = index;		/* Root MenuList */
				mlist->path[i]->fidx = index;   /* End MenuList */
			}
		}
		/* Clear selected item index */
		else {
			mlist->fidx = -1;
			mlist->path[i]->fidx = -1;
		}

	} /* End for() */

	return 0;
}

/*---------------------------------------------------------
If mlist has a MenuItem selected, then run the ESUF_PROG as
linked/associated in MenuItem.run_script.
If mlist->fidx<0, no selection, ignore.

mlist:	An pointer to ESURF_MENULIST

Return:
	0	OK
	<0	Fails
--------------------------------------------------------*/
int egi_surfMenuList_runMenuProg(const ESURF_MENULIST *mlist)
{
	int i;
	char *strp=NULL;
	char run_script[1024]={0};  /* prog_path + argv */
	pid_t apid;
	char *prog_path=NULL;
	char *pargv[1024]; /* Max. 1024-1 argvs,
			    * pargv[0] MUST be the prog_name!
			    * pargv[last] MUST be NULL for execv()
			    */

	if( mlist==NULL || mlist->fidx <0 ) /* No MenuItem selected */
		return -1;

	strp=(char *)mlist->path[mlist->depth]->mitems[mlist->fidx].run_script;
	if(strp==NULL) {
		egi_dpstd("run_script is NULL!\n");
		return -2;
	}

	/* Copy to run_script[] */
	strncpy(run_script, strp, sizeof(run_script)-1);
	run_script[ sizeof(run_script)-1 ] ='\0';

	/* Get prog_path, pargv */
	strp=strtok(run_script," ");
	if(strp) {
		/* Prog path */
		prog_path=strp;

		/* Get argv[0] */
		i=0;
		pargv[0]=strp;

		/* Get argv[1-...] */
		for( i=1; (strp=strtok(NULL," ")) && (i < sizeof(pargv)-1); i++) {
			pargv[i]=strp;
			printf("argv: %s\n", strp);
		}

		/* The last argv[] for execv() MUST be NULL */
		pargv[i] = NULL;
	}
	else {
		return -3;
	}


        egi_dpstd("Start vfork ...\n");
        apid=vfork();

        /* In PROG */
	if(apid==0) {
        	egi_dpstd("Start execv prog...\n");
                execv(prog_path, pargv);
                /* Warning!!! if fails! it still holds whole copied context data!!! */
                EGI_PLOG(LOGLV_ERROR, "%s: fail to execv '%s' after vfork(), error:%s",
                                                            __func__, run_script, strerror(errno) );
                exit(255); /* 8bits(255) will be passed to parent */
	}

        /* In the caller's context */
	else if(apid <0) {
        	EGI_PLOG(LOGLV_ERROR, "%s: Fail to launch APP '%s'!",__func__, run_script);
                        return -3;
        }
        else {
                //EGI_PLOG(LOGLV_CRITICAL, "%s: APP '%s' launched successfully!\n", __func__, run_script);
		egi_dpstd("Succeed to launch ESURF_PROG '%s'.\n", run_script);
        }


	return 0;
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
