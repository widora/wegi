/*-----------------------   egi_objtxt.c    ----------------------
EBOX OBJs derived from EBOX_TXT

1. All txt type ebox are to be allocated/freed dynamically.

Midas Zhou
----------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "egi_common.h"
//#include "egi_bjp.h"
//#include "egi_color.h"
//#include "egi.h"
//#include "egi_txt.h"
#include "egi_objtxt.h"
//#include "egi_timer.h"
//#include "egi_debug.h"
//#include "egi_timer.h"
#include "egi_symbol.h"


static int egi_txtbox_decorate(EGI_EBOX *ebox);



/*--------------  EBOX MEMO  ------------------
Create an 240x30 size txt ebox
return:
	txt ebox pointer 	OK
	NULL			fai.
-----------------------------------------------*/
EGI_EBOX *create_ebox_memo(void)
{
	/* 1. create memo_txt */
	EGI_DATA_TXT *memo_txt=egi_txtdata_new(
		5,5, /* offset X,Y */
      	  	12, /*int nl, lines  */
       	 	24, /*int llen, chars per line */
        	&sympg_testfont, /*EGI_SYMPAGE *font */
        	WEGI_COLOR_BLACK /* uint16_t color */
	);

	/* 2. fpath */
	memo_txt->fpath="/home/memo.txt";

	/* 3. create memo ebox */
	EGI_EBOX  *ebox_memo= egi_txtbox_new(
		"memo stick", /* tag */
        	memo_txt,  /* EGI_DATA_TXT pointer */
        	true, /* bool movable */
       	 	12,0, /* int x0, int y0 */
        	240,320, /* int width, int height */
        	-1, /* int frame, -1=no frame */
        	WEGI_COLOR_ORANGE /*int prmcolor*/
	);

	return ebox_memo;
}


/*------------  EBOX_CLOCK  ------------------
Create an  txt ebox digital clock
return:
	txt ebox pointer 	OK
	NULL			fai.
--------------------------------------------*/
EGI_EBOX *create_ebox_clock(void)
{

	/* 1. create a data_txt */
	EGI_DATA_TXT *clock_txt=egi_txtdata_new(
		20,0, /* offset X,Y */
      	  	3, /*int nl, lines  */
       	 	64, /*int llen, chars per line */
        	&sympg_testfont, /*EGI_SYMPAGE *font */
        	WEGI_COLOR_BLACK /* uint16_t color */
	);

	strncpy(clock_txt->txt[1],"abcdefg",5);

	/* 2. create memo ebox */
	EGI_EBOX  *ebox_clock= egi_txtbox_new(
		"timer txt", /* tag */
        	clock_txt,  /* EGI_DATA_TXT pointer */
        	true, /* bool movable */
       	 	60,5, /* int x0, int y0 */
        	120,20, /* int width, int height */
        	0, /* int frame,0=simple frmae, -1=no frame */
        	WEGI_COLOR_BROWN /*int prmcolor*/
	);

	return ebox_clock;
}


/*------------  EBOX NOTE ----------*/
EGI_EBOX *create_ebox_note(void)
{

	/* 1. create a data_txt */
	EGI_DATA_TXT *note_txt=egi_txtdata_new(
		5,5, /* offset X,Y */
      	  	2, /*int nl, lines  */
       	 	32, /*int llen, chars per line */
        	&sympg_testfont, /*EGI_SYMPAGE *font */
        	WEGI_COLOR_BLACK /* uint16_t color */
	);

	/* 2. create memo ebox */
	EGI_EBOX  *ebox_note= egi_txtbox_new(
		"note pad", /* tag */
        	note_txt,  /* EGI_DATA_TXT pointer */
        	true, /* bool movable */
       	 	5,80, /* int x0, int y0 */
        	230,60, /* int width, int height */
        	-1, /* int frame, -1=no frame */
        	WEGI_COLOR_GRAY /*int prmcolor*/
	);

	return ebox_note;
}


/*--------------  EBOX_NOTES  --------------------
Create an txt note ebox with parameters

num: 		id number for txt
x0,y0:		left top cooordinate
bkcolor:	ebox color

return:
	txt ebox pointer 	OK
	NULL			fai.
------------------------------------------------*/
EGI_EBOX *create_ebox_notes(int num, int x0, int y0, uint16_t bkcolor)
{
	/* 1. create a data_txt */
	EGI_PDEBUG(DBG_OBJTXT,"start to egi_txtdata_new()...\n");
	EGI_DATA_TXT *clock_txt=egi_txtdata_new(
		10,30, /* offset X,Y */
      	  	3, /*int nl, lines  */
       	 	64, /*int llen, chars per line */
        	&sympg_testfont, /*EGI_SYMPAGE *font */
        	WEGI_COLOR_BLACK /* uint16_t color */
	);

	if(clock_txt == NULL)
	{
		printf("create_ebox_notes(): clock_txt==NULL, fails!\n");
		return NULL;
	}
	else if (clock_txt->txt[0]==NULL)
	{
		printf("create_ebox_notes(): clock_txt->[0]==NULL, fails!\n");
		return NULL;
	}

        sprintf(clock_txt->txt[0],"        2019 ");
        sprintf(clock_txt->txt[1],"Happy New Year!");
        sprintf(clock_txt->txt[2],"Note NO. %d", num);

	/* 2. create memo ebox */
	EGI_PDEBUG(DBG_OBJTXT,"create_ebox_notes(): strat to egi_txtbox_new().....\n");
	EGI_EBOX  *ebox_clock= egi_txtbox_new(
		NULL, /* tag, put later */
        	clock_txt,  /* EGI_DATA_TXT pointer */
        	true, /* bool movable */
       	 	x0,y0, /* int x0, int y0 */
        	160,66, /* int width, int height */
        	0, /* int frame,0=simple frmae, -1=no frame */
        	bkcolor /*int prmcolor*/
	);

	return ebox_clock;
}


/*----------------------------------------------
A simple demo for txt type ebox
as a reaction function for button.

int (*reaction)(EGI_EBOX *, enum egi_touch_status);
----------------------------------------------*/
int egi_txtbox_demo(EGI_EBOX *ebox, EGI_TOUCH_DATA * touch_data)
{

	if(touch_data->status != pressing)
	   return btnret_IDLE;

	int total=56;
	int i;
	EGI_EBOX *txtebox[56];
	int ret;

	printf("--------- txtebox demon --- START ---------\n");

	for(i=0;i<total;i++)
	{
	      EGI_PDEBUG(DBG_OBJTXT,"create ebox notes txtebox[%d].\n",i);
	      txtebox[i]=create_ebox_notes(i, egi_random_max(80), egi_random_max(320-108), egi_color_random(color_light));
	      if(txtebox[i]==NULL)
	      {
			printf("egi_txtbox_demon(): create a txtebox[%d] fails!\n",i);
			return -1;
	      }
	      /* put decorate */
	      txtebox[i]->decorate=egi_txtbox_decorate;

	      EGI_PDEBUG(DBG_OBJTXT,"egi_txtbox_demon(): start to activate txtebox[%d]\n",i);
	      ret=txtebox[i]->activate(txtebox[i]);
	      if(ret != 0)
			printf(" egi_txtbox_demo() txtebox activate fails with ret=%d\n",ret);

	      /* apply decoration method */
	      EGI_PDEBUG(DBG_OBJTXT,"egi_txtbox_demon(): start to decorate txtebox[%d]\n",i);
	      ret=txtebox[i]->decorate(txtebox[i]);
	      if(ret != 0)
			printf(" egi_txtbox_demo() txtebox decorate fails with ret=%d\n",ret);


	      tm_delayms(5);
//	      usleep(200000);
	}
	tm_delayms(2000); /* hold on SHOW */

	for(i=total-1;i>=0;i--)
	{
		      txtebox[i]->sleep(txtebox[i]); /* dispare from LCD */
		      txtebox[i]->free(txtebox[i]);
		      tm_delayms(5);
	      // --- free ---
	}
//	tm_delayms(2000); /* hold on CLEAR */
	//getchar();
	printf("--------- txtebox demon --- END ---------\n");

	return pgret_OK; /* to treat as a page returen, so host page will refresh itsel */
}



/*-----------   EGI_PATTERN :  TITLE BAR 240x30 -------------
Create an txt_ebox  for a title bar.
standard tag "title_bar"

x0,y0:		left top cooordinate
offx,offy:	offset of txt
bkcolor:	ebox color, bkcolor

return:
	txt ebox pointer 	OK
	NULL			fai.
------------------------------------------------------------*/
EGI_EBOX *create_ebox_titlebar(
	int x0, int y0,
	int offx, int offy,
	uint16_t bkcolor,
	char *title
)
{
	/* 1. create a data_txt */
	EGI_PDEBUG(DBG_OBJTXT,"start to egi_txtdata_new()...\n");
	EGI_DATA_TXT *title_txt=egi_txtdata_new(
		offx,offy, /* offset X,Y */
      	  	1, /*int nl, lines  */
       	 	64, /*int llen, chars per line, however also limited by width */
        	&sympg_testfont, /*EGI_SYMPAGE *font */
        	WEGI_COLOR_BLACK /* int16_t color */
	);

	if(title_txt == NULL)
	{
		printf("create_ebox_titlebar(): title_txt=egi_txtdata_new()=NULL, fails!\n");
		return NULL;
	}
	else if (title_txt->txt[0]==NULL)
	{
		printf("create_ebox_titlebar(): title_txt->[0]==NULL, fails!\n");
		return NULL;
	}

	/* 2. put title string */
	if(title==NULL)
	{
		EGI_PDEBUG(DBG_OBJTXT,"create_ebox_titlebar(): title==NULL, use default title\n");
	        strncpy(title_txt->txt[0], "--- title bar ---", title_txt->llen-1); /* default */
	}
	else
	{
	        strncpy(title_txt->txt[0], title, title_txt->llen-1); /* default */
	}


	/* 3. create memo ebox */
	EGI_PDEBUG(DBG_OBJTXT,"create_ebox_titlebar(): start egi_txtbox_new().....\n");
	EGI_EBOX  *ebox_title= egi_txtbox_new(
		"title_bar", /* tag, or put later */
        	title_txt,  /* EGI_DATA_TXT pointer */
        	true, /* bool movable */
       	 	x0,y0, /* int x0, int y0 */
        	240,30, /* int width;  int height,which also related with symheight and offy */
        	0, /* int frame, 0=simple frmae, -1=no frame */
        	bkcolor /*int prmcolor*/
	);

	return ebox_title;
}


/*--------------------------------------------------
	txtbox decorationg function
---------------------------------------------------*/
static int egi_txtbox_decorate(EGI_EBOX *ebox)
{
        int ret=0;

        EGI_PDEBUG(DBG_TXT,"egi_txtbox_decorate(): start to show_jpg()...\n");
        ret=show_jpg("/tmp/openwrt.jpg", &gv_fb_dev, SHOW_BLACK_NOTRANSP, ebox->x0+2, ebox->y0+2);

        return ret;
}


/*--------------------------  MSG BOX  -----------------------------
Message Box, initial size:240x50, will be ajusted.
Display for a while, then release/free it.

@msg: 		message string
@ms:  		>0	It's an instant msgbox, delay/display in ms time, then destroy it.
		=0	Just keep the msgbox, to destroy it later.
     		<0 	1. It's a progress indicator/messages box,
			2. Do not release it until the indicated progress ends.
			3. data_txt.foff then will be used as progress value. MAX.100

@bkcolor:	back colour
@ms: msg displaying time, in ms.

Return:
	ms>0	NULL
	ms=<0	pointer to an txt_type ebox

---------------------------------------------------------------------*/
EGI_EBOX * egi_msgbox_create(char *msg, long ms, uint16_t bkcolor)
{
	if(msg==NULL)return NULL;

	int x0=0; 	/* ebox top left point */
	int y0=50;
	int width=240; 	/* ebox W/H */
	int height=50;
	int yres=(&gv_fb_dev)->vinfo.yres;
	int offx=10;
	int offy=8;	/* offset x,y of txt */
	int maxnl=(yres-offy*2)/(&sympg_testfont)->symheight;
	int nl=maxnl;  	/* first, set nl as MAX line number for a full screen. */
	int llen=64; 	/* max. chars for each line,also limited by ebox width */
	int pnl=0; 	/* number of pushed txt lines */

	EGI_DATA_TXT *msg_txt=NULL;
	EGI_EBOX *msgbox=NULL;

   	while(1) /* repeat 1 more time to adjust size of the ebox to fit with the message */
   	{
		/* 1. create a data_txt */
		EGI_PDEBUG(DBG_OBJTXT,"egi_msgbox_create(): start to egi_txtdata_new()...\n");
		msg_txt=egi_txtdata_new(
			offx,offy, 	 /* offset X,Y */
      		  	nl, 		 /*int nl, lines  */
       	 		llen, 		 /*int llen, chars per line, however also limited by ebox width */
        		&sympg_testfont, /*EGI_SYMPAGE *font */
        		WEGI_COLOR_BLACK /* int16_t color */
		);

		if(msg_txt == NULL)
		{
			printf("egi_msgbox_create(): msg_txt=egi_txtdata_new()=NULL, fails!\n");
			return NULL;
		}
		else if (msg_txt->txt[0]==NULL)
		{
			printf("egi_msgbox_create(): msg_txt->[0]==NULL, fails!\n");
			return NULL;
		}

		/* 3. create msg ebox */
		EGI_PDEBUG(DBG_OBJTXT,"egi_msgbox_create(): start egi_txtbox_new().....\n");
		height=nl*((&sympg_testfont)->symheight)+2*offy; /*adjust ebox height */
		msgbox= egi_txtbox_new(
			"msg_box", 	/* tag, or put later */
        		msg_txt,  	/* EGI_DATA_TXT pointer */
        		true, 		/* bool movable */
       	 		x0,y0, 		/* int x0, int y0 */
        		width,height, 	/* int width;  int height,which also related with symheight,nl and offy */
        		1, 		/* int frame, 0=simple frmae, -1=no frame */
        		bkcolor 	/*int prmcolor*/
		);

		/* 4. push txt to ebox */
		egi_push_datatxt(msgbox, msg, &pnl);

		/* 5. adjust nl then release and loop back and re-create msg ebox */
		EGI_PDEBUG(DBG_OBJTXT,"egi_msgbox_create(): total number of pushed lines pnl=%d, while nl=%d \n",pnl,nl);
		if(ms<0) pnl+=1; /* one more line for progress information */
		if(nl>pnl) 	 /* if nl great than number of pushed lines */
		{
			nl=pnl;
			msgbox->free(msgbox);
			msg_txt=NULL;
			msgbox=NULL;
		}
		else
			break;

 	 } /* end of while() */

	/*.7 put progress information txt */
	if(ms<0)
	{
		int np=nl<maxnl?nl:maxnl; /* get progress info line number */
		strncpy(msg_txt->txt[np-1],"	Processing...  0/100",llen-1);
	}

	/* 6. display msg box */
	msgbox->activate(msgbox);/* activate to display */

	/* 7. return or destroy the msg */
	if(ms<=0) /* It's a progress info. msgbox or keep it */
	{
		return msgbox;
	}
	else /* It's an instant msgbox */
	{
		tm_delayms(ms);/* displaying for a while */
		msgbox->sleep(msgbox); /* erase the image */
		msgbox->free(msgbox);  /* release */
		return NULL;
	}
}


/*-----------------------------------------------
Update progress value for a msgbox

msgbox:		message ebox
pv:		progress indicating value (0-100)
------------------------------------------------*/
void egi_msgbox_pvupdate(EGI_EBOX *msgbox, int pv)
{
	/* check data */
	if(msgbox==NULL)return;

	/* set limit */
	if(pv<0)pv=0;
	else if(pv>100)pv=100;

	EGI_DATA_TXT *msg_txt=msgbox->egi_data;
	if(msg_txt==NULL) return;

	int nl=msg_txt->nl;
	/* WARNING:: string to be the same as of in egi_display_msgbox() */
	sprintf(msg_txt->txt[nl-1],"	processing...  %d/100", pv);

	/* refresh the msgbox */
	msgbox->need_refresh=true;
	msgbox->refresh(msgbox);
}


/*-----------------------------------------------
destroy/release the msgbox

msgbox:		message ebox
------------------------------------------------*/
void egi_msgbox_destroy(EGI_EBOX *msgbox)
{
	if(msgbox == NULL) return;
	msgbox->sleep(msgbox); /* erase image */
	msgbox->free(msgbox); /* release it */
}
