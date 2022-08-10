/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test EGI_3D Class and Functions.

Journal:
2022-08-06:
	1. Test Class E3D_Radial,E3D_Plane and relevant functions.

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

#if 1 ///////////////  E3D_Radial / E3D_Plane  ///////////////////////////
	E3D_Vector  vp0(5,6,7);
	E3D_Vector  vd(3,4,5);

   //E3D_Radial -------------------------
	E3D_Radial  Rd1(vp0,vd);
	printf("Rd1---\n");
	Rd1.vp0.print(NULL);
	Rd1.vd.print(NULL);

	E3D_Radial  Rd2(1.1,1.2,1.3, 2.1,2.2,2.3);
	printf("Rd2---\n");
	Rd2.vp0.print(NULL);
	Rd2.vd.print(NULL);

   //E3D_Plane -------------------------
	E3D_Vector v1(0,0,0), v2(2,4,6), v3(5,4,9);
	E3D_Plane Plane1(v1,v2,v3);
	printf("\nPlane1---\n");
	Plane1.vn.print(NULL);
	printf("fd=%f.3\n",Plane1.fd);
	float c=Plane1.vn*(v1-v2);
	printf("c=Plane1.vn*(v1-v2)=%f\n",c);
	float d=Plane1.vn*(v2-v3);
	printf("d=Plane1.vn*(v2-v3)=%f\n",d);
	float e=Plane1.vn*(v1-v3);
	printf("e=Plane1.vn*(v1-v3)=%f\n",e);

   //E3D_RadialVerticalRadial(const E3D_Radial &rad1, const E3D_Radial &rad2)
        E3D_Radial Rd3(v1, Plane1.vn);
	printf("Rd1 and Plane1 is %s parallel.\n", true==E3D_RadialParallelRadial(Rd1, Rd3) ? "just" : "NOT");

   //E3D_RadialParallelPlane(const E3D_Radial &rad, const E3D_Plane &plane)
	printf("Rd1 and Plane1 is %s parallel.\n", true==E3D_RadialParallelPlane(Rd1,Plane1) ? "just" : "NOT");

   //E3D_Vector E3D_RadialIntsectPlane(const E3D_Radial &rad, const E3D_Plane &plane)
	bool forward;

     #if 0
	E3D_Radial Rd4(0,0,0, 1,1,1);
	E3D_Vector  pv1(10,0,0),pv2(0,10,10),pv3(0,0,10);
	E3D_Plane plane4(pv1,pv2,pv3);

	E3D_Vector Vinsct=E3D_RadialIntsectPlane(Rd4, plane4, forward);
	printf("Intersection point at %s: \n", forward?"forward":"backward");
	Vinsct.print();
     #endif

     #if 1
	E3D_Radial Rd5(0,0,0, 10,30,0);
	E3D_Vector pv1(0,40,0),pv2(40,0,0),pv3(40,0,10);
	E3D_Plane plane5(pv1,pv2,pv3);

	E3D_Vector Vinsct=E3D_RadialIntsectPlane(Rd5, plane5, forward);
	printf("Intersection point at %s: \n", forward?"forward":"backward");
	Vinsct.print();
     #endif



	exit(0);
#endif

#if 0 ///////////////  E3D_Vector  //////////////////////////////
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

#if 0 ///////////////  E3D_RTMatrix ////////////////////////////////
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
