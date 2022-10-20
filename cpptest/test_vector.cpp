/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test EGI_3D Class and Functions.

Journal:
2022-08-06:
	1. Test Class E3D_Ray,E3D_Plane and relevant functions.
2022-09-28:
	1. Test apPointsToNDC() / pointOutFrustumCode().
2022-10-03:
	1. Test E3D_ZNearClipTriangle()
2022-10-17:
	1. Test E3D_RTMatrix::inverse()

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
知之者不如好之者好之者不如乐之者
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

#if 1 ////////////////  E3D_RTMatrix::inverse()  ////////////////
        E3D_Vector axisX(1,0,0);
        E3D_Vector axisY(0,1,0);
        E3D_Vector axisZ(0,0,1);

        E3D_RTMatrix RTMat;
	E3D_RTMatrix invRTMat;
	E3D_RTMatrix testMat;

        E3D_RTMatrix RXmat, RYmat, RZmat, SCmat, TXYZmat;
        RXmat.setRotation(axisX, 20.0/180*MATH_PI);
        RYmat.setRotation(axisY, 60.0/180*MATH_PI);
        RZmat.setRotation(axisZ, 80.0/180*MATH_PI);
	SCmat.setScaleXYZ(0.01,1,100);  /* Scale is NOT orthogonal operation */


  #if 1 ////////////////////////////////////

	TXYZmat.setTranslation(10,20,30);
	RTMat=RXmat*TXYZmat;
	RTMat.print("RTMat=RXmat*TXYZmat");
/***
<<< RTMatrix 'RTMat=RXmat*TXYZmat' >>>
Rotation_Matrix[3x3]:
1.000000  0.000000  0.000000
0.000000  0.939693  0.342020
0.000000  -0.342020  0.939693
TxTyTz:
10.000000 20.000000 30.000000
*/

	RTMat=TXYZmat*RXmat;
	RTMat.print("RTMat=TXYZmat*RXmat");
/***
   <<< RTMatrix 'RTMat=TXYZmat*RXmat' >>>
Rotation_Matrix[3x3]:
1.000000  0.000000  0.000000
0.000000  0.939693  0.342020
0.000000  -0.342020  0.939693
TxTyTz:
10.000000 8.533248 35.031181
*/
	invRTMat=RTMat;
	invRTMat.inverse();
	invRTMat.print("invRTMat");
/**
   <<< RTMatrix 'invRTMat' >>>
Rotation_Matrix[3x3]:
1.000000  -0.000000  0.000000
-0.000000  0.939693  -0.342020
0.000000  0.342020  0.939693
TxTyTz:
-10.000001 -20.000002 -30.000000
*/
   #endif //////////////////////////////////

   #if 0 ////////////////////////////////////
	RTMat=RXmat*RYmat*RZmat*SCmat;
	RTMat.setTranslation(10, 20, 30);
	RTMat.print("RTMat");

	invRTMat=RTMat;
	invRTMat.inverse();
	invRTMat.print("invRTMat");

	testMat=RTMat*invRTMat;
	testMat.print("testMat");
  #endif ////////////////////////////////////////

exit(0);
#endif

#if 0 ////////////////  E3D_ZNearClipTriangle()  ////////////////
	E3DS_RenderVertex  rvts[4]; /* MAX. possible results~ */
	int np=3;
	int ret;
	rvts[0].pt.assign(0,10,50);
	rvts[1].pt.assign(50,20,150);
	rvts[2].pt.assign(-50,30,150);
	rvts[3].pt.assign(-80,40,75);

	float zc=100.0;
	ret=E3D_ZNearClipTriangle(zc, rvts, np, VTXNORMAL_ON|VTXCOLOR_ON|VTXUV_ON);

	printf("A trianlge is clipped on %d edges, and left with %d vertices.\n", ret, np);
	for(int k=0; k<np; k++)
		rvts[k].pt.print("rvts");

exit(0);
#endif

#if 0 ////////////////   mapPointsToNDC() / pointOutFrustumCode() --- E3D_ISOMETRIC_VIEW --- ////////////////
	E3DS_ProjMatrix projMatrix;

	/* Init projMatrix  (projmatrix , int type, int winW, int winH, int dnear, int dfar, int dv) */
	E3D_InitProjMatrix(projMatrix, E3D_ISOMETRIC_VIEW, 320, 240, 500, 10000000, 500);

	E3D_Vector vpts[8], savevpts[8];

	/* Points within the view frustum */
	vpts[0].assign(160 -0.1, 120 -1, 500 +1);
	vpts[1].assign(160 -0.1, 120.0 -0.1, 10000000 -1);
	vpts[2].assign(-160 +0.1, -120.0 +0.1, 10000000 -1);
	vpts[3].assign(0,0, 500 +0.1);

	/* Points out of the view frustum */
	vpts[4].assign(-160 -0.01, -120, 500);
	vpts[5].assign(160.0 +1, 120.0, 10000000 -1);
	vpts[6].assign(-160.0 -1, -120.0, 10000000);

	/* Note: For float type: 6 digits accuracy, for double type: 15 digits accuracy! */
	vpts[7].assign(0,0, 10000000+1); /* For ISOMETRIC, OK */

	for(int i; i<8; i++)
		savevpts[i]=vpts[i];

	/* Map to NDC coordinates ---- CAUTION: vpts[] modified! ---- */
	mapPointsToNDC(vpts, 8, projMatrix);

	/* Check, NOW vpts holds NDC coordinates value. */
	int code;
	for(int k=0; k<8; k++) {
		if( (code=pointOutFrustumCode(vpts[k])) ) {
			printf("vpts[%d] is out of ISOMETRIC frustum! code=0x%02X, ", k,code);
			vpts[k].print("NDC vpt");
			printf("\n\n");
		}
		else {
			printf("vpts[%d] is within ISOMETRIC frustum! code=0x%02X, ", k,code);
			vpts[k].print("NDC vpt");
			printf("\n\n");
		}
	}
#endif


#if 0 ////////////////   mapPointsToNDC() / pointOutFrustumCode() --- E3D_PERSPECTIVE_VIEW --- ////////////////
	E3DS_ProjMatrix projMatrix;

	/* Init projMatrix  (projmatrix , int type, int winW, int winH, int dnear, int dfar, int dv) */
	E3D_InitProjMatrix(projMatrix, E3D_PERSPECTIVE_VIEW, 320, 240, 500, 10000000, 500);

	E3D_Vector vpts[8], savevpts[8];
//	vpts[0]=pt0; vpts[1]=pt1; vpts[2]=pt2; vpts[3]=pt3; vpts[4]=pt4; vpts[5]=pt5;

	/* Points in the view frustum */
	vpts[0].assign(160 -0.1, 120 -1, 500 +1);
	vpts[1].assign(160.0*(10000000.0/500) -1, 120.0*(10000000.0/500) -1, 10000000 -1);
	vpts[2].assign(-160.0*(10000000.0/500) +1, -120.0*(10000000.0/500) +1, 10000000 -1);
	vpts[3].assign(0,0, 500 -.1); //10000000/2);

	/* Points out of the view frustum */
	vpts[4].assign(-160 -0.01, -120, 500);
	vpts[5].assign(160.0*(10000000.0/500), 120.0*(100000000.0/500), 10000000 -1);
	vpts[6].assign(-160.0*(10000000.0/500) -1, -120.0*(10000000.0/500) -1, 10000000);

	/* Note: For float type: 6 digits accuracy, for double type: 15 digits accuracy! */
	//vpts[7].assign(0,0, 10000000+100); /*  within frustum! code=0x00, Vector NDC vpt:(-0.000000, 0.000000, 1.000000) */
	vpts[7].assign(0,0, 10000000+4000); /* out of frustum! code=0x20, Vector NDC vpt:(-0.000000, 0.000000, 1.000000) */
	//vpts[7].assign(0,0, 10000000+60000); /* out of frustum! code=0x20, Vector NDC vpt:(-0.000000, 0.000000, 1.000001) <---- */

	for(int i; i<8; i++)
		savevpts[i]=vpts[i];

	/* Map to NDC coordinates ---- CAUTION: vpts[] modified! ---- */
	mapPointsToNDC(vpts, 8, projMatrix);

	/* Check, NOW vpts holds NDC coordinates value. */
	int code;
	for(int k=0; k<8; k++) {
		if( (code=pointOutFrustumCode(vpts[k])) ) {
			printf("vpts[%d] is out of PERSPECTIVE frustum! code=0x%02X, ", k,code);
			vpts[k].print("NDC vpt");
			printf("\n\n");
		}
		else {
			printf("vpts[%d] is within PERSPECTIVE frustum! code=0x%02X, ", k,code);
			vpts[k].print("NDC vpt");
			printf("\n\n");
		}
	}
#endif

#if 0 //////////////////  E3D_RtaSphere ///////////////////////////

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
	sceneA.importObj("fish.obj"); //"lion.obj");
	sceneA.importObj("volumes.obj");
	sceneA.importObj("nonexist.obj");
	sceneA.importObj("boxman.obj");
	sceneA.importObj("thinker.obj");
	sceneA.importObj("nonexist.obj");

	printf("Scene TriMeshList.size = %d\n", sceneA.triMeshList.size());
	for(unsigned int k=0; k<sceneA.triMeshList.size(); k++) {
		printf("fpathList[%d]: %s\n",k, sceneA.fpathList[k].c_str());
	}
	printf("Total meshInstanceList.size = %d\n", sceneA.meshInstanceList.size());
	for(unsigned int k=0; k<sceneA.meshInstanceList.size(); k++) {
		printf("meshInstanceList[%d] name: '%s'.\n",k, sceneA.meshInstanceList[k]->name.c_str());
	}

	/* Create an MeshInstance */
	if(sceneA.meshInstanceList.size()>2) {
 		E3D_MeshInstance meshInstanceA(*sceneA.meshInstanceList[0]);
		E3D_MeshInstance meshInstanceB(*sceneA.meshInstanceList[1]);
	}

	usleep(250000);
}

	exit(0);
#endif

#if 0 //////////////////  reverse_projectPoints( ) ///////////////////////////
	int ret;
	E3DS_ProjMatrix projMatrix={ .type=E3D_PERSPECTIVE_VIEW, .dv=500, .dnear=500, .dfar=10000000, .winW=320, .winH=240};
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

#if 0 ///////////////  E3D_Ray / E3D_Plane  ///////////////////////////
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
