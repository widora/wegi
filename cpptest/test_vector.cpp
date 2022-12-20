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
2022-11-02:
	1. Test E3D_Quaternion functions
2022-11-03:
	1. Test E3D_Quatrix functions
2022-12-17:
	1. Test E3D glTF functions:
	   E3D_glReadJsonKey(), E3D_glExtractJsonObjects(), E3D_glLoadBuffers()

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#include <iostream>
#include <stdio.h>
#include <vector>

#include "egi_math.h"
#include "egi_debug.h"
#include "egi_fbdev.h"
#include "egi_color.h"
#include "egi_utils.h"

#include "e3d_vector.h"
#include "e3d_trimesh.h"
#include "e3d_volumes.h"
#include "e3d_animate.h"
#include "e3d_glTF.h"


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

#if 0
int main(void)
{
	cout << "Hello, this is C++!\n" << endl;
	E3D_Pyramid apyramid(50,50);
	E3D_RtaSphere aball(2,100);
	E3D_ABone abone(50,200);
}
#endif

#if 1  /////////////////////
int main(void)
{
	cout << "Hello, this is C++!\n" << endl;


#if 1 //////////////// E3D glTF  ////////////////
E3D_TriMesh trimesh;
trimesh.loadGLTF("/tmp/doggy.gltf");
exit(0);

int cnt=0;
while(1) {
	string key;
	string strValue;
	vector<E3D_glBuffer> glbuffers;
	vector<E3D_glBufferView> glBufferViews;
	vector<E3D_glAccessor> glAccessors;
	vector <E3D_glMesh> glMeshes;

	char *pjson;
	EGI_FILEMMAP *fmap;
	fmap=egi_fmap_create("/tmp/doggy.gltf", 0, PROT_READ, MAP_SHARED);
	if(fmap==NULL) exit(0);
	else printf("fmap create~!\n");

	pjson=fmap->fp;
	/* Read Top_Level glTF Jobjects */
	while( (pjson=E3D_glReadJsonKey(pjson, key, strValue)) ) {

		cout << DBG_GREEN"Key: "<< key << DBG_RESET << endl;
		cout << DBG_GREEN"Value: "DBG_RESET<< strValue << endl;

		/* Case glTF Jobject: buffers */
		if(!key.compare("buffers")) {
			printf("--- buffers ---\n");

		#if 1 /* TEST: E3D_glExtractJsonObjects() ------------ Extract Jobjects to vector Jobjs */
			vector<string> Jobjs;
			E3D_glExtractJsonObjects(strValue, Jobjs);
			egi_dpstd(DBG_GREEN"Totally %zu objects found.\n"DBG_RESET, Jobjs.size());
			for(size_t k=0; k<Jobjs.size(); k++)
				printf("Jobjs[%zu]: %s\n", k, Jobjs[k].c_str() );
		#endif

#if 1
			/* Load glBuffers as per Json descript */
			E3D_glLoadBuffers(strValue, glbuffers);

			egi_dpstd(DBG_YELLOW"Totally %zu E3D_glBuffers loaded.\n"DBG_RESET, glbuffers.size());
			for(size_t k=0; k<glbuffers.size(); k++)
				printf("glbuffers[%zu]: byteLength=%d, uri='%s'\n", k, glbuffers[k].byteLength, glbuffers[k].uri.c_str() );
#endif

			//break;

		}
		/* Case glTF Jobject: bufferViews */
		else if(!key.compare("bufferViews")) {
			printf("--- bufferViews ---\n");

			/* Load data to glBufferViews */
			E3D_glLoadBufferViews(strValue, glBufferViews);

			printf(DBG_YELLOW"Totally %zu E3D_glBufferViews loaded.\n"DBG_RESET, glBufferViews.size());
			for(size_t k=0; k<glBufferViews.size(); k++) {
			    printf("glBufferViews[%zu]: name='%s', bufferIndex=%d, byteOffset=%d, byteLength=%d, byteStride=%d, target=%d.\n",
				k, glBufferViews[k].name.c_str(),
				glBufferViews[k].bufferIndex, glBufferViews[k].byteOffset, glBufferViews[k].byteLength,
				glBufferViews[k].byteStride, glBufferViews[k].target);
			}

			//break;
		}
		else if(!key.compare("accessors")) {
			printf("--- accessors ---\n");

			/* Load data to glAccessors */
			E3D_glLoadAccessors(strValue, glAccessors);

			printf(DBG_YELLOW"Totally %zu E3D_glAccessors loaded.\n"DBG_RESET, glAccessors.size());
			for(size_t k=0; k<glAccessors.size(); k++) {
		    printf("glAccessors[%zu]: name='%s', type='%s', count=%d, bufferViewIndex=%d, byteOffset=%d, componentType=%d, normalized=%s.\n",
				     k, glAccessors[k].name.c_str(), glAccessors[k].type.c_str(), glAccessors[k].count,
				     glAccessors[k].bufferViewIndex, glAccessors[k].byteOffset, glAccessors[k].componentType,
				     glAccessors[k].normalized?"Yes":"No" );
			}

			//break;
		}
		/* Case glTF Jobject: meshes */
		else if(!key.compare("meshes")) {
			printf("--- meshes ---\n");

			/* Load data to glMeshes */
			E3D_glLoadMeshes(strValue, glMeshes);

			printf(DBG_GREEN"Totally %zu E3D_glMeshes loaded.\n"DBG_RESET, glMeshes.size());
			for(size_t k=0; k<glMeshes.size(); k++) {
				printf(DBG_MAGENTA"\n    --- mesh_%d ---\n"DBG_RESET, k);
				printf("name: %s\n",glMeshes[k].name.c_str());
				for(size_t j=0; j<glMeshes[k].primitives.size(); j++) {
					int mode =glMeshes[k].primitives[j].mode;
					printf("mode:%d for '%s' \n",mode, (mode>=0&&mode<8)?glPrimitiveModeName[mode].c_str():"Unknown");
					printf("indices(AccIndex):%d\n",glMeshes[k].primitives[j].indicesAccIndex);
					printf("material(Index):%d\n",glMeshes[k].primitives[j].materialIndex);

					/* attributes */
					printf("Attributes:\n");
					printf("   POSITION(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.positionAccIndex);
					printf("   NORMAL(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.normalAccIndex);
					printf("   TANGENT(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.tangentAccIndex);
					printf("   TEXCOORD_0(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.texcoord);
					printf("   COLOR_0(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.color);
				}
			}

			break;
		}

	}

	/* Free */
	egi_fmap_free(&fmap);

	cnt++;
	printf("---cnt=%d, free fmap---\n", cnt);

usleep(100000);
 }//while()

	exit(0);
#endif

#if 0 ////////////////  E3D_BMatrixTree::E3D_BMatrixTree(const char *fbvh)  ////////////////
	E3D_BMatrixTree  bmtree("test.bvh");

exit(0);
#endif

#if 0 ////////////////  E3D_AnimQuatrices  ////////////////
while(1) {
	E3D_AnimQuatrices animQts;
	E3D_Quatrix qt;
	float t;

	t=0.2f;
	qt.setRotation('Y', MATH_PI*0.5);
	qt.setTranslation(0,0,0);
	animQts.insert(t, qt);

        t=0.5f;
        qt.setRotation('Y', MATH_PI);
	qt.setTranslation(30,60,90);
        animQts.insert(t, qt);

        t=0.123f;
        qt.setRotation('Y', MATH_PI*0.2);
        animQts.insert(t, qt);

        t=0.01f;
	qt.setTranslation(10,10,10);
        qt.setRotation('Y', MATH_PI*0.3);
        animQts.insert(t, qt);

        t=0.76f;
	qt.setTranslation(80,80,80);
        qt.setRotation('X', MATH_PI*0.7);
        animQts.insert(t, qt);

	animQts.print("AnimQts");

	/* Interpolate */
	animQts.interp(0.3, qt);
	qt.print("qt(t=0.3)");
	printf("Rotation angle: %fDeg\n", qt.getRotationAngle()*180/MATH_PI);
	animQts.interp(0.5, qt);
	qt.print("qt(t=0.5)");
	animQts.interp(0.0, qt);
	qt.print("qt(t=0.0)");
	animQts.interp(1.2, qt);
	qt.print("qt(t=1.2)");


	//animQts.qts[0].print("qts[0]");
	//animQts.qts[1].print("qts[1]");
	usleep(100000);
}
exit(0);
#endif


#if 0 ////////////////  E3D_Quatrix  ////////////////

	E3D_Quatrix qt;
	E3D_Quatrix qt0,qt1;
	E3D_RTMatrix mat;
	float ang;
	E3D_Vector axis;

	qt.setTranslation(10,20,30);

	printf("\n ------ Quatrix to Matrix ----\n\n");
	qt.setRotation('x', MATH_PI/2);
	qt.print();
	qt.toMatrix(mat);
	mat.print("X90");
	qt.setRotation('Y', MATH_PI/2);
	qt.print();
	qt.toMatrix(mat);
	mat.print("Y90");
	qt.setRotation('Z', MATH_PI/2);
	qt.print();
	qt.toMatrix(mat);
	mat.print("Z90");

	printf("\n ------Matrix to Quatrix ----\n\n");
	mat.setRotation('x', MATH_PI/2);
	mat.print("X90");
	qt.fromMatrix(mat);
	qt.print();
	mat.setRotation('y', MATH_PI/2);
	mat.print("Y90");
	qt.fromMatrix(mat);
	qt.print();
	mat.setRotation('z', MATH_PI/2);
	mat.print("Y90");
	qt.fromMatrix(mat);
	qt.print();

	printf("\n ----- Quatrix Inperpolation ----\n\n");
	qt0.setRotation('x', MATH_PI*0/180);
	qt1.setRotation('x', MATH_PI*90/180);
	qt.interp(qt0,qt1, 0.333333);

/* CAUTION: 0-180 interpolation axis can be '-X'! */
	ang=qt.getRotationAngle();
	printf("qt Rotation angle: %fDeg\n", ang/MATH_PI*180);
	axis=qt.getRotationAxis();
	axis.print("qt axis");

  exit(0);
#endif


#if 0 ////////////////  E3D_Quaternion  ////////////////

	E3D_Quaternion quat;
	E3D_Quaternion quat0,quat1;
	E3D_RTMatrix mat;
	float ang;
	E3D_Vector axis;

	printf("\n ------ Quaternion to Matrix ----\n\n");
	quat.setRotation('x', MATH_PI/2);
	quat.print();
	quat.toMatrix(mat);
	mat.print("X90");
	quat.setRotation('Y', MATH_PI/2);
	quat.print();
	quat.toMatrix(mat);
	mat.print("Y90");
	quat.setRotation('Z', MATH_PI/2);
	quat.print();
	quat.toMatrix(mat);
	mat.print("Z90");

	printf("\n ------Matrix to Quatermion ----\n\n");
	mat.setRotation('x', MATH_PI/2);
	mat.print("X90");
	quat.fromMatrix(mat);
	quat.print();
	mat.setRotation('y', MATH_PI/2);
	mat.print("Y90");
	quat.fromMatrix(mat);
	quat.print();
	mat.setRotation('z', MATH_PI/2);
	mat.print("Y90");
	quat.fromMatrix(mat);
	quat.print();

	printf("\n ----- Spherical Linear Inperpolation ----\n\n");
	quat0.setRotation('x', MATH_PI*0/180);
	quat1.setRotation('x', MATH_PI*180/180);
	quat.slerp(quat0,quat1, 0.25);

/* TODO CAUTION: 0-180 interpolation axis can be '-X'! */
	ang=quat.getRotationAngle();
	printf("Rotation angle: %fDeg\n", ang/MATH_PI*180);
	axis=quat.getRotationAxis();
	axis.print("axis");


	printf("\n ----- Quaternion setIntriRoation ----\n\n");
	float angs[3]={0.5*MATH_PI, 0.25*MATH_PI, 0.1*MATH_PI};
	mat.identity();
	mat.combIntriRotation("ZXY", angs);
	mat.print("ZXY");
        quat.fromMatrix(mat);

        quat.print();
	quat.setIntriRotation("zxy", angs);
	quat.print();
	quat.toMatrix(mat);
	mat.print("ZXY");


  exit(0);
#endif

#if 0 ////////////////  E3D_ABone/TestSkeleton  ////////////////
//  for(int k=0; k<5; k++) {
//  	E3D_TestSkeleton AmeshModel(50);
//  }

	E3D_ABone abone(50,200);
  	E3D_TestSkeleton AmeshModel(50);

//  	E3D_TestSkeleton BmeshModel(50);
 //	E3D_TestSkeleton CmeshModel(50);

  exit(0);
#endif

#if 0 ////////////////  E3DS_BMTreeNode  ////////////////
while(1) {
	E3D_BMatrixTree  bmtree;
	E3DS_BMTreeNode *pnode;

	/* Add 10 child node under root */
	for(int k=0; k<10; k++)
		bmtree.addChildNode(bmtree.root, 1.0*(k+1));

	/* Add subtree under root->firtChild */
	pnode=bmtree.root->firstChild;
	for(int k=0; k<5; k++) {
		/* sub.sub.sub..tree */
		pnode=bmtree.addChildNode(pnode, 1.0/(k+1));
	}

	/* set root->amat */
//	bmtree.root->amat.setTranslation(5.0, 5.1, 5.2);
	float ang[3]={0.5*MATH_PI, 0.5*MATH_PI, 0.5*MATH_PI};

	//E3D_combExtriRotation("X", &ang, bmtree.root->amat);
	bmtree.root->amat.combExtriRotation("X", ang);
	ang[0]=-0.5*MATH_PI;
	bmtree.root->amat.combIntriRotation("XY", ang);

	/* updateNodePtrList and Node.gmat */
	bmtree.updateNodePtrList(); /* update bmtree.NodePtrList and all NODE.gmat */
	printf("bmtree has %d nodes(self_Included) totally.\n", bmtree.nodePtrList.size());

	vector<E3DS_BMTreeNode*> listL1=bmtree.getTreeNodePtrList(bmtree.root->firstChild);
	printf("bmtree.root->firstChild has %d nodes(self_Included) totally.\n", listL1.size());
	for(size_t i=0; i<listL1.size(); i++)
		listL1[i]->gmat.print();

	usleep(10000);
}
#endif

#if 0 ////////////////  E3DS_BMTreeNode  ////////////////
	E3DS_BMTreeNode *root;
	E3DS_BMTreeNode *pnode;

 while(1) {
	/* Create root node */
	root=BMT_createRootNode();

	/* Add 10 child node under root */
	for(int k=0; k<10; k++)
		BMT_addChildNode(root, 1.0);

	/* Add subtree under root->firtChild */
	pnode=root->firstChild;
	for(int k=0; k<5; k++) {
		/* sub.sub.sub..tree */
		pnode=BMT_addChildNode(pnode,1.0);
	}

	/* Update all gmats */
	printf("Update tree gmats...\n");
	BMT_updateTreeGmats(root);

	/* Delete tree */
	printf("Delete tree...\n");
	BMT_deleteTreeNodes(root);

	usleep(10000);
}

exit(0);
#endif

#if 0 ////////////////  E3D_RTMatrix::inverse()  ////////////////
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

#endif //////////
