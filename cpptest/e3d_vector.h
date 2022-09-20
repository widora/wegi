/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.



EGI 3D Vector Class
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
	1. Add struct E3D_ProjectFrustum, as E3D_ProjMatrix.

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

Midas Zhou
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
3D Transform Matrix (4x3)  (Rotation+Translation)
Note:
1. For inertial Coord, just zeroTranslation().
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
	 */

	/* Constructor */
	E3D_RTMatrix();

	/* Destructor */
	~E3D_RTMatrix();

	/* Print */
	void print(const char *name) const;

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
	void setRotation( const E3D_Vector &axis, float angle);

	/* Set this matrix as a projection Matrix */
	void setupProject(const E3D_Vector &n);
};


/*--------------------------------------------------
DONT fully understand projection matrix under NDC,
so use this struct first!  :)))))))

Projection frustum ---> Projectoin matrix

TODO:
1. other parameters to define the shape of the frustum.
2. Replace struct E3D_ProjMatrix with a class Matrix
   under Normalized Device Coordinates(NDC).
   Ref. OpenGL Projection Matrix.


-------------------------------------------------*/
enum {
	E3D_ISOMETRIC_VIEW	=0,
	E3D_PERSPECTIVE_VIEW    =1,
};
typedef struct E3D_ProjectFrustum  E3D_ProjMatrix;
struct E3D_ProjectFrustum {
	/* Projectoin type, for quick response. */
	int type;		/*  0:	Isometric projectoin(default).
				 *  1:  Perspective projectoin
				 *  Others: ...
				 */

	/* Distance from the Foucs(view origin) to projetingPlane */
	int	dv;

	/* Define the shape of the Viewing Frustum */
	int	dnear;		/* Distancd from the Foucs to the Near_clip_plane
				 * NOW: ALWAYS take dnear=dv.
				 */
	int	dfar;		/* Distancd from the Foucs to the Far_clip_plane */
	//TODO: other parameters to define the shape of the frustum */

	/* Define the displaying window */
	int	winW;
	int 	winH;

	/* TODO: a projection matrix under Normalized Device Coordinates:
	 *  	 The viewing frustum space is mapped to a 2x2x2 cube, with origin
	 *       at the center of the cube. Any mapped point out of the cube will be clipped then.
	 */
};




#endif

