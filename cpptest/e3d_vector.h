/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

EGI 3D Vector Class
EGI 3D Quaternion Class
EGI 3D Quatrix Class
EGI 3D RotationTranslation Matrix Class
EGI 3D Projection Matrix (Struct)

Refrence:
1. "3D Math Primer for Graphics and Game Development"
			by Fletcher Dunn, Ian Parberry


Journal:
2021-07-28:	Create egi_vector3D.h
2021-07-29:
	1. Class E3D_RTMatrix.
2021-08-10:
	1. Add E3D_RTMatrix operator*();
	2. Add E3D_RTMatrix operator+();
	3. Add setupProject().
	4. For Class E3D_RTMatrix
	   If a function has a E3D_RTMatrix to return, then it will Segmentfault!
	   float *pmat; ----> float pmat[4*3];
2021-08-11:
	1. Add E3D_Vector operator * (const E3D_Vector &va, const E3D_RTMatrix  &ma)
2021-08-18:
	1. Add struct E3D_ProjectFrustum, as E3DS_ProjMatrix.

2021-08-19:
	1. Add E3D_draw_circle(),  +E3D_draw_line().
2021-08-20:
	1. Move E3D_draw_xxx functions to e3d_trimesh.h
	2. Add E3D_RTMatrix::zeroRotation()
2021-09-22:
	1. Add vectorRGB().
2021-10-05:
	1. Add void  E3D_transform_vectors().
2021-10-12:
	1. Add E3D_RTMatrix :: operator *=(const E3D_RTMatrix & ma)
	2. Add void combIntriRotation().
2021-10-13:
	1. Add E3D_RTMatrix::transpose()
	2. Add E3D_combGlobalIntriRotation() ---> rename  E3D_combIntriTranslation() 10-23
	3  Constructor E3D_RTMatrix() { identity(); } <---identity!

2021-10-20:
	1. Add E3D_RTMatrix::isIdentity()
	2. Add E3D_RTMatrix::isOrthogonal()
2021-10-21:
	1. Add E3D_RTMatrix::orthNormalize()
2021-10-23:
	1. Add E3D_combIntriRotation()
	2. Add E3D_combIntriTranslation()
2021-10-26:
	1. Add E3D_combExtriRotation()
2022-08-06:
	1. Add E3D_Ray, E3D_Plane,
	2. Add E3D_RayVerticalRay(), E3D_RayParallelRay(), E3D_RayParallelPlane()
	   E3D_RayIntsectPlane()
2022-08-08:
	1. Add E3D_Vector::transform()
	2. Add E3D_Plane::transform()

2022-08-09:
        1. Split e3d_vector.cpp from e3d_vector.h
2022-08-25:
	1. Remane 'Radial' to 'Ray'
2022-09-28:
        1. struct E3D_ProjectFrustum(): E3D Clippling Matrix parameters.


Midas Zhou
知之者不如好之者好之者不如乐之者
midaszhou@yahoo.com(Not in use since 2022_03_01)
------------------------------------------------------------------*/
#ifndef __E3D_VECTOR_H__
#define __E3D_VECTOR_H__

#include <stdio.h>
#include <math.h>
#include <iostream>
#include "egi_debug.h"
#include "egi_fbdev.h"
#include "egi_fbgeom.h"
#include "egi_math.h"

using namespace std;

#define VPRODUCT_EPSILON        (1.0e-6)  /* -6, Unit vector vProduct=vct1*vtc2 */
#define VPRODUCT_NEARZERO	(1.0e-35)  /* Check parallel/vertical */

typedef struct
{
         float x;
         float y;
	 float z;
} E3D_POINT;

/* Classes */
class E3D_Vector;
class E3D_RTMatrix;
class E3D_Ray;
class E3D_Plane;
struct E3D_ProjectFrustum;

/* NON_Class Functions: E3D_Vector */
float E3D_vector_magnitude(const E3D_Vector &v); // OBSOLETE, see E3D_Vector::module()
E3D_Vector E3D_vector_crossProduct(const E3D_Vector &va, const E3D_Vector &vb);
float E3D_vector_angleAB(const E3D_Vector &va, const E3D_Vector &vb);
E3D_Vector operator *(float k, const E3D_Vector &v);
float E3D_vector_distance(const E3D_Vector &va, const E3D_Vector &vb);
E3D_Vector E3D_vector_reflect(const E3D_Vector &vi, const E3D_Vector &fn);


/* NON_Class Functions: Compute barycentric coordinates of point pt to triangle vt[3] */
bool E3D_computeBarycentricCoord(const E3D_Vector vt[3],const E3D_Vector &pt, float bc[3]);

/* NON_Class Functions: E3D_Ray, E3D_Plane */
bool E3D_RayVerticalRay(const E3D_Ray &rad1, const E3D_Ray &rad2);
bool E3D_RayParallelRay(const E3D_Ray &rad1, const E3D_Ray &rad2);
bool E3D_RayParallelPlane(const E3D_Ray &rad, const E3D_Plane &plane);
//E3D_Vector E3D_RayIntersectPlane(const E3D_Ray &rad, const E3D_Plane &plane, bool &forward);
bool E3D_RayIntersectPlane(const E3D_Ray &rad, const E3D_Plane &plane, E3D_Vector &vp, bool &forward);


/* NON_Class Functions: E3D_RTMatrix */
E3D_RTMatrix operator* (const E3D_RTMatrix &ma, const E3D_RTMatrix &mb);
E3D_RTMatrix operator+ (const E3D_RTMatrix &ma, const E3D_RTMatrix &mb);
void E3D_getCombRotation(const E3D_RTMatrix &ma, const E3D_RTMatrix &mb, E3D_RTMatrix &mc);
//void E3D_combIntriRotation(char axisTok, float ang, E3D_RTMatrix &RTmat);
//void E3D_combIntriRotation(char axisTok, float ang,  E3D_RTMatrix CoordMat, E3D_RTMatrix & RTmat);
void E3D_combGlobalIntriRotation(const char *axisToks, float ang[3],  E3D_RTMatrix CoordMat, E3D_RTMatrix & RTmat);
void E3D_combIntriRotation(const char *axisToks, float ang[3],  E3D_RTMatrix & RTmat);
void E3D_combIntriTranslation(E3D_Vector Vxyz,  E3D_RTMatrix & RTmat);
void E3D_combIntriTranslation(float tx, float ty, float tz,  E3D_RTMatrix & RTmat);
void E3D_combExtriRotation(const char *axisToks, float ang[3],  E3D_RTMatrix & RTmat);

/* NON_Class Functions: E3D_Vector, E3D_RTMatrix */
E3D_Vector operator* (const E3D_Vector &va, const E3D_RTMatrix  &ma);
void  E3D_transform_vectors( E3D_Vector *vts, unsigned int vcnt, const E3D_RTMatrix &RTMatrix);

/*-----------------------------
	Class: E3D_Point
3D Point.
-----------------------------*/
#define E3D_Point E3D_Vector

/*-----------------------------
	Class: E3D_Vector
3D Vector.
-----------------------------*/
class E3D_Vector {
public:
	float x,y,z;

	/* Constructor */
	E3D_Vector();
	E3D_Vector(const E3D_Vector &v);
	E3D_Vector(float ix, float iy, float iz);

	/* Destructor */
	~E3D_Vector();

	/* Vector operator: zero */
	void zero(void);

	/* Vector normalize */
	int normalize(void);

	/* Vector assign */
	void assign(float nx, float ny, float nz);

	/* Module */
	float module(void) const;

	/* Overload operator '=' */
	E3D_Vector & operator =(const E3D_Vector &v);

	/* If is infinity */
	bool isInf() const;

        /* If is zero */
        bool isZero() const;

	/* Overload operator '==' */
	bool operator ==(const E3D_Vector &v) const;

 	/* Overload operator '!=' */
	bool operator !=(const E3D_Vector &v) const;

	/* Vector operator '-' with one operand */
	E3D_Vector operator -() const;

	/* Vector operator '-' with two operands */
	E3D_Vector operator -(const E3D_Vector &v) const;

	/* Vector operator '+' with two operands */
	E3D_Vector operator +(const E3D_Vector &v) const;

	/* Vector operator '*' with a scalar at right! (left see below) */
	E3D_Vector operator *(float a) const;

	/* Vector operator '/' with a scalar at right. */
	E3D_Vector operator /(float a) const;

	/* Overload operator '+=' */
	E3D_Vector & operator +=(const E3D_Vector &v);

	/* Overload operator '-=' */
	E3D_Vector & operator -=(const E3D_Vector &v);

	/* Overload operator '*=' */
	E3D_Vector & operator *=(float a);

	/* Overload operator '/=' */
	E3D_Vector & operator /=(float a);

	/* Overload operator '*' for Dot_Product  */
	float operator *(const E3D_Vector &v) const;

	/* Vectorize 16bits color to RGB(0-255,0-255,0-255) to xyz(0-1.0, 0-1.0, 0-1.0) */
	void  vectorRGB(EGI_16BIT_COLOR color);

	/* Covert vector xyz(0-1.0, 0-1.0, 0-1.0) to 16bits color. RGB(0-255,0-255,0-255) */
	EGI_16BIT_COLOR  color16Bits() const;

	/* Print */
	void print(const char *name);

	/* Transform */
	void transform(const E3D_RTMatrix  &RTMatrix);
};


/*-----------------------------
	Class: E3D_Ray
E3D Ray
-----------------------------*/
class E3D_Ray {
public:
	E3D_Vector vp0; /* Startpoit, default (0,0,0) */
	E3D_Vector vd;  /* Direction, default (1,0,0), NOT necessary to be an unit vector. */

	/* Constructor */
	E3D_Ray();
	E3D_Ray(const E3D_Vector &Vp, const E3D_Vector &Vd);
	E3D_Ray(float px, float py, float pz, const E3D_Vector &Vd);
	E3D_Ray(float px, float py, float pz, float dx, float dy, float dz);

	/* Destructor */
	~E3D_Ray();
};


/*-----------------------------
	Class: E3D_Plane
E3D Plane
-----------------------------*/
class E3D_Plane {
public:
	E3D_Vector vn; /* plane normal,(a,b,c), default as (0,0,1). NOT necessary to be an unit vector */
	float fd;	/* fd=P*vn=a*x+b*y+c*z, a constant value for all points on the plane.
			 * default =0.0
			 */

	/* Constructor */
	E3D_Plane();
	E3D_Plane(const E3D_Vector &Vn, float d);
	E3D_Plane(float a, float b, float c, float d);
	E3D_Plane(const E3D_Vector &vp1, const E3D_Vector &vp2, const E3D_Vector &vp3);

	/* Functions */
	void transform(const E3D_RTMatrix &RTmatrix);

	/* Destructor */
	~E3D_Plane();
};


/*-----------------------------------------------
       		Class: E3D_RTMatrix
3D Transform Matrix (4x3)
Rotation+Translation: Orthogonal Matrix
Scale is NOT orthogonal operation!

Note:
1. If it comprises ONLY Rotation+Translation,
   then it's an Orthogonal Matrix.
2. For inertial Coord, just zeroTranslation().

-----------------------------------------------*/
class E3D_RTMatrix {
public:
        //const int nr=4;         /* Rows */
        //const int nc=3;         /* Columns */
        //float* pmat;		/* If a function has a E3D_RTMatrix to retrun, then it will Segmentfault!!! */
	float pmat[4*3];
	/*** Note:
	 * Rotation Matrix:
		   pmat[0,1,2]: m11, m12, m13
		   pmat[3,4,5]: m21, m22, m23
		   pmat[6,7,8]: m31, m32, m33
	 * Translation:
		   pmat[9,10,11]: tx,ty,tz
	 * If zeroTranslation, then it's under Inertial Coord.

		              !!!--- CAVEATS ---!!!
	   1.  pamt[] has two components: RotationMat and TranslationMat
	   2.  *E3D_RTMatrix = *(RotationMat*TranslationMat),  !!! NOT *(TranslationMat*RotationMat) !!!

	 */

	/* Constructor */
	E3D_RTMatrix();

	/* Destructor */
	~E3D_RTMatrix();

	/* Print */
	void print(const char *name=NULL) const;

	/* Matrix identity: Zero Translation and Zero Rotation */
	void identity();

	/* Multiply a matrix with a float */
	E3D_RTMatrix & operator * (float a);

        /* Overload operator '*=', TODO to test. */
        E3D_RTMatrix & operator *=(const E3D_RTMatrix & ma);

	/* Zero Translation */
	void zeroTranslation();

	/* Zero Rotation */
	void zeroRotation();

	/* Transpose: for an orthogonal matrix, its inverse is equal to its transpose! */
	void transpose();

	/* Inverse the matrix. Rotation/Scale component + Translation component */
	void inverse();

	/* Check wheather the matrix is identity */
	bool isIdentity();

	/* Check wheather the matrix is orthogonal */
	bool isOrthogonal();

	/* Orthogonalize+Normalize the RTMatrix. */
	void orthNormalize();

	/* Set part of Translation TxTyTz in pmat[] */
	void setTranslation( const E3D_Vector &tv );

	/* Set part of Translation TxTyTz in pmat[] */
	void setScaleXYZ( float sx, float sy, float sz);

	/* Set part of Translation TxTyTz in pmat[] */
	void setTranslation( float x, float y, float z );

	/* Add up translation */
	void addTranslation( float dx, float dy, float dz );

	/* Set part of RotationMatrix in pmat[] */
	void setRotation(char chaxis, float angle); /* one axis rotation */
	void setRotation( const E3D_Vector &axis, float angle);
	void setRotation( const E3D_Vector &vfrom, const E3D_Vector &vto );
	void setScaleRotation( const E3D_Vector &vfrom, const E3D_Vector &vto );
	void setTransformMatrix( const E3D_Vector &Vas, const E3D_Vector &Vae, const E3D_Vector &Vbs, const E3D_Vector &Vbe);
	void combExtriRotation(const char *axisToks, float ang[3]);
	void combIntriRotation(const char *axisToks, float ang[3]);

	/* Set this matrix as a projection Matrix */
	void setupProject(const E3D_Vector &n);
};


/*-----------------------------------------------
       		Class: E3D_Quaternion

Note:
1. Here all quaterioins are unit quaternion!
   [w (x,y,z)] --> cos(ang/2), sin(ang/2)Nx, sin(ang/2)Ny, sin(ang/2)Nz
2. Quaternion applys for Rotation component ONLY
-----------------------------------------------*/
class E3D_Quaternion {
public:
	float w;	/* cos(ang/2) */
	float x,y,z;    /* sin(ang/2)Nx, sin(ang/2)Ny, sin(ang/2)Nz */

	/* Constructor */
	E3D_Quaternion();

	/* Destructor */
	//~E3D_Quaternion();

	/* Functions */
	void identity();
        void print(const char *name=NULL) const;

	/* Normalize */
	void normalize();

	/* Overload operator '*' for Cross_Product  */
	E3D_Quaternion  operator *(const E3D_Quaternion &q) const;
	E3D_Quaternion & operator *=(const E3D_Quaternion &q);

	/* Inverse quaternion */
	void inverse();

	/* Function: get rotation angle(radian) */
	float getRotationAngle() const;
	E3D_Vector getRotationAxis() const;

	/* Functions: Set rotation */
	void setRotation(char chAxis, float angle);
	void setIntriRotation(const char *axisToks, float ang[3]);
	void setExtriRotation(const char *axisToks, float ang[3]);

	/* Functions: convert to/from RTMatrix */
	void toMatrix(E3D_RTMatrix &mat) const;
	void fromMatrix(const E3D_RTMatrix &mat);

	/* Functions: interpolation */
	void slerp(const E3D_Quaternion &q0, const E3D_Quaternion &q1, float rt); /* Spherical Linear Interpolation */
};

/*-----------------------------------------------
       		Class: E3D_Quatrix
Quaternion+Translation(Tx,Ty,Tx)
-----------------------------------------------*/
class E3D_Quatrix {
public:
	E3D_Quaternion  q;  //NOT qt, qt for quatrix.  HK2022-11-03
	float 		tx, ty, tz;


	/* Constructor */
	E3D_Quatrix();

	/* Destructor */
	//~E3D_Quatrix();

	/* Functions */
	void identity();
        void print(const char *name=NULL) const;

	/* Function: get rotation angle(radian) */
	float getRotationAngle() const;
	E3D_Vector getRotationAxis() const;

	/* Functions: Set rotation */
	void setRotation(char chAxis, float angle);
	void setIntriRotation(const char *axisToks, float ang[3]);
	void setExtriRotation(const char *axisToks, float ang[3]);
	/* Function: Set translation */
	void setTranslation(float dx, float dy, float dz);

	/* Functions */
	void toMatrix(E3D_RTMatrix &mat) const;
	void fromMatrix(const E3D_RTMatrix &mat);

	/* Functions: interpolation */
	void interp(const E3D_Quatrix &qt0, const E3D_Quatrix &qt1, float rt);
};

/*------------------------------------------------------
DONT fully understand projection matrix under NDC,
so use this struct first!  :)))))))

Projection frustum ---> Projectoin matrix

	     !!!--- CAUTION ----!!!
0. projectPoints() and reverse_projectPoints() use other
   mapping algorithm, they DO NOT need aabbccddABCD.
1. Be carefule with mixing type calculation. (float and integer here).
2. For float type: 6 digits accuracy, for double type: 15 digits accuracy!
   see TODO_4 also.
3. Clipping matrix item A,B,C,D must be updated/recomputed
   whenever any parameter of the view frustum changes.	   <--------

TODO:
1. other parameters to define the shape of the frustum.
XXX 2. Replace struct E3DS_ProjMatrix with a class Matrix
   under Normalized Device Coordinates(NDC).
   Ref. OpenGL Projection Matrix.
3. To be included/replaced by E3D_Camera!
4. In calculating PERSPECTIVE view_frustum NDC:
   It has high precision at near plane,
   BUT very little precision at far plane. It means
   small change of Zeye will NOT change Zndc at all!
   that MAY result in failing to pick out meshes far
   away and out side of the view frustum.
   ( For ISOMETRIC view_frustum NDC mapping, since
    Ze is irrelevant to Xn and Yn, so no such problem exists. )

   Examples:  ( Float type with 6 digits accuracy )
        vpts[7].assign(0,0, 10000000(dfar)+100); ---- Out of view frustum
            pointOutFrustumCode() result ERROR: within frustum! code=0x00, Vector NDC vtp:(-0.000000, 0.000000, 1.000000)
        vpts[7].assign(0,0, 10000000+4000); ---- Out of view frustum
	    pointOutFrustumCode() result OK:  code=0x20, Vector NDC vpt:(-0.000000, 0.000000, 1.000000)
        vpts[7].assign(0,0, 10000000+60000); ---- Out of view frustum
	    ointOutFrustumCode() result OK: code=0x20, Vector NDC vpt:(-0.000000, 0.000000, 1.000001) <----- See last digit

--------------------------------------------------------*/
enum {
	E3D_ISOMETRIC_VIEW	=0,
	E3D_PERSPECTIVE_VIEW    =1,
};
typedef struct E3D_ProjectFrustum  E3DS_ProjMatrix;
struct E3D_ProjectFrustum {
	/* Projectoin type, for quick response. */
	int type;		/*  0:	Isometric projectoin(default).
				 *  1:  Perspective projectoin
				 *  Others: ...
				 */

	/* Distance from the Foucs(eye origin) to projetionPlane */
	int	dv;

	/* Define the shape of the Viewing Frustum */
	int	dnear;		/* Distancd from the Foucs point to the Near_clip_plane
				 * NOW: ALWAYS take dnear=dv.
				 */
	int	dfar;		/* Distancd from the Foucs to the Far_clip_plane */
	//XXXTODO: other parameters to define the shape of the frustum */

	/* Define the displaying window */
	int	winW;
	int 	winH;

	/* For view frustum */
	float r,l; /* right/left limit value
		      * For symmetrical screen plane: r=-winW/2, l=winW/2
		      */
	float t,b; /* top/bottom limit value
		      * For symmetrical screen plane: t=winH/2, b=-winH2
		      */

/*** E3D Clippling Matrix for Type   ------ E3D_ISOMETRIC_VIEW ------   HK2022-10-09
             Reference: http://www.songho.ca/opengl/gl_projectionmatrix.html

 	      [
		2/(l-r)     0         0      (r+l))/(r-l)
		   0       2/(t-b)    0      (b+t)/(b-t)
		   0         0     2/(f-n)   (n+f)/(n-f)
		   0         0        0           1
							   ]
	      For a symmetricl screen plane, where r=-l and b=-t.
 	      [
		  -1/r       0        0             0
		   0       1/t        0             0
		   0         0      2/(f-n)    (n+f)/(n-f)
		   0         0        0             1
							    ]
aa,bb,cc,dd:   aa=-1/r; bb=1/t; cc=2/(f-n); dd=(n+f)/(n-f);

*/
	  double aa,bb,cc,dd;  /* Clippling Matrix items, for symmetrical screen plane */


	/* XXXTODO: a projection matrix under Normalized Device Coordinates:
	 *  	 The viewing frustum space is mapped to a 2x2x2 cube, with origin
	 *       at the center of the cube. Any mapped point out of the cube will be clipped then.
	 */

/*** E3D Clippling Matrix for Type    ------ E3D_PERSPECTIVE_VIEW ------

             Reference: http://www.songho.ca/opengl/gl_projectionmatrix.html

 	      [
		2n/(r-l)     0         (r+l)/(r-l)    0
		   0       2n/(t-b)    (b+t)/(b-t)    0
		   0         0         (f+n)/(f-n)    -2fn/(f-n)
		   0         0              1         0
								  ]
	      For a symmetricl screen plane, where r=-l and b=-t.
 	      [
		  n/r       0         0             0
		   0       n/t        0             0
		   0         0   (f+n)/(f-n)    -2fn/(f-n)
		   0         0        1             0
							    ]

A,B,C,D:   A=n/r; B=n/t; C=(f+n)/(f-n); D=-2fn/(f-n);

	      Note:
		1. r,l,t,b:  right, left, top, bottom limit value on XY plane, which are all signed.
		                      !!! --- CAUTION --- !!!
		   Here right/left top/bottom are at the view point from -z ---> +z as E3D View direction.
		2. f,n:      far,near limit value in Z direction, which are unsigned.
		3. with Wc=Ze
	     XXXTODO:
		4. If all r,l,t,b,f, are fixed, then we can compute above matrix and save it.
		   OR to compute result of matrix items as A,B,C,D,... see in mapPointsToNDC().

	 */

	  double A,B,C,D;  /* Clippling Matrix items, for symmetrical screen plane */
	  // f=dfar, n=dnear

};

void E3D_InitProjMatrix(E3DS_ProjMatrix &projMatrix, int type, int winW, int winH, int dnear, int dfar, int dv);
void E3D_RecomputeProjMatrix(E3DS_ProjMatrix &projMatrix); /* Recompute clipping matrix items: A,B,C,D... */



#endif

