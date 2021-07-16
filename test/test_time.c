/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <egi_timer.h>
#include <time.h>
#include <stdint.h>

long long tm_costus(void);
long long tm_costus2(void);



/*----------------------------
	    Main()
----------------------------*/
int main(int argc, char **argv)
{

#if 1 /* TEST: CLOCK_BOOTTIME  --------*/

/***

 <uapi/linux/time.h>
 * The IDs of the various system clocks (for POSIX.1b interval timers):

#define CLOCK_REALTIME                  0
#define CLOCK_MONOTONIC                 1
#define CLOCK_PROCESS_CPUTIME_ID        2
#define CLOCK_THREAD_CPUTIME_ID         3
#define CLOCK_MONOTONIC_RAW             4
#define CLOCK_REALTIME_COARSE           5
#define CLOCK_MONOTONIC_COARSE          6
#define CLOCK_BOOTTIME                  7
#define CLOCK_REALTIME_ALARM            8
#define CLOCK_BOOTTIME_ALARM            9
#define CLOCK_SGI_CYCLE                 10      // Hardware specific
#define CLOCK_TAI                       11

#define MAX_CLOCKS                      16
#define CLOCKS_MASK                     (CLOCK_REALTIME | CLOCK_MONOTONIC)
#define CLOCKS_MONO                     CLOCK_MONOTONIC



  struct timeval {
	time_t      tv_sec;     // seconds
	suseconds_t tv_usec;    // microseconds
  };

  struct timespec {
	time_t   tv_sec;        // seconds
	long     tv_nsec;       // nanoseconds
  };

***/


struct timespec tp;

       #ifdef _POSIX_SOURCE
           printf("_POSIX_SOURCE defined\n");
       #endif

       #ifdef _POSIX_C_SOURCE
           printf("_POSIX_C_SOURCE defined: %ldL\n", (long) _POSIX_C_SOURCE);
       #endif

       #ifdef _ISOC99_SOURCE
           printf("_ISOC99_SOURCE defined\n");
       #endif

       #ifdef _ISOC11_SOURCE
           printf("_ISOC11_SOURCE defined\n");
       #endif

       #ifdef _XOPEN_SOURCE
           printf("_XOPEN_SOURCE defined: %d\n", _XOPEN_SOURCE);
       #endif


/* -----  Clock resolution:  0.1 second !!! ------ */
clock_getres(CLOCK_REALTIME, &tp);
printf("Clock resolution:  %ld.%ld second.\n", tp.tv_sec, tp.tv_nsec);
clock_getres(CLOCK_MONOTONIC, &tp);
printf("CLOCK_MONOTONIC resolution:  %ld.%ld second.\n", tp.tv_sec, tp.tv_nsec);
clock_getres(7, &tp);
printf("CLOCK boottime(7) resolution:  %ld.%ld second.\n", tp.tv_sec, tp.tv_nsec);


do {
	if( clock_gettime(CLOCK_REALTIME, &tp)!= 0 )
		perror("clock_gettime");
	else
		printf("Clock realtime:		%ld.%ld\n", tp.tv_sec, tp.tv_nsec);

	if( clock_gettime(5, &tp) == 0 )  /* CLOCK_REALTIME_COARSE */
		printf("Clock realtime coarse:	%ld.%ld\n", tp.tv_sec, tp.tv_nsec);

	if( clock_gettime(CLOCK_MONOTONIC, &tp)==0 )
		printf("Clock monotonic:	%ld.%ld\n", tp.tv_sec, tp.tv_nsec);

	if( clock_gettime(7, &tp) == 0 )  /* CLOCK_BOOTTIME */
		printf("Clock boottime:  	%ld.%ld\n", tp.tv_sec, tp.tv_nsec);

}while(0);

#endif


#if 1 /* TEST: EGI_CLOCK    ---------- */
int i,n,tm;
long cost;
long total;
int gap;
EGI_CLOCK eclock={0};
EGI_CLOCK eclock2={0};

//# define INT32_MIN              (-2147483647-1)
//# define INT64_MIN              (-9223372036854775807LL-1)
//# define INT32_MAX              (2147483647)
//# define INT64_MAX              (9223372036854775807LL)
long ltmax = INT32_MAX;
long long lltm;

lltm = ltmax +1;
printf("lltm=%lld\n", lltm);  /* lltm=-2147483648 !!! */
lltm = ltmax +1LL;
printf("lltm=%lld\n", lltm);  /* lltm=2147483648 !!! */

printf("tm_costus()=%lld\n", tm_costus());
printf("tm_costus2()=%lld\n", tm_costus2());



	tm=555555;
	egi_clock_start(&eclock);
	usleep(tm);
	egi_clock_stop(&eclock);
	printf(" eclock clock cost=%lld,  tm=%d, Err=%lld (us)  \n",egi_clock_readCostUsec(&eclock), tm, egi_clock_readCostUsec(&eclock)-tm );

total=0;
n=100;
gap=1000;
for(i=1; i<n+1; i++) {
	tm=i*gap;
	egi_clock_start(&eclock2);
	egi_clock_start(&eclock);
	usleep(tm);
	egi_clock_stop(&eclock);
	egi_clock_pause(&eclock2);

	cost=egi_clock_readCostUsec(&eclock);
	printf(" eclock clock cost=%ld,  tm=%d, Err=%ld (us)  \n",cost, tm, cost-tm );
	total += cost-tm;
	usleep(50000);
}
	printf(" eclock total cost Err=%ld (us) \n", total);

	/* TOTAL ERR */
	cost=egi_clock_readCostUsec(&eclock2);
	printf(" eclock2 total cost=%ld, tm total=%d, Err=%ld (us)  \n", cost, gap*(1+n)/2*n, cost-gap*(1+n)/2*n );
	printf("Consider system scheduled out time,  Err: %.2f%%\n", 100*1.0*(cost-gap*(1+n)/2*n)/(gap*(1+n)/2*n) );

exit(0);
#endif   /* --------- END TEST: EGI_CLOCK ---------- */




	return 0;
}



long long tm_costus(void)
{
	long ltmax = INT32_MAX;

	return ltmax*1LL+1;
}

long long tm_costus2(void)
{
	long long costus;

	long ltmax = INT32_MAX;

	costus=ltmax+1LL;
	return costus;
}
