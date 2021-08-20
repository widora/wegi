/*------------------------------------------------------
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

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------*/
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

typedef struct
{
         float x;
         float y;
	 float z;
} E3D_POINT;


/*-----------------------------
	Class: E3D_Vector
3D Vector.
-----------------------------*/
class E3D_Vector {
public:
	float x,y,z;

	/* Constructor */
	E3D_Vector() {
		/* Necessary? */
		x=0.0; y=0.0; z=0.0;
	}
	E3D_Vector(const E3D_Vector &v) : x(v.x), y(v.y), z(v.z) { }
	E3D_Vector(float ix, float iy, float iz): x(ix), y(iy), z(iz) { }

	/* Destructor */
	~E3D_Vector() {
		//printf("E3D_Vector destructed!\n");
	}

	/* Vector operator: zero */
	void zero(void) { x=0; y=0; z=0; }

	/* Vector normalize */
	int normalize(void) {
		float sumsq, osqrt;
		sumsq=x*x+y*y+z*z;
		if( sumsq > 0.0f ) {
			osqrt=1.0f/sqrt(sumsq);
			x *= osqrt;
			y *= osqrt;
			z *= osqrt;

			return 0;
		}
		else {
			egi_dpstd("!!!WARNING!!! normalize error! sumsq==0!\n");
			return -1;
		}
	}

	/* Overload operator '=' */
	E3D_Vector & operator =(const E3D_Vector &v) {
		x=v.x;
		y=v.y;
		z=v.z;

		return *this;
	}

	/* If is infinity */
	bool isInf() const {
		/* isinf(x)   returns 1 if x is positive infinity, and -1 if x is negative infinity. */
		return ( isinf(x) || isinf(y) || isinf(z) );
	}

        /* If is zero */
        bool isZero() const {
                return ( fabs(x)<1.0e10 && fabs(y)<1.0e10 && fabs(z)<0.1e10 );
        }

	/* Overload operator '==' */
	bool operator ==(const E3D_Vector &v) const {
		//return ( x==v.x && y==v.y && z==v.z );
		return ( fabs(x-v.x)<1.0e-8 && fabs(y-v.y)<1.0e-8 && fabs(z-v.z)<1.0e-8);
	}

 	/* Overload operator '!=' */
	bool operator !=(const E3D_Vector &v) const {
		return ( x!=v.x || y!=v.y || z!=v.z );
	}

	/* Vector operator '-' with one operand */
	E3D_Vector operator -() const {
		return E3D_Vector(-x, -y, -z);
	}

	/* Vector operator '-' with two operands */
	E3D_Vector operator -(const E3D_Vector &v) const {
		return E3D_Vector(x-v.x, y-v.y, z-v.z);
	}

	/* Vector operator '+' with two operands */
	E3D_Vector operator +(const E3D_Vector &v) const {
		return E3D_Vector(x+v.x, y+v.y, z+v.z);
	}

	/* Vector operator '*' with a scalar at right! (left see below) */
	E3D_Vector operator *(float a) const {
		return E3D_Vector(x*a, y*a, z*a);
	}

	/* Vector operator '/' with a scalar at right. */
	E3D_Vector operator /(float a) const {		/* TODO divided by zero! */
		float oa=1.0f/a;
		return E3D_Vector(x*oa, y*oa, z*oa);
	}

	/* Overload operator '+=' */
	E3D_Vector & operator +=(const E3D_Vector &v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}

	/* Overload operator '-=' */
	E3D_Vector & operator -=(const E3D_Vector &v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}

	/* Overload operator '*=' */
	E3D_Vector & operator *=(float a) {
		x *= a;
		y *= a;
		z *= a;
		return *this;
	}

	/* Overload operator '/=' */
	E3D_Vector & operator /=(float a) {
		float oa=1.0f/a;		/* TODO divided by zero! */
		x *= oa;
		y *= oa;
		z *= oa;
		return *this;
	}

	/* Overload operator '*' for Dot_Product  */
	float operator *(const E3D_Vector &v) const {
		return  x*v.x + y*v.y +z*v.z;
	}

	/* Print */
	//void print() { printf("V(%f, %f, %f)\n",x,y,z); };
	void print(const char *name=NULL) { printf("%s(%f, %f, %f)\n",name, x,y,z); };
};


/*  Calculate the magnitude of a vector */
inline float E3D_vector_magnitude(const E3D_Vector &v)
{
	return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

/* Calculate Cross_Product of two vectors */
inline E3D_Vector E3D_vector_crossProduct(const E3D_Vector &va, const E3D_Vector &vb)
{
	return E3D_Vector(
		va.y*vb.z - va.z*vb.y,
		va.z*vb.x - va.x*vb.z,
		va.x*vb.y - va.y*vb.x
	);
}

/* Overload operator '*' with a scalar at left. */
inline E3D_Vector operator *(float k, const E3D_Vector &v)
{
	//printf("k at left\n");
	return E3D_Vector(k*v.x, k*v.y, k*v.z);
}

/* Calculate distance between two points(vectors) */
inline float E3D_vector_distance(const E3D_Vector &va, const E3D_Vector &vb)
{
	float dx, dy, dz;
	dx=va.x-vb.x;
	dy=va.y-vb.y;
	dz=va.z-vb.z;

	return sqrt(dx*dx+dy*dy+dz*dz);
}


/*---------------------------------------------
       		Class: E3D_RTMatrix
3D Transform Matrix (4x3)  (Rotation+Translation)
----------------------------------------------*/
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
	 */

	/* Constructor */
	E3D_RTMatrix() {
		//pmat = new float[4*3];
	}

	/* Destructor */
	~E3D_RTMatrix() {
		//delete [] pmat;
		//printf("E3D_RTMatrix destructed!\n");
	}

	/* Print */
	void print(const char *name=NULL) {
		int i,j;
		printf("   <<< %s >>>\nRotation_Matrix[3x3]: \n",name);
		for(i=0; i<3; i++) {
			for(j=0; j<3; j++)
				printf("%f  ",pmat[i*3+j]);
			printf("\n");
		}
		printf("TxTyTz:\n");
		for(i=0; i<3; i++)
			printf("%f ", pmat[3*3+i]);
		printf("\n");

	}

	/* Matrix identity: Zero Translation and Zero Rotation */
	void identity() {
		pmat[0]=1.0f; pmat[1]=0;    pmat[2]=0;     /* m11 =1 */
		pmat[3]=0;    pmat[4]=1.0f; pmat[5]=0;     /* m22 =1 */
		pmat[6]=0;    pmat[7]=0;    pmat[8]=1.0f;  /* m33 =1 */
		pmat[9]=0;    pmat[10]=0;   pmat[11]=0;    /* tz=1 */
	}

	E3D_RTMatrix & operator * (float a);

	/* Zero Translation */
	void zeroTranslation() {
		pmat[9]=0;    pmat[10]=0;   pmat[11]=0;    /* tx/ty/tz */
	}
	/* Zero Rotation */
	void zeroRotation() {
		pmat[0]=1.0f; pmat[1]=0;    pmat[2]=0;     /* m11 =1 */
		pmat[3]=0;    pmat[4]=1.0f; pmat[5]=0;     /* m22 =1 */
		pmat[6]=0;    pmat[7]=0;    pmat[8]=1.0f;  /* m33 =1 */
	}

	/* Set part of Translation TxTyTz in pmat[] */
	void setTranslation( const E3D_Vector &tv ){
		pmat[9]=tv.x;    pmat[10]=tv.y;   pmat[11]=tv.z;    /* tx/ty/tz */
	}

	/* Set part of Translation TxTyTz in pmat[] */
	void setScaleXYZ( float sx, float sy, float sz) {
		pmat[0]=sx;    pmat[4]=sy;    pmat[8]=sz;
	}

	/* Set part of Translation TxTyTz in pmat[] */
	void setTranslation( float dx, float dy, float dz ){
		pmat[9]=dx;    pmat[10]=dy;   pmat[11]=dz;    /* tx/ty/tz */
	}

	/* Set part of RotationMatrix in pmat[]
         * @axis:  Rotation axis, MUST be normalized!
	 * @angle: Rotation angle, in radian
	 * Note: Translation keeps!
	 */
	void setRotation( const E3D_Vector &axis, float angle) {
		float vsin, vcos;
		float a,ax,ay,az;

		if(fabs(axis*axis-1.0f)>0.01f) {
			egi_dpstd("Input axis is NOT normalized!\n");
			return;
		}

		vsin=sin(angle);
		vcos=cos(angle);

		a=1.0f-vcos;
		ax=a*axis.x;
		ay=a*axis.y;
		az=a*axis.z;

		pmat[0]=ax*axis.x+vcos;		//m11
		pmat[1]=ax*axis.y+axis.z*vsin;	//m12
		pmat[2]=ax*axis.z-axis.y*vsin;	//m13

		pmat[3]=ay*axis.x-axis.z*vsin;	//m21
		pmat[4]=ay*axis.y+vcos;	        //m22
		pmat[5]=ay*axis.z+axis.x*vsin;	//m23

		pmat[6]=az*axis.x+axis.y*vsin;	//m31
		pmat[7]=az*axis.y-axis.x*vsin;	//m32
		pmat[8]=az*axis.z+vcos;	        //m33

	}

	/* Set projection Matrix, move to let XY as normal to n
	 * and the projection plan contains original origin.
         * @n: Normal of projection plan
	 */
	void setupProject(const E3D_Vector &n) {
		if( fabs(n*n-1.0f)>0.01f ) {
			egi_dpstd("Input axis is NOT normalized!\n");
			return;
		}

		pmat[0]=1.0f-n.x*n.x;
		pmat[4]=1.0f-n.y*n.y;
		pmat[8]=1.0f-n.z*n.z;

		pmat[1]=pmat[3]=-n.x*n.y;
		pmat[2]=pmat[6]=-n.x*n.z;
		pmat[5]=pmat[7]=-n.y*n.z;

		pmat[9]=0.0f; pmat[10]=0.0f; pmat[11]=0.0f;
	}
};


/*--------------------  NON_CLASS_MEMBERE FUNCTIONS  ------------------*/
/*----------------------------------------------
Multiply a matrix with a float
mc=ma*mb;
-----------------------------------------------*/
E3D_RTMatrix & E3D_RTMatrix:: operator * (float a)
{
	/*** Note:
	 * Rotation Matrix:
	   pmat[0,1,2]: m11, m12, m13
	   pmat[3,4,5]: m21, m22, m23
	   pmat[6,7,8]: m31, m32, m33
	 * Translation:
	   pmat[9,10,11]: tx,ty,tz
	 */

	for(int i=0; i<12; i++)
		pmat[i] *= a;

	return *this;
}

/*----------------------------------------------
Multiply two matrix
mc=ma*mb;
-----------------------------------------------*/
E3D_RTMatrix operator* (const E3D_RTMatrix &ma, const E3D_RTMatrix &mb)
{
	/*** Note:
	 * Rotation Matrix:
	   pmat[0,1,2]: m11, m12, m13
	   pmat[3,4,5]: m21, m22, m23
	   pmat[6,7,8]: m31, m32, m33
	 * Translation:
	   pmat[9,10,11]: tx,ty,tz
	 */
	E3D_RTMatrix mc;

	/* Rotation */
	mc.pmat[0]=ma.pmat[0]*mb.pmat[0] + ma.pmat[1]*mb.pmat[3] + ma.pmat[2]*mb.pmat[6];
	mc.pmat[1]=ma.pmat[0]*mb.pmat[1] + ma.pmat[1]*mb.pmat[4] + ma.pmat[2]*mb.pmat[7];
	mc.pmat[2]=ma.pmat[0]*mb.pmat[2] + ma.pmat[1]*mb.pmat[5] + ma.pmat[2]*mb.pmat[8];

	mc.pmat[3]=ma.pmat[3]*mb.pmat[0] + ma.pmat[4]*mb.pmat[3] + ma.pmat[5]*mb.pmat[6];
	mc.pmat[4]=ma.pmat[3]*mb.pmat[1] + ma.pmat[4]*mb.pmat[4] + ma.pmat[5]*mb.pmat[7];
	mc.pmat[5]=ma.pmat[3]*mb.pmat[2] + ma.pmat[4]*mb.pmat[5] + ma.pmat[5]*mb.pmat[8];

	mc.pmat[6]=ma.pmat[6]*mb.pmat[0] + ma.pmat[7]*mb.pmat[3] + ma.pmat[8]*mb.pmat[6];
	mc.pmat[7]=ma.pmat[6]*mb.pmat[1] + ma.pmat[7]*mb.pmat[4] + ma.pmat[8]*mb.pmat[7];
	mc.pmat[8]=ma.pmat[6]*mb.pmat[2] + ma.pmat[7]*mb.pmat[5] + ma.pmat[8]*mb.pmat[8];

	/* Translation */
	mc.pmat[9] =ma.pmat[9]*mb.pmat[0] + ma.pmat[10]*mb.pmat[3] + ma.pmat[11]*mb.pmat[6] +mb.pmat[9];
	mc.pmat[10] =ma.pmat[9]*mb.pmat[1] + ma.pmat[10]*mb.pmat[4] + ma.pmat[11]*mb.pmat[7] +mb.pmat[10];
	mc.pmat[11] =ma.pmat[9]*mb.pmat[2] + ma.pmat[10]*mb.pmat[5] + ma.pmat[11]*mb.pmat[8] +mb.pmat[11];

	return mc;
}


/*----------------------------------------------
Add two matrix
mc=ma+mb;
-----------------------------------------------*/
E3D_RTMatrix operator+ (const E3D_RTMatrix &ma, const E3D_RTMatrix &mb)
{
	/*** Note:
	 * Rotation Matrix:
	   pmat[0,1,2]: m11, m12, m13
	   pmat[3,4,5]: m21, m22, m23
	   pmat[6,7,8]: m31, m32, m33
	 * Translation:
	   pmat[9,10,11]: tx,ty,tz
	 */
	E3D_RTMatrix mc;

	for(int i=0; i<12; i++)
		mc.pmat[i]=ma.pmat[i]+mb.pmat[i];

	return mc;

}

/*----------------------------------------------
Multiply Vector_va and RTMatrix_ma
vb=va*ma;
-----------------------------------------------*/
E3D_Vector operator * (const E3D_Vector &va, const E3D_RTMatrix  &ma)
{
	E3D_Vector vb;

        float xx;
        float yy;
        float zz;

        xx=va.x;
        yy=va.y;
        zz=va.z;

        vb.x = xx*ma.pmat[0]+yy*ma.pmat[3]+zz*ma.pmat[6] +ma.pmat[9];
        vb.y = xx*ma.pmat[1]+yy*ma.pmat[4]+zz*ma.pmat[7] +ma.pmat[10];
        vb.z = xx*ma.pmat[2]+yy*ma.pmat[5]+zz*ma.pmat[8] +ma.pmat[11];

	return vb;
}



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

	/* Distance from the Foucs to viewPlane/projetingPlane */
	int	dv;

	/* Define the shape of the Viewing Frustum */
	int	dnear;		/* Distancd from the Foucs to the Near_clip_plane */
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


