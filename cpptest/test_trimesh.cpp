/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test E3D TriMesh
3D model reference: www.cgmodel.com

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

cube.obj -c -s 1.75 -X 160 -P -T cubetexture3.png -a 5 -D
teapot.obj -c -s 1.5 -X 195 -r		!!! --- Gouraud test --- !!!
teapot.obj -r -s 3.25 -X -160 -a 25 -P  // Lighting Computation Demo
jaguar.obj -c -s 5 -X -170 -T texture_jaguar.png -P -a 10
head_man04.obj -c -s 18 -X 180 -P -y 30       !!! --- Gouraud test --- !!!
dog.obj -c -s 400 -X -175 -T texdog.png -a 5 -P -b
mingren.obj -c -s 4 -A 0 -Y 90 -X 90 -x -15 -T texmingren.png -a 10 -f 0.25
boat.obj -c -s 100 -X -P -y -50
ding.obj -c -s 300 -A 2 -X 110 -P -T texding.png
ding.obj -c -s 250 -A 2 -X 120 -P -T texding.png -a 5 -t -p 2000
juese.obj -c -s 4 -A 1 -Z -90 -x 30 -B -T texjuese.png -a 25
killer-whale.obj -c -s 1.75 -z -400  -A 2 -X 105 -P
earth.obj -c -s 9 -D -T TexEarth.jpg -X 130 -A 2 -a 10
head_man05.obj -c -s 60 -X 160 -x 50 -a 10
rabbit.obj -c -s 100 -X 90 -A 2 -y 20 -a 10 -T TexRabbit.jpg
crownfish.obj -c -s 20 -X 90 -A 2 -P -a 10 -T crownfish.jpg
covid.obj -c -X 190 -a 10 -T covid.png
3Dhead.obj -c -s 120 -X 180 -T 3DheadColor.jpg
3Dhead.obj -c -s 150 -X 180 -Y -45 -a -5 -T 3DheadColor.jpg -M /mmc/texxx.motion  !!!--- Text_Texutre and vlight---!!!
3Dhead.obj -c -s 220 -X 180 -a -3 -P -T 3DheadColor.jpg -M /mmc/3dhead2.mot
angel.obj -c -s 200 -X 180 -z -250 -P
rock.obj -c -s 15 -X 60 -A 2 -a -2.5 -P -M /mmc/rock.motion
temple.obj -c -s 0.5 -z -200 -A 2 -y -100 -Z 180 -X -80 -a 25 -P -T temple.png
monk.obj -c -s 200 -y 120 -X 185 -r
monk.obj -c -s 150 -Z -90 -X 10 -x -20 -r
arm_hand.obj -c -s 125 -y 120 -X 90 -A 2 -a -2.5 -M /mmc/arm_hand.motion
hen.obj -c  -s 0.26 -y -20 -A 2 -X 90 -T texhen.png -P -a -2.5
mario.obj -c -s 400 -z -200 -Z 90 -A 0 -P -a 10

//PCB.obj -c -s 150 -y -150 -X -80 -T PCB.jpg -P  !!! XXX --- Show flaws as one pixZ value for all Tri pixels.  --- !!!
PCB.obj -c -s 150 -y -150 -X -120 -a -3 -T PCB.jpg -P -M /mmc/pcb3.mot
PCB.obj -c -s 100 -X -120 -a 10 -T PCB.jpg -f 0.5 -t  ISO

venus.obj -c -s 0.3 -Z 90 -A 0 -x 200 -a -2.5 -t
gsit.obj -c -s 2.25 -Z 90 -X 20 -Y 150 -a -5 !!! --- Test Sub_object group --- !!!
	 -c -s 2.25 -x -5 -Z 90 -X 0 -Y 145 -a 40
	-c -s 2.25 -x -5 -Z 90 -X 0 -Y 145 -a 10 -M ...
leaf.obj -c -s 900 -z -300 -X 210 -a -2.5 -P
ctrees.obj -c -s 0.04 -X 185 -a 10 -P
	-c -s 0.04 -y -30 -X 190 -a 10 -P   --- Motion ---
	-c -s 0.035 -y -30 -z -300 -X 190 -P -M /mmc/ctree2.motoin
	-c -s 0.03 -y -30 -Z 90 -X -10 -P   --- Portrait ---

deer.obj -s 1 -y -100 -z 3500 -c -b -X -20
sportsmen.obj -c -s 2.0 -X -170 -y 0 -P -t -a 25
myPack.obj -s 1.5 -c -z 750 -X -30 -b -y -25  	!!! --- Check frustum clipping --- !!!
bird.obj -s 80 -z 800 -b -w -X -20    		!!! --- Check frustum clipping --- !!!
bird.obj -c -s 80 -b -X -25  Auto_z
sail.obj -z 40 -c -X -60 -R -b -y 2   !!! --- Mesh between Foucs and View_plane --- !!!
sail.obj -c -s 25 -X 165 -z -250 -a 25 -P -D
lion.obj -s 0.025 -c -X -30
lion.obj -c -s 0.06 -P -X -170 -t -a -3
jet.obj -c -s 1.5 -X 160
plane.obj -c -s .3 -X 200
plane.obj -c -s 1 -Y 90 -X 185 -P
plane.obj -c -s 1 -Y 90 -X 185 -a -3 -G -P -t -M /mmc/plane.mot
plane.obj -c -s 0.8 -X 190 -Y 200 -a -5  -w -B -t   // See fuselage(main body) inside/backface
plane.obj -c -s 1.0 -z -600 -X 190 -Y 197 -a 175 -P -w -B -t  // ON Head and tail, -z -800 to see inside
plane.obj -c -s 1.0 -z -600 -A 2 -X 200 -a 175 -P -B -t  // See up/down sides of Wings and Hstabilizer
plane.obj -c -s .8 -A 2 -X 90 -a 175 -P -t //To and bottom view

audi.obj -c -s 15 -a 10 -X 175 -P
//TEST: audi.obj -l -c -s 16  -X 190 -P -t
dwarf.obj -c -s 100 -Z 90
wm.obj  -c -s 800 -X 200 -t -P
F458.obj -c -s 100 -t -X 90 -A 2 -a 10
fish.obj -c -s 300 -X 170 -P -t
eye.obj -c -s 100 -X -180 a 5 -t
strong.obj -c -s 18 -X 180 -y 40
./test_trimesh 3Dhead.obj -c -s 80 -X -160 -a 10 -G -t  // Shadow TEST
oldship2.obj -c -s 200000 -X 200 -y -150 -P -t -M oldship.mot
statue.obj -c -s 10 -P -X 90 -t -A 2


	----- Shadow TEST -----
teapot.obj -c -r -s 2.5 -X 200 -t -W -P -M teapotShadow.mot
3Dhead.obj -c -s 90 -X -160 -G -M HeadShadow.mot
fish.obj -c -s 220 -x 30 -X -150 -W -P -a -2.5 -t -M fishShadow.mot
thinker.obj -c -s 2 -P -W -x 100 -X -160 -t
tower.obj -c -s 3 -W -X -120 -x 200 -P -G -t
boxman.obj -c -s 10 -W -X -150 -x 100 -P -G -t -a 10
thinker.obj -c -s 2 -P -W -X -165 -z 200 -t -a -5 -G -M thinkerShadow.mot  // Rotate lighting
widora.obj -c -s 7 -A 2 -X -70 -P -W -t -M widora.mot
widoraX.obj -c -s 8 -X -160 -P -W -G -M widoraX.mot

	---- Signle Ray Trace Demo ----
rta20.obj -c -s 2 -X 25 -P -a 10
thinker.obj -c -s 2 -P -X -160 -t -a -4 -M thinkerRay.mot

	---- Ray backtracing shadow ----
thinker.obj -c -s 2 -P -X -160 -t -M  rayshadow.mot

volumes.obj -c -s 30 -A 2 -X 110 -y -30 -P -a 5 -C -t   ( Perspective view )
volumes.obj -c -s 15 -A 2 -X 110 -C -t     ( ISO view)
volumes.obj -c -s 35 -A 2 -X 110 -y -30 -P -a -3 -t -C -M building.mot

volumes2.obj -c -s 15 -A 2 -X 120 -y -30 -t -M volumes2.mot ( ISO view)
volumes3.obj -c -s 20 -P -A 2 -Z 0 -X 120 -y -50 -a 30 -t  (Perspective )
volumes3.obj -c -s 10 -A 2 -Z 0 -X 110 -y -20 -a 30 -t  (ISO View)

	---- Focal length stepping/varing OR zooming ----
volumes.obj -c -s 60 -A 2 -X 95 -Z -30 -y -200 -m -P -F (-C) -t -a -3 -l -M zoomSD.mot
volumes.obj -c -s 60 -A 2 -X 95 -Z -90 -y -250 -m -P -F (-C) -t -a -3 -l -M zoomSD.mot (with boxman)

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
6. Navigating sphere icon:
   For OPTION_1, to indicate workMesh local coord relative to global coord.
   For OPTION_2, to indicate global coord relative to camera/viewport coord.

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
   6.7 Here we transform the workMesh instead of the camera, so vLight SHOULD align with the camera COORD,
       or shadowPlane will backface to vLights.

7.   			--- OPTION_2: ViewPoit_Movement ---

   7.1 Keep workMesh static, move ViewCoord(Camera), View direction as View_COORD_Z (-z-->+z).
       The Foucus is behind Viewplane(View_COORD_Z==0).
8. Try to show the mesh at first using a small scale factor, which will greatly
   improve rending speed.
9. For GOURAUD_SHADING, each vertex shall have its own normal value, otherwise it
   turns out to be FLAT_SHADING actually.


TODO:
1. NOW it uses integer type zbuff[]. ---> Should be float/double type.
2. XXX NOW it uses triangle center Z value for zbuffering all pixels on the triangle.
       ---> Z value should be computed for each pixel!
3. Above 1+2 will cause surface hiding failure sometimes.
   Perspective view of a cube.obj at angle=95 to demo such fault.
4. Each edge of a triangle is drawn twice, as it belongs to TWO trianlges.
   ( Edge data should be stored and drawn independtly )
5. Fixed point calculation in some draw_functions makes triangle wire lines miss_align
   with filled_triangle edge.
6. E3D_FLAT_SHADING --- Wall/Plane interference OK
   E3D_TEXTURE_MAPPING --- Wall/Plane interference WRONG!
7. CHECK: transformVertices() and triGroupList[i].omat.pmat[9-11] shall be updated BOTH?!

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
2021-08-23:
	Test texture mapping.
2021-08-24:
	OPTION_1: Rotating_axis selection.
2021-08-30:
	Test gouraud shading.
2021-09-09:
	Same FB data as subimges of simg, and then for serial playing.
2021-09-10:
	Save screen as a frame of a motion file.
2021-09-16/17:
	Test FTsymbol texturing.
	Test vLighting
2021-09-27:
	Test E3D_Materil, E3D_TriMesh::TriGroup, defMaterial, mtlList[], triGroupList[]
2021-10-9:
	Test BOXMAN Head/Arm action.
2022-01-07:
	Add input option loopRender_on.
2022-08-08:
	Option for mesh shadow displaying.
2022-08-10:
	gv_vLight rotating.
2022-08-25/26:
	1. Test rayHitFace().
2022-09-14:
	1. TEST: Add boxman into volumes.obj
2022-09-26:
	1. TEST: renderInstance()

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
#include "e3d_volumes.h"
using namespace std;


#define MOTION_FILE  "/mmc/mesh.motion"

//Option '-i': Test view_frustum near plane clipping
#define TEST_SCENE	   0  /* TEST E3D_Scene rendering, ONLY when TEST_MESHINSTANCE==0 and TEST_MULTIOBJ==1 */
#define TEST_MESHINSTANCE  0

#define TEST_MULTIOBJ	   0  /* ONLY if !TEST_SCENE. Load/render boxman and sportsman  */
#define TEST_MTXT_TEXTURE  0
#define TEST_VLIGHTING	   0

const UFT8_PCHAR ustr=(UFT8_PCHAR)"Hello_World! 世界你好!\n12345 子丑寅卯辰\nABCEDEFG 甲乙丙丁戊\nE3D MESH E3D MESH E3D MESH";
//const UFT8_PCHAR ustr=(UFT8_PCHAR)"'技术需要沉淀，\n成长需要痛苦.\n成功需要坚持，\n敬仰需要奉献！'";

void print_help(const char *name)
{
	printf("Usage: %s obj_file [-hirbBDGmNRSPVLWCwbtclFf:s:x:y:z:A:M:X:Y:Z:T:a:p:]\n", name);
	printf("-h     Help\n");
	printf("-i     Test view_frustum near plane clipping\n");
	printf("-r     Reverse trianlge normals\n");
	printf("-B     Display back faces with default color, OR texture for back faces also.\n");
	printf("-D     Show mesh detail statistics and other info.\n");
	printf("-F     To adjust focal length step by step.\n");
	printf("-G     Show grid.\n");
	printf("-m     Force texture mapping.\n");
	printf("-N     Show coordinate navigating sphere/frame.\n");
	printf("-R     Reverse vertex Z direction.\n");
	printf("-S     Save serial FB images and loop playing.\n"); /* OBSOLETE */
	printf("-P     Perspective ON\n");
	printf("-V     Auto compute all vtxNormals if no provided in data, for gouraud shading.\n");
	printf("-L     Adjust lighting direction.\n");
	printf("-W     Shadow on grid plane.\n");

	printf("-C     Test backward ray tracing to create self_shadowing.\n");
	printf("-w     Wireframe ON\n");
	printf("-b     AABB ON\n");
	printf("-c    Move local COORD origin to center of mesh, 0-VtxCenter 1-AABB Center.\n");
	printf("-t    Show rendering process.\n");
	printf("-l    Loop rendering.\n");
	printf("-s:    Scale the mesh\n");
	printf("-x:    X_offset for display, relative to ScreenCoord.\n");
	printf("-y:    Y_offset for display\n");
	printf("-z:    Z_offset for display\n");
	printf("-A:    0,1,2 Rotation axis, default FB_COORD_Y, 0_X, 1_Y, 2_Z \n");
	printf("-M:    Save frames to a motion file.\n");
	printf("-X:    X delta angle for display\n");
	printf("-Y:    Y delta angle for display\n");
	printf("-Z:    Z delta angle for display\n");
	printf("-T:    Texture image file(jpg,png).\n");
	printf("-f:    Texture resize factor.\n");
	printf("-a:    delt angle for each move/rotation. angle += (5.0+da) \n If with '-i', then it is clipping step with percentage of dobj");
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
	int		RenderTriCnt;	 /* Actual triangles rendered */

	E3D_TriMesh *TriMeshList[2]={NULL,NULL};

	/* 24bit color ref
	 0xD0BC4B--Golden
	 0xB8DCF0--Milk white
	 0xF0CCA8--Bronze
	*/

	EGI_16BIT_COLOR	  bkgScreenColor=WEGI_COLOR_GRAYB;//GRAY3;//GREEN; //GRAY5; // DARKPURPLE
	EGI_16BIT_COLOR	  faceColor=COLOR_24TO16BITS(0xF0CCA8);//WEGI_COLOR_PINK;
	EGI_16BIT_COLOR	  wireColor=WEGI_COLOR_BLACK; //DARKGRAY;
	EGI_16BIT_COLOR	  aabbColor=WEGI_COLOR_LTBLUE;
	EGI_16BIT_COLOR	  fontColor=WEGI_COLOR_GREEN;
	EGI_16BIT_COLOR	  fontColor2=WEGI_COLOR_PINK;//GREEN;
	EGI_16BIT_COLOR	  gridColor=WEGI_COLOR_PURPLE;
	EGI_16BIT_COLOR	  backFaceColor=WEGI_COLOR_RED;

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
	unsigned int	frameCount=0;

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
	bool		loopRender_on=false;    /* loop rendering */
	bool		force_textureMap=false; /* Force texturemaping, if ONLY one(or more) face group has texture file. */
	bool		shadow_on=false;	/* Shadown on grid plane */
	bool            rayTrace_on=false;  	/* Test backward ray tracing to vLight, ONLY to create self_shadowing. */
	bool		focalenVarying_on=false;  /* To adjust focal length projMatrix.dv step by step, and demo the effect. */
	bool		test_clipping=false;	/* Test clipping with near plane of the view frustum */

	/* Projectionn matrix */
	float		dobj;   /* Object MAX size */
	float		dview;	/* OBSOLETE, to use dobj instead  XXX Distance from the Focus(usually originZ) to the Screen/ViewPlane */
	float		dvv;	/* varying of projMatrix.dv */
	float		dstep;
	/* Note: ALWAYS take dnear==dv, and they are to be adjusted later. see 6. */
	E3DS_ProjMatrix projMatrix; //{ .type=E3D_ISOMETRIC_VIEW, .dv=500, .dnear=500, .dfar=10000000, .winW=320, .winH=240};
	E3D_InitProjMatrix(projMatrix, E3D_ISOMETRIC_VIEW, 320, 240, 500, 10000000, 500); /* matrix, type, winW,winH,dnear,dfar,dv */

	/* Plane for shadow projection. */
	E3D_Plane shadowPlane(0.0,1.0,0.0, 0.0); /* On y=0.0 plane */

#if 0
	E3D_Vector shadowPts[3];
	shadowPts[0].x=0.0; shadowPts[0].y=0.0; shadowPts[0].z=0.0;
	shadowPts[1].x=1.0; shadowPts[1].y=0.0; shadowPts[1].z=0.0;
	shadowPts[2].x=0.0; shadowPts[2].y=0.0; shadowPts[2].z=1.0;
#endif

	/* Ray for test */
	E3D_Ray Ray;

	/* For plane AABB Tx/Ty/Tz */
	float Tx=0.0, Ty=0.0, Tz=0.0;

#if TEST_MESHINSTANCE
	E3D_MeshInstance *meshInstances[64*64];
#endif

#if TEST_SCENE
	E3D_Scene myScene;
#endif

        /* Parse input option */
	int opt;
        while( (opt=getopt(argc,argv,"hirbBDGmNRSPVLWCwbtclFf:s:x:y:z:A:M:X:Y:Z:T:a:p:"))!=-1 ) {
                switch(opt) {
                        case 'h':
				print_help(argv[0]);
				break;
			case 'i':
				test_clipping=true;
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
			case 'F':
				focalenVarying_on=true;
				break;
			case 'G':
				showGrid_on=true;
				break;
			case 'm':
				force_textureMap=true;
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
			case 'W':
				shadow_on=true;
				break;
			case 'C':
				rayTrace_on=true;
				break;
			case 'b':
				AABB_on=true;
				break;
			case 'c':
				autocenter_on=true;
				break;
			case 'l':
				loopRender_on=true;
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

	/* P1. Initilize sys FBDEV */
        printf("init_fbdev()...\n");
        if(init_fbdev(&gv_fb_dev))
                return -1;
	xres=gv_fb_dev.vinfo.xres;
	yres=gv_fb_dev.vinfo.yres;

        /* P2. Set sys FB mode */
        fb_set_directFB(&gv_fb_dev,false);
        fb_position_rotate(&gv_fb_dev,0);
	gv_fb_dev.pixcolor_on=true;		/* Pixcolor ON */
	gv_fb_dev.zbuff_on = true;		/* Zbuff ON */
	fb_init_zbuff(&gv_fb_dev, INT_MIN);

	/* P3. Prepare fbimg, and relates to FB */
	fbimg=egi_imgbuf_alloc();
	fbimg->width=xres;
	fbimg->height=yres;
	fbimg->imgbuf=(EGI_16BIT_COLOR*)gv_fb_dev.map_fb;

#if 0	/* XXX Prepare for serial images playing XXX */
	if( serialPlay_on ) {

		/* Prepare simg or serial image. Max 72 subimg  */
		simg = egi_imgbuf_createWithoutAlpha(yres, xres*72, 0);
		if(simg==NULL) {
			printf("Fail to create simg!\n");
			exit(EXIT_FAILURE);
		}
		simg->submax=72-1; /* Set submax */

		/* Allocate subimg boxes */
		simg->subimgs=egi_imgboxes_alloc(72);
		if(simg->subimgs==NULL) {
			printf("Fail to create simg->subimgs!\n");
			exit(EXIT_FAILURE);
		}
		for(i=0; i<72; i++) {
			simg->subimgs[i].x0=xres*i;
			simg->subimgs[i].y0=0;
			simg->subimgs[i].w=xres;
			simg->subimgs[i].h=yres;
		}
		/* Reset imgcnt */
		imgcnt=0;
	}
#endif

        /* P4. Load freetype appfonts */
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }


	/* 0. Set global light vector */
	float vang=45.0; /*  R=1.414, angle for vector(x,y) of vLight  */
	float dvang=5; //2.5;  /* Delta angle for vangle */
if( adjustLight_on ) {  /* For Portrait.. */
	gv_vLight.assign(1, -1, 1); //2); //4);
	gv_vLight.normalize();
        gv_auxLight.assign(0,0,0.5);
	//gv_auxLight.normalize();
}
else {	/* For Landscape */
	if(shadow_on) {
	/* NOPE! will be reassigned, see W3a. ..... */
	   gv_vLight.assign(-1, 0.5, 1);
	   //gv_vLight.assign(-1, 1, 0);
	   gv_vLight.normalize();
	   gv_auxLight.assign(0,0,1); //(1,1,1);
	   //gv_auxLight.normalize();
	}
	else {
	   // gv_vLight.assign(-1, 1.5, 0); // (1, 1.5, 0);
	   gv_vLight.assign(1,1,1);
	   gv_vLight.normalize();
	   //gv_auxLight.assign(-1, -1, 1);
	   //gv_auxLight.assign(-1,-1,0);
	   gv_auxLight.assign(0,0,0.25); //(-1,0,0.5); //(0,0,1);
	   //gv_auxLight.normalize();
	}

	//E3D_Vector vLight(-0.5, 1, 1);
	//E3D_Vector vLight(1, 1, 1);
	//vLight.normalize();
	//gv_vLight=vLight;
}


	/////////////////   Read/Create meshModel   ////////////////
#if 1 /* 1. Read obj file to meshModel */
	cout<< "Read obj file '"<< fobj <<"' into E3D_TriMesh... \n";
	E3D_TriMesh	meshModel(fobj);

#else /* TEST: Primitive volumes */
	cout<<"Create primitive volumes...\n";
	E3D_RtaSphere   meshModel(2, 100); /* xxx -c -s 2.0 -X 30 -P */
//	E3D_Cuboid   meshModel(100,200,300); /* xxx -c -s 1.25 -X 30 -P -a 10 */

#endif

#if TEST_MULTIOBJ
	E3D_TriMesh	boxman("/tmp/boxman.obj");
	E3D_TriMesh	sportsmen("/tmp/sportsmen.obj");
#endif

	if(meshModel.vtxCount()==0)
		exit(-1);
  	/* Read textureFile into meshModel. */
	if( textureFile ) {
		meshModel.readTextureImage(textureFile, -1); //xres);
		if(meshModel.textureImg) {
			if( texScale>0.0 ) {
			    if(egi_imgbuf_scale_update(&meshModel.textureImg,
					   texScale*meshModel.textureImg->width, texScale*meshModel.textureImg->height)==0) {
			        egi_dpstd("Scale texture image size to %dx%d\n",meshModel.textureImg->width, meshModel.textureImg->height);
				meshModel.defMaterial.img_kd=meshModel.textureImg; /* !!!!  */
			    }
			    else
			        egi_dpstd("Fail to scale texture image!\n");
			}
		}
	}

#if 1  /* TEST: -------- Print out all materials ---- */
	for(unsigned int k=0; k<meshModel.mtlList.size(); k++)
			meshModel.mtlList[k].print();
#endif

#if 0  /* TEST: -------- */
	meshModel.printAllVertices("meshModel");
	meshModel.printAllTriangles("meshModel");
	meshModel.printAllTextureVtx("meshModel");

	exit(0);
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
	float vltangle=0.0; 			/* gv_vLight adjusting angle */

	/* ------ 8.TEST: Save FB images to a motion file.  ------------------------- */
	if(saveMotion_on) {
		if(fmotion==NULL)
			fmotion=MOTION_FILE;
		if( egi_imgmotion_saveHeader(fmotion, xres, yres, 50, 1) !=0 )  /* (fpath, width, height, delayms, compress) */
			exit(EXIT_FAILURE);
	}


	/* ------ 9.TEST: Imgbuf texture with FTsymbols, for 3Dhead specially. ---------------- */
#if TEST_MTXT_TEXTURE
	EGI_16BIT_COLOR texcolor=COLOR_24TO16BITS(0x908CA8);
	EGI_16BIT_COLOR txtcolor=WEGI_COLOR_RED;//PURPLE;
	EGI_IMGBUF *teximg=NULL;

	int fw=100, fh=100;
//	int sx=2048-200, sy=2048-200;
	int sx=2048-350, sy=2048-200 -300;
	int txtangle=-75; //45;
#endif


#if 1 //////////////////////  OPTION_1. Move/Scale workMesh and Keep View Direction as Global_COORD_Z (-z-->+z)  /////////////////////////
			   /*  Loop Rendering: renderMesh(fbdev)  */

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
        fb_clear_workBuff(&gv_fb_dev, bkgScreenColor); //DARKPURPLE);
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
	dobj=meshModel.AABBdSize();
	dview=dobj;
	if(focalenVarying_on) {
		dvv=2.75*dobj;       /* To change dvv by dstep later, and assign to projMatrix.dv */
		dstep=-0.05; //Percentage //dview/40;
	}
	else {
	    if(test_clipping)
	        offz -= 0.5*dobj; /* Start clipping at very near position. */
	    dvv=1.0*dobj;
	    dstep=0.0f;
	}
	projMatrix.dv=dvv; projMatrix.dnear=dvv;  /* ALWAYS takes dv==dnear */
	cout << "dobj=" <<dobj<<endl;
	sleep(1);


/* TEST: -------- Get Ty for grid plane translation */
	Tx=-meshModel.aabbox.vmax.x -5;
	Ty=-meshModel.aabbox.vmax.y -5;
	Tz=-meshModel.aabbox.vmax.z -5;

	/* 7. Init. E3D Vector and Matrixes for meshModel position. */
	RXmat.identity(); 	/* !!! setTranslation(0,0,0); */
	RYmat.identity();
	RZmat.identity();
	VRTmat.identity();	/* Combined transform matrix */
	ScaleMat.identity();
	AdjustMat.identity();

	/* 8. Prepare WorkMesh (Under global Coord) */
	E3D_TriMesh *workMesh=new E3D_TriMesh(meshModel.vtxCount(), meshModel.triCount());

	/* 9. Check if it has input texture with option -T path. (NOT as defined in .mtl file.) */
	if( force_textureMap || (textureFile && meshModel.textureImg) || (meshModel.mtlList.size()>0 && meshModel.mtlList[0].img_kd) ) {
		refimg=meshModel.textureImg; /* Just ref, to get imgbuf size later. */
		workMesh->shadeType=E3D_TEXTURE_MAPPING;
	}
	else {
		/* Note: if the mesh has NO nv data(all 0), then result of GOURAUD_SHADING will be BLACK */
		workMesh->shadeType=E3D_GOURAUD_SHADING;
//		workMesh->shadeType=E3D_FLAT_SHADING;
	}


	/* 10. Test multiline text texture */
#if  TEST_MTXT_TEXTURE
	if( refimg ) {
		teximg=egi_imgbuf_blockCopy(refimg, 0,0, refimg->height, refimg->width);
		if(teximg==NULL) exit(-1);
	}
	/* Reset color for teximg */
	else {
		teximg=egi_imgbuf_createWithoutAlpha(2048, 2048, texcolor);
		if( teximg==NULL ) exit(-1);
	}

	/* 11. Re_assign for workMesh */
	workMesh->shadeType=E3D_TEXTURE_MAPPING;
	workMesh->textureImg=teximg;
	workMesh->defMaterial.img_kd=teximg;

	/* also for meshModel */
	meshModel->textureImg=teximg;
	meshModel->defMaterail.img_kd=teximg;

	/* 12. TEST: to teximg display on screen */
	FTsymbol_uft8strings_writeIMG2( teximg, egi_appfonts.bold, fw, fh, ustr, /* imgbuf, face, fw, fh, pstr */
                                        2000, 5, 20,             /* pixpl, lines, gap */
                                        sx, sy, txtangle, txtcolor, /* x0, y0, angle, fontcolor */
                                        NULL, NULL, NULL,NULL); /* *cnt, *lnleft, *penx, *peny */

	egi_subimg_writeFB(teximg, &gv_fb_dev, 0, -1, 0, 0); /* imgbuf, fbdev, subnum subcolor, x0,y0 */
	fb_render(&gv_fb_dev);
	sleep(3);
#endif

			/* (((-----  Loop move and render workMesh  -----))) */
  while(1) {
	/* W0. Update Projectoin Matrix */

	projMatrix.dv = dvv; /* Update/Adjust dvv at W14. if(focalenVarying_on) */
	projMatrix.dnear = dvv; /* ALWAYS taken dv==dnear */
	printf(DBG_BLUE"[[[  dvv=%f, dv=%d, dnear=%d, dobj=%f  ]]]\n"DBG_RESET, dvv, projMatrix.dv, projMatrix.dnear, dobj);

	/* W1. Clone meshModel to workMesh, with: vertices/triangles/normals/vtxcenter/AABB/textureImg/
	 *     /defMateril/mtlList[]/triGroupList[].....
	 *   Note:
	 *	1. EGI_IMGBUF pointers of textureImg and map_kd in materials NEED NOT to free/release before cloning,
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

        /* Turn backFace on */
	if(backFace_on) {
 		workMesh->backFaceOn=true;
		workMesh->bkFaceColor=backFaceColor;
	}

	/* Turn on testRayTracing */
	if(rayTrace_on) {
		workMesh->testRayTracing=true;
	}


//	workMesh->shadeType=E3D_GOURAUD_SHADING;

	/* W2. Prepare transform matrix */
	/* W2.1 Rotation Matrix combining computation
	 *			---- PRACTICE!! ---
	 * Note:
	 * 1. For tranforming objects, sequence of Matrix multiplication matters!!
	 * 2. If rotX/Y/Z is the rotating axis, then it will added as AdjustMat.
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
	VRTmat.setTranslation(offx, offy, offz +(dvv+dobj)); /* take Focus to obj center */

#if TEST_MESHINSTANCE  /////////////  Mesh Instances (before transform_workMesh and omat_adjust)  /////////////
        //////// Terracotta warrior /////////
	E3D_RTMatrix rtmat;
	int instColumns=5;
	int instRows=5;
	int instTotal=instColumns*instRows; /* Must < 64*64 as sizeof meshInstances[] */

	/* Create MeshInstance array: Just change objmat. */
	for(int k=0; k<instTotal; k++) {
		printf("Create meshInstance[%d]...instTotal=%d\n", k,instTotal);
		meshInstances[k]=new E3D_MeshInstance(*workMesh);
		rtmat.setTranslation(0.4*dobj*(k%instColumns-2), 0.45*dobj*(k/instRows),  0);
		meshInstances[k]->objmat = rtmat*meshInstances[k]->objmat*VRTmat;
	}

#endif

#if !TEST_MESHINSTANCE /* CAUTION, if workMesh as refTriMesh of meshInstance[], then its data should NOT be changed! */
	/* W3. Transform workMesh */
	cout << "Transform workMesh...\n";
	//workMesh->transformMesh(RTYmat*RTXmat, ScaleMat); /* Here, scale ==1 */
	workMesh->transformMesh(VRTmat, ScaleMat); /* Here, scale ==1 */
#endif

#if TEST_MULTIOBJ    //////////////////  Add more objs into the scene (before omat_adjust)  /////////////////
	float bxang[3];
   #if 1 /* TEST: For add boxman into voluems.obj */
	boxman.shadeType=E3D_FLAT_SHADING; //E3D_TEXTURE_MAPPING;
	ScaleMat.identity();
	ScaleMat.setScaleXYZ(scale/30, scale/30, scale/30);  //workMesh scale=30, it=5;
	boxman.objmat.identity();
	//boxman.objmat.setRotation(axisX, MATH_PI*90/180); /* Object coord */
	bxang[0]=-MATH_PI*90/180; bxang[1]=MATH_PI*90/180;
	E3D_combExtriRotation("YX", bxang, boxman.objmat);
	boxman.objmat.addTranslation(5.0*scale, -4.0*scale, -6*scale); //24*scale);  //workMesh scale=30, dy=
	boxman.objmat=ScaleMat*boxman.objmat*VRTmat;  /* Sequence! */
	ScaleMat.identity();
   #endif

   #if 1 /* TEST: For add sportsmen into voluems.obj */
	sportsmen.shadeType=E3D_FLAT_SHADING; //E3D_TEXTURE_MAPPING;
	ScaleMat.identity();
	ScaleMat.setScaleXYZ(scale/100, scale/100, scale/100);  //workMesh scale=30, it=5;
	sportsmen.objmat.identity();
	bxang[0]=MATH_PI*90/180; bxang[1]=0.0; //MATH_PI*90/180;
	E3D_combExtriRotation("X", bxang, sportsmen.objmat);
	sportsmen.objmat.addTranslation(-2.0*scale, 10*scale, -6.0*scale); //(0.0, 8.0 -6.0)
	sportsmen.objmat=ScaleMat*sportsmen.objmat*VRTmat;  /* Sequence! */
	ScaleMat.identity();
  #endif

#endif


/* --------- TEST omat_adjust: AFTER transformMesh, we must adjust triGroupList[].omat accordingly.
	TODO: To apply above in transformMesh, AND to store VRTmat in TriMesh
*/
if(1) {
	int n;
	E3D_RTMatrix omat;
	E3D_RTMatrix TVRTmat=VRTmat;
	TVRTmat.transpose(); /* Inverse */
	for(n=0; n < (int)workMesh->triGroupList.size(); n++) {  /* Traverse all Trigroups */
		omat = TVRTmat*workMesh->triGroupList[n].omat*VRTmat;
		/* MUST ignore translation */
		for(unsigned int k=0; k<9; k++)
			workMesh->triGroupList[n].omat.pmat[k]=omat.pmat[k];
	}
}
/* --- END TEST --- */


	/* W3a. Update shadow Plane */
	if(shadow_on) {

#if 0 /* TEST: Rotate lighting, set option '-a -5' to stop rotating workmesh ----------- */
		E3D_RTMatrix gmat;
		E3D_RTMatrix rmat,tmat;

		/* W3a.1.  gmat for Origin offset to shadowPlane */
		if(rotAxis==1) /* Y */
			gmat.setTranslation(0, 1.0*Ty, 0);
		else if(rotAxis==2) /* Z */
			gmat.setTranslation(0, 0, 1.0*Tz);
		else /* X */
			gmat.setTranslation(1.0*Tx, 0, 0);

		/* W3a.2. Update RYmat for gv_vLgith */
		vltangle -=3.0;
		printf("angle=%f,vltangle=%f\n",angle, vltangle);
		if( !loopRender_on && abs(vltangle)>360-5.0 )
			exit(0);

		/* W3a.3. Set rmat as axisY rotation */
		if(rotAxis==1) /* Y */
			rmat.setRotation(axisY, 1.0*vltangle/180*MATH_PI);
		else if(rotAxis==2) /* Z */
			rmat.setRotation(axisZ, 1.0*vltangle/180*MATH_PI);
		else /* X */
			rmat.setRotation(axisX, 1.0*vltangle/180*MATH_PI);

/* NOTE: Here we transform the workMesh instead of the camera, so vLight SHOULD align with the camera COORD,
 *	 or shadowPlane will backface to vLights
 */

		/* W3a.4. Transform main lighting direction:: Rotating around COOR_Y */
		if(rotAxis==1) /* Y */
			gv_vLight.assign(4, -1, 0); //(3,-1,0)
		else if(rotAxis==2) /* X */
			gv_vLight.assign(-4, -1, 0);

		tmat=VRTmat;
		tmat.setTranslation(0,0,0);  /* tmat for normal transformation */
		gv_vLight.transform(rmat*tmat);
		gv_vLight.normalize();

		/* W3a.5. Rest gv_auxLight */
		gv_auxLight.assign(-1,-1,-1);
		gv_auxLight.normalize();
		gv_auxLight.transform(rmat*tmat);
 		gv_auxLight.normalize();

		/* W3a.6. Reset shadowPlan */
		if(rotAxis==1) /* Y */
			shadowPlane.vn = E3D_Vector(0.0, 1.0, 0.0);
		else if(rotAxis==2) /* Z --- CAUTION: Consider original position --*/
			shadowPlane.vn = E3D_Vector(0.0, 0.0, 1.0);
		else /* X */
			shadowPlane.vn = E3D_Vector(1.0, 0.0, 0.0);
		shadowPlane.fd = 0.0;

		/* W3a.7. Transform shadowPlane */
		shadowPlane.transform(gmat*VRTmat);
		printf("2 shadowPlane: fd=%f\n",shadowPlane.fd); shadowPlane.vn.print(NULL);

#else /* DO NOT move lighting */

		/* gmat for Origin offset to shadowPlane */
		E3D_RTMatrix gmat;

/* NOTE: Here we transform the workMesh instead of the camera, so vLight SHOULD align with the camera COORD,
 *	 or shadowPlane will backface to vLights
 */
                /* Set lighting */
                if(rotAxis==1) { /* Y */
	                gv_vLight.assign(-1,1,1); //fish
			//gv_vLight.assign(1, 1, 2);
			//gv_vLight.assign(1,-1, 6);
	                gv_vLight.normalize();
		}
                else if(rotAxis==2) { /* X */
                        //gv_vLight.assign(6, 1, 3);  //widora.obj -c -s 7 -A 2 -X -70 -P -W -t -M xxx.mot
			gv_vLight.assign(1,-1, 5); //widora.obj -c -s 8 -A 2 -X -70 -y -30 -P -W -t -M xxx.mot
	                gv_vLight.normalize();
			gv_auxLight.assign(-0.2,0,1);
			gv_auxLight.normalize();
		}
		//else

		/* gmat for Origin offset to shadowPlane */
		if(rotAxis==1) /* Y */
			gmat.setTranslation(0, 1.0*Ty, 0);
		else if(rotAxis==2) /* Z --- Consider original position ---  */
			gmat.setTranslation(0, 0, -1.0*Tz); /* -1.0*Tz >0! */
		else /* X */
			gmat.setTranslation(1.0*Tx, 0, 0);

		/* Reset shadowPlan */
		if(rotAxis==1) /* Y */
			shadowPlane.vn = E3D_Vector(0.0, 1.0, 0.0);
		else if(rotAxis==2) /* Z --- Consider original position --- */
			shadowPlane.vn = E3D_Vector(0.0, 0.0, -1.0);
		else /* X */
			shadowPlane.vn = E3D_Vector(1.0, 0.0, 0.0);
		shadowPlane.fd = 0.0;  /* aX+bY+cZ=0.0 */

		/* Transform shadowPlane */
		printf("2 shadowPlane before transform ------>plane.vn"); shadowPlane.vn.print(NULL);
		shadowPlane.transform(gmat*VRTmat);
		shadowPlane.vn.normalize();
		printf("2 shadowPlane: fd=%f, ------>plane.vn",shadowPlane.fd); shadowPlane.vn.print(NULL);
#endif

   } /* END shadow_on */


	/* W5. Set directFB to display drawing of each pixel. */
/* ---------->>> Set as NON_driectFB */
   	if( process_on ) {
	        fb_set_directFB(&gv_fb_dev,false);
	}

	/* Clear FB work_buffer */
        fb_clear_workBuff(&gv_fb_dev, bkgScreenColor);
	fb_init_zbuff(&gv_fb_dev, INT_MIN);
	gv_fb_dev.zbuff_on=true;

/* <<<----------  Render then Set as directFB, to see rendering process. */
	if( process_on ) {
		fb_render(&gv_fb_dev);
        	fb_set_directFB(&gv_fb_dev,true);
	}

#if 1	/* TEST: W7. Render as wire frames, on current FB image (before FB is cleared). */
	if(wireframe_on) { // && workMesh->shadeType!=E3D_TEXTURE_MAPPING) {
		cout << "Set as E3D_WIRE_FRAMING ...\n";
		fbset_color2(&gv_fb_dev, wireColor); //WEGI_COLOR_DARKGRAY);
		workMesh->shadeType=E3D_WIRE_FRAMING;
	}
#endif

#if 1   /* W6. Render the workMesh first */
//	if(workMesh->mtlList.size()>0)
//		faceColor=workMesh->mtlList[0].kd.color16Bits();

//	fbset_color2(&gv_fb_dev, faceColor);

        /* OR  updateAllTriNormals() here */

#if 0//TEST:
	if( frameCount%4==0 || frameCount%4==1 )
	   	workMesh->shadeType=E3D_GOURAUD_SHADING;
	else
		workMesh->shadeType=E3D_FLAT_SHADING;
#endif

        cout << "Render mesh ... angle=" << angle <<endl;
#if 1	/* Render workMesh */
//workMesh->shadeType=E3D_FLAT_SHADING;
       	RenderTriCnt=workMesh->renderMesh(&gv_fb_dev, projMatrix);
	printf("workMesh->renderMesh OK!\n");
#endif

	#if TEST_MESHINSTANCE  ///////////  Render Mesh Instance: Terracotta warrior   /////////
for(int k=0; k<instTotal; k++) {
	/* Render Mesh Instance */
	egi_dpstd(DBG_ORANGE"Render mesh instance[%d] ... angle=%.2f\n"DBG_RESET, k, angle);
	meshInstances[k]->renderInstance(&gv_fb_dev, projMatrix);

	/* Release and free */
        delete meshInstances[k];
}
#endif

#if TEST_MULTIOBJ  ///////////  More objs  ///////////
	printf("Render boxman...\n");
boxman.shadeType=E3D_TEXTURE_MAPPING;
	boxman.updateAllTriNormals(); /* For FLAT_SHADING */
	#if !TEST_SCENE
	boxman.renderMesh(&gv_fb_dev,projMatrix);
	#endif
	printf("Render sprotsmen...\n");
	gv_auxLight.assign(-1,1,1);
	gv_auxLight.normalize();
sportsmen.shadeType=E3D_FLAT_SHADING;
	sportsmen.updateAllTriNormals(); /* For FLAT_SHADING */
        #if !TEST_SCENE
	sportsmen.renderMesh(&gv_fb_dev, projMatrix);
	#endif
	gv_auxLight.assign(0,0,0);
#endif

#if TEST_SCENE  /////////////  Test E3D scene  ///////////
/* Note: ReftriMesh for instances are NOT listed in E3D_Scene::triMeshList /meshInstanceList here!!!
         it'is for test ONLY!
*/

    #if 0  /////////////  boxman + sprtsmen ////////////
	myScene.addMeshInstance(boxman);
	myScene.addMeshInstance(sportsmen);
	//myScene.addMeshInstance(workMesh);

    #else /////////////  BALLS, NOTE: set meshModel to be rta20   ////////////
	E3D_RTMatrix rtmat;
	int instColumns=9;//29;
	int instRows=5;//15;
	int instTotal=instColumns*instRows; /* Must < 64*64 as sizeof meshInstances[] */
	char strtmp[128];
	string strname;
	/* Create MeshInstance array: Just change objmat. */
	for(int j=0; j<2; j++) {
	   for(int k=0; k<instTotal; k++) {
		printf("Create meshInstance[%d]...instTotal=%d\n", k,instTotal);
		sprintf(strtmp, "ball_%02d", j*instTotal+k);
		strname=strtmp; //string strname(strtmp); ALSO OK!
		myScene.addMeshInstance(meshModel,strname); /* workMesh transformed!!! */
		rtmat.setTranslation(1.0*dobj*(k%instColumns-instColumns/2), (j==0?1.0:-1.0)*dobj*(k/instColumns)+((j==0?0.5:-0.5)*dobj), 0);
		myScene.meshInstanceList.back()->objmat = rtmat*myScene.meshInstanceList.back()->objmat*VRTmat;
	   }
	}
    #endif

	printf(DBG_ORANGE"Render E3D_Scene ...\n"DBG_RESET);

	/* Before renderSecne, re_compute clippling matrix items in projMatrix, in case dv changed etc. */
	E3D_RecomputeProjMatrix(projMatrix);
	myScene.renderScene(&gv_fb_dev, projMatrix);

	myScene.clearMeshInstanceList(); /* Clear list, for next round.  triMeshList keeps! */

#endif

	/* W6a. Render shadow */
	if(shadow_on) {
		cout << "Create shadow ..." << endl;
		workMesh->shadowMesh(&gv_fb_dev, projMatrix, shadowPlane);
	}

	/* W6b. Draw normal lines */
	//workMesh->faceNormalLen=20;
	//workMesh->drawNormal(&gv_fb_dev, projMatrix);

#endif

#if 0	/* W7. Render as wire frames, on current FB image (before FB is cleared). */
	//if(wireframe_on && workMesh->shadeType!=E3D_TEXTURE_MAPPING)
	{
		//gv_fb_dev.zbuff_on=false;
		fbset_color2(&gv_fb_dev, wireColor); //WEGI_COLOR_DARKGRAY);
		cout << "Draw meshwire ...\n";
		workMesh->drawMeshWire(&gv_fb_dev, projMatrix);
		//gv_fb_dev.zbuff_on=true;
	}
#endif

	/* W8. Draw the AABB, on current FB image. */
	if( AABB_on ) {
	        cout << "Draw AABB ...\n";
		fbset_color2(&gv_fb_dev, aabbColor);
		/* Use meshModel to draw AABB! */
		//meshModel.drawAABB(&gv_fb_dev, RTYmat*RTXmat, projMatrix);
		meshModel.drawAABB(&gv_fb_dev, VRTmat, projMatrix);
	}


#if 0 /* TEST:  Single Ray Trace Demo, ray hit and bounce.  rta20.obj -c -s 2 -X 25 -P -a 10 ------------ */
	int gindex, tindex;
	E3D_RTMatrix RTMatNone;
	E3D_Vector rayStart; /* Ray source */
	E3D_Vector Vinsct;    /* Intersection point */
	E3D_Vector reflectVd; /* Reflecting vector */
	E3D_Vector fnormal;   /* Surface/trimesh normal */

	float dd=meshModel.AABBdSize(); // *scale;
	//E3D_Vector rayStart(-dd,-dd,-dd);
	//E3D_Vector rayDirect(dd,dd,dd);

	//workMesh->updateAABB();
	E3D_Vector aabbCenter=workMesh->aabbCenter();
	aabbCenter.print("aabbCenter");

	E3D_Vector vtxsCenter=workMesh->vtxsCenter();
	vtxsCenter.print("vtxsCenter");
	vtxsCenter.y -=100;

	/* !!! --- CAUTION --- !!! Points out of frustum will cause projection error! wrong shape and position etc. */
	//E3D_Vector rayStart(-0.5*dd, -0.5*dd,  projMatrix.dv+0.2*dd);
	//E3D_Vector rayStart(-0.3*dd, -0.3*dd,  projMatrix.dv+0.2*dd);

	//rayStart=E3D_Vector(-0.3*dd, -0.3*dd,  projMatrix.dv+0.2*dd);

	float rayLen=dview-100;
	float Vang=70.0/180.0*MATH_PI;
	//float Hang=-30.0/180.0*MATH_PI;
	float Hang=-10.0/180.0*MATH_PI;

	/* Creat a rayStart: distributed on arc of a sphere */
	rayStart.x=rayLen*fabs(sin(Vang))*sin(Hang);
	rayStart.y=-rayLen*cos(Vang);
	rayStart.z=vtxsCenter.z-rayLen*fabs(sin(Vang))*cos(Hang);
	rayStart.print("rayStart");

	/* rayDirection */
	//E3D_Vector rayDirect(vtxsCenter.x+dd, vtxsCenter.y+dd, vtxsCenter.z+dd);
	E3D_Vector rayDirect=vtxsCenter-rayStart;

	/* Create the Ray */
	E3D_Ray Ray = E3D_Ray(rayStart, rayDirect);
	//Ray.vd.normalize();
	Ray.vp0.print("Ray_source");
	Ray.vd.print("Ray_direction");

	/* Ray hit mesh surface */
	int hitcnt=0;
	EGI_16BIT_COLOR raycolor;
	while(1) {
	   switch(hitcnt) {
	   	case 0: raycolor=WEGI_COLOR_RED; break;
		case 1: raycolor=WEGI_COLOR_GREEN; break;
		case 2: raycolor=WEGI_COLOR_BLUE; break;
		default: raycolor=WEGI_COLOR_GRAY; break;
	   }

	   if( workMesh->rayHitFace(Ray, gindex, tindex, Vinsct, fnormal) ) {
		printf("Ray hit mesh at triList[%d] of triGroupList[%d]: ", tindex, gindex);
		Vinsct.print(NULL);

//		E3D_Ray revRay=Ray; revRay.vd=-revRay.vd;
		printf("Incident angle: %fdeg \n", E3D_vector_angleAB(-Ray.vd, fnormal)*180/MATH_PI);

	        /* Notice: Viewing from z- --> z+ */
        	gv_fb_dev.flipZ=true;

		/* Draw incoming Ray for THIS hit. */
		fbset_color2(&gv_fb_dev,  raycolor);
		gv_fb_dev.zbuff_IgnoreEqual=false;
		E3D_draw_line(&gv_fb_dev, Ray.vp0, Vinsct, RTMatNone, projMatrix);
		gv_fb_dev.zbuff_IgnoreEqual=true;

		/* Reflecting vector, same mod as Ray.vd */
		reflectVd=E3D_vector_reflect(Ray.vd, fnormal);
		reflectVd.print("reflectVd");


        	gv_fb_dev.flipZ=false;

		/* Update the Ray for next hitFace check */
		Ray.vp0 = Vinsct;
		Ray.vd = reflectVd;

		/* Next intersection */
		hitcnt ++;
	   }
	   else {

		/* Draw the last Ray beam */
        	gv_fb_dev.flipZ=true;
		fbset_color2(&gv_fb_dev,  raycolor);
		gv_fb_dev.zbuff_IgnoreEqual=false;
		E3D_draw_line(&gv_fb_dev, Ray.vp0, Ray.vp0+Ray.vd, RTMatNone, projMatrix);
		gv_fb_dev.zbuff_IgnoreEqual=true;
        	gv_fb_dev.flipZ=false;

		break;
	   }

	} /* END while(1) */

	if(hitcnt==0)
		printf(DBG_BLUE"Ray NOT intersection with the workMesh\n"DBG_RESET);
	else
		printf(DBG_YELLOW"Total hit count: %d\n"DBG_RESET, hitcnt);
#endif

	/* W9. Draw the Coordinate Navigating_Sphere/Frame (coordNavSphere/coordNavFrame) */
	if(coordNavigate_on) {
	   E3D_draw_coordNavSphere(&gv_fb_dev, 0.4*workMesh->AABBdSize(), VRTmat, projMatrix);
	   E3D_draw_coordNavFrame(&gv_fb_dev, false, 0.5*workMesh->AABBdSize(), VRTmat, projMatrix);
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

		if(rotAxis==1) { /* Y */
			/* As original XZ plane grid of the mesh object. */
			gmat.setRotation(axisX, 90.0/180*MATH_PI);
			//float Ty=workMesh->aabbox.vmax.y;  /* !!! XXX It Changes! */
			printf("gmat.setTranslation Ty=%f, dSize=%d\n", Ty, dSize);
			gmat.setTranslation(0, Ty, 0);
		}
		else if(rotAxis==2) {/* Z */
			/* As original XY plane grid of the mesh object. */
			//gmat.setRotation(axisX, 0.0/180*MATH_PI);
			printf("gmat.setTranslation -Tz=%f, dSize=%d\n", -Tz, dSize);
			gmat.setTranslation(0, 0, -Tz);
		}
		else {  /* X */
			/* As original YZ plane grid of the mesh object. */
			gmat.setRotation(axisY, 90.0/180*MATH_PI); /* As original YZ plane grid of the mesh object. */
			printf("gmat.setTranslation Tx=%f, dSize=%d\n", Tx, dSize);
			gmat.setTranslation(Tx, 0, 0);
		}

		/* Too big value of sx,sy to get out of the View_Frustum */
	        E3D_draw_grid(&gv_fb_dev, 3.0*dSize, 3.0*dSize, dSize/10, gmat*VRTmat, projMatrix); /* fbdev sx,sy, us, VRTmat, projMatrix */
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
			(int)roundf(dvv*100.0f/dobj) );
			//dvv*100.0f/dobj); /* !!! (int) or fail to d% */
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

        /* Display shade type and faceColor */
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

	printf("--- %s ---\n", strtmp);

	#if 0
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

   	///////////////////////////////  Post_Render  ////////////////////////////////

/* ---TEST Record Screen: Save screen as a frame of a motion file. ------------------------- */
	if(saveMotion_on) {
		if( egi_imgmotion_saveFrame(fmotion, fbimg)!=0 ) {
			exit(-1);
		}
	}

	/* W13. Update angle */
	if(!test_clipping) {
		printf("Update angle...\n");
	     	angle += (5.0+da); // *MATH_PI/180;
	}
	else { // test_clipping. Keep dvv=dobj
		printf("VRTmat.translation.tz= %f\n", VRTmat.pmat[11]);
		printf("Update offz=%fdobj...\n", offz/dobj);
		offz += da/100.0*dobj;   /* Init: offz=0 and DelZ=offz+2*dobj, see VRTmat.setTranslation(offx, offy, offz +(dvv+dobj)) */
		if( offz+2*dobj < 0.5*dobj || RenderTriCnt==0 ) {
			da=fabsf(da);
		}
		//else if( offz+2*dobj > 1.5*dobj) {
		//	da=-fabsf(da);
		//}

		/* End clipping when moving back */
		E3D_MeshInstance workInst(*workMesh);
		workInst.objmat=VRTmat;  /* !!!, note workMesh DOES NOT hold it. */
		if( da>0.0f ) {
		   if( workInst.aabbInViewFrustum(projMatrix) ) {
			printf(DBG_YELLOW"workInst is completely in the ViewFrustum!\n"DBG_RESET);
		 	exit(0);
		   }
		   else {
			printf(DBG_YELLOW"workInst is partially out of the ViewFrustum!\n"DBG_RESET);
 		  }
		}
	}

	/* W13. Increase frameCount */
	frameCount ++;

#if 0 /* TEST: ----- gsit.obj change mtl, NOTE. mtlList[] will NOT recopy to workMesh.  */
     if(angle>=360.0 ) {
	/* mtlList[0]: overall, mtlList[1]: hair,  mtlList[3]: shirt */
	workMesh->mtlList[0].kd.vectorRGB( egi_color_random(color_medium) );
	workMesh->mtlList[1].kd.vectorRGB( egi_color_random(color_medium) );
	workMesh->mtlList[3].kd.vectorRGB( egi_color_random(color_medium) );
     }
#endif


#if 0	/* XXX If saveFB and serial playing. XXX */
	if( serialPlay_on ) {
	   if(angle>360) {
	 	//exit(0);
		/* Reset simg->submax */
		simg->submax=imgcnt-1;
		/* Loop play simg */
		while(1) {
			printf("Serial write simg...\n");
			egi_subimg_serialWriteFB(&gv_fb_dev, simg, 0, 0, 50);
		}
	   }
	   else {
		#if 0 /* Save serial image to file */
		sprintf(fpath,"/mmc/tmp/%03d.jpg", (int)angle);
		egi_save_FBjpg(&gv_fb_dev, fpath, 100);
		#else /* Save to simg */

		/* Copy FB image into simg, (destimg, srcimg, blendON, bw, bh, xd, yd, xs, ys) */
		if( imgcnt <= simg->submax ) {
			egi_imgbuf_copyBlock( simg, fbimg, false, xres, yres, xres*imgcnt, 0, 0, 0);
			imgcnt ++;
			printf("Simg adds a subimg: imgcnt=%d \n", imgcnt);
		}

		#endif
	   }
	}
#endif

	/* Hold on... */
	sleep(1);

	/* To keep precision: Suppose it starts with angle==0!  */
	if( (int)angle >=360 || (int)angle <= -360) {
		printf("Reset angle...\n");
		if((int)angle>=360) {
			angle -=360;
			if(!loopRender_on)
				exit(0);     /* <------------- TEST */
		}
		if((int)angle<-360) angle +=360;

		/* Random face color */
		faceColor=egi_color_random(color_all);
		if(egi_color_getY(faceColor) < 150 )
		faceColor=egi_colorLuma_adjust(faceColor, 150-egi_color_getY(faceColor));

//		projMatrix.type=!projMatrix.type; /* Switch projection type */

#if 0		/* Switch shadeType: Loop FLAT --> WIRE --> TEXTURE  */
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

#if TEST_MTXT_TEXTURE /* ------TEST: Imgbuf texture with FTsymbols, update teximg.  --------- */

	/* If it has input texture file, the copy from original texture imgbuf */
	if( refimg ) {
		egi_imgbuf_free2(&teximg);
		teximg=egi_imgbuf_blockCopy(refimg, 0,0, refimg->height, refimg->width);
		if(teximg==NULL) exit(-1);
	}
	/* Reset color for teximg */
	else
		egi_imgbuf_resetColorAlpha(teximg, texcolor, -1);

	sx -= 2048/200;
//	sy -= 2048/100;
	sy += (2048/200)*tanf(MATH_PI*txtangle/180.0);

	FTsymbol_uft8strings_writeIMG2( teximg, egi_appfonts.bold, fw, fh, ustr, /* imgbuf, face, fw, fh, pstr */
                                        2000, 5, 20,             /* pixpl, lines, gap */
                                        sx, sy, txtangle, txtcolor, /* x0, y0, angle, fontcolor */
                                        NULL, NULL, NULL,NULL); /* *cnt, *lnleft, *penx, *peny */
#endif

	/* W14. Update dvv: Distance from the Focus to the Screen/ViewPlane */
	if(focalenVarying_on) {
		printf("Stepping dvv...\n");
		dvv += dvv*dstep; /* Keep percentage increment */
		//if( dvv > 1.9*dobj || dvv < 0.1*dobj )
		if( dvv < 0.1*dobj )
			dstep=-dstep;
		else if( dvv > 2.75*dobj)
			exit(0);
	}


	/* W15. Adjust global light vector */
#if TEST_VLIGHTING
	if(vang > 45.0 +360) exit(0);
	vang +=dvang;
	vLight.x=1.414*cosf(MATH_PI*vang/180);
	vLight.y=1.414*sinf(MATH_PI*vang/180);
	vLight.z=1; //2,4;
	vLight.normalize();
	vLight.print();
	gv_vLight=vLight;
#endif

   }


#else //////////////////////  OPTION_2. Keep meshModel and Change View Postion  /////////////////////////
			   /*  Loop Rendering: renderMesh(fbdev, &VRTMatrix)  */

	/* Rotation axis/ Normalized. */
//	E3D_Vector axisX(1,0,0);
//	E3D_Vector axisY(0,1,0);
//	E3D_Vector axisZ(0,0,1);
//	E3D_Vector vctXY;  /* z==0 */

	/* Prepare View_Coord VRTMatrix as relative to Global_Coord. */
//	E3D_RTMatrix VRTmat;
//	E3D_RTMatrix RXmat, RYmat, RZmat;
//	E3D_RTMatrix ScaleMat;
//	float angle=0.0;

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
        meshModel.updateAllTriNormals(); /* For FLAT_SHADING */
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
	cout<<"rotAxis="<<rotAxis<<endl;
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
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
                                        16, 16,                           /* fw,fh */
                                        (UFT8_PCHAR)strtmp, 		  /* pstr */
                                        300, 4, 4,                        /* pixpl, lines, fgap */
                                        5, 4,                             /* x0,y0, */
                                        fontColor, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                        20, 20,                         /* fw,fh */
                                        (UFT8_PCHAR)"E3D MESH", /* pstr */
                                        300, 1, 0,                        /* pixpl, lines, fgap */
                                        xres-120, yres-28,                /* x0,y0, */
                                        fontcolor2, -1, 255,        /* fontcolor, transcolor,opaque */
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
