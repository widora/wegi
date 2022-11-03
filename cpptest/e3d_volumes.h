/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Basic volumes

Midas Zhou
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#ifndef __E3D_VOLUMES_H__
#define __E3D_VOLUMES_H__

#include "e3d_trimesh.h"
#include "e3d_animate.h"

/*------------------------------------------
            Class E3D_Cuboid
    (Derived+Friend Class of E3D_TriMesh)
------------------------------------------*/
class E3D_Cuboid: public E3D_TriMesh
{
public:
	/* Constructor */
	E3D_Cuboid(float dx=10, float dy=10, float dz=10);

	/* Destructor */
	~E3D_Cuboid();

	float dx;	/* Length at X direction */
	float dy;	/* Length at Y direction */
	float dz;	/* Length at Z direction */
};

/*------------------------------------------
            Class E3D_Tetrahedron
    (Derived+Friend Class of E3D_TriMesh)

A tetrahedron has a regular triangle as base
,and the apex is right above the base centre.
------------------------------------------*/
class E3D_Tetrahedron: public E3D_TriMesh
{
public:
	/* Constructor */
	E3D_Tetrahedron(float s=10, float h=10);

	/* Destructor */
	~E3D_Tetrahedron();

	float side;	/* Side of the base triangle */
	float height;	/* Height from base square centre to the apex */
};


/*------------------------------------------
            Class E3D_Pyramid
    (Derived+Friend Class of E3D_TriMesh)
------------------------------------------*/
class E3D_Pyramid: public E3D_TriMesh
{
public:
	/* Constructor */
	E3D_Pyramid(float s=10, float h=10);

	/* Destructor */
	~E3D_Pyramid();

	float side;	/* Side of the base square */
	float height;	/* Height from base square centre to the apex */
};


/*------------------------------------------
            Class E3D_ABone
            A kind of bone bar.
    (Derived+Friend Class of E3D_TriMesh)
------------------------------------------*/
class E3D_ABone: public E3D_TriMesh
{
public:
	/* Constructor */
	E3D_ABone(float s=10, float l=60); /* Normally take s=(1/6~1/7)*l */

	/* Destructor */
	~E3D_ABone();

	float side;	/* Side of the base square */
	float len;	/* Lenght of the bone bar, Base located 1/10L to Big End, 9/10L to Small End. */
};


/*------------------------------------------
        Class E3D_RtaShpere
    (Derived+Friend Class of E3D_TriMesh)

Shpere derived from Regular Twenty Aspect
------------------------------------------*/
class E3D_RtaSphere: public E3D_TriMesh
{
public:
	/* Constructor */
	//E3D_RtaSphere(); Use default params as follows, OR it will cause ambiguous error in definition!
	E3D_RtaSphere(int ng=0, float rad=1.0);

	/* Destructor */
	~E3D_RtaSphere();

	int n;		/* Grade of rtashpere, 0,1,2... each is subdivided from the former one.
			 * n==0: Regular Twenty Aspect
			 * Limit to 5.
			*/

	float r;	/* radius */
};


/*------------------------------------------
            Class E3D_TestCompound
    (Derived+Friend Class of E3D_TriMesh)
------------------------------------------*/
class E3D_TestCompound: public E3D_TriMesh
{
public:
	/* Constructor */
	E3D_TestCompound(float s=10);

	/* Destructor */
	~E3D_TestCompound();

	float side;	/* Side of the base square */

//  	E3D_ABone  abone;
//     	E3D_RtaSphere  joint;
};

/*------------------------------------------
            Class E3D_TestSkeleton
    (Derived+Friend Class of E3D_TriMesh)
------------------------------------------*/
class E3D_TestSkeleton: public E3D_TriMesh
{
public:
	/* Constructor */
	//E3D_TestSkeleton(float s=10);
	E3D_TestSkeleton(E3D_BMatrixTree &bmtree);

	/* Destructor */
	~E3D_TestSkeleton();

	float side;	/* Side of the refere bone base square */
};


#endif
