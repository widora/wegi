/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test E3D CubeMesh.

Journal:
2021-07-31:
	1. Create the file.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <iostream>
#include <stdio.h>
#include "e3d_vector.h"
#include "e3d_trimesh.h"
#include "egi_math.h"
#include "egi_debug.h"
#include "egi_fbdev.h"
#include "egi_color.h"
#include "egi_FTsymbol.h"

using namespace std;

int main(int argc, char **argv)
{
	char            strtmp[256]={0};

        int             vertexCount=0;
        int             triangleCount=0;
        int             normalCount=0;
        int             textureCount=0;
        int             faceCount=0;

#if 0
	readObjFileInfo("/tmp/Chick.obj", vertexCount, triangleCount,normalCount,textureCount,faceCount);
        egi_dpstd("Statistics:  Vertices %d;  Normals %d;  Texture %d; Faces %d; Triangle %d.\n",
                                vertexCount, normalCount, textureCount, faceCount, triangleCount );

#endif


        /* Initilize sys FBDEV */
        printf("init_fbdev()...\n");
        if(init_fbdev(&gv_fb_dev))
                return -1;

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev,false);
        fb_position_rotate(&gv_fb_dev,0);

        /* Load freetype appfonts */
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }


	/* meshCube (under local COORD) */
	int num_vertices=8;
	int num_faces=12;
	float s=140.;  /* Side of the cube */

	E3D_TriMesh	meshCube(num_vertices, num_faces);

	E3D_POINT pts[8]= {
/* 0 -3  */
0, 0, 0,
s, 0, 0,
s, s, 0,
0, s, 0,

/* 4 - 7 */
0, 0, s,
s, 0, s,
s, s, s,
0, s, s,
};

/* Vertex Index Group to define Triangles */
int VtxIndices[12*3]={
/* Bottom */
0,2,1,
0,3,2,
/* Sides */
0,5,4,
0,1,5,
1,6,5,
1,2,6,
2,3,6,
3,7,6,
7,3,0,
0,4,7,
/* Up */
4,5,6,
4,6,7,
};

	meshCube.addVertexFromPoints(pts,8);
	meshCube.addTriangleFromVtxIndx(VtxIndices, 12);
	meshCube.printAllVertices("meshCube");
	meshCube.printAllTriangles("meshCube");
        sprintf(strtmp,"Vertex: %d\nTriangle: %d", meshCube.vtxCount(), meshCube.triCount());

	/* Init. meshCube postion. (under local Coord.) */
	/* Rotation axis */
	E3D_Vector axis(1,1,1);
	axis.normalize();

	/* Prepare Transform Matrix */
	E3D_RTMatrix RTmat;
	RTmat.identity(); /* !!! setTranslation(0,0,0); */
	RTmat.setRotation(axis, 30.0/180*MATH_PI);

	//RTmat.setRotation(axis, -150.0/180*MATH_PI);
	//RTmat.setTranslation((320-s)/2, (240-s)/2, 0); /* !!! this will affect later Rotation/Translation center */

	/* Mesh Local Position: Transform the MESH :: 0,0,0 as local center */
	meshCube.transformVertices(RTmat);
	meshCube.printAllVertices("Transformed meshCube");

	/* ------ WorkMesh (Under global Coord ) ------ */
	float angle=0.0;
	E3D_TriMesh *workMesh=new E3D_TriMesh(meshCube.vtxCount(), meshCube.triCount());

   while(1) {   ///////////////   LOOP TEST   ///////////////////

	workMesh->cloneMesh(meshCube);
	cout <<"workMesh cloned!\n";

	angle +=MATH_PI/90; //180;

	/* Transform workMesh */
	RTmat.setRotation(axis, angle);
	RTmat.setTranslation((320-s)/2, (240-s)/2, 0);

	workMesh->transformVertices(RTmat);
	cout << "workMesh transformed!\n";

	/* Compute all normals */
	workMesh->updateAllTriNormals();

	/* Draw wires of the workMesh */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKPURPLE);
	gv_fb_dev.pixcolor_on=true;
	fbset_color2(&gv_fb_dev, WEGI_COLOR_GRAYB); //WEGI_COLOR_PINK);

	//cout << "Draw meshwire ...\n";
	//workMesh->drawMeshWire(&gv_fb_dev);

	cout << "Render mesh ...\n";
	workMesh->renderMesh(&gv_fb_dev);

	/* Note */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                        20, 20,                         /* fw,fh */
                                        (UFT8_PCHAR)"E3D MESH", /* pstr */
                                        300, 1, 0,                        /* pixpl, lines, fgap */
                                        320-125, 240-30,                            /* x0,y0, */
                                        WEGI_COLOR_GRAYA, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
                                        16, 16,                           /* fw,fh */
                                        (UFT8_PCHAR)strtmp, 		  /* pstr */
                                        300, 2, 4,                        /* pixpl, lines, fgap */
                                        10, 5,                            /* x0,y0, */
                                        WEGI_COLOR_GREEN, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */


	fb_render(&gv_fb_dev);


	usleep(50000);
   }

	return 0;
}
