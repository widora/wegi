/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

              --- EGI 3D glTF Classes and Functions ---

This is for E3D_TriMesh::loadGLTF().

Reference:
1. glTF 2.0 Specification (The Khronos® 3D Formats Working Group)
2. glTF models for test:
   https://github.com/KhronosGroup/glTF-Sample-Models
   http://gltf.com, https://www.cgtrader.com
3. Relations of top_leve glTF array.

   Scene ---> node ---> mesh(s) ---> accessor(s) ---> bufferView(s) ---> buffer(s)
		    	    |
			    | ---> material(s) ---> texture(s) ---> image(s) ---> bufferViews(s)
							|
							|---> sampler(s)


	       |---> mesh(s) ....
	       |
	       |---> camera(s)
	       |
   Scene ---> node(s) --->  skin(s) ---> accesors(s)
	       |
	       |---> node(s)



  	(((  ----- Primitive Texture_mapping Roadmap  ----- )))

glPrimitive ---------> glMaterials ---> glPBRMetallicRoughness ---> glTextrueInfo  ---> glTexture ---> glImage
   |			      |                                       |
   |--->glPrimitiveAttribute  |	         			      |---> glPrimitiveAttribute.TEXCOORD_x
       (TEXCOORD[] list)      |
	                      |---> glNormalTexture  ----> glNormalTextureInfo(texCoord) ---> glTexture ---> glImage
	                      |					   |
	                      |					   |---> glPrimitiveAttribute.TEXCOORD_x
                              |
         	              |--->glEmissiveTexture ----> glTextrueInfo  ---> glTexture ---> glImage
	                      |                              |
	                      |				     |---> glPrimitiveAttribute.TEXCOORD_x
	                      |
	                      |--->glOcclutionTexture  ----> glOcclusionTextureInfo ---> glTexture ---> glImage
		      				                        |
                                                                        |--->glPrimitiveAttribute.TEXCOORD_x

   A scene: TODO
   A node: TODO
   A mesh: To define its primitives and attributes relating to accessors.
        data relating to accessors:
	primitive: indices, materials   (all accessor_indices)
	primitive attributes: positions, normals, tangents, texcoords, colors.. (All accessor_indices)
   A accessor: To define a component type and its data chunk relating to a bufferView.
   A bufferView: To define a data chunk relating to a buffer.
   A buffer: To define a data source relating to an URI.


Note:
1. glTF uses a right-hand coordinate system(same as E3D). +Y as UP, +Z as FORWARD, +X as LEFT.
   glTF texture UV coordinate system: origin at leftTop, RIGHTWARD as +u ,DOWNWARD as +v.(same as E3D)
2. All units in glTF are meters and all angles are in radians.
3. Positive rotation is counter_colockwise.
4. All buffer data defined by glTF are in little endian byte order.

TODO:
1. NOW it supports baseColorTexture ONLY.

Journal:
2022-12-14: Creates THIS file.
2022-12-15: Add E3D_glReadJsonKey()
2022-12-16: Add E3D_glLoadBuffers()
2022-12-17: Add E3D_glLoadBufferViews()
2022-12-18:
	1. Add E3D_glLoadAccessors(),
	2. Add E3D_glLoadPrimitiveAttributes(),
	3. Add E3D_glLoadPrimitives(),
	4. Add E3D_glLoadMeshes()
2022-12-19:
	1. Add glElementSize()
2022-12-22:
	1. Add E3D_glMaterial(), E3D_glImage(), E3D_glTextureInfo(),
	       E3D_glNormalTextureInfo(), E3D_glOcclusionTextureInfo(),
	       E3D_glPBRMetallicRoughness(), E3D_glSampler(), E3D_glTexture()
	2. Add E3D_glLoadMaterials()
2022-12-23:
	1. Add E3D_glLoadTextureInfo()
	2. Add E3D_glLoadPBRMetaRough()
2022-12-24:
	1. Add E3D_glLoadTextures()
	2. Add E3D_glLoadImages()
2022-12-28:
	1. E3D_glLoadImages(): If jpeg/png image data embedded in uri, then decode it
	   and read in to imgbuf.

Midas Zhou
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
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
#include "egi_bjp.h"

#include "e3d_vector.h"
#include "e3d_glTF.h"

using namespace std;

/*** The topology type of primitives to render
   0--Points, 1--Lines, 2--Line_Loop, 3--Line_Strip, 4--Triangles
   5--Triangle_strip, 6--Triangle_fan
 */
string glPrimitiveModeName[]={"Points", "Lines", "Line_Loop", "Line_Strip", "Triangles", "Triangle_strip", "Triangle_fan", "Unknown"};
//vector<string> glPrimitiveTopologyMode(7);

/*-----------------------------------------
Return byte size of the element:

@etype: Element type
-------------------------------
   Type       Components
   SCALAR          1
   VEC2            2
   VEC3            3
   VEC4            4
   MAT2            4
   MAT3            9
   MAT4            16

@ctype: Component type
--------------------------------
   Type		 bits
   5120    signed byte     8
   5121    unsigned byte   8
   5122    signed short    16
   5123    unsigned short  16
   5125    unsigned int    32
   5126    float           32

Return:
   >0  size of the element, in bytes.
   <0   Errors
-----------------------------------------*/
int glElementSize(string etype, int ctype)
{
	int comps=-1, bytes=-1;

	/* Component bytes */
	switch(ctype) {
		case 5120:
			bytes=1; break;
		case 5121:
			bytes=1; break;
		case 5122:
			bytes=2; break;
		case 5123:
			bytes=2; break;
		case 5125:
			bytes=4; break;
		case 5126:
			bytes=4; break;
		default: /* Errors */
		     bytes=-1; break;
	}

	/* Element components */
	if(!etype.compare("SCALAR"))
		comps=1;
	else if(!etype.compare("VEC2"))
		comps=2;
	else if(!etype.compare("VEC3"))
		comps=3;
	else if(!etype.compare("VEC4"))
		comps=4;
	else if(!etype.compare("MAT2"))
		comps=4;
	else if(!etype.compare("MAT3"))
		comps=9;
	else if(!etype.compare("MAT4"))
		comps=16;
	else  /* Erros */
		comps=-1;

	/* Check data */
	if(comps<0 || bytes<0)
		return -1;
	else
		return comps*bytes;
}



		/*-------------------------------------
     	      	     Class E3D_glPrimtiveAttribute
		-------------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glPrimitiveAttribute::E3D_glPrimitiveAttribute()
{
	/* Init index of accessor */
        positionAccIndex=-1;
        normalAccIndex=-1;
        tangentAccIndex=-1;

        texcoord=-1;;
        color=-1;
	//joints_0;
	//weights_0;
}


		/*-------------------------------------
     	      	         Class E3D_glPrimitive
		-------------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glPrimitive::E3D_glPrimitive()
{
   indicesAccIndex=-1;
   materialIndex=-1;
   mode=4;   /* 4--Triangles */
   //targets
   //extensions;
   //extras;
}


		/*-------------------------------
        		Class E3D_glBuffer
		--------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glBuffer::E3D_glBuffer()
{
	/* Init */
	byteLength=0;
	fmap=NULL;
	data=NULL;
}

/*--------------------
   The destructor
--------------------*/
E3D_glBuffer::~E3D_glBuffer()
{
	if(fmap!=NULL) {
		egi_fmap_free(&fmap);
	        egi_dpstd("Freed fmap of uri: %s.\n", uri.c_str());

		/* data=fmap->fp */
		data=NULL;
	}
	/* data != fmap->fp */
	else if(data!=NULL) {
		delete [] data;
	}
}

		/*-------------------------------
		        Class E3D_glBufferView
		-------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glBufferView::E3D_glBufferView()
{
	/* Init */
	bufferIndex=0;
	byteLength=0;
	byteOffset=0;
	byteLength=0;
	byteStride=0;
	target=0;

	data=NULL; /* NOT the Owner */
}


		/*-------------------------------
		        Class E3D_glAccessor
		-------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glAccessor::E3D_glAccessor()
{
	/* Init */
        bufferViewIndex=0;
        byteOffset=0;
        componentType=0;
        normalized=false;
        count=0;

	elemsize=0;
	data=NULL; /* NOT the Owner */
}


		/*-------------------------------
		        Class E3D_glImage
		-------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glImage::E3D_glImage()
{
	/* Init */
	bufferViewIndex=-1;

	imgbuf=NULL;
}

/*--------------------
   The destructor
--------------------*/
E3D_glImage::~E3D_glImage()
{
	if(imgbuf)
	    egi_imgbuf_free(imgbuf);
}


		/*------------------------------------
        		Class E3D_glSampler
		------------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glSampler::E3D_glSampler()
{
	/* Init */
	wrapS=10497;
	wrapT=10497;
}

		/*------------------------------------
        		Class E3D_glTexture
		------------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glTexture::E3D_glTexture()
{
	/* Init */
	samplerIndex=-1;
	imageIndex=-1;
}


		/*------------------------------------
        		Class E3D_glTextureInfo
		------------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glTextureInfo::E3D_glTextureInfo()
{
	/* Init */
	index=-1;
	texCoord=0;
}


	/*------------------------------------
   	    Class E3D_glNormalTextureInfo
	------------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glNormalTextureInfo::E3D_glNormalTextureInfo()
{
	/* Init */
	index=-1;
	texCoord=0;
	scale=1.0;
}


	/*-------------------------------------
   	    Class E3D_glOcclusionTextureInfo
	-------------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glOcclusionTextureInfo::E3D_glOcclusionTextureInfo()
{
	/* Init */
	index=-1;
	texCoord=0;
	strength=1.0;
}


		/*------------------------------------------
			Class E3D_glPBRMetallicRoughness
		------------------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glPBRMetallicRoughness::E3D_glPBRMetallicRoughness()
{
	/* Init */
	baseColorFactor_defined=false;
	baseColorFactor.assign(1.0,1.0,1.0,1.0);

	//baseColorTexture_defined=false;   baseColorTexture.index as token

	metallicFactor=1.0;
	roughnessFactor=1.0;
}


		/*-----------------------------------
        		Class E3D_glMaterial
		-----------------------------------*/

/*--------------------
   The constructor
--------------------*/
E3D_glMaterial::E3D_glMaterial()
{
	/* Init */
	emissiveFactor.assign(0.0,0.0,0.0);
	alphaMode="OPAQUE";
	alphaCutoff=0.5;
	doubleSided=false;

}



/*--------------------------------------------------
Extract Json objects from a Json obj array descript.
Each object is written within a pair of {}.
Assume that all glTF top_level objects are preseted
in arry.

@jarray:   Descript of a Json array.
	   Example: [ {...}, {...}, ...{...} ]

@jobjs:	   Json object string as extracted,
	   including {} pair.

Return:
	>=0 Number of Json objects extracted
	<0 Fails
--------------------------------------------------*/
int E3D_glExtractJsonObjects(const string & Jarray, vector<string> & Jobjs)
{
	string Jobj;	/* Json object itme */
	int ocnt=0;
	char *pt=(char *)Jarray.c_str();
	char stok='{', etok='}';  /* Each Json object is written in pair {} */
	char *ps=NULL, *pe=NULL;
	bool  psOK=false, peOK=false;
	int nestCnt=0;  /* Counter for inner nested tag pair of [] */

	/* Read Json objects within brace pair "{..}" (at top level) */
	while(*pt) {
		/* W1. Find stok , stok!=etok */
		if(*pt==stok) {
		    if(nestCnt==0 && !psOK) {
			ps=pt;
			psOK=true;
		    }
		    else { /* Nesting... */
			nestCnt++;
		    }
		}
		/* W2. Find etok */
		else if( *pt==etok ) {
		   if(nestCnt==0 ) {
			pe=pt;
			peOK=true;

			/* ---- { One Json buffer description } ---- */
			Jobj.assign(ps, pe+1-ps); /* Including stok and etok */

			/* Push into JBuffObjs */
			Jobjs.push_back(Jobj);

			/* Count */
			ocnt++;

			/* ---- Reset and continue while() ---- */
			psOK=false, peOK=false;
			nestCnt=0;

			/* MUST pass the last etok, otherwise it's trapped in dead loop here. while()-->W2-->while()  */
			pt++;

			continue;
/* <-------- continue while() */

		   }
		   else {   /* Denesting.. */
			nestCnt--;
			if(nestCnt<0) {
			   egi_dpstd(DBG_RED"nest count <0!, fail to parse Json text.\n"DBG_RESET);
			   return -1;
			}
		   }
		}

		/* W3. Go on.. */
		pt++;
	}


	return ocnt;
}


/*-------------------------------------------------------------------
Read Json key and value in a string, it searchs to get the KEY in the
first pair of "" ,then extract VALUE after the colon :.
Continously call the function to read out all top_level key/value pairs.

glTF Top_Level Json keywords:
  scene, scenes, nodees, meshes, accessors, bufferViews, buffers
  ... ...

		  === Json Syntax ===

1. All Json data is represeted in NAME:VALUE pair, with ':' as delimiter.
2. Types of NAME and VALUE:
	String:		Written in double quotes "".
	Number:		Integer or float type.
	Boolean:	true or false, without double quotes.
			Example:  "saved":false
	null:		empty value, without doubel quotes.
			Example:   "address":null
	Object:		An UNORDERED collection of KEY/VALUE pairs, written in {}.
	Array:		An ORDERED sequence of VALUEs, written in [].
3. glTF JSON data SHOULD be written with UTF-8 encoding without BOM.


Note: 1. Returned strvalue is the string after token ':', it also includes the
   bracketing pair of stok/etok if exists, such as "...", {...}, [...] ets.
   but not necessary, example: "scene": 2,
2. No nesting case for "", see 6.1.
3. TODO. It may return NULL after read key and value, it happens when
   the NEXT char happens to be terminating NULL! --- Rare case though!

@pstr:		Pointer to json text/string.

@key:   	Key string as returned.
		1. stok/etok " " are stripped.

@strval:	Value of the key as returned.
		1. If the VALUE is a string, then " " are stripped.
		2. If the VALUE is Json object or array, then {} OR [] keeps in strval.
		   TODO: if scalars in [], strip []?,  Example: "Color" : [0.263, 0.443, 0.312]
			 OR if string in []: "Color": [ "magenta", "blue", "cyan" ]
		   OK,keep it!

		3. Or if scalar, no stok/etok.
		4. Or true, false, null.

Return:
	NULL	Fail to read, Ending parsing json text.
		TODO: OR get key/value. NEXT is the END of the pstr????
	!NULL	Pointer to next/remaining json content.
---------------------------------------------------------------------*/
char* E3D_glReadJsonKey(const char *pstr, string & key, string & strval)
{
	if(pstr==NULL)
		return NULL;

	char *pt=(char *)pstr;

	char stok=0, etok=0; /* token pairs: [], { } or " ". 0s for digits */

	/* start/end position of Key OR strValue */
	char *ps=NULL, *pe=NULL;
	bool  psOK=false, peOK=false;

	int nestCnt=0;  /* Counter for inner nested tag pair of [] */

	/* 1. Find first pair of "", which quotes the key. Json keys MUST be wrriten with double quotes.*/
	while(*pt) {
		if( *pt=='"') {
		   if(!psOK) {
			ps=pt;
			psOK=true;
		   }
		   else {
			pe=pt;
			peOK=true;
			break;   	/* ----------> break */
		   }
		}

		/* go on... */
		pt++;
	}

	/* 2. Check if pair "" found */
	if(!psOK || !peOK) {
//		egi_dpstd(DBG_YELLOW"No Json key found!\n"DBG_RESET);
		return NULL;
	}
	else {
		/* Copy key, strip ""  */
		key.assign(ps+1, pe-ps-1);
		//egi_dpstd(DBG_YELLOW"Found Key: '%s'\n"DBG_RESET, key.c_str());

		/* Reset psOK,peOK... NO HERE, SEE 6. */
		//psOK=false;
		//peOK=false;
	}

	/* 3. Pass ':' and get to value zone */
	while(*pt) {
	   if(*pt==':') {
		pt++;
		break;  /* ---------> Break */
	   }
	   else
		pt++;
	}
	if(*pt==0)
		return NULL;

//	egi_dpstd(DBG_YELLOW" ':' is found! \n"DBG_RESET);


/* Note: Consider following Key/Value cases
	case 1:  Starts with Digits OR string "..", one key and one value type.
	Example: "scene": 0,
	         "name" : "xxx"

	case 2:  Starts with '{', just one group data(keys and values) in {...}.
	Example: "asset" : { "version" : "2.0", ..., ... }

	case 3:  Starts with bracket '[', an array of group data(objects) in [...], each object enclosed in  {..}
	Example: "meshes" : [ {...}, {...}, {...} ]

	case 4: Booleans
	Example: "Saved":true, "Hold":false

	case 5: NULL
	Example: "Address":null,
 */
	/* 4. Get the leading/ending token */
	stok=0; etok=0;
	while(*pt) {
	   if(*pt=='"') {
		stok='"';
		etok='"';
		break;  /* -----> Break */
	   }
	   else if(*pt=='[') {
		stok='[';
		etok=']';
		break;  /* -----> Break */
	   }
	   else if(*pt=='{') {
		stok='{';
		etok='}';
		break;  /* -----> Break */
	   }
	   // else: assume as a scalar.  <<<< TODO >>>>
	   else if(*pt=='.' || isdigit(*pt)) {
		//printf(" *pt='%c':  <<<< scalar >>>>\n", *pt);
		break;  /* -----> Break */
	   }
	   /* Booleans */
	   else if(*pt=='t') {
		if(strlen(pt)>4) {
			strval.assign("true");
			return  pt+4;		/* ----------> Return */
		}
	   }
	   else if(*pt=='f') {
		if(strlen(pt)>5) {
			strval.assign("false");
			return  pt+5;		/* ----------> Return */
		}
	   }
	   /* null */
	   else if(*pt=='n') {
		if(strlen(pt)>4) {
			strval.assign("null");
			return  pt+4;		/* ----------> Return */
		}
	   }

	   /* Go on.. */
	   pt++;
	}
	if(*pt==0)
		return NULL;

//	egi_dpstd(DBG_YELLOW"stok=%c is found \n"DBG_RESET,stok);

	/* 5. Case_1: Value as a scalar */
	if(stok==0 && etok==0) {
		/* NOW: pt points to '.' or first digit */
		ps=pt;

		while(*pt && (*pt=='.' || isdigit(*pt)) )
			pt++;

		/* NOW: pt points to end of the scalar */
		pe=pt;

		/* Check */
		if(ps==pe) {
			egi_dpstd(DBG_RED"Json digits presentation error!\n"DBG_RESET);
			return NULL;
		}

		/* Copy value */
		strval.assign(ps, pe-ps);

		/* OK */
		return pt;  /* -----------> Return */
	}

	/* NOW: pt points to the starting token: stok */

	/* 6. Case_2: Value as [] {} or "".  Find the pair [..] OR "", OR { }, and extract content */
	//pt++; Nope, DO NOT skip stok!
	nestCnt=0;
	psOK=false; peOK=false;
	while(*pt) {
	   /* 6.1 Case: " "  !!!---CAUTION---!!! stok==etok here!
	    *  Note: NO nesting case for "" !
	    */
	   if(*pt==stok && stok=='"' ) {
		/* Find the pair */
		if(!psOK) {
			ps=pt;
			psOK=true;
		}
		else {
			pe=pt;
			peOK=true;
			break;  /* -----------> break */
		}
	   }
	   /* 6.2 Case: [] or {} */
	   else {
		/* Find the pair */
		if(*pt==stok) {
		    if(nestCnt==0 && !psOK) {
			ps=pt;
			psOK=true;
		    }
		    else { /* Nesting... */
			nestCnt++;
		    }
		}
		else if( *pt==etok ) { /* !!!---CAUTION---!!!   For "", stok==etok!! */
		   if(nestCnt==0) {
			pe=pt;
			peOK=true;
			break;		/* -----------> break */
		   }
		   else {   /* Denesting.. */
			nestCnt--;
			if(nestCnt<0) {
			   egi_dpstd(DBG_RED"nest count <0!, fail to parse Json text.\n"DBG_RESET);
			   return NULL;
			}
		   }
		}
	    }

	    /* go on... */
	    pt++;
	}

	/* 7. Check if pair [..], "..", or {..}, is found */
	if(!psOK || !peOK) {
		egi_dpstd(DBG_YELLOW"Fail to found key value!\n"DBG_RESET);
		return NULL;
	}
	else {
//		egi_dpstd(DBG_YELLOW"psOK&&peOK, ps: %s \n"DBG_RESET, ps);

		/* Copy value */
		if(stok=='"') /* Strip " " */
		     strval.assign(ps+1, pe-ps-1);
		else {	     /* Keep {} and [] (stok=etok=0 see 5.case_1 ) */

		     //egi_dpstd(" start 7.1: *ps='%c',*pe='%c', strval.size()=%d. strval: %s \n", *ps, *pe, strval.size(), strval.c_str());
		     strval.assign(ps, pe+1-ps); /* Including stok and etok */
		}
		/* Reset psOK,peOK */
		//psOK=false;
		//peOK=false;
	}

	/* 8. skip last etok and return to next char */
	return pt+1;
}


/*------------------------------------------------
Parse Jbuffers and load resources to glBuffers.

	 !!! --- CAUTION --- !!!
Input glBuffers SHOULD be empty! Since it has pointer
members, vector capacity growth is NOT allowed here!

@Jbuffers: Json description for key 'buffers'.
@glBuffers: To pass out Vector <E3D_glBuffer>

Jbuffers Example:
  "buffers" :  <<--- This part NOT included in Jbuffers --->>
  [
    {
      "byteLength": 1234,
      "uri": "mis/data.bin"
    },
    {
	...
    }
    ...
  ]


Return:
	>0	OK, number of E3D_glBuffers loaded.
	<0	Fails
-------------------------------------------------*/
int E3D_glLoadBuffers(const string & Jbuffers, vector <E3D_glBuffer> & glBuffers)
{
	vector<string>  JBuffObjs; /* Json description for n buffers, each included in a pair of { } */
	string Jitem;	/* Json description for a E3D_glBuffer. */
	int nj=0;	/* Items in JBuffObjs */

	char *pstr=NULL;
	string key;
	string value;

	/* Check input */
	if(!glBuffers.empty())
		egi_dpstd(DBG_YELLOW"Input glBuffers SHOULD be empty, it will be cleared/resized!\n"DBG_RESET);
	//else
	//	glBuffers.clear();

#if 1  /////////////////////  Function  /////////////////
	egi_dpstd("Start func...\n");

	E3D_glExtractJsonObjects(Jbuffers, JBuffObjs);

#else  /////////////////////   Codes  ////////////////////
	egi_dpstd("Start coding...\n");

	/* Read E3D_glBuffer Json objects, each with brace pair "{..}" (at top level) */
	char *pt=(char *)Jbuffers.c_str();
	char stok='{', etok='}';
	char *ps=NULL, *pe=NULL;
	bool  psOK=false, peOK=false;
	int nestCnt=0;  /* Counter for inner nested tag pair of [] */

	while(*pt) {
		/* W1. Find stok */
		if(*pt==stok) {
		    if(nestCnt==0 && !psOK) {
			ps=pt;
			psOK=true;
		    }
		    else { /* Nesting... */
			nestCnt++;
		    }
		}
		/* W2. Find etok */
		else if( *pt==etok ) {
		   if(nestCnt==0 ) {
			pe=pt;
			peOK=true;

			/* ---- { One Json buffer description } ---- */
			Jitem.assign(ps, pe+1-ps); /* Including stok and etok */

			/* Push into JBuffObjs */
			JBuffObjs.push_back(Jitem);

			/* ---- Reset and continue while() ---- */
			psOK=false, peOK=false;
			nestCnt=0;

			/* MUST pass the last etok, otherwise it's trapped in dead loop here. while()-->W2-->while()  */
			pt++;

			continue;
/* <-------- continue while() */

		   }
		   else {   /* Denesting.. */
			nestCnt--;
			if(nestCnt<0) {
			   egi_dpstd(DBG_RED"nest count <0!, fail to parse Json text.\n"DBG_RESET);
			   return -1;
			}
		   }
		}

		/* W3. Go on.. */
		pt++;
	}

#endif /////////////////////////////////////////////

	/* Parse Json object of glBuffer item by itme */
	nj=JBuffObjs.size();
	egi_dpstd(DBG_GREEN"Totally %d glBuffer Json objects found.\n"DBG_RESET, nj);

	/* Resize glBuffers, which is empty NOW. */
	glBuffers.resize(nj);

	for(int k=0; k<nj; k++)  {

		/* Json descript for the buffer */
		pstr=(char *)JBuffObjs[k].c_str();
		//printf("pstr: %s\n", pstr);

/*-------------------------------
        string  	name;
	string  	uri;
        int     	byteLength;
	EGI_FILEMMAP 	*fmap;
	unsigned char 	*data;
--------------------------------*/

		/* Read and parse key/value pair */
		while( (pstr=E3D_glReadJsonKey(pstr, key, value)) ) {
			//printf("key: %s, value: %s\n", key.c_str(), value.c_str());
			//printf("Left string is: %s\n", pstr);
			if(!key.compare("name")) {
				glBuffers[k].name=value;
			}
			else if(!key.compare("byteLength")) {
				glBuffers[k].byteLength=atoi(value.c_str());
			}
			else if(!key.compare("uri")) {
                                glBuffers[k].uri=value;
                        }
			else
				egi_dpstd(DBG_YELLOW"Undefined key found! key: %s, value: %s\n"DBG_RESET, key.c_str(), value.c_str());
		}

		/* TODO Check data integrity. */

		/* Load uri resource */
		if(glBuffers[k].uri.size()>0) {
			/* Buffer data MAY alternatively be embedded in the glTF file via data: URI with base64 encoding. */
			/* data:application/octet-stream;base64... etc. */
			 /* Example "data:application/octet-stream;base64,r21jvDmktTfnQru7x3VOvGektTdsifq7p8gpvBHxuDfo..." */
			if( strncmp(glBuffers[k].uri.c_str(), "data:application/octet-stream;base64", 36)==0 )  {
				egi_dpstd(DBG_YELLOW"Buffer data is embedded in Json file!\n"DBG_RESET);
				try {
					glBuffers[k].data=new unsigned char[glBuffers[k].byteLength];
				}
        			catch ( std::bad_alloc ) {
		                	egi_dpstd("Fail to allocate glBuffers[%d].data!\n", k);
                			return -1;
        			}
				/* Decode base64 into data. Frome after ...;base64, */
				const char *pt=strstr(glBuffers[k].uri.c_str(), "base64,");
				int datasize;
				datasize=egi_decode_base64(0, pt+7, strlen(pt+7), (char *)glBuffers[k].data);
				if(datasize!=glBuffers[k].byteLength) {
					egi_dpstd(DBG_RED"glBuffers[%d].byteLength=%d: Decode base64 get WRONG datasize=%d\n"DBG_RESET,
							k, glBuffers[k].byteLength, datasize);
					return -1;
					//exit(1);
				}
				egi_dpstd(DBG_YELLOW"Succeed to decode base64 string to glBuffers[%d].data.\n"DBG_RESET,k);
			}
			/* A data file URI */
			else {
				glBuffers[k].fmap=egi_fmap_create(glBuffers[k].uri.c_str(), 0, PROT_READ, MAP_SHARED);
				if(glBuffers[k].fmap==NULL) {
				    egi_dpstd(DBG_RED"Fail to fmap uri: %s\n"DBG_RESET, glBuffers[k].uri.c_str());
				    //exit(1);
				}
				else {
				    /* Check data length */
				    if( glBuffers[k].byteLength !=glBuffers[k].fmap->fsize) {
				        egi_dpstd(DBG_RED"glBuffers[%d].byteLength!=fmap.fsize, data error.\nCheck URI:%s\n"DBG_RESET,
					  k, glBuffers[k].uri.c_str());
					egi_fmap_free(&glBuffers[k].fmap);
				    } else {
					/* Link data to fmap->fp */
					glBuffers[k].data=(unsigned char *)glBuffers[k].fmap->fp;

					egi_dpstd(DBG_GREEN"Succeed to mmap URI: '%s', and link to glBuffers[%d].data\n"DBG_RESET,
						glBuffers[k].uri.c_str(), k);
				    }
				}
			}
		}

	}

	/* OK */
	return nj;
}


/*------------------------------------------------
Parse JBufferViews and load data to glBufferViews.

	 !!! --- CAUTION --- !!!
Input glBufferViews SHOULD be empty! Since it has pointer
members, vector capacity growth is NOT allowed here!

@JBufferViews: Json description for glTF key 'bufferviews'.
@glBufferViews: To pass out Vector <E3D_glBufferView>

JBufferViews Example:
  "bufferViews" :  <<--- This part NOT included in JBufferViews --->>
   [
    {
      "buffer" : 0,
      "byteOffset" : 0,
      "byteLength" : 16,
      "target" : 34963
    },
    {
      "buffer" : 0,
      "byteOffset" : 16,
      "byteLength" : 248,
      "target" : 34962
    },
    ...
  ]


Return:
	>0	OK, number of E3D_glBufferViews loaded.
	<0	Fails
-------------------------------------------------*/
int E3D_glLoadBufferViews(const string & JBufferViews, vector <E3D_glBufferView> & glBufferViews)
{
	vector<string>  JBVObjs; /* Json description for n bufferViews, each included in a pair of { } */
	string Jitem;	/* Json description for a E3D_glBufferView. */
	int nj=0;	/* Items in JBVObjs */

	char *pstr=NULL;
	string key;
	string value;

	/* Check input */
	if(!glBufferViews.empty())
		egi_dpstd(DBG_YELLOW"Input glBufferViews SHOULD be empty, it will be cleared/resized!\n"DBG_RESET);

	/* Extract all Json bufferView objects and put to JBVObjs */
	E3D_glExtractJsonObjects(JBufferViews, JBVObjs);

	/* Parse Json object of glBuffer item by itme */
	nj=JBVObjs.size();
	egi_dpstd(DBG_GREEN"Totally %d glBufferView Json objects found.\n"DBG_RESET, nj);

	/* Resize glBuffers, which is empty NOW. */
	glBufferViews.resize(nj);

	for(int k=0; k<nj; k++)  {
		/* Json descript for the buffer */
		pstr=(char *)JBVObjs[k].c_str();
		//printf("pstr: %s\n", pstr);

/*-------------------------------
        string  name;
        int     bufferIndex; ( glTF Json name "buffer" )
        int     byteOffset;
        int     byteLength;
        int     byteStride;
        int     target;
--------------------------------*/

		/* Read and parse key/value pairs */
		while( (pstr=E3D_glReadJsonKey(pstr, key, value)) ) {
			//printf("key: %s, value: %s\n", key.c_str(), value.c_str());
			if(!key.compare("name")) {
				glBufferViews[k].name=value;
			}
			else if(!key.compare("buffer")) {  /* <---- NOT bufferIndex!! */
				glBufferViews[k].bufferIndex=atoi(value.c_str());
			}
			else if(!key.compare("byteOffset")) {
				glBufferViews[k].byteOffset=atoi(value.c_str());
			}
			else if(!key.compare("byteLength")) {
				glBufferViews[k].byteLength=atoi(value.c_str());
			}
			else if(!key.compare("byteStride")) {
				glBufferViews[k].byteStride=atoi(value.c_str());
			}
			else if(!key.compare("target")) {
				glBufferViews[k].target=atoi(value.c_str());
			}
			else
				egi_dpstd(DBG_YELLOW"Undefined key found! key: %s, value: %s\n"DBG_RESET, key.c_str(), value.c_str());
		}

		/* TODO Check data integrity. */

	} /*END for() */

	/* OK */
	return nj;
}


/*------------------------------------------------
Parse JAccessors and load data to glAccessors.

	 !!! --- CAUTION --- !!!
Input glAccessors SHOULD be empty! Since it has pointer
members, vector capacity growth is NOT allowed here!

@JAccessors: Json description for glTF key 'accessors'.
@glAccessors: To pass out Vector <E3D_glAccessor>

JAccessors Example:
  "accessors" :  <<--- This part NOT included in JAccessors --->>
  [
    {
      "bufferView" : 0,
      "byteOffset" : 0,
      "componentType" : 5123,
      "count" : 10,
      "type" : "SCALAR",
      "max" : [ 123 ],
      "min" : [ 0 ]
    },
    {
      "bufferView" : 1,
      "byteOffset" : 0,
      "componentType" : 5126,
      "count" : 3,
      "type" : "VEC3",
      "max" : [ 1.0, 10.0, 20.0 ],
      "min" : [ -10.0, -10.0, -20.0 ]
    },
    ...
  ]


Return:
	>0	OK, number of E3D_glAccessor loaded.
	<0	Fails
-------------------------------------------------*/
int E3D_glLoadAccessors(const string & JAccessors, vector <E3D_glAccessor> & glAccessors)
{
	vector<string>  JAObjs; /* Json description for n Jaccessors, each included in a pair of { } */
	string Jitem;	/* Json description for a E3D_glAccessor. */
	int nj=0;	/* Items in JAObjs */

	char *pstr=NULL;
	string key;
	string value;

	/* Check input */
	if(!glAccessors.empty())
		egi_dpstd(DBG_YELLOW"Input glAccessors SHOULD be empty, it will be cleared/resized!\n"DBG_RESET);

	/* Extract all Json access objects and put to JAObjs */
	E3D_glExtractJsonObjects(JAccessors, JAObjs);

	/* Get size */
	nj=JAObjs.size();
	egi_dpstd(DBG_GREEN"Totally %d glAccessor Json objects found.\n"DBG_RESET, nj);

	/* Resize glAccessors, which is empty NOW. */
	glAccessors.resize(nj);

	/* Parse Json object of glAccessor item by itme */
	for(int k=0; k<nj; k++)  {
		/* Json descript for the accessor */
		pstr=(char *)JAObjs[k].c_str();
		//printf("pstr: %s\n", pstr);

/*-------------------------------
        string  name;
        int     bufferViewIndex; (glTF Json name "bufferView")
        int     byteOffset;
        int     componentType;
        bool    normalized;
        int     count;
        string  type;  (Allowed values: SCALAR,VEC2,VEC3,VEC4,MAT2,MAT3,MAT4)
   TODO:
        vector<> max;
        vector<> min;
        sparse;
        extensions;
        extra;
--------------------------------*/

		/* Read and parse key/value pairs */
		while( (pstr=E3D_glReadJsonKey(pstr, key, value)) ) {
			//egi_dpstd("key: %s, value: %s\n", key.c_str(), value.c_str());
			if(!key.compare("name")) {
				glAccessors[k].name=value;
			}
			else if(!key.compare("bufferView")) {  /* <---- NOT bufferIndex!! */
				glAccessors[k].bufferViewIndex=atoi(value.c_str());
			}
			else if(!key.compare("byteOffset")) {
				glAccessors[k].byteOffset=atoi(value.c_str());
			}
			else if(!key.compare("componentType")) {
				glAccessors[k].componentType=atoi(value.c_str());
			}
			else if(!key.compare("normalized")) {
				glAccessors[k].normalized=true;
			}
			else if(!key.compare("count")) {
				glAccessors[k].count=atoi(value.c_str());
			}
			else if(!key.compare("type")) {
				glAccessors[k].type=value;
			}
			else
				egi_dpstd(DBG_YELLOW"Undefined key found!\n key: %s, value: %s\n"DBG_RESET, key.c_str(), value.c_str());
		}

		/* Compute element size */
		glAccessors[k].elemsize=glElementSize(glAccessors[k].type, glAccessors[k].componentType);
		if(glAccessors[k].elemsize<0) {
			egi_dpstd(DBG_RED"glAccessors[%d].componentType OR .type incorrect!\n"DBG_RESET,k);
			//OK, save elemsize as <0.
		}

		/* TODO Check data integrity */

	} /*END for() */

	/* OK */
	return nj;
}


/*------------------------------------------------
Parse JPAttributes and load data to glPAttribute.

	!!!--- CAUTION ---!!!
Input glPAttributes SHOULD be inited!

@JPAttributes: Json description for glTF key 'attributes'.
@glPAttributes: To pass out Vector <E3D_glPrimitiveAttribute>

JPAttributes Example:
  "attributes": <<--- This part NOT included in JPAttributes --->>
   {
            "NORMAL": 5,
            "POSITION": 4,
            "TEXCOORD_0": 6
   }

Note:
	!!!  --- NOTICED ---- !!!
1. Actually there is ALWAYS one glPrimitiveAttribute in "primitives"

TODO:
1. XXX NOW assume there is only one "attribute" in each "primitive". --0K, yes in {} pair.
2. NOW assume there is only one set of TEXCOORD, and it's TEXCOORD_0.

Return:
	>0	OK, number of E3D_glPrimitiveAttribute loaded.
	<0	Fails
-------------------------------------------------*/
int E3D_glLoadPrimitiveAttributes(const string & JPAttributes, E3D_glPrimitiveAttribute & glPAttributes)
{
	vector<string>  JPAObjs; /* Json description for n JPAttribute, each included in a pair of { } */
	string Jitem;	/* Json description for a E3D_glAccessor. */

	int nj=0;	/* Items in JPAObjs  ---- ALWAYS 1 ----  */

	char *pstr=NULL;
	string key;
	string value;

	/* XXX Check input --- NOT array! */
	//if(!glPAttributes.empty())
	//	egi_dpstd(DBG_YELLOW"Input glPAttributes SHOULD be empty, it will be cleared/resized!\n"DBG_RESET);

	/* Extract all Json primitive attribute objects and put to JPAObjs */
	E3D_glExtractJsonObjects(JPAttributes, JPAObjs);

	/* Get size ---- SHOULD BE 1 --- */
	nj=JPAObjs.size();
	egi_dpstd(DBG_GREEN"Totally %d glPrimitiveAttribute Json objects found.\n"DBG_RESET, nj);
	if(nj<1) return 0;

	/* XXX Resize glPAttributes, which assumes as empty NOW. */
	//glPAttributes.resize(nj);

	/* Parse Json object of glPrimitiveAttribute item by itme */
	//for(int k=0; k<nj; k++) {
	for(int k=0; k<1; k++)  {   /* There is ONE attribute in each Jprimitive */
		/* Json descript for  PrimitiveAttribuess */
		pstr=(char *)JPAObjs[k].c_str();
		//printf("pstr: %s\n", pstr);

/*------------------------------------------------
int positionAccIndex;  ( Json name "POSITION" )
int normalAccIndex;    ( Json name "NORMAL" )
int tangentAccIndex;   ( Json name "TANGENT" )

//following are also accessor indices
int texcoord;		( Json name "TEXCOORD_0" )
int color;		( Json name "COLOR_0" )
//int joints_0;		( Json name "JOINTS_0" )
//int weights_0;		( Json name "WEIGHTS_0" )
--------------------------------------------------*/

		/* Read and parse key/value pairs */
		while( (pstr=E3D_glReadJsonKey(pstr, key, value)) ) {
			//egi_dpstd("key: %s, value: %s\n", key.c_str(), value.c_str());
			if(!key.compare("POSITION"))
				glPAttributes.positionAccIndex=atoi(value.c_str());
			else if(!key.compare("NORMAL"))
				glPAttributes.normalAccIndex=atoi(value.c_str());
			else if(!key.compare("TANGENT"))
				glPAttributes.tangentAccIndex=atoi(value.c_str());
			/* TODO: Assume always TEXCOORD_0 */
			else if(!key.compare("TEXCOORD_0"))
				glPAttributes.texcoord=atoi(value.c_str());
			else if(!key.compare("COLOR_0"))
				glPAttributes.color=atoi(value.c_str());
			else
				egi_dpstd(DBG_YELLOW"Undefined key found!\n key: %s, value: %s\n"DBG_RESET, key.c_str(), value.c_str());
		}

		/* TODO Check data integrity */

	} /*END for() */

	/* OK */
	return 1;
}


/*------------------------------------------------
Parse JPrimitives and load data to glPrimitives

	 !!! --- CAUTION --- !!!
Input glMeshes SHOULD be empty! Since it has pointer
members, vector capacity growth is NOT allowed here!

@JPrimitives: Json description for glTF key 'primitives'.
@glPrimitives: To pass out Vector <E3D_glPrimitive>

JPrimitives Example:
  "primitives": <<--- This part NOT included in JPRrimitives --->>
      [
        {
          "attributes": {
            "NORMAL": 5,
            "POSITION": 4,
            "TEXCOORD_0": 6
          },
          "indices": 7,
          "material": 0,
          "mode": 4      <------ If NOT defined, mode=4 as default.
        },
	{
	...
	}
      ]

Return:
	>0	OK, number of E3D_glPrimitive loaded.
	<0	Fails
-------------------------------------------------*/
int E3D_glLoadPrimitives(const string & JPrimitives, vector<E3D_glPrimitive> & glPrimitives)
{
	vector<string>  JPObjs; /* Json description for n JPrimitive, each included in a pair of { } */
	string Jitem;	/* Json description for a E3D_glPrimitive. */

	int nj=0;	/* Items in JPObjs  */

	char *pstr=NULL;
	string key;
	string value;

	/* Check  */
	if(!glPrimitives.empty())
		egi_dpstd(DBG_YELLOW"Input glPrimitives SHOULD be empty, it will be cleared/resized!\n"DBG_RESET);

	/* Extract all Json primitive objects and put to JPObjs */
	E3D_glExtractJsonObjects(JPrimitives, JPObjs);

	/* Get size */
	nj=JPObjs.size();
	egi_dpstd(DBG_GREEN"Totally %d glPrimitvie Json objects found.\n"DBG_RESET, nj);
	if(nj<1) return 0;

	/* Resize glPrimitives, which assume as empty NOW. */
	glPrimitives.resize(nj);

	/* Parse Json object of glPrimitive item by itme */
	for(int k=0; k<nj; k++) {
		/* Json descript for a primitive */
		pstr=(char *)JPObjs[k].c_str();
		//printf("pstr: %s\n", pstr);

/*--------------------------------------------------------------------
   E3D_glPrimitiveAttribute  attributes;  (Json name "attributes")
   int   indicesAccIndex;   (Json name "indices")
   int   materialIndex;     (Json name "material")
   int    mode;	      	 (Json name "mode")
   //targets
   //extensions;
   //extras;
---------------------------------------------------------------------*/

		/* Read and parse key/value pairs */
		while( (pstr=E3D_glReadJsonKey(pstr, key, value)) ) {
			//egi_dpstd("key: %s, value: %s\n", key.c_str(), value.c_str());
			if(!key.compare("attributes")) {
				/* Load primitive attributes */
				if( E3D_glLoadPrimitiveAttributes(value, glPrimitives[k].attributes)<1 )
					egi_dpstd(DBG_RED"Fail to load attributes in: %s\n"DBG_RESET, value.c_str());
			}
			else if(!key.compare("indices"))
				 glPrimitives[k].indicesAccIndex=atoi(value.c_str());
			else if(!key.compare("material"))
				 glPrimitives[k].materialIndex=atoi(value.c_str());
			else if(!key.compare("mode"))
				 glPrimitives[k].mode=atoi(value.c_str());
			else
				egi_dpstd(DBG_YELLOW"Undefined key found!\n key: %s, value: %s\n"DBG_RESET, key.c_str(), value.c_str());
		}

		/* TODO Check data integrity */

	} /*END for() */

	/* OK */
	return nj;
}

/*------------------------------------------------
Parse JMeshes and load data to glMeshes.

	 !!! --- CAUTION --- !!!
Input glMeshes SHOULD be empty! Since it has pointer
members, vector capacity growth is NOT allowed here!

@JMeshes: Json description for glTF key 'meshes'.
@glMeshes: To pass out Vector <E3D_glMesh>

JMeshes Example:
  "meshes" :  <<--- This part NOT included in JMeshes --->>
  [
    {
      "name": "Head",
      "primitives": [
        {
          "attributes": {
            "NORMAL": 1,
            "POSITION": 0,
            "TEXCOORD_0": 2
          },
          "indices": 3,
          "material": 0,
          "mode": 4
        }
      ]
    },
    {
	(meshe 2)
    },
    ...
  ]


TODO:
1. NOW assume there is only one set of TEXCOORD, and it's TEXCOORD_0.

Return:
	>0	OK, number of E3D_glMesh loaded.
	<0	Fails
-------------------------------------------------*/
int E3D_glLoadMeshes(const string & JMeshes, vector <E3D_glMesh> & glMeshes)
{
	vector<string>  JMObjs; /* Json description for n Jmeshes, each included in a pair of { } */
	string Jitem;	/* Json description for a E3D_glMesh. */
	int nj=0;	/* Items in JMObjs */

	char *pstr=NULL;
	string key;
	string value;

	/* Check input */
	if(!glMeshes.empty())
		egi_dpstd(DBG_YELLOW"Input glMeshes SHOULD be empty, it will be cleared/resized!\n"DBG_RESET);

	/* Extract all Json Mesh objects and put to JMObjs */
	E3D_glExtractJsonObjects(JMeshes, JMObjs);

	/* Get JMObjs.size */
	nj=JMObjs.size();
	egi_dpstd(DBG_GREEN"Totally %d glMesh Json objects found.\n"DBG_RESET, nj);

	/* Resize glMeshes, which assume as empty NOW. */
	glMeshes.resize(nj);

	/* Parse Json object of glMesh item by itme */
	for(int k=0; k<nj; k++)  {
		/* Json descript for a mesh */
		pstr=(char *)JMObjs[k].c_str();
		//printf("pstr: %s\n", pstr);

/*----------------------------------------------------
E3D_glMesh
	string name;
        vector<E3D_glPrimitive>  primitives;   -----> glTF Json name "primitives"
	//weights;
	//extensions;
	//extras;

------
see .E3D_glPrimitive
       E3D_glPrimitiveAttribute  attributes;  -----> glTF Json name "attributes"
       int              indicesAccIndex;
       int              materialIndex;
       int              mode;
	//targes
	//extensions;
	//extras;

E3D_glPrimitiveAttribute
	int positionAccIndex;
        int normalAccIndex;
        int tangentAccIndex;

        //following are also accessor indices
        int texcoord_0;
        int color_0;
        int joints_0;
        int weights_0
 
----------------------------------------------------*/

		/* Read and parse key/value pairs */
		while( (pstr=E3D_glReadJsonKey(pstr, key, value)) ) {
			//egi_dpstd("key: %s, value: %s\n", key.c_str(), value.c_str());
			if(!key.compare("name")) {
				glMeshes[k].name=value;
			}
			else if(!key.compare("primitives")) {
				/* Load primitives */
				if( E3D_glLoadPrimitives(value, glMeshes[k].primitives)<1 )
					egi_dpstd(DBG_RED"Fail to load glPrimitives in: %s\n"DBG_RESET, value.c_str());
			}
			else
				egi_dpstd(DBG_YELLOW"Undefined key found!\n key: %s, value: %s\n"DBG_RESET, key.c_str(), value.c_str());
		}

		/* TODO Check data integrity */

	} /*END for() */

	/* OK */
	return nj;
}


/*-------------------------------------------------------------
Parse JTexInfo and load data to glTexInfo   (<-----NOT ARRYA!)

	 !!! --- CAUTION --- !!!
glTexInfo SHOULD be inited.

@JTexInfo: Json description for glTF key of E3D_TextureInfo type,
	   "baseColorTexture", "emissiveTexture" eg.
@glTexInfo: To pass out value.

JTexInfo Example: ( NOT ARRAY! )
  "baseColorTexture" :  <<--- This part NOT included in JTexInfo --->>
   {
            "index" : 1
			   (<----- texCoord NOT defined, default=0)
   }


Return:
	>0	Ok number of E3D_glTextureInfo loaded.
	<0	Fails
---------------------------------------------------------------*/
int E3D_glLoadTextureInfo(const string & JTexInfo, E3D_glTextureInfo & glTexInfo)
{
	char *pstr=NULL;
	string key;
	string value;

	if(JTexInfo.empty())
		return -1;

	/* Json descript for a textureinfo */
	pstr=(char *)JTexInfo.c_str();
	//printf("pstr: %s\n", pstr);

/*----------------------------------------------------
	int  index;
	int  texCoord;
	//Extensions
	//extras
----------------------------------------------------*/

	/* Read and parse key/value pairs */
	while( (pstr=E3D_glReadJsonKey(pstr, key, value)) ) {
		//egi_dpstd("key: %s, value: %s\n", key.c_str(), value.c_str());
		if(!key.compare("index")) {
			glTexInfo.index=atoi(value.c_str());
			if(glTexInfo.index<0) {
				egi_dpstd(DBG_RED"Error index<0! in: %s\n"DBG_RESET, value.c_str());
				glTexInfo.index=0;
			}
		}
		else if(!key.compare("texCoord")) {
			glTexInfo.texCoord=atoi(value.c_str());
			if(glTexInfo.texCoord<0) {
				egi_dpstd(DBG_RED"Error texCoord<0! in: %s\n"DBG_RESET, value.c_str());
				glTexInfo.texCoord=0;
			}
		}
		else
			egi_dpstd(DBG_YELLOW"Undefined key found!\n key: %s, value: %s\n"DBG_RESET, key.c_str(), value.c_str());
	}

	/* TODO Check data integrity */

	/* OK */
	return 1;
}

/*------------------------------------------------
Parse JMetaRough and load data to glPBRMetaRough   (<-----NOT ARRYA!)

	 !!! --- CAUTION --- !!!
InputglPBRMetaRough SHOULD be inited.

@JMetaRough: Json description for glTF key "pbrMetallicRoughness".
@glPBRMetaRough: To pass out Vector <E3D_glPBRmetallicRoughness>

JMetaRough Example: ( NOT ARRAY! )

  "pbrMetallicRoughness" :  <<--- This part NOT included in JMetaRough --->>
   {
       "baseColorTexture" : {
              "index" : 1
        },
        "metallicFactor" : 0,
        "roughnessFactor" : 0.7913385629653931
   }

Example 2:
   {
       "baseColorFactor" : [
           0.7686280012130737,
           0.6705880165100098,
           0.37254899740219116,
           1
        ],
        "metallicFactor" : 0,
        "roughnessFactor" : 0.7440944910049438
    }

Return:
	>0	OK, number of E3D_glMaterial loaded. (ALWAYS 1)
	<0	Fails
-------------------------------------------------*/
int E3D_glLoadPBRMetaRough(const string & JMetaRough, E3D_glPBRMetallicRoughness & glPBRMetaRough)
{
	char *pstr=NULL;
	string key;
	string value;

	if(JMetaRough.empty())
		return -1;

	/* Json descript for a PBRMetallicRoughness */
	pstr=(char *)JMetaRough.c_str();
	//printf("pstr: %s\n", pstr);

/*----------------------------------------------------
	E3D_Vector 		baseColorFactor; (number [4])
				strValue example: [0.2223, 0.3454, 0.8612, 1 ]  --[]included
	E3D_glTextureInfo 	baseColorTexture;
        float 			metallicFactor;
        float 			roughnessFactor;
	//Extensions
	//extras
----------------------------------------------------*/

	/* Read and parse key/value pairs */
	while( (pstr=E3D_glReadJsonKey(pstr, key, value)) ) {
		//egi_dpstd("key: %s, value: %s\n", key.c_str(), value.c_str());
		if(!key.compare("baseColorFactor")) {
			/* Set token*/
			glPBRMetaRough.baseColorFactor_defined=true;
			/* Read numer [4] */
			sscanf(value.c_str()+1, "%f,%f,%f,%f", /* +1, pass the '[' */
				&glPBRMetaRough.baseColorFactor.x, &glPBRMetaRough.baseColorFactor.y,
				&glPBRMetaRough.baseColorFactor.z, &glPBRMetaRough.baseColorFactor.w
			);
		}
		else if(!key.compare("baseColorTexture")) {
			if( E3D_glLoadTextureInfo(value, glPBRMetaRough.baseColorTexture)<1 )
				egi_dpstd(DBG_RED"Fail to load glPBRMetaRough.baseColorTextrue in: %s\n"DBG_RESET, value.c_str());
			//else  /* XXX ok, glPBRMetaRough.index<0 as undefined! */
			//	glPBRMetaRough.baseColorTexture_defined=true;
		}
		else if(!key.compare("metallicFactor")) {
			sscanf(value.c_str(),"%f", &glPBRMetaRough.metallicFactor);
		}
		else if(!key.compare("roughnessFactor")) {
			sscanf(value.c_str(),"%f", &glPBRMetaRough.roughnessFactor);
		}
		else
			egi_dpstd(DBG_YELLOW"Undefined key found!\n key: %s, value: %s\n"DBG_RESET, key.c_str(), value.c_str());
	}

	/* TODO Check data integrity */

	/* OK */
	return 1;
}


/*------------------------------------------------
Parse JMaterials and load data to glMaterials.

	 !!! --- CAUTION --- !!!
Input glMaterials SHOULD be empty! Since it has pointer
members, vector capacity growth is NOT allowed here!

@JMaterials: Json description for glTF key 'materials'.
@glMaterials: To pass out Vector <E3D_glMaterial>

JMaterials Example:
  "materials" :  <<--- This part NOT included in JMaterials --->>
   [
        {
            "doubleSided" : true,
            "name" : "CGTGband",
            "pbrMetallicRoughness" : {
                "baseColorFactor" : [
                    0.4357537428533221,
                    0.3463456348538666,
                    0.5372945773729394,
                    1
                ],
                "metallicFactor" : 0
            }
        },
	... ...
        {
            "doubleSided" : true,
            "name" : "gris",
            "pbrMetallicRoughness" : {
                "baseColorTexture" : {
                    "index" : 1
                },
                "metallicFactor" : 0,
                "roughnessFactor" : 0.5372945773723535
            }
        },
	... ...
   ]


Return:
	>0	OK, number of E3D_glMaterial loaded.
	<0	Fails
-------------------------------------------------*/
int E3D_glLoadMaterials(const string & JMaterials, vector <E3D_glMaterial> & glMaterials)
{
	vector<string>  JMObjs; /* Json description for n JMaterial, each included in a pair of { } */
	string Jitem;	/* Json description for a E3D_glMaterial. */
	int nj=0;	/* Items in JMObjs */

	char *pstr=NULL;
	string key;
	string value;

	/* Check input */
	if(!glMaterials.empty())
		egi_dpstd(DBG_YELLOW"Input glMaterials SHOULD be empty, it will be cleared/resized!\n"DBG_RESET);

	/* Extract all Json material objects and put to JMObjs */
	E3D_glExtractJsonObjects(JMaterials, JMObjs);

	/* Get JMObjs.size */
	nj=JMObjs.size();
	egi_dpstd(DBG_GREEN"Totally %d glMaterial Json objects found.\n"DBG_RESET, nj);

	/* Resize glMaterials, which assume as empty NOW. */
	glMaterials.resize(nj);

	/* Parse Json object of glMaterial item by itme */
	for(int k=0; k<nj; k++)  {
		/* Json descript for a material */
		pstr=(char *)JMObjs[k].c_str();
		//printf("pstr: %s\n", pstr);


/*-------------------------------------------------------
  string  name;
  E3D_glPBRMetallicRoughness      pbrMetallicRoughness;
  E3D_glNormalTextureInfo         normalTexture;
  E3D_glOcclusionTextureInfo      occlutionTexture;
  E3D_glTextureInfo               emissiveTexture;

  E3D_Vector emissiveFactor;

  string alphaMode;
  float alpahCutoff;

  bool  doubleSided;

  //extensions
  //extras
--------------------------------------------------------*/

		/* Read and parse key/value pairs */
		while( (pstr=E3D_glReadJsonKey(pstr, key, value)) ) {
			//egi_dpstd("key: %s, value: %s\n", key.c_str(), value.c_str());
			if(!key.compare("name")) {
				glMaterials[k].name=value;
			}
			else if(!key.compare("pbrMetallicRoughness")) {
				/* Load pbrMetallicRoughness */
				if( E3D_glLoadPBRMetaRough(value, glMaterials[k].pbrMetallicRoughness)<1 )
					egi_dpstd(DBG_RED"glMaterials[%d]: Fail to load pbrMetallicRoughness in: %s\n"DBG_RESET, k, value.c_str());
				else
					 glMaterials[k].pbrMetallicRoughness_defined=true;
			}
			else if(!key.compare("emissiveTexture")) {
                        	if( E3D_glLoadTextureInfo(value, glMaterials[k].emissiveTexture)<1 )
                                	egi_dpstd(DBG_RED"glMaterials[%d]: Fail to load emissiveTexture in: %s\n"DBG_RESET, k, value.c_str());
			}
			else if(!key.compare("alphaMode")) {
				glMaterials[k].alphaMode=value;
			}
			else if(!key.compare("alphaCutoff")) {
				glMaterials[k].alphaCutoff=atof(value.c_str());
			}
			else if(!key.compare("doubleSided")) {
				glMaterials[k].doubleSided=true;
			}
			else
				egi_dpstd(DBG_YELLOW"Undefined key found!\n key: %s, value: %s\n"DBG_RESET, key.c_str(), value.c_str());
		}

		/* TODO Check data integrity */

	} /*END for() */

	/* OK */
	return nj;
}


/*------------------------------------------------
Parse JTextures and load data to glTextures.

	 !!! --- CAUTION --- !!!
Input glTextures SHOULD be empty! Since it has pointer
members, vector capacity growth is NOT allowed here!

@JTextures: Json description for glTF key 'textures'.
@glTextures: To pass out Vector <E3D_glTexture>

JTextures Example:
  "textures" :  <<--- This part NOT included in JTextures --->>
   [
        {
            "sampler" : 0,
            "source" : 0
        },
        {
            "sampler" : 0,
            "source" : 0
        },
        {
            "sampler" : 0,
            "source" : 1
        },
	... ...
   ]


Return:
	>0	OK, number of E3D_glTexture loaded.
	<0	Fails
-------------------------------------------------*/
int E3D_glLoadTextures(const string & JTextures, vector <E3D_glTexture> & glTextures)
{
	vector<string>  JTObjs; /* Json description for n JTexture, each included in a pair of { } */
	string Jitem;	/* Json description for a E3D_glTexture. */
	int nj=0;	/* Items in JTObjs */

	char *pstr=NULL;
	string key;
	string value;

	/* Check input */
	if(!glTextures.empty())
		egi_dpstd(DBG_YELLOW"Input glTextures SHOULD be empty, it will be cleared/resized!\n"DBG_RESET);

	/* Extract all Json texture objects and put to JMObjs */
	E3D_glExtractJsonObjects(JTextures, JTObjs);

	/* Get JTObjs.size */
	nj=JTObjs.size();
	egi_dpstd(DBG_GREEN"Totally %d glTexture Json objects found.\n"DBG_RESET, nj);

	/* Resize glTextures, which assume as empty NOW. */
	glTextures.resize(nj);

	/* Parse Json object of glTexture item by itme */
	for(int k=0; k<nj; k++)  {
		/* Json descript for a texture */
		pstr=(char *)JTObjs[k].c_str();
		//printf("pstr: %s\n", pstr);

/*-------------------------------------------------------
  string name;
  int  samplerIndex; (Json name "sampler")
  int  imageIndex;   (Json name "source")
  //extensions
  //extras
--------------------------------------------------------*/

		/* Read and parse key/value pairs */
		while( (pstr=E3D_glReadJsonKey(pstr, key, value)) ) {
			//egi_dpstd("key: %s, value: %s\n", key.c_str(), value.c_str());
			if(!key.compare("name")) {
				glTextures[k].name=value;
			}
			else if(!key.compare("sampler")) {
				glTextures[k].samplerIndex=atoi(value.c_str());
			}
			else if(!key.compare("source")) {
				glTextures[k].imageIndex=atoi(value.c_str());
			}
			else
				egi_dpstd(DBG_YELLOW"Undefined key found!\n key: %s, value: %s\n"DBG_RESET, key.c_str(), value.c_str());
		}

		/* TODO Check data integrity */

	} /*END for() */

	/* OK */
	return nj;
}


/*------------------------------------------------
Parse JImages and load data to glImages.

	 !!! --- CAUTION --- !!!
Input glImages SHOULD be empty! Since it has pointer
members, vector capacity growth is NOT allowed here!

@JImages: Json description for glTF key 'images'.
@glImages: To pass out Vector <E3D_glImage>

JImages Example:
  "images" :  <<--- This part NOT included in JTextures --->>
   [
        {
            "bufferView" : 16,
            "mimeType" : "image/jpeg",
            "name" : "fish"
        },
        {
            "bufferView" : 25,
            "mimeType" : "image/jpeg",
            "name" : "head"
        },
	... ...
   ]


Return:
	>0	OK, number of E3D_glImage loaded.
	<0	Fails
-------------------------------------------------*/
int E3D_glLoadImages(const string & JImages, vector <E3D_glImage> & glImages)
{
	vector<string>  JIObjs; /* Json description for n JImage, each included in a pair of { } */
	string Jitem;	/* Json description for a E3D_glImage. */
	int nj=0;	/* Items in JIObjs */

	char *pstr=NULL;
	string key;
	string value;

	/* 1. Check input */
	if(!glImages.empty())
		egi_dpstd(DBG_YELLOW"Input glImages SHOULD be empty, it will be cleared/resized!\n"DBG_RESET);

	/* 2. Extract all Json Image objects and put to JMObjs */
	E3D_glExtractJsonObjects(JImages, JIObjs);

	/* 3. Get JIObjs.size */
	nj=JIObjs.size();
	egi_dpstd(DBG_GREEN"Totally %d glImage Json objects found.\n"DBG_RESET, nj);

	/* 4. Resize glTextures, which assume as empty NOW. */
	glImages.resize(nj);

	/* 5. Parse Json object of glTexture item by itme */
	for(int k=0; k<nj; k++)  {
		/* 5.1 Json descript for the accessor */
		pstr=(char *)JIObjs[k].c_str();
		//printf("pstr: %s\n", pstr);

/*-------------------------------------------------------
  string  name;
  string  uri;
  string  mimeType;   (Allowed values: image/jpeg, image/png)
  int     bufferViewIndex; (Json name "bufferView")

  //extensions
  //extras
--------------------------------------------------------*/

		/* 5.2 Read and parse key/value pairs */
		while( (pstr=E3D_glReadJsonKey(pstr, key, value)) ) {
			//egi_dpstd("key: %s, value: %s\n", key.c_str(), value.c_str());
			if(!key.compare("name")) {
				glImages[k].name=value;
			}
			else if(!key.compare("uri")) {
				glImages[k].uri=value;
			}
			else if(!key.compare("mimeType")) {
				glImages[k].mimeType=value;
			}
			else if(!key.compare("bufferView")) {
				glImages[k].bufferViewIndex=atoi(value.c_str());
			}
			else
				egi_dpstd(DBG_YELLOW"Undefined key found!\n key: %s, value: %s\n"DBG_RESET, key.c_str(), value.c_str());
		}

		/* TODO Check data integrity */

	} /*END for() */

	/* 6. Load image into imgbuf */
	for(size_t k=0; k<glImages.size(); k++) {
	   /* 6.1 URI is defined */
	   if(glImages[k].uri.size()>0) {
	   	/* 6.1.1 URI is embedded with image data.
		   Example: "uri" : "data:image/png;base64,Rw0KG....
		 	    "uri" : "data:image/jpeg;base64,fg5Bw....
		 */
		if( strncmp(glImages[k].uri.c_str(), "data:image/png;base64,", 22)==0
		    || strncmp(glImages[k].uri.c_str(), "data:image/jpeg;base64,", 23)==0  ) {
			const char *pt=strstr(glImages[k].uri.c_str(), "base64,");
			char *data=NULL;
			int datasize;
                        try {
	                       data=new char[strlen(pt+7)]; //actually use 2/3 size */
                        }
                        catch ( std::bad_alloc ) {
                                egi_dpstd("glImges[%d]:Fail to allocate data for base64 decoding!\n", k);
                                return -1;
                        }

			/* Decode base64 */
 			datasize=egi_decode_base64(0, pt+7, strlen(pt+7), data);

			/* Read imgbuf from png data */
			if( strncmp(glImages[k].uri.c_str(), "data:image/png;base64,", 22)==0 )
				glImages[k].imgbuf=egi_imgbuf_readPngBuffer((const unsigned char *)data, (unsigned long)datasize);
			/* Read imgbuf from jpeg data */
			else
				glImages[k].imgbuf=egi_imgbuf_readJpgBuffer((const unsigned char *)data, (unsigned long)datasize);

			/* Free data */
			delete []data;

			if(glImages[k].imgbuf==NULL) {
			        egi_dpstd(DBG_RED"glImages[%d]:Fail to read imgbuf from uri embedded data!\n"DBG_RESET, k);
				//return -1;
			}

		}
		/* 6.1.2 URI refers to an external image file. */
		else {
			glImages[k].imgbuf=egi_imgbuf_readfile(glImages[k].uri.c_str());
			if(glImages[k].imgbuf==NULL) {
			    egi_dpstd(DBG_RED"glImages[%d]: Fail to read imgbuf from uri: %s\n"DBG_RESET, k, glImages[k].uri.c_str());
			    //return -1;
			}
		}
	   }
	   /* 6.2 URI is not defined */
	   #if 0 /* Try to load imgbuf from bufferView data if mimeType is "image/jpeg" OR "image/png" */
	   else {
			/* See at E3D_TriMesh::loadGLTF() 4a. */
	   }
	   #endif

	} /* End for(k) */


	/* OK */
	return nj;
}
