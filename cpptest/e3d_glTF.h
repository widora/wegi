/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

              --- EGI 3D glTF Parsing Classes ---
Reference:
glTF 2.0 Specification (The Khronos® 3D Formats Working Group)

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


/* Non Class Functions */
char* E3D_glReadJsonKey(const char *pstr, string & key, string & strval);
int E3D_glExtractJsonObjects(const string & Jarray, vector<string> & Jobjs);

int E3D_glLoadBuffers(const string & Jbuffers, vector<E3D_glBuffer> & glBuffers);
int E3D_glLoadBufferViews(const string & JBufferViews, vector <E3D_glBufferView> & glBufferViews);
int E3D_glLoadAccessors(const string & JAccessors, vector <E3D_glAccessor> & glAccessors);
int E3D_glLoadPrimitiveAttributes(const string & JPAttributes, E3D_glPrimitiveAttribute & glPAttributes);
int E3D_glLoadPrimitives(const string & JPrimitives, vector <E3D_glPrimitive> & glPrimitives);
int E3D_glLoadMeshes(const string & JMeshes, vector <E3D_glMesh> & glMeshes);

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
TANGENT     VEC4 float XYZW vertex tangents,(XYZ is normalized, W is(-1 or +1)

TEXCOORD_n  VEC2 float Textrue coordinates
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
	int texcoord; //_0; //[3];
	int color; //_0; //[3];
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
	string 	     uri;  /* */
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



#endif


