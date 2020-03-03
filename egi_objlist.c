/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/

#include <stdlib.h>
#include <stdint.h>
#include "egi.h"
#include "egi_list.h"
#include "egi_timer.h"
#include "egi_objlist.h"
#include "egi_symbol.h"
#include "egi_color.h"
#include "egi_objtxt.h"

/*----------------------------------------------------------------
int reaction(EGI_EBOX *ebox, EGI_TOUCH_DATA *touch_data)
----------------------------------------------------------------*/
int egi_listbox_test(EGI_EBOX *ebox, EGI_TOUCH_DATA *touch_data)
{
	if(touch_data->status != pressing)
		return btnret_IDLE;

	int i,j;
	int inum=13;
	int nwin=5; /* window size in items */
	int nl=2;
	static int count=0; /* test counter */

	printf("egi_listbox_test(): egi_list_new()... \n");
	/* 1. create a list ebox */
	EGI_EBOX *list=egi_listbox_new (
        	0,30,  	//int x0, int y0, /* left top point */
	        inum,	//int inum,       /* item number of a list */
        	nwin,   /* number of items in displaying window */
	       	240,	//int width,      /* h/w for each list item - ebox */
	        56,	//int height,
	        0,      //frame, 	  /* -1 no frame for ebox, 0 simple .. */
	        2,	//int nl,         /* number of txt lines for each txt ebox */
	        30,	//int llen,       /* in byte, length for each txt line */
	        &sympg_testfont,	//EGI_SYMPAGE *font, /* txt font */
	        45,	//int txtoffx,     /* offset of txt from the ebox, all the same */
	        2,	//int txtoffy,
	        0,	//int iconoffx,   /* offset of icon from the ebox, all the same */
	        0	//int iconoffy
	);


	EGI_DATA_LIST *data_list=(EGI_DATA_LIST *)(list->egi_data);
printf("egi_listbox_test(): finish egi_listbox_new(). \n");

	/* 2. icons to be loaded later */


	/* 3. update item */
	char data[13][2][30] =  /* inum=5; nl=2; llen=30 */
	{
		{"S&P 500   2,610.30","+1.07%"},
		{"Dow 30    24,065.59","+0.65%"},
		{"Nasdaq    7,023.83","+1.71%"},
		{"S&P 500   2,610.30","+1.07%"},
		{"Russel 2000  1,445,22","+0.87%"},
		{"Nikkei 225  20,392.26","-0.79%"},
		{"Crude Oil  51.83","-0.54%"},
		{"Gold      1,290.20","+0.14%"},
		{"Silver    15.63","+0.06%"},
		{"EUR/USD   1.1408","-0.00000%"},
		{"USD/JPY   108.4310","-0.2043%"},

/*
		{"Widora-NEO     11111","mt7688"},
		{"Widora-BIT5    22222","mt7628dan"},
		{"Widora-AIR     33333","ESP32"},
		{"Widora-BIT     44444","mt7688AN"},
		{"Widora-Ting    55555555","SX1278"}
*/
	};


	egi_msgbox_create("Message: \n\n   Start list test ... \n\n",1000, WEGI_COLOR_ORANGE);



	char **pdata;
	pdata=malloc(nl*sizeof(char *)); //nl=2;

	/* COLORFUL */
//	uint16_t color[]= {0xDEFB, 0xFE73, 0x07FF, 0xFE73, 0xFE7F};
//	uint16_t color[]= {0x67F9, 0xFFE6, 0x0679, 0xFE79, 0x9E6C};
	/* GRAY */
	uint16_t color[5]= {WEGI_COLOR_GRAY,WEGI_COLOR_GRAY1,WEGI_COLOR_GRAY2,WEGI_COLOR_GRAY3,WEGI_COLOR_GRAY4};

	uint32_t color24[4]={0xCCFFFF,0xCCCCFF,0x99FFCC,0xFFCCFF};

	for(i=0; i<inum; i++)
	{
		/* set item data, nl=2  */
		for(j=0;j<nl;j++)
		{
			pdata[j]=&data[i][j][0];
			printf("pdata[%d]: %s \n",j,pdata[j]);
		}

		/* set icon */
		data_list->icons[i]=&sympg_icons;
		data_list->icon_code[i]=31;

		/* update items */
		egi_listbox_updateitem(list, i, COLOR_24TO16BITS(color24[i%4]),(const char **)pdata); //color[i%5], pdata);
	}

	/* 4. activate the list	*/
//	egi_listbox_activate(list);
	printf("egi_listbox_test(): finish egi_list_activate(). \n");


	char *txt="Widora-NEO     12345	\
		   \n mt7688	\
		\n Widora-BIT5     23233	\
		\n mt7628dan	\
		\n Widora-AIR    43534	\
		\n ESP32	\
		\n Widora-BIT     48433	\
		\n mt7688AN	\
		\n Widora-Ting    496049999	\
		\n SX1278";


	/* 5. loop refresh */
	i=0;
	j=0;
	pdata[0]="Hello! NEO world!";
	/* set start pw */
	data_list->pw=0;
	/* set refresh motion type */
	data_list->motion=1;/* sliding from left */

	while(1)
	{
#if 0
		/* update item 0  */
		sprintf(pdata[1],"count: %d", i++);
		egi_listbox_updateitem(list, 0, -1, NULL); /* -1, keep old color */
		/* update item 1 */
		egi_listbox_updateitem(list, 1, -1, NULL); /* -1, keep old color and txt  */
		/* update item 2 */
		egi_listbox_updateitem(list, 2, -1, NULL); /* -1, keep old color and txt */
		/* update item 3 */
		egi_listbox_updateitem(list, 3, -1, NULL); /* -1, keep old color and txt */
		/* update item 4 */
		sprintf(pdata[1],"count: %d", i++);
		egi_listbox_updateitem(list, 4, -1, NULL); /* -1, keep old color */
#endif

		/* update all items before refresh */
		for(i=0;i<inum;i++)
		{
			egi_listbox_updateitem(list, i, -1, NULL); /* -1, keep old color and txt */
		}
		egi_listbox_refresh(list);
		tm_delayms(1000);


		/* slide displaying window by adjust pw */
		data_list->pw++;
		if(data_list->pw > data_list->inum-1 )
			data_list->pw=0;


		/* end loop */
		j++;
		if(j>1)
		{
			egi_msgbox_create("Message: \n\n End of list test. Thanks! \n\n",1000, WEGI_COLOR_ORANGE);
			break;
		}

	}

	list->sleep(list);
	printf("egi_listbox_test(): start list->free(list)... \n");
	list->free(list);
	printf("egi_listbox_test(): start free(pdata)... \n");
	free(pdata);


	return pgret_OK; /* treat as a page return, then the host page will refresh itself */
}


