/*----------------------- egi_pic.c ------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


egi pic type ebox functions

TODO: take data_pic->width,height or use imgbuf->width, height ?

Midas Zhou
----------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "egi_color.h"
#include "egi.h"
#include "egi_pic.h"
#include "egi_debug.h"
#include "egi_symbol.h"
#include "egi_bjp.h"


/*-------------------------------------
      pic ebox self_defined methods
--------------------------------------*/
static EGI_METHOD picbox_method=
{
        .activate=egi_picbox_activate,
        .refresh=egi_picbox_refresh,
        .decorate=NULL,
        .sleep=NULL,
        .free=NULL, /* to see egi_ebox_free() in egi.c */
};



/*-----------------------------------------------------------------------------
Dynamically create pic_data struct

Note:
     1. EBOX size is depended on data_pic.
     2. *pimgbuf will be reset to NULL, as ownership is transfered to data_pic->imgbuf.
     3. !!!! It will segmentfault if you try to dereference pimgbuf when it'is NULL. !!!

@offx, offy: 		offset from host ebox left top
@**pimgbuf:		image buffer for data_pic, if NULL, call egi_imgbuf_alloc()
			and egi_imgbuf_init() with default H*W=60*120
			*pimgbuf will be reset to NULL, as ownership transfered to
			data_pic->imgbuf.
@bkcolor		bkcolor for data_pic
@imgpx, imgpy:		origin of the image focusing/looking window, relating to
			image coord system.
font: 			symbol page for the title, or NULL to ignore title.

return:
        poiter          OK
        NULL            fail
-----------------------------------------------------------------------------*/
EGI_DATA_PIC *egi_picdata_new( int offx, int offy,
			       EGI_IMGBUF **pimgbuf,
			       int imgpx, int imgpy,
			       int bkcolor,
			       EGI_SYMPAGE *font
			     )
{
	EGI_IMGBUF *eimg=NULL;

	/* check data */
//	if(imgbuf==NULL) {
//		printf("%s: Input imgbuf is NULL, fail to proceed. \n",__func__);
//		return NULL;
//	}

//	if( imgbuf->height<=0 || imgbuf->width<=0 ) {
//		printf("%s: imgbuf->height or width is invalid, fail to proceed. \n",__func__);
//		return NULL;
//	}

    /* Create a default 60x120 imgbuf if NULL */
    if(pimgbuf==NULL || *pimgbuf==NULL)
    {
	/* malloc a EGI_IMGBUF struct */
	EGI_PDEBUG(DBG_PIC,"egi_picdata_new(): input imgbuf==NULL, egi_imgbuf_alloc()...\n");
	eimg =egi_imgbuf_alloc(); /* calloc */
	if(eimg == NULL)
        {
                printf("egi_picdata_new(): egi_imgbuf_alloc() fails.\n");
		return NULL;
	}

	/* set default height and width for imgbuf */
	EGI_PDEBUG(DBG_PIC,"egi_picdata_new(): egi_imgbuf_init()...\n");
	if ( egi_imgbuf_init(eimg, 60, 120)!=0 ) {	/* no mutex lock needed */
		printf("%s: egi_imgbuf_init() fails!\n",__func__);
		egi_imgbuf_free(eimg);
		return NULL;
	}
    }

        /* calloc a egi_data_pic struct */
        EGI_PDEBUG(DBG_PIC,"egi_picdata_new(): calloc data_pic ...\n");
        EGI_DATA_PIC *data_pic=calloc(1, sizeof(EGI_DATA_PIC));
        if(data_pic==NULL)
        {
                printf("egi_picdata_new(): fail to calloc egi_data_pic.\n");
		egi_imgbuf_free(eimg);
		return NULL;
	}

	/* assign data_pic->imgbuf */
   	if(pimgbuf==NULL || *pimgbuf == NULL) {
		data_pic->imgbuf=eimg;
   	}
   	else /* IF imgbuf != NULL */
   	{
		/* transfer Ownership of data in imgbuf to data_pic->imgbuf */
		if( pthread_mutex_lock(&(*pimgbuf)->img_mutex) !=0 ) {
       	        	printf("%s: Fail to lock image mutex!\n", __func__);
			return NULL;
		}
		data_pic->imgbuf=*pimgbuf;
		*pimgbuf=NULL;		/* !!! Ownership is transfered !!! */
		/* !!!NOTE: ownership transfered, imgbuf is NULL! */
		pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));
    	}

	/* assign other struct numbers */
	data_pic->offx=offx;
	data_pic->offy=offy;
	data_pic->imgpx=imgpx;
	data_pic->imgpy=imgpy;
	data_pic->bkcolor=bkcolor;
	data_pic->font=font;
	data_pic->title=NULL;

	return data_pic;
}


/*---------------------------------------------------------------------------------
Free and renew imgbuf in ebox->data_pic

Note:
     1. The Ownership of imgbuf will be transfered to the ebox, and eimg reset to NULL.
     2. !!! It will segmentfault if you try to dereference peimg when it'is NULL. !!!

@ebox	a PIC type ebox.
@**eimg	a EGI_IMGBUF struct with image data.

Reutrn:
	0	OK
	<0	Fails
----------------------------------------------------------------------------------*/
int egi_picbox_renewimg(EGI_EBOX *ebox, EGI_IMGBUF **peimg)
{
	/* check data */

        if( ebox == NULL ) {
                printf("%s: ebox is NULL!\n",__func__);
                return -1;
        }
	if( peimg==NULL ) {
		printf("%s: input peimg is NULL!\n",__func__);
		return -1;
	}
	if( *peimg==NULL || (*peimg)->imgbuf==NULL) {
                printf("%s: input imgbuf is NULL, or its image data is NULL!\n",__func__);
		return -1;
	}

	/* confirm ebox type */
        if(ebox->type != type_pic) {
                printf("%s: Not a PIC type ebox!\n",__func__);
                return -2;
        }
	EGI_DATA_PIC *data_pic=(EGI_DATA_PIC *)(ebox->egi_data);
        if( data_pic == NULL) {
                printf("%s: data_pic is NULL!\n",__func__);
                return -3;
        }

	/* free old imgbuf, !!! image mutex inside the func !!! */
//        EGI_PDEBUG(DBG_PIC,"start to egi_imgbuf_free() old imgbuf...\n");
	egi_imgbuf_free(data_pic->imgbuf);

        /* get input imgbuf's mutex lock */
//        EGI_PDEBUG(DBG_PIC,"start to mutex lock input eimg ...\n");
        if( *peimg==NULL || pthread_mutex_lock(&(*peimg)->img_mutex)!=0) { /* reasure eimg!=NULL */
       	        printf("%s: Fail to lock image mutex!\n", __func__);
            	return -4;
        }
	/* renew imgbuf, ownership transfered */
	data_pic->imgbuf=*peimg;
	*peimg=NULL;
	EGI_PDEBUG(DBG_PIC,"data_pic->imgbuf renewed with height=%d, width=%d.\n",
				data_pic->imgbuf->height, data_pic->imgbuf->width);

	/* !!!NOTE: ownership transfered, eimg is NULL! */
//        EGI_PDEBUG(DBG_PIC,"start to mutex unlock eimg ...\n");
	pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));

	return 0;
}


/*---------------------------------------------------------------------------------
Dynamically create a new pic type ebox

egi_data: pic data
movable:  whether the host ebox is movable,relating to bkimg mem alloc.
x0,y0:    host ebox origin position.
frame:	  frame type of the host ebox.
prmcolor:  prime color for the host ebox.

NOTE:
1. egi_data->imgbuf MUST not be NULL!
2. Ebox height/width is decided by egi_data image size and title font/symbol.
3. WARNING: The init image size shall be big enough for EBOX (see 2.) It will mess up
   if the image size is enlarged later.

return:
        poiter          OK
        NULL            fail
----------------------------------------------------------------------------------*/
EGI_EBOX * egi_picbox_new( char *tag, /* or NULL to ignore */
        EGI_DATA_PIC *data_pic,
        bool movable,
        int x0, int y0,
        int frame,
        int prmcolor /* applys only if prmcolor>=0 and egi_data->icon != NULL */
)
{
        EGI_EBOX *ebox;

        /* 0. check egi_data */
        if(data_pic==NULL || data_pic->imgbuf==NULL )
        {
                printf("egi_picbox_new(): data_pic or its imgbuf is NULL. \n");
                return NULL;
        }
        /* get imgbuf mutex lock */
        if(pthread_mutex_lock(&(data_pic->imgbuf->img_mutex)) !=0 ){
       	        printf("%s: Fail to lock image mutex!\n", __func__);
            	return NULL;
        }

        /* 1. create a new common ebox */
        EGI_PDEBUG(DBG_PIC,"egi_picbox_new(): start to egi_ebox_new(type_pic)...\n");
        ebox=egi_ebox_new(type_pic);// data_pic NOT allocated in egi_ebox_new()!!!
        if(ebox==NULL)
	{
	        pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));
                printf("egi_picbox_new(): fail to execute egi_ebox_new(type_pic). \n");
                return NULL;
	}

        /* 2. default method to be assigned in egi_ebox_new(),...see egi_ebox_new() */

        /* 3. pic ebox object method */
        EGI_PDEBUG(DBG_PIC,"egi_picbox_new(): assign self_defined methods: ebox->method=methd...\n");
        ebox->method=picbox_method;

        /* 4. fill in elements for concept ebox */
	egi_ebox_settag(ebox,tag);
        ebox->egi_data=data_pic; /* ----- assign egi data here !!!!! */
        ebox->movable=movable;
        ebox->x0=x0;
	ebox->y0=y0;

	/* 5. host ebox size is according to pic offx,offy and symheight */
	int symheight=0;
	if(data_pic->font != NULL)
	{
		symheight=data_pic->font->symheight;
	}

	ebox->width=data_pic->imgbuf->width+2*(data_pic->offx);

	if(data_pic->font != NULL && data_pic->title != NULL)
		ebox->height=data_pic->imgbuf->height+2*(data_pic->offy)+symheight;/* one line of title string */
	else
		ebox->height=data_pic->imgbuf->height+2*(data_pic->offy);

	/* 6. set frame and color */
        ebox->frame=frame;
	ebox->prmcolor=prmcolor;

        /* 7. set pointer default value*/
        //ebox->bkimg=NULL; Not neccessary, already set by memset() to 0 in egi_ebox_new().

	/* put image mutex lock */
        pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));

        return ebox;
}


/*-----------------------------------------------------------------------
activate a picture type ebox:
	1. malloc bkimg and store bkimg.
	2. set status, ebox as active.
	3. refresh the picbox

TODO:
	1. if ebox size changes(enlarged), how to deal with bkimg!!??

Return:
	0	OK
	<0	fails!
------------------------------------------------------------------------*/
int egi_picbox_activate(EGI_EBOX *ebox)
{
	/* check data */
        if( ebox == NULL)
        {
                printf("egi_picbox_activate(): ebox is NULL!\n");
                return -1;
        }
	EGI_DATA_PIC *data_pic=(EGI_DATA_PIC *)(ebox->egi_data);
        if( data_pic == NULL)
        {
                printf("egi_picbox_activate(): data_pic is NULL!\n");
                return -1;
        }

        /* get imgbuf mutex lock */
        if(pthread_mutex_lock( &(data_pic->imgbuf->img_mutex)) !=0 ) {
       	        printf("%s: Fail to lock image mutex!\n",__func__);
            	return -1;
        }

	/* 1. confirm ebox type */
        if(ebox->type != type_pic)
        {
                printf("egi_picbox_activate(): Not PIC type ebox!\n");
	        pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));
                return -1;
        }

        /* TODO:  activate(or wake up) a sleeping ebox ( referring to egi_txt.c and egi_btn.c )
         */

	/* host ebox size alread assigned according to pic offx,offy and symheight in egi_picbox_new() ...*/

	/* 2. verify pic data if necessary. --No need here*/
        if( ebox->height==0 || ebox->width==0)
        {
	        pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));
                printf("egi_picbox_activate(): height or width is 0 in ebox '%s'! fail to activate.\n",ebox->tag);
                return -1;
        }

   if(ebox->movable) /* only if ebox is movale */
   {
	/* 3. realloc bkimg for the host ebox, it may re_enter after sleep */
	if(egi_realloc_bkimg(ebox, ebox->height, ebox->width)==NULL)
	{
	        pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));
		printf("egi_picbox_activate(): fail to egi_realloc_bkimg()!\n");
		return -2;
	}

	/* 4. update bkimg box */
	ebox->bkbox.startxy.x=ebox->x0;
	ebox->bkbox.startxy.y=ebox->y0;
	ebox->bkbox.endxy.x=ebox->x0+ebox->width-1;
	ebox->bkbox.endxy.y=ebox->y0+ebox->height-1;

	#if 0 /* DEBUG */
	EGI_PDEBUG(DBG_PIC," pic ebox activating... fb_cpyto_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x, ebox->bkbox.endxy.y);
	#endif

	/* 5. store bk image which will be restored when this ebox's position/size changes */
	if( fb_cpyto_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
				ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) <0) {
	        pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));
		printf("%s: fail to fb_cpyto_buf()!\n",__func__);
		return -3;
	}
    } /* end of movable codes */

	/* 6. set status */
	ebox->status=status_active; /* if not, you can not refresh */

	/* 7. set need_refresh */
	ebox->need_refresh=true; /* if not, ignore refresh */

	/* 8. refresh pic ebox */
      pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex)); /* egi_picbox_refresh() will deal with mutex lock */

	if( egi_picbox_refresh(ebox) != 0)
		return -4;

	EGI_PDEBUG(DBG_PIC,"egi_picbox_activate(): a '%s' ebox is activated.\n",ebox->tag);
	return 0;
}


/*-----------------------------------------------------------------------
refresh a pic type ebox:
	1.refresh a pic ebox according to updated parameters:
		--- position x0,y0, offx,offy
		--- symbol page, symbol code
		--- fpath for a pic/movie ...etc.
		... ...
	2. restore bkimg and store bkimg.
	3. drawing a picture, or start a motion pic, or play a movie ...etc.
	4. take actions according to pic_status (released, pressed).???

TODO:
	1. if ebox size changes(enlarged), how to deal with bkimg!!??

Return:
	1	need_refresh=false
	0	OK
	<0	fails!
------------------------------------------------------------------------*/
int egi_picbox_refresh(EGI_EBOX *ebox)
{
//	int i;
//	int symheight;
//	int symwidth;

	/* check data */
        if( ebox == NULL)
        {
                printf("egi_picbox_refresh(): ebox is NULL!\n");
                return -1;
        }
	/* confirm ebox type */
        if(ebox->type != type_pic)
        {
                printf("egi_picbox_refresh(): Not pic type ebox!\n");
                return -2;
        }
	/*  check the ebox status  */
	if( ebox->status != status_active )
	{
		EGI_PDEBUG(DBG_PIC,"ebox '%s' is not active! fail to refresh. \n",ebox->tag);
		return -3;
	}

	/* only if need_refresh is true */
	if(!ebox->need_refresh)
	{
//		EGI_PDEBUG(DBG_PIC,"need_refresh=false, refresh action is ignored.\n");
		return 1;
	}

   if(ebox->movable) /* only if ebox is movale */
   {
	/* 2. restore bk image use old bkbox data, before refresh */
	#if 0 /* DEBUG */
	EGI_PDEBUG(DBG_PIC,"pic refresh... fb_cpyfrom_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x,ebox->bkbox.endxy.y);
	#endif
        if( fb_cpyfrom_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
                               ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) < 0)
		return -3;

   } /* end of movable codes */

	/* 3. get updated data */
	EGI_DATA_PIC *data_pic=(EGI_DATA_PIC *)(ebox->egi_data);
        if( data_pic == NULL)
        {
                printf("egi_picbox_refresh(): data_pic is NULL!\n");
                return -4;
        }

        /* get imgbuf mutex lock */
        EGI_PDEBUG(DBG_PIC,"start to mutex lock data_pic...\n");
//        if(pthread_mutex_lock( &(data_pic->imgbuf->img_mutex)) !=0 ) {
        if( (data_pic->imgbuf == NULL)   /* !!!!! reasure imgbuf */
	     ||  pthread_mutex_trylock( &(data_pic->imgbuf->img_mutex)) !=0 ) /* just trylock */
	{
       	        printf("%s: Fail to lock image mutex!\n",__func__);
            	return -5;
        }

	/* check ebox size */
        if( ebox->height<=0 || ebox->width<=0)
        {
	        pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));
                printf("%s: height(%d) or width(%d) is invalid in PIC ebox '%s'!\n"
				,__func__, ebox->height, ebox->width, ebox->tag);
                return -1;
        }
	int old_height=ebox->height;
	int old_width=ebox->width;

	/* get parameters */
	int symheight=0;
	if(data_pic->font != NULL)
	    symheight=data_pic->font->symheight;

	int wx0=ebox->x0+data_pic->offx; /* displaying_window origin */

	int wy0=ebox->y0+data_pic->offy;
	if(data_pic->title != NULL)
	    wy0=ebox->y0+data_pic->offy+symheight;

	int imgh=data_pic->imgbuf->height;
	int imgw=data_pic->imgbuf->width;

	/* resize ebox: ebox size modified according to pic offx,offy and symheight */
	ebox->width=imgw+2*(data_pic->offx);
	if(data_pic->font != NULL && data_pic->title != NULL)
		ebox->height=imgh+2*(data_pic->offy)+symheight;/* one line of title string */
	else
		ebox->height=imgh+2*(data_pic->offy);
	EGI_PDEBUG(DBG_PIC,"Resize ebox as per data_pic: ebox.height=%d, ebox.width=%d\n",
						ebox->height, ebox->width);

   if(ebox->movable) /* only if ebox is movale */
   {
       /* ---- 4. redefine bkimg box range, in case it changes... */
	/* check ebox height and font lines in case it changes, then adjust the height */
	/* updata bkimg->bkbox according */
        ebox->bkbox.startxy.x=ebox->x0;
        ebox->bkbox.startxy.y=ebox->y0;
        ebox->bkbox.endxy.x=ebox->x0+ebox->width-1;
        ebox->bkbox.endxy.y=ebox->y0+ebox->height-1;

	/* if size changed, reallocate ebox->bkimg */
	if( old_height != ebox->height || old_width != ebox->width )
	{
		EGI_PDEBUG(DBG_PIC,"ebox size changed, call egi_realloc_bkimg()...\n");
		if(egi_realloc_bkimg(ebox, ebox->height, ebox->width)==NULL) {
	       	 	pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));
			printf("%s: fail to egi_realloc_bkimg()!\n",__func__);
			return -2;
		}
	}

	#if 0 /* DEBUG */
	EGI_PDEBUG(DBG_PIC,"egi_picbox_refresh(): fb_cpyto_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x,ebox->bkbox.endxy.y);
	#endif
        /* ---- 5. store bk image which will be restored when you refresh it later,
		this ebox position/size changes */
        if(fb_cpyto_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
                                ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) < 0)
	{
	        pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));
		printf("egi_picbox_refresh(): fb_cpyto_buf() fails.\n");
		return -4;
	}
   } /* end of movable codes */

        /* 6. set prime color and drawing shape  */
        if(ebox->prmcolor >= 0 )
        {
                /* set color */
           	fbset_color(ebox->prmcolor);
		/* draw ebox  */
	        draw_filled_rect(&gv_fb_dev, ebox->x0, ebox->y0,
						ebox->x0+ebox->width-1, ebox->y0+ebox->height-1);
	}
       	/* draw frame */
    	if(ebox->frame >= 0) /* 0: simple type */
       	{
               	fbset_color(0); /* use black as frame color  */
               	draw_rect(&gv_fb_dev, ebox->x0, ebox->y0,
					ebox->x0+ebox->width-1, ebox->y0+ebox->height-1);
        }

	/* 7. put title  */
	if(data_pic->title != NULL && data_pic->font != NULL)
	{
		symbol_string_writeFB(&gv_fb_dev, data_pic->font, SYM_NOSUB_COLOR, SYM_FONT_DEFAULT_TRANSPCOLOR,
					 ebox->x0+data_pic->offx, ebox->y0 + data_pic->offy/2,
//			 data_pic->offy > symheight ? (ebox->y0+(data_pic->offy-symheight)/2) : ebox->y0, /*title position */
				     	 data_pic->title, -1); /* -1, no alpha */
	}

  	/* 8. draw picture in the displaying ebox window */
	/*-------------------------------------------------------------------------------------
	egi_imgbuf:     an EGI_IMGBUF struct which hold bits_color image data of a picture.
	(xp,yp):        coodinate of the displaying window origin(left top) point, relative to
        	        the coordinate system of the picture(also origin at left top).
	(xw,yw):        displaying window origin, relate to the LCD coord system.
	winw,winh:              width and height of the displaying window.

	int egi_imgbuf_windisplay(const EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev, int xp, int yp,
                                int xw, int yw, int winw, int winh)
	---------------------------------------------------------------------------------------*/
	/*  display imgbuf if not NULL */

	/* MUST unlock image mutex, egi_imgbuf_windisplay() also need mutex lock!!! */
        pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));
	if( data_pic->imgbuf != NULL && data_pic->imgbuf->imgbuf != NULL )
	{
#if 0
		egi_imgbuf_windisplay(data_pic->imgbuf, &gv_fb_dev, -1,    /* -1, no substituting color */
					data_pic->imgpx, data_pic->imgpy,
					wx0, wy0, imgw, imgh );
#else
		/* egi_imgbuf_windisplay2() to speed up, without FB FILO support and SUBCOLOR*/
		egi_imgbuf_windisplay2(data_pic->imgbuf, &gv_fb_dev,    /* -1, no substituting color */
					data_pic->imgpx, data_pic->imgpy,
					wx0, wy0, imgw, imgh );
#endif
//		printf("%s: finish egi_imgbuf_windisplay()....\n", __func__);
	}
	/* else if fpath != 0, load jpg file */
	else if(data_pic->fpath != NULL)
	{
		/* scale image to fit to the displaying window */

		/* keep original picture size */

	}
	else if( data_pic->bkcolor >=0 ) /* draw canvan if imgbuf is NULL */
	{
               	fbset_color(0); /* use black as frame color  */
               	draw_filled_rect(&gv_fb_dev,wx0,wy0,wx0+imgw-1,wy0+imgh-1);
	}

	/* 9. decorate functoins */
	if(ebox->decorate)
		ebox->decorate(ebox);

	/* 10. finally, reset need_refresh */
	ebox->need_refresh=false;

	/* put image mutex lock */
//Not necessary now        pthread_mutex_unlock(&(data_pic->imgbuf->img_mutex));

	return 0;
}


/*-----------------------------------------------------
put a txt ebox to sleep
1. restore bkimg
2. reset status

return
        0       OK
        <0      fail
------------------------------------------------------*/
int egi_picbox_sleep(EGI_EBOX *ebox)
{
	if(ebox==NULL)
	{
		printf("egi_picbox_sleep(): ebox is NULL, fail to make it sleep.\n");
		return -1;
	}

        if(ebox->movable) /* only for movable ebox */
        {
                /* restore bkimg */
                if(fb_cpyfrom_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
                               ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) <0 )
                {
                        printf("egi_picbox_sleep(): fail to restor bkimg for a '%s' ebox.\n",ebox->tag);
                        return -2;
                }
        }

        /* reset status */
        ebox->status=status_sleep;

        EGI_PDEBUG(DBG_PIC,"egi_picbox_sleep(): a '%s' ebox is put to sleep.\n",ebox->tag);
        return 0;
}


/*-------------------------------------------------
     release struct egi_data_pic
--------------------------------------------------*/
void egi_free_data_pic(EGI_DATA_PIC *data_pic)
{
	if(data_pic != NULL) {
		if(data_pic->imgbuf != NULL) {
			egi_imgbuf_free(data_pic->imgbuf);
		}
		free(data_pic);
		data_pic=NULL;
	}
}


