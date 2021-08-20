#include <stdio.h>
#include <float.h> /* toolchain   gcc/gcc/ginclude/float.h */
#include <math.h>

#include <stdint.h>
/* Minimum for largest signed integral type.  */
//# define INTMAX_MIN             (-LONG_LONG_MAX-1)
/* Maximum for largest signed integral type.  */
//# define INTMAX_MAX             (LONG_LONG_MAX)

#include <limits.h>
/* Minimum and maximum values a `signed int' can hold.  */
//#  define INT_MIN       (-INT_MAX - 1)
//#  define INT_MAX       2147483647



int main(void)
{
	printf("INT32_MIN: %d	INT32_MAX: %d\n", INT32_MIN, INT32_MAX);
	printf("INT_MIN: %d	INT_MAX: %d\n", INT_MIN, INT_MAX);
	printf("\n	===============  Float Limits  =============\n");
	printf("Number of bits in the mantissa of a float: %d\n", FLT_MANT_DIG);
	printf("Min. number of significant decimal digits for a float: %d\n", FLT_DIG);
        printf("Min. base-10 negative exponent for a float with a full set of significant figures: %d\n",FLT_MIN_10_EXP);
        printf("Max. base-10 positive exponent for a float: %d\n",FLT_MAX_10_EXP);

	printf("Min. value for a positive float retaining full precision: %f =%e\n", FLT_MIN, FLT_MIN);
	/* Note: for 32bits system, = 1*2^(0-127+1) = 2^(-126)= 1.175494e-38 */
	printf("Max. value for a positive float: %f  =%e\n", FLT_MAX,FLT_MAX);
	/* Note: for 32bits system, = 1*2^(255-127)=2^128=3.402823669×e+38 */

	printf("Diff. between 1.00 and the leaset float value greater than 1.00: %f=%e\n",FLT_EPSILON,FLT_EPSILON);
	printf(" 1/(2^(FLT_MANT_DIG-1)) = %0.12f\n",1.0/pow(2,FLT_MANT_DIG-1));
	printf("	============================================\n\n");

        printf("\n      ===============  Double Limits  =============\n");
        printf("Number of bits in the mantissa of a double: %d\n", DBL_MANT_DIG);
        printf("Min. number of significant decimal digits for a double: %d\n", DBL_DIG);
        printf("Min. base-10 negative exponent for a double with a full set of significant figures: %d\n",DBL_MIN_10_EXP);
        printf("Max. base-10 positive exponent for a double: %d\n",DBL_MAX_10_EXP);
        printf("Min. value for a positive double retaining full precision: %.20f =%e\n", DBL_MIN, DBL_MIN);
        printf("Max. value for a positive double: %.20f  =%e\n", DBL_MAX,DBL_MAX);
        printf("Diff. between 1.00 and the leaset double value greater than 1.00: %.20f=%e\n",DBL_EPSILON,DBL_EPSILON);
        printf(" 1/(2^(DBL_MANT_DIG-1)) = %0.20f\n",1.0/pow(2,DBL_MANT_DIG-1));
        printf("        ============================================\n\n");


	//int a=0b0 10000010 10010000000000000000000;
	int a=0b01000001010010000000000000000000;
	float b=*(float *)&a;

 	printf("int a=%d\n",a);
	printf("float b=%f \n",b);


	unsigned int i=1;
	if( -1 > i)
		printf("-1 > %d \n",i);

	if( -1 > 0x44 )
		printf(" -1 > 012 \n" );


	return 0;
}
