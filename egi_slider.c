/*----------------------   egi_slider.c  -------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

egi slider type ebox functions

1. A slider_ebox is derived from a btn_ebox.
2. The slider is a btn_ebox, and its slot is drawn by decoration function.
3. The slot is alway fixed, and btn_ebox is movable.

Midas Zhou
------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "egi_color.h"
#include "egi.h"
#include "egi_btn.h"
#include "egi_slider.h"
#include "egi_debug.h"
#include "egi_symbol.h"

/*-------------------------------------
      slider ebox self_defined methods
--------------------------------------*/
static EGI_METHOD slider_method=
{
        .activate=egi_slider_activate,
        .refresh=egi_slider_refresh,
        .decorate=NULL,  /* draw slot for the slider */
        .sleep=NULL,
        .free=NULL, /* to see egi_ebox_free() in egi.c */
};

/*-----------------------------------------------------------------------------
Dynamically create a btn_data struct, containing a slider data.

id: 		id number of the button
shape: 		shape of the button
icon: 		symbol page for the icon
icon_code:	code of the symbol
font:		symbol page for the ebox->tag font

return:
        poiter          OK
        NULL            fail
-----------------------------------------------------------------------------*/
EGI_DATA_BTN *egi_sliderdata_new(
			      	/* for btnbox */
			      	int id, enum egi_btn_type btn_shape,
			      	EGI_SYMPAGE *icon, int icon_code,
			      	EGI_SYMPAGE *font,

			     	/* for slider */
				enum egi_slid_type   slid_type,	/* slider type */
			        EGI_POINT pxy,			/* slider starting point */
				int swidth, int slen,
				int value,	/* usually to be 0 */
				EGI_16BIT_COLOR val_color, EGI_16BIT_COLOR void_color,
				EGI_16BIT_COLOR slider_color
			       )
{
	/* 0. check input data */
	if(swidth==0||slen==0) {
                printf("%s: Input swidth or slen is 0!\n",__func__);
		return NULL;
	}

	/*  1. calloc a egi_data_slider struct */
        EGI_PDEBUG(DBG_SLIDER,"Calloc data_slider ...\n");
        EGI_DATA_SLIDER *data_slider=calloc(1,sizeof(EGI_DATA_SLIDER));
        if(data_slider==NULL)
        {
                printf("%s: Fail to calloc egi_data_slider.\n",__func__);
		return NULL;
	}
	/* assign data slider struct number */
	data_slider->ptype=slid_type;
	data_slider->sxy=pxy;
	data_slider->sw=swidth;
	data_slider->sl=slen;
	data_slider->val=value;
	data_slider->color_valued=val_color;
	data_slider->color_void=void_color;
	data_slider->color_slider=slider_color;

        /* 2. calloc a egi_data_btn struct */
        EGI_PDEBUG(DBG_SLIDER,"Calloc data_btn ...\n");
        EGI_DATA_BTN *data_btn=calloc(1,sizeof(EGI_DATA_BTN));
        if(data_btn==NULL)
        {
                printf("%s: Fail to malloc egi_data_btn.\n",__func__);
		free(data_slider);
		return NULL;
	}
	/* assign data btn struct number */
	data_btn->id=id;
	data_btn->shape=btn_shape;
	data_btn->icon=icon;
	data_btn->icon_code=icon_code;
	data_btn->font=font;
	//data_btn->font_color=0; /* black as default */
	data_btn->prvdata=data_slider; /* slider data as private data */

	return data_btn;
}


/*-------------------------------------------------------
Dynamically create a new slider ebox

return:
        poiter          OK
        NULL            fail
-------------------------------------------------------*/
EGI_EBOX * egi_slider_new(
	char *tag, 		 /* or NULL to ignore */
        EGI_DATA_BTN *egi_data,
        int width, int height,   /* for frame drawing and prmcolor filled_rectangle */
	int twidth, int theight, /* for touch area,which is coupled with ebox->x0,y0 */
        int frame,
        int prmcolor 		 /*  1. Let <0, it will draw default slider, instead of applying gemo or icon.
				  *  2. prmcolor geom applys only if prmcolor>=0 and egi_data->icon != NULL
				  */
)
{
        EGI_EBOX *ebox;

        /* 0. check egi_data */
        if(egi_data==NULL || egi_data->prvdata==NULL )
        {
                printf("%s: egi_data or its prvdata is NULL. \n",__func__);
                return NULL;
        }

        /* 1. create a new common ebox */
        EGI_PDEBUG(DBG_SLIDER,"Start to egi_ebox_new(type_slider)...\n");
        ebox=egi_ebox_new(type_slider);// egi_data NOT allocated in egi_ebox_new()!!!
        if(ebox==NULL)
	{
                printf("%s: Fail to execute egi_ebox_new(type_slider). \n",__func__);
                return NULL;
	}

        /* 2. default method assigned in egi_ebox_new() */

        /* 3. slider ebox object method */
        EGI_PDEBUG(DBG_SLIDER,"Assign defined mehtod ebox->method=methd...\n");
        ebox->method=slider_method;

        /* 4. fill in elements for concept ebox */
	EGI_DATA_SLIDER *data_slider=(EGI_DATA_SLIDER *)egi_data->prvdata;

	egi_ebox_settag(ebox,tag);
        ebox->egi_data=egi_data; /* ----- assign egi data here !!!!! */
        ebox->movable=true; /* MUST true for a slider */
        ebox->width=((width>>1)<<1)+1;	 /* ODD */
	ebox->height=((height>>1)<<1)+1; /* ODD */
	if(data_slider->ptype==slidType_horiz) /* Horizontal Type */
	{
		/* ebox->x0,y0 */
	        ebox->x0=data_slider->sxy.x+data_slider->val-(width>>1);
		ebox->y0=data_slider->sxy.y-(height>>1); /* beware of LCD Y dir */

		/* couple touch area with slider position */
		ebox->touchbox.startxy.x=data_slider->sxy.x+data_slider->val-(twidth>>1);
		ebox->touchbox.startxy.y=data_slider->sxy.y-(theight>>1);
		ebox->touchbox.endxy.x=data_slider->sxy.x+data_slider->val+(twidth>>1);
		ebox->touchbox.endxy.y=data_slider->sxy.y+(theight>>1);
	}
	else if( data_slider->ptype==slidType_vert )/* Vertical Type, start point is at lower end  */
	{
		/* ebox->x0,y0 */
		ebox->x0=data_slider->sxy.x-(width>>1);
		ebox->y0=data_slider->sxy.y-data_slider->val-(height>>1); /* slider x0y0 at bigger Y end of LCD */

		/* couple touch area with slider position */
		ebox->touchbox.startxy.x=data_slider->sxy.x-(twidth>>1);
		ebox->touchbox.startxy.y=data_slider->sxy.y-data_slider->val-(theight>>1);
		ebox->touchbox.endxy.x=data_slider->sxy.x+(twidth>>1);
		ebox->touchbox.endxy.y=data_slider->sxy.y-data_slider->val+(theight>>1);
	}
	else {  /* --- undefined slider type --- */
	        EGI_PDEBUG(DBG_SLIDER,"Undefined slider type, fail to create new slider!\n");
		//egi_ebox_free(ebox);
		//return NULL;
		// Go on...
	}

        ebox->frame=frame;
	ebox->prmcolor=prmcolor;

        /* 5. pointer default */
        ebox->bkimg=NULL;

        return ebox;
}


/*-----------------------------------------------------------------------
Activate a slider type ebox:
	1. get icon symbol information
	2. malloc bkimg and store bkimg for bar slider(btnbox).
	3. refresh the slider box
	4. set status, ebox as active, and botton assume to be released.

TODO:
	1. if ebox size changes(enlarged), how to deal with bkimg!!??

Return:
	0	OK
	<0	fails!
------------------------------------------------------------------------*/
int egi_slider_activate(EGI_EBOX *ebox)
{
	int icon_code;

	/* check data */
        if( ebox == NULL)
        {
                printf("%s: ebox is NULL!\n",__func__);
                return -1;
        }
	EGI_DATA_BTN *data_btn=(EGI_DATA_BTN *)(ebox->egi_data);
        if( data_btn == NULL || data_btn->prvdata==NULL )
        {
                printf("%s: data_btn  or its prvdata for slider is NULL!\n",__func__);
                return -1;
        }
	EGI_DATA_SLIDER *data_slider=(EGI_DATA_SLIDER *)(data_btn->prvdata);

	/* 1. confirm ebox type */
        if(ebox->type != type_slider)
        {
                printf("%s: Not slider type ebox!\n", __func__);
                return -1;
        }

        /* TODO:  activate(or wake up) a sleeping ebox ( referring to egi_txt.c and egi_btn.c )
         */

	/* only if it has an icon, get symheight and symwidth */
	if(data_btn->icon != NULL)
	{
		/* Extract icon code from  SYM_SUB_COLOR+CODE */
		//SAME AS: icon_code=( (data_btn->icon_code)<<16 )>>16;
		icon_code=data_btn->icon_code & 0xffff;

		//int bkcolor=data_btn->icon->bkcolor;
		int symheight=data_btn->icon->symheight;
		int symwidth=data_btn->icon->symwidth[icon_code];

		/* use bigger size for ebox height and width !!! */
		if(symheight > ebox->height ) {
			/* re_adjust ebox->y0, whatever type  */
			ebox->y0 -= (symheight - ebox->height)>>1;
			/* re_assign height */
			ebox->height=symheight;
	 	}
		if( symwidth > ebox->width ) {
			/* re_adjust ebox->x0, whatever type  */
			ebox->x0 -= (symwidth - ebox->width)>>1;
			/* re_assign widht */
			ebox->width=symwidth;
		}
	}

	/* else if no icon, use prime shape and size */

	/* origin(left top), for btn H&W x0,y0 is same as ebox
	 * NOTE: already initialized according to sxy and ls in egi_slider_new()
	 */
	int x0=ebox->x0;
	int y0=ebox->y0;

	/* 2. verify btn data if necessary. --No need here*/
        if( ebox->height==0 || ebox->width==0 )
        {
                printf("%s: Ebox '%s' has 0 height or width!\n",__func__, ebox->tag);
                return -1;
        }

   if(ebox->movable) /* only if ebox is movale */
   {
	/* A.1 Calloc data_slider->slot_bkimg */
	int slot_width=(data_slider->sw>>1)*2+5; /* !!!!! to cover simple circulare slider */
	int slot_length=data_slider->sl+slot_width; /* For two round ends */

	data_slider->slot_bkimg=calloc(1, slot_width*slot_length*sizeof(uint16_t));
        if(data_slider==NULL) {
 		printf("%s: Fail to calloc data_slider->slot_bkimg for Ebox '%s'!\n", __func__, ebox->tag);
		return -2;
    	}

	/* A.2 update slot_bkimg box */
        if(data_slider->ptype==slidType_horiz) {
		data_slider->bkbox.startxy.x=data_slider->sxy.x-slot_width/2;
		data_slider->bkbox.startxy.y=data_slider->sxy.y-slot_width/2;
		data_slider->bkbox.endxy.x=data_slider->sxy.x+data_slider->sl+slot_width/2;
		data_slider->bkbox.endxy.y=data_slider->sxy.y+slot_width/2;
	}
	else {
		data_slider->bkbox.startxy.x=data_slider->sxy.x+slot_width/2;
		data_slider->bkbox.startxy.y=data_slider->sxy.y+slot_width/2;
		data_slider->bkbox.endxy.x=data_slider->sxy.x-slot_width/2;
		data_slider->bkbox.endxy.y=data_slider->sxy.y-data_slider->sl-slot_width/2;
	}

	/* 3. malloc bkimg width re_assigned/updated width and height  */
	if(egi_alloc_bkimg(ebox, ebox->height, ebox->width)==NULL)
	{
		printf("%s: Fail to egi_alloc_bkimg() for Ebox '%s'!\n", __func__, ebox->tag);
		free(data_slider->slot_bkimg); data_slider->slot_bkimg=NULL;
		return -2;
	}


	/* 4. update bkimg box */
	ebox->bkbox.startxy.x=x0;
	ebox->bkbox.startxy.y=y0;
	ebox->bkbox.endxy.x=x0+ebox->width-1;
	ebox->bkbox.endxy.y=y0+ebox->height-1;

	#if 0 /* DEBUG */
	EGI_PDEBUG(DBG_SLIDER,"button activating... fb_cpyto_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x, ebox->bkbox.endxy.y);
	#endif


	/* 5. store bk image which will be restored when this ebox position/size changes */
	/* 5.1 slot_bkimg */
	if( fb_cpyto_buf(&gv_fb_dev, data_slider->bkbox.startxy.x, data_slider->bkbox.startxy.y,
		    data_slider->bkbox.endxy.x, data_slider->bkbox.endxy.y, data_slider->slot_bkimg) <0) {
		return -3;
	}
	/* 5.2 ebox bkimg */
	if( fb_cpyto_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
				ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) <0)  {
		return -3;
	}


    } /* end of movable codes */

	/* 6. set button status */
	ebox->status=status_active;     /* if not, you can not refresh */
	data_btn->status=released_hold; /* set initial touch status */

	/* 7. set need_refresh */
	ebox->need_refresh=true;

	/* 8. refresh btn ebox */
	if( egi_slider_refresh(ebox) != 0)
		return -4;

	EGI_PDEBUG(DBG_SLIDER,"A '%s' ebox is activated.\n",ebox->tag);
	return 0;
}


/*-----------------------------------------------------------------------
refresh a slider type ebox:
	1.refresh button ebox according to updated parameters:
		--- position x0,y0, offx,offy
		--- symbol page, symbol code ...etc.
		... ...
	2. restore bkimg and store bkimg.
	3. drawing the icon
	4. take actions according to btn_status (released, pressed).

	5. A H_slider can move only vertically as whole, while a V_slider
	   can only move horizontally as whole.

TODO:
	1. if ebox size changes(enlarged), how to deal with bkimg!!??


Return:
	1	need_refresh=false
	0	OK
	<0	fails!
------------------------------------------------------------------------*/
int egi_slider_refresh(EGI_EBOX *ebox)
{
	int i;
	int bkcolor=SYM_FONT_DEFAULT_TRANSPCOLOR;
	int symheight;
	int symwidth;
	int icon_code=0; 	 /* 0, first ICON */;
	uint16_t sym_subcolor=0; /* symbol substitute color, 0 as no subcolor */
	int x0,y0;		 /* slider left top */
	int twidth,theight;      /* widht/height of ebox->touchbox */

	/* check data */
        if( ebox == NULL || ebox->egi_data == NULL
			 || ((EGI_DATA_BTN *)(ebox->egi_data))->prvdata == NULL)
        {
                printf("%s: Ebox or its egi_data, or prvdata is NULL!\n", __func__);
                return -1;
        }

	/* confirm ebox type */
        if(ebox->type != type_slider)
        {
                printf("%s: The ebox is NOT a slider!\n",__func__);
                return -1;
        }

	/* check ebox size */
        if( ebox->height==0 || ebox->width==0)
        {
                printf("%s: Ebox '%s' has 0 height or width!\n", __func__, ebox->tag);
                return -1;
        }

	/*  check the ebox status  */
	if( ebox->status != status_active )
	{
		EGI_PDEBUG(DBG_SLIDER,"ebox '%s' is not active! refresh action is ignored! \n",ebox->tag);
		return -2;
	}

	/* only if need_refresh is true */
	if(!ebox->need_refresh)
	{
		//EGI_PDEBUG(DBG_SLIDER,"need_refresh=false, abort refresh.\n");
		return 1;
	}

	/* 2.1 get updated data */
	EGI_DATA_BTN *data_btn=(EGI_DATA_BTN *)(ebox->egi_data);
        if( data_btn == NULL || data_btn->prvdata == NULL ) {
                printf("%s: data_btn or its prvdata is NULL!\n",__func__);
                return -1;
        }
	EGI_DATA_SLIDER *data_slider=(EGI_DATA_SLIDER *)(data_btn->prvdata);

	/* 2.2 get touchbox height and width */
	twidth=ebox->touchbox.endxy.x - ebox->touchbox.startxy.x;
	theight=ebox->touchbox.endxy.y - ebox->touchbox.startxy.y;

   if(ebox->movable) /* only if ebox is movale */
   {
	/* 3. restore bk image use old bkbox data, before refresh */
	#if 0 /* DEBUG */
	EGI_PDEBUG(DBG_SLIDER,"button refresh... fb_cpyfrom_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x,ebox->bkbox.endxy.y);
	#endif

        /* Check wheather PAGE bkimg changed,
         * If changed, do NOT copy old ebox->bkimg to FB, as PAGE bkimg is already the new one!
         */
        if( ebox->container==NULL || ebox->container->page_update !=true ) {
		/* slot_bkimg */
		if( fb_cpyfrom_buf(&gv_fb_dev, data_slider->bkbox.startxy.x, data_slider->bkbox.startxy.y,
		     data_slider->bkbox.endxy.x, data_slider->bkbox.endxy.y, data_slider->slot_bkimg) <0) {
		          printf("%s: Fail to call fb_cpyfrom_buf() for data_slider->slot_bkimg of '%s'.\n",__func__, ebox->tag);
			  return -3;
		}
		/* ebox bkimg */
	        if( fb_cpyfrom_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
        	                       ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) < 0) {
			 printf("%s: Fail to call fb_cpyfrom_buf() for ebox->bkimg of '%s'.\n",__func__, ebox->tag);
			 return -3;
		}
	}

   } /* end of movable codes */

	/* only if it has an icon */
	if(data_btn->icon != NULL)
	{
		icon_code=( (data_btn->icon_code)<<16 )>>16; /* as SYM_SUB_COLOR + CODE */
		sym_subcolor = (data_btn->icon_code)>>16;

		bkcolor=data_btn->icon->bkcolor;
		symheight=data_btn->icon->symheight;
		symwidth=data_btn->icon->symwidth[icon_code];

		/***
		 *  TO update ebox height and withd if symheight/symwidth is bigger!
		 *  PENDING: bkimg to be reallocated!
		 */

	}
	/* else if no icon, use prime shape */

	/* Update btn origin(left top),  for btn H&W x0,y0 is same as ebox */
	x0=ebox->x0;
	y0=ebox->y0;
        if(data_slider->ptype==slidType_horiz) {  /* Horizontal type:  Y0 is to drive data_slider->sxy.y */
		data_slider->sxy.y=y0+(ebox->height>>1);
	}
	else {					  /* Vertical type: X0 is to drive data_slider->sxy.x */
		data_slider->sxy.x=x0+(ebox->width>>1);
	}


   if(ebox->movable) /* only if ebox is movale, For SLIDER it's sure!!! */
   {
       /* ---- 4. Update bkimg box range, in case it changes */

	int slot_width=(data_slider->sw>>1)*2+5; /* !!!!! to cover simple circulare slider */
	int slot_length=data_slider->sl+slot_width; /* For two round ends */

	/* 4.1  update slot_bkimg data_slider->bkbox */
        if(data_slider->ptype==slidType_horiz) {
		data_slider->bkbox.startxy.x=data_slider->sxy.x-slot_width/2;
		data_slider->bkbox.startxy.y=data_slider->sxy.y-slot_width/2;
		data_slider->bkbox.endxy.x=data_slider->sxy.x+data_slider->sl+slot_width/2;
		data_slider->bkbox.endxy.y=data_slider->sxy.y+slot_width/2;
	}
	else {
		data_slider->bkbox.startxy.x=data_slider->sxy.x+slot_width/2;
		data_slider->bkbox.startxy.y=data_slider->sxy.y+slot_width/2;
		data_slider->bkbox.endxy.x=data_slider->sxy.x-slot_width/2;
		data_slider->bkbox.endxy.y=data_slider->sxy.y-data_slider->sl-slot_width/2;
	}

	/* check ebox height and font lines in case it changes, then adjust the height */
	/* 4. 2 updata bkimg->bkbox according */
        ebox->bkbox.startxy.x=x0;
        ebox->bkbox.startxy.y=y0;
        ebox->bkbox.endxy.x=x0+ebox->width-1;
        ebox->bkbox.endxy.y=y0+ebox->height-1;


	#if 1 /* DEBUG */
	EGI_PDEBUG(DBG_SLIDER,"fb_cpyto_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x,ebox->bkbox.endxy.y);
	#endif
        /* ---- 5. store bk image which will be restored when you refresh it later,
		this ebox position/size changes */
	/* 5.1 slot_bkimg */
	if( fb_cpyto_buf(&gv_fb_dev, data_slider->bkbox.startxy.x, data_slider->bkbox.startxy.y,
	     data_slider->bkbox.endxy.x, data_slider->bkbox.endxy.y, data_slider->slot_bkimg) <0) {
	          printf("%s: fb_cpyto_buf() for data_slider->slot_bkimg of '%s' fails.\n",__func__, ebox->tag);
		  return -3;
	}
	/* 5.2 ebox bkimg */
        if(fb_cpyto_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
                                ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) < 0)
	{
		printf("%s: fb_cpyto_buf() for Ebox '%s' fails.\n",__func__, ebox->tag);
		return -4;
	}


   } /* end of movable codes */


	/*  ---- update slider value according to ebox->x0y0 position and redraw sliding slot */
	if(data_slider->ptype==slidType_horiz)  	/* Horizontal sliding bar */
	{
		/* get slider value */
		data_slider->val=x0+(ebox->width>>1)-data_slider->sxy.x;

		/* check limit */
		if(data_slider->val > data_slider->sl)
				data_slider->val=data_slider->sl;
		else if(data_slider->val<0)
				data_slider->val=0;

		/* couple touch area with slider position */
		ebox->touchbox.startxy.x=data_slider->sxy.x+data_slider->val-(twidth>>1);
		ebox->touchbox.startxy.y=data_slider->sxy.y-(theight>>1);
		ebox->touchbox.endxy.x=data_slider->sxy.x+data_slider->val+(twidth>>1);
		ebox->touchbox.endxy.y=data_slider->sxy.y+(theight>>1);

		/* draw void sliding slot */
		fbset_color(data_slider->color_void);
		draw_wline(&gv_fb_dev, data_slider->sxy.x+data_slider->val, data_slider->sxy.y, /* slider point*/
			      data_slider->sxy.x+data_slider->sl, data_slider->sxy.y, /* slot end */
			   data_slider->sw);

		/* draw valued sliding slot */
		fbset_color(data_slider->color_valued);
		draw_wline(&gv_fb_dev,data_slider->sxy.x, data_slider->sxy.y, /* slot start point */
			      data_slider->sxy.x+data_slider->val, data_slider->sxy.y, /* slider point*/
			   data_slider->sw);
	}
	else if(data_slider->ptype==slidType_vert)	/* Vertical sliding bar */
	{
		/* Noticed that sval is upside down for LCD display */
		/* get slider value, start point of V type bar is at lower end, with bigger Y */
		data_slider->val= data_slider->sxy.y - (y0+(ebox->height>>1));
		if(data_slider->val > data_slider->sl)
				data_slider->val=data_slider->sl;
		else if(data_slider->val<0)
				data_slider->val=0;

		/* couple touch area with slider position */
		ebox->touchbox.startxy.x=data_slider->sxy.x-(twidth>>1);
		ebox->touchbox.startxy.y=data_slider->sxy.y-data_slider->val-(theight>>1);
		ebox->touchbox.endxy.x=data_slider->sxy.x+(twidth>>1);
		ebox->touchbox.endxy.y=data_slider->sxy.y-data_slider->val+(theight>>1);

		/* draw void sliding slot */
		fbset_color(data_slider->color_void);
		draw_wline(&gv_fb_dev, data_slider->sxy.x, data_slider->sxy.y-data_slider->val, /* slider point */
			      data_slider->sxy.x, data_slider->sxy.y-data_slider->sl, /* slot end */
			   data_slider->sw);

		/* draw valued sliding slot */
		fbset_color(data_slider->color_valued);
		draw_wline(&gv_fb_dev, data_slider->sxy.x, data_slider->sxy.y, /* slot start point */
			      data_slider->sxy.x, data_slider->sxy.y-data_slider->val, /* slider point*/
			   data_slider->sw);
	}
	else {
		EGI_PDEBUG(DBG_SLIDER,"Warning: Undefined slider type!\n");
		// Go on...
	}

	/* Draw the slider!!! only if no prmcolor and icon is NOT availbale */
        if(ebox->prmcolor <0 && data_btn->icon == NULL )
	{
		/* draw a circle with R=sw */
		fbset_color(data_slider->color_slider);
		if(data_slider->ptype==slidType_horiz) {	/* Horizontal bar */
			draw_filled_circle(&gv_fb_dev,
				 	   data_slider->sxy.x+data_slider->val, data_slider->sxy.y,
					   data_slider->sw + 2
			);
			/* Draw a red inner circle */
			fbset_color(WEGI_COLOR_RED);
			draw_filled_circle(&gv_fb_dev,
				 	   data_slider->sxy.x+data_slider->val, data_slider->sxy.y,
					   3
			);
		}

		else  {   					/* Vertical bar */
			draw_filled_circle(&gv_fb_dev,
				 	   data_slider->sxy.x, data_slider->sxy.y-data_slider->val,
					   data_slider->sw + 2
			);
		}
	}

        /* ---- 6. set color and drawing shape ----- */
        if(ebox->prmcolor >= 0 && data_btn->icon == NULL )
        {
		int height=ebox->height;
		int width=ebox->width;

                /* set color */
           	fbset_color(ebox->prmcolor);
		switch(data_btn->shape)
		{
			case btnType_square:
			        draw_filled_rect(&gv_fb_dev,x0,y0,x0+width-1,y0+height-1);
        			/* --- draw frame --- */
       			        if(ebox->frame >= 0) /* 0: simple type */
       				{
                			fbset_color(0); /* use black as frame color  */
                			draw_rect(&gv_fb_dev,x0,y0,x0+width-1,y0+height-1);
			        }
				break;
			case btnType_circle:
				draw_filled_circle(&gv_fb_dev, x0+width/2, y0+height/2,
								width>height?height/2:width/2);
        			/* --- draw frame --- */
       			        if(ebox->frame >= 0) /* 0: simple type */
       				{
                			fbset_color(0); /* use black as frame color  */
					draw_circle(&gv_fb_dev, x0+width/2, y0+height/2,
								width>height?height/2:width/2);
			        }
       			        if(ebox->frame > 0) /* >0: double circle !!! UGLY !!! */
       				{
                			fbset_color(0); /* use black as frame color  */
					draw_circle( &gv_fb_dev, x0+width/2, y0+height/2,
								width>height ? (height/2-1):(width/2-1) );
			        }

				break;

			default:
				printf("%s: Slider shape not defined! \n",__func__);
				break;
		}
        }

	/* 7. draw the button symbol if it has an icon */
	if(data_btn->icon != NULL)
	{
		/* if icon_code = SUB_COLOR + CODE */
		if( sym_subcolor > 0 ) /* sym_subcolor may be 0xF.., so it should be unsigned ..*/
		{
			/* extract SUB_COLOR and CODE for symbol */
			//printf("egi_btnbox_refresh(): icon_code=SYM_SUB_COLOR + CODE. \n");
			symbol_writeFB( &gv_fb_dev,data_btn->icon, sym_subcolor, bkcolor,
							    x0, y0, icon_code, data_btn->opaque );
		}
		/* else icon_code = CODE only, sym_subcolor==0 BLACk is NOT applicable !!! */
		else {
			symbol_writeFB( &gv_fb_dev,data_btn->icon, SYM_NOSUB_COLOR, bkcolor,
							     x0, y0, icon_code, data_btn->opaque );
		}
	}

	/* 8. decorate functoins  */
	if(ebox->decorate)
		ebox->decorate(ebox);


  /* 9. draw ebox->tag on the button if necessary */
   /* 9.1 check whether sympg is set */
   if(data_btn->font == NULL)
		printf("egi_btnbox_refresh(): data_btn->font is NULL, fail to put tag on button.\n");

   else if(data_btn->showtag==true)
   {
	/* 9.2 get length of tag */
	int taglen=0;
	while( ebox->tag[taglen] )
	{
		taglen++;
	}
	//printf("egi_btnbox_refresh(): length of ebox->tag:'%s' is %d (chars).\n", ebox->tag, taglen);

	/* only if tag has content */
	if(taglen>0)
	{
		int strwidth=0; /* total number of pixels of all char in ebox->tag */
		// int symheight=data_btn->icon->symheight; already above assigned.
		int shx=0, shy=0; /* shift x,y to fit the tag just in middle of ebox shape */
		/* WARNIGN: Do not mess up with sympg icon!  sympy font is right here! */
		int *wdata=data_btn->font->symwidth; /*symwidth data */
		int  fontheight=data_btn->font->symheight;

		for(i=0;i<taglen;i++)
		{
			/* only if in code range */
			if( (ebox->tag[i]) <= (data_btn->font->maxnum) )
				strwidth += wdata[ (unsigned int)ebox->tag[i] ];
		}
		shx = (ebox->width - strwidth)/2;
		shx = shx>0 ? shx:0;
		shy = (ebox->height - fontheight)/2;
		shy = shy>0 ? shy:0;
/*
use following COLOR:
#define SYM_NOSUB_COLOR -1  --- no substitute color defined for a symbol or font
#define SYM_NOTRANSP_COLOR -1 --- no transparent color defined for a symbol or font
48void symbol_string_writeFB(FBDEV *fb_dev, const EGI_SYMPAGE *sym_page,   \
                int fontcolor, int transpcolor, int x0, int y0, const char* str)
*/
		symbol_string_writeFB(&gv_fb_dev, data_btn->font, ebox->tag_color, 1,
 							ebox->x0+shx, ebox->y0+shy, ebox->tag, -1);

	} /* endif: taglen>0 */

   }/* endif: data_btn->font != NULL */

	/* 10. finally, reset need_refresh */
	ebox->need_refresh=false;

	return 0;
}


/*------------------------------------------------
Set slider percentage value in range [0 100]
reset slider ebox x0y0 as per data_slider->sxy and
psval.

@slider:	The concerning slider.
@psval:		Percentage value [0 100]

Return:
	0	OK
	<0	Fails
-------------------------------------------------*/
int egi_slider_setpsval(EGI_EBOX *slider, int psval)
{
	int sl;
        EGI_DATA_BTN *data_ebox=NULL;
        EGI_DATA_SLIDER *data_slider=NULL;

	if( slider==NULL || slider->egi_data==NULL )
		return -1;

        if( slider->type != type_slider)
		return -2;

	/* Get data */
        data_ebox=(EGI_DATA_BTN *)(slider->egi_data);
        data_slider=(EGI_DATA_SLIDER *)(data_ebox->prvdata);
	if(data_slider==NULL)
		return -3;

	sl=data_slider->sl; /* slider bar length */

	/* convert psval to within range [0 data_slider->sl-1] */
	psval=psval*(data_slider->sl-1)/100;
	if(psval<0)psval=0;
	else if(psval > sl-1)psval=sl-1;

	data_slider->val=psval;
	/* Slider value is drivered from ebox->x0/y0 for Horizotal slider in refresh().
	 * So set slider->x0/y0, NOT data_slider->val.
	 */
	if(data_slider->ptype==slidType_horiz) {     	/* Horizontal sliding bar */
        	slider->x0=data_slider->sxy.x+psval-(slider->width>>1);
		/* Here also refresh y0, in case we want to refresh slider x0y0 as per updated
		 * data_slider->sxy !!! ---
		 * NO!!! slider->y0 will drive sxy.y again !!!!
		 */
        	//slider->y0=data_slider->sxy.y-(slider->height>>1);

	}
	else if(data_slider->ptype==slidType_vert) {	/* Vertical sliding bar */
        	slider->y0=data_slider->sxy.y-psval-(slider->height>>1);
		/* Here also refresh x0, in case we want to refresh slider x0y0 as per updated
		 * data_slider->sxy !!!
		 * NO!!! slider->x0 will drive sxy.x again !!!!
		 */
        	//slider->x0=data_slider->sxy.x-(slider->width>>1);
	}
	else {
		EGI_PDEBUG(DBG_SLIDER,"Warning: Undefined slider type! Fail to set value.\n");
		return -4;
	}

	return 0;
}


/*--------------------------------------------------------------------
Reset parameters for slider ebox according to psval, sl, and sxy.

@slider:        The concerning slider.
@psval:		Percentage value [0 100]
@sl:		total length of the bar
@sxy:		starting point of the bar

Return:
        0       OK
        <0      Fails
--------------------------------------------------------------------*/
int egi_slider_reset(EGI_EBOX *slider, int psval, int sl, EGI_POINT sxy)
{
        EGI_DATA_BTN *data_ebox=NULL;
        EGI_DATA_SLIDER *data_slider=NULL;

        if( slider==NULL || slider->egi_data==NULL )
                return -1;

        if( slider->type != type_slider)
                return -2;

        /* Get data */
        data_ebox=(EGI_DATA_BTN *)(slider->egi_data);
        data_slider=(EGI_DATA_SLIDER *)(data_ebox->prvdata);
        if(data_slider==NULL)
                return -3;

	/* reset sl and sxy */
        data_slider->sl=sl; 	/* total length of the bar */
        data_slider->sxy=sxy;   /* starting point */

	/* set x0y0 of sliding bar as per psval */
	if( egi_slider_setpsval(slider, psval) !=0 )
		return -4;

	return 0;
}


/*------------------------------------------------
Get EGI_DATA_SLIDER from a slider
Return:
	A pointer to EGI_DATA_SLIDER 	OK
	NULL				Fails
-------------------------------------------------*/
EGI_DATA_SLIDER *egi_slider_getdata(EGI_EBOX *slider)
{
	if(slider==NULL)
		return NULL;

	if(slider->type != type_slider)
		return NULL;

	if(slider->egi_data==NULL)
		return NULL;

	EGI_DATA_BTN *data_btn=(EGI_DATA_BTN *)(slider->egi_data);

        return (EGI_DATA_SLIDER *)(data_btn->prvdata);
}


/*----------------------------------------------------
release data btn.  to be called by egi_free_ebox()
Remember data slider is contained in data_btn.
------------------------------------------------------*/
void egi_free_data_slider(EGI_DATA_BTN *data_btn)
{
        EGI_DATA_SLIDER *data_slider=NULL;

	if(data_btn != NULL)
	{
		if(data_btn->prvdata !=NULL ) {
		        data_slider=(EGI_DATA_SLIDER *)(data_btn->prvdata);
			if(data_slider->slot_bkimg != NULL)
				free(data_slider->slot_bkimg);

			free(data_btn->prvdata);
		}

		free(data_btn);

		data_btn=NULL;  /* ineffective though */
	}
}

