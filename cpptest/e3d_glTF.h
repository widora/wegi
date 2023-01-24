/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

              --- EGI 3D glTF Parsing Classes ---
Reference:
glTF 2.0 Specification (The Khronos® 3D Formats Working Group)

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
#include "e3d_vector.h"
#include "egi.h"
#include "egi_fbgeom.h"
#include "egi_color.h"
#include "egi_image.h"
#include "egi_cstring.h"

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


/* Non Class Functions */
char* E3D_glReadJsonKey(const char *pstr, string & key, string & strval);
int E3D_glExtractJsonObjects(const string & Jarray, vector<string> & Jobjs);

int E3D_glLoadBuffers(const string & Jbuffers, vector<E3D_glBuffer> & glBuffers);
int E3D_glLoadBufferViews(const string & JBufferViews, vector <E3D_glBufferView> & glBufferViews);
int E3D_glLoadAccessors(const string & JAccessors, vector <E3D_glAccessor> & glAccessors);
int E3D_glLoadPrimitiveAttributes(const string & JPAttributes, E3D_glPrimitiveAttribute & glPAttributes);  //<--- NOT ARRAY!
int E3D_glLoadPrimitives(const string & JPrimitives, vector <E3D_glPrimitive> & glPrimitives);
int E3D_glLoadMeshes(const string & JMeshes, vector <E3D_glMesh> & glMeshes);

int E3D_glLoadMaterials(const string & JMaterials, vector <E3D_glMaterial> & glMaterials);
int E3D_glLoadTextureInfo(const string & JTexInfo, E3D_glTextureInfo & glTexInfo);  //<--- NOT ARRAY!
int E3D_glLoadNormalTextureInfo(const string & JNormalTexInfo, E3D_glNormalTextureInfo & glNormalTexInfo);  //<--- NOT ARRAY!
int E3D_glLoadPBRMetaRough(const string & JMetaRough, E3D_glPBRMetallicRoughness & glPBRMetaRough);  //<--- NOT ARRAY!
int E3D_glLoadTextures(const string & JTextures, vector <E3D_glTexture> & glTextures);
int E3D_glLoadImages(const string & JImages, vector <E3D_glImage> & glImages);


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

*/

	/* The value of the property is the index of an accessor that contains the data.
	 * Default =-1 as invalid.
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
//	int joints_0; //[3];
//	int weights_0; //[3];

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
	//targets;			   /* Required:No, An array of morph targets */
	//extensions;
	//extras;
};

/*-------------------------------
        Class E3D_glMesh
--------------------------------*/
class E3D_glMesh {
public:
        /* Constructor */
        //E3D_glMesh();
        /* Destructor */
        //~E3D_glMesh();

	/* attribues{NORMAL, POSITION,TEXTCOORD_n, ..}, indices, material, mode */

	string		 name; /* Required:No, User-defined name of this object */

	/* The value of the property is the index of an accessor that contains the data.*/
	vector<E3D_glPrimitive>  primitives;   /* An array of primitives, each defining geometry to be rendered */
	//weights; /* Array of weights to be applied to the morph target */
	//extensions;
	//extras;
};

/*-------------------------------
        Class E3D_glNode
--------------------------------*/
class E3D_glNode {
public:
        /* Constructor */
        //E3D_glNode();
        /* Destructor */
        //~E3D_glNode();

	E3D_glMesh	mesh;
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

	EGI_FILEMMAP *fmap;   /* MMAP to an underlying file */
 	unsigned char *data;  /* 1. If fmap!=NULL, then data=fmap->fp.
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
				    (unless such layout is explicitly enabled by an extension).
				  */

	/* The hint representing the intended GPU buffer type to use with this buffer view */
        int 	target;	   /* Required: NO, Allowed values: 34962_ARRAY_BUFFER, 34964_ELEMENT_ARRAY_BUFFER */


	/* Pointer to the data in buffer. THIS is NOT the data Owner.
	 *  To be linked/assigned NOT in E3D_glLoadAccessors()
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


