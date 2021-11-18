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
		 P'xyz = Pxyz*RTMatrix.
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
		    objects to the expected postion.
   E3D_ProjMatrix:  Projection Matrix, to project/map objects in a defined
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
2. NOW: All pixels of a triangle use the same pixz value for zbuff[], which is the
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

XXX 4. Option to display/hide back faces of meshes.

4.  In case the triangle degenerates into a line, then we MUST figure out the
    facing(visible) side(s)(with the bigger Z values), before we compute color/Z value
    for each pixel. ( see. in draw_filled_trianglX() and egi_imgbuf_mapTriWriteFBX() )

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
	4. Add E3D_draw_line(FBDEV *, const E3D_Vector &, const E3D_RTMatrix &, const E3D_ProjMatrix &)
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

Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------*/
#ifndef __E3D_TRIMESH_H__
#define __E3D_TRIMESH_H__

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

using namespace std;

#define TEST_PERSPECTIVE  	1
#define VPRODUCT_EPSILON	(1.0e-6)  /* -6, Unit vector vProduct=vct1*vtc2 */

int readObjFileInfo(const char* fpath, int & vertexCount, int & triangleCount,
				       int & vtxNormalCount, int & textureVtxCount, int & faceCount);

////////////////////////////////////  E3D_Draw Function  ////////////////////////////////////////
void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb);
void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);
void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);

void E3D_draw_grid(FBDEV *fbdev, int sx, int sy, int us, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);
void E3D_draw_circle(FBDEV *fbdev, int r, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);

void E3D_draw_coordNavSphere(FBDEV *fbdev, int r, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);
void E3D_draw_coordNavFrame(FBDEV *fbdev, int size, bool minZ, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);
void E3D_draw_coordNavIcon2D(FBDEV *fbdev, int size, const E3D_RTMatrix &RTmatrix, int x0, int y0);


/* Mesh Rendering/Shading type */
enum E3D_SHADE_TYPE {
	E3D_FLAT_SHADING	=0,
	E3D_GOURAUD_SHADING	=1,
	E3D_WIRE_FRAMING	=2,
	E3D_TEXTURE_MAPPING	=3,
};


/* Global light vector */
E3D_Vector gv_vLight(0, 0, 1);

/* Global View TransformMatrix */

/*-------------------------------
	Class E3D_Material
--------------------------------*/
class E3D_Material {
public:
	/* Constructor */
	E3D_Material();

	/* Destructor */
	~E3D_Material();

	/* Init,Setdefault */
	void setDefaults();

	/* Print out */
	void print();


		      /* !!!--- Caution for Pointer Memebers ---!!! */

/* Note:
 * 1. For vector application: to assign any pointer member AFTER finishing allocating all vector
 *    intems, such as to avoid releasing those pointers unexpectedly when free and remalloc mem
 *    DURING resizing/growing_up the vector capacity.
 *    Example: It triggers to egi_imgbuf_free(img_kd..) when reallocating vecotr mem spaces.
 *
 * 2. push_back() of a vector WOULD LIKELY to trigger unexpected pointer_free/release.
 *    to avoid above situation, one could DestVector.resize() first to allocate all mem space,
 *    then assign item one by one: DestVector[i]=SrcVector[i].
 *
 * 3. TODO: If one map_kd(/ks/kd) file is referred/used by more than ONE material, it will be
 *    loaded into img_kd(/ks/ka) for each material respectively!  Seems redundant....
 */

public:
	/* Material name */
	string name;	  /* Material name */

	/* Colors: To keep same as obj file */
	E3D_Vector ka;  /* ambient color [0 1] ---> (RGB) [0 255] */
	//EGI_16BIT_COLOR acolor;
	E3D_Vector kd;  /* Diffuse color [0 1] ---> (RGB) [0 255] */
	//EGI_16BIT_COLOR dcolor;
	E3D_Vector ks;  /* specular color [0 1] ---> (RGB) [0 255] */
	//EGI_16BIT_COLOR scolor;

	/* Illumination model:
 	   0		Color on and Ambient off
	   1		Color on and Ambient on
 	   2		Highlight on
 	   3		Reflection on and Ray trace on
 	   4		Transparency: Glass on
 			Reflection: Ray trace on
 	   5		Reflection: Fresnel on and Ray trace on
	   6		Transparency: Refraction on
 			Reflection: Fresnel off and Ray trace on
	   7		Transparency: Refraction on
 			Reflection: Fresnel on and Ray trace on
	   8		Reflection on and Ray trace off
	   9		Transparency: Glass on
 			Reflection: Ray trace off
	  10		Casts shadows onto invisible surfaces
        */
	int 	     illum; /* [0-10] */

	/* --- NOT applied in setDefault and readMtlFile() --- */
	//E3D_Vector Tf;   /* The transmission filte (rgb)[0 1] */
	//float      Ni;   /* Transmission filter */
	//float	     Ns;   /* Specifies the specular exponent, defautl 0.0 */

	/* Transparency, use one of d/Tr */
	float       d;    /* The dissolve factor, as Non-transparency of the material, default 1.0, totally untransparent. */
	//float       Tr;   /* as for Transparency of the material, default 0.0,  */

	/* Textrue/Maps */
	string map_kd;  /* Map file path ---> img_kd */
	string map_ka;
	string map_ks;

	/* Texture imgbuf. TODO: here or at triGroup??? ---OK, HERE! */
	EGI_IMGBUF *img_kd; /* img_ka, img_ks */

	int end;
};

/*---------------------------
Set default for memebers.
----------------------------*/
void E3D_Material::setDefaults()
{
	illum=0;
	d=1.0;
	//Tr=0.0;

	kd.vectorRGB(COLOR_24TO16BITS(0xF0CCA8)); //WEGI_COLOR_PINK); /* Default kd */
	ka.vectorRGB(WEGI_COLOR_GRAY2); /* Default ka */
	ks.vectorRGB(WEGI_COLOR_GRAY); /* Default ks */

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
		/* map_Ka */
		else if( strcmp(pt,"map_Ka")==0 ) {
			material.map_ka=cstr_trim_space(saveptr);
		}
		/* Kd */
		else if( strcmp(pt,"Kd")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.kd.x, &material.kd.y, &material.kd.z); /* rgb[0 1] */
		}
		/* map_Kd */
		else if( strstr(pt,"map_Kd") ) {
			material.map_kd=cstr_trim_space(saveptr);
		}
		/* Ks */
		else if( strcmp(pt,"Ks")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.ks.x, &material.ks.y, &material.ks.z); /* rgb[0 1] */
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
		/* map_Ka */
		else if( strcmp(pt,"map_Ka")==0 ) {
			material.map_ka=cstr_trim_space(saveptr);
			egi_dpstd("material.map_ka: %s\n", material.map_ka.c_str());
		}
		/* Kd */
		else if( strcmp(pt,"Kd")==0 ) {
			sscanf(saveptr, "%f %f %f", &material.kd.x, &material.kd.y, &material.kd.z); /* rgb[0 1] */
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
		/* map_Ks */
		else if( strcmp(pt,"map_Ks")==0 ) {
			material.map_ks=cstr_trim_space(saveptr);
			egi_dpstd("material.map_ks: %s\n", material.map_ks.c_str());
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

	/* Check if last material is waiting for pushback. */
	if( !material.name.empty() ) {
		mtlList.push_back(material);

		/* Load imgbuf after push_back all material! see CAUTION in Class E3D_Material! */

		/* Increase counter */
		mcnt ++;
	}

	/* Load img_kd AFTER pushing back ALL materials! see CAUTION in Class E3D_Material! */
	for(int k=0; k<mcnt; k++) {
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

/*-----------------------------------------------------
	     Class E3D_PivotKeyFrame

Pivotal(Skeleton joint etc.) position data for the
time key frame.

1. RTxyz data may be relative, OR absolute, depending
   on different application scenarios.



In a .mot file: 'k' lines: (keyframe )
     "k  tf  rx ry rz  tx ty tz"
----------------------------------------------------*/
class E3D_PivotKeyFrames {
public:
	/* Constructor */
	E3D_PivotKeyFrames();

	class PivotFrame {
	    public:
		float tf;	   /* Time value, in float. to define frame time position. within [0 100].  */
		float rx, ry, rz;  /* Rotation RX,RY,RZ in degree */
		float tx, ty, tz;  /* Translation */
	};


public:
	vector<PivotFrame> kframes;
};



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

		/* Vector as point coord. */
		E3D_Vector pt;

		/* Ref Coord. ? */
		float u,v;

		/* Normal vector. Normalized!
		 * Note:
		 * 1. If each vertex has ONLY one normal value, save it here.
		 * 2. If each vertex has more than one normal value, then save it
		 *    to triList[].vtx[].nv
		 * 3. Init. to all 0! as in E3D_Vector().
		 */
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

		/* Struct Vertx */
		struct Vertx {
			int   index;	/* index as of vtxList[] */
			float u,v;	/* Ref coord. for texture. */
			E3D_Vector vn;  /* Vertex normal, Normalized!
					 * 1. Init all 0!
					 * 2. If each vertex has ONLY one normal value, then save to vtxList[].normal.
					 */
		};
		Vertx vtx[3];		/* 3 points to form a triangle */

		/* Normal, Triangle face normal */
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

	/* :: Class Material --- See Class E3D_Material */

	/* :: Class TriGroup, NOT for editting!!!  */
	/* Note:
	 *	1. All TriGroups are under the same object/global COORD!
	 */
	class TriGroup {
	public:
		TriGroup() { setDefaults(); };
		void setDefaults() {
			omat.identity();
			name.clear();
			hidden=false;
			mtlID=-1;
			tcnt=0; stidx=0; etidx=0;
		}

		void print() {
			printf("   <<< TriGroup %s >>>\n",name.c_str());
			omat.print(name.c_str());
			printf("Material ID: %d\n", mtlID);
			printf("Totally %d trianges in triList[%d - %d].\n", tcnt, stidx, etidx);
		}

		E3D_Vector	pivot;		/* Pivot/(rotation center) of the TriGroup relative to its superior/parent.
						 * It also under Object COORD!
						 * Initially omat.pmat[9-11] == pivot.
						 */
		/* OK.  translation of omat is same as oxyz */
		E3D_RTMatrix	omat;		/* omat.pmat[0-8] is the Orientation/attitude of the TriGroup Origin (local Coord) relative
						 * to its INITIAL position which USUALLY be the same as (align with) its superior node's COORD,
						 * and EXCEPT its tx,ty,tz value, it's center/origin/pivotal of the TriGroup.
						 * omat.pmat[9-11](tx,ty,tz) are under the SAME COORD as all other vertices in vtxList[]!
						 * Before rotate_transform the TriGroup, (tx,ty,tz) should be set as rotation center.
						 *
						 * The purpose is to fulfill hierarchical TRMatrix passing_down:
						 * 		Parent_Coord * Parent_omat *This_omat[relative_txtytz] = This_Coord.  <--
						 *
						 * 1. Init as identity().
						 * 1A. In cloneMesh(): the omat MUST copy each time in order to reset/reinit it.
						 * 1B. In transformMesh(): omat.pmat[9-11](pivotal) MUST be transformed same as other vertices.
						 * 2. If the superior/parent node transforms, all its subordinate nodes
						 *    MUST transform also. While the subordinate node's transformation will NOT
						 *    affect its superior node.
					 	 * 3. In .obj file: 'x', 'm' lines:
						 *    x tx ty tz    ---> RTMatrix translation omat.pmat[9,10,11]: tx,ty,tz
						 *    m m1 ... m9  ----> Rotation Matrix:
						 *			 omat.pmat[0,1,2]: m11, m12, m13
						 *                       omat.pmat[3,4,5]: m21, m22, m23
						 *			 omat.pmat[6,7,8]: m31, m32, m33
						 * 4. In .mot file: 'k' lines: (keyframe )
						 *    k  tf  rx ry rz  tx ty tz
						 *     tf:   Time value, in float.  [0 100]
						 *     rx ry rz: Rotation RX,RY,RZ in degree.
						 *     tx ty tz: translation
						 */

		string		name;		/* Name of the sub_object group (of triangles) */
		bool		hidden;		/* Hidden or visible. */
		int		mtlID;		/* Material ID, as of mtlList[x]
						 * If <0, invalid!
						 */

		/* XXX Imgbuf loaded from material texture map files. */
		//EGI_IMGBUF *img_kd;  // NOW only diffuse.  ka, ks ...

		/* Define group include triangles from triList[] to triList[] */
		unsigned int	tcnt;  		/* Total trianlges in the group */
		unsigned int	stidx;  	/* Start/Begin index of triList[] */
		unsigned int	etidx;  	/* End index of triList[], NOT incuded!!!
				 		 * !!! eidx=sidex+sidx !!!
						 */
	};

	/* :: Class OptimationParameters */

	/* :: Class AABB (axially aligned bounding box */
	class AABB {
	public:
		E3D_Vector vmin;
		E3D_Vector vmax;
		void print() {
			egi_dpstd("AABDD: vmin{%f,%f,%f}, vmax{%f,%f,%f}.\n",
						vmin.x,vmin.y,vmin.z, vmax.x,vmax.y,vmax.z);
		}
		float xSize() const { return vmax.x -vmin.x; }
		float ySize() const { return vmax.y -vmin.y; }
		float zSize() const { return vmax.z -vmin.z; }
		void toHoldPoint(const E3D_Vector &vpt);		/* Revise vmin/vmax to make AABB contain the point. */
	};

	/* Constructor E3D_TriMesh() */
	E3D_TriMesh();
	E3D_TriMesh(const E3D_TriMesh &tmesh);
	E3D_TriMesh(int nv, int nt);
	E3D_TriMesh(const char *fobj);
	E3D_TriMesh(const E3D_TriMesh &smesh, float r);

	/* Destructor ~E3D_TriMesh() */
	~E3D_TriMesh();

	/* Init member/parameters/variabls. */
	void initVars();

	/* Functions: Print for debug */
	void printAllVertices(const char *name);
	void printAllTriangles(const char *name);
	void printAllTextureVtx(const char *name);
	void printAABB(const char *name);

	/* TEST FUNCTIONS: */
	void setVtxCount(int cnt) { if(cnt>=0) vcnt=cnt; };
	void setTriCount(int cnt) { if(cnt>=0) tcnt=cnt; };

	/* Function: Count vertices and triangles */
	int vtxCount() const { return vcnt; };
	int triCount() const { return tcnt; };

	/* Function: Get AABB size */
	float AABBxSize() {
		return aabbox.xSize();
	}
	float AABBySize() {
		return aabbox.ySize();
	}
	float AABBzSize() {
		return aabbox.zSize();
	}
	float AABBdSize() {  /* diagonal size */
		float x=aabbox.xSize();
		float y=aabbox.ySize();
		float z=aabbox.zSize();
		return sqrt(x*x+y*y+z*z);
	}

	/* Function: Retrun ref. of indexed vertex */
	E3D_Vector & vertex(int vtxIndex) const;
	E3D_Vector vtxsCenter(void) const {
		return vtxscenter;
	}
	E3D_Vector aabbCenter(void) const {
		return 0.5*(aabbox.vmax-aabbox.vmin);
	}

	/* Function: Calculate center point of all vertices */
	void updateVtxsCenter()
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
	/* Function: Move VtxsCenter to current origin. */
	void moveVtxsCenterToOrigin()
	{
		/* Update all vertex XYZ */
		for(int i=0; i<vcnt; i++) {
			vtxList[i].pt.x -=vtxscenter.x;
			vtxList[i].pt.y -=vtxscenter.y;
			vtxList[i].pt.z -=vtxscenter.z;
		}

		/* Update TriGroup omat.pmat[9-11], as pivotal of the TriGroup. */
		for(unsigned int k=0; k<triGroupList.size(); k++) {
			triGroupList[k].omat.pmat[9] -= vtxscenter.x;
			triGroupList[k].omat.pmat[10] -= vtxscenter.y;
			triGroupList[k].omat.pmat[11] -= vtxscenter.z;

			triGroupList[k].pivot -= vtxscenter;
		}

		/* Update AABB */
		aabbox.vmax.x-=vtxscenter.x;
		aabbox.vmax.y-=vtxscenter.y;
		aabbox.vmax.z-=vtxscenter.z;
		aabbox.vmin.x-=vtxscenter.x;
		aabbox.vmin.y-=vtxscenter.y;
		aabbox.vmin.z-=vtxscenter.z;

		/* Reset vtxscenter */
		vtxscenter.x =0.0;
		vtxscenter.y =0.0;
		vtxscenter.z =0.0;
	}

	/* Function: Calculate AABB aabbox */
	void updateAABB(void) {

		/* Reset first */
		aabbox.vmin.zero();
		aabbox.vmax.zero();

		if( vcnt<1 ) return;

		/* To hold all vertices */
		aabbox.vmin=vtxList[0].pt; /* Init with vtxList[0].pt */
		aabbox.vmax=vtxList[0].pt;
		for( int i=1; i<vcnt; i++ )
			aabbox.toHoldPoint(vtxList[i].pt);
	}

	/* Function: Move VtxsCenter to current origin. */
	void moveAabbCenterToOrigin()
	{
		float xc=0.5*(aabbox.vmax.x+aabbox.vmin.x);
		float yc=0.5*(aabbox.vmax.y+aabbox.vmin.y);
		float zc=0.5*(aabbox.vmax.z+aabbox.vmin.z);

		for(int i=0; i<vcnt; i++) {
			vtxList[i].pt.x -=xc;
			vtxList[i].pt.y -=yc;
			vtxList[i].pt.z -=zc;
		}

		/* Move AABB */
		aabbox.vmax.x-=xc;
		aabbox.vmax.y-=yc;
		aabbox.vmax.z-=zc;

		aabbox.vmin.x-=xc;
		aabbox.vmin.y-=yc;
		aabbox.vmin.z-=zc;

		/* Set objmat.pmat[9-11] as origin */
		objmat.pmat[9]=0.0;
		objmat.pmat[10]=0.0;
		objmat.pmat[11]=0.0;

		/* Move TriGroup omat */
		for(unsigned int k=0; k<triGroupList.size(); k++) {
			triGroupList[k].omat.pmat[9]  -=xc;
			triGroupList[k].omat.pmat[10] -=yc;
			triGroupList[k].omat.pmat[11] -=zc;

			triGroupList[k].pivot.x -=xc;
			triGroupList[k].pivot.y -=yc;
			triGroupList[k].pivot.z -=zc;
		}

		/* Move vtxscenter */
                vtxscenter.x -=xc;
                vtxscenter.y -=yc;
                vtxscenter.z -=zc;
	}

	/* Function: Add vertices into vtxList[] */
	int addVertex(const Vertex &v);
	int addVertexFromPoints(const E3D_POINT *pts, int n); /* Add veterx from points */
	void reverseAllVtxZ(void);  /* Reverse Z value/direction for all vertices, for coord system conversion */

	/* Function: Add triangle into triList[] */
	int addTriangle(const Triangle *t);
	int addTriangleFromVtxIndx(const int *vidx, int n);    /* To add/form triangles with vertex indexes list. */

	/* Function: scale up/down mesh/vertices, current vtx coord origin as pivotal.  */
	void scaleMesh(float scale);

	/* Function: Transform vertices/normals */
	void transformVertices(const E3D_RTMatrix  &RTMatrix);
	void transformAllVtxNormals(const E3D_RTMatrix  &RTMatrix);   /* vtxList[].normal */
	void transformAllTriNormals(const E3D_RTMatrix  &RTMatrix);  /* triList[].normal + trilist[].vtx[].nv */
	void transformAABB(const E3D_RTMatrix  &RTMatrix);
	void transformMesh(const E3D_RTMatrix  &RTMatrix, const E3D_RTMatrix &ScaleMatrix); /* Vertices+TriNormals */

	/* Function: Calculate normals for all (triangles)faces. */
	void updateAllTriNormals();

	/* Funtion: Compute all vtxList[].normal, by averaging all triList[].vtx[].vn refering the vertex.  */
	void computeAllVtxNormals(void);

	/* Function: Reverse triangle/vertex normals */
	void reverseAllTriNormals(bool reverseVtxIndex);	/* Reverse triList[].normal + trilist[].vtx[].nv ?? */
	void reverseAllVtxNormals(void);			/* Revese vtxList[].normal */

	/* Check if vertex normals are available! */
	bool vtxNormalIsZero(void) {
		return vtxList[0].normal.isZero();
	}

	/* Function: clone vertices/trianges from the tmesh.  */
	void cloneMesh(const E3D_TriMesh &tmesh); /* !!! Some data are NOT cloned and replaced! */

	/* Function: Project according to Projection matrix */
	// int  projectPoints(E3D_Vector vpts[], int np, const E3D_ProjMatrix &projMatrix) const;

	/* Function: Read texture image into textureImg */
	void readTextureImage(const char *fpath, int nw);

	/* Function: Get material ID */
	int getMaterialID(const char *name) const;

	/* Function: Draw wires/faces */
	void drawMeshWire(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix) const ;
	void drawMeshWire(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix) const;

	/* Function: Draw AABB as wire. */
	void drawAABB(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix, const E3D_ProjMatrix projMatrix) const ;

	/* Function: Render all triangles */
	void renderMesh(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix) const ;
	/* TODO: to be oboselet, to apply TriMesh.objmat instead ? */
	void renderMesh(FBDEV *fbdev, const E3D_RTMatrix &VRTmatrix, const E3D_RTMatrix &ScaleMatrix) const;

private:
	/* All members inited at E3D_TriMesh() */
	E3D_Vector 	vtxscenter;	/* Center of all vertices, NOT necessary to be sAABB center! */

	#define		TRIMESH_GROW_SIZE	64	/* Growsize for vtxList and triList --NOT applied!*/
	int		vCapacity;	/* MemSpace capacity for vtxList[] */
	int		vcnt;		/* Vertex counter */
	Vertex  	*vtxList;	/* Vertex array */

	int		tCapacity;	/* MemSpace capacity for triList[] */
	int		tcnt;		/* Triangle counter  */
	Triangle	*triList;	/* Triangle array */

public:
	AABB		aabbox;		/* Axially aligned bounding box */
	//int mcnt;

	E3D_RTMatrix    objmat;		/* Orientation/Position of the TriMesh object relative to Global/System COORD.
					 * 1. Init as identity() in initVars().
					 * 2. moveAabbCenterToOrigin() will reset objmat.pmat[9-11] all as 0.0!
					 */

	E3D_Material	defMaterial;	/* Default material:
					 * 1. If mtlList[] is empty, then use this as default material
					 * 2. defMaterial.img_kd re_assigned to textureImg in E3D_TriMesh::readTextureImage()
				         */

	vector<E3D_Material>  mtlList;	/* Material list. */
	vector<TriGroup>  triGroupList;	/* trimesh group array
					 * 1. At least one TriGroup, including all triangles.
					 * 2. If there's NO triGroup explictly defined in .obj file, then all mesh belongs to
					 *    TriGroupList[0], and if mtlList is NOT empty, use mtlList[0] for the only TriGroup,
					 *    otherwise use defMaterial.
					 */

	E3D_SHADE_TYPE   shadeType;	/* Rendering/shading type, default as FLAT_SHADING */
	EGI_IMGBUF 	*textureImg;	/* Texture imgbuf
					 * 1. E3D_TriMesh::readTextureImage() to read image, and then
					 *    assign it to defMaterial.img_kd.
					 */
	bool		backFaceOn;	/* If true, display back facing triangles.
					 * If false, ignore it.
					 * Default as false.
					 */
	EGI_16BIT_COLOR   bkFaceColor;  /* Faceback face color for flat/gouraud/wire shading.
					 * Default as 0(BLACK).
					 */
	EGI_16BIT_COLOR	wireFrameColor; /* Color for wire frame
					 * Default as 0(BLACK).
					 */

	int		faceNormalLen;   /* face normal length, if >0, renderMesh() will show the normal line. */
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


/* Revise vmin/vmax to make AABB contain the point. */
void E3D_TriMesh::AABB::toHoldPoint(const E3D_Vector &pt)
{
	if(pt.x<vmin.x) vmin.x=pt.x;
	if(pt.x>vmax.x) vmax.x=pt.x;
	if(pt.y<vmin.y) vmin.y=pt.y;
	if(pt.y>vmax.y) vmax.y=pt.y;
	if(pt.z<vmin.z) vmin.z=pt.z;
	if(pt.z>vmax.z) vmax.z=pt.z;
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

	/* Init other params */
	//textureImg=NULL;
	//shadeType=E3D_FLAT_SHADING;
        //backFaceOn=false;
        //bkFaceColor=WEGI_COLOR_BLACK;
	initVars();
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

        /* Init other params */
        //textureImg=NULL;
        //shadeType=E3D_FLAT_SHADING;
        //backFaceOn=false;
        //bkFaceColor=WEGI_COLOR_BLACK;
	initVars();
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
   AND_2. When we read face data, all their vertice are already
   in vtxList[].
   ( We can also read in all vertice('v') data and put
     into vtxList[] first, then read all face ('f') data. )
2. All polygons are divided into triangles.
   Example: A face has MAX.4 vertices!(a quadrilateral)
   and a quadrilateral will be stored as 2
   triangles, recursive for 5,6...
3. Face(triangle) normals are not provided in obj file,
   they should be computed later.
4. If no 'g' group defined, then a default group will
   be created and push into triGroupList<>.
5. If no .mtl file provided, then the default material
   of E3D_TriMesh.defMaterial to be used.

			--- OBJ File Format ---
v		vertex:  xyz
vt		vertex texture coordinate: uv[w]
vn		vertex normal: xyz
o		object name
s		smoothing group: on, off
g		su_object group

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

	E3D_Vector *tuvList;  		/* Texture vertices uv list */
	int	    tuvListCnt=0;
	E3D_Vector *vnList;		/* Vertex normal List */
	int	    vnListCnt=0;

	/* TODO: NOW object 'o' and group 'g' all treated as triGroup. any they CAN NOT appear at same time! */
	TriGroup    triGroup;
	int	    groupCnt=0;	/* Triangles group counter */
	int	    groupStidx=0;	/* Group start index as of triList[sidx] */
	int	    groupEtidx=0;	/* Group end index as of triList[eidx-1] */

	/* 0. Init params */
	vcnt=0;
	tcnt=0;
	initVars();
        //textureImg=NULL;
        //shadeType=E3D_FLAT_SHADING;
	//backFaceOn=false;
	//bkFaceColor=WEGI_COLOR_BLACK;

	/* 1. Read statistic of obj data */
	egi_dpstd("Read obj file info...\n");
	if( readObjFileInfo(fobj, vertexCount, triangleCount, vtxNormalCount, textureVtxCount, faceCount) !=0 )
		return;

	egi_dpstd("'%s' Statistics:  Vertices %d;  vtxNormals %d;  TextureVtx %d; Faces %d; Triangles %d.\n",
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
				g eya
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
			/* Change frist char 'f' to ' ' */
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

			   /* NOW: k is total number of vertices in this face. */

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
					egi_dpstd("!!! WARNING !!! vtxIndex omitted, data may be ERROR!\n");
					/* Need to reorder data from to vtxIndex/textureIndex(WRONG!, it's textureIndex/normalIndex)
					   vtxIndex/textureIndex/normalIndex(CORRECT!) */
					normalIndex[k]=textureIndex[k];
					textureIndex[k]=vtxIndex[k];
					vtxIndex[k]=-1;		/* vtxIndex omitted!!! */
				}
				/* FB2.5 In case "5/0/", -- m==2, normalIndex omitted!  */
				else if( m==2 ) { /* pt[0]=='/' also has m==2! */
//					egi_dpstd("vtxNormalIndex omitted!\n");
					normalIndex[k]=0-1;
				}

				/* FB2.6 Get next indices */
				pt=strtok_r(NULL, " ", &savept);

			   } /* End for(k) */

			   /* NOW: k is total number of vertices in this face. */

#if 1 /* Test: plane.obj ------ */
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
			  if( k>0 && groupCnt==4 ) {
			  	 if(   vtxList[ vtxIndex[0] ].pt.x> 240.0 && vtxList[ vtxIndex[0] ].pt.x < 458.0
			  	    && vtxList[ vtxIndex[0] ].pt.z<64   ) // right wing slat
				printf("Face line:  %s\n",strline2);
			   }
#endif

			   /*  FB3. Assign to triangle vtxindex */
			   if( k>2 ) {
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
				printf("%s: k=%d has parsed vtxIndex <0! vtxIndex[0,1,2,3]: %d, %d, %d, %d \n",
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
	   egi_dpstd("vtxNormalIndex omitted! each vertex has same normal value as vtxList[].normal!\n");
	   for( int k=0; k<vertexCount; k++) {
		 if(k>=vnListCnt)
			break;
		 vtxList[k].normal=vnList[k];
	  }
        }
	else
	   egi_dpstd("vtxNormalIndex assigned! each vertex may have more than one normal values, as in triList[].vtx[].vn!\n");

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

	egi_dpstd("Finish reading obj file: %d Vertices, %d Triangels. %d TextureVertices. %d TriGroups, %d Materis.\n",
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

/* Init public members */
	objmat.identity();
        textureImg=NULL;
        shadeType=E3D_FLAT_SHADING;
        backFaceOn=false;
        bkFaceColor=WEGI_COLOR_BLACK;
	wireFrameColor=WEGI_COLOR_BLACK;
	faceNormalLen=0;

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
Calculate normals for all triangles
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
			egi_dpstd("!!!WARNING!!! triList[%d].vtx[%d] is <0!\n", i,j);
			vtxIndex[j]=0;
		}
	   }

	   v01=vtxList[vtxIndex[1]].pt-vtxList[vtxIndex[0]].pt;
	   v12=vtxList[vtxIndex[2]].pt-vtxList[vtxIndex[1]].pt;

	   triList[i].normal=E3D_vector_crossProduct(v01,v12);
	   if( triList[i].normal.normalize() <0 ) {
		egi_dpstd("Triangle triList[%d] error! or degenerated!\n", i);
		vtxList[vtxIndex[0]].pt.print();
		vtxList[vtxIndex[1]].pt.print();
		vtxList[vtxIndex[2]].pt.print();
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
   the projection plane and it should be picked out.

@Tri:		Input triangle.
@np:		Number of points.
@projMatrix:	The projectio matrix.

Return:
	0	OK
	>0 	Point out of the viewing frustum!
		OR too small fabs(vpts[k].z)!
	<0	Fails
---------------------------------------------------------------*/
int projectPoints(E3D_Vector vpts[], int np, const E3D_ProjMatrix & projMatrix)
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
         	vpts[k].x = roundf( vpts[k].x/vpts[k].z*projMatrix.dv  /* Xv=Xs/Zs*d */
                                    +projMatrix.winW/2 );              /* Adjust viewplane origin to window X center */

         	vpts[k].y = roundf( vpts[k].y/vpts[k].z*projMatrix.dv  /* Yv=Ys/Zs*d */
                                    +projMatrix.winH/2 );              /* Adjust viewplane origin to window Y center */
		//Keep vpts[k].z

	   }

	}

	return ret;
}


/*-----------------------------------------------------
Draw mesh wire.
fbdev:		Pointer to FBDEV
projMatrix:	Projection Matrix

View direction: TriMesh Global_Coord. Z axis.
-----------------------------------------------------*/
void E3D_TriMesh::drawMeshWire(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix) const
{
	int vtidx[3];	    /* vtx index of a triangle */
//	EGI_POINT   pts[3];
	E3D_Vector  vpts[3];
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); /* View direction */
	float	    vProduct;   /* dot product */

	/* NOTE: Set fbdev->flipZ, as we view from -z---->+z */
	fbdev->flipZ=true;

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
void E3D_TriMesh::drawAABB(FBDEV *fbdev, const E3D_RTMatrix &VRTMatrix, const E3D_ProjMatrix projMatrix) const
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
	for( int k=0; k<8; k++)
		vpts[k]= vpts[k]*VRTMatrix;

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


/*---------------------------------------------------------------
Render/draw the whole mesh by filling all triangles with FBcolor.
View_Coord:	Same as Global_Coord.
View direction: View_Coord Z axis.

@fbdev:		Pointer to FBDEV
@projMatrix	Projection Matrix

Note:
1. For E3D_GOURAUD_SHADING:
   If the mesh has NO vertex normal data, then triangle face
   normal will be used, so it's same effect as E3D_FLAT_SHADING
   in this case.
----------------------------------------------------------------*/

///////////////   Apply triGroupList[] and Material color/map     ///////////////////

/* TODO:
   1. NOW tgRTMat is applied ONLY for case E3D_TEXTURE_MAPPING and E3D_FLAT_MAPPING.
 */
void E3D_TriMesh::renderMesh(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix) const
{
	int j,k;
	unsigned int i,n;
	int vtidx[3];  				/* vtx index of a triangel */
	E3D_Vector  vpts[3];			/* Projected 3D points */
	EGI_POINT   pts[3];			/* Projected 2D points */
	E3D_Vector  trinormal;  		/* Face normal of a triangle */
	E3D_Vector  vView(0.0f, 0.0f, 1.0f); 	/* View direction */
	float	    vProduct;   		/* dot product */
	EGI_16BIT_COLOR	 vtxColor[3];		/* Triangle 3 vtx color */

	bool	    IsBackface=false;		/* Viewing the back side of a Trimesh. */
//	EGI_16BIT_COLOR	 bkFaceColor=WEGI_COLOR_RED; //DARKRED;  /* Default backface color, if applys. */
	E3D_POINT cpt;	    			/* Gravity center of triangle */

	/* render color */
	EGI_16BIT_COLOR color; /* NOW: Diffuse color.  TODO others: ka,ks */
	/* Material color */
	EGI_16BIT_COLOR mcolor;
	//EGI_8BIT_CCODE codeY=egi_color_getY(color);
	/* Defualt texture map */
	EGI_IMGBUF *imgKd=NULL; /* NOW: Diffuse map.  TODO others: imgKa, imgKs */

	/* RTMatrixes for the Object and TriGroup */
	E3D_RTMatrix	tgRTMat; //tgRotMat; /* TriGroup RTmatrix, for Pxyz(at TriGroup Coord) * TriGroup->object */
	E3D_Vector	tgOxyz;		   /* TriGroup origin/pivot XYZ */
	E3D_RTMatrix	ltRotMat;	   /* For light product, to be zeroTranslation() */

//	objmat.print("objmat");

	/* -------TEST: Project to Z plane, Draw X,Y only */
	for(n=0; n<triGroupList.size(); n++) {  /* Tranverse all Trigroups */

	    /* 0. Check visibility */
	    if( triGroupList[n].hidden ) {
		egi_dpstd("triGroupList[%d]: '%s' is set as hidden!\n", n, triGroupList[n].name.c_str());
		continue;
	    }

	    egi_dpstd("Rendering triGroupList[%d]: '%s' , ", n, triGroupList[n].name.c_str());

	    /* 1. Get material color(kd) and map(img_kd)  */
	    if( triGroupList[n].mtlID < 0){
		printf("with defMaterial, ...\n");
		mcolor=defMaterial.kd.color16Bits(); /* Diffuse color */
	    	imgKd = defMaterial.img_kd;
	    }
	    else {
		printf("with material mtlList[%d] ...\n", triGroupList[n].mtlID);
		mcolor=mtlList[ triGroupList[n].mtlID ].kd.color16Bits();
		imgKd=mtlList[ triGroupList[n].mtlID ].img_kd;
	    }

	    /* 2. Extract TriGroup rotation matrix and rotation origin */

	    /* To combine RTmatrix:  Pxyz(at TriGroup Coord) * TriGroup->object * object->Global */
	    tgRTMat = triGroupList[n].omat*objmat;

	    /* Light RotMat */
	    ltRotMat = tgRTMat;
 	    ltRotMat.zeroTranslation(); /* !!! */

	    /* TriGroup (original) pivot, under object COORD. */
	    tgOxyz = triGroupList[n].pivot;

	    /* 3. Traverse and render all triangles in the triGroupList[n]
	     *  TODO: sorting, render Tris from near to far field.... */
	    for(i=triGroupList[n].stidx; i<triGroupList[n].etidx; i++) {

		/* R1. Pick out back_facing triangles if !backFaceOn.
		 * Note:
		 *    A. If you turn off back_facing triangles, it will be transparent there.
		 *    B. OR use other color for the back face. For texture mapping, just same as front side.
		 *    c. tgRTMat to be counted in.
		 */
		vProduct=vView*(triList[i].normal*ltRotMat); // *(-1.0f); // -vView as *(-1.0f);
		/* Note: Because of float precision limit, vProduct==0.0f is NOT possible. */
		if ( vProduct > -VPRODUCT_EPSILON ) {  /* >=0.0f */
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

		/* R2. Copy triangle vertices, and project to viewPlan COORD. */
		vtidx[0]=triList[i].vtx[0].index;
		vtidx[1]=triList[i].vtx[1].index;
		vtidx[2]=triList[i].vtx[2].index;

		vpts[0]=vtxList[vtidx[0]].pt;
		vpts[1]=vtxList[vtidx[1]].pt;
		vpts[2]=vtxList[vtidx[2]].pt;

#if 1		/* R2B. TriGroup local Rotation computation. */

		/* vpts under TriGroup COORD/Pivot */
		vpts[0] -= tgOxyz;
		vpts[1] -= tgOxyz;
		vpts[2] -= tgOxyz;

		/* Transform vpts[]: TriGroup->object; object->Global */
		E3D_transform_vectors(vpts, 3, tgRTMat);

     #if 0  /* XXX Back to under object/global coord. XXX */
		vpts[0] += tgOxyz;
		vpts[1] += tgOxyz;
	   	vpts[2] += tgOxyz;
     #endif
#endif

		/* R2C. Project to screen/view plane */
		if( projectPoints(vpts, 3, projMatrix) !=0 )
			continue;

		/* TEST: Check range, ONLY simple way.... TODO: by projectPoints() */
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

		/* R3. Get 2D points on the screen/view plane  */
		pts[0].x=roundf(vpts[0].x); pts[0].y=roundf(vpts[0].y);
		pts[1].x=roundf(vpts[1].x); pts[1].y=roundf(vpts[1].y);
		pts[2].x=roundf(vpts[2].x); pts[2].y=roundf(vpts[2].y);

		/* R4. ---------TEST: Set pixz zbuff. Should init zbuff[] to INT32_MIN: -2147483648 */
		/* A simple test: Triangle center point Z for all pixles on the triangle.  TODO ....*/
		//gv_fb_dev.pixz=roundf((vtxList[vtidx[0]].pt.z+vtxList[vtidx[1]].pt.z+vtxList[vtidx[2]].pt.z)/3.0);
		fbdev->pixz=roundf((vpts[0].z+vpts[1].z+vpts[2].z)/3.0);
		/* !!!! Views from -z ----> +z */
		fbdev->pixz = -fbdev->pixz;

		/* R5. Draw triangles with specified shading type. */
		switch( shadeType ) {
		   case E3D_FLAT_SHADING:
			/* Calculate light reflect strength for the TriFace:  TODO: not correct, this is ONLY demo. */
			vProduct=gv_vLight*(triList[i].normal*ltRotMat);  // *(-1.0f); // -vLight as *(-1.0f);
			//if( vProduct >= 0.0f )
			if(IsBackface) {
				vProduct=-vProduct;
				color=bkFaceColor;
			}
			else
				color=mcolor;

			if( vProduct > -VPRODUCT_EPSILON )
				vProduct=0.0f;
			else /* Flip to get vProduct absolute value for luma */
				vProduct=-vProduct;
			/* Adjust luma for pixcolor as per vProduct. */
			fbdev->pixcolor=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
			//fbdev->lumadelt=(vProduct-1.0f)*240.0) +50; /* MUST reset later */
			//egi_dpstd("vProduct: %e, LumaY: %d\n", vProduct, (unsigned int)egi_color_getY(fbdev->pixcolor));

			#if 0 /* Fill the TriFace */
			draw_filled_triangle(fbdev, pts);
			#else /* pixZ applys */
			draw_filled_triangle2(fbdev,pts[0].x, pts[0].y,
						    pts[1].x, pts[1].y,
						    pts[2].x, pts[2].y,
						    /* Views from -z ----> +z  */
						    -vpts[0].z, -vpts[1].z, -vpts[2].z );
			#endif

			break;

		   case E3D_GOURAUD_SHADING:
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
				/* Case_A: Use vtxList[].normal */
				//if(triList[i].vtx[0].vn.isZero())
				if(vtxList[ triList[i].vtx[0].index ].normal.isZero()==false) {
				   for(k=0; k<3; k++) {
			      		vProduct=gv_vLight*(vtxList[ triList[i].vtx[k].index ].normal);
			   		//if( vProduct >= 0.0f )
					if(IsBackface) {
						vProduct=-vProduct;
						color=bkFaceColor;
					}
					else
						color=mcolor;

			   		if( vProduct > -VPRODUCT_EPSILON )
						vProduct=0.0f;
			   		else /* Flip to get vProduct absolute value for luma */
						vProduct=-vProduct;
			   		/* Adjust luma for color as per vProduct. */
			   		vtxColor[k]=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
				   }
				}
			   	/* Case_B: Use triList[].vtx[].vn */
			   	else if(triList[i].vtx[0].vn.isZero()==false) {
				   for(k=0; k<3; k++) {
			      		vProduct=gv_vLight*(triList[i].vtx[k].vn);  // *(-1.0f); // -vLight as *(-1.0f);
			   		//if( vProduct >= 0.0f )
					if(IsBackface) {
						vProduct=-vProduct;
						color=bkFaceColor;
					}
					else
						color=mcolor;

			   		if( vProduct > -VPRODUCT_EPSILON )
						vProduct=0.0f;
			   		else /* Flip to get vProduct absolute value for luma */
						vProduct=-vProduct;
			   		/* Adjust luma for color as per vProduct. */
			   		vtxColor[k]=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
				   }
				}
				/* Case_C: AS FLAT_SHADING, Use face normal triList[].normal. */
				else {
				   //for(k=0; k<3; k++) {
			      		vProduct=gv_vLight*(triList[i].normal);  // *(-1.0f); // -vLight as *(-1.0f);
			   		//if( vProduct >= 0.0f )
					if(IsBackface) {
						vProduct=-vProduct;
						color=bkFaceColor;
					}
					else
						color=mcolor;

			   		if( vProduct > -VPRODUCT_EPSILON )
						vProduct=0.0f;
			   		else /* Flip to get vProduct absolute value for luma */
						vProduct=-vProduct;
			   		/* Adjust luma for color as per vProduct. */
			   		vtxColor[0]=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
					vtxColor[2]=vtxColor[1]=vtxColor[0];
				   //}
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

			break;

		   case E3D_WIRE_FRAMING:
			/*  Calculate light reflect strength for the TriFace:  TODO: not correct, this is ONLY demo.  */
			vProduct=gv_vLight*(triList[i].normal);  // *(-1.0f); // -vLight as *(-1.0f);
			//if( vProduct >= 0.0f )
			if(IsBackface) {
				break;
				vProduct=-vProduct;
				color=bkFaceColor;
			}
			else
				color=wireFrameColor;

			if( vProduct > -VPRODUCT_EPSILON )
				vProduct=0.0f;
			else /* Flip to get vProduct absolute value for luma */
				vProduct=-vProduct;

			/* Adjust luma for pixcolor as per vProduct. */
			fbdev->pixcolor=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );
			//fbdev->lumadelt=(vProduct-1.0f)*240.0) +50; /* Must reset later.. */
			//egi_dpstd("vProduct: %e, LumaY: %d\n", vProduct, (unsigned int)egi_color_getY(fbdev->pixcolor));

			#if 0/* Draw line: OR draw_line_antialias() */
			draw_line(fbdev, pts[0].x, pts[0].y, pts[1].x, pts[1].y);
			draw_line(fbdev, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
			draw_line(fbdev, pts[0].x, pts[0].y, pts[2].x, pts[2].y);
			#else
			draw_filled_triangle_outline(fbdev, pts);
			#endif
			break;

		   case E3D_TEXTURE_MAPPING:
			/* Note: If backFaceOn, then backface texutre is also SAME as frontface texture. */
			/* IF: No texture, apply flat_shading then */
			if( imgKd==NULL ) {  // textureImg==NULL ) { textureImg assigned to defMaterial.img_kd
			   /* Calculate light reflect strength for the TriFace:  TODO: not correct, this is ONLY demo. */
			   vProduct=gv_vLight*(triList[i].normal*ltRotMat);  // *(-1.0f); // -vLight as *(-1.0f);
			   //if( vProduct >= 0.0f )
			   if( vProduct > -VPRODUCT_EPSILON )
				vProduct=0.0f;
			   else /* Flip to get vProduct absolute value for luma */
				vProduct=-vProduct;

			   /* Adjust luma for pixcolor as per vProduct. */
			   color=mcolor;
			   fbdev->pixcolor=egi_colorLuma_adjust(color, (int)roundf((vProduct-1.0f)*240.0) +50 );

			   draw_filled_triangle(fbdev, pts);
			}
			/* ELSE: Apply texture map */
			else {
			   /* Calculate light reflect strength for the TriFace:  TODO: not correct, this is ONLY demo.  */
			   vProduct=gv_vLight*(triList[i].normal*ltRotMat);  // *(-1.0f); // -vLight as *(-1.0f);
			   //if( vProduct >= 0.0f )
			   if( vProduct > -VPRODUCT_EPSILON )
				vProduct=0.0f;
			   else /* Flip to get vProduct absolute value for luma */
				vProduct=-vProduct;

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
			  #else /* OPTION_3: Barycentric coordinates mapping, INT type x/y. */
		        	egi_imgbuf_mapTriWriteFB3(imgKd, fbdev,
					/* u0,v0,u1,v1,u2,v2,  x0,y0, x1,y1, x2,y2 */
        	                        triList[i].vtx[0].u, 1.0-triList[i].vtx[0].v,  /* 1.0-x: Adjust uv ORIGIN */
                	                triList[i].vtx[1].u, 1.0-triList[i].vtx[1].v,
                        	        triList[i].vtx[2].u, 1.0-triList[i].vtx[2].v,
                                	roundf(pts[0].x), roundf(pts[0].y),
	                                roundf(pts[1].x), roundf(pts[1].y),
        	                        roundf(pts[2].x), roundf(pts[2].y)
                	        );
			  #endif
			}

			/* Reset lumadelt */
			fbdev->lumadelt=0;
			break;

		   default:
			break;

		} /* End switch */
	   } /* End for(i) */
	} /* End for(n) */

        #if 0	/* Draw face normal line. */
	if( faceNormalLen>0 )
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
	#endif

	/* Restore pixcolor */
	fbset_color2(&gv_fb_dev, color);
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
4 trianlges.
Here, the most simple regular sphere is an icosahedron!

@smesh:	A standard regular sphere_mesh derived from an icosahedron.
@r:	Radius of the icosahedron.

-------------------------------------------------------------*/
E3D_TriMesh::E3D_TriMesh(const E3D_TriMesh &smesh, float r)
{
	int k;

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
		/* ..Check if vNA already in vtxList[] */
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

		/* Add 4 triangles */
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


////////////////////////////////////  E3D_Draw Function  ////////////////////////////////////////

/*----------------------------------------------
Draw a 3D line between va and vb. zbuff applied.
@fbdev:	  Pointer to FBDEV.
@va,vb:	  Two E3D_Vectors as two points.
-----------------------------------------------*/
inline void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb)
{
	draw3D_line( fbdev, roundf(va.x), roundf(va.y), roundf(va.z),
		            roundf(vb.x), roundf(vb.y), roundf(vb.z) );
}

void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix)
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

void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix)
{
	E3D_Vector vpts[2];

	vpts[0]=va*RTmatrix;
	vpts[1]=vb*RTmatrix;

	/* Project vpts */
	if( projectPoints(vpts, 2, projMatrix) !=0) {
//		return;
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
void E3D_draw_grid(FBDEV *fbdev, int sx, int sy, int us, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix)
{
	int err;
	E3D_Vector vpts[2]; /* Init all 0 */

	egi_dpstd("sx=%zu, sy=%zu, us=%zu\n", sx, sy, us);

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
void E3D_draw_circle(FBDEV *fbdev, int r, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix)
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
void E3D_draw_coordNavSphere(FBDEV *fbdev, int r, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix)
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
void E3D_draw_coordNavFrame(FBDEV *fbdev, int size, bool minZ, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix)
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
	E3D_ProjMatrix projMatrix;
	projMatrix.type=0;
	projMatrix.dv = INT_MIN;        /* Try to ingore z value */
	projMatrix.winW=fbdev->pos_xres;
	projMatrix.winH=fbdev->pos_yres;

	/* Draw a coordNaveSphere */
	E3D_draw_coordNavFrame(fbdev, size, false, pmatrix, projMatrix);

	fbdev->zbuff_on=zbuff_save;
}

#endif


