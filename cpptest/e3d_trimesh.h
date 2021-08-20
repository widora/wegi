/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


	         EGI 3D Triangle Mesh

Refrence:
1. "3D Math Primer for Graphics and Game Development"
                        by Fletcher Dunn, Ian Parberry

Note:
1. E3D default as Right_Hand coordinating system.
   1.1 Screen XY plane sees the Origin at the upper_left corner, +X axis at the upper side,
       +Y axis at the left side, while the +Z axis pointing to the back of the screen.
   1.2 If the 3D object looks up_right at Normal XY plane(Origin at lower_left corner),
       it shall rotates 180Deg around X-axis in order to show up_right face at Screen XY plane.
       OR to rotate the ViewPlane(-z --> +z) 180Deg around X-axis.
   1.3 Usually the origin is located at the center of the object, OR offset X/Y
       to move the projected image to center of the Screen.

2. Matrix as an input paramter in functions:
   E3D_RTMatrix:    Transform(Rotation+Translation) matrix, to transform
		    objects to the expected postion.
   E3D_ProjMatrix:  Projection Matrix, to project/map objects in a defined
		    frustum space to the Screen.

3. Since view direction set as ViewCoord(Camera) -Z ---> +Z, it needs to set FBDEV.flipZ=true
   to flip zbuff[] values.


TODO:
	1. Grow vtxList[] and triList[].


Journal:
2021-07-30:     Create egi_trimesh.h
2021-07-31:
	1. Add E3D_TriMesh::cloneMesh();
	2. ADD readObjFileInfo()
2021-08-02:
	1. Add E3D_TriMesh::E3D_TriMesh(const char *fobj)
2021-08-03:
	1. Improve E3D_TriMesh::E3D_TriMesh(const char *fobj).
2021-08-04:
	1. Add E3D_TriMesh::updateAllTriNormals(), test to display normals.
	2. Add E3D_TriMesh::renderMesh(FBDEV *fbdev)
	2. Add E3D_TriMesh::transformTriNormals()
	3. Add E3D_TriMesh::reverseAllTriNormals()
	4. Add E3D_TriMesh::transformMesh()
2021-08-05:
	1. Add E3D_TriMesh(const E3D_TriMesh &smesh, int n, float r);
	   to create regular_80_aspects solid ball.
2021-08-07:
	1. E3D_TriMesh(const char *fobj):
	   1.1 If A face data line uses '/' as delim, and ends with a SPACE ' '
	   	, it would be parsed as a new vertex!!!  ---OK, corrected!
	   	example: f 3/3/3 2/2/2 5/5/5 ' '.
	   1.2 To subdivide a 4_edge_polygon to TWO triangles correctly!
	2. Add updateVtxsCenter().
	3. Add moveVtxsCenterToOrigin()
	4. E3D_TriMesh::renderMesh(): A simple Zbuff test.
	   E3D_TriMesh::drawMeshWire(): A simple zbuff test.
2021-08-10:
	1. Rename: transformTriNormals() TO transformAllTriNormals()!
	2. Add E3D_TriMesh::renderMesh(FBDEV *fbdev, const E3D_RTMatrix & ViewRTmatrix);
2021-08-11:
	1. Add E3D_TriMesh::AABB class, and its functions.
	2. Add E3D_TriMesh::drawAABB(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix);
	3. Apply ScaleMatrix for transformMesh() and renderMesh().
2021-08-13:
	1. Correct transform matrixces processing, let Caller to flip
	   Screen_Y with fb_position_rotate().
2021-08-15:
	1. TEST_PERSPECTIVE for:
	   E3D_TriMesh:: drawMeshWire(FBDEV *fbdev)/drawAABB()/renderMesh(FBDEV *fbdev)
2021-08-16:
	1. drawMeshWire(FBDEV *fbdev): roundf for pts[].
2021-08-17:
	1. Modify: E3D_TriMesh::reverseAllTriNormals(bool reverseVtxIndex);
	2. Add: E3D_TriMesh::reverseAllVtxZ()
2021-08-18:
	1. Modify E3D_TriMesh::E3D_TriMesh(const char *fobj): To parse max.64 vertices in a face.
	2. Add  E3D_TriMesh::float AABBdSize()
	3. Add  E3D_TriMesh::projectPoints()
			--- Apply Projection Matrix ---
	4. Modify E3D_TriMesh::renderMesh() with ProjMatrix.
	5. Modify E3D_TriMesh::drawAABB() with ProjMatrix
2021-08-19:
	1. Add E3D_TriMesh::moveAabbCenterToOrigin().
2021-08-20:
	1. Function projectPoints() NOT an E3D_TriMesh member.
	2. Add E3D_draw_line() E3D_draw_circle() form e3d_vector.h
	3. Add E3D_draw_coordNavSphere()
	4. Add E3D_draw_line(FBDEV *, const E3D_Vector &, const E3D_RTMatrix &, const E3D_ProjMatrix &)

Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------*/
#ifndef __E3D_TRIMESH_H__
#define __E3D_TRIMESH_H__

#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include "egi_debug.h"
#include "e3d_vector.h"
#include "egi.h"
#include "egi_fbgeom.h"
#include "egi_color.h"

using namespace std;


#define TEST_PERSPECTIVE  	1
#define VPRODUCT_EPSILON	(1.0e-6)  /* -6, Unit vector vProduct=vct1*vtc2 */

int readObjFileInfo(const char* fpath, int & vertexCount, int & triangleCount,
				        int & normalCount, int & textureCount, int & faceCount);


////////////////////////////////////  E3D_Draw Function  ////////////////////////////////////////
void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb);
void E3D_draw_circle(FBDEV *fbdev, int r, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);


/* Global light vector */
E3D_Vector gv_vLight(0, 0, 1);

/* Global View TransformMatrix */

/*-------------------------------------------------------
        Class: E3D_TriMesh

Refrence:
"3D Math Primer for Graphics and Game Development"
                        by Fletcher Dunn, Ian Parberry

-------------------------------------------------------*/
class E3D_TriMesh {
public:
	/* :: Class Vertex */
	class Vertex {
	public:
		/* Constructor */
		Vertex() { setDefaults(); };

		/* Vector as point */
		E3D_Vector pt;

		/* Ref Coord. */
		float	u,v;

		/* Normal vector */
		E3D_Vector normal;

		/* Mark number */
		int mark;

		/* Functions */
		void setDefaults();	/* Set default/initial values */
	};


	/* :: Class Triangle */
	class Triangle {
	public:
		/* Constructor */
		Triangle() { setDefaults(); };

		/* Sturct Vertx */
		struct Vertx {
			int   index;	/* index as of */
			float u,v;	/* Ref coord. */
		};
		Vertx vtx[3];		/* 3 points to form a triangle */

		/* Normal */
		E3D_Vector normal;

		/* int part;  int material; */

		/* Mark number */
		int mark;

		/* Functions */
		void setDefaults();			/* Set default/initial values */
		bool isDegenerated() const;		/* If degenerated into a line, or a point */
		int  findVertex(int vtxIndex) const;    /* If vtxList[vtxIndex] is point of the triangle.
							 * Then return 0,1 or 2 as of index of Triangel.vtx[].
							 * Else return -1.
							 */
	};

	/* :: Class Material */

	/* :: Class OptimationParameters */

	/* :: Class AABB (axially aligned bounding box */
	class AABB {
	public:
		E3D_Vector vmin;
		E3D_Vector vmax;
		void print() {
			egi_dpstd("AABDD: vmin{%f,%f,%f}, vmax{%f,%f,%f}.\n",
						vmin.x,vmin.y,vmin.z, vmax.x,vmax.y,vmax.z);
		}
		float xSize() const { return vmax.x -vmin.x; }
		float ySize() const { return vmax.y -vmin.y; }
		float zSize() const { return vmax.z -vmin.z; }
		void toHoldPoint(const E3D_Vector &vpt);		/* Revise vmin/vmax to make AABB contain the point. */
	};

	/* Constructor E3D_TriMesh() */
	E3D_TriMesh();
	E3D_TriMesh(const E3D_TriMesh &tmesh);
	E3D_TriMesh(int nv, int nt);
	E3D_TriMesh(const char *fobj);
	E3D_TriMesh(const E3D_TriMesh &smesh, float r);

	/* Destructor ~E3D_TriMesh() */
	~E3D_TriMesh();

	/* Functions: Print for debug */
	void printAllVertices(const char *name);
	void printAllTriangles(const char *name);
	void printAABB(const char *name);

	/* TEST FUNCTIONS: */
	void setVtxCount(int cnt) { if(cnt>=0) vcnt=cnt; };
	void setTriCount(int cnt) { if(cnt>=0) tcnt=cnt; };

	/* Function: Count vertices and triangles */
	int vtxCount() const { return vcnt; };
	int triCount() const { return tcnt; };

	/* Function: Get AABB size */
	float AABBxSize() {
		return aabbox.xSize();
	}
	float AABBySize() {
		return aabbox.ySize();
	}
	float AABBzSize() {
		return aabbox.zSize();
	}
	float AABBdSize() {  /* diagonal size */
		float x=aabbox.xSize();
		float y=aabbox.ySize();
		float z=aabbox.zSize();
		return sqrt(x*x+y*y+z*z);
	}

	/* Function: Retrun ref. of indexed vertex */
	E3D_Vector & vertex(int vtxIndex) const;
	E3D_Vector vtxsCenter(void) const {
		return vtxscenter;
	}
	E3D_Vector aabbCenter(void) const {
		return 0.5*(aabbox.vmax-aabbox.vmin);
	}

	/* Function: Calculate center point of all vertices */
	void updateVtxsCenter()
	{
		float sumX=0;
		float sumY=0;
		float sumZ=0;
		for(int i=0; i<vcnt; i++) {
			sumX += vtxList[i].pt.x;
			sumY += vtxList[i].pt.y;
			sumZ += vtxList[i].pt.z;
		}

		vtxscenter.x = sumX/vcnt;
		vtxscenter.y = sumY/vcnt;
		vtxscenter.z = sumZ/vcnt;
	}
	/* Function: Move VtxsCenter to current origin. */
	void moveVtxsCenterToOrigin()
	{
		/* Update all vertex XYZ */
		for(int i=0; i<vcnt; i++) {
			vtxList[i].pt.x -=vtxscenter.x;
			vtxList[i].pt.y -=vtxscenter.y;
			vtxList[i].pt.z -=vtxscenter.z;
		}

		/* Update AABB */
		aabbox.vmax.x-=vtxscenter.x;
		aabbox.vmax.y-=vtxscenter.y;
		aabbox.vmax.z-=vtxscenter.z;
		aabbox.vmin.x-=vtxscenter.x;
		aabbox.vmin.y-=vtxscenter.y;
		aabbox.vmin.z-=vtxscenter.z;

		/* Reset vtxscenter */
		vtxscenter.x =0.0;
		vtxscenter.y =0.0;
		vtxscenter.z =0.0;
	}

	/* Function: Calculate AABB aabbox */
	void updateAABB(void) {
		/* Reset first */
		aabbox.vmin.zero();
		aabbox.vmax.zero();
		/* To hold all vertices */
		for( int i=0; i<vcnt; i++ )
			aabbox.toHoldPoint(vtxList[i].pt);
	}
	/* Function: Move VtxsCenter to current origin. */
	void moveAabbCenterToOrigin()
	{
		float xc=0.5*(aabbox.vmax.x+aabbox.vmin.x);
		float yc=0.5*(aabbox.vmax.y+aabbox.vmin.y);
		float zc=0.5*(aabbox.vmax.z+aabbox.vmin.z);

		for(int i=0; i<vcnt; i++) {
			vtxList[i].pt.x -=xc;
			vtxList[i].pt.y -=yc;
			vtxList[i].pt.z -=zc;
		}

		/* Move AABB */
		aabbox.vmax.x-=xc;
		aabbox.vmax.y-=yc;
		aabbox.vmax.z-=zc;

		aabbox.vmin.x-=xc;
		aabbox.vmin.y-=yc;
		aabbox.vmin.z-=zc;

		/* Move vtxscenter */
                vtxscenter.x -=xc;
                vtxscenter.y -=yc;
                vtxscenter.z -=zc;
	}

	/* Function: Add vertices into vtxList[] */
	int addVertex(const Vertex &v);
	int addVertexFromPoints(const E3D_POINT *pts, int n); /* Add veterx from points */
	void reverseAllVtxZ(void);  /* Reverse Z value/direction for all vertices, for coord system conversion */

	/* Function: Add triangle into triList[] */
	int addTriangle(const Triangle *t);
	int addTriangleFromVtxIndx(const int *vidx, int n);    /* To add/form triangles with vertex indexes list. */

	/* Function: scale up/down mesh/vertices */
	void scaleMesh(float scale);

	/* Function: Transform vertices/normals */
	void transformVertices(const E3D_RTMatrix  &RTMatrix);
	void transformAllTriNormals(const E3D_RTMatrix  &RTMatrix);
	void transformAABB(const E3D_RTMatrix  &RTMatrix);
	void transformMesh(const E3D_RTMatrix  &RTMatrix, const E3D_RTMatrix &ScaleMatrix); /* Vertices+TriNormals*/

	/* Function: Calculate normals for all triangles */
	void updateAllTriNormals();
	void reverseAllTriNormals(bool reverseVtxIndex);

	/* Function: clone vertices/trianges from the tmesh.  */
	void cloneMesh(const E3D_TriMesh &tmesh);

	/* Function: Project according to Projection matrix */
	// int  projectPoints(E3D_Vector vpts[], int np, const E3D_ProjMatrix &projMatrix) const;

	/* Function: Draw wires/faces */
	void drawMeshWire(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix) const ;
	void drawMeshWire(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix) const;

	/* Function: Draw AABB as wire. */
	void drawAABB(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix, const E3D_ProjMatrix projMatrix) const ;

	/* Function: Render all triangles */
	void renderMesh(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix) const ;
	void renderMesh(FBDEV *fbdev, const E3D_RTMatrix &VRTmatrix, const E3D_RTMatrix &ScaleMatrix) const;

private:
	/* All members inited at E3D_TriMesh() */
	E3D_Vector 	vtxscenter;	/* Center of all vertices, NOT necessary to be sAABB center! */

	#define		TRIMESH_GROW_SIZE	64	/* Growsize for vtxList and triList --NOT applied!*/
	int		vCapacity;	/* MemSpace capacity for vtxList[] */
	int		vcnt;		/* Vertex counter */
	Vertex  	*vtxList;	/* Vertex array */

	int		tCapacity;	/* MemSpace capacity for triList[] */
	int		tcnt;		/* Triangle counter  */
	Triangle	*triList;	/* Triangle array */

	AABB		aabbox;		/* Axially aligned bounding box */
	//int mcnt;
	//Material  *mList;

};


void E3D_TriMesh::Vertex::setDefaults()
{
	/* Necessary ? */
	u=0;
	v=0;
	E3D_TriMesh::Vertex::mark=0;
	/* E3D_Vector objs: default init as 0 */
}

void E3D_TriMesh::Triangle::setDefaults()
{
	/* Not necessary ? */
	E3D_TriMesh::Triangle::mark=0;

}



/* Revise vmin/vmax to make AABB contain the point. */
void E3D_TriMesh::AABB::toHoldPoint(const E3D_Vector &pt)
{
	if(pt.x<vmin.x) vmin.x=pt.x;
	if(pt.x>vmax.x) vmax.x=pt.x;
	if(pt.y<vmin.y) vmin.y=pt.y;
	if(pt.y>vmax.y) vmax.y=pt.y;
	if(pt.z<vmin.z) vmin.z=pt.z;
	if(pt.z>vmax.z) vmax.z=pt.z;
}


/*-------------------------------
     E3D_TriMesh  Constructor
--------------------------------*/
E3D_TriMesh::E3D_TriMesh()
{
	/* Init vtxList[] */
	vCapacity=TRIMESH_GROW_SIZE;
	vcnt=0;
	try {
		vtxList= new Vertex[TRIMESH_GROW_SIZE]; //(); /* Call constructor to setDefault */
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Vertex[]!\n");
		return;
	}

	/* Init triList[] */
	tCapacity=TRIMESH_GROW_SIZE;
	tcnt=0;
	try {
		triList= new Triangle[TRIMESH_GROW_SIZE]; //();
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Triangle[]!\n");
		return;
	}

}

/*--------------------------------------
     E3D_TriMesh  Constructor
 nv:	Init memspace for vtxList[].
 nt:    Init memspace for triList[]

	!!! WARNING !!!
     vcnt/tcnt reset as 0!
 --------------------------------------*/
E3D_TriMesh::E3D_TriMesh(int nv, int nt)
{
	/* Check input */
	if(nv<=0) nv=TRIMESH_GROW_SIZE;
	if(nt<=0) nt=TRIMESH_GROW_SIZE;

	/* Init vtxList[] */
	vCapacity=0;  /* 0 before alloc */
	try {
		vtxList= new Vertex[nv]; //();	/* Call constructor to setDefault. TODO: NOT necessary! also call constructor. */
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Vertex[]!\n");
		return;
	}
	vcnt=0;
	vCapacity=nv;


	/* Init triList[] */
	tCapacity=0;  /* 0 before alloc */
	try {
		triList= new Triangle[nt]; //();
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Triangle[]!\n");
		return;
	}
	tcnt=0;
	tCapacity=nt;

}

/*------------------------------------------
          E3D_TriMesh  Constructor

 To create by copying from another TriMesh
 -------------------------------------------*/
E3D_TriMesh::E3D_TriMesh(const E3D_TriMesh & tmesh)
{
	int nv=tmesh.vCapacity;
	int nt=tmesh.tCapacity;

	/* Init vtxList[] */
	vCapacity=0;  /* 0 before alloc */
	try {
		vtxList= new Vertex[nv]; //();	/* Call constructor to setDefault. TODO: NOT necessary! also call constructor. */
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Vertex[]!\n");
		return;
	}
	vcnt=tmesh.vcnt; /* !!! */
	vCapacity=nv;

	/* Init triList[] */
	tCapacity=0;  /* 0 before alloc */
	try {
		triList= new Triangle[nt]; //();
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Triangle[]!\n");
		return;
	}
	tcnt=tmesh.tcnt; /* !!! */
	tCapacity=nt;

	/* Copy vtxList */
	for(int i=0; i<vcnt; i++) {
		/* Vector as point */
		vtxList[i].pt = tmesh.vtxList[i].pt;

		/* Ref Coord. */
		vtxList[i].u = tmesh.vtxList[i].u;
		vtxList[i].v = tmesh.vtxList[i].v;

		/* Normal vector */
		vtxList[i].normal = tmesh.vtxList[i].normal;

		/* Mark number */
		vtxList[i].mark = tmesh.vtxList[i].mark;
	}

	/* Copy triList */
	for(int i=0; i<tcnt; i++) {
		/* Vertx vtx[3] */
		triList[i].vtx[0]=tmesh.triList[i].vtx[0];
		triList[i].vtx[1]=tmesh.triList[i].vtx[1];
		triList[i].vtx[2]=tmesh.triList[i].vtx[2];

		/* Normal */
		triList[i].normal=tmesh.triList[i].normal;

		/* int part;  int material; */

		/* Mark number */
		triList[i].mark=tmesh.triList[i].mark;
	}

}


/*------------------------------------------
          E3D_TriMesh  Constructor

To create by reading from a *.obj file.
@fobj:	Full path of a *.obj file.

Note:
1. A face has MAX.4 vertices!(a quadrilateral)
   and a quadrilateral will be stored as 2
   triangles.
--------------------------------------------*/
E3D_TriMesh::E3D_TriMesh(const char *fobj)
{
	ifstream	fin;
	char		ch;

        char *savept;
        char *savept2;
        char *pt, *pt2;
        char strline[1024];
        char strline2[1024];

	int		j,k;		    /* triangle index in a face, [0  MAX.3] */
        int             m;                  /* 0: vtxIndex, 1: texutreIndex 2: normalIndex  */
	float		xx,yy,zz;
	int		vtxIndex[64]={0};    	/* To store vtxIndex for MAX. 4x16_side/vertices face */
	int		textureIndex[64]={0};
	int		normalIndex[64]={0};

	int		vertexCount=0;
	int		triangleCount=0;
	int		normalCount=0;
	int		textureCount=0;
	int		faceCount=0;


	/* 1. Read statistic of obj data */
	if( readObjFileInfo(fobj, vertexCount, triangleCount, normalCount, textureCount, faceCount) !=0 )
		return;

	egi_dpstd("'%s' Statistics:  Vertices %d;  Normals %d;  Texture %d; Faces %d; Triangle %d.\n",
				fobj, vertexCount, normalCount, textureCount, faceCount, triangleCount );

	/* 2. Allocate vtxList[] and triList[] */
	/* 2.1 New vtxList[] */
	vCapacity=0;  /* 0 before alloc */
	try {
		vtxList= new Vertex[vertexCount];
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Vertex[]!\n");
		return;
	}
	vcnt=0;
	vCapacity=vertexCount;

	/* 2.2 New triList[] */
	tCapacity=0;  /* 0 before alloc */
	try {
		triList= new Triangle[triangleCount]; //();
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Triangle[]!\n");
		return;
	}
	tcnt=0;
	tCapacity=triangleCount;

	/* 3. Open obj file */
	fin.open(fobj);
	if(fin.fail()) {
		egi_dpstd("Fail to open '%s'.\n", fobj);
		return;
	}

	/* 4. Read data into TriMesh */
	while( fin.getline(strline, 1024-1) ) {
		ch=strline[0];

		switch(ch) {
		   case '#':	/* 0. Comments */
			break;
		   case 'v':	/* 1. Vertex data */
			ch=strline[1]; /* get second token */

			switch(ch) {
			   case ' ':  /* Read vertices X.Y.Z. */
				if(vcnt==vCapacity) {
					egi_dpstd("tCapacity used up!\n");
					goto END_FUNC;
				}

				sscanf( strline+2, "%f %f %f", &xx, &yy, &zz);
				vtxList[vcnt].pt.x=xx;
				vtxList[vcnt].pt.y=yy;
				vtxList[vcnt].pt.z=zz;

				#if 0 /* TEST: -------- */
				cout << "Vtx XYZ: ";
				cout << "x " << vtxList[vcnt].pt.x
				     << "y "<< vtxList[vcnt].pt.y
				     << "z "<< vtxList[vcnt].pt.z
				     << endl;
				#endif

				vcnt++;

				break;
			   case 't':  /* Read texture vertices, uv coordinates */
				/* TODO  */
				break;
			   case 'n':  /* Read vertex normals, (for vertices of each face ) */
				/* TODO */
				break;
			   case 'p': /* Parameter space vertices */

				break;

			   default:
				break;
			}

			break;

		   case 'f':	/* 2. Face data. vtxIndex starts from 1!!! */
			/* Change frist char 'f' to ' ' */
			strline[0] = ' ';

			/* Copy for test */
			strline[1024-1]=0;
			strline2[1024-1]=0;
			strncpy(strline2, strline, 1024-1);
			//cout<<"Face line: " <<strline2 <<endl;

		   	/* Case A. --- ONLY vtxIndex
		    	 * Example: f 61 21 44
		    	 */
			if( strstr(strline, "/")==NULL ) {
			   /* To extract face vertex indices */
			   pt=strtok(strline+1, " ");
			   for(k=0; pt!=NULL && k<64; k++) {
				vtxIndex[k]=atoi(pt) -1;
				pt=strtok(NULL, " ");
			   }

			   /* NOW: k is total number of vertices in this face. */

#if 0 ////////////////////////////////////////////////////////////////////////
			   /*  Assign to triangle vtxindex: triList[tcnt].vtx[i].index */
			   if( k>=3 ) {
				triList[tcnt].vtx[0].index=vtxIndex[0];
				triList[tcnt].vtx[1].index=vtxIndex[1];
				triList[tcnt].vtx[2].index=vtxIndex[2];
				tcnt++;
			   }
			   if( k==4 ) {  /* Divide into 2 triangles */
				triList[tcnt].vtx[0].index=vtxIndex[1];
				triList[tcnt].vtx[1].index=vtxIndex[2];
				triList[tcnt].vtx[2].index=vtxIndex[3];
				tcnt++;
			   }
#endif /////////////////////////////////////////////////////////////////////////
			   /* TODO:  textrueIndex[] and normalIndex[], value <0 as omitted.  */

			   /*  Assign to triangle vtxindex: triList[tcnt].vtx[i].index */
			   if( k>2 ) {
				/* To divide the face into triangles in way of a fan. */
				for( j=0; j<k-2; j++ ) {
					if(tcnt==tCapacity) {
						egi_dpstd("tCapacity used up!\n");
						goto END_FUNC;
					}

					triList[tcnt].vtx[0].index=vtxIndex[0];
	                                triList[tcnt].vtx[1].index=vtxIndex[j+1];
        	                        triList[tcnt].vtx[2].index=vtxIndex[j+2];
					tcnt++;
				}
			   }

			   #if 0 /* TEST:----- Print all vtxIndex[] */
			   egi_dpstd("vtxIndex[]:");
			   for(int i=0; i<k; i++)
				printf(" %d", vtxIndex[i]);
			   printf("\n");
			   #endif

		   	} /* End Case A. */

		   	/* Case B. --- NOT only vtxIndex, maybe ALSO with textureIndex,normalIndex, with delim '/',
		         * Example: f 69//47 68//47 43//47 52//47
		         */
		        else {    /* ELSE: strstr(strline, "/")!=NULL */

			   /* To replace "//" with "/0/", to avoid strtok() returns only once!
			    * However, it can NOT rule out conditions like "/0/5" and "5/0/",
			    * see following  "In case "/0/5" ..."
			    */
			   while( (pt=strstr(strline, "//")) ) {
				int len;
				len=strlen(pt+1)+1; /* len: from second '/' to EOF */
				memmove(pt+2, pt+1, len);
				*(pt+1) ='0'; /* insert '0' btween "//" */
			   }

			   /* To extract face vertex indices */
			   pt=strtok_r(strline+1, " ", &savept); /* +1 skip first ch */
			   for(k=0; pt!=NULL && k<4*6; k++) {
				//cout << "Index group: " << pt<<endl;
				/* If no more "/"... example: '2/3/4 5/6/4 34 ', cast last '34 '
				 * This must befor strtok_r(pt, ...)
				 */
				if( strstr(pt, "/")==NULL) {
					//printf("No '/' when k=%d\n", k);
					break;
				}

				/* Parse vtxIndex/textureIndex/normalIndex */
				pt2=strtok_r(pt, "/", &savept2);

				for(m=0; pt2!=NULL && m<3; m++) {
					//cout <<"pt2: "<<pt2<<endl;
					switch(m) {
					   case 0:
						vtxIndex[k]=atoi(pt2)-1;
						//if( vtxIndex[k]<0 )
						//egi_dpstd("!!!WARNING!!!\nLine '%s' parsed as vtxIdex[%d]=%d <0!\n",
						//					strline2, k, vtxIndex[k]);
						break;
					   case 1:
						textureIndex[k]=atoi(pt2)-1;
						break;
					   case 2:
						normalIndex[k]=atoi(pt2)-1;
					}


					/* Next param */
					pt2=strtok_r(NULL, "/", &savept2);
				}

				/* In case  "/0/5", -- Invalid, vtxIndex MUST NOT be omitted! */
				if(pt[0]=='/' ) {
					egi_dpstd("!!!WARNING!!!  vtxIndex omitted!\n");
					/* Need to reorder data from to vtxIndex/textureIndex(WRONG!, it's textureIndex/normalIndex)
					   vtxIndex/textureIndex/normalIndex(CORRECT!) */
					normalIndex[k]=textureIndex[k];
					textureIndex[k]=vtxIndex[k];
					vtxIndex[k]=-1;		/* vtxIndex omitted!!! */
				}
				/* In case "5/0/", -- m==2, normalIndex omitted!  */
				else if( m==2 ) { /* pt[0]=='/' also has m==2! */
					normalIndex[k]=0-1;
				}

				/* Get next indices */
				pt=strtok_r(NULL, " ", &savept);

			   } /* End for(k) */

			   /* NOW: k is total number of vertices in this face. */

#if 0 ////////////////////////////////////////////////////////////
			   /*  Assign to triangle vtxindex */
			   if( k>=3 ) {
				triList[tcnt].vtx[0].index=vtxIndex[0];
				triList[tcnt].vtx[1].index=vtxIndex[1];
				triList[tcnt].vtx[2].index=vtxIndex[2];
				tcnt++;
			   }
			   if( k==4 ) {  /* Divide into 2 triangles */
				triList[tcnt].vtx[0].index=vtxIndex[2];
				triList[tcnt].vtx[1].index=vtxIndex[3];
				triList[tcnt].vtx[2].index=vtxIndex[0];
				tcnt++;
			   }
#endif //////////////////////////////////////////////////////////////
			   /*  Assign to triangle vtxindex */
			   if( k>2 ) {
				/* To divide the face into triangles in way of a fan. */
				for( j=0; j<k-2; j++ ) {
					if(tcnt==tCapacity) {
						egi_dpstd("tCapacity used up!\n");
						goto END_FUNC;
					}

					triList[tcnt].vtx[0].index=vtxIndex[0];
	                                triList[tcnt].vtx[1].index=vtxIndex[j+1];
        	                        triList[tcnt].vtx[2].index=vtxIndex[j+2];
					tcnt++;
				}
			   }


			   /* TEST: ------ */
			  if( vtxIndex[0]<0 || vtxIndex[1]<0 ||vtxIndex[2]<0 || (k==4 && vtxIndex[3]<0 ) ) {

				cout<<"Face line: "<<strline2<<endl;
				printf("%s: k=%d has parsed vtxIndex <0! vtxIndex[0,1,2,3]: %d, %d, %d, %d \n",
				 	strline2, k, vtxIndex[0],vtxIndex[1],vtxIndex[2],vtxIndex[3] );
				//printf("!!!WARNING!!!\nLine '%s', has vtxIndex <0!\n", strline2);
				exit(0);
			  }

			   /* TODO:  textrueIndex[] and normalIndex[], value <0 as omitted. */

			   #if 0 /* TEST: ------ Print all indices */
			   egi_dpstd("vtx/texture/normal Index:");
			   for(int i=0; i<k; i++)
				printf(" %d/%d/%d", vtxIndex[i], textureIndex[i], normalIndex[i]);
			   printf("\n");
			   #endif

			} /* End Case B. */

			break;

		   default:
			break;
		} /* End swith() */

	} /* End while() */

END_FUNC:
	/* Close file */
	fin.close();

	egi_dpstd("Finish reading obj file: %d Vertices, %d Triangels.\n", vcnt, tcnt);
}


/*------------------------
	Destructor
-------------------------*/
E3D_TriMesh:: ~E3D_TriMesh()
{
	delete [] vtxList;
	delete [] triList;

	egi_dpstd("TriMesh destructed!\n");
}


/*---------------------
Print out all Vertices.
---------------------*/
void E3D_TriMesh::printAllVertices(const char *name=NULL)
{
	printf("\n   <<< %s Vertices List >>>\n", name);
	if(vcnt<=0) {
		printf("No vertex in the TriMesh!\n");
		return;
	}

	for(int i=0; i<vcnt; i++) {
		printf("[%d]:  %f, %f, %f\n", i, vtxList[i].pt.x, vtxList[i].pt.y, vtxList[i].pt.z);
	}
}

/*------------------------
Print out all Triangels.
------------------------*/
void E3D_TriMesh::printAllTriangles(const char *name=NULL)
{
	printf("\n   <<< %s Triangle List >>>\n", name);
	if(tcnt<=0) {
		printf("No triangle in the TriMesh!\n");
		return;
	}

	for(int i=0; i<tcnt; i++) {
		/* Each triangle has 3 vertex index, as */
		printf("[%d]:  vtxIdx{%d, %d, %d}\n",
			i, triList[i].vtx[0].index, triList[i].vtx[1].index, triList[i].vtx[2].index);
	}
}

/*------------------------
Print out AABB.
------------------------*/
void E3D_TriMesh::printAABB(const char *name=NULL)
{
	printf("\n   <<< %s AABB >>>\n", name);
	printf(" vmin{%f, %f, %f}\n vmax{%f, %f, %f}\n",
			aabbox.vmin.x, aabbox.vmin.y, aabbox.vmin.z,
			aabbox.vmax.x, aabbox.vmax.y, aabbox.vmax.z );
}

/*-----------------------------------------
  Return refrence of the indexed vertex
------------------------------------------*/
E3D_Vector & E3D_TriMesh::vertex(int vtxIndex) const
{
	if(vtxIndex <0 || vtxIndex>vcnt-1) {
		egi_dpstd("Input vtxIndex out of range!\n");
		return *(E3D_Vector *)NULL;	/* TODO: assert ?! */
	}
	else
		return vtxList[vtxIndex].pt;
}


/*-------------------------------------------------------
Create vertexes with input points, and add into vtxList[]
in sequence.

 @pts:	Pointer to points array
 @n:	Number of points in pts[]

 Return:
	<0	Fails
	>=0	Number of vertexes successfully added.
--------------------------------------------------------*/
int E3D_TriMesh::addVertexFromPoints(const E3D_POINT *pts, int n)
{
	int i;

	if(pts==NULL || n<1)
		return -1;

	for(i=0; i<n; i++) {
		/* Check capacity */
		if( vCapacity <= vcnt ) {
			egi_dpstd("vCapacity <= vcnt! Memspace of vtxList[] NOT enough!\n");
			return i;
		}

		/* Just copy point XYZ */
		//vtxList[vcnt].setDefaults();
		vtxList[vcnt].pt.x=pts[i].x;
		vtxList[vcnt].pt.y=pts[i].y;
		vtxList[vcnt].pt.z=pts[i].z;

		/* Other params for the vertex,...as default. */

		/* Increment vctn */
		vcnt++;
	}

	return n;
}

/*----------------------------------------
Reverse Z value/direction for all vertices,
for Coord system conversion.
-----------------------------------------*/
void E3D_TriMesh::reverseAllVtxZ(void)
{
	for(int i=0; i<vcnt; i++)
		vtxList[i].pt.z = -vtxList[i].pt.z;
}

/*-----------------------------------------------------------
Create triangles with input vertex indexes, and add into
triList[] in sequence.

 @vidx:  Index of vertex as of vtxList[vidx].
	 MUST be at least 3*n vertexes in vidx[]

 @n:	 Total number of triangles to add.

 Return:
	<0	Fails
	>=0	Number of triangles successfully added.
------------------------------------------------------------*/
int E3D_TriMesh::addTriangleFromVtxIndx(const int *vidx, int n)
{
	int i;

	if(vidx==NULL || n<1)
		return -1;

	for(i=0; i<n; i++) {
		/* Check capacity */
		if( tCapacity <= tcnt ) {
			egi_dpstd("tCapacity <= tcnt! Memspace of triList[] NOT enough!\n");
			return i;
		}

		/* Assign 3 vertex index to triangle */
		triList[tcnt].vtx[0].index=vidx[3*i];
		triList[tcnt].vtx[1].index=vidx[3*i+1];
		triList[tcnt].vtx[2].index=vidx[3*i+2];

		/* Other params for the triangle,...as default. */

		/* Increment tctn */
		tcnt++;
	}

	return n;
}


/*--------------------------------------------------------------
Transform all vertices of the TriMesh.

@RTMatrix:	RotationTranslation Matrix
--------------------------------------------------------------*/
void E3D_TriMesh::transformVertices(const E3D_RTMatrix  &RTMatrix)
{
	float xx;
	float yy;
	float zz;

	/* TODO: Input RTMatrix Must be an orthogonal matrix. */

        /*** E3D_RTMatrix.pmat[]
         * Rotation Matrix:
                   pmat[0,1,2]: m11, m12, m13
                   pmat[3,4,5]: m21, m22, m23
                   pmat[6,7,8]: m31, m32, m33
         * Translation:
                   pmat[9,10,11]: tx,ty,tz
         */

	for( int i=0; i<vcnt; i++) {
		xx=vtxList[i].pt.x;
		yy=vtxList[i].pt.y;
		zz=vtxList[i].pt.z;

		vtxList[i].pt.x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6] +RTMatrix.pmat[9];
		vtxList[i].pt.y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7] +RTMatrix.pmat[10];
		vtxList[i].pt.z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8] +RTMatrix.pmat[11];
	}

}

/*--------------------------------------------------------------
Transform all triangle normals. NO translation component!

@RTMatrix:	RotationTranslation Matrix
----------------------------------------------------------------*/
void E3D_TriMesh::transformAllTriNormals(const E3D_RTMatrix  &RTMatrix)
{
	float xx;
	float yy;
	float zz;

	for( int j=0; j<tcnt; j++) {
		xx=triList[j].normal.x;
		yy=triList[j].normal.y;
		zz=triList[j].normal.z;
		triList[j].normal.x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6]; /* Ignor translation!!! */
		triList[j].normal.y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7];
		triList[j].normal.z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8];
	}

}

/*-----------------------------------------------------------
Transform AABB (axially aligned bounding box).
	  		!!! CAUTION !!!
1. The function ONLY transform the ORIGINAL two Vectors(vmin/vmax)
   that  defined the AABB.

@RTMatrix:	RotationTranslation Matrix
-----------------------------------------------------------*/
void E3D_TriMesh::transformAABB(const E3D_RTMatrix  &RTMatrix)
{
#if 0
	float xx;
	float yy;
	float zz;

	xx=aabbox.vmin.x;
	yy=aabbox.vmin.y;
	zz=aabbox.vmin.z;
	aabbox.vmin.x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6] +RTMatrix.pmat[9];
	aabbox.vmin.y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7] +RTMatrix.pmat[10];
	aabbox.vmin.z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8] +RTMatrix.pmat[11];

	xx=aabbox.vmax.x;
	yy=aabbox.vmax.y;
	zz=aabbox.vmax.z;
	aabbox.vmax.x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6] +RTMatrix.pmat[9];
	aabbox.vmax.y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7] +RTMatrix.pmat[10];
	aabbox.vmax.z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8] +RTMatrix.pmat[11];
#else

	aabbox.vmin =aabbox.vmin*RTMatrix;
	aabbox.vmax =aabbox.vmax*RTMatrix;
#endif
}

/*--------------------------------------------------------------
Transform all vertices, all triangle normals and AABB

@RTMatrix:	RotationTranslation Matrix
@ScaleMatrix:	ScaleMatrix
----------------------------------------------------------------*/
void E3D_TriMesh::transformMesh(const E3D_RTMatrix  &RTMatrix, const E3D_RTMatrix &ScaleMatrix)
{
	E3D_RTMatrix  cmatrix=RTMatrix*ScaleMatrix;

	transformVertices(cmatrix);
	transformAllTriNormals(RTMatrix); /* normals should NOT apply scale_matrix ! */
	transformAABB(cmatrix);

	/*transform vtxscenter */
	vtxscenter = vtxscenter*cmatrix;

//	cmatrix.print("cmatrix");
}

/*-----------------------------------------
Scale up/down the whole mesh.
-----------------------------------------*/
void E3D_TriMesh::scaleMesh(float scale)
{
	for( int i=0; i<vcnt; i++) {
		vtxList[i].pt.x *= scale;
		vtxList[i].pt.y *= scale;
		vtxList[i].pt.z *= scale;
	}

	/* Scale AABB accrodingly */
	aabbox.vmin *= scale;
	aabbox.vmax *= scale;
}

/*------------------------------------
Calculate normals for all triangles
------------------------------------*/
void E3D_TriMesh::updateAllTriNormals()
{
	E3D_Vector  v01, v12;
	E3D_Vector  normal;
	int vtxIndex[3];

	for(int i=0; i<tcnt; i++) {
	   for(int j=0; j<3; j++) {
		vtxIndex[j]=triList[i].vtx[j].index;
		if(vtxIndex[j]<0) {
			egi_dpstd("!!!WARNING!!! triList[%d].vtx[%d] is <0!\n", i,j);
			vtxIndex[j]=0;
		}
	   }

	   v01=vtxList[vtxIndex[1]].pt-vtxList[vtxIndex[0]].pt;
	   v12=vtxList[vtxIndex[2]].pt-vtxList[vtxIndex[1]].pt;

	   triList[i].normal=E3D_vector_crossProduct(v01,v12);
	   if( triList[i].normal.normalize() <0 ) {
		egi_dpstd("Triangle triList[%d] error! or degenerated!\n", i);
		vtxList[vtxIndex[0]].pt.print();
		vtxList[vtxIndex[1]].pt.print();
		vtxList[vtxIndex[2]].pt.print();
	   }

   	}
}

/*------------------------------------
Reverse normals of all triangles
------------------------------------*/
void E3D_TriMesh::reverseAllTriNormals(bool reverseVtxIndex)
{
	int td;
	for(int i=0; i<tcnt; i++) {
	   /* Reverse normal */
	   triList[i].normal *= -1.0f;

	   /*  Reverse vtx index */
	   if(reverseVtxIndex) {
		triList[i].vtx[0].index=td;
		triList[i].vtx[0].index=triList[i].vtx[2].index;
		triList[i].vtx[2].index=td;
    	   }
	}
}


/*--------------------------------------------------
Clone all vertices and triangles from the tmesh
, to replace old data.
-------------------------------------------------*/
void E3D_TriMesh::cloneMesh(const E3D_TriMesh &tmesh)
{
	/* Check Memspace */
	if( tmesh.vcnt > vCapacity ) {
		egi_dpstd("tmesh.vcnt > vCapacity! vCapacity is NOT enough!\n");
		return;
	}
	if( tmesh.tcnt > tCapacity ) {
		egi_dpstd("tmesh.tcnt > tCapacity! tCapacity is NOT enough!\n");
		return;
	}

	/* Clone vtxList[] */
	vcnt =tmesh.vcnt;
	for(int i=0; i<vcnt; i++) {
		/* Vector as point */
		vtxList[i].pt = tmesh.vtxList[i].pt;

		/* Ref Coord. */
		vtxList[i].u = tmesh.vtxList[i].u;
		vtxList[i].v = tmesh.vtxList[i].v;

		/* Normal vector */
		vtxList[i].normal = tmesh.vtxList[i].normal;

		/* Mark number */
		vtxList[i].mark = tmesh.vtxList[i].mark;
	}

	/* Clone triList[] */
	tcnt =tmesh.tcnt;
	for(int i=0; i<tcnt; i++) {
		/* Vertx vtx[3] */
		triList[i].vtx[0]=tmesh.triList[i].vtx[0];
		triList[i].vtx[1]=tmesh.triList[i].vtx[1];
		triList[i].vtx[2]=tmesh.triList[i].vtx[2];

		/* Normal */
		triList[i].normal=tmesh.triList[i].normal;

		/* int part;  int material; */

		/* Mark number */
		triList[i].mark=tmesh.triList[i].mark;
	}

	/* Clone AABB */
	aabbox=tmesh.aabbox;

	/* Clone vtxscenter */
	vtxscenter=tmesh.vtxscenter;
}


/*--------------------------------------------------------------
Project an array of 3D vpts according to the projection matrix,
and all vpts[] will be modified after calling.

Note:
1. Values of all vpts[] will be updated/projected!

@Tri:		Input triangle.
@np:		Number of points.
@projMatrix:	The projectio matrix.

Return:
	0	OK
	>0 	Point out of the viewing frustum!
	<0	Fails
---------------------------------------------------------------*/
int projectPoints(E3D_Vector vpts[], int np, const E3D_ProjMatrix & projMatrix)
{
	/* 0. Check input */
	if(np<1)
	   return -1;

	/* 1. If Isometric projection */
	if( projMatrix.type==0 ) {
		/* Just adjust viewplane origin to Window center */
		for(int k=0; k<np; k++) {
			vpts[k].x = vpts[k].x+projMatrix.winW/2;
			vpts[k].y = vpts[k].y+projMatrix.winH/2;
		}
		return 0;
	}

        /* 2. Projecting point onto the viewPlane/screen  */
        for(int k=0; k<np; k++) {
		/* Check */
		if( abs(vpts[k].z) < VPRODUCT_EPSILON ) {
			egi_dpstd("Some points out of the viewing frustum!\n");
			return 1;
		}

         	vpts[k].x = roundf( vpts[k].x/vpts[k].z*projMatrix.dv  /* Xv=Xs/Zs*d */
                                    +projMatrix.winW/2 );              /* Adjust viewplane origin to window X center */

         	vpts[k].y = roundf( vpts[k].y/vpts[k].z*projMatrix.dv  /* Xv=Xs/Zs*d */
                                    +projMatrix.winH/2 );              /* Adjust viewplane origin to window Y center */
		//Keep vpts[k].z
	}

	/* 3. Clip if out of frustum. plan view_z==0 */
	for( int i=0; i<np; i++) {
		if( vpts[i].z < projMatrix.dv)
			return 1;
	}

	return 0;
}


/*-----------------------------------------------------
Draw mesh wire.
fbdev:		Pointer to FBDEV
projMatrix:	Projection Matrix

View direction: TriMesh Global_Coord. Z axis.
-----------------------------------------------------*/
void E3D_TriMesh::drawMeshWire(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix) const
{
	int vtidx[3];	    /* vtx index of a triangle */
	EGI_POINT   pts[3];
	E3D_Vector  vpts[3];
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); /* View direction */
	float	    vProduct;   /* dot product */

	/* NOTE: Set fbdev->flipZ, as we view from -z---->+z */
	fbdev->flipZ=true;

	/* TEST: Project to Z plane, Draw X,Y only */
	for(int i=0; i<tcnt; i++) {

		/* 1. To pick out back faces.  */
		/* Note: For perspective view, this is NOT always correct!
		 * XXX A triangle visiable in ISOmetric view maynot NOT visiable in perspective view! XXX
		 */
		vProduct=vView*(triList[i].normal); // *(-1.0f); // -vView as *(-1.0f);
		if( vProduct > -VPRODUCT_EPSILON ) {  /* >=0.0f */
			//egi_dpstd("triList[%d] vProduct=%f >=0 \n",i, vProduct);
                        continue;
		}
		//egi_dpstd("triList[%d] vProduct=%f \n",i, vProduct);

		/* 2. Get vtxindex */
		vtidx[0]=triList[i].vtx[0].index;
		vtidx[1]=triList[i].vtx[1].index;
		vtidx[2]=triList[i].vtx[2].index;

		/* 3. Copy triangle vertices, and project to viewPlan COORD. */
		vpts[0]=vtxList[vtidx[0]].pt;
		vpts[1]=vtxList[vtidx[1]].pt;
		vpts[2]=vtxList[vtidx[2]].pt;

		if( projectPoints(vpts, 3, projMatrix) !=0 )
			continue;

		/* 4. ---------TEST: Set pixz zbuff. Should init zbuff[] to INT32_MIN: -2147483648 */
		/* A simple ZBUFF test: Triangle center point Z for all pixles on the triangle.  TODO ....*/
		gv_fb_dev.pixz=roundf((vpts[0].z+vpts[1].z+vpts[2].z)/3.0);

		/* 5. Draw line one View plan, OR draw_line_antialias() */
		for(int k=0; k<3; k++)
			draw_line(fbdev, roundf(vpts[k].x), roundf(vpts[k].y),
					 roundf(vpts[(k+1)%3].x), roundf(vpts[(k+1)%3].y) );

		#if 0 /* TEST: draw triangle normals -------- */
		E3D_POINT cpt;	    /* Center of triangle */
		float nlen=20.0f;   /* Normal line length */
		cpt.x=(vtxList[vtidx[0]].pt.x+vtxList[vtidx[1]].pt.x+vtxList[vtidx[2]].pt.x)/3.0f;
		cpt.y=(vtxList[vtidx[0]].pt.y+vtxList[vtidx[1]].pt.y+vtxList[vtidx[2]].pt.y)/3.0f;
		cpt.z=(vtxList[vtidx[0]].pt.z+vtxList[vtidx[1]].pt.z+vtxList[vtidx[2]].pt.z)/3.0f;

		draw_line(fbdev,cpt.x, cpt.y, cpt.x+nlen*(triList[i].normal.x), cpt.y+nlen*(triList[i].normal.y) );
		#endif
	}

	/* Reset fbdev->flipZ */
	fbdev->flipZ=false;
}

/*---------------------------------------------------------------------
Draw mesh wire.

Note:
1. View direction: from -z ----> +z, of Veiw_Coord Z axis.
2. Each triangle in mesh is transformed as per VRTMatrix, and then draw
   to FBDEV, TriMesh data keeps unchanged!
3. !!! CAUTION !!!
   Normals will also be scaled here if VRTMatrix has scale matrix combined!

@fbdev:		Pointer to FB device.
@VRTmatrix:	View_Coord transform matrix, from same as Global_Coord
		to expected view position. (Combined with ScaleMatrix)
		OR: --- View_Coord relative to Global_Coord ---.
-----------------------------------------------------------------------*/
void E3D_TriMesh::drawMeshWire(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix) const
{
	E3D_Vector  vView(0.0f, 0.0f, 1.0f);    /* View direction -z--->+z */
	float	    vProduct;   		/* dot product */

	/* For EACH traversing triangle */
	int vtidx[3];  		/* vtx index of a triangle, as of triList[].vtx[0~2].index */
	E3D_Vector  trivtx[3];	/* 3 vertices of transformed triangle. */
	E3D_Vector  trinormal;  /* Normal of transformed triangle */

	/* NOTE: Set fbdev->flipZ, as we view from -z---->+z */
	fbdev->flipZ=true;

	/* TEST: Project to Z plane, Draw X,Y only */
	for(int i=0; i<tcnt; i++) {
		/* Transform triangle vertices to under View_Coord */
		for(int k=0; k<3; k++) {
			/* Get indices of triangle vertices */
			vtidx[k]=triList[i].vtx[k].index;
			/* To transform vertices to under View_Coord */
			//trivtx[k] = -(vtxList[vtidx[k]].pt*VRTMatrix); /* -1, to flip XYZ to fit Screen coord XY */
			trivtx[k] = vtxList[vtidx[k]].pt*VRTMatrix;
		}

		/* Transform triangle normal. !!! WARNING !!! normal is also scaled here if VRTMatrix has scalematrix combined! */
		//trinormal = -(triList[i].normal*VRTMatrix); /* -1, same as above trivtx[] to flip  */
		trinormal = triList[i].normal*VRTMatrix;

#if 1		/* To pick out back faces.  */
		vProduct=vView*trinormal; /* -vView as *(-1.0f), -z ---> +z */
		if ( vProduct >= 0.0f )
                        continue;
#endif

		/* 5. ---------TEST: Set pixz zbuff. Should init zbuff[] to INT32_MIN: -2147483648 */
		/* A simple ZBUFF test: Triangle center point Z for all pixles on the triangle.  TODO ....*/
		gv_fb_dev.pixz=roundf((trivtx[0].z+trivtx[1].z+trivtx[2].z)/3.0);

		/* !!!! Views from -z ----> +z */
		// gv_fb_dev.pixz = -gv_fb_dev.pixz; /* OR set fbdev->flipZ=true */

		/* Draw line: OR draw_line_antialias() */
		draw_line(fbdev, trivtx[0].x, trivtx[0].y, trivtx[1].x, trivtx[1].y);
		draw_line(fbdev, trivtx[1].x, trivtx[1].y, trivtx[2].x, trivtx[2].y);
		draw_line(fbdev, trivtx[0].x, trivtx[0].y, trivtx[2].x, trivtx[2].y);

		#if 0 /* TEST: draw triangle normals -------- */
		E3D_POINT cpt;	    /* Center of triangle */
		float nlen=20.0f;   /* Normal line length */
		cpt.x=(trivtx[0].x+trivtx[1].x+trivtx[2].x)/3.0f;
		cpt.y=(trivtx[0].y+trivtx[1].y+trivtx[2].y)/3.0f;
		cpt.z=(trivtx[0].z+trivtx[1].z+trivtx[2].z)/3.0f;
		trinormal.normalize();
		draw_line(fbdev, cpt.x, cpt.y, cpt.x+nlen*(trinormal.x), cpt.y+nlen*(trinormal.y) );
		#endif
	}

	/* Reset fbdev->flipZ */
	fbdev->flipZ=false;
}


/*------------------------------------------------------------------
Draw AABB of the mesh as wire.

Note:
1. View direction: from -z ----> +z, of Veiw_Coord Z axis.
2.		!!! CAUTION !!!
  This functions works ONLY WHEN original AABB is aligned with
  the global_coord! otherwise is NOT possible to get vpt[8]!

@fbdev:		Pointer to FB device.
@VRTmatrix:	View_Coord transform matrix, from same as Global_Coord
		to expected view position.
		OR: --- View_Coord relative to Global_Coord ---.
---------------------------------------------------------------------*/
void E3D_TriMesh::drawAABB(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix, const E3D_ProjMatrix projMatrix) const
{
	E3D_Vector vctMin;
	E3D_Vector vctMax;
	E3D_Vector vpts[8];

	float xsize=aabbox.xSize();
	float ysize=aabbox.ySize();
	float zsize=aabbox.zSize();

	/* Get all vertices of the AABB */
	vpts[0]=aabbox.vmin;
	vpts[1]=vpts[0]; vpts[1].x +=xsize;
	vpts[2]=vpts[1]; vpts[2].y +=ysize;
	vpts[3]=vpts[0]; vpts[3].y +=ysize;

	vpts[4]=vpts[0]; vpts[4].z +=zsize;
	vpts[5]=vpts[4]; vpts[5].x +=xsize;
	vpts[6]=vpts[5]; vpts[6].y +=ysize;
	vpts[7]=vpts[4]; vpts[7].y +=ysize;

	/* 2. Transform vpt[] */
	for( int k=0; k<8; k++)
		vpts[k]= vpts[k]*VRTMatrix;

	/* 3. Project to viewPlan COORD. */
        projectPoints(vpts, 8, projMatrix); /* Do NOT check clipping */

#if 0//////////////////////////////////////
	#if TEST_PERSPECTIVE  /* TEST: --Perspective matrix processing.
	        * Note: without setTranslation(x,y,.), Global XY_origin and View XY_origin coincide!
		*/
		if( abs(vpts[k].z) <1.0f ) {
			egi_dpstd("Point Z too samll, out of the Frustum?\n");
			return;
		}

		vpts[k].x = vpts[k].x/vpts[k].z*500  /* Xv=Xs/Zs*d, d=300 */
			   +160;	       /* Adjust to FB/Screen X center */
		vpts[k].y = vpts[k].y/vpts[k].z*500  /* Yv=Ys/Zs*d */
			   +120;  	       /* Adjust to FB/Screen Y center */
		/* keep vpt[k].z */
	#endif
	}
#endif /////////////////////////////////////

	/* Notice: Viewing from z- --> z+ */
	fbdev->flipZ=true;

	/* E3D_draw lines 0-1, 1-2, 2-3, 3-0 */
	E3D_draw_line(fbdev, vpts[0], vpts[1]);
	E3D_draw_line(fbdev, vpts[1], vpts[2]);
	E3D_draw_line(fbdev, vpts[2], vpts[3]);
	E3D_draw_line(fbdev, vpts[3], vpts[0]);
	/* Draw lines 4-5, 5-6, 6-7, 7-4 */
	E3D_draw_line(fbdev, vpts[4], vpts[5]);
	E3D_draw_line(fbdev, vpts[5], vpts[6]);
	E3D_draw_line(fbdev, vpts[6], vpts[7]);
	E3D_draw_line(fbdev, vpts[7], vpts[4]);
	/* Draw lines 0-4, 1-5, 2-6, 3-7 */
	E3D_draw_line(fbdev, vpts[0], vpts[4]);
	E3D_draw_line(fbdev, vpts[1], vpts[5]);
	E3D_draw_line(fbdev, vpts[2], vpts[6]);
	E3D_draw_line(fbdev, vpts[3], vpts[7]);

	/* Reset flipZ */
	fbdev->flipZ=false;
}

/*--------------------------------------------------
Render/draw the whole mesh by filling all triangles
with FBcolor.
View_Coord:	Same as Global_Coord.
View direction: View_Coord Z axis.


Note:
1.

@fbdev:			Pointer to FB device.
@projectMatrix:		Projection matrix.
--------------------------------------------------*/
#if 0 //////////////////////////////////////////////////
void E3D_TriMesh::renderMesh(FBDEV *fbdev) const
{
	int vtidx[3];  	/* vtx index of a triangel */
	EGI_POINT   pts[3];
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); 	/* View direction */
	float	    vProduct;   		/* dot product */
	Triangle    Tri;

	/* Default color */
	EGI_16BIT_COLOR color=fbdev->pixcolor;
	//EGI_8BIT_CCODE codeY=egi_color_getY(color);

	/* -------TEST: Project to Z plane, Draw X,Y only */
	/* Traverse and render all triangles */
	for(int i=0; i<tcnt; i++) {
		vtidx[0]=triList[i].vtx[0].index;
		vtidx[1]=triList[i].vtx[1].index;
		vtidx[2]=triList[i].vtx[2].index;

	   #if TEST_PERSPECTIVE  /* TEST: ---Clip triangles out of frustum. plan view_z==0 */
		if( vtxList[vtidx[0]].pt.z <500 || vtxList[vtidx[1]].pt.z <500 || vtxList[vtidx[2]].pt.z <500 )
			continue;
	   #endif

		/* 1. Pick out back_facing triangles.  */
		vProduct=vView*(triList[i].normal); // *(-1.0f); // -vView as *(-1.0f);
		/* Note: Because of float precision limit, vProduct==0.0f is NOT possible. */
		if ( vProduct > -VPRODUCT_EPSILON ) {  /* >=0.0f */
			//egi_dpstd("triList[%d] vProduct=%e >=0 \n",i, vProduct);
	                continue;
		}
		//egi_dpstd("triList[%d] vProduct=%f \n",i, vProduct);

		/* 2. Calculate light reflect strength:  TODO: not correct, this is ONLY demo.  */
		vProduct=gv_vLight*(triList[i].normal);  // *(-1.0f); // -vLight as *(-1.0f);
		//if( vProduct >= 0.0f )
		if( vProduct > -VPRODUCT_EPSILON )
			vProduct=0.0f;
		else /* Flip to get vProduct absolute value for luma */
			vProduct=-vProduct;

		/* 3. Adjust luma for pixcolor as per vProduct. */
		//cout <<"vProduct: "<< vProduct;
		fbdev->pixcolor=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
		//cout <<" getY: " << (unsigned int)egi_color_getY(fbdev->pixcolor) << endl;

		/* 4. Get triangle pts. */
		for(int k=0; k<3; k++) {
		   #if TEST_PERSPECTIVE  /* TEST: --Perspective matrix processing.
		        * Note: If no setTranslation(x,y,.) Global XY_origin and View XY_origin coincide!
			*       Here to adjust/map Global origin to center of View window center.
			*       However vanshing point....
			*/
			if(vtxList[vtidx[k]].pt.z <1.0f ) {
				egi_dpstd("Point Z too samll, out of the Frustum?\n");
				return;
			}
			pts[k].x = roundf( vtxList[vtidx[k]].pt.x/vtxList[vtidx[k]].pt.z*500  /* Xv=Xs/Zs*d, d=300 */
				   +160 );				/* Adjust to FB/Screen X center */
			pts[k].y = roundf( vtxList[vtidx[k]].pt.y/vtxList[vtidx[k]].pt.z*500  /* Yv=Ys/Zs*d */
				   +120 );				/* Adjust to FB/Screen Y center */
//			pts[k].z = Zfar*(1.0f-Znear/pts[k].z)/(Zfar-Znear);
		  #else
			/* 4.1 Get pts[].x/y */
			pts[k].x=roundf( vtxList[vtidx[k]].pt.x );
			pts[k].y=roundf( vtxList[vtidx[k]].pt.y );
		  #endif
		}

		/* 5. ---------TEST: Set pixz zbuff. Should init zbuff[] to INT32_MIN: -2147483648 */
		/* A simple test: Triangle center point Z for all pixles on the triangle.  TODO ....*/
		gv_fb_dev.pixz=roundf((vtxList[vtidx[0]].pt.z+vtxList[vtidx[1]].pt.z+vtxList[vtidx[2]].pt.z)/3.0);
		/* !!!! Views from -z ----> +z */
		gv_fb_dev.pixz = -gv_fb_dev.pixz;

		/* 6. Draw filled triangle */
		//draw_triangle(&gv_fb_dev, pts);
		draw_filled_triangle(&gv_fb_dev, pts);
	}

	/* Restore pixcolor */
	fbset_color2(&gv_fb_dev, color);
}
#else //////////////////////////////////////////////

void E3D_TriMesh::renderMesh(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix) const
{
	int vtidx[3];  	/* vtx index of a triangel */
	EGI_POINT   pts[3];
	E3D_Vector  vpts[3];
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); 	/* View direction */
	float	    vProduct;   		/* dot product */
	//Triangle    Tri;

//	E3D_ProjMatrix projMatrix={ .type=1, .dv=500, .dnear=500, .dfar=10000000, .winW=320, .winH=240};

	/* Default color */
	EGI_16BIT_COLOR color=fbdev->pixcolor;
	//EGI_8BIT_CCODE codeY=egi_color_getY(color);

	/* -------TEST: Project to Z plane, Draw X,Y only */
	/* Traverse and render all triangles */
	for(int i=0; i<tcnt; i++) {
		vtidx[0]=triList[i].vtx[0].index;
		vtidx[1]=triList[i].vtx[1].index;
		vtidx[2]=triList[i].vtx[2].index;

		/* 1. Pick out back_facing triangles.  */
		vProduct=vView*(triList[i].normal); // *(-1.0f); // -vView as *(-1.0f);
		/* Note: Because of float precision limit, vProduct==0.0f is NOT possible. */
		if ( vProduct > -VPRODUCT_EPSILON ) {  /* >=0.0f */
			//egi_dpstd("triList[%d] vProduct=%e >=0 \n",i, vProduct);
	                continue;
		}
		//egi_dpstd("triList[%d] vProduct=%f \n",i, vProduct);

		/* 2. Copy triangle vertices, and project to viewPlan COORD. */
		vpts[0]=vtxList[vtidx[0]].pt;
		vpts[1]=vtxList[vtidx[1]].pt;
		vpts[2]=vtxList[vtidx[2]].pt;

		if( projectPoints(vpts, 3, projMatrix) !=0 )
			continue;

		/* 3. Get 2D points */
		pts[0].x=vpts[0].x; pts[0].y=vpts[0].y;
		pts[1].x=vpts[1].x; pts[1].y=vpts[1].y;
		pts[2].x=vpts[2].x; pts[2].y=vpts[2].y;

		/* 4. Calculate light reflect strength:  TODO: not correct, this is ONLY demo.  */
		vProduct=gv_vLight*(triList[i].normal);  // *(-1.0f); // -vLight as *(-1.0f);
		//if( vProduct >= 0.0f )
		if( vProduct > -VPRODUCT_EPSILON )
			vProduct=0.0f;
		else /* Flip to get vProduct absolute value for luma */
			vProduct=-vProduct;

		/* 4. Adjust luma for pixcolor as per vProduct. */
		//cout <<"vProduct: "<< vProduct;
		fbdev->pixcolor=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
		//cout <<" getY: " << (unsigned int)egi_color_getY(fbdev->pixcolor) << endl;

		/* 6. ---------TEST: Set pixz zbuff. Should init zbuff[] to INT32_MIN: -2147483648 */
		/* A simple test: Triangle center point Z for all pixles on the triangle.  TODO ....*/
		//gv_fb_dev.pixz=roundf((vtxList[vtidx[0]].pt.z+vtxList[vtidx[1]].pt.z+vtxList[vtidx[2]].pt.z)/3.0);
		gv_fb_dev.pixz=roundf((vpts[0].z+vpts[1].z+vpts[2].z)/3.0);
		/* !!!! Views from -z ----> +z */
		gv_fb_dev.pixz = -gv_fb_dev.pixz;

		/* 6. Draw filled triangle */
		//draw_triangle(&gv_fb_dev, pts);
		draw_filled_triangle(&gv_fb_dev, pts);
	}

	/* Restore pixcolor */
	fbset_color2(&gv_fb_dev, color);
}
#endif



/*--------------------------------------------------------------------
Render/draw the whole mesh by filling all triangles with FBcolor.

Note:
1. View direction: from -z ----> +z, of Veiw_Coord Z axis.
2. Each triangle in mesh is transformed as per VRTMatrix+ScaleMatrix, and
   then draw to FBDEV, TriMesh data keeps unchanged!
3. !!! CAUTION !!!
   3.1 ScaleMatrix does NOT apply to triangle normals!
   3.2 Translation component in VRTMatrix is ignored for triangle normals!
       

View_Coord: 	Transformed from same as Global_Coord by RTmatrix.
View direction: Frome -z ---> +z, of View_Coord Z axis.

@fbdev:		Pointer to FB device.
@VRTmatrix	View_Coord tranform matrix, from same as Global_Coord.
		OR: View_Coord relative to Global_Coord.
		NOT combined with scaleMatrix here!
@ScaleMatrix    Scale Matrix.
---------------------------------------------------------------------*/
void E3D_TriMesh::renderMesh(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix, const E3D_RTMatrix &ScaleMatrix) const
{
	E3D_RTMatrix CompMatrix; /* Combined with VRTMatrix and ScaleMatrix */

        float xx;
       	float yy;
       	float zz;

	EGI_POINT   pts[3];

	E3D_Vector  vView(0.0f, 0.0f, 1.0f); /* View direction */
	float	    vProduct;   /* dot product */

	/* For EACH traversing triangle */
	int vtidx[3];  		/* vtx index of a triangle, as of triList[].vtx[0~2].index */
	E3D_Vector  trivtx[3];	/* 3 vertices of transformed triangle. */
	E3D_Vector  trinormal;  /* Normal of transformed triangle */

	/* Default color and Y */
	EGI_16BIT_COLOR color=fbdev->pixcolor;
	//EGI_8BIT_CCODE  codeY=egi_color_getY(color);

	/* Final transform Matrix for vertices */
	CompMatrix.identity();
	CompMatrix = VRTMatrix*ScaleMatrix;

	/* NOTE: Set fbdev->flipZ, as we view from -z---->+z */
	fbdev->flipZ=true;

	/* -------TEST: Project to Z plane, Draw X,Y only */
	/* Traverse and render all triangles */
	for(int i=0; i<tcnt; i++) {
		/* 1. Get indices of 3 vertices to define current triangle */
		vtidx[0]=triList[i].vtx[0].index;
		vtidx[1]=triList[i].vtx[1].index;
		vtidx[2]=triList[i].vtx[2].index;

		/* 2. Transform 3 vertices of the triangle to under View_Coord */
        	for( int k=0; k<3; k++) {
                	xx=vtxList[ vtidx[k] ].pt.x;
	                yy=vtxList[ vtidx[k] ].pt.y;
        	        zz=vtxList[ vtidx[k] ].pt.z;

			/* Get transformed trivtx[] under View_Coord, with scale matrix! */
			/* With Scale matrix */
                	trivtx[k].x = xx*CompMatrix.pmat[0]+yy*CompMatrix.pmat[3]+zz*CompMatrix.pmat[6] +CompMatrix.pmat[9];
	                trivtx[k].y = xx*CompMatrix.pmat[1]+yy*CompMatrix.pmat[4]+zz*CompMatrix.pmat[7] +CompMatrix.pmat[10];
        	        trivtx[k].z = xx*CompMatrix.pmat[2]+yy*CompMatrix.pmat[5]+zz*CompMatrix.pmat[8] +CompMatrix.pmat[11];

			#if TEST_PERSPECTIVE  /* Perspective matrix processing */
			if(trivtx[k].z <10) {
				egi_dpstd("Point Z too samll, out of the Frustum?\n");
				return;
			}
			trivtx[k].x = trivtx[k].x/trivtx[k].z*300;  /* Xv=Xs/Zs*d */
			trivtx[k].y = trivtx[k].y/trivtx[k].z*300;  /* Yv=Ys/Zs*d */
			#endif
        	}

		/* 3. Transforme normal of the triangle to under View_Coord */
		/* Note:
		 *	1. -1() to flip XYZ to fit Screen coord XY
		 *	2. Normal should NOT apply ScaleMatrix!!
		 *         If scaleX/Y/Z is NOT same,  then ALL normals to be recalcualted/updated!
	 	 */
		xx=triList[i].normal.x;
		yy=triList[i].normal.y;
		zz=triList[i].normal.z;
		trinormal.x = xx*VRTMatrix.pmat[0]+yy*VRTMatrix.pmat[3]+zz*VRTMatrix.pmat[6]; /* Ignor translation */
		trinormal.y = xx*VRTMatrix.pmat[1]+yy*VRTMatrix.pmat[4]+zz*VRTMatrix.pmat[7];
		trinormal.z = xx*VRTMatrix.pmat[2]+yy*VRTMatrix.pmat[5]+zz*VRTMatrix.pmat[8];

		/* 4. Pick out back_facing triangles.  */
		vProduct=vView*trinormal; // *(-1.0f);  /* !!! -vView as *(-1.0f) */
		if ( vProduct >= 0.0f )
                        continue;

		/* 5. Calculate light reflect strength, a simple/approximate way.  TODO: not correct, this is ONLY demo.  */
		vProduct=gv_vLight*trinormal;  // *(-1.0f); // -vLight as *(-1.0f);
		if( vProduct > 0.0f )
			vProduct=0.0f;
		else	/* Flip to get vProduct absolute value for luma */
			vProduct=-vProduct;

		/* 6. Adjust luma for pixcolor as per vProduct. */
		//cout <<"vProduct: "<< vProduct;
		fbdev->pixcolor=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
		//cout <<" getY: " << (unsigned int)egi_color_getY(fbdev->pixcolor) << endl;

		/* 7. Get triangle pts. */
		for(int k=0; k<3; k++) {
			pts[k].x=roundf(trivtx[k].x);
			pts[k].y=roundf(trivtx[k].y);
		}

		/* 5. ---------TEST: Set pixz zbuff. Should init zbuff[] to INT32_MIN: -2147483648 */
		/* A simple ZBUFF test: Triangle center point Z for all pixles on the triangle.  TODO ....*/
		gv_fb_dev.pixz=roundf((trivtx[0].z+trivtx[1].z+trivtx[2].z)/3.0);

		/* !!!! Views from -z ----> +z   */
		//gv_fb_dev.pixz = -gv_fb_dev.pixz; /* OR set fbdev->flipZ=true */

		/* 6. Draw triangle */
		//draw_triangle(&gv_fb_dev, pts);
		draw_filled_triangle(&gv_fb_dev, pts);
	}

	/* Reset fbdev->flipZ */
	fbdev->flipZ=false;

	/* Restore pixcolor */
	fbset_color2(&gv_fb_dev, color);
}


/*----------------------------------------------------
Read Vertices and other information from an obj file.
All faces are divided into triangles.

Note:
1. A description sentence MUST be put in one line in
   *.obj file!

@fpath:	Full path of the obj file
@vcnt:  Count for vertices.

Return:
	0	OK
	<0	Fails
----------------------------------------------------*/
int readObjFileInfo(const char* fpath, int & vertexCount, int & triangleCount,
				        int & normalCount, int & textureCount, int & faceCount)
{
	ifstream	fin;
	char		ch;
	int		fvcount;	/* Count vertices on a face */

	/* Init vars */
	vertexCount=0;
	triangleCount=0;
	normalCount=0;
	textureCount=0;
	faceCount=0;

	/* Read statistic of obj data */
	fin.open(fpath);
	if(fin.fail()) {
		egi_dpstd("Fail to open '%s'.\n", fpath);
		return -1;
	}

	while(!fin.eof() && fin.get(ch) ) {
		switch(ch) {
		   case 'v':	/* Vertex data */
			fin.get(ch);
			switch(ch) {
			   case ' ':
				vertexCount++;
				break;
			   case 't':
				textureCount++;
				break;
			   case 'n':
				normalCount++;
				break;
			}
			break;

		   case 'f':	/* Face data */
			fin.get(ch);
			if(ch==' ') {
				faceCount++;

				/* F. Count vertices on the face */
				fvcount=0;
				while(1) {
				   /* F.1 Get rid of following BLANKs */
				   while( ch==' ' ) fin.get(ch);
				   if( ch== '\n' || fin.eof() ) break; /* break while() */
				   /* F.2 If is non_BLANK char */
				   fvcount ++;
				   /* F.3 Get rid of following non_BLANK char. */
				   while( ch!=' ' ) {
					fin.get(ch);
					if(ch== '\n' || fin.eof() )break;
				   }
				   if( ch== '\n' || fin.eof() )break; /* break while() */
				}

				/* Add up triangle, face divided into triangles. */
				if( fvcount>0 ) {
					if( fvcount<3) {
						egi_dpstd("WARNING! A face has less than 3 vertices!\n");
						triangleCount++;
					}
					else if( fvcount==3 )
						triangleCount++;
					else // if( fvcount >3 )
						triangleCount += fvcount-2;
				}
			}
			break;

		   default:
			while( ch != '\n' )
				fin.get(ch);
			break;
		}

	} /* End while() */

	/* Close file */
	fin.close();

	egi_dpstd("Statistics:  Vertices %d;  Normals %d;  Texture %d; Faces %d; Triangle %d.\n",
				vertexCount, normalCount, textureCount, faceCount, triangleCount );

	return 0;
}

/* --- NOT friend class call ---
void ttest(const E3D_TriMesh &smesh, int n, float r)
{
	int k=smesh.tcnt;
}
*/

/*-------------------------------------------------------------
To refine a regular sphere mesh by adding a new vertex at middle
of each edge of a triangle, then subdividing the triangle into
4 trianlges.
Here, the most simple regular sphere is an icosahedron!

@smesh:	A standard regular sphere_mesh derived from an icosahedron.
@r:	Radius of the icosahedron.

-------------------------------------------------------------*/
E3D_TriMesh::E3D_TriMesh(const E3D_TriMesh &smesh, float r)
{
	int k;

	/* n MUST BE 1 Now */
	if(smesh.tcnt==20) {
		/* Convert to 80_faces ball */
		vcnt=smesh.vcnt+smesh.tcnt*3; /* this vcnt is for vCapacity! */
		tcnt=smesh.tcnt*4;
	}
	else if(smesh.tcnt==80) {
		/* Convert to 320_faces ball */
		vcnt=smesh.vcnt+smesh.tcnt*3; /* this vcnt is for vCapacity! */
		tcnt=smesh.tcnt*4;
	}
	else {
		vcnt=smesh.vcnt+smesh.tcnt*3; /* this vcnt is for vCapacity! */
		tcnt=smesh.tcnt*4;
	}

#if 0	/* Use vcnt/tcnt as capacity, later to reset to 0 */
	vcnt=smesh.vcnt;
	tcnt=smesh.tcnt;
	for(int i=0; i<n; i++) {
		vcnt *= 3; //2; TODO: pick out abundant vertice!
		tcnt *= 4;
	}
#endif
	egi_dpstd("vcnt=%d, tcnt=%d\n", vcnt, tcnt);

	/* Init vtxList[] */
	vCapacity=0;  /* 0 before alloc */
	try {
		vtxList= new Vertex[vcnt];
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Vertex[]!\n");
		return;
	}
	vCapacity=vcnt;
	vcnt=0;  // reset to 0


	/* Init triList[] */
	tCapacity=0;  /* 0 before alloc */
	try {
		triList= new Triangle[tcnt];
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Triangle[]!\n");
		return;
	}
	tCapacity=tcnt;
	tcnt=0;  // reset to 0

	/* Copy all vertices from smesh to newMesh */
	vcnt=smesh.vcnt;
	for( int i=0; i<vcnt; i++) {
		/* Vector as point */
		vtxList[i].pt=smesh.vtxList[i].pt; //vertex(i);

		/* Ref Coord. uv */
		/* Normal vector */
		/* Mark number */
	}
	/* DO NOT copy triangle from smesh! they will NOT be retained in newMesh! */

	/* Create new triangles by Growing/splitting old strianlge in smesh. */
	E3D_Vector vA, vB, vC;
	E3D_Vector vNA, vNB, vNC;  /* Newly added vetices in original triangle */
	int NAidx, NBidx, NCidx;   /* New vtx index */

	/* NOW: tcnt=0 */
	printf(" ------ vcnt =%d -------\n", vcnt);
	for(k=0; k<smesh.tcnt; k++) { // Class tcnt is changing!

		/* NOW: it hold ONLY same vtxList[].pt as smesh.vtxList[].pt */

		vA= smesh.vtxList[smesh.triList[k].vtx[0].index].pt;
		vB= smesh.vtxList[smesh.triList[k].vtx[1].index].pt;
		vC= smesh.vtxList[smesh.triList[k].vtx[2].index].pt;

		/* 0, 1   c= a0+(a1-a0)/2, c=R/Mod(c)*c;   */
		vNA=vA+(vB-vA)/2.0f;
		vNA *= r/E3D_vector_magnitude(vNA);
		/* 1, 2   c= a1+(a2-a1)/2, c=R/Mod(c)*c;   */
		vNB=vB+(vC-vB)/2.0f;
		vNB *= r/E3D_vector_magnitude(vNB);
		/* 2, 0   c= a2+(a0-a2)/2, c=R/Mod(c)*c;   */
		vNC=vC+(vA-vC)/2.0f;
		vNC *= r/E3D_vector_magnitude(vNC);

		/* Add new vertices */
		/* ..Check if vNA already in vtxList[] */
		NAidx = -1;
		for( int j=0; j<vcnt; j++) {
			/* Vertex Already exists */
			if(vtxList[j].pt == vNA) {
				NAidx=j;
				break;
			}
		}
		/* New vertex */
		if( NAidx<0 ) {
			vtxList[vcnt].pt = vNA;
			NAidx=vcnt;
			vcnt++;
		}
		/* ..Check if vNB already in vtxList[] */
		NBidx = -1;
		for( int j=0; j<vcnt; j++) {
			/* Vertex Already exists */
			if(vtxList[j].pt == vNB) {
				NBidx=j;
				break;
			}
		}
		/* New vertex */
		if( NBidx<0 ) {
			vtxList[vcnt].pt = vNB;
			NBidx=vcnt;
			vcnt++;
		}
		/* ..Check if vNC already in vtxList[] */
		NCidx = -1;
		for( int j=0; j<vcnt; j++) {
			/* Vertex Already exists */
			if(vtxList[j].pt == vNC) {
				NCidx=j;
				break;
			}
		}
		/* New vertex */
		if( NCidx<0 ) {
			vtxList[vcnt].pt = vNC;
			NCidx=vcnt;
			vcnt++;
		}

		/* Add 4 triangles */
		/* Index: vA, vNA, vNC */
		triList[tcnt].vtx[0].index=smesh.triList[k].vtx[0].index; //vA
		triList[tcnt].vtx[1].index=NAidx; //vNA
		triList[tcnt].vtx[2].index=NCidx; //vNC
		tcnt++;

		/* Index: vNA, vB, vNB */
		triList[tcnt].vtx[0].index=NAidx; //vNA
		triList[tcnt].vtx[1].index=smesh.triList[k].vtx[1].index; //vB
		triList[tcnt].vtx[2].index=NBidx; //vNB
		tcnt++;
		/* Index: vNA, vNB, vNC */
		triList[tcnt].vtx[0].index=NAidx; //vNA
		triList[tcnt].vtx[1].index=NBidx; //vNB
		triList[tcnt].vtx[2].index=NCidx; //vNC
		tcnt++;
		/* Index: vNC, vNB, vC */
		triList[tcnt].vtx[0].index=NCidx; //vNC
		triList[tcnt].vtx[1].index=NBidx; //vNB
		triList[tcnt].vtx[2].index=smesh.triList[k].vtx[2].index; //vC
		tcnt++;
	}

}


////////////////////////////////////  E3D_Draw Function  ////////////////////////////////////////

/*----------------------------------------------
Draw a 3D line between va and vb. zbuff applied.
@fbdev:	  Pointer to FBDEV.
@va,vb:	  Two E3D_Vectors as two points.
-----------------------------------------------*/
inline void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb)
{
	draw3D_line( fbdev, roundf(va.x), roundf(va.y), roundf(va.z),
		            roundf(vb.x), roundf(vb.y), roundf(vb.z) );
}

void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix)
{
	E3D_Vector vpts[2];

	vpts[0].zero();
	vpts[1]=va;

	vpts[0]=vpts[0]*RTmatrix;
	vpts[1]=vpts[1]*RTmatrix;

	/* Project vpts */
	if( projectPoints(vpts, 2, projMatrix) !=0)
		return;

	draw3D_line( fbdev, roundf(vpts[0].x), roundf(vpts[0].y), roundf(vpts[0].z),
		            roundf(vpts[1].x), roundf(vpts[1].y), roundf(vpts[1].z) );
}


/*-------------------------------------------------------
Draw a 3D Circle. (Zbuff applied as in draw_line)
Draw a circle at XY plane then transform it by RTmatrix.

@fbdev:	  	Pointer to FBDEV.
@r:	  	Radius.
@RTmatrix: 	Transform matrix.
-------------------------------------------------------*/
void E3D_draw_circle(FBDEV *fbdev, int r, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix)
{
	if(r<=0) return;

	/* Angle increment step */
	float astep= asinf(0.5/r)*2;
	int   nstep= MATH_PI*2/astep;

	//float astart;
	float aend;	/* segmental arc start/end angle at XY plan. */
	E3D_Vector  vstart, vend; /* segmental arc start/end point at 3D space */
	E3D_Vector  vpts[2];

	/* Draw segment arcs */
	for( int i=0; i<nstep+1; i++) {
		/* Assign aend to astart */
		//astart=aend;
		vstart=vend;

		/* Update aend and vend */
		aend= i*astep;
		vend.x=cos(aend)*r;
		vend.y=sin(aend)*r;
		vend.z=0;

		/* Transform vend as per Nmatrix */
		vend=vend*RTmatrix;

		/* Projection to vpts */
		vpts[0]=vstart; vpts[1]=vend;

		if(i>0) {
			/* Project mat */
			if( projectPoints(vpts, 2, projMatrix) !=0)
				continue;
			/* Draw segmental arc */
			E3D_draw_line(fbdev, vpts[0], vpts[1]);
		}
	}

}


/*---------------------------------------------------------------
Draw the Coordinate_Navigating_Sphere(Frame).
Draw a circle at Global XY plane then transform it by RTmatrix.

@fbdev:	 	Pointer to FBDEV.
@r:	  	Radius.
@RTmatrix: 	Transform matrix.
----------------------------------------------------------------*/
void E3D_draw_coordNavSphere(FBDEV *fbdev, int r, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix)
{
	if(r<=0)
		return;

        E3D_Vector axisX(1,0,0);
        E3D_Vector axisY(0,1,0);
	E3D_RTMatrix TMPmat;
        TMPmat.identity();

    gv_fb_dev.flipZ=true;

        /* XY plane -->Z */
        //cout << "Draw XY circle\n";
        fbset_color2(&gv_fb_dev, WEGI_COLOR_RED);
        E3D_draw_circle(&gv_fb_dev, r, RTmatrix, projMatrix); /* AdjustMat to Center screen */

        /* XZ plane -->Y */
        //cout << "Draw XZ circle\n";
        TMPmat.setRotation(axisX, -0.5*MATH_PI);
        fbset_color2(&gv_fb_dev, WEGI_COLOR_GREEN);
        E3D_draw_circle(&gv_fb_dev, r, TMPmat*RTmatrix, projMatrix);

        /* YZ plane -->X */
        //cout << "Draw YZ circle\n";
        TMPmat.setRotation(axisY, 0.5*MATH_PI);
        fbset_color2(&gv_fb_dev, WEGI_COLOR_BLUE);
        E3D_draw_circle(&gv_fb_dev, r, TMPmat*RTmatrix, projMatrix);

        /* Envelope circle, which is perpendicular to the ViewDirection */
        //cout << "Draw envelope circle\n";
	TMPmat=RTmatrix;
        TMPmat.zeroRotation();
        fbset_color2(&gv_fb_dev, WEGI_COLOR_GRAY);
        E3D_draw_circle(&gv_fb_dev, r, TMPmat, projMatrix);

    gv_fb_dev.flipZ=false;
}

/*---------------------------------------------------------------
Draw the Coordinate_Navigating_Sphere(Frame).
Draw a circle at Global XY plane then transform it by RTmatrix.

@fbdev:	 	Pointer to FBDEV.
@r:	  	Radius.
@RTmatrix: 	Transform matrix.
----------------------------------------------------------------*/
#include "egi_FTsymbol.h"
void E3D_draw_coordNavFrame(FBDEV *fbdev, int size, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix)
{
	if( size<=0 )
		return;

        E3D_Vector axisX(size,0,0);
        E3D_Vector axisY(0,size,0);
        E3D_Vector axisZ(0,0,size);
        E3D_Vector vpt;

    gv_fb_dev.flipZ=true;

        /* Axis_Z */
        fbset_color2(&gv_fb_dev, WEGI_COLOR_RED);
	E3D_draw_line(&gv_fb_dev, axisZ, RTmatrix, projMatrix);

	vpt=axisZ*RTmatrix;
        if( projectPoints(&vpt, 1, projMatrix)==0) {
	        FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                     16, 16,                         /* fw,fh */
                                     (UFT8_PCHAR)"Z", 		     /* pstr */
                                     300, 1, 0,                      /* pixpl, lines, fgap */
                                     -8+vpt.x, -8+vpt.y,                   /* x0,y0, */
                                     WEGI_COLOR_RED, -1, 255,        /* fontcolor, transcolor,opaque */
                                     NULL, NULL, NULL, NULL );       /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}

        /* Axis_Y */
        fbset_color2(&gv_fb_dev, WEGI_COLOR_GREEN);
	E3D_draw_line(&gv_fb_dev, axisY, RTmatrix, projMatrix);

	vpt=axisY*RTmatrix;
        if( projectPoints(&vpt, 1, projMatrix)==0) {
	        FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                     16, 16,                         /* fw,fh */
                                     (UFT8_PCHAR)"Y", 		     /* pstr */
                                     300, 1, 0,                      /* pixpl, lines, fgap */
                                     -8+vpt.x, -8+vpt.y,                   /* x0,y0, */
                                     WEGI_COLOR_GREEN, -1, 255,        /* fontcolor, transcolor,opaque */
                                     NULL, NULL, NULL, NULL );       /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}

        /* Axis_X */
        fbset_color2(&gv_fb_dev, WEGI_COLOR_BLUE);
	E3D_draw_line(&gv_fb_dev, axisX, RTmatrix, projMatrix);

	vpt=axisX*RTmatrix;
        if( projectPoints(&vpt, 1, projMatrix)==0) {
	        FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                     16, 16,                         /* fw,fh */
                                     (UFT8_PCHAR)"X", 		     /* pstr */
                                     300, 1, 0,                      /* pixpl, lines, fgap */
                                     -8+vpt.x, -8+vpt.y,                   /* x0,y0, */
                                     WEGI_COLOR_BLUE, -1, 255,        /* fontcolor, transcolor,opaque */
                                     NULL, NULL, NULL, NULL );       /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}


    gv_fb_dev.flipZ=false;
}


#endif


