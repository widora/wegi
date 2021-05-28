/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test multi_threads application for FTsymbol.

Journal:
2021-05-27: Start programming...
	    Finish.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <getopt.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_cstring.h"

const char *ptext="话说天下大势，分久必合，合久必分。周末七国分争，并入于秦。及秦灭之后，楚、汉分争，又并入于汉。汉朝自高祖斩白蛇而起义，一统天下，\
后来光武中兴，传至献帝，遂分为三国。推其致乱之由，殆始于桓、灵二帝。";

struct context_arg {
	int id;
	int x0;
	int y0;
	EGI_16BIT_COLOR color;
};
void *display_text(void *arg);


int main(int argc, char **argv)
{
	int i;


  /* <<<<<<  1. EGI general init  EGI初始化流程 >>>>>> */
  /* 对不必要的一些步骤可以忽略 */

  /* 1.1 Start sys tick         开启系统计数 */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();
  /* 1.2 Start egi log          开启日志记录 (忽略) */
  /* 1.3 Load symbol pages      加载图形/符号映像 (忽略) */
  /* 1.4 Load freetype fonts    加载FreeType字体 */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
        printf("Fail to load FT sysfonts, quit.\n");
        return -1;
  }
  //FTsymbol_set_SpaceWidth(1);
  //FTsymbol_set_TabWidth(4.5);
  /* 1.5 Initilize sys FBDEV    初始化FB显示设备 */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;
  /* 1.6 Start touch read thread 启动触摸屏线程 (忽略) */
  /* 1.7 Start mouse event thread 启动鼠标响应线程(忽略) */
  /* 1.8 Set sys FB mode        设置显示模式: 是否直接操作FB映像数据， 设置横竖屏 */
  fb_set_directFB(&gv_fb_dev,false);//true);   /* 直接操作FB映像数据,不通过FBbuffer. 播放动画时可能出现撕裂线。 */
  fb_position_rotate(&gv_fb_dev,0);   /* 横屏模式 */

  /* <<<<<  End of EGI general init EGI初始化流程结束  >>>>>> */


  /* <<<<< 2. MAIN Process >>>>> */
  pthread_t thread_ids[3];
  struct context_arg ctxt_args[3]={0};

  for(i=0; i<3; i++) {
	/* Assign params */
	ctxt_args[i].id=i;
	ctxt_args[i].x0=0;
	ctxt_args[i].y0=i*80;
	ctxt_args[i].color = i==0 ? WEGI_COLOR_RED : (i==1 ? WEGI_COLOR_GREEN : WEGI_COLOR_OCEAN );

	/* Start threads */
	pthread_create(thread_ids+i, NULL, display_text, ctxt_args+i);
	pthread_detach(thread_ids[i]);
  }

  /* Joint threads */
  while(1) {
	tm_delayms(100);
  }


  /* <<<<<  3. EGI general release EGI释放流程   >>>>>> */
  /* 根据初始化流程做对应的释放　*/
  /* 3.1 Release sysfonts and appfonts 释放所有FreeTpype字体 (忽略) */
   FTsymbol_release_library(&egi_sysfonts);
  /* 3.2 Release all symbol pages 释放所有图形/符号映像 (忽略) */
  /* 3.3 Release FBDEV and its data 释放FB显示设备及数据 */
  printf("fb_filo_flush() and release_fbdev()...\n");
  fb_filo_flush(&gv_fb_dev);
  release_fbdev(&gv_fb_dev);
  /* 3.4 Release virtual FBDEV 释放虚拟FB显示设备 (忽略) */
  /* 3.5 End touch read thread  结束触摸屏线程 (忽略) */
  /* 3.6 End mouse event thread  结束鼠标响应线程 (忽略) */
  /* 3.7 结束日志记录  (忽略) */

  return 0;

}


/*----------------------------
	A thread function
To display the text.

@arg:	Pointer to (x0,y0).
-----------------------------*/
void *display_text(void *arg)
{
	EGI_IMGBUF *imgbuf=NULL;
	char pstr[1024];
	int charlen;
	char *pch;
	int off;
	int px, py;
	if(arg==NULL)
		return (void *)-1;

	struct context_arg *ctxt_arg=(struct context_arg *)arg;

	imgbuf=egi_imgbuf_createWithoutAlpha(80, 320, WEGI_COLOR_BLACK);

	pch= (char *)ptext;

	px=0; py=0;

  while(1) {
	printf("thread[%d]: Start write...\n", ctxt_arg->id);

	/* Prepare pstr */
	if( *pch != '\0' ) {
		charlen = cstr_charlen_uft8( (UFT8_PCHAR)pch);
		strncpy(pstr, pch, charlen);
		pstr[charlen]=0;
		pch += charlen;
//		off=pch-ptext;
//		strncpy(pstr, ptext, off < 1024-1 ? off:1024-1 );
//		pstr[off < 1024-1 ? off:1024-1]=0;

	}
	else {
		//printf("thread[%d]: End of text!\n",ctxt_arg->id);
		pch=(char *)ptext;
		continue;
	}

	/* Clear area */
//	egi_imgbuf_resetColorAlpha(imgbuf, WEGI_COLOR_BLACK, -1);

	/* If line is full, next line. */
	if( px + 15 > 320 ) {  /* Assume cha width as 15pix */
		px=0;
		py += 15+5;
	}

	/* Write text to imgbuf */
        FTsymbol_uft8strings_writeIMG(  imgbuf, egi_sysfonts.regular, /* FBdev, fontface */
                                        15, 15,(UFT8_PCHAR)pstr,	/* fw,fh, pstr */
                                        320, 4, 5,      		/* pixpl, lines, fgap */
                                        px, py,     			/* x0,y0, */
                                        ctxt_arg->color, -1, 255,      	/* fontcolor, transcolor,opaque */
                                        NULL, NULL, &px, &py);        /* int *cnt, int *lnleft, int* penx, int* peny */
	/* If pad is full. */
	if( py >= (15+5)*4 ) {
		px=0; py=0;
		/* Clear imgbuf */
		egi_imgbuf_resetColorAlpha(imgbuf, WEGI_COLOR_BLACK, -1);

		pch=(char *)ptext;
		continue;
	}

	/* Write imgbuf to FBDEV */
	egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, ctxt_arg->x0, ctxt_arg->y0);

	fb_render(&gv_fb_dev);

	//tm_delayms(5);
 }

}
