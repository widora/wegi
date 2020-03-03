/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to test  EGI Fixed Point Fast Fourier Transform function.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "egi_math.h"
#include <math.h>
#include <inttypes.h>
#include "egi_timer.h"

#define FLOAT_INPUT	0	/* 1 -- input/out in float, or 0 -- in INT */

int main(void)
{
	int i,k;



#if 0  /////////////  TEST SQRTU32()  ///////////
int m;
int sfa=-1024;

printf(" (-1024)<<10 =%d, (-1024)>>9 = %d \n",sfa<<10, sfa>>9);
for(m=0;m<32; m++)
{
  printf("sqrtu32( 1<<%d )=%"PRId64"\n", m, (mat_fp16_sqrtu32(1<<m)>>16));
}
#endif   ///////////////////////////////////////

	struct timeval tm_start,tm_end;
	/* nexp+aexp MAX 21 */
	int nexp=11;  /* exponent of 2,  for Element number */
	int aexp=10;  /* exponent of 2,  MSB for Max. amplitude, Max amp=2^12-1 */
	uint16_t np=1<<nexp;
	float famp;
	unsigned int   namp;
	unsigned int   nsamp;

	float *x;	    /* input float */
	int   *nx;	    /* input INT */
	EGI_FCOMPLEX *ffx;  /* result */
	EGI_FCOMPLEX *wang; /* unit phase angle factor */

	/* allocate vars */
	x=calloc(np, sizeof(float));
	if(x==NULL)
        	return -1;
	nx=calloc(np, sizeof(int));
	if(nx==NULL)
        	return -1;
	ffx=calloc(np, sizeof(EGI_FCOMPLEX));
	if(ffx==NULL)
		return -1;

	/* generate NN points samples */

	/* signal Freq = 1000Hz, 2000Hz, 3000Hz */
#if 0
	for(i=0; i<np; i++) {
		x[i]=10.5*sin(2.0*MATH_PI*1000*i/8000) +0.5*sin(2.0*MATH_PI*2000*i/8000+3.0*MATH_PI/4.0) \
		     +1.0*((1<<aexp)+(1<<9))*sin(2.0*MATH_PI*3000*i/8000+MATH_PI/4.0);  /* Amplitud Max. abt.(1<<12)+(1<<10) */
		printf(" x[%d]: %f\n", i, x[i]);
	}
#endif

#if FLOAT_INPUT  /* float type x[] */
	for(i=0; i<np; i++) {
		x[i]=10.5*cos(2.0*MATH_PI*1000*i/8000) +0.5*cos(2.0*MATH_PI*2000*i/8000+3.0*MATH_PI/4.0) \
		     +1.0*((1<<aexp))*cos(2.0*MATH_PI*3000*i/8000+MATH_PI/4.0);  /* Amplitud Max. abt.(1<<12)+(1<<10) */
		printf(" x[%d]: %f\n", i, x[i]);
	}
#else		/* INT type nx[] */
	for(i=0; i<np; i++) {
		nx[i]= (int)( 333*cos(2.0*MATH_PI*1000*i/16000) )
		      +(int)( 444*cos(2.0*MATH_PI*2000*i/16000+3.0*MATH_PI/4.0) )
		      +(int)( 555*cos(2.0*MATH_PI*3000*i/16000-1.0*MATH_PI/4.0) )
		      +(int)( ((1<<(aexp+1))-1)*cos(2.0*MATH_PI*5000*i/16000+MATH_PI/4.0) );
		printf(" nx[%d]: %d\n", i, nx[i]);
	}
#endif

	/* prepare phase angle */
	wang=mat_CompFFTAng(np);


k=0;
do {    ///// ---------------------- LOOP TEST FFT ---------------------- //////
k++;

	gettimeofday(&tm_start, NULL);

	/* call fix_FFT */
	#if FLOAT_INPUT /* float x[] */
	mat_egiFFFT(np, wang, x, NULL, ffx);
	#else /* int nx[] */
	mat_egiFFFT(np, wang, NULL, nx, ffx);
	#endif

	gettimeofday(&tm_end, NULL);

	/* print result */
	#if 1
	if( (k&(64-1))==0 )
	  for(i=0; i<np; i++) {
		#if FLOAT_INPUT  /* get float modulus */
		 famp=mat_floatCompAmp(ffx[i])/(np/2);   /* np MUST be powers of 2 */
		 if( famp > 0.001 || famp < -0.001 ) {
			printf("ffx[%d] ",i);
			mat_CompPrint(ffx[i]);
		 	printf(" Amp=%f ", famp);
			printf("\n");
		}
	        #else		/* get int moduls */
		 namp=mat_uintCompAmp(ffx[i])/(np/2);   /* np MUST be powers of 2 */
		 if( namp > 0 ) {
			printf("ffx[%d] ",i);
			mat_CompPrint(ffx[i]);
		 	printf(" Amp=%d ", namp);
			if(nexp>1)
				nsamp=mat_uintCompSAmp(ffx[i])>>(2*(nexp-1)); // /(np/2)/(np/2) );
			else
				nsamp=mat_uintCompSAmp(ffx[i]);
			printf(" SAmp=%d\n", nsamp);
			printf("\n");
		}
	        #endif
  	}
	#endif


	if( (k&(64-1))==0 )
	   printf(" --- K=%d  time cost: %d us\n", k, tm_diffus(tm_start,tm_end));

} while(1);  ///// ------------------  END LOOP TEST  ----------------- //////


	/* free resources */
	free(wang);
	free(x);
	free(nx);
	free(ffx);

return 0;
}


