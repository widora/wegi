/*----------------------- egi_btn.c ------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


egi btn type ebox functions

Midas Zhou
----------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "egi_color.h"
#include "egi.h"
#include "egi_btn.h"
#include "egi_debug.h"
#include "egi_symbol.h"
#include "egi_timer.h"

/*-------------------------------------
      button ebox self_defined methods
--------------------------------------*/
static EGI_METHOD btnbox_method=
{
        .activate=egi_btnbox_activate,
        .refresh=egi_btnbox_refresh,
        .decorate=NULL,
        .sleep=NULL,
        .free=NULL, /* to see egi_ebox_free() in egi.c */
};

static void egi_btn_touch_effect(EGI_EBOX *ebox, EGI_TOUCH_DATA *touch_data);


/*-----------------------------------------------------------------------------
Dynamically create btn_data struct

id: 		id number of the button
shape: 		shape of the button
icon: 		symbol page for the icon
icon_code:	code of the symbol
font:		symbol page for the ebox->tag font

return:
        poiter          OK
        NULL            fail
-----------------------------------------------------------------------------*/
EGI_DATA_BTN *egi_btndata_new(int id, enum egi_btn_type shape,
			      EGI_SYMPAGE *icon, int icon_code,
			      EGI_SYMPAGE *font )
{
        /* malloc a egi_data_btn struct */
        EGI_PDEBUG(DBG_BTN,"egi_btndata_new(): malloc data_btn ...\n");
        EGI_DATA_BTN *data_btn=malloc(sizeof(EGI_DATA_BTN));
        if(data_btn==NULL)
        {
                printf("egi_btndata_new(): fail to malloc egi_data_btn.\n");
		return NULL;
	}
	/* clear data */
	memset(data_btn,0,sizeof(EGI_DATA_BTN));

	/* assign struct number */
	data_btn->id=id;
	data_btn->shape=shape;
	data_btn->icon=icon;
	data_btn->icon_code=icon_code;
	data_btn->font=font;
	data_btn->opaque=255; //-1; 255 100% forecolor /*, <0, No alpha effect */
	data_btn->touch_effect=egi_btn_touch_effect;
	//data_btn->font_color=0; /* black as default */

	return data_btn;
}


/*-----------------------------------------------------------------------------
Dynamically create a new btnbox

return:
        poiter          OK
        NULL            fail
-----------------------------------------------------------------------------*/
EGI_EBOX * egi_btnbox_new( char *tag, /* or NULL to ignore */
        EGI_DATA_BTN *egi_data,
        bool movable,
        int x0, int y0,
        int width, int height, /* for frame drawing and prmcolor filled_rectangle */
        int frame,
        int prmcolor /* applys only if prmcolor>=0 and egi_data->icon != NULL */
)
{
        EGI_EBOX *ebox;

        /* 0. check egi_data */
        if(egi_data==NULL)
        {
                printf("egi_btnbox_new(): egi_data is NULL. \n");
                return NULL;
        }

        /* 1. create a new common ebox */
        EGI_PDEBUG(DBG_BTN,"egi_btnbox_new(): start to egi_ebox_new(type_btn)...\n");
        ebox=egi_ebox_new(type_btn);// egi_data NOT allocated in egi_ebox_new()!!!
        if(ebox==NULL)
	{
                printf("egi_btnbox_new(): fail to execute egi_ebox_new(type_btn). \n");
                return NULL;
	}

        /* 2. default method assigned in egi_ebox_new() */

        /* 3. btn ebox object method */
        EGI_PDEBUG(DBG_BTN,"egi_btnbox_new(): assign defined mehtod ebox->method=methd...\n");
        ebox->method=btnbox_method;

        /* 4. fill in elements for concept ebox */
	//strncpy(ebox->tag,tag,EGI_TAG_LENGTH); /* addtion EGI_TAG_LENGTH+1 for end token here */
	egi_ebox_settag(ebox,tag);
        ebox->egi_data=egi_data; /* ----- assign egi data here !!!!! */
        ebox->movable=movable;
        ebox->x0=x0;    ebox->y0=y0;
        ebox->width=width;      ebox->height=height;
        ebox->frame=frame;      ebox->prmcolor=prmcolor;

        /* 5. pointer default */
        ebox->bkimg=NULL;

        return ebox;
}


/*-----------------------------------------------------------------------
activate a button type ebox:
	1. get icon symbol information
	2. malloc bkimg and store bkimg.
	3. refresh the btnbox
	4. set status, ebox as active, and botton assume to be released.

TODO:
	1. if ebox size changes(enlarged), how to deal with bkimg!!??

Return:
	0	OK
	<0	fails!
------------------------------------------------------------------------*/
int egi_btnbox_activate(EGI_EBOX *ebox)
{
	int icon_code;
	int symheight;
	int symwidth;

	/* check data */
        if( ebox == NULL)
        {
                printf("egi_btnbox_activate(): ebox is NULL!\n");
                return -1;
        }
	EGI_DATA_BTN *data_btn=(EGI_DATA_BTN *)(ebox->egi_data);
        if( data_btn == NULL)
        {
                printf("egi_btnbox_activate(): data_btn is NULL!\n");
                return -1;
        }


	/* 1. confirm ebox type */
        if(ebox->type != type_btn)
        {
                printf("egi_btnbox_activate(): Not button type ebox!\n");
                return -1;
        }

        /* 2. activate(or wake up) a sleeping ebox
         *     not necessary to adjust ebox size and allocate bkimg memory for a sleeping ebox
	 *    TODO: test,   If PAGE wallpaper has changed ?????
         */
        if(ebox->status==status_sleep)
        {
                ebox->status=status_active;     /* reset status before refresh !! */
		ebox->need_refresh=true;
                if(egi_btnbox_refresh(ebox)!=0) /* refresh the graphic display */
                {
			ebox->status=status_sleep;
                        EGI_PDEBUG(DBG_TXT,"Fail to wake up sleeping ebox '%s'!\n",ebox->tag);
                        return -3;
                }
                EGI_PDEBUG(DBG_TXT,"Wake up a sleeping '%s' ebox.\n",ebox->tag);
                return 0;
        }


	/* only if it has an icon, get symheight and symwidth */
	if(data_btn->icon != NULL)
	{
		icon_code=( (data_btn->icon_code)<<16 )>>16; /* as SYM_SUB_COLOR+CODE */
		/* check icon_code */
		if(icon_code<0 || icon_code > data_btn->icon->maxnum) {
                	printf("%s: icon code is invalid! reset icon to NULL \n", __func__);
			data_btn->icon=NULL;
		}
		else {
			//int bkcolor=data_btn->icon->bkcolor;
			symheight=data_btn->icon->symheight;
			symwidth=data_btn->icon->symwidth[icon_code];
			/* use bigger size for ebox height and width !!! */
			if(symheight > ebox->height || symwidth > ebox->width )
			{
				ebox->height=symheight;
				ebox->width=symwidth;
	 		}
		}
	}

	/* else if no icon, use prime shape and size */


	/* origin(left top), for btn H&W x0,y0 is same as ebox */
	int x0=ebox->x0;
	int y0=ebox->y0;

	/* 2. verify btn data if necessary. --No need here*/
        if( ebox->height==0 || ebox->width==0)
        {
                printf("egi_btnbox_activate(): height or width is 0 in ebox '%s'!\n",ebox->tag);
                return -1;
        }


   if(ebox->movable) /* only if ebox is movale , TODO: or prmcolor<0 */
   {
	/* 3. malloc bkimg for the icon, not ebox, so use symwidth and symheight */
	if(egi_alloc_bkimg(ebox, ebox->height, ebox->width)==NULL)
	{
		printf("egi_btnbox_activate(): fail to egi_alloc_bkimg()!\n");
		return -2;
	}

	/* 4. update bkimg box */
	ebox->bkbox.startxy.x=x0;
	ebox->bkbox.startxy.y=y0;
	ebox->bkbox.endxy.x=x0+ebox->width-1;
	ebox->bkbox.endxy.y=y0+ebox->height-1;

	#if 0 /* DEBUG */
	EGI_PDEBUG(DBG_BTN," button activating... fb_cpyto_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x, ebox->bkbox.endxy.y);
	#endif
	/* 5. store bk image which will be restored when this ebox position/size changes
	 *    when bkbox is NOT totally out of the FB BOX
	 */
	if( !box_outbox(&ebox->bkbox, &gv_fb_box) ) {
  	     if( fb_cpyto_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
				ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) <0) {
		  return -3;
	     }
	     ebox->bkimg_valid=true;
	}
	else
	    ebox->bkimg_valid=false;

    } /* end of movable codes */

	/* 6. set button status */
	data_btn->status=released_hold;

	/*** In case _hide() is called just before _activate()!
	 * Do not display for a hidden ebox */
        if( ebox->status==status_hidden )
                goto BTN_ACTIVATE_END;

	/* Set ebox status */
	ebox->status=status_active; /* If not, you can not refresh */

	/* 7. set need_refresh */
	ebox->need_refresh=true;

	/* 8. refresh btn ebox */
	if( egi_btnbox_refresh(ebox) != 0)
		return -4;

BTN_ACTIVATE_END:
	EGI_PDEBUG(DBG_BTN,"egi_btnbox_activate(): a '%s' ebox is activated.\n",ebox->tag);
	return 0;
}


/*-----------------------------------------------------------------------
refresh a button type ebox:
	1.refresh button ebox according to updated parameters:
		--- position x0,y0, offx,offy
		--- symbol page, symbol code ...etc.
		... ...
	2. restore bkimg and store bkimg.
	3. drawing the icon
	4. take actions according to btn_status (released, pressed).

TODO:
	1. if ebox size changes(enlarged), how to deal with bkimg!!??

Return:
	1	need_refresh=false
	0	OK
	<0	fails!
------------------------------------------------------------------------*/
int egi_btnbox_refresh(EGI_EBOX *ebox)
{
	int i;
	int bkcolor=SYM_FONT_DEFAULT_TRANSPCOLOR;
	int symheight;
	int symwidth;
	int icon_code=0; /* 0, first ICON */;
	uint16_t sym_subcolor=0; /* symbol substitute color, 0 as no subcolor */

	/* check data */
        if( ebox == NULL)
        {
                printf("egi_btnbox_refresh(): ebox is NULL!\n");
                return -1;
        }

	/* confirm ebox type */
        if(ebox->type != type_btn)
        {
                printf("egi_btnbox_refresh(): Not button type ebox!\n");
                return -1;
        }

        /* check the ebox status */
        if( ebox->status != status_active && ebox->status != status_hidden )
        {
                EGI_PDEBUG(DBG_TXT,"Ebox '%s' is not active/hidden! refresh action is ignored! \n",ebox->tag);
                return -2;
        }

	/* only if need_refresh is true */
	if(!ebox->need_refresh)
	{
		EGI_PDEBUG(DBG_BTN,"egi_btnbox_refresh(): need_refresh=false, abort refresh.\n");
		return 1;
	}


   if(ebox->movable && ebox->bkimg_valid) /* only if ebox is movable and bkimg valid */
   {
	/* 2. restore bk image use old bkbox data, before refresh */
	#if 0 /* DEBUG */
	EGI_PDEBUG(DBG_BTN,"button refresh... fb_cpyfrom_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x,ebox->bkbox.endxy.y);
	#endif

	/* Check wheather PAGE bkimg changed or not,
	 * If changed, do NOT copy old ebox->bkimg to FB, as PAGE bkimg is already the new one!
	 */
	if( ebox->container==NULL || ebox->container->page_update !=true )
	{
        	if( fb_cpyfrom_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
                     	 		   ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) < 0)
		{
			printf("%s: Fail to call fb_cpyfrom_buf!\n", __func__);
			return -3;
		}
	}

   } /* end of movable codes before update data */


	/* 3. ---------- get updated data  ------- */
	EGI_DATA_BTN *data_btn=(EGI_DATA_BTN *)(ebox->egi_data);
        if( data_btn == NULL)
        {
                printf("%s: data_btn is NULL!\n", __func__);
                return -1;
        }

	/* only if it has an icon */
	if(data_btn->icon != NULL)
	{
		icon_code=( (data_btn->icon_code)<<16 )>>16; /* as SYM_SUB_COLOR + CODE */
		/* check icon_code */
		if(icon_code<0 || icon_code > data_btn->icon->maxnum) {
                	printf("%s: icon code is invalid! reset icon to NULL \n", __func__);
			data_btn->icon=NULL;
		}
		else {
			sym_subcolor = (data_btn->icon_code)>>16;

			bkcolor=data_btn->icon->bkcolor;
			symheight=data_btn->icon->symheight;
			symwidth=data_btn->icon->symwidth[icon_code];
			/* use bigger size for ebox height and width !!! */
			if(symheight > ebox->height || symwidth > ebox->width )
			{
				ebox->height=symheight;
				ebox->width=symwidth;
		 	}
		}
	}
	/* else if no icon, use prime shape */

	/* check ebox size */
        if( ebox->height==0 || ebox->width==0)
        {
                printf("egi_btnbox_refresh(): height or width is 0 in ebox '%s'!\n",ebox->tag);
                return -1;
        }

	/* origin(left top) for btn H&W x0,y0 is same as ebox */
	int x0=ebox->x0;
	int y0=ebox->y0;

	int height=ebox->height;
	int width=ebox->width;

   if(ebox->movable) /* only if ebox is movale, TODO: or prmcolor < 0 */
   {
       /* ---- 4. redefine bkimg box range, in case it changes
	* check ebox height and font lines in case it changes, then adjust the height
	* updata bkimg->bkbox according
	*/
        ebox->bkbox.startxy.x=x0;
        ebox->bkbox.startxy.y=y0;
        ebox->bkbox.endxy.x=x0+ebox->width-1;
        ebox->bkbox.endxy.y=y0+ebox->height-1;

	#if 1 /* DEBUG */
	EGI_PDEBUG(DBG_BTN,"egi_btnbox_refresh(): fb_cpyto_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x,ebox->bkbox.endxy.y);
	#endif
        /* ---- 5. Store bk image which will be restored when you refresh it later,
	 *	   in case ebox position/size changes.
	 *
	 *  Refresh only if bkbox is NOT completely out of FB box! */
        if(!box_outbox(&ebox->bkbox, &gv_fb_box)) {
         	if(fb_cpyto_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
                                ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) < 0)
		{
			printf("egi_btnbox_refresh(): fb_cpyto_buf() fails.\n");
			return -4;
		}
		ebox->bkimg_valid=true;
	}
	else {
		printf("'%s' bkbox out of gv_fb_box!\n",ebox->tag);
		ebox->bkimg_valid=false;
	}

   } /* end of movable codes */


        /* If status is hidden, then skip displaying codes */
        if( ebox->status == status_hidden )
                goto BTN_REFRESH_END;


        /*  ------------>   DISPLAYING CODES STARTS  <----------  */


        /* --- 6.1 Draw shape --- */
        if(ebox->prmcolor >= 0 && data_btn->icon == NULL )
        {
                /* set color */
           	fbset_color(ebox->prmcolor);
		switch(data_btn->shape)
		{
			case btnType_square:
			        draw_filled_rect(&gv_fb_dev,x0,y0,x0+width-1,y0+height-1);

			#if 0 	/* --- draw frame --- */
       			        if(ebox->frame >= 0) /* 0: simple type */
       				{
                			fbset_color(ebox->tag_color); /* use ebox->tag_color  */
                			draw_rect(&gv_fb_dev,x0,y0,x0+width-1,y0+height-1);
			        }
			#endif
				break;
			case btnType_circle:
				draw_filled_circle(&gv_fb_dev, x0+width/2, y0+height/2,
								width>height?height/2:width/2);
        		#if 0	/* --- draw frame --- */
       			        if(ebox->frame == 0) /* 0: simple type */
       				{
                			fbset_color(ebox->tag_color); /* use ebox->tag_color  */
					draw_circle(&gv_fb_dev, x0+width/2, y0+height/2,
								width>height?height/2:width/2);
			        }
       			        else if(ebox->frame > 0) /* >0: width=3 circle  */
       				{
                			fbset_color(ebox->tag_color); /* use ebox->tag_color  */
					draw_filled_annulus( &gv_fb_dev, x0+width/2, y0+height/2,
								width>height ? (height/2-1):(width/2-1), 3);
			        }
			#endif
				break;

			default:
				printf("egi_btnbox_refresh(): shape not defined! \n");
				break;
		}
        }


	/* 6.2 Draw simple outline frames */
	if( ebox->prmcolor >0 && ebox->frame >= 0 && ebox->frame < 100 ) /* >=100 for frame_img */
	{
       	    fbset_color(ebox->prmcolor);
	    switch(data_btn->shape)
	    {
		case btnType_square:
		        if(ebox->frame >= 0) /* 0: simple type */
				draw_rect(&gv_fb_dev,x0,y0,x0+width-1,y0+height-1);
			break;
		case btnType_circle:
		        if(ebox->frame == 0) /* 0: simple type */
       			{
				draw_circle(&gv_fb_dev, x0+width/2, y0+height/2,
							width>height?height/2:width/2);
		        }
       		        else if(ebox->frame > 0) /* >0: width=3 circle  */
       			{
				draw_filled_annulus( &gv_fb_dev, x0+width/2, y0+height/2,
							width>height ? (height/2-1):(width/2-1), 3);
		        }
			break;

		default:
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

   if(data_btn->font == NULL) {
		/* do nothing */
  		printf("%s: ebox '%s' data_btn->font is NULL, fail to put tag on button.\n",__func__, ebox->tag);
   }
   else if(data_btn->showtag==true)
   {
	/* 9.2 get length of tag */
	int taglen=0;
	taglen=strlen(ebox->tag);

//	while( ebox->tag[taglen] )
//	{
//		taglen++;
//	}
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
		//symbol_string_writeFB(&gv_fb_dev, data_btn->font, SYM_NOSUB_COLOR, 1,
		//symbol_string_writeFB(&gv_fb_dev, data_btn->font, data_btn->font_color, 1,

		/* !!!! tag_color shall NOT be the same as symbol bkcolor, OR it will NOT display */
		symbol_string_writeFB(&gv_fb_dev, data_btn->font, ebox->tag_color, 1,
 							ebox->x0+shx, ebox->y0+shy, ebox->tag, -1);

	} /* endif: taglen>0 */

   }/* endif: data_btn->font != NULL */


BTN_REFRESH_END:
	/* 10. finally, reset need_refresh */
	ebox->need_refresh=false;

	return 0;
}




/*-----------------------------------------------------------------------
Refresh a group of button type ebox with three for() sessions, the purpose is
to restore/save bkimgs together and avoid interfering with each other.

  FOR() PART-1: restor original bkimg to fb.
  FOR() PART-2: save bkimg for updated position.
  FOR() PART-3: refresh all other elements for each ebox.

@ebox_group:	a group of ebox
@num:		number eboxes int the group

Return:
	1	need_refresh=false
	0	OK
	<0	fails!
------------------------------------------------------------------------*/
void egi_btngroup_refresh(EGI_EBOX **ebox_group, int num)
{
	int n;
	int i;
	int bkcolor=SYM_FONT_DEFAULT_TRANSPCOLOR;
	int symheight;
	int symwidth;
	int icon_code=0; /* 0, first ICON */;
	uint16_t sym_subcolor=0; /* symbol substitute color, 0 as no subcolor */
	EGI_EBOX *ebox;
	EGI_DATA_BTN *data_btn;
	int x0,y0;
	int height,width;

  /* FOR() PART-1: restor original bkimg to fb */
  for(n=0;n<num;n++) {
	ebox=ebox_group[n];

	/* check data */
        if( ebox == NULL)
        {
                printf("egi_btngroup_refresh(): ebox_group[%d] is NULL!\n",n);
                continue; //return -1;
        }

	/* confirm ebox type */
        if(ebox->type != type_btn)
        {
                printf("egi_btngroup_refresh(): ebox_group[%d] is Not a button type ebox!\n",n);
                continue; //return -1;
        }

	/*  check the ebox status  */
	if( ebox->status != status_active )
	{
		EGI_PDEBUG(DBG_BTN,"ebox '%s' is not active! refresh action is ignored! \n",ebox->tag);
		continue; //return -2;
	}

	/* only if need_refresh is true */
	if(!ebox->need_refresh)
	{
		EGI_PDEBUG(DBG_BTN,"egi_btngroup_refresh(): ebox '%s' need_refresh=false, abort refresh.\n",
											ebox->tag);
		continue; //return 1;
	}


   if(ebox->movable && ebox->bkimg_valid) /* only if ebox is movale and bkimg is valid*/
   {
	/* 2. restore bk image use old bkbox data, before refresh */
	#if 0 /* DEBUG */
	EGI_PDEBUG(DBG_BTN,"button refresh... fb_cpyfrom_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x,ebox->bkbox.endxy.y);
	#endif
        if( fb_cpyfrom_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
                               ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) < 0)
	{
		printf("egi_btngroup_refresh(): fail to cpyfrom buf for ebox '%s'.\n",ebox->tag);
		continue; //return -3;
	}
   } /* end of movable codes */

  } /* end FOR() PART-1 */


  /* FOR() PART-2: save bkimg for updated position */
  for(n=0;n<num;n++) {
	ebox=ebox_group[n];

	/* 3. get updated data */
	data_btn=(EGI_DATA_BTN *)(ebox->egi_data);
        if( data_btn == NULL)
        {
                printf("egi_btngroup_refresh(): data_btn is NULL!\n");
                continue; //return -1;
        }

	/* only if it has an icon */
	if(data_btn->icon != NULL)
	{
		icon_code=( (data_btn->icon_code)<<16 )>>16; /* as SYM_SUB_COLOR + CODE */
//		sym_subcolor = (data_btn->icon_code)>>16;

//		bkcolor=data_btn->icon->bkcolor;
		symheight=data_btn->icon->symheight;
		symwidth=data_btn->icon->symwidth[icon_code];
		/* use bigger size for ebox height and width !!! */
		if(symheight > ebox->height || symwidth > ebox->width )
		{
			ebox->height=symheight;
			ebox->width=symwidth;
	 	}
	}
	/* else if no icon, use prime shape */

	/* check ebox size */
        if( ebox->height==0 || ebox->width==0)
        {
                printf("egi_btngroup_refresh(): height or width is 0 in ebox '%s'!\n",ebox->tag);
                continue; //return -1;
        }

	/* origin(left top) for btn H&W x0,y0 is same as ebox */
	x0=ebox->x0;
	y0=ebox->y0;

   if(ebox->movable) /* only if ebox is movale */
   {
       /* ---- 4. redefine bkimg box range, in case it changes */
	/* check ebox height and font lines in case it changes, then adjust the height */
	/* updata bkimg->bkbox according */
        ebox->bkbox.startxy.x=x0;
        ebox->bkbox.startxy.y=y0;
        ebox->bkbox.endxy.x=x0+ebox->width-1;
        ebox->bkbox.endxy.y=y0+ebox->height-1;

	#if 1 /* DEBUG */
	EGI_PDEBUG(DBG_BTN,"fb_cpyto_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x,ebox->bkbox.endxy.y);
	#endif
        /* ---- 5. store bk image which will be restored when you refresh it later,
	 *	 ebox position/size changes */
        if(!box_outbox(&ebox->bkbox, &gv_fb_box)) {   /* If bkbox NOT totally out of FB box */
	        if(fb_cpyto_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
                                ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) < 0)
	        {
		     printf("egi_btngroup_refresh(): fb_cpyto_buf() fails.\n");
		     continue; //return -4;
	        }

		ebox->bkimg_valid=true;
	}
	else /* if bkbox is outof FB box */
		ebox->bkimg_valid=false;

   } /* end of movable codes */

 } /* end FOR() PART -2 */


  /* FOR() PART-3: refresh all other elements for each ebox */
  for(n=0;n<num;n++) {
	ebox=ebox_group[n];

	/* origin(left top) for btn H&W x0,y0 is same as ebox */
	x0=ebox->x0;
	y0=ebox->y0;

	/* get updated data */
	data_btn=(EGI_DATA_BTN *)(ebox->egi_data);
        if( data_btn == NULL)
        {
                printf("egi_btngroup_refresh(): data_btn is NULL!\n");
                continue; //return -1;
        }

	if(data_btn->icon != NULL)
	{
		icon_code=( (data_btn->icon_code)<<16 )>>16; /* as SYM_SUB_COLOR + CODE */
		sym_subcolor = (data_btn->icon_code)>>16;

		bkcolor=data_btn->icon->bkcolor;
//		symheight=data_btn->icon->symheight;
//		symwidth=data_btn->icon->symwidth[icon_code];
	}

        /* ---- 6. set color and drawing shape ----- */
        if(ebox->prmcolor >= 0 && data_btn->icon == NULL )
        {
		height=ebox->height;
		width=ebox->width;

                /* set color */
           	fbset_color(ebox->prmcolor);
		switch(data_btn->shape)
		{
			case btnType_square:
			        draw_filled_rect(&gv_fb_dev,x0,y0,x0+width-1,y0+height-1);
        			/* --- draw frame --- */
       			        if(ebox->frame >= 0) /* 0: simple type */
       				{
                			fbset_color(ebox->tag_color); /* use ebox->tag_color  */
                			draw_rect(&gv_fb_dev,x0,y0,x0+width-1,y0+height-1);
			        }
				break;
			case btnType_circle:
				draw_filled_circle(&gv_fb_dev, x0+width/2, y0+height/2,
								width>height?height/2:width/2);
        			/* --- draw frame --- */
       			        if(ebox->frame == 0) /* 0: simple type */
       				{
                			fbset_color(ebox->tag_color); /* use ebox->tag_color  */
					draw_circle(&gv_fb_dev, x0+width/2, y0+height/2,
								width>height?height/2:width/2);
			        }
       			        else if(ebox->frame > 0) /* >0: width=3 circle  */
       				{
                			fbset_color(ebox->tag_color); /* use ebox->tag_color  */
					draw_filled_annulus( &gv_fb_dev, x0+width/2, y0+height/2,
								width>height ? (height/2-1):(width/2-1), 3);
			        }

				break;

			default:
				printf("egi_btngroup_refresh(): shape not defined! \n");
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

   if(data_btn->font == NULL) {
  		//printf("egi_btnbox_refresh(): data_btn->font is NULL, fail to put tag on button.\n");
   }
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

#if 0
		int strwidth=0; /* total number of pixels of all char in ebox->tag */
		// int symheight=data_btn->icon->symheight; already above assigned.
		/* WARNIGN: Do not mess up with sympg icon!  sympy font is right here! */
		int *wdata=data_btn->font->symwidth; /*symwidth data */

		for(i=0;i<taglen;i++)
		{
			/* only if in code range */
			if( (ebox->tag[i]) <= (data_btn->font->maxnum) )
				strwidth += wdata[ (unsigned int)ebox->tag[i] ];
		}
#endif
		int strwidth=symbol_string_pixlen(ebox->tag, data_btn->font);
		int shx=0, shy=0; /* shift x,y to fit the tag just in middle of ebox shape */
		int  fontheight=data_btn->font->symheight;

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
		//symbol_string_writeFB(&gv_fb_dev, data_btn->font, SYM_NOSUB_COLOR, 1,
		//symbol_string_writeFB(&gv_fb_dev, data_btn->font, data_btn->font_color, 1,
		symbol_string_writeFB(&gv_fb_dev, data_btn->font, ebox->tag_color, 1,
 							ebox->x0+shx, ebox->y0+shy, ebox->tag, -1);

	} /* endif: taglen>0 */

   }/* endif: data_btn->font != NULL */

	/* 10. finally, reset need_refresh */
	ebox->need_refresh=false;

 } /* end FOR() PART -3 */

}


/*----------------------------------------
Put a button ebox to disappear from FB.
1. Restore bkimg to erase image of itself.
2. Reset status

Return
        0       OK
        <0      fail
-----------------------------------------*/
int egi_btnbox_hide(EGI_EBOX *ebox)
{
        if( ebox==NULL ) {
                printf("%s: ebox is NULL or type error, fail to make it hidden.\n",__func__);
                return -1;
        }

        if( ebox->movable && ebox->bkimg !=NULL ) { /* only for movable ebox, it holds bkimg. */
                /* restore bkimg */
                if(fb_cpyfrom_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
                               ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) <0 )
                {
                        printf("%s: Fail to restor bkimg for a '%s' ebox.\n", __func__, ebox->tag);
                        return -1;
                }
        }

        /* reset status */
        ebox->status=status_hidden;

        EGI_PDEBUG(DBG_BTN,"A '%s' ebox is put to hide.\n",ebox->tag);

        return 0;
}


/*----------------------------------------
Bring a hidden ebox back to active by set
status to status_active.

Return
        0       OK
        <0      fail
-----------------------------------------*/
int egi_btnbox_unhide(EGI_EBOX *ebox)
{
        if(ebox==NULL) {
                printf("%s: ebox is NULL, fail to make it hidden.\n",__func__);
                return -1;
        }

        /* reset status */
        ebox->status=status_active;

        EGI_PDEBUG(DBG_TXT,"Unhide a '%s' ebox and set status as active.\n",ebox->tag);
        return 0;
}


/*-------------------------------------------------
release struct egi_btn_data
--------------------------------------------------*/
void egi_free_data_btn(EGI_DATA_BTN *data_btn)
{
	if(data_btn != NULL)
		free(data_btn);

	data_btn=NULL;  /* useless */
}


/*--------------------------------------------------------------------------------
Default effect when the button is touched, or to redefine it later.

TODO: To use ebox frame_img, or default icon_code and effect icon_code ?!


Note:   This func may cause double_clicking status nerver detected !!!!!!
	For displaying effect cost too much time, which exceed TM_DBCLICK_INTERVAL
----------------------------------------------------------------------------------*/
static void egi_btn_touch_effect(EGI_EBOX *ebox, EGI_TOUCH_DATA *touch_data) //enum egi_touch_status touch_status)
{

	/* for pressing down touch_status only */
        if(ebox==NULL )  return;

	/* if different effect for different shape */
//	enum egi_btn_type btn_shape=((EGI_DATA_BTN *)ebox->egi_data)->shape;

        /* draw pressing effect image */
        fbset_color(WEGI_COLOR_WHITE);

	//printf("dx=%d, dy=%d \n", touch_data->dx, touch_data->dy);

	/* Display effective image for pressing
	 * NOTE: It's better to update btn icon_code rather than draw geoms directly!!!
	 */
	if(touch_data->status == pressing ) {

		printf("'%s' trigger touch effect for 'pressing'.\n", ebox->tag);
	/* for Circle shape */
//	  if(btn_shape==circle) {
	        draw_filled_annulus(&gv_fb_dev, ebox->x0+ebox->width/2, ebox->y0+ebox->height/2,
			(ebox->width < ebox->height) ? (ebox->width/2-5/2):(ebox->height/2-5/2), 5);  /* r, circle width=5 */
		//printf("---- ebox: H%d x W%d, dannulus: R=%d ----\n",ebox->height,ebox->width,
		//	(ebox->width < ebox->height) ? (ebox->width/2-5/2):(ebox->height/2-5/2) );
//	  }
	/* for Square/Rect shape */
//	  else if(btn_shape==square) {
//		draw_wrect(&gv_fb_dev, ebox->x0+5/2,ebox->y0+5/2,
//				ebox->x0+ebox->width-5/2,ebox->y0+ebox->height-5/2, 5/2);
//	  }

	        tm_delayms(75);
	}

	/* If touch sliding a distance, refresh and restore btn icon */
	else if(  ebox->movable
		  && (touch_data->dx)*(touch_data->dx)+(touch_data->dy)*(touch_data->dy) >25 ) {
		printf("'%s' trigger touch effect for sliding.\n", ebox->tag);
		egi_ebox_needrefresh(ebox);
		//egi_ebox_refresh(ebox);  Let page routine do it!
	}

	/* refresh button icon for 'releasing's:
	 *   1. pressing on btn then releasing.
	 *   2. slide on btn then releasing.
	 */
	else if( touch_data->status == releasing
                    /* NOTE: to avoid refresh unmovalbe btn with opaque value */
          	    && ((EGI_DATA_BTN*)ebox->egi_data)->opaque <= 0 )
	{
		printf("'%s' trigger touch effect for 'releasing'.\n", ebox->tag);
	        egi_ebox_needrefresh(ebox);
      		//egi_ebox_refresh(ebox);   Let page routine do it!
	}

	/* NOTE: In page routine, if a touch is sliding away from a btn(losing focus), it will
	 *    	 trigger egi_ebox_forcerefresh().
	 */
}



/*--------------------------------------------------------------------
Set icon substitue color for a button ebox

Note:
    Pure BLACK set as transparent color, so here black subcolor should
    not be 0 !!!

return:
        0       OK
        <0      fail
--------------------------------------------------------------------*/
int egi_btnbox_setsubcolor(EGI_EBOX *ebox, EGI_16BIT_COLOR subcolor)
{
        /* check data */
        if( ebox==NULL || ebox->egi_data==NULL )
        {
                printf("egi_btnbox_setsubcolor(): input ebox is invalid or NULL!\n");
                return -1;
        }

        /* confirm ebox type */
        if(ebox->type != type_btn)
        {
                printf("egi_btnbox_setsubcolor(): Not button type ebox!\n");
                return -2;
        }

        /* Pure BLACK set as transparent color, so here black subcolor should NOT be 0 */
        if(subcolor==0)subcolor=1;

        /* set subcolor in icon_code: SUBCOLOR(16bits) + CODE(16bits) */
        EGI_DATA_BTN *data_btn=(EGI_DATA_BTN *)(ebox->egi_data);
        data_btn->icon_code = ((data_btn->icon_code)&0x0000ffff)+(subcolor<<16);

        //printf("%s(): subcolor=0x%04X, set icon_code=0x%08X \n",__func__, subcolor, data_btn->icon_code);

        return 0;
}


/*------------------------------------------------------------------
Duplicate a new ebox and its egi_data from an input ebox.

TODO:  Dangerous!  Not tested!!

return:
        EGI_EBOX pointer        OK
        NULL                    fail
--------------------------------------------------------------------*/
EGI_EBOX *egi_copy_btn_ebox(EGI_EBOX *ebox)
{
        /* check data */
        if( ebox==NULL || ebox->egi_data==NULL )
        {
                printf("egi_copy_btn_ebox(): input ebox is invalid or NULL!\n");
                return NULL;
        }
        /* confirm ebox type */
        if(ebox->type != type_btn)
        {
                printf("egi_copy_btn_ebox(): Not button type ebox!\n");
                return NULL;
        }

        /* copy data_btn */
        EGI_DATA_BTN *data_btn=(EGI_DATA_BTN *)malloc(sizeof(EGI_DATA_BTN));
        if(data_btn==NULL)
        {
                printf("egi_copy_btn_ebox(): malloc data_btn fails!\n");
                return NULL;
        }
        *data_btn=*((EGI_DATA_BTN *)(ebox->egi_data));

        /* copy EGI_EBOX */
        EGI_EBOX *newbox=(EGI_EBOX *)malloc(sizeof(EGI_EBOX));
        if(newbox==NULL)
        {
               printf("egi_copy_btn_ebox(): malloc newbox fails!\n");
               return NULL;
        }
        *newbox=*ebox;

        /* assign egi_data */
        newbox->egi_data=(void *)data_btn;

        return newbox;
}


