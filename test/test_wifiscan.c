/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple graphical Wifi Scanner.

Usage:
	./surf_wifiscan

Note:
1. Sometimes, 'aps' command results in several APs, and have to
   reboot OP. (Power supply?)

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pty.h>
#include <sys/wait.h>
#include "egi_log.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"
#include "egi_cstring.h"
#include "egi_iwinfo.h"

void print_help(const char* cmd)
{
        printf("Usage: %s [-h]\n", cmd);
        printf("        -h   Help \n");
}

void parse_apinfo(char *info, int index);
void draw_bkg(void);
void FBwrite_total(int total);
void FBwrite_tip(void);
void draw_cpuload(void);

int blpx=22;		/* Base line left point XY */
int blpy=240-8;
int fw=12;
int fh=12;

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

        /* <<<<<  EGI general init  >>>>>> */
#if 0
        printf("tm_start_egitick()...\n");
        tm_start_egitick();		   	/* start sys tick */
#endif
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_wifiscan") != 0) {	/* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }

#if 0
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

	/* Load freetype fonts */
  	printf("FTsymbol_load_sysfonts()...\n");
  	if(FTsymbol_load_sysfonts() !=0) {
        	printf("Fail to load FT sysfonts, quit.\n");
        	return -1;
  	}
  	//FTsymbol_set_SpaceWidth(1);


  	/* Initilize sys FBDEV */
  	printf("init_fbdev()...\n");
  	if(init_fbdev(&gv_fb_dev))
        	return -1;

  	/* Start touch read thread */
#if 0
  	printf("Start touchread thread...\n");
  	if(egi_start_touchread() !=0)
        	return -1;
#endif
  	/* Set sys FB mode */
	fb_set_directFB(&gv_fb_dev,false);
	fb_position_rotate(&gv_fb_dev,0);

 /* <<<<<  End of EGI general init  >>>>>> */

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

	/* Create displaying window */
	draw_bkg();
	FBwrite_tip();
	fb_render(&gv_fb_dev);

/* ------ LOOP ------ */
while(1) {

	EGI_PLOG(LOGLV_TEST,"Start to forkpty...");

	/* Create a pty shell to execute aps */
	shpid=forkpty(&ptyfd, ptyname, NULL, NULL);
        /* Child process to execute shell command */
	if( shpid==0) {
		/* Widora aps RETURN Examples:
		 *  ( -- Ch  SSID              BSSID               Security               Signal(%)W-Mode  ExtCH  NT WPS DPID -- )
		 *	12  Chinawwwwww      55:55:55:55:55:55   WPA1PSKWPA2PSK/AES     81       11b/g/n BELOW  In YES
		 *	11  ChinaNet-678     66:66:66:66:66:66   WPA2PSK/TKIPAES        65       11b/g/n NONE   In  NO
		 *	7   CMCC-RED         77:77:77:77:77:77   WPA1PSKWPA2PSK/TKIPAES 55       11b/g/n NONE   In  NO
		 */
                execl("/bin/sh", "sh", "-c", "aps | grep 11b/g/n", (char *)0);

		/* On success, execve() does not return! */
		EGI_PLOG(LOGLV_CRITICAL, "execl() fail!");
		exit(0);
        }
	else if( shpid <0 ){
		EGI_PLOG(LOGLV_CRITICAL, "Fail to forkpty! Err'%s'.  retry later...", strerror(errno));
		sleep(3);
		continue;
	}

	printf("ptyname: %s\n",ptyname);
	EGI_PLOG(LOGLV_TEST,"Start to read aps results and parse data...");

	/* ELSE: Parent process to get execl() result */
        bzero(obuf,sizeof(obuf));
        nbuf=0;
	index=0;
        do {

		/* Check remaining space of obuf[] */
                if(0==sizeof(obuf)-nbuf-1) {
	        	/* TODO: memgrow obuf */
                	EGI_PLOG(LOGLV_TEST,"Buffer NOT enough! nbuf=%zd!", nbuf);
			break;
		}
                else
                        printf("nbuf=%zd \n", nbuf);

		/* Continue to read output from ptyfd */
                nread=read(ptyfd, obuf+nbuf, sizeof(obuf)-nbuf-1);
                printf("nread=%zd\n",nread);
                //printf("obuf: \n%s\n",obuf);
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

		/* Extract data from obuf[] line by line. */
                pnl=strstr(obuf,"\n");  /* Check if there is a NL in obuf[]. */
                if( pnl!=NULL || nread<0 ) {  /* If nread<0, get remaining data, even withou NL. */
                	/* Copy to linebuf, TODO: memgrow linebuf */
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

			/* Parse result line. */
			printf("Result: %s", linebuf);
			if(index==0) {
				draw_bkg();
				draw_cpuload();
			}
			parse_apinfo(linebuf, index++);
		}

	} while( nread>=0 || nbuf>0 );

	/* Put total number */
	FBwrite_total(index);

	/* Render */
	fb_render(&gv_fb_dev);

	/* Close and release */
	close(ptyfd);
	/* waitpid */
	retpid=waitpid(shpid, &status, 0);
	EGI_PLOG(LOGLV_CRITICAL, "waitpid get retpid=%d", retpid);
	if(WIFEXITED(status))
		EGI_PLOG(LOGLV_CRITICAL, "Child %d exited normally!", shpid);
	else
		EGI_PLOG(LOGLV_CRITICAL, "Child %d is abnormal!", shpid);

} /* END LOOP */

END_PROG:



        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
        printf("egi_quit_log()...\n");
        egi_quit_log();
        printf("<-------  END  ------>\n");

return 0;
}


/*--------------------------------
Parse AP information, draw curve.

Ch  SSID     BSSID               Security               Signal(%)W-Mode  ExtCH  NT WPS DPID
------------------------------------------------------------------------------------------
12  Chinaw   55:55:55:55:55:55   WPA1PSKWPA2PSK/AES     81       11b/g/n BELOW  In YES

--------------------------------*/
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

	/* Draw channel */

	/* 256COLOR Select */
	EGI_16BIT_COLOR color;
	unsigned int color_codes[]={
	  9,10,11,12,13,14,15,36,39,100,105,144,148,155,158,169,197,201,202,205,207,208,213,214,219,220,227,230
	};
	int color_total=sizeof(color_codes)/sizeof(color_codes[0]);
	int code;
	code=(cstr_hash_string(ssid, 0)%1000)%color_total;
	printf("ssid: %s, color code=%d\n", ssid, code);
	#if 0
	color=egi_256color_code(color_codes[code]);
	#else
	color=egi_256color_code(color_codes[index%color_total]);
	#endif
	fbset_color(color);

	/* Cal. top point XY of sginal strength arc. */
	if(ch==14) {
		px_cf=blpx+10*ppm+(12*cfg+12)*ppm;
		py_cf=blpy-signal*pps-1;
		//draw_line(&gv_fb_dev, px_cf, blpy, px_cf, py_cf);
	}
	else {
		px_cf=blpx+10*ppm+(ch-1)*cfg*ppm;
		py_cf=blpy-signal*pps-1;
		//draw_line(&gv_fb_dev, px_cf, blpy, px_cf, py_cf);
	}

	/* Draw an arc to indicate bandwidth and signal strength */
	pts[0].x=px_cf-bw/2*ppm; 	pts[0].y=blpy;
	pts[1].x=px_cf;			pts[1].y=py_cf;
	pts[2].x=px_cf+bw/2*ppm;  	pts[2].y=blpy;

	gv_fb_dev.antialias_on=true;
	draw_spline(&gv_fb_dev, 3, pts, 2, 1);
	gv_fb_dev.antialias_on=false;

	draw_filled_spline(&gv_fb_dev, 3,pts, 2, 1, blpy, color, 50); /* baseY, color, alpha */

	/* Mark SSID */
	int pixlen=FTsymbol_uft8strings_pixlen(egi_sysfonts.regular, fw, fh, ssid);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, /* FBdev, fontface */
                                        fw, fh,(const UFT8_PCHAR)ssid,    /* fw,fh, pstr */
                                        320, 1, 0,      		/* pixpl, lines, fgap */
                                        px_cf-pixlen/2, py_cf-fw-2,    	/* x0,y0, */
                                        color, -1, 255,                 /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */
}


/*------------------
Draw back ground
-------------------*/
void draw_bkg(void)
{
	int i,j;
	char percent[8]={0};
	int freqcent;

        /* Init. FB working buffer */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKGRAY1); //DARKPURPLE);

        /* Prepare backgroup grids image for transparent image */
        for(i=0; i < gv_fb_dev.vinfo.yres/20; i++) {
        	for(j=0; j < gv_fb_dev.vinfo.xres/20; j++) {
                	if( (i*(gv_fb_dev.vinfo.xres/20)+j+i)%2 )
			//if( i%2 )
                        	fbset_color(WEGI_COLOR_DARKGRAY);
                        else
                        	fbset_color(WEGI_COLOR_DARKGRAY1);
                        draw_filled_rect(&gv_fb_dev, j*20, i*20-(240-blpy) , (j+1)*20-1, (i+1)*20 -(240-blpy)-1 );
                }
        }

	/* Draw upper banner */
	fbset_color(WEGI_COLOR_GRAYA);
	draw_filled_rect(&gv_fb_dev, 0,0, 320-1, 15-1);

	/* Draw title */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, /* FBdev, fontface */
       	                                15, 15,(const UFT8_PCHAR)"EGI WiFi Scanner", /* fw,fh, pstr */
               	                        320, 1, 0,      		  /* pixpl, lines, fgap */
                       	                10, -2,         /* x0,y0, */
       	                      	        WEGI_COLOR_BLACK, -1, 255,        /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      	  /* int *cnt, int *lnleft, int* penx, int* peny */

	/* Draw lower banner */
	fbset_color(WEGI_COLOR_GRAY);
	draw_filled_rect(&gv_fb_dev, 0,blpy+1, 320-1,240-1);

	/* Draw spectrum line */
	//fbset_color(WEGI_COLOR_GRAYC);
	//draw_wline_nc(&gv_fb_dev, blpx,blpy, blpx+92*3-1, blpy, 1);

	/* Draw Channel Freq. Center Mark */
	fbset_color(WEGI_COLOR_BLACK);
	/* CHANNEL 1-13 */
	for(i=0; i<13; i++) {
		//draw_rect(&gv_fb_dev, blpx+10*ppm+cfg*ppm*i, blpy, blpx+10*ppm+cfg*ppm*(i+1), blpy+10);
		freqcent=blpx+(10+cfg*i)*ppm;
		draw_filled_rect(&gv_fb_dev, freqcent -2, blpy+1, freqcent +2, blpy+10);
	}
	/* CHANNEL 14 */
	freqcent=blpx+(10+cfg*12+12)*ppm;
	draw_filled_rect(&gv_fb_dev, freqcent -2, blpy+1,  freqcent +2, blpy+10);

	/* Draw signal percentage mark */
	fbset_color(WEGI_COLOR_GRAY);
	for(i=0; i<11; i++) {
		sprintf(percent,"%d",10*i);
	        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, /* FBdev, fontface */
        	                                fw, fh,(const UFT8_PCHAR)percent,    /* fw,fh, pstr */
                	                        320, 1, 0,      		/* pixpl, lines, fgap */
                        	                320-20, blpy-fh-i*pps*10,       /* x0,y0, */
         	                      	        WEGI_COLOR_WHITE, -1, 255,      /* fontcolor, transcolor,opaque */
	                                       	NULL, NULL, NULL, NULL);     	/* int *cnt, int *lnleft, int* penx, int* peny */
	}
}

/*-----------------------
Put total number of APs
------------------------*/
void FBwrite_total(int total)
{
	char str[32];

	sprintf(str,"Total %d APs",total);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular, /* FBdev, fontface */
       	                                15, 15,(const UFT8_PCHAR)str, 	  /* fw,fh, pstr */
               	                        320, 1, 0,      		  /* pixpl, lines, fgap */
                       	                320-100, -2,         /* x0,y0, */
       	                      	        WEGI_COLOR_BLACK, -1, 255,        /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      	  /* int *cnt, int *lnleft, int* penx, int* peny */
}

/*------------------------
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
                                       	NULL, NULL, NULL, NULL);      	  /* int *cnt, int *lnleft, int* penx, int* peny */
}


void draw_cpuload(void)
{
	int i;
	static EGI_POINT pts[17]={0};
	int st=sizeof(pts)/sizeof(pts[0]);	/* Size of vload[] */
        float avgload[3]={0};


        /* Get average load */
        if( iw_read_cpuload( avgload )!=0 ) {
        	printf("Fail to read avgload!\n");
        }
        else {
        	/* Shift vload/pts values */
                memmove( pts+1, pts, (st-1)*sizeof(EGI_POINT));

                /* Update pts.Y as vload */
                pts[0].y=blpy-avgload[0]*(10*20)/5.0;   /* Set Max of avg is 5, as 100%, pixel 10*20 */
		if( pts[0].y < (blpy-10*20) )
			pts[0].y=blpy-10*20;  /* Limit */
        }

	/* Re_assign  pts.X */
	for(i=0; i<st; i++)
		pts[i].x=i*20;
	/* The last pts */
	pts[16].x=320-1;

	/* Draw spline of CPU load */
	gv_fb_dev.antialias_on=true;
	draw_spline(&gv_fb_dev, st, pts, 2, 1);
	gv_fb_dev.antialias_on=false;

	draw_filled_spline(&gv_fb_dev, st, pts, 2, 1, blpy, WEGI_COLOR_RED, 60); /* baseY, color, alpha */

}
