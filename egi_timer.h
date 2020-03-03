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


#define TM_TICK_INTERVAL	2000 //5000  /* us */
#define TM_DBCLICK_INTERVAL	400000 /*in us,  Max for double click   */

/* shared data */
extern struct itimerval tm_val, tm_oval;
extern const char *str_weekday[];
extern const char *str_month[];
extern const char *stru8_weekday[];
extern const char *stru8_month[];
extern char tm_strbuf[];


/* functions */
long long unsigned int tm_get_tmstampms(void);
void tm_get_strtime(char *tmbuf);
void tm_get_strday(char *tmdaybuf);
void tm_get_ustrday(char *udaybuf);
void tm_sigroutine(int signo);
void tm_settimer(int us);
//static void tm_tick_settimer(int us);
//static void tm_tick_sigroutine(int signo);
void tm_start_egitick(void);
long long unsigned int tm_get_tickcount(void);
void tm_delayms(unsigned long ms);/* !!! To be abandoned !!!! Not good! */
bool tm_pulseus(long long unsigned int gap, unsigned int t); /* gap(us) */
unsigned int tm_diffus(struct timeval t_start, struct timeval t_end);
int tm_signed_diffms(struct timeval tm_start, struct timeval tm_end);
void egi_sleep(unsigned char fd, unsigned int s, unsigned int ms);

#endif
