/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

NOTE:
1.!!! Suppose that Left/Right bit_shifting are both arithmetic here !!!.

2. A float type represents accurately at least the first six decimal digits.
   and a range at lease 10^-37 to 10^37,   while a double has min. 15
   significant decimal digits and a range at lease 10^-307 - 10^308

                -----  Float Limits (see float.h)  -----

FLT_MANT_DIG:   Number of bits in the mantissa of a float:  24
FLT_DIG:        Min. number of significant decimal digits for a float:  6
FLT_MIN_10_EXP: Min. base-10 negative exponent for a float with a full set of significant figures: -37
FLT_MAX_10_EXP: Max. base-10 positive exponent for a float: 38
FLT_MIN:        Min. value for a positive float retaining full precision: 0.000000 =1.175494e-38
FLT_MAX:        Max. value for a positive float: 340282346638528880000000000000000000000.000000  =3.402823e+38
FLT_EPSILON:    Diff. between 1.00 and the leaset float value greater than 1.00: 0.000000=1.192093e-07
 1/(2^(FLT_MANT_DIG-1)) = 0.000000119209

                -----  Double Limits (see float.h)  -----

DBL_MANT_DIG:   Number of bits in the mantissa of a double: 53
DBL_DIG:        Min. number of significant decimal digits for a double: 15
DBL_MIN_10_EXP: Min. base-10 negative exponent for a double with a full set of significant figures: -307
DBL_MAX_10EXP:  Max. base-10 positive exponent for a double: 308
DBL_MIN:        Min. value for a positive double retaining full precision: 0.00000000000000000000 =2.225074e-308
DBL_MAX:        Max. value for a positive double: 17976931348623153000000.......00000000000.00000000000000000000  =1.797693e+308
DBL_EPSILON:    Diff. between 1.00 and the leaset double value greater than 1.00: 0.00000000000000022204=2.220446e-16
 1/(2^(DBL_MANT_DIG-1)) = 0.00000000000000022204

Journal:
2021-08-18:
	1. Add  mat_max() mat_maxf().

Midas-Zhou
--------------------------------------------------------------------*/
#include "egi.h"
#include "egi_fbgeom.h"
#include "egi_math.h"
#include "egi_debug.h"
#include "egi_timer.h"
#include <sys/mman.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <stdlib.h>

//#define MATH_PI 3.1415926535897932

int fp16_sin[360]={0}; /* fixed point sin() value table, divisor 2<<16 */
int fp16_cos[360]={0}; /* fixed point cos() value table, divisor 2<<16 */


/* ------------------------  PIXED_POINT FUNCTION  ------------------------

1. After calculatoin, a Fix point value must reset its exponent 'div' to the
   default value as '15'

2. Limitation analysis:
   1st grade precision: 0.00003

--------------------------------------------------------------------------*/

/*---------------------------
	Print function
---------------------------*/
void mat_FixPrint(EGI_FVAL a)
{
	printf("Float: %.8f   ", mat_floatFix(a) );
	printf("[num:%"PRId64", div:%d]",a.num, a.div);  /* OR lld for uint64_t */
}

/*-------------------------------------------------------
	Convert fixed point to float value
--------------------------------------------------------*/
inline float mat_floatFix(EGI_FVAL a)
{

//	return (float)1.0*a.num/(1u<<a.div);
	return 1.0*(a.num>>a.div);

}

/*-------------------------------------------------------
	Addup of two fixed point value: a+b
!!!TODO NOTE: Divisors of two EGI_FVAL must be the same!
--------------------------------------------------------*/
inline EGI_FVAL mat_FixAdd(EGI_FVAL a, EGI_FVAL b)
{
	return (EGI_FVAL){ a.num+b.num, a.div };
}

/*-------------------------------------------------------
	Subtraction of two fixed point value: a+b
!!!TODO NOTE: Divisors of two EGI_FVAL must be the same!
--------------------------------------------------------*/
inline EGI_FVAL mat_FixSub(EGI_FVAL a, EGI_FVAL b)
{
	return (EGI_FVAL){ a.num-b.num, a.div };
}

/*-------------------------------------------------------
	Mutliplication of two fixed point value: a+b
!!!TODO NOTE: Divisors of two EGI_FVAL must be the same!
--------------------------------------------------------*/
inline EGI_FVAL mat_FixMult(EGI_FVAL a, EGI_FVAL b)
{
	int64_t c;

	c=a.num*b.num;		/* !!!! Limit: (16num+16div)bit*(16num+16div)bit, for DIVEXP=15 */
	c>>=a.div;	        /* Right shifting is supposed to be arithmatic */
	return (EGI_FVAL){c, a.div};

#if 0
	if( (a.num > 0 && b.num<0) || (a.num<0 && b.num>0) ) {
		return (EGI_FVAL){ -( (int64_t)(-a.num*b.num)>>a.div ), a.div };
	}
	else if ( a.num<0 && b.num<0 ) {

		return (EGI_FVAL){ (int64_t)( (-a.num)*(-b.num) )>>a.div, a.div };
	}
	else {
		return (EGI_FVAL){ (int64_t)(a.num*b.num)>>a.div, a.div };
	}
#endif
}


/*-------------------------------------------------------
Mutliplication of a fixed point value and an integer.
!!!TODO NOTE: Divisors of two EGI_FVAL must be the same!
--------------------------------------------------------*/
inline int mat_FixIntMult(EGI_FVAL a, int b)
{
	int64_t c;

	c=a.num*b;
	c>>=a.div;	/* Right shifting is supposed to be arithmatic */

	return (int)c;
}


/*-------------------------------------------------------
	Division of two fixed point value: a/b
!!!TODO NOTE: Divisors of two EGI_FVALV must be the same!
--------------------------------------------------------*/
inline EGI_FVAL mat_FixDiv(EGI_FVAL a, EGI_FVAL b)
{

	/* Min value for 0 */
	if(b.num==0)
		b.num=1;

	return (EGI_FVAL){  (a.num<<a.div)/b.num, a.div }; /* suppose left shif is arithmatic! */

#if 0
	if(a.num<0) {
		return (EGI_FVAL){ -( ((-a.num)<<a.div)/b.num ), a.div };
	}
	else
		return (EGI_FVAL){ (a.num << a.div)/b.num, a.div }; /* div=MATH_DIVEXP ! */
#endif 
}


/*--------------------   PIXED_POINT COMPLEX FUNCTION  ----------------*/


/*---------------------------
	Print function
---------------------------*/
void mat_CompPrint(EGI_FCOMPLEX a)
{
	printf(" Float: %.8f %+.8fj   ", mat_floatFix(a.real), mat_floatFix(a.imag));
	printf(" Real[num:%"PRId64", div:%d],Imag[num:%"PRId64", div:%d] ",
			a.real.num, a.real.div, a.imag.num, a.imag.div);

}


/*-------------------------------------------------------
		Addup two complex: a+b
!!!TODO NOTE: Divisors of two EGI_FVAL must be the same!
--------------------------------------------------------*/
inline EGI_FCOMPLEX mat_CompAdd(EGI_FCOMPLEX a, EGI_FCOMPLEX b)
{
	return (EGI_FCOMPLEX){
			     {( a.real.num+b.real.num),  a.real.div },
			     {( a.imag.num+b.imag.num),  a.imag.div }
			   };
}

/*-------------------------------------------------------
	Subtraction of two complex: a-b
!!!TODO NOTE: Divisors of two EGI_FCOMPLEX must be the same!
--------------------------------------------------------*/
inline EGI_FCOMPLEX mat_CompSub(EGI_FCOMPLEX a, EGI_FCOMPLEX b)
{
	return (EGI_FCOMPLEX){
			     {a.real.num-b.real.num,  a.real.div },
			     {a.imag.num-b.imag.num,  a.imag.div }
			   };
}

/*-------------------------------------------------------
	Multiplication of two complex: a*b
!!!TODO NOTE: Divisors of two EGI_FCOMPLEX must be the same!
--------------------------------------------------------*/
inline EGI_FCOMPLEX mat_CompMult(EGI_FCOMPLEX a, EGI_FCOMPLEX b)
{
	EGI_FVAL	real;
	EGI_FVAL	imag;

	real=mat_FixSub(
		mat_FixMult(a.real, b.real),
		mat_FixMult(a.imag, b.imag)
	     );

	imag=mat_FixAdd(
		mat_FixMult(a.real, b.imag),
		mat_FixMult(a.imag, b.real)
	     );

	return (EGI_FCOMPLEX){real, imag};
}

/*----------------------------------------------------------
	Division of two complex: a/b
!!!TODO NOTE: Divisors of two EGI_FCOMPLEX must be the same!

  ar+jai/(br+jbi)=(ar+jai)(br-jbi)/(br^2+bi^2)
----------------------------------------------------------*/
inline EGI_FCOMPLEX mat_CompDiv(EGI_FCOMPLEX a, EGI_FCOMPLEX b)
{
	EGI_FCOMPLEX	d,ad;
	EGI_FVAL	den;

	/* d=br-j(bi) */
	d.real=b.real;
	d.imag.num=-b.imag.num;
	d.imag.div=b.imag.div;
//	printf("d=");
//	mat_CompPrint(d);
//	printf("\n");

	/* a*d */
	ad=mat_CompMult(a,d);
//	printf("ad=");
//	mat_CompPrint(ad);
//	printf("\n");

	/* rb*rb+ib*ib */
	den=mat_FixAdd( mat_FixMult(b.real, b.real),
			mat_FixMult(b.imag, b.imag) );

	/*  ad/den */
	ad.real=mat_FixDiv(ad.real, den); /* 0 will reset to 1.0/2^15 in FixDiv() */
	ad.imag=mat_FixDiv(ad.imag, den);

	return ad;
}


/*---------------------------------------------------------
work out the amplitude(modulus) of a complex, in float type.
Check also: int mat_intCompAmp( EGI_FCOMPLEX a )

Limit:  a.real MAX. 2^16   for DIVEXP=15
		    2^18   for DIVEXP=13
		    2^20   for DIVEXP=11
---------------------------------------------------------*/
float mat_floatCompAmp( EGI_FCOMPLEX a )
{
	EGI_FVAL c; /* ar^2+ai^2 */
	uint64_t uamp;

#if 0
	printf(">>>a: ");
	mat_CompPrint(a);
	printf("<<<");
#endif

	/* Limit here as in mat_FixMult(): (16+16)bit*(16+16)bit */
	c=mat_FixAdd(	mat_FixMult(a.real, a.real),
			mat_FixMult(a.imag, a.imag)
		    );


#if 0
	printf(">>>c:");
	mat_FixPrint(c);
	printf("<<<");
#endif

#if 0 /* 1. use float method sqrt */
	return sqrt(mat_floatFix(c));

#else  /* 2. use fixed point method sqrtu32(Max.1<<29):
           (a^2 -> exp 32), exp32+div15=47> 32, and >29(for sqrtu())
	   31+31-div15 = 47 > 29 !!!  47-29=18!!
   	   div=15=1+14, reduce 1 for sqrt, 14/2 for later division

	  TODO: 1. For big modulus value of complex c:
		   [ Contradiciton:
		     1.1 Too big shift value will decrease precision of small value input!!!!
		     1.2 Too small shift value will cause sqrtu32() fault!!!
		     example: Consider Max Amplitude value:
				Amp_Max: 1<<12;  FFT point number: 1<<5
				Limit: Amp_Max*(np/2) : 12+(5-1)=16; when MATH_DIVEXP=16-1=15
				(Amp_Max + MATH_DIVEXP = 31)
				 ( or Amp_Max: 1<<10; FFT point number: 1<<7 )
				  result |X|^2:	   16*2+div15= 47 > Max29 for sqrtu32()
						   18*2+div13= 49 > Max29
						   20*2+div11= 51 > Max29
				  taken min preshift:  47-29 = 18  +1. (__TRACE_BIG_DATA__)
						       49-29 = 20  +1.
						       51-29 = 22  +1
			          At this point, input Amp_Min = 2^((18-div15)-(5-1))=0.5 (0.5 OK)
				  input amp < 0.5 will be trimmed to be 0.!!!

			XXXXXXXXXXXXXXXXXXXXXXXXX WRONG XXXXXXXXXXXXXXXXXXXXXXXXX
			XXX    example 2: Amp_Max: 1<<12;  FFT point number: 1<<10
			XXX	  Amp_Max*(np/2) : 12+1+(10-1)=22 > 16 !!!! OVERFLOW !!!!
			XXX	  result |X|^2:	   22*2+div15=59 > Max 29 for sqrtu32()
			XXX	  taken min preshift:  57-29 = 30  +1.  (___TRACE_BIG_DATA___)
			XXX       At this point, input Amp_Min = 2^((30-div15)-(10-1))=64
			XXX	  input amp < 16 will be trimmed to be 0.!!!

		   ]
	  	2. For small modulus value :
		   the precision is extreamly lower if shift 16bits first
	           before sqrtu32, example: float resutl 0.500, but this fix type 0.375!!!
		So need to give a rather small shift bits number,instead of 16 in this case.
      */

//	return ( mat_fp16_sqrtu32( (c.num>>(18+1) ) ) >>16 )*(1<<(18/2-14/2)); /* MAX. amp=2^12 +2^10 */
//	uamp=mat_fp16_sqrtu32( c.num>>(18+1) )<<(18/2-14/2);
	uamp=mat_fp16_sqrtu32(c.num>>(47+(15-MATH_DIVEXP)-29 +1)) << ((47+(15-MATH_DIVEXP)-29)/2-MATH_DIVEXP/2);

//	uamp= ( mat_fp16_sqrtu32( (uint32_t)(c.num>>(28+1)) ) >>16 )*(1<<(18/2-14/2));  /* preshift 18 */

	/* Pre_shift 28  */
//		uamp= mat_fp16_sqrtu32( (c.num>>(28+1)) )*(1u<<(28/2-14/2)); uamp>>=16;
//		uamp= ( mat_fp16_sqrtu32( (c.num>>(28+1)) ) << (28/2-14/2) );

	/* if float */
	return 1.0*uamp/(1<<16);

#endif

}



/*--------------------------------------
	Return log2 of np
@np:	>0
--------------------------------------*/
unsigned int mat_uint32Log2(uint32_t np)
{
	int i;
	for(i=0; i<32; i++) {
        	if( (np<<i) & (1<<31) ){
			return 31-i;
        	}
	}

	return 0;
}


/*---------------------------------------------------------
work out the amplitude(modulus) of a complex, in INT type.
check also: float mat_floatCompAmp( EGI_FCOMPLEX a )
---------------------------------------------------------*/
unsigned int mat_uintCompAmp( EGI_FCOMPLEX a )
{
	EGI_FVAL c; /* ar^2+ai^2 */
	uint64_t uamp;

	/* Limit here as in mat_FixMult(): (16+16)bit*(16+16)bit */
	c=mat_FixAdd(	mat_FixMult(a.real, a.real),
			mat_FixMult(a.imag, a.imag)
		    );

#if 0 /* 1. use float method sqrt */
	return sqrt(mat_floatFix(c));

#else  /* 2. use fixed point method sqrtu32(Max.1<<29) */

	uamp=mat_fp16_sqrtu32( c.num>>(47+(15-MATH_DIVEXP)-29 +1))<<((47+(15-MATH_DIVEXP)-29)/2-MATH_DIVEXP/2);

	return uamp>>16;
#endif

}

/*---------------------------------------------------
Return square of the amplitude(modulus) of a complex
check also: float mat_floatCompAmp( EGI_FCOMPLEX a )

----------------------------------------------------*/
uint64_t mat_uintCompSAmp( EGI_FCOMPLEX a )
{
        EGI_FVAL c; /* ar^2+ai^2 */

        /* Limit here as in mat_FixMult(): (16+16)bit*(16+16)bit (for DIVEXP=15) */
        c=mat_FixAdd(   mat_FixMult(a.real, a.real),
                        mat_FixMult(a.imag, a.imag)
                    );

	return c.num>>MATH_DIVEXP;
}



/*--------- May use static array instead -------------
Generate complex phase angle array for FFT

@np	Phanse angle numbers as per FFT point numbers
	np will be normalized first to powers of 2

	!!! DO NOT FORGET TO FREE IT !!!

Return:
	a pointer to EGI_FCOMPLEX	OK

	NULL				fails
----------------------------------------------------*/
EGI_FCOMPLEX *mat_CompFFTAng(uint16_t np)
{
	int i;
	int exp=0;
	int nn;

	EGI_FCOMPLEX *wang;

	/* get exponent number of 2 */
        for(i=0; i<16; i++) {
                if( (np<<i) & (1<<15) ){
                        exp=16-i-1;
                        break;
                }
        }
//        if(exp==0) /* ok */
//              return NULL;

        /* reset nn to be powers of 2 */
        nn=1<<exp;

	/* calloc wang */
	wang=calloc(nn, sizeof(EGI_FCOMPLEX));
	if(wang==NULL) {
		printf("%s,Fail to calloc wang!\n",__func__);
		return NULL;
	}

	/* generate phase angle */
        for(i=0;i<nn;i++) {
                wang[i]=MAT_CPANGLE(i, nn);
                //mat_CompPrint(wang[i]);
                //printf("\n");
        }

	return wang;
}



/*-------------------------------------------------------------------------------------
Fixed point Fast Fourier Transform

     !!! --- WARNING: Static arrays applied, for one running instance only --- !!!
		( Use variable length arrays instead ??!?!?! )
Parameter:
@np:    Total number of data for FFT, will be ajusted to a number of 2 powers;
        np=1, result is 0!
	np=0, free all intermediate arrays!
	Note:
	1. All intermediate variable arrays(ffodd,ffeve,ffnin) are set as static, so if next
	input np is the same, they need NOT to allocate again!
	2. If exp. of input np is changed, then above arrays will be re_allocated.
	3. Only when input np=0, all intermediate variable arrays will be freed then.


@wang   complex phase angle factor, according to normalized np.
@x:     Pointer to array of input float data x[].
@nx:	Pointer to array of input INT data nx[].  NOTE: if x==NULL, use nx[] then.
@ffx:   Pointer to array of FFT result ouput in form of x+jy.

Note:
0. If input x[] is NULL, then use nx[].
1. Input number of points will be ajusted to a number of powers of 2.
2. Actual amplitude is FFT_A/(NN/2), where FFT_A is ffx[] result amplitude(modulus).
3. Use real and imaginery part of ffx[] to get phase angle(relative to cosine!!!):
   atan(ffx[].real/ffx[].imag),
4. !!!--- Cal overflow ---!!!
   Expected amplitude:       MSB. 1<<N 	( 1<<(N+1)-1 )  Max. Value: 2^(N+1)-1
   expected sampling points: 1<<M
   Taken: N+M-1=16  to incorporate with mat_floatCompAmp()
         OR N+M-1=16+(15-MATH_DIVEXP) --> N+M=17+(15-MATH_DIVEXP) [ MATH_DIVEXP MUST be odd!]

   Example:     when MATH_DIVEXP = 15   LIMIT: Amp_Max*(np/2) is 17bits;
                we may take Amp_Max: (1<<(12+1))-1;  FFT point number: 1<<5:
		  (NOTE: 12 is the highest significant bit for amplitude )

                when MATH_DIVEXP = 11   LIMIT: Amp_Max*(np/2) is 22bits;
                we may take Amp_Max: (1<<(11+1))-1;  FFT point number: 1<<10:

                Ok, with reduced precision as you decrease MATH_DIVEXP !!!!!

5. So,you have to balance between data precision, data scope and number of sample points(np)
   , which also decides Min. analysis frequency.


			  	-----	TBD & TODO ?   -----

	Frequent dynamic memory allocating will create huge numbers of memory
	fregments????(doublt!), which will slow down next memory allocating speed, and further
	to deteriorate system operation.
	It's better to apply static memory allocation method. (variable length arrays ?!?!)

	!!! --- WARNING: Static memory allocation, for one instance only --- !!!

Return:
        0       OK
        <0      Fail
---------------------------------------------------------------------------------------*/
int mat_egiFFFT( uint16_t np, const EGI_FCOMPLEX *wang,
					 const float *x, const int *nx, EGI_FCOMPLEX *ffx)
{
        int i,j,s;
        int kx,kp;
        int exp=0;                   	    /* 2^exp=nn */
        static int nn=0;                    /* number of data for FFT analysis, a number of 2 powers */
        static EGI_FCOMPLEX *ffodd=NULL;    /* for FFT STAGE 1,3,5.. , store intermediate result */
        static EGI_FCOMPLEX *ffeven=NULL;   /* for FFT STAGE 0,2,4.. , store intermediate result */
        static int *ffnin=NULL;             /* normal order mapped index */
	//int fftmp;

        /* check np  */
	if ( np==0 ) {
		/* free them all first */
		if(ffodd != NULL)   free(ffodd);
		if(ffeven != NULL)  free(ffeven);
		if(ffnin != NULL)   free(ffnin);
		return 0;
	}

        /* check other input data */
        if(  wang==NULL || ( x==NULL && nx==NULL ) || ffx==NULL) {
                printf("%s: Invalid input data!\n",__func__);
                return -1;
        }

        /* get exponent number of 2 */
        for(i=0; i<16; i++) {
                if( (np<<i) & (1<<15) ){
                        exp=16-i-1;
                        break;
                }
        }
//      if(exp==0) /* ok */
//              return -1;

        /*** Check if it needs to reallcate memory for arrays
	 * Only when exp. of np changed does it need to re_allocate memory for those variable arrays!
	 * If nn is keeps the same, so skip this stage to save time!
	 */
	if( nn != 1<<exp )
	{
		/* Reset nn to be powers of 2 */
	        nn=1<<exp;

		/* free them all first */
		if(ffodd != NULL)   free(ffodd);
		if(ffeven != NULL)  free(ffeven);
		if(ffnin != NULL)   free(ffnin);

	        /* allocate memory !!!!! This is time consuming !!!!!! */
	        ffodd=calloc(nn, sizeof(EGI_FCOMPLEX));
        	if(ffodd==NULL) {
                	printf("%s: Fail to alloc ffodd[] \n",__func__);
			nn=-1;  /* reset nn, so next call willtrigger re_allocation  */
	                return -1;
        	}
	        ffeven=calloc(nn, sizeof(EGI_FCOMPLEX));
        	if(ffeven==NULL) {
                	printf("%s: Fail to alloc ffeven[] \n",__func__);
	                free(ffodd);
			nn=-1;  /* reset nn, so next call willtrigger re_allocation  */
        	        return -1;
	        }
        	ffnin=calloc(nn, sizeof(int));
	        if(ffnin==NULL) {
        	        printf("%s: Fail to alloc ffnin[] \n",__func__);
                	free(ffodd); free(ffeven);
			nn=-1; /* reset nn, so next call willtrigger re_allocation  */
	                return -1;
        	}
	}

        /* ----------    FFT  Algrithm  ---------- */
        /* 1. map normal order index to  input x[] index */
        for(i=0; i<nn; i++)
        {
                ffnin[i]=0;
                for(j=0; j<exp; j++) {
                        /*  move i(j) to i(0), then left shift (exp-j) bits and assign to ffnin[i](exp-j) */
                        ffnin[i] += ((i>>j)&1) << (exp-j-1);
                }
                //printf("ffnin[%d]=%d\n", i, ffnin[i]);
        }

       /*  2. store x() to ffeven[], index as mapped according to ffnin[] */
	if(x)  {	/* use float type x[] */
	        for(i=0; i<nn; i++)
        	        ffeven[i]=MAT_FCPVAL(x[ffnin[i]], 0.0);
	} else {        /* use INT type nx[] */
	        for(i=0; i<nn; i++) {
        	        //ffeven[i]=MAT_FCPVAL_INT(nx[ffnin[i]], 0.0);
			ffeven[i].real.num=nx[ffnin[i]]<<MATH_DIVEXP; /* suppose left bit_shift is also arithmatic! */
			ffeven[i].real.div=MATH_DIVEXP;
			ffeven[i].imag.num=0;
			ffeven[i].imag.div=MATH_DIVEXP;
		}

	}

        /* 3. stage 2^1 -> 2^2 -> 2^3 -> 2^4....->NN point DFT */
        for(s=1; s<exp+1; s++) {
            for(i=0; i<nn; i++) {   /* i as normal order index */
                /* get coupling x order index: ffeven order index -> x order index */
                kx=((i+ (1<<(s-1))) & ((1<<s)-1)) + ((i>>s)<<s); /* (i+2^(s-1)) % (2^s) + (i/2^s)*2^s) */

                /* get complex phase angle index */
                kp= (i<<(exp-s)) & ((1<<exp)-1); // k=(i*1)%8

                if( s & 1 ) {   /* odd stage */
                        if(i < kx )
                                ffodd[i]=mat_CompAdd( ffeven[i], mat_CompMult(ffeven[kx], wang[kp]) );
                        else      /* i > kx */
                                ffodd[i]=mat_CompAdd( ffeven[kx], mat_CompMult(ffeven[i], wang[kp]) );
                }
                else {          /* even stage */
                        if(i < kx) {
                                ffeven[i]=mat_CompAdd( ffodd[i], mat_CompMult(ffodd[kx], wang[kp]) );
                                //printf(" stage 2: ffeven[%d]=ffodd[%d]+ffodd[%d]*wang[%d]\n", i,i,kx,kp);
                        }
                        else {  /* i > kx */
                                ffeven[i]=mat_CompAdd( ffodd[kx], mat_CompMult(ffodd[i], wang[kp]) );
                                //printf(" stage 2: ffeven[%d]=ffodd[%d]+ffodd[%d]*wang[%d]\n", i,kx,i,kp);
                        }
                } /* end of odd and even stage cal. */
            } /* end for(i) */
        } /* end for(s) */


        /* 4. pass result */
        if(exp&1) {     /* 4.1 result in odd array */
                for(i=0; i<nn; i++) {
                        ffx[i]=ffodd[i];
#if 0 /* test only */
                        printf("X(%d) ",i);
                        mat_CompPrint(ffodd[i]);
                        printf(" Amp=%f ",mat_floatCompAmp(ffodd[i])/(nn/2) );
                        printf("\n");
#endif
                }
        } else {      /* 4.2 result in even array */
                for(i=0; i<nn; i++) {
                        ffx[i]=ffeven[i];
#if 0 /* test only */
                        printf("X(%d) ",i);
                        mat_CompPrint(ffeven[i]);
                        printf(" Amp=%f ",mat_floatCompAmp(ffeven[i])/(nn/2) );
                        printf("\n");
#endif
                }
        }

	/*** 		---- DO NOT FREE HERE! -----
	 *    Call mat_egiFFFT() with input np=0 to free them at last!
	 */
	//free(ffodd);
        //free(ffeven);
	//free(ffnin);

	return 0;
}


/*--------------------------------------------------------------
Create fixed point lookup table for
trigonometric functions: sin(),cos()
angle in degree

fb16 precision is abt.   0.0001, that means 1 deg rotation of
a 10000_pixel long radius causes only 1_pixel distance error.??
---------------------------------------------------------------*/
void mat_create_fpTrigonTab(void)
{
	double pi=3.1415926535897932;  /* double: 16 digital precision excluding '.' */
	int i;

	for(i=0;i<360;i++)
	{
		fp16_sin[i]=round(sin(1.0*i*pi/180.0)*(1<<16));
		fp16_cos[i]=round(cos(1.0*i*pi/180.0)*(1<<16));
	}
}


/*--------------------------------------------------------------
Create fixed point lookup table for  atan()
angle in degree

fb16 precision is abt.   0.0001, that means 1 deg rotation of
a 10000_pixel long radius causes only 1_pixel distance error.??
---------------------------------------------------------------*/
void mat_create_fpAtanTab(void)
{

}

/*----------------------------------------------------------------------------------
Fix point square root for uint32_t, the result is <<16 shifted, so you need to shift
>>16 back after call to get the right answer.

Limit input x <= 2^30-1    //4294967296(2^32)   4.2949673*10^9


Original Source:
	http://read.pudn.com/downloads286/sourcecode/embedded/1291109/sqrt.c__.htm
------------------------------------------------------------------------------------*/
uint64_t mat_fp16_sqrtu32(uint32_t x)
{
	uint64_t x0=x;
	x0<<=32;
	uint64_t x1;
	uint32_t s=1;
	uint64_t g0,g1;

	if(x<=1)return x;

	x1=x0-1;
	if(x1>4294967296-1) {
		s+=16;
		x1>>=32;
	}
	if(x1>65535) {
		s+=8;
		x1>>=16;
	}
	if(x1>255) {
		s+=4;
		x1>>=8;
	}
	if(x1>15) {
		s+=2;
		x1>>=4;
	}
	if(x1>3) {
		s+=1;
	}

	g0=1<<s;
	g1=(g0+(x0>>s))>>1;

	while(g1<g0) {
		g0=g1;
		g1=(g0+x0/g0)>>1;
	}

	return g0; /* after call, right shift >>16 to get right answer */
}


/*---------------------------------------------------------------------
Get Min and Max value of a float array
data:	data array
num:	item size of the array
min,max:	Min and Max value of the array

---------------------------------------------------------------------*/
void mat_floatArray_limits(float *data, int num, float *min, float *max)
{

	int i=num;
	float fmin=data[0];
	float fmax=data[0];

	if(data==NULL || num<=0 )
		return;

	for(i=0;i<num;i++) {
		if(data[i]>fmax)
			fmax=data[i];
		else if(data[i]<fmax)
			fmin=data[i];
	}

	if(min != NULL)
		*min=fmin;
	if(max != NULL)
		*max=fmax;
}

/*---------------------------------------------------------------------------------------
Generate a rotation lookup map for a square image block

1. rotation center is the center of the square area.
2. The square side length must be in form of 2n+1, so the center is just at the middle of
   the symmetric axis.
3. Method 1 (BUG!!!!): from input coordinates (x,y) to get rotated coordinate(i,j) matrix,
   because of calculation precision limit, this is NOT a one to one mapping!!!  there are
   points that may map to the same point!!! and also there are points in result matrix that
   NOT have been mapped,they keep original coordinates.
4. Method 2: from rotated coordinate (i,j) to get input coordinates (x,y), this can ensure
   that the result points mastrix all be mapped.

n: 		pixel number for square side.
angle: 		rotation angle. clockwise is positive.
centxy 		the center point coordinate of the concerning square area of LCD(fb).
SQMat_XRYR: 	after rotation
	      square matrix after rotation mapping, coordinate origin is same as LCD(fb) origin.

-------------------------------------------------------------------------------------------*/
/*------------------------------- Method 1 --------------------------------------------*/

#if 0
void mat_pointrotate_SQMap(int n, double angle, EGI_POINT centxy,
                       					 EGI_POINT *SQMat_XRYR)
{
	int i,j;
	double sinang,cosang;
	double xr,yr;
	EGI_POINT  x0y0; /* the left top point of the square */
	double pi=3.1415926535897932;


	/* check if n can be resolved in form of 2*m+1 */
	if( n&0x1 == 0)
	{
		printf("mat_pointrotate_SQMap(): the number of pixels on the square side must be n=2*m+1.\n");
	 	return;
	}

/* 1. generate matrix of point_coordinates for the concerning square area, origin LCD(fb) */
	/* get left top coordinate */
	x0y0.x=centxy.x-((n-1)>>1);
	x0y0.y=centxy.y-((n-1)>>1);
	/* */
	for(i=0;i<n;i++) /* row index,Y */
	{
		for(j=0;j<n;j++) /* column index,X */
		{
			SQMat_XRYR[i*n+j].x=x0y0.x+j;
			SQMat_XRYR[i*n+j].y=x0y0.y+i;
			//printf("LCD origin: SQMat_XY[%d]=(%d,%d)\n",i*n+j,SQMat_XY[i*n+j].x, SQMat_XY[i*n+j].y);
		}
	}

/* 2. transform coordinate origin from LCD(fb) origin to the center of the square*/
	for(i=0;i<n*n;i++)
	{
		SQMat_XRYR[i].x -= x0y0.x+((n-1)>>1);
		SQMat_XRYR[i].y -= x0y0.y+((n-1)>>1);
	}

/* 3. map coordinates of all points to a new matrix by rotation transformation */
	sinang=sin(angle/180.0*pi);//MATH_PI);
	cosang=cos(angle/180.0*pi);//MATH_PI);
	//printf("sinang=%f,cosang=%f\n",sinang,cosang);

	for(i=0;i<n*n;i++)
	{
		/* point rotation transformation ..... */
		xr=(SQMat_XRYR[i].x)*cosang+(SQMat_XRYR[i].y)*sinang;
		yr=(SQMat_XRYR[i].x)*(-sinang)+(SQMat_XRYR[i].y)*cosang;
		//printf("i=%d,xr=%f,yr=%f,\n",i,xr,yr);

		/* check if new piont coordinate is within the square */
		if(  (xr >= -((n-1)/2)) && ( xr <= ((n-1)/2))
					 && (yr >= -((n-1)/2)) && (yr <= ((n-1)/2))  )
		{
			SQMat_XRYR[i].x=nearbyint(xr);//round(xr);
			SQMat_XRYR[i].y=nearbyint(yr);//round(yr);
		/*
			printf("JUST IN RANGE: xr=%f,yr=%f , origin point (%d,%d), new point (%d,%d)\n",
				xr,yr,SQMat_XY[i].x,SQMat_XY[i].y,SQMat_XRYR[i].x, SQMat_XRYR[i].y);
		*/
		}
		/*else:	just keep old coordinate */
		else
		{
			//printf("NOT IN RANGE: xr=%f,yr=%f , origin point (%d,%d)\n",xr,yr,SQMat_XY[i].x,SQMat_XY[i].y);
		}
	}


/* 4. transform coordinate origin back to X0Y0  */
	for(i=0;i<n*n;i++)
	{
                SQMat_XRYR[i].x += (n-1)>>1;
                SQMat_XRYR[i].y += (n-1)>>1;
		//printf("FINAL: SQMat_XRYR[%d]=(%d,%d)\n",i, SQMat_XRYR[i].x, SQMat_XRYR[i].y);
	}
	//printf("FINAL: SQMat_XRYR[%d]=(%d,%d)\n",i, SQMat_XRYR[i].x, SQMat_XRYR[i].y);
}
#endif

#if 1
/*----------------------- Method 2: revert rotation (float point)  -------------------------*/
void mat_pointrotate_SQMap(int n, double angle, EGI_POINT centxy, EGI_POINT *SQMat_XRYR)
{
	int i,j;
	double sinang,cosang;
	double xr,yr;

	sinang=sin(1.0*angle/180.0*MATH_PI);
	cosang=cos(1.0*angle/180.0*MATH_PI);

	/* clear the result matrix  */
	memset(SQMat_XRYR,0,n*n*sizeof(EGI_POINT));

	/* check if n can be resolved in form of 2*m+1 */
	if( (n&0x1) == 0 )
	{
		printf("!!! WARNING !!! mat_pointrotate_SQMap(): the number of pixels on	\
							the square side is NOT in form of n=2*m+1.\n");
	}

/* 1. generate matrix of point_coordinates for the square, result Matrix is centered at square center. */
	/* */
	for(i=-n/2;i<=n/2;i++) /* row index,Y */
	{
		for(j=-n/2;j<=n/2;j++) /* column index,X */
		{
			/*   get XRYR revert rotation matrix centered at SQMat_XRYU's center
			this way can ensure all SQMat_XRYR[] points are filled!!!  */

			xr = j*cosang+i*sinang;
			yr = -j*sinang+i*cosang;

                	/* check if new piont coordinate is within the square */
                	if(  ( xr >= -((n-1)>>1)) && ( xr <= ((n-1)>>1))
                                         && ( yr >= -((n-1)>>1)) && ( yr <= ((n-1)>>1))  )
			{
				SQMat_XRYR[((2*i+n)/2)*n+(2*j+n)/2].x= round(xr);
				SQMat_XRYR[((2*i+n)/2)*n+(2*j+n)/2].y= round(yr);
			}
		}
	}


/* 2. transform coordinate origin back to X0Y0  */
	for(i=0;i<n*n;i++)
	{
//		if( SQMat_XRYR[i].x == 0 )
//			printf(" ---- SQMat_XRYR[%d].x == %d\n",i,SQMat_XRYR[i].x );

                SQMat_XRYR[i].x += ((n-1)>>1); //+centxy.x);
                SQMat_XRYR[i].y += ((n-1)>>1); //+centxy.y);
		//printf("FINAL: SQMat_XRYR[%d]=(%d,%d)\n",i, SQMat_XRYR[i].x, SQMat_XRYR[i].y);
	}
	//printf("FINAL: SQMat_XRYR[%d]=(%d,%d)\n",i, SQMat_XRYR[i].x, SQMat_XRYR[i].y);



//	free(Mat_tmp);
}
#endif

/*----------------------- Method 3: revert rotation (fixed point)  -------------------------*/
void mat_pointrotate_fpSQMap(int n, int angle, EGI_POINT centxy, EGI_POINT *SQMat_XRYR)
{
	int i,j;
	int xr,yr;

	/* Normalize angle to be within [0-360] */
	int ang=angle%360;	/* !!! WARING !!!  The result is depended on the Compiler */
	int asign=ang >= 0 ? 1:-1; /* angle sign */
	ang= (ang>=0 ? ang: 360+ang) ;

	/* Clear the result matrix */
	memset(SQMat_XRYR,0,n*n*sizeof(EGI_POINT));

	/* Check if n can be resolved in form of 2*m+1 */
	if( (n&0x1) == 0 )
	{
		printf("!!! WARNING !!! mat_pointrotate_fpSQMap(): the number of pixels on	\
							the square side is NOT in form of n=2*m+1.\n");
	}

	/* Check whether lookup table fp16_cos[] and fp16_sin[] is generated */
	if( fp16_sin[30] == 0)
		mat_create_fpTrigonTab();

/* 1. generate matrix of point_coordinates for the square, result Matrix is centered at square center. */
	/* */
	for(i=-n/2;i<=n/2;i++) /* row index,Y */
	{
		for(j=-n/2;j<=n/2;j++) /* column index,X */
		{
			/*   get XRYR revert rotation matrix centered at SQMat_XRYU's center
			this way can ensure all SQMat_XRYR[] points are filled!!!  */
			xr = (j*fp16_cos[ang]+i*asign*fp16_sin[ang])>>16; /* !!! Arithmetic_Right_Shifting */
			yr = (-j*asign*fp16_sin[ang]+i*fp16_cos[ang])>>16;

                	/* check if new piont coordinate is within the square */
                	if(  ( xr >= -((n-1)>>1)) && ( xr <= ((n-1)>>1))
                                         && ( yr >= -((n-1)>>1)) && ( yr <= ((n-1)>>1))  )
			{
				SQMat_XRYR[((2*i+n)/2)*n+(2*j+n)/2].x= xr;
				SQMat_XRYR[((2*i+n)/2)*n+(2*j+n)/2].y= yr;
			}
		}
	}

/* 2. transform coordinate origin back to X0Y0  */
	for(i=0;i<n*n;i++)
	{
//		if( SQMat_XRYR[i].x == 0 )
//			printf(" ---- SQMat_XRYR[%d].x == %d\n",i,SQMat_XRYR[i].x );

                SQMat_XRYR[i].x += ((n-1)>>1); //+centxy.x);
                SQMat_XRYR[i].y += ((n-1)>>1); //+centxy.y);
		//printf("FINAL: SQMat_XRYR[%d]=(%d,%d)\n",i, SQMat_XRYR[i].x, SQMat_XRYR[i].y);
	}
	//printf("FINAL: SQMat_XRYR[%d]=(%d,%d)\n",i, SQMat_XRYR[i].x, SQMat_XRYR[i].y);

}


/*----------------------- Annulus Mapping: revert rotation (fixed point)  ------------------
Generate a rotation lookup map for an annulus shape image block.

1. rotation center is the center of the square area.
2. The square side length must be in form of 2n+1, so the center is just at the middle of
   the symmetric axis.
3. Method 2: from rotated coordinate (i,j) to get input coordinates (x,y), this can ensure
   that the result points mastrix all be mapped.

n:              pixel number for ouside square side. also is the outer diameter for the annulus.
ni:		pixel number for inner square side, alss is the inner diameter for the annulus.
angle:          rotation angle. clockwise is positive.
centxy          the center point coordinate of the concerning square area of LCD(fb).
ANMat_XRYR:     after rotation
                square matrix after rotation mapping, coordinate origin is same as LCD(fb) origin.

-------------------------------------------------------------------------------------------*/
void mat_pointrotate_fpAnnulusMap(int n, int ni, int angle, EGI_POINT centxy,
                       					 EGI_POINT *ANMat_XRYR)
{
	int i,j,m,k;
//	int sinang,cosang;
	int xr,yr;

	/* normalize angle to be within 0-359 */
	int ang=angle%360; /* !!! WARING !!!  The result is depended on the Compiler */
	int asign=ang >= 0 ? 1:-1; /* angle sign */
	ang=(ang>=0 ? ang:-ang) ;

	/* clear the result maxtrix */
	memset(ANMat_XRYR,0,n*n*sizeof(EGI_POINT));

	/* check if n can be resolved in form of 2*m+1 */
	if( (n-1)%2 != 0)
	{
		printf("!!! WARNING !!! mat_pointrotate_fpAnnulusMap(): the number of pixels on	\
							the square side is NOT in form of n=2*m+1.\n");
	}


	/* check whether fp16_cos[] and fp16_sin[] is generated */
	//printf("prepare fixed point sin/cos ...\n");
	if( fp16_sin[30] == 0)
		mat_create_fpTrigonTab();


/* 1. generate matrix of point_coordinates for the square, result Matrix is centered at square center. */
	/* */
	for(j=0; j<=n/2;j++) /* row index,Y: 0->n/2 */
	{
		m=sqrt( (n/2)*(n/2)-j*j ); /* distance from Y to the point on outer circle */

		if(j<ni/2)
			k=sqrt( (ni/2)*(ni/2)-j*j); /* distance from Y to the point on inner circle */
		else
			k=0;

		for(i=-m;i<=-k;i++) /* colum index, X: -m->-n, */
		{
			/*   get XRYR revert rotation matrix centered at ANMat_XRYU's center
			this way can ensure all ANMat_XRYR[] points are filled!!!  */
			/* upper and left part of the annuls j: 0->n/2*/
			xr = (j*fp16_cos[ang]+i*asign*fp16_sin[ang])>>16;  /* !!! Arithmetic_Right_Shifting */
			yr = (-j*asign*fp16_sin[ang]+i*fp16_cos[ang])>>16;
			ANMat_XRYR[(n/2-j)*n+(n/2+i)].x= xr;
			ANMat_XRYR[(n/2-j)*n+(n/2+i)].y= yr;
			/* lower and left part of the annulus -j: 0->-n/2*/
			xr = (-j*fp16_cos[ang]+i*asign*fp16_sin[ang])>>16;
			yr = (j*asign*fp16_sin[ang]+i*fp16_cos[ang])>>16;
			ANMat_XRYR[(n/2+j)*n+(n/2+i)].x= xr;
			ANMat_XRYR[(n/2+j)*n+(n/2+i)].y= yr;
			/* upper and right part of the annuls -i: m->n */
			xr = (j*fp16_cos[ang]-i*asign*fp16_sin[ang])>>16;
			yr = (-j*asign*fp16_sin[ang]-i*fp16_cos[ang])>>16;
			ANMat_XRYR[(n/2-j)*n+(n/2-i)].x= xr;
			ANMat_XRYR[(n/2-j)*n+(n/2-i)].y= yr;
			/* lower and right part of the annulus */
			xr = (-j*fp16_cos[ang]-i*asign*fp16_sin[ang])>>16;
			yr = (j*asign*fp16_sin[ang]-i*fp16_cos[ang])>>16;
			ANMat_XRYR[(n/2+j)*n+(n/2-i)].x= xr;
			ANMat_XRYR[(n/2+j)*n+(n/2-i)].y= yr;
		}
	}
        clear_screen(&gv_fb_dev,WEGI_COLOR_BLACK);
while(1) {

        for(i=0; i<24; i++) {

        fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
        fb_filo_on(&gv_fb_dev); /* start collecting old FB pixel data */

                fbset_color(egi_color_random(color_medium));
                draw_pcircle(&gv_fb_dev, 120, 120, 5*i, 5); //atoi(argv[1]), atoi(argv[2]));
                tm_delayms(55);

        fb_filo_off(&gv_fb_dev); /* start collecting old FB pixel data */

        }
}


/* 2. transform coordinate origin back to X0Y0  */
	for(i=0;i<n*n;i++)
	{
                ANMat_XRYR[i].x += ((n-1)>>1);
                ANMat_XRYR[i].y += ((n-1)>>1);
	}

}


/*-----------------------------------------------------------
A method to calculate pseudo curvature with 3 points.

With three points a,b,c defines two vectors, as VA=a->b, VB=a->c;
Define vector VC=VAxVB, which seems proportional to the real
circling curvature. ( No mathematic analysis yet )

1. Under right_hand normal coord system:
	when VC<0, a->b->c is clockwise.
	     VC>0, a->b->c is countercolocwise.
	     VC=0, straight line

2. Notice that we are under TOUCH PAD coord system:
	when VC<0, a->b->c is counterclockwise.
	     VC>0, a->b->c is colocwise.
	     VC=0, straight line

3. !!! The caller MUST ensure *pt holds 3 EGI_POINTs.

4. If system or touch sampling speed is rather slow, then too
   fast circling may cause sampling points NOT in right order
   and get ps.curv opposite in sign.

@pt	A pointer to an array with 3 EGI_POINTs.

Return:
	A negative value for clockwise ps.curv.
	A positive value for counterclockwise ps.curv.
------------------------------------------------------------*/
inline int mat_pseudo_curvature(const EGI_POINT *pt)
{
	int vc;

	vc=(pt[1].x-pt[0].x)*(pt[2].y-pt[0].y)-(pt[2].x-pt[0].x)*(pt[1].y-pt[0].y);

	#if 0 /* ------ For test only ------ */
	if(vc<0)
		printf("%s: vc=%d, CCW circling.\n",__func__,vc);   /* LCD Coord. */
	else if(vc>0)
		printf("%s: vc=%d, CW circling.\n", __func__,vc);
	else
		printf("%s: vc=0, Straight line.\n",__func__);
	#endif

	return vc;
}


/*---------------------------------------
Return a random integer within in range:
 [0, max)   (when max >0)
	OR
 (max, 0]   (when max <0)

Example:
egi_random_range(5):  0,1,2,3,4
egi_random_range(-5): -4,-3,-2,-1,0

max>1:  0<= ret <=max-1   [0 max-1]
max=0 or 1:  0
max<0:  max+1 <= ret <=0  [max+1 0]
---------------------------------------*/
int mat_random_range(int max)
{
	/* It seems Method 1 is better! */

    #if 1 /* Randomize seeds:  Method 1 */
        struct timeval tmval;
        gettimeofday(&tmval,NULL);
        srand(tmval.tv_usec);
    #else /* Randomize seed:  Method 2 */
	//usleep(10000); /* diff time seeds, necessary?? */
	srand((int)time(NULL));
    #endif

	return  (int)((float)max*rand()/(RAND_MAX+1.0));
}


/*---------------------------------------------------------------
Sort integers with Insertion_Sort algorithm.

With reference to:
	"Data structures and algorithm analysis in C"
		by Mark Allen Weiss

@array:		An integer array.
@n:		size of the array.
---------------------------------------------------------------*/
void mat_insert_sort( int *array, int n )
{
	int i;		/* To tranverse elements in array, 1 --> n-1 */
	int k;		/* To decrease, k --> 1,  */
	int tmp;

	for( i=1; i<n; i++) {
		tmp=array[i];	/* the inserting integer */

	/* Slide the inseting integer left, until to the first smaller integer */
		for( k=i; k>0 && array[k-1]>tmp; k--)
			array[k]=array[k-1];

		array[k]=tmp;
	}
}


/*---------------------------------------------------------------
Sort integers with Quick_Sort algorithm.

With reference to:
	"Data structures and algorithm analysis in C"
			by Mark Allen Weiss

@array:		An integer array.
@start:		Satrt index of array[]
@end:		End index of array[]

---------------------------------------------------------------*/
//#define QUICK_SORT_CUTOFF	(3)   /* At least 3, for mid=(start+end)/2 */
void mat_quick_sort( int *array, int start, int end, int cutoff )
{
	int i,j;
	int pivot;
	int mid;   /* As pivot index */
	int tmp;

	/* Start and End */
	if(start>=end)
		return;

	/* Limit cutoff */
	if(cutoff<3)
		cutoff=3;

/* 1. Implement quicksort */
	if( end-start >= cutoff ) //QUICK_SORT_CUTOFF )
	{
	/* 1.1 Select pivot, by sorting array[start], array[mid], array[end] */
		/* Get mid index */
		mid=(start+end)/2;
		//printf("Befor_3_sort: %d, %d, %d\n", array[start], array[mid], array[end]);

		/* Sort [start] and [mid] */
		if( array[start] > array[mid] ) {
			tmp=array[start];
			array[start]=array[mid];
			array[mid]=tmp;
		}
		/* Now: [start]<=array[mid] */

		/* IF: [mid] >= [start] > [end] */
		if( array[start] > array[end] ) {
			tmp=array[start]; /* [start] is center */
			array[start]=array[end];
			array[end]=array[mid];
			array[mid]=tmp;
		}
		/* ELSE:   [start]<=[mid] AND [start]<=[end] */
		else if( array[mid] > array[end] ) {  /* If: [start]<=[end]<[mid] */
		//	if( array[mid] > array[end] ) {
				tmp=array[end];   /* [end] is center */
				array[end]=array[mid];
				array[mid]=tmp;
		//	}
			/* Else: [start]<=[mid]<=[end] */
		}
		/* Now: array[start] <= array[mid] <= array[end] */

		pivot=array[mid];
		//printf("After_3_sort: %d, %d, %d\n", array[start], array[mid], array[end]);

		/* Swap array[mid] and array[end-1], still keep array[start] <= array[mid] <= array[end]!   */
		tmp=array[end-1];
		array[end-1]=array[mid];   /* !NOW, we set memeber [end-1] as pivot */
		array[mid]=tmp;

	/* 1.2 Quick sort: array[start] ... array[end-1], array[end]  */
		i=start;
		j=end-1;   /* As already sorted to: array[start]<=pivot<=array[end-1]<=array[end], see 1. above */
		for(;;)
		{
			/* Stop at array[i]>=pivot: We preset array[end-1]==pivot as sentinel, so i will stop at [end-1]  */
			while( array[++i] < pivot ){ };  /*  Acturally: array[++i] < array[end-1] which is the pivot memeber */

			/* Stop at array[j]<=pivot: We preset array[start]<=pivot as sentinel, so j will stop at [start]  */
			while( array[--j] > pivot ){ };

			if( i<j ) {
				/* Swap array[i] and array[j] */
				tmp=array[i];
				array[i]=array[j];
				array[j]=tmp;
			}
			else {
				break;
			}
		}
		/* Swap pivot memeber array[end-1] with array[i], we left this step at last.  */
		array[end-1]=array[i];
		array[i]=pivot; /* Same as array[i]=array[end-1] */

	/* 1.3 Quick sort: recursive call for sorted parts. and leave array[i] as mid of the two parts */
		mat_quick_sort(array, start, i-1, cutoff);
		mat_quick_sort(array, i+1, end, cutoff);

	}
/* 2. Implement insertsort */
	else
		mat_insert_sort( array+start, end-start+1 );

}

/*------------------------------------
Return MAX. of a and b.
------------------------------------*/
inline int mat_max(int a, int b)
{
	return (a>b?a:b);
}
inline float  mat_maxf(float a, float b)
{
	return (a>b?a:b);
}

/*---------------------------------------------------
Normal (Gaussian) probability distribution.

@x:	Random variable
@u:	mean of distribution. MUST >=0.
@dev:	standard deviation of distribution.

Return normal probability distribution.
----------------------------------------------------*/
float mat_normal_distribute(float x, float u, float dev)
{
	if(dev<0)dev=0;
	return 1.0/dev/sqrt(2.0*MATH_PI)*pow(MATH_E, -0.5*pow((x-u)/dev, 2.0));
}


/*-----------------------------------------------
Rayleigh probability distribution.

@x:	Random variable
@dev:	standard deviation of distribution.

Return Rayleigh probability distribution.
------------------------------------------------*/
float mat_rayleigh_distribute(float x, float dev)
{
	if(dev<0)dev=0;
	if(x<0)x=0;

	return x/pow(dev,2)*pow(MATH_E, -0.5*x*x/dev/dev);
}

/*--------------------------------------
Calculate factorial of n.
MAX. n=12!
----------------------------------------*/
inline unsigned int mat_factorial(int n)
{
	int i;
	unsigned int fn=1;

	if(n<1)
		return 1;

	for(i=n; i>1; i--)
		fn *= i;

	return fn;
}


/*------------------------------------------
Calculate factorial of n. return double type.
MAX.n=170

!168=2.5260757449731982e+302
!169=4.2690680090047050e+304
!170=7.2574156153080034e+306
!171=inf
!172=inf
!173=inf

------------------------------------------*/
inline double mat_double_factorial(int n)
{
        int i;
        double fn=1.0;

        if(n<1)
                return 1.0;

        for(i=n; i>1; i--)
                fn *= (double)i;

        return fn;
}

/*------------------------------------------
Return n-th Bernstein polynomial value.

@n:	Degree number
@i:	Polynomial index number
@u:	Independent variable [0 1]
-------------------------------------------*/
double mat_bernstein_polynomial(int n, int i, double u)
{
	int 	k,lead;
	double  ti,tni, tn;  /* i!, (n-i)!, n! */

	if( n<1 || i>n || i<0 )
		return 0.0;

	/* IF: i > n-i, n>=i */
	if( i > n-i ) {
		lead=n-i;
		tni=mat_double_factorial(lead);

		ti=tni;
		for(k=lead+1; k<=i; k++)
			ti *= k;

		tn=ti;
		for(k=i+1; k<=n; k++)
			tn *= k;

	}
	/* ELSE: i<=n-i, n>=n-1 */
	else {
		lead=i;
		ti=mat_double_factorial(lead);

		tni=ti;
		for(k=lead+1; k<=n-i; k++)
			tni *= k;

		tn=tni;
		for(k=n-i+1; k<=n; k++)
			tn *=k;
	}


	return tn/ti/tni*pow(u,i)*pow(1-u,n-i);
}


/*-------------------------------------------------------------
Return all n-th Bernstein polynomial value.

Refrence: <<The NURBS book>> by Les Piegl & Wayne Tiller

Recursive definition:
	B[i,n](u)=(1-u)*B[i,n-1](u) + u*B[i-1,n-1](u)
	B[i,n](u)==0 when i<0 OR i>n
	start with B[0,0]=B[0,1]=B[1,1]=1.0


@n:	Degree number
@u:	Independent variable [0 1]
@berns:  Out put array with Bernstein polynomial values
        If NULL, the function will calloc it first.

Return:
	A pointer to a double array with n elemetns, as of berns.
---------------------------------------------------------------*/
double *mat_bernstein_polynomials(int n, double u, double *berns)
{
	int i,j;
	double tmp1;    /* for (1-u)*B[i,n-1](u) */
	double tmp2;    /* for u*B[i-1,n-1](u) */

	if(n<1)
		return NULL;

        /* Calloc berns */
	if(berns==NULL) {
	        berns=calloc(n, sizeof(double));
        	if(berns==NULL) {
                	fprintf(stderr,"%s: Fail to calloc berns!\n",__func__);
			return NULL;
        	}
	}

	berns[0]=1.0;

	/* Calculate from B[:,1] --> B[:,2] --> B[:,3] --> ...  --> B[:,n] */
	for(i=1; i<n; i++) {
		tmp1=0.0;
		/* Cal form B[0,i]-->B[1,i]-->B[2,i]--> ... -->B[i,i] */
		for( j=0; j<i; j++) {
			tmp2=berns[j];
			berns[j]=tmp1 + (1.0-u)*tmp2;
			tmp1 = u*tmp2;  /* for next */
		}
		berns[i]=tmp1;
	}

	return berns;
}


/*-------------------------------------------------------------------
Calcualte nonzero B_spline basis functions for degree deg, and put
results in N[].
Refrence: <<The NURBS book>> by Les Piegl & Wayne Tiller

Recursive definition:
  N[i,p](u)= (u-u[i])/(u[i+p]-u[i])*N[i,p-1](u)
			+ (u[i+p+1]-u)/(u[i+p+1]-u[i+1])*N[i+1,p-1](u)

@i:	The i-th knot span
@deg:	Degree of the function
@u:	u for interpolation, u:[0 1]
@vu:	All knot vectors
@LN:	Array for basis function values, with [deg+1] elements!
	as of N[i-p,p] to N[i,p] for all N[]s.

TODO:  left[deg+1];

Return:
	0	OK
	<0	Fails
----------------------------------------------------------------------*/
int mat_bspline_basis(int i, int deg, float u, const float *vu, float *LN)
{
	int k,j;
	float *left=NULL;  /* as for u-u[i+1-j] */
	float *right=NULL;  /* as for u[i+j]-u */
	float tmp;
	float saved;

	/* Neglet input check */

	/* Calloc tmp1/tmp2 */
        left=calloc(2*(deg+1), sizeof(float));
        if(left==NULL) {
                fprintf(stderr,"%s: Fail to calloc left/right!\n",__func__);
                return -1;
        }
	right=left+(deg+1);

	LN[0]=1.0;
	for( j=1; j<=deg; j++) {
		left[j]=u-vu[i+1-j];
		right[j]=vu[i+j]-u;
		saved=0.0;
		for(k=0; k<j; k++) {
			tmp=LN[k]/(right[k+1]+left[j-k]);
			LN[k]=saved+right[k+1]*tmp;
			saved=left[j-k]*tmp;
		}
		LN[j]=saved;
	}

	/* Free */
	free(left); /* together with right */

	return 0;
}

/*-------------------------------------------------------------
A fast way to calculate inverse square root, this legendary
algorithm is from source code Quake III Arena. and later Chris
Lomont worked out a slightly better constant 0x5f375a86.
It trades some accuracy(~1%) for faster speed( 2~4 times).

Math. principle:
	y=f(x)=1/sqrt(x)=a0+a1*x+a2*x^2+a3*a^3+....
	y ~= a0+a1*x;  where: a0=0x5f375a86, a1=-0.5;
	Here same as: i=0x5f375a86 - (i>>1);
	Next, Newton's iteration to improve accuracy:
	x=x*(1.5-y*x*x)
-------------------------------------------------------------*/
float mat_FastInvSqrt( float x )
{
	int i;
	float y=0.5f*x;

	i=*(int *)&x;
	i=0x5f375a86 - (i>>1); /* 0x5f3759df, Evil floating point bit level hacking! */
	x=*(float *)&i;

	x=x*(1.5f-y*x*x);

	return x;
}


/*--------------------------------------------------------
To generate SHA-256 hash(digest) for the input message.

Principle refrence:
https://qvault.io/2020/07/08/how-sha-2-works-step-by-step-sha-256

Note:
1. Assume that Right_shift and Left_shift are both both arithmetic shifting!
2. All addition result in modulo of 2^32!
3. Macros are for 32bit variables, x in BIG_ENDIAN!

TODO:
1. Bitlen NOT of 8 multiples.

@input:		Pointer to input message.
@len:		Length of input message, in bytes.
@init_hv[8]:	Initial hash values.
		If NULL,
@init_kv[64]:	Initial round constants.
		If NUll, use builtin default.
@hv[8]:		To pass out final hash values. (256bits)
		The caller MUST enusre hv[8] space!!!
@digest[8*8+1]:	Output digest string. If NULL, ignore.
		convert u32 hv[0-7] to string by sprintf(%08x)
		The caller MUST ensure 8*8+1 bytes space!!!

Return:
	0	OK
	<0	Fails.
---------------------------------------------------------*/
int mat_sha256_digest(const uint8_t *input, uint32_t len,  uint32_t *init_hv, uint32_t *init_kv, uint32_t *hv, char *digest)
{
	int i;
	int nk;
	uint8_t chunk_mem[512/8]={0};
	uint8_t *chunk_data=NULL;    /* Just a pointer ref */
	//uint8_t chunk_data[512/8]={0}; /* 512bits/8 = 64bytes */
	unsigned int nch;	/* Total number of 512bits_chunks */
	unsigned int mod;	/* Result of mod calculation: msgbitlen%512 */

	uint32_t words[64]={0}; /* 32*64 = 2048 bits */
	uint64_t bitlen;	/* length in bits */
	//unsigned char digest[4*8*4+1]={0}; /* convert u32 hv[0-7] to string by sprintf(%04x) */

	/* hv_primes[8]
	 * They represent the first 32bits of the fractional parts of the square
         * roots of the first 8 primes: 2,3,5,7,11,13,17,19.
         */
	static uint32_t hv_primes[8]= {
		0x6a09e667,
		0xbb67ae85,
		0x3c6ef372,
		0xa54ff53a,
		0x510e527f,
		0x9b05688c,
		0x1f83d9ab,
		0x5be0cd19
	};

	/* SHA compression vars */
	uint32_t a,b,c,d,e,f,g,h;
	uint32_t s0, s1, ch, temp1, temp2, maj;

	/* kv_primes[64]
	 * Each is the first 32bits of the fractional parts of the cube roots of the first 64 primes(2-311)
	 */
	const uint32_t kv_primes[64]= {
	  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};

	/* Variable round constants kv[64] */
	uint32_t kv[64];

	/* Check input */
	if( input==NULL || hv==NULL || len==0 )
		return -1;

	/* 1. Pre-processing */
	bitlen=len*8;

	/* Calculate bitlen mod */
	mod=bitlen%512;
	/* mod > 448-1: fill the last chunk and need a new chunck */
	if(mod > 448-1) {
		nch=bitlen/512+2;
	}
	/* mod <= 448-1: fill the last chunk. */
	else
		nch=bitlen/512+1;

//	printf("%s: Input message has %llubits, with mod=%d %s 448-1!\n",
//					 __func__,bitlen, mod, mod>448-1?">":"<=");

	/* 2. Init hash values hv[8] */
	for(i=0; i<8; i++) {
		if(init_hv)
			hv[i]=init_hv[i];
		else
			hv[i]=hv_primes[i];
	}

	/* 3. Init round constants kv[64] */
	for(i=0; i<64; i++) {
		if(init_kv)
			kv[i]=init_kv[i];
		else
			kv[i]=kv_primes[i];
	}

/* ---- chunk loop ---- */
for(nk=0; nk<nch; nk++) {
	//printf("\n\t--- nk=%d ---\n", nk);

	/* 4. Load chunck_data[64] */
	if( mod <= 448-1 ) {
		/* For complete chunck blocks */
		if( nk < nch-1 ) {
			chunk_data=(uint8_t *)input+nk*(512>>3); /* No copy, just a ref */
			//memcpy((void *)chunk_data, input+(nk*(512>>3)), (512>>3));
		}
		/* The last block, Need to fill 1+0s for the last chunk */
		else {  /* nk == nch -1 */
			chunk_data=chunk_mem; /* Ref. chunk_data to chunk_mem */
			bzero(chunk_data, sizeof(chunk_mem));
			memcpy((void *)chunk_data, input+(nk*(512>>3)), mod>>3);
			/* Append a single 1, big_endian */
			chunk_data[mod>>3]=0x80;
			/* Append 64bits to the end of chunk, as BIT_LENGTH of original message, in big_endian! */
			for(i=0; i<sizeof(bitlen); i++)
				*(chunk_data+(512>>3)-1-i) = (bitlen>>(i*8))&0xff; /* Big_endian */
		}
	}
	else if( mod > 448-1 ) {
		/* For complete chunck blocks */
		if( nk < nch-2 ) {
			chunk_data=(uint8_t *)input+nk*(512>>3); /* No copy, just a ref */
			//memcpy((void *)chunk_data, input+(nk*(512>>3)), (512>>3));
		}
		/* The last block of original msg, appended with 1+0s to complete a 512bits chunck. */
		else if( nk == nch-2 ) {
			chunk_data=chunk_mem; /* Ref. chunk_data to chunk_mem */
			bzero(chunk_data, sizeof(chunk_mem));
			memcpy((void *)chunk_data, input+(nk*(512>>3)), mod>>3);
			/* Append a single 1, big_endian */
			chunk_data[mod>>3]=0x80;
		}
		/* Additional new chunk, as the last chunck. */
		else { /* nk == nch -1 */
			chunk_data=chunk_mem; /* Ref. chunk_data to chunk_mem */
			/* All 0s */
			bzero(chunk_data, sizeof(chunk_mem));
			/* Append 64bits to the end of chunk, as BIT_LENGTH of original message, in big_endian! */
			for(i=0; i<sizeof(bitlen); i++)
				*(chunk_data+(512>>3)-1-i) = (bitlen>>(i*8))&0xff; /* Big_endian */
		}
	}
	#if 0	/* TEST:-------- Print chunk_data */
	for(i=0; i<512/8; i++) {
		printf("%02x ", chunk_data[i]);
		if((i%8)==7) printf("\n");
	}
	printf("\n");
	#endif

	/* 5. Create message schedule: u32 words[64] */
	/* 5.1 words[0]~[15]:  u8 chunck_data[64] ---> u32 words[64] */
	for(i=0; i<16; i++) {
		/* Convert chunk_data[](type u8) TO words[] (BIG_ENDIAN! type unint32_t) */
		// OR words[i]=htonl(*(uint32_t *)(chunk_data+4*i));
		words[i]=(chunk_data[4*i]<<24) +(chunk_data[4*i+1]<<16)+(chunk_data[4*i+2]<<8)+chunk_data[4*i+3];
		//printf("words[%d]: %08x\n", i, words[i]);
	}
	//printf("RTROT(words[1],7): %08x\n", RTROT(sizeof(words[1]), words[1],7));

	/* 5.2 words[15]~[63]: 48 more words */
	for(i=16; i<64; i++) {
		/* s0 = (w[i-15] rightrotate 7) xor (w[i-15] rightrotate 18) xor (w[i-15] rightshift 3) */
		s0=MAT_RTROT(4,words[i-15],7) ^ MAT_RTROT(4,words[i-15],18) ^ MAT_RTSHIFT(words[i-15],3);
		/* s1 = (w[i- 2] rightrotate 17) xor (w[i- 2] rightrotate 19) xor (w[i- 2] rightshift 10) */
		s1=MAT_RTROT(4,words[i-2],17) ^ MAT_RTROT(4,words[i-2],19) ^ MAT_RTSHIFT(words[i-2],10);
		/* w[i] = w[i-16] + s0 + w[i-7] + s1 */
		words[i]=words[i-16]+s0+words[i-7]+s1;

		//printf("words[%d]: %08x\n", i, words[i]);
	}
	//printf("\n");

#if 0	/* TEST: -----Print u32 words[64] */
	printf("Message schedule u32 words[64]: \n");
	for(i=0; i<64; i++) {
		printf("%08x ",words[i]);
		if(i%2)printf("\n");
	}
	printf("\n");
#endif

	/* 6. SHA Compression, 64 rounds. */
	/* Update a,b,c,d,e,f,g,h */
	a=hv[0]; b=hv[1]; c=hv[2]; d=hv[3]; e=hv[4]; f=hv[5]; g=hv[6]; h=hv[7];
	/* Compress for 64 rounds */
	for(i=0; i<64; i++) {
		/* S1 = (e rightrotate 6) xor (e rightrotate 11) xor (e rightrotate 25) */
		s1=MAT_RTROT(4,e,6)^MAT_RTROT(4,e,11)^MAT_RTROT(4,e,25);
		//printf("s1: %08x\n", s1);

		/* ch = (e and f) xor ((not e) and g) */
		ch= (e&f)^((~e)&g);
		//printf("ch: %08x\n", ch);

		/* temp1 = h + S1 + ch + kv[i] + w[i] */
		temp1=h+s1+ch+kv[i]+words[i];
		//printf("temp1: %08x\n", temp1);

		/* S0 = (a rightrotate 2) xor (a rightrotate 13) xor (a rightrotate 22) */
		s0=MAT_RTROT(4,a,2)^MAT_RTROT(4,a,13)^MAT_RTROT(4,a,22);
		//printf("s0: %08x\n", s0);

		/* maj = (a and b) xor (a and c) xor (b and c) */
		maj=(a&b)^(a&c)^(b&c);
		//printf("maj: %08x\n", maj);

		/* temp2 = S0 + maj */
		temp2=s0+maj;
		//printf("temp2: %08x\n", temp2);

		h=g;
		g=f;
		f=e;
		e=d+temp1;
		d=c;
		c=b;
		b=a;
		a=temp1+temp2;
	}

#if 0	/* Print a~h */
	printf("a: %08x \n", a);
	printf("b: %08x \n", b);
	printf("c: %08x \n", c);
	printf("d: %08x \n", d);
	printf("e: %08x \n", e);
	printf("f: %08x \n", f);
	printf("g: %08x \n", g);
	printf("h: %08x \n", h);
#endif

	/* 7. Modify final values */
	hv[0] += a;
	hv[1] += b;
	hv[2] += c;
	hv[3] += d;
	hv[4] += e;
	hv[5] += f;
	hv[6] += g;
	hv[7] += h;

} /* ---- END: chunk loop ---- */

	/* 8. Generate final hash digest string */
	if( digest!=NULL ) {
		for(i=0; i<8; i++)
			sprintf(digest+8*i,"%08X",hv[i]); /*Convert to string */
		//printf("\nDigest: %s\n", digest);
	}

	return 0;
}

