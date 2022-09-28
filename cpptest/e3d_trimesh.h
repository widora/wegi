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
		    objects to expected position.
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

XXX 4. Option to display/hide back faces of meshes.

4.  In case the triangle degenerates into a line, then we MUST figure out the
    facing(visible) side(s)(with the bigger Z values), before we compute color/Z value
    for each pixel. ( see. in draw_filled_trianglX() and egi_imgbuf_mapTriWriteFBX() )

5.  XXX If some points are within E3D_ProjectFrustum.dv(Distance from the Foucs to viewPlane/projetingPlane)
    Then the projected image will be distorted!  --- To improve projection algrithm E3D_TriMesh.projectPoints()

6.  E3D_TriMesh.projectPoints(): NOW clip whole Tri if any vtx is out of frustum.

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
2022-08-04:
	1. E3D_TriMesh::renderMesh:  Test auxiliary lights.
	2. Add gv_auxLight.
2022-08-07:
	1. Add E3D_TriMesh::shadowMesh()
2022-08-09:
        1. Split e3d_trimesh.cpp from e3d_trimesh.h

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
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

#include "e3d_vector.h"

using namespace std;

#define TEST_PERSPECTIVE  	1
//#define VPRODUCT_EPSILON	(1.0e-6)  /* -6, Unit vector vProduct=vct1*vtc2 -----> See e3d_vector.h */

#if 1 /* Classes */
//class E3D_RTMatrix;
//class E3D_Vector;
class E3D_Material;
class E3D_PivotKeyFrames;
class E3D_TriMesh;
#endif


/* NON_Class Functions:  E3D_Material */
int readMtlFile(const char *fmtl, const char *mtlname, E3D_Material &material);
int readMtlFile(const char *fmtl, vector<E3D_Material> & mtlList);

/* NON_Class Functions */
int readObjFileInfo(const char* fpath, int & vertexCount, int & triangleCount,
				       int & vtxNormalCount, int & textureVtxCount, int & faceCount);

/* NON_Class Functions:  E3D_ProjMatrix */
int projectPoints(E3D_Vector vpts[], int np, const E3D_ProjMatrix & projMatrix);  /* Project points in camera space onto screen */
int reverse_projectPoints(E3D_Vector vpts[], int np, const E3D_ProjMatrix & projMatrix);
int mapPointsToNDC(E3D_Vector vpts[], int np, const E3D_ProjMatrix & projMatrix); /* map points in camera space to NDC */
int pointOutFrustumCode(const E3D_Vector &pt); //(float x, float y, float z);

/* Class Friend Functions: Intersection */
//bool E3D_rayHitTriMesh(const E3D_Ray &rad, const E3D_TriMesh &trimesh, E3D_Vector &vp);

/* NON_Class Functions:  E3D_Draw Function */
void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb);
void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb, const E3D_ProjMatrix &projMatrix);
void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);
void E3D_draw_line(FBDEV *fbdev, const E3D_Vector &va, const E3D_Vector &vb, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);

void E3D_draw_grid(FBDEV *fbdev, int sx, int sy, int us, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);
void E3D_draw_circle(FBDEV *fbdev, int r, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);

void E3D_draw_coordNavSphere(FBDEV *fbdev, int r, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);
void E3D_draw_coordNavFrame(FBDEV *fbdev, int size, bool minZ, const E3D_RTMatrix &RTmatrix, const E3D_ProjMatrix &projMatrix);
void E3D_draw_coordNavIcon2D(FBDEV *fbdev, int size, const E3D_RTMatrix &RTmatrix, int x0, int y0);

/* Compute lightings to get the color as we finally see. */
EGI_16BIT_COLOR E3D_seeColor(const E3D_Material &mtl, const E3D_Vector &normal, float dl);


/* Mesh Rendering/Shading type */
enum E3D_SHADE_TYPE {
	E3D_FLAT_SHADING	=0,
	E3D_GOURAUD_SHADING	=1,
	E3D_WIRE_FRAMING	=2,
	E3D_TEXTURE_MAPPING	=3,
};
extern const char* E3D_ShadeTypeName[];


/* Global light vector, as UNIT vecotr! OR it will affect face luma in renderMesh()  */
extern E3D_Vector gv_vLight;    //(0.0, 0.0, 1.0);   /*  factor=1.0 when rendering mesh */
extern E3D_Vector gv_auxLight;  //(1.0, 0.0, 0.0);   /* factor=0.5 when rendering mesh */
extern E3D_Vector gv_ambLight;  /* Global ambient lighting */

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
			!!! --- CAUTION --- !!!
 * 1. For vector application: to assign any pointer member AFTER finishing allocating all vector
 *    items, such as to avoid releasing those pointers unexpectedly when free and remalloc mem
 *    DURING resizing/growing_up the vector capacity.
 *    Example: It triggers to egi_imgbuf_free(img_kd..) when reallocating vecotr mem spaces.
 * 1A. As operator '=' is NOT overloaded, all pointers will be directly copied though its referred
 *    mem will NOT! so the underlying mem will be released twice by destructor, and cause segmentation fault.
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
	E3D_Vector ka;  /* TODO ambient color [0 1] ---> (RGB) [0 255] */
	//EGI_16BIT_COLOR acolor;
	E3D_Vector kd;  /* Diffuse color [0 1] ---> (RGB) [0 255] */
	//EGI_16BIT_COLOR dcolor;
	E3D_Vector ks;  /* TODO specular color [0 1] ---> (RGB) [0 255] */
	//EGI_16BIT_COLOR scolor;

	E3D_Vector Ss; /* Light source specular color */
	E3D_Vector Sd; /* Lighting source diffuse color */

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

/* Friend Classes */
	friend class E3D_Cuboid;
	friend class E3D_RtaSphere;

public:
	/* :: Class Vertex  !!! NO pointer member allowed !!! */
	class Vertex {
	public:
		/* Constructor */
		Vertex(); /* setDefaults */

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
		void assign(float x, float y, float z);
	};

	/* :: Class Triangle  !!! NO pointer member allowed !!! */
	class Triangle {
	public:
		/* Constructor */
		Triangle(); /* setDefaults */

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
		void assignVtxIndx(int v1, int v2, int v3); /* assign vtx[0-2].index */
	};

	/* :: Class Material --- See Class E3D_Material */

	/* :: Class TriGroup, NOT for editting!!!  */
	/* Note:
	 *	1. All TriGroups are under the same object/global COORD!
	 *	2. For vector application: !!!--- Caution for Pointer Memebers if any ---!!!
	 */
	class TriGroup {
	public:
		/* Constructor */
		TriGroup(); /* setDefaults() */

		/* Set defaults */
		void setDefaults();

		/* Print */
		void print();

		E3D_Vector	pivot;		/* Pivot/(rotation center) of the TriGroup relative to its superior/parent.
						 * It also under Object COORD!
						 * Initially omat.pmat[9-11] == pivot !!! as transform from Object Origin to trigroup local Origin
						 */
		//E3D_RTMatrix  pivot;          /* Pivot as matrix, as transform from local COORD to object COORD */
		/* OK.  translation of omat is same as oxyz */
		E3D_RTMatrix	omat;		/* omat.pmat[0-8] is the Orientation/attitude of the TriGroup Origin (local Coord) relative
						 * to its INITIAL position which USUALLY be the same as (align with) its superior node's COORD,
						 * and EXCEPT its tx,ty,tz value. XXX it's center/origin/pivotal of the TriGroup. XXX
						 * omat.pmat[9-11](tx,ty,tz) are under the SAME COORD as all other vertices in vtxList[]!
						 * they are translation of object origin to group(local) origin, and those values are same
						 * as pivot coordinates.
						 * XXX Before rotate_transform the TriGroup, (tx,ty,tz) should be set as rotation center.XXX
						 *
						 * The purpose is to fulfill hierarchical TRMatrix passing_down:
						 * 		Parent_Coord * Parent_omat *This_omat[relative_txtytz] = This_Coord.  <--
						 *
						 * 1. Init as identity().
						 * 1A. In cloneMesh(): the omat MUST copy each time in order to reset/reinit it.
						 * 1B. In transformMesh(): omat.pmat[9-11](pivot) MUST be transformed same as other vertices.
						 * 2. If the superior/parent node transforms, all its subordinate nodes
						 *    MUST transform also. While the subordinate node's transformation will NOT
						 *    affect its superior node.
					 	 * 3. In .obj file: 'x', 'm' lines:
						 *    x tx ty tz    ---> RTMatrix translation omat.pmat[9,10,11]: tx,ty,tz
						 *    m m1 ... m9  ----> Rotation Matrix:
						 *			 omat.pmat[0,1,2]: m11, m12, m13
						 *                       omat.pmat[3,4,5]: m21, m22, m23
						 *			 omat.pmat[6,7,8]: m31, m32, m33
						 *    r (X/Y/Z)23.3 (X/Y/Z)45.0 (X/Y/Z)120.  ---> Rotation
						 * 4. In .mot file: 'k' lines: (keyframe )
						 *     k  tf  rx ry rz  tx ty tz
						 *     tf:   Time value, in float.  [0 100]
						 *     XXX rx ry rz: Rotation RX,RY,RZ in degree.
						 *     tx ty tz: translation
						 * 5. Usually omat keeps unchanged before transformMesh(VRTmat, ScaleMat). After transformMesh(),
						 *    omat MUST be adjusted as per inversed VRTmat since all vertices positions changed.
						 *    and omat.pmat[9-11](tx,ty,tz) SHOULD keep unchanged after adjustment.
						 *    ( Use camera transformaton instead of mesh transformation can avoid above adjustment.)
						 */

		string		name;		/* Name of the sub_object group (of triangles) */
		bool		hidden;		/* Hidden or visible. */
		int		mtlID;		/* Material ID, as of mtlList[x]
						 * If <0, invalid!
						 */
		bool		ignoreTexture;  /* Ignore texture, use defined material color */
		bool		backFaceOn;	/* defautl as False,  ( this || E3D_TriMesh::backFaceOn )  */

		/* XXX Imgbuf loaded from material texture map files. */
		//EGI_IMGBUF *img_kd;  // NOW only diffuse.  ka, ks ...

		/* Define group include triangles from triList[] to triList[] */
		unsigned int	tcnt;  		/* Total trianlges in the group */
		unsigned int	stidx;  	/* Start/Begin index of triList[] */
		unsigned int	etidx;  	/* End index of triList[], NOT incuded!!!
				 		 * !!! CAUTION !!!  eidx=sidex+tcnt !!!
						 */
	};

	/* :: Class OptimationParameters */

	/* :: Class AABB (axially aligned bounding box */
	class AABB {
	public:
		E3D_Vector vmin;
		E3D_Vector vmax;
		void print();

		float xSize() const;
		float ySize() const;
		float zSize() const;
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
	void setVtxCount(int cnt);
	void setTriCount(int cnt);

	/* Function: Count vertices and triangles */
	int vtxCount() const;
	int triCount() const;

	/* Function: Get AABB size */
	float AABBxSize();
	float AABBySize();
	float AABBzSize();
	float AABBdSize();  /* diagonal size */

	/* Function: Retrun ref. of indexed vertex */
	E3D_Vector & vertex(int vtxIndex) const;

	/* Return vtxscenter */
	E3D_Vector vtxsCenter(void) const;

	/* Return AABB center */
	E3D_Vector aabbCenter(void) const;

/* TEST: ------------------2022_1_29 */
	void updateVtxsCenter(float dx, float dy, float dz);

	/* Function: Calculate center point of all vertices */
	void updateVtxsCenter();

	/* Function: Move VtxsCenter to current origin. */
	void moveVtxsCenterToOrigin();

	/* Function: Calculate AABB aabbox */
	void updateAABB(void);

	/* Function: Move VtxsCenter to current origin. */
	void moveAabbCenterToOrigin();

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

	/* Function: draw normal line */
	void drawNormal(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix) const;

	/* Function: compute shadow on a plane */
	void shadowMesh(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix, const E3D_Plane &plane) const;

	/* Function: Ray hit face of the trimesh */
	bool rayHitFace(const E3D_Ray &ray, int rgindx, int rtindx,
                        E3D_Vector &vphit, int &gindx, int &tindx, E3D_Vector &fn) const;

	bool rayBlocked(const E3D_Ray &ray) const;

private:
	/* All members inited at E3D_TriMesh() */
	E3D_Vector 	vtxscenter;	/* Center of all vertices, NOT necessary to be sAABB center! */

	#define		TRIMESH_GROW_SIZE	64	/* Growsize for vtxList and triList --NOT applied!*/
	int		vCapacity;	/* MemSpace capacity for vtxList[] */
	int		vcnt;		/* Vertex counter */
	Vertex  	*vtxList;	/* Vertex array
					 * 		!!! ---- CAUTION ---- !!!
					 * Must initialize to NULL!
					 * Allocate by 'new Vertice[cnt]',  in  E3D_TriMesh()
					 * Free by 'delete [] vtxList', in ~E3D_TriMesh()
					 */
	int		tCapacity;	/* MemSpace capacity for triList[] */
	int		tcnt;		/* Triangle counter */
	Triangle	*triList;	/* Triangle array
					 * 		!!! ---- CAUTION ---- !!!
					 * Must initialize to NULL!
					 * Allocate by 'new Triangle[cnt]',  in  E3D_TriMesh()
					 * Free by 'delete [] triList', in ~E3D_TriMesh()
					 */

public:
	int		instanceCount;	/* Instance counter */

	AABB		aabbox;		/* Axially aligned bounding box */
	//int mcnt;
//TODO	E3D_RTMatrix    VRTMat;		/* Combined Rotation/Translation Matrix. */
	E3D_RTMatrix    objmat;		/* Orientation/Position of the TriMesh object relative to Global/System COORD.
					 * 1. Init as identity() in initVars().
					 * 2. moveAabbCenterToOrigin() will reset objmat.pmat[9-11] all as 0.0!
					 * ??? Necessary ???  NOW an *.obj file contains only 1 object with N triGroups.
					 * XXX OK ,that indicates the object's position under global COORD, in case there are
					 *    more than 1 objects loaded in then scene.
					 * 3. NOT necessary in an obj file! it will be stored as VRTMat in an E3D_Scene file,
					 *    so when the E3D_Scene opens, it first load the obj file, then load the objmat to
					 *    locate the TriMesh to the scene.
					 * 4.                   !!! --- CAUTION ---- !!!
					 *    E3D_TRIMESH.objmat normally SHOULD keep as an identity, and E3D_MeshInstance.objmat
					 *    prevails in renderMeshInstance() by replacing it with this E3D_TRIMESH.objmat, and then
					 *    calling E3D_TRIMESH::renderMesh().
					 *    TODO: NOT good to replace/restore E3D_TRIMESH.objmat in E3D_MeshInstacne::renderMeshInstance().
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
					 * 2. Try to free in Destructor of ~E3D_Trimesh(), ALSO in ~E3D_Materil(), BUT OK!
					 */
	bool		backFaceOn;	/* If true, display back facing triangles.
					 * If false, ignore it.
					 * Default as false.
					 * Note:  ( this || E3D_TriMesh::TriGroup::backFaceOn ) HK2022-09-15
					 */
	EGI_16BIT_COLOR   bkFaceColor;  /* Faceback face color for flat/gouraud/wire shading.
					 * Default as 0(BLACK).
					 */
	EGI_16BIT_COLOR	wireFrameColor; /* Color for wire frame
					 * Default as 0(BLACK).
					 */

	int		faceNormalLen;   /* face normal length, if >0, renderMesh() will show the normal line. */
	bool		testRayTracing;	 /* Test backward ray tracing to vLight, to create self_shadowing. HK2022-09-08 */
};


/*----------------------------------------------
        Class: E3D_MeshInstance
Instance of E3D_TriMesh.
----------------------------------------------*/
class E3D_MeshInstance {
public:
	/* Constructor */
	E3D_MeshInstance(E3D_TriMesh &refTriMesh);
	E3D_MeshInstance(E3D_MeshInstance &refInstance);

	/* Destructor */
	~E3D_MeshInstance();

	/* Render MeshInstance */
	void renderInstance(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix) const;

	/* Reference E3D_TriMesh */
	E3D_TriMesh & refTriMesh;

	/* objmat matrix for the instance mesh. refTriMesh.objmat to be ignored then! */
	E3D_RTMatrix objmat;

	/* TriGroup omats. refTriMesh::triGroupList[].omat to be ignored then! */
	vector<E3D_RTMatrix> omats;
};


/*-------------------------------------------
              Class: E3D_Scene

-------------------------------------------*/
class E3D_Scene {
public:
	/* Constructor */
	E3D_Scene();
	/* Destructor. destroy all E3D_TriMesh in TriMeshList[] */
	~E3D_Scene();

	/* Import .obj into TriMeshList */
	void importObj(const char *fobj);
	/* Add instance */
        void addMeshInstance(E3D_TriMesh &refTriMesh);
        void addMeshInstance(E3D_MeshInstance &refInstance);
	/* Clear instance list */
	void clearMeshInstanceList();
	/* Render Scene */
	void renderScene(FBDEV *fbdev, const E3D_ProjMatrix &projMatrix) const;


	vector<string>  fpathList;	    /* List of fpath to the obj file */
	vector<E3D_TriMesh*>  triMeshList;  /* List of TriMesh  <--- Pointer --->  */
	//vector<E3D_RTMatrix>  ObjMatList;   /* Corresponding to each TriMesh::objmat  NOPE! */

	/* Instances List */
	vector<E3D_MeshInstance*> meshInstanceList; /* List of mesh instances
						     * Note:
						     * 1. Each time a fobj is imported, a MeshInstance is added in the list.
						     */
	/* TODO: */
	//List of vLights
private:

};

#endif
