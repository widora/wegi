/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

              --- EGI 3D glTF Parsing Classes ---
Reference:
glTF 2.0 Specification (The Khronos® 3D Formats Working Group)

	      --- Mapping from glTF to E3D ---

     glMesh ---> E3D_TriMesh
glPrimitive ---> E3D_TriMesh.TriGroup
     glNode ---> E3D_MeshInstance
    glScene ---> E3D_Scene

Note:
1. This is for E3D_TriMesh::loadGLTF(). search 'loadGLTF' to do crosscheck.
2. glTF also defines texture UV origin at leftTop of image, and same
   COORD_axis directions as E3D's.

Journal:
2022-12-13: Creates THIS file.

Midas Zhou
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#ifndef __E3D_GLTF_H__
#define __E3D_GLTF_H__

#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>
#include "egi_debug.h"
#include "egi.h"
#include "egi_fbgeom.h"
#include "egi_color.h"
#include "egi_image.h"
#include "egi_cstring.h"
#include "egi_utils.h"
#include "e3d_vector.h"
#include "e3d_animate.h"

using namespace std;

/*** The topology type of primitives to render
   0--Points, 1--Lines, 2--Line_Loop, 3--Line_Strip, 4--Triangles
   5--Triangle_strip, 6--Triangle_fan
 */
extern string glPrimitiveModeName[];

/*** component type
5120	signed byte	8
5121	unsigned byte	8
5122	signed short	16
5123	unsigned short	16
5125	unsigned int	32
5126	float		32
*/
enum {
	glSByte   =5120,
	glUSByte  =5121,
	glSShort  =5122,
	glUSShort =5123,
	glUSInt   =5125,
	glFloat   =5126,
};

/*** element type
element_type     number_of_components
-----------------------------------
SCALAR          1
VEC2            2
VEC3            3
VEC4            4
MAT2            4
MAT3            9
MAT4            16
**/

/* Return byte size of the element */
int glElementSize(string etype, int ctype);


/* Classes */
class E3D_glPrimitiveAttribute;
class E3D_glMorphTarget;
class E3D_glPrimitive;
class E3D_glMesh;
class E3D_glNode;
class E3D_glAccessor;
class E3D_glBuffer;
class E3D_glBufferView;

class E3D_glImage;
class E3D_glSampler;
class E3D_glTexture;
class E3D_glTextureInfo;
class E3D_glNormalTextureInfo;
class E3D_glOcclusionTextureInfo;
class E3D_glPBRMetallicRoughness;
class E3D_glMaterial;

class E3D_glAnimChanSampler;
class E3D_glAnimChanTarget;
class E3D_glAnimChannel;
class E3D_glAnimation;

class E3D_glNode;
class E3D_glSkin;
class E3D_glScene;

/* via */
const extern vector<E3D_glNode>  _empty_glNodes;

/* Non Class Functions */
char* E3D_glReadJsonKey(const char *pstr, string & key, string & strval);
int E3D_glExtractJsonObjects(const string & Jarray, vector<string> & Jobjs);

int E3D_glLoadBuffers(const string & Jbuffers, vector<E3D_glBuffer> & glBuffers);
int E3D_glLoadBufferViews(const string & JBufferViews, vector <E3D_glBufferView> & glBufferViews);
int E3D_glLoadAccessors(const string & JAccessors, vector <E3D_glAccessor> & glAccessors);
int E3D_glLoadPrimitiveAttributes(const string & JPAttributes, E3D_glPrimitiveAttribute & glPAttributes);  //<--- NOT ARRAY!
int E3D_glLoadMorphTargets(const string & JTargets, vector<E3D_glMorphTarget> & glTargets);
int E3D_glLoadPrimitives(const string & JPrimitives, vector <E3D_glPrimitive> & glPrimitives);
int E3D_glLoadMeshes(const string & JMeshes, vector <E3D_glMesh> & glMeshes);

int E3D_glLoadMaterials(const string & JMaterials, vector <E3D_glMaterial> & glMaterials);
int E3D_glLoadTextureInfo(const string & JTexInfo, E3D_glTextureInfo & glTexInfo);  //<--- NOT ARRAY!
int E3D_glLoadNormalTextureInfo(const string & JNormalTexInfo, E3D_glNormalTextureInfo & glNormalTexInfo);  //<--- NOT ARRAY!
int E3D_glLoadPBRMetaRough(const string & JMetaRough, E3D_glPBRMetallicRoughness & glPBRMetaRough);  //<--- NOT ARRAY!
int E3D_glLoadTextures(const string & JTextures, vector <E3D_glTexture> & glTextures);
int E3D_glLoadImages(const string & JImages, vector <E3D_glImage> & glImages);

int E3D_glLoadAnimChanSamplers(const string & JAnimChanSamplers, vector <E3D_glAnimChanSampler> & glAnimChanSamplers);
int E3D_glLoadAnimChanTarget(const string & JAnimChanTarget, E3D_glAnimChanTarget & glAnimChanTarget);   //<--- NOT ARRAY!
int E3D_glLoadAnimChannels(const string & JAnimChannels, vector <E3D_glAnimChannel> & glAnimChannels);
int E3D_glLoadAnimations(const string & JAnimations, vector <E3D_glAnimation> & glAnimations);

int E3D_glLoadNodes(const string & JNodes, vector <E3D_glNode> & glNodes);
int E3D_glLoadSkins(const string & JSkins, vector <E3D_glSkin> & glSkins);
int E3D_glLoadScenes(const string & JScenes, vector <E3D_glScene> & glScenes);



/*-------------------------------
        Class E3D_glScene
--------------------------------*/


/*---------------------------------------
     Class E3D_glPrimtiveAttribute
--------------------------------------*/
class E3D_glPrimitiveAttribute {
public:
        /* Constructor */
        E3D_glPrimitiveAttribute();
        /* Destructor */

        //~E3D_glPrimitiveAttribute();

/*** Primitive Attribute Memebers

	(---- Ref. glTF2.0 Spek 3.7.2.1. Overview ----)

Name 	  Accessor	Component      Note
	   Type(s)       Type(s)
--------------------------------------------------
POSITION    VEC3           float       Unitless XYZ vertex positions
NORMAL      VEC3           float       Normalized XYZ vertex normals
TANGENT     VEC4 	   float       XYZW vertex tangents,(XYZ is normalized, W is(-1 or +1)

TEXCOORD_n  VEC2 	   float        Textrue coordinates
			   unsigned byte normalized
		           unsigned short normalized

COLOR_n	    VEC3/VEC4      float	RGB or RGBA vertex color linear multiplier
			   unsigned byte normalized
			   unsigned short normalized
JOINTS_n    VEC4           unsigned byte unsigned short  See Skinned Mesh Attributes
WEIGHTS_n   VEC4           float	See Skinned Mesh Attributes
			   unsigned byte normalized
			   unsigned short normalized

(Others ref. to: MAT2,MAT3,MAT4...)
*/

	/* The value of the property is the index of an accessor that contains the data.
	 * Init/Default =-1 as invalid.
         *
         *    		!!! --- CAUTION --- !!!
	 *     Cross-check to E3D_glMorphTarget memebers!
	 */
	int positionAccIndex;  /* Index of accessor, holding data of vertex position XYZ */
	int normalAccIndex;    /* Index of accessor, holdingg data of Normalized XYZ vertex normals */
	int tangentAccIndex;

	/* following are also accessor indices */
#define	PRIMITIVE_ATTR_MAX_TEXCOORD 8
	int texcoord[PRIMITIVE_ATTR_MAX_TEXCOORD];
				    /* 1. XXX TODO: NOW ONLY TEXCOORD_0 applys, assume it's for TEXTURE COORDINATES.
				       1. The index corresponds to E3D_glXXXXTextureInfo.texcoord
				       2. crosscheck at E3D_TriMesh::loadGLTF() at 11.3.4
				       3.  			!!!--- NOTE ---!!!
				 	   glTF also defines UV origin at leftTop of the image, SAME as E3D.
				    */
#define	PRIMITIVE_ATTR_MAX_COLOR 8
	int color[PRIMITIVE_ATTR_MAX_COLOR];

#define	PRIMITIVE_ATTR_MAX_JOINTS 8

	/*** For skinned primitive mesh.
	 *   1. A skin defines influencing JOINTs and their WEIGHTs for subjected mesh(primitives).
	 *   2. Each attribute refers to an accessor that provides one data element for each vertex of the primitive.
	 *   3. TODO: NOW only support Max. 1 skin for a primitive.
	 */
//	int jonits[PRIMITIVE_ATTR_MAX_JOINTS];
	int joints_0; //TODO _n;  // Json name "JOINTS_n", 
				  /*** 1 .Refers to an accessor that contains the indices of the joints that should have an influence
				          on the vertex during the skinning process.  (joints_n ref. to skin_n)
				       2. For each vertex,it has usually 4 influencing joints, so the accessor type is VEC4.
				       3. "The joint weights for each vertex MUST NOT be negative.
				           Joints MUST NOT contain more than one non-zero weight for a given vertex. "
				       4. init to -1<0, as invalid.
				       5. accessor componentType must be: unsigned byte or unsigned short.
				   */
//	int weights[PRIMITIVE_ATTR_MAX_JOINTS]
	int weights_0; //TODO _n; // Json name "WEIGHT_n", as companying with above joints_n
				  /*** 1. Refers to an accessor that provides information about how strongly each
				          joint should influence each vertex. ALSO the accessor type is VEC4.
				       2. init to -1<0, as invalid.
				       3. accessor componentType must be: float, or normalized unsigned byte, or normalized unsigned short
				   */
};


/*---------------------------------------
   Class E3D_glMorphTarget
Mesh morph target.
---------------------------------------*/
class E3D_glMorphTarget {
public:
        /* Constructor */
	E3D_glMorphTarget();
	/* Destructor */

        /***   		       !!! --- CAUTION --- !!!
	 *     Cross-check and refert to  E3D_glPrimitiveAttribute memebers.
	 * 1. Init/Default =-1 as invalid.
	 * 2. POSITION accessor MUST have its min and max properties defined.
	 * 3. Displacements for POSITION,NORMAL and TANGENT attributes MUST be applied before any transformation matrice
         *    affecting the mesh vertices such as skinning or node transftorms.
	 *
         */
	int positionAccIndex;    /* POSITION: Index of accessor for XYZ displacements, accessort type: VE3, component type: float */
	int normalAccIndex;      /* NORMAL:   Index of accessor for vtx normal displacements, accessort type: VE3, component type: float */
	int tangentAccIndex;     /* TANGENT:   Index of accessor for vtx tangent displacements, accessort type: VE3, component type: float */
	int texcoord[PRIMITIVE_ATTR_MAX_TEXCOORD];   /* TEXCOORD_n:  Refer to E3D_glPrimitiveAttribute memebers */
	int color[PRIMITIVE_ATTR_MAX_COLOR];	     /* COLOR_n:  Refer to E3D_glPrimitiveAttribute memebers */
};


/*-------------------------------------------------
        Class E3D_glPrimitive (Mesh Primitive)
--------------------------------------------------*/
class E3D_glPrimitive {
public:
        /* Constructor */
        E3D_glPrimitive();
        /* Destructor */
        //~E3D_glPrimitive();

	/* ----- !glPrimitve has NO name! ----- */

	E3D_glPrimitiveAttribute  attributes;
	int		 indicesAccIndex;  /* the index of the accessor that contains the vertex indices.
					    * When defined, the accessor MUST have SCALAR type and an unsigned integer component type.
										!!! CAUTION !!!
					       Above mentioned unsigned integer includes: unsigned int, unsigned short, unsigned char
					    */
	int		 materialIndex;    /* The index of the material to apply to this primitive when rendering
					    If <0, use default.
					   */
	int 		 mode;		   /* Default=4; the topology type of primitives to render
					     0--Points, 1--Lines, 2--Line_Loop, 3--Line_Strip, 4--Triangles
					     5--Triangle_strip, 6--Triangle_fan
					   */

	/* targets */
	vector<E3D_glMorphTarget> morphs;  /* Json name 'targets', Required:No, An array of mesh morph targets */

	//extensions;
	//extras;
};


/*-------------------------------
        Class E3D_glMesh
--------------------------------*/
class E3D_glMesh {
public:
        /* Constructor */
        E3D_glMesh();
        /* Destructor */
        //~E3D_glMesh();

	/* attribues{NORMAL, POSITION,TEXTCOORD_n, ..}, indices, material, mode */

	string		 name; /* Required:No, User-defined name of this object */

	/* The value of the property is the index of an accessor that contains the data.*/
	vector<E3D_glPrimitive>  primitives;   /* An array of primitives, each defining geometry to be rendered */

	/* TBD&TODO  All primitives in a mesh MUST share the same weights array!!!   */
	vector<float> mweights; /*** Array of weights to be applied to the morph target,
				* If(mweights.empty()),no morph for the mesh
				* Json name "weights"
			        * The number of array elements MUST match the number of morph targets in primitives[].targets.
			        * Example: to compute vertex position for a mesh primitive:
				  renderedPrimitive[i].POSITION = primitives[i].attributes.POSITION
								 + weights[0] * primitives[i].targets[0].POSITION
  								 + weights[1] * primitives[i].targets[1].POSITION +
  								 + weights[2] * primitives[i].targets[2].POSITION + ...
				* See 11.-1.7 of E3D_Scene::loadGLTF()
			        */

	int skinIndex;		/* index of glSkins[], ONLY if the mesh has been skinned.
				 * init to -1.
				 * TBD&TODO: NOW assume one mesh has NO MORE THAN one skin defined.
				 */

	//extensions;
	//extras;
};


enum E3D_NODE_TYPE {
	glNode_Node   =0,
	glNode_Mesh   =1,
	glNode_Camera =2,
	glNode_Skin   =3,
};
extern const char*  _strNodeTypes[];

/*---------------------------------------------------------------
        Class E3D_glNode

Note:
   1. When the node contains skin, mesh[skinMeshIndexmesh].primitives
      MUST contain JOINTS_0 and WEIGHTS_0 attributes.
   2. A skin_node associate a glMesh and a glSkin.
-------------------------------------------------------------------*/
class E3D_glNode {
public:
        /* Constructor */
        E3D_glNode();
        /* Destructor */
        //~E3D_glNode();

	/* Functions */
	void print();

	enum E3D_NODE_TYPE type;  /* Default as glNode_Node */

	/* name          string        The user-defined name of this node-object. */
	string name;

	/* children nodes     integer[1-*]  The indices of this node's children. */
	vector<int>	children;     /* As the indices of nodes list */

/* For <<<camera_node>>> ONLY */
	/* camera_node 	 integer       The index of the camera referenced by this node. */
	int cameraIndex;		/* Init to -1, if>=0, node type is glNode_camera */

/* For <<<skin_node>>> ONLY, binding with a mesh. */
	/* skin_node       integer       The index of the skin referenced by this node. */
	int skinIndex;			/* Init to -1, When defined, skinMeshIndex MUST also be defined.
					 * index to glSkins[]
					 * If >=0, node type is glNode_skin.
					 */
	int skinMeshIndex;              /* Init to -1, index of a mesh binding with above skinning.  HK2023-03-08
					 * index to glMeshes[]
					 * To be set by E3D_glLoadNodes()
					 */
/* For <<<mesh_node>>> ONLY */
	/* (mesh_node)	 integer       The index of the mesh for this node. */
	int meshIndex;			/* Init to -1, if>=0, node type is glNode_mesh */


	/*** Note for matrix and TRS:
	   0. Node matrix/TRS is presetting Transform matrix, for the node-object's initial position/orientation.
	   1. "When matrix is defined, it MUST be decomposable to TRS properties."
	   2. "When a node is targeted for animation (referenced by an animation.channel.target),
	       only TRS properties MAY be present; matrix MUST NOT be present."
	   3. TRS application order: "first the scale is applied to the vertices, then the rotation, and then the translation."
	      >>>>>  pxyz*MatScale*MatRotation*MatTranslation = pxyz*MatScale*(MatRotation*MatTranslation) = pxy*MatScale*RTMat <<<<
	*/

	/* matrix	number[16]    A floating-point 4x4 transformation matrix stored in column-major order.
				      default: [1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
	   Note: glTF colume-major is E3D row-major order.
	*/
	E3D_RTMatrix    matrix;		/* pmat[4*3] in 4 row, 3 colums. in row-major order, init as identity.
					 * SHOULD BE decomposable to TRS properties! say it can convert to a quatrix!
					 */
	bool		matrix_defined;  /* If matrix is defined */

	/* rotation      number[4]     The nodes's unit quaternion rotation in the order(x,y,z,w), where w is the scalar.
				      default [0,0,0,1]  (Note: for quaternion rotation, (-x,-y,-z,-w) same as (x,y,z,w) ) */
	/* translation   number[3]     The node's translation along the x,y, and z axes. */
	/* qt: rotation_quaternion + translationXYZ. Default identity. */
	E3D_Quatrix 	qt;

	/* scale         number[3]     The node's non-uniform scale, given as the scaling factor along the x,y and z axes. */
	//float scaleX, scaleY, scaleZ;   /* Default 1,1,1 */
	E3D_RTMatrix    scaleMat;       /* Scale matrix with scaleXYZ only, interim param, it integrates into pmat. see pmat.
					 * 		--- NO MORE USE! ---
					 * Integrated into qt.sx/sy/sz.
					 */
	bool 		SRT_defined;   /* If scale and/or rotation and/or translation is defined in glTF Json descript */


	E3D_RTMatrix pmat;      /* Presetting Transform matrix, for the object's initial position/orientation, OR local transform.
				 * 1. If matrix is present, then pmat=matrix
				 *    Else pmat=MatScale*(MatRotation*MatTranslation)=scaleMat*qt.toMatrix
				 * 2. Computed in E3D_glLoadNodes()
				 * 3. If SRT_defined, try to use qt in some cases, for pmat MAY integrated with scaleMat.
			 	*/
	E3D_RTMatrix amat;     /* Acting/animating mat, normally interpolated from animQts.
				* See at E3D_Scene::renderSceneNode() and E3D_Scene::interpNodeAmats()
				*/
	E3D_RTMatrix gmat;    /*** Globalizing transform matrix, to transform coordinates from under this node COORD to under Global COORD
				 * See at E3D_Scene::renderSceneNode()  <-----
				 *    node->gmat=node->amat*node->pmat*(node->parent->gmat)
						!!!--- CAUTION ---!!!
				 *    To apply gmat ONLY after immediate whole tree computation!
				 */

//	E3D_Vector NOxyz  <<<--->>>  gmat.tx/ty/tz As node local origin under global COORD.


			/* ===== SKinning data (ONLY if type==glNode_Skin ) ====== (HK2023-03-05) */


       	 /* Note:
	  * 1. TDB&TODO: NOW assume ONE node can ONLY belong to one skeleton(a set of joints).
	  *    If more than 1 set of joints(skeleton) refer to the same node(as in glSkins), ONLY the first set will apply.
	  */

	bool	    IsJoint;	/* ONLY if this node is joint of a skin. --- ALSO ref. to skinIndex in above.
				 * Default: false
				 * Ref. to E3D_Scene::loadGLTF() 9.
				 */
	int	    jointSkinIndex;   /* Index of gSkins[] to which this joint belongs, init to -1. */


	E3D_RTMatrix invbMat;   /* inverseBindMatrix, to transform vertices to under this node's COORD
				 *
				 *		--- Implementation note ---
				 * "The fourth row of each matrix MUST be set to [0.0, 0.0, 0.0, 1.0].
                                 *  The matrix defining how to pose the skin’s geometry for use with the joints
				 *  (also known as “Bind Shape Matrix”) should be premultiplied to mesh data or
				 *  to Inverse Bind Matrices."
				 *
				 * It MAY integrated with ScaleMat!
				 * Ref. to E3D_Scene::loadGLTF() 9.
				 *
				 * This matrix is taken from E3D_glSkin, ONLY if this node is a joint of the skin.
				 *		   !!!--- CAUTION ---!!!
				 *  TDB&TODO: NOW one node can ONLY be a joint of ONE skin. If more than one skin
				 *  has this node as its joint, then you should update/revise invbMat case by case
				 *  in computing skinning.
				 */
	E3D_RTMatrix jointMat;  /* jointMat=invMat*gmat, matrix to bind vertices to the transformation of this node.
				 * As one skin vertex will be affected by max.4 jointMats, so jontMats of all glNodes
				 * SHOULD be resolved before (shading vertices/)rendering a mesh.
				 */



					/* ===== Animation data ====== */


	vector<E3D_AnimQuatrices> animQtsStacks;  /* HK2023-04-06. Array of animation data, One stack for a set of animation data. */
	//E3D_AnimQuatrices       animQts;
				    /* Animating Quatrices for KeyFrames, interpolate to assign to amat!
                                     * TBD&TODO: check and normalize quaternion before converting to/from matrix. ???
				       Ref. E3D_Scene::loadGLTF()/loadGLB 12.: Load animation data into glNodes.animQtsStacks.
				        :glNodes[node].animQts.insertSRT("S,R,T")
					       				!!!--- NOTICE --!!!
				        Allocate capacity: glNodes[ka].animQtsStacks.resize(animTotal), though there may
					be NO RTS animation data at all! see E3D_Scene::loadGLTF/loadGLB() 12.
                                     */


	/* ------ Morph weigts for mesh nodes ------ */

	/* TODO weights       number[1-*]   The weights of the instantiated morph target.(when defined, mesh MUST also be defined)
	 			       The number of array elements MUST match the number of morph targets of the referenced mesh.
					TBD&TODO: For what???
        			     */
//	vector<int> weightIndices;
/* XXX 	vector<float>     pmwts      Note: init position morph weights are loaded into E3D_TriMesh.mwegiths */

	vector<float>     amwts;
					     /* Acting morph weights for node mesh, normally interpolated from animMorphwtsStacks[]
					       Ref. E3D_Scene::renderSceneNode() and E3D_Scene::interpNodeAmwts()
					     */

	vector<E3D_AnimMorphWeights> animMorphwtsStacks;  /* HK2023-04-07 */
//       E3D_AnimMorphWeights animMorphwts;
					    /* Animating morph weights for KeyFrames, it applys to node's mesh.primitives
					       Ref. E3D_Scene::loadGLTF()/loadLGB 12.: Load animation data into glNodes.animMorphwtsStacks.
					       				!!!--- NOTICE --!!!
					        Allocate capacity: glNodes[ka].animMorphwtsStacks.resize(animTotal), though there may
						be NO morph data at all! see E3D_Scene::loadGLTF/loadGLB() 12.
					     */
	//extensions
	//extras
};


/*-------------------------------
        Class E3D_glSkin
--------------------------------*/
class E3D_glSkin {
public:
        /* Constructor */
	E3D_glSkin();
        /* Destructor */

	/* name          string        The user-defined name of this scene */
	string name;

	/* inverseBindMatrices  integer  The index of the accessor contains float-point 4x4 matrices(accessor type MAT4)
	 * Its accessor.count property MUST be greater than or equal to the number of joints.size().
	 */
	int invbMatsAccIndex;  /* Required: NO.  Init to -1. <0 as undefined, then all invbMats are identity.
				 An accessor referenced by inverseBindMatrices MUST have floating-point components
			         of "MAT4" type. The number of elements of the accessor referenced by inverseBindMatrices
				 MUST greater than or equal to the number of joints elements. The fourth row of each matrix
				 MUST be set to [0.0, 0.0, 0.0, 1.0].
				The matrix defining how to pose the skin’s geometry for use with the joints (also known as “Bind Shape Matrix”)
				should be premultiplied to mesh data or to Inverse Bind Matrices.
				*/

	/* skeleton     integer    the index of the node used as a skeleton root */
	int skeleton;          /* Required: NO. init to -1 */

	/* joints     integer[1-*]    indices of skeleton nodes, used as joints in this skin */
	vector<int>   joints;	/* Required: YES.  */

	//extensions
	//extras
};


/*-------------------------------
        Class E3D_glScene
--------------------------------*/
class E3D_glScene {
public:
        /* Constructor */
        //E3D_glScene();
        /* Destructor */
        //~E3D_glScene();

	/* name          string        The user-defined name of this scene */
	string name;

        /* nodes     integer[1-*]  The indices of this node's children. */
        vector<int>     rootNodeIndices;     /* As the indices of root node */

	//extensions
	//extras
};


/*-----------------------------------
        Class E3D_glAccessor

component_type	data_type      bits
-----------------------------------
5120		signed byte	8
5121		unsigned byte	8
5122		signed short	16
5123		unsigned short	16
5125		unsigned int	32
5126		float		32


element_type     number_of_components
-----------------------------------
SCALAR		1
VEC2		2
VEC3		3
VEC4		4
MAT2		4
MAT3		9
MAT4		16


        !!! --- CAUTION --- !!!

Accessors of matrix type have data stored in column-major order.
(glTF column-major order == E3D row_major order )

----------------------------------*/
class E3D_glAccessor {
public:
        /* Constructor */
        E3D_glAccessor();
        /* Destructor */
        //~E3D_glAccessor();

	string  name;		/* Reuried:No, the user-defined name of this object */

	int	bufferViewIndex; /* glTF Json name "bufferView",  The index of the bufferView */
	int 	byteOffset;	/* Required:No, default=0. The offset relative to the start of the bufferview in bytes */
	int	componentType;  /* the datatype of the accessor's components, see list above. */
	bool	normalized;	/* Required:No, specifies whether INTEGER data values are normalized (true) to [0, 1] (for unsigned types)
				   OR to [-1, 1] (for signed types) when they are accessed.
				 * Default=false
				 */
	int	count;		/* The number of ELEMENTs referenced by this accessor */
	string  type;		/* Specifies if the accessor's elements are scalars, vecotrs, or matrices, see above note. */

//	vector<> max;	/* max. value of each component in this accessor, number of components [1-16] */
//	vector<> min;	/* min. value of each component in this accessor, number of components [1-16] */

	//sparse;		/* Required:No, sparse storage of elements that deviate from their initialization value. */

	//extensions;
	//extra;

	int elemsize;		/* Byte size for each element. =glElementSize(type, componentType)
				 * <0  Error for type or componetType.
				 */

	/* Pointer to the data in buffer, NOT the Owner.
	 *  To be linked/assigned NOT in E3D_glLoadAccessors()
	 */
	const unsigned char *data;
	int     byteStride;		/* THIS IS SAME AS glBufferViews[bufferViewIndex].byteStride
					 * Assigned in E3D_Scene::loadGLTF/loadGLB(): 4.Link buffersViews.data to accessors.data
					 */
};


/*-------------------------------
        Class E3D_glBuffer
--------------------------------*/
class E3D_glBuffer {
public:
        /* Constructor */
        E3D_glBuffer();
        /* Destructor */
        ~E3D_glBuffer();

	string	     name; /* Required:No */
	string 	     uri;  /* URI to the resource. OR maybe base64 encoded data ebedded in glTF. */
	int 	     byteLength;

	EGI_FILEMMAP *fmap;   /* MMAP to an underlying file, then fmap will be MMAP of whole GLB file! */
	bool          GLBstored;  /* True: Is the ONLY GLB BIN buffer */
 	unsigned char *data;  /* 1. If fmap!=NULL, then data=fmap->fp, OR for GLB stored: data=fmap->fp+fmap->fp+28+chunk0_length;
				    (see at loadGLB(): Case glTF Jobject: buffers )  HK2023-02-22
				    Else data should be allocated by new [], see in ~E3D_glBuffer().
				 2. THIS is the data OWNER.
				 3. All buffer data defined by glTF are in little endian byte order.
			       */
};


/*-----------------------------------
        Class E3D_glBufferView

target	   	type
------------------------------------
34962		ARRAY_BUFFER
34964		ELEMENT_ARRAY_BUFFER

-----------------------------------*/
class E3D_glBufferView {
public:
        /* Constructor */
        E3D_glBufferView();
        /* Destructor */
        //~E3D_glBufferView();

	string  name;		/* Required:No */
        int 	bufferIndex;    /* glTF Json name "buffer", Index of glBuffer */
	int 	byteOffset;	/* Requred:No, default=0 */
        int 	byteLength;
	int 	byteStride;	/* Required:NO, when two or more accessors use the same buffer view
				   this filed MUST be defined.
				   Min>=4, Max<=252;

					    !!!--- CAUTION ---!!!
				    byteStride is ONLY for vertex attribute data.
				    (OR unless such layout is explicitly enabled by an extension).
				  */

	/* The hint representing the intended GPU buffer type to use with this buffer view */
        int 	target;	   /* Required: NO, Allowed values: 34962_ARRAY_BUFFER, 34964_ELEMENT_ARRAY_BUFFER */


	/* Pointer to the data in buffer. THIS is NOT the data Owner.
	 *  To be linked/assigned in E3D_Scene::loadGLTF/loadGLB(): 3. Link buffers.data to bufferViews.data
	 */
	const unsigned char *data;

	//extensions
	//extras
	//schema
};




/*-----------------------------------
        Class E3D_glImage

-----------------------------------*/
class E3D_glImage {
public:
        /* Constructor */
        E3D_glImage();
        /* Destructor */
        ~E3D_glImage();

	string  name;	  /* Required:No,Jobject name */
	string 	uri;  	  /* Required:No, MUST NOT be defined when bufferView is defined.
			     Instead of referencing an external file, this field MAY contain a data:-URI.
			     If uri in glTF file contains embedded data, then THIS uri will be empty.
			   */
	bool    IsDataURI;  /* If it's a dataURI */

	string 	mimeType; /* Required:No, MUST be defined when bufferView is defined
			   * Allowed values: image/jpeg, image/png
			   * It's empty also when image data is embedded in above uri.
			   */
	int	bufferViewIndex; /* Required:No, glTF Json name "bufferView"
				  * The index of the bufferView contains the image.
				  * MUST NOT be defined when uri is defined.
				  */
	//extensions
	//extras

	 EGI_IMGBUF *imgbuf;  /* EGI_IMGBUF of the image
				 		!!!--- CAUTION ---!!!
				 Ownership transfers in E3D_TriMesh::loadGLTF(): mtlList[].img_kd=glImages[].imgbuf;
				 1. E3D_glLoadImages(): Load from data:-URI or an external file.
				 2. loadGLTF(): Load from glBufferViews[].data.
				*/
};

/*---------------------------------------
   	Class E3D_glSampler

Texture sampler for filtering and wrapping modes
---------------------------------------*/
class E3D_glSampler {
public:
        /* Constructor */
        E3D_glSampler();
        /* Destructor */
        //~E3D_glSampler();

	string  name;		/* Required:No */
	int magFilter;		/* Required:No,  Magnification filter Allowed values: 9728--NEAREST, 9729--LINEAR */
	int minFilter;		/* Required:No,  Minification filter Allowed values: 9728,9729, 9984-9987 */
	int wrapS;		/* Required:No,  S(U) wrapping mode, default=10497
				   Allowed values: 33071 CLAMP_TO_EDGE,33648 MIRRORED_REPEAT,10497 REPEAT
				 */
	int wrapT;		/* Required:No,  T(V) wrapping mode, default=10497.
				   Allowed values: 33071 CLAMP_TO_EDGE,33648 MIRRORED_REPEAT,10497 REPEAT
				 */
	//extensions
	//extras
};



enum {
	AnimInterpolation_Linear  =0,
	AnimInperpolation_Step    =1,
	AnimInterpolation_Cubicspline  =2,
};

/*---------------------------------------
   Class E3D_glAnimChanSampler
Animation Channel Sampler
---------------------------------------*/
class E3D_glAnimChanSampler {
public:
        /* Constructor */
        E3D_glAnimChanSampler();
        /* Destructor */
        //~E3D_glAnimChanSampler();


	int     input;   	    /* The index of an accessor containing keyframe timestamps
				     * The accessor MUST be of scalar type with floating-point components.
				     * Required Yes,  Init to -1, as invalid
				     */
	int     interpolation;      /* Interpolation algorithm. Default "LINEAR"(0), others "STEP"(1), "CUBICSPLINE"(2)
				     * Default 0. */
	int 	output;		    /* The index of an accessor containing keyframe output
				     * Required Yes,  Init to -1, as invalid
				     */

	//extensions
	//extras
};

enum E3D_ANIMPATH_TYPE {
	ANIMPATH_UNKNOWN       =-1,
	ANIMPATH_WEIGHTS       =0,
	ANIMPATH_TRANSLATION   =1,
	ANIMPATH_ROTATION      =2,
	ANIMPATH_SCALE         =3,
};
/* Check max value of pathTypeMAX, see loadGLTF() 12.1 (12.1.2) */


/*---------------------------------------
   Class E3D_glAnimTarget
Animation channel target
---------------------------------------*/
class E3D_glAnimChanTarget {
public:
        /* Constructor */
	E3D_glAnimChanTarget();
        /* Destructor */

	int    node;	/* The index of the node to animate, when undefinde the
			 * animated object MAY be defined by an extension
			 */

	string path;    /* Then name of the node's TRS property
			 * Required: YES,   'weights', 'translation'[x,y,z], 'rotation'[X,Y,Z,W], 'scale'[X,Y,Z]
		                Path        Accessor_type     Component_type
				-------------------------------------------------------
			     translation     VEC3                 float
			     rotation        VEC4                 float/signed byte normalized/unsinged byte normalized/...
			     scale           VEC3                 float

			     weights         SCALAR               float/signed byte normalized/unsinged byte normalized/....
			 */

	enum E3D_ANIMPATH_TYPE pathType; 	 /* According to above path, Init to -1, <0 invalid */

#if 0  /* enum E3D_ANIMPATH_TYPE */
   #define ANIMPATH_WEIGHTS     	0
   #define ANIMPATH_TRANSLATION    1
   #define ANIMPATH_ROTATION       2
   #define ANIMPATH_SCALE          3
/* Check max value of pathTypeMAX, see loadGLTF() 12.1 */
#endif

	//extensions
	//extras
};


/*---------------------------------------
   Class E3D_glAnimChannel
Animation channel

Example:
  Application sequence for E3D is pxyz*ScaleMat*RotMat*TransMat

            "channels" : [
                {
                    "sampler" : 0,
                    "target" : {
                        "node" : 64,
                        "path" : "translation"
                    }
                },
                {
                    "sampler" : 1,
                    "target" : {
                        "node" : 64,
                        "path" : "rotation"
                    }
                },
                {
                    "sampler" : 2,
                    "target" : {
                        "node" : 64,
                        "path" : "scale"
                    }
                },
		....
	     ]

---------------------------------------*/
class E3D_glAnimChannel {
public:
        /* Constructor */
        /* Destructor */

	int                  samplerIndex;  /* Required: YES.  The index of a animSampler */

	E3D_glAnimChanTarget target;	/* The descriptor of the animated property */



	//extensions
	//extras
};


/*----------------------------------------------------------
   Class E3D_glAnimation
Animation

Note:
1. Animation data wil be loaded into glNodes.animQts
------------------------------------------------------------*/
class E3D_glAnimation {
public:
        /* Constructor */
        /* Destructor */

	string  name;

	vector<E3D_glAnimChannel>       channels;  /* Note: Different channels of the same animation
						      MUST NOT have the same targets(means same node and path)
						   */

	vector<E3D_glAnimChanSampler>   samplers;  /*  list of samplers, to be referred by channels[].samplerIndex */

	//extensions
	//extras
};





/*---------------------------------------
   Class E3D_glTexture

---------------------------------------*/
class E3D_glTexture {
public:
        /* Constructor */
        E3D_glTexture();
        /* Destructor */
        //~E3D_glTexture();

	string name;		/* Required:No */
	int samplerIndex;	/* Required:No, Json name "sampler". The index of the sampler used by this texture
				  <0 invalid  Init.-1
				 */
	int imageIndex;		/* Required:No, Json name "source".  The index of the image used by this texture
				  <0 invalid  Init.-1
				 */

	//extensions
	//extras
};

/*---------------------------------------
   Class E3D_glTextureInfo

---------------------------------------*/
class E3D_glTextureInfo {
public:
        /* Constructor */
        E3D_glTextureInfo();
        /* Destructor */
        //~E3D_glTextureInfo();

	int	index;		/* Required:Yes, the index of the texture. <0 as invalid, init-1 */
	int 	texCoord;	/* Required:No,  The set index of texture's TEXCOORD attribute
				   used for texture coordinate mapping.
				   This integer value is used to construct a string in the format TEXCOORD_<set index>
				   which is a reference to a key in mesh.primitives.attributes
				   (e.g. a value of 0 corresponds to TEXCOORD_0).
				   A mesh primitive MUST have the corresponding texture coordinate attributes for
				   the material to be applicable to it.
				   default=0;
				   CrossCheck at E3D_TriMesh::loadGLTF() at 11.3.4.
				   as index of glMeshes[].primitives[].attributes.texcoord[texCoord], see E3D_glPrimitiveAttribute
				 */

        //extensions
        //extras
};

/*---------------------------------------
   Class E3D_glNormalTextureInfo

The tangent space normalTexture.
---------------------------------------*/
class E3D_glNormalTextureInfo {
public:
        /* Constructor */
        E3D_glNormalTextureInfo();
        /* Destructor */
        //~E3D_glNormalTextureInfo();

	int	index;		/* Required:Yes,  the index of the texture. <0 as invalid, init-1 */
	int 	texCoord;	/* Required:No,  The set index of texture's TEXCOORD attribute
				   used for texture coordinate mapping.
				   Default=0;
				   CrossCheck at E3D_TriMesh::loadGLTF() at 11.3.4.
				 */
	float  scale;		/* Required:No, The scalar parameter applied to each normal vector of the
				   normal texture,
				   default=1.0;
				 */

        //extensions
        //extras
};

/*---------------------------------------
   Class E3D_glOcclusionTextureInfo

---------------------------------------*/
class E3D_glOcclusionTextureInfo {
public:
        /* Constructor */
        E3D_glOcclusionTextureInfo();
        /* Destructor */
        //~E3D_glOcclusionTextureInfo();

	int	index;		/* Required:Yes,  the index of the texture.  <0 as invalid. */
	int 	texCoord;	/* Required:No,  The set index of texture's TEXCOORD attribute
				   used for texture coordinate mapping.
				   Default=0;
				 */
	float  strength;	/* Required:No, A scalar multiplier controlling the amount
				   of occlusion applied.
				   default=1.0;
				 */

        //extensions
        //extras
};


/*---------------------------------------
   Class E3D_glPBRMetallicRoughness

---------------------------------------*/
class E3D_glPBRMetallicRoughness {
public:
        /* Constructor */
        E3D_glPBRMetallicRoughness();
        /* Destructor */
        //~E3D_glPBRMetallicRoughness();

	bool baseColorFactor_defined;		/* Default=false, If undefined, let E3D_Material.kd keep its own default value. */
	E4D_Vector baseColorFactor;	  	/* Required:No, factors for base color of the material
						 * This value defines linear multipliers for the sampled texels of the base color texture.
						 * number[4] default [1.0,1.0,1.0,1.0],  each componet >=0.0 and <=1.0
						 * RGBA <--------------  E3D_Material.kd
						 */

	//bool baseColorTexture_defined;		/* Default=false */
	E3D_glTextureInfo baseColorTexture;	/* Required:No, the base color texture. this maps to E3D_Material.img_kd
						 *** ---- baseColorTexture.index<0 as undifined!
						 */

	float metallicFactor;			/* Required:No, default=1.0,  min>=0.0; max<=1.0 */
	float roughnessFactor;			/* Required:No, default=1.0,  min>=0.0; max<=1.0 */

	E3D_glTextureInfo metallicRoughnessTexture;  /* Required:No */

	//extensions
	//extras
};

/*-----------------------------------
        Class E3D_glMaterial

-----------------------------------*/
class E3D_glMaterial {
public:
        /* Constructor */
        E3D_glMaterial();
        /* Destructor */
        //~E3D_glMaterial();

	string  name;	  	/* Required:No,Jobject name */

	bool	pbrMetallicRoughness_defined;
	E3D_glPBRMetallicRoughness	pbrMetallicRoughness; 	/* A set paramters to define metallic-roughness material model from
				 				   Physically Based Rendering(PBR) methodology.)
				 				   When undefined.  all default values MUST apply
			       	 				 */
	//bool	normalTexture_defined;
	E3D_glNormalTextureInfo   	normalTexture;	 	/* Required:No, The tangent space normal texture
								 *** normalTexture.index<0 as Undifned!
								 */
	//bool occlutionTexture_defined;
	E3D_glOcclusionTextureInfo 	occlutionTexture; 	/* Required:No, The occlusion texture
								 *** occlutionTexture.index<0 as Undifned!
								 */
	//bool emissiveTexture_defined;
	E3D_glTextureInfo		emissiveTexture;	/* Required:No, The emissive texture
								 *** emissiveTexture.index<0 as Undifned!
								 */

	//bool	emissiveFactor_defined;
	E3D_Vector emissiveFactor;	/* Required:No,  the factors for the emissive color of the material
				  	   number [3], default [0.0, 0.0 ,0.0]

				 	*/

	string alphaMode;	/* Required:No, The alpah rendering mode of the material, default "OPAQUE"
				   Allowed value: "OPAQUE", "MASK", "BLEND"
				 */
	float alphaCutoff;	/* Required:No, The alpha cutoff value of the material, default: 0.5 */

	bool  doubleSided;	/* Required:No, Specifies whether the material is double sided. default: false */

	//extensions
	//extras
};


#endif


