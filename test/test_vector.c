/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A program to test EGI VECTOR.

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_FTsymbol.h>
#include <egi_utils.h>
#include <egi_matrix.h>
#include <egi_input.h>

static EGI_MOUSE_STATUS *pmostat;

/* Signals/Acts, Flags/Status */
static bool mouse_request;	    /* Set to true after each mouse_callback */
EGI_POINT   mpt;      	    	    /* Mouse point */
//static bool flagPickPoints;
//static bool actRectSelect;	    /* activate rectanguler selection */
static int  mouseX, mouseY, mouseZ; /* Mouse point position */
void FTsymbol_writeFB(char *txt, int fw, int fh, EGI_16BIT_COLOR color, int px, int py);
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus);
static void draw_mcursor(int x, int y);


/*----------------------------
	     MAIN
-----------------------------*/
int main(int argc, char **argv)
{
  int i;

  /* <<<<<<  1. EGI general init  EGI初始化流程 >>>>>> */
  /* 对不必要的一些步骤可以忽略 */

  /* 1.1 Start sys tick 	开启系统计数 */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();
  /* 1.2 Start egi log 		开启日志记录 (忽略) */
  /* 1.3 Load symbol pages 	加载图形/符号映像 (忽略) */
  /* 1.4 Load freetype fonts 	加载FreeType字体 */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
        printf("Fail to load FT sysfonts, quit.\n");
        return -1;
  }
  //FTsymbol_set_SpaceWidth(1);
  //FTsymbol_set_TabWidth(4.5);
  /* 1.5 Initilize sys FBDEV 	初始化FB显示设备 */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;
  /* 1.6 Start touch read thread 启动触摸屏线程 (忽略) */
  /* 1.7 Start mouse event thread 启动鼠标响应线程 */
  printf("Start mouse_read thread...\n");
  pmostat=egi_start_mouseread(NULL, mouse_callback);
  if(pmostat==NULL) {
	printf("Fail to start mouseread thread!\n");
	return -1;
  }
  /* 1.8 Set sys FB mode 	设置显示模式: 是否直接操作FB映像数据， 设置横竖屏 */
  fb_set_directFB(&gv_fb_dev,false);//true);   /* 直接操作FB映像数据,不通过FBbuffer. 播放动画时可能出现撕裂线。 */
  fb_position_rotate(&gv_fb_dev,0);   /* 横屏模式 */

  /* <<<<<  End of EGI general init EGI初始化流程结束  >>>>>> */


   const int	ntg=12;	/* Total number of triangles */

   typedef struct triangle_data {
		EGI_POINT 	*pts;
		bool		selected;
   } TRIANG_DATA;
   TRIANG_DATA triangs[ntg];

   EGI_POINT	pts[3*12]=
		{
			{80,100}, {240,100}, {160, 160},	/* Triangle 0 */
			{0,80}, {80, 160}, {80,240-1},		/* Triangle 1 */
			{320-1,80}, {240,160}, {240,240-1},	/* Triangle 2 */
			{80,240-1}, {120, 160}, {160,240-1},	/* Triangle 3 */
			{160,240-1}, {200, 160}, {240,240-1},	/* Triangle 4 */
			{0,80}, {160,0}, {120, 60},		/* Triangle 5 */
			{160,0}, {200,60}, {320-1, 80},		/* Triangle 6 */
			{40,80}, {40,80}, {100,140},		/* An inclined line: Triangle 7 */
			{280,80}, {220,140}, {220,140},		/* An inclined line: Triangle 8 */
			{80, 80}, {80, 80}, {240, 80}, 		/* A Horizontal line: Triangle 9 */
			{160,170},{160,170+60}, {160,170+60},   /* A vertical line: Triangle 10 */
			{160, 60}, {160, 60},{160, 60}		/* A point: Triangle 11 */
		};

   /* Define triangles */
   for(i=0; i<ntg; i++) {
	triangs[i].pts=pts+i*3;
	triangs[i].selected=false;
   }

   /* FB background */
   fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKGRAY);
   draw_filled_circle2(&gv_fb_dev, 320-45, 25, 25, WEGI_COLOR_BLUE);
   FTsymbol_writeFB("EGI", 18,18, WEGI_COLOR_WHITE, 260, 2);
   FTsymbol_writeFB("Vector", 24,20, WEGI_COLOR_WHITE, 242, 20);

   /* Turn on FB filo and set map pointer */
   fb_filo_on(&gv_fb_dev);

   /* Draw triangles */
   fbset_color(WEGI_COLOR_GRAY);
   for( i=0; i<ntg; i++)
	   draw_filled_triangle(&gv_fb_dev, triangs[i].pts);

   /* Realtime modifying spline */
   while(1) {
	while( !mouse_request ) { tm_delayms(10);}; //{ usleep(1000); };

	/* Get moustXY */
	mpt.x=pmostat->mouseX; mpt.y=pmostat->mouseY;

#if 1	/* Check if touchs triangle */
	fb_filo_off(&gv_fb_dev);
	for( i=0; i<ntg; i++ ) {
	   if( triangs[i].selected==false ) {
		triangs[i].selected=true;
		if( point_intriangle(&mpt, triangs[i].pts) ) {
			fbset_color(WEGI_COLOR_RED);
			draw_filled_triangle(&gv_fb_dev, triangs[i].pts);

			/* A point */
			if(i==11) {
				draw_circle(&gv_fb_dev, triangs[i].pts->x, triangs[i].pts->y, 5);
			}
		}
	   }
	   else {  /* Clear and restore orignal color. */
		triangs[i].selected=false;
		if( point_intriangle(&mpt, triangs[i].pts)==false ) {
                        fbset_color(WEGI_COLOR_GRAY);
                        draw_filled_triangle(&gv_fb_dev, triangs[i].pts);

			/* A point */
			if(i==11) {
				fbset_color(WEGI_COLOR_DARKGRAY);
				draw_circle(&gv_fb_dev, triangs[i].pts->x, triangs[i].pts->y, 5);
			}
                }
	   }
	}
	fb_filo_on(&gv_fb_dev);
#endif

	/* If NO mouse_request, then it ONLY need to redraw mouse with new position */
	if( pmostat->KeysIdle ) {
		// printf("Mouse Key Idle\n"); /* NOTE: There usually (at lease) 1 No_request after a request! for the mouse_callback NOT ready yet!! */

		fb_render(&gv_fb_dev);
		draw_mcursor(pmostat->mouseX, pmostat->mouseY);
		tm_delayms(20);

		mouse_request=false;
		continue;
	}

   	/* Status pressing: See if touch a knot */
   	if( pmostat->LeftKeyDown ) {
		printf(" LeftKeyDown  %d,%d \n", pmostat->mouseX, pmostat->mouseY);

	}
	/*  Else Status pressed_hold */
   	else if( pmostat->LeftKeyDownHold )
	{
		printf("MLeftKeyDownHold\n");
	}
	/* Else: Left key is releasing. */
	else if(pmostat->LeftKeyUp) {
	}

	/* Render to display */
	fb_render(&gv_fb_dev);
	draw_mcursor(pmostat->mouseX, pmostat->mouseY);
	tm_delayms(10);

	/* Reset mouse_request at last !!! */
	mouse_request=false;
   }

  /* <<<<<  3. EGI general release EGI释放流程	 >>>>>> */
  /* 根据初始化流程做对应的释放　*/
  /* 3.1 Release sysfonts and appfonts 释放所有FreeTpype字体 (忽略) */
  /* 3.2 Release all symbol pages 释放所有图形/符号映像 (忽略) */
  /* 3.3 Release FBDEV and its data 释放FB显示设备及数据 */
  printf("fb_filo_flush() and release_fbdev()...\n");
  fb_filo_flush(&gv_fb_dev);
  release_fbdev(&gv_fb_dev);
  /* 3.4 Release virtual FBDEV 释放虚拟FB显示设备 (忽略) */
  /* 3.ntg End touch read thread  结束触摸屏线程 (忽略) */
  /* 3.6 End mouse event thread  结束鼠标响应线程 */
  printf("egi_end_mouseread()...\n");
  egi_end_mouseread();
  /* 3.7 结束日志记录  (忽略) */

  return 0;
}


/*-------------------------------------
        FTsymbol WriteFB TXT
@txt:           Input text
@px,py:         LCD X/Y for start point.
--------------------------------------*/
void FTsymbol_writeFB(char *txt, int fw, int fh, EGI_16BIT_COLOR color, int px, int py)
{
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, 	/* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,      /* fw,fh, pstr */
                                        320, 1, 0,      		/* pixpl, lines, fgap */
                                        px, py,                         /* x0,y0, */
                                        color, -1, 220,                 /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */
}

/*-----------------------------------------------------------
		Callback for mouse input

------------------------------------------------------------*/
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus)
{
 	/* Wait until main thread finish last mouse_request */
	 while( mouse_request ) { usleep(1000); };

	/* NOPE! */
//	mouseX=mostatus->mouseX;
//	mouseY=mostatus->mouseY;
//	mouseZ=mostatus->mouseZ;

	/* Request for respond */
	mouse_request = true;
        //printf("Mouse coord: X=%d, Y=%d, Z=%d, dz=%d\n", pmostat->mouseX, pmostat->mouseY, mouseZ, mdZ);
}


/*------------------------------------
	   Draw mouse
-------------------------------------*/
static void draw_mcursor(int x, int y)
{
        static EGI_IMGBUF *mcimg=NULL;

#ifdef LETS_NOTE
	#define MCURSOR_ICON_PATH 	"/home/midas-zhou/egi/mcursor.png"
	#define TXTCURSOR_ICON_PATH 	"/home/midas-zhou/egi/txtcursor.png"
#else
	#define MCURSOR_ICON_PATH 	"/mmc/mcursor.png"
	#define TXTCURSOR_ICON_PATH 	"/mmc/txtcursor.png"
#endif

        if(mcimg==NULL)
                mcimg=egi_imgbuf_readfile(MCURSOR_ICON_PATH);

	/* Always draw mouse in directFB mode */
	/* directFB and Non_directFB mixed mode will disrupt FB synch, and make the screen flash?? */
   fb_filo_off(&gv_fb_dev);
	//mode=fb_get_FBmode(&gv_fb_dev);
	fb_set_directFB(&gv_fb_dev,true);

        /* OPTION 1: Use EGI_IMGBUF */
        egi_subimg_writeFB(mcimg, &gv_fb_dev, 0, -1, pmostat->mouseX, pmostat->mouseY ); /* subnum,subcolor,x0,y0 */

        /* OPTION 2: Draw geometry */

	/* Restore FBmode */
	fb_set_directFB(&gv_fb_dev,false);
	//fb_set_directFB(&gv_fb_dev, mode);
   fb_filo_on(&gv_fb_dev);

}



