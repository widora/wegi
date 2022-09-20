/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Basic volumes

Midas Zhou
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#ifndef __E3D_RTASPHERE_H__
#define __E3D_RTASPHERE_H__

#include "e3d_trimesh.h"

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
        Class E3D_RtaShpere
    (Derived+Friend Class of E3D_TriMesh)

Shpere derived from Regular Twenty Aspect
------------------------------------------*/
class E3D_RtaSphere: public E3D_TriMesh
{
public:
	/* Constructor */
	//E3D_RtaSphere(); Use default params as follows, OR it will cause ambiguous error in definition!
	E3D_RtaSphere(int n=0, float r=1.0);

	/* Destructor */
	~E3D_RtaSphere();

	int n;		/* Grade of rtashpere, 0,1,2... each is subdivided from the former one.
			 * n==0: Regular Twenty Aspect
			 * Limit to 5.
			*/

	float r;	/* radius */
};

#endif
