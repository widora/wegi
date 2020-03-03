/*----------------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A program to draw stock index/price in a line chart.

NOTE:
   0. Http request hq.sinajs.cn to get stock data.
   1. Initiate 'fdmax' and 'fdmin' with first input data_point[].
   2. Initiate 'famp' with a value that is smaller than the real amplitude of fluctuation,
      so the chart will reflect the fluctuation sufficiently.
      A too big value results in small fluctuation in the chart, and may be indiscernible.
   3. Initiate data_point[] with the opening point/price.
   4. Initiate 'fbench' with yesterday's point/price, or the opening price. whatever you want.
      Assign current price to it, then you'll be likely to observe more significant fluctuation
      in the line chart.
   5. Adjust weight value to chart_time_range/sampling_time_gap to get a reasonable chart result.
   6. Request/Sampling time is NOT stable due to heavy CPU load or network delay.
   7. Stock data stream from SINA does NOT stop at 15:00:00! It keeps running until 15:01:53. !!!!!!

TODO:
   1. It may miss data occassionly, because of network delay OR request interval too big???
				---try to increase request frequency.
   2. Rule out market close/holidy day.
   3. If the network is down for a while during market trading, those data will be missing and the
      chart will NOT show any evidence of this breakdown.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include "egi_timer.h"
#include "egi_iwinfo.h"
#include "egi_cstring.h"
#include "egi_fbgeom.h"
#include "egi_symbol.h"
#include "egi_color.h"
#include "egi_log.h"
#include "egi.h"
#include "egi_math.h"


#define REQUEST_INTERVAL_TIMEMS	500 /* request interval time in ms */

/* data compression type */
enum compress_type {
	none,		/* when new data comes, shift data_point[] to drop data_point[0]
			 * then save the latest data into data_point[num-1]
                         */
	interpolation,  /* when new data comes, update all data_point[] by interpolation */
	common_avg,	/* when new data comes, update all data_point[] by different weights */
	fold_avg,       /* avg data_point[0], then [1], then[2]....  */
};


/*------------------------------------
 	    MAIN FUNCTION
------------------------------------*/
int main(int argc, char **argv)
{
	int i,k;

	/* --- init logger --- */
#if 1
  	if(egi_init_log("/mmc/log_stock") != 0)
	{
		printf("Fail to init logger,quit.\n");
		return -1;
	}
#endif

        /* --- start egi tick --- */
//        tm_start_egitick();

        /* --- prepare fb device --- */
        gv_fb_dev.fdfd=-1;
        init_dev(&gv_fb_dev);

	/* --- load all symbol pages --- */
	symbol_load_allpages();


/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>  TEST STOCK MARKET DATA  DISPLAY >>>>>>>>>>>>>>>>>>>>>>>>>*/

        char data[256];		/* for http REQUEST reply buff */
        char *pt;
        float point,tprice,yprice,volume,turnover;
	int num=238+1; /* points on chart X axis
		       * put in form of num=2*N+1, data_point[num] is the latest data, and never compressed,
		       * fold compression is applied as num=2*N+1;
		       */
	float data_point[240]={0}; /* store stock point/price for every second,or time_gap as set*/
	int navg_first=2; /* first average number for data compression.
			    3600*2*4/240=120
			    2x(2<<6)=128 <=120  --- YES!  3x(2<<5)=96 --- NOPE!
			     get navg_th average of data_point[]  = (data[0]+data[1]...data[navg-1])/navg  */
	int navg=navg_first; /* later ajust to 2 */
	int navg_count=1; /* start from 1, for data_point[] initialized with bench data alread */
	int npstore=0; /*0 to num-1; index of current data_point[npstore] for compression operation . */
	int ncompress=0; /* represent as nth compression */
	bool buff_filled=false; /* before we start data compression, data_point[] shall be filled with new data */
	int nshift=0; /* for temp count */
	EGI_POINT pxy[240]={0};  /* point x,y relating to LCD coord */
	EGI_16BIT_COLOR symcolor;

	/* Float Precision: 6-7 digitals, double precisio: 15-16 digitals */

	float fdmin=0; 		/* Min value for all data_point[] history data */
	float fdmax=0; 		/* Max value for all data_point[] history data */
	float fbench=0;		/* Bench value, usually Yesterday's value, or opening price
				 * Reset with 0 first, It's a token for updating limits and bench value.
				 */
	int   wh=210-60; 		/* height of display window */
	int   ng=6;		/* number of grids/slots */
	int   hlgap=wh/ng; 	/* grid H LINE gap */

	/* updd and lower deviation from benchmark */
	float fupp_dev=0; /* fupp_dev=fdmax-fbench, if fdmax>fbench;  OR fupp_dev=0;  */
	float flow_dev=0; /* flow_dev=fbench-fdmin, if fdmin<fbench;  OR flow_dev=0;  */
	/*    if(flow_dev<fupp_dev), low_ng=flow_dev/(fupp_dev+flow_dev)+1;
	 *    else if(flow_dev>fupp_dev), upp_ng=fupp_dev/(fupp_dev+flow_dev)+1;
	 *    else: keep defalt upp_ng and low_ng
	 */
	int   upp_ng=3;	/* upper amplitude grid number */
	int   low_ng=3;	/* lower amplitude grid number */

	/* chart window position */
	int   chart_x0=0;  /* char x0 as of LCD FB */
	int   chart_y0=70; /* chart y0 as of LCD FB */
	int   offy=chart_y0+hlgap*upp_ng;  /* bench mark line offset value, offset from LCD y=0 */

	/* chart upp limit and low limit value */
	float   upp_limit; 	/* upper bar of chart */
	float   low_limit; 	/* low bar of char */
	float	famp=0.05;      /* Only for initial amplitude of point/price fluctuation */
	float   funit=wh/2/famp; /* 240/2/famp, pixles per point/price  */
	char strdata[64]={0};
	char *sname="s_sh000001"; /* Index or stock name */
//	char *sname="sh600389";/* Index or stock name */
//	char *sname="sz000931";
	char strrequest[32]; /* request string */
	int  favor_num=3;
	const char *favor_stock[3]={"sh600389","sz000931","sz300223"};

	int px,py;
	int pn;
	bool market_closed=true;
	bool market_recess=true;

	/* timer related */
	struct tm *tm_local; /* local time */
	time_t tm_t; /* seconds since 1970 */
	int mincount; /* tm_local->hour * 60 +tm_local->min */
	int seccount; /* tm_local->hour*3600+tm_local->min*60+tm_local->sec */
	int wcount; 	/* counter for while() loop */

	/* generate http request string */
	memset(strrequest,0,sizeof(strrequest));
	strcat(strrequest,"/list=");
	strcat(strrequest,sname);

	/* init pxy[].X  */
	for(i=0;i<num-1;i++) {
		pxy[i].x=240/(num-1)*i;
		//data_point[i]=point_bench;
	}
	pxy[num-1].x=240-1;

	/* clear areana */
	//fbset_color(WEGI_COLOR_BLACK);
	//draw_filled_rect(&gv_fb_dev,0,30,240-1,320-1);
	clear_screen(&gv_fb_dev,WEGI_COLOR_BLACK);

	/* draw grids */
	fbset_color(WEGI_COLOR_GRAY3);
        for(i=0;i<=wh/hlgap;i++)
	 	draw_line(&gv_fb_dev,0,offy+wh/2-hlgap*i,240-1,offy+wh/2-hlgap*i);

	/* make some dash line effects */
	for(i=0;i<11;i++)
	{
		fbset_color(WEGI_COLOR_BLACK);
		draw_wline(&gv_fb_dev,20+i*20,40,20+i*20,320-1,9);
	}

/*  <<<<<<<<<<<   Loop reading data and drawing line chart    >>>>>>>>>>  */
wcount=0;
while(1)
{

	/* <<<<<<<<<<<<<<<<<   Check Time    >>>>>>>>>>>>>> */
	tm_t=time(NULL);
	tm_local=localtime(&tm_t);
	seccount=tm_local->tm_hour*3600+tm_local->tm_min*60+tm_local->tm_sec;

	/*  market close time */
	if( seccount < 9*3600+30*60+0 || seccount >=15*3600+20 ) /* +20 for transaction lingering ???? */
	{
		if(!market_closed) { /* toggle token */
			market_closed=true;
		}

		/* if there is plenty of time before market opens, then starts deep sleep...  */
		if( ( seccount >=15*3600+30 || seccount < 9*3600+30*60 - 10 )
			&& wcount>=30  && fbench!=0 )  /* get fbench and go some rounds then...*/
		{
			printf(" <<<<<<<<<<<<<<<<<   Close Time, Deep Sleep NOW.   >>>>>>>>>>>>>>>> \n");
			egi_sleep(0,5,0);
			continue;
			//exit(1);
		}
		/* get last data, set fbench and draw, then loop to wait for market_open */
		else if( fbench!=0 ) {
			printf(" <<<<<<<<<<<<<<<<< Market Closed >>>>>>>>>>>>>>>> \n");
			wcount++;
			egi_sleep(0,0,500);
			continue;
		}

		/* else if fbench==0, go on to get data and set fbench */
	}
	/* recess time */
	/* Note:
         *	1. Noon recess starts at 11:30:10, while data stream stop updating. ---2019-4-19
	*/
	else if ( seccount >= 11*3600+30*60+20 && seccount < 13*3600 ) /* +20 for transaction lingering??? */
	{
//		EGI_PLOG(LOGLV_INFO,"seccount=%d, start recess...\n",seccount);
		printf(" <<<<<<<<<<<<<<<<<   Recess at Noon   >>>>>>>>>>>>>>>>> \n");
		if(!market_recess)
			market_recess=true;

		/* Do NOT draw chart during noon recession,loop to hold on */
		if( wcount>0 && fbench!=0 ) {/* first get fbench and last stock index/price, then hold on. */
			egi_sleep(0,0,500);
			//tm_delayms(500);
			continue;
		}
	}

	/*  market trade time */
	else {
		printf(" <<<<<<<<<<<<<<<<<   Trade Time   >>>>>>>>>>>>>>>>> \n");
		if(market_closed) {   /* toggle token */
			market_closed=false;
			fbench=0; /* need reset fbench */
			wcount=0; /* reset wcount */
		}
		if(market_recess)
			market_recess=false;
	}


	/* <<<<<<<<<<<<<<<<  HTTP Request Market Data    >>>>>>>>>>>>>> */
	/* 1. HTTP REQEST for data */
	memset(strrequest,0,sizeof(strrequest));
	strcat(strrequest,"/list=");
	strcat(strrequest,sname);
	if( iw_http_request("hq.sinajs.cn", strrequest, data) !=0 ) {
		egi_sleep(0,0,1000);
		//tm_delayms(1000);
		continue;
	}
	pt=cstr_split_nstr(data,"var ",1);
	if(pt !=NULL)
		printf("HTTP Reply: %s\n",pt);


	/* 2. First, get bench mark value and fdmax,fdmin.
	 * Usually bench mark value is yesterday's price, or opening price, or current price, as you like.
	 * Run once only.
	 */
	if(fbench==0)
	{
		/* 2.1 If INDEX, get current point */
		if(strstr(sname,"s_")) {
			pn=1;
			data_point[num-1]=atof(cstr_split_nstr(data,",",pn));/* get current point */
			pt=cstr_split_nstr(data,",",pn+1); /* get deviation pointer */
			if(pt==NULL) {
				printf("%s: reply data format error.\n",__func__);
				return -1;
			}
			fbench = data_point[num-1]-atof(pt);  /* yesterday's point=current-dev */
			printf("Yesterday's Index: fbench=%0.2f\n",fbench);
		}
		/* 2.2 If STOCK PRICE, get yesterday's closing price as bench mark */
		else {
			pn=2;
			data_point[num-1]=atof(cstr_split_nstr(data,",",pn+1));/* get current point */
			pt=cstr_split_nstr(data,",",pn); /* get yesterday closing price pointer */
			if(pt==NULL) {
				printf("%s: reply data format error.\n",__func__);
				return -2;
			}
			fbench=atof(pt);
			printf("Yestearday's Price: fbench=%0.2f\n",fbench);
		}

		/* <<<<<<  You may ignore above and redefine fbench here  >>>>>> */
//		fbench=data_point[num-1];

		/* init fdmax,fdmin whit current point */
		fdmax=data_point[num-1];
		fdmin=data_point[num-1];

		/* init upper and lower limit bar */
		upp_limit=fbench+famp;
		low_limit=fbench-famp;

		/* Init data_point[]  with opening price or current price or fbench (yesterday's points/price) */
		for(i=0;i<num;i++)
			data_point[i]=data_point[num-1];//fbench;
	}

        /* extract data:
	 *
	 * 1. INDEX data format:
	 *  	index name, current_point,up_down value,up_down_rate, volume, turn_over,
	 *
	 * 2. STOCK data format:
	 *  	stock name, Today's opening_price, Yestearday 's closing_price, current_price,
	 *	Highest price, Lowest price, Buy 1 price, Sell 1 price, number of traded shares/100
	 *	traded value/10000, Buy1 Volume, Buy1 Price,......Sell1 Volume, Sell1 Price....
	 */

      /* ( if  INDEX  POINT  ) */
	if(strstr(sname,"s_")){  /* Get current STOCK INDEX:  after 1st ',' token */
		pn=1;
	}
      /* ( if STOCK  PRICE  ) */
	else {		/* Get current STOCK PRICE: after 3rd ',' token */
		pn=3;
	}

      /* Get pointer to current data */
      pt=cstr_split_nstr(data,",",pn);
      if( pt==NULL ) continue;


     /* <<<<<<<<<<<<<<<<<   DATA SAVE AND COMPRESSION    >>>>>>>>>>>>>> */
     printf("Start data save and compression  ((( ---> ");
#if 0		/* METHOD 1: shift data and push point value into data_point[num-1] */
		memmove(data_point,data_point+1,sizeof(data_point)-sizeof(data_point[0]));
#endif

#if 0		/* METHOD 2: Common_Average compression, but keep lastest data_point[num-1]  */
		for(i=0;i<num-1;i++) {
			//data_point[i]=(data_point[i]+data_point[i+1])/2.0; /* AVERAGE */
			//data_point[i]=(data_point[i]*10.0+data_point[i+1])/11.0; /* SMOOTH_BIA AVERAGE */
			data_point[i]=( data_point[i]*( (num-i)*1 )+ data_point[i+1])
							/ ( (num-i)*1+1 ); /* SMOOTH_BIA AVERAGE */
		}
#endif

#if 0		/* METHOD 3: Interpolation compression  */
		for(i=0;i<num-1;i++) { /* data_point[num-1] unchanged */
			data_point[i]=data_point[i]*(num-2-i)/(num-2)+data_point[i+1]*i/(num-2);
		}
#endif

#if 1	/* METHOD 4: Fold_Average Compression */

	if( !market_closed && !buff_filled )  /* first, update all data_point[] */
	{
		memmove(data_point,data_point+1,sizeof(data_point)-sizeof(data_point[0]));
		nshift++;
		if(nshift==num) /* all data_point[] refreshed */
			buff_filled=true;
	}
	else if( !market_closed )  /* then, compress data by averaging */
	{
		/* remember current npstore for next for() */
		k=npstore;
		/* add next data to get average and update data_point[npstore] */
		data_point[k]=data_point[k]*navg_count+data_point[k+1];
		navg_count++;
		data_point[k] /= navg_count;
		printf("npstore=%d, navg_count=%d\n, current navg=%d",npstore,navg_count, navg);

		/* check if curretn data_point[x] avg compression completes */
		if(navg_count==navg) {
			navg_count=1; /* reset to 1, for data_point[] already initialized with bench data */
			npstore++; /* next data_point index for avg compression */

			/* 1. If in mid of round: if not 1st compression, then adjust navg */
			if( ncompress > 0 && npstore == (num>>1) ) {
				navg=navg_first*(1<<ncompress); /* ajust navg for uncompressed data */
			}

			/* 2. If end of round: reset npstore, !!!!! keep data_point[num-1] alway uncompressed !!!! */
			else if(npstore == num-1) { /* data[num-1] never compress, reset npstore to 0 */
				ncompress++;
				npstore=0;  /* data[num-1] never compress, reset npstore to 0 */
				/* !!! adjust navg to 2 for first half data_point[x], which are already compressed */
				navg=2;
			}
		}

		/* just shift followed data */
#if 0
		for( ++k; k<num-1; k++)
		{
			data_point[k]=data_point[k+1];
		}
#else
		/* k as current [npstore] */
		memmove(data_point+(k+1), data_point+(k+1)+1, sizeof(data_point[0]) * (num-1-(k+1)) );
#endif

	}
	else /* if market closed, do NOT compress data, only for display */
		memmove(data_point,data_point+1,sizeof(data_point)-sizeof(data_point[0]));

#endif

     printf(" ---> End.  ))) \n");


     /* <<<<<<<<<<<<<  Update Pxy Data and Parameters    >>>>>>>>>>>>>> */

		/* 1. Update the latest point value */
                data_point[num-1]=atof(pt);
		EGI_PLOG(LOGLV_INFO,"[%02d:%02d:%02d] latest data_point=%0.2f \n",
					tm_local->tm_hour,tm_local->tm_min,tm_local->tm_sec,data_point[num-1]);

		/* 2. Update fdmin, fdmax, flow_dev, fupp_dev */
		if(data_point[num-1] < fdmin) {
			fdmin=data_point[num-1];
			/* update lower deviation */
			if(fdmin < fbench)
				flow_dev=fbench-fdmin;
		}
		else if(data_point[num-1] > fdmax) {
			fdmax=data_point[num-1];
			/* updata upper deviation */
			if(fdmax > fbench)
				fupp_dev=fdmax-fbench;
		}
		/* Init value as we set before,fdmix=fdmax=data_point[num-1]
		 * Just trigger to calculate fupp_dev and flow_dev.
		 */
		else if(fdmin==fdmax) {
			if(fdmax > fbench)
			  	fupp_dev=fdmax-fbench;
			if(fdmin < fbench)
				flow_dev=fbench-fdmin;
		}

		/* 3. Update upp and low grid number
		 * NOTE: upp_ng is NOT equal to fdmax, also low_ng is NOT equal to fdmin.!!!
		 * They are the same only when fdmax>fbench and fdmin<fbench.
		 */
		if(flow_dev<fupp_dev) {
			/* estimate low and upp ng */
			low_ng=flow_dev*ng/(fupp_dev+flow_dev)+1;
			upp_ng=fupp_dev*ng/(fupp_dev+flow_dev);
			/* again */
			low_ng=ng*low_ng/(low_ng+upp_ng);
			//if(low_ng==0)low_ng=1; /* At least 1 grid */
			upp_ng=ng-low_ng;
			/* update funit */
			/* flow_dev may BE ZERO ! */
			funit=fmin(1.0*upp_ng*hlgap/fupp_dev, 1.0*low_ng*hlgap/flow_dev);
		}
		else if(flow_dev>fupp_dev) {
			/* estimate low and upp ng */
			upp_ng=fupp_dev*ng/(fupp_dev+flow_dev)+1;
			low_ng=flow_dev*ng/(fupp_dev+flow_dev);
			/* again */
			upp_ng=ng*upp_ng/(low_ng+upp_ng);
			//if(upp_ng==0)upp_ng=1; /* At least 1 grid */
			low_ng=ng-upp_ng;
			/* update funit */
			/* fupp_dev may BE ZERO */
			if(fupp_dev !=0 )
				funit=fmin(1.0*upp_ng*hlgap/fupp_dev, 1.0*low_ng*hlgap/flow_dev);
			else
				funit=1.0*low_ng*hlgap/flow_dev;
		}
		else  { //elseif(flow_dev==fupp_dev)
			upp_ng=ng>>1;
			low_ng=ng>>1;
		}

		/* 4. Update offy for benchmark line */
		offy=chart_y0+hlgap*upp_ng;

		/* 5. Update upp_limit and low_limit */
		upp_limit=fbench+upp_ng*hlgap/funit;
		low_limit=fbench-low_ng*hlgap/funit;

		printf("funit=%0.3f, hlgap/funit=%0.3f \n",funit,hlgap/funit);
		printf("low_ng=%d, upp_ng=%d.\n",low_ng,upp_ng);
		printf("upp_limit=%0.2f, low_limit=%0.2f \n",upp_limit,low_limit);
		printf("fupp_dev*funit/hlgap=%0.1f <----> upp_ng=%d \n",fupp_dev*funit/hlgap,upp_ng);
		printf("flow_dev*funit/hlgap=%0.2f <----> low_ng=%d \n",flow_dev*funit/hlgap,low_ng);

//        } /* end of if(pt!=NULL) */


	/* update the latest pxy according to the latest data_point*/
	printf("Update Pxy[] for drawing.  start  ---> ");
	for(i=0;i<num;i++)
	{
		/* update PXY according to data_point[] */
		if(data_point[i]>fbench)
		 	pxy[i].y=offy-(data_point[i]-fbench)*funit;
		else
		 	pxy[i].y=offy+(fbench-data_point[i])*funit;
	}
	printf(" End.\n ");

	wcount++;
	egi_sleep(2,0,REQUEST_INTERVAL_TIMEMS); /* tick off!!! */
        //tm_delayms(REQUEST_INTERVAL_TIMEMS);
	/* Go on only if wcount%4==0;  wcount>5 to let go at least one time */
	if( (wcount & 0b11) != 0 && wcount>5 ) continue;

	/* <<<<<<<    Flush FB and Turn on FILO  >>>>>>> */
	printf("Flush pixel data in FILO, start  ---> ");
        fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
        fb_filo_on(&gv_fb_dev); /* start collecting old FB pixel data */
	printf("End.\n");

	/* 1. Draw bench_mark line according to low_ng and upp_ng */
	printf("Draw bench mark line, start  ---> ");
	fbset_color(WEGI_COLOR_GRAY3);
 	draw_line(&gv_fb_dev,0,offy,240-1,offy);
	printf("End.\n");

	/* 2. Draw poly line of Pxy */
	printf("Draw poly lines from Pxy[], start  ---> ");
	fbset_color(WEGI_COLOR_YELLOW);//GREEN);
	draw_pline(&gv_fb_dev, pxy, num, 0);
	printf("End.\n");

	/* 3. ----- Draw marks and symbols ------ */
	printf("Draw marks and symbols, start  ((( ---> ");
	/* 3.1 INDEX or STOCK name */
        symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, WEGI_COLOR_CYAN,
                                        	1, 60, 5 ,sname); /* transpcolor, x0,y0, str */
	/* 3.2 fbench value */
	sprintf(strdata,"%0.2f",fbench);
        symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, WEGI_COLOR_GRAY3,
                                        	1, 80, chart_y0+upp_ng*hlgap-20 ,strdata); /* transpcolor, x0,y0, str */
	/* 3.3 upp_limit value */
	sprintf(strdata,"%0.2f",upp_limit);
        symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, WEGI_COLOR_GRAY3,
                                        	1, 80, chart_y0-20 ,strdata); /* transpcolor, x0,y0, str */
	/* 3.3 low_limit value */
	sprintf(strdata,"%0.2f",low_limit);
        symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, WEGI_COLOR_GRAY3,
                                        	1, 80, chart_y0+wh-20, strdata); /* transpcolor, x0,y0, str */
	/* 3.4 draw fdmax mark and symbol */
	if(fdmax > fbench) {
		py=offy-(fdmax-fbench)*funit;
		//symcolor=WEGI_COLOR_RED;
	}
	else {
		py=offy+(fbench-fdmax)*funit;
		//symcolor=WEGI_COLOR_GREEN;
	}
	symcolor=WEGI_COLOR_RASPBERRY;
	fbset_color(symcolor);
	draw_wline(&gv_fb_dev, 0,py,60,py,0);
	sprintf(strdata,"%0.2f",fdmax); //fbench+(wh/2)/unit_upp);
        symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, symcolor,
                                        	1, 0, py-20 ,strdata); /* transpcolor, x0,y0, str */
	/* 3.5 draw fdmin mark and symbol*/
	if(fdmin > fbench) {
		py=offy-(fdmin-fbench)*funit;
		//symcolor=WEGI_COLOR_RED;
	}
	else {
		py=offy+(fbench-fdmin)*funit;
		//symcolor=WEGI_COLOR_GREEN;
	}
	printf(" draw fdmin mark: fdmin=%0.2f.\n",fdmin);
	symcolor=WEGI_COLOR_BLUE;
	fbset_color(symcolor);
	draw_wline(&gv_fb_dev, 0,py,60,py,0);
	sprintf(strdata,"%0.2f",fdmin); //fbench+(wh/2)/unit_upp);
        symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, symcolor,
                                        	1, 0, py-20 ,strdata); /* transpcolor, x0,y0, str */
	/* 3.6 write current point/price value */
	symcolor=WEGI_COLOR_WHITE;
	sprintf(strdata,"%0.2f",data_point[num-1]);
        symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, symcolor,
        // move with pxy              	1, 240-70, pxy[num-1].y-20, strdata); /* transpcolor, x0,y0, str */
					1, 240-70, chart_y0-40, strdata); /* display on top */
	/* 3.7 write up_down value */
	/* draw a gray pad */
	//draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY3,
	//			  240-70, pxy[num-1].y-20-15, 240-70+60, pxy[num-1].y-20);

	if(data_point[num-1]>fbench)symcolor=WEGI_COLOR_RED;
	else {	symcolor = WEGI_COLOR_GREEN; }	sprintf(strdata,"%+0.2f",data_point[num-1]-fbench);
        symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, symcolor,
        // move with pxy                     	1, 240-70, pxy[num-1].y-20-20, strdata); /* transpcolor, x0,y0, str */
					1, 90, chart_y0-40, strdata); /*fixed position, display on top */

	/* 3.8 write up_down percentage */
	sprintf(strdata,"%%%+0.2f",(data_point[num-1]-fbench)*100/fbench);
        symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, symcolor,
                                       	1, 0, chart_y0-40, strdata); /* transpcolor, x0, y0, str */

	printf(" ---> End.  ))) \n");

	/* if market recessed or closed */
	if(market_recess)
		symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, WEGI_COLOR_RED,
						1, 20,chart_y0+wh+10, "Market Recess" );
	else if(market_closed)
		symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, WEGI_COLOR_RED,
						1, 20,chart_y0+wh+10, "Market Closed" );
	else { /* otherwise display favorate stock */
		//symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, WEGI_COLOR_WHITE,
		//				1, 20,320-35, "Trade Time" );

		/* favorate stock price */
		memset(strrequest,0,sizeof(strrequest));
		strcat(strrequest,"/list=");
		strcat(strrequest,favor_stock[wcount%3]);
		while( iw_http_request("hq.sinajs.cn", strrequest, data) !=0 ) {
			egi_sleep(0,0,300);
			//tm_delayms(300);
		}
		if(strstr(data,",")==NULL)
		{
			printf("Return data error!\n %s\n", data);
			wcount++;
			continue;
		}
		memset(strdata,0,sizeof(strdata));
		tprice=atof(cstr_split_nstr(data,",",3)); /* current price */
		yprice=atof(cstr_split_nstr(data,",",2)); /* yesterday price */
		if(tprice-yprice>0)symcolor=WEGI_COLOR_RED;
		else symcolor=WEGI_COLOR_GREEN;
		sprintf(strdata,"%s   %0.2f   %%%+0.2f",favor_stock[wcount%3],tprice,(tprice-yprice)*100/yprice);
		symbol_string_writeFB(&gv_fb_dev, &sympg_testfont, symcolor,
						1, 5,chart_y0+wh+10, strdata ); //320-35
	}

	/* <<<<<<<    Turn off FILO  >>>>>>> */
	fb_filo_off(&gv_fb_dev);

//	wcount++;
//	egi_sleep(0,0,REQUEST_INTERVAL_TIMEMS);
        //tm_delayms(REQUEST_INTERVAL_TIMEMS);
}

/* <<<<<<<<<<<<<<<<<<<<<  END TEST  <<<<<<<<<<<<<<<<<<*/

  	/* quit logger */
  	egi_quit_log();

        /* close fb dev */
        munmap(gv_fb_dev.map_fb,gv_fb_dev.screensize);
        close(gv_fb_dev.fdfd);

	return 0;
}

