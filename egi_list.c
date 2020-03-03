/*----------------------- egi_list.c -------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


1. a list ebox consists of item txt_eboxes set in egi_data.
2. Icon of a list item is drawed by calling its txt_eboxe decorate function.


TODO:
1. Color set for each line of txt to be fixed later in egi_txt.c.

Midas Zhou
---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "egi_color.h"
#include "egi.h"
#include "egi_txt.h"
#include "egi_objtxt.h"
#include "egi_timer.h"
#include "egi_debug.h"
//#include "egi_timer.h"
#include "egi_symbol.h"
#include "egi_list.h"


/* decoration function */
static int egi_itembox_decorate(EGI_EBOX *ebox);


/*-------------------------------------
      list ebox self_defined methods
--------------------------------------*/
static EGI_METHOD listbox_method=
{
        .activate=egi_listbox_activate,
        .refresh=egi_listbox_refresh,
        .decorate=NULL,
        .sleep=NULL,
        .free=NULL, /* to see egi_ebox_free() in egi.c */
};



/*-------------------------------------------------------------
Create an list ebox.
standard tag "list"

default ebox prmcolor: 	WEGI_COLOR_GRAY
default font color: 	WEGI_COLOR_BLACK

NOTE:
    1.  THe hosting ebox is movable, while item txt_eboxes
	are fixed. If items are movable,  their images
	will be messed up during sliding show refresh.
	here movable/fixed means that ebox's bkimag is ON/OFF.

return:
	txt ebox pointer 	OK
	NULL			fai.
-----------------------------------------------------------------*/
EGI_EBOX *egi_listbox_new (
	int x0, int y0, /* left top point */
        int inum,  	/* item number of a list */
	int nwin,	/* number of items in displaying window */
        int width, 	/* H/W for each list item ebox, W/H of the hosting ebox depends on it */
        int height,
	int frame, 	/* -1 no frame for ebox, 0 simple .. */
	int nl,		/* number of txt lines for each txt ebox */
	int llen,  	/* in byte, length for each txt line */
        EGI_SYMPAGE *font, /* txt font */
        int txtoffx,	 /* offset of txt from the ebox, all the same */
        int txtoffy,
        int iconoffx, 	/* offset of icon from the ebox, all the same */
        int iconoffy
)
{
	int i;


        EGI_EBOX *ebox;

        /* 1. create a new common ebox with type_list */
        EGI_PDEBUG(DBG_LIST,"egi_listbox_new(): start to egi_ebox_new(type_list)...\n");
        ebox=egi_ebox_new(type_list);// egi_data NOT allocated in egi_ebox_new()!!! , (void *)egi_data);
        if(ebox==NULL)
        {
                printf("egi_listbox_new(): fail to execute egi_ebox_new(type_list). \n");
                return NULL;
        }

        /* 2. default methods have been assigned in egi_ebox_new() */

        /* 3. list ebox object method */
        EGI_PDEBUG(DBG_LIST,"egi_listbox_new(): assign defined mehtod ebox->method=methd...\n");
        ebox->method=listbox_method;

        /* 4. fill in elements for main ebox */
        egi_ebox_settag(ebox,"list");

        ebox->x0=x0;
    	ebox->y0=y0;
        ebox->movable=true; /*  however item txt_ebox to be fixed */
        ebox->width=width;
	ebox->height=height*nwin; /* for displaying window height */
        ebox->frame=-1; /* no frame, let item ebox prevail */
	ebox->prmcolor=-1; /* no prmcolor, let item ebox prevail */
	ebox->need_refresh=true; /* the hosting_ebox need not to be refreshed,
	the token should always be true */

	/* to assign ebox->egi_data latter ......*/


	/* 5 malloc data_txt for list item eboxes,
	   Temprary this function only, to be released   */
	EGI_DATA_TXT **data_txt=malloc(inum*sizeof(EGI_DATA_TXT *));
	if( data_txt == NULL )
	{
		printf("egi_listbox_new(): fail to malloc  **data_txt.\n");
		return NULL;
	}
	memset(data_txt,0,inum*sizeof(EGI_DATA_TXT *));
printf("egi_listbox_new(): malloc **data_txt end...\n");

	/* 6. malloc txt_boxes for list item eboxes */
	EGI_EBOX **txt_boxes=malloc(inum*sizeof(EGI_EBOX *));
	if( txt_boxes == NULL )
	{
		printf("egi_listbox_new(): fail to malloc a EGI_EBOX **txt_box.\n");
		return NULL;
	}
	memset(txt_boxes,0,inum*sizeof(EGI_EBOX *));
printf("egi_listbox_new(): malloc txtboxes end...\n");

	/* 7. init **icon */
	EGI_SYMPAGE **icons=malloc(inum*sizeof(EGI_SYMPAGE *));
	if( icons == NULL )
	{
		printf("egi_listbox_new(): fail to malloc a symbol_page **icon.\n");
		return NULL;
	}
	memset(icons,0,inum*sizeof(EGI_SYMPAGE *));
printf("egi_listbox_new(): malloc icons end...\n");

	/* 8. init *icon_code */
	int *icon_code =malloc(inum*sizeof(int));
	if( icon_code == NULL )
	{
		printf("egi_listbox_new(): fail to malloc *icon_code.\n");
		return NULL;
	}
	memset(icon_code,0,inum*sizeof(int));
printf("egi_listbox_new(): malloc icon_code end...\n");


	/* 9. init bkcolor
	   Temprary this function only, to be released   */
	uint16_t *prmcolor=malloc(inum*sizeof(uint16_t));
	/* init prmcolor with default bkcolor */
	memset(prmcolor,0,inum*sizeof(uint16_t));
	for(i=0;i<inum;i++)
		prmcolor[i]=LIST_DEFAULT_BKCOLOR;
printf("egi_listbox_new(): malloc prmcolor end...\n");


	/* 10. init egi_data_list */
	EGI_DATA_LIST *data_list=malloc(sizeof(EGI_DATA_LIST));
	if( data_list == NULL )
	{
		printf("egi_listbox_new(): fail to malloc *data_list.\n");
		return NULL;
	}
	memset(data_list,0,sizeof(EGI_DATA_LIST));
printf("egi_listbox_new(): malloc data_list end...\n");

	/* 11. assign to data_list */
	data_list->inum=inum;
	data_list->nwin=nwin;
	data_list->pw=0; /* default first item in displaying window */
	data_list->txt_boxes=txt_boxes;
	data_list->icons=icons;
	data_list->icon_code=icon_code;
	data_list->iconoffx=iconoffx;
	data_list->iconoffy=iconoffy;

	/* 12. create txt_type eboxes for item txt_ebox->egi_data  */
	printf("egi_listbox_new(): total list item number inum=%d \n",inum);
	for(i=0;i<inum;i++)
	{
		/* 12.1  create a data_txt */
		EGI_PDEBUG(DBG_LIST,"egi_listbox_new(): start to data_txt[%d]=egi_txtdata_new()...\n",i);
		data_txt[i]=egi_txtdata_new(
				txtoffx,txtoffy, /* offset X,Y */
      	 		 	nl, /*int nl, lines  */
       	 			llen, /*int llen, chars per line, however also limited by width */
        			font, /*EGI_SYMPAGE *font */
        			WEGI_COLOR_BLACK /* int16_t color */
			     );
		if(data_txt[i] == NULL) /* if fail retry...*/
		{
			printf("egi_listbox_new(): data_txt[%d]=egi_txtdata_new()=NULL,  retry... \n",i);
			i--;
			continue;
		}
printf("egi_listbox_new(): data_txt[%d]=egi_txtdata_new() end...\n",i);

		/* 12.2  creates all those txt eboxes */
	        EGI_PDEBUG(DBG_LIST,"egi_listbox_new(): start egi_txtbox_new().....\n");
       		(data_list->txt_boxes)[i] = egi_txtbox_new(
                			"----", /* tag, or put later */
		                	data_txt[i], /* EGI_DATA_TXT pointer */
               				false, /* (bool movable) fixed type. to provent refreshing images
						from messing up during sliding show */
			                x0, y0+i*height, /* int x0, int y0 */
                			width, height, /* int width;  int height,which also related with symheight and offy */
                			frame, /* int frame, 0=simple frmae, -1=no frame */
                			prmcolor[i] /*int prmcolor*/
  		    	     );
		if( (data_list->txt_boxes)[i] == NULL )
		{
			printf("egi_listbox_new(): data_list->txt_eboxes[%d]=NULL,  retry... \n",i);
			free(data_txt[i]);
			i--;
			continue;
		}
//printf("egi_listbox_new(): data_list->txt_boxes[%d]=egi_txtbox_new() end...\n",i);

		/* 12.3 set father or hosting ebox */
		data_list->txt_boxes[i]->father = ebox;

		/* 12.4  set decorate functions for draw icons for each item txt_ebox */
		data_list->txt_boxes[i]->decorate=egi_itembox_decorate;

		/* 12.5  set id for data_txt */
		( (EGI_DATA_TXT *)(data_list->txt_boxes[i]->egi_data) )->id = i+1; /* id start from 1 */

		/* 12.6 set-tag */
		sprintf(data_list->txt_boxes[i]->tag,"item_%d",i);
//printf("egi_list_new(): data_list->txt_boxes[%d] set_tag  end...\n",i);

	}

	/* 13. assign list_data to type_list ebox */
	ebox->egi_data=(void *)data_list;

	/* 14. free data_txt */
	free(data_txt);

	/* 15. free prmcolor */
	free(prmcolor);

	return ebox;
}


/*----------------------------------------------
release EGI_DATA_LIST
----------------------------------------------*/
void egi_free_data_list(EGI_DATA_LIST *data_list)
{
	if(data_list==NULL)
		return;

	int i;
 	int inum=data_list->inum;

	/* free txt_boxes */
	for(i=0;i<inum;i++)
	{
		if( data_list->txt_boxes[i] != NULL )
			(data_list->txt_boxes[i])->free(data_list->txt_boxes[i]);
	}
	/* free **txt_boxes */
	free(data_list->txt_boxes);

	/* free icons and icon_code */
	if(data_list->icons != NULL)
		free(data_list->icons);
	if(data_list->icon_code != NULL)
		free(data_list->icon_code);

	/* free data_list */
	free(data_list);
}


/*--------------------------------------------------------------
0.to activate a list, activate items in displaying windows only!!!
1. activate each txt ebox in the list.
2. draw icon for drawing item.


Retrun:
	0	OK
	<0	fail
--------------------------------------------------------------*/
int egi_listbox_activate(EGI_EBOX *ebox)
{
	int i;
	int inum; /* total number of items in the list */
	int nwin; /* number of itmes in displaying window */
	int pw;	/* first item number in the displaying window */
	int itnum; /* tmp item number */

	EGI_DATA_LIST *data_list;

	/* 1. check list  */
	if(ebox==NULL)
	{
		printf("egi_listbox_activate(): input list ebox is NULL! fail to activate.");
		return -1;
	}

	/* 2. check ebox type */
	if(ebox->type != type_list)
	{
		printf("egi_listbox_activate(): input ebox is not list type! fail to activate.");
		return -3;
	}

        /* TODO:  activate(or wake up) a sleeping ebox ( referring to egi_txt.c and egi_btn.c )
         */


	/* 3. check egi_data */
	if(ebox->egi_data==NULL)
	{
		printf("egi_listbox_activate(): input ebox->egi_data is NULL! fail to activate.");
		return -2;
	}

	data_list=(EGI_DATA_LIST *)ebox->egi_data;
	inum=data_list->inum;
	pw=data_list->pw;
	nwin=data_list->nwin;

	/* seet host ebox need_refresh */
	ebox->need_refresh=true;

	/* activate all items */
	for(i=0; i<nwin; i++)
	{
		itnum=pw+i;	/* displaying from first item pw */
		if(itnum >= inum)
			itnum=itnum%inum; /* roll back then */

		EGI_PDEBUG(DBG_LIST,"egi_listbox_activate():----------itnum=%d----------\n",itnum);

		/* 4.1 update position, within displaying window */
		data_list->txt_boxes[itnum]->y0=ebox->y0	\
						+i*(data_list->txt_boxes[itnum]->height);/* update postion */

		/* 4.2 activate it */
		if( egi_txtbox_activate(data_list->txt_boxes[itnum]) < 0)
		{
			printf("egi_listbox_activate(): fail to activate data_list->txt_boxes[%d].\n",itnum);
			return -4;
		}

		/* 4.3 FB write icon */
		if(data_list->icons[itnum])
		{
			symbol_writeFB(&gv_fb_dev, data_list->icons[itnum], SYM_NOSUB_COLOR,
					data_list->icons[itnum]->bkcolor,
 					data_list->txt_boxes[itnum]->x0, data_list->txt_boxes[itnum]->y0,
					data_list->icon_code[itnum], 0 ); /* opaque 0 */
		}
	}

	return 0;
}


/*---------------------------------------------------------------------------------------------
to refresh item txt_eboxes in the displaying window, with item index from pw to pw+nwin-1 only.


1. TODO: update/push data to fill list
2. the hosting_ebox need not to be refreshed, so its need_refresh
   token should always be true.
3. refresh each txt ebox in the data_list.
4. update and draw icon for each item.
5. before refresh a list display-window:
   5,1 set starting item index data_list->pw.
   5.2 awake the txt_ebox.
   5.3 set need_refresh flag.

6. !!!! Because of drawing sequence, bkimg refresh will mess up the image, so all txt_ebox to 
   shall set to be fixed type.!!!!
7. you may select the refresh sliding type. 


Retrun:
	0	OK
	<0	fail
-----------------------------------------------------------------------------------------*/
int egi_listbox_refresh(EGI_EBOX *ebox)
{
	int  i,j;
	int nwin; /* number of itmes in displaying window */
	int pw;	/* first item number in the displaying window */
	int inum;
	int itnum; /* tmp item number */

	EGI_DATA_LIST *data_list;

        /* 1. check list ebox  */
        if(ebox==NULL)
        {
                printf("egi_listbox_refresh(): input list ebox is NULL! fail to refresh.");
                return -1;
        }

        /* 2. check egi_data */
        if(ebox->egi_data==NULL)
        {
                printf("egi_listbox_refresh(): input ebox->egi_data is NULL! fail to activate.");
                return -2;
        }

	/* check host ebox need_refresh */
	if(!ebox->need_refresh)
		return -3;

        data_list=(EGI_DATA_LIST *)ebox->egi_data;
        inum=data_list->inum;
	pw=data_list->pw;
	nwin=data_list->nwin;

        /* 2. check data_list->txt_boxes */
	if(data_list == NULL )
	{
                printf("egi_listbox_refresh(): input ebox->data_list is NULL! fail to refresh.");
                return -3;
	}
        if(data_list->txt_boxes==NULL)
        {
                printf("egi_listbox_refresh(): input data_list->txt_boxes is NULL! fail to refresh.");
                return -4;
        }

	/* 3. update and push data to the list */
	//TODO

	/* 4. refresh those txt_eboxe in displaying window only */
	/*  seems no need to put all to sleep first ???
	   since need_refresh not set, and sleep already set in activate() */
	//for(i=0; i<inum; i++)
	//	egi_txtbox_sleep(data_list->txt_boxes[i]);

	/* activate those in displaying windows only */
	for(i=0; i<nwin; i++)
	{
		itnum=pw+i;	/* displaying from first item index pw */
		if(itnum >= inum)
			itnum=itnum%inum; /* roll back then */

		EGI_PDEBUG(DBG_LIST,"egi_listbox_refresh():----------itnum=%d----------\n",itnum);

		/* 4.1 update position, within displaying window */
		data_list->txt_boxes[itnum]->y0=ebox->y0	\
						+i*(data_list->txt_boxes[itnum]->height);/* update postion */

/* ----- motion default: */
           if(data_list->motion == 0 ) /* no motion,default */
	   {

		/* 4.2 activate it first, or bkimg=NULL will fail refresh 
		   to activate an txt_ebox also will refresh it */
		if( (data_list->txt_boxes[itnum])->status != status_active)
		{
			egi_txtbox_activate(data_list->txt_boxes[itnum]);

		}
		else
		{
			/* 4.3 refresh txtbox if do not take activation above */
			if( egi_txtbox_refresh(data_list->txt_boxes[itnum]) < 0 )
			{
				printf("egi_listbox_refresh(): fail to refresh data_list->txt_boxes[%d].\n",itnum);
				return -5;
			}
		}

		/* 4.4 refresh icon */
		if(data_list->icons[itnum])
		{
			symbol_writeFB(&gv_fb_dev, data_list->icons[itnum], SYM_NOSUB_COLOR,
					data_list->icons[itnum]->bkcolor,
 					data_list->txt_boxes[itnum]->x0, data_list->txt_boxes[itnum]->y0,
					data_list->icon_code[itnum], 0 ); /* opaque 0 */
		}

	   }/*end motion type 0 */

/* ----- motion sliding: */
	   else if(data_list->motion) /* motion type 1 or other ... *,sliding from left */
	   {
 	    /* sliding for() */
	    for(j=0;j<10;j++)
	    {
		data_list->txt_boxes[itnum]->x0=240-240/10*(j+1);

		/* set reflresh flag */
		data_list->txt_boxes[itnum]->need_refresh=true;

		if( (data_list->txt_boxes[itnum])->status != status_active)
		{
			egi_txtbox_activate(data_list->txt_boxes[itnum]);

		}
		else
		{
			/* 4.3 refresh txtbox if do not take activation above */
			if( egi_txtbox_refresh(data_list->txt_boxes[itnum]) < 0 )
			{
				printf("egi_listbox_refresh(): fail to refresh data_list->txt_boxes[%d].\n",itnum);
				return -5;
			}
		}

		/* 4.4 refresh icon */
		if(data_list->icons[itnum])
		{
			symbol_writeFB(&gv_fb_dev, data_list->icons[itnum], SYM_NOSUB_COLOR,
					data_list->icons[itnum]->bkcolor,
 					data_list->txt_boxes[itnum]->x0, data_list->txt_boxes[itnum]->y0,
					data_list->icon_code[itnum], 0 ); /* opaque 0 */
		}

		/* hold on for a while */
		tm_delayms(55); //100

	    } /* end for(), end sliding_refresh one txt_ebox */
	    tm_delayms(500);

	   }/*end of motion type 1 sliding */

	}/* end of refresh for */
	return 0;
}


/*-------------------------------------------------------------------------
1. to update prmcolor and data for item's data_txt in a list type ebox:

EGI_EBOX: 	list ebox to be updated
n:		number of the list's item considered
txt[nl][llen]:	txt to push to:
		data_list->txt_boxes[i]->egi_data->txt
		dimension numbers (nl and llen) MUST coincide with item data_txt 
		definition.
		NULL -- keep unchanged.
prmcolor:	prime color for item ebox
		<0;  do not update
		>=0:  uint16_t color

Retrun:
	1	data_txt keep unchanged
	0	OK
	<0	fail
-------------------------------------------------------------------------*/
int egi_listbox_updateitem(EGI_EBOX *ebox, int n, int prmcolor, const char **txt)
{
	int i;
//	int inum=0;
	EGI_DATA_LIST *data_list;


        /* 1. check list ebox */
        if(ebox==NULL)
        {
                printf("egi_listbox_updateitem(): input list is NULL! fail to push data to list item.");
                return -2;
        }

	/* 2. check ebox type */
	if(ebox->type != type_list)
	{
		printf("egi_listbox_updateitem(): input ebox is not list type! fail to update.");
		return -3;
	}

        data_list=(EGI_DATA_LIST *)ebox->egi_data;
//       inum=data_list->inum;

        /* 3. check data_list and data_list->txt_boxes */
	if(data_list == NULL )
	{
                printf("egi_listbox_updateitem(): input ebox->data_list is NULL! fail to update.");
                return -3;
	}
        if(data_list->txt_boxes==NULL)
        {
                printf("egi_listbox_updateitem(): input data_list->txt_boxes is NULL! fail to update.");
                return -3;
        }


	/* 4. check number */
	if(n > data_list->inum)
        {
                printf("egi_listbox_updateitem(): n is greater than list->inum.");
                return -4;
        }

        /* 5. check list->txt_boxes */
        if(data_list->txt_boxes==NULL)
        {
                printf("egi_listbox_updateitem(): input data_list->txt_boxes is NULL! fail to update item.");
                return -5;
        }

	/* 6. check data txt */
	EGI_EBOX *item_txtbox=data_list->txt_boxes[n];
	EGI_DATA_TXT *data_txt=(EGI_DATA_TXT *)(item_txtbox->egi_data);
	if(data_txt == NULL)
	{
                printf("egi_listbox_updateitem(): input data_list->txt_boxes->data_txt is NULL! fail to update item.");
                return -6;
	}


	/* 7. update prmcolor of the item ebox, only if >=0  */
	if(prmcolor>=0)
		data_list->txt_boxes[n]->prmcolor=prmcolor;


	/* 8. check txt */
	if(txt==NULL || txt[0]==NULL)
	{
//                EGI_PDEBUG(DBG_LIST,"egi_listbox_updateitem(): input txt is NULL! keep data_txt unchanged.\n");

		/* !!! put need_refresh flag before quit */
		item_txtbox->need_refresh=true;

                return 1;
	}


	/* 9. push txt into data_txt->txt */
	for(i=0; i<(data_txt->nl); i++)
	{
		strncpy(data_txt->txt[i], txt[i], data_txt->llen-1);/* 1 byte for end token */
		EGI_PDEBUG(DBG_LIST,"egi_listbox_updateitem(): txt_boxes[%d], txt[%d]='%s'\n",
										n,i,data_txt->txt[i]);
	}

	/* 9. set need_refresh flag */
	item_txtbox->need_refresh=true;

	return 0;
}

/*---------------------------------------------
decoration:
draw icon in the list item txt_ebox
----------------------------------------------*/
static int egi_itembox_decorate(EGI_EBOX *ebox)
{
	/* 1. suppose that ebox has already been checked and verified by the caller egi_listbox_refresh() */

	/* 2. get EGI_DATA_LIST */
	EGI_EBOX *father=ebox->father;
	EGI_DATA_LIST *data_list=(EGI_DATA_LIST *)(father->egi_data);

	/* 3. draw icons for each item txt_ebox */
	EGI_DATA_TXT *data_txt=(EGI_DATA_TXT *)(ebox->egi_data);
	int id=data_txt->id;
	if( data_list->icons[id-1] ) /* data_txt->id starts from 1, icons index from 0  */
	{
		symbol_writeFB(&gv_fb_dev, data_list->icons[id-1], SYM_NOSUB_COLOR,
				data_list->icons[id-1]->bkcolor,
				data_list->txt_boxes[id-1]->x0, data_list->txt_boxes[id-1]->y0,
				data_list->icon_code[id-1], 0 );
	}

	return 0;
}
