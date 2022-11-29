/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

	            --- EGI 3D Triangle Mesh ---

Refrence:
1. "3D Math Primer for Graphics and Game Development"
                        by Fletcher Dunn, Ian Parberry

		   --- Definition and glossary ---

Object:	   The TriMesh object, usually imported from an .obj file,
	   it contains at leat one TriGroup.
	   All vertices of an object are under Object COORD,
	   the Object COORD coincides with the Global COORD initially,
	   and object.objmat transforms the object to under Global COORD.

TriGroup:  A triangle group in an TriMesh object.
	   All vertices of the TriGroup are under Object COORD as stated above.
	   TriGroup.pivot is the pivot of the TriGroup, which is also
	   under Object COORD!
	   The TriGroup COORD coincides with the Object COORD initially,
	   and TriGroup.omat transforms the TriGroup, however to under Object COORD ALSO!

Note:
0. E3D viewpoint for RTMatrix and transformation computation:
		 P'xyz = Pxyz*RTMatrix. ( Pxyz is row vector here.  NOT column vector! )
		 P'xyz = Pxyz*Ma*Mb*Mc = Pxyz*(Ma*Mb*Mc)
   and notice the sequence of Ma*Mb*Mc as affect to Pxyz.

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
		    objects to expected position.
   E3DS_ProjMatrix:  Projection Matrix, to project/map objects in a defined
		    frustum space to the Screen.

3. Since view direction set as ViewCoord(Camera) -Z ---> +Z, it needs to set FBDEV.flipZ=true
   to flip zbuff[] values.

4. The origin of texture vertex coordinates(uv) may be different:
   E3D: Left_TOP corner
   3ds: Left_BOTTOM corner.

5. Size of texture image should be approriate: ???
   Function egi_imgbuf_mapTriWriteFB() maps pixels one by one, if the texture image
   size is too big, the two mapped pixels may be located far between each other!
   so the result looks NOT smooth. --- more sampling and averaging.

6. Normal values in Class E3D_TriMesh:
   vtxList[].normal: 	    Vertex normal, for each vertex element in the class.
		 	    (Maybe omitted in obj file!)
   triList[].vtx[0-2].vn:   Triangle vertex normal (for each vertex of a triangle).
			    If omitted. then same as vtxList[].normal.
   triList[].normal:        Triangle face normal.

7. Transformation matrix for all normals(Vertex normals, TriFaceNormals, TriVtxNormals)
   should contain ONLY rotation components, and keep them all as UNIT normals!
   (whose vector norm is 1! )


TODO:
1. FBDEV.zbuff[] is integer type, two meshes MAY be viewed as overlapped if distance
   between them is less than 1!
2. XXX NOW: All pixels of a triangle use the same pixz value for zbuff[], which is the
   coord. Z value of the barycenter of this trianlge. This causes zbuff error in some
   position! To turn on back facing triangles to see it clearly.
   ( Example: 1. At outline edges  2. double shell meshes... )

   To compute zbuff value for each pixel at draw_filled_triangle3() and egi_imgbuf_mapTriWriteFB3().
   NOW: draw_filled_triangle4() with float Z0/Z1/Z2.

3. Different algrithms for drawing triangles may result in inconsistent appearance (in edges especially).
   Example:
	draw_triangle() and draw_filled_triangle() get outlines a little different.
	draw_filled_triangle() and draw_filled_triangle3() result in different coverage of pixels.

   To check: draw wire frames (edges) first, then draw faces, there will be some wires(edges)
   left uncovered by face pixels.

3.1 Since all triangles are drawn by filling with pixels, it may appear some leaking(unfilled) points
    along the seam of two triangles.
    Especially when these two triangles are NOT on the same plane. ???
    OR when the jointing edges are NOT the same length. ???

3.2 Given that FBDEV.pixZ is integer type, there will be overlapped/interlaced points along seam of two triangles
    because of Z-depth accuracy.

XXX 4. Option to display/hide back faces of meshes.

4.  In case that triangle degenerates into a line, we MUST figure out the
    facing(visible) side(s)(with the bigger Z values), before we compute color/Z value
    for each pixel. ( see. in draw_filled_trianglX() and egi_imgbuf_mapTriWriteFBX() )

5.  XXX If some points are within E3D_ProjectFrustum.dv(Distance from the Foucs to viewPlane/projetingPlane)
    Then the projected image will be distorted!  --- To improve projection algrithm E3D_TriMesh.projectPoints()

6.  E3D_TriMesh.projectPoints(): NOW clip whole Tri if any vtx is out of frustum.

7.  For imported TriMesh, its default face normal values(triList[].normal) are all zeros, and result of FLAT_SHADING wil be all BLACK,
    updateAllTriNormals() to generate face normals.

x. AutoGrow vtxList[] and triList[].


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
2021-08-07:
	1. E3D_TriMesh(const char *fobj):
	   1.1 If A face data line uses '/' as delim, and ends with a SPACE ' '
	   	, it would be parsed as a new vertex!!!  ---OK, corrected!
	   	example: f 3/3/3 2/2/2 5/5/5 ' '.
	   1.2 To subdivide a 4_edge_polygon to TWO triangles correctly!
	2. Add updateVtxsCenter().
	3. Add moveVtxsCenterToOrigin()
	4. E3D_TriMesh::renderMesh(): A simple Zbuff test.
	   E3D_TriMesh::drawMeshWire(): A simple zbuff test.
2021-08-10:
	1. Rename: transformTriNormals() TO transformAllTriNormals()!
	2. Add E3D_TriMesh::renderMesh(FBDEV *fbdev, const E3D_RTMatrix & ViewRTmatrix);
2021-08-11:
	1. Add E3D_TriMesh::AABB class, and its functions.
	2. Add E3D_TriMesh::drawAABB(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix);
	3. Apply ScaleMatrix for transformMesh() and renderMesh().
2021-08-13:
	1. Correct transform matrixces processing, let Caller to flip
	   Screen_Y with fb_position_rotate().
2021-08-15:
	1. TEST_PERSPECTIVE for:
	   E3D_TriMesh:: drawMeshWire(FBDEV *fbdev)/drawAABB()/renderMesh(FBDEV *fbdev)
2021-08-16:
	1. drawMeshWire(FBDEV *fbdev): roundf for pts[].
2021-08-17:
	1. Modify: E3D_TriMesh::reverseAllTriNormals(bool reverseVtxIndex);
	2. Add: E3D_TriMesh::reverseAllVtxZ()
2021-08-18:
	1. Modify E3D_TriMesh::E3D_TriMesh(const char *fobj): To parse max.64 vertices in a face.
	2. Add  E3D_TriMesh::float AABBdSize()
	3. Add  E3D_TriMesh::projectPoints()
			--- Apply Projection Matrix ---
	4. Modify E3D_TriMesh::renderMesh() with ProjMatrix.
	5. Modify E3D_TriMesh::drawAABB() with ProjMatrix
2021-08-19:
	1. Add E3D_TriMesh::moveAabbCenterToOrigin().
2021-08-20:
	1. Function projectPoints() NOT an E3D_TriMesh member.
	2. Add E3D_draw_line() E3D_draw_circle() form e3d_vector.h
	3. Add E3D_draw_coordNavSphere()
	4. Add E3D_draw_line(FBDEV *, const E3D_Vector &, const E3D_RTMatrix &, const E3DS_ProjMatrix &)
2021-08-21:
	1. readObjFileInfo(): Some obj txt may have '\r' at line end.
2021-08-22:
	1. Rename NormalCount-->vtxNormalCount, textureCount--->textureVtxCount.
	2. Modify E3D_TriMesh::E3D_TriMesh(const char *fobj):
	   Read texture vertices into TriMesh.
	3. Add E3D_TriMesh::readTextureImage(const char *fpath).
	4. E3D_TriMesh::renderMesh(): map texture.
2021-08-24:
	1. Add E3D_TriMesh::shadeType
2021-08-27:
	1. Modify E3D_TriMesh::renderMesh()
	   Add case E3D_WIRE_FRAMING: to drawing frame wires.
2021-08-28:
      >>>> Add triangle vertex normals: triList[].vtx[].vn  for GOURAUD shading <<<<
	1. E3D_TriMesh(const char *fobj)
	   Read OBJ vertex noramls into
		1.1 TriMesh::triList[].vtx[].vn   <-----
	    OR  1.2 TriMesh::vtxList[].normal	  <-----
	2. renderMesh()
	   Add case E3D_GOURAUD_SHADING: to draw faces with gouraud shading.
	3. cloneMesh(const E3D_TriMesh &tmesh)
	   Add copy triList[].vtx[].vn    <-----
	4. transformAllTriNormals(const E3D_RTMatrix  &RTMatrix)
	   Add Transform triList[].vtx[].vn   <-----
	5. transformVertices(const E3D_RTMatrix  &RTMatrix)
	   Add Transform vtxList[].normal     <-----
	6. Add E3D_TriMesh::transformAllVtxNormals() to transform vtxList[].normal.
	7. Add reverseAllVtxNormals().
2021-09-03:
	1. Add void E3D_draw_coordNavIcon2D()
2021-09-14:
	1. Add E3D_TriMesh member 'backFaceOn' and 'bkFaceColor'.
2021-09-18:
	1. Add E3D_TriMesh::computeAllVtxNormals()
	   Add E3D_TriMesh::vtxNormalIsZero()
2021-09-22:
	1. Add E3D_TriMesh::initVars().
	2. Add Class E3D_Material.
2021-09-23:
	1. Add readMtlFile(const char *fmtl, const char *mtlname, E3D_Material &material)
2021-09-24:
	1. Add readMtlFile(const char *fmtl, vector<E3D_Material> & mtlList)
2021-09-25:
	1. E3D_TriMesh::E3D_TriMesh(const char *fobj): read and add triGroupList[].
	2. Add  E3D_TriMesh::getMaterialID()
2021-09-26/27:
	1. E3D_TriMesh::renderMesh(): Apply triGroupList[] and its material.color/map.
	2. E3D_TriMesh::cloneMesh(): Clone more class members.
2021-10-04:
	1. Add: E3D_draw_grid()
2021-10-05:
	1. Add: E3D_TriMesh::TriGroup::E3D_RTMatrix  omat
	2. E3D_TriMesh::E3D_TriMesh(const char *fobj):
	   	Read case 'x' for RTMatrix translation pmat[9,10,11]: tx,ty,tz.
	3.  ---  Function related to omat ---
	    >3.0  E3D_TriMesh::cloneMesh() ---Ok
	    3.1   E3D_TriMesh::transformMesh():
		  Transform TriGroup.omat.pmat[9-11] as group origin.
	    >3.2  E3D_TriMesh::scaleMesh();
	    >3.3  E3D_TriMesh::moveVtxsCenterToOrigin()
	    >3.4  E3D_TriMesh::moveAabbCenterToOrigin()   OK...2021-10-19
2021-10-07:
	1. Add E3D_draw_line(fbdev, va, vb, RTmatrix, projMatrix)
	2. Add E3D_TriMesh::objmat AS Orientation/Postion of the trimesh object relative
	   to Global COORD.
2021-10-15:
	1. Add class E3D_PivotKeyFrame
2021-10-16:
	1. Modify E3D_TriMesh::updateAABB(): Init with vtxList[0].pt!
	2. Modify readObjFileInfo():  Break for SPACE in case 'v'.
2021-10-17:
	1. E3D_TriMesh::moveAabbCenterToOrigin(): Reset objmat.pmat[9-11] to all as 0.0!
	2. E3D_draw_coordNavFrame(FBDEV *fbdev, int size, bool minZ): Option to show -Z axis.
	3. readMtlFile()s: Apply cstr_trim_space() for material.map_kd=saveptr!
	4. E3D_TriMesh::E3D_TriMesh(const char *fobj): If NO 'g' in .obj file, then all grouped as triGroupList[0],
	   and if mtlList is NOT empty, then assign mtlList[0] to the only TriGroup,
2021-10-19:
	1. Add E3D_TriMesh::TriGroup member: pivot.
        2. Functions related to TriGroup pivot ---
            >2.0  E3D_TriMesh::cloneMesh()
            >2.1  E3D_TriMesh::transformMesh():
            >2.2  E3D_TriMesh::scaleMesh();
            >2.3  E3D_TriMesh::moveVtxsCenterToOrigin()
	    >2.4  E3D_TriMesh::moveAabbCenterToOrigin()
	    >2.5  E3D_TriMesh::E3D_TriMesh(const char *fobj): read pivot.
	    >2.6  E3D_TriMesh::renderMesh()
2021-11-1:
	1. Add E3D_TriMesh::TriGroup member: hidden.
2021-11-4:
	1. Modify projectPoints(): return value ret.
2021-11-7:
	1. E3D_TriMesh::renderMesh(): backface color also relate with vProduct value.
2021-11-8:
	1. Add E3D_TriMesh member wireFrameColor.
2021-11-9:
	1. E3D_TriMesh::renderMesh() -> Test  draw_filled_triangle4() for GOURAUD_SHADING.
				              draw_filled_triangle2() for FLAT_SHADING.
2022-08-04:
	1. E3D_TriMesh::renderMesh:  Test auxiliary lights.
	2. Add gv_auxLight.
2022-08-07:
	1. Add E3D_TriMesh::shadowMesh()
2022-08-09:
        1. Create e3d_trimesh.cpp from e3d_trimesh.h
2022-08-23:
	1. Add reverse_projectPoints().
2022-08-24:
	1. Add E3D_TriMesh::rayHitFace()
2022-08-29:
	1. Add E3D_TriMesh::drawNormal()
2022-08-31:
	1. E3D_TriMesh::renderMesh(): backward_ray_tracing shadow.
2022-09-01:
	1. Add E3D_TriMesh::rayBlocked()
2022-09-05:
	1. Add void E3D_draw_line(fbdev, &va, &vb, &projMatrix)
2022-09-06:
	1. E3D_TriMesh::rayHitFace(): Add rgindx/rtindx for Ray.vp0.
2022-09-07:
	1. projectPoints() / reverse_projectPoints():  project/reverse pts[].Z to/from Pseudo-depth z.
2022-09-11:
	1. E3D_TriMesh(const char *fobj): add case 'r', case 't'.
2022-09-13:
	1. Apply triGroupList[] in E3D_TriMesh::drawMeshWire(FBDEV *fbdev, const E3DS_ProjMatrix &projMatrix).
	2. Add E3D_TriMesh::TriGroup:: bool ignoreTexture
2022-09-15:
	1. Add E3D_TriMesh::TriGroup::backFaceOn, and (E3D_TriMesh::backFaceOn || E3D_TriMesh::TriGroup::backFaceOn) takes effects.
	2. E3D_TriMesh::E3D_TriMesh(obj file): Parse "bf ON" as set TriGroup.backFaceOn.
	3. Add class E3D_Scene.  and E3D_Scene::importObj().
2022-09-17:
	1. Add E3D_seeColor().
2022-09-18:
	1. readMtlFile(): Add KdRBG,KaRGB,KsRGB
2022-09-19:
	1. Add E3D_TriMesh::Vertex::assign()
	2. Add E3D_TriMesh::Triangle::assignVtxIndx()
2022-09-21:
	1. E3D_TriMesh::renderMesh(): Check cases and assign actShadeType before rendering triGroup.
2022-09-24:
	1. Add Casll E3D_MeshInstance, and constructor + destructor.
	2. Add E3D_Scene::meshInstanceList
2022-09-26:
	1. Add E3D_MeshInstance::renderInstance()
2022-09-27:
	1. Add E3D_Scene::renderScene()
	2. Add E3D_Scene:addMeshInstance()
	3. clearMeshInstanceList()
2022-09-28:
	1. Add mapPointsToNDC().
	2. Add pointOutFrustumCode().
2022-09-29:
	1. Add E3D_MeshInstance::aabbOutViewFrustum()
2022-10-03:
	1. Add struct E3D_RenderVertex.
	2. Add E3D_ZNearClipTriangle()
	3. E3D_ProjMatrix ----->  E3DS_ProjMatrix, 'S' stands for 'struct',as distinct from 'class'.
2022-10-05:
	1. E3D_TriMesh::renderMesh():  return RenderTriCnt as total number of triangles that are finally rendered.
2022-10-08:
	1. Add Class E3D_Light.
	2. Add  E3D_MeshInstance::aabbInViewFrustum()
2022-10-09:
	1. mapPointsToNDC(): Conside E3D_ISOMETRIC_VIEW
2022-10-11:
	1. Add E3D_shadeTriangle()
2022-10-12:
	1. readMtlFile(): Add Ns as specular exponent.
	2. E3D_TriMesh::renderMesh(): TEST E3D_shadeTriangle() for case TEXTURE_MAPPING.
2022-10-18:
	1. E3D_TriMesh::renderMesh():  TEST_Y_CLIPPING.
2022-10-18:
	1. E3D_shadeTriangle(): Apply pixel normal.
2022-10-24:
	1. readMtlFile(): Add Ss,Sd,ke
2022-10-25:
	1. Add E3D_TriMesh::moreVtxListCapacity()
	2. Add E3D_TriMesh::moreTriListCapacity()

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
知之者不如好之者好之者不如乐之者
-----------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>
#include "egi_debug.h"
#include "e3d_vector.h"
#include "egi.h"
#include "egi_fbgeom.h"
#include "egi_color.h"
#include "egi_image.h"
#include "egi_cstring.h"

#include "e3d_trimesh.h"

using namespace std;

//#define VPRODUCT_EPSILON	(1.0e-6)  /* -6, Unit vector vProduct=vct1*vtc2 -----> See e3d_vector.h */

/* Camera view direction: from -z to +z */

/* Global light vector, as UNIT vecotr! OR it will affect face luma in renderMesh()  */
E3D_Vector gv_vLight(0.0, 0.0, 1.0);	 /* (vector) Variable lighting.  factor=1.0 when rendering mesh */
E3D_Vector gv_auxLight(1.0, 0.0, 0.0);   /* (vector) factor=0.5 when rendering mesh */

E3D_Vector gv_ambLight(0.25, 0.25, 0.25);  /* (color, NOT vector) Global ambient lighting */

/* Mesh Rendering/Shading type */
const char* E3D_ShadeTypeName[]=
{
	"E3D_FLAT_SHADING",
	"E3D_GOURAUD_SHADING",
	"E3D_WIRE_FRAMING",
	"E3D_TEXTURE_MAPPING"
};


	/*------------------------------------
	    Class E3D_Light :: Functions
	-------------------------------------*/
/*---------------------------
Set default for memebers.
----------------------------*/
void E3D_Light::setDefaults()
{
        /* Position and orientation */
	pt.assign(0.0f, 0.0f, 0.0f);

	/* Light type */
	type = E3D_DIRECT_LIGHT;

	/* Light Direction */
	direct.assign(0.0f, 0.0f, 1.0f);

        /* Source light specular color */
	Ss.assign(0.75f, 0.75f, 0.75f);
        /* Source light diffuse color */
	Sd.assign(0.8f, 0.8f, 0.8f);

        /* Attenuation factor for light intensity */
        atten=0.75f;
}

/*--------------------
   The constructor
--------------------*/
E3D_Light::E3D_Light()
{
	setDefaults();
}

/*--------------------
   The constructor
--------------------*/
E3D_Light::~E3D_Light()
{

}


	/*------------------------------------
	    Class E3D_Material :: Functions
	-------------------------------------*/

/*---------------------------
Set default for memebers.
----------------------------*/
void E3D_Material::setDefaults()
{
	illum=0;
	d=1.0;
	Ns=0.0;
	//Tr=0.0;
	kd.vectorRGB(COLOR_24TO16BITS(0xF0CCA8)); //WEGI_COLOR_PINK); /* Default kd */
//	kd.vectorRGB(COLOR_24TO16BITS(0xE0DCA8));
	ka.assign(0.8f, 0.8f, 0.8f); //ka.vectorRGB(WEGI_COLOR_GRAY2); /* Default ka */
	ks.assign(0.0f, 0.0f, 0.0f); //vectorRGB(WEGI_COLOR_GRAY); /* Default ks */
	ke.assign(0.0f, 0.0f, 0.0f);

	/* Light source specular and diffuse color */
	Ss.assign(1.0f, 1.0f, 1.0f);
	Sd.assign(1.0f, 1.0f, 1.0f);

	if(!name.empty())
		name.clear();

	map_kd.clear();
	map_ka.clear();
	map_ks.clear();

	img_kd=NULL;
}


/*----------------------------
Print out main vars/paramters
of the materail
-----------------------------*/
E3D_Material::E3D_Material()
{
	setDefaults();
}

/*----------------------------
Print out main vars/paramters
of the materail
-----------------------------*/
E3D_Material::~E3D_Material()
{
	/* Free imgbuf, and reset img_kd to NULL. */
	egi_imgbuf_free2(&img_kd);
}

/*----------------------------
Print out main vars/paramters
of the materail
-----------------------------*/
void E3D_Material::print()
{
	cout<<"------------------------"<<endl;
	cout<<"Material name: "<< name <<endl;

	cout<<"illum: "<< illum <<endl;
	ka.print("ka");
	kd.print("kd");
	ks.print("ks");
	cout<<"d: "<< d <<endl;
	cout<<"map_kd: "<< map_kd <<endl;
	cout<<"img_kd is "<< (img_kd==NULL?"NULL!":"loaded.") <<endl;
//	cout<<"map_ka: "<< map_ka <<endl;
//	cout<<"map_ks: "<< map_ks <<endl;

	cout<<"------------------------"<<endl;
	cout<<endl;
}


/*-------------------  OBOSELETE! -------------------------------------------
Read material from an .mtl file.

		---- OBJ .mtl Material File Format ----

xxx.mtl		Name of obj mtl file, as specified in the .obj file:
		mtllib xxx.mtl
newmtl  ABC	Use material ABC
illum  [0 10]   Illumination model
Kd		Diffuse color, rgb, each [0-1.0]
Ka		Ambient color, rgb, each [0-1.0]
Ks		Specular color,rgb, each [0-1.0]
KdRGB		Diffuse color, RGB each [0-255]
KaRGB		Ambient color, RGB each [0-255]
KsRGB		Specular color, RGB each [0-255]
Tf		Transmission filter
Ni		Optical density
d		Dissolve factor
//Tr		Transparency
map_Kd		Diffuse map
map_Ka		Ambient map
map_Ks		Specular map
map_d		Alpha map
map_bump/bump   Bump map

@ftml: 		Path for the .mtl file.
@mtlname:	Material name descripted in the mtl file.
		If NULL, ignore the name. Just read first newmtl
		if the mtl file.
@material:	E3D_Material

Return:
	>0	Material name NOT found int he mtl file.
	0	OK
	<0	Fails
----------------------------------------------------------------*/
int readMtlFile(const char *fmtl, const char *mtlname, E3D_Material &material)
{
	ifstream  fin;
	char strline[1024];
	const char *delim=" 	";    /* space AND tab, \r\n */
	char *pt=NULL;
	char *saveptr=NULL;
	bool  found_newmtl=false;
	char *pname=NULL;

	if(fmtl==NULL)
		return -1;

	/* Open .mtl file */
	fin.open(fmtl);
	if(fin.fail()) {
		egi_dperr("Fail to open '%s'",fmtl);
		return -2;
	}

	/* Read data */
	strline[sizeof(strline)-1]=0;
	while( fin.getline(strline, sizeof(strline)-1) ) {
//		egi_dpstd("strline: %s\n", strline);

		pt=strtok_r(strline, delim, &saveptr);
		if(pt==NULL) continue;

		/* New materila: check first keyword */
		if( strcmp(pt, "newmtl")==0 ) {
			/* If alread reading newmtl, end of current material. */
			if(found_newmtl==true)
				break;

			/* Get rid of any type of line EOFs! */
			pname=cstr_trim_space(saveptr); /* cstr_trim_space() Modifys saveptr!*/

			/* Check material name */
			if( mtlname!=NULL && strcmp(pname,mtlname)!=0 )
				continue;

			/* ELSE: find the materil */
			material.name=pname;
			found_newmtl=true;
		}

		/* Until find the newmtl name string */
		if(found_newmtl == false)
			continue;

		/* Ka */
		if( strcmp(pt,"Ka")==0 ) {
//			printf("saveptr: %s\n", saveptr);
			sscanf(saveptr, "%f %f %f", &material.ka.x, &material.ka.y, &material.ka.z); /* rgb[0 1] */
		}
		/* KaRGB */
		else if( strcmp(pt,"KaRGB")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.ka.x, &material.ka.y, &material.ka.z);
			material.ka.x /= 255; material.ka.y /= 255; material.ka.z /= 255;
		}
		/* map_Ka */
		else if( strcmp(pt,"map_Ka")==0 ) {
			material.map_ka=cstr_trim_space(saveptr);
		}
		/* Kd */
		else if( strcmp(pt,"Kd")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.kd.x, &material.kd.y, &material.kd.z); /* rgb[0 1] */
		}
		/* KdRGB */
		else if( strcmp(pt,"KdRGB")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.kd.x, &material.kd.y, &material.kd.z);
			material.kd.x /= 255; material.kd.y /= 255; material.kd.z /= 255;
		}
		/* map_Kd */
		else if( strstr(pt,"map_Kd") ) {
			material.map_kd=cstr_trim_space(saveptr);
		}
		/* Ks */
		else if( strcmp(pt,"Ks")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.ks.x, &material.ks.y, &material.ks.z); /* rgb[0 1] */
		}
		/* KsRGB */
		else if( strcmp(pt,"KsRGB")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.ks.x, &material.ks.y, &material.ks.z);
			material.ks.x /= 255; material.ks.y /= 255; material.ks.z /= 255;
		}
		/* map_Ks */
		else if( strstr(pt,"map_Ks") ) {
			material.map_ks=cstr_trim_space(saveptr);
		}

		/* illum */
		else if( strcmp(pt,"illum")==0 ) {
			sscanf(saveptr, "%d", &material.illum); /* illum [0-10] */
		}
		/* d */
		else if( strcmp(pt,"d")==0 ) {
			sscanf(saveptr, "%f", &material.d); /* d [0 1] */
		}
		#if 0 /* Tr */
		else if( strcmp(pt,"Tr")==0 ) {
			sscanf(saveptr, "%f", &material.Tr); /* Tr [0 1] */
		}
		#endif
	}

	/* Check data */
	// if( material.kd.x > 1.0 || materil.kd.y >1.0 || material.kd.z >1.0 )

END_FUNC:
	/* Close fmtl */
	fin.close();

	if(found_newmtl) return 0;
	else {
	      egi_dpstd("Materil '%s' NOT found in .mtl file!\n", mtlname);
	      return 1;
	}
}


/*----------------------------------------------------------------
Read material from an .mtl file.

		---- OBJ .mtl Material File Format ----

xxx.mtl		Name of obj mtl file, as specified in the .obj file:
		mtllib xxx.mtl
newmtl  ABC	Use material ABC
illum  [0 10]   Illumination model
Kd		Diffuse color, rgb, each [0-1.0]
Ka		Ambient color, rgb, each [0-1.0]
Ks		Specular color,rgb, each [0-1.0]
Ke		Self_emission color, rgb, reach [0-1.0]
KdRGB		Diffuse color, RGB each [0-255]
KaRGB		Ambient color, RGB each [0-255]
KsRGB		Specular color, RGB each [0-255]
Tf		Transmission filter
Ni		Optical density
Ns		Specular exponent
d		Dissolve factor
Ss		Light source specular color [0 1], Default 0.8
Sd		Light source diffuse color [0 1], Default 0.8
//Tr		Transparency
map_Kd		Diffuse map
map_Ka		Ambient map
map_Ks		Specular map
map_d		Alpha map
map_bump/bump   Bump map

Note:
1. Path for map texture file should be correctly set in the.mtl file,
   OR it will fail to load into imgbuf.
   It's a good idea to use full paths in the .mtl file.

@ftml: 		Path for the .mtl file.
@mtlList:	E3D_Material list/vectr.
		It will be cleared first!
Return:
	0	OK
	<0	Fails
----------------------------------------------------------------*/
int readMtlFile(const char *fmtl, vector<E3D_Material> & mtlList)
{
	ifstream  fin;
	char strline[1024];
	const char *delim=" 	";    /* space AND tab, \r\n */
	char *pt=NULL;
	char *saveptr=NULL;
	char *pname=NULL;

	E3D_Material material;
	int   mcnt=0;		/* Material counter */

	unsigned int xrgb;

	if(fmtl==NULL)
		return -1;

	/* Clear mtlList */
	if(!mtlList.empty())
		mtlList.clear();

	/* Open .mtl file */
	fin.open(fmtl);
	if(fin.fail()) {
		egi_dperr("Fail to open '%s'",fmtl);
		return -2;
	}

	/* Read data */
	strline[sizeof(strline)-1]=0;
	while( fin.getline(strline, sizeof(strline)-1) ) {
		egi_dpstd("strline: %s\n", strline);

		pt=strtok_r(strline, delim, &saveptr);
		if(pt==NULL) continue;

		/* New materila: check first keyword */
		if( strcmp(pt, "newmtl")==0 ) {
			/* Get rid of any type of line EOFs! */
			pname=cstr_trim_space(saveptr); /* cstr_trim_space() Modifys saveptr!*/

			/* If first item, just set name for material. */
			if(material.name.empty()) {
				/* Set name */
				material.name=pname; //saveptr;
				egi_dpstd("Material name '%s' found.\n", pname); //saveptr);
			}
			/* Else, push current material into mtlList before start reading a new one. */
			else {
				//egi_dpstd("Start to push_back material '%s'.\n", material.name.c_str());
				mtlList.push_back(material);

				/* Load imgbuf after push_back all material! see CAUTION in Class E3D_Material! */

				/* Reset material, !!!--- img_kd should NOT be loaded here! ---!!! */
				material.setDefaults();  /* name also cleared */

				/* Set name */
				material.name=pname;
				egi_dpstd("Material name '%s' found.\n", pname);

				/* Increase counter */
				mcnt++;
			}
		}

		/* Ka */
		if( strcmp(pt,"Ka")==0 ) {
//			printf("saveptr: %s\n", saveptr);
			sscanf(saveptr, "%f %f %f", &material.ka.x, &material.ka.y, &material.ka.z); /* rgb[0 1] */
		}
		/* KaRGB */
		else if( strcmp(pt,"KaRGB")==0 ) {
			//sscanf(saveptr, "%f %f %f", &material.ka.x, &material.ka.y, &material.ka.z);
			//material.ka.x /= 255; material.ka.y /= 255; material.ka.z /= 255;
			sscanf(saveptr, "%x", &xrgb);
			material.ka.x=(xrgb>>16)/255.0; material.ka.y=((xrgb&0xFF00)>>8)/255.0; material.ka.z=(xrgb&0xFF)/255.0;
		}
		/* map_Ka */
		else if( strcmp(pt,"map_Ka")==0 ) {
			material.map_ka=cstr_trim_space(saveptr);
			egi_dpstd("material.map_ka: %s\n", material.map_ka.c_str());
		}
		/* Kd */
		else if( strcmp(pt,"Kd")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.kd.x, &material.kd.y, &material.kd.z); /* rgb[0 1] */
		}
		/* KdRGB */
		else if( strcmp(pt,"KdRGB")==0 ) {
			//sscanf(saveptr, "%f %f %f", &material.kd.x, &material.kd.y, &material.kd.z);
			//material.kd.x /= 255; material.kd.y /= 255; material.kd.z /= 255;
			sscanf(saveptr, "%x", &xrgb);
			material.kd.x=(xrgb>>16)/255.0; material.kd.y=((xrgb&0xFF00)>>8)/255.0; material.kd.z=(xrgb&0xFF)/255.0;
		}
		/* map_Kd */
		else if( strcmp(pt,"map_Kd")==0 ) {
			material.map_kd=cstr_trim_space(saveptr);
			egi_dpstd("material.map_kd: %s\n", material.map_kd.c_str());
		}
		/* Ks */
		else if( strcmp(pt,"Ks")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.ks.x, &material.ks.y, &material.ks.z); /* rgb[0 1] */
		}
		/* KsRGB */
		else if( strcmp(pt,"KsRGB")==0 ) {
			//sscanf(saveptr, "%f %f %f", &material.ks.x, &material.ks.y, &material.ks.z);
			//material.ks.x /= 255; material.ks.y /= 255; material.ks.z /= 255;
			sscanf(saveptr, "%x", &xrgb);
			material.ks.x=(xrgb>>16)/255.0; material.ks.y=((xrgb&0xFF00)>>8)/255.0; material.ks.z=(xrgb&0xFF)/255.0;
		}
		/* map_Ks */
		else if( strcmp(pt,"map_Ks")==0 ) {
			material.map_ks=cstr_trim_space(saveptr);
			egi_dpstd("material.map_ks: %s\n", material.map_ks.c_str());
		}
		/* Ke */
		else if( strcmp(pt,"Ke")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.ke.x, &material.ke.y, &material.ke.z); /* rgb[0 1] */
		}
		/* Ns */
		else if( strcmp(pt,"Ns")==0 ) {
                        sscanf(saveptr, "%f", &material.Ns); /* Ns, normally range [0-1000] */
                }
		/* illum */
		else if( strcmp(pt,"illum")==0 ) {
			sscanf(saveptr, "%d", &material.illum); /* illum [0-10] */
		}
		/* d */
		else if( strcmp(pt,"d")==0 ) {
			sscanf(saveptr, "%f", &material.d); /* d [0 1] */
		}
		/* Ss */
		else if( strcmp(pt,"Ss")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.Ss.x, &material.Ss.y, &material.Ss.z); /* rgb[0 1] */
		}
		/* Sd */
		else if( strcmp(pt,"Sd")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.Sd.x, &material.Sd.y, &material.Sd.z); /* rgb[0 1] */
		}
		#if 0 /* Tr */
		else if( strcmp(pt,"Tr")==0 ) {
			sscanf(saveptr, "%f", &material.Tr); /* Tr [0 1] */
		}
		#endif
	}

	/* Check if last material is waiting for pushback. */
	if( !material.name.empty() ) {
		mtlList.push_back(material);

		/* Load imgbuf after push_back all material! see CAUTION in Class E3D_Material! */

		/* Increase counter */
		mcnt ++;
	}

	/* Load img_kd AFTER pushing back ALL materials! see CAUTION in Class E3D_Material! */
	for(int k=0; k<mcnt; k++) {
		/* MidasHK_2022_01_30 */
		if( mtlList[k].map_kd.empty() )
			continue;

		mtlList[k].img_kd=egi_imgbuf_readfile(mtlList[k].map_kd.c_str());
		if( mtlList[k].img_kd == NULL )
			egi_dpstd("Fail to load map_kd '%s'.\n", mtlList[k].map_kd.c_str());
		else
			egi_dpstd("Succed to load map_kd '%s'.\n", mtlList[k].map_kd.c_str());
	}

	/* Close fmtl */
	fin.close();

	egi_dpstd("Totally %d materils found in the mtl file '%s'!\n", mcnt, fmtl);
	return 0;
}

	/*--------------------------------------------
	     Class E3D_PivotKeyFrame :: Functions
	---------------------------------------------*/


	/*--------------------------------------
             Class E3D_TriMesh :: Functions
	---------------------------------------*/

/*---------------------------------------
   Constructor for E3D_TriMesh::Vertex
---------------------------------------*/
E3D_TriMesh::Vertex::Vertex()
{
	setDefaults();
};

/*-------------------------------------
   E3D_TriMesh::Vertex::setDefaults()
-------------------------------------*/
void E3D_TriMesh::Vertex::setDefaults()
{
	/* Necessary ? */
	u=0.0f;
	v=0.0f;
	E3D_TriMesh::Vertex::mark=0;

	/* E3D_Vector objs: default init as 0 */
}

/*-------------------------------------
   E3D_TriMesh::Vertex::setDefaults()
-------------------------------------*/
void E3D_TriMesh::Vertex::assign(float x, float y, float z)
{
	pt.x=x; pt.y=y; pt.z=z;
}

/*-----------------------------------------
   Constructor for E3D_TriMesh::Triangle
-----------------------------------------*/
E3D_TriMesh::Triangle::Triangle()
{
	setDefaults();
};

/*-----------------------------------------
   E3D_TriMesh::Triangle::setDefaults()
-----------------------------------------*/
void E3D_TriMesh::Triangle::setDefaults()
{
	/* Not necessary ? */
	E3D_TriMesh::Triangle::mark=0;
}

/*-----------------------------------------
   E3D_TriMesh::Triangle::setDefaults()
-----------------------------------------*/
void E3D_TriMesh::Triangle::assignVtxIndx(int v1, int v2, int v3)
{
	vtx[0].index=v1; vtx[1].index=v2; vtx[2].index=v3;
}


/*-----------------------------------------
   Constructor for E3D_TriMesh::TriGrop
-----------------------------------------*/
E3D_TriMesh::TriGroup::TriGroup()
{
	setDefaults();
};

/*-----------------------------------------
   E3D_TriMesh::TriGrop::setDefaults()
-----------------------------------------*/
void E3D_TriMesh::TriGroup::setDefaults()
{
	omat.identity();
        name.clear();
        hidden=false;
        mtlID=-1;
        tcnt=0; stidx=0; etidx=0;
	ignoreTexture=false;
	backFaceOn=false;
}

/*-----------------------------------------
   E3D_TriMesh::TriGrop::print()
-----------------------------------------*/
void E3D_TriMesh::TriGroup::print()
{
	printf("   <<< TriGroup %s >>>\n",name.c_str());
        omat.print(name.c_str());
        printf("Material ID: %d\n", mtlID);
        printf("Totally %d trianges in triList[%d - %d].\n", tcnt, stidx, etidx);
}


/*-----------------------------------------
   E3D_TriMesh::AABB::print()
-----------------------------------------*/
void E3D_TriMesh::AABB::print()
{
	egi_dpstd("AABDD: vmin{%f,%f,%f}, vmax{%f,%f,%f}.\n",
                  vmin.x,vmin.y,vmin.z, vmax.x,vmax.y,vmax.z);
}

/*--------------------------------------------
   E3D_TriMesh::AABB::xSize()/ySize()/zSize
--------------------------------------------*/
float E3D_TriMesh::AABB::xSize() const
{
	return vmax.x -vmin.x;
}
float E3D_TriMesh::AABB::ySize() const
{
	return vmax.y -vmin.y;
}
float E3D_TriMesh::AABB::zSize() const
{
	return vmax.z -vmin.z;
}


/*----------------------------------------------------
   Revise vmin/vmax to make AABB contain the point.
----------------------------------------------------*/
void E3D_TriMesh::AABB::toHoldPoint(const E3D_Vector &pt)
{
	if(pt.x<vmin.x) vmin.x=pt.x;
	if(pt.x>vmax.x) vmax.x=pt.x;
	if(pt.y<vmin.y) vmin.y=pt.y;
	if(pt.y>vmax.y) vmax.y=pt.y;
	if(pt.z<vmin.z) vmin.z=pt.z;
	if(pt.z>vmax.z) vmax.z=pt.z;
}

/*-------  TEST FUNCTIONS ---------
--------------------------------*/
void E3D_TriMesh::setVtxCount(int cnt)
{
	if(cnt>=0) vcnt=cnt;
};

void E3D_TriMesh::setTriCount(int cnt)
{
	if(cnt>=0) tcnt=cnt;
};

/*--------------------------------
   Count vertices and triangles
--------------------------------*/
int E3D_TriMesh::vtxCount() const { return vcnt; };
int E3D_TriMesh::triCount() const { return tcnt; };

/*-----------------
   Get AABB size
-----------------*/
float E3D_TriMesh::AABBxSize()
{
	return aabbox.xSize();
}
float E3D_TriMesh::AABBySize()
{
        return aabbox.ySize();
}
float E3D_TriMesh::AABBzSize()
{
        return aabbox.zSize();
}
float E3D_TriMesh::AABBdSize()
{  /* diagonal size */
        float x=aabbox.xSize();
        float y=aabbox.ySize();
        float z=aabbox.zSize();
        return sqrt(x*x+y*y+z*z);
}

/*---------------------
   Return vtxscenter
---------------------*/
E3D_Vector E3D_TriMesh::vtxsCenter(void) const
{
	return vtxscenter;
}

/*----------------------
   Return AABB center
----------------------*/
E3D_Vector E3D_TriMesh::aabbCenter(void) const
{
        return 0.5*(aabbox.vmax-aabbox.vmin);
}

/*-----------------------
   Update Vtxscenter   		TEST: ---- 2022_1_29
-----------------------*/
void E3D_TriMesh::updateVtxsCenter(float dx, float dy, float dz)
{
	vtxscenter.x +=dx;
        vtxscenter.y +=dy;
        vtxscenter.z +=dz;
}

/*------------------------------------------
   Calculate center point of all vertices
------------------------------------------*/
void E3D_TriMesh::updateVtxsCenter()
{
	float sumX=0;
        float sumY=0;
        float sumZ=0;
        for(int i=0; i<vcnt; i++) {
        	sumX += vtxList[i].pt.x;
                sumY += vtxList[i].pt.y;
                sumZ += vtxList[i].pt.z;
        }

        vtxscenter.x = sumX/vcnt;
        vtxscenter.y = sumY/vcnt;
        vtxscenter.z = sumZ/vcnt;
}

/*------------------------------------
  Move VtxsCenter to current origin.

 CROSS CHECK ---> E3D_TriMesh::moveAabbCenterToOrigin()

------------------------------------*/
void E3D_TriMesh::moveVtxsCenterToOrigin()
{
	/* 1. Update all vertex XYZ */
        for(int i=0; i<vcnt; i++) {
        	vtxList[i].pt.x -=vtxscenter.x;
                vtxList[i].pt.y -=vtxscenter.y;
                vtxList[i].pt.z -=vtxscenter.z;
        }

        /* 2. Update TriGroup omat.pmat[9-11], as pivotal of the TriGroup. */
        for(unsigned int k=0; k<triGroupList.size(); k++) {
        	triGroupList[k].omat.pmat[9] -= vtxscenter.x;
                triGroupList[k].omat.pmat[10] -= vtxscenter.y;
                triGroupList[k].omat.pmat[11] -= vtxscenter.z;

                triGroupList[k].pivot -= vtxscenter;
        }

        /* 3. Update AABB */
        aabbox.vmax.x-=vtxscenter.x;
        aabbox.vmax.y-=vtxscenter.y;
        aabbox.vmax.z-=vtxscenter.z;
        aabbox.vmin.x-=vtxscenter.x;
        aabbox.vmin.y-=vtxscenter.y;
        aabbox.vmin.z-=vtxscenter.z;

        /* 4. Reset vtxscenter */
        vtxscenter.x =0.0;
        vtxscenter.y =0.0;
        vtxscenter.z =0.0;
}

/*-------------------------
   Calculate AABB aabbox
-------------------------*/
void E3D_TriMesh::updateAABB(void)
{
	/* 1. Reset first */
        aabbox.vmin.zero();
        aabbox.vmax.zero();

        if( vcnt<1 ) return;

        /* 2. To hold all vertices */
        aabbox.vmin=vtxList[0].pt; /* Init with vtxList[0].pt */
        aabbox.vmax=vtxList[0].pt;
        for( int i=1; i<vcnt; i++ )
               aabbox.toHoldPoint(vtxList[i].pt);
}

/*--------------------------------------
   Move AABB Center to current origin.
   CROSS CHECK ---> E3D_TriMesh::moveVtxsCenterToOrigin()
--------------------------------------*/
void E3D_TriMesh::moveAabbCenterToOrigin()
{
	float xc=0.5*(aabbox.vmax.x+aabbox.vmin.x);
        float yc=0.5*(aabbox.vmax.y+aabbox.vmin.y);
        float zc=0.5*(aabbox.vmax.z+aabbox.vmin.z);

	/* 1. Update all vertex XYZ */
        for(int i=0; i<vcnt; i++) {
        	vtxList[i].pt.x -=xc;
                vtxList[i].pt.y -=yc;
                vtxList[i].pt.z -=zc;
        }

        /* 2. Move AABB */
        aabbox.vmax.x-=xc;
        aabbox.vmax.y-=yc;
        aabbox.vmax.z-=zc;

        aabbox.vmin.x-=xc;
        aabbox.vmin.y-=yc;
        aabbox.vmin.z-=zc;

        /* 3. Set objmat.pmat[9-11] as origin */
        objmat.pmat[9]=0.0;
        objmat.pmat[10]=0.0;
        objmat.pmat[11]=0.0;

        /* 4. Move TriGroup omat and pivot */
        for(unsigned int k=0; k<triGroupList.size(); k++) {
        	triGroupList[k].omat.pmat[9]  -=xc;
                triGroupList[k].omat.pmat[10] -=yc;
                triGroupList[k].omat.pmat[11] -=zc;

                triGroupList[k].pivot.x -=xc;
                triGroupList[k].pivot.y -=yc;
                triGroupList[k].pivot.z -=zc;
        }

        /* 5. Move vtxscenter */
        vtxscenter.x -=xc;
        vtxscenter.y -=yc;
        vtxscenter.z -=zc;
}


/*-------------------------------
     E3D_TriMesh  Constructor
--------------------------------*/
E3D_TriMesh::E3D_TriMesh()
{
	/* Init other params */
	initVars();

	/* Init vtxList[] */
	vcnt=0;
	try {
		vtxList= new Vertex[TRIMESH_GROW_SIZE]; //(); /* Call constructor to setDefault */
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Vertex[]!\n");
		return;
	}
	vCapacity=TRIMESH_GROW_SIZE;

	/* Init triList[] */
	tcnt=0;
	try {
		triList= new Triangle[TRIMESH_GROW_SIZE]; //();
	}
	catch ( std::bad_alloc ) {
		egi_dpstd("Fail to allocate Triangle[]!\n");
		return;
	}
	tCapacity=TRIMESH_GROW_SIZE;


#if 0	/* XXX Move to beginning, HK2022-09-16. Init other params */
	//textureImg=NULL;
	//shadeType=E3D_FLAT_SHADING;
        //backFaceOn=false;
        //bkFaceColor=WEGI_COLOR_BLACK;
	initVars();
#endif
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
        /* Init other params */
	initVars();

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

#if 0   /* XXX move to beginning, HK2022-09-16. Init other params */
        //textureImg=NULL;
        //shadeType=E3D_FLAT_SHADING;
        //backFaceOn=false;
        //bkFaceColor=WEGI_COLOR_BLACK;
	initVars();
#endif
}

/*------------------------------------------------
          E3D_TriMesh  Constructor

To create by copying from another TriMesh

Note:
1. The textureImg and shadeType keep unchanged!
   so copy trimesh just before loading texture!
------------------------------------------------*/
E3D_TriMesh::E3D_TriMesh(const E3D_TriMesh & tmesh)
{
	int nv=tmesh.vCapacity;
	int nt=tmesh.tCapacity;

	/* 0. Init params */
	initVars();

	/* 1. Init vtxList[] */
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

	/* 2. Init triList[] */
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

	/* 3. Copy vtxList */
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

	/* 4. Copy triList */
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

	/* TODO: Set texturerImg, triGroupList, mtlList... */
}


/*----------------------------------------------------
          E3D_TriMesh  Constructor

To create a trimesh by reading from a *.obj file.

@fobj:	Full path of a *.obj file.

Note:
1. Assume that indices of all vertice('v') in obj file are
   numbered in order, same of E3D_TriMesh.vtxList[].
   AND_1. All face vtxIndex use the SAME index order!
   AND_2. When we read face data, all vertice are already
   in vtxList[].
   ( We can also read in all vertice('v') data and put
     into vtxList[] first, then read all face ('f') data. )
2. All polygons are divided into triangles.
   Example: A face has MAX.4 vertices!(a quadrilateral)
   and a quadrilateral will be stored as 2
   triangles, recursive for 5,6...
	       --- CAUTION ---
3. Face(triangle) normals are not provided in obj file,
   they should be computed later.
4. If no 'g' group defined, then a default group will
   be created and push into triGroupList<>.
5. If no .mtl file provided, then the default material
   of E3D_TriMesh.defMaterial to be used.
6. Tag 'o'(as object) in *.obj is NOT supported here!

			--- OBJ File Format ---
v		vertex:  xyz
vt		vertex texture coordinate: uv[w]
vn		vertex normal: xyz
f x/x/x         face: vtxIndex/uvIndex/normalIndex, index starts from 1, NOT 0! (uvIndex and normalIndex MAYBE omitted)
		f v1[/vt1][/vn1] v2[/vt2][/vn2] v3[/vt3][/vn3] ...
o		object name
s		smoothing group: on, off
g		su_object group
		  !!! --- MUST have 'g xxx' defined before usemtl and faces. --- !!!
		If NOT explicitly defined, an object name will NOT be used as su_object group name
		which may be omitted in some cases.
x		TriGroup pivot xyz
r		TriGroup rotation XYZ in degree.   r X45.8 Z33 Y43.5
t    		TriGroup translation XYZ. t x100.0 y0.0 z56.7
bf ON		TriGroup backFaceOn.

mtllib xxx.mtl  Use material file xxx.mtl to load materials.
usemtl yyy 	Use material yyy for the followed elements.

TODO:
1. tv: for u/v <0?
2. NOW only supports ONE .mtl file.
3. NOW only support 'g'(as group). 'o'(as object) NOT support.
   Comment 'o' OR change 'o' to 'g' in .obj file.

@fobj:	File path.
-----------------------------------------------------*/
E3D_TriMesh::E3D_TriMesh(const char *fobj)
{
	ifstream	fin;
	char		ch;

        char *savept;
        char *savept2;
        char *pt, *pt2;
        char strline[1024];
        char strline2[1024];

	int	j,k;		    /* triangle index in a face, [0  MAX.3] */
        int     m;                  /* 0: vtxIndex, 1: texutreIndex 2: normalIndex  */
	float	xx,yy,zz;
	int	vtxIndex[64]={0};    	/* To Buffer each input line:  store vtxIndex for MAX. 4x16_side/vertices face */
	int	textureIndex[64]={0};
	int	normalIndex[64]={0};
		normalIndex[0]=-1;	/* For token */

	int	vertexCount=0;
	int	triangleCount=0;
	int	vtxNormalCount=0;
	int	textureVtxCount=0;
	int	faceCount=0;

	/* Temp. use, to free them before return. */
	E3D_Vector *tuvList=NULL;  		/* Texture vertices uv list */
	int	    tuvListCnt=0;
	E3D_Vector *vnList=NULL;		/* Vertex normal List */
	int	    vnListCnt=0;

	/* TODO: NOW object 'o' and group 'g' all treated as triGroup. any they CAN NOT appear at same time! */
	TriGroup    triGroup;
	int	    groupCnt=0;	/* Triangles group counter */
	int	    groupStidx=0;	/* Group start index as of triList[sidx] */
	int	    groupEtidx=0;	/* Group end index as of triList[eidx-1] */

	/* For omat rotation */
        E3D_Vector axisX(1,0,0);
        E3D_Vector axisY(0,1,0);
	E3D_Vector axisZ(0,0,1);
	E3D_RTMatrix VRTmat;
	float rang;
	float trans;

	/* 0. Init params */
	vcnt=0;
	tcnt=0;
	initVars();
        //textureImg=NULL;
        //shadeType=E3D_FLAT_SHADING;
	//backFaceOn=false;
	//bkFaceColor=WEGI_COLOR_BLACK;

	/* 1. Read statistic of obj data */
	egi_dpstd("Read obj file info for '%s'...\n", fobj);
	if( readObjFileInfo(fobj, vertexCount, triangleCount, vtxNormalCount, textureVtxCount, faceCount) !=0 ) {
		egi_dpstd(DBG_RED"Fail to readObjFileInfo!\n"DBG_RESET);
		return;
	}

	egi_dpstd("3D Object '%s' Statistics:  Vertices %d;  vtxNormals %d;  TextureVtx %d; Faces %d; Triangles %d.\n",
				fobj, vertexCount, vtxNormalCount, textureVtxCount, faceCount, triangleCount );

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

	/* 2.3 New uvList */
	tuvList = new E3D_Vector[textureVtxCount];

	/* 2.4 New vnList */
	vnList = new E3D_Vector[vtxNormalCount];

	/* 3. Open obj file */
	fin.open(fobj);
	if(fin.fail()) {
		egi_dperr("Fail to open '%s'.\n", fobj);
		delete [] tuvList;
		delete [] vnList;
		return;
	}

	/* 4. Read data into TriMesh */
	strline[1024-1]=0;
	while( fin.getline(strline, 1024-1) ) {
		ch=strline[0];

#if 0 /* TEST: ----------- */
		egi_dpstd("strline: %s\n", strline);
#endif

		/* Replace all mid '#' with '\0' */
		for(int n=1; n<(int)strlen(strline); n++)
			if(strline[n]=='#') strline[n]=0;

		/* Parse line */
		switch(ch) {
		   case '#':	/* 0. Comments */
			break;
		   case 'm':    /* Read material file */
			if( strcmp(strline,"mtllib ")>0 ) {
			   /* Input mtList will cleared firt in readMtlFile() */
			   readMtlFile( cstr_trim_space(strline+strlen("mtllib ")), mtlList);
			}
			break;
		   case 'o':   /* TODO:Start a new object. */
			/* TODO:  If 'o' and 'g' appears at same time, it faults then!
				...
				o head
				g eye
				usemtl  material_eye
				f 3/1 1/2 2/3
				f 1/2 3/1 4/4
				...
			*/
			break;
		   case 'g':   /* Start a new group */
#if 0 /* Test:  To print current vtx count;  */
			egi_dpstd("Current vcnt=%d when start group '%s'\n", vcnt, cstr_trim_space(strline+1));
#endif
			/* g.1 Assign LAST groupEtidx, the last_group.groupEtidx to be assiged at see E2. */
			if(groupCnt>0) {
				triGroupList.back().etidx=groupEtidx;
				triGroupList.back().tcnt = groupEtidx-groupStidx; /* last not included. */

				/* Reset groupStidex for next group. */
				groupStidx = groupEtidx;
			}

			/* g.2 Assign triGroup */
			triGroup.name = cstr_trim_space(strline+1);
			triGroup.stidx = groupStidx;    /* Assign stdix ONLY! */
			//triGroup.etidx = groupEtidx;  /* To assign when next group starts, while groupEtidx finishing counting! */

			//triGroup.mtlID = materialID; /* SEE case 'u' */
			triGroup.mtlID = -1;  /* <0, Preset as non_material! */

			/* g.3 Push into triGroupList */
			triGroupList.push_back(triGroup);
			groupCnt++;

			/* g.4 Clear triGroup for next group */
			triGroup.setDefaults();
			break;
		   case 'b':
			/* bf ON : Set backFaceOn */
			if( strstr(strline, "bf ON") ) {
			    if(groupCnt<1) break;
			    else   triGroupList.back().backFaceOn=true;
			}
			break;
		   case 'x':   /*  TriGroup origin/center xyz */
			if(groupCnt<1)break;
			sscanf( strline+2, "%f %f %f", &triGroupList.back().pivot.x,
						       &triGroupList.back().pivot.y,
						       &triGroupList.back().pivot.z );

			/* Init omat.pmat[9-11] same as pivot, as Translation TxTyTz from the Object ORIGIN */
			triGroupList.back().omat.pmat[9]=triGroupList.back().pivot.x;
			triGroupList.back().omat.pmat[10]=triGroupList.back().pivot.y;
			triGroupList.back().omat.pmat[11]=triGroupList.back().pivot.z;
			break;
		   case 'r':    /* TRiGroup rotation XYZ */
			if(groupCnt<1)break;

			VRTmat.identity();
			/* Noticed that omat.[9-11] is pivot.xyz, and should NOT be affected. TO restore later. */
			triGroupList.back().omat.pmat[9]=0.0f;
			triGroupList.back().omat.pmat[10]=0.0f;
			triGroupList.back().omat.pmat[11]=0.0f;

			/* r Z90.0 X45.0 Y43.0 */
                        pt=strtok(strline+2, " ");
                        for(k=0; pt!=NULL && k<3; k++) {
			    /* Parse XYZ rotation */
			    switch(pt[0]) {
				case 'x': case 'X':
					printf(" --- omat RotX  ----\n");
					rang=atof(pt+1);
					VRTmat.setRotation(axisX, 1.0*rang/180*MATH_PI);
					triGroupList.back().omat = triGroupList.back().omat*VRTmat;
					break;
				case 'y': case 'Y':
					printf(" --- omat RotY  ----\n");
					rang=atof(pt+1);
                                        VRTmat.setRotation(axisY, 1.0*rang/180*MATH_PI);
                                        triGroupList.back().omat = triGroupList.back().omat*VRTmat;
					break;
				case 'z': case 'Z':
					printf(" --- omat RotZ  ----\n");
					rang=atof(pt+1);
                                        VRTmat.setRotation(axisZ, 1.0*rang/180*MATH_PI);
                                        triGroupList.back().omat = triGroupList.back().omat*VRTmat;
					break;
			    }

			    /* Next */
                            pt=strtok(NULL, " ");
                        }

			/* Noticed that omat.[9-11] is pivot.xyz, need to restore it. */
			triGroupList.back().omat.pmat[9]=triGroupList.back().pivot.x;
			triGroupList.back().omat.pmat[10]=triGroupList.back().pivot.y;
			triGroupList.back().omat.pmat[11]=triGroupList.back().pivot.z;
			break;
		   case 't':    /* TRiGroup translation XYZ */
			if(groupCnt<1)break;

			/* t (x/y/z)100 (x/y/z)300 (x/y/z)500 */
                        pt=strtok(strline+2, " ");
                        for(k=0; pt!=NULL && k<3; k++) {
			    /* Parse XYZ translation.  */
			    switch(pt[0]) {
				case 'x': case 'X':
					printf(" --- omat TransX  ----\n");
					trans=atof(pt+1);
					triGroupList.back().omat.addTranslation(trans, 0.0f, 0.0f); /* dx,dy,dz */
					break;
				case 'y': case 'Y':
					printf(" --- omat TransY  ----\n");
					trans=atof(pt+1);
					triGroupList.back().omat.addTranslation(0.0f, trans, 0.0f); /* dx,dy,dz */
					break;
				case 'z': case 'Z':
					printf(" --- omat TransZ  ----\n");
					trans=atof(pt+1);
					triGroupList.back().omat.addTranslation(0.0f, 0.0f, trans); /* dx,dy,dz */
					break;
			    }

			    /* Next */
                            pt=strtok(NULL, " ");
                        }
			break;
		   case 'u':    /* Group material name */
			/* Note: 'u' MUST follow a 'g' token(or 'o' modified to be 'g' ),  OR getMaterialID() wil fail! */
			if(groupCnt<1)break; /* 'usemtl' SHALL aft 'g' */

			/* Material name for current group */
			if( strcmp(strline,"usemtl ")>0 ) {
			   /* Get MaterialID, current group already in triGropList[] */
			   triGroupList[groupCnt-1].mtlID = getMaterialID(cstr_trim_space(strline+strlen("usemtl ")));
			}
			break;
		   case 'v':	/* 1. Vertex data */
			ch=strline[1]; /* get second token */

			switch(ch) {
			   case ' ':  /* Read vertices X.Y.Z. */
				if(vcnt==vCapacity) {
					egi_dpstd("tCapacity used up!\n");
					goto END_FUNC;
				}

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
				if(tuvListCnt>=textureVtxCount) {
					egi_dpstd("Error, tuvListCnt > textureVtxCount!\n");
					goto END_FUNC;
				}
				sscanf( strline+2, "%f %f %f",
					&tuvList[tuvListCnt].x, &tuvList[tuvListCnt].y, &tuvList[tuvListCnt].z);

				/* TODO: U,V <0 means....?? */
				if(tuvList[tuvListCnt].x<0) tuvList[tuvListCnt].x=1.0+tuvList[tuvListCnt].x;
				if(tuvList[tuvListCnt].y<0) tuvList[tuvListCnt].y=1.0+tuvList[tuvListCnt].y;
				if(tuvList[tuvListCnt].z<0) tuvList[tuvListCnt].z=1.0+tuvList[tuvListCnt].z;

				tuvListCnt++;
				break;
			   case 'n':  /* Read vertex normals, (for each vertex. ) */
				if(vnListCnt>=vtxNormalCount) {
					egi_dpstd("Error, tuvListCnt > vtxNormalCount!\n");
					goto END_FUNC;
				}
				sscanf( strline+2, "%f %f %f",
					&vnList[vnListCnt].x, &vnList[vnListCnt].y, &vnList[vnListCnt].z);
				/* Normalize it */
				vnList[vnListCnt].normalize();

				vnListCnt++;
				break;
			   case 'p': /* Parameter space vertices */

				break;

			   default:
				break;
			}

			break;

		   case 'f':	/* 2. Face data. vtxIndex starts from 1!!! */
			/* Change first char 'f' to ' ' */
			strline[0] = ' ';

			/* Copy for test */
			strline[1024-1]=0;
			strline2[1024-1]=0;
			strncpy(strline2, strline, 1024-1);
			//cout<<"Face line: " <<strline2 <<endl;

		   	/* FA Case A. --- ONLY vtxIndex data
		    	 * Example: f 61 21 44
		    	 */
			if( strstr(strline, "/")==NULL ) {
			   /* FA.1 To extract face vertex indices */
			   pt=strtok(strline+1, " ");
			   for(k=0; pt!=NULL && k<64; k++) {
				vtxIndex[k]=atoi(pt) -1;
				pt=strtok(NULL, " ");
			   }

			   /* NOW: k is total number of vertices in this face.<--- */

			   /* TODO:  textrueIndex[] and normalIndex[], value <0 as omitted.  */

			   /*  FA.2  Assign to triangle vtxindex: triList[tcnt].vtx[i].index */
			   if( k>2 ) {
				/* To divide the face into triangles in way of a fan. */
				for( j=0; j<k-2; j++ ) {
					if(tcnt==tCapacity) {
						egi_dpstd("tCapacity used up!\n");
						goto END_FUNC;
					}

					triList[tcnt].vtx[0].index=vtxIndex[0];
	                                triList[tcnt].vtx[1].index=vtxIndex[j+1];
        	                        triList[tcnt].vtx[2].index=vtxIndex[j+2];
					tcnt++;

					/* increase group etidx */
					groupEtidx++;
				}
			   }

			   #if 0 /* TEST:----- Print all vtxIndex[] */
			   egi_dpstd("vtxIndex[]:");
			   for(int i=0; i<k; i++)
				printf(" %d", vtxIndex[i]);
			   printf("\n");
			   #endif

		   	} /* End Case A. */

		   	/* FB Case B. --- NOT only vtxIndex, maybe ALSO with textureIndex,normalIndex, with delim '/',
		         * Example: f 69//47 68//47 43//47 52//47
		         */
		        else {    /* ELSE: strstr(strline, "/")!=NULL */

			   /* FB1. To replace "//" with "/0/", to avoid strtok() returns only once!
			    * However, it can NOT rule out conditions like "/0/5" and "5/0/",
			    * see following  "In case "/0/5" ..."
			    */
			   while( (pt=strstr(strline, "//")) ) {
//				egi_dpstd("!!!WARNING!!! textureVtxIndex omitted!\n");
				int len;
				len=strlen(pt+1)+1; /* len: from second '/' to EOF */
				memmove(pt+2, pt+1, len);
				*(pt+1) ='0'; /* insert '0' btween "//" */
			   }

			   /* FB2. To extract face vertex indices */
			   pt=strtok_r(strline+1, " ", &savept); /* +1 skip first ch */
			   for(k=0; pt!=NULL && k<6*4; k++) {
				//cout << "Index group: " << pt<<endl;
				/* FB2.1 If no more "/"... example: '2/3/4 5/6/4 34 ', cast last '34 '
				 * This must befor strtok_r(pt, ...)
				 */
				if( strstr(pt, "/")==NULL) {
					//printf("No '/' when k=%d\n", k);
					break;
				}

				/* FB2.2 Parse vtxIndex/textureIndex/normalIndex */
				pt2=strtok_r(pt, "/", &savept2);

				/* FB2.3 Read vtxIndex/textureIndex/normalIndex; as separated by '/' */
				for(m=0; pt2!=NULL && m<3; m++) {
					//cout <<"pt2: "<<pt2<<endl;
					switch(m) {
					   case 0:
						vtxIndex[k]=atoi(pt2)-1;  /* In obj, it starts from 1, NOT 0! */
						//if( vtxIndex[k]<0 )
						//egi_dpstd("!!!WARNING!!!\nLine '%s' parsed as vtxIdex[%d]=%d <0!\n",
						//					strline2, k, vtxIndex[k]);
						break;
					   case 1:
						textureIndex[k]=atoi(pt2)-1;
						break;
					   case 2:
						normalIndex[k]=atoi(pt2)-1;
						break;
					}


					/* Next param */
					pt2=strtok_r(NULL, "/", &savept2);
				}

				/* FB2.4 In case  "/0/5", -- Invalid, vtxIndex MUST NOT be omitted! */
				if(pt[0]=='/' ) {
					egi_dpstd(DBG_RED"!!! WARNING !!! vtxIndex omitted, data may be ERROR!\n"DBG_RESET);
					/* Need to reorder data from to vtxIndex/textureIndex(WRONG!, it's textureIndex/normalIndex)
					   vtxIndex/textureIndex/normalIndex(CORRECT!) */
					normalIndex[k]=textureIndex[k];
					textureIndex[k]=vtxIndex[k];
					vtxIndex[k]=-1;		/* vtxIndex omitted!!! */
				}
				/* FB2.5 In case "5/0/", -- m==2, normalIndex omitted!  */
				else if( m==2 ) { /* pt[0]=='/' also has m==2! */
					egi_dpstd(DBG_RED"WARNING:  vtxNormalIndex omitted!\n"DBG_RESET);
					normalIndex[k]=0-1;
				}

				/* FB2.6 Get next indices */
				pt=strtok_r(NULL, " ", &savept);

			   } /* End for(k) */

			   /* NOW: k is total number of vertices in this face. */

#if 0 /* Test: plane.obj ------ */
			      //z>400.0 tailcone  // z<-139.1 nose
			   //if( k>0 && groupCnt==6 && vtxList[ vtxIndex[0] ].pt.z>390.0 && vtxList[ vtxIndex[0] ].pt.y<293.0 ) { // rudder
			   //if( k>0 && groupCnt==5 ) {
			 	 //if(   vtxList[ vtxIndex[0] ].pt.x>158.5 && vtxList[ vtxIndex[0] ].pt.x<230.0
				 //   && vtxList[ vtxIndex[0] ].pt.z>379.0   ) // right elevator
			  //if( k>0 && groupCnt==5 ) {
			  	// if(   vtxList[ vtxIndex[0] ].pt.x>6.0 && vtxList[ vtxIndex[0] ].pt.x<77.0
			  	//    && vtxList[ vtxIndex[0] ].pt.z>379.0   ) // left elevator
			  //if( k>0 && groupCnt==4 ) {
			  	// if(   vtxList[ vtxIndex[0] ].pt.x>232.7 && vtxList[ vtxIndex[0] ].pt.x<380.3
			  	//    && vtxList[ vtxIndex[0] ].pt.z>125   ) // right wing flap
			  //if( k>0 && groupCnt==4 ) {
			  //	 if(   vtxList[ vtxIndex[0] ].pt.x> -145.0 && vtxList[ vtxIndex[0] ].pt.x < 3.0
			  //	    && vtxList[ vtxIndex[0] ].pt.z>125   ) // left wing flap
			  //	printf("Face line:  %s\n",strline2);
			  // }
			  if( k>0 && groupCnt!=7 && groupCnt!=8 && groupCnt<11 ) { //&& groupCnt==4 ) {
				 //float fk=(42.8-1.38)/(395.2-241.58);  // right wing slat
				 float fk=(1.385-53.45)/(-5.55-(-194.26));   //left wing slat
			     int nn;
			     for(nn=0; nn<k; nn++) {
			  	 //if(   vtxList[ vtxIndex[nn] ].pt.x> 240.0 && vtxList[ vtxIndex[nn] ].pt.x < 458.0
			  	 //   && vtxList[ vtxIndex[nn] ].pt.z < fk*(vtxList[ vtxIndex[nn] ].pt.x-241.58)+1.385+2.0 )  // right wing slat
			  	 if(   vtxList[ vtxIndex[nn] ].pt.x> -195.0 && vtxList[ vtxIndex[nn] ].pt.x < -5.0
			  	    && vtxList[ vtxIndex[nn] ].pt.z < fk*(vtxList[ vtxIndex[nn] ].pt.x-(-194.26))+53.45 +2.0 )  // left wing slat
					continue;
				 else
					break;
			     }
			     if(nn==k) /* All vtx within the range */
				printf("Face line:  %s\n",strline2);
			   }
#endif

			   /*  FB3. Assign to triangle vtxindex */
			   if( k>2 ) {  /* k is total number of vertices of the face */
				/* FB3.1 To divide the face into triangles in way of folding a fan. */
				for( j=0; j<k-2; j++ ) {
					/* FB3.1.1 check tCapacity */
					if(tcnt==tCapacity) {
						egi_dpstd("tCapacity used up!\n");
						goto END_FUNC;
					}

					/* FB3.1.2 Read trianlge vtxIndex/vtxTextureIndex/vtxNormal */
					if( vtxIndex[0]>-1 && vtxIndex[j+1]>-1 && vtxIndex[j+1]>-1 )
					{
					    /* FB3.1.2.1  Trianlge vtx index */
					    triList[tcnt].vtx[0].index=vtxIndex[0]; /* Fix first vertex! */
		                            triList[tcnt].vtx[1].index=vtxIndex[j+1];
        		                    triList[tcnt].vtx[2].index=vtxIndex[j+2];

					   /* All faces divided into triangles in way of folding a fan. */

					   /* FB3.1.2.2 Triangle vtx texture coord uv. */
					   if(textureIndex[0]>-1 && textureIndex[j+1]>-1 && textureIndex[j+2]>-1) {
						triList[tcnt].vtx[0].u=tuvList[textureIndex[0]].x;
						triList[tcnt].vtx[0].v=tuvList[textureIndex[0]].y;
						triList[tcnt].vtx[1].u=tuvList[textureIndex[j+1]].x;
						triList[tcnt].vtx[1].v=tuvList[textureIndex[j+1]].y;
						triList[tcnt].vtx[2].u=tuvList[textureIndex[j+2]].x;
						triList[tcnt].vtx[2].v=tuvList[textureIndex[j+2]].y;
					   }

					   /* FB3.1.2.3 Triangle vertex normal index. */
					   /* Case 1: vtxNormalIndex omitted!!
					    *         then each vertex has the same normal value! */
					   if( normalIndex[0]<0 ) {
						//// To deal this case at last, just after while() loop.
					   }
					   /* Case 2: Each vertex may have more than one normal value, one for one triangle as it belongs to. */
					   else if( normalIndex[0]>-1 && normalIndex[j+1]>-1 && normalIndex[j+2]>-1) {
						triList[tcnt].vtx[0].vn=vnList[normalIndex[0]];
						triList[tcnt].vtx[1].vn=vnList[normalIndex[j+1]];
						triList[tcnt].vtx[2].vn=vnList[normalIndex[j+2]];
					  }

					}
					tcnt++;

					/* increase group etidx */
					groupEtidx++;
				}
			   }

    /* TEST: ------ */
			  if( vtxIndex[0]<0 || vtxIndex[1]<0 ||vtxIndex[2]<0 || (k==4 && vtxIndex[3]<0 ) ) {

				cout<<"Face line: "<<strline2<<endl;
				printf(DBG_RED"%s: k=%d has parsed vtxIndex <0! vtxIndex[0,1,2,3]: %d, %d, %d, %d \n"DBG_RESET,
				 	strline2, k, vtxIndex[0],vtxIndex[1],vtxIndex[2],vtxIndex[3] );
				//printf("!!!WARNING!!!\nLine '%s', has vtxIndex <0!\n", strline2);
				exit(0);
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

	/* E1.  Assign vtxList[].nromal, if vtxNormalIndex omitted!
	 * See FB3.1.2.3, If vtxNormalIndex omitted! then each vertex has the same normal value!
	 */
	if( normalIndex[0]<0 ) {  /* Init normalIndex[0]=-1 for token */
	   egi_dpstd(DBG_YELLOW"vtxNormalIndex omitted! each vertex has same normal value as vtxList[].normal!\n"DBG_RESET);
	   for( int k=0; k<vertexCount; k++) {
		 if(k>=vnListCnt)
			break;
		 vtxList[k].normal=vnList[k];
	  }
        }
	else
	   egi_dpstd(DBG_YELLOW"vtxNormalIndex assigned! each vertex may have more than one normal values, as in triList[].vtx[].vn!\n"DBG_RESET);

	/* E2.  Assign groupEtidx for the last group. just BEFORE E3. */
	if( groupCnt>0 ) {
		triGroupList.back().etidx=groupEtidx;
		triGroupList.back().tcnt = groupEtidx-groupStidx; /* last not included. */
	}

	/* E3.  If no group defined in .obj file , then add a default triGroup to include all triangles. */
	if( groupCnt<1 ) {
		/* Assign triGroup */
		triGroup.name = "Default";
		triGroup.stidx = 0;		/* start index of triList[] */
		triGroup.etidx = tcnt;
		triGroup.tcnt = tcnt;

		/* If mtList is NOT empty, then use the first mtlID */
		if(mtlList.size()>0)
			triGroup.mtlID=0;
		/* ELSE: Set <0, so as to use defualt materila: Trimesh.defMaterial */
		else
			triGroup.mtlID=-1;

		/* Push into triGroupList */
		triGroupList.push_back(triGroup);

		groupCnt ++;
	}

END_FUNC:
	/* Close file */
	fin.close();

	egi_dpstd("Finish reading obj file: %d Vertices, %d Triangels. %d TextureVertices. %zu TriGroups, %zu Materials.\n", /* MidasHK__ */
		  								vcnt, tcnt, tuvListCnt, triGroupList.size(), mtlList.size());

#if 0	/* TEST: ---------print all textureVtx tuvList[] */
	for(int k=0; k<tuvListCnt; k++)
		printf("tuvList[%d]: %f,%f,%f\n", k, tuvList[k].x, tuvList[k].y, tuvList[k].z);
#endif

#if 0	/* TEST: ---------print all vtxNormal vnList[] */
	for(int k=0; k<vnListCnt; k++)
		printf("vnList[%d]: %f,%f,%f\n", k, vnList[k].x, vnList[k].y, vnList[k].z);
#endif

#if 0	/* TEST: ---------print all triGroupList[] */
	egi_dpstd("Totally %d triangle groups defined: \n", triGroupList.size());
	for(unsigned int k=0; k<triGroupList.size(); k++) {

		triGroupList[k].print();

		#if 0
		printf(" triGroupList[k].mtlID=%d \n", triGroupList[k].mtlID );
		printf("  %d.  '%s'  materialID=%d  triList[%d %d)  Tris=%d  map_kd:'%s'\n",
		             k, triGroupList[k].name.c_str(), triGroupList[k].mtlID,
				triGroupList[k].stidx, triGroupList[k].etidx,
				triGroupList[k].etidx - triGroupList[k].stidx,
				triGroupList[k].mtlID >=0 && !(mtlList[triGroupList[k].mtlID].map_kd.empty())
						? mtlList[triGroupList[k].mtlID].map_kd.c_str() : "NULL");
		#endif
	}
#endif


	/* Delete */
	delete [] tuvList;
	delete [] vnList;
}


/*------------------------
	Destructor
-------------------------*/
E3D_TriMesh:: ~E3D_TriMesh()
{
//	if(vtxList!=NULL)
//		egi_dpstd(DBG_YELLOW"vtxList != NULL!\n"DBG_RESET);

	delete [] vtxList;
	delete [] triList;

	egi_imgbuf_free2(&textureImg);

	egi_dpstd("TriMesh destructed!\n");
}

/*--------------------------------
Initialize calss variable members.
--------------------------------*/
void E3D_TriMesh:: initVars()
{
/* Init private members: see in E3D_TriMesh() constructors... */
	vtxList=NULL;  /* Must init. to NULL, OR segmenation fault in case E3D_TriMesh() fails! HK2022-09-15 */
	triList=NULL;

/* Init public members */
	objmat.identity();
        textureImg=NULL;
        shadeType=E3D_FLAT_SHADING;
        backFaceOn=false;
        bkFaceColor=WEGI_COLOR_BLACK;
	wireFrameColor=WEGI_COLOR_BLACK;
	faceNormalLen=0;
	testRayTracing=false;
}

/*---------------------
Increase vCapacity

Return:
	0	OK
	<0      Fails
----------------------*/
int E3D_TriMesh::moreVtxListCapacity(size_t more)
{
        Vertex *vtxNewList=NULL;

        /* Allocate vtxNewList[] */
        try {
                vtxNewList= new Vertex[vCapacity+more];  /* vertices */
        }
        catch ( std::bad_alloc ) {
                egi_dpstd("Fail to allocate Vertex[]!\n");
                return -1;
        }
        vCapacity += more;

        /* Copy content from old arrays */
        for(int k=0; k<vcnt; k++)
                vtxNewList[k]=vtxList[k];

        /* Delete old arrays */
	if(vtxList!=NULL)
	        delete [] vtxList;

	/* Assign to vtxList */
	vtxList=vtxNewList;

	return 0;
}

/*--------------------
Increase tCapacity

Return:
	0	OK
	<0      Fails
--------------------*/
int E3D_TriMesh::moreTriListCapacity(size_t more)
{
        Triangle *triNewList=NULL;

        /* Allocate triNewList[]  */
        try {
                triNewList=new Triangle[tCapacity+more]; /* triangles */
        }
        catch ( std::bad_alloc ) {
                egi_dpstd("Fail to allocate Triangle[]!\n");
                return -1;
        }
        tCapacity += more;

        /* Copy content from old arrays */
        for(int k=0; k<tcnt; k++)
                triNewList[k]=triList[k];

        /* Delete old arrays */
	if(triList!=NULL)
	        delete [] triList;

	/* Assign to triList */
	triList=triNewList;

	return 0;
}

/*---------------------
Print out all Vertices.
---------------------*/
void E3D_TriMesh::printAllVertices(const char *name=NULL)
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
void E3D_TriMesh::printAllTriangles(const char *name=NULL)
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

/*---------------------------------
Print out all texture vertices(UV).
----------------------------------*/
void E3D_TriMesh::printAllTextureVtx(const char *name=NULL)
{
	printf("\n   <<< %s Texture Vertices List >>>\n", name);
	if(tcnt<=0) {
		printf("No triangle in the TriMesh!\n");
		return;
	}

	for(int i=0; i<tcnt; i++) {
		/* Each triangle has 3 vertex index, each has texture vertices as [u,v] //,w] */
		printf("[%d]:  [0]{%f, %f}, [1]{%f, %f}, [2]{%f, %f}\n",
			i, triList[i].vtx[0].u, triList[i].vtx[0].v,
			   triList[i].vtx[1].u, triList[i].vtx[1].v,
			   triList[i].vtx[2].u, triList[i].vtx[2].v  );
	}
}

/*------------------------
Print out AABB.
------------------------*/
void E3D_TriMesh::printAABB(const char *name=NULL)
{
	printf("\n   <<< %s AABB >>>\n", name);
	printf(" vmin{%f, %f, %f}\n vmax{%f, %f, %f}\n",
			aabbox.vmin.x, aabbox.vmin.y, aabbox.vmin.z,
			aabbox.vmax.x, aabbox.vmax.y, aabbox.vmax.z );
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

/*----------------------------------------
Reverse Z value/direction for all vertices,
for Coord system conversion.
-----------------------------------------*/
void E3D_TriMesh::reverseAllVtxZ(void)
{
	egi_dpstd("reverse all vertex Z values...\n");
	for(int i=0; i<vcnt; i++)
		vtxList[i].pt.z = -vtxList[i].pt.z;

	for(unsigned int j=0; j<triGroupList.size(); j++) {
		triGroupList[j].pivot.z = -triGroupList[j].pivot.z;
		triGroupList[j].omat.pmat[11] = -triGroupList[j].omat.pmat[11];
	}

	/* TODO: objmat */
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


/*-------------------------------------------------------------------
Transform all vertex normals. NO translation component!
(Without Triangle VtxNormals triList[].vtx[].vn! )

Note:
1. Scale MUST not be applied!!!
2. Without Triangle VtxNormals triList[].vtx[].vn!

@RTMatrix:	RotationTranslation Matrix
---------------------------------------------------------------------*/
void E3D_TriMesh::transformAllVtxNormals(const E3D_RTMatrix  &RTMatrix)
{
	float xx;
	float yy;
	float zz;

	/* Transform Vertex normals */
	if( vtxList[0].normal.isZero()==false )
	{
	   egi_dpstd("Transform all vertex normals...\n");
	   for(int i=0; i<vcnt; i++) {
		xx=vtxList[i].normal.x;
		yy=vtxList[i].normal.y;
		zz=vtxList[i].normal.z;

		vtxList[i].normal.x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6]; /* Ignor translation!!! */
		vtxList[i].normal.y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7];
		vtxList[i].normal.z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8];
	   }
	}
}


/*--------------------------------------------------------------
Transform all triangle face/vtx normals. NO translation component!
( Without vertex normals vtxList[].normal! )

Note:
1. Scale MUST not be applied!
2. Without vertex normals vtxList[].normal!

@RTMatrix:	RotationTranslation Matrix
----------------------------------------------------------------*/
void E3D_TriMesh::transformAllTriNormals(const E3D_RTMatrix  &RTMatrix)
{
	float xx;
	float yy;
	float zz;

	/* Transform trianlge face normal */
	egi_dpstd("Transform all triangle face normals...\n");
	for( int j=0; j<tcnt; j++) {
		xx=triList[j].normal.x;
		yy=triList[j].normal.y;
		zz=triList[j].normal.z;
		triList[j].normal.x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6]; /* Ignor translation!!! */
		triList[j].normal.y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7];
		triList[j].normal.z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8];
	}

   	/* Transform triangle vertex normals as in triList[].vtx.vn */
	if( !triList[0].vtx[0].vn.isZero() ) {
	   egi_dpstd("Transform all triangle vtx normals...\n");
	   for( int j=0; j<tcnt; j++) {
	   	for(int k=0; k<3; k++) {
			xx=triList[j].vtx[k].vn.x;
			yy=triList[j].vtx[k].vn.y;
			zz=triList[j].vtx[k].vn.z;

			triList[j].vtx[k].vn.x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6]; /* Ignor translation!!! */
			triList[j].vtx[k].vn.y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7];
			triList[j].vtx[k].vn.z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8];
		}
	   }
	}

	//egi_dpstd("vtxList[0].normal: {%f, %f, %f}\n",vtxList[0].normal.x, vtxList[0].normal.y, vtxList[0].normal.z);
}

/*-----------------------------------------------------------
Transform AABB (axially aligned bounding box).
	  		!!! CAUTION !!!
1. The function ONLY transform the ORIGINAL two Vectors(vmin/vmax)
   that  defined the AABB.

@RTMatrix:	RotationTranslation Matrix
-----------------------------------------------------------*/
void E3D_TriMesh::transformAABB(const E3D_RTMatrix  &RTMatrix)
{
#if 0
	float xx;
	float yy;
	float zz;

	xx=aabbox.vmin.x;
	yy=aabbox.vmin.y;
	zz=aabbox.vmin.z;
	aabbox.vmin.x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6] +RTMatrix.pmat[9];
	aabbox.vmin.y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7] +RTMatrix.pmat[10];
	aabbox.vmin.z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8] +RTMatrix.pmat[11];

	xx=aabbox.vmax.x;
	yy=aabbox.vmax.y;
	zz=aabbox.vmax.z;
	aabbox.vmax.x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6] +RTMatrix.pmat[9];
	aabbox.vmax.y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7] +RTMatrix.pmat[10];
	aabbox.vmax.z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8] +RTMatrix.pmat[11];
#else

	aabbox.vmin =aabbox.vmin*RTMatrix;
	aabbox.vmax =aabbox.vmax*RTMatrix;
#endif
}

/*-------------- TODO: To be obsolete, apply TriMesh.objmat instead ------------
Transform all vertices, all triangle normals and AABB

@RTMatrix:	RotationTranslation Matrix
@ScaleMatrix:	ScaleMatrix
-------------------------------------------------------------------------------*/
void E3D_TriMesh::transformMesh(const E3D_RTMatrix  &RTMatrix, const E3D_RTMatrix &ScaleMatrix)
{
	E3D_RTMatrix  cmatrix=RTMatrix*ScaleMatrix;

	/* Transform all vertices */
	transformVertices(cmatrix);

	/* Transform TriGroup omat */
 	E3D_Vector  tgOxyz;
	for(unsigned int i=0; i<triGroupList.size(); i++)   {
		/* !!! ONLY transform TriGroup::omat.pmat[9-11](tx,ty,tz) as TriGroiup local origin.
	 	 *  tgRotMat is RELATIVE to its superior node,wh, NO NEED.
		 */
		tgOxyz.assign( triGroupList[i].omat.pmat[9],
				triGroupList[i].omat.pmat[10],
				 triGroupList[i].omat.pmat[11] );

		tgOxyz = tgOxyz * cmatrix;

		triGroupList[i].omat.pmat[9] = tgOxyz.x;
		triGroupList[i].omat.pmat[10] = tgOxyz.y;
		triGroupList[i].omat.pmat[11] = tgOxyz.z;

		/* pivot */
		triGroupList[i].pivot = triGroupList[i].pivot * cmatrix;
	}

	transformAllVtxNormals(RTMatrix);  /* normals should NOT apply scale_matrix ! */
	transformAllTriNormals(RTMatrix);  /* normals should NOT apply scale_matrix ! */

	transformAABB(cmatrix);

	/*transform vtxscenter */
	vtxscenter = vtxscenter*cmatrix;

//	cmatrix.print("cmatrix");
}

/*----- TODO: to be obsolete, apply TriMesh.objmat instead -----
Scale up/down the whole mesh.
--------------------------------------------------------------*/
void E3D_TriMesh::scaleMesh(float scale)
{
	/* Scale all vertices */
	for( int i=0; i<vcnt; i++) {
		vtxList[i].pt.x *= scale;
		vtxList[i].pt.y *= scale;
		vtxList[i].pt.z *= scale;
	}

	/* Scale TriGroup.omat.pmat[9-11] as TriGroup origin/pivotal. */
	for( unsigned int k=0; k<triGroupList.size(); k++) {
		triGroupList[k].omat.pmat[9] *=scale;
		triGroupList[k].omat.pmat[10] *=scale;
		triGroupList[k].omat.pmat[11] *=scale;

		triGroupList[k].pivot *=scale;
	}

	/* Scale AABB accrodingly */
	aabbox.vmin *= scale;
	aabbox.vmax *= scale;
}

/*------------------------------------
Compute/update all triangle face normals.
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
			egi_dpstd("!!!WARNING!!! triList[%d].vtx[%d].index <0!\n", i,j);
			vtxIndex[j]=0;
		}
	   }

	   v01=vtxList[vtxIndex[1]].pt-vtxList[vtxIndex[0]].pt;
	   v12=vtxList[vtxIndex[2]].pt-vtxList[vtxIndex[1]].pt;

	   triList[i].normal=E3D_vector_crossProduct(v01,v12);
	   if( triList[i].normal.normalize() <0 ) {
		egi_dpstd(DBG_RED"Triangle triList[%d] error! or degenerated!\n"DBG_RESET, i);
		vtxList[vtxIndex[0]].pt.print(NULL);
		vtxList[vtxIndex[1]].pt.print(NULL);
		vtxList[vtxIndex[2]].pt.print(NULL);
	   }

   	}
}


/*--------------------------------------------------
Compute all vtxList[].normal, by averaging all
triList[].vtx[].vn, of which triangle is refering
the vertex.
--------------------------------------------------*/
void E3D_TriMesh::computeAllVtxNormals(void)
{
	int i,j,k;
	int refcnt;
	E3D_Vector vnsum;  /* init as (0,0,0) */

	/* TODO: way too slow. */
	for(i=0; i<vcnt; i++) {   /* traverse all vertices */
		refcnt=0;
		vnsum.zero();
		for(j=0; j<tcnt; j++) { /* traverse all trianlges */
		    for(k=0;k<3;k++) {
			  if( i==triList[j].vtx[k].index ) {
				refcnt++;
				vnsum+=triList[j].normal;
				//vnsum+=triList[j].vtx[k].vn;
			  }
		    }
		}

		/* Averaging */
		if(refcnt>0)
		     vtxList[i].normal=vnsum/(float)refcnt;
	}
}


/*--------------------------------------------------
Reverse normals of all triangles

@reverseVtxIndex:  If TURE, reverse vtx index also!

-------------------------------------------------*/
void E3D_TriMesh::reverseAllTriNormals(bool reverseVtxIndex)
{
	int td;

	egi_dpstd("Reverse all triangle normals...\n");
	/* Reverse triangle face normals */
	for(int i=0; i<tcnt; i++) {
	   //triList[i].normal *= -1.0f;
	   triList[i].normal =-triList[i].normal;
	}

	/* Reverse triangle vtx index */
	if(reverseVtxIndex) {
		egi_dpstd("Reverse all triangle vtx indices...\n");
		for(int i=0; i<tcnt; i++) {
			td=triList[i].vtx[0].index;
			triList[i].vtx[0].index=triList[i].vtx[2].index;
			triList[i].vtx[2].index=td;
		}
	}

	/* Reverse vtx normal */
	if( triList[0].vtx[0].vn.isZero()==false) {
		egi_dpstd("Reverse all triangle vertex normals..\n");
		for(int i=0; i<tcnt; i++) {
			triList[i].vtx[0].vn *= -1.0f;
			triList[i].vtx[1].vn *= -1.0f;
			triList[i].vtx[2].vn *= -1.0f;
		}
	}
}


/*--------------------------------------------------
Reverse all vertex normals.

@RTMatrix:      RotationTranslation Matrix
--------------------------------------------------*/
void E3D_TriMesh::reverseAllVtxNormals(void)
{
	if( vtxList[0].normal.isZero()==false ) {
		egi_dpstd("Reverse all vertex normals...\n");
		for(int i=0; i<vcnt; i++)
			vtxList[i].normal =-vtxList[i].normal;
	}
	else
		egi_dpstd("normal is zero!...\n");
}


/*-----------------------------------------------------
Copy all vertices and triangles from the input tmesh
, just to replace SOME old data! and the target TriMesh
is NOT identical to tmesh after operation!

Note:
		!!! Caution !!!
1. NOW: Some data are NOT cloned and replaced!

		!!! Caution !!!
2. TODO: If the subject TriMesh has pointer members
   to be replaced, it MUST free/release them at first!
3. Here mtlList[] and triGroupList[] to be cloned ONLY
   when they are emtpy! this is to avoid free/release
   pointer members inside.

-----------------------------------------------------*/
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
egi_dpstd("Clone vtxList[]...\n");
	vcnt =tmesh.vcnt;
	for(int i=0; i<vcnt; i++) {
		/* Vector as point */
		vtxList[i].pt = tmesh.vtxList[i].pt;

		/* Ref Coord. */
		vtxList[i].u = tmesh.vtxList[i].u;
		vtxList[i].v = tmesh.vtxList[i].v;

		/* Normals */
		vtxList[i].normal = tmesh.vtxList[i].normal;

		/* Mark number */
		vtxList[i].mark = tmesh.vtxList[i].mark;
	}

	/* Clone triList[] */
egi_dpstd("Clone triList[]...\n");
	tcnt =tmesh.tcnt;
	for(int i=0; i<tcnt; i++) {
		/* Vertx vtx[3] */
		triList[i].vtx[0]=tmesh.triList[i].vtx[0];
		triList[i].vtx[1]=tmesh.triList[i].vtx[1];
		triList[i].vtx[2]=tmesh.triList[i].vtx[2];

		/* Face Normal */
		triList[i].normal=tmesh.triList[i].normal;

		/* Vtexter normal vector */
		triList[i].vtx[0].vn = tmesh.triList[i].vtx[0].vn;
		triList[i].vtx[1].vn = tmesh.triList[i].vtx[1].vn;
		triList[i].vtx[2].vn = tmesh.triList[i].vtx[2].vn;

		/* int part;  int material; */

		/* Mark number */
		triList[i].mark=tmesh.triList[i].mark;
	}

	/* Clone AABB */
	aabbox=tmesh.aabbox;

	/* Clone vtxscenter */
	vtxscenter=tmesh.vtxscenter;

	/* Clone textureImg */
	textureImg=tmesh.textureImg;

	/* Clone defMaterial */
	defMaterial=tmesh.defMaterial;

#if 1   /* To avoid free/release imgbuf pointers in mtlList[] and triGroupList[],
	 * clone those data ONLY ONCE !!!
	 */
	/* Clone material list: mtlList[] */
	if( mtlList.size()<1 && tmesh.mtlList.size()>0 ) {
	    mtlList.resize(tmesh.mtlList.size());   /* .resize(), NOT .reserve() */
	    for(unsigned int i=0; i<tmesh.mtlList.size(); i++) {
		printf("Clone mtlList[%d]... \n", i);
		//mtlList.push_back(tmesh.mtlList[i]); /* This will trigger poiner free/release during capacity growing up! */
		mtlList[i]=tmesh.mtlList[i];
	    }
	}
        /* Clone triangle group list: triGroupList[] */
	if( triGroupList.size()<1 && tmesh.triGroupList.size()>0 ) {
	    triGroupList.resize(tmesh.triGroupList.size()); /* .resize(), NOT .reserve()! */
	    for(unsigned int i=0; i<tmesh.triGroupList.size(); i++) {
		//triGroupList.push_back(tmesh.triGroupList[i]); /* This will trigger poiner free/release during capacity growing up! */
		triGroupList[i]=tmesh.triGroupList[i];
	   }
	}
#endif  /////////////////////////////////////////////////////////////////////////

	/* !!! triGroupList[i].omat MUST copy each time. as to reset each time !!! */
	for( unsigned int i=0; i<tmesh.triGroupList.size(); i++) {
		triGroupList[i].omat=tmesh.triGroupList[i].omat;
		triGroupList[i].pivot=tmesh.triGroupList[i].pivot;
	}


	/* TODO: Clone other params if necessary. */
}


/*----------------------------------------
Read texture image into textureImg, and
to defMaterial.img_kd.

@fpath: Path to image file.
@nw:    >0 As new image width, keep ratio.
	<=0 Ignore.
----------------------------------------*/
void E3D_TriMesh::readTextureImage(const char *fpath, int nw)
{
	textureImg=egi_imgbuf_readfile(fpath);
	if(textureImg==NULL)
		egi_dpstd("Fail to read texture image from '%s'.\n", fpath);

	if( nw>0 ) {
		if( egi_imgbuf_resize_update(&textureImg, true, nw, -1)==0 )
			egi_dpstd("Succeed to resize image to %dx%d.\n", textureImg->width, textureImg->height);
		else
			egi_dpstd("Fail to resize image! keep size %dx%d.\n", textureImg->width, textureImg->height);
	}

	/* Assign to defMaterial */
	if(textureImg) {
		egi_imgbuf_free2(&defMaterial.img_kd);
		defMaterial.img_kd = textureImg;
		egi_dpstd("defMaterial.img_kd re_assign with texture image '%s'.\n", fpath);
	}
}

/*--------------------------------------------
Get MaterialID with given material name.
Return:
	<0	Fails
	>=0	OK
--------------------------------------------*/
int E3D_TriMesh::getMaterialID(const char *name) const
{
	if(name==NULL)
		return -1;

	if( mtlList.size()<1 )
		return -2;

	for(unsigned int i=0; i<mtlList.size(); i++) {
		if( mtlList[i].name.compare(name)==0 )
			return i;
	}

	return -3;
}

/*--------------------------------------------------------------
Project an array of 3D vpts according to the projection matrix,
and all vpts[] will be modified after calling.

Note:
1. Values of all vpts[] will be updated/projected!
2. The projection plane_XY is at Z=projMatrix.dv>0,
   while the Focus point is the Origin(Z=0).
3. If vpts[].z < projMatrix.dv, it means the point is behind
   the projection plane(the near plane) and it should be picked out.

TODO:
XXX 1. This is a simple projection mathod for testing.
   It does NOT consider depth(z) mapping. so z values will be
   incorrect.
2. Frustum culling(clipping) is NOT performed!

@vpts:		Input vertices/points in view(camera) COORD.
@np:		Number of points.
@projMatrix:	The projection matrix.

Return:
	0	OK
	>0 	Point out of the viewing frustum!
		OR too small fabs(vpts[k].z)!
	<0	Fails
---------------------------------------------------------------*/
int projectPoints(E3D_Vector vpts[], int np, const E3DS_ProjMatrix & projMatrix)
{
	int ret=0;

	/* 0. Check input */
	if(np<1)
	   return -1;

	/* 1. If Isometric projection */
	if( projMatrix.type==E3D_ISOMETRIC_VIEW ) {
		/* TODO: Check range */
		/* Just adjust viewplane origin to Window center */
		for(int k=0; k<np; k++) {
#if 1			/* Clip if out of frustum. plan view_z==0 */
			if( vpts[k].z < projMatrix.dv) {
				//egi_dpstd("Out of ViewFrustum! z=%f < projMatrix.dv=%d \n",  vpts[k].z, projMatrix.dv);
				ret=1;
			}
#endif
			vpts[k].x = vpts[k].x+projMatrix.winW/2;
			vpts[k].y = vpts[k].y+projMatrix.winH/2;
			/* Keeps Z */
		}
	}

        /* 2. Perspective view: projecting point onto the viewPlane/screen */
	else if( projMatrix.type==E3D_PERSPECTIVE_VIEW ) {
           for(int k=0; k<np; k++) {
#if 1		/* Clip if out of frustum. plan view_z==0 */
		if( vpts[k].z < projMatrix.dv) {
			//egi_dpstd("Some points out of the ViewFrustum!\n");
			ret=1;
		}
#endif
		/* Check Z */
		if( fabs(vpts[k].z) < VPRODUCT_EPSILON ) {
			egi_dpstd("fabs(vpts[].z) < VPRODUCT_EPSILON!\n");
			ret +=2;
		}
         	//vpts[k].x = roundf( vpts[k].x/vpts[k].z*projMatrix.dv  /* Xv=Xs/Zs*dv */
                //                    +projMatrix.winW/2 );              /* Adjust viewplen origin to window X center */
         	vpts[k].x = vpts[k].x/vpts[k].z*projMatrix.dv  /* Xv=Xs/Zs*dv */
                            +projMatrix.winW/2.0;              /* Adjust viewplane origin to window X center */

         	//vpts[k].y = roundf( vpts[k].y/vpts[k].z*projMatrix.dv  /* Yv=Ys/Zs*dv */
                //                    +projMatrix.winH/2 );              /* Adjust viewplane origin to window Y center */
         	vpts[k].y = vpts[k].y/vpts[k].z*projMatrix.dv  /* Yv=Ys/Zs*dv */
                            +projMatrix.winH/2.0;              /* Adjust viewplane origin to window Y center */

		/* Project z to Pseudo-depth within [0 1]. See page 184 at  <<计算机图形学基础教程(第2版)>> 孔令德  HK2022-09-07
		 * Zv[dnear dfar] maps to Zs[0 1.0]
		 */
		vpts[k].z = projMatrix.dfar*(1.0-projMatrix.dnear/vpts[k].z)/(projMatrix.dfar-projMatrix.dnear);

		/* Linear maps Zs[0 1.0] to [dnear dfar], as we need pixZ in integer type!  */
		vpts[k].z = projMatrix.dnear+(projMatrix.dfar-projMatrix.dnear)*vpts[k].z;
	   }

	}

	return ret;
}

/*-----------------------------------------------------
This is reverse operation of projectPoints().
vpts[] will be mapped/modified from screen coordinates
to 3D space coordinate.

Note:
1. Values of all vpts[] will be updated/projected!
2. The projection plane_XY is at Z=projMatrix.dv>0,
   while the Focus point is the Origin(Z=0).
3. vpts[].z NOT changed!

TODO:
XXX 1. It does NOT consider depth(z) mapping. so z values will be
incorrect.

@vpts:		Input vertices/points in Screen COORD.
@np:		Number of points.
@projMatrix:	The projection matrix.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------*/
int reverse_projectPoints(E3D_Vector vpts[], int np, const E3DS_ProjMatrix & projMatrix)
{
	/* 0. Check input */
	if(np<1)
	   return -1;

	/* 1. Isometric projection */
	if( projMatrix.type==E3D_ISOMETRIC_VIEW ) {
		/* Just adjust viewplane origin to GLOBAL coord. */
		for(int k=0; k<np; k++) {
			vpts[k].x = vpts[k].x-projMatrix.winW/2;
			vpts[k].y = vpts[k].y-projMatrix.winH/2;
			/* Keep Z */
		}
	}

        /* 2. Perspective view */
	else if( projMatrix.type==E3D_PERSPECTIVE_VIEW ) {
           for(int k=0; k<np; k++) {
                /* Linear remaps Z from [dnear dfar] to [0 1.0]  */
		vpts[k].z = (vpts[k].z-projMatrix.dnear)/(projMatrix.dfar-projMatrix.dnear);

	   	/* Reverse Pseudo-depth z. See page 184 at  <<计算机图形学基础教程(第2版)>> 孔令德  HK2022-09-07
		 * Zs[0 1.0] maps to Zv in View COORD
		 */
	   	vpts[k].z = projMatrix.dnear/(1.0-vpts[k].z*(1.0-projMatrix.dnear/projMatrix.dfar));

                /* -projMatrix.winW/2: Adjust Z axis aligns with view Z */
		vpts[k].x = (vpts[k].x - projMatrix.winW/2.0)/projMatrix.dv*vpts[k].z; /* Xs=Xv/dv*Zs,  suppose vpts[].z NOT changed. */
		vpts[k].y = (vpts[k].y - projMatrix.winH/2.0)/projMatrix.dv*vpts[k].z; /* Ys=Yv/dv*Zs,  suppose vpts[].z NOT changed. */
	   }
	}

	return 0;
}


/*-----------------------------------------------------
Map points XYZ to the Normalized Device Coordinates(NDC)
, and results XYZ are within [-1 1].

Note:
1. Values of all vpts[] will be updated/projected!
2. The projection plane_XY is at Z=projMatrix.dv>0,
   while the Focus point is the Origin(Z=0).
3. --- NOTICED ---
   projectPoints() and reverse_projectPoints() use other
   mapping algorithm.

@vpts:		Input vertices/points in Screen COORD.
@np:		Number of points.
@projMatrix:	The projection matrix.

Return:
	0	OK
	<0	Fails
-------------------------------------------------------*/
int mapPointsToNDC(E3D_Vector vpts[], int np, const E3DS_ProjMatrix & projMatrix)
{
	/* 0. Check input */
	if(np<1)
	   return -1;

/**  For a symmetricl screen plane, where r=-l and b=-t.
     The clipping matrix is:
              [
                  n/r(=A)       0         0             0
                   0       n/t(=B)        0             0
                   0         0   (f+n)/(f-n)(=C)    -2fn/(f-n)(=D)
                   0         0        1             0
                                                            ]
     Then:
	taken Wc==Ze.
	[Xc,Yc,Zc,Wc] = CMatrix*([Xe,Ye,Ze,We=1]^T)
*/

	float xc,yc,zc;	/* clipping coordinates */

   if(projMatrix.type==E3D_PERSPECTIVE_VIEW) {
	for(int k=0; k<np; k++) {
		/* Map to get clipping Coordinates */
		xc=vpts[k].x*projMatrix.A;
		yc=vpts[k].y*projMatrix.B;
		zc=vpts[k].z*projMatrix.C+projMatrix.D;	/* we=1.0 */
		//Wc=Ze

		/* NDC coordinates, taken Wc=Ze, then XYZn = XYZc/Ze */
		vpts[k].x=xc/vpts[k].z;
		vpts[k].y=yc/vpts[k].z;
		vpts[k].z=zc/vpts[k].z;  /* !!! At last ze -> zn, vpts[].z updated to be Zn! */
	}
   }
   else { /* projMatrix.type==E3D_ISOMETRIC_VIEW HK2022-10-09 */
	for(int k=0; k<np; k++) {
		/* Map to get clipping Coordinates==NDC coordinates */
		vpts[k].x=vpts[k].x*projMatrix.aa;
		vpts[k].y=vpts[k].y*projMatrix.bb;
		vpts[k].z=vpts[k].z*projMatrix.cc+projMatrix.dd; /* we=wc=1.0 */
	}
   }

	return 0;
}


/*----------------------------------------------------
Return code number for clipping a vertex.

Note:
1.For E3D, NDC x/y/z is limited within [-1 1].

@pt:	Point under NDC coordinate.

Return:
       if(code>0), then the point is out of the frustum!
------------------------------------------------------*/
int pointOutFrustumCode(const E3D_Vector &pt)
{
	int code=0; /* preset as within the frustum */

	/* Check faces of the frustum */
	if(pt.x<-1.0) code |= 0x01;
	else if(pt.x>1.0)  code |= 0x02;

	if(pt.y<-1.0) code |= 0x04;
	else if(pt.y>1.0)  code |= 0x08;

	if(pt.z<-1.0) code |= 0x10;
	else if(pt.z>1.0)  code |= 0x20;

	return code;
}


/*-----------------------------------------------------------------------
Check if the Ray hit the trimesh.

Note:
1. Hidden groups will be ignored.
2. Triangles backFacing to Ray will be ignored,
   unless trimesh.backFaceOn==TRUE.
3. There may be more than one triangle mesh hit/intersect
   with the incoming/incident ray, return the one with the nearest
   hit point.
4. !!! --- CAUTION ---- !!!
   If rgindx/rtindx are NOT provided then...
   The caller MUST check distance between ray.vp0 and vphit to
   see if ray.vp0 is on the same plane!

@rad:      The Ray
@rgindx:   Index of the TriGroup, to which Ray.vp0 belongs.
	   If <0. ignored.
@rtindx:   Index of the triangle, to which Ray.vp0 belongs.
	   If <0. ignored.

@vphit:	   (To pass out) Hit/intersection point.
@gindx     (To pass in/out) Index of the hit TriGroup, as triGroupList[gindx]
@tindx:	   (To pass in/out) Index of the hit triangle, as triList[tindx]


@fn:	   (To pass out) Reflecting surface normal.

Return:
	True:  The Ray intersect with the trimesh.
	Fals:  No intersection.
------------------------------------------------------------------------*/
bool E3D_TriMesh::rayHitFace(const E3D_Ray &ray, int rgindx, int rtindx,
			     E3D_Vector &vphit, int &gindx, int &tindx, E3D_Vector &fn) const
{
	unsigned int i,n;
//	int vtidx[3];  				/* vtx index of a triangel */
	E3D_Vector  vpts[3];			/* Projected 3D points */
//	EGI_POINT   pts[3];			/* Projected 2D points */
	E3D_Vector  trinormal;  		/* Face normal of a triangle, after transformation */
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); 	/* View direction */
	float	    vProduct;   		/* dot product */
	bool	    backfacing=false;		/* Triangle is backfacing the Ray */
	bool	    vp0PlaneChecked=false;	/* Triangle with rgindx and rtindx is checked out.*/

	E3D_RTMatrix  tgRTMat;  /* TriGroup RTMatrix, for Pxyz(under TriGroupCoord)*TriGroup--->object */
	E3D_Vector    tgOxyz;   /* TriGroup origin/pivot XYZ */
	E3D_RTMatrix  nvRotMat; /* RTmatrix(rotation component only) for face normal rotation, to be zeroTranslation() */

	E3D_Plane plane;  /* Plane of a triangle mesh. */
	E3D_Vector vp;    /* tmp. hit point  */
        float t;   /* Distance from vp0 to the hit/intersection point */
        float fp;
	float bc[3]; /* Barycenter coordinates value: XYZ */

	float distMIN=-1.0;  /* Init as <0; the minimum hit distance: from ray.vp0 to hit point vp. */
	float dist;
	float fbc[3]; /* corresponding to the final pvhit */


	/* Check rgindx and rtindx */
	if( rgindx<0 || rtindx<0 )
		vp0PlaneChecked=true;

	/* Traverse all trimesh */
	for(n=0; n<triGroupList.size(); n++) {

		/* 1. Check visibility */
		if(triGroupList[n].hidden) {
			egi_dpstd("triGroupList[%d]: '%s' is set as hidden!\n", n, triGroupList[n].name.c_str());
			continue;
		}

//		egi_dpstd("Check triGroupList[%d]: '%s'\n", n, triGroupList[n].name.c_str());

		/* 2. Extract TriGroup rotation matrix and rotation pivot */
		/* 2.1 To combine RTmatrix: Pxyz(under TriGroup Coord)*TrGroup-->object*object--->Global */
		tgRTMat=triGroupList[n].omat*objmat;

		/* 2.2 Face normal RotMat */
		nvRotMat = tgRTMat;
		nvRotMat.zeroTranslation();

		/* 2.3 Get TriGroup pivot, under object COORD */
		tgOxyz=triGroupList[n].pivot;

		/* 3. Traverse all triangles in the triGroupList[n] */
		for(i=triGroupList[n].stidx; i<triGroupList[n].etidx; i++) {
			/* Pick out the triangle to which Ray.vp0 belongs. HK2022-09-06 */
			if(!vp0PlaneChecked) {
			    /* rgindx and rtindx already checked at beginning. */
			    if( (int)n==rgindx && (int)i==rtindx ) {
				vp0PlaneChecked=true;
				continue;
			    }
			}

			/* 3.0 Cal. trinormal */
			trinormal=triList[i].normal*nvRotMat;

			/* 3.1 Pick out triangles backfacing the Ray */
			vProduct=ray.vd*trinormal;
			if( vProduct >=0.0f ) { /* >=0.0f */
				backfacing=true;
//				egi_dpstd(DBG_RED"--- backfacing! ---\n"DBG_RESET);

				if(!backFaceOn)
					continue;
			}
			else {
				backfacing=false;
			}

			/* 3.2 Copy triangle vertices */
			vpts[0] = vtxList[ triList[i].vtx[0].index ].pt;
			vpts[1] = vtxList[ triList[i].vtx[1].index ].pt;
			vpts[2] = vtxList[ triList[i].vtx[2].index ].pt;

			/* 3.3 TriGroup transform */
			/* 3.3.1 vpts under TriGroup COORD/pivot */
			vpts[0] -= tgOxyz;
			vpts[1] -= tgOxyz;
			vpts[2] -= tgOxyz;
			/* 3.3.2 Transform vpts[]: TriGroup---->Object; object--->Global */
			E3D_transform_vectors(vpts,3,tgRTMat);

			/* 3.4 Compute intersection point */
			/* 3.4.1 Define the Plane */
			plane.vn=trinormal; //triList[i].normal*nvRotMat;
			//plane.fd=vpts[0]*plane.vn; /* ax+by+cz=fd */
			plane.fd=(vpts[0]+vpts[1]+vpts[2])/3.0*plane.vn;

			/* 3.4.2 vd*vn product */
			fp=ray.vd*plane.vn;

		        /* Check if parrallel. NOT Necessary,alread picked out. see 3.1 */
        		//if( fabs(fp) < VPRODUCT_NEARZERO )
                	//	return false;

		        /* 3.4.3 Distance factor, to intersection point */
        		//t=(plane.fd-ray.vp0*plane.vn)/(ray.vd*plane.vn);
        		t=(plane.fd-ray.vp0*plane.vn)/fp;

#if 0 /* TEST: !!! ----------- the vp0 of ray just maybe belong to the the plane also! */
			/* NOTE: It depends on precision setting, Let caller to check it! */
			if( fabsf(t) < VPRODUCT_EPSILON)
				egi_dpstd(DBG_RED"--- t<EPSILON---\n"DBG_RESET);
#endif

        		/* OK, t>0!  see 3.1 */
        		//forward = (t>0.0f ? true:false);
			if(t<0.0f) { /*  backward directon */

//				egi_dpstd(DBG_RED"--- back trace ---\n"DBG_RESET);
				continue;
			}

        		/* 3.4.4 Get intersection point */
        		vp=ray.vp0+t*ray.vd;

			/* 3.5 Compute Barycenter to check if vp is on/in the triangle */
			if( E3D_computeBarycentricCoord(vpts, vp, bc) ) {
				if( bc[0]<0.0f || bc[1]<0.0f || bc[2]<0.0f ) {
					// Out of the triangle
//					printf("Hit point out of triangle, i=%d, fp=%f,t=%f, bc[]: %f,%f,%f\n",
//							i, fp,t, bc[0],bc[1],bc[2]);
				}
				/* OK, hit point is on/in the triangle */
				else {


/* TEST: -------- Unreasonable bc values */
					if( bc[0]>1.0f || bc[1]>1.0f || bc[2]>1.0f )
						egi_dpstd(DBG_RED"WARNING! bc[] > 1.0!\n"DBG_RESET);

//					printf(DBG_YELLOW"Hit on the triangle, i=%d, fp=%f,t=%f, hit at point (%f,%f,%f) \n"DBG_RESET,
//													i, fp,t, vp.x, vp.y, vp.z);
//					printf(DBG_YELLOW"bc[]: %f,%f,%f\n"DBG_RESET, bc[0],bc[1],bc[2]);

					/* Distance from ray.vp0 to hit point */
					dist=E3D_vector_distance(ray.vp0, vp);

					/* Check if it's the nearest collision point */
					if( distMIN<0.0f || distMIN > dist )  {  /* distMIN <0 AS init */
						/* Update distMIN */
						distMIN=dist;

						/* Update param to pass out */
						gindx=n;
						tindx=i;
						fn=plane.vn;
						vphit=vp;
						fbc[0]=bc[0]; fbc[1]=bc[1]; fbc[2]=bc[2];
					}
					else  {
						egi_dpstd(DBG_MAGENTA"dist >distMIN, ignored!\n"DBG_RESET);
					}
				}
			}
			else {
				egi_dpstd(DBG_RED"BarycentricCoord unsolvable!\n"DBG_RESET);
			}

		} /* END for(i) */
	} /* END for(n) */

	/* Check distMIN to see if collision happens. =0.0 Just on the plane */
	if( !(distMIN<0.0f) ) {
//		egi_dpstd(DBG_YELLOW"hitpoint bc[]: %f,%f,%f\n"DBG_RESET, fbc[0],fbc[1],fbc[2]);
		return true;
	}
	else
	        return false;
}

/*--------------------------------------------------------------------
Check if the Ray is blocked be the TriMesh, it's same as rayHitFace()
except that it returns when the ray hits any triangle, included backfaced.

Note:
1. Hidden groups will be ignored.
2. Triangles backFacing also counts!!!
3. There may be more than one triangle mesh hit/intersect, BUT it
   returns immediately after hitting the first one.

@rad:      The Ray

Return:
	True:  The Ray is blocked by the trimesh.
	Fals:  Not blocked.
--------------------------------------------------------------*/
bool E3D_TriMesh::rayBlocked(const E3D_Ray &ray) const
{
	unsigned int i,n;
//	int vtidx[3];  				/* vtx index of a triangel */
	E3D_Vector  vpts[3];			/* Projected 3D points */
//	EGI_POINT   pts[3];			/* Projected 2D points */
	E3D_Vector  trinormal;  		/* Face normal of a triangle, after transformation */
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); 	/* View direction */
	float	    vProduct;   		/* dot product */
	bool	    backfacing=false;		/* Triangle is backfacing the Ray */

	E3D_RTMatrix  tgRTMat;  /* TriGroup RTMatrix, for Pxyz(under TriGroupCoord)*TriGroup--->object */
	E3D_Vector    tgOxyz;   /* TriGroup origin/pivot XYZ */
	E3D_RTMatrix  nvRotMat; /* RTmatrix(rotation component only) for face normal rotation, to be zeroTranslation() */

	E3D_Plane plane;  /* Plane of a triangle mesh. */
	E3D_Vector vp;    /* tmp. hit point  */
        float t;   /* Distance from vp0 to the intersection point */
        float fp;
	float bc[3]; /* Barycenter coordinates value: XYZ */

	/* Traverse */
	for(n=0; n<triGroupList.size(); n++) {

		/* 1. Check visibility */
		if(triGroupList[n].hidden) {
			egi_dpstd("triGroupList[%d]: '%s' is set as hidden!\n", n, triGroupList[n].name.c_str());
			continue;
		}

//		egi_dpstd("Check triGroupList[%d]: '%s'\n", n, triGroupList[n].name.c_str());

		/* 2. Extract TriGroup rotation matrix and rotation pivot */
		/* 2.1 To combine RTmatrix: Pxyz(under TriGroup Coord)*TrGroup-->object*object--->Global */
		tgRTMat=triGroupList[n].omat*objmat;

		/* 2.2 Face normal RotMat */
		nvRotMat = tgRTMat;
		nvRotMat.zeroTranslation();

		/* 2.3 Get TriGroup pivot, under object COORD */
		tgOxyz=triGroupList[n].pivot;

		/* 3. Traverse all triangles in the triGroupList[n] */
		for(i=triGroupList[n].stidx; i<triGroupList[n].etidx; i++) {
			/* 3.0 Cal. trinormal */
			trinormal=triList[i].normal*nvRotMat;

			/* 3.1 DO NOT Pick out triangles backfacing the Ray */

			/* 3.2 Copy triangle vertices */
			vpts[0] = vtxList[ triList[i].vtx[0].index ].pt;
			vpts[1] = vtxList[ triList[i].vtx[1].index ].pt;
			vpts[2] = vtxList[ triList[i].vtx[2].index ].pt;

			/* 3.3 TriGroup transform */
			/* 3.3.1 vpts under TriGroup COORD/pivot */
			vpts[0] -= tgOxyz;
			vpts[1] -= tgOxyz;
			vpts[2] -= tgOxyz;
			/* 3.3.2 Transform vpts[]: TriGroup---->Object; object--->Global */
			E3D_transform_vectors(vpts,3,tgRTMat);

			/* 3.4 Compute intersection point */
			/* 3.4.1 Define the Plane */
			plane.vn=trinormal; //triList[i].normal*nvRotMat;
			//plane.fd=vpts[0]*plane.vn; /* ax+by+cz=fd */
			plane.fd=(vpts[0]+vpts[1]+vpts[2])/3.0*plane.vn;

			/* 3.4.2 vd*vn product */
			fp=ray.vd*plane.vn;

		        /* Check if parrallel. NOT Necessary,alread picked out. see 3.1 */
        		//if( fabs(fp) < VPRODUCT_NEARZERO )
                	//	return false;

		        /* 3.4.3 Distance factor, to intersection point */
        		//t=(plane.fd-ray.vp0*plane.vn)/(ray.vd*plane.vn);
        		t=(plane.fd-ray.vp0*plane.vn)/fp;

/* TEST: !!! ----------- the vp0 of ray just maybe belong to the the plane also! */
			if( fabsf(t) < VPRODUCT_EPSILON)
				egi_dpstd(DBG_RED"--- t<EPSILON---\n"DBG_RESET);

        		//forward = (t>0.0f ? true:false);
			if(t<0.0f) { /*  backward */
//				egi_dpstd(DBG_RED"--- back trace ---\n"DBG_RESET);
				continue;
			}

        		/* 3.4.4 Get intersection point */
        		vp=ray.vp0+t*ray.vd;

			/* 3.5 Compute Barycenter to check if vp is on/in the triangle */
			if( E3D_computeBarycentricCoord(vpts, vp, bc) ) {
				if( bc[0]<0.0f || bc[1]<0.0f || bc[2]<0.0f ) {
					// Out of the triangle
//					printf("Hit point out of triangle, i=%d, fp=%f,t=%f, bc[]: %f,%f,%f\n",
//							i, fp,t, bc[0],bc[1],bc[2]);
				}
				/* OK, hit point is on/in the triangle */
				else {

/* TEST: -------- Unreasonable bc values */
					if( bc[0]>1.0f || bc[1]>1.0f || bc[2]>1.0f )
						egi_dpstd(DBG_RED"WARNING! bc[] > 1.0!\n"DBG_RESET);

//					printf(DBG_YELLOW"Hit on the triangle, i=%d, fp=%f,t=%f, hit at point (%f,%f,%f) \n"DBG_RESET,
//													i, fp,t, vp.x, vp.y, vp.z);
//					printf(DBG_YELLOW"bc[]: %f,%f,%f\n"DBG_RESET, bc[0],bc[1],bc[2]);

						return true;

				}
			}
			else {
				egi_dpstd(DBG_RED"BarycentricCoord unsolvable!\n"DBG_RESET);
			}

		} /* END for(i) */
	} /* END for(n) */

        return false;
}


/*-----------------------------------------------------
Draw mesh wire.
fbdev:		Pointer to FBDEV
projMatrix:	Projection Matrix

View direction: TriMesh Global_Coord. Z axis.
-----------------------------------------------------*/
void E3D_TriMesh::drawMeshWire(FBDEV *fbdev, const E3DS_ProjMatrix &projMatrix) const
{
	unsigned int i,n;
	int vtidx[3];	    /* vtx index of a triangle */
	E3D_Vector  vpts[3];			/* Projected 3D points */
	E3D_Vector  trinormal;  		/* Face normal of a triangle */
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); /* View direction */
	float	    vProduct;   /* dot product */
	bool	    IsBackface=false;		/* Viewing the back side of a Trimesh. */

	/* RTMatrixes for the Object and TriGroup */
	E3D_RTMatrix	tgRTMat; //tgRotMat; /* TriGroup RTmatrix, for Pxyz(at TriGroup Coord) * TriGroup--->object */
	E3D_Vector	tgOxyz;		   /* TriGroup origin/pivot XYZ */
	E3D_RTMatrix	nvRotMat;	   /* RTmatrix(rotation component only) for face normal rotation, to be zeroTranslation() */

	/* NOTE: Set fbdev->flipZ, as we view from -z---->+z */
	fbdev->flipZ=true;

	/* Traverse all triGroups  */
	for(n=0; n<triGroupList.size(); n++) {  /* Traverse all Trigroups */
	    /* 1. Check visibility */
	    if( triGroupList[n].hidden ) {
		egi_dpstd("triGroupList[%d]: '%s' is set as hidden!\n", n, triGroupList[n].name.c_str());
		continue;
	    }

	    /* 2. Extract TriGroup rotation matrix and rotation pivot */
	    /* 2.1 To combine RTmatrix:  Pxyz(at TriGroup Coord) * TriGroup--->object * object--->Global */
	    tgRTMat = triGroupList[n].omat*objmat;
	    /* 2.2 Light RotMat */
	    nvRotMat = tgRTMat;
 	    nvRotMat.zeroTranslation(); /* !!! */
	    /* 2.3 TriGroup (original) pivot, under object COORD. */
	    tgOxyz = triGroupList[n].pivot;

	    /* 3. Traverse and render all triangles in the triGroupList[n] */
	    for(i=triGroupList[n].stidx; i<triGroupList[n].etidx; i++) {
                /* 3.R0. Extract triangle vertices */
                vtidx[0]=triList[i].vtx[0].index;
                vtidx[1]=triList[i].vtx[1].index;
                vtidx[2]=triList[i].vtx[2].index;

                vpts[0]=vtxList[vtidx[0]].pt;
                vpts[1]=vtxList[vtidx[1]].pt;
                vpts[2]=vtxList[vtidx[2]].pt;

		/* 3.R01. Convert vpts to under TriGroup COORD/Pivot */
		vpts[0] -= tgOxyz;
		vpts[1] -= tgOxyz;
		vpts[2] -= tgOxyz;

		/* 3.R02. Transform vpts[] to under Global COORD: TriGroup--->object; object--->Global */
		E3D_transform_vectors(vpts, 3, tgRTMat);

		/* 3.R0a.  Set PERSPECTIVE View vector. HK2022-09-08 */
		if( projMatrix.type == E3D_PERSPECTIVE_VIEW ) {
			vView=(vpts[0]+vpts[1]+vpts[2])/3.0;
		}

		/* 3.R1. Pick out back_facing triangles if !backFaceOn.
		 * Note:
		 *    A. If you turn off back_facing triangles, it will be transparent there.
		 *    B. OR use other color for the back face. For texture mapping, just same as front side.
		 *    C. tgRTMat to be counted in.
	 	 *    D. A face that is innvisible is ISO view, MAY BE visible in persepective view!
		 *	 Consider frustrum projection.
		 */
		trinormal=triList[i].normal*nvRotMat;
	        vProduct=vView*trinormal; //(triList[i].normal*nvRotMat); // *(-1.0f); // -vView as *(-1.0f);

		/* Note:
		 * 1. Because of float precision limit, vProduct==0.0f is NOT possible.
		 * 2. Take VPRODCUT_XXX a little bigger to make the triangle visible as possible, and to pick out by FBDEV.pixZ later.
		 */
		if ( vProduct >= VPRODUCT_NEARZERO ) { // -VPRODUCT_EPSILON ) {  /* >=0.0f */
			IsBackface=true;
			fbdev->zbuff_IgnoreEqual=true; /* Ignore to redraw shape edge! */
			//egi_dpstd("triList[%d] vProduct=%e >=0 \n",i, vProduct);

			/* If backFaceOn: To display back side (with specified color) ... */
			if(!backFaceOn)
		                continue;
		}
		else {
			IsBackface=false;
			fbdev->zbuff_IgnoreEqual=false; /* OK to redraw on same zbuff level */
			//egi_dpstd("triList[%d] vProduct=%f \n",i, vProduct);
		}

		/* 5.R2C. Project to screen/view plane */
		if( projectPoints(vpts, 3, projMatrix) !=0 )
			continue;

		/* 6. Draw lines */
		E3D_draw_line(&gv_fb_dev, vpts[0], vpts[1]); //, projMatrix);
		E3D_draw_line(&gv_fb_dev, vpts[1], vpts[2]); //, projMatrix);
		E3D_draw_line(&gv_fb_dev, vpts[0], vpts[2]); //, projMatrix);

	    } /* End for(i) */

	}/* End for(n) */

#if 0  ////////////////////////////////////////////////////////
	/* TEST: Project to Z plane, Draw X,Y only */
	for(int i=0; i<tcnt; i++) {

		/* 1. To pick out back faces.  */
		/* Note: For perspective view, this is NOT always correct!
		 * XXX A triangle visiable in ISOmetric view maynot NOT visiable in perspective view! XXX
		 */
		vProduct=vView*(triList[i].normal); // *(-1.0f); // -vView as *(-1.0f);
		if( vProduct > -VPRODUCT_EPSILON ) {  /* >=0.0f */
			//egi_dpstd("triList[%d] vProduct=%f >=0 \n",i, vProduct);
                        continue;
		}
		//egi_dpstd("triList[%d] vProduct=%f \n",i, vProduct);

		/* 2. Get vtxindex */
		vtidx[0]=triList[i].vtx[0].index;
		vtidx[1]=triList[i].vtx[1].index;
		vtidx[2]=triList[i].vtx[2].index;

		/* 3. Copy triangle vertices, and project to viewPlan COORD. */
		vpts[0]=vtxList[vtidx[0]].pt;
		vpts[1]=vtxList[vtidx[1]].pt;
		vpts[2]=vtxList[vtidx[2]].pt;

		if( projectPoints(vpts, 3, projMatrix) !=0 )
			continue;

		/* 4. ---------TEST: Set pixz zbuff. Should init zbuff[] to INT32_MIN: -2147483648 */
		/* A simple ZBUFF test: Triangle center point Z for all pixles on the triangle.  TODO ....*/
		gv_fb_dev.pixz=roundf((vpts[0].z+vpts[1].z+vpts[2].z)/3.0);

		/* 5. Draw line one View plan, OR draw_line_antialias() */
		for(int k=0; k<3; k++)
			draw_line(fbdev, roundf(vpts[k].x), roundf(vpts[k].y),
					 roundf(vpts[(k+1)%3].x), roundf(vpts[(k+1)%3].y) );

		#if 0 /* TEST: draw triangle normals -------- */
		E3D_POINT cpt;	    /* Center of triangle */
		float nlen=20.0f;   /* Normal line length */
		cpt.x=(vtxList[vtidx[0]].pt.x+vtxList[vtidx[1]].pt.x+vtxList[vtidx[2]].pt.x)/3.0f;
		cpt.y=(vtxList[vtidx[0]].pt.y+vtxList[vtidx[1]].pt.y+vtxList[vtidx[2]].pt.y)/3.0f;
		cpt.z=(vtxList[vtidx[0]].pt.z+vtxList[vtidx[1]].pt.z+vtxList[vtidx[2]].pt.z)/3.0f;

		draw_line(fbdev,cpt.x, cpt.y, cpt.x+nlen*(triList[i].normal.x), cpt.y+nlen*(triList[i].normal.y) );
		#endif
	}
#endif  ///////////////////////////////////

	/* Reset fbdev->flipZ */
	fbdev->flipZ=false;
}

/*---------------------------------------------------------------------
Draw mesh wire.
		!!! triGroupList[] AND omat NOT applys here !!!

Note:
1. View direction: from -z ----> +z, of Veiw_Coord Z axis.
2. Each triangle in mesh is transformed as per VRTMatrix, and then draw
   to FBDEV, TriMesh data keeps unchanged!
3. !!! CAUTION !!!
   Normals will also be scaled here if VRTMatrix has scale matrix combined!

@fbdev:		Pointer to FB device.
@VRTmatrix:	View_Coord transform matrix, from same as Global_Coord
		to expected view position. (Combined with ScaleMatrix)
		OR: --- View_Coord relative to Global_Coord ---.
-----------------------------------------------------------------------*/
void E3D_TriMesh::drawMeshWire(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix) const
{
	E3D_Vector  vView(0.0f, 0.0f, 1.0f);    /* View direction -z--->+z */
	float	    vProduct;   		/* dot product */

	/* For EACH traversing triangle */
	int vtidx[3];  		/* vtx index of a triangle, as of triList[].vtx[0~2].index */
	E3D_Vector  trivtx[3];	/* 3 vertices of transformed triangle. */
	E3D_Vector  trinormal;  /* Normal of transformed triangle */

	/* NOTE: Set fbdev->flipZ, as we view from -z---->+z */
	fbdev->flipZ=true;

	/* TEST: Project to Z plane, Draw X,Y only */
	for(int i=0; i<tcnt; i++) {
		/* Transform triangle vertices to under View_Coord */
		for(int k=0; k<3; k++) {
			/* Get indices of triangle vertices */
			vtidx[k]=triList[i].vtx[k].index;
			/* To transform vertices to under View_Coord */
			//trivtx[k] = -(vtxList[vtidx[k]].pt*VRTMatrix); /* -1, to flip XYZ to fit Screen coord XY */
			trivtx[k] = vtxList[vtidx[k]].pt*VRTMatrix;
		}

		/* Transform triangle normal. !!! WARNING !!! normal is also scaled here if VRTMatrix has scalematrix combined! */
		//trinormal = -(triList[i].normal*VRTMatrix); /* -1, same as above trivtx[] to flip  */
		trinormal = triList[i].normal*VRTMatrix;

#if 1		/* To pick out back faces.  */
		vProduct=vView*trinormal; /* -vView as *(-1.0f), -z ---> +z */
		//if ( vProduct >= 0.0f )
                //        continue;
		/* Note: Because of float precision limit, vProduct==0.0f is NOT possible. */
		if ( vProduct > -VPRODUCT_EPSILON ) {  /* >=0.0f */
			/* Ignore backface */
			continue;
		}
#endif

		/* 5. ---------TEST: Set pixz zbuff. Should init zbuff[] to INT32_MIN: -2147483648 */
		/* A simple ZBUFF test: Triangle center point Z for all pixles on the triangle.  TODO ....*/
		gv_fb_dev.pixz=roundf((trivtx[0].z+trivtx[1].z+trivtx[2].z)/3.0);

		/* !!!! Views from -z ----> +z */
		// gv_fb_dev.pixz = -gv_fb_dev.pixz; /* OR set fbdev->flipZ=true */

		/* Draw line: OR draw_line_antialias(): TODO: inconsistent with draw_filled_triangleX() */
		draw_line(fbdev, trivtx[0].x, trivtx[0].y, trivtx[1].x, trivtx[1].y);
		draw_line(fbdev, trivtx[1].x, trivtx[1].y, trivtx[2].x, trivtx[2].y);
		draw_line(fbdev, trivtx[0].x, trivtx[0].y, trivtx[2].x, trivtx[2].y);

		#if 0 /* TEST: draw triangle normals -------- */
		E3D_POINT cpt;	    /* Center of triangle */
		float nlen=20.0f;   /* Normal line length */
		cpt.x=(trivtx[0].x+trivtx[1].x+trivtx[2].x)/3.0f;
		cpt.y=(trivtx[0].y+trivtx[1].y+trivtx[2].y)/3.0f;
		cpt.z=(trivtx[0].z+trivtx[1].z+trivtx[2].z)/3.0f;
		trinormal.normalize();
		draw_line(fbdev, cpt.x, cpt.y, cpt.x+nlen*(trinormal.x), cpt.y+nlen*(trinormal.y) );
		#endif
	}

	/* Reset fbdev->flipZ */
	fbdev->flipZ=false;
}


/*------------------------------------------------------------------
Draw AABB of the mesh as wire.

Note:
1. View direction: from -z ----> +z, of Veiw_Coord Z axis.
2.		!!! CAUTION !!!
  This functions works ONLY WHEN original AABB is aligned with
  the global_coord! otherwise is NOT possible to get vpt[8]!

@fbdev:		Pointer to FB device.
@VRTmatrix:	View_Coord transform matrix, from same as Global_Coord
		to expected view position.
		OR: --- View_Coord relative to Global_Coord ---.
---------------------------------------------------------------------*/
void E3D_TriMesh::drawAABB(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix, const E3DS_ProjMatrix projMatrix) const
{
	E3D_Vector vctMin;
	E3D_Vector vctMax;
	E3D_Vector vpts[8];

	float xsize=aabbox.xSize();
	float ysize=aabbox.ySize();
	float zsize=aabbox.zSize();

	/* Get all vertices of the AABB */
	vpts[0]=aabbox.vmin;
	vpts[1]=vpts[0]; vpts[1].x +=xsize;
	vpts[2]=vpts[1]; vpts[2].y +=ysize;
	vpts[3]=vpts[0]; vpts[3].y +=ysize;

	vpts[4]=vpts[0]; vpts[4].z +=zsize;
	vpts[5]=vpts[4]; vpts[5].x +=xsize;
	vpts[6]=vpts[5]; vpts[6].y +=ysize;
	vpts[7]=vpts[4]; vpts[7].y +=ysize;

	/* 2. Transform vpt[] */
      #if 0
	for( int k=0; k<8; k++)
		vpts[k]= vpts[k]*VRTMatrix;
      #else
        E3D_transform_vectors(vpts, 8, VRTMatrix);
      #endif

	/* 3. Project to viewPlan COORD. */
        projectPoints(vpts, 8, projMatrix); /* Do NOT check clipping */

#if 0//////////////////////////////////////
	#if TEST_PERSPECTIVE  /* TEST: --Perspective matrix processing.
	        * Note: without setTranslation(x,y,.), Global XY_origin and View XY_origin coincide!
		*/
		if( abs(vpts[k].z) <1.0f ) {
			egi_dpstd("Point Z too samll, out of the Frustum?\n");
			return;
		}

		vpts[k].x = vpts[k].x/vpts[k].z*500  /* Xv=Xs/Zs*d, d=300 */
			   +160;	       /* Adjust to FB/Screen X center */
		vpts[k].y = vpts[k].y/vpts[k].z*500  /* Yv=Ys/Zs*d */
			   +120;  	       /* Adjust to FB/Screen Y center */
		/* keep vpt[k].z */
	#endif
	}
#endif /////////////////////////////////////

	/* Notice: Viewing from z- --> z+ */
	fbdev->flipZ=true;

	/* E3D_draw lines 0-1, 1-2, 2-3, 3-0 */
	E3D_draw_line(fbdev, vpts[0], vpts[1]);
	E3D_draw_line(fbdev, vpts[1], vpts[2]);
	E3D_draw_line(fbdev, vpts[2], vpts[3]);
	E3D_draw_line(fbdev, vpts[3], vpts[0]);
	/* Draw lines 4-5, 5-6, 6-7, 7-4 */
	E3D_draw_line(fbdev, vpts[4], vpts[5]);
	E3D_draw_line(fbdev, vpts[5], vpts[6]);
	E3D_draw_line(fbdev, vpts[6], vpts[7]);
	E3D_draw_line(fbdev, vpts[7], vpts[4]);
	/* Draw lines 0-4, 1-5, 2-6, 3-7 */
	E3D_draw_line(fbdev, vpts[0], vpts[4]);
	E3D_draw_line(fbdev, vpts[1], vpts[5]);
	E3D_draw_line(fbdev, vpts[2], vpts[6]);
	E3D_draw_line(fbdev, vpts[3], vpts[7]);

	/* Reset flipZ */
	fbdev->flipZ=false;
}


/*-------------------------------------------------------------------
Render the whole mesh and draw to FBDEV, triGroup by triGroup.
View_Coord:	Same as Global_Coord.
View direction: View_Coord Z axis.

@fbdev:		Pointer to FBDEV
@projMatrix	Projection Matrix

Note:
1. For E3D_GOURAUD_SHADING:
   If the mesh has NO vertex normal data, then triangle face
   normal will be used, so it's same effect as E3D_FLAT_SHADING
   in this case.

TODO:
1. Case TEXTURE_MAPPING: Apply vtxList[].normal OR triList[].vtx[].vn
    for lighting vector.(NOW is triList[].normal, vpts[] NOT applied.)
XXX 2. Case TEXTURE_MAPPING:  Apply pixz for each pixel, (NOW use same pixz value,
    as Z of the centroid, for all pixels in a triangle)
---------------------------------------------------------------------*/

///////////////   Apply triGroupList[] and Material color/map     ///////////////////

/* XXXTODO:
   1. NOW tgRTMat is applied ONLY for case E3D_TEXTURE_MAPPING and E3D_FLAT_MAPPING.
 */
int E3D_TriMesh::renderMesh(FBDEV *fbdev, const E3DS_ProjMatrix &projMatrix) const
{
	/* Count total triangles rendered actually */
	int RenderTriCnt=0;

	int k; //j;
	unsigned int i,n;
	int vtidx[3];  				/* vtx index of a triangel */
	E3D_Vector  vpts[3];			/* Projected 3D points */
	EGI_POINT   pts[3];			/* Projected 2D points */
	E3D_Vector  trinormal;  		/* Face normal of a triangle */
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); 	/* ISO View direction, adusted later if perspective view see 3.R0a. */
	float	    vProduct;   		/* dot product */
	EGI_16BIT_COLOR	 vtxColor[3];		/* Triangle 3 vtx color */
	E3D_Vector upLight(0,1,0);
	upLight.normalize();
	float vpd1,vpd2,vpd3;			/* vProdcut for lights */

	E3DS_RenderVertex rvtx[4];		/* 4 for MAX. Znear-clipping results */
	int  rnp;				/* number of vertices after Znear-clipping */

	int deltLuma;
	bool BackFacingLight=false;	/* A trimesh is fackfacing to the gv_vLight */
	bool HasVertexNormal=false;	/* If the mesh has vertex normal values. */
	bool IsBackface=false;		/* Viewing the back side of a Trimesh. */

	/* render color */
	EGI_16BIT_COLOR color; /* NOW: Diffuse color.  TODO others: ka,ks */
	/* Material color */
	EGI_16BIT_COLOR mcolor;
	/* Defualt texture map */
	EGI_IMGBUF *imgKd=NULL; /* NOW: Diffuse map.  TODO others: imgKa, imgKs */

	/* triGroup Material */
	E3D_Material tgmtl;

	/* Scene, NOT applied yet. */
	E3D_Scene scene;

	enum E3D_SHADE_TYPE  actShadeType;  /* Final shadeType in rendering a group, NOT shadeType in E3D_TriMesh.shadeType */

	/* RTMatrixes for the Object and TriGroup */
	E3D_RTMatrix	tgRTMat; //tgRotMat; /* TriGroup RTmatrix, for Pxyz(at TriGroup Coord) * TriGroup--->object */
	E3D_Vector	tgOxyz;		   /* TriGroup origin/pivot XYZ */
	E3D_RTMatrix	nvRotMat;	   /* RTmatrix(rotation component only) for light product, to be zeroTranslation() */

	/* For backward_ray_tracing shadow */
	float *pXYZ=NULL;
	size_t capacity=1024*3; /* capacity for pXYZ*/
	size_t np;
	E3D_Vector  vpt;
	E3D_Vector  tn;
	E3D_Ray	    vray;
	int gindx; int tindx;
	E3D_Vector vphit;     /* hit point on trimesh */
	E3D_Vector fn;	      /* reflect normal */
	float dist;

	/* Note: For MeshInstance, use MeshInstance::objmat! */
//	objmat.print("objmat");

	/* drawMeshWire() is diffierent from here in case E3D_WIRE_FRAMING */
	// drawMeshWire(fbdev, projMatrix); return;

	/* G1. To decidce actual shading type for in case of E3D_GOURAUD_SHADING. */
	actShadeType=shadeType;

	const char *strShadeSub=NULL;
	if(shadeType==E3D_GOURAUD_SHADING) {
		/* Each vertex in vxtList has normal value.  Triangles are weleded at the vertex. */
	        if(vtxList[ triList[i].vtx[0].index ].normal.isZero()==false) {
			HasVertexNormal=true;
		}
		/* Each vertex in a triList[] has normal value. */
		else if(triList[i].vtx[0].vn.isZero()==false) {  /* Triangles NOT welded at the vertex. */
			HasVertexNormal=true;
		}
		/* Triangle has face normal value, at least. */
		else {
			HasVertexNormal=false;
			actShadeType=E3D_FLAT_SHADING;

			strShadeSub="--->FLAT_SHADING";
			//Apply FLAT_SHADING
			egi_dpstd(DBG_GREEN"Do %s%s\n"DBG_RESET,E3D_ShadeTypeName[shadeType], strShadeSub);
		}
	}

	/* G2 Traverse all triGroupList[] */
	for(n=0; n<triGroupList.size(); n++) {

	    /* G2.0 Check visibility/hidden of the triGroupList[n] */
	    if( triGroupList[n].hidden ) {
		egi_dpstd("triGroupList[%d]: '%s' is set as hidden!\n", n, triGroupList[n].name.c_str());
		continue;
	    }
	    egi_dpstd("Rendering triGroupList[%d]: '%s' , ", n, triGroupList[n].name.c_str());

	    /* G2.1 Get material color(kd) and map(img_kd). Get E3D_Material for the triGroup  */
	    /* G2.1.1 triGroupList[n].mtlID < 0 (then mtlList[].img_kd MUST be NULL), use defMaterial  */
	    if( triGroupList[n].mtlID < 0 ){
		printf("with defMaterial, ...\n");
		mcolor=defMaterial.kd.color16Bits(); /* Diffuse color */
	    	imgKd = defMaterial.img_kd;

		tgmtl=defMaterial;  /* TODO operator '=' is NOT overloaded, see P3.  */
	    }
	    /* G2.1.2 triGroupList[n].mtlID>=0 && mtlList[].img_kd==NULL. use defMaterial.img_kd AND mtlList[triGroupList[n].mtlID]. */
	    else if( mtlList[ triGroupList[n].mtlID ].img_kd ==NULL ) { /* MidasHK_2022_01_07 */
		printf("triGroupList[n].mtlID =%d, img_kd==NULL! use Kd color or tgmtl. \n", triGroupList[n].mtlID);
		mcolor=mtlList[ triGroupList[n].mtlID ].kd.color16Bits();
		imgKd = defMaterial.img_kd;
		//imgKd = textureImg; /* In case USER scale_update meshModel.textureImg! instead of defMaterial.img_kd!! */

		tgmtl = mtlList[ triGroupList[n].mtlID ]; /* TODO: operator '=' is NOT overloaded, see P3.  */
	    }
	    /* G2.1.3 triGroupList[n].mtlID>=0 && mtlList[].img_kd!=NULL, use mtlList[ triGroupList[n].mtlID ] */
	    else {
		printf("with material mtlList[%d] ...\n", triGroupList[n].mtlID);
		mcolor=mtlList[ triGroupList[n].mtlID ].kd.color16Bits();
		imgKd=mtlList[ triGroupList[n].mtlID ].img_kd;

		tgmtl = mtlList[ triGroupList[n].mtlID ];  /* TODO: operator '=' is NOT overloaded, see P3.  */
	    }

	    /* G2.1A Check imgKd, reset actShadeType in case of E3D_TEXTURE_MAPPING */
	    /* TODO: imgKd to be cancelled, and use tgmtl instead.  */
	    if( shadeType==E3D_TEXTURE_MAPPING ) {
		if( imgKd==NULL || triGroupList[n].ignoreTexture ) {
			if(HasVertexNormal)
				actShadeType=E3D_GOURAUD_SHADING;
			else
				actShadeType=E3D_FLAT_SHADING;
		}
		else if(imgKd!=NULL)
			actShadeType=E3D_TEXTURE_MAPPING;
	    }
	    egi_dpstd(DBG_GREEN"Reset actShadeType to %s\n"DBG_RESET, E3D_ShadeTypeName[actShadeType]);

	    /* G2.2 Extract TriGroup transformation matrix 'tgRTmat', and rotation pivot 'tgOxyz'. */
	    /* G2.2.1 To combine RTmatrix to get tgRTmat:  Pxyz(at TriGroup Coord) * TriGroup--->object * object--->Global */
	    tgRTMat = triGroupList[n].omat*objmat;
	    /* G2.2.2 nvRotMat for lighting computatoin, without translation component. */
	    nvRotMat = tgRTMat;
 	    nvRotMat.zeroTranslation(); /* !!! */
	    /* G2.2.3 Get TriGroup (original) pivot 'tgOxyz, under object COORD. */
	    tgOxyz = triGroupList[n].pivot;

	    /* G2.3 Traverse and render all triangles in the triGroupList[n]
	     *  TODO: sorting, render Tris from near to far field.... */
	    for(i=triGroupList[n].stidx; i<triGroupList[n].etidx; i++) {

                /* G2.3.1 Extract triangle vertices, and put to vpts[0-2] */
                vtidx[0]=triList[i].vtx[0].index;
                vtidx[1]=triList[i].vtx[1].index;
                vtidx[2]=triList[i].vtx[2].index;

                vpts[0]=vtxList[vtidx[0]].pt;
                vpts[1]=vtxList[vtidx[1]].pt;
                vpts[2]=vtxList[vtidx[2]].pt;

		/* G2.3.2 Convert vpts[] to under TriGroup local COORD, Pivot as its origin */
		vpts[0] -= tgOxyz;
		vpts[1] -= tgOxyz;
		vpts[2] -= tgOxyz;

		/* G2.3.3 Transform vpts[] to under Global/View/Eye COORD: TriGroup--->object; object--->Global */
		E3D_transform_vectors(vpts, 3, tgRTMat);

		/* G2.3.4 Set PERSPECTIVE View vector(eye origin to triangle center). HK2022-09-08 */
		if( projMatrix.type == E3D_PERSPECTIVE_VIEW ) {
			vView=(vpts[0]+vpts[1]+vpts[2])/3.0;
		}


#define TEST_Y_CLIPPING 0

		/* <<<<<----- TEST: RTMatrix operation functions! ---->>>>>  */

/* NOTE: Here transform points coordinates ONLY, keep normaml values, as we will reverse back to again. */
#if TEST_Y_CLIPPING /* TEST: transform object axisY to position of eye -axisZ. ---------------- */
// Example: ./test_trimesh -c -s 130 3Dhead.obj -X -165 -z -90 -t -m -i -B -a -0.75 -M headclipY.mot

        /* 1.bkMat to transform back to init its position */
        E3D_RTMatrix bkMat=tgRTMat;
        bkMat.inverse(); /* inverse */

        /* 2.Translation back to under object COORD */
        E3D_RTMatrix transMat, _transMat;
        E3D_Vector  Oxyz=tgOxyz;  /* Not its the original Oxyz */
        transMat.setTranslation(Oxyz.x, Oxyz.y, Oxyz.z);

        /* 3.Roate object_axisY to position of object -axisZ for clipping.  (later reverse to Y ) */
        E3D_RTMatrix rotMat;
        float angs[3]={0};
        angs[0]=MATH_PI*75/180;  /* rotat object +Y to eye_-Z direction, '75' incooperate to option '-X -165' */
      	E3D_combIntriRotation("X", angs, rotMat);

        /* Transform points ONLY, keep normaml values, as we will reverse back to.  */

        /* 4. Transform Oxyz, as rotMat. and reset transMat, _transMat */
        E3D_transform_vectors(&Oxyz, 1, rotMat);
	/* NOW: Oxyz is aft rotMat */
        _transMat.setTranslation(-Oxyz.x, -Oxyz.y, -Oxyz.z);

 	/* a. Reverse vpts back to init, and applay rotMat
              E3D_transform_vectors(vpts, 3, bkMat*transMat*rotMat);
           b. Apply -tgOxy and tgRTMat again to let under current set eye COORD:
              E3D_transform_vectors(vpts,3, _transMat*tgRTMat);
	   Consolidate above a,b steps into: bkMat*transMat*rotMat*_transMat*tgRTMat
	*/
	/* Reverse vpts back to init position, then rotMat, and then apply tgRTMat again. */
      	/* Note: transMat(+Oxyz) is original Oxyz, and _transMat(-Oxyz) is aft rotMat
       	 *  under view/eye COORD --> *bkRMat-->under triGroup local COORD --> *transMat --> under object COORD --> *rotMat --
	 *  --> rotated under object COORD --> *_transMat --> under triGroup local COORD --> *tgRTMat --> under view/eye COORD
	 */
        E3D_transform_vectors(vpts, 3, bkMat*transMat*rotMat*_transMat*tgRTMat);
#endif  /////////////////////// TEST_Y_CLIPPING /////////////////////////////


		/* G2.3.5 Check trinormal and pick out back_facing triangles. IF(backFaceOn) then flip trinormal and go on.
		 * Note:
		 *    A. If you turn off back_facing triangles, it will be transparent there.
		 *    B. OR use other color for the back face. For texture mapping, just same as front side.
		 *    C. tgRTMat to be counted in.
	 	 *    D. A face that is innvisible in ISO view, it MAY BE visible in persepective view!
		 *	 Consider frustrum projection.
		 */
		trinormal=triList[i].normal*nvRotMat;
	        vProduct=vView*trinormal; //(triList[i].normal*nvRotMat); // *(-1.0f); // -vView as *(-1.0f);

		/* Note:
		 * 1. Because of float precision limit, vProduct==0.0f is NOT possible.
		 * 2. Take VPRODCUT_XXX a little bigger to make the triangle visible as possible, and to pick out by FBDEV.pixZ later.
		 */
		if ( vProduct >= VPRODUCT_NEARZERO ) { // -VPRODUCT_EPSILON ) {  /* >=0.0f */
			IsBackface=true;
			fbdev->zbuff_IgnoreEqual=true; /* Ignore to redraw shape edge! */
			//egi_dpstd("triList[%d] vProduct=%e >=0 \n",i, vProduct);

			/* If backFaceOn: To display back side (with specified color) ... */
			if( !backFaceOn && !triGroupList[n].backFaceOn)
		                continue;
			else if(triGroupList[n].backFaceOn)
				egi_dpstd(DBG_GREEN"triGroupList[%d].backFaceOn!\n"DBG_RESET, n);

			/* To flip vProduct and trinormal */
			trinormal=-trinormal;
		}
		else {
			IsBackface=false;
			fbdev->zbuff_IgnoreEqual=false; /* OK to redraw on same zbuff level */
			//egi_dpstd("triList[%d] vProduct=%f \n",i, vProduct);
		}

		/* Note: In E3D_Scene::renderScene(), instances completely out of the view frustum will be picked out. */

		/* TODO: To rule out triangles totally out of view_frustum */

		/* G2.3.6 Clipping with near plane of the view_frustum, apply rvtx[] instead of vpts[].   HK2022-10-04
		 * Note: If triangle completely IN the frustum, then keep rvtx[] unchanged.
		 *	 If triangle completely OUT of the frustum near plane, then return rnp==0!
		 */

		/* G2.3.6.1 Each triangle with 3 vertices, assign to rvtx[].pt */
		rnp=3;
		rvtx[0].pt=vpts[0]; rvtx[1].pt=vpts[1]; rvtx[2].pt=vpts[2];
		/* Assign rvtx[].normal at G2.3.6.3 */

		/* G2.3.6.2 IF(actShadeType==E3D_FLAT_SHADING) */
		if(actShadeType==E3D_FLAT_SHADING) {
			/* Clip with the near plane of the viewFrustum */
			E3D_ZNearClipTriangle(projMatrix.dnear, rvtx, rnp, 0);
		}
		/* G2.3.6.3 IF(actShadeType==E3D_TEXTURE_MAPPING), assign rvtx[].normal depending on CaseA/B/C. */
		else if(actShadeType==E3D_TEXTURE_MAPPING || actShadeType==E3D_GOURAUD_SHADING) {
			/* Case_A: Use vtxList[ triList[i].vtx[k].index ].normal.  vertices are welded. */
			if(vtxList[ triList[i].vtx[0].index ].normal.isZero()==false) {
				egi_dpstd("Case_A\n");
				/* Assign rvtx[].normal */
				for(int k=0; k<3; k++) {
					rvtx[k].normal=vtxList[ triList[i].vtx[k].index ].normal*nvRotMat;
					if(IsBackface) rvtx[k].normal=-rvtx[k].normal;
					//if(IsBackface) rvtx[k].normal=-0.85*rvtx[k].normal; /* darken */
				}
			}
			/* Case_B: Use triList[].vtx[].vn;   vertices are NOT welded. */
			else if(triList[i].vtx[0].vn.isZero()==false) {
				egi_dpstd("Case_B\n");
				/* Assign rvtx[].normal */
				for(int k=0; k<3; k++) {
				    #if 1
					rvtx[k].normal=(triList[i].vtx[k].vn)*nvRotMat;
				    #else
					rvtx[k].normal=triList[i].normal*nvRotMat; //<---- TEST
				    #endif
					if(IsBackface) rvtx[k].normal=-rvtx[k].normal;
					//if(IsBackface) rvtx[k].normal=-0.85*rvtx[k].normal; /* darken */
				}
			}
			/* Case_C: Use face normal triList[].normal. FLAT_SHADING
			 * Noticed that triList[].normal will NOT read from obj file.
			 */
			else {
				egi_dpstd("Case_C\n");
				/* Assign rvtx[].normal */
				for(int k=0; k<3; k++) {
					rvtx[k].normal=triList[i].normal*nvRotMat;
					if(IsBackface) rvtx[k].normal=-rvtx[k].normal;
					//if(IsBackface) rvtx[k].normal=-0.85*rvtx[k].normal; /* darken */
				}
			}

			/* Vertex uv */
			if(actShadeType==E3D_TEXTURE_MAPPING) {
			    for(int m=0; m<3; m++) {
				/* Notice E3D uv origin at leftTop, while .obj at leftBottom */
				rvtx[m].u=triList[i].vtx[m].u;
				rvtx[m].v=1.0-triList[i].vtx[m].v;
			    }
			}

			/* Clip with the near plane of the viewFrustum */
			E3D_ZNearClipTriangle(projMatrix.dnear, rvtx, rnp, VTXNORMAL_ON|VTXUV_ON); //no VTXCOLOR_ON
		}
		/* TODO more cases: for VTXNORMAL_ON|VTXCOLOR_ON|VTXUV_ON */

		if(rnp>3) egi_dpstd(DBG_YELLOW"After clipping rnp=%d\n"DBG_RESET, rnp);

#if TEST_Y_CLIPPING /* TEST: Rotate object -axisZ back to position axisY ---------------- */
        /* 1. Reverse bkMat */
        //bkMat.inverse(), OK ==tgRTMat;

	/* 2. Oxyz is before reversing rotMat */
        transMat.setTranslation(Oxyz.x, Oxyz.y, Oxyz.z);

        /* 3. Reverse PI/4 rotation */
        rotMat.inverse();

	/* 4. Oxyz after Reversing rotMat */
	//E3D_transform_vectors(&Oxyz, 1, rotMat); OK ==tgOxyz
	Oxyz=tgOxyz;  /* Reverse back to original tgOxyz */
	_transMat.setTranslation(-Oxyz.x, -Oxyz.y, -Oxyz.z);

        /* Reverse triangle(points) back to under eye COORD and preset Orientation */
        for(int kk=0; kk<rnp; kk++) {
	      /* Note: transMat(+Oxyz) is beffore reversing rotMat, _transMat(-Oxyz) is aft reversing rotMat
       	       *  under view/eey COORD --> *bkRMat-->under triGroup local COORD --> *transMat --> under object COORD --> *rotMat --
	       *  --> rotated under object COORD --> *_transMat --> under local COORD --> *tgRTMat --> under view/eye COORD
	       */
	      E3D_transform_vectors(&rvtx[kk].pt, 1, bkMat*transMat*rotMat*_transMat*tgRTMat);
	}
#endif  /////////////////////// TEST_Y_CLIPPING /////////////////////////////


		/* G2.3.7 As backFaceOn && IsBackface, assign bkFaceColor to tgmtl.kd */
		if(IsBackface)
			tgmtl.kd.vectorRGB(bkFaceColor);
		else
			tgmtl.kd=defMaterial.kd;

		/* G2.3.8 Project rvtx[].pt to screen/view plane */
		for(int m=0; m<rnp; m++) {
		      projectPoints(&rvtx[m].pt, 1, projMatrix);
		}

	/* G2.3.9 To render the triangle after clipping, apply rvtx[]. (One triangle is clipped into 1 or 2 new triangles, or NOT clipped at all.) */
        for(int j=3; j<=rnp; j++) {  /* Note: rnp=0 if Tri is completely out of the frustum */

//NOT for E3D_shadeTriangle()  TEXTURE_MAPPING
		/* G2.3.9.1 Assign rvtx[].pt to vpts[3], for 1 or 2 triangles. */
	        if(j==3) { /* 0,1,2 for the first triangle */
		   vpts[0]=rvtx[0].pt;
		   vpts[1]=rvtx[1].pt;
		   vpts[2]=rvtx[2].pt;
		}
		else {  /* 2,3,0 for the second triangle */
		   vpts[0]=rvtx[2].pt;
		   vpts[1]=rvtx[3].pt;
		   vpts[2]=rvtx[0].pt;
		   /* TODO: VTXNORMAL/VTXCOLOR/VTXUV */
		}

          #if 0 //Already projected, see G2.3.8
		/*  vpts[] Project to screen/view plane */
		if( projectPoints(vpts, 3, projMatrix) !=0 )
			continue;
          #endif

		/* G2.3.9.2 Get 2D point pts[].xyz=roundf(vpts[].xyz), pts[] as 2D points on the screen/view plane  */
		pts[0].x=roundf(vpts[0].x); pts[0].y=roundf(vpts[0].y);
		pts[1].x=roundf(vpts[1].x); pts[1].y=roundf(vpts[1].y);
		pts[2].x=roundf(vpts[2].x); pts[2].y=roundf(vpts[2].y);
		// pts[].z = vpts[].z,  NOT assigned here, see at draw_filled_triangleX(..., -vpts[].z)

		/* ---------TEST: Set pixz zbuff. Should init zbuff[] to INT32_MIN: -2147483648, see in caller's main(). */
		/* A simple test: Triangle center point Z for all pixles on the triangle.  TODO ....*/
		//gv_fb_dev.pixz=roundf((vtxList[vtidx[0]].pt.z+vtxList[vtidx[1]].pt.z+vtxList[vtidx[2]].pt.z)/3.0);
		fbdev->pixz=roundf((vpts[0].z+vpts[1].z+vpts[2].z)/3.0);
		/* !!!! Views from -z ----> +z */
		fbdev->pixz = -fbdev->pixz;
		/* Above is NO USE for draw_filled_triangleX() with Z0/Z1/Z2 */

		/* TODO: For draw_filled_triangleX(..., float z0, z1,z2), suppose that Z values are not affected by projection! */
////////////////////////////

		/* G2.3.9.3 Sort rvtx[] for E3D_shadeTriangle(), for 1 or 2 triangles. */
		if(j==3) { /* 0,1,2 for the first triangle */
		}
		else { /* (j==4) 0,2,3 for the second triangle */
			//rvtx[0]=rvtx[0];
			rvtx[1]=rvtx[2];
			rvtx[2]=rvtx[3];
		}

		/* G2.3.9.4 Draw triangles with specified shading type. */
		switch( actShadeType ) {
		   case E3D_FLAT_SHADING:
			/* Compute lighting color */
			fbdev->pixcolor=E3D_seeColor(tgmtl, trinormal, 0); /* material, normal, dl */

			#if 0 /* Fill the TriFace */
			draw_filled_triangle(fbdev, pts);
			#else /* pixZ applys */
			draw_filled_triangle2(fbdev, pts[0].x, pts[0].y,
						     pts[1].x, pts[1].y,
						     pts[2].x, pts[2].y,
						    /* Views from -z ----> +z  */
						    -vpts[0].z, -vpts[1].z, -vpts[2].z );  /* Pseudo-depth */
			#endif

			break;

		   case E3D_GOURAUD_SHADING:

#if 0 ///////////////////////////  DEMO  /////////////////////
			/* 1. Display back face also */
//			if( IsBackface ) {
//				vtxColor[0]=bkFaceColor;
//				vtxColor[1]=bkFaceColor;
//				vtxColor[2]=bkFaceColor;
//			}
//			/* 2. Display front face, compute 3 vtx color respectively. */
//			else
//			{
			   	/* Calculate light reflect strength for each vertex:  TODO: not correct, this is ONLY demo. */
				/* Case_A: Use vtxList[ triList[i].vtx[k].index ].normal */
				if(vtxList[ triList[i].vtx[0].index ].normal.isZero()==false) {
				   egi_dpstd("Case_A\n");
				   for(k=0; k<3; k++) {
				     #if 0 /* No gv_auxLight */
			      		vProduct=gv_vLight*(vtxList[ triList[i].vtx[k].index ].normal);
			   		//if( vProduct >= 0.0f )
					if(IsBackface) {
						vProduct=-vProduct;
						color=bkFaceColor;
					}
					else
						color=mcolor;

					/* If Backfacing to gv_vLight */
			   		if( vProduct > -VPRODUCT_NEARZERO ) {
						vProduct=0.0f;
						BackFacingLight=true;
					}
			   		else { /* Flip to get vProduct absolute value for luma */
						vProduct=-vProduct;
						BackFacingLight=false;
					}

				     #else /* With gv_auxLight */
					E3D_Vector tnv=vtxList[ triList[i].vtx[k].index ].normal*nvRotMat;
					vpd1=gv_vLight*tnv;
					vpd2=0.5*gv_auxLight*tnv;

					/* Backfacing to the main gv_vLight */
					if(vpd1 > -VPRODUCT_NEARZERO)
					    BackFacingLight=true;
					else
					    BackFacingLight=false;

					/* Backface to the viewer */
					if(IsBackface) {
						//vProduct=-vProduct;
						vProduct=(vpd1>vpd2?vpd1:vpd2); /* no flip */
						color=bkFaceColor;
					}
					else {
						vProduct=( vpd1 < vpd2 ? vpd1 : vpd2); /* +- flips */
						color=mcolor;
					}

					/* If Backfacing to gv_vLight */
			   		if( vProduct > -VPRODUCT_NEARZERO ) {
						vProduct=0.0f;
						//XXXBackFacingLight=true;
					}
			   		else { /* Flip to get vProduct absolute value for luma */
						vProduct=-vProduct;
						//XXXBackFacingLight=false;
					}
				     #endif

			   		/* Adjust luma for color as per vProduct. */
					#if 0
					int  deltLuma=(int)roundf((vProduct-1.0f)*240.0);
					if(deltLuma<-120) deltLuma=-120;
			   		vtxColor[k]=egi_colorLuma_adjust(color, deltLuma);
					#else
			   		vtxColor[k]=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
					#endif
				   }
				}
			   	/* Case_B: Use triList[].vtx[].vn */
			   	else if(triList[i].vtx[0].vn.isZero()==false) {
				   //egi_dpstd("Case_B\n");
				   for(k=0; k<3; k++) {
					vpd1=gv_vLight*((triList[i].vtx[k].vn)*nvRotMat);
					vpd2=0.5*gv_auxLight*((triList[i].vtx[k].vn)*nvRotMat);

					/* Backfacing to the main gv_vLight */
					if(vpd1 > -VPRODUCT_NEARZERO)
					    BackFacingLight=true;
					else
					    BackFacingLight=false;

					/* Backface to the viewer */
					if(IsBackface) {
						//vProduct=-vProduct;
						vProduct=(vpd1>vpd2?vpd1:vpd2); /* no flip */
						color=bkFaceColor;
					}
					else {
						vProduct=( vpd1 < vpd2 ? vpd1 : vpd2); /* +- flips */
						color=mcolor;
					}

					/* If Backfacing to gv_vLight */
			   		if( vProduct > -VPRODUCT_NEARZERO ) {
						vProduct=0.0f;
						//XXXBackFacingLight=true;
					}
			   		else { /* Flip to get vProduct absolute value for luma */
						vProduct=-vProduct;
						//XXXBackFacingLight=false;
					}

			   		/* Adjust luma for color as per vProduct. */
					#if 1
					deltLuma=(int)roundf((vProduct-1.0f)*240.0);
					//if(deltLuma<-120) deltLuma=-120;
					//int luma=egi_color_getY(color);
					//if(deltLuma < -3*luma/4) deltLuma = -3*luma/4;
			   		vtxColor[k]=egi_colorLuma_adjust(color, deltLuma);
					#endif

			   		//vtxColor[k]=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
				   }
				}
				/* Case_C: AS E3D_FLAT_SHADING, Use face normal triList[].normal. */
				else {
				   egi_dpstd("Case_C\n");
			      		//vProduct=gv_vLight*((triList[i].normal)*nvRotMat);  // *(-1.0f); // -vLight as *(-1.0f);
			      		vpd1=gv_vLight*(triList[i].normal*nvRotMat);
					vpd2=0.5*gv_auxLight*(triList[i].normal*nvRotMat);

					/* normal flips for backface */
					if(IsBackface) {
						vpd1=-vpd1;
						vpd2=-vpd2;
					}

					/* Backfacing to the main gv_vLight */
					if(vpd1 > -VPRODUCT_NEARZERO)
					    BackFacingLight=true;
					else
					    BackFacingLight=false;

					/* Get main vProduct */
					vProduct=( vpd1 < vpd2 ? vpd1 : vpd2); /* +- flips */
					color=mcolor;

					/* Backfacing to all Lights */
			   		if( vProduct > -VPRODUCT_NEARZERO ) {
						vProduct=0.0f;
						//XXXBackFacingLight=true;
					}
			   		else { /* Flip to get vProduct absolute value for luma */
						vProduct=-vProduct;
						//XXXBackFacingLight=false;
					}

			   		/* Adjust luma for color as per vProduct. */
					#if 0
					int  deltLuma=(int)roundf((vProduct-1.0f)*240.0);
					if(deltLuma<-120) deltLuma=-120;
			   		vtxColor[0]=egi_colorLuma_adjust(color, deltLuma);
					vtxColor[2]=vtxColor[1]=vtxColor[0];
					#else
			   		vtxColor[0]=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
					vtxColor[2]=vtxColor[1]=vtxColor[0];
					#endif
				}
//			} /* END else() */

			/* 3. Fill triangle with pixel color, by barycentric coordinates interpolation. */
			#if 0 /* Float x,y 	---- OBSOLETE---- 	*/
			draw_filled_triangle2(fbdev,vpts[0].x, vpts[0].y,
						    vpts[1].x, vpts[1].y,
						    vpts[2].x, vpts[2].y,
						    vtxColor[0], vtxColor[1], vtxColor[2] );
			#elif 0 /* Int x,y */
			draw_filled_triangle3(fbdev,pts[0].x, pts[0].y,
						    pts[1].x, pts[1].y,
						    pts[2].x, pts[2].y,
						    vtxColor[0], vtxColor[1], vtxColor[2] );
			#else  /* Int x,y, float z */
			draw_filled_triangle4(fbdev,pts[0].x, pts[0].y,
						    pts[1].x, pts[1].y,
						    pts[2].x, pts[2].y,
						    /* Views from -z ----> +z  */
						    -vpts[0].z, -vpts[1].z, -vpts[2].z,
						    vtxColor[0], vtxColor[1], vtxColor[2] );
			#endif

#else //////////////  E3D_shadeTriangle( ) /////////////////////
  		       E3D_shadeTriangle(fbdev, rvtx, tgmtl, scene); /* fbdev, rvtx[3], mtl, scene */

#endif //////////////////////////////////////////////////////////////
			break;

		   case E3D_WIRE_FRAMING:
			/*  Calculate light reflect strength for the TriFace:  TODO: not correct, this is ONLY demo.  */
			vProduct=gv_vLight*((triList[i].normal)*nvRotMat);  // *(-1.0f); // -vLight as *(-1.0f);
			/* TODO: gv_auxLight NOT applied yet. */
			//if( vProduct >= 0.0f )
			if(IsBackface) {
				break;
				vProduct=-vProduct;
				color=bkFaceColor;
			}
			else
				color=wireFrameColor;

			/* If Backfacing to gv_vLight */
			if( vProduct > -VPRODUCT_NEARZERO ) {
				vProduct=0.0f;
				BackFacingLight=true;
			}
			else { /* Flip to get vProduct absolute value for luma */
				vProduct=-vProduct;
				BackFacingLight=false;
			}

			/* Adjust luma for pixcolor as per vProduct. */
			fbdev->pixcolor=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
			//fbdev->lumadelt=(vProduct-1.0f)*240.0) +50; /* Must reset later.. */
			//egi_dpstd("vProduct: %e, LumaY: %d\n", vProduct, (unsigned int)egi_color_getY(fbdev->pixcolor));

			#if 0/* Draw line: OR draw_line_antialias() */
			draw_line(fbdev, pts[0].x, pts[0].y, pts[1].x, pts[1].y);
			draw_line(fbdev, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
			draw_line(fbdev, pts[0].x, pts[0].y, pts[2].x, pts[2].y);
			#elif 0
			draw_filled_triangle_outline(fbdev, pts); // NOT good, need to improve.
			#else
			E3D_draw_line(&gv_fb_dev, vpts[0], vpts[1]); //, projMatrix);
			E3D_draw_line(&gv_fb_dev, vpts[1], vpts[2]); //, projMatrix);
			E3D_draw_line(&gv_fb_dev, vpts[0], vpts[2]); //, projMatrix);
			#endif
			break;
		   case E3D_TEXTURE_MAPPING:

			/* Note: If backFaceOn, then backface texutre is also SAME as frontface texture. */

#if 0 ///////////////////////////  DEMO  /////////////////////

     #if 0 ///////////////  This case is picked out and it goes to case E3D_FLAT_SHADING, see 1A.  ////////////////////
			/* IF: No texture, apply E3D_FLAT_SHADING then */
			if( imgKd==NULL || triGroupList[n].ignoreTexture ) {  // textureImg==NULL ) { textureImg assigned to defMaterial.img_kd
			   /* Calculate light reflect strength for the TriFace:  TODO: not correct, this is ONLY demo. */
			   vProduct=gv_vLight*(triList[i].normal*nvRotMat);  // *(-1.0f); // -vLight as *(-1.0f);

			   /* TODO: gv_auxLight NOT applied yet */
			   if(IsBackface) vProduct=-vProduct;

			   /* If Backfacing to gv_vLight */
			   if( vProduct > -VPRODUCT_NEARZERO ) {  /* >0.0f */
				vProduct=0.0f;
				BackFacingLight=true;
			   }
			   else { /* Flip to get vProduct absolute value for luma */
				vProduct=-vProduct;
				BackFacingLight=false;
			   }

			   /* Adjust luma for pixcolor as per vProduct. */
			   color=mcolor;
			   fbdev->pixcolor=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +120 ); //50 );

			   #if 0 /* Fill the TriFace */		/* MidasHK_2022_02_09 */
			   draw_filled_triangle(fbdev, pts);
			   #else /* pixZ applys */
			   draw_filled_triangle2(fbdev,pts[0].x, pts[0].y,
						    pts[1].x, pts[1].y,
						    pts[2].x, pts[2].y,
						    /* Views from -z ----> +z  */
						    -vpts[0].z, -vpts[1].z, -vpts[2].z );
			   #endif

			}
			/* ELSE: Apply texture map */
			else
     #endif  //////////////////////////////////////////////////////////
		 /*  Keep/Need this braced block, to define local variables of vpd1,vpd2,vpd3  */
			   /* Calculate light reflect strength for the TriFace:  TODO: not correct, this is ONLY demo.  */

		       #if 0  /* NO auxiliary light */
			   vProduct=gv_vLight*(triList[i].normal*nvRotMat);  // *(-1.0f); // -vLight as *(-1.0f);
			   /* If Backfacing to gv_vLight */
			   if( vProduct > -VPRODUCT_NEARZERO ) {
				vProduct=0.0f;
				BackFacingLight=true;
			   }
			   else { /* Flip to get vProduct absolute value for luma */
				vProduct=-vProduct;
				BackFacingLight=false;
			   }
		       #else /* Use auxiliary light */
			   tn=triList[i].normal*nvRotMat;

			   /* Normal flips for backface */
		   	   if(IsBackface) tn=-tn;

			   vpd1=gv_vLight*tn;
			   vpd2=0.6*gv_auxLight*tn;
			   vpd3=0.25*upLight*tn;

			   /* Get main vProduct */
			   vProduct=mat_min3f(vpd1, vpd2, vpd3);

			   /* If Backfacing to gv_vLight */
			   if( vProduct > -VPRODUCT_NEARZERO ) {
				vProduct=0.0f;
				BackFacingLight=true;
			   }
			   else { /* Flip to get vProduct absolute value for luma */
				vProduct=-vProduct;
				BackFacingLight=false;
			  }
		       #endif

			   /* Adjust side luma according to vProudct. TODO: This is for DEMO only. */
			   //fbdev->lumadelt=(vProduct-0.75)*100+50;
			   fbdev->lumadelt=(vProduct-1.0)*200+50;
			   //egi_dpstd("lumadelt=%d\n", fbdev->lumadelt);

				/* Map texture.
				 *	  !!! --- CAUTION --- !!!
				 *  Noticed that UV ORIGIN is different!
				 * EGI: uv ORIGIN at Left_TOP corner
				 * 3DS: uv ORIGIN at Left_BOTTOM corner.
				 */
			   #if 0  /* OPTION_1: Matrix Mapping. TODO: cube shows a white line at bottom side!?? */
		        	egi_imgbuf_mapTriWriteFB(imgKd, fbdev,
					/* u0,v0,u1,v1,u2,v2,  x0,y0,z0,  x1,y1,z1, x2,y2,z2 */
        	                        triList[i].vtx[0].u, 1.0-triList[i].vtx[0].v, /* 1.0-x: Adjust uv ORIGIN */
                	                triList[i].vtx[1].u, 1.0-triList[i].vtx[1].v,
                        	        triList[i].vtx[2].u, 1.0-triList[i].vtx[2].v,
                                	pts[0].x, pts[0].y, 1,  /* NOTE: z values NOT to be 0/0/0!  */
	                                pts[1].x, pts[1].y, 2,
        	                        pts[2].x, pts[2].y, 3
                	        );
			  #elif 0 /* OPTION_2: Barycentric coordinates mapping, FLOAT type x/y. */
		        	egi_imgbuf_mapTriWriteFB2(imgKd, fbdev,
					/* u0,v0,u1,v1,u2,v2,  x0,y0, x1,y1, x2,y2 */
        	                        triList[i].vtx[0].u, 1.0-triList[i].vtx[0].v,  /* 1.0-x: Adjust uv ORIGIN */
                	                triList[i].vtx[1].u, 1.0-triList[i].vtx[1].v,
                        	        triList[i].vtx[2].u, 1.0-triList[i].vtx[2].v,
                                	pts[0].x, pts[0].y,
	                                pts[1].x, pts[1].y,
        	                        pts[2].x, pts[2].y
                	        );
			  #else /* OPTION_3: Barycentric coordinates mapping, INT type x/y. TODO: to apply vpts[] */
				#if 0 /* without z0,z1,z2 */
		        	egi_imgbuf_mapTriWriteFB3(imgKd, fbdev,
					/* u0,v0,u1,v1,u2,v2,  x0,y0, x1,y1, x2,y2 */
        	                        triList[i].vtx[0].u, 1.0-triList[i].vtx[0].v,  /* 1.0-x: Adjust uv ORIGIN */
                	                triList[i].vtx[1].u, 1.0-triList[i].vtx[1].v,
                        	        triList[i].vtx[2].u, 1.0-triList[i].vtx[2].v,
                                	pts[0].x, pts[0].y,
	                                pts[1].x, pts[1].y,
        	                        pts[2].x, pts[2].y
                	        );
				#else /* with z0,z1,z2 */
				//egi_dpstd("mapTriWriteFB3...\n");
		        	egi_imgbuf_mapTriWriteFB3(imgKd, fbdev,
					/* u0,v0,u1,v1,u2,v2,  x0,y0, x1,y1, x2,y2, z0,z1,z2 */
        	                        triList[i].vtx[0].u, 1.0-triList[i].vtx[0].v,  /* 1.0-x: Adjust uv ORIGIN */
                	                triList[i].vtx[1].u, 1.0-triList[i].vtx[1].v,
                        	        triList[i].vtx[2].u, 1.0-triList[i].vtx[2].v,
                                	pts[0].x, pts[0].y,
	                                pts[1].x, pts[1].y,
        	                        pts[2].x, pts[2].y,
				        /* Views from -z ----> +z  */
				        -vpts[0].z, -vpts[1].z, -vpts[2].z
                	        );
				#endif
			  #endif

			/* Reset lumadelt */
			fbdev->lumadelt=0;
#else //////////////  E3D_shadeTriangle( ) /////////////////////

     #if 0	   /* TEST: ----- If backface, then apply FLAT_SHADING */
		   if(IsBackface) {
			/* Compute lighting to get color */
			fbdev->pixcolor=E3D_seeColor(tgmtl, trinormal, 0); /* material, normal, dl */

			/* Fill the TriFace */
			draw_filled_triangle2(fbdev, pts[0].x, pts[0].y,
						     pts[1].x, pts[1].y,
						     pts[2].x, pts[2].y,
						    /* Views from -z ----> +z  */
						    -vpts[0].z, -vpts[1].z, -vpts[2].z );  /* Pseudo-depth */
		   }
		   else
    #endif
  		       E3D_shadeTriangle(fbdev, rvtx, tgmtl, scene); /* fbdev, rvtx[3], mtl, scene */

#endif //////////////////////////////////////////////////////////////

			break;

		   default:
			egi_dpstd("Unknown actShadeType: %d\n", actShadeType);
			break;

		} /* End switch */

#if 1  /* TEST: -------- To create shadow by backward ray tracing (See 4. for its drawbacks)
 * Note:
 *  1. Any trimesh backfacing to the gv_vLight is ignored, since they are alreay darksome.
 *  2. For Persepective View: It does NOT consider depth(z) mapping, so z values will be incorrect,
 *     which makes shadow deviate from the right position!
       For ISO view, it's much better.
 *  3. As pixelate_filled_triangle2() use integer (not float type) for XY, it brings errors,
 *     and reverse_projectPoints() will again scale up those errors for corresponding global XYZ coordinates. ??? XXX
 *  4. Here it backtraces each pixel on a triangle, and between 2 triangles, pixels(or even the whole line) may be overlapped on the seam,
 *     it wil be brighter with doubled pixalpha. (Even if we set fbdev->zbuff_IgnoreEqual=true.)
 *  5. To avoid 4, it's better to backtrace pixels on the screen directly, one by one. well that's sort of Global Illumination.
 */
if( testRayTracing==true &&  BackFacingLight==false
    && ( shadeType==E3D_FLAT_SHADING || shadeType==E3D_GOURAUD_SHADING || shadeType==E3D_TEXTURE_MAPPING ) )
{
		bool tested=false;

		/* Malloc pXYZ. !!! CAUTION:  DONT forget to free !!! */
		if(pXYZ==NULL) {
			pXYZ=(float *)malloc(capacity*sizeof(float));
			if(pXYZ==NULL) continue;
		}

		/* Pixelate a PROJECTED triangle mesh */
		pixelate_filled_triangle2( pts[0].x, pts[0].y,
					   pts[1].x, pts[1].y,
					   pts[2].x, pts[2].y,
                                        /* Views from -z ----> +z  */
                                        -vpts[0].z, -vpts[1].z, -vpts[2].z,
					   &pXYZ, &capacity, &np
					 );
		egi_dpstd("pixelate np=%zu\n", np);

		/* Overwrite pixels with equal zbuff */
		fbdev->zbuff_IgnoreEqual=false;

		/* Set shadow color */
		fbdev->pixcolor=WEGI_COLOR_DARKGRAY;
		fbdev->pixalpha=130;
		fbdev->pixalpha_hold=true;

		/* Backward tracing each pixel to gv_vLight */
		for(j=0; j<(int)np; j++) {

			/* A pixel point on screen COORD */
			vpt.x=pXYZ[3*j];  vpt.y=pXYZ[3*j+1]; vpt.z=-pXYZ[3*j+2]; /* remind that pXYZ.z already changed to be -z */

			/* Pick out pixels out of screen */
			if(vpt.x>fbdev->pos_xres || vpt.x<0.0 || vpt.y>fbdev->pos_yres || vpt.y<0.0 )
				continue;


			/* Reverse_project vpt from screen to object: TODO: some error here caused by integer type pixz */
	                if( reverse_projectPoints(&vpt, 1, projMatrix) !=0 ) {
				egi_dpstd("Fail to reverse_projectPoints!\n");
                        	continue;
			}

			/* Update Back tracing ray */
			vray.vp0 = vpt;
			vray.vd  = -gv_vLight;

			/* If hit any trimesh, draw a shadow dot on screen coord. */
			if( this->rayHitFace(vray, n,i, vphit, gindx, tindx, fn) ) { /* n--current trigroup, i--current triangle */
			#if 1	/* Though rayHitFace() picks out the triangle to which vray.vp0 belongs, sill there are cases
				 * that vray.vp0 and hitpoint coincide, Example: vray.vp0 is on the common edge of two faces which are
				 * vertical to each other.
 			         */
				/* Note: HK2022-09-03
				 * Since reverse_projectPoints() will scale up computation error, etc. roundf() a float to an integer.
				 * 1 pixel distance in screen coord may be much bigger in global coord.
				 */
				float dist0=E3D_vector_distance(vphit, vray.vp0);
				dist = dist0/( fabsf(vphit.z)/projMatrix.dv );
				if(dist<1.0) {
					egi_dpstd(DBG_YELLOW"dist0=%f, projected hit dist<1.0!\n"DBG_RESET, dist0);
				}
				else
			#endif  /* vray.vp0 plane is picked out in rayHitFace */
				{
					/* Note:
					 * Between 2 triangles, there may be overlapped pixels, which will be brighter with doubled pixalpha.
					 */
					//fbdev->pixcolor=WEGI_COLOR_DARKGRAY; // see above 'Set shadow color'
					//fbdev->pixalpha=120;  //However, MUST reset each time before draw_dot() without pixalpha_holdon
					fbdev->pixz=roundf(pXYZ[3*j+2]); /* NOT roundf(-vpt.z)! we need presudo_Z here! */
					draw_dot(fbdev, pXYZ[3*j], pXYZ[3*j+1]); /* shadow dot, pXYZ on screen COORD */

					//gv_vLight.print("vLight");
				   	//egi_dpstd("vLight_to_ray angle: %fDeg\n", E3D_vector_angleAB(vray.vp0-vphit, gv_vLight)*180/MATH_PI);

					#if 0 /* TEST:------- show rays */
					if(!tested) {
				   	   egi_dpstd("vray.vd_trinormal angle: %fDeg\n", E3D_vector_angleAB(vray.vd, trinormal)*180/MATH_PI);

					   vphit.print("vphit");
					   fbdev->pixcolor=WEGI_COLOR_RED;
					   fbdev->flipZ=true;
					   E3D_draw_line(fbdev, vray.vp0, vphit, projMatrix);
					   fbdev->flipZ=false;
					   tested=true;
					}
				        #endif
				}
			}
			//if( this->rayBlocked(vray) ) {
			else {
				//egi_dpstd("NOT intersect!\n");
				//fbdev->pixcolor=WEGI_COLOR_BLACK;
				//fbdev->pixz=-roundf(vpt.z); /* look from -z ----> +z */
				//draw_dot(fbdev, pXYZ[3*j], pXYZ[3*j+1]);
			}
		}

		/* Reset zbuff_IgnoreEqual */
		fbdev->zbuff_IgnoreEqual=true;
		/* Reset pixalpha_hold */
		fbdev->pixalpha_hold=false;
}
#endif /* END: ---------- back_tracing shadow */

		/* Count total triangles rendered actually */
		RenderTriCnt++;

	} /* END for(j) 4.A Rendering clipped triangles */

	   } /* End for(i) traverse trigroup::triangles */

	} /* End G2.  for(n)  traverse trigroups */

        #if 0	/* XXX Draw face normal line. --- See E3D_TriMesh::drawNormal() */
	if( faceNormalLen>0 ) {
		for(i=0; i<tcnt; i++) {
			//float nlen=20.0f;   /* Normal line length */
			cpt.x=(vpts[0].x+vpts[1].x+vpts[2].x)/3.0f;
			cpt.y=(vpts[0].y+vpts[1].y+vpts[2].y)/3.0f;
			//cpt.z=(vpt[0].z+vpt[1].z+vpt[2].z)/3.0f;
			fbset_color2(fbdev, WEGI_COLOR_GREEN);
			draw_line(fbdev, cpt.x, cpt.y,
					 cpt.x+faceNormalLen*(triList[i].normal.x),
					 cpt.y+faceNormalLen*(triList[i].normal.y) );
		}
	}
	#endif

	/* P1. Restore pixcolor */
	fbset_color2(&gv_fb_dev, color);

	/* P2. Free */
	free(pXYZ);

	/* P3. TODO:  reset tgmtl.img_kd to NULL, NOT the Ownership */
	tgmtl.img_kd=NULL;

	/* P4. Return totally triangles rendered actually */
	egi_dpstd(DBG_YELLOW"RenderTriCnt=%d\n"DBG_RESET, RenderTriCnt);
	return RenderTriCnt;
}


/*-------------------------------------------------------------------
To draw face normal for each trimesh.

@fbdev:		Pointer to FBDEV
@projMatrix	Projection Matrix
--------------------------------------------------------------------*/
void E3D_TriMesh::drawNormal(FBDEV *fbdev, const E3DS_ProjMatrix &projMatrix) const
{
	unsigned int i,n;
	E3D_Vector  vpts[3];	/* 3D points */
	E3D_Vector  trinormal;  /* Face normal of a triangle, after transformation */

	E3D_RTMatrix  tgRTMat;  /* TriGroup RTMatrix, for Pxyz(under TriGroupCoord)*TriGroup--->object */
	E3D_Vector    tgOxyz;   /* TriGroup origin/pivot XYZ */
	E3D_RTMatrix  nvRotMat; /* RTmatrix(rotation component only) for face normal rotation, to be zeroTranslation() */

	E3D_Vector  npts[2];    /* Normal line end points */

	/* Traverse */
	for(n=0; n<triGroupList.size(); n++) {

		/* 1. Check visibility */
		if(triGroupList[n].hidden) {
			egi_dpstd("triGroupList[%d]: '%s' is set as hidden!\n", n, triGroupList[n].name.c_str());
			continue;
		}

//		egi_dpstd("Check triGroupList[%d]: '%s'\n", n, triGroupList[n].name.c_str());

		/* 2. Extract TriGroup rotation matrix and rotation pivot */
		/* 2.1 To combine RTmatrix: Pxyz(under TriGroup Coord)*TrGroup-->object*object--->Global */
		tgRTMat=triGroupList[n].omat*objmat;

		/* 2.2 Face normal RotMat */
		nvRotMat = tgRTMat;
		nvRotMat.zeroTranslation();

		/* 2.3 Get TriGroup pivot, under object COORD */
		tgOxyz=triGroupList[n].pivot;

		/* 3. Traverse all triangles in the triGroupList[n] */
		for(i=triGroupList[n].stidx; i<triGroupList[n].etidx; i++) {
			/* 3.0 Cal. trinormal */
			trinormal=triList[i].normal*nvRotMat;

			/* 3.1 Copy triangle vertices */
			vpts[0] = vtxList[ triList[i].vtx[0].index ].pt;
			vpts[1] = vtxList[ triList[i].vtx[1].index ].pt;
			vpts[2] = vtxList[ triList[i].vtx[2].index ].pt;

			/* 3.2 TriGroup transform */
			/* 3.2.1 vpts under TriGroup COORD/pivot */
			vpts[0] -= tgOxyz;
			vpts[1] -= tgOxyz;
			vpts[2] -= tgOxyz;
			/* 3.2.2 Transform vpts[]: TriGroup---->Object; object--->Global */
			E3D_transform_vectors(vpts,3,tgRTMat);

			/* 3.3 Get normal line ends */
			npts[0]=(vpts[0]+vpts[1]+vpts[2])/3.0f;
			npts[1]=npts[0]+faceNormalLen*trinormal;

			/* 3.4 Project to screen/view plane */
                	if( projectPoints(npts, 2, projMatrix) !=0 )
                        	continue;

			/* 3.5 Draw normal line */
			fbset_color2(fbdev, WEGI_COLOR_GREEN);
			gv_fb_dev.flipZ=true;
			E3D_draw_line(fbdev, npts[0], npts[1]);
			gv_fb_dev.flipZ=false;

		} /* END for(i) */

	} /* END for(n) */
}


/*--------------------------------------------------------------------------
To create the mesh shadowMesh on to a plane, gv_vLight applys to calculate
the shadow.

@fbdev:		Pointer to FBDEV
@projMatrix	Projection Matrix
@plane:		The plane where shadow is created.
--------------------------------------------------------------------------*/
void E3D_TriMesh::shadowMesh(FBDEV *fbdev, const E3DS_ProjMatrix &projMatrix, const E3D_Plane &plane) const
{
	int k; //j
	unsigned int i,n;
	int vtidx[3];  				/* vtx index of a triangel */
	E3D_Vector  vpts[3];			/* Projected 3D points */
	EGI_POINT   pts[3];			/* Projected 2D points */
	E3D_Vector  trinormal;  		/* Face normal of a triangle */
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); 	/* View direction */
	float	    vProduct;   		/* dot product */
//	int deltLuma;
	bool	    forward;			/* intersection point position relative to Ray vp0 */


//	bool	    IsBackface=false;		/* Viewing the back side of a Trimesh. */
//	E3D_POINT cpt;	    			/* Gravity center of triangle */

	/* render color */
	EGI_16BIT_COLOR color=COLOR_DimGray; /* Shadow color */

	/* RTMatrixes for the Object and TriGroup */
	E3D_RTMatrix	tgRTMat; //tgRotMat; /* TriGroup RTmatrix, for Pxyz(at TriGroup Coord) * TriGroup--->object */
	E3D_Vector	tgOxyz;		   /* TriGroup origin/pivot XYZ */
	E3D_RTMatrix	nvRotMat;	   /* RTmatrix(rotation component only) for face normal rotation, to be zeroTranslation() */

//	objmat.print("objmat");

#if 1	/* Check view direction to plane normal */
	vProduct = vView*plane.vn;
	if ( vProduct > VPRODUCT_EPSILON ) {
		egi_dpstd(DBG_RED"Plane backface to vView!\n"DBG_RESET);
		return;
	}

	/* Check lighting direction to plane normal */
	vProduct = plane.vn*gv_vLight;
	if( vProduct > VPRODUCT_EPSILON ) {
                egi_dpstd(DBG_RED"Plane backface to gv_vLight!\n"DBG_RESET);
                return;
        }
#endif

	/* Set shadow color */
	fbdev->pixcolor=color;

	/* Compute and draw shadow triangles of triGroupList onto the plane, under gv_vLight. */
	for(n=0; n<triGroupList.size(); n++) {  /* Traverse all Trigroups */

	    /* F1. Extract TriGroup rotation matrix and rotation pivot */

	    /* F1.1 To combine RTmatrix:  Pxyz(at TriGroup Coord) * TriGroup--->object * object--->Global */
	    tgRTMat = triGroupList[n].omat*objmat;

	    /* F1.2 Light RotMat */
	    nvRotMat = tgRTMat;
 	    nvRotMat.zeroTranslation(); /* !!! */

	    /* F1.3 TriGroup (original) pivot, under object COORD. */
	    tgOxyz = triGroupList[n].pivot;

	    /* F2. Traverse and render all triangles in the triGroupList[n]
	     *  TODO: sorting, render Tris from near to far field.... */
	    for(i=triGroupList[n].stidx; i<triGroupList[n].etidx; i++) {

		/* F2.1 Copy triangle vertices, and project to viewPlan COORD. */
		vtidx[0]=triList[i].vtx[0].index;
		vtidx[1]=triList[i].vtx[1].index;
		vtidx[2]=triList[i].vtx[2].index;

		vpts[0]=vtxList[vtidx[0]].pt;
		vpts[1]=vtxList[vtidx[1]].pt;
		vpts[2]=vtxList[vtidx[2]].pt;

#if 1		/* F2.2 TriGroup local Rotation computation. */
		/* F2.2.1 vpts under TriGroup COORD/Pivot */
		vpts[0] -= tgOxyz;
		vpts[1] -= tgOxyz;
		vpts[2] -= tgOxyz;

		/* F2.2.2 Transform vpts[]: TriGroup--->object; object--->Global */
		E3D_transform_vectors(vpts, 3, tgRTMat);

     #if 0  /* XXX Back to under object/global coord. XXX */
		vpts[0] += tgOxyz;
		vpts[1] += tgOxyz;
	   	vpts[2] += tgOxyz;
     #endif
#endif

		/* F2.3 To calculate intersection points as gv_vLight projecting vpts[] to the plane */
//		bool allforward=true;
		for(k=0; k<3; k++) {
			/* F2.3.1 Create Ray light */
			E3D_Ray  Ray(vpts[k], gv_vLight);

			/* F2.3.2 Get intersection of Ray to the plane */
			E3D_Vector Vinsct;
			E3D_RayIntersectPlane(Ray, plane, Vinsct, forward);
			/* Check intersection position */
			//if(!forward) {
			//	allforward=false;
			//	break;
			//}

			vpts[k] = Vinsct;
		}

		/* NOPE! Here only consider parallel lighting, from infinite distance.  */
		//if(allforward==false) {
		//	egi_dpstd("allforward==false!\n");
		//	continue;
		//}

		/* F2.4 Project trimesh to screen/view plane */
		if( projectPoints(vpts, 3, projMatrix) !=0 )
			continue;

#if 0	/* -------------- TEST: Check range, ONLY simple way.
		 * Ignore the triangle if all 3 vpts are out of range.
		 * TODO: by projectPoints()
		 */
		for( j=0; j<3; j++) {
		     if(  (vpts[j].x < 0.0 || vpts[j].x > projMatrix.winW-1 )
		          || (vpts[j].y < 0.0 || vpts[j].y > projMatrix.winH-1 ) )
			continue;
		     else
			break;
		}
		/* If all 3 vertices out of win, then ignore this triangle. */
		if(j==3) {
		     //printf("Tri %d OutWin.\n", i);
		     continue;
		}
#endif

		/* F2.5 Get 2D points on the screen/view plane  */
		pts[0].x=roundf(vpts[0].x); pts[0].y=roundf(vpts[0].y);
		pts[1].x=roundf(vpts[1].x); pts[1].y=roundf(vpts[1].y);
		pts[2].x=roundf(vpts[2].x); pts[2].y=roundf(vpts[2].y);
		// pts[].z = vpts[].z,  NOT assigned here.

#if 0		/*  ---------TEST: Set pixz zbuff. Should init zbuff[] to INT32_MIN: -2147483648, see in caller's main(). */
		/* A simple test: Triangle center point Z for all pixles on the triangle.  TODO ....*/
		//gv_fb_dev.pixz=roundf((vtxList[vtidx[0]].pt.z+vtxList[vtidx[1]].pt.z+vtxList[vtidx[2]].pt.z)/3.0);
		fbdev->pixz=roundf((vpts[0].z+vpts[1].z+vpts[2].z)/3.0);
		/* !!!! Views from -z ----> +z */
		fbdev->pixz = -fbdev->pixz;
#endif

		/* F2.6 Draw shadow triangle */
		#if 0 /* Fill the TriFace */
		draw_filled_triangle(fbdev, pts);
		#else /* pixZ applys */
		draw_filled_triangle2(fbdev,pts[0].x, pts[0].y,
					    pts[1].x, pts[1].y,
					    pts[2].x, pts[2].y,
					    /* Views from -z ----> +z  */
					    -vpts[0].z, -vpts[1].z, -vpts[2].z );
		#endif

	   } /* End for(i) */

	} /* End for(n) */
}


/*--------------------------------------------------------------------
Render/draw the whole mesh by filling all triangles with FBcolor.

Note:
1. View direction: from -z ----> +z, of Veiw_Coord Z axis.
2. Each triangle in mesh is transformed as per VRTMatrix+ScaleMatrix, and
   then draw to FBDEV, TriMesh data keeps unchanged!
3. !!! CAUTION !!!
   3.1 ScaleMatrix does NOT apply to triangle normals!
   3.2 Translation component in VRTMatrix is ignored for triangle normals!

View_Coord: 	Transformed from same as Global_Coord by RTmatrix.
View direction: Frome -z ---> +z, of View_Coord Z axis.

@fbdev:		Pointer to FB device.
@VRTmatrix	View_Coord tranform matrix, from same as Global_Coord.
		OR: View_Coord relative to Global_Coord.
		NOT combined with scaleMatrix here!
@ScaleMatrix    Scale Matrix.
---------------------------------------------------------------------*/
void E3D_TriMesh::renderMesh(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix, const E3D_RTMatrix &ScaleMatrix) const
{
	E3D_RTMatrix CompMatrix; /* Combined with VRTMatrix and ScaleMatrix */

        float xx;
       	float yy;
       	float zz;

	EGI_POINT   pts[3];

	E3D_Vector  vView(0.0f, 0.0f, 1.0f); /* View direction */
	float	    vProduct;   /* dot product */

	/* For EACH traversing triangle */
	int vtidx[3];  		/* vtx index of a triangle, as of triList[].vtx[0~2].index */
	E3D_Vector  trivtx[3];	/* 3 vertices of transformed triangle. */
	E3D_Vector  trinormal;  /* Normal of transformed triangle */

	/* Default color and Y */
	EGI_16BIT_COLOR color=fbdev->pixcolor;
	//EGI_8BIT_CCODE  codeY=egi_color_getY(color);

	/* Final transform Matrix for vertices */
	CompMatrix.identity();
	CompMatrix = VRTMatrix*ScaleMatrix;

	/* NOTE: Set fbdev->flipZ, as we view from -z---->+z */
	fbdev->flipZ=true;

	/* -------TEST: Project to Z plane, Draw X,Y only */
	/* Traverse and render all triangles */
	for(int i=0; i<tcnt; i++) {
		/* 1. Get indices of 3 vertices to define current triangle */
		vtidx[0]=triList[i].vtx[0].index;
		vtidx[1]=triList[i].vtx[1].index;
		vtidx[2]=triList[i].vtx[2].index;

		/* 2. Transform 3 vertices of the triangle to under View_Coord */
        	for( int k=0; k<3; k++) {
                	xx=vtxList[ vtidx[k] ].pt.x;
	                yy=vtxList[ vtidx[k] ].pt.y;
        	        zz=vtxList[ vtidx[k] ].pt.z;

			/* Get transformed trivtx[] under View_Coord, with scale matrix! */
			/* With Scale matrix */
                	trivtx[k].x = xx*CompMatrix.pmat[0]+yy*CompMatrix.pmat[3]+zz*CompMatrix.pmat[6] +CompMatrix.pmat[9];
	                trivtx[k].y = xx*CompMatrix.pmat[1]+yy*CompMatrix.pmat[4]+zz*CompMatrix.pmat[7] +CompMatrix.pmat[10];
        	        trivtx[k].z = xx*CompMatrix.pmat[2]+yy*CompMatrix.pmat[5]+zz*CompMatrix.pmat[8] +CompMatrix.pmat[11];

			#if TEST_PERSPECTIVE  /* Perspective matrix processing */
			if(trivtx[k].z <10) {
				egi_dpstd("Point Z too samll, out of the Frustum?\n");
				return;
			}
			trivtx[k].x = trivtx[k].x/trivtx[k].z*300;  /* Xv=Xs/Zs*d */
			trivtx[k].y = trivtx[k].y/trivtx[k].z*300;  /* Yv=Ys/Zs*d */
			#endif
        	}

		/* 3. Transforme normal of the triangle to under View_Coord */
		/* Note:
		 *	1. -1() to flip XYZ to fit Screen coord XY
		 *	2. Normal should NOT apply ScaleMatrix!!
		 *         If scaleX/Y/Z is NOT same,  then ALL normals to be recalcualted/updated!
	 	 */
		xx=triList[i].normal.x;
		yy=triList[i].normal.y;
		zz=triList[i].normal.z;
		trinormal.x = xx*VRTMatrix.pmat[0]+yy*VRTMatrix.pmat[3]+zz*VRTMatrix.pmat[6]; /* Ignor translation */
		trinormal.y = xx*VRTMatrix.pmat[1]+yy*VRTMatrix.pmat[4]+zz*VRTMatrix.pmat[7];
		trinormal.z = xx*VRTMatrix.pmat[2]+yy*VRTMatrix.pmat[5]+zz*VRTMatrix.pmat[8];

		/* 4. Pick out back_facing triangles.  */
		vProduct=vView*trinormal; // *(-1.0f);  /* !!! -vView as *(-1.0f) */
		if ( vProduct >= 0.0f )
                        continue;

		/* 5. Calculate light reflect strength, a simple/approximate way.  TODO: not correct, this is ONLY demo.  */
		vProduct=gv_vLight*trinormal;  // *(-1.0f); // -vLight as *(-1.0f);
		if( vProduct > 0.0f )
			vProduct=0.0f;
		else	/* Flip to get vProduct absolute value for luma */
			vProduct=-vProduct;

		/* 6. Adjust luma for pixcolor as per vProduct. */
		fbdev->pixcolor=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
		//egi_dpstd("vProduct: %e, LumaY: %d\n", vProduct, (unsigned int)egi_color_getY(fbdev->pixcolor));

		/* 7. Get triangle pts. */
		for(int k=0; k<3; k++) {
			pts[k].x=roundf(trivtx[k].x);
			pts[k].y=roundf(trivtx[k].y);
		}

		/* 5. ---------TEST: Set pixz zbuff. Should init zbuff[] to INT32_MIN: -2147483648 */
		/* A simple ZBUFF test: Triangle center point Z for all pixles on the triangle.  TODO ....*/
		gv_fb_dev.pixz=roundf((trivtx[0].z+trivtx[1].z+trivtx[2].z)/3.0);

		/* !!!! Views from -z ----> +z   */
		//gv_fb_dev.pixz = -gv_fb_dev.pixz; /* OR set fbdev->flipZ=true */

		/* 6. Draw triangle */
		//draw_triangle(&gv_fb_dev, pts);
		draw_filled_triangle(&gv_fb_dev, pts);
	}

	/* Reset fbdev->flipZ */
	fbdev->flipZ=false;

	/* Restore pixcolor */
	fbset_color2(&gv_fb_dev, color);
}


/*----------------------------------------------------
Read Vertices and other information from an obj file.
All faces are divided into triangles.

@vtxNormalCount:	Normal list count.
@textureVtxCount:	Texture uv count;

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
				        int & vtxNormalCount, int & textureVtxCount, int & faceCount)
{
	ifstream	fin;
	char		ch;
	int		fvcount;	/* Count vertices on a face */

	/* Init vars */
	vertexCount=0;
	triangleCount=0;
	vtxNormalCount=0;
	textureVtxCount=0;
	faceCount=0;

	/* Read statistic of obj data */
	fin.open(fpath);
	if(fin.fail()) {
		egi_dpstd("Fail to open '%s'.\n", fpath);
		return -1;
	}

	while(!fin.eof() && fin.get(ch) ) {
#if 0 /* TEST: ------ */
		egi_dpstd("ch: %c\n",ch);
#endif
		switch(ch) {
		   case 'v':	/* Vertex data */
			fin.get(ch);
			switch(ch) {
			   case ' ':
				vertexCount++;
				break;
			   case 't':
				textureVtxCount++;
				break;
			   case 'n':
				vtxNormalCount++;
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
				   while( ch==' ' ) {
					if(fin.eof()) break;
					fin.get(ch);
				   }
				   /* Some obj txt may have '\r' as line end. */
				   if( ch== '\n' || ch=='\r' || fin.eof() ) break; /* break while() */
				   /* F.2 If is non_BLANK char */
				   fvcount ++;
				   /* F.3 Get rid of following non_BLANK char. */
				   while( ch!=' ') {
					fin.get(ch);
					if(ch== '\n' || ch=='\r' || fin.eof() )break;
				   }
				   if( ch== '\n' || ch=='\r' || fin.eof() )break; /* break while() */
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
			while( ch != '\n' ) {
				if(fin.eof()) break;
				fin.get(ch);
			}
			break;
		}

	} /* End while() */

	/* Close file */
	fin.close();

	egi_dpstd("Statistics:  Vertices %d;  vtxNormals %d;  TextureVtx %d; Faces %d; Triangle %d.\n",
				vertexCount, vtxNormalCount, textureVtxCount, faceCount, triangleCount );

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
4 trianlges, and its radiu keeps the same size.
Here, the most simple regular sphere is an icosahedron!

@smesh:	A standard regular sphere_mesh derived from an icosahedron.
@r:	Radius of the icosahedron(smesh), MUST be same as smesh.

-------------------------------------------------------------*/
E3D_TriMesh::E3D_TriMesh(const E3D_TriMesh &smesh, float r)
{
	int k;

	/* Test: V-F/2=2 */
	if(smesh.vcnt-smesh.tcnt/2 !=2) {
		egi_dpstd(DBG_RED"Input smesh is NOT a legal mesh derived from an icosahedron!\n"DBG_RESET);
		return;
	}

	/* Init vars */
	initVars();

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
		/* ..Check if vNA already in vtxList[]. let NAidx=j then.
		 * Note: The edge of the added vertex is shared by two old faces, check to avoid creating it twice.
		 */
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

		/* Add 4 triangles. tcnt init as 0,as no triangle copied from smesh. */
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


		 /*----------------------------------------------
       		        Class E3D_MeshInstance :: Functions
			      Instance of E3D_TriMesh.
		 ----------------------------------------------*/

/*-------------------------------------------
                The constructor.

         !!!--- CAUTION ---!!!
The referred triMesh SHOULD keep unchanged
(especially SHOULD NOT be rotated, and usually
its objmat is identity also),so its original XYZ
align with Global XYZ, as well as its AABB.
If NOT SO, you must proceed with it carefully.
--------------------------------------------*/
E3D_MeshInstance::E3D_MeshInstance(E3D_TriMesh &triMesh, string pname)
:name(pname), refTriMesh(triMesh)   /* Init order! */
{
	/* Assign name */
	//name=pname;

	/* Init instance objmat */
	objmat=refTriMesh.objmat;

	/* Init instance omats */
	for(unsigned int k=0; k<refTriMesh.triGroupList.size(); k++)
		omats.push_back(refTriMesh.triGroupList[k].omat);

	/* Increase counter */
	refTriMesh.instanceCount += 1;

	/* A default name, user can re-assign later */

	egi_dpstd("A MeshInstance is added!\n");
}

E3D_MeshInstance::E3D_MeshInstance(E3D_MeshInstance &refInstance, string pname)
:name(pname), refTriMesh(refInstance.refTriMesh)
{
	/* Assign name */
	//name=pname;

	/* Get refTriMesh */
	//refTriMesh=refInstance.refTriMesh; A reference member MUST be initialized in the initializer list.

	/* Init instance objmat */
	objmat=refInstance.objmat;

	/* Init instance omats */
	for(unsigned int k=0; k<refInstance.omats.size(); k++)
		omats.push_back(refInstance.omats[k]);

	/* Increase counter */
	refInstance.refTriMesh.instanceCount += 1;

	egi_dpstd("A MeshInstance is added!\n");
}

/*-----------------------------------------------------------------
Check whether AABB of the MeshInstance is out of the view frustum.

	      !!! --- CAUTION ---- !!!
It's assumed that AABB of the 'refTriMesh' are aligned
/parallel with XYZ axises.

TODO:
1. The MeshInstance is confirmed to be out of the frustum
   ONLY IF each vertex of its AABB is out of the SAME
   face of the frustum, However that is NOT necessary.
   Consider the case: a MeshInstance is near the corner of
   the frustum and is completely out of the frustum, vertices
   of its AABB may be NOT out of the SAME face of the furstum.

Return:
	True	Completely out of the view frustum.
	False   Ten to one it's in the view frustum.  see TODO_1.
-------------------------------------------------------------------*/
bool E3D_MeshInstance::aabbOutViewFrustum(const E3DS_ProjMatrix &projMatrix)
{
	E3D_Vector vpts[8];
	int code[8], precode;
	bool ret=true;

	/* Get 8 points of the AABB */
        float xsize=refTriMesh.aabbox.xSize();
        float ysize=refTriMesh.aabbox.ySize();
        float zsize=refTriMesh.aabbox.zSize();

        /* Get all vertices of the AABB: __ONLY_IF__ sides of AABB
	 * are all algined with XYZ axises.
	 */
        vpts[0]=refTriMesh.aabbox.vmin;
        vpts[1]=vpts[0]; vpts[1].x +=xsize;
        vpts[2]=vpts[1]; vpts[2].y +=ysize;
        vpts[3]=vpts[0]; vpts[3].y +=ysize;
        vpts[4]=vpts[0]; vpts[4].z +=zsize;
        vpts[5]=vpts[4]; vpts[5].x +=xsize;
        vpts[6]=vpts[5]; vpts[6].y +=ysize;
        vpts[7]=vpts[4]; vpts[7].y +=ysize;

	/* Transform as per MeshInstance.objmat, vpts updated. */
	E3D_transform_vectors(vpts, 8, objmat);

#if 0 /* TEST: ----- If OutOfViewFrustum --------- */
	E3D_Vector  pts[8];
	for(int k=0; k<8; k++) pts[k]=vpts[k];
#endif

	/* Map points to NDC coordinates, vpts updated. */
	mapPointsToNDC(vpts, 8, projMatrix);

	/* Check each vertex, see if they are beside and out of the SAME face of the frustum */
	 ////////////// Hi, Dude! :)  HK2022-10-10 //////////
	precode=0xFF;
	for(int k=0; k<8; k++) {
		code[k]=pointOutFrustumCode(vpts[k]);
		precode &= code[k];
	}
	if(precode) /* All aabb vertices out of a same side */
		ret=true;
	else
		ret=false;

	/* TODO: Further.... to check whether AABB intersects with frustum face planes.  */

#if 0 /* TEST: enable abov TEST also -------  If OutOfViewFrustum: Print pts coordinates and draw AABB ------- */
     if(ret==true) {
	egi_dpstd(" --- AABB OutViewFrustum ---\n");
	char strtmp[32];
	for(int k=0; k<8; k++) {
		sprintf(strtmp, "pts[%d]: ",k);
		pts[k].print(strtmp);
	}

        /* Project to viewPlan COORD. */
        projectPoints(pts, 8, projMatrix); /* Do NOT check clipping */

	FBDEV *fbdev=&gv_fb_dev;

        /* Notice: Viewing from z- --> z+ */
        fbdev->flipZ=true;

        /* E3D_draw lines 0-1, 1-2, 2-3, 3-0 */
        E3D_draw_line(fbdev, pts[0], pts[1]);
        E3D_draw_line(fbdev, pts[1], pts[2]);
        E3D_draw_line(fbdev, pts[2], pts[3]);
        E3D_draw_line(fbdev, pts[3], pts[0]);
        /* Draw lines 4-5, 5-6, 6-7, 7-4 */
        E3D_draw_line(fbdev, pts[4], pts[5]);
        E3D_draw_line(fbdev, pts[5], pts[6]);
        E3D_draw_line(fbdev, pts[6], pts[7]);
        E3D_draw_line(fbdev, pts[7], pts[4]);
        /* Draw lines 0-4, 1-5, 2-6, 3-7 */
        E3D_draw_line(fbdev, pts[0], pts[4]);
        E3D_draw_line(fbdev, pts[1], pts[5]);
        E3D_draw_line(fbdev, pts[2], pts[6]);
        E3D_draw_line(fbdev, pts[3], pts[7]);

        /* Reset flipZ */
        fbdev->flipZ=false;

     }
#endif

	return ret;
}


/*-----------------------------------------------------------------------
Check whether AABB of the MeshInstance is completely within the view frustum.

	      !!! --- CAUTION ---- !!!
It's assumed that AABB of the 'refTriMesh' are aligned
/parallel with XYZ axises.

Return:
	True	AABB completely within the view frustum.
	False
------------------------------------------------------------------------*/
bool E3D_MeshInstance::aabbInViewFrustum(const E3DS_ProjMatrix &projMatrix)
{
	E3D_Vector vpts[8];
	int code, precode;
	bool ret=true;

	/* Get 8 points of the AABB */
        float xsize=refTriMesh.aabbox.xSize();
        float ysize=refTriMesh.aabbox.ySize();
        float zsize=refTriMesh.aabbox.zSize();

        /* Get all vertices of the AABB: __ONLY_IF__ sides of AABB
	 * are all algined with XYZ axises.
	 */
        vpts[0]=refTriMesh.aabbox.vmin;
        vpts[1]=vpts[0]; vpts[1].x +=xsize;
        vpts[2]=vpts[1]; vpts[2].y +=ysize;
        vpts[3]=vpts[0]; vpts[3].y +=ysize;
        vpts[4]=vpts[0]; vpts[4].z +=zsize;
        vpts[5]=vpts[4]; vpts[5].x +=xsize;
        vpts[6]=vpts[5]; vpts[6].y +=ysize;
        vpts[7]=vpts[4]; vpts[7].y +=ysize;

	/* Transform as per MeshInstance.objmat, vpts updated. */
	E3D_transform_vectors(vpts, 8, objmat);

	/* Map points to NDC coordinates, vpts updated. */
	mapPointsToNDC(vpts, 8, projMatrix);

	/* Check each AABB vertex, see if they are all within the frustum */
	ret=true;
	for(int k=0; k<8; k++) {
		code=pointOutFrustumCode(vpts[k]);
		if(code) {
			ret=false;  /* Init ret=ture */
			break;
		}
	}

	return ret;
}


/*-------------------------
      The destructor.
--------------------------*/
E3D_MeshInstance:: ~E3D_MeshInstance()
{
	/* Decrease counter */
	refTriMesh.instanceCount -= 1;
}


/*--------------------------------------------------------------------------
Render MeshInstance.

Note:
1. MeshInstance.objmat prevails in renderInstance() by replacing it
   with this E3D_TRIMESH.objmat, and then calling E3D_TRIMESH::renderMesh().

TODO:
1. It's BAD to replace and restore refTriMesh.objmat.
   and this function SHOULD be a whole and complete rendering function instead
   of calling renderMesh().
2. E3D_MeshInstance.omats NOT applied yet.

@fbdev:	       Pointer to FBDEV
@projMatrix:   Projection Matrix.
---------------------------------------------------------------------------*/
void E3D_MeshInstance::renderInstance(FBDEV *fbdev, const E3DS_ProjMatrix &projMatrix) const
{
	/* Replace refTriMesh.objmat with  */
	E3D_RTMatrix tmpMat=refTriMesh.objmat;
	refTriMesh.objmat=this->objmat;

	/* TODO: E3D_MeshInstance.omats NOT applied yet. */

	/* Call renderTriMesh.*/
	refTriMesh.renderMesh(fbdev, projMatrix);

#if 1 /* TEST: ------ */
	refTriMesh.drawAABB(fbdev, objmat, projMatrix);
#endif

	/* Restore refTriMesh.obj */
	refTriMesh.objmat=tmpMat;
}


        	/*------------------------------------
            	    Class E3D_Scene :: Functions
        	-------------------------------------*/

/*-------------------------
      The constructor.
--------------------------*/
E3D_Scene::E3D_Scene()
{
}

/*-------------------------
      The destructor.
--------------------------*/
E3D_Scene::~E3D_Scene()
{
	/* delete mechanism will ensure ~E3D_TriMesh() be called for each item.  */
	for(unsigned int i=0; i<triMeshList.size(); i++) {
	    delete triMeshList[i];
	    //NOT necessary for ::fpathList[]

	    delete meshInstanceList[i];
	}
}

/*----------------------------------------------
Import TriMesh from an .obj file, then push into
TriMeshList[].

@fobj:  Obj file path.
----------------------------------------------*/
void E3D_Scene::importObj(const char *fobj)
{
	E3D_TriMesh *trimesh = new E3D_TriMesh(fobj);

	/* fobj file may be corrupted. */
	if( trimesh->vtxCount() >0) {
		/* Push to triMeshList */
		triMeshList.push_back(trimesh);
		/* Push to fpathList */
		fpathList.push_back(fobj);

		/* Create an instance and push to meshInstanceList */
		E3D_MeshInstance *meshInstance = new E3D_MeshInstance(*trimesh, cstr_get_pathFileName(fobj));
		meshInstanceList.push_back(meshInstance);
	}
	else {
		egi_dpstd(DBG_RED"Fail to import obj file '%s'!\n"DBG_RESET, fobj);
		delete trimesh;
	}
}

/*-------------------------------------------------------------
Add mesh instance to the list.

    	     !!! --- CAUTION --- !!!

The refTriMesh OR refInstance MUST all in E3D_Scene::triMeshList
/meshInstanceList!
---------------------------------------------------------------*/
void E3D_Scene::addMeshInstance(E3D_TriMesh &refTriMesh, string pname)
{
	E3D_MeshInstance *meshInstance = new E3D_MeshInstance(refTriMesh, pname); /* refTriMesh counter increased */
	meshInstanceList.push_back(meshInstance);

}
void E3D_Scene::addMeshInstance(E3D_MeshInstance &refInstance, string pname)
{
	E3D_MeshInstance *meshInstance = new E3D_MeshInstance(refInstance, pname); /* refInstance counter increase */
	meshInstanceList.push_back(meshInstance);
}

/*--------------------------------------
Clear meshInstanceList[]
--------------------------------------*/
void E3D_Scene::clearMeshInstanceList()
{
	/* Clear list item. */
	for(unsigned int k=0; k<meshInstanceList.size(); k++) {
			/* Decreased instance counter */
			//meshInstanceList[k]->refTriMesh.instanceCount -=1;	/* NOPE! To decrease in ~E3D_MeshInstance() */

			/* Destroy E3D_MeshInstance */
			delete meshInstanceList[k];
	}

	/* Clear list */
	meshInstanceList.clear();
}


/*-------------------------------------------------------------------
Render each mesh in the scene, and apply lights/settings.

Note:
		!!!--- CAUTION ---!!!
1. Before renderSecen, the caller SHOULD recompute clipping matrix
   items A,B,C,D...etc, in case any projMatrix parameter(such as dv)
   changed.

-------------------------------------------------------------------*/
void E3D_Scene::renderScene(FBDEV *fbdev, const E3DS_ProjMatrix &projMatrix) const
{
	int outcnt=0; /* Count instances out of the view frustum */

	/* 1. Lights/setting preparation */

	/* 2. Recompute clipping matrix items A,B,C,D.. if any projMatrix parameter(such as dv) changes */
	//XXX E3D_RecomputeProjMatrix(projMatrix); XXX NOT here!

	/* 3. Render each instance in the list */
	for(unsigned int k=0; k< meshInstanceList.size(); k++) {
		/* Clip Instance, and exclude instances which are out of view frustrum. */
		if( meshInstanceList[k]->aabbOutViewFrustum(projMatrix) ) {
			outcnt ++;
			egi_dpstd(DBG_ORCHID"Instance '%s' is out of view frustum!\n"DBG_RESET, meshInstanceList[k]->name.c_str());

		   #if 0  /* TEST: -------- Assign COLOR_HotPink to any instanceout out of frustum and render it */
			E3D_Vector saveKd;
			/* Save kd */
			saveKd=meshInstanceList[k]->refTriMesh.defMaterial.kd;
			/* Change kd as mark */
			meshInstanceList[k]->refTriMesh.defMaterial.kd.vectorRGB(COLOR_HotPink);
			/* Render instance */
			meshInstanceList[k]->renderInstance(fbdev, projMatrix);
			/* Restore kd */
			meshInstanceList[k]->refTriMesh.defMaterial.kd=saveKd;
		   #endif
		}
		else
		{
			/* Render instances within the view frustum */
			meshInstanceList[k]->renderInstance(fbdev, projMatrix);
		}
	}

	egi_dpstd(DBG_BLUE"Total %d instances are out of view frustum!\n"DBG_RESET, outcnt);
}


////////////////////////////////////  E3D_Draw Function  ////////////////////////////////////////

/*----------------------------------------------
Draw a 3D line between va and vb. zbuff applied.
@fbdev:	  Pointer to FBDEV.
@va,vb:	  Two E3D_Vectors as two points.
-----------------------------------------------*/
//inline cause error: undefined reference to `E3D_draw_line(fbdev*, E3D_Vector const&, E3D_Vector const&)'
void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb)  /* NO projection conversion, No flipZ */
{
	draw3D_line( fbdev, roundf(va.x), roundf(va.y), roundf(va.z),
		            roundf(vb.x), roundf(vb.y), roundf(vb.z) );
}

void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb, const E3DS_ProjMatrix &projMatrix)
{
	E3D_Vector vpts[2];
	vpts[0]=va;
	vpts[1]=vb;

	/* Project vpts */
	if( projectPoints(vpts, 2, projMatrix) !=0)
		return;

	draw3D_line( fbdev, roundf(vpts[0].x), roundf(vpts[0].y), roundf(vpts[0].z),
		            roundf(vpts[1].x), roundf(vpts[1].y), roundf(vpts[1].z) );
}

void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_RTMatrix &RTmatrix, const E3DS_ProjMatrix &projMatrix)
{
	E3D_Vector vpts[2];

//	vpts[0].zero();
	vpts[1]=va;

	vpts[0]=vpts[0]*RTmatrix;
	vpts[1]=vpts[1]*RTmatrix;

	/* Project vpts */
	if( projectPoints(vpts, 2, projMatrix) !=0)
		return;

	draw3D_line( fbdev, roundf(vpts[0].x), roundf(vpts[0].y), roundf(vpts[0].z),
		            roundf(vpts[1].x), roundf(vpts[1].y), roundf(vpts[1].z) );
}

void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb, const E3D_RTMatrix &RTmatrix, const E3DS_ProjMatrix &projMatrix)
{
	E3D_Vector vpts[2];

	vpts[0]=va*RTmatrix;
	vpts[1]=vb*RTmatrix;

	/* Project vpts */
	if( projectPoints(vpts, 2, projMatrix) !=0) {
		egi_dpstd(DBG_RED"projectPoints() error!\n"DBG_RESET);
		return;
	}

	/* Draw 3D line */
	draw3D_line( fbdev, roundf(vpts[0].x), roundf(vpts[0].y), roundf(vpts[0].z),
		            roundf(vpts[1].x), roundf(vpts[1].y), roundf(vpts[1].z) );
}


/*---------------------------------------------------
Draw a piece of 3D grid, origin at mid. of sx,sy.
A grid on XY plane then transform as per RTmatrix.

@fbdev:	  	Pointer to FBDEV.
@sx,sy:	  	Size of the grid, in pixels.
@us:		Square unit side size, in pixels.
@RTmatrix:	Transform matrix.
@projMatrix:	ProjMatrix
----------------------------------------------------*/
void E3D_draw_grid(FBDEV *fbdev, int sx, int sy, int us, const E3D_RTMatrix &RTmatrix, const E3DS_ProjMatrix &projMatrix)
{
	int err;
	E3D_Vector vpts[2]; /* Init all 0 */

	/* Check input, us!=0 HK2022-11-01 */
	if(sx<1 || sy<1 || us<1) {
		egi_dpstd(DBG_RED"Data error: sx=%d, sy=%d, us=%d\n"DBG_RESET, sx, sy, us);
		return;
	}

	/* Backup color */
//	EGI_16BIT_COLOR oldcolor=fbdev->pixcolor;

   /* Notice: Viewing from z- --> z+ */
   fbdev->flipZ=true;

	for(int i=-(sx/2/us); i<=(sx/2/us); i++) {
		/* Tow points of a line */
		vpts[0].x = i*us;
		vpts[1].x = i*us;
		vpts[0].y = -(sy/2/us)*us;
		vpts[1].y = (sy/2/us)*us;
		vpts[0].z = 0.0;   /* ON XY plane */
		vpts[1].z = 0.0;

		/* Transform */
		vpts[0]=vpts[0]*RTmatrix;
        	vpts[1]=vpts[1]*RTmatrix;

		/* Project line to camera/screen coord. */
		if( (err=projectPoints(vpts, 2, projMatrix)) !=0) {
//			egi_dpstd("ProjectPoints error=%d!\n", err);
		//	return;
//			fbdev->pixcolor=WEGI_COLOR_RED;
		}

		/* Draw the line */
		draw3D_line( fbdev, roundf(vpts[0].x), roundf(vpts[0].y), roundf(vpts[0].z),
		            roundf(vpts[1].x), roundf(vpts[1].y), roundf(vpts[1].z) );

		/* Restore color */
//		fbdev->pixcolor=oldcolor;
	}

	for(int i=-sy/2/us; i<=sy/2/us; i++) {
		/* Tow points of a line */
		vpts[0].x = -(sx/2/us)*us;
		vpts[1].x = (sx/2/us)*us;
		vpts[0].y = i*us;
		vpts[1].y = i*us;
		vpts[0].z = 0.0;
		vpts[1].z = 0.0;

		/* Transform */
		vpts[0]=vpts[0]*RTmatrix;
        	vpts[1]=vpts[1]*RTmatrix;

		/* Project line to camera/screen coord. */
		if( (err=projectPoints(vpts, 2, projMatrix)) !=0) {
//			egi_dpstd("ProjectPoints error=%d!\n", err);
		//	return;
//			fbdev->pixcolor=WEGI_COLOR_RED;
		}

		/* Draw the line */
		draw3D_line( fbdev, roundf(vpts[0].x), roundf(vpts[0].y), roundf(vpts[0].z),
		            roundf(vpts[1].x), roundf(vpts[1].y), roundf(vpts[1].z) );

		/* Restore color */
//		fbdev->pixcolor=oldcolor;
	}

   /* Reset flipZ. Notice: Viewing from z- --> z+ */
   fbdev->flipZ=false;

}


/*-------------------------------------------------------
Draw a 3D Circle. (Zbuff applied as in draw_line)
Draw a circle at XY plane then transform it by RTmatrix.

@fbdev:	  	Pointer to FBDEV.
@r:	  	Radius.
@RTmatrix: 	Transform matrix.
-------------------------------------------------------*/
void E3D_draw_circle(FBDEV *fbdev, int r, const E3D_RTMatrix &RTmatrix, const E3DS_ProjMatrix &projMatrix)
{
	if(r<=0) return;

	/* Angle increment step */
	float astep= asinf(0.5/r)*2;
	int   nstep= MATH_PI*2/astep;

	//float astart;
	float aend;	/* segmental arc start/end angle at XY plan. */
	E3D_Vector  vstart, vend; /* segmental arc start/end point at 3D space */
	E3D_Vector  vpts[2];

	/* Draw segment arcs */
	for( int i=0; i<nstep+1; i++) {
		/* Assign aend to astart */
		//astart=aend;
		vstart=vend;

		/* Update aend and vend */
		aend= i*astep;
		vend.x=cos(aend)*r;
		vend.y=sin(aend)*r;
		vend.z=0;

		/* Transform vend as per Nmatrix */
		vend=vend*RTmatrix;

		/* Projection to vpts */
		vpts[0]=vstart; vpts[1]=vend;

		if(i>0) {
			/* Project mat */
			if( projectPoints(vpts, 2, projMatrix) !=0)
				continue;
			/* Draw segmental arc */
			E3D_draw_line(fbdev, vpts[0], vpts[1]);
		}
	}
}


/*---------------------------------------------------------------
Draw the Coordinate_Navigating_Sphere(Frame).
Draw a circles at Global XY plane then transform it by RTmatrix,
same way as for XZ,YZ plane circles.

@fbdev:	 	Pointer to FBDEV.
@r:	  	Radius.
@RTmatrix: 	Transform matrix.
----------------------------------------------------------------*/
void E3D_draw_coordNavSphere(FBDEV *fbdev, int r, const E3D_RTMatrix &RTmatrix, const E3DS_ProjMatrix &projMatrix)
{
	if(r<=0)
		return;

        E3D_Vector axisX(1,0,0);
        E3D_Vector axisY(0,1,0);
	E3D_RTMatrix TMPmat;
        TMPmat.identity();

    gv_fb_dev.flipZ=true;

        /* XY plane -->Z */
        //cout << "Draw XY circle\n";
        fbset_color2(&gv_fb_dev, WEGI_COLOR_RED);
        E3D_draw_circle(&gv_fb_dev, r, RTmatrix, projMatrix); /* AdjustMat to Center screen */

        /* XZ plane -->Y */
        //cout << "Draw XZ circle\n";
        TMPmat.setRotation(axisX, -0.5*MATH_PI);
        fbset_color2(&gv_fb_dev, WEGI_COLOR_GREEN);
        E3D_draw_circle(&gv_fb_dev, r, TMPmat*RTmatrix, projMatrix);

        /* YZ plane -->X */
        //cout << "Draw YZ circle\n";
        TMPmat.setRotation(axisY, 0.5*MATH_PI);
        fbset_color2(&gv_fb_dev, WEGI_COLOR_BLUE);
        E3D_draw_circle(&gv_fb_dev, r, TMPmat*RTmatrix, projMatrix);

        /* Envelope circle, which is perpendicular to the ViewDirection */
        //cout << "Draw envelope circle\n";
	TMPmat=RTmatrix;
        TMPmat.zeroRotation();
        fbset_color2(&gv_fb_dev, WEGI_COLOR_GRAY);
        E3D_draw_circle(&gv_fb_dev, r, TMPmat, projMatrix);

    gv_fb_dev.flipZ=false;
}

/*---------------------------------------------------------------
Draw the Coordinate_Navigating_Frame.

@fbdev:	 	Pointer to FBDEV.
@size:	  	size of each axis.
@minZ:		If true, to show -Z axis also.
@RTmatrix: 	Transform matrix.
----------------------------------------------------------------*/
#include "egi_FTsymbol.h"
void E3D_draw_coordNavFrame(FBDEV *fbdev, int size, bool minZ, const E3D_RTMatrix &RTmatrix, const E3DS_ProjMatrix &projMatrix)
{
	if( size<=0 )
		return;

        E3D_Vector axisX(size,0,0);
        E3D_Vector axisY(0,size,0);
        E3D_Vector axisZ(0,0,size);  /* +Z direction */
        E3D_Vector axisRZ(0,0,-size); /* -Z direction */
        E3D_Vector vpt;

    gv_fb_dev.flipZ=true;

   /* To show Axis +Z and -Z */
   if(minZ) {
        /* Axis_Z */
        fbset_color2(&gv_fb_dev, WEGI_COLOR_RED);
	E3D_draw_line(&gv_fb_dev, axisZ, RTmatrix, projMatrix);

	vpt=axisZ*RTmatrix;
        if( projectPoints(&vpt, 1, projMatrix)==0) {
	        FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                     16, 16,                         /* fw,fh */
                                     (UFT8_PCHAR)"+Z", 		     /* pstr */
                                     300, 1, 0,                      /* pixpl, lines, fgap */
                                     -8+vpt.x, -8+vpt.y,                   /* x0,y0, */
                                     WEGI_COLOR_RED, -1, 255,        /* fontcolor, transcolor,opaque */
                                     NULL, NULL, NULL, NULL );       /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}

        /* Axis_-Z */
        fbset_color2(&gv_fb_dev, WEGI_COLOR_RED);
	E3D_draw_line(&gv_fb_dev, axisRZ, RTmatrix, projMatrix);

	vpt=axisRZ*RTmatrix;
        if( projectPoints(&vpt, 1, projMatrix)==0) {
	        FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                     16, 16,                         /* fw,fh */
                                     (UFT8_PCHAR)"-Z", 		     /* pstr */
                                     300, 1, 0,                      /* pixpl, lines, fgap */
                                     -8+vpt.x, -8+vpt.y,             /* x0,y0, */
                                     WEGI_COLOR_RED, -1, 255,        /* fontcolor, transcolor,opaque */
                                     NULL, NULL, NULL, NULL );       /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}
  }
  /* To show Axis_+Z Only */
  else {
        /* Axis_Z */
        fbset_color2(&gv_fb_dev, WEGI_COLOR_RED);
	E3D_draw_line(&gv_fb_dev, axisZ, RTmatrix, projMatrix);

	vpt=axisZ*RTmatrix;
        if( projectPoints(&vpt, 1, projMatrix)==0) {
	        FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                     16, 16,                         /* fw,fh */
                                     (UFT8_PCHAR)"Z", 		     /* pstr */
                                     300, 1, 0,                      /* pixpl, lines, fgap */
                                     -8+vpt.x, -8+vpt.y,                   /* x0,y0, */
                                     WEGI_COLOR_RED, -1, 255,        /* fontcolor, transcolor,opaque */
                                     NULL, NULL, NULL, NULL );       /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}
  }

        /* Axis_Y */
        fbset_color2(&gv_fb_dev, WEGI_COLOR_GREEN);
	E3D_draw_line(&gv_fb_dev, axisY, RTmatrix, projMatrix);

	vpt=axisY*RTmatrix;
        if( projectPoints(&vpt, 1, projMatrix)==0) {
	        FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                     16, 16,                         /* fw,fh */
                                     (UFT8_PCHAR)"Y", 		     /* pstr */
                                     300, 1, 0,                      /* pixpl, lines, fgap */
                                     -8+vpt.x, -8+vpt.y,                   /* x0,y0, */
                                     WEGI_COLOR_GREEN, -1, 255,        /* fontcolor, transcolor,opaque */
                                     NULL, NULL, NULL, NULL );       /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}

        /* Axis_X */
        fbset_color2(&gv_fb_dev, WEGI_COLOR_BLUE);
	E3D_draw_line(&gv_fb_dev, axisX, RTmatrix, projMatrix);

	vpt=axisX*RTmatrix;
        if( projectPoints(&vpt, 1, projMatrix)==0) {
	        FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                     16, 16,                         /* fw,fh */
                                     (UFT8_PCHAR)"X", 		     /* pstr */
                                     300, 1, 0,                      /* pixpl, lines, fgap */
                                     -8+vpt.x, -8+vpt.y,                   /* x0,y0, */
                                     WEGI_COLOR_BLUE, -1, 255,        /* fontcolor, transcolor,opaque */
                                     NULL, NULL, NULL, NULL );       /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	}


    gv_fb_dev.flipZ=false;
}

/*-----------------------------------------------------------------------
Draw a 2D Coordinate_Navigating_Sphere(Frame), zbuff is OFF!

@fbdev:	 	Pointer to FBDEV.
@size:	  	size of each axis.
@RTmatrix: 	Transform matrix.
		!!! Translation component will be cleared !!!
@x0,y0:		Center point of the coordNavFrame
		Under SCREEN Coord.
------------------------------------------------------------------------*/
void E3D_draw_coordNavIcon2D(FBDEV *fbdev, int size, const E3D_RTMatrix &RTmatrix, int x0, int y0)
{
	if(fbdev==NULL) return;

	bool zbuff_save=fbdev->zbuff_on;
	fbdev->zbuff_on=false;

	/* Reset translation */
	E3D_RTMatrix pmatrix=RTmatrix;
	pmatrix.zeroTranslation();

	/* Since other E3D_draw function will atuo. adjust origin to SCREEN center ... */
	pmatrix.setTranslation(x0-fbdev->pos_xres/2, y0-fbdev->pos_yres/2, 0);

	/* ISOmetric projMatrix */
	E3DS_ProjMatrix projMatrix;
	projMatrix.type=E3D_ISOMETRIC_VIEW;
	projMatrix.dv = INT_MIN;        /* Try to ingore z value */
	projMatrix.winW=fbdev->pos_xres;
	projMatrix.winH=fbdev->pos_yres;

	/* Draw a coordNaveSphere */
	E3D_draw_coordNavFrame(fbdev, size, false, pmatrix, projMatrix);

	fbdev->zbuff_on=zbuff_save;
}


///////////////////////////  Lighting   /////////////////////////////

/*-----------------------------------------------------------------
Compute lightings to get the color as we finally see.

Note:
1. All vectors in lighting model are unit vectors.
2. To define kd,ks,ka in *.mtl file.
   E3D_Materil() default: ka(0.8,0.8,0.8), ks(0.6,0.6,0.6)
   and kd=vectorRGB(COLOR_24TO16BITS(0xF0CCA8))

TODO:
E3D_Lighting

@mtl:		Surface material.
@normal:	Surface normal. (Unit Vector)
//@view:		View direction, from object to eye! (Unit Vector)
@dl:		Distance from lighting source to the pixel.

Return:
		EGI_16BIT_COLOR
------------------------------------------------------------------*/
EGI_16BIT_COLOR E3D_seeColor(const E3D_Material &mtl, const E3D_Vector &normal, float dl)
{
	/* Vectors for view and lighting */
	E3D_Vector toView(0,0,-1); /* == -vView(0,0,1); camera view from -z to +z */
	E3D_Vector toLight=-gv_vLight;

	/* Source light specular color */
        //E3D_Vector Ss(0.75, 0.75, 0.75);
//	E3D_Vector Ss=mtl.Ss; /* Default 0.8 0.8 0.8 */
	/* Source light diffuse color */
        //E3D_Vector Sd(0.8, 0.8, 0.8);
//	E3D_Vector Sd=mtl.Sd; /* Default 0.8 0.8 0.8 */

	/* Attenuation factor for light intensity */
	float atten=1.0;  /* TODO */

	/* glossiness, smaller for wider and brighter. Default 0.0 */
	float mgls=mtl.Ns; /* mtl->Ns as the specular exponent */

	E3D_Vector Cs,Cd,Ca,Ce;	/* Components */
	E3D_Vector Cres; 	/* Compound */
	E3D_Vector rf; /* Light Refection vector, mirror of toLight along normal. */

	/* Final RGB */
	//uint8_t R=0,G=0,B=0;
	int R=0,G=0,B=0;

   for(int i=0; i<2; i++) {
	/* Vector to Lights. NOTE: adjust module of vLight as attenuation factor also. */
	if(i==0)
	     toLight=-gv_vLight;
	else
	     toLight=-gv_auxLight;

	/* Init as zeros */
	Cs.zero(); Cd.zero(); Ca.zero();

 //    if(i==0)
     { /* Once only */
	/* Component 1: Phong specular reflection  Cs=max(v*r,0)^mgls*Ss(x)Ms */
	/* Light Refection vector, mirror of toLight along normal. */
	rf=2*(normal*toLight)*normal-toLight;
	Cs=powf(fmaxf(toView*rf, 0.0), mgls)*mtl.Ss;
	Cs.x=Cs.x*mtl.ks.x;
	Cs.y=Cs.y*mtl.ks.y;
	Cs.z=Cs.z*mtl.ks.z;
     }
	/* Component 2: Diffuse reflection  Cd=(n*l)*Sd(x)Md */
	Cd=fmaxf(normal*toLight,0.0)*mtl.Sd;  /* Light direction here is from obj to light source! */
	Cd.x=Cd.x*mtl.kd.x;
	Cd.y=Cd.y*mtl.kd.y;
	Cd.z=Cd.z*mtl.kd.z;
//	Cd.print("Cd");

     if(i==0) {  /* Once only */
	/* Component 3: Ambient reflection Ca=gamb(x)Ma */
	Ca.x=gv_ambLight.x*mtl.ka.x; /* Note: gv_ambLight is NOT directoin vector! */
	Ca.y=gv_ambLight.y*mtl.ka.y;
	Ca.z=gv_ambLight.z*mtl.ka.z;
//	Ca.print("Ca");

	/* Component 4: Self-emission Ce=Me HK2022-10-24 */
	Ce = mtl.ke;
     }

	/* Result */
	Cres=atten*(Cs+Cd)+Ca+Ce; /* HK2022-10-24 */

	//uint8_t  R=roundf(Cres.x*255); G=roundf(Cres.y*255); B=roundf(Cres.z*255);
	R+=roundf(Cres.x*255); if(R>255)R=255; else if(R<0)R=0;
	G+=roundf(Cres.y*255); if(G>255)G=255; else if(G<0)G=0;
	B+=roundf(Cres.z*255); if(B>255)B=255; else if(B<0)B=0;
	//egi_dpstd("seecolor: RGB 0x%02X%02X%02X\n", R,G,B);

   } /* END for()*/

	return COLOR_RGB_TO16BITS(R,G,B);
}

/*---------------------------------------------------------------
Clip a trianlge with near plane of the view frustum and update
RenderVertice to form a new shape, any vertices with z<zc will
be clipped.

Note:
1. The caller MUST ensure enough space of rvts[] to store
   results of clipping.
   For clipping a triangle, it will get MAX. 2 new triangles.

@zc:	To define clipping plane Z=zc. Usually as the near plane.
	and vertices z<zc will be clipped!
@rtvs:	Input: Render_vertices with position/uv/normal/color info etc.
	Output: RenderVertices after clipping.
	rvts[] SHOULD be stored in right/left hand seqence to
	form a certain shape.

@np:	Input:  Number/Size of rvts[].
	Output: Number/Size of rvts[].
	NOW: Input 2 or 3, Output  2,3 or 4.

@options: Any combination of VTXNORMAL_ON, VTXCOLOR_ON and VTXUV_ON.

TODO:
XXX 1. For clipping a polygon and input more than 3 RenderVerteice.
2. If ONE or TWO vertices are JUST ON the zc plane, and others are outside,
   then the clipped ploygon will be a degenerated shape, as a point or a line.

Return:
	>0  Number of edges clipped.
	=0  Complete IN or OUT, check np value.
	<0  Fails
----------------------------------------------------------------*/
int E3D_ZNearClipTriangle(float zc, struct E3D_RenderVertex rvts[], int &np, int options)
{
	bool  zOut[3];	/* If z<zc, and need to clip */
	int   epos;	/* edge position */
	struct E3D_RenderVertex nrvts[4]; /* MAX. 4 vertices for 2 triangles */
	int  vcnt;	/* count nrvts */
	int  icnt,ocnt; /* IN/OUT vertex count */
	int  ke;	/* stands for k+1 */
	float zratio;	/* (zc-rvts[k].pt.z)/(rvts[ke].pt.z-rvts[k].pt.z) */
	int  ret=0;

	/* Check np. --- MIN.3 --- */
	if(np<2) return -1;

	/* Check zOut[] */
	icnt=0; ocnt=0;
	for(int k=0; k<np; k++) {
		if(rvts[k].pt.z < zc) {
			zOut[k]=true;
			ocnt++;
		}
		else {
			zOut[k]=false;
			icnt++;
		}
	}

	/* Completely OUT near plane of the view frustum */
	if(ocnt==np) {
		np=0;
		return 0;
	}
	/* Completely IN near plane of the view frustum */
	else if(icnt==np) {
		//Keep all vertices
		return 0;
	}

	/* Check and clip each edge of the triangle */
	vcnt=0;
	for(int k=0; k< (np>2?np:1); k++) {  /* When np==2, it's a line, NOT closed ploygon, so it loops ONCE ONLY! */
		/* K as vertex indices for Edges: 0-1, 1-2, 2-0 */
		epos=0;
		if(zOut[k]) epos |= 0x1;   /* Edge start vertex position, in(0)/out(1) frustum */
		ke=(k+1)%np;
		if(zOut[ke]) epos |= 0x2;  /* Edge end vertex position, in(0)/out(2) frustum */

		/* Zratio for clipped XY */
		zratio = (zc-rvts[k].pt.z)/(rvts[ke].pt.z-rvts[k].pt.z);

		/* Switch cases */
		switch(epos) {
		   case 0:	/* in_in,  save start vertex */
			nrvts[vcnt]=rvts[k];
			vcnt++;
			break;
		   case 1:	/* out_in, save clip (If a line: + save end) */

			/* Clip the edge, save clipped point */
			nrvts[vcnt].pt.x = rvts[k].pt.x+(rvts[ke].pt.x-rvts[k].pt.x)*zratio;
			nrvts[vcnt].pt.y = rvts[k].pt.y+(rvts[ke].pt.y-rvts[k].pt.y)*zratio;
			nrvts[vcnt].pt.z = zc;
			/* Interpolate normal,color,uv */
			if(options & VTXNORMAL_ON)
				nrvts[vcnt].normal = rvts[k].normal+(rvts[ke].normal-rvts[k].normal)*zratio;
			if(options & VTXCOLOR_ON)
				nrvts[vcnt].color = rvts[k].color+(rvts[ke].color-rvts[k].color)*zratio;
			if(options & VTXUV_ON) {
				nrvts[vcnt].u = rvts[k].u+(rvts[ke].u-rvts[k].u)*zratio;
				nrvts[vcnt].v = rvts[k].v+(rvts[ke].v-rvts[k].v)*zratio;
			}
			/* Count nrvts */
			vcnt++;

			/* If it's a line, NOT closed ploygon, save the end vertex. */
			if(np==2) {
				nrvts[vcnt]=rvts[1];

				/* Count vcnt */
				vcnt++;
			}

			/* Count ret */
			ret++;
			break;
		   case 2:	/* in_out, save start + save clip (OK for a line) */
			/* Save start */
			nrvts[vcnt]=rvts[k];
			/* Count nrvts */
			vcnt++;

			/* Clip the edge, save clipped point  */
			nrvts[vcnt].pt.x = rvts[k].pt.x+(rvts[ke].pt.x-rvts[k].pt.x)*zratio;
                        nrvts[vcnt].pt.y = rvts[k].pt.y+(rvts[ke].pt.y-rvts[k].pt.y)*zratio;
                        nrvts[vcnt].pt.z = zc;
                        /* Interpolate normal,color,uv */
			if(options & VTXNORMAL_ON)
	                        nrvts[vcnt].normal = rvts[k].normal+(rvts[ke].normal-rvts[k].normal)*zratio;
			if(options & VTXCOLOR_ON)
	                        nrvts[vcnt].color = rvts[k].color+(rvts[ke].color-rvts[k].color)*zratio;
			if(options & VTXUV_ON) {
	                        nrvts[vcnt].u = rvts[k].u+(rvts[ke].u-rvts[k].u)*zratio;
        	                nrvts[vcnt].v = rvts[k].v+(rvts[ke].v-rvts[k].v)*zratio;
			}
                        /* Count nrvts */
                        vcnt++;

			/* Count ret */
			ret++;
			break;
		   case 3:	/* out_out, save nothing */
			break;
		}

	}

	/* Pass out new rvts[] */
	np=vcnt;
	for(int k=0; k<np; k++)
		rvts[k]=nrvts[k];

	return ret;
}


////////////////////////////   Triangle Shader   ////////////////////////////////////

/*----------------------------------------------------------------------
Shade and render a triangle to fb_dev.

@fb_dev:	Pointer to FBDEV
@rvtx[3]:	E3DS_RenderVertex of the triangle, with XYZ already projected
		to SCREEN plane.
@mtl:		E3D_Material for the triangle
@scene:		E3D_Scene as rendering environment. NOT applied yet.

Note:
1. Sequence of xyz0-xyz2 MUST NOT be shifted, as they MUST keep same sequenece
   as UVs and Normals.


TODO:
XXX 0. Call E3D_computeBarycentricCoord()?? ---NOPE!
1. Repeating rvtx[] data in renderMesh(), to buffer them.
2. E3D_shadeVetex() for renderMesh()?
XXX 3. Pixel normal interpolation. NOW pixel normal == face normal =rvtx[0].normal. ---OK
4. E3D_seeColor() use default lighting now.
----------------------------------------------------------------------*/
void E3D_shadeTriangle( FBDEV *fb_dev, const E3DS_RenderVertex rvtx[3], const E3D_Material &mtl, const E3D_Scene &scene )
{

	if(fb_dev==NULL)
		return;

	/* 1. Material for pixel. kd will be updated for each pixel! */
	E3D_Material pmtl=mtl;
	pmtl.img_kd=NULL;	/* DO NOT own/hold img_kd! */

	/* 2. Get vetex coordinates */
	//int x0=roundf(rvtx[0].pt.x);
	int x0=roundf(rvtx[0].pt.x);
	int y0=roundf(rvtx[0].pt.y);
	float z0=-rvtx[0].pt.z; /* Views from -z ----> +z  */

	int x1=roundf(rvtx[1].pt.x);
	int y1=roundf(rvtx[1].pt.y);
	float z1=-rvtx[1].pt.z; /* Views from -z ----> +z  */

	int x2=roundf(rvtx[2].pt.x);
	int y2=roundf(rvtx[2].pt.y);
	float z2=-rvtx[2].pt.z; /* Views from -z ----> +z  */

	/* 3. Check input imgbuf */
	EGI_IMGBUF *imgbuf=NULL;
	int imgw=0;
	int imgh=0;
	float u0=0.0, v0=0.0, u1=0.0, v1=0.0, u2=0.0, v2=0.0;

	imgbuf=mtl.img_kd;
	if( imgbuf==NULL || imgbuf->imgbuf==NULL ) {
		//egi_dpstd("Input EGI_IMBUG is NULL!\n"); ---OK
	}
	else {
		/* Get image size */
		imgw=imgbuf->width;
		imgh=imgbuf->height;

		/* Get uv coordinates for texture mapping. */
		u0=rvtx[0].u; v0=rvtx[0].v;
		u1=rvtx[1].u; v1=rvtx[1].v;
		u2=rvtx[2].u; v2=rvtx[2].v;

#if 1		/* Check input u/v. TODO: Reasoning... */
		if( u0<0.0 || u0>1.0 ) u0=-floorf(u0)+u0;
		if( v0<0.0 || v0>1.0 ) v0=-floorf(v0)+v0;
		if( u1<0.0 || u1>1.0 ) u1=-floorf(u1)+u1;
		if( v1<0.0 || v1>1.0 ) v1=-floorf(v1)+v1;
		if( u2<0.0 || u2>1.0 ) u2=-floorf(u2)+u2;
		if( v2<0.0 || v2>1.0 ) v2=-floorf(v2)+v2;
#endif
	}

	/* 4. Barycentric coordinates (a,b,r) for points inside the triangle:
	 * P(x,y)=a*A + b*B + r*C;  where a+b+r=1.0.
	 */
	float a, b, r, ftmp;
	float u,v;  /* uv of a pixel */
	int x;    /* x of a pixel  <--------- TODO */
	EGI_16BIT_COLOR color;	  /* draw_dot() pixel color */
	E3D_Vector pnormal;	/* As pixel normal */

	/* As for 2D vetex */
	struct {
		//int x; int y; //float z;
		float x; float y;
	} points[3];
	points[0].x=x0; points[0].y=y0; //points[0].z=z0;
	points[1].x=x1; points[1].y=y1; //points[1].z=z1;
	points[2].x=x2; points[2].y=y2; //points[2].z=z2;

	int i, k, kstart, kend;
	int nl=0,nr=0;  /* left and right point index */
	int nm; 	/* mid point index */

	float klr,klm,kmr;
        float  fcheck1,fcheck2;

	/* Compute fcheck1/fcheck2 */
	fcheck1 = -1.0*(x0-x1)*(y2-y1)+(y0-y1)*(x2-x1); // 1.0*(y0-y1)*(x2-x1);
	fcheck2 = -1.0*(x1-x2)*(y0-y2)+(y1-y2)*(x0-x2); // 1.0*(y1-y2)*(x0-x2);

	/* use float type */  /* <-------------- */
	float yu=0;
	float yd=0;
	float ymu=0;

	float zu,zd; /* Z value for start-end point */
	float us,ue, vs,ve; /* u,v for start-end point */

	/* Imgbuf pixel data offset */
	long int locimg;


	/* --- Case 1 ---: All points are the SAME! */
	if(x0==x1 && x1==x2 && y0==y1 && y1==y2) {
//		egi_dpstd("the Tri degenerates into a point!\n");

/* TODO&TBD: --------------------------- */
// return;

        	/* Set a/b/r. */
		a=0.3333;    b=0.3333;    r=0.3333;

		/* Set pixz */
		fb_dev->pixz = roundf(a*z0+b*z1+r*z2);

		/* Get interpolated u,v */
		u=a*u0+b*u1+r*u2;
		//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
		v=a*v0+b*v1+r*v2;
		//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;

                /* Get interpolated pixel normal */
                pnormal=a*rvtx[0].normal+b*rvtx[1].normal+r*rvtx[2].normal;
		pnormal.normalize();

                if(imgbuf==NULL) {  /* Use default material kd */
                	/* See the pixel color */
                        color=E3D_seeColor(pmtl, pnormal, 0); /* (mtl, normal, dl) */
                        fbset_color2(fb_dev, color);
                        draw_dot(fb_dev, x0, y0);
                }
		else { /* imgbuf!=NULL */
			/* Get mapped pixel and daw_dot */
        	        /* image data location */
                	locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf! */
			if( locimg>=0 && locimg < imgh*imgw ) {
				//fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);

				/* Material for each pixel updated */
				pmtl.kd.vectorRGB(imgbuf->imgbuf[locimg]);
				/* See the pixel color */
				fbset_color2(fb_dev, E3D_seeColor(pmtl, rvtx[0].normal, 0)); /* (mtl, normal, dl) */

				if(imgbuf->alpha)
					fb_dev->pixalpha=imgbuf->alpha[locimg];
                        	draw_dot(fb_dev, x0, y0);
			}
#if 1 /* TEST: --------- */
			else egi_dpstd("Out range u,v: %e,%e\n", u,v);
#endif
		}

		return;
	}

#if 0 //Move to beginning	/* If all points are collinear, including the case that TWO pionts are at SAME position! */
	fcheck1 = -1.0*(x0-x1)*(y2-y1)+(y0-y1)*(x2-x1); // 1.0*(y0-y1)*(x2-x1);
	fcheck2 = -1.0*(x1-x2)*(y0-y2)+(y1-y2)*(x0-x2); // 1.0*(y1-y2)*(x0-x2);
#endif

	/* Cal nl, nr. just after collinear checking! */
	for(i=1; i<3; i++) {
		if(points[i].x < points[nl].x) nl=i;
		if(points[i].x > points[nr].x) nr=i;
	}

	/* --- Case 2 ---: All points are collinear as a vertical line. */
	if(nl==nr) {
//		egi_dpstd("the Tri degenerates into a vertical line!\n");

/* TODO&TBD: ----------- */
return;
		/* Init yu yd, u,v. zu,zd */
		yu=points[0].y;  ue=u0; ve=v0;
		yd=points[0].y;  us=u0; vs=v0;
		zu=zd=z0;

		/* Get yu,yd, zu,zd,  us/ue/vs/ve */
		for(i=1; i<3; i++) {
			if(points[i].y>yu) {
				yu=points[i].y;
				if(i==1) { ue=u1; ve=v1; zu=z1;}
				else     { ue=u2; ve=v2; zu=z2;} //i==2
			}
			if(points[i].y<yd) {
				yd=points[i].y;
				if(i==1) { us=u1; vs=v1; zd=z1;}
				else     { us=u2; vs=v2; zd=z2;} //i==2
			}
		}

		x=roundf(points[0].x);
		for(k=yd; k<=yu; k++) {
			/* Get interpolated u,v */
			u=us+(ue-us)*(k-yd)/(yu-yd);
			v=vs+(ve-vs)*(k-yd)/(yu-yd);

                        /* Get interpolated pixel normal */
                        //TODO: pnormal=

			/* Set pixZ */
			fb_dev->pixz = roundf(zd +(zu-zd)*(k-yd)/(yu-yd));

                	if(imgbuf==NULL) {  /* Use default material kd */
                		/* See the pixel color */
                        	color=E3D_seeColor(pmtl, pnormal, 0); /* (mtl, normal, dl) */
                        	fbset_color2(fb_dev, color);
                        	draw_dot(fb_dev, x0, y0);
                	}
			else { /* imgbuf!=NULL */
				/* Get mapped pixel and draw_dot */
        	                /* image data location */
                	        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf TODO: interpolate */
				if( locimg>=0 && locimg < imgh*imgw ) {
		            		//fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);

			    		/* Material for each pixel updated */
			    		pmtl.kd.vectorRGB(imgbuf->imgbuf[locimg]);
			    		/* See the pixel color */
			    		fbset_color2(fb_dev, E3D_seeColor(pmtl, rvtx[0].normal, 0)); /* (mtl, normal, dl) */

			    		if(imgbuf->alpha)
				  		fb_dev->pixalpha=imgbuf->alpha[locimg];
                            		draw_dot(fb_dev, x, k);
				}
#if 1 /* TEST: --------- */
				else egi_dpstd("Out range u,v: %e,%e\n", u,v);
			}
#endif
		}  /* End: for(k) */

		return;
	}

	/* ---- Case 3 ---: All points are collinear as an oblique/horizontal line. */
	/* Note:
	 *	1. You may skip case_3, to let Case_4 draw the line, however it draws ONLY discrete
	 *	   dots for a steep line.
	 *	2. Color of the midpoint of the line will be ineffective!
	 * 	   TODO: NEW algrithm for interpolation color at a three_point line.
	 */
	if( (x0-x1)*(y2-y1)==(y0-y1)*(x2-x1) || (x1-x2)*(y0-y2)==(y1-y2)*(x0-x2) )
	{
//		egi_dpstd("the Tri degenerates into an oblique/horiz line!\n");

/* TODO&TBD----------: If so, to apply pnormal and E3D_seeColor() */
return;

		int j;

                int x1=points[nl].x;
                int y1=points[nl].y;
		float pixz1= (nl==0?z0:(nl==1?z1:z2));

                int x2=points[nr].x;
                int y2=points[nr].y;
		float pixz2= (nr==0?z0:(nr==1?z1:z2));

		int tekxx=x2-x1;
		int tekyy=y2-y1;

		float foff;
		float flen=sqrtf(1.0*(x2-x1)*(x2-x1)+1.0*(y2-y1)*(y2-y1));

		int tmp;
//		EGI_16BIT_COLOR colorTmp; /* Color corresponds to k(y)=tmp */
		EGI_16BIT_COLOR colorJ;   /* Color corresponds to k(y)=J */
		EGI_8BIT_ALPHA  alphaJ;

		/* NOW: x2>x1 */
		tmp=y1;

		/* nl as start point */
		if(nl==0) { us=u0; vs=v0; }
		else if(nl==1) { us=u1; vs=v1; }
		else { us=u2; vs=v2; }

		/* nr as end point */
		if(nr==0) { ue=u0; ve=v0; }
		else if(nr==1) { ue=u1; ve=v1; }
		else { ue=u2; ve=v2; }

		/* Draw all points */
		for(i=x1; i<=x2; i++) {
		     	/* j as Py */
		    	j=roundf( 1.0*(i-x1)*tekyy/tekxx+y1 );

			/* Get interpolated u,v for point (i,j) */
			u=us+(ue-us)*(i-x1)/(x2-x1);
			v=vs+(ve-vs)*(i-x1)/(x2-x1);

			/* Set pixZ */
			fb_dev->pixz = roundf(pixz1+(pixz2-pixz1)*(i-x1)/(x2-x1));

                        /* Image pixel color location, (i,j) */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			if( locimg>=0 && locimg < imgh*imgw ) {
			    //colorJ=imgbuf->imgbuf[locimg];

			    /* Material for each pixel updated */
			    pmtl.kd.vectorRGB(imgbuf->imgbuf[locimg]);
			    /* See the pixel color */
			    colorJ=E3D_seeColor(pmtl, rvtx[0].normal, 0); /* (mtl, normal, dl) */

			    if(imgbuf->alpha)
				  alphaJ=imgbuf->alpha[locimg];
			}
			else
			    continue;

			if(y2>=y1) {
				/* Traverse tmp (+)-> pY */
				for(k=tmp; k<=j; k++) {  //note tmp=y1
					if(tmp==j) {
						fbset_color2(fb_dev, colorJ);
						if(imgbuf->alpha)
							fb_dev->pixalpha=alphaJ;
						draw_dot(fb_dev,i,k);
						break;
					}

					/* Get interpolated u,v */
					foff=sqrtf(1.0*(i-x1)*(i-x1)+1.0*(k-y1)*(k-y1));
					u=us+(ue-us)*foff/flen;
					v=vs+(ve-vs)*foff/flen;

		                        /* Image pixel color location */
                        		locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			                if( locimg>=0 && locimg < imgh*imgw ) {
                            			//color=imgbuf->imgbuf[locimg];

			    			/* Material for each pixel updated */
			    			pmtl.kd.vectorRGB(imgbuf->imgbuf[locimg]);
			    			/* See the pixel color */
			    			color=E3D_seeColor(pmtl, rvtx[0].normal, 0); /* (mtl, normal, dl) */
					}
                        		else
                            			continue;

				     	/* Set color for point(i,k) */
 				     	//egi_16bitColor_interplt( colorTmp, colorJ, 0, 0, (k-tmp)*(1<<15)/(j-tmp), &color, NULL);
					fbset_color2(fb_dev, color);

				     	draw_dot(fb_dev,i,k);
				}
			}
			else {  /* y2<y1, Traverse tmp (-)-> pY*/
				for(k=tmp; k>=j; k--) {  //note tmp=y1
					if(tmp==j) {
						fbset_color2(fb_dev, colorJ);
						if(imgbuf->alpha)
                                                        fb_dev->pixalpha=alphaJ;
						draw_dot(fb_dev,i,k);
						break;
					}

					/* Get interpolated u,v */
					foff=sqrtf(1.0*(i-x1)*(i-x1)+1.0*(k-y1)*(k-y1));
					u=us+(ue-us)*foff/flen;
					v=vs+(ve-vs)*foff/flen;

		                        /* Image pixel color location */
                        		locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			                if( locimg>=0 && locimg < imgh*imgw ) {
                            			//color=imgbuf->imgbuf[locimg];
			    			/* Material for each pixel updated */
			    			pmtl.kd.vectorRGB(imgbuf->imgbuf[locimg]);
			    			/* See the pixel color */
			    			color=E3D_seeColor(pmtl, rvtx[0].normal, 0); /* (mtl, normal, dl) */
					}
                        		else
                            			continue;

				     	/* Color for point(i,k) */
 				     	//egi_16bitColor_interplt( colorTmp, colorJ, 0, 0, (tmp-k)*(1<<15)/(tmp-j), &color, NULL);
					fbset_color2(fb_dev, color);
			    		if(imgbuf->alpha)
				  		fb_dev->pixalpha=imgbuf->alpha[locimg];

				     	draw_dot(fb_dev,i,k);
				}
			}

			tmp=j; /* Renew tmp as j(pY) */
//			colorTmp=colorJ; /* Renew colroTmp as color J */
		}

		return;
	}


	/* ---- Case 4 ---: As a true triangle. */

	/* Get x_mid point index, NOW: nl != nr. */
	nm=3-nl-nr;

	/* Ruled out (points[nr].x == points[nl].x), as nl==nr.  */
	//if(nl!=nr)
	       klr=1.0*(points[nr].y-points[nl].y)/(points[nr].x-points[nl].x);
	//else
	//       klr=1000000.0;

	if(points[nm].x != points[nl].x) {
		klm=1.0*(points[nm].y-points[nl].y)/(points[nm].x-points[nl].x);
	}
	else
		klm=1000000.0;

	if(points[nr].x != points[nm].x) {
		kmr=1.0*(points[nr].y-points[nm].y)/(points[nr].x-points[nm].x);
	}
	else
		kmr=1000000.0;

	/* Draw left part  */
	//for( i=0; i< points[nm].x-points[nl].x; i++)
	for( i=0; i< points[nm].x-points[nl].x+1; i++)
	{
		yu=roundf(klr*i+points[nl].y);
		yd=roundf(klm*i+points[nl].y);
		//egi_dpstd("nm-nl: yu=%d, yd=%d\n", yu,yd);

		/* Cal. x */
		x=points[nl].x+i;

		if(yu>yd) { kstart=yd; kend=yu; }
		else	  { kstart=yu; kend=yd; }

		for(k=kstart; k<=kend; k++) {
			/* Calculate barycentric coordinates: a,b,r */
			/*Note: y=k */
			/* Note: Necessary for precesion check! */
			//a=(-1.0*(x-x1s)*(y2s-y1s)+(k-y1s)*(x2s-x1s))/fcheck1;
			//b=(-1.0*(x-x2s)*(y0s-y2s)+(k-y2s)*(x0s-x2s))/fcheck2;
			a=(-1.0*(x-x1)*(y2-y1)+(k-y1)*(x2-x1))/fcheck1;
			b=(-1.0*(x-x2)*(y0-y2)+(k-y2)*(x0-x2))/fcheck2;

#if 1 /* TEST: ------------------------------- */
			if(a!=a) egi_dpstd("a is NaN!\n");
			if(b!=b) egi_dpstd("b is NaN!\n");
#endif

			/* Normalize a/b/r */
			if(a<0)a=-a; if(b<0)b=-b;
			ftmp=a+b;
			if(ftmp>1.0f+0.001) {
				//egi_dpstd("a+b>1.0! a=%e, b=%e\n",a,b);
				a=a/ftmp; b=b/ftmp;
			}
			if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
			if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
			r=1.0-a-b;
			if(r<0.0f) r=0.0f; //continue;

			/* Get interpolated pixel u,v */
			u=a*u0+b*u1+r*u2;
			//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
			v=a*v0+b*v1+r*v2;
			//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;

			/* Get interpolated pixel normal */
			pnormal=a*rvtx[0].normal+b*rvtx[1].normal+r*rvtx[2].normal;

			/* Set pixz */
			fb_dev->pixz = roundf(a*z0+b*z1+r*z2);

			if(imgbuf==NULL) {  /* Use default material kd */
 				/* See the pixel color */
    				color=E3D_seeColor(pmtl, pnormal, 0); /* (mtl, normal, dl) */
				fbset_color2(fb_dev, color);
                         	draw_dot(fb_dev, x, k);
			}
			else { /* imgbuf!=NULL */
				/* Get mapped pixel and draw_dot */
        	                /* image data location */
                	        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
#if 1 /* TEST: ---- */
				if(locimg >imgh*imgw-1) locimg=imgh*imgw-1;
#endif
				if( locimg>=0 && locimg < imgh*imgw ) {
		            		//fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);

	    				/* Material for each pixel updated */
    					pmtl.kd.vectorRGB(imgbuf->imgbuf[locimg]);
    				    	/* See the pixel color */
    				    	color=E3D_seeColor(pmtl, pnormal, 0); /* (mtl, normal, dl) */
			    		fbset_color2(fb_dev, color);
			    		/* Set alpha */
	    		    		if(imgbuf->alpha)
	  			 		fb_dev->pixalpha=imgbuf->alpha[locimg];

                            		draw_dot(fb_dev, x, k);
				}
#if 1 /* TEST: -------------- */
				else egi_dpstd("Out range u,v: %e,%e\n", u,v);
#endif
			}

		} /* End: for(i) */
	} /* End: draw left part */

	/* Draw right part */
	ymu=klr*(i-1)+points[nl].y; //ymu=yu; yu MAYBE replaced by yd!
	//for( i=0; i<points[nr].x-points[nm].x; i++)
	for( i=0; i< points[nr].x-points[nm].x+1; i++)
	{
		yu=roundf(klr*i+ymu);
		yd=roundf(kmr*i+points[nm].y);
		//egi_dpstd("nr-nm: yu=%d, yd=%d\n", yu,yd);

		/* Cal. x */
		x=points[nm].x+i;

		if(yu>yd) { kstart=yd; kend=yu; }
		else	  { kstart=yu; kend=yd; }

		for(k=kstart; k<=kend; k++) {
			/* Calculate barycentric coordinates: a,b,r */
			// y=k;
			/* Note: Necessary for precesion check! */
			//a=(-1.0*(x-x1s)*(y2s-y1s)+(k-y1s)*(x2s-x1s))/fcheck1;
			//b=(-1.0*(x-x2s)*(y0s-y2s)+(k-y2s)*(x0s-x2s))/fcheck2;
			a=(-1.0*(x-x1)*(y2-y1)+(k-y1)*(x2-x1))/fcheck1;
			b=(-1.0*(x-x2)*(y0-y2)+(k-y2)*(x0-x2))/fcheck2;

#if 1 /* TEST: ------------------------------- */
			if(a!=a) egi_dpstd("a is NaN!\n");
			if(b!=b) egi_dpstd("b is NaN!\n");
#endif

			/* Normalize a/b/r */
			if(a<0)a=-a; if(b<0)b=-b;
			ftmp=a+b;
			if(ftmp>1.0f+0.001) {
				//egi_dpstd("a+b>1.0! a=%e, b=%e\n",a,b);
				a=a/ftmp; b=b/ftmp;
			}
			if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
			if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
			r=1.0-a-b;
			if(r<0.0f) r=0.0; //continue;

			/* Get interpolated u,v */
			u=a*u0+b*u1+r*u2;
			//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
			v=a*v0+b*v1+r*v2;
			//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;

			/* Get interpolated pixel normal */
			pnormal=a*rvtx[0].normal+b*rvtx[1].normal+r*rvtx[2].normal;
//			pnormal.print("pnormal");

			/* Set pixz */
			fb_dev->pixz = roundf(a*z0+b*z1+r*z2);
			if(imgbuf==NULL) {  /* Use default material kd */
 				/* See the pixel color */
    				color=E3D_seeColor(pmtl, pnormal, 0); /* (mtl, normal, dl) */
				fbset_color2(fb_dev, color);
                         	draw_dot(fb_dev, x, k);
			}
			else { /* imgbuf!=NULL */
				/* Get mapped pixel and draw_dot */
        	                /* image data location */
                	        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
#if 1 /* TEST: ---- */
				if(locimg >imgh*imgw-1) locimg=imgh*imgw-1;
#endif
				if( locimg>=0 && locimg < imgh*imgw ) {
		            		//fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
    			    		/* Material for each pixel updated */
    			    		pmtl.kd.vectorRGB(imgbuf->imgbuf[locimg]);
	    				/* See the pixel color */
    					color=E3D_seeColor(pmtl, pnormal, 0); /* (mtl, normal, dl) */
					fbset_color2(fb_dev, color);
			    		/* Set alpha */
	    		    		if(imgbuf->alpha)
	  					fb_dev->pixalpha=imgbuf->alpha[locimg];

                            		draw_dot(fb_dev, x, k);
				}
#if 1 /* TEST: --------- */
				else egi_dpstd("Out range u,v: %e,%e\n", u,v);
#endif
			}

		} /* End: for(i) */
	} /* End: draw left part */

}
