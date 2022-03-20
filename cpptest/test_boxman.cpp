/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

		>>>>>  EGi_BOXMAN  <<<<
boxman.obj -c -s 6 -X 200 -Y 155 -w	!!! --- Test Sub_object group: omat --- !!!
boxman.obj -c -s 12 -X 200 -Y 155 -P -G
boxman.obj -c -s 8 -Z 90 -X -20 -L             <--- Portrait --->
boxman.obj -c -s 16 -Y 155 -Z 90 -X -20 -P -L  <--- Portrait --->
boxman.obj -c -s 16 -Y 155 -Z 90 -X 20 (OR -20) -a -3 -P -L  !!! --- For Demo --- !!!
boxman.obj -c -s 16 -Y 300 -Z 90 -X 20 -a -5 -P -L   ((  Back/Up/Squit_angle View ))
boxman.obj -c -s 16 -Y -195 -Z 90 -X 20 -a -5 -P -L -M /mmc/boxman_hand.mot   !!! --- For arm_roll DEMO --- !!!

boxdog.obj -s 12 -y 150 -X 190 -Y -60 -P -L -G


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

Journal:
2021-10-15: Create the file.

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
------------------------------------------------------------------*/
#include <iostream>
#include <stdio.h>
#include <limits.h>

#include "egi_math.h"
#include "egi_debug.h"
#include "egi_fbdev.h"
#include "egi_color.h"
#include "egi_FTsymbol.h"
#include "egi_timer.h"
#include "egi_bjp.h"

#include "e3d_vector.h"
#include "e3d_trimesh.h"

using namespace std;

#define MOTION_FILE  "/mmc/mesh.motion"

void print_help(const char *name)
{
	printf("Usage: %s obj_file [-hrbBDGNRSPVLwbtcf::s:x:y:z:A:M:X:Y:Z:T:a:p:]\n", name);
	printf("-h     Help\n");
	printf("-r     Reverse trianlge normals\n");
	printf("-B     Display back faces with default color, OR texture for back faces also.\n");
	printf("-D     Show mesh detail statistics and other info.\n");
	printf("-G     Show grid.\n");
	printf("-N     Show coordinate navigating sphere/frame.\n");
	printf("-R     Reverse vertex Z direction.\n");
	printf("-S     Save serial FB images and loop playing.\n");
	printf("-P     Perspective ON\n");
	printf("-V     Auto compute all vtxNormals if no provided in data, for gouraud shading.\n");
	printf("-L     Adjust lighting direction.\n");
	printf("-w     Wireframe ON\n");
	printf("-b     AABB ON\n");
	printf("-c    Move local COORD origin to center of mesh, 0-VtxCenter 1-AABB Center.\n");
	printf("-t:    Show rendering process.\n");
	printf("-s:    Scale the mesh\n");
	printf("-x:    X_offset for display, relative to ScreenCoord.\n");
	printf("-y:    Y_offset for display\n");
	printf("-z:    Z_offset for display\n");
	printf("-A:    0,1,2 Rotation axis, default FB_COORD_Y, 0_X, 1_Y, 2_Z \n");
	printf("-M:    Save frames to a motion file.\n");
	printf("-X:    X delta angle for display\n");
	printf("-Y:    Y delta angle display\n");
	printf("-Z:    Z delta angle for display\n");
	printf("-T:    Texture image file(jpg,png).\n");
	printf("-f:    Texture resize factor.\n");
	printf("-a:    delt angle for each move/rotation.\n");
	printf("-p:    Pause mseconds\n");
        exit(0);
}


/*-----------------------------
	    MAIN()
-----------------------------*/
int main(int argc, char **argv)
{
	int i;
        int             vertexCount=0;
        int             triangleCount=0;

	/* 24bit color ref
	 0xD0BC4B--Golden
	 0xB8DCF0--Milk white
	 0xF0CCA8--Bronze
	*/

	EGI_16BIT_COLOR	  bkgColor=WEGI_COLOR_GRAY3;//GREEN; //GRAY5; // DARKPURPLE
	EGI_16BIT_COLOR	  faceColor=COLOR_24TO16BITS(0xF0CCA8);//WEGI_COLOR_PINK;
	EGI_16BIT_COLOR	  wireColor=WEGI_COLOR_BLACK; //DARKGRAY;
	EGI_16BIT_COLOR	  aabbColor=WEGI_COLOR_LTBLUE;
	EGI_16BIT_COLOR	  fontColor=WEGI_COLOR_GREEN;
	EGI_16BIT_COLOR	  fontColor2=WEGI_COLOR_PINK;//GREEN;
	EGI_16BIT_COLOR	  gridColor=WEGI_COLOR_DARKPURPLE;

	char 		*textureFile=NULL;	/* Fpath */

	char		fpath[1024];		/* File path for saving JPG */
	EGI_IMGBUF	*fbimg=NULL;		/* A ref. to FB data */
	EGI_IMGBUF 	*simg=NULL;		/* Serial imgbuf for motion picture */
	EGI_IMGBUF	*refimg=NULL;		/* Temp. ref. to workMesh->textureImg */
	int		imgcnt=0;		/* Counter of subimg of simg */

	unsigned int	xres,yres;
	char 		strtmp[256]={0};
	const char 	*fobj;
	const char	*strShadeType=NULL;
	char		*fmotion=NULL;		/* File path for motion file */

	float		scale=1.0f;
	float		offx=0.0f, offy=0.0f, offz=0.0f;
	float		rotX=0.0f, rotY=0.0f, rotZ=0.0f;
	float		da=0.0f;	/* Delta angle for rotation */
	int		psec=0;		/* Pause second */

	int		rotAxis=1;	/* Under FB_COORD, 0-X, 1-Y, 2-Z */
	float		texScale=-1.0;   /* Texture image resize factor, valid only >0.0f!  */
	bool		reverseNormal=false;	/* Adjust after obj input */
	bool		reverseZ=false;		/* Adjust after obj input */
	bool		perspective_on=false;
	bool		wireframe_on=false;
	bool		autocenter_on=false;	/* Auto. move local COORD origin to center of the mesh */
	bool		process_on=false;       /* FB_direct to show rendering process */
	bool		AABB_on =false;		/* Draw axially aligned bounding box */
//	bool		flipXYZ_on =false;	/* Flip XYZ for screen coord */
	bool		coordNavigate_on=false; /* Show coordinate navigating sphere/frame */
	bool		showInfo_on=false;	/* Show mesh detail statistics and other info. */
	bool		serialPlay_on=false;	/* Save serial FB image and then loop play */
	bool		saveMotion_on=false;	/* Save screen as a frame of a motion file */
	bool		backFace_on=false;	/* Display back faces */
	bool		calVtxNormals_on=false; /* To compute all vtxNormals if not provided in data, for gouraud shading. */
	bool		adjustLight_on=false;	/* Adjust lighting direction */
	bool		showGrid_on=false;	/* Show grid */

	/* Projectionn matrix */
	float		dview;		/* Distance from the Focus(usually originZ) to the Screen/ViewPlane */
	float		dvv;		/* varying of dview */
	float		dstep;
	E3D_ProjMatrix projMatrix={ .type=E3D_ISOMETRIC_VIEW, .dv=500, .dnear=500, .dfar=10000000, .winW=320, .winH=240};

	/* Boxman */
	int headAng=0, pitchAng=0;
	int stepAng=5;
	int roundTok=0;

        /* Parse input option */
	int opt;
        while( (opt=getopt(argc,argv,"hrbBDGNRSPVLwbtcf:s:x:y:z:A:M:X:Y:Z:T:a:p:"))!=-1 ) {
                switch(opt) {
                        case 'h':
				print_help(argv[0]);
				break;
                        case 'r':
                                reverseNormal=true;
                                break;
			case 'B':
				backFace_on=true;
				break;
			case 'D':
				showInfo_on=true;
				break;
			case 'G':
				showGrid_on=true;
				break;
			case 'N':
				coordNavigate_on=true;
				break;
			case 'R':
				reverseZ=true;
				break;
			case 'S':
				serialPlay_on=true;
				break;
			case 'P':
				perspective_on=true;
				break;
			case 'V':
				calVtxNormals_on=true;
				break;
			case 'L':
				adjustLight_on=true;
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
			case 'f':
				texScale=atof(optarg);
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
			case 'A':
				rotAxis=atoi(optarg);
				if(rotAxis<0 || rotAxis>2) rotAxis=1;
				break;
			case 'M':
				saveMotion_on=true;
				fmotion=optarg;
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
			case 'T':
				textureFile=optarg;
				break;
			case 'a':
				da=strtof(optarg,NULL); //atof(optarg);
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

	/* Prepare fbimg, and relates to FB */
	fbimg=egi_imgbuf_alloc();
	fbimg->width=xres;
	fbimg->height=yres;
	fbimg->imgbuf=(EGI_16BIT_COLOR*)gv_fb_dev.map_fb;

        /* Load freetype appfonts */
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }


	/* 0. Set global light vector */
	float vang=45.0; /*  R=1.414, angle for vector(x,y) of vLight  */
	float dvang=5; //2.5;  /* Delta angle for vangle */
if( adjustLight_on ) {  /* For Portrait.. */
	E3D_Vector vLight(1, -1, 1); //2); //4);
	vLight.normalize();
	gv_vLight=vLight;
}
else {		/* For Landscape */
	E3D_Vector vLight(1, 1, 1); //2); //4);
	vLight.normalize();
	gv_vLight=vLight;
}

	/* 1. Read obj file to meshModel */
	cout<< "Read obj file '"<< fobj <<"' into E3D_TriMesh... \n";
	E3D_TriMesh	meshModel(fobj);
	if(meshModel.vtxCount()==0)
		exit(-1);
  	/* Read textureFile into meshModel. */
	if( textureFile ) {
		meshModel.readTextureImage(textureFile, -1); //xres);
		if(meshModel.textureImg) {
			if( texScale>0.0 ) {
			    if(egi_imgbuf_scale_update(&meshModel.textureImg,
					    texScale*meshModel.textureImg->width, texScale*meshModel.textureImg->height)==0)
			        egi_dpstd("Scale texture image size to %dx%d\n",meshModel.textureImg->width, meshModel.textureImg->height);
			    else
			        egi_dpstd("Fail to scale texture image!\n");
			}
		}
	}

#if 1  /* TEST: -------- Print out all materials ---- */
	for(unsigned int k=0; k<meshModel.mtlList.size(); k++)
			meshModel.mtlList[k].print();
#endif
	sleep(2);

	/* 2. Calculate/update AABB */
	meshModel.updateAABB();
	meshModel.printAABB("meshModel");

	/* 3. Set Projection type */
	if(perspective_on)
		projMatrix.type=E3D_PERSPECTIVE_VIEW;
	else
		projMatrix.type=E3D_ISOMETRIC_VIEW;

	/* 4. Set projection screen size */
	projMatrix.winW=xres;
	projMatrix.winH=yres;

	/* 5. Get meshModel statistics */
	vertexCount=meshModel.vtxCount();
	triangleCount=meshModel.triCount();
	sprintf(strtmp,"Vertex: %d\nTriangle: %d\n%s", vertexCount, triangleCount,
				projMatrix.type?(UFT8_PCHAR)"Perspective":(UFT8_PCHAR)"ISOmetric" );

	/* 6. Rotation axis/ Normalized. */
	E3D_Vector axisX(1,0,0);
	E3D_Vector axisY(0,1,0);
	E3D_Vector axisZ(0,0,1);
	E3D_Vector vctXY;  /* z==0 */

	/* 7. Prepare View_Coord VRTMatrix as relative to Global_Coord. */
	E3D_RTMatrix RXmat, RYmat, RZmat;	/* Rotation only */
	E3D_RTMatrix VRTmat;			/* Combined RTMatrix */
	E3D_RTMatrix ScaleMat;
	E3D_RTMatrix AdjustMat;  		/* For adjust */
	float angle=0.0;

	/* ------ 8.TEST: Save FB images to a motion file.  ------------------------- */
	if(saveMotion_on) {
		if(fmotion==NULL)
			fmotion=MOTION_FILE;
		if( egi_imgmotion_saveHeader(fmotion, xres, yres, 50, 1) !=0 )  /* (fpath, width, height, delayms, compress) */
			exit(EXIT_FAILURE);
	}


//////////////////////  OPTION_1. Move/Scale workMesh and Keep View Direction as Global_COORD_Z (-z-->+z)  /////////////////////////


	/* 1. Reverse Z/Normal after reading obj file. */
	if(reverseZ) {
		meshModel.reverseAllVtxZ(); 	   /* To updateAllTriNormals() later.. */
		meshModel.reverseAllVtxNormals();
	}
	if(reverseNormal) {
		meshModel.reverseAllTriNormals(true); /* Reverse triangle vtxIndex also. */
	}

	/* 2. Scale up/down */
	cout<< "Scale mesh..."<<endl;
	meshModel.scaleMesh(scale);
	//meshModel.printAllVertices("meshModel scaled");

        /* 3. Compute all triangle normals, before draw wireframe(maybe with normal lines) */
	cout<< "Update all triNormals..."<<endl;
        meshModel.updateAllTriNormals();

	/* 3.1 Compute vertex noramls if necessary, for gouraud shading. */
	if( calVtxNormals_on && meshModel.vtxNormalIsZero() ) {
		cout<< "Compute all vertex Normals..."<<endl;
		meshModel.computeAllVtxNormals();
	}

#if 1   /* 4. Draw mesh wireframe. */
	cout<< "Draw wireframe..."<<endl;
        fb_clear_workBuff(&gv_fb_dev, bkgColor); //DARKPURPLE);
  gv_fb_dev.zbuff_on=false;
	fbset_color2(&gv_fb_dev, WEGI_COLOR_PINK);
	meshModel.drawMeshWire(&gv_fb_dev, projMatrix);
	fb_render(&gv_fb_dev);
  gv_fb_dev.zbuff_on=true;
	cout<< "Finish wireframe under local coord..."<<endl;
	sleep(2);
#endif
	/* 5. Move meshModel center to Origin.  */
	if(autocenter_on) {
		#if 0  /* To Vtx Center */
		meshModel.updateVtxsCenter();
		meshModel.moveVtxsCenterToOrigin(); /* AABB updated also */
		#else  /* To AABB center */
		meshModel.updateAABB();
		meshModel.moveAabbCenterToOrigin(); /* AABB updated also */
		#endif
	}
	//meshModel.printAABB("Scaled meshModel");

	/* 6. Calculate dview, as distance from the Focus to the Screen */
	dview=meshModel.AABBdSize();  /* Taken dview == objSize */
	dvv=dview; //dview/5;	       /* To change dvv by dstep later... */
	dstep=0; //dview/40;
	projMatrix.dv=dview;
	cout << "dview=" <<dview<<endl;
	sleep(1);

	/* 7. Init. E3D Vector and Matrixes for meshModel position. */
	RXmat.identity(); 	/* !!! setTranslation(0,0,0); */
	RYmat.identity();
	RZmat.identity();
	VRTmat.identity();	/* Combined transform matrix */
	ScaleMat.identity();
	AdjustMat.identity();

	/* 8. Prepare WorkMesh (Under global Coord) */
	E3D_TriMesh *workMesh=new E3D_TriMesh(meshModel.vtxCount(), meshModel.triCount());

#if 0	/* 9. Check if it has input texture with option -T path. (NOT as defined in .mtl file.) */
	if( (textureFile && meshModel.textureImg) || (meshModel.mtlList.size()>0 && meshModel.mtlList[0].img_kd) ) {
		refimg=meshModel.textureImg; /* Just ref, to get imgbuf size later. */
		workMesh->shadeType=E3D_TEXTURE_MAPPING;
	}
	else {
		/* Note: if the mesh has NO nv data(all 0), then result of GOURAUD_SHADING will be BLACK */
		workMesh->shadeType=E3D_GOURAUD_SHADING;
	}
#else
		workMesh->shadeType=E3D_FLAT_SHADING;
#endif

			/* (((-----  Loop move and render workMesh  -----))) */
  while(1) {

	/* W1. Clone meshModel to workMesh, with: vertices/triangles/normals/vtxcenter/AABB/textureImg/
	 *     /defMateril/mtlList[]/triGroupList[].....
	 *   Note:
	 *	1. EGI_IMGBUF pointers of textureImg and map_kd in materials NEED NOT to free/release befor cloning,
	 *	   as they are read/loaded ONLY once.
	 *	2. mtlList[] and triGroupList[] to be cloned ONLY WHEN they are emtpy! this is to avoid free/release
   	 *	   pointer members. see in E3D_TriMesh::cloneMesh().
	 */
	cout <<"Clone meshModel data into workMesh, NOT all data is cloned and replaced! ... \n";
	workMesh->cloneMesh(meshModel);
	egi_dpstd("workMesh: %d Triangles, %d TriGroups, %d Materials, %s, defMateril.img_kd is '%s'.\n",
				workMesh->triCount(), workMesh->triGroupList.size(),
				workMesh->mtlList.size(),
				workMesh->mtlList.size()>0 ? "" : "defMaterial applys.",
				workMesh->defMaterial.img_kd!=NULL ? "loaded" : "NULL"
				);

#if 0  /* TEST: -------- Print out all materials ---- */
        for(unsigned int k=0; k < workMesh->mtlList.size(); k++)
                        workMesh->mtlList[i].print();
#endif


        /* Turn bakcFace on */
	if(backFace_on) {
 		workMesh->backFaceOn=true;
		workMesh->bkFaceColor=WEGI_COLOR_RED;
	}

//	workMesh->shadeType=E3D_GOURAUD_SHADING;

	/* W2. Prepare transform matrix */
	/* W2.1 Rotation Matrix combining computation
	 *			---- PRACTICE!! ---
	 * Note:
	 * 1. For tranforming objects, sequence of Matrix multiplication matters!!
	 * 2. If rotX/Y/Z is the rotating axis, the it will added as AdjustMat.
	 * Considering distances for vertices to the center are NOT the same!
	 *  Rotation ONLY.
	 */
	cout<<"rotAxis="<<rotAxis<<endl;
	if( rotAxis==0 )  { /* 0 FB_COORD_X */
		RXmat.setRotation(axisX, 1.0*angle/180*MATH_PI);
		AdjustMat.setRotation(axisX, rotX/180*MATH_PI);
		RYmat.setRotation(axisY, rotY/180*MATH_PI);
		RZmat.setRotation(axisZ, rotZ/180*MATH_PI);
		//VRTmat=(RYmat*RZmat)*AdjustMat*RXmat; /* Xadjust and RXmat at last */
		VRTmat=AdjustMat*RXmat*(RYmat*RZmat); /* Xadjust and RXmat at first. --last */
	}
	else if( rotAxis==2 ) { /* 2 FB_COORD_Z */
		RZmat.setRotation(axisZ, 1.0*angle/180*MATH_PI);
		AdjustMat.setRotation(axisZ, rotZ/180*MATH_PI);
		RXmat.setRotation(axisX, rotX/180*MATH_PI);
		RYmat.setRotation(axisY, rotY/180*MATH_PI);
		//VRTmat=(RXmat*RYmat)*AdjustMat*RZmat; /* Zadjust and RZmat at last */
		VRTmat=AdjustMat*RZmat*(RXmat*RYmat); /* Zadjust and RZmat at first */
	}
	else { /* whatever, 1 FB_COORD_Y */
		RYmat.setRotation(axisY, 1.0*angle/180*MATH_PI);
		AdjustMat.setRotation(axisY, rotY/180*MATH_PI);
		RXmat.setRotation(axisX, rotX/180*MATH_PI);
		RZmat.setRotation(axisZ, rotZ/180*MATH_PI);
		//VRTmat=(RXmat*RZmat)*AdjustMat*RYmat; /* Yadjust and RYmat at last */
		VRTmat=AdjustMat*RYmat*(RXmat*RZmat); /* Yadjust and RYmat at first! */
	}

	/* XXX W2.3: Rotate around X to make SCREEN COORD upright! XXX */
	//AdjustMat.setRotation(axisX, (-140.0+rotX)/180*MATH_PI);  /* AntiCloswise. -140 for Screen XYZ flip.  */
	//VRTmat=VRTmat*AdjustMat;

	/* W2.2: Set translation ONLY.  Note: View from -Z ---> +Z */
	VRTmat.setTranslation(offx, offy, offz +dview*2); /* take Focus to obj center=2*dview */

#if 1  /* TEST: -------- Boxman: Head(the last TriGroup) rotation ---- */
	workMesh->objmat = VRTmat;
    	E3D_RTMatrix tm_VRTmat=VRTmat; /* = Identity()*VRTmat OR VRTmat*Identidy() */
    	tm_VRTmat.zeroTranslation();  /* !!!! <---- */
	float rotAng[3]={0.0};

	/* T1. Adjust Head Pitch_Angle */
	if(headAng==0 && (roundTok>=2 && roundTok<=4) ) {
	    /* Boxman's head: triGroupList[5] */
//	    workMesh->triGroupList.back().omat.setRotation(tm_axisX, 1.0*pitchAng/180*MATH_PI); /* axisY --> TriMesh object axisY under Global COORD */
	    rotAng[0]=1.0*pitchAng/180*MATH_PI;
	    E3D_combGlobalIntriRotation("X", rotAng, VRTmat, workMesh->triGroupList[5].omat);

	    /* PitchAng increment */
	    pitchAng +=stepAng;
	    if(pitchAng >45) {
		pitchAng=45; stepAng *=-1;
		roundTok++;
	    }
	    else if(pitchAng<-45) {
		pitchAng=-45; stepAng *=-1;
		roundTok++;
	    }

	    /* Reset roundTok to quick Pitching */
	    //if(roundTok==4 && pitchAng==0)
	    if(roundTok==3 && pitchAng==0)  /* ONLY nod! */
		roundTok=0;
	}

	/* T2. Adjust Head(Sway) heading_Angle + ARM_action + LEG_action */
	else if( pitchAng==0) {

	    /* Limit roundTok within [0 4] */
	    if(roundTok>=4)roundTok=0;

	    /* Boxman's Head action. head: triGroupList[5] */
//	    workMesh->triGroupList.back().omat.setRotation(tm_axisY, 1.0*headAng/180*MATH_PI); /* axisY --> TriMesh object axisY under Global COORD */
	    rotAng[0]=1.0*headAng/180*MATH_PI;  /* Heading/yaw angle */
	    E3D_combGlobalIntriRotation("Y", rotAng, VRTmat, workMesh->triGroupList[5].omat);

	    /* Boxman's Arm action */
	    /* Right_arm: triGroupList[1] */
	    rotAng[0]=(1.0*headAng*65/45)/180*MATH_PI; /* Arm pitch angle (forward/aftward sway) */
	    rotAng[1]=(1.0*headAng*90/45)/180*MATH_PI; /* Arm roll angle */
	    E3D_combGlobalIntriRotation("XY", rotAng, VRTmat, workMesh->triGroupList[1].omat); /* Note 'XY' effect sequence is reversed! */

	    /* Left_arm: triGroupList[2] */
	    rotAng[0]=(-1.0*headAng*65/45)/180*MATH_PI; /* Arm pitch angle (forward/aftward sway) */
	    rotAng[1]=(-1.0*headAng*90/45)/180*MATH_PI; /* Arm roll angle */
	    E3D_combGlobalIntriRotation("XY", rotAng, VRTmat, workMesh->triGroupList[2].omat);

	    /* Boxman's Leg action */
	    /* Right_leg: triGroupList[3], Left_leg: triGroupList[4] */
	    //rotAng[0]=(-1.0*headAng*25/45)/180*MATH_PI; /* MAN */
	    rotAng[0]=(-1.0*headAng*65/45)/180*MATH_PI; /* Dog */
	    E3D_combGlobalIntriRotation("X", rotAng, VRTmat, workMesh->triGroupList[3].omat);
	    rotAng[0]=-rotAng[0];
	    E3D_combGlobalIntriRotation("X", rotAng, VRTmat, workMesh->triGroupList[4].omat);

	    /* BoxDOG' tail: triGroupList[6] */
	    if(workMesh->triGroupList.size()>6) {
	       rotAng[0]=(1.0*headAng*60/45 -60.0)/180*MATH_PI; /* Dog tail */
	       E3D_combGlobalIntriRotation("X", rotAng, VRTmat, workMesh->triGroupList[6].omat);
	    }

	    /* headAng Increment */
	    headAng +=stepAng;

	    if(headAng >45) {
		headAng=45; stepAng *=-1;
		roundTok++;
	    }
	    else if(headAng<-45) {
		headAng=-45; stepAng *=-1;
		roundTok++;
	    }

	}
#endif

	/* W3. Transform workMesh */
	cout << "Transform workMesh...\n";
	//workMesh->transformMesh(RTYmat*RTXmat, ScaleMat); /* Here, scale ==1 */
	workMesh->transformMesh(VRTmat, ScaleMat); /* Here, scale ==1 */

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

#if 1   /* W6. Render the workMesh. */
	fbset_color2(&gv_fb_dev, faceColor);

        /* OR  updateAllTriNormals() here */

        cout << "Render mesh ... angle="<< angle <<endl;
        workMesh->renderMesh(&gv_fb_dev, projMatrix);
#endif

	/* W7. Render as wire frames, on current FB image (before FB is cleared). */
	if(wireframe_on && workMesh->shadeType!=E3D_TEXTURE_MAPPING) {
		fbset_color2(&gv_fb_dev, wireColor); //WEGI_COLOR_DARKGRAY);
		cout << "Draw meshwire ...\n";
		workMesh->drawMeshWire(&gv_fb_dev, projMatrix);
	}

	/* W8. Draw the AABB, on current FB image. */
	if( AABB_on ) {
	        cout << "Draw AABB ...\n";
		fbset_color2(&gv_fb_dev, aabbColor);
		/* Use meshModel to draw AABB! */
		//meshModel.drawAABB(&gv_fb_dev, RTYmat*RTXmat, projMatrix);
		meshModel.drawAABB(&gv_fb_dev, VRTmat, projMatrix);
	}

	/* W9. Draw the Coordinate Navigating_Sphere/Frame (coordNavSphere/coordNavFrame) */
	if(coordNavigate_on) {
	   E3D_draw_coordNavSphere(&gv_fb_dev, 0.4*workMesh->AABBdSize(), VRTmat, projMatrix);
	   E3D_draw_coordNavFrame(&gv_fb_dev, 0.5*workMesh->AABBdSize(), VRTmat, projMatrix);
	}

#if 0 /* XXX not correct!  TEST: ---------------- boxman: HEAD omat  coordFrame */
	E3D_draw_coordNavSphere(&gv_fb_dev, 0.2*workMesh->AABBdSize(), workMesh->triGroupList.back().omat, projMatrix);
#endif

	/* W9A. Draw grids */
	if( showGrid_on ) {
		E3D_RTMatrix gmat;			/* Combined RTMatrix */
		int dSize=(int)workMesh->AABBdSize();
		fbset_color2(&gv_fb_dev, gridColor);
		gmat.identity();
		gmat.setRotation(axisX, 90.0/180*MATH_PI); /* As original XZ plane grid of the mesh object. */
	        E3D_draw_grid(&gv_fb_dev, 1.5*dSize, 1.5*dSize, dSize/10, gmat*VRTmat, projMatrix); /* fbdev sx,sy, us, VRTmat, projMatrix */
	}

	/* W10. Write note & statistics */
	gv_fb_dev.zbuff_on=false;

	/* W10.1 Navigating sphere icon */
	E3D_draw_coordNavIcon2D(&gv_fb_dev, 30, VRTmat, xres-35, 35);

	/* W10.2 Statistics and info. */
	if(showInfo_on) {
	   sprintf(strtmp,"Vertex: %d\nTriangle: %d\n%s\ndvv=%d%%",
			vertexCount, triangleCount,
			projMatrix.type?(UFT8_PCHAR)"Perspective":(UFT8_PCHAR)"Isometric",
			(int)roundf(dvv*100.0f/dview) );
			//dvv*100.0f/dview); /* !!! (int) or fail to d% */
           FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
                                        16, 16,                           /* fw,fh */
                                        (UFT8_PCHAR)strtmp, 		  /* pstr */
                                        300, 5, 4,                        /* pixpl, lines, fgap */
                                        5, 4,                             /* x0,y0, */
                                        fontColor, -1, 200,        	  /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /* int *cnt, int *lnleft, int* penx, int* peny */
	}

	sprintf(strtmp, "RotAngle: %.1f", angle); /* Rotation angle */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
                                        16, 16,                           /* fw,fh */
                                        (UFT8_PCHAR)strtmp, 		  /* pstr */
                                        300, 1, 0,                        /* pixpl, lines, fgap */
                                        xres-122, yres-42,                /* x0,y0, */
                                        fontColor2, -1, 255,        	  /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /* int *cnt, int *lnleft, int* penx, int* peny */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                        20, 20,                         /* fw,fh */
                                        (UFT8_PCHAR)"E3D MESH", 	/* pstr */
                                        300, 1, 0,                      /* pixpl, lines, fgap */
                                        xres-125, yres-25,              /* x0,y0, */
                                        fontColor2, -1, 255,            /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );       /* int *cnt, int *lnleft, int* penx, int* peny */

        #if 0  /* Display shade type and faceColor */
	/* Shade type and face color */
	switch(workMesh->shadeType) {
		case E3D_FLAT_SHADING:
			strShadeType="Flat shading"; break;
		case E3D_GOURAUD_SHADING:
			strShadeType="Gouraud shading"; break;
		case E3D_WIRE_FRAMING:
			strShadeType="Wire framing"; break;
		case E3D_TEXTURE_MAPPING:
			strShadeType="Texture mapping"; break;
		default:
			strShadeType=NULL; break;
	}
	sprintf( strtmp,"0x%06X Y%d\n%s",
		 COLOR_16TO24BITS(faceColor), egi_color_getY(faceColor),
		 //workMesh->shadeType==E3D_GOURAUD_SHADING?(UFT8_PCHAR)"Gouraud shading":(UFT8_PCHAR)"Flat shading"
		 strShadeType
		);

       	FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
                                       16, 16,                     	/* fw,fh */
                                       (UFT8_PCHAR)strtmp, 		/* pstr */
                                       300, 2, 4,                      /* pixpl, lines, fgap */
                                       5, yres-45,              	/* x0,y0, */
                                       fontColor, -1, 255,             /* fontcolor, transcolor,opaque */
                                       NULL, NULL, NULL, NULL );       /* int *cnt, int *lnleft, int* penx, int* peny */
	#endif
	gv_fb_dev.zbuff_on=true;

	/* W11.  BLANK */

	/* W12. Render to FBDEV */
	fb_render(&gv_fb_dev);
	usleep(50000);
	if(psec) tm_delayms(psec); //usleep(psec*1000);

	/* W13. Update angle */
	angle += (5.0+da); // *MATH_PI/180;

/* ---TEST Record Screen: Save screen as a frame of a motion file. ------------------------- */
	if(saveMotion_on) {
		if( egi_imgmotion_saveFrame(fmotion, fbimg)!=0 ) {
			exit(-1);
		}
	}

	/* To keep precision: Suppose it starts with angle==0!  */
	if( (int)angle >=360 || (int)angle <= -360) {
		if((int)angle>=360) {
			angle -=360;
//			exit(0);     /* <------------- TEST */
		}
		if((int)angle<-360) angle +=360;

		/* Random face color */
		faceColor=egi_color_random(color_all);
		if(egi_color_getY(faceColor) < 150 )
		faceColor=egi_colorLuma_adjust(faceColor, 150-egi_color_getY(faceColor));

//		projMatrix.type=!projMatrix.type; /* Switch projection type */

#if 1		/* Switch shadeType: Loop FLAT --> WIRE --> TEXTURE  */
		if(workMesh->shadeType==E3D_FLAT_SHADING) {
//			workMesh->shadeType=E3D_GOURAUD_SHADING;
			workMesh->shadeType=E3D_TEXTURE_MAPPING;
		}
		else if(workMesh->shadeType==E3D_GOURAUD_SHADING) {
			//workMesh->shadeType=E3D_TEXTURE_MAPPING;
//			workMesh->shadeType=E3D_WIRE_FRAMING;
#if 0 /* TEST: ---------- */
			if( saveMotion_on )
				exit(0);
#endif
		}
		else if(workMesh->shadeType==E3D_WIRE_FRAMING) {
			if(workMesh->textureImg || workMesh->mtlList.size()>0 )
				workMesh->shadeType=E3D_TEXTURE_MAPPING;
			else
				workMesh->shadeType=E3D_FLAT_SHADING;
		}
		else if(workMesh->shadeType==E3D_TEXTURE_MAPPING) {
			workMesh->shadeType=E3D_FLAT_SHADING;
/* TEST: ---------- */
			if( saveMotion_on )
				exit(0);
		}
#endif
	}

	/* W14. Update dvv: Distance from the Focus to the Screen/ViewPlane */
	dvv += dstep;
	if( dvv > 1.9*dview || dvv < 0.1*dview )
		dstep=-dstep;

   }

	return 0;
}
