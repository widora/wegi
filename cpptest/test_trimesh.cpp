/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test E3D TriMesh

teapot.obj -r -s 1.75 -X -5
fish.obj -s 45
deer.obj -s .2 -X -30 -x 20 -y -10
bird.obj -s 45
chick.obj -s 40
myPack.obj -s 1.5 -X -20
Bust_man.obj -s 17 -X -20 -y 20
man11.obj 0.5 -s 2 -y 20
Discobolus.obj -s 0.2 -a 15

Note:
1. meshModel holds the original model data, and meshWork is transformed
   for displaying at different positions.
   meshModel is under local COORD, meshWork is under global COORD.
2. Use meshWork to avoid accumulative computing error.
   TODO: Use ViewPoint matrix to avoid using meshWork.

Journal:
2021-07-31:
	1. Create the file.
2021-08-07:
	1. Simple zbuff test.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <iostream>
#include <stdio.h>
#include <limits.h>
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

	unsigned int	xres,yres;
	char 		strtmp[256]={0};
	const char 	*fobj;

	float		scale=1.0f;
	float		offx=0.0f, offy=0.0f;
	float		rotX=0.0f;
	float		da=0.0f;	/* Delta angle for rotation */

	bool		reverseNormal=false;
	bool		wireframe_on=false;


#if 0
	readObjFileInfo("/tmp/Chick.obj", vertexCount, triangleCount,normalCount,textureCount,faceCount);
        egi_dpstd("Statistics:  Vertices %d;  Normals %d;  Texture %d; Faces %d; Triangle %d.\n",
                                vertexCount, normalCount, textureCount, faceCount, triangleCount );

#endif


        /* Parse input option */
	int opt;
        while( (opt=getopt(argc,argv,"hrws:x:y:X:a:"))!=-1 ) {
                switch(opt) {
                        case 'h':
				printf("Usage: %s obj_file [-hrws:x:y:X:a:]\n", argv[0]);
				printf("-h     Help\n");
				printf("-r     Reverse normals\n");
				printf("-w     Wireframe ON\n");
				printf("-s:    Scale the mesh\n");
				printf("-x:    X_offset for display\n");
				printf("-y:    Y_offset for display\n");
				printf("-d:    Delt angle for rotation\n");
				printf("-X:    X_rotate for display\n");
                                exit(0);
                                break;
			case 'w':
				wireframe_on=true;
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
			case 'a':
				da=atoi(optarg);
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
	xres=gv_fb_dev.vinfo.xres;
	yres=gv_fb_dev.vinfo.yres;

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev,false);
        fb_position_rotate(&gv_fb_dev,0);
	gv_fb_dev.pixcolor_on=true;		/* Pixcolor ON */
	gv_fb_dev.zbuff_on = true;		/* Zbuff ON */
	fb_init_zbuff(&gv_fb_dev, INT_MIN);

        /* Load freetype appfonts */
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

	/* Set global light vector */
	E3D_Vector vLight(1,1,4);
	vLight.normalize();
	gv_vLight=vLight;

	/* Read obj file to meshModel */
	E3D_TriMesh	meshModel(fobj);
	meshModel.printAllVertices("meshModel");
	meshModel.printAllTriangles("meshModel");

	/* Get meshModel statistics */
	readObjFileInfo(fobj, vertexCount, triangleCount,normalCount,textureCount,faceCount);
	sprintf(strtmp,"Vertex: %d\nTriangle: %d\nZbuff ON", vertexCount, triangleCount);

	/* Scale up/down */
	meshModel.scaleMesh(scale);
	//meshModel.printAllVertices("meshModel scaled");

#if 1   /* Draw wireframe positioned under FB coord, as original obj file. */
        /* Now compute all triangle normals, before draw wireframe(with normal line maybe) */
        meshModel.updateAllTriNormals();
	if(reverseNormal)
		meshModel.reverseAllTriNormals();

	/* Draw wireframe under local coord. */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKPURPLE);
	fbset_color2(&gv_fb_dev, WEGI_COLOR_PINK);
	meshModel.drawMeshWire(&gv_fb_dev);
	fb_render(&gv_fb_dev);
	sleep(2);
#endif
	/* Move mesh to its local center */
	meshModel.updateVtxsCenter();
	meshModel.moveVtxsCenterToOrigin();

	/* Init. meshModel position. (under local Coord.) */
	/* Rotation axis */
	E3D_Vector axis(1,0,0);
	axis.normalize();

	/* Prepare Transform Matrix */
	E3D_RTMatrix RTmat;
	RTmat.identity(); /* !!! setTranslation(0,0,0); */

	/* ------ WorkMesh (Under global Coord ) ------ */
	float angle=0.0;
	E3D_TriMesh *workMesh=new E3D_TriMesh(meshModel.vtxCount(), meshModel.triCount());
	E3D_Vector axis2(0,1,0);

while(1) {

	/* Clone meshModel to workMesh, with vertices/triangles/normals */
	workMesh->cloneMesh(meshModel);
	cout <<"workMesh cloned!\n";

	angle +=(4+da)*MATH_PI/180; //180;

	/* Transform workMesh */
	/* Rotate around axis_Y under its local coord. */
	RTmat.identity(); /* !!!Reset */
	RTmat.setRotation(axis2, angle);
	#if 0
	workMesh->transformVertices(RTmat);
	workMesh->transformTriNormals(RTmat);
	#else
	workMesh->transformMesh(RTmat);
	#endif

	/* Rotate around axis_X and Move to LCD center */
	RTmat.identity(); /* !!!Reset */
	RTmat.setRotation(axis, (-140.0+rotX)/180*MATH_PI);  /* AntiCloswise */
	RTmat.setTranslation(xres/2 +offx, yres/2 +offy, 0);
	#if 0
	workMesh->transformVertices(RTmat);
	workMesh->transformTriNormals(RTmat);
	#else
	workMesh->transformMesh(RTmat);
	#endif

	cout << "workMesh transformed!\n";

	/* Clear FB work_buffer */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKPURPLE);
	fb_init_zbuff(&gv_fb_dev, INT_MIN);

#if 1   /* Solid */
	fbset_color2(&gv_fb_dev, WEGI_COLOR_PINK);

        /* OR  Compute all normals */
//      workMesh->updateAllTriNormals();

        cout << "Render mesh ...\n";
        workMesh->renderMesh(&gv_fb_dev);
#endif
	/* WireFrame */
	if(wireframe_on) {
		fbset_color2(&gv_fb_dev, WEGI_COLOR_DARKGRAY);

		cout << "Draw meshwire ...\n";
		workMesh->drawMeshWire(&gv_fb_dev);
	}

	/* Note & statistics */
	gv_fb_dev.zbuff_on=false;
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                        20, 20,                         /* fw,fh */
                                        (UFT8_PCHAR)"E3D MESH", /* pstr */
                                        300, 1, 0,                        /* pixpl, lines, fgap */
                                        xres-130, yres-28,                /* x0,y0, */
                                        WEGI_COLOR_OCEAN, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
                                        16, 16,                           /* fw,fh */
                                        (UFT8_PCHAR)strtmp, 		  /* pstr */
                                        300, 3, 4,                        /* pixpl, lines, fgap */
                                        5, 4,                            /* x0,y0, */
                                        WEGI_COLOR_GREEN, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	gv_fb_dev.zbuff_on=true;

	/* Render */
	fb_render(&gv_fb_dev);

	usleep(50000);
   }

	return 0;
}
