#include <stdio.h>
#include <float.h> /* toolchain   gcc/gcc/ginclude/float.h */
#include <math.h>

int main(void)
{
	printf("\n	===============  Float Limits  =============\n");
	printf("Number of bits in the mantissa of a float: %d\n", FLT_MANT_DIG);
	printf("Min. number of significant decimal digits for a float: %d\n", FLT_DIG);
        printf("Min. base-10 negative exponent for a float with a full set of significant figures: %d\n",FLT_MIN_10_EXP);
        printf("Max. base-10 positive exponent for a float: %d\n",FLT_MAX_10_EXP);
	printf("Min. value for a positive float retaining full precision: %f =%e\n", FLT_MIN, FLT_MIN);
	printf("Max. value for a positive float: %f  =%e\n", FLT_MAX,FLT_MAX);
	printf("Diff. between 1.00 and the leaset float value greater than 1.00: %f=%e\n",FLT_EPSILON,FLT_EPSILON);
	printf(" 1/(2^(FLT_MANT_DIG-1)) = %0.12f\n",1.0/pow(2,FLT_MANT_DIG-1));
	printf("	============================================\n\n");

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
