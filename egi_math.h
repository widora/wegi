/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Journal:
2021-05-22:
	1. Add MAT_VECTOR2D macros for 2D Vector operation.

Midas Zhou
-------------------------------------------------------------------*/
#ifndef __EGI_MATH_H__
#define __EGI_MATH_H__

#include "egi.h"
#include "egi_fbgeom.h"
#include <inttypes.h>
#include <unistd.h> /* usleep */

#ifdef __cplusplus
 extern "C" {
#endif


#define MATH_E		2.7182818284590452
#define MATH_PI 	3.1415926535897932
#define MATH_DIVEXP   	11  /* An odd number!!  exponent of 2, as for divisor of fixed point number */

#define MATH_EPSILON	1.0*e-8;
/* Min. value for a positive float retaining full precision:FLT_MIN  32bits system = 1*2^(0-127+1) = 2^(-126)= 1.175494e-38 */

extern int fp16_sin[360];
extern int fp16_cos[360];

/* EGI fixed point / complex number */
typedef struct {
int64_t         num;	/* divident */
int	        div;	/* divisor, 2 exponent taken 16 */
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
 @n:	n*(2*PI)/N  nth phase angle
 @N:	Total parts of a circle(2*PI).
*/
#define MAT_CPANGLE(n, N)  ( MAT_FCPVAL( cos(2.0*n*MATH_PI/N), -sin(2.0*n*MATH_PI/N) ) )

/* fixed point hamming window factor
@n:	nth hanmming window factor, also [0 N-1] index of input sample data
@N:	Total number of sample data for each FFT session.
*/
#define MAT_FHAMMING(n, N)  ( MAT_FVAL(0.54-0.46*cos(2.0*n*MATH_PI/N)) )

/* Right_shift and Right_rotate operation
 @x:  	 input variable.
 @n:  	 Number of places to be moved.
 */
#define MAT_RTSHIFT(x,n)        ( (x)>>(n) )

/* Right_rotate operation
 @x:  	 input variable.
 @xsize: size of input variable
 @n:    Rotate position
 */
#define MAT_RTROT(xsize,x,n)    ( ((x)>>(n)) | ( ((x)&((1<<(n+1))-1)) << ( ((xsize)<<3)-(n) )) )


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
int 		mat_random_range(int max);
void 		mat_insert_sort( int *array, int n );
void 		mat_quick_sort( int *array, int start, int end, int cutoff );
int		mat_max(int a, int b);
float		mat_fmax(float a, float b);

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

int  	mat_pseudo_curvature(const EGI_POINT *pt);
float 	mat_normal_distribute(float x, float u, float dev);
float 	mat_rayleigh_distribute(float x, float dev);

unsigned int 	mat_factorial(int n);
double 	mat_double_factorial(int n);
double 	mat_bernstein_polynomial(int n, int i, double u);
double *mat_bernstein_polynomials(int n, double u, double *berns);
int 	mat_bspline_basis(int i, int deg, float u, const float *vu, float *LN);
float   mat_FastInvSqrt( float x );

int 	mat_sha256_digest(const uint8_t *input, uint32_t len, uint32_t *init_hv, uint32_t *init_kv, uint32_t *hv, char *digest);



/* ----- 2D Vector Operation ----- */
typedef struct {
	float x;
	float y;
} MAT_VECTOR2D;

/* Following: a,b are MAT_VECTOR2 */

#define VECTOR2D_ADD(a, b)	( (MAT_VECTOR2D){a.x+b.x, a.y+b.y} )	/* Add */
#define VECTOR2D_SUB(a, b)	( (MAT_VECTOR2D){a.x-b.x, a.y-b.y} )	/* Subtract */
#define VECTOR2D_MULT(a, k)	( (MAT_VECTOR2D){ k*a.x, k*a.y } )	/* Multiply */
#define VECTOR2D_DIV(a, k)	( (MAT_VECTOR2D){ a.x/k, a.y/k } )	/* Divide, K!=0 */

#define VECTOR2D_MOD(a)		( sqrt(a.x*a.x+a.y*a.y) )		/* Module a!=0 */
#define VECTOR2D_NORM(a)	( VECTOR2D_DIV(a, VECTOR2D_MOD(a) ) )	/* Normal, a!=0 */

#define VECTOR2D_DOTPD(a, b)	( a.x*b.x + a.y*b.y )			/* Dot product */
#define VECTOR2D_CROSSPD(a, b)	( a.x*b.y - b.x*a.y )			/* Cross product */


#ifdef __cplusplus
 }
#endif

#endif
