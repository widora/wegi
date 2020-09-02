/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A program to test EGI draw curve functions. --- Use mouse!

Note:
1. Applying FILO to speed up mouse rendering ONLY suitable for low
   speed hardware! For hight speed this may lead to overlapped image.

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
//static bool flagPickPoints;
static bool actRectSelect;	    /* activate rectanguler selection */
EGI_BOX   selectBox;

static int  mouseX, mouseY, mouseZ; /* Mouse point position */


EGI_POINT   tchpt;	/* Touch point */

void FTsymbol_writeFB(char *txt, int fw, int fh, EGI_16BIT_COLOR color, int px, int py);
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus);
static void draw_mcursor(int x, int y);


/*----------------------------
	     MAIN
-----------------------------*/
int main(int argc, char **argv)
{
  int i;
  char strtmp[32];

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


   /**************************************
    *  	      2. Draw Geometry 程序部分
    ***************************************/
   /* 三角形顶点坐标 */
   EGI_POINT	tripoints[3]={ {60,20}, {140,80}, {20,100} };
   /* 折线顶点坐标 */
   EGI_POINT	mpoints[6]={ {10,230}, {60,100}, {100, 190}, {180,50}, {230,120}, {300,20} };

   fbset_speed(10); /* 设置画点速度 */

#if 1
   /* 在屏幕上绘制几何图形  */
   /* 2.1 清屏 */
   clear_screen( &gv_fb_dev, WEGI_COLOR_GRAY5);

   /* 2.2 绘制线条 */
   fbset_color(WEGI_COLOR_BLACK); /* 设置画笔颜色 */
   for(i=0; i<21; i++)
	draw_wline_nc(&gv_fb_dev, 15+i*20, 10, 15+i*20, 240-10, i/3+1);	/* 绘制垂直线条 */

   /* 2.8 绘制折线 */
   fbset_color(WEGI_COLOR_FIREBRICK);
   draw_pline(&gv_fb_dev, mpoints, 6, 5);

   /* Non_parametric Spline */
   fbset_speed(0);
   fbset_color(WEGI_COLOR_GREEN);
   draw_spline(&gv_fb_dev, 6, mpoints, 0, 5);
   fbset_color(WEGI_COLOR_BLUE);
   draw_spline(&gv_fb_dev, 6, mpoints, 1, 5);
   fbset_color(WEGI_COLOR_PINK);
   draw_spline(&gv_fb_dev, 6, mpoints, 2, 5);

   fbset_color(WEGI_COLOR_WHITE);
   draw_bezier_curve(&gv_fb_dev, 6, mpoints, NULL, 1);

   fb_render(&gv_fb_dev);
   getchar();

#endif


   /* =======  Test: draw_bezier / draw_spline2 / draw_spline  ======== */
   #define TEST_SPLINE2		0
   #define TEST_BEZIER 		0
   #define TEST_BSPLINE		1


#ifdef LETS_NOTE
   int		np=24;
#else
   int 		np=6;//9;
#endif

   int		deg;
   if(np>3)deg=3;
   else deg=np-1;
   int		npt=-1;
// int		step;
   bool		check_threshold=false; /* Check threshold to avoid moving points when just touching the point. */

   EGI_POINT	cptM={65,20}; //{20,20}; //{120,20};	/* Middle of two dumbbells */
   EGI_POINT	cptL,cptR;	/* Center of L/R dumbbells */
   int		wscale=8;	/* Weight value scale pixels/ws[] */
   int		whv;		/* Half of weight value pixels */
   int 		bias=4/wscale;  /* [-bias +bias] ws[] is 0 */
   bool	adjust_ws=false;

#ifdef LETS_NOTE
   EGI_POINT    pts[np];
   float	ws[np];

   for(i=0; i<np; i++) {
	 pts[i].x= 100+50*i;
	 pts[i].y= 350+300*sin(pts[i].x*2*MATH_PI/300);
	 ws[i]=10.0;
   }

   EGI_POINT	 rpts[np];
#else

   /* A little pterosaur for draw_spline2() */
   // EGI_POINT	  pts[9]={{89, 78}, {48, 63}, {98, 33}, {133, 117}, {218, 100}, {177, 172}, {265, 182}, {116, 198}, {92, 78} };
   // float	  ws[9]={ 10, 10, 10, 10, 10, 10, 10, 10, 10 };

   /* Symmetric test, heart shape */
   // EGI_POINT	  pts[9]={{140, 50}, {90, 30}, {20, 30}, {20, 130}, {160, 220}, {300, 130}, {300, 30}, {230, 30}, {180, 50} };
   // float	  ws[9]={ 10, 10, 10, 10, 10, 10, 10, 10, 10 };

   /* Two_leaves shape */
   // EGI_POINT    pts[9]={{144, 223}, {97, 207}, {85, 180}, {48, 48}, {144, 183}, {230, 47}, {203, 180}, {189, 213}, {144, 223}, };
   // float	ws[9]={10.0, 10.0, 10.0, 156.3, 134.6, 156.3, 10.0, 10.0, 10.0};

   /* A fish for NURBS */
   EGI_POINT    pts[9]= {{252, 127}, {247, 157}, {127, 189}, {72, 59}, {45, 233}, {0, 13}, {80, 169}, {140, 0}, {252, 127}, };
   float	ws[9]={10.0, 10.0, 10.0, 6.8, 11.9, 10.6, 10.6, 8.1, 10.0 };


   /* Up Going Waves for draw_spline() */
   //EGI_POINT	 pts[9]={ {10,230}, {50,100}, {90, 190}, {130, 150}, {170,180}, {210,50}, {250,120}, {280,50}, {310,10} };
   //float	  ws[9]={ 10, 10, 10, 10, 10, 10, 10, 10, 10 };

   /* Weight values */
   //float	  ws[9]={ 10, 5.5, 20, 10, 55.5, 20, 20, 10, 10 };  /* Put 0 to invalidate the control point */

   EGI_POINT	 rpts[9];
#endif


   /* FB background */
   fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKGRAY); //GRAY5);
   draw_filled_circle2(&gv_fb_dev, 320-45, 25, 25, WEGI_COLOR_DARKGREEN);
   FTsymbol_writeFB("EGI", 18,18, WEGI_COLOR_BLACK, 260, 2);
   FTsymbol_writeFB("CURVE", 20,20, WEGI_COLOR_BLACK, 242, 20);

   /* Turn on FB filo and set map pointer */
   fb_filo_on(&gv_fb_dev);

#if 0  /* Draw step_changing spline */
   step=20;
while(1) {
   fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */

   /* change point */
   pts[4].y +=step;
   if(pts[4].y > 240) step=-20;
   else if(pts[4].y <0 ) step=20;

   /* Draw spline */
   fbset_color(WEGI_COLOR_LTBLUE);
   for(i=0; i<np; i++)
	draw_circle(&gv_fb_dev, pts[i].x, pts[i].y, 5);
   fbset_color(WEGI_COLOR_PINK);
   //draw_spline2(&gv_fb_dev, np, pts, 0, 5);
   draw_spline2(&gv_fb_dev, np, pts, 1, 5);
   fb_render(&gv_fb_dev);
}
#endif

   /* Draw spline */
   fbset_color(WEGI_COLOR_LTBLUE);
   for(i=0; i<np; i++)
	draw_circle(&gv_fb_dev, pts[i].x, pts[i].y, 5);
   fbset_color(WEGI_COLOR_PINK);
 #if TEST_SPLINE2
   //draw_spline2(&gv_fb_dev, np, pts, 0, 5);
   draw_spline2(&gv_fb_dev, np, pts, 1, 5);
 #elif TEST_BEZIER
   draw_bezier_curve(&gv_fb_dev, np, pts, NULL, 5);
 #elif TEST_BSPLINE
   fbset_color(WEGI_COLOR_PINK);
   draw_Bspline(&gv_fb_dev, np, pts, NULL, deg, 5);
   for(i=0; i<np; i++)
	rpts[i]=pts[np-1-i];
   fbset_color(WEGI_COLOR_GREEN);
   draw_Bspline(&gv_fb_dev, np, rpts, NULL, deg, 1);

 #else
   draw_spline(&gv_fb_dev, np, pts, 1, 5);
 #endif
   fb_render(&gv_fb_dev);

   getchar();

   /* Realtime modifying spline */
   while(1) {
	while( !mouse_request ) { tm_delayms(10);}; //{ usleep(1000); };

	/* If NO mouse_request, then it ONLY need to redraw mouse with new position */
	if( pmostat->KeysIdle ) {
		printf("Mouse Idle\n"); /* NOTE: There usually (at lease) 1 No_request after a request! for the mouse_callback NOT ready yet!! */

		fb_render(&gv_fb_dev);
		draw_mcursor(pmostat->mouseX, pmostat->mouseY);
		tm_delayms(30);

		mouse_request=false;
		continue;
	}

   	/* Status pressing: See if touch a knot */
   	if( pmostat->LeftKeyDown ) {
		printf(" LeftKeyDown  %d,%d \n", pmostat->mouseX, pmostat->mouseY);
		tchpt.x=pmostat->mouseX; tchpt.y=pmostat->mouseY;

		/* 1. First, Check if a control piont is active already and the mouse touchs adjusting dumbbells */
		if(npt>=0) {
			//whv=ws[npt]*wscale/2;  /* half */
			whv=10*sqrt(ws[npt]*wscale/2);  /* half */
			#if 0 /* Disable left dumbbell */
			cptL.x=cptM.x-whv; cptL.y=cptM.y
			#endif
			cptR.x=cptM.x+(whv>0?whv+bias:0); cptR.y=cptM.y;
			if(point_incircle(&tchpt, &cptR, 15)) {
				//printf("Right ---\n");
				adjust_ws=true;
			}
			#if 0 /* Disable left dumbbell */
			else if(point_incircle(&tchpt, &cptL, 15)) {
				//printf("Left ---\n");
				adjust_ws=true;
			}
			#endif
			else
				adjust_ws=false;
		}

		/* 2. Then, Check if otherwise the mouse just clicks/picks any knot(control point). */
		if( !adjust_ws ) {
			for(i=0;i<np;i++) {
				if(point_incircle(&tchpt, pts+i, 10)) {
					check_threshold=true;
					npt=i;
					break;
				}
			}
			/* Deselect all control points */
			if(i==np) /* no knot touched */
				npt=-1;
		}

		/* 3. Last, if no knot selected, activate rectanglur_selection  */
		if ( npt<0 ) {
			actRectSelect=true;
			selectBox.startxy=tchpt;
			selectBox.endxy=tchpt;
		}
		else
			actRectSelect=false;

	}
	/*  Else Status pressed_hold */
   	else if( pmostat->LeftKeyDownHold )
	{
		printf("MLeftKeyDownHold\n");
		tchpt.x=pmostat->mouseX; tchpt.y=pmostat->mouseY;

		/* 1. If adjust ws on dumbbell */
		if(adjust_ws) {
			if( pmostat->mouseX > cptM.x+bias )
				//ws[npt]=2*(pmostat->mouseX-cptM.x-bias)/wscale;
				ws[npt]= 2.0*((pmostat->mouseX-cptM.x-bias)*(pmostat->mouseX-cptM.x-bias)/100.0)/wscale;
			#if 0 /* Disable left dumbbell */
			else if( pmostat->mouseX < cptM.x-bias )
				//ws[npt]=2*(cptM.x-bias-pmostat->mouseX)/wscale;
				ws[npt]= 2.0*((cptM.x-bias-pmostat->mouseX)*(pmostat->mouseX-cptM.x-bias)/100.0)/wscale;
			#endif
			else  /*  Zero zone: [cptM.x-5, cptM.x+5] */
				ws[npt]=0;
		}

		/* 2. If selected a knot and NOT adjust_ws, then update control points coord.  */
		if( npt>=0 && !adjust_ws ) {
			/* Update control points coord */
			/* Note: first touch should avoid moving the points within threshold. */
			if(check_threshold) {
//				if( (pts[npt].x-pmostat->mouseX)*(pts[npt].x-pmostat->mouseX)+(pts[npt].y-pmostat->mouseY)*(pts[npt].y-pmostat->mouseY) >25 )
			} else {
				pts[npt].x=pmostat->mouseX;
				pts[npt].y=pmostat->mouseY;
			}
			check_threshold=false;

			/* If coincide with other knots */
			for(i=0; i<np; i++) {
				if(i==npt)continue;
				if(point_incircle(&tchpt, pts+i, 15)) {
					pts[npt].x=pts[i].x;
					pts[npt].y=pts[i].y;
				}
			}
		}

		/* 3. If no knot selected, then actRectSelect.*/
		if(actRectSelect) {
			selectBox.endxy=tchpt;
		}
	}
	/* Else: Left key is releasing. */
	else if(pmostat->LeftKeyUp) {
		actRectSelect=false;
	}

	#if 1 /* TEST ---- */
	EGI_POINT mypt={320-45, 25};
	if( !adjust_ws && point_incircle(&tchpt, &mypt, 15)) {
		printf("Pts[]: ");
		for( i=0; i<np; i++)
			printf("{%d, %d}, ",pts[i].x, pts[i].y);
		printf("\n");
		printf("ws[]: ");
		for( i=0; i<np; i++)
			printf("%.1f, ",ws[i]);
		printf("\n");
	}
	#endif

	/* --- Redraw spline --- */
	fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */

	/* 1. Draw mark circle */
	for(i=0; i<np; i++) {
		if( npt>=0 && i==npt ) {
		   	fbset_color(WEGI_COLOR_YELLOW);
			draw_filled_circle(&gv_fb_dev, pts[i].x, pts[i].y, 5);

			#if 1/* Display point weight value */
			sprintf(strtmp,"w=%.1f",ws[npt]);
			FTsymbol_writeFB(strtmp, 16,16,WEGI_COLOR_YELLOW, 5, 3);
			#endif

		} else {
		   	fbset_color(WEGI_COLOR_LTBLUE);
			draw_circle(&gv_fb_dev, pts[i].x, pts[i].y, 5);
		}
	}

	/* 2. Draw curves */
	fbset_color(WEGI_COLOR_PINK);

	#if TEST_SPLINE2  	/* --- 2.1 Draw spline2 --- */
	draw_spline2(&gv_fb_dev, np, pts, 1, 5);

	#elif  TEST_BEZIER 	/* --- 2.2 Draw Bezier curve  --- */
	fbset_color(WEGI_COLOR_GRAY2);
	for(i=0; i<np-1; i++) {
		draw_dash_wline(&gv_fb_dev,pts[i].x,pts[i].y,pts[i+1].x,pts[i+1].y, 1, 10, 10);
	}
	fbset_color(WEGI_COLOR_PINK);
	draw_bezier_curve(&gv_fb_dev, np, pts, ws, 5);

	#elif TEST_BSPLINE 	/* --- 2.3 Draw Bspline/NURBS curve  --- */

	//draw_filled_circle2(&gv_fb_dev, 320-45, 25, 25, WEGI_COLOR_DARKGREEN);
	//FTsymbol_writeFB("EGI", 18,18, WEGI_COLOR_BLACK, 260, 2);
	//FTsymbol_writeFB("NURBS", 20,20, WEGI_COLOR_BLACK, 242, 20);

	/* Draw dash lines between control points  */
	fbset_color(WEGI_COLOR_GRAY2);
	for(i=0; i<np-1; i++) {
		draw_dash_wline(&gv_fb_dev,pts[i].x,pts[i].y,pts[i+1].x,pts[i+1].y, 1, 10, 10);
	}

	/* Draw B_Spline */
	fbset_color(WEGI_COLOR_PINK);
	draw_Bspline(&gv_fb_dev, np, pts, ws, deg, 5); /* 5, ws==NULL for B_Spline */

	#if 0 /* TEST ----- reversed B_spline */
	for(i=0; i<np; i++)
		rpts[i]=pts[np-1-i];
   	fbset_color(WEGI_COLOR_GREEN);
	draw_Bspline(&gv_fb_dev, np, rpts, NULL, deg, 1); /* ws[] all same */
	#endif

	/* Draw weight value adjusting dumbbell  */
	if( npt>=0 ) {
		//whv=ws[npt]*wscale/2;  /* half */
		whv=10*sqrt(ws[npt]*wscale/2);  /* half */
		fbset_color(WEGI_COLOR_ORANGE);
		#if 0 /* Disable left dumbbell */
		cptL.x=cptM.x-(whv>0?whv+bias:0);
		draw_filled_circle(&gv_fb_dev, cptL.x, cptM.y, 10); /* left part */
		#endif
		cptR.x=cptM.x+(whv>0?whv+bias:0);
		draw_filled_circle(&gv_fb_dev, cptR.x, cptM.y, 10); /* left part */
		fbset_color(WEGI_COLOR_GRAY2);
		draw_wline(&gv_fb_dev, cptM.x-whv, cptM.y, cptM.x+whv, cptM.y, 1);
	}

	#else /* --- 2.4 Draw spline  --- */
	draw_spline(&gv_fb_dev, np, pts, 1, 5);

   	#endif

	/* 3. Draw selecton area */
	if( actRectSelect ) {
		fbset_color(WEGI_COLOR_BLACK);
		draw_rect(&gv_fb_dev,selectBox.startxy.x, selectBox.startxy.y, selectBox.endxy.x, selectBox.endxy.y);
		draw_blend_filled_rect(&gv_fb_dev, selectBox.startxy.x, selectBox.startxy.y, selectBox.endxy.x, selectBox.endxy.y,
						WEGI_COLOR_GRAY, 120);
	}

	/* 4. Draw mouse cursor */
	//draw_mcursor(pmostat->mouseX, pmostat->mouseY);

FB_RENDER:
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
  /* 3.5 End touch read thread  结束触摸屏线程 (忽略) */
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
                                        color, -1, 255,                 /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      	/*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
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



