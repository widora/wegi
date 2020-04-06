/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

	A*X=B

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_matrix.h"

int main(void)
{

#if 0
 	float matAB[3*4]=  // row 3, column 4
 	{
		1,1,1,6,
		3,1,-1,10,
		2,2,5,15
	};
	struct float_Matrix mat_AB;
	mat_AB.nr=3; mat_AB.nc=4; mat_AB.pmat=matAB;

	printf("matAB:\n");
	Matrix_Print(&mat_AB);

	EGI_MATRIX *matX=init_float_Matrix(3,1);
#else

 	float matAB[4*5]=  /* 4 rows, 5 columns */
 	{
		2,3,4,-5,-6,
		6,7,-8,9,96,
		10,11,12,13,312,
		14,15,16,17,416
	};
	struct float_Matrix mat_AB;
	mat_AB.nr=4; mat_AB.nc=5; mat_AB.pmat=matAB;

	printf(" ---- matAB : A*X=B ---- \n");
	Matrix_Print(&mat_AB);

	EGI_MATRIX *matX=init_float_Matrix(4,1);
#endif


	Matrix_SolveEquations(&mat_AB, matX);
	printf(" ---- Result : X ----\n");
	Matrix_Print(matX);

	release_float_Matrix(matX);


return 0;
}
