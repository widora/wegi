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

#define FLOAT_INPUT	1	/* 1 -- input/out in float, or 0 -- in INT */

int main(void)
{
	int i,k;

#if 0  /////////////  TEST SQRTU32()  ///////////
int m;
int sfa=-1024;
unsigned int num;

printf(" (-1024)<<10 =%d, (-1024)>>9 = %d \n",sfa<<10, sfa>>9);

num= (1<<30)-1;
printf("sqrtu32( %d )=%lld\n", num, mat_fp16_sqrtu32( num )>>16 );


for(m=0;m<32; m++)
{
  printf("sqrtu32( 1<<%d )=%"PRId64"\n", m, (mat_fp16_sqrtu32(1<<m)>>16));
  //printf("sqrtu32( 1<<%d )=%lld\n", m, (mat_fp16_sqrtu32( (1<<m) + (m>1?1<<(m-1):0) )>>16));
}

exit(0);
#endif   ///////////////////////////////////////


	struct timeval tm_start,tm_end;
	/* nexp+aexp MAX 21 */
	int nexp=11;  /* exponent of 2,  for Element number */
	int aexp=10;  /* exponent of 2,  MSB for Max. amplitude.  Max amp=2^(10+1)-1 */
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
	/*---------------------------------
	(Note: Result in difference if Amp with different frequence value, considering sampling frequence! )
    	  Input Amp        Result Amp (typical)
		0.1		0.125
		0.2		0.197642
		0.3		0.582961 3kHz D0.28
		0.4		0.405046
		0.5		0.496078
		0.6		0.602728
		0.7		0.698771
		0.8		1.038327  3kHz D0.23
		0.9		1.133647  3kHz D0.23
		1.0		1.001951
		1.1		1.098649
		1.2		1.425219  3kHz D0.22
		1.3		1.302041
		1.4		1.398939
		1.5		1.718466  3kHz D0.21
		1.6		1.600780
		1.7		1.699034
		1.8		2.014595  3kHz D0.21
		1.9		1.900863
		2.0		1.999022
		3.0		3.204611  3kHz D0.20
		4.0		4.000000
		5.0		4.999609
		6.0		6.196647  3kHz D0.20
		7.0		6.999721
		8.0		7.999511
		9.0		9.193663
		10.0		9.999804
		20.0		19.998829
		30.0		30.186981  3kHz  D0.19
		100.0		99.996719
		200.0		199.989288
		300.0		300.149536  3kHz D0.15
		...
		512		7500HZ 	512	D0
		512		8KHz   	724	D212
		1024          	8KHz   	1448.154	D424
		...
		2^11-1		4kHz 	2046.999   D0.0
		2^11-1		5kHz 	2046.701   D0.3
		2^11-1		7500Hz 	2046.330   D0.7
		2^11-1          7600Hz  (Spectrum expended!)
		2^11-1          ...     (Expended!)
		2^11-1          7900Hz  (Expended!)
		2^11-1          8KHz    (NO Results)
		..................
		deviation [0	0.28]
         ---------------------------------*/
	for(i=0; i<np; i++) {
		/*
		x[i]=10.5*cos(2.0*MATH_PI*1000*i/8000) +0.5*cos(2.0*MATH_PI*2000*i/8000+3.0*MATH_PI/4.0) \
		     +1.0*((1<<(aexp+1))-1)*cos(2.0*MATH_PI*3000*i/8000+MATH_PI/4.0);
		*/
		x[i]= 100.0*cos(2.0*MATH_PI*1000*i/16000)
		      +200.0*cos(2.0*MATH_PI*2000*i/16000+3.0*MATH_PI/4.0)
		      +300.0*cos(2.0*MATH_PI*3000*i/16000-1.0*MATH_PI/4.0)
		      -1.0*((1<<(aexp+1))-1)*cos(2.0*MATH_PI*5000*i/16000+MATH_PI/4.0);
		 //     +1.0*(512)*cos(2.0*MATH_PI*7500*i/16000+MATH_PI/4.0);
		printf(" x[%d]: %f\n", i, x[i]);
	}
#else		/* INT type nx[] */
	/*---------------------------------
	(Note: Result in difference if Amp with different frequence value )
	Input Amp        Result Amp (typical)
		1	        0
		2               1
		3	        2
		4	        3
		5	        4
		6		5
		7		6
		8		7
		9		8
		10		10
		11		10
		12		11
		13		12
		14		13
		15		14
		16		15
		17		16
		18		17
		...
		31		30
		32		31
		33		32
		34		33
		35		34
		...
		2^11-1		4kHz 2046
		2^11-1		5kHz 2046
		2^11-1		7kHz 2046
		2^11-1          7500Hz 2045
		2^11-1          7600Hz  (Spectrum expended!)
		2^11-1          ...     (Expended!)
		2^11-1          7900Hz  (Expended!)
		2^11-1          8KHz    (NO Results)
		..................
		deviation [0 ~ 1]
         ---------------------------------*/
	for(i=0; i<np; i++) {
		nx[i]= (int)( 33*cos(2.0*MATH_PI*1000*i/16000) )
		      +(int)( 34*cos(2.0*MATH_PI*2000*i/16000+3.0*MATH_PI/4.0) )
		      +(int)( 35*cos(2.0*MATH_PI*3000*i/16000-1.0*MATH_PI/4.0) )
		      +(int)( -1.0*((1<<(aexp+1))-1)*cos(2.0*MATH_PI*7600*i/16000+MATH_PI/4.0) );
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
		 if( famp > 0.001 || famp < -0.001 ) {   /* Ignore too small amp values. */
			printf("ffx[%d] ",i);
			mat_CompPrint(ffx[i]);
		 	printf(" Amp=%f ", famp);
			printf("\n");
		 }
	        #else		/* get int moduls */
		 namp=mat_uintCompAmp(ffx[i])/(np/2);   /* np MUST be powers of 2 */
		 if( namp > 0 ) {			/* Ignore zero amp */
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
	   printf(" --- K=%d  time cost: %lu us\n", k, tm_diffus(tm_start,tm_end));

} while(1);  ///// ------------------  END LOOP TEST  ----------------- //////


	/* free resources */
	free(wang);
	free(x);
	free(nx);
	free(ffx);

return 0;
}


