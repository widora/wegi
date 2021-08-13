/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test E3D TriMesh

teapot.obj -r -s 1.75 -X -5
teapot.obj -r -s 1.3 -X 15 -w -b
fish.obj -s 45
deer.obj -s .2 -X -30 -x 20 -y -10
deer.obj -s .15 -c -b -X -15 -y 20
bird.obj -s 45
chick.obj -s 40
myPack.obj -s 1.5 -X -20
Bust_man.obj -s 17 -X -20 -y 20
Bust_man.obj -s 8 -b -X 25 -c
Bust_man.obj -s 9.5 -c -X 20 -b -y -10 -w
Bust_man.obj -s 9.75 -c -X 15 -b -y -10 -w
man11.obj 0.5 -s 2 -y 20
Discobolus.obj -s 0.2 -a 15  (-X 40)
budda.obj  -c -s .3 -A 2 -X -120 -y 50 -a 15

Note:
1. meshModel holds the original model data, and meshWork is transformed
   for displaying at different positions.
   meshModel is under local COORD, meshWork is under global COORD.
2. Use meshWork to avoid accumulative computing error.
   XXXTODO: Use ViewPoint matrix to avoid using meshWork. ---OK


Journal:
2021-07-31:
	1. Create the file.
2021-08-07:
	1. Simple zbuff test.
2021-08-11:
	1. Draw AABB.
	2. Apply ScaleMatrix.
2021-08-12:
	1. Apply meshModel.drawMeshWire(&gv_fb_dev, VRTmat*ScaleMat);
2021-08-13:
        1. Correct transform matrixces processing, let Caller to flip
           Screen_Y with fb_position_rotate().

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
#if 0
        int             normalCount=0;
        int             textureCount=0;
        int             faceCount=0;
#endif

	unsigned int	xres,yres;
	char 		strtmp[256]={0};
	const char 	*fobj;

	float		scale=1.0f;
	float		offx=0.0f, offy=0.0f;
	float		rotX=0.0f, rotY=0.0f, rotZ=0.0f;
	float		da=0.0f;	/* Delta angle for rotation */
	int		psec=0;		/* Pause second */

	int		rotAxis=1;	/* Under FB_COORD, 0-X, 1-Y, 2-Z */
	bool		reverseNormal=false;
	bool		wireframe_on=false;
	bool		autocenter_on=false;	/* Auto. move local COORD origin to center of the mesh */
	bool		process_on=false;       /* FB_direct to show rendering process */
	bool		AABB_on =false;		/* Draw axially aligned bounding box */
	bool		flipXYZ_on =false;	/* Flip XYZ for screen coord */


        /* Parse input option */
	int opt;
        while( (opt=getopt(argc,argv,"hrwbtcs:x:y:A:X:Y:Z:a:p:"))!=-1 ) {
                switch(opt) {
                        case 'h':
				printf("Usage: %s obj_file [-hrwbtcs:x:y:A:X:Y:Z:a:p:]\n", argv[0]);
				printf("-h     Help\n");
				printf("-r     Reverse normals\n");
				printf("-w     Wireframe ON\n");
				printf("-b     AABB ON\n");
				printf("-c     Move local COORD origin to center of mesh.\n");
				printf("-t:    Show rendering process.\n");
				printf("-s:    Scale the mesh\n");
				printf("-x:    X_offset for display\n");
				printf("-y:    Y_offset for display\n");
				printf("-d:    Delt angle for rotation\n");
				printf("-A:    Rotation axis, default FB_COORD_Y, 0_X, 1_Y, 2_Z \n");
				printf("-X:    X delta angle for display\n");
				printf("-Y:    Y delta angle display\n");
				printf("-Z:    Z delta angle for display\n");
				printf("-a:    delt angle for each move\n");
				printf("-p:    Pause seconds\n");
                                exit(0);
                                break;
                        case 'r':
                                reverseNormal=true;
                                break;
			case 'w':
				wireframe_on=true;
				break;
			case 'b':
				AABB_on=true;
				break;
			case 'c':
				autocenter_on=true;
				break;
			case 't':
				process_on=true;
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
			case 'A':
				rotAxis=atoi(optarg);
				if(rotAxis<0 || rotAxis>2) rotAxis=1;
				break;
			case 'X':
				rotX=atof(optarg);
				break;
			case 'Y':
				rotY=atof(optarg);
				break;
			case 'Z':
				rotZ=atof(optarg);
				break;
			case 'p':
				psec=atoi(optarg);
				if(psec<=0)
					psec=0;
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
	cout<< "Read obj file '"<< fobj <<"' into E3D_TriMesh... \n";
	E3D_TriMesh	meshModel(fobj);
#if 0  /* TEST: -------- */
	meshModel.printAllVertices("meshModel");
	meshModel.printAllTriangles("meshModel");
#endif
	/* Calculate/update AABB */
	meshModel.updateAABB();
	meshModel.printAABB("meshModel");

	/* Get meshModel statistics */
	//readObjFileInfo(fobj, vertexCount, triangleCount,normalCount,textureCount,faceCount);
	vertexCount=meshModel.vtxCount();
	triangleCount=meshModel.triCount();
	sprintf(strtmp,"Vertex: %d\nTriangle: %d\nZbuff ON", vertexCount, triangleCount);


#if 0 //////////////////////  OPTION_1. Move/Scale workMesh and Keep View Direction as COORD_Z (-z-->+z)  /////////////////////////
			   /*  Loop Rendering: renderMesh(fbdev)  */

	/* Scale up/down */
	cout<< "Scale mesh..."<<endl;
	meshModel.scaleMesh(scale);
	//meshModel.printAllVertices("meshModel scaled");

#if 1   /* Draw wireframe positioned under FB coord, as original obj file. */
        /* Now compute all triangle normals, before draw wireframe(with normal line maybe) */
	cout<< "Update all triNormals..."<<endl;
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
	/* Move meshModel center to its local center. NOT AABB center! */
	if(autocenter_on) {
		meshModel.updateVtxsCenter();
		meshModel.moveVtxsCenterToOrigin();
	}

	/* Init. meshModel position. (under local Coord.) */
	/* Rotation axis */
	E3D_Vector axis(1,0,0);
	axis.normalize();

	/* Prepare Transform Matrix */
	E3D_RTMatrix RTmat, ScaleMat;
	RTmat.identity(); /* !!! setTranslation(0,0,0); */
	ScaleMat.identity();

	/* ------ WorkMesh (Under global Coord ) ------ */
	float angle=0.0;
	E3D_TriMesh *workMesh=new E3D_TriMesh(meshModel.vtxCount(), meshModel.triCount());
	E3D_Vector axis2(0,1,0);

  while(1) {

	/* Clone meshModel to workMesh, with vertices/triangles/normals */
	cout <<"Clone workMesh... \n";
	workMesh->cloneMesh(meshModel);

	/* Transform workMesh */
	/* Rotate around axis_Y under its local coord. */
	RTmat.identity(); /* !!!Reset */
	RTmat.setRotation(axis2, angle);
	#if 0
	workMesh->transformVertices(RTmat);
	workMesh->transformTriNormals(RTmat);
	#else
	workMesh->transformMesh(RTmat, ScaleMat); /* Here, scale ==1  */
	#endif

	/* Rotate around axis_X and Move to LCD center */
	cout << "Transform workMesh...\n";
	RTmat.identity(); /* !!!Reset */
	RTmat.setRotation(axis, (-140.0+rotX)/180*MATH_PI);  /* AntiCloswise */
	RTmat.setTranslation(xres/2 +offx, yres/2 +offy, 0);
	#if 0
	workMesh->transformVertices(RTmat);
	workMesh->transformTriNormals(RTmat);
	#else
	workMesh->transformMesh(RTmat,ScaleMat); /* Here, scale ==1 */
	#endif

/* ---------->>> Set as NON_driectFB */
   	if( process_on ) {
	        fb_set_directFB(&gv_fb_dev,false);
	}

	/* Clear FB work_buffer */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKPURPLE);
	fb_init_zbuff(&gv_fb_dev, INT_MIN);
	gv_fb_dev.zbuff_on=true;

/* <<<----------  Render then Set as directFB, to see rendering process. */
	if( process_on ) {
		fb_render(&gv_fb_dev);
        	fb_set_directFB(&gv_fb_dev,true);
	}

#if 1   /* Render as SolidMesh */
	fbset_color2(&gv_fb_dev, WEGI_COLOR_PINK);

        /* OR  Compute all normals */
//      workMesh->updateAllTriNormals();

        cout << "Render mesh ...\n";
        workMesh->renderMesh(&gv_fb_dev);
#endif
	/* Render as WireFrame */
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
                                        5, 4,                             /* x0,y0, */
                                        WEGI_COLOR_GREEN, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	gv_fb_dev.zbuff_on=true;

	/* Render */
	fb_render(&gv_fb_dev);
	usleep(50000);
	if(psec) sleep(psec);

	/* Update angle */
	angle +=(4+da)*MATH_PI/180;
   }

#else //////////////////////  OPTION_2. Keep meshModel and Change View Postion  /////////////////////////
			   /*  Loop Rendering: renderMesh(fbdev, &VRTMatrix)  */

	/* Rotation axis/ Normalized. */
	E3D_Vector axisX(1,0,0);
	E3D_Vector axisY(0,1,0);
	E3D_Vector axisZ(0,0,1);
	E3D_Vector vctXY;  /* z==0 */

	/* Prepare View_Coord VRTMatrix as relative to Global_Coord. */
	E3D_RTMatrix VRTmat;
	E3D_RTMatrix RXmat, RYmat, RZmat;
	E3D_RTMatrix ScaleMat;
	float angle=0.0;

	/* Scale matrix. NOTE: If ScaleX/Y/Z is NOT the same, then all normals should be re-calculated, see in renderMesh(). */
	ScaleMat.identity();
	ScaleMat.setScaleXYZ(scale, scale, scale);
	VRTmat.identity();
	/* View_Coord translation relative to FB(Global) Coord. */
	if(flipXYZ_on)  /* Flip for Screen coord, same as in drawXXX()  */
	    VRTmat.setTranslation( -(xres/2/scale +offx/scale), -(yres/2/scale +offy/scale), 0);
	else /* No flip */
	    VRTmat.setTranslation( xres/2/scale +offx/scale, yres/2/scale +offy/scale, 0);

	if(flipXYZ_on)  /* Flip for Screen coord, same as in drawXXX()  */
	   VRTmat = VRTmat*(-1.0);

	/* 1. Move meshModel center to its local/obj center. NOT AABB center! */
	if(autocenter_on) {
		meshModel.updateVtxsCenter();
		meshModel.moveVtxsCenterToOrigin();
		/* Update AABB after moveVtxsCenterToOrigin() */
		meshModel.updateAABB();
	}

   	/* 2. Draw wireframe positioned under FB coord, as original obj file if NOT  */
        /* 2.2 Now compute all triangle normals, before draw wireframe(with normal line maybe) */
	cout<< "Update all triNormals..."<<endl;
        meshModel.updateAllTriNormals();
	if(reverseNormal)
		meshModel.reverseAllTriNormals();

	/* 2.3 Draw wireframe under local/obj COORD. */
        fb_position_rotate(&gv_fb_dev, 2); /* flip Y */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKPURPLE);
	fbset_color2(&gv_fb_dev, WEGI_COLOR_PINK);
	gv_fb_dev.zbuff_on=false;
	meshModel.drawMeshWire(&gv_fb_dev, VRTmat*ScaleMat);
	fb_render(&gv_fb_dev);
	sleep(2);

  while(1) {

	/* 3. Prepare transform matrix */
	/* VRTmat as View_Coord position relative to Screen/Global_Coord */
	cout<<"Set VRTmat...\n";
	VRTmat.identity(); /* !!!Reset */
	RXmat.identity();
	RYmat.identity();
	RZmat.identity();

	/* 4. Rotation axis and angle, rotAxis MUST NOT be same as rotX/Y/Z!!! */
	/* Note: Sequence of Matrix multiplication matters!!!
	 * Considering distances for vertices to the center are NOT the same!
	 */
	if( rotAxis==0 )  { /* 0 FB_COORD_X */
		RXmat.setRotation(axisX, angle);
		RYmat.setRotation(axisY, rotY/180*MATH_PI);
		RZmat.setRotation(axisZ, rotZ/180*MATH_PI);
		VRTmat=(RXmat*RYmat)*RZmat;
	}
	else if( rotAxis==2 ) { /* 2 FB_COORD_Z */
		RZmat.setRotation(axisZ, angle);
		RXmat.setRotation(axisX, rotX/180*MATH_PI);
		RYmat.setRotation(axisY, rotY/180*MATH_PI);
		VRTmat=(RZmat*RYmat)*RXmat;
	}
	else { /* whatever, 1 FB_COORD_Y */
		RYmat.setRotation(axisY, angle);
		RXmat.setRotation(axisX, rotX/180*MATH_PI);
		RZmat.setRotation(axisZ, rotZ/180*MATH_PI);
		VRTmat=(RYmat*RXmat)*RZmat;
	}

	/* 5. Combine MATRIX: Combine rotation matrixes frist, then translation matrix. */
	if(flipXYZ_on) /* Flip for Screen coord, same as in drawXXX()  */
	   VRTmat.setTranslation( -(xres/2/scale +offx/scale), -(yres/2/scale +offy/scale), 0); /* Notice View_Coord Screen/Global_Coord */
	else /* No flip */
	   VRTmat.setTranslation( xres/2/scale +offx/scale, yres/2/scale +offy/scale, 0);

	if(flipXYZ_on)  /* Flip for Screen coord, same as in drawXXX()  */
	   VRTmat = VRTmat*(-1.0);

	/* 6. Clear/Prepare FB workbuffer */
	/* ---------->>>  Set as NON_driectFB */
	if(process_on) {
        	fb_set_directFB(&gv_fb_dev,false);
	}

	/* Flip Screen Y */
        fb_position_rotate(&gv_fb_dev, 2);

	/* Clear FB work_buffer */
        fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKPURPLE);
	fb_init_zbuff(&gv_fb_dev, INT_MIN);
	gv_fb_dev.zbuff_on=true;

	/* <<<----------  Render BKG then Set as directFB, to see rendering process. */
	if(process_on) {
		fb_render(&gv_fb_dev);
        	fb_set_directFB(&gv_fb_dev,true);
	}

   	/* 7. Draw solid mesh */
        cout << "Render mesh ...\n";
	fbset_color2(&gv_fb_dev, WEGI_COLOR_PINK);
        meshModel.renderMesh(&gv_fb_dev, VRTmat, ScaleMat);

	/* 8. Draw AABB */
	if( AABB_on ) {
	        cout << "Draw AABB ...\n";
		fbset_color2(&gv_fb_dev, WEGI_COLOR_GRAY2);
		meshModel.drawAABB(&gv_fb_dev, VRTmat*ScaleMat);
	}

	/* 9. Draw WireFrame */
	if(wireframe_on) {
		fbset_color2(&gv_fb_dev, WEGI_COLOR_DARKGRAY);
		cout << "Draw meshwire ...\n";
		meshModel.drawMeshWire(&gv_fb_dev, VRTmat*ScaleMat);
	}

	/* 10. Note & statistics */
        /* Reset Screen Y for EGI functions */
        fb_position_rotate(&gv_fb_dev, 0);
	/* Zbuff OFF */
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
                                        5, 4,                             /* x0,y0, */
                                        WEGI_COLOR_GREEN, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	gv_fb_dev.zbuff_on=true;

	/* 11. Render to FBDEV */
	fb_render(&gv_fb_dev);
	usleep(50000);
	if(psec) sleep(psec);

	/* 12. Update rotation angle */
	angle +=(4+da)*MATH_PI/180;

   }

#endif ////////////////////////

	return 0;
}
