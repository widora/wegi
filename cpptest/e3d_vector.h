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

/*  NON_Class functions */
class E3D_RTMatrix;
class E3D_Vector;

E3D_Vector operator* (const E3D_Vector &va, const E3D_RTMatrix  &ma);

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

void  E3D_transform_vectors( E3D_Vector *vts, unsigned int vcnt, const E3D_RTMatrix &RTMatrix);


/*-----------------------------
	Class: E3D_Vector
3D Vector.
-----------------------------*/
class E3D_Vector {
public:
	float x,y,z;

	/* Constructor */
	E3D_Vector() {
		/* Necessary!!! Yes! */
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

	/* Vector assign */
	void assign(float nx, float ny, float nz) {
		x=nx;
		y=ny;
		z=nz;
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
                return ( fabs(x)<1.0e-10 && fabs(y)<1.0e-10 && fabs(z)<1.0e-10 );
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

	/* Vectorize 16bits color to RGB(0-255,0-255,0-255) to xyz(0-1.0, 0-1.0, 0-1.0) */
	void  vectorRGB(EGI_16BIT_COLOR color) {
	        x=1.0*COLOR_R8_IN16BITS(color)/255;
        	y=1.0*COLOR_G8_IN16BITS(color)/255;
        	z=1.0*COLOR_B8_IN16BITS(color)/255;
	}

	/* Covert vector xyz(0-1.0, 0-1.0, 0-1.0) to 16bits color. RGB(0-255,0-255,0-255) */
	EGI_16BIT_COLOR  color16Bits() const {
		return COLOR_RGB_TO16BITS( (int)roundf(x*255), (int)roundf(y*255), (int)roundf(z*255) );
	}

	/* Print */
	//void print() { printf("V(%f, %f, %f)\n",x,y,z); };
	void print(const char *name=NULL) { printf("Vector'%s':(%f, %f, %f)\n",name, x,y,z); };
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


/*-------------------------------------------------
       		Class: E3D_RTMatrix
3D Transform Matrix (4x3)  (Rotation+Translation)
Note:
1. For inertial Coord, just zeroTranslation().
-------------------------------------------------*/
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
	E3D_RTMatrix() {
		identity();
		//pmat = new float[4*3];
	}

	/* Destructor */
	~E3D_RTMatrix() {
		//delete [] pmat;
		//printf("E3D_RTMatrix destructed!\n");
	}

	/* Print */
	void print(const char *name=NULL) const {
		int i,j;
		printf("   <<< RTMatrix '%s' >>>\nRotation_Matrix[3x3]: \n",name);
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

        /* Overload operator '*=', TODO to test. */
        E3D_RTMatrix & operator *=(const E3D_RTMatrix & ma) {
		E3D_RTMatrix  mb=(*this)*ma;

		*this = mb;
                return *this;
        }

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

	/* Transpose: for an orthogonal matrix, its inverse is equal to its transpose! */
	void transpose() {
	        E3D_RTMatrix copyMat=*this;

        	for( int i=0; i<3; i++)
	           for( int j=0; j<3; j++)
        	        pmat[i*3+j]=copyMat.pmat[i+j*3];
	}

	/* Check wheather the matrix is identity */
	bool isIdentity() {
		for(int i=0; i<9; i++) {
			if(i%4==0) {
			    if(fabs(pmat[i]-1.0)>1.0e-5)
			    	return false;
			}
			else if(fabs(pmat[i])>1.0e-5)
				return false;
		}

		return true;
	}

	/* Check wheather the matrix is orthogonal */
	bool isOrthogonal() {
#if 1 //////////  METHOD_1 //////////
		E3D_RTMatrix  Tmat=*this; /* Transpose of this matrix */
		Tmat.transpose();

		Tmat = Tmat*(*this);
		//Tmat.print("M*M^t");

		return( Tmat.isIdentity() );
#else ////////  Method_2 ////////////
		

#endif

	}

	/* Orthogonalize+Normalize the RTMatrix.
	 * Reference: Direction Cosine Matrix IMU: Theory (Eqn 19-21)
	 *	       by Paul Bizard and  Ecole Centrale
	 */
	void orthNormalize() {
		E3D_Vector axisX, new_axisX;
		E3D_Vector axisY, new_axisY;
		E3D_Vector new_axisZ;
		float error;  /* As  dot_product of axisX(row0)*axisY(row1) of RTMatrix,  */

		axisX.x = pmat[0]; axisX.y=pmat[1]; axisX.z=pmat[2];
		axisY.x = pmat[3]; axisY.y=pmat[4]; axisY.z=pmat[5];

		/* dot_product of axisX and axisY */
		error=pmat[0]*pmat[3]+pmat[1]*pmat[4]+pmat[2]*pmat[5];

	/* TEST: -------- */
		egi_dpstd("Dot_product: axisX*axisY=%f  (should be 0.0!) \n", error);

		/* Average error to axisX and axisY */
		new_axisX = axisX-(0.5*error)*axisY;
	        new_axisY = axisY-(0.5*error)*axisX;
		new_axisZ = E3D_vector_crossProduct(new_axisX,new_axisY);

		/* Normalize, use Taylor's expansion! */
		new_axisX = 0.5*(3.0-new_axisX*new_axisX)*new_axisX;
		new_axisY = 0.5*(3.0-new_axisY*new_axisY)*new_axisY;
		new_axisZ = 0.5*(3.0-new_axisZ*new_axisZ)*new_axisZ;

		/* Upate pmat[] */
		pmat[0]=new_axisX.x; pmat[1]=new_axisX.y; pmat[2]=new_axisX.z;
		pmat[3]=new_axisY.x; pmat[4]=new_axisY.y; pmat[5]=new_axisY.z;
		pmat[6]=new_axisZ.x; pmat[7]=new_axisZ.y; pmat[8]=new_axisZ.z;

		/* Keep TxTyTz */
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


		/* Keep pmat[9-11] */
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



/*----------------------------------------------
Multiply a matrix with a float
m=m*a;
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

/*--------------------  NON_CLASS_MEMBERE FUNCTIONS  ------------------*/

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

/*----------------------------------------------------
To combine rotation transformation of ma*mb,
then assign to mc, Translation of mc KEEP unchanged!

-----------------------------------------------------*/
void E3D_getCombRotation(const E3D_RTMatrix &ma, const E3D_RTMatrix &mb, E3D_RTMatrix &mc)
{
	/*** Note:
	 * Rotation Matrix:
	   pmat[0,1,2]: m11, m12, m13
	   pmat[3,4,5]: m21, m22, m23
	   pmat[6,7,8]: m31, m32, m33
	 * Translation:
	   pmat[9,10,11]: tx,ty,tz
	 */

	E3D_RTMatrix nmc=ma*mb;

	for(unsigned int k=0; k<9; k++)
		mc.pmat[k]=nmc.pmat[k];

}


/*----------------------------------------------
Add two matrix
mc=ma+mb;
----------------------------------------------*/
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


#if 0 ////////////////////////////////////////////////////////
/*----------------------------------------------------
To combine intrinsic rotation with original RTMatrix.
Translation of RTmat keeps UNCHANGED!

Note:
1.  All vertex coordinates MUST be under LOCAL Coord!!!

@aix: 		Rotation aix Token:
		'X'('x') 'Y'('y') 'Z'('z')
@ang: 		Rotation angle.
@RTMatrix:	Original RTMatrix, to be combined with
		other RTMatrix.
------------------------------------------------------*/
void E3D_combIntriRotation(char axisTok, float ang, E3D_RTMatrix & RTmat)
{
		E3D_RTMatrix imat;
		imat.identity();
		imat.zeroTranslation();

		float npmat[9];

		bool case_OK=true;
		  /* Rotation Matrix:
		   pmat[0,1,2]: m11, m12, m13
		   pmat[3,4,5]: m21, m22, m23
		   pmat[6,7,8]: m31, m32, m33
		  */
		switch(axisTok) {
			case 'x': case 'X':
			   /*  Rx: [1 0 0;  0 cos(a) sin(a); 0 -sin(a) cos(a)] */
			   imat.pmat[0]=1.0;  imat.pmat[1]=0.0;       imat.pmat[2]=0.0;
			   imat.pmat[3]=0.0;  imat.pmat[4]=cos(ang);  imat.pmat[5]=sin(ang);
			   imat.pmat[6]=0.0;  imat.pmat[7]=-sin(ang); imat.pmat[8]=cos(ang);

			   break;
			case 'y': case 'Y':
			   /*  Ry: [cos(a) 0 -sin(a); 0 1 0; sin(a) 0 cos(a)] */
			   imat.pmat[0]=cos(ang); imat.pmat[1]=0.0; imat.pmat[2]=-sin(ang);
			   imat.pmat[3]=0.0;      imat.pmat[4]=1.0; imat.pmat[5]=0.0;
			   imat.pmat[6]=sin(ang); imat.pmat[7]=0.0; imat.pmat[8]=cos(ang);

			   break;
			case 'z': case 'Z':
			   /*  Rz: [cos(ang) sin(ang) 0; -sin(ang) cos(ang) 0; 0 0 1] */
			   imat.pmat[0]=cos(ang);  imat.pmat[1]=sin(ang); imat.pmat[2]=0.0;
			   imat.pmat[3]=-sin(ang); imat.pmat[4]=cos(ang); imat.pmat[5]=0.0;
			   imat.pmat[6]=0.0;       imat.pmat[7]=0.0;      imat.pmat[8]=1.0;

				break;
			default:
			   case_OK=false;
			   egi_dpstd("Unrecognizable aix token '%c', NOT in 'xXyYzZ'!\n", axisTok);
			   break;
		}

	if( case_OK ) {
#if 1		/* Only combine Rotation:  RMatrix = RMatrix*imat; */
		npmat[0]=RTmat.pmat[0]*imat.pmat[0] + RTmat.pmat[1]*imat.pmat[3] + RTmat.pmat[2]*imat.pmat[6];
		npmat[1]=RTmat.pmat[0]*imat.pmat[1] + RTmat.pmat[1]*imat.pmat[4] + RTmat.pmat[2]*imat.pmat[7];
		npmat[2]=RTmat.pmat[0]*imat.pmat[2] + RTmat.pmat[1]*imat.pmat[5] + RTmat.pmat[2]*imat.pmat[8];

		npmat[3]=RTmat.pmat[3]*imat.pmat[0] + RTmat.pmat[4]*imat.pmat[3] + RTmat.pmat[5]*imat.pmat[6];
		npmat[4]=RTmat.pmat[3]*imat.pmat[1] + RTmat.pmat[4]*imat.pmat[4] + RTmat.pmat[5]*imat.pmat[7];
		npmat[5]=RTmat.pmat[3]*imat.pmat[2] + RTmat.pmat[4]*imat.pmat[5] + RTmat.pmat[5]*imat.pmat[8];

		npmat[6]=RTmat.pmat[6]*imat.pmat[0] + RTmat.pmat[7]*imat.pmat[3] + RTmat.pmat[8]*imat.pmat[6];
		npmat[7]=RTmat.pmat[6]*imat.pmat[1] + RTmat.pmat[7]*imat.pmat[4] + RTmat.pmat[8]*imat.pmat[7];
		npmat[8]=RTmat.pmat[6]*imat.pmat[2] + RTmat.pmat[7]*imat.pmat[5] + RTmat.pmat[8]*imat.pmat[8];
#else		/* Only combine Rotation:  RMatrix = imat*RMatrix */
		npmat[0]=imat.pmat[0]*RTmat.pmat[0] + imat.pmat[1]*RTmat.pmat[3] + imat.pmat[2]*RTmat.pmat[6];
		npmat[1]=imat.pmat[0]*RTmat.pmat[1] + imat.pmat[1]*RTmat.pmat[4] + imat.pmat[2]*RTmat.pmat[7];
		npmat[2]=imat.pmat[0]*RTmat.pmat[2] + imat.pmat[1]*RTmat.pmat[5] + imat.pmat[2]*RTmat.pmat[8];

		npmat[3]=imat.pmat[3]*RTmat.pmat[0] + imat.pmat[4]*RTmat.pmat[3] + imat.pmat[5]*RTmat.pmat[6];
		npmat[4]=imat.pmat[3]*RTmat.pmat[1] + imat.pmat[4]*RTmat.pmat[4] + imat.pmat[5]*RTmat.pmat[7];
		npmat[5]=imat.pmat[3]*RTmat.pmat[2] + imat.pmat[4]*RTmat.pmat[5] + imat.pmat[5]*RTmat.pmat[8];

		npmat[6]=imat.pmat[6]*RTmat.pmat[0] + imat.pmat[7]*RTmat.pmat[3] + imat.pmat[8]*RTmat.pmat[6];
		npmat[7]=imat.pmat[6]*RTmat.pmat[1] + imat.pmat[7]*RTmat.pmat[4] + imat.pmat[8]*RTmat.pmat[7];
		npmat[8]=imat.pmat[6]*RTmat.pmat[2] + imat.pmat[7]*RTmat.pmat[5] + imat.pmat[8]*RTmat.pmat[8];
#endif
		for(unsigned int k=0; k<9; k++)
			RTmat.pmat[k]=npmat[k];

		/* Ingore Translation !!! */

	}
}

/*-------------------------------------------------------
To combine intrinsic rotation with original RTmat, then
update RTMat to make objects under Global Coord.

Translation of RTmat keeps UNCHANGED!

TODO: Test
	--- Translation is Ignored! ----
Note:
1.  All vertex coordinates MUST be under LOCAL Coord!!!


@aix: 		Rotation aix Token:
		'X'('x') 'Y'('y') 'Z'('z')
@ang: 		Rotation angle.
@CoordMat:	RTMatrix for transforimg global Coord to
		current local Coord.
@RTMatrix:	Original RTMatrix, to be combined with
		other RTmatrix.
---------------------------------------------------------*/
void E3D_combIntriRotation(char axisTok, float ang,  E3D_RTMatrix CoordMat, E3D_RTMatrix & RTmat)
{
	/* imat:  translation is ignored! */
	E3D_RTMatrix imat;
	imat.identity();
	imat.zeroTranslation();

	/* cmat:  translation is ignored! */
	E3D_RTMatrix cmat=RTmat;
	cmat.zeroTranslation();

	/* CoordMat: translation is ignored! */
	CoordMat.zeroTranslation();

	/* Assume case_OK, OR to reset it at case_default */
	bool case_OK=true;

		  /* Rotation Matrix:
		   pmat[0,1,2]: m11, m12, m13
		   pmat[3,4,5]: m21, m22, m23
		   pmat[6,7,8]: m31, m32, m33
		  */
		switch(axisTok) {
			case 'x': case 'X':
			   /*  Rx: [1 0 0;  0 cos(a) sin(a); 0 -sin(a) cos(a)] */
			   imat.pmat[0]=1.0;  imat.pmat[1]=0.0;       imat.pmat[2]=0.0;
			   imat.pmat[3]=0.0;  imat.pmat[4]=cos(ang);  imat.pmat[5]=sin(ang);
			   imat.pmat[6]=0.0;  imat.pmat[7]=-sin(ang); imat.pmat[8]=cos(ang);

			   break;
			case 'y': case 'Y':
			   /*  Ry: [cos(a) 0 -sin(a); 0 1 0; sin(a) 0 cos(a)] */
			   imat.pmat[0]=cos(ang); imat.pmat[1]=0.0; imat.pmat[2]=-sin(ang);
			   imat.pmat[3]=0.0;      imat.pmat[4]=1.0; imat.pmat[5]=0.0;
			   imat.pmat[6]=sin(ang); imat.pmat[7]=0.0; imat.pmat[8]=cos(ang);

			   break;
			case 'z': case 'Z':
			   /*  Rz: [cos(ang) sin(ang) 0; -sin(ang) cos(ang) 0; 0 0 1] */
			   imat.pmat[0]=cos(ang);  imat.pmat[1]=sin(ang); imat.pmat[2]=0.0;
			   imat.pmat[3]=-sin(ang); imat.pmat[4]=cos(ang); imat.pmat[5]=0.0;
			   imat.pmat[6]=0.0;       imat.pmat[7]=0.0;      imat.pmat[8]=1.0;

				break;
			default:
			   case_OK=false;
			   egi_dpstd("Unrecognizable aix token '%c', NOT in 'xXyYzZ'!\n", axisTok);
			   break;
		}

	if( case_OK ) {
#if 1		/* Only combine Rotation:  RTMatrix = cmat(RTMatrix)*imat* */
		cmat = cmat*imat*CoordMat;
#else		/* Only combine Rotation:  RMatrix = imat*cmat(RMatrix)* */
		cmat = CoordMat*imat*cmat;
#endif
	}

		/* Ignore translation */
                for(unsigned int k=0; k<9; k++)
                        RTmat.pmat[k]=cmat.pmat[k];

}
#endif /////////////////////////////////////////////////

/*------------------------------------------------------------------------------------
To combine object intrinsic rotation with RTMatrix, with all vertices are under Global
COORD. Suppose initial Local_Coord aligns with the Global_Coord (Origin not considered),
and current Local_Coord is transformed from by CoordMat.
Here 'Global' of 'combGlobalIntriRotation()' means input CoordMat and RTmat is ALL under
Global COORD! ( axisToks and ang[] is under LOCAL COORD!)

Translation of input RTmat keeps UNCHANGED!

Note:
1.  For vertices which will apply the result RTmat:
    1.1 All vertex coordinates MUST be under GLOBAL Coord!
2.  Intrinsic rotation are taken under LOCAL COORD, which can be restored by TcoordMat.
3.  Even if input axisToks==NULL, CoordMat will apply to RTmat also.
4.  Motice that axisToks are applied/mutiplied with Pxyz in reverse order!

	--- Translation is Ignored! ----

@axisToks: 	Intrinsic rotation sequence Tokens(rotating axis), including:
		'X'('x') 'Y'('y') 'Z'('z')
		Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...

		1. "ZYX" intrinsic rotation sequence: RotZ -> RotY -> RotX, matrix multiplation: Mx*My*Mz
		    Notice that 'reversed' matrix multiplation applys.
		2. The function reverse axisToks[] to RxyzToks[].

@ang[3]: 	Rotation angles cooresponding to axisToks.
@CoordMat:	Current object RTMatrix, OR object COORD RTMatrix under Global COORD.
		Suppose initial Object COORD aligns with the Global coord.
		This CoordMat transforms the object( and its COORD) to current position.

		XXX However, all object vertex coordinates are under GLOBAL coord ALWAYS!
		In case of BOXMAN object transfromation, it will then be transformed
		(rotated) by the combined RTmat under local ORIGIN, but NOT local COORD!!!

@RTmat:		Original RTMatrix, to be combined with. under GLOBAL COORD.
		Usually same as CoordMat, but not necessary.
		Target: finally we can use RTmat to transform TriGroup under its Local Coord.
------------------------------------------------------------------------------------*/
void E3D_combGlobalIntriRotation(const char *axisToks, float ang[3],  E3D_RTMatrix CoordMat, E3D_RTMatrix & RTmat)
{
	/* Get reversed sequence of rotation */
	int toks=strlen(axisToks);

	/* imat:  translation is ignored! */
	E3D_RTMatrix imat;
	//imat.identity(); /* Constructor to identity */
	imat.zeroTranslation();

	/* cmat as combined:  translation is ignored! */
	E3D_RTMatrix cmat=RTmat;
	cmat.zeroTranslation();

	/* CoordMat: translation is ignored! */
	CoordMat.zeroTranslation();

	/* TcoordMat: Transposed of CoordMat, For an orthogonal matrix, its inverse is equal to its transpose! */
	E3D_RTMatrix TcoordMat=CoordMat;
	TcoordMat.transpose();

	bool case_OK;
	unsigned int np=0;
	int rnp;  /* np as backward */

//	while( RxyzToks[np] ) {
	while( (rnp=toks-1 -np)>=0 && axisToks[toks-1 -np] ) {

		/* Revers index of axisToks[] */
		rnp=toks-1 -np;

		/* 1. Max. 3 tokens */
		if(np>3) break;

		/* 2. Assmue case_Ok, OR to reset it at case_default */
		case_OK=true;

		/* 3. Compute imat as per rotation axis and angle */
		switch( axisToks[rnp] ) {
			case 'x': case 'X':
			   /*  Rx: [1 0 0;  0 cos(a) sin(a); 0 -sin(a) cos(a)] */
			   imat.pmat[0]=1.0;  imat.pmat[1]=0.0;            imat.pmat[2]=0.0;
			   imat.pmat[3]=0.0;  imat.pmat[4]=cos(ang[rnp]);  imat.pmat[5]=sin(ang[rnp]);
			   imat.pmat[6]=0.0;  imat.pmat[7]=-sin(ang[rnp]); imat.pmat[8]=cos(ang[rnp]);

			   break;
			case 'y': case 'Y':
			   /*  Ry: [cos(a) 0 -sin(a); 0 1 0; sin(a) 0 cos(a)] */
			   imat.pmat[0]=cos(ang[rnp]); imat.pmat[1]=0.0; imat.pmat[2]=-sin(ang[rnp]);
			   imat.pmat[3]=0.0;           imat.pmat[4]=1.0; imat.pmat[5]=0.0;
			   imat.pmat[6]=sin(ang[rnp]); imat.pmat[7]=0.0; imat.pmat[8]=cos(ang[rnp]);

			   break;
			case 'z': case 'Z':
			   /*  Rz: [cos(ang) sin(ang) 0; -sin(ang) cos(ang) 0; 0 0 1] */
			   imat.pmat[0]=cos(ang[rnp]);  imat.pmat[1]=sin(ang[rnp]); imat.pmat[2]=0.0;
			   imat.pmat[3]=-sin(ang[rnp]); imat.pmat[4]=cos(ang[rnp]); imat.pmat[5]=0.0;
			   imat.pmat[6]=0.0;            imat.pmat[7]=0.0;      imat.pmat[8]=1.0;

				break;
			default:
			   case_OK=false;
			   egi_dpstd("Unrecognizable aix token '%c', NOT in 'xXyYzZ'!\n", axisToks[rnp]);
			   break;
		}

		/* 4. Multiply imats  */
		if( case_OK ) {
			/* Only combine Rotation */
			/* Note:
			 * 4.1. Here cmat is deemed as RTMatrix under LOCAL_COORD! ( See 5. with TcoordMat*cmat.. )
			 *    as we extract cmat*imat from TcoordMat*cmat*imat*CoordMat
			 * 4.2. cmat here is combined with XYZ rotmatrix in sequence.
			 * 4.3. To optimize at last, so to apply TcoordMat and CoordMat  at last, see 6.
			 */
			cmat = cmat*imat; /* Notice that RxyzToks already reversed. */

			// cmat = imat*cmat; // imat at first! Result of axisTok 'XYZ' to be: P*Mz*My*Mx*RTmat

		}

		/* np increment */
		np++;
	}

	/* HERE NOW: Maybe axisToks==NULL! */

	/* 5. Finally apply TcoordMat and CoordMat:
	 *  Note: Since cmat is RTMatrix under LOCAL_COORD! while Pxyz is under GLOBAL_COORD, so
	 *  first transform with Pxyz*TcoordMat(invsere of CoordMat) to make Pxyz under LOCAL_COORD,
         *  then apply cmat. After that, transform back to under GLOBAL COORD again by multiply
	 *  CoordMat.
	 */
	cmat = TcoordMat*cmat*CoordMat;

	/* RTmat = (imat*imat...)*RTmat */

	/* 6. Update RTmat,  ignore translation */
        for(unsigned int k=0; k<9; k++)
        	RTmat.pmat[k]=cmat.pmat[k];

}

/*----------------------------------------------------------------------------------
Parse axisToks and left-mutilply imats to RTmat.

@axisToks: 	Intrinsic rotation sequence Tokens(rotating axis), including:
		'X'('x') 'Y'('y') 'Z'('z')
		Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...

		1. "ZYX" intrinsic rotation sequence: RotZ -> RotY -> RotX, matrix multiplation: Mx*My*Mz
		    Notice that 'reversed' matrix multiplation applys.
		2. The function reverse axisToks[] to RxyzToks[].

@ang[3]: 	Rotation angles cooresponding to axisToks.
@RTmat:		Original RTMatrix. to be combined with.
-----------------------------------------------------------------------------------*/
void E3D_combIntriRotation(const char *axisToks, float ang[3],  E3D_RTMatrix & RTmat)
{
	if(axisToks==NULL)
		return;

	/* Get reversed sequence of rotation */
	int toks=strlen(axisToks);

	/* imat:  translation is ignored! */
	E3D_RTMatrix imat;

	/* cmat as combined:  translation is ignored! */
//	E3D_RTMatrix cmat;

	bool case_OK;
	unsigned int np=0;
	int rnp;  /* np as backward */

	while( (rnp=toks-1 -np)>=0 && axisToks[toks-1 -np] ) {

		/* Revers index of axisToks[] */
		rnp=toks-1 -np;

		/* 1. Max. 3 tokens */
		if(np>3) break;

		/* 2. Assmue case_Ok, OR to reset it at case_default */
		case_OK=true;

		/* 3. Compute imat as per rotation axis and angle */
		switch( axisToks[rnp] ) {
			case 'x': case 'X':
			   /*  Rx: [1 0 0;  0 cos(a) sin(a); 0 -sin(a) cos(a)] */
			   imat.pmat[0]=1.0;  imat.pmat[1]=0.0;            imat.pmat[2]=0.0;
			   imat.pmat[3]=0.0;  imat.pmat[4]=cos(ang[rnp]);  imat.pmat[5]=sin(ang[rnp]);
			   imat.pmat[6]=0.0;  imat.pmat[7]=-sin(ang[rnp]); imat.pmat[8]=cos(ang[rnp]);

			   break;
			case 'y': case 'Y':
			   /*  Ry: [cos(a) 0 -sin(a); 0 1 0; sin(a) 0 cos(a)] */
			   imat.pmat[0]=cos(ang[rnp]); imat.pmat[1]=0.0; imat.pmat[2]=-sin(ang[rnp]);
			   imat.pmat[3]=0.0;           imat.pmat[4]=1.0; imat.pmat[5]=0.0;
			   imat.pmat[6]=sin(ang[rnp]); imat.pmat[7]=0.0; imat.pmat[8]=cos(ang[rnp]);

			   break;
			case 'z': case 'Z':
			   /*  Rz: [cos(ang) sin(ang) 0; -sin(ang) cos(ang) 0; 0 0 1] */
			   imat.pmat[0]=cos(ang[rnp]);  imat.pmat[1]=sin(ang[rnp]); imat.pmat[2]=0.0;
			   imat.pmat[3]=-sin(ang[rnp]); imat.pmat[4]=cos(ang[rnp]); imat.pmat[5]=0.0;
			   imat.pmat[6]=0.0;            imat.pmat[7]=0.0;      imat.pmat[8]=1.0;

				break;
			default:
			   case_OK=false;
			   egi_dpstd("Unrecognizable aix token '%c', NOT in 'xXyYzZ'!\n", axisToks[rnp]);
			   break;
		}

		/* 4. Multiply imats  */
		if( case_OK ) {
			/* Only combine Rotation */
			//cmat = cmat*imat; /* Notice that RxyzToks already reversed. */
			RTmat = imat*RTmat;
		}

		/* np increment */
		np++;
	}
}


/*----------------------------------------------------------------------------------
Parse axisToks and right-mutilply imats to RTmat.

@axisToks: 	Extrinsic rotation sequence Tokens(rotating axis), including:
		'X'('x') 'Y'('y') 'Z'('z')
		Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...
		1. "ZYX" extrinsic rotation sequence: RotZ -> RotY -> RotX, matrix multiplation: Mz*My*Mx

@ang[3]: 	Rotation angles cooresponding to axisToks.
@RTmat:		Original RTMatrix. to be combined with.
-----------------------------------------------------------------------------------*/
void E3D_combExtriRotation(const char *axisToks, float ang[3],  E3D_RTMatrix & RTmat)
{
	if(axisToks==NULL)
		return;

	/* Get reversed sequence of rotation */
	int toks=strlen(axisToks);

	/* imat:  translation is ignored! */
	E3D_RTMatrix imat;

	/* cmat as combined:  translation is ignored! */
//	E3D_RTMatrix cmat;

	bool case_OK;
	unsigned int np=0;
//	int rnp;  /* np as backward */

	while( axisToks[np] ) {

		/* 1. Max. 3 tokens */
		if(np>3) break;

		/* 2. Assmue case_Ok, OR to reset it at case_default */
		case_OK=true;

		/* 3. Compute imat as per rotation axis and angle */
		switch( axisToks[np] ) {
			case 'x': case 'X':
			   /*  Rx: [1 0 0;  0 cos(a) sin(a); 0 -sin(a) cos(a)] */
			   imat.pmat[0]=1.0;  imat.pmat[1]=0.0;           imat.pmat[2]=0.0;
			   imat.pmat[3]=0.0;  imat.pmat[4]=cos(ang[np]);  imat.pmat[5]=sin(ang[np]);
			   imat.pmat[6]=0.0;  imat.pmat[7]=-sin(ang[np]); imat.pmat[8]=cos(ang[np]);

			   break;
			case 'y': case 'Y':
			   /*  Ry: [cos(a) 0 -sin(a); 0 1 0; sin(a) 0 cos(a)] */
			   imat.pmat[0]=cos(ang[np]); imat.pmat[1]=0.0; imat.pmat[2]=-sin(ang[np]);
			   imat.pmat[3]=0.0;          imat.pmat[4]=1.0; imat.pmat[5]=0.0;
			   imat.pmat[6]=sin(ang[np]); imat.pmat[7]=0.0; imat.pmat[8]=cos(ang[np]);

			   break;
			case 'z': case 'Z':
			   /*  Rz: [cos(ang) sin(ang) 0; -sin(ang) cos(ang) 0; 0 0 1] */
			   imat.pmat[0]=cos(ang[np]);  imat.pmat[1]=sin(ang[np]); imat.pmat[2]=0.0;
			   imat.pmat[3]=-sin(ang[np]); imat.pmat[4]=cos(ang[np]); imat.pmat[5]=0.0;
			   imat.pmat[6]=0.0;           imat.pmat[7]=0.0;          imat.pmat[8]=1.0;

				break;
			default:
			   case_OK=false;
			   egi_dpstd("Unrecognizable aix token '%c', NOT in 'xXyYzZ'!\n", axisToks[np]);
			   break;
		}

		/* 4. Multiply imats  */
		if( case_OK ) {
			/* Only combine Rotation */
			RTmat = RTmat*imat; /* Notice that RxyzToks already reversed. */
		}

		/* np increment */
		np++;
	}
}


/*--------------------------------------------------------------
To combine intrinsic translation by left-multiply imat to RTmat.

@tx,ty,tz:		Intrinsic translation XYZ values.
	OR
@Vxyz:			Intrinsic translation vector.

@RTmat:		RTMatrix to combine with.
--------------------------------------------------------------*/
void E3D_combIntriTranslation(E3D_Vector Vxyz,  E3D_RTMatrix & RTmat)
{
	E3D_RTMatrix imat;
	imat.pmat[9] = Vxyz.x;
	imat.pmat[10] = Vxyz.y;
	imat.pmat[11] = Vxyz.z;

	/* So easy!!! Pxyz*imat*RTmat:
	 * 1. Pxyz*imat: translate Vxyz under current COORD(RTmat).
	 * 2. *RTmat: transform to under Global COORD.
	 */
	RTmat = imat*RTmat;
}
void E3D_combIntriTranslation(float tx, float ty, float tz,  E3D_RTMatrix & RTmat)
{
	E3D_RTMatrix imat;
	imat.pmat[9] = tx;
	imat.pmat[10] = ty;
	imat.pmat[11] = tz;

	RTmat = imat*RTmat;
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


/*--------------------------------------------------
Transform a list of vectors.

Also refer to: E3D_Vector operator * (const E3D_Vector &va, const E3D_RTMatrix  &ma)

@vts[]:	List of vectors.
@vcnt:  Vector counter.
@RTMatrix  Transform matrix.
--------------------------------------------------*/
void  E3D_transform_vectors( E3D_Vector *vts, unsigned int vcnt, const E3D_RTMatrix  &RTMatrix)
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
        for( unsigned int i=0; i<vcnt; i++) {
                xx=vts[i].x;
                yy=vts[i].y;
                zz=vts[i].z;

                vts[i].x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6] +RTMatrix.pmat[9];
                vts[i].y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7] +RTMatrix.pmat[10];
                vts[i].z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8] +RTMatrix.pmat[11];
        }
}


#endif


