/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "egi_matrix.h"
#include "egi_timer.h"

int main(void)
{
 int i,j;
 struct timeval tm_start;
 struct timeval tm_end;

 float matA[3*4]=  // row 3, column 4
 {
   1,2,3,8,
   4,5,6,10,
   3,0,0.3,4
 };
 struct float_Matrix mat_A;
 mat_A.nr =3; mat_A.nc=4; mat_A.pmat=matA;

#if 0 /* -------TEST: Matrix_GuassSolve() */
	   float mat8X9[8*9]={
		1548196.19929056,       317736.825975421,       65209.2355136882,       13382.9133063996,       2746.5797928119,
		563.681493376731,       115.684542210269,       23.7419774529573,	100,
		537.339513653319,       267.231594241498,       132.900564999072,       66.0945807220341,       32.8703914904499,
		16.3472197710038,        8.12985736172746,      4.04316952043861,	100,
		0.0226191669422462,     0.0344597830259496,     0.0524986905674963,     0.0799805532503283,     0.121848541917522,
		0.185633464186686,      0.282808333063805,      0.430852021216898,	100,
		107730.122892644,       29729.5669002975,       8204.2712321051,        2264.07827183226,       624.802651687529,
		172.422640335594,       47.5823315089988,       13.1309801742137,	100,
		753042.807566855,       167432.476943158,       37227.1457261511,       8277.13000618498,       1840.34740786374,
		409.185137734936,       90.9787338128256,       20.2283251342007,	100,
		40359797792706,      281614466038.468,      0.329949967550431,      0.386581636298681,      0.452933402699983,
		0.530673596515285,      0.621756894854155,      0.728473470014719,	100,
		9.87536751506927e-007,  4.59013148592374e-006,  2.13352131208463e-005,  9.91673812194352e-005,  0.000460936079814079,
		0.00214245921452982,    0.00995828204156898,    0.0462867066718926,	100,
	#if 0 /* Test repeated data  */
		9.87536751506927e-007,  4.59013148592374e-006,  2.13352131208463e-005,  9.91673812194352e-005,  0.000460936079814079,
		0.00214245921452982,    0.00995828204156898,    0.0462867066718926,	100,
	#else
		2600.45128660534,       1085.42261166079,       453.052995829233,       189.103317753609,       78.9313063032946,
		32.9457525587259,	13.7514842018456,       5.7398390708038,	100,
	#endif
	   };

	 struct float_Matrix mat_8X9;
	 mat_8X9.nr=8; mat_8X9.nc=9; mat_8X9.pmat=mat8X9;

	 EGI_MATRIX *matRX=NULL;
	 //while(1)
	 {
		Matrix_Print(&mat_8X9);
		printf("Start GuassSolve ...\n");
		gettimeofday(&tm_start,NULL);
	 	matRX=Matrix_GuassSolve( &mat_8X9 );
		gettimeofday(&tm_end,NULL);
		printf("cost time=%ldms, Resolved matX:\n",tm_diffus(tm_start, tm_end)/1000);
		Matrix_Print(matRX);
		release_float_Matrix(matRX);
		usleep(500000);
	 }
	 exit(0);
#endif 	/* END: Matrix_GuassSolve() */


#if 1 	/* -----TEST: Matrix_ThomasSolve() */
 float matAD[4*5]=  // row 4, column 5
 {
    2,-1, 0, 0,   6,
   -1, 3,-2, 0,   1,
    0,-1, 2,-1,   0,
    0, 0,-3, 5,	  1,
 };
 struct float_Matrix mat_AD;
 mat_AD.nr =4; mat_AD.nc=5; mat_AD.pmat=matAD;

 float matABCD[4*4]=  // row 4, column 4
 {
    2, 3, 2, 5,
    0, -1, -1, -3,
   -1,-2, -1, 0,
    6, 1, 0, 1,
 };
 struct float_Matrix mat_ABCD;
 mat_ABCD.nr =4; mat_ABCD.nc=4; mat_ABCD.pmat=matABCD;

 EGI_MATRIX *matTX1=Matrix_ThomasSolve( NULL, &mat_AD);
 Matrix_Print(matTX1);

 EGI_MATRIX *matTX2=Matrix_ThomasSolve( &mat_ABCD, NULL);
 Matrix_Print(matTX2);

 release_float_Matrix(&matTX1);
 release_float_Matrix(&matTX2);

 exit(0);

#endif 	/* END: Matrix_ThomasSolve() */



 float matB[3*4]=  // row 3, column 4
 {
   43,4,6,0.5,
   6,5,4,5,
   3,2,1,5
 };
 struct float_Matrix mat_B;
 mat_B.nr =3; mat_B.nc=4; mat_B.pmat=matB;

 float matX[4*4]=
{
   6,3,0.6,0.3,
   5,2,0.5,0.2,
   4,1,0.4,0.1,
   4,1,0.4,0.1
};
 struct float_Matrix mat_X;
 mat_X.nr =4; mat_X.nc=4; mat_X.pmat=matX;


float matClm[3*1]={0};


 float matM[2*2]=
{
   14.23,-2.5,
   2.10,80.445877
};
struct float_Matrix mat_M;
mat_M.nr=2; mat_M.nc=2; mat_M.pmat=matM;

 float matN[2*2]=
{
  3,23.2,
  3.5,13.6
};
struct float_Matrix mat_N;
mat_N.nr=2; mat_N.nc=2; mat_N.pmat=matN;

 float invmatN[2*2]={0};
 struct float_Matrix invmat_N; invmat_N.nr=2; invmat_N.nc=2; invmat_N.pmat=invmatN;


 float determ;

  float matC[3*4]={0};
  struct float_Matrix mat_C; mat_C.nr=3; mat_C.nc=4; mat_C.pmat=matC;

  float matD[2*4]={0};

  float matY[3*4]={0};
  struct float_Matrix mat_Y; mat_Y.nr=3; mat_Y.nc=4; mat_Y.pmat=matY;

  //----- Matrix ADD_SUB Operation ----
  printf("mat_A = \n");
  Matrix_Print(&mat_A);
  printf("mat_B = \n");
  Matrix_Print(&mat_B);
  Matrix_Add(&mat_A,&mat_B,&mat_C);
  printf("mat_C = \n");
  Matrix_Print(&mat_C);
  //----- copy a column from a matrix ---
  Matrix_CopyColumn(&mat_A,2,&mat_A,1);
  printf("Matrix_CopyColumn(mat_A,2,mat_A,1), mat_A=\n");
  Matrix_Print(&mat_A);
  Matrix_Add(&mat_A, &mat_A,&mat_A);
  printf("mat_A = mat_A + mat_A =\n");
  Matrix_Print(&mat_A);
  Matrix_Sub(&mat_A,&mat_B,&mat_A);
  printf("matA = matA-matB =\n");
  Matrix_Print(&mat_A);

  //----- Matrix Multiply Operation ----
   printf("\nmatB=\n");
   Matrix_Print(&mat_B);
   printf("\nmatX=\n");
   Matrix_Print(&mat_X);
   Matrix_Multiply(&mat_B, &mat_X, &mat_Y);
   printf("\n matY = matB*matX = \n");
   Matrix_Print(&mat_Y);

   //----- Matrix MultFactor and DivFactor ----
   Matrix_MultFactor(&mat_X,5.0);
   printf("matX = matX*5.0 = \n");
   Matrix_Print(&mat_X);
   Matrix_DivFactor(&mat_Y,5.0);
   printf("matY = matY/5.0 = \n");
   Matrix_Print(&mat_Y);

   //----- Matrix Transpose Operation ----
   Matrix_Transpose(&mat_Y, &mat_Y);
   printf("\nmatY = Transpose(matY,matY) = \n");
   Matrix_Print(&mat_Y);

   //----- Matrix Determinant Calculation ----
   Matrix_Determ(&mat_M, &determ);
   printf("\nmatM = \n");
   Matrix_Print(&mat_M);
   printf("Matrix_Determ(mat_M)=%f \n",determ);

   //---- Matrix Inverse Computatin -----
   printf("\nmatN = \n");
   Matrix_Print(&mat_N);
   Matrix_Determ(&mat_N, &determ);
   printf(" Matrix_Determ(matN) = %f \n",determ);
   Matrix_Inverse(&mat_N, &invmat_N);
   printf("\ninvmatN = \n");
   Matrix_Print(&invmat_N);

   //----- 3X3 Matrix Determinant Computation ----
   float mat3X3[3*3]={
		23,35,6.455,
		2.353,434,5656,
		233,344,343543.23
		};
   printf("mat3x3 = \n");
   for(i=0; i<3; i++)
   {
	for(j=0; j<3; j++)
		printf("%f ",mat3X3[3*i+j]);
	printf("\n");
   }
   printf("|mat3X3| = %f \n",Matrix3X3_Determ(mat3X3) );

   //----- 4X4 Matrix Determinant Computation ----
   float mat4X4[4*4]={
		1,1,1,1,
		1,23,35,6.455,
		1,2.353,434,5656,
		1,233,344,343543.23
		};
   struct float_Matrix mat_4X4;
   mat_4X4.nr=4; mat_4X4.nc=4; mat_4X4.pmat=mat4X4;
   float invmat4X4[4*4]={0};
   struct float_Matrix invmat_4X4;
   invmat_4X4.nr=4; invmat_4X4.nc=4; invmat_4X4.pmat=invmat4X4;
   printf("\nmat_4X4 = \n");
   Matrix_Print(&mat_4X4);
   Matrix_Determ(&mat_4X4, &determ);
   printf("determ of |mat4X4| = %e \n",determ );
   Matrix_Inverse(&mat_4X4, &invmat_4X4);
   printf("\ninvmat_4X4 = \n");
   Matrix_Print(&invmat_4X4);

/*
   printf("mat4x4 = \n");
   for(i=0; i<4; i++)
   {
	for(j=0; j<4; j++)
		printf("%f ",mat4X4[4*i+j]);
	printf("\n");
   }
   printf("|mat4X4| = %e \n",MatrixGT3X3_Determ(4,mat4X4) );
*/
   //----- 8X8 Matrix Determinant Computation ----
   float mat6X6[6*6]={
1548196.19929056,       317736.825975421,       65209.2355136882,       13382.9133063996,       2746.5797928119,
563.681493376731,
537.339513653319,       267.231594241498,       132.900564999072,       66.0945807220341,       32.8703914904499,
16.3472197710038,
0.0226191669422462,     0.0344597830259496,     0.0524986905674963,     0.0799805532503283,     0.121848541917522,
0.185633464186686,
107730.122892644,       29729.5669002975,       8204.2712321051,        2264.07827183226,       624.802651687529,
172.422640335594,
753042.807566855,       167432.476943158,       37227.1457261511,       8277.13000618498,       1840.34740786374,
409.185137734936,
40359797792706,      0.281614466038468,      0.329949967550431,      0.386581636298681,      0.452933402699983,
0.530673596515285,
		};

   struct float_Matrix mat_6X6;
   mat_6X6.nr=6; mat_6X6.nc=6; mat_6X6.pmat=mat6X6;

   float invmat6X6[6*6]={0};
   struct float_Matrix invmat_6X6;
   invmat_6X6.nr=6; invmat_6X6.nc=6; invmat_6X6.pmat=invmat6X6;

   float matmult6X6[6*6]={0};
   struct float_Matrix matmult_6X6;
   matmult_6X6.nr=6; matmult_6X6.nc=6; matmult_6X6.pmat=matmult6X6;


   printf("\nmat_6X6 = \n");
   Matrix_Print(&mat_6X6);

   Matrix_Determ(&mat_6X6, &determ);
   printf("determ of |mat6X6| = %e \n",determ );

   Matrix_Inverse(&mat_6X6, &invmat_6X6);
   printf("\ninvmat_6X6 = \n");
   Matrix_Print(&invmat_6X6);

   Matrix_Multiply(&mat_6X6,&invmat_6X6, &matmult_6X6);
   printf("\nmatmult_6X6 = mat_6X6 * invmat_6X6 \n");
   Matrix_Print(&matmult_6X6);

/*
   printf("mat6x6 = \n");
   for(i=0; i<6; i++)
   {
	for(j=0; j<6; j++)
		printf("%f   ",mat6X6[6*i+j]);
	printf("\n");
   }
   printf("|mat6X6|= %e \n",MatrixGT3X3_Determ(6,mat6X6) );
*/

   //----- 8X8 Matrix Determinant Computation ----
   float mat8X8[8*8]={
1548196.19929056,       317736.825975421,       65209.2355136882,       13382.9133063996,       2746.5797928119,
563.681493376731,       115.684542210269,       23.7419774529573,
537.339513653319,       267.231594241498,       132.900564999072,       66.0945807220341,       32.8703914904499,
16.3472197710038,        8.12985736172746,      4.04316952043861,
0.0226191669422462,     0.0344597830259496,     0.0524986905674963,     0.0799805532503283,     0.121848541917522,
0.185633464186686,      0.282808333063805,      0.430852021216898,
107730.122892644,       29729.5669002975,       8204.2712321051,        2264.07827183226,       624.802651687529,
172.422640335594,       47.5823315089988,       13.1309801742137,
753042.807566855,       167432.476943158,       37227.1457261511,       8277.13000618498,       1840.34740786374,
409.185137734936,       90.9787338128256,       20.2283251342007,
40359797792706,      0.281614466038468,      0.329949967550431,      0.386581636298681,      0.452933402699983,
0.530673596515285,      0.621756894854155,      0.728473470014719,
9.87536751506927e-007,  4.59013148592374e-006,  2.13352131208463e-005,  9.91673812194352e-005,  0.000460936079814079,
0.00214245921452982,    0.00995828204156898,    0.0462867066718926,
2600.45128660534,       1085.42261166079,       453.052995829233,       189.103317753609,       78.9313063032946,
32.9457525587259,	13.7514842018456,       5.7398390708038,
		};



   struct float_Matrix mat_8X8;
   mat_8X8.nr=8; mat_8X8.nc=8; mat_8X8.pmat=mat8X8;

   float invmat8X8[8*8]={0};
   struct float_Matrix invmat_8X8;
   invmat_8X8.nr=8; invmat_8X8.nc=8; invmat_8X8.pmat=invmat8X8;

   float matmult8X8[8*8]={0};
   struct float_Matrix matmult_8X8;
   matmult_8X8.nr=8; matmult_8X8.nc=8; matmult_8X8.pmat=matmult8X8;

   printf("\nmat_8X8 = \n");
   Matrix_Print(&mat_8X8);

   Matrix_Determ(&mat_8X8, &determ);
   printf("determ of |mat8X8| = %e \n",determ );

   Matrix_Inverse(&mat_8X8, &invmat_8X8);
   printf("\ninvmat_8X8 = \n");
   Matrix_Print(&invmat_8X8);

   Matrix_Multiply(&mat_8X8,&invmat_8X8, &matmult_8X8);
   printf("\nmatmult_8X8 = mat_8X8 * invmat_8X8 \n");
   Matrix_Print(&matmult_8X8);



/*
   printf("mat8x8 = \n");
   for(i=0; i<8; i++)
   {
	for(j=0; j<8; j++)
		printf("%f   ",mat8X8[8*i+j]);
	printf("\n");
   }
   printf("|mat8X8|= %f \n",MatrixGT3X3_Determ(8,mat8X8) );
*/
#if 1
   system("date");

   float mat9X9[9*9]={0};
   for(i=0; i<9; i++)
	for(j=0;j<9;j++)
		mat9X9[i*9+j]=(0.2*(i+1))*j+0.1*i;
   struct float_Matrix mat_9X9;
   mat_9X9.nr=9; mat_9X9.nc=9; mat_9X9.pmat=mat9X9;
   printf("\nmat_9X9 = \n");
   Matrix_Print(&mat_9X9);

   float invmat9X9[9*9]={0};
   struct float_Matrix invmat_9X9;
   invmat_9X9.nr=9; invmat_9X9.nc=9; invmat_9X9.pmat=invmat9X9;

   float  matmult9X9[9*9]={0}; /* For: matmult=mat_9X9 * invmat_9X9 */
   struct float_Matrix matmult_9X9;
   matmult_9X9.nr=9; matmult_9X9.nc=9; matmult_9X9.pmat=matmult9X9;

   Matrix_Determ(&mat_9X9, &determ);
   printf("determ of |mat9X9| = %e \n",determ );

   Matrix_Inverse(&mat_9X9, &invmat_9X9);
   printf("\ninvmat_9X9 = \n");
   Matrix_Print(&invmat_9X9);

   Matrix_Multiply(&mat_9X9,&invmat_9X9, &matmult_9X9);
   printf("\nmatmult_9X9 = mat_9X9 * invmat_9X9 \n");
   Matrix_Print(&matmult_9X9);

   system("date");
#endif

#if 0
   system("date");

   float mat10X10[10*10]={0};
   for(i=0; i<10; i++)
	for(j=0;j<10;j++)
		mat10X10[i*10+j]=(0.2*(i+1))*j+0.1*i;
   struct float_Matrix mat_10X10;
   mat_10X10.nr=10; mat_10X10.nc=10; mat_10X10.pmat=mat10X10;
   printf("\nmat_10X10 = \n");
   Matrix_Print(&mat_10X10);

   float invmat10X10[10*10]={0};
   struct float_Matrix invmat_10X10;
   invmat_10X10.nr=10; invmat_10X10.nc=10; invmat_10X10.pmat=invmat10X10;

   float  matmult10X10[10*10]={0}; /* For: matmult=mat_10x10 * invmat_10x10 */
   struct float_Matrix matmult_10X10;
   matmult_10X10.nr=10; matmult_10X10.nc=10; matmult_10X10.pmat=matmult10X10;

   Matrix_Determ(&mat_10X10, &determ);
   printf("determ of |mat10X10| = %e \n",determ );

   Matrix_Inverse(&mat_10X10, &invmat_10X10);
   printf("\ninvmat_10X10 = \n");
   Matrix_Print(&invmat_10X10);

   Matrix_Multiply(&mat_10X10,&invmat_10X10, &matmult_10X10);
   printf("\nmatmult_10X10 = mat_10X10 * invmat_10X10 \n");
   Matrix_Print(&matmult_10X10);

   system("date");
#endif

//----- (3,1) * (1,1) Matrix_Multiply()  -------
   float Mat3X1A[3*1]=
{
  1,
  2,
  3
};
   struct float_Matrix Mat_3X1A;
   Mat_3X1A.nr=3; Mat_3X1A.nc=1; Mat_3X1A.pmat=Mat3X1A;

   float Mat3X1B[3*1]={0};
   struct float_Matrix Mat_3X1B;
   Mat_3X1B.nr=3; Mat_3X1B.nc=1; Mat_3X1B.pmat=Mat3X1B;

 float Mat1X1A[1*1]=
{
  0.5
};
  struct float_Matrix Mat_1X1A;
  Mat_1X1A.nr=1; Mat_1X1A.nc=1; Mat_1X1A.pmat=Mat1X1A;

printf("Mat_3X1A=\n");
Matrix_Print(&Mat_3X1A);
printf("Mat_1x1A=\n");
Matrix_Print(&Mat_1X1A);

Matrix_Multiply(&Mat_3X1A, &Mat_1X1A, &Mat_3X1B);
printf("Mat_3x1B=\n");
Matrix_Print(&Mat_3X1B);

return 0;
}
