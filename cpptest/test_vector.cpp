/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test EGI_3D Class and Functions.

Journal:
2022-08-06:
	1. Test Class E3D_Ray,E3D_Plane and relevant functions.

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
------------------------------------------------------------------*/
#include <iostream>
#include <stdio.h>

#include "egi_math.h"
#include "egi_debug.h"
#include "egi_fbdev.h"
#include "egi_color.h"

#include "e3d_vector.h"
#include "e3d_trimesh.h"
#include "e3d_volumes.h"

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

#if 1 //////////////////  E3D_RtaSphere ///////////////////////////

while(1) {
//	E3D_RtaSphere sphereA() Error! parsed as function declaration!
	E3D_RtaSphere sphereA;
	E3D_RtaSphere sphereB(1.0); //(1,2.0);
	E3D_RtaSphere sphereC(1,2.0);

	sleep(1);
}
	exit(0);
#endif


#if 0 //////////////////  E3D_Scene ///////////////////////////
	int k=0;

while (0){
	printf(DBG_MAGENTA" ----- Test TriMesh: %d ----\n"DBG_RESET, ++k);
	//E3D_TriMesh trimesh("teapot.obj");
	E3D_TriMesh trimesh("lion.obj");
	sleep(1);
}

while (1){
	printf(DBG_MAGENTA" ----- Test Scene: %d ----\n"DBG_RESET, ++k);
	E3D_Scene sceneA;

	sceneA.importObj("teapot.obj");
	sceneA.importObj("lion.obj");
	sceneA.importObj("volumes.obj");
	sceneA.importObj("nonexist.obj");

	printf("Scene TriMeshList.size = %d\n", sceneA.TriMeshList.size());
	sleep(1);
}

	exit(0);
#endif

#if 0 //////////////////  reverse_projectPoints( ) ///////////////////////////
	int ret;
	E3D_ProjMatrix projMatrix={ .type=E3D_PERSPECTIVE_VIEW, .dv=500, .dnear=500, .dfar=10000000, .winW=320, .winH=240};
	E3D_Vector pt(100.0, 100.0, 1000.0);

	printf("View(camera) coord: %f,%f,%f\n",pt.x,pt.y,pt.z);
	ret=projectPoints(&pt, 1, projMatrix);
	printf("projectPoints ret=%d\n",ret);//ret==1, pt out of frustum
	printf("Project to Screen coord: %f,%f,%f\n",pt.x,pt.y,pt.z);
	reverse_projectPoints(&pt,1, projMatrix);
	printf("Reverse to View(camera) coord: %f,%f,%f\n",pt.x,pt.y,pt.z);

	exit(0);
#endif

#if 0 ///////////////  E3D_computeBarycentricCoord  ///////////////////////////
	//bool E3D_computeBarycentricCoord(const E3D_Vector vt[3], const E3D_Vector &pt, float bc[3])
	float bc[3]={0};

	E3D_Vector vt[3];
	vt[0].x=6; vt[0].y=12; vt[0].z=0;
	vt[1].x=6; vt[1].y=3; vt[1].z=0;
	vt[2].x=3; vt[2].y=9; vt[2].z=0;

	E3D_Vector pt(5,8,0);
	E3D_computeBarycentricCoord(vt, pt, bc);
	printf("Barycenter: %f,%f,%f \n", bc[0],bc[1],bc[2]);

/* ON the vertices */
	E3D_Vector pt2(6,12,0);
	E3D_computeBarycentricCoord(vt, pt2, bc);
	printf("Barycenter: %f,%f,%f \n", bc[0],bc[1],bc[2]);

	E3D_Vector pt3(6,3,0);
	E3D_computeBarycentricCoord(vt, pt3, bc);
	printf("Barycenter: %f,%f,%f \n", bc[0],bc[1],bc[2]);

	E3D_Vector pt4(3,9,0);
	E3D_computeBarycentricCoord(vt, pt4, bc);
	printf("Barycenter: %f,%f,%f \n", bc[0],bc[1],bc[2]);

/* On sieds */
	E3D_Vector pt5(6,5,0);
	E3D_computeBarycentricCoord(vt, pt5, bc);
	printf("Barycenter: %f,%f,%f \n", bc[0],bc[1],bc[2]);

	E3D_Vector pt6(4.5,6,0);
	E3D_computeBarycentricCoord(vt, pt6, bc);
	printf("Barycenter: %f,%f,%f \n", bc[0],bc[1],bc[2]);

	E3D_Vector pt7(4.5,10.5,0);
	E3D_computeBarycentricCoord(vt, pt7, bc);
	printf("Barycenter: %f,%f,%f \n", bc[0],bc[1],bc[2]);

#endif

#if 1 ///////////////  E3D_Ray / E3D_Plane  ///////////////////////////
	E3D_Vector  vp0(5,6,7);
	E3D_Vector  vd(3,4,5);

   //E3D_Ray -------------------------
	E3D_Ray  Rd1(vp0,vd);
	printf("Rd1---\n");
	Rd1.vp0.print(NULL);
	Rd1.vd.print(NULL);

	E3D_Ray  Rd2(1.1,1.2,1.3, 2.1,2.2,2.3);
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

   //E3D_RayVerticalRadial(const E3D_Ray &rad1, const E3D_Ray &rad2)
        E3D_Ray Rd3(v1, Plane1.vn);
	printf("Rd1 and Plane1 is %s parallel.\n", true==E3D_RayParallelRay(Rd1, Rd3) ? "just" : "NOT");

   //E3D_RayParallelPlane(const E3D_Ray &rad, const E3D_Plane &plane)
	printf("Rd1 and Plane1 is %s parallel.\n", true==E3D_RayParallelPlane(Rd1,Plane1) ? "just" : "NOT");

   //E3D_Vector E3D_RayIntsectPlane(const E3D_Ray &rad, const E3D_Plane &plane)
	bool forward;

	E3D_Vector Vinsct;

     #if 1
	E3D_Ray Rd4(0,0,0, 1,1,1);
	E3D_Vector pv1(10,0,0),pv2(0,10,0),pv3(0,0,10);
	E3D_Plane plane4(pv1,pv2,pv3);

	if(E3D_RayIntersectPlane(Rd4, plane4, Vinsct, forward)) {
		printf("Rd4/plane4 intersection point at %s: \n", forward?"forward":"backward");
		Vinsct.print(NULL);
	}
	else
		printf("Rd4 is parallel to plane4!\n");

     #endif

     #if 1
	//E3D_Ray Rd5(0,0,0, 10,30,0);
	E3D_Ray Rd5(0,0,0, 0,0,10);
	E3D_Vector pva(0,40,0),pvb(40,0,0),pvc(40,0,10);
	E3D_Plane plane5(pva,pvb,pvc);

	if(E3D_RayIntersectPlane(Rd5, plane5, Vinsct, forward)) {
		printf("Rd5/plane5 intersection point at %s: \n", forward?"forward":"backward");
		Vinsct.print(NULL);
	}
	else
		printf("Rd5 is parallel to plane5!\n");

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
