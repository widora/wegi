/*------------------------------------------------------
	         EGI 3D Triangle Mesh

Refrence:
1. "3D Math Primer for Graphics and Game Development"
                        by Fletcher Dunn, Ian Parberry


TODO:
	1. Grow vtxList[] and triList[].


Journal:
2021-07-30:     Create egi_trimesh.h
2021-07-31:
	1. Add E3D_TriMesh::cloneMesh();
	2. ADD readObjFileInfo()
2021-08-2:
	1. Add E3D_TriMesh::E3D_TriMesh(const char *fobj)

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------*/
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

using namespace std;

int readObjFileInfo(const char* fpath, int & vertexCount, int & triangleCount,
				        int & normalCount, int & textureCount, int & faceCount);

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
	E3D_Vector & vertex(int vtxIndex);

	/* Function: Add vertices into vtxList[] */
	int addVertex(const Vertex &v);
	int addVertexFromPoints(const E3D_POINT *pts, int n); /* Add veterx from points */

	/* Function: Add triangle into triList[] */
	int addTriangle(const Triangle *t);
	int addTriangleFromVtxIndx(const int *vidx, int n);    /* To add/form triangles with vertex indexes list. */

	/* Function: scale up/down mesh/vertices */
	void scaleMesh(float scale);

	/* Function: Transform vertices */
	void transformVertices(const E3D_RTMatrix  &RTMatrix);

	/* Function: clone vertices/trianges from the tmesh.  */
	void cloneMesh(const E3D_TriMesh &tmesh);

	/* Function: Draw wires/faces */
	void drawMeshWire(FBDEV *fbdev);

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
 -------------------------------------------*/
E3D_TriMesh::E3D_TriMesh(const char *fobj)
{
	ifstream	fin;
	int		off;
	int		offret; /* Point to line end '\n' */
	char		ch;

	int		k;		    /* triangle index in a face, [0 3] */
	int		vtxIndex[4]={0};    /* To store vtxIndex for 4_side face */
	int		normalIndex[4]={0};
	int		textureIndex[4]={0};

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
	while(!fin.eof() && fin.get(ch) ) {
		switch(ch) {
		   cout << "ch " << ch <<endl;
		   case 'v':	/* 1. Vertex data */
			fin.get(ch);
			cout << "v:ch " << ch <<endl;
			switch(ch) {
			   case ' ':  /* Read vertices X.Y.Z. */
				cout << "Vtx XYZ: ";;
				fin >> vtxList[vcnt].pt.x >> vtxList[vcnt].pt.y >> vtxList[vcnt].pt.z;

				cout << "x " << vtxList[vcnt].pt.x
				     << "y "<< vtxList[vcnt].pt.y
				     << "z "<< vtxList[vcnt].pt.z
				     << endl;
				vcnt++;

				/* To finish the txt line */
				//while( ch!='\n' )fin.get(ch);
				break;
			   case 't':  /* Read texture vertices, uv coordinates */
				/* TODO  */

				/* To finish the txt line */
				//while( ch!='\n' )fin.get(ch);
				break;
			   case 'n':  /* Read vertex normals, (for vertices of each face ) */
				/* TODO */

				/* To finish the txt line */
				//while( ch!='\n' )fin.get(ch);
				break;
			   case 'p': /* Parameter space vertices */

				/* To finish the txt line */
				//while( ch!='\n' )fin.get(ch);
				break;
			   default:
				//while( ch!='\n' )fin.get(ch);
				break;
			}

			/* To finish the txt line */
                        while( ch!='\n' && fin.get(ch) ) { };
			break;

		   case 'f':	/* 2. Face data. vtxIndex starts from 1!!! */

			#if 1 /* Only vtxIndex:  f  31  32  37 */
			fin >>vtxIndex[0] >>vtxIndex[1] >>vtxIndex[2];
			triList[tcnt].vtx[0].index = vtxIndex[0]-1;
			triList[tcnt].vtx[1].index = vtxIndex[1]-1;
			triList[tcnt].vtx[2].index = vtxIndex[2]-1;

			tcnt++;
			#endif

#if 0
			/* To get line end. */
			off=fin.tellg();
			while( ch!='\n' ) {
				fin.get(ch);
			}
			offret=fin.tellg();
			offret -=1;	/* Point to line end '\n' */
			fin.seekg(off, ios::beg);

			/* Eat up SPACEs */
			fin.get(ch);
			while( ch==' ' ) { fin.get(ch); };
			fin.seekg(-1, ios::cur);

			/* Face has MAX. 4 vertices */
		        /* Example 1: Only vtxIndex
				   f  31  32  37
			   Example 2: other index with delim '/'
 				   f 69//47 68//47 43//47 52//47
			 */
			for( k=0; k<4; k++) {
				/* Check line END */
			        if( ch=='\n' || ch=='\r' )
                			break;
			        else {  /* To test(erase) if all SPACEs followed */
			                fin.get(ch);
			                while( ch==' ' ){ fin.get(ch); };

					/* Point to first non_SPACE char */
			                off=fin.tellg();
					off-=1;

			                if( ch=='\n' || ch=='\r' || fin.eof() ) {
	                        		cout << "End of face line  " <<k <<endl;
			                        break;  /* END of line */
                			}
		        	        else  /* Restore offset */
                        			fin.seekg(off, ios::beg);
        			}
			        //cout<<"Face vertex_= "<<k<<endl;

			        /* 1. [VtxIndex] MUST be at first */
				fin.get(ch);
				if( ch=='\n' )
					break;
				else
					fin.seekg(-1, ios::cur); /* Backward */

			        fin >>vtxIndex[k];

			        /* 2. Read next ch
				      delim '/' for other index
				      OR delim ' ' for NEXT vtxIndex ( ONLY vtxIndex )
				  */
			        fin.get(ch);
				//if( ch == '/ ' )  /* Other index followed */
				/* If fin>>vtxIndex[] read in also the '\n' */
				if( fin.tellg() > offret ) {
					k+=1; /* Count k, we need this as numerb of vertices. */
					egi_dpstd("vtx/textrue/normal Index[%d]: %d/%d/%d\n", k, vtxIndex[k], textureIndex[k], normalIndex[k]);
					fin.seekg(offret, ios::beg); /* Point to '\n', see case last!  */
					break;
				}
				else if( ch==' ') {	    /* Next vtxIndex (ONLY vtxIndex) */
					egi_dpstd("vtx/textrue/normal Index[%d]: %d/%d/%d\n", k, vtxIndex[k], textureIndex[k], normalIndex[k]);
					continue;
				}

				//else: assume it's delim '/'

		        	/* 3. [textureIndex] ONLY if not second '/' */
        			fin.get(ch);
        			if(ch != '/') {
                			fin.seekg(-1, ios::cur); /* Backward */
                			fin >>textureIndex[k];
        			}
        			else {  /* IS '/', textureIndex NOT exists! */
                			textureIndex[k]=0;
        			}

			        /* 4. [normalIndex]  */
			        fin.get(ch);
			        /* 4.0 If EOL: ---> Check at beginning of for()... */
			        /* 4.1 ONLY If textrueIndex exists at 3. */
			        if( ch == '/' ) {
			                fin >>normalIndex[k];
			        }
			        /* 4.2 If textrueIndex NOT exist at 3. */
			        else if ( ch != ' ' ) {
			                fin.seekg(-1, ios::cur);
			                fin >>normalIndex[k];
			        }
			        else {   // ch==' ' normalIndex NOT exists!
			                normalIndex[k]=0;
        			}

				/* Print all data */
				egi_dpstd("vtx/textrue/normal Index[%d]: %d/%d/%d\n", k, vtxIndex[k], textureIndex[k], normalIndex[k]);


			} /* END for(k) */

			/* Add triangle. */
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

			/* To finish the txt line */
			while( ch!='\n' && fin.get(ch) );
			break;
#endif

		   default:
			/* To finish the txt line */
			while( ch != '\n' && fin.get(ch) );
			break;
		}
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
E3D_Vector & E3D_TriMesh::vertex(int vtxIndex)
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
-----------------------------------------------------*/
void E3D_TriMesh::drawMeshWire(FBDEV *fbdev)
{
	int vtidx[3];

	/* TEST: Project to Z plane, Draw X,Y only */
	for(int i=0; i<tcnt; i++) {

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

	}

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

#if 0
	int		vertexCount=0;
	int		triangleCount=0;
	int		normalCount=0;
	int		textureCount=0;
	int		faceCount=0;
#else
	vertexCount=0;
	triangleCount=0;
	normalCount=0;
	textureCount=0;
	faceCount=0;
#endif

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


#endif


