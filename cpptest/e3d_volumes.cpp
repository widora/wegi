/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Journal:
2022-09-19: Create the file.
	    1. Create Class E3D_RtaSphere and the constructor.
2022-09-20:
	1.E3D_RtaSphere(): use vtxLinkList to store info. such as
	  its originally linked vertices and newly added vertices
	  between them.
	2. Add Class E3D_Cuboid and the constructor.

Midas Zhou
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#include "e3d_volumes.h"
#include "egi_debug.h"
#include "egi_utils.h"

using namespace std;


		/*------------------------------------------
            		      Class E3D_Cubiod
    		    (Derived+Friend Class of E3D_TriMesh)
		------------------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_Cuboid::E3D_Cuboid(float dx, float dy, float dz): E3D_TriMesh()
{
	/* 1. Clear vtxList[] and triList[], E3D_TriMesh() may allocate them.  */
	if(vCapacity>0) {
		delete [] vtxList;
		vCapacity=0;
		vcnt=0;
	}
	if(tCapacity>0) {
		delete [] triList;
		tCapacity=0;
		tcnt=0;
	}

	/* 2. Init triList[] for a cuboid */
	try {
		vtxList= new Vertex[8];  /* 8 vertices */
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Vertex[]!\n");
		return;
	}
	vCapacity=8;

	/* 3. Init triList[] for a cuboid */
	try {
		triList= new Triangle[12]; /* 12 triagnles */
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Triangle[]!\n");
		return;
	}
	tCapacity=12;

	/* 4. Create vertices for a cuboid */
	vtxList[0].assign(0.0, 0.0, 0.0);
	vtxList[1].assign(0.0, dy, 0.0);
	vtxList[2].assign(dx, dy, 0.0);
	vtxList[3].assign(dx, 0.0, 0.0);

	vtxList[4].assign(0.0, 0.0, dz);
	vtxList[5].assign(0.0, dy, dz);
	vtxList[6].assign(dx, dy, dz);
	vtxList[7].assign(dx, 0.0, dz);

	/* 5. All vertices counted in */
	vcnt=8;

	/* 6. Create triangles */
/* Bottom: f 0 1 2;  f 0 2 3
   Top:    f 4 6 5;  f 4 7 6
   Sides:  f 0 4 1;  f 1 4 5; f 1 5 6; f 1 6 2; f 2 6 7; f 2 7 3; f 3 7 0; f 7 4 0;
*/
	triList[0].assignVtxIndx(0,1,2);
	triList[1].assignVtxIndx(0,2,3);
	triList[2].assignVtxIndx(4,6,5);
	triList[3].assignVtxIndx(4,7,6);

	triList[4].assignVtxIndx(0,4,1);
	triList[5].assignVtxIndx(1,4,5);
	triList[6].assignVtxIndx(1,5,6);
	triList[7].assignVtxIndx(1,6,2);
	triList[8].assignVtxIndx(2,6,7);
	triList[9].assignVtxIndx(2,7,3);
	triList[10].assignVtxIndx(3,7,0);
	triList[11].assignVtxIndx(7,4,0);

	/* 7. All triangles counted in */
	tcnt=12;

	/* 8. Assign the ONLY triGroup */
	triGroupList.resize(1);
	triGroupList[0].tcnt=tcnt; /* 12 triangles */
	triGroupList[0].stidx=0;
	triGroupList[0].etidx=0+tcnt; /* Caution!!! etidx is NOT the end index! */
}


/*--------------------------------
	The destructor
--------------------------------*/
E3D_Cuboid::~E3D_Cuboid()
{
	egi_dpstd("E3D_Cuboid destroyed!\n");
}



		/*------------------------------------------
        		    Class E3D_RtaShpere
		  Shpere derived from Regular Twenty Aspect
		------------------------------------------*/

/*--------------------------------
	The constructor
--------------------------------*/
E3D_RtaSphere::E3D_RtaSphere(int ng, float rad):E3D_TriMesh()
{
	egi_dpstd("Call E3D_RtaShpere(%d, %f)\n", ng,rad);

	/* Assign param */
	if(ng<0)ng=0;
	n=(ng>5?5:ng);  /* Limit n */
	r=rad;

	int** vtxLinkList=NULL;
	int indxA,indxB,indxC;
	float  rtW=r*2.0/sqrt(2*sqrt(5.0)+10.0); /* W of a golen_ration rectangle, with r as its diagonal line lenght.  */
	float  rtL=r*(sqrt(5.0)+1.0)/sqrt(2*sqrt(5.0)+10.0); /* L */
	rtL += 0.5*r; //Grow original vertices
	int i,j,k;
	int oldvcnt; /* vcnt before subdividing */
	int oldtcnt;

	/* For temp. use */
	Vertex *_vtxList=NULL;
	Vertex *vtxTmp=NULL;
	Triangle *_triList=NULL;
	Triangle *triTmp=NULL;

	/* Subdivide 1 triangle to 4. */
	E3D_Vector vA, vB, vC;	   /* old vertices */
	E3D_Vector vNA, vNB, vNC;  /* Newly added vertices on old edges */
	int NAidx, NBidx, NCidx;   /* New vtx index */

  for(i=0; i<=ng; i++) {

     /* The regular twenty aspects */
     if(i==0) {

	/* Clear vtxList[] and triList[], E3D_TriMesh() may allocate them.  */
	if(vCapacity>0) {
		delete [] vtxList;
		vCapacity=0;
		vcnt=0;
	}
	if(tCapacity>0) {
		delete [] triList;
		tCapacity=0;
		tcnt=0;
	}

	/* Init triList[] for RTA20 */
	try {
		vtxList= new Vertex[12];  /* 12 vertices */
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Vertex[]!\n");
		return;
	}
	vCapacity=12;

	/* Init triList[] for RTA20 */
	try {
		triList= new Triangle[20]; /* 20 triagnles */
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Triangle[]!\n");
		return;
	}
	tCapacity=20;

	/* Create vertices for Icosahedron, Regular twenty aspect */
/* 0.6180 NOT correct
v 0.0 100.0 61.80
v 0.0 100.0 -61.80
v 100.0 61.80 0.0
v 100.0 -61.80 0.0
v 0.0 -100.0 -61.80
v 0.0 -100.0 61.80

v 61.80 0.0 100.0
v -61.80 0.0 100.0
v 61.80 0.0 -100.0
v -61.80 0.0 -100.0
v -100.0 61.80 0.0
v -100.0 -61.80 0.0
*/
	vtxList[0].assign(0.0, rtL, rtW);
	vtxList[1].assign(0.0, rtL, -rtW);
	vtxList[2].assign(rtL, rtW, 0.0);
	vtxList[3].assign(rtL, -rtW, 0.0);
	vtxList[4].assign(0.0, -rtL, -rtW);
	vtxList[5].assign(0.0, -rtL, rtW);

	vtxList[6].assign(rtW, 0.0, rtL);
	vtxList[7].assign(-rtW, 0.0, rtL);
	vtxList[8].assign(rtW, 0.0, -rtL);
	vtxList[9].assign(-rtW, 0.0, -rtL);
	vtxList[10].assign(-rtL, rtW, 0.0);
	vtxList[11].assign(-rtL, -rtW, 0.0);

	/* All vertices counted in */
	vcnt=12;

	/* Create triangles */
/*
f 1 7 3
f 3 7 4
f 4 7 6
f 6 7 8
f 1 8 7
f 2 9 10
f 4 5 9
f 4 6 5
f 5 6 12
f 8 11 12
*/
	triList[0].assignVtxIndx(0,6,2);
	triList[1].assignVtxIndx(2,6,3);
	triList[2].assignVtxIndx(3,6,5);
	triList[3].assignVtxIndx(5,6,7);
	triList[4].assignVtxIndx(0,7,6);
	triList[5].assignVtxIndx(1,8,9);
	triList[6].assignVtxIndx(3,4,8);
	triList[7].assignVtxIndx(3,5,4);
	triList[8].assignVtxIndx(4,5,11);
	triList[9].assignVtxIndx(7,10,11);

/*
f 3 4 9
f 2 3 9
f 1 3 2
f 1 2 11
f 2 10 11
f 1 11 8
f 5 12 10
f 5 10 9
f 6 8 12
f 10 12 11
*/
	triList[10].assignVtxIndx(2,3,8);
	triList[11].assignVtxIndx(1,2,8);
	triList[12].assignVtxIndx(0,2,1);
	triList[13].assignVtxIndx(0,1,10);
	triList[14].assignVtxIndx(1,9,10);
	triList[15].assignVtxIndx(0,10,7);
	triList[16].assignVtxIndx(4,11,9);
	triList[17].assignVtxIndx(4,9,8);
	triList[18].assignVtxIndx(5,7,11);
	triList[19].assignVtxIndx(9,11,10);

	/* All triangles counted in */
	tcnt=20;

	/* Assign the ONLY triGroup */
	triGroupList.resize(1);
	triGroupList[0].tcnt=20; /* 20 triangles */
	triGroupList[0].stidx=0;
	triGroupList[0].etidx=0+20; /* Caution!!! etidx is NOT the end index! */
     }
     /* Else: to refine the mesh by recursively subdividing all triangles, 1 to 4. */
     else {

	egi_dpstd("create array2D...\n");
	/* vtxLinkList[] to store linked vertice indice in vtxLinkList[][0-6],
	   and index of newly added vertex between them at vtxLinkList[][7-11].  */
	vtxLinkList=(int **)egi_create_array2D(vcnt, 6*2, sizeof(int)); /* Max 6 lined vertices for each vertex */
	if(vtxLinkList==NULL) {
		egi_dpstd(DBG_RED"Fail to create_array2D!\n"DBG_RESET);
		return;
	}
	/* init [0-6], NOT necessary for [7-11] */
	egi_dpstd("Init array2D...\n");
	for(int i=0; i<vcnt; i++)
		for(int j=0; j<6; j++) /* no necessy for [7-11] */
			vtxLinkList[i][j]=-1;

	/* Compute vCapacity and tCapacity for new sphere mesh */
	vCapacity += tcnt*3/2;  /* tcnt is of last grade sphere */
	tCapacity += tcnt*3;

	/* Allocate triList[] for new sphere */
	try {
		_vtxList= new Vertex[vCapacity];
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate _vtxList!\n");
		vCapacity -= tcnt*3/2;
		return;
	}

	/* Allocate triList[] for new sphere */
	try {
		_triList= new Triangle[tCapacity];
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate _triList!\n");
		tCapacity -= tcnt*3;
		return;
	}

	/* Copy old data of vtxList[], NOT copy triList[], which will be whole new. */
	for(j=0; j<vcnt; j++) {
		_vtxList[j].pt = vtxList[j].pt;
		/* Ref Coord. uv */
		/* Normal vector */
		/* Mark number */
	}

	/* DO NOT free triList, just swap triList and _triList ---> to free it later. */
	vtxTmp=vtxList;  vtxList=_vtxList;  _vtxList=vtxTmp;
	triTmp=triList;  triList=_triList;  _triList=triTmp;

	/* NOW: _vtxList/_triList holds the old data */

	/* Reset tcnt, as NOW there is NO trianlge in triList[]. */
	oldtcnt=tcnt;  oldvcnt=vcnt;
	tcnt=0; //vcnt keeps

	for(k=0; k<oldtcnt; k++) {

		/* Vertices of each triangle in old mesh */
		indxA=_triList[k].vtx[0].index;
		indxB=_triList[k].vtx[1].index;
		indxC=_triList[k].vtx[2].index;
		vA= _vtxList[indxA].pt;
		vB= _vtxList[indxB].pt;
		vC= _vtxList[indxC].pt;

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
		/* ..Check if vNA already in vtxList[]. let NAidx=j then.
		 * Note: The edge of the added vertex is shared by two old faces, check to avoid creating it twice.
		 */
		NAidx = -1;
#if 0 ////////////////////////////
		for( int j=0; j<vcnt; j++) {

			/* Vertex Already exists */
			if(E3D_vector_distance(vtxList[j].pt, vNA)<1.0e-4) {
				NAidx=j;
				break;
			}
		}
#endif //////////////////////////
		/* Check if a vertex alread exists between --- vA/vB --- */
		for(int m=0; m<6; m++) {
egi_dpstd("m=%d\n", m);
		     if(vtxLinkList[indxA][m]==indxB)
			  NAidx=vtxLinkList[indxA][m+6];
		}
		/* New vertex */
		if( NAidx<0 ) {
			vtxList[vcnt].pt = vNA;
			NAidx=vcnt;
			vcnt++;

			/* Update vtxLinkList[][] */
                        for(int m=0; m<6; m++) {
                             if(vtxLinkList[indxA][m]<0) {
                                  vtxLinkList[indxA][m]=indxB;  /* Linked vertex */
				  vtxLinkList[indxA][m+6]=NAidx;  /* Newly add vertex */
				  break;
			     }
			}
			for(int m=0; m<6; m++) {
			     if(vtxLinkList[indxB][m]<0) {
                                  vtxLinkList[indxB][m]=indxA;  /* Linked vertex */
                                  vtxLinkList[indxB][m+6]=NAidx; /* Newly add vertex */
				  break;
			     }
			}
		}

		/* ..Check if vNB already in vtxList[] */
		NBidx = -1;
#if 0 //////////////////////////
		for( int j=0; j<vcnt; j++) {
			/* Vertex Already exists */
			if(E3D_vector_distance(vtxList[j].pt, vNB)<1.0e-4) {
				NBidx=j;
				break;
			}
		}
#endif //////////////////////////
		/* Check if a vertex alread exists between --- vB/vC --- */
		for(int m=0; m<6; m++) {
		     if(vtxLinkList[indxB][m]==indxC)
			  NBidx=vtxLinkList[indxB][m+6];
		}
		/* New vertex */
		if( NBidx<0 ) {
			vtxList[vcnt].pt = vNB;
			NBidx=vcnt;
			vcnt++;

			/* Update vtxLinkList[][] */
                        for(int m=0; m<6; m++) {
                             if(vtxLinkList[indxB][m]<0) {
                                  vtxLinkList[indxB][m]=indxC;  /* Linked vertex */
				  vtxLinkList[indxB][m+6]=NBidx;  /* Newly add vertex */
				  break;
			     }
			}
			for(int m=0; m<6; m++) {
			     if(vtxLinkList[indxC][m]<0) {
                                  vtxLinkList[indxC][m]=indxB;  /* Linked vertex */
                                  vtxLinkList[indxC][m+6]=NBidx; /* Newly add vertex */
				  break;
			     }
			}
		}

		/* ..Check if vNC already in vtxList[] */
		NCidx = -1;
#if 0 //////////////////////////////////
		for( int j=0; j<vcnt; j++) {
			/* Vertex Already exists */
			//if(vtxList[j].pt == vNC) {
			if(E3D_vector_distance(vtxList[j].pt, vNC)<1.0e-4) {
				NCidx=j;
				break;
			}
		}
#endif //////////////////////////////////
		/* Check if a vertex alread exists between --- vA/vC --- */
		for(int m=0; m<6; m++) {
		     if(vtxLinkList[indxA][m]==indxC)
			  NCidx=vtxLinkList[indxA][m+6];
		}
		/* New vertex */
		if( NCidx<0 ) {
			vtxList[vcnt].pt = vNC;
			NCidx=vcnt;
			vcnt++;

			/* Update vtxLinkList[][] */
                        for(int m=0; m<6; m++) {
                             if(vtxLinkList[indxA][m]<0) {
                                  vtxLinkList[indxA][m]=indxC;  /* Linked vertex */
				  vtxLinkList[indxA][m+6]=NCidx;  /* Newly add vertex */
				  break;
			     }
			}
			for(int m=0; m<6; m++) {
			     if(vtxLinkList[indxC][m]<0) {
                                  vtxLinkList[indxC][m]=indxA;  /* Linked vertex */
                                  vtxLinkList[indxC][m+6]=NCidx; /* Newly add vertex */
				  break;
			     }
			}
		}

		/* Add 4 triangles. tcnt init as 0,as no triangle copied from. */
		/* Index: vA, vNA, vNC */
		triList[tcnt].vtx[0].index=_triList[k].vtx[0].index; //vA
		triList[tcnt].vtx[1].index=NAidx; //vNA
		triList[tcnt].vtx[2].index=NCidx; //vNC
		tcnt++;

		/* Index: vNA, vB, vNB */
		triList[tcnt].vtx[0].index=NAidx; //vNA
		triList[tcnt].vtx[1].index=_triList[k].vtx[1].index; //vB
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
		triList[tcnt].vtx[2].index=_triList[k].vtx[2].index; //vC
		tcnt++;
	}


	/* Reassign the ONLY triGroup */
	triGroupList.resize(1);
	triGroupList[0].tcnt=tcnt; /* 20 triangles */
	triGroupList[0].stidx=0;
	triGroupList[0].etidx=0+tcnt; /* Caution!!! etidx is NOT the end index! */

	/* Free old data */
	delete [] _vtxList;
	delete [] _triList;

	/* Free  vtxLinkList */
	egi_dpstd("free array2D...\n");
	egi_free_array2D((void ***)&vtxLinkList, oldvcnt);

	egi_dpstd("Subdivide mesh into vcnt=%d, tcnt=%d, tCapacity=%d, vCapacity=%d\n", vcnt, tcnt, vCapacity, tCapacity);

     } /* else */

  }/* for(k) */

}

/*--------------------------------
	The destructor
--------------------------------*/
E3D_RtaSphere::~E3D_RtaSphere()
{

	egi_dpstd("E3D_RtaShpere destroyed!\n");

}


