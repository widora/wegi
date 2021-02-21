/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple graphical Wifi Scanner.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <pty.h>
#include <sys/wait.h>
#include <errno.h>
#include "egi_log.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"

/* 函数 */
void print_help(const char* cmd)
{
        printf("Usage: %s [-h]\n", cmd);
        printf("        -h   Help \n");
}
void parse_apinfo(char *info, int index);   /* 解析一条WiFi热点AP信息，并将其图形化 */
void draw_bkg(void);			    /* 绘制画面背景 */
void FBwrite_total(int total);		    /* 显示扫描到的总AP数 */
void FBwrite_tip(void);			    /* 显示扫描启动提示 */

/* 下部显示条位置 */
int blpx=22;		/* Bottom banner left point XY */
int blpy=240-8;

/* 字体宽高 */
int fw=12;
int fh=12;

/* 图形比例设置 */
int cfg=5;	/* Central freq. gap: 5MHz */
int ppm=3;	/* Pixel per MHz */
int pps=2;	/* Pixel per signal strength(percentage) 2*100 */

/*----------------------------
	   MAIN
-----------------------------*/
int main(int argc, char** argv)
{
	int opt;

	int shpid;
	int retpid;
	int status;
	int ptyfd;
	int nread;
	char obuf[2048];
        size_t nbuf;     	/* bytes of data in obuf */
	char *pnl;	 	/* pointer to a newline */
	char linebuf[256]; 	/* As we already know abt. the size */
	int linesize;
	int index;
	char ptyname[256]={0};

  /* <<<<<  EGI 初始化流程  >>>>>> */

	/* 开启日志记录 */
        if(egi_init_log("/mmc/log_wifiscan")!=0)
                exit(EXIT_FAILURE);

	/* 加载FreeType字体 */
  	if(FTsymbol_load_sysfonts()!=0)
                exit(EXIT_FAILURE);

  	/* 初始化FB显示设备 */
  	if(init_fbdev(&gv_fb_dev)!=0)
                exit(EXIT_FAILURE);

  	/* 设置显示模式 */
	fb_set_directFB(&gv_fb_dev,false);
	fb_position_rotate(&gv_fb_dev,0);

  /* <<<<<  EGI初始化流程结束  >>>>>> */

        /* Parse input option */
        while( (opt=getopt(argc,argv,"h"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
                                break;
		}
	}
        if( optind < argc ) {
	}

	/* 创建并显示背景图形 */
	draw_bkg();
	FBwrite_tip();
	fb_render(&gv_fb_dev);

/* ------ 循环扫描Wifi并进行图形化显示 ------ */
while(1) {

	EGI_PLOG(LOGLV_TEST,"Start to forkpty...");

	/* fork一个伪终端用来运行shell命令，并获取其输出 */
	shpid=forkpty(&ptyfd, ptyname, NULL, NULL);
        /* 子进程运行 */
	if( shpid==0) {
		/* OP中扫描wifi基本命令: iwpriv ra0 get_site_survey
		 * 返回结果示例:
		 *  ( -- Ch  SSID              BSSID               Security      ignal(%)   W-Mode  ExtCH  NT WPS DPID -- )
		 *	12  ChinaAAA     55:55:55:55:55:55   WPA1PSKWPA2PSK/AES     81       11b/g/n BELOW  In YES
		 *	11  ChinaBBB     66:66:66:66:66:66   WPA2PSK/TKIPAES        65       11b/g/n NONE   In  NO
		 *	7   CCCC         77:77:77:77:77:77   WPA1PSKWPA2PSK/TKIPAES 55       11b/g/n NONE   In  NO
		 */
                execl("/bin/sh", "sh", "-c", "aps.sh | grep 11b/g/n", (char *)0);

		/* On success, execve() does not return! */
		EGI_PLOG(LOGLV_CRITICAL, "execl() fail!");
		exit(0);
        }
	else if( shpid <0 ){
		EGI_PLOG(LOGLV_CRITICAL, "Fail to forkpty! Err'%s'.  retry later...", strerror(errno));
		sleep(3);
		continue;
	}

	/* ELSE: 父进程读取子进程的输出结果 */

	EGI_PLOG(LOGLV_TEST,"%s created!, start to read results and parse data...",ptyname);

        bzero(obuf,sizeof(obuf));
        nbuf=0;
	index=0;
        do {
		/* Check remaining space of obuf[] */
                if(0==sizeof(obuf)-nbuf-1) {
                	EGI_PLOG(LOGLV_TEST,"Buffer NOT enough! nbuf=%zd!", nbuf);
			break;
		}
                else
                        printf("nbuf=%zd \n", nbuf);

		/* 读取子进程的输出数据并保存至obuf[] */
                nread=read(ptyfd, obuf+nbuf, sizeof(obuf)-nbuf-1);
                printf("nread=%zd\n",nread);
                if(nread>0) {
                	nbuf += nread;
                }
                else if(nread==0) {
                	printf("nread=0...\n");
                }
                else  { /* nread<0 */
			/* Maybe ptyfd already read out, and ptyfd is unavailable.
			 * However data in obuf[] has NOT been completely parsed yet.
			 */
                }

		/* 从obuf[]中一行一行地读出数据 */
                pnl=strstr(obuf,"\n");  /* Check if there is a NewLine in obuf[]. */
                if( pnl!=NULL || nread<0 ) {  /* If nread<0, get remaining data, even withou NL. */
                	/* Copy to linebuf */
                        if( pnl==NULL && nread<0 ) /* shell pid ends, and remains in obuf without '\n'! */
                        	linesize=strlen(obuf);
                        else  /* A line with '\n' at end. */
                        	linesize=pnl+1-obuf;

                        /* Limit linesize */
                        if(linesize > sizeof(linebuf)-1 ) {
                        	printf("linebuf is NOT enough!, current linesize=%zd\n",linesize);
	                        linesize=sizeof(linebuf)-1;  /* linesize LIMIT */
                        }

			/* Copy the line into obuf[], with '\n'. */
                      	strncpy(linebuf, obuf, linesize);
                        linebuf[linesize]='\0';

                        /* Move copied line out from obuf[], and update nbuf. */
                        memmove(obuf, obuf+linesize, nbuf-linesize+1); /* +1 EOF */
                        nbuf -= linesize;
                        obuf[nbuf]='\0';

			/* 解析一条AP信息，并将其图形化 */
			printf("Result: %s", linebuf);
			if(index==0) draw_bkg();
			parse_apinfo(linebuf, index++);
		}

	} while( nread>=0 || nbuf>0 );

	/* 写上扫描到的AP总数 */
	FBwrite_total(index);

	/* 刷新屏幕显示 */
	fb_render(&gv_fb_dev);

	/* 关闭伪终端设备文件并释放资源 */
	close(ptyfd);
	retpid=waitpid(shpid, &status, 0);
	EGI_PLOG(LOGLV_CRITICAL, "waitpid get retpid=%d", retpid);
	if(WIFEXITED(status))
		EGI_PLOG(LOGLV_CRITICAL, "Child %d exited normally!", shpid);
	else
		EGI_PLOG(LOGLV_CRITICAL, "Child %d is abnormal!", shpid);

} /* END LOOP */


        /* <<<<<  EGI释放流程 >>>>> */
	/* 释放FreeTpype字体 */
        FTsymbol_release_allfonts();
	/* 释放FB显示设备及数据 */
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
	/* 结束日志记录 */
        egi_quit_log();
        printf("<-------  END  ------>\n");

	return 0;
}


/*----------------------------------
解析一条AP信息，并将其绘制成图形呈现．
Parse AP information, draw curve.

AP信息示例
Ch  SSID     BSSID              Security            Signal(%)   W-Mode  ExtCH  NT WPS DPID
------------------------------------------------------------------------------------------
12  Chinaw   55:55:55:55:55:55   WPA1PSKWPA2PSK/AES     81       11b/g/n BELOW  In YES

-----------------------------------*/
void parse_apinfo(char *info, int index)
{
	UFT8_CHAR ssid[256]; /* SSID */
	int ch;		/* Channel */
	int signal; 	/* In percent */
	int bw;		/* HT bandwidth, Mode HT20, HT40+/- */
	char *ps;
	int k;
	char *delim=" "; /* SPACE as delimer */
	int px_cf;	/* Cetral Freq. Mark coordinate X */
	int py_cf;	/* Cetral Freq. Mark coordinate Y */
	EGI_POINT pts[3];

	/* sss */
	if(index>30)
		EGI_PLOG(LOGLV_CRITICAL, "Info[%d]: %s", index, info);

	/* Defaule mode HT20 */
	bw=20;

        /* Get parameters from string info. */
        ps=strtok(info, delim);
        for(k=0; ps!=NULL; k++) {
		switch(k) {
			case 0:	/* Channel */
				ch=atoi(ps);
				break;
			case 1: /* SSID */
				strncpy((char *)ssid, ps, sizeof(ssid)-1);
				break;
			case 2: /* BSSID */
				break;
			case 3: /* Security */
				break;
			case 4: /* Signal strength in percentage */
				signal=atoi(ps);
				printf("Signal=%d\n", signal);
				break;
			case 5: /* W-Mode */
				break;
			case 6: /* ExtCH, HT40+ OR HT40-  */
				if(strstr(ps,"ABOVE") || strstr(ps,"BELOW"))
					bw=40;
				break;
			case 7: /* NT */
				break;
			case 8: /* WPS */
				break;
		}
		ps=strtok(NULL, delim);
	}

	/* Avoid uncomplete info, example hidden SSID. */
	printf("k=%d\n",k);
	if(k<10) /* including LN token, 10 items in each info string. */
		return;

	/* 根据index的数值选取一个对应颜色 */
	EGI_16BIT_COLOR color;
	unsigned int color_codes[]={
	  9,10,11,12,13,14,15,36,39,100,105,144,148,155,158,169,197,201,202,205,207,208,213,214,219,220,227,230
	};
	int color_total=sizeof(color_codes)/sizeof(color_codes[0]);
	color=egi_256color_code(color_codes[index%color_total]);
	fbset_color(color);

	/* 计算弧形曲线的顶点坐标 */
	if(ch==14) {
		px_cf=blpx+10*ppm+(12*cfg+12)*ppm;
		py_cf=blpy-signal*pps-1;
	}
	else {
		px_cf=blpx+10*ppm+(ch-1)*cfg*ppm;
		py_cf=blpy-signal*pps-1;
	}

	/* 通过三点来绘制一个弧形曲线，以表示其所占信道和带宽 */
	pts[0].x=px_cf-bw/2*ppm; 	pts[0].y=blpy;
	pts[1].x=px_cf;			pts[1].y=py_cf;
	pts[2].x=px_cf+bw/2*ppm;  	pts[2].y=blpy;
	draw_spline(&gv_fb_dev, 3, pts, 2, 1);
	draw_filled_spline(&gv_fb_dev, 3,pts, 2, 1, blpy, color, 50);

	/* 在弧线顶部写上 SSID */
	int pixlen=FTsymbol_uft8strings_pixlen(egi_sysfonts.regular, fw, fh, ssid);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, /* FBdev, fontface */
                                        fw, fh,(const UFT8_PCHAR)ssid,    /* fw,fh, pstr */
                                        320, 1, 0,      		 /* pixpl, lines, fgap */
                                        px_cf-pixlen/2, py_cf-fw-2,        /* x0,y0, */
                                        color, -1, 255,                  /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}


/*------------------
  绘制背景图
-------------------*/
void draw_bkg(void)
{
	int i,j;
	char percent[8]={0};
	int freqcent;

        /* 设置背景色 */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKGRAY1);

        /* 绘制黑灰格子 */
        for(i=0; i < gv_fb_dev.vinfo.yres/20; i++) {
        	for(j=0; j < gv_fb_dev.vinfo.xres/20; j++) {
                	if( (i*(gv_fb_dev.vinfo.xres/20)+j+i)%2 )
                        	fbset_color(WEGI_COLOR_DARKGRAY);
                        else
                        	fbset_color(WEGI_COLOR_DARKGRAY1);
                        draw_filled_rect(&gv_fb_dev, j*20, i*20-(240-blpy) , (j+1)*20-1, (i+1)*20 -(240-blpy)-1 );
                }
        }

	/* 绘制上部条幅 */
	fbset_color(WEGI_COLOR_GRAYA);
	draw_filled_rect(&gv_fb_dev, 0,0, 320-1, 15-1);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, /* FBdev, fontface */
       	                                15, 15,(const UFT8_PCHAR)"EGI WiFi Scanner", /* fw,fh, pstr */
               	                        320, 1, 0,      		  /* pixpl, lines, fgap */
                       	                10, -2,         /* x0,y0, */
       	                      	        WEGI_COLOR_BLACK, -1, 255,        /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      	  /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

	/* 绘制下部条幅 */
	fbset_color(WEGI_COLOR_GRAY);
	draw_filled_rect(&gv_fb_dev, 0,blpy+1, 320-1,240-1);

	/* 绘制WiFi频道刻度线 */
	fbset_color(WEGI_COLOR_BLACK);
	/* CHANNEL 1-13 */
	for(i=0; i<13; i++) {
		freqcent=blpx+(10+cfg*i)*ppm;
		draw_filled_rect(&gv_fb_dev, freqcent -2, blpy+1, freqcent +2, blpy+10);
	}
	/* CHANNEL 14 */
	freqcent=blpx+(10+cfg*12+12)*ppm;
	draw_filled_rect(&gv_fb_dev, freqcent -2, blpy+1,  freqcent +2, blpy+10);

	/* 绘制信号强度百分刻度 */
	fbset_color(WEGI_COLOR_GRAY);
	for(i=0; i<11; i++) {
		sprintf(percent,"%d",10*i);
	        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, /* FBdev, fontface */
        	                                fw, fh,(const UFT8_PCHAR)percent,    /* fw,fh, pstr */
                	                        320, 1, 0,      		 /* pixpl, lines, fgap */
                        	                320-20, blpy-fh-i*pps*10,          /* x0,y0, */
         	                      	        WEGI_COLOR_WHITE, -1, 255,       /* fontcolor, transcolor,opaque */
	                                       	NULL, NULL, NULL, NULL);      /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}
}

/*-----------------------
 写总数量
 Put total number of APs
------------------------*/
void FBwrite_total(int total)
{
	char str[32];

	sprintf(str,"Total %d APs",total);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, /* FBdev, fontface */
       	                                15, 15,(const UFT8_PCHAR)str, 	  /* fw,fh, pstr */
               	                        320, 1, 0,      		  /* pixpl, lines, fgap */
                       	                320-100, -2,         		  /* x0,y0, */
       	                      	        WEGI_COLOR_BLACK, -1, 255,        /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      	  /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}

/*------------------------
 写提示文字
 FBwrite tips.
-------------------------*/
void FBwrite_tip(void)
{
	/* Put scanning tip... */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, /* FBdev, fontface */
       	                                24, 24,(const UFT8_PCHAR)"Scanning...",	  /* fw,fh, pstr */
               	                        320, 1, 0,      		  /* pixpl, lines, fgap */
                       	                100, 100,         		  /* x0,y0, */
       	                      	        WEGI_COLOR_WHITE, -1, 200,        /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      	  /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}
