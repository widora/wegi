/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/
#ifndef __EGI_TIMER_H__
#define __EGI_TIMER_H__

#include <sys/time.h>
#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
 extern "C" {
#endif

#define TM_TICK_INTERVAL	2000 //5000  /* us */
#define TM_DBCLICK_INTERVAL	400000 /*in us,  Max for double click   */

/* An EGI_CLOCK: to record/estimate time pasted(timeout). NOT for measuring time cost for a process!!! */
typedef struct
{
        #define ECLOCK_STATUS_IDLE      0          /* Default, NOT activated */
        #define ECLOCK_STATUS_RUNNING   1          /* tm_start is set, tm_end NOT yet */
        #define ECLOCK_STATUS_PAUSE     (1<<1)     /* tm_start, tm_end are both set. tm_cost updated. to be continued.. */
        #define ECLOCK_STATUS_STOP      (1<<2)     /* tm_start, tm_end are both set. tm_cost updated */
        int  status;

        struct timeval tm_start; /* Start time */
        struct timeval tm_end;   /* End time */
	struct timeval tm_cost;  /* Addup of running time, with each duration of tm_end - tm_start. */
        struct timeval tm_dur;   /* Preset time duration as tm_end - tm_start. */

} EGI_CLOCK;  /* EGI_WATCH is better */
#define EGI_WATCH EGI_CLOCK

/* shared data */
extern struct itimerval tm_val, tm_oval;
extern const char *str_weekday[];
extern const char *str_month[];
extern const char *stru8_weekday[];
extern const char *stru8_month[];
extern char tm_strbuf[];


/* functions */
long long unsigned int tm_get_tmstampms(void);
/*  SUBSTITUE: size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) */
/* int ftime(struct timeb *tp): returns the current time as seconds and milliseconds since the Epoch */
void tm_get_strtime(char *tmbuf);
void tm_get_strtime2(char *tmbuf, const char *appen);
void tm_get_strday(char *tmdaybuf);
void tm_get_strdaytime(char *dtbuf);
void tm_get_ustrday(char *udaybuf);
void tm_sigroutine(int signo);
void tm_settimer(int us);
//static void tm_tick_settimer(int us);
//static void tm_tick_sigroutine(int signo);
void tm_start_egitick(void);
long long unsigned int tm_get_tickcount(void);
void tm_delayms(unsigned long ms);/* !!! To be abandoned !!!! Not good! */
bool tm_pulseus(long long unsigned int gap, unsigned int t); /* gap(us) */
unsigned long tm_diffus(struct timeval t_start, struct timeval t_end);
int tm_signed_diffms(struct timeval tm_start, struct timeval tm_end);
void egi_sleep(unsigned char fd, unsigned int s, unsigned int ms);

/* EGI CLOCK */
int egi_clock_start(EGI_CLOCK *eclock);
int egi_clock_stop(EGI_CLOCK *eclock);
int egi_clock_restart(EGI_CLOCK *eclock);
int egi_clock_pause(EGI_CLOCK *eclock);
long long egi_clock_readCostUsec(EGI_CLOCK *eclock);
long long egi_clock_peekCostUsec(EGI_CLOCK *eclock);

#ifdef __cplusplus
 }
#endif

#endif
