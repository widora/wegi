/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test sphere as regular polyedron.

Note:
1. meshModel holds the original model data, and meshWork is transformed
   for displaying at different positions.

Journal:
2021-8-5: Create the file.

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
        int             vertexCount=0;
        int             triangleCount=0;
        int             normalCount=0;
        int             textureCount=0;
        int             faceCount=0;

	char 		strtmp[256]={0};
	const char 	*fobj;

	float		scale=1.0f;
	float		offx=0.0f, offy=0.0f;
	float		rotX=0.0f;

	bool		reverseNormal=false;


        /* Parse input option */
	int opt;
        while( (opt=getopt(argc,argv,"hrs:x:y:X:"))!=-1 ) {
                switch(opt) {
                        case 'h':
				printf("Usage: %s obj_file [-hrs:x:y:X:]\n", argv[0]);
				printf("-h     Help\n");
				printf("-r     Reverse normals\n");
				printf("-s:    Scale the mesh\n");
				printf("-x:    X_offset for display\n");
				printf("-y:    Y_offset for display\n");
				printf("-X:    X_rotate for display\n");
                                exit(0);
                                break;
                        case 'r':
                                reverseNormal=true;
                                break;
                        case 's':
				scale=atof(optarg);
				if(scale<0) scale=1.0;
                                break;
			case 'x':
				offx=atoi(optarg);
				break;
			case 'y':
				offy=atoi(optarg);
				break;
			case 'X':
				rotX=atof(optarg);
				break;
		}
	}
        fobj=argv[optind];
        if(fobj==NULL) {
		printf("Usage: %s obj_file [-hrs:x:y:]\n", argv[0]);
                exit(-1);
        }

        /* Initilize sys FBDEV */
        printf("init_fbdev()...\n");
        if(init_fbdev(&gv_fb_dev))
                return -1;

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev,false);
        fb_position_rotate(&gv_fb_dev,0);
	gv_fb_dev.pixcolor_on=true;

        /* Load freetype appfonts */
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

	/* Set global light vector */
	E3D_Vector vLight(1,1,4);
	vLight.normalize();
	gv_vLight=vLight;


	/* Create sphere mesh */
	E3D_TriMesh	mesh20(fobj);
	E3D_TriMesh     mesh80(mesh20, sqrt(100.0f*100.0f+61.8f*61.8f));
	E3D_TriMesh     mesh320(mesh80, sqrt(100.0f*100.0f+61.8f*61.8f));
	E3D_TriMesh     meshModel(mesh320, sqrt(100.0f*100.0f+61.8f*61.8f));

	meshModel.printAllVertices("meshModel");
	meshModel.printAllTriangles("meshModel");

	/* Re_get meshModel statistics */
	vertexCount=meshModel.vtxCount();
	triangleCount=meshModel.triCount();
	sprintf(strtmp,"Vertex: %d\nTriangle: %d", vertexCount, triangleCount);

	/* Scale up */
	meshModel.scaleMesh(scale);
	//meshModel.printAllVertices("meshModel scaled");

	/* Init. meshModel postion. (under local Coord.) */
	/* Rotation axis */
	E3D_Vector axis(1,0,0);
	axis.normalize();

	/* Prepare Transform Matrix */
	E3D_RTMatrix RTmat;
	RTmat.identity(); /* !!! setTranslation(0,0,0); */

	/* Mesh Local Position: Transform the MESH :: 0,0,0 as local center */
	meshModel.transformVertices(RTmat);
	meshModel.printAllVertices("Transformed meshModel");

        /* Now compute all triangle normals */
        meshModel.updateAllTriNormals();
	if(reverseNormal)
		meshModel.reverseAllTriNormals();

	/* Draw wireframe under local coord. */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKPURPLE);
	fbset_color2(&gv_fb_dev, WEGI_COLOR_PINK);
	meshModel.drawMeshWire(&gv_fb_dev);
	fb_render(&gv_fb_dev);
	sleep(2);

	/* ------ WorkMesh (Under global Coord ) ------ */
	float angle=0.0;
	E3D_TriMesh *workMesh=new E3D_TriMesh(meshModel.vtxCount(), meshModel.triCount());
	E3D_Vector axis2(0,1,0);

while(1) {

	/* Clone meshModel to workMesh, with vertices/triangles/normals */
	workMesh->cloneMesh(meshModel);
	cout <<"workMesh cloned!\n";

	angle +=MATH_PI/90; //180;

	/* Transform workMesh */
	/* Rotate around axis_Y under its local coord. */
	RTmat.identity(); /* !!! reset RTmat*/
	RTmat.setRotation(axis2, angle);
	#if 0
	workMesh->transformVertices(RTmat);
	workMesh->transformTriNormals(RTmat);
	#else
	workMesh->transformMesh(RTmat);
	#endif

	/* Rotate around axis_X and Move to LCD center */
	RTmat.identity(); /* !!! setTranslation(0,0,0); */
	RTmat.setRotation(axis, -140.0/180*MATH_PI+rotX);  /* AntiCloswise */
	RTmat.setTranslation(320/2 +offx, 240/2 +offy, 0);
	#if 0
	workMesh->transformVertices(RTmat);
	workMesh->transformTriNormals(RTmat);
	#else
	workMesh->transformMesh(RTmat);
	#endif

	cout << "workMesh transformed!\n";

	/* Clear FB work_buffer */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKPURPLE);

#if 1   /* Solid */
	fbset_color2(&gv_fb_dev, WEGI_COLOR_PINK);

        /* OR  Compute all normals */
//      workMesh->updateAllTriNormals();

        cout << "Render mesh ...\n";
        workMesh->renderMesh(&gv_fb_dev);
#endif
#if 1  /* WireFrame */
	fbset_color2(&gv_fb_dev, WEGI_COLOR_DARKGRAY);

	cout << "Draw meshwire ...\n";
	workMesh->drawMeshWire(&gv_fb_dev);
#endif
	/* Note */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                        20, 20,                         /* fw,fh */
                                        (UFT8_PCHAR)"E3D MESH", /* pstr */
                                        300, 1, 0,                        /* pixpl, lines, fgap */
                                        320-125, 240-30,                            /* x0,y0, */
                                        WEGI_COLOR_GRAYA, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	/* Statistics */
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
