/*----------------------------------------------------------------------------
embedded graphic interface based on frame buffer, 16bit color tft-LCD.

Very simple concept:
0. Basic philosophy: Loop and Roll-back.
1. The basic elements of egi objects are egi_element boxes(ebox).
2. All types of EGI objects are inherited from basic eboxes. so later it will be
   easy to orgnize and manage them.
3. Only one egi_page is active on the screen.
4. An active egi_page occupys the whole screen.
5. An egi_page hosts several type of egi_ebox, such as type_txt,type_button,...etc.
6. First init egi_data_xxx for different type, then init the egi_element_box with it.

7. some ideas about egi_page ....
  7.0 Life-circle of a page:
      create a page ---> activate the page ---> run page routine ---> exit the routine ---> free the page.
  7.1 Home egi_page (wallpaper, app buttons, head informations ...etc.)
  7.2 Current active egi_page (current running/displaying egi_page, accept all pad-touch events)
  7.3 Back Running egi_page (running in back groud, no pad-touch reaction. )
  7.4 A egi page

TODO:
	0. egi_txtbox_filltxt(),fill txt buffer of txt_data->txt.
	1. different symbol types in a txt_data......
	2. egi_init_data_txt(): llen according to ebox->height and width.---not necessary if multiline auto. 		   adjusting.
	3. To read FBDE vinfo to get all screen/fb parameters as in fblines.c, it's improper in other source files.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h> /*malloc*/
#include <string.h> /*memset*/
#include <unistd.h> /*usleep*/
#include "egi.h"
#include "egi_timer.h"
#include "egi_txt.h"
#include "egi_btn.h"
#include "egi_list.h"
#include "egi_pic.h"
#include "egi_slider.h"
#include "egi_debug.h"
#include "sys_list.h"
#include "egi_symbol.h"
#include "egi_color.h"

/* button touch status:
 * corresponding to enum egi_touch_status in egi.h
 */
static const char *str_touch_status[]=
{
        "unkown",               /* during reading or fails */
        "releasing",            /* status transforming from pressed_hold to released_hold */
        "pressing",             /* status transforming from released_hold to pressed_hold */
        "released_hold",
        "pressed_hold",
        "db_releasing",         /* double click, the last releasing */
        "db_pressing",         /* double click, the last pressing */
	"undefined",
};

/*	convert touch status to string 	 */
const char *egi_str_touch_status(enum egi_touch_status touch_status)
{
	enum egi_touch_status status_unkown=unkown;
	enum egi_touch_status status_undefined=undefined;

	if( touch_status >=status_unkown && touch_status < status_undefined )
		return str_touch_status[touch_status];
	else
		return str_touch_status[status_undefined];
}


/*---------------------------------------
return a random value not great than max

Example:
egi_random_max(5):  1,2,3,4,5
egi_random_max(-5): -3,-2,-1,0,1

max>0: 	1<= ret <=max
max=0:  1
max<0:  max+2 <= ret <=1

---------------------------------------*/
int egi_random_max(int max)
{
        int ret;
        struct timeval tmval;

        gettimeofday(&tmval,NULL);
        srand(tmval.tv_usec);
        ret = 1+(int)((float)max*rand()/(RAND_MAX+1.0));
        //printf("random max ret=%d\n",ret);

        return ret;
}


/*---------------------------------------------------------------------
allocate memory for ebox->bkimg with specified H and W, in 16bit color.
!!! height/width and ebox->height/width MAY NOT be the same.

ebox:	pointer to an ebox
height: height of an image.
width: width of an image.

Return:
	pointer to the mem.	OK
	NULL			fail
------ --------------------------------------------------------------*/
void *egi_alloc_bkimg(EGI_EBOX *ebox, int width, int height)
{
	/* 1. check data */
	if( height<=0 || width <=0 )
	{
		printf("egi_alloc_bkimg(): ebox height(%d) or width(%d) is <=0!\n",height,width);
		return NULL;
	}
	/* 2. malloc exbo->bkimg for bk image storing */
	if( ebox->bkimg != NULL)
	{
		printf("egi_alloc_bkimg(): ebox->bkimg is not NULL, fail to malloc!\n");
		return NULL;
	}
	/* 3. alloc memory */
	EGI_PDEBUG(DBG_EGI,"egi_alloc_bkimg(): start to malloc() ....!\n");
	ebox->bkimg=malloc(height*width*sizeof(uint16_t));

	if(ebox->bkimg == NULL)
	{
		printf("egi_alloc_bkimg(): fail to malloc for ebox->bkimg!\n");
		return NULL;
	}
	EGI_PDEBUG(DBG_EGI,"egi_alloc_bkimg(): finish!\n");

	return ebox->bkimg;
}


/*-------------------------------------------------------------------------
re_allocate memory for ebox->bkimg with specified H and W, in 16bit color.

NOTE:
1. "The contents will be unchanged in the range from the start of the region
up to the minimum of the old and new sizes.  If the new size is larger than
the old size, the added memory will not be initialized."  - man realloc

2. If it fails, the original mem of ebox->bkimg will not be changed or moved.

ebox:	pointer to an ebox
height: height of an image.
width: width of an image.

Return:
	pointer to the mem.	OK
	NULL			fail
-------------------------------------------------------------------------*/
void *egi_realloc_bkimg(EGI_EBOX *ebox, int width, int height)
{
	/* 1. check data */
	if( height<=0 || width <=0 )
	{
		printf("egi_realloc_bkimg(): ebox height(%d) or width(%d) is <=0!\n",height,width);
		return NULL;
	}
	/* 2. reallocate memory */
	EGI_PDEBUG(DBG_EGI,"egi_realloc_bkimg(): start to realloc() ....!\n");
	ebox->bkimg=realloc(ebox->bkimg, height*width*sizeof(uint16_t));
	if(ebox->bkimg == NULL)
	{
		printf("egi_realloc_bkimg(): fail to realloc for ebox->bkimg!\n");
		return NULL;
	}
	EGI_PDEBUG(DBG_EGI,"egi_realloc_bkimg(): finish!\n");

	return ebox->bkimg;
}



/*---------- obselete!!!, substitued by egi_getindex_ebox() now!!! ----------------
  check if (px,py) in the ebox
  return true or false
  !!!--- ebox local coordinate original is NOT sensitive in this function ---!!!
--------------------------------------------------------------------------------*/
bool egi_point_inbox(int px,int py, EGI_EBOX *ebox)
{
        int xl,xh,yl,yh;
        int x1=ebox->x0;
	int y1=ebox->y0;
        int x2=x1+ebox->width;
	int y2=y1+ebox->height;

        if(x1>=x2){
                xh=x1;xl=x2;
        }
        else {
                xl=x1;xh=x2;
        }

        if(y1>=y2){
                yh=y1;yl=y2;
        }
        else {
                yh=y2;yl=y1;
        }

        if( (px>=xl && px<=xh) && (py>=yl && py<=yh))
                return true;
        else
                return false;
}


/*------------------------------------------------------------------
1. find the ebox index according to given x,y
2. a sleeping ebox will be ignored.

x,y:   point at request
ebox:  ebox pointer
num:   total number of eboxes referred by *ebox.

return:
	>=0   Ok, as ebox pointer index
	<0    not in eboxes
-------------------------------------------------------------------*/
int egi_get_boxindex(int x,int y, EGI_EBOX *ebox, int num)
{
	int i=num;

	/* Now we only consider button ebox*/
	if(ebox->type==type_btn)
	{
		for(i=0;i<num;i++)
		{
			if(ebox[i].status==status_sleep)continue; /* ignore sleeping ebox */

			if( x>=ebox[i].x0 && x<=ebox[i].x0+ebox[i].width \
				&& y>=ebox[i].y0 && y<=ebox[i].y0+ebox[i].height )
				return i;
		}
	}

	return -1;
}


/*-----------------------------------------------------------------------------
1. In a page, find the ebox index according to given x,y
2. A sleeping/hidden ebox will be ignored.
3. Type may be multiple, like: type_txt|type_slider|type_btn ...etc.

@x,y:   Point under request, under default coordinate system!!!

	!!! TBD&TODO: Current ONLY for default gv_fb_dev coord. mapping !!!

@page:  An egi page containing eboxes

return:
	pointer to an ebox  	Ok
	NULL			fail or no ebox is hit.
------------------------------------------------------------------------------*/
EGI_EBOX *egi_hit_pagebox(int px, int py, EGI_PAGE *page, enum egi_ebox_type type)
{
	int x=0,y=0;
	struct list_head *tnode;
	int xres= gv_fb_dev.vinfo.xres;
	int yres= gv_fb_dev.vinfo.yres;

	EGI_EBOX *ebox;
	EGI_POINT *sxy;
	EGI_POINT *exy;

        /* check page */
        if(page==NULL)
        {
                printf("egi_get_pagebtn(): page is NULL!\n");
                return NULL;
        }

        /* check list */
        if(list_empty(&page->list_head))
        {
                printf("egi_get_pagebtn(): page '%s' has no child ebox.\n",page->ebox->tag);
                return NULL;
        }

        /* check FB.pos_rotate
	 * Map default touch coordinates to current pos_rotate coordinates.
         */
        switch(gv_fb_dev.pos_rotate) {
                case 0:                 /* FB defaul position */
                        x=px;
                        y=py;
                        break;
                case 1:                 /* Clockwise 90 deg */
			x=py;
                        y=(xres-1)-px;
                        break;
                case 2:                 /* Clockwise 180 deg */
			x=(xres-1)-px;
			y=(yres-1)-py;
                        break;
                case 3:                 /* Clockwise 270 deg */
			x=(yres-1)-py;
			y=px;
                        break;
        }


        /* traverse the list, not safe */
        list_for_each(tnode, &page->list_head)
        {
                ebox=list_entry(tnode, EGI_EBOX, node);
                //EGI_PDEBUG(DBG_EGI,"egi_get_pagebtn(): find child --- ebox: '%s' --- \n",ebox->tag);

		sxy=&(ebox->touchbox.startxy);
		exy=&(ebox->touchbox.endxy);

		if(ebox->type & type)
		{
			/* ignore sleeping and hidden ebox */
	        	 if(ebox->status==status_sleep || ebox->status==status_hidden )
				continue;

			 /* check whether the ebox is hit */
			 if( sxy->x==0 && exy->x==0 ) {    /* 1. If touch box NOT defined */
	        	         if( x > ebox->x0 && x < ebox->x0+ebox->width \
                	                	&& y > ebox->y0 && y < ebox->y0+ebox->height )
	                  	return ebox;
			 }
			 else {    			   /* 2. If touch box defined */
				if( x > sxy->x && x < exy->x  && y > sxy->y && y < exy->y )
				  return ebox;
			 }
		}
	}

	return NULL;
}




/*------------------------------------------------
OBSOLETE!!!
all eboxes to be public !!!
return ebox status
-------------------------------------------------*/
enum egi_ebox_status egi_get_ebox_status(const EGI_EBOX *ebox)
{
	return ebox->status;
}







///xxxxxxxxxxxxxxxxxxxxxxxxxx(((   OK   )))xxxxxxxxxxxxxxxxxxxxxxxxx

/*------------------------------------------------------------
put tag for an ebox
--------------------------------------------------------------*/
inline void egi_ebox_settag(EGI_EBOX *ebox, const char *tag)
{
	/* 1. clear tag */
	memset(ebox->tag,0,EGI_TAG_LENGTH+1);

	/* 2. check data */
	if(ebox == NULL)
	{
		printf("egi_ebox_settag(): EGI_EBOX *ebox is NULL, fail to set tag.\n");
		return;
	}

	/* 3. set NULL */
	if(tag == NULL)
	{
		// OK, set NULL, printf("egi_ebox_settag(): char *tag is NULL, fail to set tag.\n");
		return;
	}

	/* 4. copy string to tag */
	strncpy(ebox->tag,tag,EGI_TAG_LENGTH);
}


/*-------------------------------------------------------------------
put need_refresh flag true

!!! WARNING: The Caller must alert to avoid PAGE.pgmutex deadlock !!!

-------------------------------------------------------------------*/
inline void egi_ebox_needrefresh(EGI_EBOX *ebox)
{
    	if(ebox == NULL)
		return;

       /* Get pgmutex for parent PAGE resource access/synch */
       if(ebox->container != NULL) {
	        if(pthread_mutex_lock(&(ebox->container)->pgmutex) !=0) {
        	        printf("%s: Fail to get PAGE.pgmutex!\n",__func__);
                	return;
        	}
	}

	/*  Race condition may still exist? */
	ebox->need_refresh=true;

       if(ebox->container != NULL)
		pthread_mutex_unlock(&(ebox->container)->pgmutex);

}


/*---------------------------------------
	set touch area
---------------------------------------*/
inline void egi_ebox_set_touchbox(EGI_EBOX *ebox, EGI_BOX box)
{
    if(ebox != NULL)
	ebox->touchbox=box;
}


/*----------------------------------------------------
ebox refresh: default method

reutrn:
	1	use default method
	0	OK
	<0	fail
------------------------------------------------------*/
int egi_ebox_refresh(EGI_EBOX *ebox)
{
	/* 1. check data */
	if( ebox==NULL || ebox->egi_data==NULL )
        {
                printf("egi_ebox_refresh(): input ebox is invalid or NULL!\n");
                return -1;
        }
	/* 2. put default methods here ...*/
	if(ebox->method.refresh == NULL)
	{
		EGI_PDEBUG(DBG_EGI,"ebox '%s' has no defined method of refresh()!\n",ebox->tag);
		return 1;
	}
	/* 3. ebox object defined method */
	else
	{
		return ebox->method.refresh(ebox); /*only if need_refresh flag is true */
	}

}


/*----------- NOT GOOD! ---------------
	Force an ebox to refresh

----------------------------------------*/
int egi_ebox_forcerefresh(EGI_EBOX *ebox)
{
	int ret;

	if(ebox==NULL)
		return -1;

       /* Get pgmutex for parent PAGE resource access/synch */
       if(ebox->container != NULL) {
                if(pthread_mutex_lock(&(ebox->container)->pgmutex) !=0) {
                        printf("%s: Fail to get PAGE.pgmutex!\n",__func__);
                        return -2;
                }
        }

	ebox->need_refresh=true;
        if(egi_ebox_refresh(ebox)!=0)
		ret=-3;

	/* Put parent PAGE.pgmutex */
	if(ebox->container != NULL)
       		pthread_mutex_unlock(&(ebox->container)->pgmutex);


	return ret;
}


/*----------------------------------------------------
ebox activate: default method

reutrn:
	1	use default method
	0	OK
	<0	fail
------------------------------------------------------*/
int egi_ebox_activate(EGI_EBOX *ebox)
{
	/* 1. put default methods here ...*/
	if(ebox->method.activate == NULL)
	{
		EGI_PDEBUG(DBG_EGI,"ebox '%s' has no defined method of activate()!\n",ebox->tag);
		return 1;
	}

	/* 2. ebox object defined method */
	else
		return ebox->method.activate(ebox);
}

/*----------------------------------------------------
ebox refresh: default method

reutrn:
	1	use default method
	0	OK
	<0	fail
------------------------------------------------------*/
int egi_ebox_sleep(EGI_EBOX *ebox)
{
	/* 1. put default methods here ...*/
	if(ebox->method.sleep == NULL)
	{
		EGI_PDEBUG(DBG_EGI,"ebox '%s' has no defined method of sleep()!\n",ebox->tag);
		return 1;
	}

	/* 2. ebox object defined method */
	else
		return ebox->method.sleep(ebox);
}

/*----------------------------------------------------
ebox decorate: default method

reutrn:
	1	use default method
	0	OK
	<0	fail
------------------------------------------------------*/
int egi_ebox_decorate(EGI_EBOX *ebox)
{
	/* 1. put default methods here ...*/
	if(ebox->method.decorate == NULL)
	{
		EGI_PDEBUG(DBG_EGI,"ebox '%s' has no defined method of decorate()!\n",ebox->tag);
		return 1;
	}

	/* 2. ebox object defined method */
	else
	{
		EGI_PDEBUG(DBG_EGI,"egi_ebox_decorate(): start ebox->method.decorate(ebox)...\n");
		return ebox->method.decorate(ebox);
	}
}


/*----------------------------------------------------
ebox refresh: default method

reutrn:
	1	use default method
	0	OK
	<0	fail
------------------------------------------------------*/
int egi_ebox_free(EGI_EBOX *ebox)
{
	/* check ebox */
	if(ebox == NULL)
	{
		printf("egi_ebox_free(): pointer ebox is NULL! fail to free it.\n");
		return -1;
	}

	/* 1. put default methods here ...*/
	if(ebox->method.free == NULL)
	{
		EGI_PDEBUG(DBG_EGI,"ebox '%s' has no defined method of free(), now use default to free it ...!\n",ebox->tag);

		/* 1.1 free ebox tyep data */
		switch(ebox->type)
		{
			case type_txt:
				if(ebox->egi_data != NULL)
				{
					EGI_PDEBUG(DBG_EGI,"egi_ebox_free():start to egi_free_data_txt(ebox->egi_data)  \
						 for '%s' ebox\n", ebox->tag);
					egi_free_data_txt(ebox->egi_data);
				 }
				break;
			case type_btn:
				if(ebox->egi_data != NULL)
				{
					EGI_PDEBUG(DBG_EGI,"egi_ebox_free():start to egi_free_data_btn(ebox->egi_data)  \
						 for '%s' ebox\n", ebox->tag);
					egi_free_data_btn(ebox->egi_data);
				}
				break;
			case type_list:
				if(ebox->egi_data != NULL)
				{
					EGI_PDEBUG(DBG_EGI,"egi_ebox_free():start to egi_free_date_list(ebox->egi_data) \
						 for '%s' ebox\n", ebox->tag);
					egi_free_data_list(ebox->egi_data);
				}
				break;
			case type_pic:
				if(ebox->egi_data != NULL)
				{
					EGI_PDEBUG(DBG_EGI,"egi_ebox_free():start to egi_free_date_pic(ebox->egi_data) \
						 for '%s' ebox\n", ebox->tag);
					egi_free_data_pic(ebox->egi_data);
				}
				break;
			case type_slider:
				if(ebox->egi_data != NULL)
				{
					EGI_PDEBUG(DBG_EGI,"egi_ebox_free():start to egi_free_data_slider(ebox->egi_data) \
						 for '%s' ebox\n", ebox->tag);
					egi_free_data_slider(ebox->egi_data);
				}
				break;
			case type_page:
				/* The PAGE has its own free method, however it will call egi_ebox_free()
				 * to release/free member page->ebox.
				 */
				break;

			default:
				printf("egi_ebox_free(): ebox '%s' type %d has not been created yet!\n",
										ebox->tag,ebox->type);
				return -2;
				break;
		}

		/* 1.2 free frame_img */
		if(ebox->frame_img != NULL) {
			egi_imgbuf_free(ebox->frame_img);
			ebox->frame_img=NULL;
		}

		/* 1.3 free ebox bkimg */
		if(ebox->bkimg != NULL) {
			free(ebox->bkimg);
			ebox->bkimg=NULL;
		}

		/* 1.4 free concept ebox */
		free(ebox);
		ebox=NULL;  /* ineffective though...*/

		return 0;
	}

	/* 2. else, use ebox object defined method */
	else
	{
		printf("use ebox '%s' selfe_defined free method to free it ...!\n",ebox->tag);
		return ebox->method.free(ebox);
	}
}


/*-------------------------------------------------------------------------------------
create a new ebox according to its type and parameters
WARNING: Memory for egi_data NOT allocated her, it's caller's job to allocate
	and assign it to the new ebox. they'll be freed all together in egi_ebox_free().

return:
	NULL		fail
	pointer		OK
-------------------------------------------------------------------------------------*/
EGI_EBOX * egi_ebox_new(enum egi_ebox_type type)  //, void *egi_data)
{
	/* malloc ebox */
	EGI_PDEBUG(DBG_EGI,"egi_ebox_new(): start to malloc for a new ebox....\n");
	EGI_EBOX *ebox=malloc(sizeof(EGI_EBOX));

	if(ebox==NULL)
	{
		printf("egi_ebox_new(): fail to malloc for a new ebox!\n");
		return NULL;
	}
	memset(ebox,0,sizeof(EGI_EBOX)); /* clear data */

	ebox->type=type;

	/* assign default methods for new ebox */
	EGI_PDEBUG(DBG_EGI,"egi_ebox_new(): assing default method to ebox ....\n");
	ebox->activate=egi_ebox_activate;
	ebox->refresh=egi_ebox_refresh;
	ebox->decorate=egi_ebox_decorate;
	ebox->sleep=egi_ebox_sleep;
	ebox->free=egi_ebox_free;

	/* other params are as default, after memset with 0:
	 * inmovable
	 * status=no_body
	 * frame=simple type
	 * prmcolor=BLACK
	 * bkimg=NULL
	 */

	EGI_PDEBUG(DBG_EGI,"egi_ebox_new(): end the call. \n");

	return ebox;
}


/*------------------------------------------------------------------
disappearing effect for a full egi page, zoom out the page image to the top
left point

return:
	0		OK
	<0		fail
--------------------------------------------------------------------*/
int egi_page_disappear(EGI_EBOX *ebox)
{
        int wid,hgt;
	int bkwid,bkhgt; /* wid and hgt for backup */
	int screensize=gv_fb_dev.screensize;
	int xres=gv_fb_dev.vinfo.xres;
	int yres=gv_fb_dev.vinfo.yres;
	uint16_t *buf;
	uint16_t *sbuf; /* for scaled image */
	uint16_t *bkimg; /* bkimg for scaled area */

	/* check ebox is a full egi page */

	/* malloc buf */
	buf=malloc(screensize);
	if(buf==NULL)
	{
		printf("egi_page_disappear(): fail to malloc buf.\n");
		return -1;
	}
	sbuf=malloc(screensize);
	if(sbuf==NULL)
	{
		printf("egi_page_disappear(): fail to malloc sbuf.\n");
		return -2;
	}
	bkimg=malloc(screensize);
	if(bkimg==NULL)
	{
		printf("egi_page_disappear(): fail to malloc bkimg.\n");
		return -3;
	}

        /* 1. grap current page image */
        printf("start fb_cpyto_buf\n");
        fb_cpyto_buf(&gv_fb_dev, 0, 0, xres-1, yres-1, buf);
        printf("start for...\n");

	/* 2. restore original page image */
    	fb_cpyfrom_buf(&gv_fb_dev,ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
				ebox->bkbox.endxy.x,ebox->bkbox.endxy.y,ebox->bkimg);
        fb_cpyto_buf(&gv_fb_dev,0,0,xres-1,yres-1,bkimg);
	bkwid=xres;bkhgt=yres;

        /* 3. zoom out to left top point */
        for(wid=xres*3/4;wid>0;wid-=4)
        {
		/* 2.1 scale the image */
                hgt=wid*yres/xres; //4/3;
                fb_scale_pixbuf(xres,yres,wid,hgt,buf,sbuf);
                /* 2.2 restore ebox bk image */
                fb_cpyfrom_buf(&gv_fb_dev,0,0,bkwid-1,bkhgt-1,bkimg);
           //     fb_cpyfrom_buf(&gv_fb_dev,ebox->bkbox.startxy.x,ebox->bkbox.startxy.y,
	//					ebox->bkbox.endxy.x,ebox->bkbox.endxy.y,ebox->bkimg);
                /* 2.3 store the area which will be replaced by scaled image */
                fb_cpyto_buf(&gv_fb_dev,0,0,wid-1,hgt-1,bkimg);
		bkwid=wid;bkhgt=hgt;
                /* 2.3 put scaled image */
                fb_cpyfrom_buf(&gv_fb_dev,0,0,wid-1,hgt-1,sbuf);
//		usleep(100000);
        }

        /* 3. */
	free(buf);
	free(sbuf);

	return 0;
}

#if 0 /////////////////////////////////////////////////////////////////
/*------------------------------------------------------------------
Duplicate a new ebox and its egi_data from an input ebox.

return:
	EGI_EBOX pointer	OK
	NULL			fail
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


/*--------------------------------------------------------------------
Set icon substitue color for a button ebox

Note:
    Pure BLACK set as transparent color, so here black subcolor should
    not be 0 !!!

return:
	0	OK
	<0	fail
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
#endif ///////////////////////////////
