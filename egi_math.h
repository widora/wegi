/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-------------------------------------------------------------------*/


#ifndef __EGI_MATH_H__
#define __EGI_MATH_H__

#include "egi.h"
#include "egi_fbgeom.h"
#include <inttypes.h>

#define MATH_PI 	3.1415926535897932
#define MATH_DIVEXP   	11  /* An odd number!!  exponent of 2, as for divisor of fixed point number */

extern int fp16_sin[360];
extern int fp16_cos[360];

/* EGI fixed point / complex number */
typedef struct {
int64_t         num;	/* divident */
int	        div;	/* divisor, 2 exponent taken 16*/
} EGI_FVAL;

typedef struct {
EGI_FVAL       real;	/* real part */
EGI_FVAL       imag;	/* imaginery part */
} EGI_FCOMPLEX;


/* TODO: adjustable exponent value  MAT_FVAL(a,exp) */
/* shitf MATH_DIVEXP bitS each time, if 15,  64-15-15-15-15-1 */

/* create a fix value */
/* (a) is float */
#define MAT_FVAL(a) ( (EGI_FVAL){ (a)*(1U<<MATH_DIVEXP), MATH_DIVEXP} ) /* 1U */

/* (a) is INT, suppose left bit_shift is also arithmatic !!! */
#define MAT_FVAL_INT(a) ( (EGI_FVAL){ a<<MATH_DIVEXP, MATH_DIVEXP} )

/* create a complex: r--real, i--imaginery */
/* (r) is float */
#define MAT_FCPVAL(r,i)	 ( (EGI_FCOMPLEX){ MAT_FVAL(r), MAT_FVAL(i) } )
/* (r) is INT, suppose left bit_shift is also arithmatic !!! */
#define MAT_FCPVAL_INT(r,i)  ( (EGI_FCOMPLEX){ MAT_FVAL_INT(r), MAT_FVAL(i) } )

/* complex phase angle factor
 @n:	m*(2*PI)/N  nth phase angle
 @N:	Total parts of a circle(2*PI).
*/
#define MAT_CPANGLE(n, N)  ( MAT_FCPVAL( cos(2.0*n*MATH_PI/N), -sin(2.0*n*MATH_PI/N) ) )

/* fixed point hamming window factor
@n:	nth hanmming window factor, also [0 N-1] index of input sample data
@N:	Total number of sample data for each FFT session.
*/
#define MAT_FHAMMING(n, N)  ( MAT_FVAL(0.54-0.46*cos(2.0*n*MATH_PI/N)) )

/* complex and FFT functions */
void 		mat_FixPrint(EGI_FVAL a);
void 		mat_CompPrint(EGI_FCOMPLEX a);
float 		mat_floatFix(EGI_FVAL a);
EGI_FVAL 	mat_FixAdd(EGI_FVAL a, EGI_FVAL b);
EGI_FVAL 	mat_FixSub(EGI_FVAL a, EGI_FVAL b);
EGI_FVAL 	mat_FixMult(EGI_FVAL a, EGI_FVAL b);
int 		mat_FixIntMult(EGI_FVAL a, int b);
EGI_FVAL 	mat_FixDiv(EGI_FVAL a, EGI_FVAL b);
EGI_FCOMPLEX 	mat_CompAdd(EGI_FCOMPLEX a, EGI_FCOMPLEX b);
EGI_FCOMPLEX 	mat_CompSub(EGI_FCOMPLEX a, EGI_FCOMPLEX b);
EGI_FCOMPLEX 	mat_CompMult(EGI_FCOMPLEX a, EGI_FCOMPLEX b);
EGI_FCOMPLEX 	mat_CompDiv(EGI_FCOMPLEX a, EGI_FCOMPLEX b);
float 		mat_floatCompAmp( EGI_FCOMPLEX a );
unsigned int 	mat_uint32Log2(uint32_t np);
unsigned int 	mat_uintCompAmp( EGI_FCOMPLEX a );
uint64_t 	mat_uintCompSAmp( EGI_FCOMPLEX a );
EGI_FCOMPLEX 	*mat_CompFFTAng(uint16_t np);
int 		mat_egiFFFT( uint16_t np, const EGI_FCOMPLEX *wang,
                                     const float *x, const int *nx, EGI_FCOMPLEX *ffx);

/* other math functions */
void 		mat_create_fpTrigonTab(void);
uint64_t 	mat_fp16_sqrtu32(uint32_t x);
void 		mat_floatArray_limits(float *data, int num, float *min, float *max);

/*
void mat_pointrotate_SQMap(int n, int angle, struct egi_point_coord centxy,
                                                        struct egi_point_coord *SQMat_XRYR);
*/

/* float point type */
void mat_pointrotate_SQMap(int n, double angle, struct egi_point_coord centxy,
                                                        struct egi_point_coord *SQMat_XRYR);
/* fixed point type */
void mat_pointrotate_fpSQMap(int n, int angle, struct egi_point_coord centxy,
                                                         struct egi_point_coord *SQMat_XRYR);

void mat_pointrotate_fpAnnulusMap(int n, int ni, int angle, struct egi_point_coord centxy,
                                                         struct egi_point_coord *ANMat_XRYR);

int  mat_pseudo_curvature(const EGI_POINT *pt);



#endif
