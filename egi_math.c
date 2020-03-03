/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

NOTE:
   !!! Suppose that Left/Right bit_shifting are both arithmetic here !!!.

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

#define MATH_PI 3.1415926535897932

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
	printf("[num:%"PRId64", div:%d]",a.num, a.div);
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

Limit:  a.real MAX. 2^16. (for DIVEXP=15)
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
	uamp=mat_fp16_sqrtu32( c.num>>(47+(15-MATH_DIVEXP)-29 +1))<<((47+(15-MATH_DIVEXP)-29)/2-MATH_DIVEXP/2);

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
	1. All intermediat variable arrays(ffodd,ffeve,ffnin) are set as static, so if next
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
   Expected amplitude:      MSB. 1<<N 	( 1<<(N+1)-1 )
   expected sampling point: 1<<M
   Taken: N+M-1=16  to incorporate with mat_floatCompAmp()
         OR N+M-1=16+(15-MATH_DIVEXP) --> N+M=17+(15-MATH_DIVEXP) [ MATH_DIVEXP MUST be odd!]

   Example:     when MATH_DIVEXP = 15   LIMIT: Amp_Max*(np/2) is 17bits;
                we may take Amp_Max: (1<<(12+1))-1;  FFT point number: 1<<5:
		  (NOTE: 12 is the highes significant bit for amplitude )

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
		fp16_sin[i]=sin(1.0*i*pi/180.0)*(1<<16);
		fp16_cos[i]=cos(1.0*i*pi/180.0)*(1<<16);
		//printf("fp16_sin[%d]=%d\n",i,fp16_sin[i]);
	}
	//printf(" ----- sin(1)=%f\n",sin(1.0*pi/180.0));
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

Limit input x <= 2^29    //4294967296(2^32)   4.2949673*10^9


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
void mat_pointrotate_SQMap(int n, double angle, struct egi_point_coord centxy,
                       					 struct egi_point_coord *SQMat_XRYR)
{
	int i,j;
	double sinang,cosang;
	double xr,yr;
	struct egi_point_coord  x0y0; /* the left top point of the square */
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
void mat_pointrotate_SQMap(int n, double angle, struct egi_point_coord centxy,
                       					 struct egi_point_coord *SQMat_XRYR)
{
	int i,j;
	double sinang,cosang;
	double xr,yr;

	sinang=sin(1.0*angle/180.0*MATH_PI);
	cosang=cos(1.0*angle/180.0*MATH_PI);

	/* clear the result matrix  */
	memset(SQMat_XRYR,0,n*n*sizeof(struct egi_point_coord));

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
generate a rotation lookup map for a annulus shape image block

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
void mat_pointrotate_fpAnnulusMap(int n, int ni, int angle, struct egi_point_coord centxy,
                       					 struct egi_point_coord *ANMat_XRYR)
{
	int i,j,m,k;
//	int sinang,cosang;
	int xr,yr;

	/* normalize angle to be within 0-359 */
	int ang=angle%360; /* !!! WARING !!!  The result is depended on the Compiler */
	int asign=ang >= 0 ? 1:-1; /* angle sign */
	ang=(ang>=0 ? ang:-ang) ;

	/* clear the result maxtrix */
	memset(ANMat_XRYR,0,n*n*sizeof(struct egi_point_coord));

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

2. Notice that we are under LCD coord system:
	when VC<0, a->b->c is counterclockwise.
	     VC>0, a->b->c is colocwise.
	     VC=0, straight line

3. !!! The caller MUST ensure *pt holds 3 EGI_POINTs.


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
