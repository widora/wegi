/*----------------------- egi_list.c -------------------------------
         ---- A List item is a txt_ebox contains an icon! -----

1. A list consists of items.
2. Each item has an icon and several txt lines, all those txt lines is
   of the same txt ebox.
3. A txt ebox's height/width is also height/width of the list obj,
   that means a list item is a txt_ebox include an icon.
3. (NOPE!)A list has a full width of the screen, that is fd.vinfo.xres in pixels.
4. //TBD//If an item txt line has NULL content, then it will be treated as the end
   line of that item txt_ebox. That means the number of txt line displayed
   for each item is adjustable.
5.  


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



/*-----------   EGI_PATTERN :  LIST   -----------------
Create an list obj.
standard tag "list"

default ebox prmcolor: 	WEGI_COLOR_GRAY
default font color: 	WEGI_COLOR_BLACK

return:
	txt ebox pointer 	OK
	NULL			fai.
-----------------------------------------------------*/
EGI_LIST *egi_list_new (
	int x0, int y0, /* left top point */
        int inum,  	/* item number of a list */
        int width, 	/* h/w for each list item - ebox */
        int height,
	int frame, 	/* -1 no frame for ebox, 0 simple .. */
	int nl,		/* number of txt lines for each txt ebox */
	int llen,  	/* in byte, length for each txt line */
        struct symbol_page *font, /* txt font */
        int txtoffx,	 /* offset of txt from the ebox, all the same */
        int txtoffy,
        int iconoffx, 	/* offset of icon from the ebox, all the same */
        int iconoffy
)
{
	int i;

	/* 1. malloc list structure */
	EGI_LIST *list=malloc(sizeof(EGI_LIST));
	if(list == NULL)
	{
		printf("egi_list_new(): fail to malloc a EGI_LIST *list.\n");
		return NULL;
	}
	memset(list,0,sizeof(EGI_LIST));
printf("egi_list_new(): malloc list end...\n");

	/* 2.1 malloc data_txt */
	EGI_DATA_TXT **data_txt=malloc(inum*sizeof(EGI_DATA_TXT *));
	if( data_txt == NULL )
	{
		printf("egi_list_new(): fail to malloc  **data_txt.\n");
		return NULL;
	}
	memset(data_txt,0,inum*sizeof(EGI_DATA_TXT *));
printf("egi_list_new(): malloc **data_txt end...\n");


	/* 2.2 malloc **txtbox  */
	EGI_EBOX **txt_boxes=malloc(inum*sizeof(EGI_EBOX *));
	if( txt_boxes == NULL )
	{
		printf("egi_list_new(): fail to malloc a EGI_EBOX **txt_box.\n");
		return NULL;
	}
	memset(txt_boxes,0,inum*sizeof(EGI_EBOX *));
printf("egi_list_new(): malloc txtboxes end...\n");


	/* 3. init **icon */
	struct symbol_page **icons=malloc(inum*sizeof(struct symbol_page *));
	if( icons == NULL )
	{
		printf("egi_list_new(): fail to malloc a symbol_page **icon.\n");
		return NULL;
	}
	memset(icons,0,inum*sizeof(struct symbol_page *));
printf("egi_list_new(): malloc icons end...\n");

	/* 3. init *icon_code */
	int *icon_code =malloc(inum*sizeof(int));
	if( icon_code == NULL )
	{
		printf("egi_list_new(): fail to malloc *icon_code.\n");
		return NULL;
	}
	memset(icon_code,0,inum*sizeof(int));
printf("egi_list_new(): malloc icon_code end...\n");


	/* 4. init bkcolor */
	uint16_t *prmcolor=malloc(inum*sizeof(uint16_t));
	/* init prmcolor with default bkcolor */
	memset(prmcolor,0,inum*sizeof(uint16_t));
	for(i=0;i<inum;i++)
		prmcolor[i]=LIST_DEFAULT_BKCOLOR;
printf("egi_list_new(): malloc prmcolor end...\n");


	/* 5. assign  elements */
	list->x0=x0;
	list->y0=y0;
	list->inum=inum;
	list->txt_boxes=txt_boxes;
//	list->width=width;	/* for txt ebox */
//	list->height=height;
	list->iconoffx=iconoffx;
	list->iconoffy=iconoffy;

	list->icons=icons;
	list->icon_code=icon_code;
	list->prmcolor=prmcolor;


	/* 6. create txt_type eboxes  */
	for(i=0;i<inum;i++)
	{
		/* 6.1  create a data_txt */
		egi_pdebug(DBG_LIST,"egi_list_new(): start to egi_txtdata_new()...\n");
		data_txt[i]=egi_txtdata_new(
				txtoffx,txtoffy, /* offset X,Y */
      	 		 	nl, /*int nl, lines  */
       	 			llen, /*int llen, chars per line, however also limited by width */
        			font, /*struct symbol_page *font */
        			WEGI_COLOR_BLACK /* int16_t color */
			     );
		if(data_txt[i] == NULL) /* if fail retry...*/
		{
			printf("egi_list_new(): data_txt=egi_txtdata_new()=NULL,  retry... \n");
			i--;
			continue;
		}
printf("egi_list_new(): %d_th data_txt=egi_txtdata_new() end...\n",i);


		/* 6.2 creates all those txt eboxes */
	        egi_pdebug(DBG_LIST,"egi_list_new(): start egi_list_new().....\n");
       		(list->txt_boxes)[i] = egi_txtbox_new(
                			"----", /* tag, or put later */
		                	data_txt[i], /* EGI_DATA_TXT pointer */
               				true, /* bool movable */
			                x0, y0+i*height, /* int x0, int y0 */
                			width, height, /* int width;  int height,which also related with symheight and offy */
                			frame, /* int frame, 0=simple frmae, -1=no frame */
                			prmcolor[i] /*int prmcolor*/
  		    		  );

printf("egi_list_new(): txt_boxes[%d]=egi_txtbox_new() end before test NULL ...\n",i);
		if( (list->txt_boxes)[i] == NULL )
		{
			printf("egi_list_new(): txt_eboxes[%d]=NULL,  retry... \n",i);
			free(data_txt[i]);
			i--;
			continue;
		}
printf("egi_list_new(): txt_boxes[%d]=egi_txtbox_new() end...\n",i);


		/* 6.3 set-tag */
		sprintf(list->txt_boxes[i]->tag,"item_%d",i);
printf("egi_list_new(): txt_boxes[%d] set_tag  end...\n",i);

	}

	/* free data_txt */
	free(data_txt);

	return list;
}

/*---------------------------------------
free a list and its items

return:
	0	OK
	<0	fail
---------------------------------------*/
int egi_list_free(EGI_LIST *list)
{
	int ret=0;
	int inum;
	int i;

	/* 1. check list  */
	if( list==NULL )
	{
		printf("egi_list_free(): input list is NULL! fail to activate.");
		return -1;
	}
	inum=list->inum;

	/* 2. check list->txt_boxes  */
	if( list->txt_boxes==NULL )
	{
		printf("egi_list_free(): list->txt_boxes is NULL! try to free other numbers of the list.");
	}
	else /* ok txt_boxes need to free */
	{
		/* 3. free each txt_ebox in the list */
		for(i=0; i<inum; i++)
		{
			if( list->txt_boxes[i]==NULL ) /* check txt_boxes */
			{
				printf("egi_list_free(): list->txt_boxes[%d] is NULL,fail to free it . \n",i);
				ret=-2;
				/* continue to free next */
				continue;
			}
			if( (list->txt_boxes[i])->free(list->txt_boxes[i]) <0 ) /* freee the ebox */
			{
				printf("egi_list_free(): fail to free list->txt_boxes[%d]. \n",i);
				ret=-3;
			}
		}

		/* 4. to free txt_boxes itself */
		free(list->txt_boxes);
	}

	/* 5. free icon pointers and icon_code */
	if(list->icons != NULL)
		free(list->icons);
	if(list->icon_code != NULL)
		free(list->icon_code);

	/* 6. free bkcolor */
	if(list->prmcolor != NULL)
		free(list->prmcolor);

	/* 7. finally, free the list */
	if(list !=NULL)
		free(list);

	return ret;
}


/*----------------------------------------------
to activate a list:
1. activate each txt ebox in the list.
2. draw icon for drawing item.

Retrun:
	0	OK
	<0	fail
----------------------------------------------*/
int egi_list_activate(EGI_LIST *list)
{
	int i;
	int inum;

	/* 1. check list  */
	if(list==NULL)
	{
		printf("egi_list_activate(): input list is NULL! fail to activate.");
		return -1;
	}
	inum=list->inum;

	/* 2. check list->txt_boxes */
	if(list->txt_boxes==NULL)
	{
		printf("egi_list_activate(): input list->txt_boxes is NULL! fail to activate.");
		return -2;
	}

	/* 3. activate each txt_ebox in the list */
	for(i=0; i<inum; i++)
	{

		/* 3.1 activate txt ebox */
		if( egi_txtbox_activate(list->txt_boxes[i]) < 0)
		{
			printf("egi_list_activate(): fail to activate list->txt_boxes[%d].\n",i);
			return -3;
		}

		/* 3.2 FB write icon */
		if(list->icons[i])
		{
			symbol_writeFB(&gv_fb_dev, list->icons[i], SYM_NOSUB_COLOR, list->icons[i]->bkcolor,
 					list->txt_boxes[i]->x0, list->txt_boxes[i]->y0,
					list->icon_code[i], 0 );
		}
	}

	/* 4. draw icon for each txt_ebox */
	//TODO


	return 0;
}


/*---------------------------------------------------------
to refresh a list:
0. TODO: update/push data to fill list
1. refresh each txt ebox in the list.
2. update and draw icon for drawing item.

Retrun:
	0	OK
	<0	fail
----------------------------------------------------------*/
int egi_list_refresh(EGI_LIST *list)
{
	int  i;
	int inum;

        /* 1. check list  */
        if(list==NULL)
        {
                printf("egi_list_refresh(): input list is NULL! fail to refresh.");
                return -1;
        }
        inum=list->inum;

        /* 2. check list->txt_boxes */
        if(list->txt_boxes==NULL)
        {
                printf("egi_list_refresh(): input list->txt_boxes is NULL! fail to refresh.");
                return -2;
        }

	/* 3. update and push data to the list */
	//TODO

	/* 4. refresh each txt eboxe */
	for(i=0; i<inum; i++)
	{

		/* 4.1 refresh txt */
		if( egi_txtbox_refresh(list->txt_boxes[i]) < 0 )
		{
			printf("egi_list_refresh(): fail to refresh list->txt_boxes[%d].\n",i);
			return -3;
		}
		/* 4.2 FB write icon */
		if(list->icons[i])
		{
			symbol_writeFB(&gv_fb_dev, list->icons[i], SYM_NOSUB_COLOR, list->icons[i]->bkcolor,
 					list->txt_boxes[i]->x0, list->txt_boxes[i]->y0,
					list->icon_code[i], 0 );
		}

	}


	return 0;
}


/*-------------------------------------------------------------------------
to update prmcolor and data for a list txt_ebox item:

EGI_LIST: 	list to be updated
n:		number of the list's item considered
data:		data to push to:
		 list->txt_boxes[i]->egi_data->txt
prmcolor:	prime color for item ebox

Retrun:
	0	OK
	<0	fail
-------------------------------------------------------------------------*/
int egi_list_updateitem(EGI_LIST *list, int n, uint16_t prmcolor, char **data)
{
	int i;

	/* 0. check data */
	if(data==NULL || data[0]==NULL)
	{
                printf("egi_list_updateitem(): input data is NULL! fail to push data to list item.");
                return -1;
	}

        /* 1. check list  */
        if(list==NULL)
        {
                printf("egi_list_updateitem(): input list is NULL! fail to push data to list item.");
                return -1;
        }
	/* check number */
	if(n > list->inum)
        {
                printf("egi_list_updateitem(): n is greater than list->inum.");
                return -1;
        }

        /* 2. check list->txt_boxes */
        if(list->txt_boxes==NULL)
        {
                printf("egi_list_updateitem(): input list->txt_boxes is NULL! fail to push data to list item.");
                return -2;
        }

	/* 3. check data txt */
	EGI_DATA_TXT *data_txt=(EGI_DATA_TXT *)(list->txt_boxes[n]->egi_data);
	if(data_txt == NULL)
	{
                printf("egi_list_updateitem(): input list->txt_boxes->data_txt is NULL! fail to push data to list item.");
                return -3;
	}

	/* 4. update prmcolor of the item ebox */
	list->prmcolor[n]=prmcolor;
	list->txt_boxes[n]->prmcolor=prmcolor;

	/* 5. push data into data_txt->txt */
	for(i=0; i<(data_txt->nl); i++)
	{
		strncpy(data_txt->txt[i], data[i], data_txt->llen-1);/* 1 byte for end token */
		egi_pdebug(DBG_LIST,"egi_list_updateitem(): data[%d]='%s'\n",i,data_txt->txt[i]);
	}

	return 0;

}
