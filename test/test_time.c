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


/*----------------------------
	    Main()
----------------------------*/
int main(int argc, char **argv)
{

#if 1 /* TEST: EGI_CLOCK    ---------- */
int i,n,tm;
long cost;
long total;
int gap;
EGI_CLOCK eclock={0};
EGI_CLOCK eclock2={0};

	tm=555555;
	egi_clock_start(&eclock);
	usleep(tm);
	egi_clock_stop(&eclock);
	printf(" eclock clock cost=%ld,  tm=%d, Err=%ld (us)  \n",egi_clock_readCostUsec(&eclock), tm, egi_clock_readCostUsec(&eclock)-tm );

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
