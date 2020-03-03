/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Test FB with multiple buffer_pages.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_pcm.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"

int main(int argc, char** argv)
{
	int i,j,k;
	int ret;

	struct timeval tm_start;
	struct timeval tm_end;

        /* <<<<<  EGI general init  >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();		   	/* start sys tick */
#if 0
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_test") != 0) {	/* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
#endif

#if 1
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {   	/* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        if(FTsymbol_load_appfonts() !=0 ) {  	/* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }
#endif

        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )		/* init sys FB */
                return -1;

        /* ---- start touch thread ---- */
	egi_start_touchread();

	/* <<<<<  End EGI Init  >>>>> */


EGI_IMGBUF* eimg=NULL;
char**	fpaths=NULL;	/* File paths */
int	ftotal=0; 	/* File counts */

int nret=0;
unsigned long int off=0; /* in bytes, offset */
unsigned long int xres, yres;
unsigned long int line_length; /* in bytes */
static EGI_TOUCH_DATA	touch_data;
int		   mark;

const wchar_t *wbook=L"人心生一念，天地尽皆知。善恶若无报，乾坤必有私。\
那菩萨闻得此言，满心欢喜，对大圣道：“圣经云：‘出其言善。\
则千里之外应之；出其言不善，则千里之外适之。’你既有此心，待我到了东土大唐国寻一个取经的人来，教他救你。你可跟他做个徒弟，秉教伽持，入我佛门。再修正果，如何？”大圣声声道：“愿去！愿去！”菩萨道：“既有善果，我与你起个法名。”大圣道：“我已有名了，叫做孙悟空。”菩萨又喜道：“我前面也有二人归降，正是‘悟’字排行。你今也是‘悟’字，却与他相合，甚好，甚好。这等也不消叮嘱，我去也。”那大圣见性明心归佛教，这菩萨留情在意访神谱。\
他与木吒离了此处，一直东来，不一日就到了长安大唐国。敛雾收云，师徒们变作两个疥癫游憎，入长安城里，竟不觉天晚。行至大市街旁，见一座土地庙祠，二人径进，唬得那土地心慌，鬼兵胆战。知是菩萨，叩头接入。那土地又急跑报与城隍社令及满长安城各庙神抵，都来参见，告道：“菩萨，恕众神接迟之罪。”菩萨道：“汝等不可走漏消息。我奉佛旨，特来此处寻访取经人。借你庙宇，权住几日，待访着真僧即回。”众神各归本处，把个土地赶到城隍庙里暂住，他师徒们隐遁真形。\
毕竟不知寻出那个取经来，且听下回分解。";

const wchar_t *wstr1=L"  大觉金仙没垢姿，\n	\
  西方妙相祖菩提。\n	\
  不生不灭三三行，\n	\
  全气全神万万慈。\n	\
  空寂自然随变化，\n	\
  真如本性任为之。\n	\
  与天同寿庄严体，\n	\
  历劫明心大法师。\n	\
  显密圆通真妙诀，\n	\
  惜修生命无他说。\n	\
  都来总是精气神，\n	\
";

const wchar_t *wstr2=L"  谨固牢藏休漏泄。\n	\
  休漏泄，体中藏，\n	\
  汝受吾传道自昌。\n	\
  口诀记来多有益，\n	\
  屏除邪欲得清凉。\n	\
  得清凉，光皎洁，\n	\
  好向丹台赏明月。\n	\
  月藏玉兔日藏乌，\n	\
  自有龟蛇相盘结。\n	\
  相盘结，性命坚，\n	\
  却能火里种金莲。\n	\
  攒簇五行颠倒用，\n	\
  功完随作佛和仙。\n	\
  口诀记来多有益，\n	\
  屏除邪欲得清凉。\n	\
  得清凉，光皎洁，\n	\
  好向丹台赏明月。\n	\
  月藏玉兔日藏乌，\n	\
  自有龟蛇相盘结。\n	\
  相盘结，性命坚，\n	\
  却能火里种金莲。\n	\
  攒簇五行颠倒用，\n	\
  功完随作佛和仙。\
";

	printf(" -7%%10=%d \n", (-7)%10 );
	printf(" -7%%10u=%d \n", (-7)%(10u) );
	printf(" -13%%10=%d \n", (-13)%10 );
	printf(" -4%%960=%d \n", (-4)%960 );

#if 1
        if(argc<2) {
                printf("Usage: %s file\n",argv[0]);
                exit(-1);
        }

        /* 1. Load pic to imgbuf */
	eimg=egi_imgbuf_readfile(argv[1]);
	if(eimg==NULL) {
        	EGI_PLOG(LOGLV_ERROR, "%s: Fail to read and load file '%s'!", __func__, argv[1]);
		return -1;
	}
#endif

do {    ////////////////////////////   1.  LOOP TEST   /////////////////////////////////

	/* Clear FB bk buffer with color */
	fb_shift_buffPage(&gv_fb_dev,0);
	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_LTYELLOW);
	fb_shift_buffPage(&gv_fb_dev,1);
	fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_LTYELLOW);


#if 0 /* --------------  TEST  ---------------- */
        /* Wstring put to FB pages */
	fb_shift_buffPage(&gv_fb_dev,o);
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          24, 24, wstr1,  		    /* fw,fh, pstr */
                                          240, (320-10)/(24+6), 6,             /* pixpl, lines, gap */
                                          0, 10,                      	    /* x0,y0, */
                                          WEGI_COLOR_BLACK, -1, -1 );   /* fontcolor, transcolor,opaque */

	fb_shift_buffPage(&gv_fb_dev,1);
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          24, 24, wstr2,  		    /* fw,fh, pstr */
                                          240, (320-10)/(24+6), 6,             /* pixpl, lines, gap */
                                          0, 10,                      	    /* x0,y0, */
                                          WEGI_COLOR_WHITE, -1, -1 );   /* fontcolor, transcolor,opaque */

	/* Refresh FB by memcpying back buffer to FB */
	fb_shift_buffPage(&gv_fb_dev,0);
	fb_refresh(&gv_fb_dev);
	tm_delayms(250);
	//sleep(1);

	fb_shift_buffPage(&gv_fb_dev,1);
	fb_refresh(&gv_fb_dev);
	tm_delayms(250);
	//sleep(1);

	/* import imgbuf to back buffer */
	if( gv_fb_dev.screensize*2 > eimg->height*eimg->width )
		memcpy(gv_fb_dev.map_buff, eimg->imgbuf, (eimg->height*eimg->width)*2 );
	else
		memcpy(gv_fb_dev.map_buff, eimg->imgbuf, gv_fb_dev.screensize*2*2 );

#endif /* --------------- */


	/* --- Write wbook to back buffers --- */
	off=0;
	for(i=0; i<FBDEV_MAX_BUFFER; i++)
	{
	        fb_shift_buffPage(&gv_fb_dev,i); /* shift to buffer page 0 */
		fb_clear_backBuff(&gv_fb_dev, WEGI_COLOR_LTYELLOW);
        	nret=FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,    /* FBdev, fontface */
                	                          18, 18, wbook+off,                    /* fw,fh, pstr */
                        	                  240-5*2, (320-10)/(18+4), 4,            /* pixpl, lines, gap */
                                	          10, 10,                            /* x0,y0, */
                                        	  WEGI_COLOR_BLACK, -1, -1 );   /* fontcolor, transcolor,opaque */
		if(nret>0)
			off+=nret;
	}
	/* refresh FB to show first buff page */
	fb_shift_buffPage(&gv_fb_dev,0);
	fb_refresh(&gv_fb_dev);

	/* ---- Scrolling up/down 3 PAGEs by touch sliding ---- */
	xres=gv_fb_dev.vinfo.xres;
	yres=gv_fb_dev.vinfo.yres;
	line_length=gv_fb_dev.finfo.line_length;
	i=0; /* line index of all FB buffer pages */
	while(1)
      	{
		if(egi_touch_getdata(&touch_data)==false) {
			tm_delayms(10);
			continue;
		}

		/* switch touch status */
		switch(touch_data.status) {
			case released_hold:
				continue;
				break;
			case pressing:
	                        printf("pressing i=%d\n",i);
                        	mark=i;
                        	continue;
				break;
			case releasing:
				printf("releasing\n");
				continue;
				break;

			case pressed_hold:
				break;

			default:
				continue;
		}


		/* Now trap into pressed/sliding routine ....*/
		i=mark-touch_data.dy;
		printf("i=%d\n",i);

		/* Normalize 'i' to: [0  yres*FBDEV_MAX_BUFFER) */
		if(i<0) {
			printf("i%%(yres*FBDEV_MAX_BUFF)=%d\n", i%(int)(yres*FBDEV_MAX_BUFFER));
			i=(i%(int)(yres*FBDEV_MAX_BUFFER))+yres*FBDEV_MAX_BUFFER;
			printf("renew i=%d\n", i);
			mark=i+touch_data.dy;
		}

		else if (i > yres*FBDEV_MAX_BUFFER-1) {
			i=0;	/* loop back to page 0 */
			mark=touch_data.dy;
			continue;
		}

		/* refresh FB with offset line */
		fb_slide_refresh(&gv_fb_dev, i);

		tm_delayms(15);
		//usleep(200000); //500);
      	}


} while(1); ///////////////////////////   END LOOP TEST   ///////////////////////////////


#if 1	/* Free eimg */
	egi_imgbuf_free(eimg);
	eimg=NULL;
#endif

	#if 0
        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
	printf9"egi_end_touchread()...\n");
	egi_end_touchread();
        printf("egi_quit_log()...\n");
        egi_quit_log();
        printf("<-------  END  ------>\n");
	#endif

return 0;
}


