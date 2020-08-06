/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A program to draw basic geometries of EGI.

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_utils.h>


int main(int argc, char **argv)
{
  int i;

  /* <<<<<<  1. EGI general init  EGI初始化流程 >>>>>> */
  /* 对不必要的一些步骤可以忽略 */

  /* 1.1 Start sys tick 	开启系统计数 (忽略) */
  /* 1.2 Start egi log 		开启日志记录 (忽略) */
  /* 1.3 Load symbol pages 	加载图形/符号映像 (忽略) */
  /* 1.4 Load freetype fonts 	加载FreeType字体 (忽略) */

  /* 1.5 Initilize sys FBDEV 	初始化FB显示设备 */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;

  /* 1.6 Start touch read thread 启动触摸屏线程 (忽略) */

  /* 1.7 Set sys FB mode 	设置显示模式: 是否直接操作FB映像数据， 设置横竖屏 */
  fb_set_directFB(&gv_fb_dev,true);   /* 直接操作FB映像数据,不通过FBbuffer. 播放动画时可能出现撕裂线。 */
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

   /* 在屏幕上绘制几何图形  */
   /* 2.1 清屏 */
   clear_screen( &gv_fb_dev, WEGI_COLOR_GRAY5);

   /* 2.2 绘制线条 */
   fbset_color(WEGI_COLOR_BLACK); /* 设置画笔颜色 */
   for(i=0; i<7; i++)
	draw_wline_nc(&gv_fb_dev, 30+i*20, 10, 30+i*20, 240-10, i);	/* 绘制垂直线条 */
   fbset_color(WEGI_COLOR_WHITE);
   for(i=0; i<5; i++)
	draw_wline(&gv_fb_dev, 5, 50+i*20, 320-5, 50+i*20, i);		/* 绘制水平线条 */

   fbset_speed(6);

   /* 绘制分布图 */
   fbset_color(WEGI_COLOR_GYBLUE);
   for(i=0; i<300; i++)
   	draw_line(&gv_fb_dev, i,100, i, 100-8000*mat_normal_distribute(i, 240, 40));
   fbset_color(WEGI_COLOR_BLUEFLOWER);
   for(i=0; i<300; i++)
	draw_line(&gv_fb_dev, i,120, i, 120-5000*mat_rayleigh_distribute(i, 30));


   /* 2.3 绘制距形 */
   fbset_color(WEGI_COLOR_MAGENTA);
   draw_wrect(&gv_fb_dev, 10, 35, 100, 80,3);
   fbset_color(WEGI_COLOR_RED);
   draw_roundcorner_wrect(&gv_fb_dev, 20, 15, 125, 60, 10, 1);

   /* 2.4 绘制条形图 */
   for(i=0; i<4; i++) {
	   fbset_color(COLOR_RGB_TO16BITS(0x99+i*0x18,0x99+i*0x18,0x99+i*0x18));
	   draw_filled_rect(&gv_fb_dev, 190+i*30, 210-50*i-50, 190+(i+1)*30, 230);
   }

   /* 2.5 绘制三角形 */
   fbset_color(WEGI_COLOR_GREEN);
   draw_filled_triangle(&gv_fb_dev, tripoints);

   /* 2.6 绘制圆形 */
   /* 2.6.1 圆形 */
   fbset_color(WEGI_COLOR_CYAN);
   draw_circle(&gv_fb_dev, 240, 80, 65);
   /* 2.6.2 填充圆形 */
   draw_filled_circle(&gv_fb_dev, 180, 60, 40);
   /* 2.6.3 宽度弧段 */
   fbset_speed(9);
   fbset_color(WEGI_COLOR_YELLOW);
   draw_warc(&gv_fb_dev, 240, 80, 55, -MATH_PI*13/24, MATH_PI*5/6, 7);
   /* 2.6.4 环形 */
   fbset_color(WEGI_COLOR_BLUE);
   draw_filled_annulus(&gv_fb_dev, 40, 200, 40, 7);

   /* 2.7 绘制饼图 */
   fbset_color(WEGI_COLOR_ORANGE);
   draw_filled_pieSlice(&gv_fb_dev, 170, 180, 50, MATH_PI/5, MATH_PI/5*9);
   fbset_color(WEGI_COLOR_DARKGOLDEN);
   draw_filled_pieSlice(&gv_fb_dev, 180, 180, 60, -MATH_PI/5, MATH_PI/5);

   fbset_speed(9);
   /* 2.8 绘制折线 */
   fbset_color(WEGI_COLOR_RED);
   draw_pline(&gv_fb_dev, mpoints, 6, 7);

   /* 2.9 绘制透明圆角矩形 */
   draw_blend_filled_roundcorner_rect(&gv_fb_dev, 30, 120, 170, 200, 15, WEGI_COLOR_LTGREEN, 100);

   /* 2.10 绘制一个笑脸 */
   fbset_color(WEGI_COLOR_GRAY5);
   draw_filled_circle(&gv_fb_dev, 295, 180, 5);
   draw_filled_circle(&gv_fb_dev, 265, 180, 5);
   draw_warc(&gv_fb_dev, 280, 190, 20, MATH_PI/6, MATH_PI*5/6, 3);

   /* 2.10 绘制一个按钮 */
   fbset_speed(0);
   draw_filled_rect2(&gv_fb_dev, COLOR_24TO16BITS(0x66cc99), 45, 135, 45+110, 135+50);
   draw_button_frame( &gv_fb_dev, 1, COLOR_24TO16BITS(0x66cc99), 45, 135, 110, 50, 4); /* 按下 */
   usleep(500000);
   draw_button_frame( &gv_fb_dev, 0, COLOR_24TO16BITS(0x66cc99), 45, 135, 110, 50, 4); /* 松开 */

   /* 2.11 绘制spline曲线 */
   EGI_POINT   pts[9]={{48, 126}, {192, 128}, {195, 22}, {259, 39}, {215, 52}, {227, 139}, {201, 202}, {63, 187}, {42, 126} };
   clear_screen( &gv_fb_dev, WEGI_COLOR_GRAY5);
   fbset_color(WEGI_COLOR_LTBLUE);
   /* 标注型值点 */
   for(i=0; i<9; i++)
	draw_circle(&gv_fb_dev, pts[i].x, pts[i].y, 15);
   /* 绘制3次spline参数曲线 */
   fbset_color(WEGI_COLOR_PINK);
   draw_spline2(&gv_fb_dev, 9, pts, 1, 5);

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
  /* 3.6 结束日志记录  (忽略) */

return 0;
}


