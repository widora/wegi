/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test E3D TriMesh

ISOmetric TEST:
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
cats.obj -y -100 -X -10 -a -15 -b


	option_1:  Perspective
Bust_man.obj -s 10 -z 500 -c -y -10 -X -10 -b
cube.obj -z 650 -c -w    !!!--- Perspective view at angle=95 to demo hiding failure,  -a 10 to diminish. ---!!!
cats.obj -z 500 -c -X -40 -y 20 -a 11 -b
deer.obj -s 1 -y -100 -z 3500 -c -b -X -20
myPack.obj -s 1.5 -z 1000 -X -30 -y 100 -b
myPack.obj -s 1.5 -c -z 750 -X -30 -b -y -25  	!!! --- Check frustum clipping --- !!!
bird.obj -s 80 -z 800 -b -w -X -20    		!!! --- Check frustum clipping --- !!!
bird.obj -c -s 80 -b -X -25  Auto_z
sail.obj -z 40 -c -X -60 -R -b -y 2   !!! --- Mesh between Foucs and View_plane --- !!!
sail.obj -s 20 -z 700 -c -X -40 -b -a 5 -y 40

	----- Test FLOAT_EPSILON -----
Render mesh ... angle=0
renderMesh(): triList[0] vProduct=7.660444e-01 >=0
renderMesh(): triList[1] vProduct=7.660444e-01 >=0
renderMesh(): triList[2] vProduct=6.427876e-01 >=0
renderMesh(): triList[3] vProduct=6.427876e-01 >=0
renderMesh(): triList[4] vProduct=0.000000e+00 >=0
renderMesh(): triList[5] vProduct=0.000000e+00 >=0
renderMesh(): triList[8] vProduct=0.000000e+00 >=0
renderMesh(): triList[9] vProduct=0.000000e+00 >=0

Render mesh ... angle=90
renderMesh(): triList[0] vProduct=-3.348487e-08 >=0
renderMesh(): triList[1] vProduct=-3.348487e-08 >=0
renderMesh(): triList[10] vProduct=3.348487e-08 >=0
renderMesh(): triList[11] vProduct=3.348487e-08 >=0

Render mesh ... angle=180
renderMesh(): triList[4] vProduct=-6.696973e-08 >=0
renderMesh(): triList[5] vProduct=-6.696973e-08 >=0
renderMesh(): triList[8] vProduct=6.696973e-08 >=0
renderMesh(): triList[9] vProduct=6.696973e-08 >=0

Render mesh ... angle=270
renderMesh(): triList[0] vProduct=9.134989e-09 >=0
renderMesh(): triList[1] vProduct=9.134989e-09 >=0
renderMesh(): triList[10] vProduct=-9.134989e-09 >=0
renderMesh(): triList[11] vProduct=-9.134989e-09 >=0

Render mesh ... angle=360
renderMesh(): triList[4] vProduct=1.339395e-07 >=0
renderMesh(): triList[5] vProduct=1.339395e-07 >=0
renderMesh(): triList[8] vProduct=-1.339395e-07 >=0
renderMesh(): triList[9] vProduct=-1.339395e-07 >=0


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

3. meshModel holds the original model data, and meshWork is transformed
   for displaying at different positions.
   meshModel is under local COORD, meshWork is under global COORD.
4. Use meshWork to avoid accumulative computing error.
   XXXTODO: Use ViewPoint matrix to avoid using meshWork. ---OK
5. Sequence of Matrix multiplication matters in computing combined RTmatrix.
XXX 5. For Perspective View, wiremesh may NOT shown properly.

6.   			--- OPTION_1: WorkMesh_Movement ---

   6.1 Keep view direction as Global_COORD_Z (-z --> +z) always, just move WorkMesh.
       The Focus is behind Viewplane(Global_COORD_Z==0).
       (TODO: TransformMaxtrix to be embedded in E3D_TRIMESH?.)
   6.2 Transform workMesh to dedicated position with combine RTmatrix.
   6.3 Factor 'scale' applys to meshModel only, and workMesh keeps same size!
   6.4 For ISOmetric view: User set offx/offy to adjust mesh movement offset.
   6.5 For perspective view: as Global XY_origin and View XY_origin coincide,
       so to adjust/map Global origin to center of View window in drawMesh/AABB/.. functons.
       TODO: perspective Z value for zbuff. ???
   6.6 ----- TEST: Distance from the focus to view plane to be 500 generally. TODO

7.   			--- OPTION_2: ViewPoit_Movement ---

   7.1 Keep workMesh static, move ViewCoord(Camera), View direction as View_COORD_Z (-z-->+z).
       The Foucus is behind Viewplane(View_COORD_Z==0).

TODO:
1. NOW it uses integer type zbuff[]. ---> Should be float/double type.
2. NOW it uses triangle center Z value for zbuffering all pixels on the triangle.
       ---> Z value should be computed for each pixel!
3. Above 1+2 will cause surface hiding failure sometimes.
   Perspective view of a cube.obj at angle=95 to demo such fault.
4. Each edge of a triangle is drawn twice, as it belongs to TWO trianlges.
   ( Edge data should be stored and drawn independtly )
5. Fixed point calculation in some draw_functions makes triangle wire lines miss_align
   with filled_triangle edge.

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
2021-08-13:
	1. Test OPTION_1: perspective view.
2021-08-17/18/19:
	Test: Perspectiveiew, E3D_draw_circle().
2021-08-20:
	Test coordinate navigating sphere/frame.

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
#include "egi_timer.h"

using namespace std;


void print_help(const char *name)
{
	printf("Usage: %s obj_file [-hrRPwbtcs:x:y:z:A:X:Y:Z:a:p:]\n", name);
	printf("-h     Help\n");
	printf("-r     Reverse trianlge normals\n");
	printf("-R     Reverse vertex Z direction.\n");
	printf("-P     Perspective ON\n");
	printf("-w     Wireframe ON\n");
	printf("-b     AABB ON\n");
	printf("-c     Move local COORD origin to center of mesh.\n");
	printf("-t:    Show rendering process.\n");
	printf("-s:    Scale the mesh\n");
	printf("-x:    X_offset for display\n");
	printf("-y:    Y_offset for display\n");
	printf("-z:    Z_offset for display\n");
	printf("-d:    Delt angle for rotation\n");
	printf("-A:    Rotation axis, default FB_COORD_Y, 0_X, 1_Y, 2_Z \n");
	printf("-X:    X delta angle for display\n");
	printf("-Y:    Y delta angle display\n");
	printf("-Z:    Z delta angle for display\n");
	printf("-a:    delt angle for each move\n");
	printf("-p:    Pause seconds\n");
        exit(0);
}

int main(int argc, char **argv)
{
        int             vertexCount=0;
        int             triangleCount=0;
#if 0
        int             normalCount=0;
        int             textureCount=0;
        int             faceCount=0;
#endif


	EGI_16BIT_COLOR	  bkgColor=WEGI_COLOR_GRAY5; // DARKPURPLE
	EGI_16BIT_COLOR	  faceColor=WEGI_COLOR_PINK;
	EGI_16BIT_COLOR	  wireColor=WEGI_COLOR_BLACK; //DARKGRAY;
	EGI_16BIT_COLOR	  aabbColor=WEGI_COLOR_LTBLUE;

	unsigned int	xres,yres;
	char 		strtmp[256]={0};
	const char 	*fobj;

	float		scale=1.0f;
	float		offx=0.0f, offy=0.0f, offz=0.0f;
	float		rotX=0.0f, rotY=0.0f, rotZ=0.0f;
	float		da=0.0f;	/* Delta angle for rotation */
	int		psec=0;		/* Pause second */

	int		rotAxis=1;	/* Under FB_COORD, 0-X, 1-Y, 2-Z */
	bool		reverseNormal=false;	/* Adjust after obj input */
	bool		reverseZ=false;		/* Adjust after obj input */
	bool		perspective_on=false;
	bool		wireframe_on=false;
	bool		autocenter_on=false;	/* Auto. move local COORD origin to center of the mesh */
	bool		process_on=false;       /* FB_direct to show rendering process */
	bool		AABB_on =false;		/* Draw axially aligned bounding box */
	bool		flipXYZ_on =false;	/* Flip XYZ for screen coord */

	/* Projectionn matrix */
	float		dview;		/* Distance from the Focus to the Screen/ViewPlane */
	float		dvv;		/* varying of dview */
	float		dstep;
	E3D_ProjMatrix projMatrix={ .type=E3D_ISOMETRIC_VIEW, .dv=500, .dnear=500, .dfar=10000000, .winW=320, .winH=240};

        /* Parse input option */
	int opt;
        while( (opt=getopt(argc,argv,"hrRPwbtcs:x:y:z:A:X:Y:Z:a:p:"))!=-1 ) {
                switch(opt) {
                        case 'h':
				print_help(argv[0]);
				break;
                        case 'r':
                                reverseNormal=true;
                                break;
			case 'R':
				reverseZ=true;
				break;
			case 'P':
				perspective_on=true;
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
			case 'z':
				offz=atoi(optarg);
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
		print_help(argv[0]);
                exit(-1);
        }

#if 0	/* Start EGI Tick */
  	printf("tm_start_egitick()...\n");
  	tm_start_egitick();
#endif

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

	/* Set Projection type */
	if(perspective_on)
		projMatrix.type=E3D_PERSPECTIVE_VIEW;
	else
		projMatrix.type=E3D_ISOMETRIC_VIEW;
	/* Set projection screen size */
	projMatrix.winW=gv_fb_dev.pos_xres;
	projMatrix.winH=gv_fb_dev.pos_yres;

	/* Get meshModel statistics */
	//readObjFileInfo(fobj, vertexCount, triangleCount,normalCount,textureCount,faceCount);
	vertexCount=meshModel.vtxCount();
	triangleCount=meshModel.triCount();
	sprintf(strtmp,"Vertex: %d\nTriangle: %d\n%s", vertexCount, triangleCount,
				projMatrix.type?(UFT8_PCHAR)"Perspective":(UFT8_PCHAR)"ISOmetric" );


#if 1 //////////////////////  OPTION_1. Move/Scale workMesh and Keep View Direction as Global_COORD_Z (-z-->+z)  /////////////////////////
			   /*  Loop Rendering: renderMesh(fbdev)  */

	/* 1. Reverse Z/Normal after reading obj file. */
	if(reverseZ)
		meshModel.reverseAllVtxZ(); /* To updateAllTriNormals() later.. */
	if(reverseNormal)
		meshModel.reverseAllTriNormals(false); /* Reverse triangle vtxIndex also. */

	/* 2. Scale up/down */
	cout<< "Scale mesh..."<<endl;
	meshModel.scaleMesh(scale);
	//meshModel.printAllVertices("meshModel scaled");

        /* 3. Now compute all triangle normals, before draw wireframe(with normal line maybe) */
	cout<< "Update all triNormals..."<<endl;
        meshModel.updateAllTriNormals();

#if 1   /* 4. Draw mesh wireframe. */
	cout<< "Draw wireframe..."<<endl;
        fb_clear_workBuff(&gv_fb_dev, bkgColor); //DARKPURPLE);
	fbset_color2(&gv_fb_dev, WEGI_COLOR_PINK);
	meshModel.drawMeshWire(&gv_fb_dev, projMatrix);
	fb_render(&gv_fb_dev);
	cout<< "Finish wireframe under local coord..."<<endl;
	sleep(2);
#endif
	/* 5. Move meshModel center to its local Origin.  */
	if(autocenter_on) {
		#if 0
		meshModel.updateVtxsCenter();
		meshModel.moveVtxsCenterToOrigin(); /* AABB updated also */
		#else
		meshModel.updateAABB();
		meshModel.moveAabbCenterToOrigin(); /* AABB updated also */
		#endif
	}
	//meshModel.printAABB("Scaled meshModel");

	/* 6. Calculate dview, as distance from the Focus to the Screen */
	dview=meshModel.AABBdSize();  /* Taken dview == objSize */
	dvv=dview*2; //dview/5;		      /* To change dvv by dstep later... */
	dstep=0; //dview/40;
	projMatrix.dv=dview;
	cout << "dview=" <<dview<<endl;
	sleep(1);

	/* 7. Init. E3D Vector and Matrixes for meshModel position. */
	E3D_Vector axisX(1,0,0);
	E3D_Vector axisY(0,1,0);

	/* Prepare Transform Matrix */
	E3D_RTMatrix RTYmat, RTXmat;
	E3D_RTMatrix ScaleMat;
	E3D_RTMatrix AdjustMat;
	E3D_RTMatrix TMPmat;

	RTXmat.identity(); /* !!! setTranslation(0,0,0); */
	RTYmat.identity();
	ScaleMat.identity();
	TMPmat.identity();
	AdjustMat.identity();

	AdjustMat.setTranslation(gv_fb_dev.pos_xres/2, gv_fb_dev.pos_yres/2,0);

	/* 8. Prepare WorkMesh (Under global Coord ) */
	int angle=0;
	E3D_TriMesh *workMesh=new E3D_TriMesh(meshModel.vtxCount(), meshModel.triCount());

  while(1) {

	/* W1. Clone meshModel to workMesh, with vertices/triangles/normals */
	cout <<"Clone workMesh... \n";
	workMesh->cloneMesh(meshModel); /* include AABB */

	/* W2. Prepare transform matrix */
	/* W2.1 Rotate around axis_Y under its local coord. */
	RTYmat.identity(); /* !!!Reset */
	RTYmat.setRotation(axisY, 1.0*angle/180*MATH_PI);

	/* W2.2 Rotate around axis_X and Move to LCD center */
	RTXmat.identity();
	RTXmat.setRotation(axisX, (-140.0+rotX)/180*MATH_PI);  /* AntiCloswise. -140 for Screen XYZ flip.  */

	/* W2.3 Translation */
	/* Note:
	 * 1. without setTranslation(x,y,.), Global XY_origin and View/Screen XY_origin coincide!
	 * 2. For TEST_PERSPECTIVE,
	 */
	//RTmat.setTranslation(xres/2 +offx, yres/2 +offy, offz);
	RTXmat.setTranslation(offx, offy, offz +dview*2); /* take Focus to obj center=2*dview */

	/* W3. Transform workMesh */
	cout << "Transform workMesh...\n";
	workMesh->transformMesh(RTYmat*RTXmat, ScaleMat); /* Here, scale ==1 */

	/* W4. Update Projectoin Matrix */
	dvv += dstep;
	projMatrix.dv = dvv;

	/* W5. Set directFB to display drawing of each pixel. */
/* ---------->>> Set as NON_driectFB */
   	if( process_on ) {
	        fb_set_directFB(&gv_fb_dev,false);
	}

	/* Clear FB work_buffer */
        fb_clear_workBuff(&gv_fb_dev, bkgColor);
	fb_init_zbuff(&gv_fb_dev, INT_MIN);
	gv_fb_dev.zbuff_on=true;

/* <<<----------  Render then Set as directFB, to see rendering process. */
	if( process_on ) {
		fb_render(&gv_fb_dev);
        	fb_set_directFB(&gv_fb_dev,true);
	}


#if 1   /* W6. Render as solid faces. */
	fbset_color2(&gv_fb_dev, faceColor);

        /* OR  updateAllTriNormals() here */

        cout << "Render mesh ... angle="<< angle <<endl;
        workMesh->renderMesh(&gv_fb_dev, projMatrix);
#endif

	/* W7. Render as wire frames. */
	if(wireframe_on) {
		fbset_color2(&gv_fb_dev, wireColor); //WEGI_COLOR_DARKGRAY);
		cout << "Draw meshwire ...\n";
		workMesh->drawMeshWire(&gv_fb_dev, projMatrix);
	}

	/* W8. Draw the AABB. */
	if( AABB_on ) {
	        cout << "Draw AABB ...\n";
		fbset_color2(&gv_fb_dev, aabbColor);
		/* Use meshModel to draw AABB! */
		meshModel.drawAABB(&gv_fb_dev, RTYmat*RTXmat, projMatrix);
	}

	/* W9. Draw the Coordinate Navigating_Sphere/Frame (coordNavSphere/coordNavFrame) */
	E3D_draw_coordNavSphere(&gv_fb_dev, 0.3*workMesh->AABBdSize(), RTYmat*RTXmat, projMatrix);
	E3D_draw_coordNavFrame(&gv_fb_dev, 0.5*workMesh->AABBdSize(), RTYmat*RTXmat, projMatrix);

	/* W10. Write note & statistics */
	sprintf(strtmp,"Vertex: %d\nTriangle: %d\n%s\ndvv=%d%%",
			vertexCount, triangleCount,
			projMatrix.type?(UFT8_PCHAR)"Perspective":(UFT8_PCHAR)"Isometric",
			(int)roundf(dvv*100.0f/dview));
			//dvv*100.0f/dview); /* !!! (int) or fail to d% */

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
                                        300, 4, 4,                        /* pixpl, lines, fgap */
                                        5, 4,                             /* x0,y0, */
                                        WEGI_COLOR_GREEN, -1, 200,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	gv_fb_dev.zbuff_on=true;

	/* W12. Render to FBDEV */
	fb_render(&gv_fb_dev);
	usleep(50000);
	if(psec) usleep(psec*1000);

	/* W13. Update angle */
	angle +=(5+da); // *MATH_PI/180;
	/* To keep precision */
	if(angle >360 || angle <-360) {
		if(angle>360) angle -=360;
		if(angle<-360) angle +=360;
		faceColor=egi_color_random(color_all);
//		projMatrix.type=!projMatrix.type; /* Switch projection type */
	}

	/* W14. Update dvv: Distance from the Focus to the Screen/ViewPlane */
	dvv += dstep;
	if( dvv > 1.9*dview || dvv < 0.1*dview )
		dstep=-dstep;
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
	    VRTmat.setTranslation( -(xres/2/scale +offx/scale), -(yres/2/scale +offy/scale), offz/scale);
	else /* No flip */
	    VRTmat.setTranslation( xres/2/scale +offx/scale, yres/2/scale +offy/scale, offz/scale);

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
	if(flipXYZ_on)
           VRTmat.setTranslation( -(xres/2/scale +offx/scale), -(yres/2/scale +offy/scale), 0);
	else
           VRTmat.setTranslation( (xres/2/scale +offx/scale), (yres/2/scale +offy/scale), offz/scale); //(300+300)/scale); //  Zmax+d;  -1.2*meshModel.AABBzSize()/scale);
//           VRTmat.setTranslation( offx/scale, offy/scale, offz/scale); //(300+300)/scale); //  Zmax+d;  -1.2*meshModel.AABBzSize()/scale);

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
                                        300, 4, 4,                        /* pixpl, lines, fgap */
                                        5, 4,                             /* x0,y0, */
                                        WEGI_COLOR_GREEN, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	gv_fb_dev.zbuff_on=true;

	/* 11. Render to FBDEV */
	fb_render(&gv_fb_dev);
	usleep(75000);
	//tm_delayms(50);
	if(psec) sleep(psec);

	/* 12. Update rotation angle */
	angle +=(5+da)*MATH_PI/180;

   }

#endif ////////////////////////

	return 0;
}
