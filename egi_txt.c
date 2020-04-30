/*----------------------- egi_txt.c ------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


egi txt tyep ebox functions

Midas Zhou
----------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
//#include <signal.h>
#include <string.h>
#include "egi_color.h"
#include "egi.h"
#include "egi_txt.h"
#include "egi_debug.h"
//#include "egi_timer.h"
#include "egi_symbol.h"
#include "egi_FTsymbol.h"
#include "egi_bjp.h"
#include "egi_filo.h"


//static int egi_txtbox_decorate(EGI_EBOX *ebox);

/*-------------------------------------
      txt ebox self_defined methods
--------------------------------------*/
static EGI_METHOD txtbox_method=
{
        .activate=egi_txtbox_activate,
        .refresh=egi_txtbox_refresh,
        .decorate=NULL, /* define in object source file if necessary.  egi_txtbox_decorate,*/
        .sleep=egi_txtbox_sleep,
        .free=NULL, /* to see egi_ebox_free() in egi.c */
};


/*-----------------------------------------------------------------------------
Dynamically create txt_data struct (For non_FTsymbols)

return:
	poiter 		OK
Y	NULL		fail
-----------------------------------------------------------------------------*/
EGI_DATA_TXT *egi_txtdata_new(	int offx, int offy,
				int nl,
				int llen,
				EGI_SYMPAGE *font,
				uint16_t color
)
{
	int i,j;

	/* malloc a egi_data_txt struct */
	EGI_PDEBUG(DBG_TXT,"malloc data_txt ...\n");
	EGI_DATA_TXT *data_txt=malloc(sizeof(EGI_DATA_TXT));
	if(data_txt==NULL)
	{
		printf("egi_txtdata_new(): fail to malloc egi_data_txt.\n");
		return NULL;
	}
	/* clear up data */
	memset(data_txt,0,sizeof(EGI_DATA_TXT));

	/*  Min. nl */
	if( nl<=0 )
		nl=1;

	/* assign parameters */
        data_txt->offx=offx;
        data_txt->offy=offy;
        data_txt->nl=nl;
        data_txt->llen=llen;
        data_txt->font=font;
        data_txt->color=color;
	data_txt->forward=1; /* >0 default forward*/

	/* init filo_off, default buff_size=1<<8 */
	data_txt->filo_off=egi_malloc_filo(1<<8, sizeof(long), 0b01); /* 0b01: auto double mem when */
	if(data_txt==NULL) {
		printf("egi_txtdata_new(): fail to init data_txt->filo_off.\n");
		free(data_txt);
		return NULL;
	}

        /*  malloc data->txt  */
	EGI_PDEBUG(DBG_TXT,"egi_txtdata_new(): malloc data_txt->txt ...\n");
//        data_txt->txt=malloc(nl*sizeof(char *));
        data_txt->txt=calloc(1,nl*sizeof(char *));
        if(data_txt->txt == NULL) /* malloc **txt */
        {
                printf("egi_txtdata_new(): fail to malloc egi_data_txt->txt!\n");
		egi_free_filo(data_txt->filo_off);
		free(data_txt);
		data_txt=NULL; /* :( */
                return NULL;
        }
//	memset(data_txt->txt,0,nl*sizeof(char *));

	/* malloc data->txt[] */
        for(i=0;i<nl;i++)
        {
		EGI_PDEBUG(DBG_TXT,"egi_txtdata_new(): start to malloc data_txt->txt[%d]...\n",i);
//                data_txt->txt[i]=malloc(llen*sizeof(char));
                data_txt->txt[i]=calloc(1,llen*sizeof(char));
                if(data_txt->txt[i] == NULL) /* malloc **txt */
                {
                        printf("egi_txtdata_new(): fail to malloc egi_data_txt->txt[]!\n");
			/* roll back to free malloced */
                        for(j=0;j<i;j++)
                        {
                                free(data_txt->txt[j]);
                                data_txt->txt[j]=NULL; /* :< */
                        }
                        free(data_txt->txt);
                        data_txt->txt=NULL; /* :) */
			egi_free_filo(data_txt->filo_off); /* free filo */
			free(data_txt);
			data_txt=NULL; /* :))) ...JUST TO EMPHOSIZE THE IMPORTANCE !!!  */

                        return NULL;
                }

                /* clear up data */
//                memset(data_txt->txt[i],0,llen*sizeof(char));
        }

	/* finally! */
	return data_txt;
}

/*-----------------------------------------------------------------------------
Dynamically create txt_data struct  ( For FTsymbols )

return:
	poiter 		OK
	NULL		fail
-----------------------------------------------------------------------------*/
EGI_DATA_TXT *egi_utxtdata_new( int offx, int offy,      /* offset from ebox left top */
				int nl,			/* lines */
				int pixpl,		/* pixels per line */
				FT_Face font_face,	/* font face type */
				int fw, int fh,		/* font width and height, in pixels */
				int gap,		/* adjust gap between lines */
				uint16_t color
)
{
//	int i,j;

	/* malloc a egi_data_txt struct */
	EGI_PDEBUG(DBG_TXT,"malloc data_txt ...\n");
	EGI_DATA_TXT *data_txt=calloc(1, sizeof(EGI_DATA_TXT));
	if(data_txt==NULL)
	{
		EGI_PDEBUG(DBG_TXT,"fail to malloc egi_data_txt.\n");
		return NULL;
	}

	/* Min. nl */
	if(nl<=0)
		nl=1;

	/* assign parameters */
        data_txt->offx=offx;
        data_txt->offy=offy;

        data_txt->nl=nl;
        data_txt->pixpl=pixpl;		/* For FTsymbol txt */
	data_txt->font_face=font_face;
        data_txt->fw=fw;
        data_txt->fh=fh;
	data_txt->gap=gap;
	data_txt->color=color;

	data_txt->forward=1; /* >0 default forward*/

	/* Do NOT malloc for data->txt[] */

	/* init filo_off, default buff_size=1<<8 */
#if 0   /* Current NO USE */
	data_txt->filo_off=egi_malloc_filo(1<<8, sizeof(long), 0b01); /* 0b01: auto double mem when */
	if(data_txt==NULL) {
		printf("egi_txtdata_new(): fail to init data_txt->filo_off.\n");
		free(data_txt);
		return NULL;
	}
#endif

	/* finally! */
	return data_txt;
}

/*-----------------------------------------------------------------------------
Dynamically create a new txtbox object  (For both nonFTsymbols and FTsymbols)

return:
	poiter 		OK
	NULL		fail
-----------------------------------------------------------------------------*/
EGI_EBOX * egi_txtbox_new( char *tag,
	/* parameters for concept ebox */
	EGI_DATA_TXT *egi_data,
	bool movable,
	int x0, int y0,
	int width, int height,
	int frame,
	int prmcolor
)

{
	EGI_EBOX *ebox;

	/* 0. check egi_data */
	if(egi_data==NULL)
	{
		EGI_PDEBUG(DBG_TXT,"egi_txtbox_new(): egi_data is NULL. \n");
		return NULL;
	}

	/* 1. create a new common ebox */
	EGI_PDEBUG(DBG_TXT,"Start to egi_ebox_new(type_txt)...\n");
	ebox=egi_ebox_new(type_txt);// egi_data NOT allocated in egi_ebox_new()!!! , (void *)egi_data);
	if(ebox==NULL)
	{
		EGI_PDEBUG(DBG_TXT,"Fail to execute egi_ebox_new(type_txt). \n");
		return NULL;
	}

	/* 2. default method assigned in egi_ebox_new() */

	/* 3. txt ebox object method */
	EGI_PDEBUG(DBG_TXT,"Assign defined mehtod ebox->method=methd...\n");
	ebox->method=txtbox_method;

	/* 4. fill in elements  */
	egi_ebox_settag(ebox,tag);
	//strncpy(ebox->tag,tag,EGI_TAG_LENGTH); /* addtion EGI_TAG_LENGTH+1 for end token here */
	ebox->egi_data=egi_data; /* ----- assign egi data here !!!!! */
	ebox->movable=movable;
	ebox->x0=x0;	ebox->y0=y0;
	ebox->width=width;	ebox->height=height;
	ebox->frame=frame;	ebox->prmcolor=prmcolor;
	//ebox->frame_img=NULL;
	ebox->frame_alpha=255;  /* init value */

	/* 5. pointer default */
	EGI_PDEBUG(DBG_TXT,"Assign ebox->bkimg=NULL ...\n");
	ebox->bkimg=NULL;

	EGI_PDEBUG(DBG_TXT,"Finish egi_txtbox_new().\n");
	return ebox;
}


/*--------------------------------------------------------------------------------
initialize EGI_DATA_TXT according	( For non_FTsymbols )

offx,offy:			offset from prime ebox
int nl:   			number of txt lines
int llen:  			in byte, data length for each line,
			!!!!! -	llen deciding howmany symbols it may hold.
EGI_SYMPAGE *font:	txt font
uint16_t color:     		txt color
char **txt:       		multiline txt data

Return:
        non-null        OK
        NULL            fails
---------------------------------------------------------------------------------*/
EGI_DATA_TXT *egi_init_data_txt(EGI_DATA_TXT *data_txt,
			int offx, int offy, int nl, int llen, EGI_SYMPAGE *font, uint16_t color)
{
	int i,j;

	/* check data first */
	if(data_txt == NULL)
	{
		printf("egi_init_data_txt(): data_txt is NULL!\n");
		return NULL;
	}
	if(data_txt->txt != NULL) /* if data_txt defined statically, txt may NOT be NULL !!! */
	{
		printf("egi_init_data_txt(): ---WARNING!!!--- data_txt->txt is NOT NULL!\n");
		return NULL;
	}
	if( nl<1 || llen<1 )
	{
		printf("egi_init_data_txt(): data_txt->nl or llen is 0!\n");
		return NULL;
	}
	if( font == NULL )
	{
		printf("egi_init_data_txt(): data_txt->font is NULL!\n");
		return NULL;
	}

	/* --- assign var ---- */
	data_txt->nl=nl;
	data_txt->llen=llen;
	data_txt->font=font;
	data_txt->color=color;
	data_txt->offx=offx;
	data_txt->offy=offy;

	/* --- malloc data --- */
	data_txt->txt=malloc(nl*sizeof(char *));
	if(data_txt->txt == NULL) /* malloc **txt */
	{
		printf("egi_init_ebox(): fail to malloc egi_data_txt->txt!\n");
		return NULL;
	}
	for(i=0;i<nl;i++) /* malloc *txt */
	{
		data_txt->txt[i]=malloc(llen*sizeof(char));
		if(data_txt->txt[i] == NULL) /* malloc **txt */
		{
			printf("egi_init_ebox(): fail to malloc egi_data_txt->txt[]!\n");
			for(j=0;j<i;j++)
			{
				free(data_txt->txt[j]);
				data_txt->txt[j]=NULL;
			}
			free(data_txt->txt);
			data_txt->txt=NULL;

			return NULL;
		}
		/* clear up data */
		memset(data_txt->txt[i],0,llen*sizeof(char));
	}

	return data_txt;
}


/*-------------------------------------------------------------------------------------
activate a txt ebox   (For both nonFTsymbols and FTsymbols)

Note:
	0. if ebox is in a sleep_status, just refresh it, and reset txt file pos offset.
	1. adjust ebox height and width according to its font lines and width.
 	2. store back image covering txtbox frame range.
	3. refresh the ebox.
	4. change status token to active,

TODO:
	if ebox_size is re-sizable dynamically, bkimg mem size MUST adjusted.

Return:
	0	OK
	<0	fails!
----------------------------------------------------------------------------------*/
int egi_txtbox_activate(EGI_EBOX *ebox)
{
//	int i,j;
	int ret;
	int x0=ebox->x0;
	int y0=ebox->y0;
	int height=ebox->height;
	int width=ebox->width;
	EGI_DATA_TXT *data_txt=(EGI_DATA_TXT *)(ebox->egi_data);

	/* 0. check data */
	if(data_txt == NULL)
	{
                printf("egi_txtbox_activate(): data_txt is NULL in ebox '%s'!\n",ebox->tag);
                return -1;
	}
	if(height==0 || width==0)
	{
                printf("egi_txtbox_activate(): height or width is 0 in ebox '%s'!\n",ebox->tag);
                return -1;
	}

	int nl=data_txt->nl;
	int gap=data_txt->gap;
//	int llen=data_txt->llen;
	int offx=data_txt->offx;
	int offy=data_txt->offy;
//	char **txt=data_txt->txt;
	int font_height;
	int rad=10;	/* radius for frame_img */

	/* 1. confirm ebox type */
        if(ebox->type != type_txt)
        {
                EGI_PDEBUG(DBG_TXT,"'%s' is not a txt type ebox!\n",ebox->tag);
                return -2;
        }

        EGI_PDEBUG(DBG_TXT,"Start to activate '%s' txt type ebox!\n",ebox->tag);
	/* 2. activate(or wake up) a sleeping ebox
	 *	not necessary to adjust ebox size and allocate bkimg memory for a sleeping ebox
	 *      TODO: test,   If PAGE wallpaper has changed ?????
	 */
	if( ebox->status==status_sleep )
	{
		((EGI_DATA_TXT *)(ebox->egi_data))->foff=0; /* reset affliated file position */
		ebox->status=status_active; 	/* reset status to active before refresh!!! */
		ebox->need_refresh=true;
		if(egi_txtbox_refresh(ebox)!=0) /* refresh the graphic display */
		{
			//ebox->status=status_sleep; /* reset status */
			EGI_PDEBUG(DBG_TXT,"---- Fail to wake up sleeping ebox '%s'!\n",ebox->tag);
			return -3;
		}

		EGI_PDEBUG(DBG_TXT,"Wake up a sleeping '%s' ebox.\n",ebox->tag);
		return 0;
	}

        /* 3. check ebox height and font lines, then adjust the height */
	/* check symbol type */
	if(data_txt->font)
		font_height=data_txt->font->symheight;
	else if(data_txt->font_face)
		font_height=data_txt->fh;
	else {
		font_height=0;
		EGI_PDEBUG(DBG_TXT, "data_txt->font and data_txt->font_face are both empty!\n");
		//return -1;
		/* Go on anyway, ingore TXT, just to refresh bkimg.... */
	}

	/* adjust ebox height */
        height= ((font_height+gap)*nl+offy) > height ? ((font_height+gap)*nl+offy) : height;
        ebox->height=height;

 	/* adjust ebox width if FTsymbol txt */
	if( data_txt->font_face ) {
		width= data_txt->pixpl+offx > width ? data_txt->pixpl+offx : width;
		ebox->width=width;
	}

	//TODO: malloc more mem in case ebox size is enlarged later????? //
	/* 4. malloc exbo->bkimg for bk image storing */
   if( ebox->movable || (ebox->prmcolor<0) ) /* only if ebox is movale or it's transparent */
   {
	EGI_PDEBUG(DBG_TXT,"Start to egi_alloc_bkimg() for '%s' ebox. height=%d, width=%d \n",
											ebox->tag,height,width);
		/* egi_alloc_bkimg() will check width and height */
		if( egi_alloc_bkimg(ebox, width, height)==NULL )
        	{
               	 	printf("%s:\033[31m Fail to egi_alloc_bkimg() for '%s' ebox!\033[0m\n", __func__, ebox->tag);
                	return -4;
        	}
		EGI_PDEBUG(DBG_TXT,"\033[32m Finish egi_alloc_bkimg() for '%s' ebox.\033[0m\n",ebox->tag);

	/* 5. store bk image which will be restored when this ebox position/size changes */
	/* define bkimg box */
	ebox->bkbox.startxy.x=x0;
	ebox->bkbox.startxy.y=y0;
	ebox->bkbox.endxy.x=x0+width-1;
	ebox->bkbox.endxy.y=y0+height-1;
#if 0 /* DEBUG */
	EGI_PDEBUG(DBG_TXT,"txt activate... fb_cpyto_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x, ebox->bkbox.endxy.y);
#endif

	EGI_PDEBUG(DBG_TXT,"Start fb_cpyto_buf() for '%s' ebox.\n",ebox->tag);
	if(fb_cpyto_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
				ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) < 0 )
		return -5;
  } /* ebox->movable codes end */

	/* 6. initiate frame_img if frame>100 and prmcolor >=0 */
	/* NOTE: here ebox->frame=100 is non for frame_img! NOT for frame outline!  */
	if( ebox->frame >100 && ebox->prmcolor>=0 ) {
		ebox->frame_img=egi_imgbuf_newFrameImg( height, width,     /* int height, int width */
                                  	ebox->frame_alpha, ebox->prmcolor, /* alpha, color */
	                                frame_round_rect,       	   /* enum imgframe_type */
         	                        1, &rad );                	   /* 1, radius, int pn, int *param */

		if(ebox->frame_img==NULL) {
			printf("%s: Fail to create frame_shape!\n",__func__);
			/* Go on though.... */
		}
	}


	/* 7. reset offset for txt file if fpath applys */
	//???? NOT activate ????? ((EGI_DATA_TXT *)(ebox->egi_data))->foff=0;

	/* 8. refresh displaying the ebox */
	if( ebox->status==status_hidden ) /* Do not display for a hidden ebox */
		goto TXT_ACTIVATE_END;

	ebox->status=status_active;  /* change status to active, if not, you can not refresh.*/
	ebox->need_refresh=true;     /* set for refresh */
	ret=egi_txtbox_refresh(ebox);
	if(ret != 0)
	{
		printf("%s: WARNING!! egi_txtbox_refresh(ebox) return with %d !=0.\n", __func__,ret);
		return -6;
	}

TXT_ACTIVATE_END:
	EGI_PDEBUG(DBG_TXT,"A '%s' ebox is activated.\n",ebox->tag);
	return 0;
}


/*-------------------------------------------------------------------------------
refresh a txt ebox.   (For both nonFTsymbols and FTsymbols)
A hidden txtbox only refresh data, and NO displaying.
A sleeping txtbox refresh nothing.

Note:
	1.refresh ebox image according to following parameter updates:
		---txt(offx,offy,nl,llen)
		---size(height,width)
		---positon(x0,y0)
		---ebox color
	----NOPE!!! affect bkimg size !!!----  2. adjust ebox height and width according to its font line set
	3.restore backgroud from bkimg and store new position backgroud to bkimg.
	4.refresh ebox color if ebox->prmcolor >0, and draw frame.
 	5.update txt, or read txt file to it.
	6.write txt to FB and display it.

Return
	2	fail to read txt file.
	1	need_refresh=false
	0	OK
	<0	fail
-------------------------------------------------------------------------------*/
int egi_txtbox_refresh(EGI_EBOX *ebox)
{
	int i;
	int ret=0;

	/* 1. check data */
	if(ebox->type != type_txt)
	{
		EGI_PDEBUG(DBG_TXT,"Not a txt type ebox!\n");
		return -1;
	}

	/* 2. check the ebox status */
	if( ebox->status != status_active && ebox->status != status_hidden )
	{
		EGI_PDEBUG(DBG_TXT,"Ebox '%s' is not active/hidden! refresh action is ignored! \n",ebox->tag);
		return -2;
	}

	/* only if need_refresh=true */
	if(!ebox->need_refresh)
	{
//		EGI_PDEBUG(DBG_TXT,"need_refresh of '%s' is false!\n",ebox->tag);
		return 1;
	}


	/* 4. get updated ebox parameters */
	int x0=ebox->x0;
	int y0=ebox->y0;
	int height=ebox->height;
	int width=ebox->width;
	EGI_PDEBUG(DBG_TXT,"Start to assign data_txt=(EGI_DATA_TXT *)(ebox->egi_data)\n");
	EGI_DATA_TXT *data_txt=(EGI_DATA_TXT *)(ebox->egi_data);
	int nl=data_txt->nl;
//	int llen=data_txt->llen;
	int offx=data_txt->offx;
	int offy=data_txt->offy;
	char **txt=data_txt->txt;
	int font_height;
	int gap=data_txt->gap;
	int rad=10;	/* radius for frame_img */

	/* check symbol type */
	if(data_txt->font)
		font_height=data_txt->font->symheight;
	else if(data_txt->font_face)
		font_height=data_txt->fh;
	else {
		font_height=0;
		EGI_PDEBUG(DBG_TXT, "data_txt->font and data_txt->font_face are both empty!\n");
		//return -1;
		/* Go on anyway, ingore TXT, just to refresh bkimg.... */
	}


#if 0
	/* test WARNING!!!! fonts only !!!--------------   print out box txt content */
	for(i=0;i<nl;i++)
		printf("egi_txtbox_refresh(): txt[%d]:%s\n",i,txt[i]);
#endif

        /* redefine bkimg box range, in case it changes
	 check ebox height and font lines, then adjust the height */
	height= ( (font_height+gap)*nl+offy)>height ? ((font_height+gap)*nl+offy) : height;

	/* TODO: Ignore width here, width enlarging will affect bkimg mem space!! */

	/* re_adjust frame_img */
	if( ebox->frame_img != NULL && ebox->height != height ) {
		egi_imgbuf_free(ebox->frame_img);
		ebox->frame_img=egi_imgbuf_newFrameImg( height, width,  /* int height, int width */
                                    ebox->frame_alpha, ebox->prmcolor,  /* alpha, color */
	                                frame_round_rect,       	/* enum imgframe_type */
         	                        1, &rad );                	/* 1, radius, int pn, int *param */
		if(ebox->frame_img==NULL) {
			printf("%s: Fail to adjust frame_img!\n",__func__);
			/* Go on though.... */
		}
	}

	/* assign new height */
	ebox->height=height;


   /* ------ restore bkimg and buffer new bkimg
     ONLY IF:
	 1. the txt ebox is movable,
	    and ebox size and position is changed.
	 2. or data_txt->.prmcolor <0 , it's transparent. EGI_NOPRIM_COLOR --- ???
	 3. !!!! If PAGE is updated!!

	Whatever, if(ebox->movable), then refresh it!
	this question to need_refresh token !!!
   */

//   if ( ( ebox->movable && ( (ebox->bkbox.startxy.x!=x0) || (ebox->bkbox.startxy.y!=y0)
//			|| ( ebox->bkbox.endxy.x!=x0+width-1) || (ebox->bkbox.endxy.y!=y0+height-1) ) )
//           || (ebox->prmcolor<0)  )
   if( ebox->movable || ebox->prmcolor<0 )
   {

#if 0 /* DEBUG */
	EGI_PDEBUG(DBG_TXT,"fb_cpyfrom_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x,ebox->bkbox.endxy.y);
#endif
	/* restore bk image before refresh */
        /* Check wheather PAGE bkimg changed or not,
         * If changed, do NOT copy old ebox->bkimg to FB, as PAGE bkimg is already the new one!
         */
        if( ebox->container==NULL || ebox->container->page_update !=true )
        {
		EGI_PDEBUG(DBG_TXT,"ebox '%s' fb_cpyfrom_buf() before refresh...\n", ebox->tag);
       	 	if(fb_cpyfrom_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
        	                       ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) <0 ) {
			return -3;
		}
	} else {
		EGI_PDEBUG(DBG_TXT,"\033[31m Page just updated, ignore fb_cpyfrom_buf() \033[0m \n");
	}

	/* store new coordinates of the ebox
	    updata bkimg->bkbox according */
        ebox->bkbox.startxy.x=x0;
        ebox->bkbox.startxy.y=y0;
        ebox->bkbox.endxy.x=x0+width-1;
        ebox->bkbox.endxy.y=y0+height-1;

#if 1 /* DEBUG */
	EGI_PDEBUG(DBG_TXT,"fb_cpyto_buf: startxy(%d,%d)   endxy(%d,%d)\n",ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
			ebox->bkbox.endxy.x,ebox->bkbox.endxy.y);
#endif
        /* ---- 6. store bk image which will be restored when this ebox position/size changes */
	EGI_PDEBUG(DBG_TXT,"fb_cpyto_buf() before refresh...\n");
        if( fb_cpyto_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
                                ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) < 0)
		return -4;

   } /* end of movable code */


	/* If status is hidden, then skip displaying codes */
	if( ebox->status == status_hidden )
		goto TXT_REFRESH_END;


	/*  ------------>   DISPLAYING CODES STARTS  <----------  */


	/* 7. Draw frame_img first */
	if( ebox->frame_img != NULL ) {
	        egi_imgbuf_windisplay( ebox->frame_img, &gv_fb_dev, -1,     /* img, FB, subcolor */
        	                       0, 0,                 /* int xp, int yp */
                	               x0, y0, width, height   /* xw, yw, winw,  winh */
                        	      );
	}
	/* 8. else draw other type frames/lines */
	else
	{
  		/* ---- 8.1 . refresh prime color under the symbol  before updating txt.  */
		if(ebox->prmcolor >= 0)
		{
			/* set color and draw filled rectangle */
			fbset_color(ebox->prmcolor);
			draw_filled_rect(&gv_fb_dev,x0,y0,x0+width-1,y0+height-1);
		}

		/* --- 8.2 draw frame according to its type  --- */
		if(ebox->frame >= 0 && ebox->frame<100 ) /* 0: simple type */
		{
			fbset_color(0); /* use black as frame color  */
			draw_rect(&gv_fb_dev,x0,y0,x0+width-1,y0+height-1);

			if(ebox->frame == 1) /* draw double line */
				draw_rect(&gv_fb_dev,x0+1,y0+1,x0+width-2,y0+height-2);

			if(ebox->frame == 2) /* draw inner double line */
				draw_rect(&gv_fb_dev,x0+3,y0+3,x0+width-3,y0+height-3);
		}
		/* TODO: other type of frame ....., alpha controled imagbuf for backgroup shape */
	}


	/* ---- 9. if data_txt->fpath !=NULL, then re-read txt file to renew txt[][] */

	/* for non_FTsymbols */
	if( data_txt->font && data_txt->fpath)
	{
		EGI_PDEBUG(DBG_TXT,"egi_txtbox_refresh():  data_txt->fpath is NOT null, re-read txt file now...\n");
		if(egi_txtbox_readfile(ebox,data_txt->fpath)<0) /* not = 0 */
		{
			printf("egi_txtbox_refresh(): fail to read txt file: %s \n",data_txt->fpath);
			ret=2;
		}
	}

	/* ---- 10. run decorate if necessary */
	if(ebox->decorate !=  NULL)
		ebox->decorate(ebox);

	/* ---- 11. refresh TXT, write txt line to FB */
        if(data_txt->font)  /*  --11.1--  For non_FTsymbols */
	{
		EGI_PDEBUG(DBG_TXT,"Start symbol_string_writeFB(), font color=%d ...\n", data_txt->color);
		for(i=0;i<nl;i++)
		{
			EGI_PDEBUG(DBG_TXT,"egi_txtbox_refresh(): txt[%d]='%s' \n",i,txt[i]);
			/* (FBDEV *fb_dev, const EGI_SYMPAGE *sym_page,   \
                		int fontcolor, int transpcolor, int x0, int y0, const char* str, int opaque);
			    1, for font/icon symbol: tranpcolor is its img symbol bkcolor!!! */

			symbol_string_writeFB( 	&gv_fb_dev,
					       	data_txt->font,
					      	data_txt->color, 1,
					       	x0+offx, y0+offy+font_height*i,
						txt[i], -1   );

		}
	}
	else if(data_txt->font_face && data_txt->utxt !=NULL ) /* --11.2-- For FTsymbols */
	{
		EGI_PDEBUG(DBG_TXT,"Start symbol_uft8string_writeFB(), font color=%d ...\n", data_txt->color);
		FTsymbol_uft8strings_writeFB( &gv_fb_dev,
					      data_txt->font_face,
					      data_txt->fw, data_txt->fh,
			  		      data_txt->utxt,
		                              data_txt->pixpl, data_txt->nl,  data_txt->gap,
                		              x0+offx, y0+offy,
                               		      data_txt->color, -1, 255,
					      NULL, NULL, NULL, NULL); /*  *cnt, *lnleft, *penx, *peny */
	}


TXT_REFRESH_END:
	/* ---- 12. reset need_refresh */
	ebox->need_refresh=false;

	return ret;
}


/*---------------------------------------
Put a txt ebox to sleep.
1. Restore bkimg to erase image of itself.
2. Reset status

Return
	0 	OK
	<0 	fail
---------------------------------------*/
int egi_txtbox_sleep(EGI_EBOX *ebox)
{
        if(ebox==NULL) {
                printf("%s: ebox is NULL, fail to make it sleep.\n",__func__);
                return -1;
        }

   	if(ebox->movable) { /* only for movable ebox, it holds bkimg. */
		/* restore bkimg */
       		if(fb_cpyfrom_buf(&gv_fb_dev, ebox->bkbox.startxy.x, ebox->bkbox.startxy.y,
                               ebox->bkbox.endxy.x, ebox->bkbox.endxy.y, ebox->bkimg) <0 )
		{
			printf("%s: Fail to restor bkimg for a '%s' ebox.\n", __func__, ebox->tag);
                	return -1;
		}
   	}

	/* reset status */
	ebox->status=status_sleep;

	EGI_PDEBUG(DBG_TXT,"A '%s' ebox is put to sleep.\n",ebox->tag);
	return 0;
}


/*----------------------------------------
Put a txt ebox to disappear from FB.
1. Restore bkimg to erase image of itself.
2. Reset status

Return
	0 	OK
	<0 	fail
-----------------------------------------*/
int egi_txtbox_hide(EGI_EBOX *ebox)
{
        if(ebox==NULL) {
                printf("%s: ebox is NULL, fail to make it hidden.\n",__func__);
                return -1;
        }

   	if( ebox->movable && ebox->bkimg != NULL ) { /* only for movable ebox, it holds bkimg. */
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

	EGI_PDEBUG(DBG_TXT,"A '%s' ebox is put to hide.\n",ebox->tag);
	return 0;
}


/*----------------------------------------
Bring a hidden ebox back to active by set
status to  status_active.

Return
        0       OK
        <0      fail
-----------------------------------------*/
int egi_txtbox_unhide(EGI_EBOX *ebox)
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

/*--------------------------------------------
Get egi_data_txt from a TXT EBOX
Retrun
	A pointer to EGI_DATA_TXT	ok
	NULL				Fails
---------------------------------------------*/
EGI_DATA_TXT *egi_txtbox_getdata(EGI_EBOX *ebox)
{
	if(ebox==NULL)
		return NULL;

        if(ebox->type != type_txt)
		return NULL;

	return (EGI_DATA_TXT *)(ebox->egi_data);
}


/*-----------------------------------------------------------------
Note:		( For non_FTsymbols )
1. Read a txt file and try to push it to egi_data_txt->txt[]
NOPE! 2. If it reaches the end of file, then reset offset and roll back.
3. llen=data_txt->llen-1  one byte for string end /0.
4. Limit conditions for line return:
	4.1 Max. char number per line   =  llen;
	4.2 Max. pixel number per line  =  bxwidth

Return:
	>0	number of chars read and stored to txt[].
	0	get end of file or stop reading.

	<0	fail
----------------------------------------------------------------*/
int egi_txtbox_readfile(EGI_EBOX *ebox, char *path)
{
	/* check input data here */
	if( ebox==NULL || ebox->egi_data==NULL ) {
		printf("%s: Input ebox or its egi_data is NULL!\n",__func__);
		return -1;
	}
	/* check ebox type */
	if( ebox->type != type_txt ) {
		printf("%s: Input ebox is NOT type_txt!\n",__func__);
		return -2;
	}

	FILE *fil;
	int i;
	char buf[32]={0};
	int nread=0;
	int ret=0;
	EGI_DATA_TXT *data_txt=(EGI_DATA_TXT *)(ebox->egi_data);
	int bxwidth=ebox->width; /* in pixel, ebox width for txt  */
//	int bxheight=ebox->height;
	char **txt=data_txt->txt;
	int nt=0;/* index, txt[][nt] */
	int nl=data_txt->nl; /* from 0, number of txt line */
	int nlw=0; /* current written line of txt */
	int llen=data_txt->llen -1; /*in bytes(chars), length for each line, one byte for /0 */
	int ncount=0; /*in pixel, counter for used pixels per line, MAX=bxwidth.*/
	int *symwidth=data_txt->font->symwidth;/* width list for each char code */
//	int symheight=data_txt->font->symheight;
	int maxnum=data_txt->font->maxnum;
	long foff=data_txt->foff; /* current file position */

	/* WOWOO,NOPE!  bxheight already enlarged to fit for nl in egi_txtbox_activate()...
	 reset llen here, to consider ebox height
	 nl = nl < (bxheight/symheight) ? nl : (bxheight/symheight);  */

	/* check ebox data here */
	if( txt==NULL ) {
		printf("%s: data_txt->txt is NULL!\n",__func__);
		return -2;
	}

	/* If STOP reading */
	if(data_txt->forward==0 )
		return 0;

	/* If read FORWARD && count<0 indicating end of file alread */
	if( data_txt->forward > 0 && data_txt->count<0 )
		return 0;

	/* If read BACKWARD, pop foff from FILO filo_off */
	if(data_txt->forward < 0) {
		if( egi_filo_pop(data_txt->filo_off, &foff)>0 ) {   /* >0, filo empty */
			foff=0;
		} /* else, foff is popped from filo_off */
	}
	/* If read FOREWARD, push foff if !feof(fil), see at the end of this function */

	/* reset txt buf and open file */
	for(i=0;i<nl;i++)
		memset(txt[i],0,data_txt->llen); /* dont use llen, here llen=data_txt->llen-1 */

	fil=fopen(path,"rbe");
	if(fil==NULL) {
		perror("egi_txtbox_readfile()");
		return -2;
	}
	printf("egi_txtbox_readfile(): succeed to open %s, current offset=%ld \n",path,foff);

	/* restore file position */
	fseek(fil,foff,SEEK_SET);

	while( !feof(fil) )
	{
		memset(buf,0,sizeof(buf));/* clear buf */

		nread=fread(buf,1,sizeof(buf),fil);
		if(nread <= 0) /* error or end of file */
			break;

		 /*TODO: for() session to be replaced by egi_push_datatxt() if possible  */
		/* here put char to egi_data_txt->txt */
		for(i=0;i<nread;i++)
		{
			//EGI_PDEBUG(DBG_TXT,"buf[%d]='%c' ascii=%d\n",i,buf[i],buf[i]);
			//printf("i=%d of nread=%d\n",i,nread);
			/*  ------ 1. if it's a return code */
			/* TODO: substitue buf[i] with space ..... */
			if( buf[i]==10 )
			{
				//EGI_PDEBUG(DBG_TXT," ------get a return \n");
				/* if return code is the first char of a line, ignore it then. */
			/*	if(nt != 0)
					nlw +=1; //new line
			*/
				nlw += 1; /* return to next line anyway */
				nt=0;ncount=0; /*reset one line char counter and pixel counter*/
#if 0 /* move to the last block of for() loop */
				/* MAX. number of lines = nl */
				if(nlw>nl-1) /* no more line for txt ebox */
					break;//return ret; /* abort the job */
				continue;
#endif
			}

			/* ----- 2. if symbol code out of range */
			else if( buf[i] > maxnum ) {
				printf("%s:symbol/font/assic code number out of range.\n",__func__);
				continue;
			}

			/* ----- 3. check available pixel space for current line
			   Max. pixel number per line = bxwidth 	*/
			else if( symwidth[ (int)buf[i] ] > bxwidth-ncount )
			{
				nlw +=1; /* new line */
				nt=0;ncount=0; /*reset line char counter and pixel counter*/
				/*else, retry then with the new empty line */
				i--;
#if 0 /* move to the last block of for() loop */
				/* MAX. number of lines = nl */
				if(nlw>nl-1) /* no more line for txt ebox */
					break;//return ret; /* abort the job */
				continue;
#endif
			}
			/* ----- 4. OK, now push a char to txt[][] */
			else
			{
				ncount+=symwidth[ (int)buf[i] ]; /*increase total bumber of pixels for current txt line*/
				//EGI_PDEBUG(DBG_TXT,"one line pixel counter: ncount=%d\n",ncount);
				txt[nlw][nt]=buf[i];
				nt++;

				/* ----- 5. check remained space
				 * check Max. char number per line =llen
				 */
				if( nt > llen-1 )  { /* txt buf end */
					nlw +=1;     /* new line */
					nt=0;ncount=0; /*reset one line char counter and pixel counter*/
				}

			}
			/* -----  5. check whether total number of lines used up  ------- */
			if(nlw>nl-1) { /* no more line for txt ebox */
				//ret+=i+1; /* count total chars, plus pushed in this for() */
				//printf("last push txt i=%d of one read nread=%d, total ret=%d \n",i,nread,ret);
				break; /* end for() session */
			}
			/* if else, go on for() to push next char */

		}/* END for() */


		/* check if txt line is used up, end this file-read session */
		if(nlw>nl-1) {
			ret+=i+1;
			break; /* end while() of fread */
		}
		/* or if just finish pushing nread (may not be sizeof(buf)) to txt */
		else {
			ret+=nread;
		}

	} /* END while() of fread */

	/* DEBUG, print out all txt in txt data buf */
#if 0
	for(i=0;i<nl;i++)
		printf("%s\n",txt[i]);
#endif

	/* renew data_txt->count, current count of chars loaded to txt[]  */
	data_txt->count=ret;

	/* If reach end of file, renew foff to end position, we'll verify it at begin of this func */
	if(feof(fil)) {
		//printf("---------------end: END OF FILE ----------------\n");
		/* push foff and set count<0, to avoid endless pushing.  */
		egi_filo_push(data_txt->filo_off, &foff);
		data_txt->count=-1;

		ret=0; /* end of file */
	}
	/* If read BACKWARD, save curretn position to data_txt->foff */
	else if( data_txt->forward<0 ) {
		((EGI_DATA_TXT *)(ebox->egi_data))->foff = foff+ret;
	}
	/* IF read FORWARD, renew data_txt->foff for NEXT read */
	else if( data_txt->forward>0 ) {
		/* push open_time txt position first, */
		egi_filo_push(data_txt->filo_off,&foff);
		/* Renew foff for NEXT read */
		((EGI_DATA_TXT *)(ebox->egi_data))->foff +=ret;
	}

	fclose(fil);

	/* set need_refresh */
	ebox->need_refresh=true;

	return ret;
}


/*--------------------------------------------------
	release EGI_DATA_TXT
---------------------------------------------------*/
void egi_free_data_txt(EGI_DATA_TXT *data_txt)
{
	if(data_txt == NULL)
		return;

	int i;
	int nl=data_txt->nl;

	if(data_txt->filo_off !=NULL ) {
		egi_free_filo(data_txt->filo_off);
		data_txt->filo_off=NULL;
	}

	if( data_txt->txt != NULL)
	{
		for(i=0;i<nl;i++)
		{
			if(data_txt->txt[i] != NULL)
			{
				free(data_txt->txt[i]);
				data_txt->txt[i]=NULL;
			}
		}
		free(data_txt->txt);
		data_txt->txt=NULL;
	}

	/* data_txt->utxt is referred from elsewhere. Do not free it here! */
	data_txt->utxt=NULL;

	free(data_txt);
	data_txt=NULL;
}

#if 0
/*--------------------------------------------------
txtbox decorationg function

---------------------------------------------------*/
static int egi_txtbox_decorate(EGI_EBOX *ebox)
{
	int ret=0;

	EGI_PDEBUG(DBG_TXT,"egi_txtbox_decorate(): start to show_jpg()...\n");
	ret=show_jpg("/tmp/openwrt.jpg", &gv_fb_dev, SHOW_BLACK_NOTRANSP, ebox->x0+2, ebox->y0+2); /* blackoff<0, show black */

	return ret;
}

#endif


/*------------------------------------------------
	(For non_FTsymbol )
 set string for first line of data_txt->txt[0]
-------------------------------------------------*/
void egi_txtbox_settitle(EGI_EBOX *ebox, char *title)
{
        /* 1. check data */
        if( ebox==NULL || ebox->type != type_txt )
        {
                printf("%s: ebox=NULL or not a txt type ebox! fail to set title!\n",__func__);
                return;
        }

	EGI_DATA_TXT *data_txt=(EGI_DATA_TXT *)(ebox->egi_data);

	if( data_txt == NULL || data_txt->txt[0]==NULL)
        {
                printf("%s: data_txt=NULL or data_txt->txt[0]=NULL! fail to set title!\n",__func__);
                return;
        }

	/* 2. clear data */
	int llen=data_txt->llen;
	memset(data_txt->txt[0],0,llen);

	/* 3. set title string */
	strncpy(data_txt->txt[0],title,llen-1);

	/* 4. set need_refresh */
	ebox->need_refresh=true;
}



/*--------------------------------------------------------------------
push txt to ebox->data_txt->txt[]	( For non_FTsymbols )

data_txt:	the target txt data
buf:		the source buffer
*pnl:		return number of lines used for written, ignore if NULL.
		<=nl, OK.
		>nl, not enough space for char *buf.

1. llen=data_txt->llen-1  one byte for string end /0.
2. limit conditions for line return:
        2.1 Max. char number per line   =  llen;
        2.2 Max. pixel number per line  =  bxwidth

Return:
	>0	bytes of char pushed
	<0	fails
--------------------------------------------------------------------*/
int egi_push_datatxt(EGI_EBOX *ebox, char *buf, int *pnl)
{
	/* check ebox type first */
	if(ebox->type != type_txt)
	{
		printf("egi_push_datatxt(): ebox is not of type_txt, fail to push data_txt.\n");
		return 0;
	}

	int i;
	EGI_DATA_TXT *data_txt=(EGI_DATA_TXT *)(ebox->egi_data);
	int bxwidth=ebox->width; /* in pixel, ebox width for txt  */
	int offx=data_txt->offx;
	char **txt=data_txt->txt;
	int nt=0; 				/* index, txt[][nt] */
	int nl=data_txt->nl; 			/* total number of txt lines */
	int nlw=0; 				/* current written line index, txt[nlw][] */
	int llen=data_txt->llen -1; 		/*in bytes(chars), length for each line, one byte for /0 */
	int ncount=0; 				/*in pixel, counter for used pixels per line, MAX=bxwidth.*/
	int *symwidth=data_txt->font->symwidth;	/* width list for each char code */
//	int symheight=data_txt->font->symheight;
	int maxnum=data_txt->font->maxnum;
	int nread=strlen(buf); /* total bytes of chars */

	/* clear data */
	for(i=0;i<nl;i++)
		memset(txt[i],0,data_txt->llen); /* dont use llen, here llen=data_txt->llen-1 */

	/* put char to data_txt->txt one by one*/
	for(i=0;i<nread;i++)
	{
		/*  ------ 1. if it's a return code */
		/* TODO: substitue buf[i] with space ..... */
		if( buf[i]==10 )
		{
			//EGI_PDEBUG(DBG_TXT," ------get a return \n");
			nlw += 1; /* return to next line anyway */
			nt=0;ncount=0; /*reset one line char counter and pixel counter*/

		}
		/* ----- 2. if symbol code out of range */
		else if( (uint8_t)buf[i] > maxnum )
		{
			//printf("egi_txtbox_readfile():symbol/font/assic code number out of range.\n");
			continue;
		}
		/* ----- 3. check available pixel space for current line
		   Max. pixel number per line = bxwidth 	*/
		else if( symwidth[ (uint8_t)buf[i] ] > (bxwidth-ncount - 2*offx) )
		{
			nlw +=1; /* new line */
			nt=0;ncount=0; /*reset line char counter and pixel counter*/
			/*else, retry then with the new empty line */
			i--;

		}
		/* ----- 4. OK, now push a char to txt[][] */
		else
		{
			ncount+=symwidth[ (uint8_t)buf[i] ]; /*increase total number of pixels for current txt line*/
			txt[nlw][nt]=buf[i];
			//printf("egi_push_datatxt(): buf[%d]='%c'. \n", i, (char)buf[i]);
			nt++;

			/* ----- . check remained space
			check Max. char number per line =llen
				*/
			if( nt > llen-1 ) /* txt buf end */
			{
				nlw +=1; /* new line */
				nt=0;ncount=0; /*reset one line char counter and pixel counter*/
			}

		}

		/* -----  5. finally,check whether total number of data_txt lines used up  ------- */
		if(nlw>nl-1) /* no more line for txt ebox */
		{
			//ret+=i+1; /* count total chars, plus pushed in this for() */
			//printf("egi_push_datatxt(): push i=%d of total nread=%d bytes of txt.\n", i, nread);
			break; /* end for() session */
		}
		/* if else, go on for() to push next char */

	}/* END for() */

	//printf("egi_push_datatxt(): finish pushing %d of total %d bytes of txt.\n", i, nread);

	/* feed back number of lined used */
	if(pnl != NULL)
		*pnl=nlw+1; /*  nlw  index from 0  */

	return i;
}

/*-------------------------------------------------------
set txt_file read/scroll direction.   ( For non_FTsymbol)
direct
	>0:	forward
	=0:	stop
	<0:	backward

Return
	0	OK
	<0	Fails
-------------------------------------------------------*/
int egi_txtbox_set_direct(EGI_EBOX *ebox, int direct)
{
	long pt=0;

	if(ebox==NULL) {
		printf("%s: input ebox is NULL.\n",__func__);
		return -1;
	}

	EGI_DATA_TXT *data_txt=(EGI_DATA_TXT *)(ebox->egi_data);
	if(data_txt==NULL) {
		printf("%s: input ebox->data_txt is NULL.\n",__func__);
		return -2;
	} /* if data_txt is OK, we are sure that data_txt->filo_off is also OK */

	/* if change from FORWARD to BACKWARD:
	 * pop out an foff value to avoid refreshing the same txt buff twice */
	if( data_txt->forward>0 && direct<0 && data_txt->foff>0 ) {
		egi_filo_pop(data_txt->filo_off, NULL);
	}
	/* if change form BACKWARD to FORWARD:
	 *  MUST push current foff again!!!, otherwise current foff will be lost in data_txt filo!  
         *  So we keep all foffs in filo, they are continuous indexes for file displaying.
 	 */
	else if (data_txt->forward<0 && direct>0 ) {
		pt=data_txt->foff - data_txt->count; /* For foff is pointed to the end of txt page */
		egi_filo_push(data_txt->filo_off, &pt);
	}

	data_txt->forward=direct;

	return 0;
}
