/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


	         EGI 3D Triangle Mesh

Refrence:
1. "3D Math Primer for Graphics and Game Development"
                        by Fletcher Dunn, Ian Parberry

Note:
1. E3D Right_hand COORD system.


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
#include "egi_debug.h"
#include "e3d_vector.h"
#include "egi.h"
#include "egi_fbgeom.h"
#include "egi_color.h"

using namespace std;

int readObjFileInfo(const char* fpath, int & vertexCount, int & triangleCount,
				        int & normalCount, int & textureCount, int & faceCount);

/* Global light vector */
E3D_Vector gv_vLight(0, 0, 1);

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

	/* TEST FUNCTIONS: */
	void setVtxCount(int cnt) { if(cnt>=0) vcnt=cnt; };
	void setTriCount(int cnt) { if(cnt>=0) tcnt=cnt; };


	/* Function: Count vertices and triangles */
	int vtxCount() const { return vcnt; };
	int triCount() const { return tcnt; };

	/* Function: Retrun ref. of indexed vertex */
	E3D_Vector & vertex(int vtxIndex) const;

	/* Function: Add vertices into vtxList[] */
	int addVertex(const Vertex &v);
	int addVertexFromPoints(const E3D_POINT *pts, int n); /* Add veterx from points */

	/* Function: Add triangle into triList[] */
	int addTriangle(const Triangle *t);
	int addTriangleFromVtxIndx(const int *vidx, int n);    /* To add/form triangles with vertex indexes list. */

	/* Function: scale up/down mesh/vertices */
	void scaleMesh(float scale);

	/* Function: Transform vertices/normals */
	void transformVertices(const E3D_RTMatrix  &RTMatrix);
	void transformTriNormals(const E3D_RTMatrix  &RTMatrix);
	void transformMesh(const E3D_RTMatrix  &RTMatrix); /* Vertices+TriNormals*/

	/* Function: Calculate normals for all triangles */
	void updateAllTriNormals();
	void reverseAllTriNormals();

	/* Function: clone vertices/trianges from the tmesh.  */
	void cloneMesh(const E3D_TriMesh &tmesh);

	/* Function: Draw wires/faces */
	void drawMeshWire(FBDEV *fbdev);

	/* Function: Render all triangles */
	void renderMesh(FBDEV *fbdev);

private:
	/* All members inited at E3D_TriMesh() */

	#define		TRIMESH_GROW_SIZE	64	/* Growsize for vtxList and triList */
	int		vCapacity;	/* MemSpace capacity for vtxList[] */
	int		vcnt;		/* Vertex counter */
	Vertex  	*vtxList;	/* Vertex array */

	int		tCapacity;	/* MemSpace capacity for triList[] */
	int		tcnt;		/* Triangle counter  */
	Triangle	*triList;	/* Triangle array */

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
   and a auadrilateral will be stored as 2
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

	int		k;		    /* triangle index in a face, [0  MAX.3] */
        int             m;                  /* 0: vtxIndex, 1: texutreIndex 2: normalIndex  */
	float		xx,yy,zz;
	int		vtxIndex[4]={0};    /* To store vtxIndex for 4_side face */
	int		textureIndex[4]={0};
	int		normalIndex[4]={0};

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

		   	/* Case A. --- ONLY vtxIndex
		    	 * Example: f 61 21 44
		    	 */
			if( strstr(strline, "/")==NULL ) {
			   /* To extract face vertex indices */
			   pt=strtok(strline+1, " ");
			   for(k=0; pt!=NULL && k<4; k++) {
				vtxIndex[k]=atoi(pt) -1;
				pt=strtok(NULL, " ");
			   }

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
			   /* TODO:  textrueIndex[] and normalIndex[], value <0 as omitted.  */

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
		        else {

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
			   for(k=0; pt!=NULL && k<4; k++) {
				//cout << "Index group: " << pt<<endl;
				/* Parse vtxIndex/textureIndex/normalIndex */
				pt2=strtok_r(pt, "/", &savept2);
				for(m=0; pt2!=NULL && m<3; m++) {
					//cout <<"pt2: "<<pt2<<endl;
					switch(m) {
					   case 0:
						vtxIndex[k]=atoi(pt2)-1;
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
					/* Reorder */
					normalIndex[k]=textureIndex[k];
					textureIndex[k]=vtxIndex[k];
					vtxIndex[k]=-1;
				}
				/* In case "5/0/", -- m==2, normalIndex omitted!  */
				else if( m==2 ) { /* pt[0]=='/' also has m==2! */
					normalIndex[k]=0-1;
				}

				/* Get next indices */
				pt=strtok_r(NULL, " ", &savept);
			   } /* End for(k) */

			   /*  Assign to triangle vtxindex */
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
void E3D_TriMesh::printAllVertices(const char *name)
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
void E3D_TriMesh::printAllTriangles(const char *name)
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
Transform all triangle normals.

@RTMatrix:	RotationTranslation Matrix
----------------------------------------------------------------*/
void E3D_TriMesh::transformTriNormals(const E3D_RTMatrix  &RTMatrix)
{
	float xx;
	float yy;
	float zz;

	for( int j=0; j<tcnt; j++) {
		xx=triList[j].normal.x;
		yy=triList[j].normal.y;
		zz=triList[j].normal.z;
		triList[j].normal.x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6]; /* Ignor translation */
		triList[j].normal.y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7];
		triList[j].normal.z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8];
	}

}


/*--------------------------------------------------------------
Transform all triangle normals.

@RTMatrix:	RotationTranslation Matrix
----------------------------------------------------------------*/
void E3D_TriMesh::transformMesh(const E3D_RTMatrix  &RTMatrix)
{
	transformVertices(RTMatrix);
	transformTriNormals(RTMatrix);
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
	   triList[i].normal.normalize();

   	}
}

/*------------------------------------
Reverse normals of all triangles
------------------------------------*/
void E3D_TriMesh::reverseAllTriNormals()
{
	for(int i=0; i<tcnt; i++)
		triList[i].normal *= -1.0f;
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

}

/*-----------------------------------------------------
Draw mesh wire.
View direction: Screen Coord. Z axis.
-----------------------------------------------------*/
void E3D_TriMesh::drawMeshWire(FBDEV *fbdev)
{
	int vtidx[3];	    /* vtx index of a triangle */
	E3D_POINT cpt;	    /* Center of triangle */
	float nlen=20.0f;   /* Normal line length */
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); /* View direction */
	float	    vProduct;   /* dot product */

	/* TEST: Project to Z plane, Draw X,Y only */
	for(int i=0; i<tcnt; i++) {

#if 1		/* To pick out back faces.  */
		vProduct=vView*(triList[i].normal)*(-1.0f); // -vView as *(-1.0f);
		if ( vProduct <= 0.0f )
                        continue;
#endif

		vtidx[0]=triList[i].vtx[0].index;
		vtidx[1]=triList[i].vtx[1].index;
		vtidx[2]=triList[i].vtx[2].index;

		/* draw_line_antialias() */
		draw_line(fbdev, vtxList[vtidx[0]].pt.x, vtxList[vtidx[0]].pt.y,
				 vtxList[vtidx[1]].pt.x, vtxList[vtidx[1]].pt.y);

		draw_line(fbdev, vtxList[vtidx[1]].pt.x, vtxList[vtidx[1]].pt.y,
				 vtxList[vtidx[2]].pt.x, vtxList[vtidx[2]].pt.y);

		draw_line(fbdev, vtxList[vtidx[0]].pt.x, vtxList[vtidx[0]].pt.y,
				 vtxList[vtidx[2]].pt.x, vtxList[vtidx[2]].pt.y);


		#if 0 /* TEST: draw triangle normals -------- */
		cpt.x=(vtxList[vtidx[0]].pt.x+vtxList[vtidx[1]].pt.x+vtxList[vtidx[2]].pt.x)/3.0f;
		cpt.y=(vtxList[vtidx[0]].pt.y+vtxList[vtidx[1]].pt.y+vtxList[vtidx[2]].pt.y)/3.0f;
		cpt.z=(vtxList[vtidx[0]].pt.z+vtxList[vtidx[1]].pt.z+vtxList[vtidx[2]].pt.z)/3.0f;

		draw_line(fbdev,cpt.x, cpt.y, cpt.x+nlen*(triList[i].normal.x), cpt.y+nlen*(triList[i].normal.y) );
		#endif
	}

}


/*---------------------------------------------
Render the whole mesh.
View direction: Screen Coord. Z axis.
---------------------------------------------*/
void E3D_TriMesh::renderMesh(FBDEV *fbdev)
{
	int vtidx[3];  	/* vtx index of a triangel */
	EGI_POINT   pts[3];
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); /* View direction */
	float	    vProduct;   /* dot product */

	/* Default color */
	EGI_16BIT_COLOR color=fbdev->pixcolor;
	EGI_8BIT_CCODE codeY=egi_color_getY(color);

	/* TEST: Project to Z plane, Draw X,Y only */
	for(int i=0; i<tcnt; i++) {
		vtidx[0]=triList[i].vtx[0].index;
		vtidx[1]=triList[i].vtx[1].index;
		vtidx[2]=triList[i].vtx[2].index;

		/* 1. To pick out back faces.  */
		vProduct=vView*(triList[i].normal)*(-1.0f); // -vView as *(-1.0f);
		if ( vProduct <= 0.0f )
                        continue;

		/* 2. To calculate light reflect strength:  TODO: this is ONLY demo.  */
		vProduct=gv_vLight*(triList[i].normal)*(-1.0f); // -vLight as *(-1.0f);
		if( vProduct <= 0.0f )
			continue;

		/* 3. Adjust luma for pixcolor as per vProduct. */
		//cout <<"vProduct: "<< vProduct;
		fbdev->pixcolor=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
		//cout <<" getY: " << (unsigned int)egi_color_getY(fbdev->pixcolor) << endl;

		/* 4. Draw triangle faces. */
		pts[0].x=vtxList[vtidx[0]].pt.x;
		pts[0].y=vtxList[vtidx[0]].pt.y;
		pts[1].x=vtxList[vtidx[1]].pt.x;
		pts[1].y=vtxList[vtidx[1]].pt.y;
		pts[2].x=vtxList[vtidx[2]].pt.x;
		pts[2].y=vtxList[vtidx[2]].pt.y;

		//draw_triangle(&gv_fb_dev, pts);
		draw_filled_triangle(&gv_fb_dev, pts);
	}

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

#endif


