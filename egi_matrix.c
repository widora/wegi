/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Note:
0. For nonsingular matrix only.
1. Beware of the case that two input matrix pionters are the same!
2. Too big the determinant value indicates that the matrix equations
   may NOT be rational!
3. A float type represents accurately at least the first six decimal digits.
   and a range at lease 10^-37 to 10^37,   while a double has min. 15
   significant decimal digits and a range at lease 10^-307 - 10^308

		-----  Float Limits (see float.h)  -----

FLT_MANT_DIG: 	Number of bits in the mantissa of a float:  24
FLT_DIG: 	Min. number of significant decimal digits for a float: 	6
FLT_MIN_10_EXP: Min. base-10 negative exponent for a float with a full set of significant figures: -37
FLT_MAX_10_EXP: Max. base-10 positive exponent for a float: 38
FLT_MIN: 	Min. value for a positive float retaining full precision: 0.000000 =1.175494e-38
FLT_MAX: 	Max. value for a positive float: 340282346638528880000000000000000000000.000000  =3.402823e+38
FLT_EPSILON: 	Diff. between 1.00 and the leaset float value greater than 1.00: 0.000000=1.192093e-07
 1/(2^(FLT_MANT_DIG-1)) = 0.000000119209

		-----  Double Limits (see float.h)  -----
DBL_MANT_DIG:   Number of bits in the mantissa of a double: 53
DBL_DIG:	Min. number of significant decimal digits for a double: 15
DBL_MIN_10_EXP:	Min. base-10 negative exponent for a double with a full set of significant figures: -307
DBL_MAX_10EXP:	Max. base-10 positive exponent for a double: 308
DBL_MIN:	Min. value for a positive double retaining full precision: 0.00000000000000000000 =2.225074e-308
DBL_MAX:	Max. value for a positive double: 17976931348623153000000.......00000000000.00000000000000000000  =1.797693e+308
DBL_EPSILON:	Diff. between 1.00 and the leaset double value greater than 1.00: 0.00000000000000022204=2.220446e-16
 1/(2^(DBL_MANT_DIG-1)) = 0.00000000000000022204


TODO:
	1. matrix norm and condition number.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include "egi_matrix.h"
#include <stdint.h>
#include <sys/time.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>

/*----------------------------------------------
Calculate and return time difference in us

Return
  if tm_start > tm_end then return 0 !!!!
  us
----------------------------------------------*/
inline uint32_t mat_get_costtimeus(struct timeval tm_start, struct timeval tm_end)
{
        uint32_t time_cost;
        if(tm_end.tv_sec>tm_start.tv_sec)
                time_cost=(tm_end.tv_sec-tm_start.tv_sec)*1000000+(tm_end.tv_usec-tm_start.tv_usec);
        /* also consider tm_sart > tm_end ! */
        else if( tm_end.tv_sec==tm_start.tv_sec )
        {
                time_cost=tm_end.tv_usec-tm_start.tv_usec;
                time_cost=time_cost>0 ? time_cost:0;
        }
        else
                time_cost=0;

        return time_cost;
}


/*-------------------------------------------------------------------------------
               Integral of time difference and fx[num]

1. Only ONE instance is allowed, since timeval and sum_dt is static var.
2. In order to get a regular time difference,you shall call this function
in a loop.
dt_us will be 0 at first call
3. parameters:
	*num ---- number of integral groups.
	*fx ---- [num] functions of time,whose unit is rad/us(10^-6 s) !!!
	*sum ----[num] results integration(summation) of fx * dt_us in sum[num]
	*pdt_us --- period time(us) for the LAST LOOP !!!
return:
	summation of dt_us
-------------------------------------------------------------------------------*/
inline uint32_t mat_tmIntegral_NG(uint8_t num, const double *fx, double *sum, uint32_t* pdt_us)
{
	   uint32_t dt_us;//time in us
	   static struct timeval tm_end,tm_start;
	   static struct timeval tm_start_1, tm_start_2;
	   static double sum_dt; //summation of dt_us
	   int i;

	   gettimeofday(&tm_end,NULL);// !!!--end of read !!!
           /* to minimize error between two timer functions */
           gettimeofday(&tm_start,NULL);// !!! --start time !!!
	   tm_start_2 = tm_start_1;
	   tm_start_1 = tm_start;
           /* get time difference for integration calculation */
	   if(tm_start_2.tv_sec != 0)
	           dt_us=mat_get_costtimeus(tm_start_2,tm_end); //return  0 if tm_start > tm_end
	   else // discard fisrt value
		   dt_us=0;

	   *pdt_us=dt_us;
  	   sum_dt +=dt_us;

	   /* integration calculation */
	   for(i=0;i<num;i++)
	   {
         	  sum[i] += fx[i]*(double)dt_us;
	   }

	   return sum_dt;
}


/*------------  single group time integral  -------------------*/
inline uint32_t mat_tmIntegral(const double fx, double *sum, uint32_t *pdt_us)
{
	   uint32_t dt_us;//time in us
	   static struct timeval tm_end,tm_start;
	   static struct timeval tm_start_1, tm_start_2;
	   static double sum_dt; //summation of dt_us

	   gettimeofday(&tm_end,NULL);// !!!--end of read !!!
           /* to minimize error between two timer functions */
           gettimeofday(&tm_start,NULL);// !!! --start time !!!
	   tm_start_2 = tm_start_1;
	   tm_start_1 = tm_start;
           /* get time difference for integration calculation */
	   if(tm_start_2.tv_sec != 0)
	           dt_us=mat_get_costtimeus(tm_start_2,tm_end); //return  0 if tm_start > tm_end
	   else // discard fisrt value
		   dt_us=0;

	   *pdt_us=dt_us;
	   sum_dt +=dt_us;

           (*sum) += fx*dt_us;

	   return sum_dt;
}



/* <<<<<<<<<<<<<<<<<<<      MATRIX OPERATIONS     >>>>>>>>>>>>>>>>>>>>>
 NOTE:
	1. All matrix data is stored from row to column.
	2. All indexes are starting from 0 !!!
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */


/*-----------------------------------------------------
Init a NULL float_Matrix pointer,
Allocate mem for struct float_Matrix and its pmat
Return:
	NULL  --- fails
	pMat --- OK
-----------------------------------------------------*/
struct float_Matrix * init_float_Matrix(int nr, int nc)
{
	struct float_Matrix * pMat;

	/* check column and row number */
	if( nc < 1 || nr < 1)
	{
		fprintf(stderr,"%s: Column or row number illegal!\n", __func__);
		return NULL;
	}

	/* allocate mem. for pMat */
	pMat=malloc(sizeof(struct float_Matrix));
        if( pMat == NULL)
	{
		fprintf(stderr,"%s: malloc for pMat failed!\n",__func__);
		return NULL;
	}

	pMat->nr=nr;
	pMat->nc=nc;

	/* allocate mem. for pMat->pmat */
        pMat->pmat=calloc(1,nr*nc*sizeof(float));
        if( pMat->pmat == NULL)
	{
		fprintf(stderr,"%s: Calloc for pMat->pmat failed!\n",__func__);
		return NULL;
	}

	return pMat;
}


/*----------------    Release Matirx    -------------------
Release a float_Matrix pointer, and its pmat of nr*nc array
----------------------------------------------------------*/
void release_float_Matrix(struct float_Matrix ** pMat)
{
	if(pMat==NULL)
		return;

	if( *pMat != NULL ) {
		if( (*pMat)->pmat != NULL )
			free( (*pMat)->pmat);
		free(*pMat);
		*pMat=NULL;
	}
}



/*------------------------------------------------------------
	Copy data from array and fill to pMat->pmat
-------------------------------------------------------------*/
struct float_Matrix * Matrix_FillArray(struct float_Matrix * pMat, const float *array)
{
    if(pMat == NULL || pMat->pmat==NULL || array == NULL)
    {
	fprintf(stderr,"%s: pMat or array is NULL!\n",__func__);
	return NULL;
    }

    memcpy(pMat->pmat,array,(pMat->nr)*(pMat->nc)*sizeof(float));

    return pMat;
}


/*-------------------------------------------
              Print matrix
-------------------------------------------*/
void Matrix_Print(struct float_Matrix *matA)
{
   int i,j;

   if( matA == NULL || matA->pmat == NULL)
   {
	printf("%s: Matrix data is NULL!\n",__func__);
	return;
   }

   int nr=matA->nr;
   int nc=matA->nc;

   printf("       ");
   for(i=0; i<nc; i++)
	printf("       Column(%d)",i);
   printf("\n");

   for(i=0; i<nr; i++)
   {
        printf("Row(%d):  ",i);
        for(j=0; j<nc; j++)
           printf("%15.5f ", (matA->pmat)[nc*i+j] );
        printf("\n");
   }
   printf("\n");
}


/*-------------------------     MATRIX ADD/SUB    -------------------------
     matC = matA +/- matB
Addup/subtraction operation of two matrices with same row and column number
*matA:   pointer to matrix A
*matB:   pointer to matrix B
*matC:   pointer to mastrix A+B or A-B
Note: matA matB and matC may be the same!!
Return:
	NULL  --- fails
	matC --- OK
-------------------------------------------------------------------------*/
struct float_Matrix* Matrix_Add( const struct float_Matrix *matA,
		   		 const struct float_Matrix *matB,
		   		 struct float_Matrix *matC )
{
   int i,j;
   int nr,nc;

   /* check pointer */
   if(matA==NULL || matB==NULL || matC==NULL)
   {
	   fprintf(stderr,"%s: matrix is NULL!\n",__func__);
	   return NULL;
   }
   /* check pmat */
   if(matA->pmat==NULL || matB->pmat==NULL || matC->pmat==NULL)
   {
	   fprintf(stderr,"%s: matrix pointer pmat is NULL!\n", __func__);
	   return NULL;
   }
   /* check dimension */
   if(  matA->nr != matB->nr || matB->nr != matC->nr ||
        matA->nc != matB->nc || matB->nc != matC->nc )
   {
	  fprintf(stderr,"%s: matrix dimensions don't match!\n",__func__);
	  return NULL;
   }

   nr=matA->nr;
   nc=matA->nc;

   for(i=0; i<nr; i++) 	/* row count */
   {
	for(j=0; j<nc; j++) /* column count */
	{
		(matC->pmat)[i*nc+j] = (matA->pmat)[i*nc+j]+(matB->pmat)[i*nc+j];
	}
   }

   return matC;
}

struct float_Matrix* Matrix_Sub( const struct float_Matrix *matA,
				 const struct float_Matrix *matB,
				 struct float_Matrix *matC  )
{
   int i,j;
   int nr,nc;

   /* check pointer */
   if(matA==NULL || matB==NULL || matC==NULL)
   {
	   fprintf(stderr,"%s: matrix is NULL!\n",__func__);
	   return NULL;
   }
   /* check pmat */
   if(matA->pmat==NULL || matB->pmat==NULL || matC->pmat==NULL)
   {
	   fprintf(stderr,"%s: matrix pointer pmat is NULL!\n", __func__);
	   return NULL;
   }
   /* check dimension */
   if(  matA->nr != matB->nr || matB->nr != matC->nr ||
        matA->nc != matB->nc || matB->nc != matC->nc )
   {
	  fprintf(stderr,"%s: matrix dimensions don't match!\n",__func__);
	  return NULL;
   }

   nr=matA->nr;
   nc=matA->nc;

   for(i=0; i<nr; i++) /* row count */
   {
        for(j=0; j<nc; j++) /* column count */
        {
                (matC->pmat)[i*nc+j] = (matA->pmat)[i*nc+j]-(matB->pmat)[i*nc+j];
        }
   }
   return matC;
}


/*-------------------------------------------------------------------
Copy a column of a matrix (matA) to another matrix (matB).
matA and matB may be the same!
Their number of rows MUST be the same!

@matA:   Pointer to matrix A
@nclmA:  The source column
@matB:   Pointer to the result matrix.
@nclmB:  The destination column
Return:
	NULL  --- fails
	matB   --- OK
--------------------------------------------------------------------*/
struct float_Matrix* Matrix_CopyColumn(struct float_Matrix *matA, int nclmA, struct float_Matrix *matB, int nclmB)
{
    int i;
    int ncA,nrA,ncB;

   /* Check pointers */
   if(matA==NULL || matB==NULL )
   {
	   fprintf(stderr,"%s: Input matrix is NULL!\n",__func__);
	   return NULL;
   }

   /* check pmat */
   if(matA->pmat==NULL || matB->pmat==NULL )
   {
	   fprintf(stderr,"%s: Input matrix pointer pmat is NULL!\n",__func__);
	   return NULL;
   }

   /* Check nr */
   if(matA->nr != matB->nr)
   {
	   fprintf(stderr,"%s: matA->nr and matB->nr are different!\n",__func__);
	   return NULL;
   }

    ncA=matA->nc;
    nrA=matA->nr;
    ncB=matB->nc;

   /* check column number */
   if(nclmA<0 || nclmB<0 || nclmA > ncA || nclmB > ncB)
   {
	   fprintf(stderr,"Matrix_CopyColumn():column number out of range!\n");
	   return NULL;
   }

   for(i=0; i<nrA; i++)
	(matB->pmat)[nclmB+i*ncB] = (matA->pmat)[nclmA+i*ncA];

   return   matB;

}


/*----------------     MATRIX MULTIPLY    ---------------------
Multiply two matrices
   !!! ncA == nrB !!!!

@matA[nrA,ncA]:   matrix A
@matB[nrB,ncB]:   matrix B
@matC[nrA,ncB]:   mastrix c=A*B

Return:
	NULL  ---  fails
	matC  --- OK
----------------------------------------------------------*/
struct float_Matrix* Matrix_Multiply( const struct float_Matrix *matA,
				      const struct float_Matrix *matB,
				      struct float_Matrix *matC )
{
	int i,j,k;
	int nrA,ncA,nrB,ncB,nrC,ncC;

	/* check pointers */
	if(matA==NULL || matB==NULL || matC==NULL)
	{
	   fprintf(stderr,"%s: matrix is NULL!\n",__func__);
	   return NULL;
	}
   	/* check pmat */
	if(matA->pmat==NULL || matB->pmat==NULL || matC->pmat==NULL)
	{
	    fprintf(stderr,"%s: matrix pointer pmat is NULL!\n",__func__);
	    return NULL;
	}

	nrA=matA->nr;
	ncA=matA->nc;
	nrB=matB->nr;
	ncB=matB->nc;
	nrC=matC->nr;
	ncC=matC->nc;

	/* match matrix dimension */
	if(ncA != nrB || nrC != nrA || ncC != ncB)
	{
	   fprintf(stderr,"%s: dimension not match!\n",__func__);
	   return NULL;
	}

	/* result matrix with row: nrA  column: ncB, recuse nrA and ncB then. */
	for(i=0; i<nrA; i++) { /* rows of matC (matA) */
		for(j=0; j<ncB; j++) /* columns of matC  (matB) */
		{
			/* clear matC->pmat first before += */
			(matC->pmat)[i*ncB+j] =0;
			for(k=0; k<ncA; k++)//
			{
				(matC->pmat)[i*ncB+j] += (matA->pmat)[i*ncA+k] * (matB->pmat)[ncB*k+j]; // (element j, row i of matA) .* (column j of matB)
				//matC[0] += matA[k]*matB[ncB*k];//i=0,j=0
			}
		}
	}

       return matC;
}


/*-----------------------------------------------
	 matA=matA*fc
Multiply a matrix with a factor

@matA:   pointer to matrix A
@fc:      the  multiplicator

Return:
	NULL  --- fails
	matA --- OK
-----------------------------------------------*/
struct float_Matrix* Matrix_MultFactor(struct float_Matrix *matA, float fc)
{
	int i;
	int nr;
	int nc;

	/* check pointer */
	if(matA==NULL)
	{
	   fprintf(stderr,"%s: matA is a NULL!\n",__func__);
	   return NULL;
	}
   	/* check pmat */
	if(matA->pmat==NULL)
	{
	    fprintf(stderr,"%s: matA->pmat is a NULL!\n",__func__);
	    return NULL;
	}

	nr=matA->nr;
	nc=matA->nc;

	for(i=0; i<nr*nc; i++)
		(matA->pmat)[i] *= fc;

	return matA;
}


/*-----------------------------------------------
	matA=matA/factor
Divide a matrix with a factor

@matA:    pointer to matrix
@fc:      the divider

Return:
	NULL  --- fails
	pointer to matA --- OK
-----------------------------------------------*/
struct float_Matrix* Matrix_DivFactor(struct float_Matrix *matA, float fc)
{
	int i;
	int nr;
	int nc;

	/* check pointer */
	if(matA==NULL)
	{
	   fprintf(stderr,"%s: matA is a NULL pointer!\n",__func__);
	   return NULL;
	}
   	/* check pmat */
	if(matA->pmat==NULL)
	{
	    fprintf(stderr,"%s: matA->pmat is a NULL!\n",__func__);
	    return NULL;
	}
	/* check fc */
	if(fc==0)
	{
	    fprintf(stderr,"%s: divider factor is ZERO!\n",__func__);
	    return NULL;
	}

	nr=matA->nr;
	nc=matA->nc;

	for(i=0; i<nr*nc; i++)
		(matA->pmat)[i] /= fc;

	return matA;
}


/*----------------     MATRIX TRANSPOSE    ---------------------
	matA(i,j) -> matB(j,i)
Transpose a matrix and copy to another matrix
transpose means: swap element matA(i,j) and matA(j,i)
after operation row number will be cn, and column number will be nr.

@matA:   pointer to matrix A
@matB:   pointer to matrix B, who will receive the transposed matrix.

Note:   1.MatA and matB may be the same!
	2.Applicable ONLY if (nrA*ncA == nrB*ncB)

Return:
	NULL 		---  fails
	point to matB  	--- OK
------------------------------------------------------------------*/
struct float_Matrix* Matrix_Transpose( const struct float_Matrix *matA,
                                       struct float_Matrix *matB )
{
        int i,j;
        int nr;
        int nc;
        int nbytes;
        float *mat=NULL; /* for temp. buff, only if matA==matB */

        /* check pointer */
        if(matA==NULL || matB==NULL)
        {
           fprintf(stderr,"%s: matrix is NULL!\n",__func__);
           return NULL;
        }
        /* check pmat */
        if(matA->pmat==NULL || matB->pmat==NULL)
        {
            fprintf(stderr,"%s: matrix data pmat is NULL!\n",__func__);
            return NULL;
        }

        nr=matA->nr;
        nc=matA->nc;
        nbytes=nr*nc*sizeof(float);

	/* If matA==matB */
	if(matA==matB) {
        	mat=malloc(nbytes);
        	if(mat==NULL)
        	{
           		fprintf(stderr,"%s: malloc() mat fails!\n",__func__);
           		return NULL;
        	}
	}
        /* match matrix dimension */
        if( nr*nc != (matB->nr)*(matB->nc) )
        {
           fprintf(stderr,"%s: dimensions do not match!\n", __func__);
           return NULL;
        }

        /* Swap to matB */
	if(matA==matB) {
	        for(i=0; i<nc; i++) //nc-> new nr
        	        for(j=0; j<nr; j++)// nr -> new nc
                	        mat[i*nr+j]=(matA->pmat)[j*nc+i];
	}
	else {
	        for(i=0; i<nc; i++) //nc-> new nr
        	        for(j=0; j<nr; j++)// nr -> new nc
                	        matB->pmat[i*nr+j]=matA->pmat[j*nc+i];
	}

        /* Recopy to matB */
	if(matA==matB)
	        memcpy(matB->pmat,mat,nbytes);

        /* revise nc and nr for matB */
        matB->nr = nc;
        matB->nc = nr;

        /* release temp. mat */
	if(mat!=NULL)
	        free(mat);

        return matB;
}


/*-------------   MATRIX DETERMINANT VALUE   --------------

	!!!! for matrix dimension NOT great than 3 ONLY !!!!

Calculate determinant value of a SQUARE matrix
@matA:   the square matrix
@determ: determinant of the matrix

Return:
	NULL ---  fails
	float pointer to the result  --- OK
----------------------------------------------------------*/
float* Matrix_Determ( const struct float_Matrix *matA,
				    float *determ )
{
     int nrc;

     /* preset determ */
     *determ = 0.0;

     /* check pointer */
     if(matA==NULL || determ==NULL)
     {
	   fprintf(stderr,"%s: matrix is NULL!\n",__func__);
	   return determ;
     }
     /*  check matrix dimension */
     if(matA->nr != matA->nc)
     {
	   fprintf(stderr,"%s: it's NOT a square matrix!\n",__func__);
	   return determ;
     }

     nrc=matA->nr;

     /* --- CASE 1 --- */
     if(nrc==1)
     {
	*determ = (matA->pmat)[0];
	return determ;
     }
     /* --- CASE 2 --- */
     else if(nrc==2)
     {
	*determ = (matA->pmat)[0]*(matA->pmat)[3]-(matA->pmat)[1]*(matA->pmat)[2];
	 return determ;
     }
     /* --- CASE 3 --- */
     else if(nrc==3)
     {
	*determ=Matrix3X3_Determ(matA->pmat);
	return determ;
     }
     /* --- CASE >3 --- */
     else
     {

	*determ=MatrixGT3X3_Determ(nrc, matA->pmat);
	return determ;

     }

}


/*--------------------     MATRIX INVERSE    ---------------------
Compute the inverse of a SQUARE matrix

@matA: the sqaure matrix,
@matAdj: for adjugate matrix, also for the final inversed square matrix!!!

Return:
	NULL ---  fails
	pointer to the result matAdj  --- OK
------------------------------------------------------------------*/
struct float_Matrix* Matrix_Inverse(const struct float_Matrix *matA,
				    struct float_Matrix *matAdj )
{
	int i,j,k;
	int nrc;
	float  det_matA; //determinant of matA
	struct float_Matrix matCof;

	/* Check pointer */
	if(matA==NULL || matAdj==NULL)
	{
		fprintf(stderr,"%s: matA and/or matAdj is a NULL!\n",__func__);
		return NULL;
	}
	/* Check data */
        if(matA->pmat==NULL || matAdj->pmat==NULL)
        {
	   	fprintf(stderr,"%s: matrix data is NULL!\n",__func__);
	   	return NULL;
        }
	/* Check dimension */
	if( matA->nc != matA->nr || matAdj->nc != matAdj->nr )
	{
		fprintf(stderr,"%s: matrix is not square!\n",__func__);
		return NULL;
	}
	if( matA->nc != matAdj->nc )
	{
		fprintf(stderr,"%s: dimensions do NOT match!\n",__func__);
		return NULL;
	}

        nrc=matA->nr;
	matCof.nc=nrc-1;
	matCof.nr=nrc-1;

	/* Check nrc and malloc matCof */
	if( nrc <= 0)
        {
		fprintf(stderr,"%s:  nrc <= 0 !! \n",__func__);
		return NULL;
        }
	else if( nrc == 1 ) // if it's ONE dimentsion matrix !!!
        {
		matAdj->pmat[0]=1.0/(matA->pmat[0]);
		return matAdj;
        }
        else // nrc > 1
        {
		/* malloc matCof */
		matCof.pmat=calloc(1, (nrc-1)*(nrc-1)*sizeof(float)); //for cofactor matrix data
		if( matCof.pmat==NULL)
		{
			fprintf(stderr,"%s: malloc. matCof.pmat failed!\n",__func__);
			return NULL;
		}
   	}

	/* Check if matrix matA is invertible */
	Matrix_Determ(matA,&det_matA); /* compute determinant of matA */
//	printf("Matrix_Inverse(): determint of input matrix is %f\n",det_matA);

	if(det_matA == 0)
	{
		fprintf(stderr,"%s: matrix is NOT invertible!\n",__func__);
		return NULL;
	}
	//else if(  ( det_matA < 0 && det_matA > -FLT_MIN*4 )  || ( det_matA > 0 && det_matA < FLT_MIN*4 )  )
	else if(  ( det_matA < 0 && det_matA > -1.0e-17 )  || ( det_matA > 0 && det_matA < 1.0e-17 )  )
	{
		printf("WARNING: determinant value is too small! check rationality of the matrix equations.\n");
	}

  	/* Compute adjugate matrix */
	for(i=0; i<nrc*nrc; i++)  /* i, also cross center element natural number */
	{
            	/* compute cofactor matrix matCof(i)[] */
		j=0; /* element number of matCof */
		k=0; /* element number of original matA */
		while(k<nrc*nrc) /* traverse all elements of the original matrix */
		{
			/* skip elements accroding to i */
			if( k/nrc == i/nrc ) /* skip row  elements */
			{
				k+=nrc;      /* skip one row */
				continue;
			}
		 	if( k%nrc == i%nrc ) /* skip column elements */
			{
				k+=1; // skip one element
				continue;
			}
			/* copy an element of matA to matCof */
			matCof.pmat[j]=matA->pmat[k];
			k++;
			j++;
		} /* end of while() */
		/* finish i_th matCof */

		/* compute determinant of the i_th cofactor matrix as i_th element of the adjugate matrix */
		Matrix_Determ(&matCof, (matAdj->pmat+i));
		/* decide the sign of the element */
		//if( (i/nrc)%2 != (i%nrc)%2 )
		if( ((i/nrc)&0x1) != ((i%nrc)&0x1) )
			(matAdj->pmat)[i] = -1.0*(matAdj->pmat)[i];
	} /* end of for(i),  computing adjugate matrix NOT finished yet */
	/* transpose the matrix to finish computing adjugate matrix !!! */
	Matrix_Transpose(matAdj,matAdj);

	/* compute inverse matrix */
	Matrix_DivFactor(matAdj,det_matA); /* matAdj /= det_matA */

	/* free mem */
	free(matCof.pmat);

	return matAdj;
}


/*-------------------------------------------------
       3x3 matrix determinant computation

@pmat: pointer to a 3x3 data array as input matrix
return:
	Fails       0
	succeed     determinant value
--------------------------------------------------*/
float  Matrix3X3_Determ(float *pmat)
{
     int i,j,k;
     float pt=0.0,mt=0.0; //plus and minus multiplication
     float tmp=1.0;
     /* preset determ */
     float determ = 0.0;

     /* check pointer */
     if(pmat==NULL)
     {
	fprintf(stderr,"Matrix4X4_Determ(): pmat is NULL!\n");
	return 0;
     }
     /* plus multiplication */
     for(i=0; i<3; i++)
     {
	   k=i;
  	   for(j=0; j<3; j++)
	   {
		tmp *= pmat[k];
		if( (k+1)%3 == 0)
			k+=1;
		else
			k+=(3+1);
     	   }
	   pt += tmp;
	   tmp=1.0;
     }

     /* minus multiplication */
     for(i=0; i<3; i++)
     {
	   k=i;
	   for(j=0; j<3; j++)
	   {
		tmp *= pmat[k];
		if( k%3 == 0 )
			k+=(2*3-1);
		else
			k+=(3-1);
	   }
	   mt += tmp;
	   tmp=1.0;
      }

     determ = pt-mt;
     return determ;
}


/*------------------------------------------------------------------------
              >3x3 matrix determinant computation
!!! Warning: THis is a self recursive function !!!!
The input Must be a square matrix, number of rows and columns are the same

@ncr: dimension number, number of columns or rows
@pmat: pointer to a >=4x4 data array as input matrix

return:
	fails       0
	succeed     determinant value
------------------------------------------------------------------------*/
float  MatrixGT3X3_Determ(int nrc, float *pmat)
{
   int i,j,k;
   float *pmatCof=NULL; /* pointer to cofactor matrix */
   float determ=0;
   float sign; 	   /* sign of element of the adjugate matrix */

   /* --- CASE   nrc<3  --- */
   if (nrc < 3)
   {
	printf("%s: matrix dimension is LESS than 3x3!\n",__func__);
	return 0;
   }
   /* ---  CASE   nrc=3  !!!! this case is a MUST for recursive call !!! --- */
   if (nrc == 3)
	return Matrix3X3_Determ(pmat);

   /* ---   CASE   nrc >3   --- */
   pmatCof=calloc(1,(nrc-1)*(nrc-1)*sizeof(float));
   if(pmatCof == NULL)
   {
	fprintf(stderr,"%s: malloc pmatConf failed!\n",__func__);
	return 0;
   }

   /* ---- TEST ---- */
   if (nrc==8)
	printf("Cal nrc==8 ..." );

   /* The terminant of a matrix equals summation of (any) one row of (element value)*sign*(determinant of its cofactor matrix) */
   for(i=0; i<nrc; i++)  /* summation first row,  i- also cross center element natural number */
   {
       /* compute cofactor matrix matCof(i)[] */
	j=0; /* element number of matCof */
	k=0; /* element number of original matA */
	while(k<nrc*nrc) /* traverse all elements of the original matrix */
	{
		/* --- skip elements accroding to i --- */
		if( k/nrc == i/nrc ) //skip row  elements
		{
			k+=nrc;  //skip one row
			continue;
		}
	 	if( k%nrc == i%nrc ) //skip column  elements
		{
			k+=1; // skip one element
			continue;
		}
		//--- copy an element of matA to matCof
		pmatCof[j]=pmat[k];
		k++;
		j++;
	}/* end of while() */
	/* finish i_th matCof */

	/* decide the sign of the element */
	//if( (i/nrc)%2 != (i%nrc)%2 )
	if( ((i/nrc)&0x1) != ((i%nrc)&0x1) )
		sign=-1.0;
	else
		sign=1.0;

	/* --- recursive call --- */
        /* summation of first row of (element value)*sign*(determinant of its cofactor */
	determ += pmat[i]*sign*MatrixGT3X3_Determ(nrc-1, pmatCof);
//	printf("determ[%d]=%f\n",i,determ);

   } /* end of for(i) */

   /* ---- TEST ---- */
   if (nrc==8)
	printf(" ... OK\n" );

  /* Free pamtCof */
  free(pmatCof);

  return determ;

}

/*-------------------------------------------------------------------------------------
Solve the equation matrix : A*X=B

@matAB: Input matrix in n*(n+1) dimensions.  n --- unkown X dimensions.
@matX: sovled result X.

return:
	fails       NULL
	succeed     pointer to the result matX
---------------------------------------------------------------------------------------*/
struct float_Matrix*  Matrix_SolveEquations( const struct float_Matrix *matAB, struct float_Matrix *matX)
{
	int n;
	int i,j;
	EGI_MATRIX *matA=NULL;
	EGI_MATRIX *matINVA=NULL;
	EGI_MATRIX *matB=NULL;

	/* Check pointer */
	if(matAB==NULL || matX==NULL)
	{
		fprintf(stderr,"%s: matA and/or matX is a NULL!\n",__func__);
		return NULL;
	}
	/* Check data */
        if(matAB->pmat==NULL || matX->pmat==NULL)
        {
	   	fprintf(stderr,"%s: matrix data is NULL!\n",__func__);
	   	return NULL;
        }

	/* Check dimension */
	if( matAB->nr+1 != matAB->nc || matAB->nr != matX->nr || matX->nc !=1 )
        {
	   	fprintf(stderr,"%s: Input matA or matX with wrong dimensions!\n",__func__);
	   	return NULL;
        }

	n=matAB->nr; /* n*(n+1) dimensions */

	/* Allocate matrix A/B as of equation: A*X=B  */
	matA=init_float_Matrix(n, n);
	matINVA=init_float_Matrix(n, n);
	matB=init_float_Matrix(n, 1);
	if(matA==NULL || matINVA==NULL || matB==NULL ) {
		fprintf(stderr,"%s: Fail to allocate matA or matB!\n",__func__);

		matX=NULL;
		goto FUNC_END;
	}

	/* Copy value from matAB */
	for(i=0; i< n; i++) {
		for(j=0; j< n; j++)
			matA->pmat[i*n+j]=matAB->pmat[i*(n+1)+j];
		matB->pmat[i]=matAB->pmat[i*(n+1)+(n+1-1)];
	}
#if 0
	printf("matA:\n");
	Matrix_Print(matA);
	printf("matB:\n");
	Matrix_Print(matB);
#endif

	/* Solve the equation */
	if( Matrix_Inverse( matA, matINVA )==NULL ) {
		matX=NULL;
		goto FUNC_END;
	}
	if( Matrix_Multiply( matINVA, matB, matX )== NULL ) {
		matX=NULL;
		goto FUNC_END;
	}


 FUNC_END:
	release_float_Matrix(&matA);
	release_float_Matrix(&matINVA);
	release_float_Matrix(&matB);

	return matX;
}


/*-------------------------------------------------------------------------
Solve matrix equation by Guass-Jordan Elimination Method
	 A*X=B  -> combine A and B to AB

@matAB: Input matrix in n*(n+1) dimensions as AB.  n --- unkown X dimensions.
@matX: sovled result X.

return:
	fails       NULL
	succeed     pointer to the result matX
-------------------------------------------------------------------------*/
struct float_Matrix* Matrix_GuassSolve( const EGI_MATRIX *matAB )
{
        int n;
        int i,j;
	int kc, kr;
	EGI_MATRIX *matX;
	EGI_MATRIX *pmatAB; /* A copy of matAB */
	int max_index;	  /* Row index of the element with Max. coefficient value in current column.  */
	float tmp;
	float val;

        /* Check pointer */
        if(matAB==NULL) {
                fprintf(stderr,"%s: matAB is a NULL!\n",__func__);
                return NULL;
        }
        /* Check data */
        if(matAB->pmat==NULL) {
                fprintf(stderr,"%s: matAB->pmat is NULL!\n",__func__);
                return NULL;
        }

        /* Check dimension */
        if( matAB->nr+1 != matAB->nc ) {
                fprintf(stderr,"%s: Input matAB with wrong dimensions,  nr+1 != nc !\n",__func__);
                return NULL;
        }

	/* Create a copy of matAB */
        pmatAB=init_float_Matrix(matAB->nr, matAB->nc);
	if(pmatAB==NULL) {
                fprintf(stderr,"%s: Fail to init. pmatAB!\n",__func__);
                return NULL;
	}
	/* Make a copy of matAB */
	for(i=0; i < matAB->nr*matAB->nc; i++)
		pmatAB->pmat[i]=matAB->pmat[i];


	/* Get number of X */
        n=pmatAB->nr; /* n*(n+1) dimensions */

        /* Allocate matX */
        matX=init_float_Matrix(n, 1);
	if(matX==NULL) {
                fprintf(stderr,"%s: Fail to init. matX!\n",__func__);
		release_float_Matrix(&pmatAB);
                return NULL;
	}

	/* Transfer matAB to a triangular shape, by eliminating coefficients of lower left part of the matrix */
	for(i=0; i<n; i++) /* Tranverse row==column, current row/column number i */
	{
		/* Find the Row index of the element with Max. value in current column */
		max_index=i;
		for(j=i+1; j<n; j++) { /* transverse row */
			if( fabsf(pmatAB->pmat[j*(n+1)+i]) > fabsf(pmatAB->pmat[max_index*(n+1)+i]) )
				max_index=j;
		}

		/* TODO: Check current coefficient: Unique solution, No solution, Infinite solutions. */
		if( fabsf(pmatAB->pmat[max_index*(n+1)+i]) < 1.0e-20 ) {
			fprintf(stderr,"%s: WARNING! Too small coefficients! the matrix may has NO unique solution.\n",__func__);
		    #if 0 /* OR ignore it */
			release_float_Matrix(&matX);
			release_float_Matrix(&pmatAB);
			return NULL;
		    #endif
		}

		/* Swap [max_index] row with current row [i] */
		for(kc=i; kc<n+1; kc++) {  /* Tranverse columns */
			tmp=pmatAB->pmat[max_index*(n+1)+kc];
			pmatAB->pmat[max_index*(n+1)+kc]=pmatAB->pmat[i*(n+1)+kc];
			pmatAB->pmat[i*(n+1)+kc]=tmp;
		}

		/* Normalize [i,i] coefficients to 1. */
		tmp=pmatAB->pmat[i*(n+1)+i];
		if(tmp==0) {
			fprintf(stderr,"%s: Dividor is zero!\n",__func__);
		}
		for(kc=i; kc<n+1; kc++) {  /* Tranverse columns */
		    #if 1 /* Skip zero elements */
			if( (val=pmatAB->pmat[i*(n+1)+kc])==0.0 )
				continue;
			pmatAB->pmat[i*(n+1)+kc] = val/tmp;
		    #else
			pmatAB->pmat[i*(n+1)+kc] /= tmp;
		    #endif
		}

		/* Eliminate column [i+1,i]..[n-1,i] coefficients for followed rows */
		for(kr=i+1; kr<n; kr++) { 		/* Transvers following rows */
			tmp=pmatAB->pmat[kr*(n+1)+i];	/* First nonzero element */
			for(kc=i; kc<n+1; kc++) {	/* Transverse columns */
				pmatAB->pmat[kr*(n+1)+kc] -= tmp*pmatAB->pmat[i*(n+1)+kc]; /* Row[kr] -= Row[i]*tmp */
			}
		}
	}
	/* NOW: pmatAB is a triangular matrix, with lower left part of the matrix all zeros.  */
	/* Test --- */
	//Matrix_Print(pmatAB);

	/* Resolve matrix */
	for(kr=n-1; kr>=0; kr--) {	/* Tranverse rows from bottom to up */
		/* Get kc==n */
		matX->pmat[kr]=pmatAB->pmat[kr*(n+1)+n];  /* For matX, nc=1 */
		for(kc=n-1; kc>kr; kc--)	/* Tranverse column from right to left  */
			matX->pmat[kr] -= pmatAB->pmat[kr*(n+1)+kc]*matX->pmat[kc];

	}

	/* Release copy */
	release_float_Matrix(&pmatAB);

	return matX;
}


/*----------------------------------------------------------------------------------
Solve matrix equation by Thomas Algorithm (or The Tridiagonal Matrix Algorithom(TDMA))
For tridiagonal matrix:

			A*X=D  --> L*U*X=D  --> L*Y=D, where Y=U*X
      _				 _       _		        _   _			 _
     |	a1 c1			  |     |  l1		         | | 1 u1		  |
     |	b2 a2 c2		  |     |  m1 l2		 | |   1  u2		  |
     |	   b3 a3 c3		  |     |     m2 l3	         | |      1  u3	   	  |
A=   |	      .. .. ..		  | --> |      ..  ...	         |*|         .. ..        |
     |        .. .. ..            |     |       ..  ...          | |          .. ...      |
     |		 .. a(N-1) c(N-1) |     |	     l(N-1)      | |             1 u(N-1) |
     |_		    b(N)   a(N)  _|     |_	     m(N)  l(N) _| |_		      1  _|

Note:
1. Matrix A MUST be strictly diagonally dominat? |a(i)|>|b(i)|+|c(i)|
   NOPE! weakly diagonally dominat also OK.
	at leaset one |a(k)|>|b(k)|+|c(k)|   others |a(k)|=|b(k)|+|c(k)|.

@abcd:  An EGI_MATRIX with row nr=4, just to store a,b,c,d.in form of:
       		{ a1,a2,a3,...a(N), b1,b2...b(N), c1,c2,....c(N), d1,d2,d3,...d(N) }
	So as to save memory space!
	Note: If abcd is invalid, the matAB will apply.

			--- WARNING ----
        1. Notice their position in the original matrix A.
	2. For input range a[0]:a[n-1]:  a[0]=0 AND c[n-1]=0

@matAD: Input matrix in n*(n+1) dimensions as AD.  n --- unkown X dimensions.
        Note: if abcd is valid then matAD will be ignored.

@matX: sovled result X.

return:
	fails       NULL
	succeed     pointer to the result matX
------------------------------------------------------------------------------------*/
EGI_MATRIX* Matrix_ThomasSolve( const EGI_MATRIX *abcd, const EGI_MATRIX *matAD)
{
        int n;	/* Matrix X dimension */
        int i;
	EGI_MATRIX *matX=NULL;
	bool abcd_valid=false;

	/* Pointer to a,b,c,d. calloc together. */
	float *a=NULL,*b=NULL,*c=NULL,*d=NULL;
	/* Pointer to l,m,u. calloc together. */
	float *l=NULL,*m=NULL,*u=NULL, *y=NULL;  /* y as middle var: A*X=D  --> L*U*X=D  --> L*Y=D, Y=U*X */

	/* Use abcd */
	if( abcd != NULL && abcd->nr==4 && abcd->pmat != NULL)
	{
		/* Get dimension */
		n=abcd->nc;

		/* Assign pinters */
		a=abcd->pmat;
		b=a+n;
		c=b+n;
		d=c+n;

		/* To apply matrix abcd */
		abcd_valid=true;

	}
	/* Use matAD instead */
	else {
	        /* Check matAB */
        	if(matAD==NULL || matAD->pmat==NULL || matAD->nr+1 != matAD->nc  ) {
                	fprintf(stderr,"%s: abcd AND matAD are both invalid!\n",__func__);
	                return NULL;
        	}

		/* Get dimension */
		n=matAD->nr;

		/* Calloc a,b,c,d */
	        a=calloc(4*n, sizeof(float));
        	if(a==NULL) {
                	fprintf(stderr,"%s: Fail to calloc a,b,c,d!\n",__func__);
			return NULL;
	        }
		b=a+n;
		c=b+n;
		d=c+n;

		/* Copy to a,b,c,d */
		for(i=0; i<n; i++) {    /* n */
			a[i]=matAD->pmat[i*(n+1+1)];  /* nr=n+1 */
			d[i]=matAD->pmat[(i+1)*(n+1)-1];  /* nr=n+1 */
		}

		for(i=1; i<n; i++)    /* n-1 */
			b[i]=matAD->pmat[i*(n+1+1)-1];

		for(i=0; i<n-1; i++)  /* n-1 */
			c[i]=matAD->pmat[i*(n+1+1)+1];

	}
	/* TEST */
	#if 0
	for(i=0; i<n; i++)
		printf("a[%d]=%f, b[%d]=%f, c[%d]=%f, d[%d]=%f\n", i,a[i],i,b[i],i,c[i],i,d[i]);
	#endif

        /* TODO: Check Matrix solution condtion: MUST be diagonally dominat. |a(i)|>|b(i)|+|c(i)| */
	#if 0  /* Ignore , weakly diagonlly dominant also OK */
	for(i=0; i<n; i++) {
		if( !(fabsf(a[i]) > fabsf(b[i])+fabsf(c[i])) ) {
			printf("%s: Matrix is NOT strictly diagonlly dominant!\n",__func__);
			goto END_FUNC;
		}
	}
	#endif

	/* Calloc l,m,u,y */
        l=calloc(4*n, sizeof(float));
       	if(l==NULL) {
               	fprintf(stderr,"%s: Fail to calloc l,m,u!\n",__func__);
		goto END_FUNC;
        }
	m=l+n;
	u=m+n;
	y=u+n;

        /* Allocate matX */
        matX=init_float_Matrix(n, 1);
	if(matX==NULL) {
                fprintf(stderr,"%s: Fail to init. matX!\n",__func__);
		goto END_FUNC;
	}

	/* Calculate l,m,u */
	l[0]=a[0];
	u[0]=c[0]/l[0];
	l[1]=a[1]-b[1]*u[0];
	for(i=1; i<n-1; i++) {  /* to n-2 */
		m[i]=b[i];
		u[i]=c[i]/l[i];
		l[i+1]=a[i+1]-b[i+1]*u[i];
	}
	/* m[0],u[n-1] is NO use */
	m[n-1]=b[n-1];

	/* Calculate middle var. y[] */
	y[0]=d[0]/l[0];
	for(i=1; i<n; i++)
		y[i]=(d[i]-m[i]*y[i-1])/l[i];

	/* Calculate final X[n*1] */
	matX->pmat[n-1]=y[n-1];
	for(i=n-2; i>=0; i--)
		matX->pmat[i]=y[i]-u[i]*matX->pmat[i+1];

END_FUNC:
	/* free */
	if(abcd_valid==false)
		free(a); /* a,b,c,d */
	free(l); /* l,m,u,y */


	return matX;
}
