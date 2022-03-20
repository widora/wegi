/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test EGI_3D Class and Functions.

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
------------------------------------------------------------------*/
#include <iostream>
#include <stdio.h>
#include "e3d_vector.h"
#include "e3d_trimesh.h"
#include "egi_math.h"
#include "egi_debug.h"
#include "egi_fbdev.h"
#include "egi_color.h"

using namespace std;

bool checkRTMat(E3D_RTMatrix & RTMat)
{
	if(RTMat.isOrthogonal()) {
//		printf("Matrix is orthorgonal!\n");
//		RTMat.print("OK");
		return true;
	}
	else {
		printf("Matrix is NOT orthorgonal!\n");
		RTMat.print("Error");
		return false;
	}
}

int main(void)
{
	cout << "Hello, this is C++!\n" << endl;

#if 0 ///////////////  E3D_Vector ////////////////////////////////
	E3D_Vector *pv = new E3D_Vector(5,5,5);
	pv->print("pv");
	delete pv;

	E3D_Vector v1(1,3,4);
	E3D_Vector v2(2,-5,8);
	E3D_Vector v3;
	E3D_Vector vv(1000,1000,1000);
	E3D_Vector v5(3,-4,5);

	v1.print("v1");
	v1.normalize();
	v1.print("Normalized v1");

	/* Restore */
	v1.x=1; v1.y=3; v1.z=4;

	v2.print("v2");
	printf("E3D_vector_distance(v1,v2)=%f\n", E3D_vector_distance(v1,v2));

	v3.print("uninit v3");

	vv.print("vv");
	v5.print("v5");
	printf("E3D_vector_magnitude(v5)=%f\n", E3D_vector_magnitude(v5));

	float dp=v1*v2;
	printf("Dot_Product: v1*v2=%f\n", dp);

	v3=v1/0.0;
	v3.print("v3=v1/0.0=");
	printf("v3 is %s!\n", v3.isInf()?"Infinite":"Normal");

	v3=-v1;
	v3.print("v3=-v1=");
	v3=-v1-v2;
	v3.print("v3=-v1-v2=");

	v3=v1+v2;
	v3.print("v3=v1+v2=");
	v3=v1+v2+vv;
	v3.print("v3=v1+v2+vv=");
	v3=v1-v2;
	v3.print("v3=v1-v2=");

	v3=v1*10;
	v3.print("v3=v1*10=");
	v3=v1/10;
	v3.print("v3=v1/10=");

	v3=0.01*v1;
	v3.print("v3=0.01*v1=");

	v3 *=1000;
	v3.print("v3*=1000 =");
	v3 /=1000;
	v3.print("v3/=1000 =");

	v3=E3D_vector_crossProduct(v1, v2);
	v3.print("v3=corssProduct(v1,v2)=");
#endif

#if 1 ///////////////  E3D_RTMatrix ////////////////////////////////
	int k;
        E3D_Vector axisX(1,0,0);
        E3D_Vector axisY(0,1,0);
        E3D_Vector axisZ(0,0,1);

	E3D_RTMatrix RTMat;
	E3D_RTMatrix RXmat, RYmat, RZmat;
	RXmat.setRotation(axisX, 20.0/180*MATH_PI);
	RYmat.setRotation(axisY, 60.0/180*MATH_PI);
	RZmat.setRotation(axisZ, 80.0/180*MATH_PI);

for(k=0; k<100000; k++) {
#if 1  /* Note: A non_orthogonal_matrix multiplied by an orthogonal_matrix may get an orthogonal_matrix!!!  */
	RTMat = RTMat*RXmat*RYmat*RZmat;
	if(!checkRTMat(RTMat)) {
		printf("k=%d \n",k);
		/* Try to orthNormalize */
		RTMat.orthNormalize();
		/* Check again */
		if(!checkRTMat(RTMat))break;
	}
#else
	RTMat = RTMat*RXmat;
	if(!checkRTMat(RTMat))break;

	RTMat = RTMat*RYmat;
	if(!checkRTMat(RTMat))break;

	RTMat = RTMat*RZmat;
	if(!checkRTMat(RTMat))break;
#endif
}
	printf("k=%d \n",k);

#endif
	return 0;
}
