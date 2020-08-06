/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#ifndef   __EGI_MATRIX__H__
#define   __EGI_MATRIX__H__

#include <stdint.h>
#include <sys/time.h>

typedef struct float_Matrix EGI_MATRIX;

struct float_Matrix
{
  	int nr; 	/* Rows */
  	int nc; 	/* Columns */
  	float* pmat;
};


/* ------function declaration ------- */
uint32_t mat_get_costtimeus(struct timeval tm_start, struct timeval tm_end);
uint32_t mat_tmIntegral_NG(uint8_t num, const double *fx, double *sum, uint32_t *pdt_us);
uint32_t mat_tmIntegral(const double fx, double *sum, uint32_t *pdt_us);


/*<<<<<<<<<<<<<<<      MATRIX ---  OPERATION     >>>>>>>>>>>>>>>>>>
NOTE:
	1. All matrix data is stored from row to column.
	2. All indexes starts from 0 !!!
------------------------------------------------------------------*/
struct float_Matrix * init_float_Matrix(int nr, int nc);
void 	release_float_Matrix(struct float_Matrix ** pMat);

void   Matrix_Print(struct float_Matrix *matA);
struct float_Matrix*  Matrix_FillArray(struct float_Matrix * pMat, const float * array); //fill pMat->pmat with data from array
float  Matrix3X3_Determ(float *pmat); // determinnat of a 3X3 matrix
float  MatrixGT3X3_Determ(int nrc, float *pmat); // determinant of a matrix with dimension more than 3x3

struct float_Matrix* Matrix_CopyColumn(struct float_Matrix *matA, int nclmA, struct float_Matrix *matB, int nclmB);

struct float_Matrix* Matrix_Add( const struct float_Matrix *matA,
				 const struct float_Matrix *matB,
				 struct float_Matrix *matC );

struct float_Matrix* Matrix_Sub( const struct float_Matrix *matA,
				 const struct float_Matrix *matB,
				 struct float_Matrix *matC  );

struct float_Matrix* Matrix_Multiply( const struct float_Matrix *matA,
				      const struct float_Matrix *matB,
				      struct float_Matrix *matC );

struct float_Matrix* Matrix_MultFactor(struct float_Matrix *matA, float fc);
struct float_Matrix* Matrix_DivFactor(struct float_Matrix *matA, float fc);
struct float_Matrix* Matrix_Transpose(const struct float_Matrix *matA, struct float_Matrix *matB );
float* Matrix_Determ( const struct float_Matrix *matA,  float *determ );
struct float_Matrix* Matrix_Inverse( const struct float_Matrix *matA, struct float_Matrix *matAdj );
struct float_Matrix* Matrix_SolveEquations( const struct float_Matrix *matAB, struct float_Matrix *matX);

/* Special Matrix Equation Algrithom */
EGI_MATRIX* 	Matrix_GuassSolve( const struct float_Matrix *matAB );
EGI_MATRIX*	Matrix_ThomasSolve( const EGI_MATRIX *abcd, const EGI_MATRIX *matAD);

#endif
