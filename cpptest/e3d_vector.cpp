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
	1. Create e3d_vector.cpp from e3d_vector.h
2022-08-22:
	1. Add E3D_computeBarycentricCoord()
2022-08-25:
	1. Add E3D_vector_reflect()
2022-08-28:
	1. Add E3D_Vector::module()
	2. Add E3D_vector_angleAB()
2022-09-11:
	1. Add E3D_Vector::addTranslation( float dx, float dy, float dz )
	2. Add E3D_InitProjMatrix();
2022-10-02:
	1. Add E3D_RecomputeProjMatrix()
2022-10-09:
	1. E3D_InitProjMatrix(), E3D_RecomputeProjMatrix(): Consider projMatrix.aa,bb,cc,dd. for ISOMETRIC_VIEW.
2022-10-17:
	1. E3D_RTMatrix::inverse()
2022-10-25:
	1. E3D_RTMatrix::setRotation( const E3D_Vector &vfrom, const E3D_Vector &vto )
	2. E3D_RTMatrix::setScaleRotation( const E3D_Vector &vfrom, const E3D_Vector &vto )
2022-10-26:
	1. E3D_RTMatrix::setTransformMatrix( )
2022-10-27:
	1. E3D_RTMatrix::combExtriRotation()
	2. E3D_RTMatrix::combIntriRotation()
2022-11-02:
	1. Add Class E3D_Quaternion
	2. Add E3D_Quaternion functions:
	   identity(), print(), getRotationAngle(), getRotationAxis(), setRotation(), toMatrix(), fromMatrix(), slerp()
	3. Add E3D_RTMatrix::setRotation(char chaxis, float angle)
2022-11-03:
	1. Add Class E3D_Quatrix and functions:
	   identity(), print(), getRotationAngle(), getRotationAxis(), setTranslation(), setRotation(), toMatrix(), fromMatrix(), interp()
2022-11-12:
	1. Add E3D_Quaternion::setIntriRotation(axisToks, angs) and E3D_Quatrix::setIntriRotation(axisToks, angs)
	2. Add E3D_Quaternion::setExtriRotation(axisToks, angs) and E3D_Quatrix::setExtriRotation(axisToks, angs)
2022-11-21:
	1. Add E3D_Quaternion::operator *, operator *=.
	2. Add E3D_Quaternion::inverse()
	3. E3D_Quaternion::setIntriRotation(): All quaternion operations.
2022-11-21:
	1. E3D_Quaternion::normalize()
2022-12-23:
	1. Add Class E4D_Vector,  ::E4D_Vector(), ::assign() ::zero().
2023-02-20:
	1. Add print_ProjMatrix()
2023-02-23:
	1. E3D_Quatrix add member for scale: sx,sy,sz, and modify E3D_Quatrix::E3D_Quatrix() and E3D_Quatrix::print()
2023-03-11:
	1. E3D_RTMatrix::operator +=(ma), operator *(float a)
	2. Non-class: E3D_RTMatrix operator* (float a, const E3D_RTMatrix & ma)
2023-03-21:
	1. E3D_Quatrix::toMatrix(): Integrate sx,sy,sz.
	2, E3D_Quatrix::interp(): Integrate sx,sy,sz.
2023-04-03:
	1. Add E3D_Quatrix::inverse()
2023-04-04:
	1 E3D_Quatrix::fromMatrix(): TEST sx,sy,sz extraction.
2023-04-21:
	1. E3D_Quatrix::toMatrix(E3D_RTMatrix &rtmat, E3D_RTMatrix &smat).

TODO:
1. acos(x)/asin(x) will return NaN if x is little bit out of [-1.0 1.0] ???

Midas Zhou
知之者不如好之者好之者不如乐之者
midaszhou@yahoo.com(Not in use since 2022_03_01)
-------------------------------------------------------*/
#include <stdio.h>
#include <math.h>
#include <iostream>
#include "egi_debug.h"
#include "egi_fbdev.h"
#include "egi_fbgeom.h"
#include "egi_math.h"
#include "egi_matrix.h"
#include "e3d_vector.h"

//using namespace std;

/* Global vector/matrix HK2023-03-05 */
const E3D_Vector    _zeroVector;  //Constructor default (0.0f, 0.0f, 0.0f);
const E3D_RTMatrix  _identityMat; //Construct default
const E3D_Quatrix   _identityQt;  //Construct default

		/*---------------------------------
	   	   Class E3D_Vector :: Functions
		---------------------------------*/

/*-------------------------
   Constructor functions
-------------------------*/
E4D_Vector::E4D_Vector() {
           /* Necessary!!! Yes! */
           x=0.0; y=0.0; z=0.0; w=0.0;
}
E4D_Vector::E4D_Vector(float ix, float iy, float iz, float iw): x(ix), y(iy), z(iz), w(iw) { }

/*------------------------
   Destructor functions
-------------------------*/
E4D_Vector:: ~E4D_Vector() {
           //printf("E3D_Vector destructed!\n");
}

/*-------------------------
   4D Vector operator: zero
--------------------------*/
void E4D_Vector::zero(void)
{ x=0.0f; y=0.0f; z=0.0f; w=0.0f; }

/*-----------------
   4D Vector assign
-----------------*/
void E4D_Vector::assign(float nx, float ny, float nz, float nw)
{
   	x=nx;
	y=ny;
        z=nz;
	w=nw;
}

		/*---------------------------------
	   	   Class E3D_Vector :: Functions
		---------------------------------*/

/*-------------------------
   Constructor functions
-------------------------*/
E3D_Vector::E3D_Vector() {
           /* Necessary!!! Yes! */
           x=0.0; y=0.0; z=0.0;
}
E3D_Vector::E3D_Vector(const E3D_Vector &v) : x(v.x), y(v.y), z(v.z) { }
E3D_Vector::E3D_Vector(float ix, float iy, float iz): x(ix), y(iy), z(iz) { }

/*-------------------------
   Destructor functions
-------------------------*/
E3D_Vector:: ~E3D_Vector() {
           //printf("E3D_Vector destructed!\n");
}

/*-------------------------
   Vector operator: zero
--------------------------*/
void E3D_Vector::zero(void)
{ x=0.0f; y=0.0f; z=0.0f; }

/*---------------------
    Vector normalize

Return:
0	Ok
<0	Fails

---------------------*/
int E3D_Vector::normalize(void)
{
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

/*-----------------
   Vector assign
-----------------*/
void E3D_Vector::assign(float nx, float ny, float nz)
{
   	x=nx;
	y=ny;
        z=nz;
}

/*-----------------
   Vector module
-----------------*/
float E3D_Vector::module(void) const
{
	return sqrt(x*x + y*y + z*z);
}

/*---------------------
   Overload operator
---------------------*/
E3D_Vector & E3D_Vector:: operator =(const E3D_Vector &v)
{
	x=v.x;
        y=v.y;
        z=v.z;
        return *this;
}

/*------------------
   If is infinity
------------------*/
bool E3D_Vector:: isInf() const {
	/* isinf(x)   returns 1 if x is positive infinity, and -1 if x is negative infinity. */
        return ( isinf(x) || isinf(y) || isinf(z) );
}

/*--------------
   If is zero
--------------*/
bool E3D_Vector::isZero() const
{
	return ( fabs(x)<1.0e-10 && fabs(y)<1.0e-10 && fabs(z)<1.0e-10 );
}

/*--------------------------
   Overload operator '=='
--------------------------*/
bool E3D_Vector::operator ==(const E3D_Vector &v) const
{
	//return ( x==v.x && y==v.y && z==v.z );
        return ( fabs(x-v.x)<1.0e-8 && fabs(y-v.y)<1.0e-8 && fabs(z-v.z)<1.0e-8);
}

/*--------------------------
   Overload operator '!='
--------------------------*/
bool E3D_Vector::operator !=(const E3D_Vector &v) const
{
	return ( x!=v.x || y!=v.y || z!=v.z );
}

/*----------------------------------------
   Vector operator '-' with one operand
----------------------------------------*/
E3D_Vector E3D_Vector::operator -() const
{
	return E3D_Vector(-x, -y, -z);
}

/*-----------------------------------------
   Vector operator '-' with two operands
-----------------------------------------*/
E3D_Vector E3D_Vector::operator -(const E3D_Vector &v) const
{
	return E3D_Vector(x-v.x, y-v.y, z-v.z);
}

/*-----------------------------------------
   Vector operator '+' with two operands
-----------------------------------------*/
E3D_Vector E3D_Vector::operator +(const E3D_Vector &v) const
{
	return E3D_Vector(x+v.x, y+v.y, z+v.z);
}

/*-----------------------------------------------
   Vector operator '*' with a scalar at right!
   (left see below)
-----------------------------------------------*/
E3D_Vector E3D_Vector::operator *(float a) const
{
	return E3D_Vector(x*a, y*a, z*a);
}

/*----------------------------------------------
   Vector operator '/' with a scalar at right.
----------------------------------------------*/
E3D_Vector E3D_Vector::operator /(float a) const  /* TODO divided by zero! */
{
	float oa=1.0f/a;
        return E3D_Vector(x*oa, y*oa, z*oa);
}

/*--------------------------
   Overload operator '+='
--------------------------*/
E3D_Vector & E3D_Vector::operator +=(const E3D_Vector &v)
{
	x += v.x;
        y += v.y;
        z += v.z;
        return *this;
}

/*--------------------------
   Overload operator '-='
--------------------------*/
E3D_Vector & E3D_Vector::operator -=(const E3D_Vector &v)
{
	x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
}

/*--------------------------
   Overload operator '*='
--------------------------*/
E3D_Vector & E3D_Vector::operator *=(float a)
{
	x *= a;
        y *= a;
        z *= a;
        return *this;
}

/*--------------------------
   Overload operator '/='
--------------------------*/
E3D_Vector & E3D_Vector::operator /=(float a)
{
	float oa=1.0f/a;                /* TODO divided by zero! */
        x *= oa;
        y *= oa;
        z *= oa;
        return *this;
}

/*----------------------------------------
   Overload operator '*' for Dot_Product
----------------------------------------*/
float E3D_Vector::operator *(const E3D_Vector &v) const
{
	return  x*v.x + y*v.y +z*v.z;
}


/*----------------------------------------------------
   Vectorize 16bits color RGB(0-255,0-255,0-255)
   to xyz(0-1.0, 0-1.0, 0-1.0)
----------------------------------------------------*/
void  E3D_Vector::vectorRGB(EGI_16BIT_COLOR color)
{
	x=1.0*COLOR_R8_IN16BITS(color)/255;
        y=1.0*COLOR_G8_IN16BITS(color)/255;
        z=1.0*COLOR_B8_IN16BITS(color)/255;
}

/*------------------------------------------
   Covert vector xyz(0-1.0, 0-1.0, 0-1.0)
   to 16bits color RGB(0-255,0-255,0-255)
------------------------------------------*/
EGI_16BIT_COLOR  E3D_Vector::color16Bits() const
{
	int R,G,B;
	R=roundf(x*255);
	if(R>255)R=255;
	G=roundf(y*255);
	if(G>255)G=255;
	B=roundf(z*255);
	if(B>255)B=255;

	return COLOR_RGB_TO16BITS(R,G,B);
}

/*----------------
   Print vector
----------------*/
void E3D_Vector::print(const char *name=NULL) const
{
     	//printf("V(%f, %f, %f)\n",x,y,z);
	printf("Vector '%s':(%f, %f, %f)\n", name?name:"", x,y,z);
};


/*------------------------------------------------
Transform the vector

	!!! --- CAUTION --- !!!
E3D_Vector as a 3D point coordiantes: -- OK.
E3D_Vector as a vector(normal etc.): translations
of input RTMatrix SHOULD be zeros.

------------------------------------------------*/
void E3D_Vector::transform(const E3D_RTMatrix &RTMatrix)
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

         xx=x; yy=y; zz=z;

         x = xx*RTMatrix.pmat[0]+yy*RTMatrix.pmat[3]+zz*RTMatrix.pmat[6] +RTMatrix.pmat[9];
         y = xx*RTMatrix.pmat[1]+yy*RTMatrix.pmat[4]+zz*RTMatrix.pmat[7] +RTMatrix.pmat[10];
         z = xx*RTMatrix.pmat[2]+yy*RTMatrix.pmat[5]+zz*RTMatrix.pmat[8] +RTMatrix.pmat[11];
}


/*  Calculate the magnitude of a vector */
float E3D_vector_magnitude(const E3D_Vector &v)
{
	return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

/* Calculate Cross_Product of two vectors */
E3D_Vector E3D_vector_crossProduct(const E3D_Vector &va, const E3D_Vector &vb)
{
	return E3D_Vector(
		va.y*vb.z - va.z*vb.y,
		va.z*vb.x - va.x*vb.z,
		va.x*vb.y - va.y*vb.x
	);
}

/* Return angle formed by va and vb, in radians.
 * if va OR vb  is ZERO vector, than NaN will return.
 */
float E3D_vector_angleAB(const E3D_Vector &va, const E3D_Vector &vb)
{
	float fv=va*vb/(va.module()*vb.module());
	if(fv>=1.0f)
		return 0.0f;
	else if(fv<=-1.0f)
		return -MATH_PI;
	else
		return acosf( va*vb/(va.module()*vb.module()) );
}

/* Overload operator '*' with a scalar at left. */
E3D_Vector operator *(float k, const E3D_Vector &v)
{
	//printf("k at left\n");
	return E3D_Vector(k*v.x, k*v.y, k*v.z);
}

/* Calculate distance between two points(vectors) */
float E3D_vector_distance(const E3D_Vector &va, const E3D_Vector &vb)
{
	float dx, dy, dz;
	dx=va.x-vb.x;
	dy=va.y-vb.y;
	dz=va.z-vb.z;

	return sqrt(dx*dx+dy*dy+dz*dz);
}


/*--------------------------------------------------
Compute reflect vector.
Suppose that reflection ray and incident ray are
symmetrical with fn.

@vi:  Vector of incoming/incident ray.
      NOT necessary to be a unit vector.
@fn:  Surface normal.
      NOT necessary to be a unit vector.

Return:
	Reflect vector.
---------------------------------------------------*/
E3D_Vector E3D_vector_reflect(const E3D_Vector &vi, const E3D_Vector &fn)
{
	/* Vr = Vi - 2n(Vi*n)/(n*n) */
	return vi-(2*fn)*(vi*fn)/(fn*fn);
}


/*** NON_Class Functions ---------------------------------------------
 Compute barycentric coordinates of point pt to triangle vt[3]

 		!!! --- CAUTION --- !!!
 The Caller MUST ensure the point is on the same plane as the triangle,
 OR result values are meaningless.

Note:
1. The point is out of the triangle there's any nagtive value in bc[3].
2. If the point is one of vertex, the bc[3] should be (1,0,0) OR (0,1,0)
   OR (0,0,1).
3. If the point in on the sides, then bc[3] should be (x,x,0) OR (x,0,x)
   OR (0,x,x).


 * @vt[3]:  triangle vertices.
 * @pt:     A point on the vt[3] plane!
 * @bc:	    To pass out barycentric coordinates.

Return:
 True: 	OK, solvable.
 False: Unsolvable, the triangle may degenerate into a line or a point.
---------------------------------------------------------------------*/
bool E3D_computeBarycentricCoord(const E3D_Vector vt[3], const E3D_Vector &pt, float bc[3])
{
	E3D_Vector e0,e1,e2;
	E3D_Vector env;
	float demon;
	E3D_Vector d0,d1,d2;
	float t0,t1,t2;

	/* Side vectors */
	e0=vt[1]-vt[0];  //v01
	e1=vt[2]-vt[1];  //v12
	e2=vt[0]-vt[2];  //v20

	/* Normal vector (not necesary to be normalized) */
	env=E3D_vector_crossProduct(e0, e1);
	demon=env*env;
	if( demon < VPRODUCT_NEARZERO) {
		egi_dpstd(DBG_RED"%s: denom ~= 0.0f! degenerated triangle.\n"DBG_RESET, __func__);
		return false;
	}

	/* Inner side vectors */
	d0=pt-vt[0];
	d1=pt-vt[1];
	d2=pt-vt[2];

	/* Areas */
	//demon/2
	t0=E3D_vector_crossProduct(e0, d1)*env; // /2;
	t1=E3D_vector_crossProduct(e1, d2)*env; // /2;
	t2=E3D_vector_crossProduct(e2, d0)*env; // /2;

	/* Barycentric coordinates */
	#if 0
	bc[0]=t0/demon;
	bc[1]=t1/demon;
	bc[2]=t2/demon;
	#endif

	/* Right sequrence vt[0]->vt[2] to bc[0]-bc[2]  HK2022-12-30 :>>> */
	bc[2]=t0/demon;
	bc[0]=t1/demon;
	bc[1]=t2/demon;

	return true;
}



	/*---------------------------------
	    Class E3D_Plane :: Functions
	---------------------------------*/

/*---------------
   Constructor
---------------*/
E3D_Plane::E3D_Plane()
{
	vn.x=0.0; vn.y=0.0; vn.z=1.0; // default (0,0,1)
        fd=0.0;
}

E3D_Plane::E3D_Plane(const E3D_Vector &Vn, float d) : vn(Vn), fd(d)
{ }

E3D_Plane::E3D_Plane(float a, float b, float c, float d)
{ /* ax+by+cz=d */
	vn.x=a; vn.y=b; vn.z=c;
        fd=d;
}

/*  vp1, vp2, vp3 --> Plane.vn  use Right_Hand Rule */
E3D_Plane::E3D_Plane(const E3D_Vector &vp1, const E3D_Vector &vp2, const E3D_Vector &vp3)
{
        vn=E3D_vector_crossProduct(vp1-vp2, vp2-vp3);
        fd=vn*vp1;
}

/*--------------
   Destructor
--------------*/
E3D_Plane:: ~E3D_Plane()
{
	//printf("E3D_Plane destructed!\n");
}

/*--------------------------------
Transform a plane.
----------------------------------*/
void E3D_Plane::transform(const E3D_RTMatrix &RTmatrix)
{
	E3D_Vector Vp;
	bool forward;

	/*  Get the intersection point with a adial from origin(0,0,0), direction vn.  */
	E3D_Ray  ray(0.0,0.0,0.0, vn);

	/* It SURELY intersects with the plane at Vp. */
	E3D_RayIntersectPlane(ray, *this, Vp, forward); /* =true */

	/* Transform Vp (on/with the plane) to new position. */
	Vp.transform(RTmatrix);

	/* Update(Transfomr) plane.nv */
	E3D_RTMatrix  vnmat=RTmatrix;
	vnmat.zeroTranslation();
	vn.transform(vnmat);

	/* Update plane.fd */
	fd=Vp*vn;
}



		/*---------------------------------
		    Class E3D_Ray :: Functions
		----------------------------------*/

/*---------------
   Constructor
---------------*/
E3D_Ray::E3D_Ray()
{
	vd.x=1.0;  vd.y=0.0;  vd.z=0.0;
}

E3D_Ray::E3D_Ray(const E3D_Vector &Vp, const E3D_Vector &Vd) // : vp0(Vp), vd(Vd)
{ vp0=Vp; vd=Vd; }

E3D_Ray::E3D_Ray(float px, float py, float pz, const E3D_Vector &Vd)
{
	vp0.x=px;  vp0.y=py;  vp0.z=pz;
        vd=Vd;
}

E3D_Ray::E3D_Ray(float px, float py, float pz, float dx, float dy, float dz)
{
        vp0.x=px;  vp0.y=py;  vp0.z=pz;
        vd.x=dx;   vd.y=dy;   vd.z=dz;
}

/*--------------
   Destructor
--------------*/
E3D_Ray::~E3D_Ray()
{
	//printf("E3D_Ray destructed!\n");
}





/*------------------------------------
Return TURE if the Ray is vertical
to the plane.
------------------------------------*/
bool E3D_RayVerticalRay(const E3D_Ray &rad1, const E3D_Ray &rad2)
{
	E3D_Vector v1=rad1.vd;
	v1.normalize();
	E3D_Vector v2=rad2.vd;
	v2.normalize();

	float vProduct=v1*v2;

	if( fabs(vProduct)<VPRODUCT_NEARZERO )
		return true;
	else
		return false;
}

/*------------------------------------
Return TURE if the Ray is parallel
with the plane.
------------------------------------*/
bool E3D_RayParallelRay(const E3D_Ray &rad1, const E3D_Ray &rad2)
{
	E3D_Vector v1=rad1.vd;
	v1.normalize();
	E3D_Vector v2=rad2.vd;
	v2.normalize();

	float vProduct=v1*v2;

	if( fabs(1.0-vProduct)<VPRODUCT_NEARZERO )
		return true;
	else
		return false;
}


/*------------------------------------
Return TURE if the Ray is parallel
with the plane.
------------------------------------*/
bool E3D_RayParallelPlane(const E3D_Ray &rad, const E3D_Plane &plane)
{
	E3D_Vector v1=rad.vd;
	v1.normalize();
	E3D_Vector v2=plane.vn;
	v2.normalize();

	float vProduct=v1*v2;

	if( fabs(vProduct)<VPRODUCT_NEARZERO )
		return true;
	else
		return false;
}


/*-----------------------------------------------------
To get the intersecting point of Ray and plane

Note:
Here rad.vn and rad.vd NOT necessary to be unit vector.

	!!! ---- CAUTION ---- !!!
The caller MUST check result.
isnan(v.x||v.y||v.z) OR isinf(v.x||v.y||v.z) implys
they are parallel objects.

@rad:   	Ref. to an E3D_Ray
@plane: 	Ref. to an E3D_Plane
@vp:		Intersection point.
@forward:	True: Intersection point is at forward,
		otherwise backward.

Return:
	True: 	Ok has intersection point vp.
	False:  No intersection.
------------------------------------------------------*/
bool E3D_RayIntersectPlane(const E3D_Ray &rad, const E3D_Plane &plane, E3D_Vector &vp, bool &forward)
{
	//E3D_Vector vp;

	float t; /* Distance from vp0 to the intersection point */
	float fp;

	//t=(plane.fd-rad.vp0*plane.vn)/(rad.vd*plane.vn);

	fp=rad.vd*plane.vn;

	/* If is parrallel */
	if( fabs(fp) < VPRODUCT_NEARZERO )
		return false;

	/* Distance */
	t=(plane.fd-rad.vp0*plane.vn)/fp;

	/* + or - */
	forward = (t>0.0f ? true:false);

	/* Intersection point */
	vp=rad.vp0+t*rad.vd;

	return true;
}






		/*-----------------------------------------------
       			Class E3D_RTMatrix :: Functions
		-----------------------------------------------*/

/*---------------
   Constructor
---------------*/
E3D_RTMatrix::E3D_RTMatrix()
{
	identity();
         //pmat = new float[4*3];
}

/*-------------
   Destructor
--------------*/
E3D_RTMatrix::~E3D_RTMatrix()
{
	//delete [] pmat;
        //printf("E3D_RTMatrix destructed!\n");
}

/*---------
   Print
---------*/
void E3D_RTMatrix::print(const char *name) const
{
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

/*-------------------------------------
   Matrix identity:
   Zero Translation and Zero Rotation
-------------------------------------*/
void E3D_RTMatrix::identity()
{
	pmat[0]=1.0f; pmat[1]=0;    pmat[2]=0;     /* m11 =1 */
        pmat[3]=0;    pmat[4]=1.0f; pmat[5]=0;     /* m22 =1 */
        pmat[6]=0;    pmat[7]=0;    pmat[8]=1.0f;  /* m33 =1 */

        pmat[9]=0;    pmat[10]=0;   pmat[11]=0;    /* txyz=0 */
}


/*----------------------------------------------
Operator '=' with one operand
-----------------------------------------------*/
E3D_RTMatrix & E3D_RTMatrix::operator =(const E3D_RTMatrix & ma)
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
		pmat[i]=ma.pmat[i];

	return *this;
}

/*----------------------------------------------
Operator '+=' with one operand
-----------------------------------------------*/
E3D_RTMatrix & E3D_RTMatrix::operator +=(const E3D_RTMatrix & ma)
{
        for(int i=0; i<12; i++)
                pmat[i] += ma.pmat[i];

	return *this;
}


/*----------------------------------------------
Multiply a matrix with a float
ma=m*a;
-----------------------------------------------*/
E3D_RTMatrix E3D_RTMatrix::operator *(float a) const
{
	E3D_RTMatrix fmat;

	/*** Note:
	 * Rotation Matrix:
	   pmat[0,1,2]: m11, m12, m13
	   pmat[3,4,5]: m21, m22, m23
	   pmat[6,7,8]: m31, m32, m33
	 * Translation:
	   pmat[9,10,11]: tx,ty,tz
	 */

	for(int i=0; i<12; i++)
		fmat.pmat[i] = pmat[i]*a;

	return fmat;
}

/*----------------------------------------------
Multiply a matrix with a float
m=m*a;
-----------------------------------------------*/
E3D_RTMatrix operator *(float a, const E3D_RTMatrix & ma)
{
	return ma*a;
}


/*--------------------------
   Overload operator '*='
   TODO to test.
-------------------------*/
E3D_RTMatrix & E3D_RTMatrix:: operator *=(const E3D_RTMatrix & ma)
{
	E3D_RTMatrix  mb=(*this)*ma;

        *this = mb;
        return *this;
}

/*--------------------
   Zero Translation
--------------------*/
void E3D_RTMatrix::zeroTranslation()
{
	pmat[9]=0;    pmat[10]=0;   pmat[11]=0;    /* tx/ty/tz */
}

/*-----------------
   Zero Rotation
-----------------*/
void E3D_RTMatrix::zeroRotation()
{
	pmat[0]=1.0f; pmat[1]=0;    pmat[2]=0;     /* m11 =1 */
        pmat[3]=0;    pmat[4]=1.0f; pmat[5]=0;     /* m22 =1 */
        pmat[6]=0;    pmat[7]=0;    pmat[8]=1.0f;  /* m33 =1 */
}

/*------------------------------------------
   Transpose: for an orthogonal matrix,
   its inverse is equal to its transpose!

	!!!--- CAUTION ---!!!
1. Scale is NOT orthogonal operation.
2. Translation part NOT applys!
------------------------------------------*/
void E3D_RTMatrix::transpose()
{
	E3D_RTMatrix copyMat=*this;

	/* Rotation part */
        for( int i=0; i<3; i++)
        	for( int j=0; j<3; j++)
                	pmat[i*3+j]=copyMat.pmat[i+j*3];

	/* KEEP translatoin part */
}

/*-----------------------------------
Inverse the RTMatrix.
-----------------------------------*/
void E3D_RTMatrix::inverse()
{
#if 0 ////////////////  3X3 XXX WRONG! XXX ////////////////////
	/* Inverse Rotation/Scale(NON_translation) component */
	float _mat9[3*3];
	for(int i=0; i<9; i++)
		_mat9[i]=this->pmat[i];
   	struct float_Matrix mat9;
	mat9.nr=3; mat9.nc=3; mat9.pmat=_mat9;

	float _invmat9[3*3];
   	struct float_Matrix invmat9;
   	invmat9.nr=3; invmat9.nc=3; invmat9.pmat=_invmat9;

	Matrix_Inverse(&mat9, &invmat9);

	for(int i=0; i<9; i++)
		this->pmat[i]=invmat9.pmat[i];

	/* Inverse translation component --- XXX WRONG! XXX ---*/
	this->pmat[9] = -this->pmat[9];
	this->pmat[10] = -this->pmat[10];
	this->pmat[11] = -this->pmat[11];

#else //////////////// 4X4 ////////////////////

	float _mat16[4*4];

	/* Put 3x3 into 4x4 matrix */
	for(int i=0; i<3; i++)
	   for(int j=0; j<3; j++)
		_mat16[i*4+j]=this->pmat[i*3+j];

	/* Put tranlation XYZ into 4xe matrix */
	_mat16[12]=this->pmat[9];
	_mat16[13]=this->pmat[10];
	_mat16[14]=this->pmat[11];

	/* Fill other area */
	_mat16[3]=0.0; _mat16[7]=0.0; _mat16[11]=0.0;
	_mat16[15]=1.0;

   	struct float_Matrix mat16;
	mat16.nr=4; mat16.nc=4; mat16.pmat=_mat16;

	float _invmat16[4*4];
   	struct float_Matrix invmat16;
   	invmat16.nr=4; invmat16.nc=4; invmat16.pmat=_invmat16;

	if( !Matrix_Inverse(&mat16, &invmat16) )
		egi_dpstd(DBG_RED"Matrix_Inverse() failed!\n"DBG_RESET);

	/* Extract 3x3 from 4x4 matrix */
	for(int i=0; i<3; i++)
	   for(int j=0; j<3; j++)
		this->pmat[i*3+j]=invmat16.pmat[i*4+j];

	/* Extract translation XYZ from 4x4 matrix */
	this->pmat[9]=invmat16.pmat[12];
	this->pmat[10]=invmat16.pmat[13];
	this->pmat[11]=invmat16.pmat[14];
#endif
}


/*-----------------------------------------
   Check wheather the matrix is identity
-----------------------------------------*/
bool E3D_RTMatrix::isIdentity()
{
	for(int i=0; i<9; i++) {
        	if(i%4==0) {
			/* Note: too small value will cost much more time OR deadlock in adjusting... */
                	if(fabs(pmat[i]-1.0)>1.0e-5) /* <---- TODO: MORE TEST  VPRODUCT_EPSILON=(1.0e-6) */
                        	return false;
                }
                else if(fabs(pmat[i])>1.0e-5)
                	return false;
        }

        return true;
}

/*-------------------------------------------
   Check wheather the matrix is orthogonal
-------------------------------------------*/
bool E3D_RTMatrix::isOrthogonal()
{
#if 1 //////////  METHOD_1 //////////
                E3D_RTMatrix  Tmat=*this; /* Transpose of this matrix */
                Tmat.transpose();

                Tmat = Tmat*(*this);
                //Tmat.print("M*M^t");

                return( Tmat.isIdentity() );
#else ////////  Method_2 ////////////


#endif

}

/*-------------------------------------------------------------
   Orthogonalize+Normalize the RTMatrix.
   Reference: Direction Cosine Matrix IMU: Theory (Eqn 19-21)
              by Paul Bizard and  Ecole Centrale

TODO: One call of orthNormalize() MAY NOT be enough!

-------------------------------------------------------------*/
void  E3D_RTMatrix::orthNormalize()
{
	E3D_Vector axisX, new_axisX;
        E3D_Vector axisY, new_axisY;
        E3D_Vector new_axisZ;
        float error;  /* As  dot_product of axisX(row0)*axisY(row1) of RTMatrix,  */

        axisX.x = pmat[0]; axisX.y=pmat[1]; axisX.z=pmat[2];
        axisY.x = pmat[3]; axisY.y=pmat[4]; axisY.z=pmat[5];

        /* dot_product of axisX and axisY */
        error=pmat[0]*pmat[3]+pmat[1]*pmat[4]+pmat[2]*pmat[5];

 /* TEST: -------- */
        egi_dpstd("Dot_product: axisX*axisY=%e  (should be 0.0!) \n", error);

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

/*--------------------------------------------
   Set part of Translation TxTyTz in pmat[]
--------------------------------------------*/
void E3D_RTMatrix::setTranslation( const E3D_Vector &tv )
{
	pmat[9]=tv.x;    pmat[10]=tv.y;   pmat[11]=tv.z;    /* tx/ty/tz */
}


/*--------------------------------------------
   Set part of scaleXYZ in pmat[]
--------------------------------------------*/
void E3D_RTMatrix::setScaleXYZ( float sx, float sy, float sz)
{
        pmat[0]=sx;    pmat[4]=sy;    pmat[8]=sz;
}

/*--------------------------------------------
   Set part of Translation TxTyTz in pmat[]
--------------------------------------------*/
void E3D_RTMatrix::setTranslation( float x, float y, float z )
{
	pmat[9]=x;    pmat[10]=y;   pmat[11]=z;    /* tx/ty/tz */
}

/*--------------------------------------------
   Add part of Translation TxTyTz in pmat[]
--------------------------------------------*/
void E3D_RTMatrix::addTranslation( float dx, float dy, float dz )
{
	pmat[9]+=dx;    pmat[10]+=dy;   pmat[11]+=dz;    /* tx/ty/tz */
}


/*----------------------------------------------
   Set part of RotationMatrix in pmat[]

   @chaxis:  'x','y' or'z' as rotation axis
   @angle: Rotation angle, in radian

   Note: Translation keeps!
----------------------------------------------*/
void E3D_RTMatrix::setRotation(char chaxis, float angle)
{
	float vsin, vcos;

        vsin=sinf(angle);
        vcos=cosf(angle);

   	switch(chaxis) {
	   case 'x': case 'X':
		pmat[0]=1.0f;  pmat[1]=0.0f;  pmat[2]=0.0f;
		pmat[3]=0.0f;  pmat[4]=vcos;  pmat[5]=vsin;
		pmat[6]=0.0f;  pmat[7]=-vsin;  pmat[8]=vcos;
		break;
	   case 'y': case 'Y':
		pmat[0]=vcos;  pmat[1]=0.0f;  pmat[2]=-vsin;
		pmat[3]=0.0f;  pmat[4]=1.0f;  pmat[5]=0.0f;
		pmat[6]=vsin;  pmat[7]=0.0f;  pmat[8]=vcos;
		break;
	   case 'z': case 'Z':
		pmat[0]=vcos;  pmat[1]=vsin;  pmat[2]=0.0f;
		pmat[3]=-vsin;  pmat[4]=vcos;  pmat[5]=0.0f;
		pmat[6]=0.0f;  pmat[7]=0.0f;  pmat[8]=1.0f;
		break;
	}

        /* Keep pmat[9-11] */
}

/*----------------------------------------------
   Set part of RotationMatrix in pmat[]

   @axis:  Rotation axis, MUST be normalized!
   @angle: Rotation angle, in radian
   Note: Translation keeps!
----------------------------------------------*/
void E3D_RTMatrix::setRotation( const E3D_Vector &axis, float angle)
{
	float vsin, vcos;
        float a,ax,ay,az;

        if(fabsf(axis*axis-1.0f)>0.01f) {
        	egi_dpstd("Input axis is NOT normalized!\n");
                return;
        }

        vsin=sinf(angle);
        vcos=cosf(angle);

        a=1.0f-vcos;
        ax=a*axis.x;
        ay=a*axis.y;
        az=a*axis.z;

        pmat[0]=ax*axis.x+vcos;         //m11
        pmat[1]=ax*axis.y+axis.z*vsin;  //m12
        pmat[2]=ax*axis.z-axis.y*vsin;  //m13

        pmat[3]=ay*axis.x-axis.z*vsin;  //m21
        pmat[4]=ay*axis.y+vcos;         //m22
        pmat[5]=ay*axis.z+axis.x*vsin;  //m23

        pmat[6]=az*axis.x+axis.y*vsin;  //m31
        pmat[7]=az*axis.y-axis.x*vsin;  //m32
        pmat[8]=az*axis.z+vcos;         //m33

        /* Keep pmat[9-11] */
}





/*----------------------------------------------
   Set part of RotationMatrix in pmat[]
   Rotation orientation is from vfrom to vto.

Note:

   @vfrom: Guide vector before rotation.
   @vto:   Guide vector after rotation.
   Note: Translation keeps!
----------------------------------------------*/
void E3D_RTMatrix::setRotation( const E3D_Vector &vfrom, const E3D_Vector &vto )
{
	E3D_Vector axis;
	float angle;
	float vsin, vcos;
        float a,ax,ay,az;

	/* Get axis vector */
	axis=E3D_vector_crossProduct(vfrom, vto);
	axis.normalize();

	/* Check vfrom.moduel()*vto.module() HK2022-11-17 */
	float mft=vfrom.module()*vto.module();
	if(mft<VPRODUCT_NEARZERO || mft<VPRODUCT_NEARZERO)
		return;

	/* Get rotation angle */
	//angle=acosf(vfrom*vto/(vfrom.module()*vto.module()));
	angle=acosf(vfrom*vto/mft);

	/* set rotation matrix for pmax[] */
        vsin=sinf(angle);
        vcos=cosf(angle);

        a=1.0f-vcos;
        ax=a*axis.x;
        ay=a*axis.y;
        az=a*axis.z;

        pmat[0]=ax*axis.x+vcos;         //m11
        pmat[1]=ax*axis.y+axis.z*vsin;  //m12
        pmat[2]=ax*axis.z-axis.y*vsin;  //m13

        pmat[3]=ay*axis.x-axis.z*vsin;  //m21
        pmat[4]=ay*axis.y+vcos;         //m22
        pmat[5]=ay*axis.z+axis.x*vsin;  //m23

        pmat[6]=az*axis.x+axis.y*vsin;  //m31
        pmat[7]=az*axis.y-axis.x*vsin;  //m32
        pmat[8]=az*axis.z+vcos;         //m33

        /* Keep pmat[9-11] */
}

/*----------------------------------------------
   Set Rotation/Scale in pmat[]:
   Rotation orientation is from vfrom to vto.
   Scale=vto.module/vfrom.module.

   @vfrom: Guide vector before rotation.
   @vto:   Guide vector after rotation.
   Note: Translation keeps!
----------------------------------------------*/
void E3D_RTMatrix::setScaleRotation( const E3D_Vector &vfrom, const E3D_Vector &vto )
{
	E3D_Vector axis;
	float angle;
	float vsin, vcos;
        float a,ax,ay,az;

	E3D_RTMatrix smat;
	float scale=vto.module()/vfrom.module();
	smat.setScaleXYZ(scale,scale,scale);

	/* Get axis vector */
	axis=E3D_vector_crossProduct(vfrom, vto);
	axis.normalize();

	/* Get rotation angle */
	angle=acosf(vfrom*vto/(vfrom.module()*vto.module()));

	/* set rotation matrix for pmax[] */
        vsin=sinf(angle);
        vcos=cosf(angle);

        a=1.0f-vcos;
        ax=a*axis.x;
        ay=a*axis.y;
        az=a*axis.z;

        pmat[0]=ax*axis.x+vcos;         //m11
        pmat[1]=ax*axis.y+axis.z*vsin;  //m12
        pmat[2]=ax*axis.z-axis.y*vsin;  //m13

        pmat[3]=ay*axis.x-axis.z*vsin;  //m21
        pmat[4]=ay*axis.y+vcos;         //m22
        pmat[5]=ay*axis.z+axis.x*vsin;  //m23

        pmat[6]=az*axis.x+axis.y*vsin;  //m31
        pmat[7]=az*axis.y-axis.x*vsin;  //m32
        pmat[8]=az*axis.z+vcos;         //m33

        /* Keep pmat[9-11] */


	/* Compound */
	*this = smat*(*this);
}

/*----------------------------------------------
	    !!! --- CAUTION --- !!!
This RTMatrix will BE set as identity at first,

   then move Vas to Oxyz as center of scale and rotation,
   then set RTMatrix for transforming Va to Vb.
   Sequence: ScaleMat*RotMat*TransMat;
   Scale center point: Vas
   At last, move Vas back to original position.

   @Vas,Vae: Guide vector before transforming.
   @Vbs,Vbe: Guide vector after transforming.
----------------------------------------------*/
void E3D_RTMatrix::setTransformMatrix( const E3D_Vector &Vas, const E3D_Vector &Vae, const E3D_Vector &Vbs, const E3D_Vector &Vbe)
{
	E3D_Vector axis;
	float angle;
	float vsin, vcos;
        float a,ax,ay,az;

	/* This RTMatrix will be set as identity first */
	this->identity();

	E3D_Vector vfrom=Vae-Vas;
	E3D_Vector vto=Vbe-Vbs;
	E3D_Vector vtrans=Vbs-Vas;

	/* move Vas to Oxyz as center of scale and rotation, then move back later */
	E3D_RTMatrix Cmat, CBmat;
	Cmat.setTranslation(-Vas); //RTMatrix to translate Vas to Oxyz origin
	CBmat.setTranslation(Vas); //RTMatrix to retrive Vas back to original position

	/* Set scale */
	E3D_RTMatrix Smat;
	float scale=vto.module()/vfrom.module();
	Smat.setScaleXYZ(scale,scale,scale);

	/* Set scale + translation */
	E3D_RTMatrix Tmat;
	Tmat.setTranslation(vtrans);

	/* Get axis vector */
	axis=E3D_vector_crossProduct(vfrom, vto);
	axis.normalize();

	/* Get rotation angle */
	angle=acosf(vfrom*vto/(vfrom.module()*vto.module()));

	/* set rotation matrix for pmax[] */
        vsin=sinf(angle);
        vcos=cosf(angle);

        a=1.0f-vcos;
        ax=a*axis.x;
        ay=a*axis.y;
        az=a*axis.z;

        pmat[0]=ax*axis.x+vcos;         //m11
        pmat[1]=ax*axis.y+axis.z*vsin;  //m12
        pmat[2]=ax*axis.z-axis.y*vsin;  //m13

        pmat[3]=ay*axis.x-axis.z*vsin;  //m21
        pmat[4]=ay*axis.y+vcos;         //m22
        pmat[5]=ay*axis.z+axis.x*vsin;  //m23

        pmat[6]=az*axis.x+axis.y*vsin;  //m31
        pmat[7]=az*axis.y-axis.x*vsin;  //m32
        pmat[8]=az*axis.z+vcos;         //m33

        /* Keep pmat[9-11] */

	/* Compound */
	/* move Vas to Oxyz center for scale and rotation, then move back. */
	*this = Cmat*Smat*(*this)*CBmat*Tmat;
}

/*----------------------------------------------------------------------------------
Parse axisToks and right-mutilply imats to RTmat.
For extrinsic rotation, each ref. COORD is the same.

@axisToks: 	Extrinsic rotation sequence Tokens(rotating axis), including:
		'X'('x') 'Y'('y') 'Z'('z')
		First char: 'L': Left-multiply,  'R': Right-multiply(Default).  HK2022-10-31
		Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...
		1. "ZYX" extrinsic rotation sequence: RotZ -> RotY -> RotX, matrix multiplation: Mz*My*Mx

@ang[3]: 	In radians, Rotation angles cooresponding to axisToks.
-----------------------------------------------------------------------------------*/
void E3D_RTMatrix::combExtriRotation(const char *axisToks, float ang[3])
{
	if(axisToks==NULL)
		return;

	/* Get reversed sequence of rotation */
//	int toks=strlen(axisToks);

	/* imat:  translation is ignored! */
	E3D_RTMatrix imat;

	/* cmat as combined:  translation is ignored! */
//	E3D_RTMatrix cmat;

	bool case_OK;
	bool Right_Multiply=true; /* Default as left-multiply to *this */
	unsigned int np=0;  /* tokens parsed */
//	int rnp;  /* np as backward */

	/* Check multiply sequence */
	if(axisToks[0]=='L')
		Right_Multiply=false;

	while( axisToks[np] ) {

		/* 1. Max. 1+3 tokens, in order. */
		if(np>4) break;

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
			if(Right_Multiply)
				*this = (*this)*imat;
			else
				*this = imat*(*this);
		}

		/* np increment */
		np++;
	}
}

/*----------------------------------------------------------------------------------
Parse axisToks and left-mutilply imats to RTmat.
For intrinsic rotation, each ref COORD is rotating also.

	    !!! ---- CAUTION ---- !!!
	Gimbal lock problem for interpolation!

@axisToks: 	Intrinsic rotation sequence Tokens(rotating axis), including:
		'X'('x') 'Y'('y') 'Z'('z')
		First char: 'L': Left-multiply(Default),  'R': Right-multiply.  HK2022-10-31
		Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...

		1. "ZYX" intrinsic rotation sequence: RotZ(a) -> RotY(b) -> RotX(r), matrix multiplation: Mx*My*Mz
		    Notice that 'reversed' matrix multiplation applys.
		2. The function reverse axisToks[] to RxyzToks[].

@ang[3]: 	In radians, Rotation angles cooresponding to axisToks.
-----------------------------------------------------------------------------------*/
void E3D_RTMatrix::combIntriRotation(const char *axisToks, float ang[3])
{
	if(axisToks==NULL)
		return;

	/* Get reversed sequence of rotation */
	int toks=strlen(axisToks);

	/* imat:  translation is ignored! */
	E3D_RTMatrix imat;
	E3D_RTMatrix cmat;

	/* cmat as combined:  translation is ignored! */
//	E3D_RTMatrix cmat;

	bool case_OK;
	bool Left_Multiply=true; /* Default as left-multiply to *this */
	unsigned int np=0; /* tokens parsed */
	int rnp;  /* np as backward */

	/* Check multiply sequence */
	if(axisToks[0]=='R')
		Left_Multiply=false;

	/* Keep axis sequence and revert angle sequene, then the result of extriRotation is same as original intriRoration */

	while( (rnp=toks-1 -np)>=0 && axisToks[toks-1 -np] ) {

		/* Revers index of axisToks[] */
		rnp=toks-1 -np;

		/* 1. Max. 3 tokens. Reverse order. */
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
			default: /* 'L','R' to be ignored */
			   case_OK=false;
			   egi_dpstd("Unrecognizable aix token '%c', NOT in 'xXyYzZ'!\n", axisToks[rnp]);
			   break;
		}

		/* 4. Multiply imats  */
		if( case_OK ) {
			/* Only combine Rotation */
//			*this= (*this)*imat;  /* Hi, Dude! :))))))) */
			/* Noticed that axis sequence reverted!, revert here again. put last mat at first,
				 	     .... Result: same effect as revert angles. */
			cmat = cmat*imat; //cmat = imat*cmat;
		}

		/* np increment */
		np++;
	}

	/* OK */
	if(Left_Multiply)
		 *this=cmat*(*this);
	else
		*this=(*this)*cmat;
}



/*---------------------------------------------------
   Set this matrix as a projection Matrix.
   Move to let XY as normal to n and the projection
   plan contains original origin.

   @n: Normal of projection plan
---------------------------------------------------*/
void E3D_RTMatrix::setupProject(const E3D_Vector &n)
{
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

 /*** For scale matrix, ONLY pmat[0],pmat[4], pmat[8] is !=0.0

	// Rotation
	mc.pmat[0]=ma.pmat[0]*Sx;
	mc.pmat[1]=ma.pmat[1]*Sy;
	mc.pmat[2]=ma.pmat[2]*Sz;

	mc.pmat[3]=ma.pmat[3]*Sx;
	mc.pmat[4]=ma.pmat[4]*Sy;
	mc.pmat[5]=ma.pmat[5]*Sz;

	mc.pmat[6]=ma.pmat[6]*Sx;
	mc.pmat[7]=ma.pmat[7]*Sy;
	mc.pmat[8]=ma.pmat[8]*Sz;

	// Translation
	mc.pmat[9]=ma.pmat[9]*Sx;
	mc.pmat[10]=ma.pmat[10]*Sy;
	mc.pmat[11]=ma.pmat[11]*Sz;

*/

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

/*-----------------------------------------------------------
To combine intrinsic rotation with original RTmat, then update
RTMat to make objects under Global Coord.

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
------------------------------------------------------------*/
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
1.  One purpose of this function is to adjust triGroupList[].omat
    AFTER whole trimesh is transformed with CoordMat, so it
    can be applied E3D_TriMesh::renderMesh().
2.  For vertices which will apply the result RTmat:
    1.1 All vertex coordinates MUST be under GLOBAL Coord!
3.  Intrinsic rotation are taken under LOCAL COORD, which can be restored by TcoordMat.
4.  Even if input axisToks==NULL, CoordMat will apply to RTmat also.
5.  Notice that axisToks are applied/mutiplied with Pxyz in reverse order!

	--- Translation is Ignored! ----

@axisToks: 	Intrinsic rotation sequence Tokens(rotating axis), including:
		'X'('x') 'Y'('y') 'Z'('z')
		Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...

		1. "ZYX" intrinsic rotation sequence: RotZ -> RotY -> RotX, matrix multiplation: Mx*My*Mz
		    Notice that 'reversed' matrix multiplation applys.
		2. The function reverse axisToks[] to RxyzToks[].

@ang[3]: 	In radian, Rotation angles corresponding to axisToks.
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
For intrinsic rotation, each ref COORD is rotating also.

@axisToks: 	Intrinsic rotation sequence Tokens(rotating axis), including:
		'X'('x') 'Y'('y') 'Z'('z')
		Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...

		1. "ZYX" intrinsic rotation sequence: RotZ -> RotY -> RotX, matrix multiplation: Mx*My*Mz
		    Notice that 'reversed' matrix multiplation applys.
		2. The function reverse axisToks[] to RxyzToks[].

@ang[3]: 	In radians, Rotation angles cooresponding to axisToks.
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


/*----------------------------------------------------------------------------------
Parse axisToks and right-mutilply imats to RTmat.
For extrinsic rotation, each ref. COORD is the same.

@axisToks: 	Extrinsic rotation sequence Tokens(rotating axis), including:
		'X'('x') 'Y'('y') 'Z'('z')
		Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...
		1. "ZYX" extrinsic rotation sequence: RotZ -> RotY -> RotX, matrix multiplation: Mz*My*Mx

@ang[3]: 	In radians, Rotation angles cooresponding to axisToks.
@RTmat:		Original RTMatrix. to be combined with.
-----------------------------------------------------------------------------------*/
void E3D_combExtriRotation(const char *axisToks, float ang[3],  E3D_RTMatrix & RTmat)
{
	if(axisToks==NULL)
		return;

	/* Get reversed sequence of rotation */
//	int toks=strlen(axisToks);

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


	/*-----------------------------------------------
		Class E3D_Quaternion :: Functions
	-----------------------------------------------*/


/*---------------------------
       Constructor
---------------------------*/
E3D_Quaternion::E3D_Quaternion()
{
	w=1.0f;
	x=y=z=0.0f;
}


/*---------------------------
       Set as identity
---------------------------*/
void E3D_Quaternion::identity()
{
	w=1.0f;
	x=y=z=0.0f;
}

/*---------------------------
       Print
---------------------------*/
void E3D_Quaternion::print(const char *name) const
{
        printf("Quaternion %s: [%f (%f, %f, %f)]\n", name?name:"", w,x,y,z);
}

/*------------------------
	Normalize
------------------------*/
void E3D_Quaternion::normalize()
{
	float mod=sqrtf(w*w+x*x+y*y+z*z);
	float invmod; /* 1.0/mod */
	if(mod<VPRODUCT_NEARZERO) {
		egi_dpstd(DBG_RED"mod<VPRODUCT_NEARZERO!\n"DBG_RESET);
		identity();
	}
	else {
		invmod=1.0f/mod;
		w *= invmod;
		x *= invmod;
		y *= invmod;
		z *= invmod;
	}
}

/*----------------------------------
Overload '*' as for Cross-Product
----------------------------------*/
E3D_Quaternion E3D_Quaternion::operator *(const E3D_Quaternion &q) const
{
	E3D_Quaternion cp;

	cp.w=w*q.w-x*q.x-y*q.y-z*q.z;
	cp.x=w*q.x+x*q.w+z*q.y-y*q.z;
	cp.y=w*q.y+y*q.w+x*q.z-z*q.x;
	cp.z=w*q.z+z*q.w+y*q.x-x*q.y;

	return cp;
}

/*----------------------------------
Overload '*=' as for Cross-Product
----------------------------------*/
E3D_Quaternion & E3D_Quaternion::operator *=(const E3D_Quaternion &q)
{
	*this = (*this)*q;

	return *this;
}

/*---------------------------------------
Inverse quaternion, as to reverse rotation.
---------------------------------------*/
void  E3D_Quaternion::inverse()
{
	x = -x;
	y = -y;
	z = -z;
}



/*--------------------------
Get rotation angle in radian.
---------------------------*/
float E3D_Quaternion::getRotationAngle() const
{
	return 2.0f*acos(w);
}

/*--------------------------
Get rotation axis.
---------------------------*/
E3D_Vector E3D_Quaternion::getRotationAxis() const
{
	E3D_Vector axis(x,y,z);
	axis.normalize();

	return axis;
}

/*-------------------------------------
Set quaternion for rotation.
@chAxis: 'X','Y' or 'Z'  rotation axis.
@angle:  Angle in radian.
--------------------------------------*/
void E3D_Quaternion::setRotation(char chAxis, float angle)
{
   /* Half angle */
   angle *=0.5;

   /* w component */
   w=cos(angle);

   switch(chAxis) {
	case 'x': case 'X':
		x=sin(angle); /* <--- X */
		y=z=0.0f;
		break;
	case 'y': case 'Y':
		y=sin(angle);  /* <--- Y */
		x=z=0.0f;
		break;
	case 'z': case 'Z':
		z=sin(angle); /* <---- Z */
		x=y=0.0f;
   }
}


/*---------------------------------------------------------------
Set extrinsic rotation.
@axisToks:      EXtrinsic rotation sequence Tokens(rotating axis), including:
                'X'('x') 'Y'('y') 'Z'('z')
                Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...

@ang[3]:        In radians, Rotation angles cooresponding to axisToks
------------------------------------------------------------------*/
void E3D_Quaternion::setExtriRotation(const char *axisToks, float ang[3])
{
	E3D_RTMatrix mat; /* Init as identity */

	/* extrinsic rotation matrix */
	mat.combExtriRotation(axisToks, ang);

	/* to THIS quaternion */
        fromMatrix(mat);
}

/*---------------------------------------------------------------
Set intrinsic rotation.
OK quaternion sequence rotation is natrually intrinsic rotatioin.

@axisToks:      Intrinsic rotation sequence Tokens(rotating axis), including:
                'X'('x') 'Y'('y') 'Z'('z')
                Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...
@ang[3]:        In radians, Rotation angles cooresponding to axisToks
------------------------------------------------------------------*/
void E3D_Quaternion::setIntriRotation(const char *axisToks, float ang[3])
{
#if 0   /////////// call E3D_RTMatrix::combIntriRotation()  //////////
	E3D_RTMatrix mat; /* Init as identity */

	/* intrinsic rotation matrxi */
	mat.combIntriRotation(axisToks, ang);

	/* to THIS  quaternion */
	fromMatrix(mat);
#else   ///////////////////////////////////////////////////////////
	E3D_Quaternion  q;

	/* Identity */
	identity();

	/* ---
         * IntriRotation: p'=(c(ba))p(c(ba))^-1,  q=abc (Quaternion natrual rotation)
	 * ??? ExtriRotation: p'=(abc)p(abc)^-1,  q=cba
	 */

	int toks=strlen(axisToks);
	unsigned int np=0;
	while( axisToks[np] ) {
		/* 1. Max. 3 tokens */
		if(np>2) break;

		/* 2. Compute imat as per rotation axis and angle */
		switch( axisToks[np] ) {
			case 'x': case 'X':
			case 'y': case 'Y':
			case 'z': case 'Z':
			   q.setRotation(axisToks[np], ang[np]);
			   *this = q*(*this);   /* For intriRotation: in corss_multip sequence 'c(ba)'  more: e(d(c(ba))) */

			   /* np increment */
			   np++;
			   break;
			default:
			   egi_dpstd("Unrecognizable aix token '%c', NOT in 'xXyYzZ'!\n", axisToks[np]);
			   break;
		}

		/* np increment */
		//np++; NOT HERE, see in case:xyzXYZ HK2022-11-29 */
	}

	/* Normalize it, TODO necessary here? OR elsewhere. */
	// normalize();

#endif  //////////////////////////////////////////////////////////
}


/*-------------------------------------
Convert quaternion to RTMatrix rotation
part ONLY, translation part of mat
keeps unchanged.
--------------------------------------*/
void E3D_Quaternion::toMatrix(E3D_RTMatrix &mat) const
{
	/*** RTMatrix
         * Rotation Matrix:
                   pmat[0,1,2]: m11, m12, m13
                   pmat[3,4,5]: m21, m22, m23
                   pmat[6,7,8]: m31, m32, m33
         * Translation:
                   pmat[9,10,11]: tx,ty,tz
	*/

	mat.pmat[0]=1.0-2.0*y*y-2.0*z*z;
	mat.pmat[1]=2.0*x*y+2.0*w*z;
	mat.pmat[2]=2.0*x*z-2.0*w*y;

	mat.pmat[3]=2.0*x*y-2.0*w*z;
	mat.pmat[4]=1.0-2.0*x*x-2.0*z*z;
	mat.pmat[5]=2.0*y*z+2.0*w*x;

	mat.pmat[6]=2.0*x*z+2.0*w*y;
	mat.pmat[7]=2.0*y*z-2.0*w*x;
	mat.pmat[8]=1.0-2.0*x*x-2.0*y*y;
}


/*-------------------------------------------------------
Convert RTMatrix rotation component to
this quaternion.

Note:
	!!! --- CAUTION --- !!!
1. Input tmat should a normalized orthogonal matrix!

   Input mat SHOULD not be integrated with scaleMat,
   otherwise, the result will be WRONG! also NOT unit quaternion.

   Example:
	0.000000  0.000000  0.327751
	0.327751  0.000000  0.000000
	0.000000  0.327751  0.000000
   Result:
	quat[0.500000 (-0.163876, -0.163876, -0.163876)]

--------------------------------------------------------*/
void E3D_Quaternion::fromMatrix(const E3D_RTMatrix &mat)
{
	/*** RTMatrix
         * Rotation Matrix:
                   pmat[0,1,2]: m11, m12, m13
                   pmat[3,4,5]: m21, m22, m23
                   pmat[6,7,8]: m31, m32, m33
         * Translation:
                   pmat[9,10,11]: tx,ty,tz
	*/
	/* Find the biggest among facts[4] */
	float facts[4];
	facts[0]=mat.pmat[0]+mat.pmat[4]+mat.pmat[8];  /* factW:  m11+m22+m33 */
	facts[1]=mat.pmat[0]-mat.pmat[4]+mat.pmat[8];  /* factX:  m11-m22+m22 */
	facts[2]=-mat.pmat[0]+mat.pmat[4]-mat.pmat[8];  /* factY: -m11+m22-m33 */
	facts[3]=-mat.pmat[0]-mat.pmat[4]+mat.pmat[8];  /* factZ: -m11-m22+m33 */

	int index=0; /* Index for the biggest one */
	for(int k=0; k<4; k++) {
		if(facts[index]<facts[k])
			index=k;
	}

	/* Example: divisor=4W(4X,4Y or 4Z), where fact W=sqrt(factW+1)/2 */
	float fact=sqrt(facts[index]+1.0)*0.5;
	float invdiv=1.0/(4.0*fact);  /* =1/divisor */

	switch(index) {
		case 0: /* fact W */
			w=fact;
			x=(mat.pmat[5]-mat.pmat[7])*invdiv;   /* (m23-m32)/4w */
			y=(mat.pmat[6]-mat.pmat[2])*invdiv;   /* (m31-m13)/4w */
			z=(mat.pmat[1]-mat.pmat[3])*invdiv;   /* (m12-m21)/4w */
			break;
		case 1: /* fact X */
			x=fact;
			w=(mat.pmat[5]-mat.pmat[7])*invdiv;   /* (m23-m32)/4w */
			y=(mat.pmat[1]+mat.pmat[3])*invdiv;   /* (m12+m21)/4w */
			z=(mat.pmat[6]+mat.pmat[2])*invdiv;   /* (m31+m13)/4w */
			break;
		case 2: /* fact Y */
			y=fact;
			w=(mat.pmat[6]-mat.pmat[2])*invdiv;   /* (m31-m13)/4w */
			x=(mat.pmat[1]+mat.pmat[3])*invdiv;   /* (m12+m21)/4w */
			z=(mat.pmat[5]+mat.pmat[7])*invdiv;   /* (m23+m32)/4w */
			break;
		case 3: /* fact Z */
			z=fact;
			w=(mat.pmat[1]-mat.pmat[3])*invdiv;   /* (m12-m21)/4w */
			x=(mat.pmat[6]+mat.pmat[2])*invdiv;   /* (m31+m13)/4w */
			y=(mat.pmat[5]+mat.pmat[7])*invdiv;   /* (m23+m32)/4w */
			break;
	}
}


/*---------------------------------------------------
Spherical Linear Interpolation.

	!!! --- CAUTION --- !!!
If interpolation between rotation angle [0-180]Deg,
the result rotation Axis direction can be ambiguous!
Example:  RotationX [0 180]Deg, slerp at t=0.5,
	  result angle 90Deg, however axis can be '-X'!

@q0, q1: two quaternions for interpolation.
@rt:      interpolation ratio parameter, range [0 1.0].

----------------------------------------------------*/
void E3D_Quaternion::slerp(const E3D_Quaternion &q0, const E3D_Quaternion &q1, float rt)
{
	float cosang;
	float w1,x1,y1,z1;
	float k0,k1;

	/* Dotproduction to cal. cosang for angle between q0->q1 */
	cosang=q0.w*q1.w+q0.x*q1.x+q0.y*q1.y+q0.z*q1.z;

	/* Check cosang */
	if(cosang<0.0f) {
		w1=-q1.w; x1=-q1.x; y1=-q1.y; z1=-q1.z;
		cosang=-cosang;
	}
	else {
		w1=q1.w; x1=q1.x; y1=q1.y; z1=q1.z;
	}

	/* If sinang=0.0! Use normal linear interpolation */
	if( cosang >0.999f ) {
		k0=1.0f-rt;
		k1=rt;
	}
	/* Slerp */
	else {
		float ang; /* in radian */
		float sinang;
		float invsinang; /* 1/sinang */

		sinang=sqrt(1.0f-cosang*cosang);
		invsinang=1.0f/sinang;
		ang=atan2f(sinang, cosang);

		k0=sin((1.0-rt)*ang)*invsinang;
		k1=sin(rt*ang)*invsinang;
	}

	/* Interpolation */
	w=q0.w*k0+w1*k1;
	x=q0.x*k0+x1*k1;
	y=q0.y*k0+y1*k1;
	z=q0.z*k0+z1*k1;
}




	/*-----------------------------------------------
		Class E3D_Quatrix :: Functions
	-----------------------------------------------*/

/*---------------------------
       Constructor
---------------------------*/
E3D_Quatrix::E3D_Quatrix()
{
	q.identity();
	tx=ty=tz=0.0f;
	sx=sy=sz=1.0f;  /* HK2023-02-23 */
}


/*---------------------------
       Set as identity
---------------------------*/
void E3D_Quatrix::identity()
{
	q.identity();
	tx=ty=tz=0.0f;
	sx=sy=sz=1.0f;  /* HK2023-02-23 */
}

/*---------------------------
       Print
---------------------------*/
void E3D_Quatrix::print(const char *name) const
{

        printf("   <<< Quatrix '%s' >>>\n",name?name:"");
        printf("q[%f (%f, %f, %f)], t(%f,%f,%f), scale(%f,%f,%f).\n", q.w,q.x,q.y,q.z, tx,ty,tz, sx,sy,sz); /* HK2023-02-23 */

        //printf("Quatrix %s: q[%f (%f, %f, %f)], t(%f,%f,%f).\n", name?name:"", q.w,q.x,q.y,q.z, tx,ty,tz);
}

/*-------------------------
      inverse()
TODO: TO test
--------------------------*/
void  E3D_Quatrix::inverse()
{
	E3D_RTMatrix mat;

	toMatrix(mat);
	mat.inverse();
	fromMatrix(mat);
}

/*--------------------------
Get rotation angle in radian.
---------------------------*/
float E3D_Quatrix::getRotationAngle() const
{
	return 2.0f*acos(q.w);
}

/*--------------------------
Get rotation axis.
---------------------------*/
E3D_Vector E3D_Quatrix::getRotationAxis() const
{
	E3D_Vector axis(q.x, q.y, q.z);
	axis.normalize();

	return axis;
}

/*-------------------------------------
Set quaternion for rotation.
@chAxis: 'X','Y' or 'Z'  rotation axis.
@angle:  Angle in radian.
--------------------------------------*/
void E3D_Quatrix::setRotation(char chAxis, float angle)
{
	q.setRotation(chAxis, angle);
}

/*---------------------------------------------------------------
Set extrinsic rotation.
@axisToks:      Extrinsic rotation sequence Tokens(rotating axis), including:
                'X'('x') 'Y'('y') 'Z'('z')
                Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...

@ang[3]:        In radians, Rotation angles cooresponding to axisToks
------------------------------------------------------------------*/
void E3D_Quatrix::setExtriRotation(const char *axisToks, float ang[3])
{
	//q.identity();
	q.setExtriRotation(axisToks, ang);
}

/*---------------------------------------------------------------
Set intrinsic rotation.
@axisToks:      Intrinsic rotation sequence Tokens(rotating axis), including:
                'X'('x') 'Y'('y') 'Z'('z')
                Example: "ZYX", "XYZ", "XYX",.. with rotation sequence as 'Z->Y->X','X->Y-Z','X->Y->X',...

@ang[3]:        In radians, Rotation angles cooresponding to axisToks
------------------------------------------------------------------*/
void E3D_Quatrix::setIntriRotation(const char *axisToks, float ang[3])
{
	//q.identity();
	q.setIntriRotation(axisToks, ang);
}

/*-------------------------------------
Set translation component of the quatrix.
--------------------------------------*/
void E3D_Quatrix::setTranslation(float dx, float dy, float dz)
{
	tx=dx; ty=dy; tz=dz;
}


/*-------------------------------------
Convert quatrix to RTMatrix.
--------------------------------------*/
void E3D_Quatrix::toMatrix(E3D_RTMatrix &mat) const
{
	/* Rotation */
	q.toMatrix(mat);

	/* Translation */
	mat.pmat[9]=tx;
	mat.pmat[10]=ty;
	mat.pmat[11]=tz;

	/* Scale */
	E3D_RTMatrix smat;
	smat.setScaleXYZ(sx,sy,sz);

	/* Integrate scale into mat. sequence: SRT */
	mat=smat*mat;
}


/*-------------------------------------
Convert quatrix to RTMatrix.

rtmat: rotation+translation matrix.
smat: scale matrix.
--------------------------------------*/
void E3D_Quatrix::toMatrix(E3D_RTMatrix &rtmat, E3D_RTMatrix &smat) const
{
	/* Rotation */
	q.toMatrix(rtmat);

	/* Translation */
	rtmat.pmat[9]=tx;
	rtmat.pmat[10]=ty;
	rtmat.pmat[11]=tz;

	/* Scale */
	smat.identity();
	smat.pmat[0]=sx;
	smat.pmat[4]=sy;
	smat.pmat[8]=sz;
}

/*-----------------------------------------------
Convert RTMatrix to Quatrix

Note:
	!!!--- CAUTION ---!!!
1. Input tmat should a normalized orthogonal matrix!

   Input mat SHOULD not be integrated with scaleMat,
   otherwise, the result will be WRONG! also NOT unit quaternion.

   Example:
	0.000000  0.000000  0.327751
	0.327751  0.000000  0.000000
	0.000000  0.327751  0.000000
   Result:
	quat[0.500000 (-0.163876, -0.163876, -0.163876)]

2. sx,sy,sz KEEPs unchanged!

-------------------------------------------------*/
void E3D_Quatrix::fromMatrix(const E3D_RTMatrix &tmat)
{

#if 0 ///////   ONLY For uniform scaling: sx==sy==sz  ///////////
	E3D_RTMatrix tmat=mat; // <------- input as mat

	/* Scale */
	sx=sqrt(tmat.pmat[0]*tmat.pmat[0]+tmat.pmat[3]*tmat.pmat[3]+tmat.pmat[6]*tmat.pmat[6]);
	sy=sqrt(tmat.pmat[1]*tmat.pmat[1]+tmat.pmat[4]*tmat.pmat[4]+tmat.pmat[7]*tmat.pmat[7]);
	sz=sqrt(tmat.pmat[2]*tmat.pmat[2]+tmat.pmat[5]*tmat.pmat[5]+tmat.pmat[8]*tmat.pmat[8]);

	if(sx<VPRODUCT_NEARZERO || sy<VPRODUCT_NEARZERO || sz<VPRODUCT_NEARZERO) {
		egi_dpstd("Fails, sx/sy/sz nearZero!\n");
		return;
	}

	tmat.pmat[0] /= sx;
	tmat.pmat[3] /= sy;
	tmat.pmat[6] /= sz;

	tmat.pmat[1] /= sx;
	tmat.pmat[4] /= sy;
	tmat.pmat[7] /= sz;

	tmat.pmat[2] /= sx;
	tmat.pmat[5] /= sy;
	tmat.pmat[8] /= sz;
#endif ////////////////////////////////////////////////////////

	/* Rotation */
	q.fromMatrix(tmat);

	/* Translation */
	tx=tmat.pmat[9];
	ty=tmat.pmat[10];
	tz=tmat.pmat[11];
}

/*-------------------------------------
Interpolate between qt0 and qt2.
-------------------------------------*/
void E3D_Quatrix::interp(const E3D_Quatrix &qt0, const E3D_Quatrix &qt1, float rt)
{
	/* Slerp qt */
	q.slerp(qt0.q, qt1.q, rt);

	/* Interp txyz */
	tx=qt0.tx+(qt1.tx-qt0.tx)*rt;
	ty=qt0.ty+(qt1.ty-qt0.ty)*rt;
	tz=qt0.tz+(qt1.tz-qt0.tz)*rt;

	/* Interp Scale */
	sx=qt0.sx+(qt1.sx-qt0.sx)*rt;
	sy=qt0.sy+(qt1.sy-qt0.sy)*rt;
	sz=qt0.sz+(qt1.sz-qt0.sz)*rt;
}



/////////////////////////   Projection Matrix   /////////////////////////

void print_ProjMatrix(const E3DS_ProjMatrix &projMat)
{
	printf("   --- ProjMatrix ---\n");
	printf("type: %d\n", projMat.type);
	printf("winW/H: %d/%d\n", projMat.winW, projMat.winH);
	printf("dnear: %d; dfar: %d\n", projMat.dnear, projMat.dfar);
	printf("r,l,t,b:  %f,%f,%f,%f\n", projMat.r, projMat.l, projMat.t, projMat.b);
	printf("aa,bb,cc,dd:  %f,%f,%f,%f\n", projMat.aa, projMat.bb, projMat.cc, projMat.dd);
	printf("A,B,C,D:  %f,%f,%f,%f\n", projMat.A, projMat.B, projMat.C, projMat.D);
}

/*---------------------------------------------------------
Initialize an E3DS_ProjMatrix struct with given parameters.

----------------------------------------------------------*/
void E3D_InitProjMatrix(E3DS_ProjMatrix &projMatrix, int type, int winW, int winH, int dnear, int dfar, int dv)
{
	/* Init with input parameters */
	projMatrix.type  =type;
	projMatrix.winW  =winW;
	projMatrix.winH  =winH;
	projMatrix.dnear =dnear;
	projMatrix.dfar  =dfar;
	projMatrix.dv    =dv;

	/* Init as symmetrical screen for view frustum, for r,l,t,b */
	projMatrix.r  =-winW/2.0;
	projMatrix.l  =winW/2.0;
	projMatrix.t  =winH/2.0;
	projMatrix.b  =-winH/2.0;

	/* Clipping Matrix items for ISOMETRIC view frustum , for symmetrical screen */
//aa,bb,cc,dd:   aa=-1/r; bb=1/t; cc=2/(f-n); dd=(n+f)/(n-f);  HK2022-10-09
	projMatrix.aa =-1.0/projMatrix.r;
        projMatrix.bb =1.0/projMatrix.t;
        projMatrix.cc =2.0/((float)projMatrix.dfar-projMatrix.dnear);
        projMatrix.dd =1.0*((float)projMatrix.dnear+projMatrix.dfar)/((float)projMatrix.dnear-projMatrix.dfar);

	/* Clipping Matrix items for PERSPECTIVE view frustum , for symmetrical screen */
//A,B,C,D:   A=n/r; B=n/t; C=(f+n)/(f-n); D=-2fn/(f-n);
	projMatrix.A =1.0*projMatrix.dnear/projMatrix.r;
        projMatrix.B =1.0*projMatrix.dnear/projMatrix.t;
        projMatrix.C =((float)projMatrix.dfar+projMatrix.dnear)/((float)projMatrix.dfar-projMatrix.dnear);
        projMatrix.D =-2.0*projMatrix.dfar*projMatrix.dnear/((float)projMatrix.dfar-projMatrix.dnear);
}

/*---------------------------------------------------
Recompute Clipping matrix items A,B,C,D... if any
paramter(such as dv) changes.
--------------------------------------------------*/
void E3D_RecomputeProjMatrix(E3DS_ProjMatrix &projMatrix)
{
	/* Clipping Matrix items for ISOMETRIC view frustum , for symmetrical screen */
//aa,bb,cc,dd:   aa=-1/r; bb=1/t; cc=2/(f-n); dd=(n+f)/(n-f);  HK2022-10-09
	projMatrix.aa =-1.0/projMatrix.r;
        projMatrix.bb =1.0/projMatrix.t;
        projMatrix.cc =2.0/((float)projMatrix.dfar-projMatrix.dnear);
        projMatrix.dd =1.0*((float)projMatrix.dnear+projMatrix.dfar)/((float)projMatrix.dnear-projMatrix.dfar);


        /* Clipping Matrix items for PERSPECTIVE view frustum , for symmetrical screen */
//A,B,C,D:   A=n/r; B=n/t; C=(f+n)/(f-n); D=-2fn/(f-n);
        projMatrix.A =1.0*projMatrix.dnear/projMatrix.r;
        projMatrix.B =1.0*projMatrix.dnear/projMatrix.t;
        projMatrix.C =((float)projMatrix.dfar+projMatrix.dnear)/((float)projMatrix.dfar-projMatrix.dnear);
        projMatrix.D =-2.0*projMatrix.dfar*projMatrix.dnear/((float)projMatrix.dfar-projMatrix.dnear);

}
