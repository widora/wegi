/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A 'Hello World' program for EGI.

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_FTsymbol.h>   /* FreeType2 矢量字体显示引擎 */


int main(int argc, char **argv)
{

  /* <<<<<<  1. EGI general init  EGI初始化流程 >>>>>> */
  /* 对不必要的一些步骤可以省略 */

  /* 1.1 Start sys tick 开启系统计数 */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();

  /* 1.2 Start egi log 开启日志记录 */
  #if 0
  printf("egi_init_log()...\n");
  if(egi_init_log("/mmc/log_scrollinput")!=0) {
        printf("Fail to init egi logger, quit.\n");
        return -1;
  }
  #endif

  /* 1.3 Load symbol pages 加载图形/符号映像 */
  #if 0
  printf("FTsymbol_load_allpages()...\n");
  if(FTsymbol_load_allpages() !=0) /* FT derived sympg_ascii */
        printf("Fail to load FTsym pages! go on anyway...\n");
  printf("symbol_load_allpages()...\n");
  if(symbol_load_allpages() !=0) {
        printf("Fail to load sym pages, quit.\n");
        return -1;
  }
  #endif

  /* 1.4 Load freetype fonts 加载FreeType字体 */
  /* 由于要用到sysfont的bold字体，在/home目录下建立一个egi.conf文件，
     收入如下内容：
     [SYS_FONTS]
     bold  = /mmc/fonts/hanserif/SourceHanSerifSC-Heavy.otf
     字体文件可以按自己喜好选定。
  */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
        printf("Fail to load FT sysfonts, quit.\n");
        return -1;
  }
  #if 0
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
        printf("Fail to load FT appfonts, quit.\n");
        return -1;
  }
  #endif

  /* 1.5 Initilize sys FBDEV 初始化FB显示设备 */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;

#if 0  /* 1.6 Start touch read thread 启动触摸屏线程 */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
        return -1;
#endif

  /* 1.7 Set sys FB mode 设置显示模式: 是否直接操作FB映像数据， 设置横竖屏 */
  fb_set_directFB(&gv_fb_dev,true);   /* 直接操作FB映像数据,不通过FBbuffer. 播放动画时可能出现撕裂线。 */
  fb_position_rotate(&gv_fb_dev,0);   /* 横屏模式 */

  /* <<<<<  End of EGI general init EGI初始化流程结束  >>>>>> */



   /**************************************
    *  	      Hello World程序部分
    ***************************************/


   /*  2. 在屏幕上书写多行文字  */
   /* 2.1 Clear scree 用黑色清屏 */
   clear_screen( &gv_fb_dev, WEGI_COLOR_DARKGRAY);
   /* 2.2 书写多行文字 */
   FTsymbol_uft8strings_writeFB(  &gv_fb_dev, egi_sysfonts.bold,          		        /* FB设备，字体 FBdev, fontface */
      	                        45, 45,(const unsigned char *)"Hello World!\n    世界你好!",    /* 字宽，字高，字符 fw,fh, pstr */
               	                320, 6, 10,                           	   			/* 每行长度，行数，行间距 pixpl, lines, gap */
                       	        10, 40,                                    			/* 起点坐标 x0,y0, */
                               	WEGI_COLOR_PINK, -1, -1,       					/* 字体颜色, fontcolor, transcolor,opaque */
                                NULL, NULL, NULL, NULL);      					/* int *cnt, int *lnleft, int* penx, int* peny */


  sleep(10);

  /* <<<<<  3. EGI general release EGI释放流程	 >>>>>> */

  /* 3.1 Release sysfonts and appfonts 释放所有FreeTpype字体 */
  printf("FTsymbol_release_allfonts()...\n");
  FTsymbol_release_allfonts();

  /* 3.2 Release all symbol pages 释放所有图形/符号映像 */
  printf("symbol_release_allpages()...\n");
  symbol_release_allpages();
  printf("FTsymbol_release_allpage()...\n");
  FTsymbol_release_allpages(); /* release FT derived symbol pages: sympg_ascii */

  /* 3.3 Release FBDEV and its data 释放FB显示设备及数据 */
  printf("fb_filo_flush() and release_fbdev()...\n");
  fb_filo_flush(&gv_fb_dev);
  release_fbdev(&gv_fb_dev);

  /* 3.4 Release virtual FBDEV 释放虚拟FB显示设备 */
  #if 0
  printf("release virtual fbdev...\n");
  release_virt_fbdev(&vfb);
  #endif

  /* 3.5 End touch read thread  结束触摸屏线程 */
  printf("egi_end_touchread()...\n");
  egi_end_touchread();

  /* 3.6 结束日志记录 */
  #if 0
  printf("egi_quit_log()...\n");
  egi_quit_log();
  #endif
  printf("<-------  END  ------>\n");

return 0;
}


