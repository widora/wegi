/*------------------------------------------------------
	 EGI 3D Vector Class

Refrence:
1. "3D Math Primer for Graphics and Game Development"
			by Fletcher Dunn, Ian Parberry


Journal:
2021-07-28:	Create egi_vector3D.h
2021-07-29:
	1. Class E3D_RTMatrix.

Midas Zhou
-------------------------------------------------------*/
#ifndef __E3D_VECTOR_H__
#define __E3D_VECTOR_H__

#include <stdio.h>
#include <math.h>
#include <iostream>
#include "egi_debug.h"
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
		//x=0.0; y=0.0; z=0.0;
	}
	E3D_Vector(const E3D_Vector &v) : x(v.x), y(v.y), z(v.z) { }
	E3D_Vector(float ix, float iy, float iz): x(ix), y(iy), z(iz) { }

	/* Destructor */
	~E3D_Vector() { printf("E3D_Vector destructed!\n"); }

	/* Vector operator: zero */
	void zero(void) { x=0; y=0; z=0; }

	/* Vector normalize */
	void normalize(void) {
		float sumsq, osqrt;
		sumsq=x*x+y*y+z*z;
		if( sumsq > 0.0f ) {
			osqrt=1.0f/sqrt(sumsq);
			x *= osqrt;
			y *= osqrt;
			z *= osqrt;
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
		return ( x==v.x && y==v.y && z==v.z );
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
	void print() { printf("V(%f, %f, %f)\n",x,y,z); };
	void print(const char *name) { printf("%s(%f, %f, %f)\n",name, x,y,z); };
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
3D Transform Matrix (4x3)
----------------------------------------------*/
class E3D_RTMatrix {
public:
        //const int nr=4;         /* Rows */
        //const int nc=3;         /* Columns */
        float* pmat;
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
		pmat = new float[4*3];
	}

	/* Destructor */
	~E3D_RTMatrix() {
		delete [] pmat;
		 printf("E3D_RTMatrix destructed!\n");
	}

	/* Print */
	void print(const char *name) {
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
		pmat[3]=0;    pmat[4]=1.0f; pmat[5]=0;     /* m22 =1*/
		pmat[6]=0;    pmat[7]=0;    pmat[8]=1.0f;  /* m33 =1 */
		pmat[9]=0;    pmat[10]=0;   pmat[11]=0;    /* tz=1 */
	}

	/* Zero Translation */
	void zeroTranslation() {
		pmat[9]=0;    pmat[10]=0;   pmat[11]=0;    /* tx/ty/tz */
	}

	/* Set part of Translation TxTyTz in pmat[] */
	void setTranslation( const E3D_Vector &tv ){
		pmat[9]=tv.x;    pmat[10]=tv.y;   pmat[11]=tv.z;    /* tx/ty/tz */
	}

	/* Set part of Translation TxTyTz in pmat[] */
	void setTranslation( float dx, float dy, float dz ){
		pmat[9]=dx;    pmat[10]=dy;   pmat[11]=dz;    /* tx/ty/tz */
	}

	/* Set part of RotationMatrix in pmat[]
         * @axis:  Rotation axis, MUST be normalized!
	 * @angle: Rotation angle, in radian
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

};




#endif
