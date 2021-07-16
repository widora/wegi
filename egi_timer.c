/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

NOTE:
1. A pthread_join() failure may block followed sleep functons such
   as egi_sleep() and tm_delay() permanently!???
2. Tick alarm signal may conflic with other app thread!


 		(--- Type of: time_t, suseconds_t ---)
time_t	=-> long
suseconds_t =-> long

linux/types.h:69:typedef __kernel_time_t		time_t;
uapi/asm-generic/posix_types.h:88:typedef __kernel_long_t	__kernel_time_t;

linux/types.h:24:typedef __kernel_suseconds_t	suseconds_t;
uapi/linux/time.h:17:	__kernel_suseconds_t	tv_usec;
uapi/asm-generic/posix_types.h:40:typedef  __kernel_long_t	__kernel_suseconds_t;

uapi/asm-generic/posix_types.h:14:typedef long		__kernel_long_t;

		(--- Struct of time var. ---)

time_t	=-> long

struct timespec {
	time_t   tv_sec;     seconds
        long     tv_nsec;    nanoseconds
};

struct timeval {
        time_t      tv_sec;    seconds
        suseconds_t tv_usec;   microseconds
};

struct tm {
        int tm_sec;    Seconds (0-60)
        int tm_min;    Minutes (0-59)
        int tm_hour;   Hours (0-23)
        int tm_mday;   Day of the month (1-31)
        int tm_mon;    Month (0-11)
        int tm_year;   Year - 1900
        int tm_wday;   Day of the week (0-6, Sunday = 0)
        int tm_yday;   Day in the year (0-365, 1 Jan = 0)
        int tm_isdst;  Daylight saving time
};

3. Conversion for 'struct time_t' AND 'struct tm':
   struct tm *localtime(const time_t *timep);
   time_t mktime(struct tm *tm);

4. double difftime(time_t time1, time_t time0);


Journal:
2021-05-03:
	1. Add egi_clock_restart().
2021-07-10:
	1. egi_clock_restart(): Force status to be RUNNING before
	   calling egi_clock_stop(). So an IDLE clock can also restart.
2021-07-13:
	1. egi_clock_readCostUsec(), egi_clock_peekCostUsec(): Modified to be long long type.

TODO:
	--- Critical ---
1. time_t overflow for 2038 YEAR problem.
2. To apply POSIX timer APIs:
   timer_create(), timer_settime(), timer_gettimer(), timer_getoverrun(), timer_delete()

Midas Zhou
-----------------------------------------------------------------*/
#include <signal.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h> /* usleep */
#include <stdbool.h>
#include <errno.h>
#include "egi_timer.h"
#include "egi_symbol.h"
#include "egi_fbgeom.h"
#include "egi_debug.h"
#include "dict.h"

#define EGI_ENABLE_TICK  0	/* Tick alarm signal may conflic with .curl? inet/unet? ... */

struct itimerval tm_val, tm_oval;

char tm_strbuf[50]={0};
const char *str_weekday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
//const char *str_weekday[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
const char *str_month[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Sub"};

/* encoding uft8 */
const char *stru8_weekday[]={"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};
const char *stru8_month[]={"一月","二月","三月","四月","五月","六月","七月","八月","九月","十月","十一月","十二月"};

/* global tick */
static struct itimerval tm_tick_val; //tm_tick_oval;
static long long unsigned int tm_tick_count=0;

//   struct timespec ts;
//   clock_gettime(CLOCK_MONOTONIC, &ts);

/*-------------------------------------
Get time stamp in ms
--------------------------------------*/
long long unsigned int tm_get_tmstampms(void)
{
	struct timeval tmval;

	gettimeofday(&tmval, NULL);

	return ( ((long long unsigned int)tmval.tv_sec)*1000+tmval.tv_usec/1000);
}


/*  SUBSTITUE: size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) */

/*---------------------------------------------
 Get local time string in form of:
 		H:M:S 	(in 24hours)
 The caller must ensure enough space for tmbuf.
---------------------------------------------*/
void tm_get_strtime(char *tmbuf)
{
	time_t tm_t; /* time in seconds */
	struct tm *tm_s; /* time in struct */

	time(&tm_t);
	tm_s=localtime(&tm_t);
	/*  tm_s->tm_year start from 1900
	    tm_s->tm_mon start from 0
	*/
	sprintf(tmbuf,"%02d:%02d:%02d",tm_s->tm_hour,tm_s->tm_min,tm_s->tm_sec);
}


/*---------------------------------------------
 Get local time string in form of:
 		H:M:S.ms 	(in 24hours)
 The caller must ensure enough space for tmbuf.
---------------------------------------------*/
void tm_get_strtime2(char *tmbuf, const char *appen)
{
	time_t tm_t; /* time in seconds */
	struct tm *tm_s; /* time in struct */
	struct timeb tp;

	time(&tm_t);
	tm_s=localtime(&tm_t);

	ftime(&tp);

	/*  tm_s->tm_year start from 1900
	    tm_s->tm_mon start from 0
	*/
	sprintf(tmbuf,"%d-%d-%d_%02d:%02d:%02d.%d%s",
			tm_s->tm_year+1900,tm_s->tm_mon+1,tm_s->tm_mday,
				tm_s->tm_hour,tm_s->tm_min,tm_s->tm_sec, tp.millitm, appen);
}


/*---------------------------------------------
Get local time in string, in form of:
 	Year_Mon_Day  Weekday
The caller must ensure enough space for tmdaybuf.
---------------------------------------------*/
void tm_get_strday(char *daybuf)
{
	time_t tm_t; 	  /* time in seconds */
	struct tm *tm_s;  /* time in struct tm */

	time(&tm_t);
	tm_s=localtime(&tm_t);

	sprintf(daybuf,"%d-%d-%d   %s", tm_s->tm_year+1900,tm_s->tm_mon+1,tm_s->tm_mday, \
					str_weekday[tm_s->tm_wday] );
}


/*---------------------------------------------
Get local time in string, in form of:
 	Year_Mon_Day H:M:S
The caller must ensure enough space for dtbuf.
---------------------------------------------*/
void tm_get_strdaytime(char *dtbuf)
{
	time_t tm_t; 	  /* time in seconds */
	struct tm *tm_s;  /* time in struct tm */

	time(&tm_t);
	tm_s=localtime(&tm_t);

	sprintf(dtbuf,"%d-%d-%d %02d:%02d:%02d",
			tm_s->tm_year+1900,tm_s->tm_mon+1,tm_s->tm_mday,
			tm_s->tm_hour,tm_s->tm_min,tm_s->tm_sec);
}


/*-----------------------------------
Get local time in uft-8 string:
	x月x日
The caller must enusre enough space for
udaybuf.
------------------------------------*/
void tm_get_ustrday(char *udaybuf)
{
        time_t tm_t;      /* time in seconds */
        struct tm *tm_s;  /* time in struct tm */

        time(&tm_t);
        tm_s=localtime(&tm_t);

        sprintf(udaybuf,"%d月%d日", tm_s->tm_mon+1,tm_s->tm_mday);

}


/*------------------------------
 	Timer routine
-------------------------------*/
void tm_sigroutine(int signo)
{
	if(signo == SIGALRM)
	{

	/* ------- routine action every tick -------- */
#if 0	// put heavy action here is not a good idea ??????  !!!!!!
        /* get time and display */
        tm_get_strtime(tm_strbuf);
        wirteFB_str20x15(&gv_fb_dev, 0, (30<<11|45<<5|10), tm_strbuf, 60, 320-38);
        tm_get_strday(tm_strbuf);
        symbol_string_writeFB(&gv_fb_dev, &sympg_testfont,0xffff,45,2,tm_strbuf);
        //symbol_string_writeFB(&gv_fb_dev, &sympg_testfont,0xffff,32,90,tm_strbuf);
#endif
	}

	/* restore tm_sigroutine */
	signal(SIGALRM, tm_sigroutine);
}

/*---------------------------------
set timer for SIGALRM
us: time interval in us.
----------------------------------*/
void tm_settimer(int us)
{
	/* time left before next expiration  */
	tm_val.it_value.tv_sec=0;
	tm_val.it_value.tv_usec=us;

	/* time interval for periodic timer */
	tm_val.it_interval.tv_sec=0;
	tm_val.it_interval.tv_usec=us;

	setitimer(ITIMER_REAL,&tm_val,NULL); /* NULL get rid of old time value */
}

/*-------------------------------------
set timer for SIGALRM of global tick
us: global tick time interval in us.
--------------------------------------*/
static void tm_tick_settimer(int us)
{
	/* time left before next expiration  */
	tm_tick_val.it_value.tv_sec=0;
	tm_tick_val.it_value.tv_usec=us;
	/* time interval for periodic timer */
	tm_tick_val.it_interval.tv_sec=0;
	tm_tick_val.it_interval.tv_usec=us;

	/* use real time */
	setitimer(ITIMER_REAL,&tm_tick_val,NULL); /* NULL get rid of old time value */
}

/* -------------------------------------
  global tick timer routine
--------------------------------------*/
static void tm_tick_sigroutine(int signo)
{
#if EGI_ENABLE_TICK
	if(signo == SIGALRM) {
		tm_tick_count+=1;
	}

	/* restore tm_sigroutine */
	signal(SIGALRM, tm_tick_sigroutine);
#endif
}

/*-------------------------------
    start egi_system tick
-------------------------------*/
void tm_start_egitick(void)
{
#if EGI_ENABLE_TICK
        tm_tick_settimer(TM_TICK_INTERVAL);
        signal(SIGALRM, tm_tick_sigroutine);
#endif
}


/*-----------------------------
    return tm_tick_count
------------------------------*/
long long unsigned int tm_get_tickcount(void)
{
	return tm_tick_count;
}

/*----- WARNING!!!! A lovely BUG :> -------
delay ms, at lease TM_TICK_INTERVAL/2000 ms

if ms<0, return.
-----------------------------------------*/
void tm_delayms(unsigned long ms)
{
	if(ms==0) return;

#if EGI_ENABLE_TICK
	unsigned int nticks;
	long long unsigned int tm_now;

	if(ms < TM_TICK_INTERVAL/1000)
		nticks=TM_TICK_INTERVAL/1000;
	else
		nticks=ms*1000/TM_TICK_INTERVAL;

	tm_now=tm_tick_count;

	while(tm_tick_count-tm_now < nticks)
	{
		usleep(500); //usleep(1000) ?usleep disrupted by the timer signal? */
	}

#else
	egi_sleep(1,0,ms);

#endif
}


/*------------------  OBSOLETE -----------------------------
return:
    TRUE: in every preset time interval gap(us), or time
interval exceeds gap(us).
    FALSE: otherwise

gap:	set period between pules
t:	number of tm_pulse timer to be used.

return:
	true	pulse hits
	false	wait pulse or fails to call pulse
----------------------------------------------------------*/
static struct timeval tm_pulse_tmnew[10]={0};
static struct timeval tm_pulse_tmold[10]={0};
bool tm_pulseus(long long unsigned int gap, unsigned int t) /* gap(us) */
{
	//struct timeval tmnew,tmold;
	if(t>=10) {
		printf("tm_pulseus(): timer number out of range!\n");
		return false;
	}

	/* first init tmold */
	if(tm_pulse_tmold[t].tv_sec==0 && tm_pulse_tmold[t].tv_usec==0)
		gettimeofday(&tm_pulse_tmold[t],NULL);

	/* get current time value */
	gettimeofday(&tm_pulse_tmnew[t],NULL);

	/* compare timers */
	if(tm_pulse_tmnew[t].tv_sec*1000000+tm_pulse_tmnew[t].tv_usec >=
				 tm_pulse_tmold[t].tv_sec*1000000+tm_pulse_tmold[t].tv_usec + gap)
	{
		/* reset tmold */
		tm_pulse_tmold[t].tv_sec=0;
		tm_pulse_tmold[t].tv_usec=0;
		tm_pulse_tmnew[t]=tm_pulse_tmold[t];

		return true;
	}
	else
		return false;
}


/*---------------------------------------------------------
return time difference in us, as an unsigned value.
t_start:	start time
t_end:		end time
-----------------------------------------------------------*/
unsigned long tm_diffus(struct timeval t_start, struct timeval t_end)
{
	long ds=t_end.tv_sec-t_start.tv_sec;
	long dus=t_end.tv_usec-t_start.tv_usec;

	long td=ds*1000000+dus;

	return ( td>0 ? td : -td );
}

/*------------------------------------------------------------------
return time difference in ms, as a signed value.
tm_start:	start time
tm_end:		end time
------------------------------------------------------------------*/
inline int tm_signed_diffms(struct timeval tm_start, struct timeval tm_end)
{
        int time_cost;
        time_cost=(tm_end.tv_sec-tm_start.tv_sec)*1000	\
			+(tm_end.tv_usec-tm_start.tv_usec)/1000;

        return time_cost;
}

/*------------------------------------------------------------
Use select to sleep

@s:	seconds
@ms:	millisecond, 0 - 999

NOTE:
	1. In thread, it's OK. NO effect with egi timer????
	2. In Main(), it'll fail, conflict with egi timer???
--------------------------------------------------------------*/
//static unsigned char gv_tm_fd[128]={0};
void egi_sleep(unsigned char fd, unsigned int s, unsigned int ms)
{
#if EGI_ENABLE_TICK
	tm_delayms(s*1000+ms);
#else
	int err;
	struct timeval tmval;
	tmval.tv_sec=s;
	tmval.tv_usec=1000*ms;

	do{
		err=select(fd+1,NULL,NULL,NULL,&tmval); /* wait until timeout */
		if(err<0)egi_dperr("select error!");
	}while( err < 0 && errno==EINTR ); 	      /* Ingore any signal */

#endif
}


/*---------------------------------------------
Start a clock, record tm_start.
Note:
1. tm_cost will be cleared if prevous status
   is ECLOCK_STATUS_STOP.

TODO: To compare with
      int clock_gettime(clockid_t clk_id, struct timespec *tp);
   XXX Check clock resolution first
       clock_getres(CLOCK_REALTIME, &tp)  --> Clock resolution:  0.1 second!!!

Return:
	0	OK
	<0	Fails
---------------------------------------------*/
int egi_clock_start(EGI_CLOCK *eclock)
{
	int ret=0;

	/* Check status */
	if(eclock==NULL)
		return -1;

	switch(eclock->status)
	{
		case ECLOCK_STATUS_RUNNING:
	     		printf("%s: ECLOCK is running already!\n",__func__);
			ret=-2;
			break;
		case ECLOCK_STATUS_PAUSE:
	     		//printf("%s: ECLOCK is paused, start to activate and continue...\n",__func__);
			/* Record tm_start and set status */
			if( gettimeofday(&eclock->tm_start,NULL)<0 )
				return -3;
			/* Reset status */
			eclock->status=ECLOCK_STATUS_RUNNING;
                        break;
		case ECLOCK_STATUS_IDLE:
		case ECLOCK_STATUS_STOP:
			/* Reset tm_cost */
			#if 0
			eclock->tm_cost.tv_sec=0;
			eclock->tm_cost.tv_usec=0;
			#else
			timerclear(&eclock->tm_cost);
			#endif

			/* Record tm_start and set status */
			if( gettimeofday(&eclock->tm_start,NULL)<0 ) {
				egi_dperr("gettimeofday");
				ret=-3;
				break;
			}

			/* Reset status */
			eclock->status=ECLOCK_STATUS_RUNNING;
		     	break;
		default:
		     	printf("%s: ECLOCK status unrecognizable!\n",__func__);
			ret=-4;
			break;
	}

	return ret;
}

/*-----------------------------------------------
Stop a clock, record tm_end and update tm_cost.
Return:
	0	OK
	<0	Fails
------------------------------------------------*/
int egi_clock_stop(EGI_CLOCK *eclock)
{
	int ret=0;

	/* Check status */
	if(eclock==NULL)
		return -1;

	switch(eclock->status)
	{
		case ECLOCK_STATUS_IDLE:
		     	printf("%s: ECLOCK status is IDLE!\n",__func__);
		     	ret=-2;
		     	break;
		case ECLOCK_STATUS_RUNNING:
			/* Record tm_end and set status */
			if( gettimeofday(&eclock->tm_end,NULL)<0 ) {
				egi_dperr("gettimeofday");
				return -3;
			}

			/* Update tm_cost */
		        timersub(&eclock->tm_end, &eclock->tm_start, &eclock->tm_cost);

			/* Reset status */
			eclock->status=ECLOCK_STATUS_STOP;
			break;
		case ECLOCK_STATUS_PAUSE:
		     	printf("%s: Stop a paused ECLOCK!\n",__func__);
			/* Reset status */
                        eclock->status=ECLOCK_STATUS_STOP;
                        break;
		case ECLOCK_STATUS_STOP:
		     	printf("%s: ECLOCK already paused/stoped!\n",__func__);
		     	break;

		default:
		     	printf("%s: ECLOCK status unrecognizable!\n",__func__);
			ret=-3;
			break;
	}

	return ret;
}


/*--------------------------------------
Restart a clock.
If current eclock status is IDLE, force
to be RUNNING to make egi_clock_stop()
succeed.

Return:
	0	OK
	<0	Fails
--------------------------------------*/
int egi_clock_restart(EGI_CLOCK *eclock)
{
	int ret=0;

	/* Force status to be RUNNING. */
	eclock->status=ECLOCK_STATUS_RUNNING;

	ret=egi_clock_stop(eclock);
	if(ret)
		return ret;
	/* NOW status is ECLOCK_STATUS_STOP */

	ret=egi_clock_start(eclock);

	return ret;
}


/*-----------------------------------------------
Pause a clock, recoder tm_end and update tm_cost.
Pausing time will NOT be counted into tm_cost.

Return:
	0	OK
	<0	Fails
------------------------------------------------*/
int egi_clock_pause(EGI_CLOCK *eclock)
{
	int ret=0;
//	suseconds_t dus;

	/* Check status */
	if(eclock==NULL)
		return -1;

	switch(eclock->status)
	{
		case ECLOCK_STATUS_IDLE:
		     	printf("%s: ECLOCK status is IDLE!\n",__func__);
		     	ret=-2;
		     	break;
		case ECLOCK_STATUS_RUNNING:
			/* Record tm_end and set status */
			if( gettimeofday(&eclock->tm_end,NULL)<0 ) {
				ret=-3;
				break;
			}
			#if 0
			/* Update tm_cost */
			dus=eclock->tm_end.tv_usec - eclock->tm_start.tv_usec;
			if(dus>=0) {
				eclock->tm_cost.tv_usec += dus;
				if( eclock->tm_cost.tv_usec > 999999 ) {
					eclock->tm_cost.tv_sec += eclock->tm_cost.tv_usec/1000000;
					eclock->tm_cost.tv_usec = eclock->tm_cost.tv_usec%1000000;
				}
				eclock->tm_cost.tv_sec += eclock->tm_end.tv_sec - eclock->tm_start.tv_sec;
			}
			else {  /* dus<0 */
				eclock->tm_cost.tv_usec += 1000000+dus;
				eclock->tm_cost.tv_sec += eclock->tm_end.tv_sec - eclock->tm_start.tv_sec-1;
			}
			#else
			struct timeval tm_tmp;
		        timersub(&eclock->tm_end, &eclock->tm_start, &tm_tmp);
		        timeradd(&eclock->tm_cost, &tm_tmp, &eclock->tm_cost);
			#endif

			/* Reset status */
			eclock->status=ECLOCK_STATUS_PAUSE;
			break;
		case ECLOCK_STATUS_PAUSE:
		     	printf("%s: ECLOCK already paused!\n",__func__);
                        break;
		case ECLOCK_STATUS_STOP:
		     	printf("%s: ECLOCK already stopped!\n",__func__);
		     	ret=-4;
		     	break;

		default:
		     	printf("%s: ECLOCK status unrecognizable!\n",__func__);
			ret=-5;
			break;
	}

	return ret;
}

/*--------------------------------------------------------
Read tm_end - tm_start, in us.(microsecond)

		!!! --- WARNING --- !!!
A big value of tm_cost will make tus_cost overflow!
Use this function only when you are sure that tm_cost in eclock
is fairly small!

# define INT32_MIN              (-2147483647-1)
# define INT64_MIN              (-9223372036854775807LL-1)
# define INT32_MAX              (2147483647)
# define INT64_MAX              (9223372036854775807LL)


Note:
1. TEST Widora_NEO:
   Under light CPU load condition, average Max. error is 100us.
   Test usleep() will occasionally interrupted by sys schedule.

Return:
	>=0	OK, time length in us.
	<0	Fails
---------------------------------------------------------*/
long long egi_clock_readCostUsec(EGI_CLOCK *eclock)
{
	long long  tus_cost;

	/* Check status */
	if(eclock==NULL) return -1;
	if( !(eclock->status&(ECLOCK_STATUS_STOP|ECLOCK_STATUS_PAUSE)) ) {
		printf("%s: ECLOCK status error, you must stop/pause the clock first!\n",__func__);
		return -2;
	}

	tus_cost=eclock->tm_cost.tv_sec*1000000LL+eclock->tm_cost.tv_usec;
	return tus_cost;
}

/*--------------------------------------------------------
Read tm_now - tm_start, in us.(microsecond)

		!!! --- WARNING --- !!!
A big value of tm_cost will make tus_cost overflow!
Use this function only when you are sure that tm_cost in eclock
is fairly small!
	2147483647/3600000000 ~= 0.6 hour

# define INT32_MIN              (-2147483647-1)
# define INT64_MIN              (-9223372036854775807LL-1)
# define INT32_MAX              (2147483647)
# define INT64_MAX              (9223372036854775807LL)

Return:
	>=0	OK, time cost in us.
	<0	Fails
---------------------------------------------------------*/
long long egi_clock_peekCostUsec(EGI_CLOCK *eclock)
{
	struct timeval tm_now;
	struct timeval tm_cost;

	/* Check status */
	if(eclock==NULL)
		return -1;
	if( !(eclock->status & (ECLOCK_STATUS_RUNNING)) ) {
		printf("%s: ECLOCK status error, you can ONLY peek a RUNNING eclock!\n",__func__);
		return -2;
	}

	/* Timersub */
	if( gettimeofday(&tm_now, NULL)<0 ) {
		egi_dperr("gettimeofday");
		return -3;
	}

        timersub(&tm_now, &eclock->tm_start, &tm_cost);

	return 1000000LL*tm_cost.tv_sec+tm_cost.tv_usec;
}

