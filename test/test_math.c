/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test egi_math functions

Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "egi_math.h"
#include <math.h>
#include <inttypes.h>

int main(void)
{

#if 1 /* ----- test  pseduo curvatur ----- */
EGI_POINT pt[3]={ {0,3}, {2,2}, {3,0} };
printf("pt[1].x=%d, pt[1].y=%d \n", pt[1].x, pt[1].y);
mat_pseudo_curvature(pt);
return 0;
#endif

/* test shifting */
uint64_t aul=68719476736L;
printf("aul>>28:%"PRIu64"\n", aul>>28);
getchar();


/*--------------- test fix point ----------------*/
//float a=-3.12322;
float a=-1.0*(1u<<15); //21);
//float b=-34.39001;
float b=-1.0*(1u<<15); //21);

EGI_FVAL fva=MAT_FVAL(a);
EGI_FVAL fvb=MAT_FVAL(b);
EGI_FVAL fvc,fvs,fvm,fvd;

printf("\n---------------- Test Fix Point Functions ------------\n");
printf("fva=");
mat_FixPrint(fva);
printf("\n");
printf("Float compare: 	a=%15.9f, fva=%15.9f\n", a, mat_floatFix(fva));

printf("fvb=");
mat_FixPrint(fvb);
printf("\n");
printf("Float compare: 	b=%15.9f, fvb=%15.9f\n", b, mat_floatFix(fvb));

fvc=mat_FixAdd(fva, fvb);	/* ADD */
printf("fvc=fva+fvc=");
mat_FixPrint(fvc);
printf("\n");
printf("Float compare: 	c=a+b=%15.9f, fvc=%15.9f\n", a+b, mat_floatFix(fvc));

fvs=mat_FixSub(fva, fvb);	/* Subtraction */
printf("fvs=fva-fvc=");
mat_FixPrint(fvs);
printf("\n");
printf("Float compare: 	s=a-b=%15.9f, fvs=%15.9f\n", a-b, mat_floatFix(fvs));

fvm=mat_FixMult(fva,fvb);	/* Multiply */
printf("Float compare: 	a*b=%15.9f, fvm=fva*fvb=%15.9f\n", a*b, mat_floatFix(fvm));

fvd=mat_FixDiv(fvb,fva);	/* Divide */
printf("Float compare: 	b/a=%15.9f, fvb/fva=%15.9f\n", b/a, mat_floatFix(fvd));


/*--------------- test fix complex ----------------*/
//float ar=0.707, aimg=-0.707;
float ar=0.0, aimg=1.0*(1u<<21);
//float br=1.414, bimg=-1.414;
float br=0.0, bimg=1.0*(1u<<21);

EGI_FCOMPLEX  ca=MAT_FCPVAL(ar, aimg);

EGI_FCOMPLEX cpa=MAT_FCPVAL(ar, aimg);
EGI_FCOMPLEX cpb=MAT_FCPVAL(br, bimg);
EGI_FCOMPLEX cpc,cps,cpm,cpd;
EGI_FVAL caa;

printf( "\n -------------- Test Fix Point Complex Functions ----------\n");


EGI_FVAL fc={(int64_t)1u<<(21+15), 15};
//printf(" fc.num=%"PRId64"\n",fc.num);
printf(" fc = %f\n", 1.0*((int64_t)1L<<(21+15))/(1L<<15) ); //mat_floatFix(fc) );
printf(" mat_floatFix(fc) =%f\n", mat_floatFix(fc) );

EGI_FVAL sqreal,sqimag;
sqreal=mat_FixMult(ca.real, ca.real);


sqimag=mat_FixMult(ca.imag, ca.imag);
printf(" sqimg: ");
mat_FixPrint(sqimag);
printf("\n");

printf(" ca: ");
mat_CompPrint(ca);
printf("\n");
caa=mat_FixAdd(sqreal, sqimag);
//caa=mat_FixAdd(   mat_FixMult(ca.real, ca.real),
//                  mat_FixMult(ca.imag, ca.imag)
//                    );
printf("caa.real=%"PRIu64" caa.imag=%d \n", caa.num, caa.div);
mat_FixPrint(caa);

printf("Float compare: ca=%f%+fj	cpa=%f%+fj\n", ar,aimg, mat_floatFix(cpa.real), mat_floatFix(cpa.imag));
printf("Float compare: cb=%f%+fj	cpb=%f%+fj\n", br,bimg, mat_floatFix(cpb.real), mat_floatFix(cpb.imag));

cpc=mat_CompAdd(cpa, cpb);	/* ADD */
printf("Float compare: cc=ca+cb=%f%+fj      cpc=cpa+cpb=%f%+fj\n",
				ar+br,aimg+bimg, mat_floatFix(cpc.real), mat_floatFix(cpc.imag));

cps=mat_CompSub(cpa, cpb);	/* Subtraction */
printf("Float compare: cs=ca-cb=%f%+fj      cps=cpa-cpb=%f%+fj\n",
				ar-br,aimg-bimg, mat_floatFix(cps.real), mat_floatFix(cps.imag));

cpm=mat_CompMult(cpa, cpb);	/* Multiply */
printf("Float compare: cm=ca*cb=%f%+fj      cpm=cpa*cpb=%f%+fj\n",
                             ar*br-aimg*bimg,ar*bimg+br*aimg, mat_floatFix(cpm.real), mat_floatFix(cpm.imag));

cpd=mat_CompDiv(cpb, cpa);     /* Multiply */
printf("Float compare: cd=cb/ca=%f%+fj      cpd=cpb/cpa=%f%+fj\n",
//                             (br*ar+aimg*bimg)/(br*br+bimg*bimg),(-ar*bimg+br*aimg)/(br*br+bimg*bimg),
                             (br*ar+aimg*bimg)/(ar*ar+aimg*aimg),(ar*bimg-br*aimg)/(ar*ar+aimg*aimg),
			     mat_floatFix(cpd.real), mat_floatFix(cpd.imag));
printf("\n\n");

getchar();

/*----------------------- test fix point FFT --------------------------
8_points FFT test example is from:
	<< Understanding Digital Signal Processing, Second Edition >>
			    by Richard G.Lyons
	p118-120


@np:	Total number of data for FFT, will be ajusted to a number of 2
	powers. Max=1<<15;
@x:	Pointer to array of input data x[]
@XX:	Pointer to array of FFT result ouput.

Note:
1. Input number of points will be ajusted to a number of 2 powers.
2. Actual amplitude is FFT_A/(NN/2), where FFT_A is FFT result amplitud.

Return:
	0	OK
	<0	Fail
----------------------------------------------------------------------*/
int i,j,s,k;
int kx,kp;
int exp=0;	/* 2^exp=np */
uint16_t np=1024;//1025; /* points for FFT analysis, it will be ajusted to the number of 2 powers */
int nn;

/* get exponent number of 2 */
for(i=0; i<16; i++) {
	if( (np<<i) & (1<<15) ){
		exp=16-i-1;
		break;
	}
}
if(exp==0)
	return -1;

nn=1<<exp;
printf("taken exp=%d, NN=%d\n", exp,nn);

getchar();

#define	NN 1024//8

EGI_FCOMPLEX  wang[NN];
EGI_FCOMPLEX ffodd[NN];		/* FFT STAGE 1, 3 */
EGI_FCOMPLEX ffeven[NN];	/* FFT STAGE 2 */
int ffnin[NN];			/* normal roder mapped index */
float famp;

/* generate complex phase angle factor */
for(i=0;i<NN;i++) {
	wang[i]=MAT_CPANGLE(i, NN);
	//mat_CompPrint(wang[i]);
	//printf("\n");
}

/* sample data */
float x[NN];//={ 0.3535, 0.3535, 0.6464, 1.0607, 0.3535, -1.0607, -1.3535, -0.3535 };
/* generate NN points samples */
//for(i=0; i<NN; i++) {
//	x[i]=sin(2.0*MATH_PI*1000*i/8000)+0.5*sin(2.0*MATH_PI*2000*i/8000+3.0*MATH_PI/4.0);
//	printf(" x[%d]: %f\n", i, x[i]);
//}
/*
 1/64=0.015625  1/32=0.03125 1/16=0.0625 1/8=0.125 1/4=0.25 1/2=0.5
*/
for(i=0; i<nn; i++) {
        x[i]=0.5*sin(2.0*MATH_PI*1000*i/8000) +64*sin(2.0*MATH_PI*2000*i/8000+3.0*MATH_PI/4.0) \
             +((1<<12))*sin(2.0*MATH_PI*3*i/8);  /* Amplitud Max. abt.(1<<12)+(1<<10)  */
        printf(" x[%d]: %f\n", i, x[i]);
}


k=0;
do {  //////////////////////////   LOOP TEST /////////////////////////////////
k++;

/* 1. map normal order index to  input x[] index */
for(i=0; i<NN; i++)
{
	ffnin[i]=0;
	for(j=0; j<exp; j++) {
		/*  move i(j) to i(0), then left shift (exp-j) bits and assign to ffnin[i](exp-j) */
		ffnin[i] += ((i>>j)&1) << (exp-j-1);
	}
	//printf("ffnin[%d]=%d\n", i, ffnin[i]);
}

/*  2. store x() to ffeven[], index as mapped according to ffnin[] */
for(i=0; i<NN; i++)
	ffeven[i]=MAT_FCPVAL(x[ffnin[i]],0.0);

/* 3. stage 2^1 -> 2^2 -> 2^3 -> 2^4....->NN point DFT */
if(exp > 0) {
	for(s=1; s<exp+1; s++) {
	    for(i=0; i<NN; i++) {   /* i as normal order index */
        	/* get coupling x order index: ffeven order index -> x order index */
	        kx=((i+ (1<<(s-1))) & ((1<<s)-1)) + ((i>>s)<<s); /* (i+2^(s-1)) % (2^s) + (i/2^s)*2^s) */

	        /* get 8th complex phase angle index */
	        kp= (i<<(exp-s)) & ((1<<exp)-1); // k=(i*1)%8

		if( s & 1 ) { 	/* odd stage */
			if(i < kx )
				ffodd[i]=mat_CompAdd( ffeven[i], mat_CompMult(ffeven[kx], wang[kp]) );
			else	  /* i > kx */
				ffodd[i]=mat_CompAdd( ffeven[kx], mat_CompMult(ffeven[i], wang[kp]) );
		}
		else {		/* even stage */
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
} /* end if exp>1 */


#if 0 //////////////////// case study ////////////////////////////////
/*** For 8 points FFT
   Order index: 	0,1,2,3, ...
   input x[] index:     0,4,2,6, ...
*/

/* stage 1: Basic 2-point DFT */
for(i=0; i<NN; i++) {  /* i as normal order index */
	if( (i&(2-1)) == 0 ) { /* i%2 */
		ffodd[i]=MAT_FCPVAL( x[ffnin[i]]+x[ffnin[i+1]], 0.0 );
		printf(" stage 1: ffodd[%d]=x(%d)+x(%d)\n", i,ffnin[i],ffnin[i+1]);
	}
	else {
		ffodd[i]=MAT_FCPVAL( x[ffnin[i-1]]-x[ffnin[i]], 0.0);
		printf(" stage 1: ffodd[%d]=x(%d)-x(%d)\n", i,ffnin[i-1],ffnin[i]);
	}

}
/* print result */
for(i=0; i<8; i++) {
        printf("ffodd(%d) ",i);
        mat_CompPrint(ffodd[i]);
        printf("\n");
}

/* stage 2 */
for(i=0; i<NN; i++) { 	/* i as normal order index */
	/* get coupling x order index: ffeven order index -> x order index */
	kx= ( (i+ (1<<(2-1)) )&((1<<2)-1) )+((i>>2)<<2); /* (i+2)%4+(i/4*4 */

	/* get 8th complex phase angle index */
	kp=(i<<(exp-2))&(8-1);  // kp=(i*2)%8;

	/* cal. ffeven */
	if(i < kx) {
		ffeven[i]=mat_CompAdd( ffodd[i], mat_CompMult(ffodd[kx], wang[kp]) );
		printf(" stage 2: ffeven[%d]=ffodd[%d]+ffodd[%d]*wang[%d]\n", i,i,kx,kp);
	}
	else {  /* i > kx */
		ffeven[i]=mat_CompAdd( ffodd[kx], mat_CompMult(ffodd[i], wang[kp]) );
		printf(" stage 2: ffeven[%d]=ffodd[%d]+ffodd[%d]*wang[%d]\n", i,kx,i,kp);
	}
}
/* print result */
for(i=0; i<NN; i++) {
       	printf("ffeven(%d) ",i);
        mat_CompPrint(ffeven[i]);
       	printf("\n");
}

/* stage 3 */
for(i=0; i<NN; i++) {   /* i as normal order index */
        /* get coupling x order index: ffeven order index -> x order index */
        kx=( (i+ (1<<(3-1)) )&((1<<3)-1) )+((i>>3)<<3); /* (i+4)%8+(i/8*8) */

        /* get 8th complex phase angle index */
        kp= (i<<(exp-3)) & (8-1); // k=(i*1)%8

	/* cal. ffodd */
	if(i < kx ) {
		ffodd[i]=mat_CompAdd( ffeven[i], mat_CompMult(ffeven[kx], wang[kp]) );
	}
	else {  /* i > kx */
		ffodd[i]=mat_CompAdd( ffeven[kx], mat_CompMult(ffeven[i], wang[kp]) );
	}
}
#endif //////////////////////////////////////////////////////////////////////////////


/* print result */
#if 1
for(i=0; i<NN; i++) {
	if(exp&1) {
		famp=mat_floatCompAmp(ffodd[i])/(np/2);
		if(i==384 || famp > 0.001 || famp < -0.001 || ffodd[i].real.num>1 || ffodd[i].imag.num>1)
		{
			printf("---X(%d)---\n ",i);
			mat_CompPrint(ffodd[i]);
			printf(" Amp=%f ", famp );
			printf("\n\n\n");
		}
	}
	else {
		famp=mat_floatCompAmp(ffeven[i])/(np/2);
		if(i==384 || famp > 0.001 || famp < -0.001|| ffodd[i].real.num>1 || ffodd[i].imag.num>1)
		{
			printf("---X(%d)---\n ",i);
			mat_CompPrint(ffeven[i]);
			printf(" Amp=%f ", famp );
			printf("\n\n\n");
		}
	}
}
#endif

printf("--------- K=%d -------- \n",k);
} while(1);   //////////////////////////   END LOOP TEST /////////////////////////////////



return 0;

}
