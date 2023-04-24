/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

              --- EGI 3D Scene and MeshInstance ---

Journal:
2023-02-09: Split codes from e3d_trimesh.h/.cpp
2023-02-15:
	1. Add E3D_Scene::loadGLTF().
2023-02-16:
	1. E3D_Scene::loadGLTF(): Proceeding... to 11.3.4.1
2023-02-17:
	1. E3D_Scene::loadGLTF(): Proceeding... uv for img kd
2023-02-20:
	1. E3D_Scene::loadGLTF(): uv for img_ke
2023-02-20:
	1. Add E3D_Scene::renderSceneNode()
	2. Add E3D_Scene::renderScene() For glScenes.
2023-02-21:
	1. E3D_Scene::loadGLTF(): Parse "animations" with E3D_glLoadAnimations()
2023-02-23:
	1. E3D_Scene::loadGLTF(): Load glAnimation[0] data into glNodes[node].animQts.
2023-02-28:
	1. E3D_Scene::loadGLTF() 12.1.10.2: Load glNodes[].animMorphwts
	2. E3D_Scene::loadGLTF() 11.3.4: Load position morph weights from primitie to trimesh.trigroup.morphs.positions.
2023-03-01:
	1. E3D_Scene::loadGLTF() 11.3.4: Load normal morph weights from primitie to trimesh.trigroup.morphs.normals.
	2. E3D_Scene::loadGLTF() 11.-1.4A: Reserve enough capacity for trimesh->mtlList before push_back operation.
2023-03-05:
	1. Add E3D_Scene::updateNodeGJmat() and E3D_Scene::updateNodeGJmats()
2023-03-08:
	1. E3D_Scene::loadGLTF() 9.: Load skin data into glNodes.IsJoint and glNodes.invbMat
2023-03-09:
	1. E3D_Scene::loadGLTF() 11.3.3A:  Assign TriGroupList[].skins
2023-03-14:
	1. Add param 'pmatON' for updateNodeGJmat(), updateNodeGJmats(), renderSceneNode(),  renderScene().
2023-03-21:
	1. Create E3D_Scene::loadGLB()
2023-03-22:
	1. Improve E3D_Scene::loadGLB():
2023-03-23:
	1. E3D_Scene:: Add memeber: tsmin (maybe <0.0)
2023-04-03:
	1. E3D_Scene::rebuildNodeInvbMats()
2023-04-06:
        1. Load all sets of animation data, for amat:
	   1.1 E3D_glNode::animQts is replaced by E3D_glNode::animQtsStacks.
	   1.2 Modify E3D_Scene::interpNodeAmats() accordingly.
	   1.3 Modify E3D_Scene::loadGLTF():12. and loadGLB():12. accordingly.
	2. Add E3D_Scene::setAnimation()
2023-04-07:
	1. Load all sets of animation data, for amwts:
	   1.1 E3D_glNode::animMorphwts is replaced by E3D_glNode::animMorphwtsStacks.
	   1.2 Modify E3D_Scene::interpNodeAmwts() accordingly.
	   1.3 Modify E3D_Scene::loadGLTF():12. and loadGLB():12. accordingly.
	   1.4 Modify E3D_Scene::setAnimation() accordingly.
2023-04-10:
	1. E3D_Scene::renderSceneNode(): Draw skeleton bone_lines.
	2. E3D_Scene::updateNodeGJmat()/updateNodeGJmats(): add param 'animON'.
2023-04-19: 1.E3D_Scene::loadGLB() 12.2: IF(glNodes[k].SRT_defined) use glNodes[k].qt!

Midas ZhouHK
知之者不如好之者好之者不如乐之者
-----------------------------------------------------------------*/
#include <string.h>
#include <vector>
#include <math.h>

#include "e3d_utils.h"
#include "e3d_scene.h"
#include "egi_bjp.h"


		 /*----------------------------------------------
       		        Class E3D_MeshInstance :: Functions
			      Instance of E3D_TriMesh.
		 ----------------------------------------------*/

/*-------------------------------------------
                The constructor.

         !!!--- CAUTION ---!!!
The referred triMesh SHOULD keep unchanged
(especially SHOULD NOT be rotated, and usually
its objmat is identity also),so its original XYZ
align with Global XYZ, as well as its AABB.
If NOT SO, you must proceed with it carefully.
--------------------------------------------*/
E3D_MeshInstance::E3D_MeshInstance(E3D_TriMesh &triMesh, string pname)
:name(pname), refTriMesh(triMesh)   /* Init order! */
{
	/* Assign name */
	//name=pname;

	/* Init instance objmat */
	objmat=refTriMesh.objmat;

	/* Init instance omats */
	for(unsigned int k=0; k<refTriMesh.triGroupList.size(); k++)
		omats.push_back(refTriMesh.triGroupList[k].omat);

	/* Increase counter */
	refTriMesh.instanceCount += 1;

	/* A default name, user can re-assign later */

	egi_dpstd("A MeshInstance is added!\n");
}

E3D_MeshInstance::E3D_MeshInstance(E3D_MeshInstance &refInstance, string pname)
:name(pname), refTriMesh(refInstance.refTriMesh)
{
	/* Assign name */
	//name=pname;

	/* Get refTriMesh */
	//refTriMesh=refInstance.refTriMesh; A reference member MUST be initialized in the initializer list.

	/* Init instance objmat */
	objmat=refInstance.objmat;

	/* Init instance omats */
	for(unsigned int k=0; k<refInstance.omats.size(); k++)
		omats.push_back(refInstance.omats[k]);

	/* Increase counter */
	refInstance.refTriMesh.instanceCount += 1;

	egi_dpstd("A MeshInstance is added!\n");
}

/*-----------------------------------------------------------------
Check whether AABB of the MeshInstance is out of the view frustum.

	      !!! --- CAUTION ---- !!!
It's assumed that AABB of the 'refTriMesh' are aligned
/parallel with XYZ axises.

TODO:
1. The MeshInstance is confirmed to be out of the frustum
   ONLY IF each vertex of its AABB is out of the SAME
   face of the frustum, However that is NOT necessary.
   Consider the case: a MeshInstance is near the corner of
   the frustum and is completely out of the frustum, vertices
   of its AABB may be NOT out of the SAME face of the furstum.

Return:
	True	Completely out of the view frustum.
	False   Ten to one it's in the view frustum.  see TODO_1.
-------------------------------------------------------------------*/
bool E3D_MeshInstance::aabbOutViewFrustum(const E3DS_ProjMatrix &projMatrix)
{
	E3D_Vector vpts[8];
	int code[8], precode;
	bool ret=true;

	/* Get 8 points of the AABB */
        float xsize=refTriMesh.aabbox.xSize();
        float ysize=refTriMesh.aabbox.ySize();
        float zsize=refTriMesh.aabbox.zSize();

        /* Get all vertices of the AABB: __ONLY_IF__ sides of AABB
	 * are all algined with XYZ axises.
	 */
        vpts[0]=refTriMesh.aabbox.vmin;
        vpts[1]=vpts[0]; vpts[1].x +=xsize;
        vpts[2]=vpts[1]; vpts[2].y +=ysize;
        vpts[3]=vpts[0]; vpts[3].y +=ysize;
        vpts[4]=vpts[0]; vpts[4].z +=zsize;
        vpts[5]=vpts[4]; vpts[5].x +=xsize;
        vpts[6]=vpts[5]; vpts[6].y +=ysize;
        vpts[7]=vpts[4]; vpts[7].y +=ysize;

	/* Transform as per MeshInstance.objmat, vpts updated. */
	E3D_transform_vectors(vpts, 8, objmat);

#if 0 /* TEST: ----- If OutOfViewFrustum --------- */
	E3D_Vector  pts[8];
	for(int k=0; k<8; k++) pts[k]=vpts[k];
#endif

	/* Map points to NDC coordinates, vpts updated. */
	mapPointsToNDC(vpts, 8, projMatrix);

	/* Check each vertex, see if they are beside and out of the SAME face of the frustum */
	 ////////////// Hi, Dude! :)  HK2022-10-10 //////////
	precode=0xFF;
	for(int k=0; k<8; k++) {
		code[k]=pointOutFrustumCode(vpts[k]);
		precode &= code[k];
	}
	if(precode) /* All aabb vertices out of a same side */
		ret=true;
	else
		ret=false;

	/* TODO: Further.... to check whether AABB intersects with frustum face planes.  */

#if 0 /* TEST: enable abov TEST also -------  If OutOfViewFrustum: Print pts coordinates and draw AABB ------- */
     if(ret==true) {
	egi_dpstd(" --- AABB OutViewFrustum ---\n");
	char strtmp[32];
	for(int k=0; k<8; k++) {
		sprintf(strtmp, "pts[%d]: ",k);
		pts[k].print(strtmp);
	}

        /* Project to viewPlan COORD. */
        projectPoints(pts, 8, projMatrix); /* Do NOT check clipping */

	FBDEV *fbdev=&gv_fb_dev;

        /* Notice: Viewing from z- --> z+ */
        fbdev->flipZ=true;

        /* E3D_draw lines 0-1, 1-2, 2-3, 3-0 */
        E3D_draw_line(fbdev, pts[0], pts[1]);
        E3D_draw_line(fbdev, pts[1], pts[2]);
        E3D_draw_line(fbdev, pts[2], pts[3]);
        E3D_draw_line(fbdev, pts[3], pts[0]);
        /* Draw lines 4-5, 5-6, 6-7, 7-4 */
        E3D_draw_line(fbdev, pts[4], pts[5]);
        E3D_draw_line(fbdev, pts[5], pts[6]);
        E3D_draw_line(fbdev, pts[6], pts[7]);
        E3D_draw_line(fbdev, pts[7], pts[4]);
        /* Draw lines 0-4, 1-5, 2-6, 3-7 */
        E3D_draw_line(fbdev, pts[0], pts[4]);
        E3D_draw_line(fbdev, pts[1], pts[5]);
        E3D_draw_line(fbdev, pts[2], pts[6]);
        E3D_draw_line(fbdev, pts[3], pts[7]);

        /* Reset flipZ */
        fbdev->flipZ=false;

     }
#endif

	return ret;
}


/*-----------------------------------------------------------------------
Check whether AABB of the MeshInstance is completely within the view frustum.

	      !!! --- CAUTION ---- !!!
It's assumed that AABB of the 'refTriMesh' are aligned
/parallel with XYZ axises.

Return:
	True	AABB completely within the view frustum.
	False
------------------------------------------------------------------------*/
bool E3D_MeshInstance::aabbInViewFrustum(const E3DS_ProjMatrix &projMatrix)
{
	E3D_Vector vpts[8];
	int code, precode;
	bool ret=true;

	/* Get 8 points of the AABB */
        float xsize=refTriMesh.aabbox.xSize();
        float ysize=refTriMesh.aabbox.ySize();
        float zsize=refTriMesh.aabbox.zSize();

        /* Get all vertices of the AABB: __ONLY_IF__ sides of AABB
	 * are all algined with XYZ axises.
	 */
        vpts[0]=refTriMesh.aabbox.vmin;
        vpts[1]=vpts[0]; vpts[1].x +=xsize;
        vpts[2]=vpts[1]; vpts[2].y +=ysize;
        vpts[3]=vpts[0]; vpts[3].y +=ysize;
        vpts[4]=vpts[0]; vpts[4].z +=zsize;
        vpts[5]=vpts[4]; vpts[5].x +=xsize;
        vpts[6]=vpts[5]; vpts[6].y +=ysize;
        vpts[7]=vpts[4]; vpts[7].y +=ysize;

	/* Transform as per MeshInstance.objmat, vpts updated. */
	E3D_transform_vectors(vpts, 8, objmat);

	/* Map points to NDC coordinates, vpts updated. */
	mapPointsToNDC(vpts, 8, projMatrix);

	/* Check each AABB vertex, see if they are all within the frustum */
	ret=true;
	for(int k=0; k<8; k++) {
		code=pointOutFrustumCode(vpts[k]);
		if(code) {
			ret=false;  /* Init ret=ture */
			break;
		}
	}

	return ret;
}


/*-------------------------
      The destructor.
--------------------------*/
E3D_MeshInstance:: ~E3D_MeshInstance()
{
	/* Decrease counter */
	refTriMesh.instanceCount -= 1;
}


/*--------------------------------------------------------------------------
Render MeshInstance.

Note:
1. MeshInstance.objmat prevails in renderInstance() by replacing it
   with this E3D_TRIMESH.objmat, and then calling E3D_TRIMESH::renderMesh().

TODO:
1. It's BAD to replace and restore refTriMesh.objmat.
   and this function SHOULD be a whole and complete rendering function instead
   of calling renderMesh().
2. E3D_MeshInstance.omats NOT applied yet.

@fbdev:	       Pointer to FBDEV
@projMatrix:   Projection Matrix.
---------------------------------------------------------------------------*/
void E3D_MeshInstance::renderInstance(FBDEV *fbdev, const E3DS_ProjMatrix &projMatrix) const
{
	/* Replace refTriMesh.objmat with  */
	E3D_RTMatrix tmpMat=refTriMesh.objmat; /* Rotation+Translation, !!! MAY include scale factors !!! */
//	E3D_RTMatrix tmpScaleMat=refTriMesh.objScaleMat; /* HK2023-02-19 */
	refTriMesh.objmat=this->objmat;
//	refTriMesh.objScaleMat=this->objScaleMat;

	/* TODO: E3D_MeshInstance.omats NOT applied yet. */

	/* Call renderTriMesh.*/
	refTriMesh.renderMesh(fbdev, projMatrix);

#if 1 /* TEST: ------ */
	//refTriMesh.drawAABB(fbdev, objScaleMat*objmat, projMatrix);
	refTriMesh.drawAABB(fbdev, objmat, projMatrix);
#endif

	/* Restore refTriMesh.obj */
	refTriMesh.objmat=tmpMat;
//	refTriMesh.objScaleMat=tmpScaleMat;
}



        	/*------------------------------------
            	    Class E3D_Scene :: Functions
        	-------------------------------------*/

/*-------------------------
      The constructor.
--------------------------*/
E3D_Scene::E3D_Scene()
{
      /* Init */
      tsmin=0.0f;
      tsmax=0.0f;
      meshBackFaceOn=false;
      skinMeshOff=false;
      skeletonLineOn=false;

      animIndex=0;
}

/*-------------------------
      The destructor.
--------------------------*/
E3D_Scene::~E3D_Scene()
{
	/* Release all imgbufs first */
	for(size_t i=0; i<imgbufs.size(); i++)
		egi_imgbuf_free2(&imgbufs[i]);

	/* Reset all mtlList[].img_x */
	for(size_t  i=0; i<triMeshList.size(); i++) {
	    for(size_t j=0; j<triMeshList[i]->mtlList.size(); j++) {
			triMeshList[i]->mtlList[j].img_kd=NULL;
			triMeshList[i]->mtlList[j].img_ke=NULL;
			triMeshList[i]->mtlList[j].img_kn=NULL;
	    }
	}


	/* delete mechanism will ensure ~E3D_TriMesh() to be called for each item.  */
	for(size_t i=0; i<meshInstanceList.size(); i++) {
	    delete meshInstanceList[i];
	}
	/* :>>> HK2023-02-16 */
	for(size_t  i=0; i<triMeshList.size(); i++) {
	    delete triMeshList[i];
	    //NOT necessary for ::fpathList[]
	}

}

/*----------------------------------------------
Import TriMesh from an .obj file, then push into
TriMeshList[].

@fobj:  Obj file path.
----------------------------------------------*/
void E3D_Scene::importObj(const char *fobj)
{
	E3D_TriMesh *trimesh = new E3D_TriMesh(fobj);

	/* fobj file may be corrupted. */
	if( trimesh->vtxCount() >0) {
		/* Push to triMeshList */
		triMeshList.push_back(trimesh);
		/* Push to fpathList */
		fpathList.push_back(fobj);

		/* Create an instance and push to meshInstanceList */
		E3D_MeshInstance *meshInstance = new E3D_MeshInstance(*trimesh, cstr_get_pathFileName(fobj));
		meshInstanceList.push_back(meshInstance);
	}
	else {
		egi_dpstd(DBG_RED"Fail to import obj file '%s'!\n"DBG_RESET, fobj);
		delete trimesh;
	}
}

/*-------------------------------------------------------------
Add mesh instance to the list.

    	     !!! --- CAUTION --- !!!

The refTriMesh OR refInstance MUST all in E3D_Scene::triMeshList
/meshInstanceList!
---------------------------------------------------------------*/
void E3D_Scene::addMeshInstance(E3D_TriMesh &refTriMesh, string pname)
{
	E3D_MeshInstance *meshInstance = new E3D_MeshInstance(refTriMesh, pname); /* refTriMesh counter increased */
	meshInstanceList.push_back(meshInstance);

}
void E3D_Scene::addMeshInstance(E3D_MeshInstance &refInstance, string pname)
{
	E3D_MeshInstance *meshInstance = new E3D_MeshInstance(refInstance, pname); /* refInstance counter increase */
	meshInstanceList.push_back(meshInstance);
}

/*--------------------------------------
Clear meshInstanceList[]
--------------------------------------*/
void E3D_Scene::clearMeshInstanceList()
{
	/* Clear list item. */
	for(unsigned int k=0; k<meshInstanceList.size(); k++) {
			/* Decreased instance counter */
			//meshInstanceList[k]->refTriMesh.instanceCount -=1;	/* NOPE! To decrease in ~E3D_MeshInstance() */

			/* Destroy E3D_MeshInstance */
			delete meshInstanceList[k];
	}

	/* Clear list */
	meshInstanceList.clear();
}


/*--------------------------------------------
Set animation for the E3D_Scene, and update
E3D_Scene::animIndex/tsmin/tsmax accordingly.

@index: index for all glNodes.animQtsStatcks[].
---------------------------------------------*/
void E3D_Scene::setAnimation(int index)
{
	if(index<0 || index>=animTotal) {
		egi_dpstd(DBG_RED"animIndex(%d)>=animTotal(%d)!\n"DBG_RESET,index, animTotal);
		return;
	}

	/* Update animIndex */
	animIndex=index;

	/* Update tsmin/tsmax */
	tsmin=tsmax=0.0f;
	for(size_t k=0; k<glNodes.size(); k++) {
		/** Traverse animQtsStacks[] tsmin/tsmax
		 *  Note:
		 *	1.glNodes[k].animQtsStacks[animIndex].ts MAY be empty!
		 */
     		if( glNodes[k].animQtsStacks[animIndex].tsmin <tsmin )
	 		tsmin=glNodes[k].animQtsStacks[animIndex].tsmin;
	     	if( glNodes[k].animQtsStacks[animIndex].tsmax >tsmax )
			tsmax=glNodes[k].animQtsStacks[animIndex].tsmax;

		/** Traverse animMorphwtsStacks[] tsmin/tsmax HK2023-04-07
		 *  Note:
		 *	1. glNodes[k].animQtsStacks[animIndex].ts MAY be empty!
		 */
     		if( glNodes[k].animMorphwtsStacks[animIndex].tsmin <tsmin )
	 		tsmin=glNodes[k].animMorphwtsStacks[animIndex].tsmin;
	     	if( glNodes[k].animMorphwtsStacks[animIndex].tsmax >tsmax )
			tsmax=glNodes[k].animMorphwtsStacks[animIndex].tsmax;
	}
}


/*-------------------------------------
To update value of each glNode.amat
by interpolating glNode.animQts.


//XXX @animIndex: Use E3D_Scene.animIndex.
@t:	    Time stamp.

TODO:
1. NOW only by slerp interpolation.
-------------------------------------*/
void E3D_Scene::interpNodeAmats(float t)
{
        E3D_Quatrix qt;

	/* Limit animIndex */
	if(animIndex<0) {
		egi_dpstd(DBG_RED"E3D_Scene::animIndex<0!\n"DBG_RESET);
		return;
	}
	else if(animIndex >= animTotal ) {
			egi_dpstd(DBG_RED"animIndex(%d) >= animTotal(%d)!\n"DBG_RESET, animIndex, animTotal);
			return;
	}

	/* Reset E3D_Scene::tsmin and tsmax */
        for(size_t k=0; k<glNodes.size(); k++) {
		if(glNodes[k].animQtsStacks.empty()) {
			egi_dpstd(DBG_RED"glNodes[%zu].animQtsStacks is empty!\n"DBG_RESET, k);
			continue;
		}
		else if(animIndex >=(int)glNodes[k].animQtsStacks.size() ) {
			egi_dpstd(DBG_RED"animIndex >= glNodes[%zu].animQtsStacks.size!\n"DBG_RESET, k);
			continue;
		}

                //qt.identity();  /* HK2023-04-03 */
                //glNodes[k].animQts.interp(t, qt); /* interpolate to get qt. */
                glNodes[k].animQtsStacks[animIndex].interp(t, qt); /* interpolate to get qt. */

		//qt.q.normalize(); /* XXX HK2023-04-03 */
                qt.toMatrix(glNodes[k].amat); /* qt to amat */

        }
}

/*-------------------------------------
To update value of each glNode.amwts
by interpolating glNode.animMorphwts.

@t:     Time statmp
@anim:  anim index, of glNodes[k].animMorphwtsArrya[]


-------------------------------------*/
void E3D_Scene::interpNodeAmwts(float t)
{
	/* Limit animIndex */
	if(animIndex<0) {
		egi_dpstd(DBG_RED"E3D_Scene::animIndex<0!\n"DBG_RESET);
		return;
	}
	else if(animIndex >= animTotal ) {
			egi_dpstd(DBG_RED"animIndex(%d) >= animTotal(%d)!\n"DBG_RESET, animIndex, animTotal);
			return;
	}

        for(size_t k=0; k<glNodes.size(); k++) {
		if(glNodes[k].animMorphwtsStacks.empty()) {
			egi_dpstd(DBG_RED"glNodes[%zu].animMorphwtsStacks is empty!\n"DBG_RESET, k);
			continue;
		}
		else if(animIndex >=(int)glNodes[k].animMorphwtsStacks.size() ) {
			egi_dpstd(DBG_RED"animIndex >= glNodes[%zu].animMorphwtsStacks.size!\n"DBG_RESET, k);
			continue;
		}

		/* Acting morph weights only for mesh node */
		if(glNodes[k].type==glNode_Mesh && glNodes[k].animMorphwtsStacks[animIndex].mmsize>0 ) {
			/* To interpolate amwts from animMorphwtsStacks[animIndex] */
			glNodes[k].animMorphwtsStacks[animIndex].interp(t, glNodes[k].amwts);
		}
        }
}


/*---------------------------------------------------------------------------
Re-compute glNodes[].invbMat and all its chirldren.
TODO: TEST!

		!!!--- CAUTION ---!!!
	  This is a recursive function!

Note:
1. Assume all mesh nodes and the root_join node have SAME direct father node.
2. !!! CAUTION !!! This SHOULD comply with init amat pose.

@node:		Node index of glNodes[].
@parentGmat:    Parent's complex TRS matrix, to pass down.
		Use _identityMat if no parent.

------------------------------------------------------------------------------*/
void E3D_Scene::rebuildNodeInvbMats(size_t node, const E3D_RTMatrix &parentGmat)
{
	E3D_RTMatrix tpmat;

	if(node>=glNodes.size())
		return;

	/* Update its invbMat */
	tpmat=glNodes[node].pmat*parentGmat;
	glNodes[node].invbMat=tpmat; //parentGmat; /* !!! NO tpmat !!! */
	glNodes[node].invbMat.inverse(); /* sustain tpmat! */

	/* Compute for its children */
	for(size_t k=0; k<glNodes[node].children.size(); k++) {

		if(glNodes[node].children[k]<0)
			continue;

		rebuildNodeInvbMats(glNodes[node].children[k], tpmat);
	}
}


/*-------------------------------------------------------------------
Render each meshInstacne in meshInstanceList[], and apply lights/settings.

Note:
		!!!--- CAUTION ---!!!
1. Before renderSecen, the caller SHOULD recompute clipping matrix
   items A,B,C,D...etc, in case any projMatrix parameter(such as dv)
   changed.

-------------------------------------------------------------------*/
void E3D_Scene::renderScene(FBDEV *fbdev, const E3DS_ProjMatrix &projMatrix) const
{
	int outcnt=0; /* Count instances out of the view frustum */

	if(meshInstanceList.size()<1) {
		egi_dpstd(DBG_RED"There's NO item in meshInstanceList[]!\n"DBG_RESET);
		return;
	}

	/* 1. Lights/setting preparation */

	/* 2. Recompute clipping matrix items A,B,C,D.. if any projMatrix parameter(such as dv) changes */
	//XXX E3D_RecomputeProjMatrix(projMatrix); XXX NOT here!

	/* 3. Render each instance in the list */
	for(unsigned int k=0; k< meshInstanceList.size(); k++) {
		/* Clip Instance, and exclude instances which are out of view frustrum. */
		if( meshInstanceList[k]->aabbOutViewFrustum(projMatrix) ) {
			outcnt ++;
			egi_dpstd(DBG_ORCHID"Instance '%s' is out of view frustum!\n"DBG_RESET, meshInstanceList[k]->name.c_str());

		   #if 0  /* TEST: -------- Assign COLOR_HotPink to any instanceout out of frustum and render it */
			E3D_Vector saveKd;
			/* Save kd */
			saveKd=meshInstanceList[k]->refTriMesh.defMaterial.kd;
			/* Change kd as mark */
			meshInstanceList[k]->refTriMesh.defMaterial.kd.vectorRGB(COLOR_HotPink);
			/* Render instance */
			meshInstanceList[k]->renderInstance(fbdev, projMatrix);
			/* Restore kd */
			meshInstanceList[k]->refTriMesh.defMaterial.kd=saveKd;
		   #endif
		}
		else
		{
			/* Render instances within the view frustum */
			meshInstanceList[k]->renderInstance(fbdev, projMatrix);
		}
	}

	egi_dpstd(DBG_BLUE"Total %d instances are out of view frustum!\n"DBG_RESET, outcnt);
}


/*-------------------------------------------------------------------
Render the node and it's chirdren nodes.

		!!!--- CAUTION ---!!!
	  This is a recursive function!



@fbdev:		Pointer to FBDEV
@parentGmat:     Parent's complex TRS matrix, to pass down.
		Use _identityMat if no parent.
		TODO: To cancel it, since all parent gmats are passed to skin_mesh's attached node.
		      and call updateNodeGJmats() before calling this function.
		      see 2.2.3:  refTriMesh->objmat=glNodes[node].gmat

@node:		Node index of glNodes[].
XXX@pmatON:     (see renderScene())
@animON:	Ture: if(skin_node), then let refTriMesh->objmat=_identityMat,
		      as the mesh is binded with joints and joint_nodes.jointMats carrys the transformation.
		False: if(skin_node), let refTriMesh->objmat=glNode.gmat, as the mesh is NOT binded with joints.

#shadeType:	Shade type.

Note:
1. Before renderSecen, the caller SHOULD recompute clipping matrix
   items A,B,C,D...etc, in case any projMatrix parameter(such as dv)
   changed.
2. This is a recursive function, so as to pass down gmat along branch.
3. Not thread safe. <------- TriMesh memeber set/reset.
4. Assume mesh_nodes and the root_joint have the SAME direct father node.
5. The inverseBindMatrice MAY integrated with scaleMat for the mesh!
-------------------------------------------------------------------*/
//void E3D_Scene::renderSceneNode(FBDEV *fbdev, const E3D_RTMatrix &parentGmat, int node, int shadeType)
void E3D_Scene::renderSceneNode(FBDEV *fbdev, const E3DS_ProjMatrix &_projMatrix, const E3D_RTMatrix &parentGmat,
							int node, bool animON, int shadeType)
{
	int meshIndex;
	E3D_RTMatrix tmpMat;  /* mesh obj matrix */
	vector<float> tmpWts; /* mesh morph weights */
	E3D_TriMesh *refTriMesh=NULL; /* Pointer to refernce TriMesh */

	if(fbdev==NULL)
		return;

	if(node<0 || node>=(int)glNodes.size() )
		return;
	if(triMeshList.size()<1) {
		egi_dpstd(DBG_RED"No item in scene.triMeshList!\n"DBG_RESET);
		return;
	}

	/* 1. Update glNode.gmat */
	//glNodes[node].gmat = glNodes[node].amat*(glNodes[node].pmat)*parentGmat; ---NOT HERE!
	//----> Note: Call updateNodeGJmats() before renderSceneNode(), see in renderScene()


#if 0 /* TEST: Hide a mesh node -------- */
	printf(">>>>>>>>>>>>>>>>>>>>\n");
	if(glNodes[node].meshIndex==26)
		return;
#endif


	/* 2. Render mesh_node or skin_node(as skined mesh)  */
	if(glNodes[node].type==glNode_Mesh || glNodes[node].type==glNode_Skin ) {  /* skin OR mesh */
	        egi_dpstd(DBG_MAGENTA" ---- Render node(%d) '%s' (shadType=%d) ----\n"DBG_RESET, node, glNodes[node].name.c_str(), shadeType);

		/* 2.1 Get meshIndex to triMeshList[] */
		//if( glNodes[node].skinIndex >=0)
		if(glNodes[node].type==glNode_Skin)
			meshIndex=glNodes[node].skinMeshIndex;
		else
			meshIndex=glNodes[node].meshIndex;

		/* Check meshIndex, in case NOT all glMeshes are imported to triMeshList when loadGLTF() */
		egi_dpstd(DBG_GREEN"Render scene.triMeshList[%d], triMeshList.size=%zu ...\n"DBG_RESET, meshIndex, triMeshList.size());
		if( meshIndex<0 || meshIndex>=(int)triMeshList.size() ) {
			egi_dpstd(DBG_RED"meshIndex is invalid! out of triMeshList[].\n"DBG_RESET);
			return;
		}

		/* 2.2 Render instance of triMeshList[meshIndex] */
		refTriMesh=triMeshList[meshIndex];
		if(refTriMesh!=NULL) {
			egi_dpstd(DBG_GREEN"Render scene.triMeshList[%d] ...\n"DBG_RESET, meshIndex);

			/* 2.2.1 Backup to tmpMat */
        		tmpMat=refTriMesh->objmat; /* Rotation+Translation+!!! MAY include scale factors !!! */
			/* 2.2.2 Backup to tmpWts */
			tmpWts=refTriMesh->mweights; /* morph weights for all mesh.trigroups */

			/* 2.2.3 Set params for refTriMesh: skinned mesh */
			if( glNodes[node].type==glNode_Skin) {
			    	if(animON) {
					/* 2.2.3.1 If mesh is skinned. gmat is already passed and integerated into joint_nodes.jointMat.
					 * A mesh CAN NOT belong/bind to a skin_node AND a mesh_node at same time!
					 */
			      		refTriMesh->objmat=_identityMat;

					//refTriMesh->pmat=glNodes[node].gmat;  /* TBD&TODO HK2023-03-28 */

					/* 2.2.3.2 Set skinON for rendering */
					refTriMesh->skinON=true;
					/* 2.2.3.3. Pass glNodes for skinning copmuation in renderMesh */
					refTriMesh->glNodes=&glNodes;
			    	}
				else { /* animOFF */
					//refTriMesh->pmat NO USE.
					refTriMesh->objmat=glNodes[node].gmat; /* when animOFF, gmat is from pmat ! */

#if 1 /* TEST: --------------------HK2023-04-11 */
					/* TODO: As inverseBindMatrice MAY integrated with scaleMat for the mesh! */
					E3D_RTMatrix smat;
					//smat.setScaleXYZ(50.0,50.0,50.0);
					smat.setScaleXYZ(100.0,100.0,100.0);
					refTriMesh->objmat = smat*refTriMesh->objmat;
#endif
				}
			}
			else  { /* glNodes[node].type==glNode_Mesh */

			      /* For mesh_node, (animation or init posture) transformation is passed from attached node. */
       		              refTriMesh->objmat=glNodes[node].gmat;
			}

			/* 2.2.4 For morphs, ALWAYS pass glNodes.amwts to E3D_TriMesh.mweights */
			refTriMesh->mweights=glNodes[node].amwts;

        		/* 2.2.6 Set shadeType */
		        if( shadeType<0 || shadeType>E3D_TEXTURE_MAPPING)
		        	refTriMesh->shadeType=E3D_TEXTURE_MAPPING;
			else
				refTriMesh->shadeType=(enum E3D_SHADE_TYPE)shadeType;

			/* 2.2.7 Set meshBackFaceOn */
			//if(meshBackFaceOn)
			//	refTriMesh->backFaceOn=true;
			refTriMesh->backFaceOn=meshBackFaceOn;

			/* 2.2.8 RenderMesh */
			if( !skinMeshOff || glNodes[node].type==glNode_Mesh ) {

egi_dpstd(DBG_YELLOW"Render TriMesh '%s'...\n"DBG_RESET, refTriMesh->name.c_str());
        			refTriMesh->renderMesh(fbdev, _projMatrix); //this->projMatrix);

#if 0 /* TEST: ------ */
			E3D_RTMatrix  identity;
        		refTriMesh->drawAABB(fbdev, identity, _projMatrix); //this->projMatrix);
#endif
			}

        		/* 2.2.9 Restore refTriMesh.obj */
        		refTriMesh->objmat=tmpMat;
			//refTriMesh->mweights.resize(0);
			refTriMesh->mweights=tmpWts; /* morph weights for all mesh.trigroups */

			/* 2.2.10 Reset skinON and glNodes, ONLY for animation. */
			// if(glNodes[node].type==glNode_Skin && animON ) {
                                refTriMesh->skinON=false;
				refTriMesh->glNodes=NULL;
			// }
		}
	}

	/* 3. Draw skeleton bone_lines from the joint to its children HK2023-04-10 -----------------
	 *    Note:
	 *	  1. The end node of a skeleton MAY NOT be a joint_node!(see tiger), which depends on GLTF/GLB generator.
	 *	  		!!! --- CAUTION --- !!!
	 *	  2. If E3D lines out of view-frustum, it will NOT be trimmed, then the result image may be distorted!
	 */
	if( skeletonLineOn && glNodes[node].IsJoint) {

		/* Save flipZ */
		bool saveFlipZ=fbdev->flipZ;

		E3D_Vector vpts[2];
		E3D_Vector vptc;

		/* 3.1 Turn_off flipZ, to let INT_MAT at top. Notice: Viewing from z- --> z+ */
		fbdev->flipZ=false;

		/* 3.2 Set color for bone lines */
		fbset_color2(fbdev,WEGI_COLOR_YELLOW);

		/* Node Origin point (under global coord.) */
		vpts[0].x=glNodes[node].gmat.pmat[9];
		vpts[0].y=glNodes[node].gmat.pmat[10];
		vpts[0].z=glNodes[node].gmat.pmat[11];

		/* Draw Origin of the node */

                /* 3.3.1 Project node Origin */
		vptc=vpts[0];
                if( projectPoints(&vptc, 1, _projMatrix)!=0 ) {
                      //egi_dpstd("ProjectPoints error=%d!\n", err);
               	      //return;
       	        }

		/* 3.3.2 Aft projectPoints: Bring on top, as filpZ=false XXX to 3.3.3 */
		//fbdev->pixz=INT_MAX;

   		/* TEST : TODO use glSkin.sketelon ------------- */
		/* 3.3.3 Set color for hips/pelvis/rootJoint */
		if( strstr(glNodes[node].name.c_str(),"hips") || strstr(glNodes[node].name.c_str(),"Hips")
		   || strstr(glNodes[node].name.c_str(),"pelvis") || strstr(glNodes[node].name.c_str(),"Pelvis")
		   || strstr(glNodes[node].name.c_str(),"rootJoint") ) {

			if(strstr(glNodes[node].name.c_str(),"rootJoint"))
				fbset_color2(fbdev,WEGI_COLOR_RED);
			else
				fbset_color2(fbdev,WEGI_COLOR_GREEN);

			fbdev->pixz=INT_MAX;  /* Aft projectPoints: Bring on top, as filpZ=false */
		}
		else {
			fbset_color2(fbdev,WEGI_COLOR_YELLOW);
			fbdev->pixz=INT_MAX-100;  /* Aft projectPoints: Bring on top, as filpZ=false */
		}

		/* 3.3.4 Draw dot as the joint Origin */
		draw_filled_circle(fbdev, roundf(vptc.x), roundf(vptc.y), 2);

		/* 3.3.5 Set color for bone lines */
		fbset_color2(fbdev,WEGI_COLOR_YELLOW);

		/* 3.3.6 Draw link lines to children */
		for(size_t k=0; k<glNodes[node].children.size(); k++) {

			/* DO NOT draw link lines from the rootJoint, for it seems too funny! :>>>  */
			if(strstr(glNodes[node].name.c_str(),"rootJoint"))
				break;

			/* Check index */
			if(glNodes[node].children[k]<0 || glNodes[node].children[k]>=(int)glNodes.size() )
				continue;

			/* DO NOT draw link line if chirld is NOT joint */
			if(!glNodes[glNodes[node].children[k]].IsJoint)
				continue;

			/* Get child Origin XYZ. notice that vpts[] will be modified after projectPoint() */
			vpts[0].x=glNodes[node].gmat.pmat[9];
			vpts[0].y=glNodes[node].gmat.pmat[10];
			vpts[0].z=glNodes[node].gmat.pmat[11];

			vpts[1].x=glNodes[glNodes[node].children[k]].gmat.pmat[9];
			vpts[1].y=glNodes[glNodes[node].children[k]].gmat.pmat[10];
			vpts[1].z=glNodes[glNodes[node].children[k]].gmat.pmat[11];

	                /* Project line to camera/screen coord. NOTE: vpts will be modified! */
	                if( projectPoints(vpts, 2, _projMatrix)!=0 ) {
	                      //egi_dpstd("ProjectPoints error=%d!\n", err);
                	      //return;
        	        }

			/* Aft projectPoints: Bring it on top BUT bellow rootJoint point, as flipZ=false. */
		 	//vpts[0].z=(float)(INT_MAX-50); vpts[1].z=(float)(INT_MAX-50);
			/* Note: -50 is NOT enough, float accuracy lost makes z=2147483648.0, while INT_MAX=2147483647! */
			vpts[0].z=(float)(INT_MAX-100); vpts[1].z=(float)(INT_MAX-100);  /* Ok, float z=2147483520.0<INT_MAX */

			/* Draw bone line */
			//draw_line(fbdev, vpts[0].x, vpts[0].y, vpts[1].x, vpts[1].y);
			E3D_draw_line(fbdev, vpts[0], vpts[1]); /* apply pixZ */
		}

		/* Restore fbdev->flipZ */
	        fbdev->flipZ=saveFlipZ;
	}  /* Finish draw skeleton bone-lines */


	/* 4. Render its children recursively. */
	for(size_t k=0; k<glNodes[node].children.size(); k++) {
		// if(glNodes[node].children[k]<0)  /* OK, Check at start of renderSceneNode()  */
		//	continue;

		renderSceneNode(fbdev, _projMatrix, glNodes[node].gmat, glNodes[node].children[k], animON, shadeType);
	}
}


/*-------------------------------------------------------------------
Render all THIS.glScenes[scene].rootNodes(and their children).

@fbdev:         Pointer to FBDEV
@parentMat:     Parent's complex matrix, to pass down.
@scene:         Scene index of glScens[].
@pmatON:	Ture: To apply pmat in gmat: gmat=amat*pmat*parentGmat
		      Example: mesh is NOT set at init posture. (animMats are acting on init posture)
		False: Not apply pmat in gmat: gmat=amat*parentGmat.
		      Example: mesh is already set at init posture. so as pmat=identity.

@animON:	Ture: if(skin_node), then let refTriMesh->objmat=_identityMat,
		      as the mesh is binded with joints and joint_nodes.jointMats carrys the transformation.
		False: if(skin_node), let refTriMesh->objmat=glNode.gmat, as the mesh is NOT binded with joints.
#shadeType:     Shade type.

Note:
1. Before renderSecen, the caller SHOULD recompute clipping matrix
   items A,B,C,D...etc, in case any projMatrix parameter(such as dv)
   changed.
2. This is a recursive function, so as to pass down gmat along branch.
3. Not thread safe. <------- TriMesh memeber set/reset.

-------------------------------------------------------------------*/
void E3D_Scene::renderScene(FBDEV *fbdev, const E3DS_ProjMatrix &_projMatrix, const E3D_RTMatrix &parentGmat,
				int scene, bool pmatON, bool animON, int shadeType)
{
	int meshIndex;
	E3D_RTMatrix tmpMat;
	E3D_TriMesh *refTriMesh=NULL; /* Pointer to refernce TriMesh */

	if(fbdev==NULL)
		return;

	if(scene<0 || scene>=(int)glScenes.size() )
		return;
	if( glScenes.size()==0 ) {
		egi_dpstd(DBG_RED"No scene defined!\n"DBG_RESET);
		return;
	}

	/* 1 .Update all GJmats(gmat+jointMat) of glNnodes in the scenes[scene]. HK2023-03-05 */
	updateNodeGJmats(scene, pmatON, animON, parentGmat);

	egi_dpstd(DBG_MAGENTA" ---- Render scene(%d) '%s' (shadType=%d), projMat.type=%d ----\n"DBG_RESET,
				scene, glScenes[scene].name.c_str(), shadeType, _projMatrix.type);

	/* 2. Render each root node(and its children) in the scene */
	for(size_t k=0; k<glScenes[scene].rootNodeIndices.size(); k++) {
		if(glScenes[scene].rootNodeIndices[k]<0 || glScenes[scene].rootNodeIndices[k] >=(int)glNodes.size() )
			continue;

		/* Note: parentGmat not used actually in renderSceneNode() */
		renderSceneNode(fbdev, _projMatrix, parentGmat, glScenes[scene].rootNodeIndices[k], animON, shadeType);
	}

}


/*---------------------------------------------------
Update  GJmat(gmat+jointMat) of a glNode.

(Update jointMat only if the node is a joint).

Node:
1. Assume glNode.amat at t=0.0f is same as glNode.pmat. ???

		!!!--- CAUTION ---!!!
	  This is a recursive function!

@node:		Node index of glNodes[].
@pmatON:	Ture: For initial posture. ---NOPE! see animON
		      To apply pmat in gmat: gmat=amat*pmat*parentGmat(USUALLY amat is identity)
		False: For animation.
		      gmat=amat*parentGmat;

@parentMat:     Parent's complex matrix, to pass down.
@animON:	Ture: apply gmat=amat*parentGmat for all nodes.
		False: apply gmat=pmat*parentGmat for all nodes.

TODO:
XXX 1. For skinning animation, pmatON=false? BUT simple-skin ....

----------------------------------------------------*/
void E3D_Scene::updateNodeGJmat(size_t node, bool pmatON, bool animON, const E3D_RTMatrix &parentGmat)
{
	if( node>=glNodes.size() ) /* include node==0, glNodes.size()==0 */
		return;

   /* 1. Update glNode.gmat */
   if(animON) {

	/* 1.1 In rare case, Usually DO NOT turn on pmatON. */
	if(pmatON && glNodes[node].IsJoint ) {
		if(glNodes[node].animQtsStacks.size()<1 || glNodes[node].animQtsStacks[animIndex].qts.size()<1)
			glNodes[node].gmat = glNodes[node].pmat*parentGmat; /* Note, that pmat may also ingegrated with scaleMat */
		else
			glNodes[node].gmat = glNodes[node].amat*glNodes[node].pmat*parentGmat;

	}
	/* 1.2 Usual case */
	else {

	    /* 1.2.1 For joints */
	    if(glNodes[node].IsJoint) { // && !strstr(glNodes[node].name.c_str(), "rootJoint") ) {
		/* Never happens, see loadGLB(): 12.2 If animQtsStacks[].qts is empty, insert qt at 0.0f!
		 * !!!except that glNodes[k].SRT_defined==false, then pmat(may with ScaleMat) WILL NOT convert to qt
		   --- CROSS-CHECK --- loadGLB/GLTF():12.2.2   HK2023-04-21
		 */
		if( glNodes[node].animQtsStacks.size()<1 || glNodes[node].animQtsStacks[animIndex].qts.size()<1)
			glNodes[node].gmat = glNodes[node].pmat*parentGmat; /* Note, that pmat may also ingegrated with scaleMat */
		else
		        glNodes[node].gmat = glNodes[node].amat*parentGmat;  /* Ignore pmat. parentGmat pass down from renderScene(parentGmat),
								    		and will bind a mesh to gmat. */
	    }
	    /* 1.2.2 For non_joint nodes and the rootJoint */
	    else { /* NOT joint */
		/*  --- also CROSS-CHECK loadGLB/GLTF().12.2 --- */
		/* Never happens if loadGLB():12.2 applys. .. If animQtsStacks[].qts is empty, insert qt at 0.0f!
		 * !!!except that glNodes[k].SRT_defined==false, then pmat(may with ScaleMat) WILL NOT convert to qt
		   --- CROSS-CHECK --- loadGLB/GLTF():12.2.2   HK2023-04-21
		 */
		if( glNodes[node].animQtsStacks.size()<1 || glNodes[node].animQtsStacks[animIndex].qts.size()<1)
			glNodes[node].gmat = glNodes[node].pmat*parentGmat; /* Note, that pmat may also ingegrated with scaleMat */
		else
			glNodes[node].gmat = glNodes[node].amat*parentGmat;
	    }
	}

	/* 2. Update glNode.jointMat */
	if(glNodes[node].IsJoint)  {
	     /* NOTE: "Bind Shape Matrix" is assumed either premultiplied with the mesh data, or postmultiplied to the invbBindMatrices"
	      *       invbMat may also ingegrated with scaleMat.
	      */
	     glNodes[node].jointMat=glNodes[node].invbMat*glNodes[node].gmat;
	}
   }
   else { /* animOFF */
	glNodes[node].gmat = glNodes[node].pmat*parentGmat; /* Note, that pmat may also ingegrated with scaleMat */

   }


#if 0 /* TEST:---- */
	egi_dpstd(DBG_GREEN"glNodes[%d].jointMat: \n"DBG_RESET, node);
	glNodes[node].jointMat.print();
#endif

	/* 3. Apply to node's children */
	for(size_t k=0; k<glNodes[node].children.size(); k++) {
		/* Invalid node index */
		if(glNodes[node].children[k]<0 || glNodes[node].children[k] >= (int)glNodes.size())
			continue;

		updateNodeGJmat(glNodes[node].children[k], pmatON, animON, glNodes[node].gmat);
	}
}


/*-------------------------------------------------
Update  GJmat(gmat+jointMat) for all nodes
in THIS.scenes[scene].

(Update jointMat only if the node is a joint).

@scene:         Scene index of glScens[].
@pmatON:	Ture: For initial posture.
		      To apply pmat in gmat: gmat=amat*pmat*parentGmat
		False: For animation.
		      gmat=amat*parentGmat;
@animON:	Ture: apply gmat=amat*parentGmat for all nodes.
		False: apply gmat=pmat*parentGmat for all nodes.
@parentMat:     Parent's complex matrix, to pass down.
		Use _identityMat if no parent.
----------------------------------------------------*/
void E3D_Scene::updateNodeGJmats(size_t scene, bool pmatON, bool animON, const E3D_RTMatrix &parentGmat)
{

	if( glScenes.size()==0) {
		egi_dpstd(DBG_RED"No scene defined!\n"DBG_RESET);
		return;
	}
	else if (scene>=glScenes.size() )
		return;

	for(size_t k=0; k<glScenes[scene].rootNodeIndices.size(); k++) {
		/* Invalid index */
		if(glScenes[scene].rootNodeIndices[k]<0 || glScenes[scene].rootNodeIndices[k]>= (int)glNodes.size() )
			continue;

		updateNodeGJmat(glScenes[scene].rootNodeIndices[k], pmatON, animON, parentGmat); /* Recursive */
	}
}


/*--------------------------------------------------
Load glTF into the scene.

		!!! --- CAUTION --- !!!
      	     THIS E3D_Scene MUST be empty!


Note:
1. Each glTF mesh is transformed into one item of triMeshList[].
   primitives of mesh are parsed as triGroup in the mesh.
   If a mesh has more than one primitve, then their corresponding
   triGroup name are mesh_name+number.
   Example: mesh name: "BOX",
   	mesh's primitie[0] triGroup name: "BOX_0"
   	mesh's primitie[1] triGroup name: "BOX_1"

            !!! --- CAUTION --- !!!
2. Reset all glImages[].imgbuf to NULL, assume they are all
   referred by scene->triMeshList[]. and Ownership transfers to
   the E3D_Scene.
3. glTF set normal values at vtxList[j].normal, on vertex, not triangle.
4. glTF set texture uv at vtxList[].u/v, while E3D_TriMesh::renderMesh() reads UV from triList[].vtx[].u/v, see 11.3.4.2.1.2.4.

@fgl:	Path to a glTF Json file.

TODO:
1. NOW For LittleEndian system ONLY, same as glTF byte order.
2. NOW byteStride is NOT considered during data extracting. (Except to see .S1.1.3 at 11.3.3A)
3. XXX NOW each glMaterial applys to ONLY one trimesh, if more than
   1 trimesh use the same glMaterial, then the later holds
   NULL imgbufs, as imgbuf Ownership transfers to the first
   trimesh only.   (see 11.3.4.1)
   ---OK, there's workaround: let more than 2 trimesh holds
  same igmbuf pointers, before E3D_Scene.

Return:
	0	OK
	<0	Fails
--------------------------------------------------*/
int E3D_Scene::loadGLTF(const string & fgl)
{
	int ret=0;
	char *pjson;
	string key;
	string strValue;

        vector<E3D_glBuffer> glBuffers;
        vector<E3D_glBufferView> glBufferViews;
        vector<E3D_glAccessor> glAccessors;
        vector<E3D_glMesh> glMeshes;
	vector<E3D_glMaterial> glMaterials;
	vector<E3D_glTexture> glTextures;
	vector<E3D_glImage> glImages;
	vector<E3D_glSampler> glSamplers;

	vector<E3D_glSkin> glSkins;
	vector<E3D_glAnimation> glAnimations;

	/* --- E3D_Scene Memebers --- */
//	vector<E3D_glNode> glNodes;
//	vector<E3D_glScene> glScenes;

	int triCount=0; /* Count total triangles */
	int vtxCount=0; /* Count total vertices */
	int tgCount=0;  /* Count total triGroup */

	int triCountBk,vtxCountBk,tgCountBk;

	/* Check whether file exists */
	if( access(fgl.c_str(), F_OK) !=0 ) {
		egi_dpstd(DBG_RED"File '%s' does NOT exist!\n"DBG_RESET, fgl.c_str());
		return -1;
	}

	/* 0. Check to ensure THIS is an empty E3D_Scene */
	if(triMeshList.size()>0 || meshInstanceList.size()>0 ) {
		egi_dpstd(DBG_RED"THIS E3D_Scene must be empty, triMeshList/meshInstanceList is NOT empty!\n"DBG_RESET);
		return -1;
	}
	if(glNodes.size()>0 || glScenes.size()>0) {
		egi_dpstd(DBG_RED"THIS E3D_Scene must be empty, glNodes/glScene is NOT empty!\n"DBG_RESET);
                return -1;
	}


////////////////// Load glTF file to glBuffers/glBufferViews/glAccessors/glMeshes /////////////

	/* 1. Mmap glTF Json file */
	EGI_FILEMMAP *fmap;
	fmap=egi_fmap_create(fgl.c_str(), 0, PROT_READ, MAP_SHARED);
	if(fmap==NULL)
		return -1;

	/* 2. Parse Top_Level glTF Jobject arrays, and load to respective glObjects */
	pjson=fmap->fp;
	while( (pjson=E3D_glReadJsonKey(pjson, key, strValue)) ) {

		//cout << DBG_GREEN"Key: "<< key << DBG_RESET << endl;
		//cout << DBG_GREEN"Value: "DBG_RESET<< strValue << endl;

		/* Case glTF Jobject: buffers */
		if(!key.compare("buffers")) {
			printf("\n=== buffers ===\n");

			/* Load glBuffers as per Json descript */
			if( E3D_glLoadBuffers(strValue, glBuffers)<0 )
				return -1;

			egi_dpstd(DBG_GREEN"Totally %zu E3D_glBuffers loaded.\n"DBG_RESET, glBuffers.size());
			char strtmp[128];
			size_t strlen;
			for(size_t k=0; k<glBuffers.size(); k++) {
				strlen=glBuffers[k].uri.copy(strtmp,64,0);
				strtmp[strlen]='\0';
				if(glBuffers[k].uri.size()>64) 	strcpy(strtmp+64, "...");
				printf("glBuffers[%zu]: byteLength=%d, uri='%s'\n", k, glBuffers[k].byteLength, strtmp);
				/* Check buffer data */
				if(glBuffers[k].byteLength>0 && glBuffers[k].data==NULL) {
					egi_dpstd(DBG_GREEN"glBuffers[%zu].data==NULL, while byteLength>0!\n"DBG_RESET, k);
					return -1;
				}
			}
		}
		/* Case glTF Jobject: bufferViews */
		else if(!key.compare("bufferViews")) {
			printf("\n=== bufferViews ===\n");

			/* Load data to glBufferViews */
			if( E3D_glLoadBufferViews(strValue, glBufferViews)<0 )
				return -1;

			printf(DBG_YELLOW"Totally %zu E3D_glBufferViews loaded.\n"DBG_RESET, glBufferViews.size());
			for(size_t k=0; k<glBufferViews.size(); k++) {
			    printf("glBufferViews[%zu]: name='%s', bufferIndex=%d, byteOffset=%d, byteLength=%d, byteStride=%d, target=%d.\n",
				k, glBufferViews[k].name.c_str(),
				glBufferViews[k].bufferIndex, glBufferViews[k].byteOffset, glBufferViews[k].byteLength,
				glBufferViews[k].byteStride, glBufferViews[k].target);
			}
		}
		/* Case glTF Jobject: accessors */
		else if(!key.compare("accessors")) {
			printf("\n=== accessors ===\n");

			/* Load data to glAccessors */
			if( E3D_glLoadAccessors(strValue, glAccessors)<0 )
				return -1;

#if 0 /////////////  See 4a. ///////////////////////////////////
			printf(DBG_YELLOW"Totally %zu E3D_glAccessors loaded.\n"DBG_RESET, glAccessors.size());
			for(size_t k=0; k<glAccessors.size(); k++) {
		    printf("glAccessors[%zu]: name='%s', type='%s', count=%d, bufferViewIndex=%d, byteOffset=%d, componentType=%d, normalized=%s.\n",
				     k, glAccessors[k].name.c_str(), glAccessors[k].type.c_str(), glAccessors[k].count,
				     glAccessors[k].bufferViewIndex, glAccessors[k].byteOffset, glAccessors[k].componentType,
				     glAccessors[k].normalized?"Yes":"No" );
data NOT ok...	   printf(" byteStride=%d\n", glBufferViews[glAccessors[k].bufferViewIndex].byteStride);
			}
#endif /////////////////////////////////////////////////
		}
		/* Case glTF Jobject: meshes */
		else if(!key.compare("meshes")) {
			printf("\n=== meshes ===\n");

			/* Load data to glMeshes */
			if( E3D_glLoadMeshes(strValue, glMeshes)<0 )
				return -1;

			printf(DBG_GREEN"Totally %zu E3D_glMeshes loaded.\n"DBG_RESET, glMeshes.size());
			for(size_t k=0; k<glMeshes.size(); k++) {
				printf(DBG_MAGENTA"\n    --- mesh_%zu ---\n"DBG_RESET, k);
				printf("name: %s\n",glMeshes[k].name.c_str());
				printf("mweight.size: %zu\n", glMeshes[k].mweights.size());
				for(size_t j=0; j<glMeshes[k].primitives.size(); j++) {
					printf(DBG_MAGENTA"\n     (primitive_%zu)\n"DBG_RESET, j);
					int mode =glMeshes[k].primitives[j].mode;
					printf("mode:%d for '%s' \n",mode, (mode>=0&&mode<8)?glPrimitiveModeName[mode].c_str():"Unknown");
					printf("indices(AccIndex):%d\n",glMeshes[k].primitives[j].indicesAccIndex);
					printf("material(Index):%d\n",glMeshes[k].primitives[j].materialIndex);

					/* attributes */
					printf("Attributes:\n");
					printf("   POSITION(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.positionAccIndex);
					printf("   NORMAL(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.normalAccIndex);
					printf("   TANGENT(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.tangentAccIndex);
				     for(int in=0; in<PRIMITIVE_ATTR_MAX_TEXCOORD; in++) {
					if(glMeshes[k].primitives[j].attributes.texcoord[in]>-1)
					   printf("   TEXCOORD_%d=(AccIndex)%d\n",in, glMeshes[k].primitives[j].attributes.texcoord[in]);
				     }
				     for(int in=0; in<PRIMITIVE_ATTR_MAX_COLOR; in++) {
					if(glMeshes[k].primitives[j].attributes.color[in]>-1)
					   printf("   COLOR_%d=(AccIndex)%d\n",in, glMeshes[k].primitives[j].attributes.color[in]);
				     }
					printf("   (skin)JOINTS_0(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.joints_0);
					printf("   (skin)WEIGHTS_0(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.weights_0);

					/* morph targets HK2023-02-27 */
					printf("Morph targets: %s\n",glMeshes[k].primitives[j].morphs.size()?"":"None");
				     for(size_t in=0; in<glMeshes[k].primitives[j].morphs.size(); in++) {
					printf("      --- morphs[%zu] ---\n", in);
					printf("      POSITION(AccIndex) %d\n", glMeshes[k].primitives[j].morphs[in].positionAccIndex);
					printf("      NORMAL(AccIndex) %d\n", glMeshes[k].primitives[j].morphs[in].normalAccIndex);
					printf("      TANGENT/TEXCOORD/COLOR todo\n");
				     }
				}
			}
		}
		/* Case glTF Jobject: materials */
		else if(!key.compare("materials")) {
			printf("\n=== materials ===\n");

			/* Load data to glMeshes */
			if( E3D_glLoadMaterials(strValue, glMaterials)<0 )
				return -1;

			printf(DBG_GREEN"Totally %zu E3D_glMaterials loaded.\n"DBG_RESET, glMaterials.size());

			for(size_t k=0; k<glMaterials.size(); k++) {
				printf(DBG_MAGENTA"\n    --- material_%zu ---\n"DBG_RESET, k);
				printf("name: %s\n",glMaterials[k].name.c_str());
				printf("doubleSided: %s\n", glMaterials[k].doubleSided ? "True" : "False");
				printf("alphaMode: %s\n", glMaterials[k].alphaMode.c_str());
				printf("alphaCutoff: %f\n", glMaterials[k].alphaCutoff);
				printf("emissiveFactor: [%f,%f,%f]\n",
						glMaterials[k].emissiveFactor.x,
						glMaterials[k].emissiveFactor.y,
						glMaterials[k].emissiveFactor.z
				);
				printf("pbrMetallicRoughness:\n");
				printf("    baseColorFactor: [%f,%f,%f,%f]\n",
						glMaterials[k].pbrMetallicRoughness.baseColorFactor.x,
						glMaterials[k].pbrMetallicRoughness.baseColorFactor.y,
						glMaterials[k].pbrMetallicRoughness.baseColorFactor.z,
						glMaterials[k].pbrMetallicRoughness.baseColorFactor.w
				);
				printf("    baseColorTexture:\n");
				printf("         index:%d,  texCoord:%d\n",
						glMaterials[k].pbrMetallicRoughness.baseColorTexture.index,
						glMaterials[k].pbrMetallicRoughness.baseColorTexture.texCoord
				);
				printf("    metallicFactor: %f\n",glMaterials[k].pbrMetallicRoughness.metallicFactor);
				printf("    roughnessFactor: %f\n",glMaterials[k].pbrMetallicRoughness.roughnessFactor);
				printf("    metallicRoughnessTexture: TODO\n");

				printf("emissiveTexture: %s\n", glMaterials[k].emissiveTexture.index>-1?"":"None");
				if(glMaterials[k].emissiveTexture.index>-1) {
					printf("         index:%d,  texCoord:%d\n",
						glMaterials[k].emissiveTexture.index,
						glMaterials[k].emissiveTexture.texCoord
					);
				}

				printf("normalTexture: %s\n", glMaterials[k].normalTexture.index>-1?"":"None");
				if(glMaterials[k].normalTexture.index>-1) {
					printf("         index:%d,  texCoord:%d, scale:%f\n",
						glMaterials[k].normalTexture.index,
						glMaterials[k].normalTexture.texCoord,
						glMaterials[k].normalTexture.scale
					);
				}

				printf("occlutionTexture: TODO\n");
			}

		}
		/* Case glTF Jobject: textures */
		else if(!key.compare("textures")) {
			printf("\n=== textures ===\n");
			if( E3D_glLoadTextures(strValue, glTextures)<0 )
				return -1;

			printf(DBG_GREEN"Totally %zu E3D_glTextures loaded.\n"DBG_RESET, glTextures.size());
			for(size_t k=0; k<glTextures.size(); k++) {
				printf("glTextures[%zu]: name'%s', sampler(Index)=%d, image(Index)=%d\n",
						k,glTextures[k].name.c_str(), glTextures[k].samplerIndex, glTextures[k].imageIndex);
			}
		}
		/* Case glTF Jobject: images */
		else if(!key.compare("images")) {
			printf("\n=== images ===\n");
			if( E3D_glLoadImages(strValue, glImages)<0 ) {
				//return -1;
				printf(DBG_RED"E3D_glLoadImages() fails!\n"DBG_RESET);
			}

			printf(DBG_GREEN"Totally %zu E3D_glImages need to load.\n"DBG_RESET, glImages.size());
                        char strtmp[128];
			size_t strlen;
			for(size_t k=0; k<glImages.size(); k++) {
				/* HK2023-01-03 */
				strlen=glImages[k].uri.copy(strtmp, 64, 0);
				strtmp[strlen]='\0';
				if(glImages[k].uri.size()>64) strcpy(strtmp+64, "...");

				printf(DBG_MAGENTA"\n    --- image_%zu---\n"DBG_RESET, k);
				printf("name: %s\n", glImages[k].name.c_str());
				printf("uri: %s\n", strtmp);
				printf("IsDataURI: %s\n", glImages[k].IsDataURI?"Yes":"No");
				printf("mimeType: %s\n", glImages[k].mimeType.c_str());
				printf("bufferView(Index): %d\n", glImages[k].bufferViewIndex);
			}
		}
#if 0		/* Case glTF Jobject: samplers TODO */
		else if(!key.compare("samplers")) {
			printf("\n=== samplers ===\n");
			if( E3D_glLoadSampler(strValue, glSampler)<0 )
                                return -1;

                        printf(DBG_GREEN"Totally %zu E3D_glSamplers loaded.\n"DBG_RESET, glSamplers.size());
                        for(size_t k=0; k<glSamplers.size(); k++) {
                                printf(DBG_MAGENTA"\n    --- sampler_%d---\n"DBG_RESET, k);
                                printf("name: %s\n", glSamplers[k].name.c_str());
				printf("wrapS: %d\n",glSamplers[k].wrapS);
				printf("wrapT: %d\n",glSamplers[k].wrapT);
				printf("magFilter: %d\n",glSamplers[k].magFilter);
				printf("minFilter: %d\n",glSamplers[k].minFilter);
                        }

		}
#endif
		/* Case glTF Jobject: nodes */
		else if(!key.compare("nodes")) {
			printf("\n=== nodes ===\n");
			if( E3D_glLoadNodes(strValue, glNodes)<0 ) {
				//return -1;
				printf(DBG_RED"E3D_glLoadNodes() fails!\n"DBG_RESET);
			}

			printf(DBG_GREEN"Totally %zu E3D_glNodes loaded.\n"DBG_RESET, glNodes.size());
			for(size_t k=0; k<glNodes.size(); k++) {
				printf(DBG_MAGENTA"\n    --- node_%zu---\n"DBG_RESET, k);
				printf("name: %s\n", glNodes[k].name.c_str());
				printf("type: %s\n", _strNodeTypes[glNodes[k].type]);
				printf("Matrix_or_qt:\n");
				if(glNodes[k].matrix_defined)
					glNodes[k].matrix.print();
				else
					glNodes[k].qt.print();
				//printf("pmat: \n"); glNodes[k].pmat.print("pmat"); /* HK2023-04-11 */
				printf("Children: ");
				for(size_t kc=0; kc<glNodes[k].children.size(); kc++)
					printf(" %d", glNodes[k].children[kc]);
				printf("\n");
			}
		}
		/* Case glTF Jobject: skins */
		else if(!key.compare("skins")) {
			printf("\n=== skins ===\n");
			if( E3D_glLoadSkins(strValue, glSkins)<0 ) {
				//return -1;
				printf(DBG_RED"E3D_glLoadSkins() fails!\n"DBG_RESET);
			}

			printf(DBG_GREEN"Totally %zu E3D_glSkins loaded.\n"DBG_RESET, glSkins.size());
			for(size_t k=0; k<glSkins.size(); k++) {
				printf(DBG_MAGENTA"\n    --- skins[%zu]---\n"DBG_RESET, k);
				printf("name: %s\n", glSkins[k].name.c_str());
				printf("invbMatsAccIndex: %d\n", glSkins[k].invbMatsAccIndex);
				if(glSkins[k].skeleton<0)
				     printf("skeleton(root): none\n");
				else
				     printf("skeleton(root): %d\n", glSkins[k].skeleton);
				printf("Total %zu joint_nodes.\n",glSkins[k].joints.size());
				printf("Joints nodes: [");
				for(size_t kk=0; kk<glSkins[k].joints.size(); kk++)
					printf(" %d ", glSkins[k].joints[kk]);
				printf("]\n");
			}
		}
		/* Case glTF Jobject: scenes */
		else if(!key.compare("scenes")) {
			printf("\n=== scenes ===\n");
			if( E3D_glLoadScenes(strValue, glScenes)<0 ) {
				//return -1;
				printf(DBG_RED"E3D_glLoadScenes() fails!\n"DBG_RESET);
			}

			printf(DBG_GREEN"Totally %zu E3D_glScenes loaded.\n"DBG_RESET, glScenes.size());
			for(size_t k=0; k<glScenes.size(); k++) {
				printf(DBG_MAGENTA"\n    --- scene_%zu---\n"DBG_RESET, k);
				printf("name: %s\n", glScenes[k].name.c_str());
				printf("Root node indices: ");
				for(size_t kk=0; kk<glScenes[k].rootNodeIndices.size(); kk++)
					printf("%d ", glScenes[k].rootNodeIndices[kk]);
				printf("\n");
			}
		}
		/* Case glTF Jobject: animations */
		else if(!key.compare("animations")) {
			printf("\n=== animations ===\n");
			if( E3D_glLoadAnimations(strValue, glAnimations)<0 ) {
				//return -1;
				printf(DBG_RED"E3D_glLoadAnimations() fails!\n"DBG_RESET);
			}

			printf(DBG_GREEN"Totally %zu E3D_glAnimations loaded.\n"DBG_RESET, glAnimations.size());
			for(size_t k=0; k<glAnimations.size(); k++) {
				printf(DBG_MAGENTA"\n    --- animation_%zu---\n"DBG_RESET, k);
				printf("name: %s\n", glAnimations[k].name.c_str());
				for(size_t kk=0; kk<glAnimations[k].channels.size(); kk++) {
					printf("Channels[%zu]: \n", kk);
					printf("   Sampler index: %d", glAnimations[k].channels[kk].samplerIndex);
					printf("   target node: %d, path: '%s'\n",
						 glAnimations[k].channels[kk].target.node, glAnimations[k].channels[kk].target.path.c_str());
				}
				for(size_t kk=0; kk<glAnimations[k].samplers.size(); kk++) {
					printf("Samplers[%zu]: \n", kk);
					printf("   input(accIndex): %d\n", glAnimations[k].samplers[kk].input);
					printf("   output(accIndex): %d\n", glAnimations[k].samplers[kk].output);
					printf("   interpolation: %d\n", glAnimations[k].samplers[kk].interpolation);
				}
			}
		}
		else
			egi_dpstd(DBG_YELLOW"Unparsed KEY/VALUE pair:\n  key: %s, value: %s\n"DBG_RESET, key.c_str(), strValue.c_str());

	}

	/* 3. Link buffers.data to bufferViews.data */
	for( size_t k=0; k<glBufferViews.size(); k++ ) {
		int index; /* index to glBuffers[] */
		int offset;
		int length;
		int stride;

		/* Check data integrity */
		index=glBufferViews[k].bufferIndex;
		if(index<0) {
			egi_dpstd(DBG_RED"glBufferViews[%zu].bufferIndex<0! give up data linking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check glBuffers[].data */
		if(glBuffers[index].data==NULL) {
			egi_dpstd(DBG_RED"glBuffers[%zu].data==NULL, give up data lingking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check offset */
		offset=glBufferViews[k].byteOffset;
		if(offset<0) {
			egi_dpstd(DBG_RED"glBufferViews[%zu].byteOffset<0! give up data lingking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check length */
		length=glBufferViews[k].byteLength;
		if(length<0) {
			egi_dpstd(DBG_RED"glBufferViews[%zu].byteLength<0! give up data lingking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check stride, byteStride is ONLY for vertex attribute data */
		stride=glBufferViews[k].byteStride;
		if(stride!=0 && (stride<4 || stride>252) ) {   /* byteStride: Minimum: >= 4, Maximum: <= 252 */
			egi_dpstd(DBG_RED"glBufferViews[%zu].byteStride=%d, without[4, 252], give up data lingking.\n"DBG_RESET, k, stride);
			//continue;
			return -1;
		}
		/* Check data length. */
		if(offset+length>glBuffers[index].byteLength) {
			egi_dpstd(DBG_RED"glBufferViews[%zu].byteOffset+byteLength > glBuffers[%d].byteLength, give up data lingking.\n"DBG_RESET,
					k, index);
			//continue;
			return -1;
		}
		/* TODO: Consider byteStride */
	/* bufferView.length >= accessor.byteOffset + EFFECTIVE_BYTE_STRIDE * (accessor.count - 1) + SIZE_OF_COMPONENT * NUMBER_OF_COMPONENTS */

		/* Link glBufferViews.data to glBuffers.data */
		glBufferViews[k].data = glBuffers[index].data+offset;
	}


	/* start = accessor.byteOffset + accessor.bufferView.byteOffset */

	/* 4. Link buffersViews.data to accessors.data */
	for( size_t k=0; k<glAccessors.size(); k++ ) {
		int index;   /* index to glBufferViews[] */
		int offset;
		int length; /* SIZE_OF_COMPONENT * NUMBER_OF_COMPONENTS */
		int esize;  /* Element size, in bytes */
		//int componentType; /* 5120-5126, NO 5124 */
		//bool normalized;
		int count;   /* Number of elements referenced by the accessor. */
		//string type;  /* SCALAR, or VEC2-4, or MAT2-4 */

		/* Check data integrity */
		index=glAccessors[k].bufferViewIndex;
		if(index<0) {
			egi_dpstd(DBG_RED"glAccessors[%zu].bufferViewIndex<0! give up data linking.\n"DBG_RESET,k);
			//continue;
			return -1;
		}
		/* Check offset */
		offset=glAccessors[k].byteOffset;
		if(offset<0) {
			egi_dpstd(DBG_RED"glAccessors[%zu].byteOffset<0! give up data linking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check count */
		count=glAccessors[k].count;
		if(count<0) {
			egi_dpstd(DBG_RED"glAccessors[%zu].count<0! give up data linking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check element size */
		esize=glAccessors[k].elemsize; //glElementSize(glAccessors[k].type, glAccessors[k].componentType);
		if(esize<0) {
			egi_dpstd(DBG_RED"glAccessors[%zu] element bytes<0! give up data linking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* If byteStride is dfined, it MUST be a multiple of the size of the accessor’s component type. */
		int stride=glBufferViews[index].byteStride; /* stride already checked at 3. */
		int compsize=glElementSize("SCALAR", glAccessors[k].componentType); /* SCALAR has 1 component */
		if(stride>0 && compsize>0) {
			if(stride%compsize) {
				egi_dpstd(DBG_RED"glAccessors[%zu]: stride(%d)%%compsize(%d)!=0, give up data linking.\n"DBG_RESET, k,stride,compsize);
				return -1;
			}
		}

		/* Check length */
		length=count*esize;
		if(length<0) {
			egi_dpstd(DBG_RED"glAccessors[%zu] data length<=0! give up data linking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check data length. */
		if(glBufferViews[index].byteLength < offset+length) {
			egi_dpstd(DBG_RED"glAccessors[%zu].byteOffset+byteLength > glBufferViews[%d].byteLength, give up data lingking.\n"DBG_RESET,
					k, index);
			//continue;
			return -1;
		}

		/* Check data length, consider byteStride */
		//egi_dpstd(DBG_GRAY"glBufferViews[%d].byteStride=%dBs, element_size=%dBs, element_count=%d\n"DBG_RESET,
		//		index, glBufferViews[index].byteStride, esize, count);
	/* bufferView.length >= accessor.byteOffset + EFFECTIVE_BYTE_STRIDE * (accessor.count - 1) + SIZE_OF_COMPONENT * NUMBER_OF_COMPONENTS */
		//if( glBufferViews[index].byteLength < offset+(glBufferViews[index].byteStride-esize)*(count-1)+length ) {
		if( glBufferViews[index].byteLength < offset+(stride-esize)*(count-1)+length ) {
 		    egi_dpstd(DBG_RED"glBufferViews[%d].byteLength insufficient for glAccessors[%zu]! give up data lingking.\n"DBG_RESET, index, k);
		    //continue;
		    return -1;
		}

		/* Link glAccessors.data to glBufferViews.data */
		glAccessors[k].data = glBufferViews[index].data+offset;
		/* Assign byteStride HK2023-03-12 */
		glAccessors[k].byteStride=stride;
	}
	/* 4a. */
	printf(DBG_YELLOW"Totally %zu E3D_glAccessors loaded.\n"DBG_RESET, glAccessors.size());
	for(size_t k=0; k<glAccessors.size(); k++) {
             printf("glAccessors[%zu]: name='%s', type='%s', count=%d, bufferViewIndex=%d, byteOffset=%d, componentType=%d, normalized=%s.\n",
		     k, glAccessors[k].name.c_str(), glAccessors[k].type.c_str(), glAccessors[k].count,
		     glAccessors[k].bufferViewIndex, glAccessors[k].byteOffset, glAccessors[k].componentType,
		     glAccessors[k].normalized?"Yes":"No" );
	     printf("         byteStride=%d\n", glBufferViews[glAccessors[k].bufferViewIndex].byteStride);
	}


	/* 4b. Load glImages[].imgbuf from glBufferViews[].data, ONLY when its bufferViewIndex>0 and mimeType='image/jpeg' OR 'image/png'
	       See E3D_glLoadImages(): If imgbuf is loaded from a file uri OR embedded uri-data.
	 */
	for( size_t k=0; k<glImages.size(); k++ ) {
	   int bvindex=glImages[k].bufferViewIndex;
	   if(glImages[k].imgbuf==NULL	/* In case it's been loaded from an image file URI, see at E3D_glLoadImages() */
		&& bvindex>=0 && bvindex<(int)glBufferViews.size() )
	   {
		egi_dpstd(DBG_YELLOW"Try to load glImages[%zu].imgbuf from bufferViews[%d]...\n"DBG_RESET, k, bvindex);
	    	if(!glImages[k].mimeType.compare("image/jpeg")) {
			glImages[k].imgbuf=egi_imgbuf_readJpgBuffer(glBufferViews[bvindex].data, glBufferViews[bvindex].byteLength);
		   	if(glImages[k].imgbuf==NULL) {
				egi_dpstd(DBG_RED"glImages[%zu]: Fail to readJpgBuffer to imgbuf!\n"DBG_RESET, k);
				//return -1;
		   	}
	    	}
            	else if(!glImages[k].mimeType.compare("image/png")) {
		   	glImages[k].imgbuf=egi_imgbuf_readPngBuffer(glBufferViews[bvindex].data, glBufferViews[bvindex].byteLength);
		   	if(glImages[k].imgbuf==NULL) {
				egi_dpstd(DBG_RED"glImages[%zu]: Fail to readJpngBuffer to imgbuf!\n"DBG_RESET, k);
				//return -1;
		   	}
            	}
            	else {
                	egi_dpstd(DBG_RED"glImages[%zu]: mimeType error for '%s'.\n"DBG_RESET, k, glImages[k].mimeType.c_str());
                	//return -1;
            	}
	   }
	}

//////////////////////// END: load glTF file to glBuffers/glBufferViews/glAccessors/glMeshes... ////////////////////
//exit(0);

	/* XXX 5. Count all trianlges and vertices */
        /* XXX 6. Clear vtxList[] and triList[], E3D_TriMesh() may allocate them.  */
        /* XXX 7. Allocate/init vtxList[] */
        /* XXX 8. Allocate/init triList[] */
	/* XXX 9. Resize triGroupList */
	/* XXX 10. Reset triCount/vtxCount */
	/* XXX 10a. Load Materils */

        /* 8. Check glNodes and set glMesh[].skinIndex accordingly. */
	for( size_t n=0; n<glNodes.size(); n++) {
  	    if(glNodes[n].type==glNode_Skin) {
		if( glNodes[n].skinMeshIndex>=0 && glNodes[n].skinMeshIndex < (int)glMeshes.size() ) {
		    if( glNodes[n].skinIndex>=0 && glNodes[n].skinIndex < (int)glSkins.size() ) {
			/* Finally.... */
			glMeshes[glNodes[n].skinMeshIndex].skinIndex = glNodes[n].skinIndex;
		    }
		    else
		        egi_dpstd(DBG_RED"glNodes[%zu].skinIndex=%d, NOT valid within [0 %zu) (glSkins.size) \n"DBG_RESET,
								n,glNodes[n].skinIndex, glSkins.size() );
		}
		else
		    egi_dpstd(DBG_RED"glNodes[%zu].skinMeshIndex=%d, NOT valid within [0 %zu) (glMeshes.size) \n"DBG_RESET,
								n,glNodes[n].skinMeshIndex, glMeshes.size() );
	    }
	}

	/* 9. Load skin data into glNodes.IsJoint and glNodes.invbMat */
printf(" ----------- xxxxxxxxxx ------------ \n");
	for( size_t  n=0; n<glSkins.size(); n++) { /* NOW one mesh is ONLY for one skin. */
	   egi_dpstd(DBG_MAGENTA"\n    --- glSkin[%zu]: nodes.inverseBindMatrices ---\n"DBG_RESET, n);

	   /* 9.1 Get data pointer to invbMat */
	   float *imdata=NULL; ///<---- { NULL }
	   int invbMatsAccIndex=glSkins[n].invbMatsAccIndex;
	   if( invbMatsAccIndex >=0 && invbMatsAccIndex <(int)glAccessors.size() ) {
	        if( !glAccessors[invbMatsAccIndex].type.compare("MAT4")
		    && glAccessors[invbMatsAccIndex].count >= (int)glSkins[n].joints.size() ) {

			imdata=(float *)glAccessors[invbMatsAccIndex].data;
		}
	   }
	   if( glAccessors[invbMatsAccIndex].componentType!=5126 )
		egi_dpstd(DBG_RED"glAccessors[invbMatsAccIndex].componentType!=5126(float)!\n"DBG_RESET);

egi_dpstd(DBG_YELLOW"glAccessors[invbMatsAccIndex].count=%d, glSkins[n].joints.size()=%zu\n"DBG_RESET,
			glAccessors[invbMatsAccIndex].count, glSkins[n].joints.size() );


	   /* 9.2 Assign glNodes.IsJoint and glNodes.invbMat */
	   for( size_t nn=0; nn<glSkins[n].joints.size(); nn++) {
		int jnode=glSkins[n].joints[nn];
		printf("glSkins[%zu].joints[%zu]=%d\n", n,nn, glSkins[n].joints[nn]);

		/* Check if jnode is reasonale */
		if(jnode<0 || jnode >= (int)glNodes.size())
			continue;

		/* TBD&TODO: NOW, a node can ONLY assign to one skin! */
		if(glNodes[jnode].IsJoint) {
			egi_dpstd(DBG_RED"glNodes[%d] is ALREADY assigned as a joint to glSkin[%d]!(now nn=%zu)\n"DBG_RESET,
						jnode, glNodes[jnode].jointSkinIndex, nn);
			continue;
		}

		/* Set IsJoint and skinIndex*/
		glNodes[jnode].IsJoint =true;
		glNodes[jnode].jointSkinIndex =n; //nn; /* index of skins */

		/* Set invbMat */
		if( imdata!=NULL ) {
printf("Extract inverseBindMatrices for glNodes[%d]....\n",jnode);
                      /*** Extract pmat[4*3] from 4*4 matrix
		       *   Note: Accessors of matrix type have data stored in column-major order! (glTF column-major is E3D row-major)
		       */

		   #if 1 /* Same as E3D row_major */
                      for(int ni=0; ni<4; ni++)  {
                          for(int nj=0; nj<3; nj++) {
                                      glNodes[jnode].invbMat.pmat[ni*3+nj]=imdata[(nn<<4)+(ni<<2)+nj]; // = [nn*16+ni*4+nj];
                         }
                      }

		  #else
                      for(int ni=0; ni<3; ni++)  {
			 /* pmat[0-8]: RSmatrix */
                         for(int nj=0; nj<3; nj++) {
                                      glNodes[jnode].invbMat.pmat[ni*3+nj]= imdata[(nn<<4)+(ni<<2)+nj]; // = [nn*16+ni*4+nj];
                                      //glNodes[jnode].invbMat.pmat[nj*3+ni]= imdata[(nn<<4)+(ni<<2)+nj]; // = [nn*16+ni*4+nj];
			 }

			  /* pmat[9-11]: txyz */
			  glNodes[jnode].invbMat.pmat[9+ni]=imdata[(nn<<4)+(ni<<2)+3];
			  //glNodes[jnode].invbMat.pmat[9+ni]=imdata[(nn<<4)+12+ni];
		      }
		  #endif
		      /* The fourth row of each matrix MUST be set to [0.0, 0.0, 0.0, 1.0]. */

     		  #if 1 /* TEST: --------  */
//		     printf(">>>> %f,%f,%f,%f <<<<<\n", /* the last row, SHOULD BE 0,0,0,1 */
//					imdata[(nn<<4)+(0<<2)+3], imdata[(nn<<4)+(1<<2)+3], imdata[(nn<<4)+(2<<2)+3],imdata[(nn<<4)+(3<<2)+3]);
		     glNodes[jnode].invbMat.print();

		     /* To print vx/vy/vz HK2023-04-21 */
		     float vx,vy,vz;
		     vx= glNodes[jnode].invbMat.pmat[0]*glNodes[jnode].invbMat.pmat[0]
			+glNodes[jnode].invbMat.pmat[1]*glNodes[jnode].invbMat.pmat[1]
			+glNodes[jnode].invbMat.pmat[2]*glNodes[jnode].invbMat.pmat[2];
		     vx=sqrtf(vx);

		     vy= glNodes[jnode].invbMat.pmat[3]*glNodes[jnode].invbMat.pmat[3]
			+glNodes[jnode].invbMat.pmat[4]*glNodes[jnode].invbMat.pmat[4]
			+glNodes[jnode].invbMat.pmat[5]*glNodes[jnode].invbMat.pmat[5];
		     vy=sqrtf(vy);

		     vz= glNodes[jnode].invbMat.pmat[6]*glNodes[jnode].invbMat.pmat[6]
			+glNodes[jnode].invbMat.pmat[7]*glNodes[jnode].invbMat.pmat[7]
			+glNodes[jnode].invbMat.pmat[8]*glNodes[jnode].invbMat.pmat[8];
		     vz=sqrtf(vz);
		     printf("Modulus(vx,vy,vz): %f,%f,%f\n", vx,vy,vz);

     		  #endif
		}
		else
		     egi_dpstd(DBG_RED"imdata=NULL! inverseBindMatrices are NOT available!\n"DBG_RESET);
	   }
	}

	/* 10. Reserve triMeshList first.  XXX to prevent unexpectd copying/releasing during auto capacity adjusting.XXX */
	triMeshList.reserve(glMeshes.size());

	/* 11. Load vtxList[], triList[] and triGroupList[] */
	int accIndex; /* Index for glAccessors[] */
	int mode;
	unsigned int vindex[3];  /* vertex index for tirangle */
	unsigned int uind;	/* To store assmebled index number */
	unsigned int utmp;
	unsigned int *udat;
				/* For triangle vertexIndex.
				 * When primitives.indices is defined. the accessor MUST have SCALAR element_type and
				 * an unsigned integer component_type.
				 *    			!!!--- CAUTION ---!!!
				 * Above "unsigned integer" includes: unsigned int, unsigned short, or unsigned char
		 		 */
	float *fdat;	/* 1. For veterx position(XYZ). element_type is VEC3, and component_type is float.
			 * 2. For veterx normal(xyz). element_type is VEC3, and component_type is float.
			 * 3. For u/v, element_type is VEC2, and component_types: float/unsigned_byte_normalized/unsinged_short_normalized
			 */
	int Vsdx;  /* Set as starting index for vtxList[], before reading each primitive */
	int Tsdx;  /* Set as starting index for triList[], before reading each primitive */
	int primitiveVtxCnt=0; /* Vertex count for a primitive, reset at 11.1A */

	vector<int> glMaterialsInMesh; /* glMaterialID list used in a trimesh */

	/* 11.-1 Traverse all meshes and primitives, to load data to vtxList[], triList[], triGroupList[], */
	for(size_t m=0; m<glMeshes.size(); m++) {
	    egi_dpstd(DBG_ORANGE"\n\nRead glMeshes[%zu]...\n"DBG_RESET, m);
	    int  primTriCnt=0;

	    /* 11.-1.1 Clear materialIDs */
	    glMaterialsInMesh.clear();

	    /* 11.-1.2 Compute tCapacity and vCapacity in glMeshes[] */
	    triCount=0; vtxCount=0; tgCount=0;
            for(size_t i=0; i<glMeshes[m].primitives.size(); i++) {
                int accID;
            	if( glMeshes[m].primitives[i].mode==4 ) { /* 4 for triangle */
                	/* Triangles */
                        accID=glMeshes[m].primitives[i].indicesAccIndex;
                        /* Each consecutive set of three vertices defines a single triangle primitive */
                        triCount += glAccessors[accID].count/3;

                        /* Vertices */
                        accID=glMeshes[m].primitives[i].attributes.positionAccIndex;
                        vtxCount += glAccessors[accID].count;

                        /* triGroup */
                        tgCount +=1;
                }
	        egi_dpstd("glMeshes[%zu].primitive[%zu] contains: triCount=%d, vtxCount=%d, tgCount=%d\n", m,i, triCount, vtxCount, tgCount);
	    }

	    /* 11.-1.3 Add a new TriMesh in triMeshList[], as corresponds with glMeshes[m] */
	    E3D_TriMesh *trimesh = new E3D_TriMesh(vtxCount, triCount); /* init vCapacity, tCapacity */
	    if(trimesh==NULL) {
		egi_dpstd(DBG_RED"Fail to allocate a new E3D_TriMesh!\n"DBG_RESET);
		return -1; //???? TODO
	    }
            /* 11.-1.4 Resize triGroupList */
            trimesh->triGroupList.resize(tgCount);
	    for(size_t kt=0; kt < trimesh->triGroupList.size(); kt++) /* HK2023-03-06 Necessary? */
			trimesh->triGroupList[kt].setDefaults();

	    /* 11.-1.4A Reserve enough capacity for trimesh->mtlList, See cautions at 11.3.4.1 */
	    int tmpIndex=-1;
	    int meshMtlCnt=0; /* Count of materials in a mesh, no repeating materialIndex. */
	    /* 11.-1.4A.1 Count all materials in a mesh */
	    for(size_t i=0; i<glMeshes[m].primitives.size(); i++) {
		if(glMeshes[m].primitives[i].materialIndex>=0) {
			tmpIndex=-1; /* as NOT found */
			/* Search in glMaterialsInMesh if same materialIndex already exists */
			for(size_t mk=0; mk<glMaterialsInMesh.size(); mk++) {
				if(glMeshes[m].primitives[i].materialIndex == glMaterialsInMesh[mk]) {
					tmpIndex=glMaterialsInMesh[mk]; /* Found same in glMaterialsInMesh */
				}
			}
			/* NOT found in glMaterialsInMesh[] */
			if(tmpIndex<0) {
				meshMtlCnt +=1;
			}
		}
	    }
	    /* 11.-1.4A.2 Reserve mtlList */
	    trimesh->mtlList.reserve(meshMtlCnt);

	    /* 11.-1.4B Clear materialIDs again. */
	    glMaterialsInMesh.clear();

	    /* 11.-1.5 Push back trimesh  !!! --- CAUTION --- !!! triMeshList capacity is reversed at 10. */
	    triMeshList.push_back(trimesh);

	    egi_dpstd("a E3D_TriMesh with tCapacity=%d, vCapacity=%d, triGroupList.size=%zu is created for glMeshes[%zu]!\n",
					trimesh->tCapacity, trimesh->vCapacity, trimesh->triGroupList.size(), m);

	    /* 11.-1.6 Reset triCount/vtxCount/tgCount for each mesh. */
	    triCountBk=triCount; 	triCount=0;
	    vtxCountBk=vtxCount; 	vtxCount=0;
	    tgCountBk=tgCount;  	tgCount=0;

	    /* 11.-1.6a Assign name HK2023-04-11 */
	    trimesh->name = glMeshes[m].name;

	    /* 11.-1.7 Assign mesh.mweights */
	    trimesh->mweights = glMeshes[m].mweights;

	    /* 11.0 Walk through all primitives in the mesh */
	    for(size_t n=0; n<glMeshes[m].primitives.size(); n++) {
	    	egi_dpstd(DBG_GREEN"   Read glMeshes[%zu].primitives[%zu]...\n"DBG_RESET, m, n);

		mode=glMeshes[m].primitives[n].mode;
		if(mode==4) primTriCnt++;

/* TODO: NOW only TRIANGLES: ----------------- Following for mode==4 (triangles) ONLY */

		/* 11.1 Save triGroupList[] starting index here! */
		Vsdx=vtxCount;
		Tsdx=triCount;
		egi_dpstd("Starting vtxIndex=%d\n", vtxCount);

		/* 11.1A Reset */
		primitiveVtxCnt =0 ;

		/* 11.2 Read glMesh.primitives.attributes.POSITION into vtxList[] */
		accIndex=glMeshes[m].primitives[n].attributes.positionAccIndex;
		if(mode==4 && accIndex>=0 ) {
			/* 11.2.0 Check data */
			if(glAccessors[accIndex].data==NULL || glAccessors[accIndex].type.compare("VEC3") ) {  /* HK2023-03-25 */
				egi_dpstd(DBG_RED"11.2.0: glAccessors[%d].data is NULL OR type is not VEC3! Abort.\n"DBG_RESET, accIndex);
				return -1;
			}

			/* 11.2.1 Get pointer to float type */
			fdat=(float *)glAccessors[accIndex].data;
			int _dstride=glAccessors[accIndex].byteStride;
			if(_dstride !=0 && _dstride!=sizeof(float)*3 )
				egi_dpstd(DBG_RED"_dstride !=0 && _dstride!=sizeof(float)*3!\n"DBG_RED);

			/* 11.2.1a Get primitiveVtxCnt */
			primitiveVtxCnt=glAccessors[accIndex].count; /* VEC3 */

			/* 11.2.2 Read in vtxList[] */
			egi_dpstd(DBG_YELLOW"Read vtxList[] from glAccessors[%d], VEC3 count=%d\n"DBG_RESET, accIndex, glAccessors[accIndex].count);
			for(int j=0; j<glAccessors[accIndex].count; j++) {  /* count is number of ELEMENTs(VEC3) */
				/* Read XYZ as fully packed. TODO: consider byteStride */
				//trimesh->vtxList[vtxCount].assign(fdat[3*j], fdat[3*j+1], fdat[3*j+2]);
			        if(_dstride)
				   trimesh->vtxList[vtxCount].assign( fdat[(_dstride>>2)*j],    /* float is 4bytes */
								      fdat[(_dstride>>2)*j+1],
								      fdat[(_dstride>>2)*j+2] );
				else
				   trimesh->vtxList[vtxCount].assign(fdat[3*j], fdat[3*j+1], fdat[3*j+2]);

//				printf("vtxList[%d](XYZ): %f,%f,%f\n", vtxCount, fdat[3*j], fdat[3*j+1], fdat[3*j+2]);

				vtxCount++;
				trimesh->vcnt=vtxCount;
			}

		   	/* 11.2.3 Read glMesh.primitives.attributes.NORMAL into vtxList[] */
		   	int normalAccIndex=glMeshes[m].primitives[n].attributes.normalAccIndex;
		   	if(normalAccIndex>=0 ) {
			    /* Check element_type and component_type for NORMAL data */
			    if(glAccessors[normalAccIndex].type.compare("VEC3"))
			       egi_dpstd(DBG_RED"glMeshes[%zu].primitives[%zu] normal_data: glAccessror[%d] element is NOT 'VEC3'!\n"DBG_RESET,
				   		m,n, normalAccIndex);
			    if(glAccessors[normalAccIndex].componentType!=5126)
			       egi_dpstd(DBG_RED"glMeshes[%zu].primitives[%zu] normal_data: glAccessror[%d] component is NOT 'FLOAT'!\n"DBG_RESET,
				  		m,n, normalAccIndex);


				/* Get pointer to float type */
				fdat=(float *)glAccessors[normalAccIndex].data;

				/* Read in vtxList[].normal */
				egi_dpstd(DBG_YELLOW"Read vtxList[].normal from glAccessors[%d], VEC3 count=%d\n"DBG_RESET,
											normalAccIndex, glAccessors[normalAccIndex].count);
				/* Check vertex POSITION count and NORMAL count. */
				if(glAccessors[accIndex].count != glAccessors[normalAccIndex].count ) {
				   egi_dpstd(DBG_RED"POSITION glAccessors[%d].count!= NORMAL glAccessors[%d].count !\n"DBG_RESET,
								     accIndex, normalAccIndex);
				}
				else {
				   /* Read in vtxList[].normal */
				   for(int j=0; j<glAccessors[normalAccIndex].count; j++) {  /* count is number of ELEMENTs(VEC3) */
					/* Assume they are normalized VEC3 */
					trimesh->vtxList[Vsdx+j].normal.assign(fdat[3*j], fdat[3*j+1], fdat[3*j+2]);

//printf("vtxList[%d].normal: %f,%f,%f\n", Vsdx+j, fdat[3*j], fdat[3*j+1], fdat[3*j+2]);
				   }
				}
			}
		}

		/* 11.3 Read glMesh.primitivies.indices into triList[] */
		accIndex=glMeshes[m].primitives[n].indicesAccIndex;
		if( mode==4 && accIndex>=0 && accIndex<(int)glAccessors.size() ) {
			/* 11.3.0 Check data */
			if(glAccessors[accIndex].data==NULL) {
				egi_dpstd(DBG_RED"glAccessors[%d].data is NULL! Abort.\n"DBG_RESET,accIndex);
				return -1;
			}

			/* 11.3.1 Get component size */
			int compsize=glAccessors[accIndex].elemsize; /* for SCALAR, number_of_components=1 */

			/* triGroupList[] starting index */
			//Vsdx=vtxCount; NOT HERE! see at 11.1

			/* 11.3.2 Read this primitive into triList[] */
			egi_dpstd(DBG_YELLOW"Read triList[] from glAccessors[%d], compsize=%d, SCALAR count=%d\n"DBG_RESET,
				accIndex, compsize, glAccessors[accIndex].count);
			for(int j=0; j<glAccessors[accIndex].count/3; j++) { /* 3 vertices for each triangle */

#if 1  //////////////// For any elemsize  /////////////////
				/* Read 3 vtx indices as fully packed.  TODO: consider byteStride  */
				for(int ii=0; ii<3; ii++) { /* 3 verter index for one triangle */
					uind=0;
					for(int jj=0; jj<compsize; jj++) { /* component bytes */
						/* read byte by byte. littlendia. */
						utmp=glAccessors[accIndex].data[(3*j+ii)*compsize+jj];
						uind += utmp<<(jj<<3); //jj*8
					}
					vindex[ii]=uind;

				#if 0	/* Check */
					if( Vsdx+(int)vindex[ii] >= vtxCount ) {
						egi_dpstd(DBG_RED"Vsdx(%d)+vtxIndex(%d) >= %d(vtxTotal)!\n"DBG_RESET,
										Vsdx,Vsdx+vindex[ii], vtxCount);
						return -1;
					}
				#endif

				}
				/* Assign to triList[] */
				trimesh->triList[triCount].assignVtxIndx(Vsdx+vindex[0], Vsdx+vindex[1], Vsdx+vindex[2]);
				//printf("triList[%d](index012): %d,%d,%d\n", triCount, Vsdx+vindex[0], Vsdx+vindex[1], Vsdx+vindex[2]);

#else ///////////////// For elemisze=4bytes ONLY //////////////
			        //udat pointer to ...
				udat=(unsigned int *)glAccessors[accIndex].data;
				trimesh->triList[triCount].assignVtxIndx(Vsdx+udat[3*j], Vsdx+udat[3*j+1], Vsdx+udat[3*j+2]);
				printf("triList[%d](index012): %d,%d,%d\n", triCount, Vsdx+udat[3*j], Vsdx+udat[3*j+1], Vsdx+udat[3*j+2]);
#endif
				/* triCount incremental */
				triCount++;
				trimesh->tcnt=triCount;
			}

			/* 11.3.3 Update TriGroupList. One meshes.primitive for one triGroup */
		        trimesh->triGroupList[tgCount].tricnt=glAccessors[accIndex].count/3; //HK2022-12-22: tricnt replaces tcnt
        		trimesh->triGroupList[tgCount].stidx=Tsdx;
        		trimesh->triGroupList[tgCount].etidx=Tsdx+glAccessors[accIndex].count/3; /* Caution!!! etidx is NOT the end index! */

			    /* HK2023-03-01 */
			trimesh->triGroupList[tgCount].vtxcnt=primitiveVtxCnt; //!!! glAccessors[accIndex].count; accIndex NOW is indicesAccIndex
			trimesh->triGroupList[tgCount].stvtxidx=Vsdx;
			trimesh->triGroupList[tgCount].edvtxidx=Vsdx+primitiveVtxCnt; /* Caution!!! edvtxidx is NOT the end index! */
			   /* glPrimitive has no param name! */
        		trimesh->triGroupList[tgCount].name=glMeshes[m].name; /* Assume ONLY 1 trianlge_primitive(mode==4) in primitives[] */
			/* If mesh has more than 1 primitive, then triGroup_name = mesh_name+append_num */
			if(glMeshes[m].primitives.size()>1) {
				char buff[32];
				buff[31]=0;
				sprintf(buff,"_%zu",n);
				trimesh->triGroupList[tgCount].name.append(buff);
			}

			/* 11.3.3A  Assign TriGroupList[].skins  */
			int jointsAccIndex=glMeshes[m].primitives[n].attributes.joints_0;
			int weightsAccIndex=glMeshes[m].primitives[n].attributes.weights_0;
			float *wtdata=NULL;  /* For weights */
			unsigned char  *jtdata0=NULL;  /* Forr joint indices */
			unsigned short *jtdata1=NULL;

			/* If has skin defined */
			if( glMeshes[m].skinIndex >=0   /* glMeshes[].skinIndx is set at 8. */
			   && jointsAccIndex >=0 && jointsAccIndex <(int)glAccessors.size()
			   && weightsAccIndex >=0 && weightsAccIndex <(int)glAccessors.size() )
			{
			   egi_dpstd(DBG_YELLOW"Read skin data for glMeshes[%zu].primitive[%zu], joints_0=%d, weights_0=%d.\n"DBG_RESET,
						m, n, jointsAccIndex, weightsAccIndex);

			   /* .S1 Check if accessors have valid data */
			   if(  !glAccessors[jointsAccIndex].type.compare("VEC4")
				  && glAccessors[jointsAccIndex].count >= primitiveVtxCnt
			     && !glAccessors[weightsAccIndex].type.compare("VEC4")
                                  && glAccessors[weightsAccIndex].count >= primitiveVtxCnt )
			   {
			      /*** Note:
				 1. glAccessors[jointsAccIndex] componentType must be: unsigned byte(5121) or unsigned short(5123).
				 2. glAccessors[weightsAccIndex] componentType must be: float(5126), or normalized unsigned byte(5121),
				    or normalized unsigned short(5123).
			       */
      egi_dpstd(DBG_MAGENTA"glAccessors[weightsAccIndex].componentType=%d(5126), glAccessors[jointsAccIndex].componentType=%d(5121or5123)\n"DBG_RESET,
					  glAccessors[weightsAccIndex].componentType, glAccessors[jointsAccIndex].componentType );

			     /* TODO: If glAccessors[weightsAccIndex].normalized */
 			     if(glAccessors[weightsAccIndex].normalized)
      				 egi_dpstd(DBG_RED"glAccessors[%d for weightsAccIndex] is normalized!\n"DBG_RESET,weightsAccIndex);

			      /* .S1.1 NOW ONLY support float component for weigths */
			      if( glAccessors[weightsAccIndex].componentType == 5126
			         && ( glAccessors[jointsAccIndex].componentType == 5121 || glAccessors[jointsAccIndex].componentType == 5123 ) )
			      {
				/* .S1.1.1 Get pinter to wtdata/jtdata */
				wtdata=(float *)glAccessors[weightsAccIndex].data;

				jtdata0=NULL; jtdata1=NULL;
				if( glAccessors[jointsAccIndex].componentType == 5121 ) /* unsigned byte */
					jtdata0=(unsigned char *)glAccessors[jointsAccIndex].data;
				else /* componentType == 5123 */
					jtdata1=(unsigned short *)glAccessors[jointsAccIndex].data;

				/* .S1.1.2 Resize triGroupList[].skins */
				trimesh->triGroupList[tgCount].skins.resize(primitiveVtxCnt);

				/* .S1.1.3 Each primitive vetex has skin data: wegiths and joints */
				int   skinIndex=glMeshes[m].skinIndex;
				float _weight;  /* skin weight */
				int _joint;	/* joint index */
				int _jointNode; /* node index */
				int  wstride=glAccessors[weightsAccIndex].byteStride;
				int  jstride=glAccessors[jointsAccIndex].byteStride;

				for( int is=0; is<primitiveVtxCnt; is++) {
				   /* .S1.1.3.1 Each vertex has Max. 4 influencing joints */

				   for( int js=0; js<4; js++ ) {

				 	/* .S1.1.3.1.1 Get triGroupList[].skins.weights[4] */
					if(wstride)
					     _weight=*(wtdata+wstride/sizeof(float)*is+js);  //sizeof(float)=4
					else
					     _weight=*(wtdata+4*is+js);
					trimesh->triGroupList[tgCount].skins[is].weights.push_back(_weight);

					/* .S1.1.3.1.2 Get _joint from accessor data */
					if(jtdata0) { /* jtdata0: unsigned byte */
					     if(jstride)
						 _joint=*(jtdata0+jstride/1*is+js); /* sizeof(unsigned char)=1 */
					     else
					         _joint=*(jtdata0+4*is+js); /* index to glSkin.joints[] */
					}
					else { /* jtdata1: unsigned short */
					     if(jstride) {
						  _joint=*(jtdata1+jstride/sizeof(unsigned short)*is+js);  /* sizeof(unsigned short)=2 */

					     }
					     else
						 _joint=*(jtdata1+4*is+js);
					}

					/* .S1.1.3.1.3 Get triGroupList[].skins.jointNodes[4] */
					if(_joint>=0 &&  _joint<(int)glSkins[skinIndex].joints.size() ) {
					    /* Covert glSkin.joints[] index to glNodes[] index */
					    _jointNode=glSkins[skinIndex].joints[_joint];

					    if( _jointNode>=0 && _jointNode<(int)glNodes.size() ) {
						 /* Finnaly...*/
						trimesh->triGroupList[tgCount].skins[is].jointNodes.push_back(_jointNode);
					    }
					    /* Whatever */
					    else {
					        trimesh->triGroupList[tgCount].skins[is].jointNodes.push_back(0);
						egi_dpstd(DBG_RED"_jointNode=%d, NOT within [0  %zu)(glNodes.size)!\n"DBG_RESET,
										_jointNode, glNodes.size());
					    }
					}
					/* .S1.1.3.1.4 Whatever */
					else {
					    /* Whatever */
					    trimesh->triGroupList[tgCount].skins[is].jointNodes.push_back(0);

					    egi_dpstd(DBG_RED"_joint=%d, NOT within [0  %zu)(glSkins[%d].joints.size)!\n"DBG_RESET,
									_joint, glSkins[skinIndex].joints.size(), skinIndex);
					}

				   }/* END for(js) */

				}/* END for(is) */

			    } /* END .S1.1 */
			    else {
				egi_dpstd(DBG_RED"glAccessors[weightsAccIndex].compentType is NOT float, not supported yet!"DBG_RESET);
			    }
			  } /* END .S1 */
			}
			/* No skin defined */
			else {
			   egi_dpstd(DBG_GRAY"NO skin data for glMeshes[%zu].primitive[%zu], skinIndex=%d, joints_0=%d, weights_0=%d.\n"DBG_RESET,
						m, n,  glMeshes[m].skinIndex, jointsAccIndex, weightsAccIndex);
			}

			/* 11.3.4 Assign material ID for the triGroup. Also see 10a. glMaterials[] ---> THIS.mtlList[] */
			int materialIndex=glMeshes[m].primitives[n].materialIndex;  //materialIndex = THIS.mtlID
			if( materialIndex >-1 && materialIndex<(int)glMaterials.size() ) { //same index glMaterials[]-->THIS.mtlList[]
				egi_dpstd(DBG_YELLOW"Assign triGroupList[].mtlID ...\n"DBG_RESET);

			        int baseColorTextureIndex;
			        int emissiveTextureIndex;
			        int normalTextureIndex;

				/* 11.3.4.0 Check if material with materialIndex has already been mapped into triMesh->mtList */
				bool mtID_found=false; /* whether this glMaterial already imported into trimesh->mtList */
				size_t mk;
				for(mk=0; mk<glMaterialsInMesh.size(); mk++) {
					if(materialIndex==glMaterialsInMesh[mk]) {
						mtID_found=true;

						/* Assign triGroupList[].mtlID */
						trimesh->triGroupList[tgCount].mtlID=mk;
						break;
					}
				}

				/* 11.3.4.1 Add a new item into trimesh->mtlList[] */
			    if(mtID_found==false) {
				int id;  //--->mtID;
			        //int baseColorTextureIndex;
			        //int emissiveTextureIndex;
			        //int normalTextureIndex;

				/* Push materialIndex into glMaterialInMesh, as corresponding with id. */
				glMaterialsInMesh.push_back(materialIndex);

			        /* One more item in mtlList */
				E3D_Material material;

				/*** 			!!!--- CAUTION ---!!!
				  material has pointer members, mtlList MUST reserve enough capacity to prevent unexpected
				  reallocation/realse of pointers when vector size grows. see 11.-1.4A
				 */
				trimesh->mtlList.push_back(material);

				/* !!! Operated with trimesh->mtllist.back(), NOT material! for it has pointer memeber !!! */
				id=trimesh->mtlList.size()-1;

				/* M1. Assign mtlList[].name */
		                trimesh->mtlList[id].name = glMaterials[materialIndex].name;

				egi_dpstd(DBG_MAGENTA"Load glMaterial[%d] '%s' into trimesh->mtlList[%d]\n"DBG_RESET,
						 	materialIndex, glMaterials[materialIndex].name.c_str(), id);

				/* M1A. Assign mtlList[].doubleSided HK2023-03-11 */
				trimesh->mtlList[id].doubleSided = glMaterials[materialIndex].doubleSided;

                		/* M2. Assign mtlList[].kd */
		                if(glMaterials[materialIndex].pbrMetallicRoughness.baseColorFactor_defined) {
                			trimesh->mtlList[id].kd.x= glMaterials[materialIndex].pbrMetallicRoughness.baseColorFactor.x;
		                    	trimesh->mtlList[id].kd.y= glMaterials[materialIndex].pbrMetallicRoughness.baseColorFactor.y;
                			trimesh->mtlList[id].kd.z= glMaterials[materialIndex].pbrMetallicRoughness.baseColorFactor.z;
					//TODO: ALPAH component
                		}
		                /* ELSE baseColorFactor undefined: keep E3D_Material default kd value. */

                		/* M3. Assign mtlList[].img_kd */
		                baseColorTextureIndex=glMaterials[materialIndex].pbrMetallicRoughness.baseColorTexture.index;
                		//if(glMaterials[k].pbrMetallicRoughness.baseColorTexture_defined) {
		                //baseColorTextureIndex >=0 for baseColorTexture_defined
                		if(baseColorTextureIndex>=0 && glTextures[baseColorTextureIndex].imageIndex>=0 ) {
					egi_dpstd(DBG_MAGENTA"Assign glImages[%d].imbuf to trimesh->mtlList[%d].img_kd\n"DBG_RESET,
							glTextures[baseColorTextureIndex].imageIndex, id);
		                        /*---- !!! CAUTION !!! Ownership transfers.  ---*/
                		        trimesh->mtlList[id].img_kd=glImages[glTextures[baseColorTextureIndex].imageIndex].imgbuf;
		                        /* glImages[glTextures[baseColorTextureIndex].imageIndex].imgbuf=NULL; see func TODO note.
					   It may be refered by other meshes, so do no reset to NULL.
					 */
					if(trimesh->mtlList[id].img_kd)
					    egi_dpstd("imgbuf W*H: %d*%d\n",
							trimesh->mtlList[id].img_kd->width, trimesh->mtlList[id].img_kd->height);
					else
				             egi_dpstd(DBG_RED"glImages[%d].imbuf is NOT loaded!\n"DBG_RESET,
										glTextures[baseColorTextureIndex].imageIndex);
                		        /* Note: Assign triGroupList[].vtx.u/v in 11.3.4 */
                		}

		                /* M4. Assign mtlList[].img_ke */
                		emissiveTextureIndex=glMaterials[materialIndex].emissiveTexture.index;
		                //emissvieTextureIndex >=0 for emissiveTexture_defined
                		if(emissiveTextureIndex>=0 && glTextures[emissiveTextureIndex].imageIndex>=0 ) {
		                        /*---- !!! CAUTION !!! Ownership transfers.  --- */
                		        trimesh->mtlList[id].img_ke=glImages[glTextures[emissiveTextureIndex].imageIndex].imgbuf;
		                        /* glImages[glTextures[emissiveTextureIndex].imageIndex].imgbuf=NULL; see TODO note.
					  It may be refered by other meshes, so do no reset to NULL.
                                         */

                		        /* Note: Assign triGroupList[].vtx.u/v in 11.3.4 */
                		}
				/* M5. Assign mtlList[].img_kn */
			        normalTextureIndex=glMaterials[materialIndex].normalTexture.index;
		                if(normalTextureIndex>=0 && glTextures[normalTextureIndex].imageIndex>=0 ) {
                		        /*---- !!! CAUTION !!! Ownership transfers.  ---*/
		                        trimesh->mtlList[id].img_kn=glImages[glTextures[normalTextureIndex].imageIndex].imgbuf;
                		        /* glImages[glTextures[normalTextureIndex].imageIndex].imgbuf=NULL; see TODO note.
					 It may be refered by other meshes, so do no reset to NULL.
                                         */

		                        /* Assign kn_scale */
                		        trimesh->mtlList[id].kn_scale=glMaterials[materialIndex].normalTexture.scale;
                		}

				/* M6. Assign triGroupList[].mtlID */
				trimesh->triGroupList[tgCount].mtlID=id;

			    } /* END if(mtID_found==false) */


///////////////////////////////////////////  uv for img kd  ///////////////////////////////////////////
				/* 11.3.4.1A Check img_kd */
                    		/* see 11.3.4.1 M3. Assign mtlList[].img_kd */
				//if( trimesh->mtlList[materialIndex].img_kd==NULL)
				if( baseColorTextureIndex <0 )
 				        egi_dpstd(DBG_RED"baseColorTextureIndex <0! Ignore triList[].vtx[].u/v .\n"DBG_RESET);

				/* 11.3.4.2 Assign triList[].vtx.u/v */
		                //else if(  trimesh->mtlList[materialIndex].img_kd!=NULL &&
				         //glMaterials[materialIndex].pbrMetallicRoughness.baseColorTexture.index>=0 ) {
				else {
				        egi_dpstd(DBG_YELLOW"Assign uv for img_kd, to triList[].vtx[].uv...\n"DBG_RESET);

                   			int textureIndex=glMaterials[materialIndex].pbrMetallicRoughness.baseColorTexture.index;

					/* 11.3.4.2.1 If texture image exists */
                   			if(textureIndex>=0 && glTextures[textureIndex].imageIndex>=0 ) {
					    /* 11.3.4.2.1.1  HK2023-01-03 */
					    int nn=glMaterials[materialIndex].pbrMetallicRoughness.baseColorTexture.texCoord;

					    //assume nn is OK, integrity checked in E3D_glLoadTextureInfo()
					    int texcoord=glMeshes[m].primitives[n].attributes.texcoord[nn]; //as Accessors index

					    /* 11.3.4.2.1.2 */
					    if( texcoord >-1 && texcoord < (int)glAccessors.size() ) {

egi_dpstd(DBG_YELLOW"Read img_kd u/v from glAccessors[texcoord(accindx)=%d], elemtype: %s, count=%d, elemsize=%d\n"DBG_RESET,
				texcoord, glAccessors[texcoord].type.c_str(), glAccessors[texcoord].count, glAccessors[texcoord].elemsize);

						/* 11.3.4.2.1.2.1 TODO: byteStripe of glAccessors[].data NOT considered */
						fdat=(float *)glAccessors[texcoord].data;

						/* Check element type */
						if(glAccessors[texcoord].type.compare("VEC2")) {
							egi_dpstd(DBG_RED"Critical: glAccessors[texcoord=%d].type:'%s', it's is NOT VEC2!\n"DBG_RESET,
									texcoord, glAccessors[texcoord].type.c_str());

							/* Clear vtxList/triList */
							trimesh->vcnt=0; trimesh->tcnt=0;  trimesh->triGroupList.clear();

							ret=-1;
							goto END_FUNC;
						}

						/* 11.3.4.2.1.2.2 */
			                        /* Get component size. component types: float, unsinged byte normalized, unsinged short normalized. */
                        			int compsize=glAccessors[texcoord].elemsize/2; /* for VEC2, number_of_components=2 */
						int compmax=(1<<(compsize<<3))-1; /* component max. value */
						float invcompmax=1.0/compmax;
						int fcnt=0; //count vertices in this primitive

			egi_dpstd("Vertice in this primitive: %d\n", vtxCount-Vsdx);  /* vtxCount updated for this primitive at 11.2.2 */

					    	/* 11.3.4.2.1.2.3 Texture coord: VEC2 float Textrue coordinates */
						/* Get and assign vtxList[].u/v */
						for(int j=Vsdx; j<vtxCount; j++) {  /* vtxCount updated for this primitive at 11.2.2 */
						    if(compsize==4) {  //component type: float
							 trimesh->vtxList[j].u=fdat[fcnt*2];
							/* Hahaha... same UV coord direction as EGI E3D :) */
							 trimesh->vtxList[j].v=fdat[fcnt*2+1];

						//	printf("trimesh->vtxList[%d].u/v:%f,%f\n",j, trimesh->vtxList[j].u, trimesh->vtxList[j].v);


			#if 0 /* TEST: --------- DO NOT addjust uv here! AFT pixels of triangle get interpolated uv....   ----- */
			if(trimesh->vtxList[j].u>1.0f || trimesh->vtxList[j].v>1.0f || trimesh->vtxList[j].u<0.0f || trimesh->vtxList[j].v<0.0f)
			{
				egi_dpstd(DBG_RED"For img_kd triList[].vtx[].uv: vtxList[].u/v(%f,%f) is NOT within [0 1]!\n"DBG_RESET,
										trimesh->vtxList[j].u, trimesh->vtxList[j].v);
				/* Try to adjust u/v to within [0 1); NOT HERE!!~ */
				 XXX E3D_wrapUV(trimesh->vtxList[j].u, trimesh->vtxList[j].v,0);
			}
			#endif

						    }
						    else if(compsize<4) { //component type: unsinged byte normalized, unsinged short normalized
							egi_dpstd(DBG_CYAN"TexCoordUV component type NOT float!\n"DBG_RESET);
							/* componet: u */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2)*compsize+kk];  //1byte incremental
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							trimesh->vtxList[j].u=uind*invcompmax;

							/* component: v */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2+1)*compsize+kk];
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							trimesh->vtxList[j].v=uind*invcompmax;
						    }

						    fcnt++; /* count vertices */

						} /* End for(j) */

						/* 11.3.4.2.1.2.4  TODO: NOW E3D_TriMesh::renderMesh() reads UV from triList[].vtx[].u/v */
						int vtxind;
				  		/* For img_kd: Store uv at triList[].vtx[].u/v  nn--- see 11.3.4.2.1.1 */
						for(int j=Tsdx; j<triCount; j++) {  /* triCount updated for this primitive at 11.3.2 */
						   for(int jj=0; jj<3; jj++) {
							vtxind=trimesh->triList[j].vtx[jj].index;
							if( vtxind<0 || vtxind>=trimesh->vcnt ) { // vtxCount) {
  egi_dpstd(DBG_RED"Error in parsing glMeshes[%zu].primitive[%zu]: vtxind out of range!\n"DBG_RESET, m,n);
  egi_dpstd(DBG_RED"Stop reading glMesh, with triMeshList.size()=%zu\n"DBG_RESET,triMeshList.size());
								vtxind=0;

								trimesh->triGroupList.clear(); /* Clear this trimesh */
								ret=-1;
								goto END_FUNC;
							}

							trimesh->triList[j].vtx[jj].u=trimesh->vtxList[vtxind].u;
							trimesh->triList[j].vtx[jj].v=trimesh->vtxList[vtxind].v;

						//	printf("trimesh->triList[%d].vtx[%d].u/v:%f,%f\n",
						//		j, jj, trimesh->triList[j].vtx[jj].u, trimesh->triList[j].vtx[jj].v);
						   }
						}
					    }
					}

				} /* END 11.3.4.2 Assign img_kd : triList[].vtx[].u/v */

///////////////////////////////////////////  uv for img_ke  ///////////////////////////////////////////

				/* 11.3.4.1A Check img_ke */
                    		/* see 10a. for mtList[].img_ke=glImages[].imgbuf */
				//if(mtlList[materialIndex].img_ke==NULL) /* NOW: glImages[].imgbuf is NULL! */
				if( emissiveTextureIndex <0 )
					egi_dpstd(DBG_RED"emissiveTextureIndex <0! Ignore mtlList[].triListUV_ke.\n"DBG_RESET);

				/* 11.3.4.2 Assign mtList[].triListUV_ke */
		                //else if(  mtlList[materialIndex].img_ke!=NULL &&
				     //glMaterials[materialIndex].emissiveTexture.index>=0 ) {
				//else if( trimesh->triGroupList[tgCount].mtlID >=0 ) {  //ALWAYS OK!
				else {
					/* 11.3.4.2.0 Resize mtList[].triListUV_ke to tcnt */
					int mtlID=trimesh->triGroupList[tgCount].mtlID;
					int tricnt=trimesh->triGroupList[tgCount].tricnt;
					trimesh->mtlList[mtlID].triListUV_ke.resize(tricnt);

				        egi_dpstd(DBG_YELLOW"Assign uv for img_ke, to mtList[].triListUV_ke...\n"DBG_RESET);

                   			int textureIndex=glMaterials[materialIndex].emissiveTexture.index;

					/* 11.3.4.2.1 If texture image exists */
                   			if(textureIndex>=0 && glTextures[textureIndex].imageIndex>=0 ) {
                        		    /* see 10a. for mtList[].img_kd=glImages[].imgbuf */
					    //if(mtList[materialIndex].img_kd==NULL) /* NOW: glImages[].imgbuf is NULL! Ownership changed.*/
					    /* 11.3.4.2.1.1  HK2023-01-03 */
					    int nn=glMaterials[materialIndex].emissiveTexture.texCoord;
					    //assume nn is OK, integrity checked in E3D_glLoadTextureInfo()
					    int texcoord=glMeshes[m].primitives[n].attributes.texcoord[nn]; //as Accessors index
					    if(texcoord<0)
					    	egi_dpstd(DBG_RED"--- img_ke texcoord(accIndex)=%d<0!\n"DBG_RESET,texcoord);
					    /* 11.3.4.2.1.2 */
					    if( texcoord >-1 && texcoord < (int)glAccessors.size() ) {

egi_dpstd(DBG_YELLOW"Read img_ke u/v from glAccessors[texcoord(accindx)=%d], elemtype: %s, count=%d, elemsize=%d\n"DBG_RESET,
				texcoord, glAccessors[texcoord].type.c_str(), glAccessors[texcoord].count, glAccessors[texcoord].elemsize);

						/* 11.3.4.2.1.2.1 TODO: byteStripe of glAccessors[].data NOT considered */
						fdat=(float *)glAccessors[texcoord].data;

						/* Check element type */
						if(glAccessors[texcoord].type.compare("VEC2")) {
							egi_dpstd(DBG_RED"Critical: glAccessors[texcoord=%d].type:'%s', it's is NOT VEC2!\n"DBG_RESET,
									texcoord, glAccessors[texcoord].type.c_str());

							/* Clear vtxList/triList */
							trimesh->vcnt=0; trimesh->tcnt=0;  trimesh->triGroupList.clear();

							ret=-1;
							goto END_FUNC;
						}

						/* 11.3.4.2.1.2.2 */
			                        /* Get component size. component types: float, unsinged byte normalized, unsinged short normalized. */
                        			int compsize=glAccessors[texcoord].elemsize/2; /* for VEC2, number_of_components=2 */
						int compmax=(1<<(compsize<<3))-1; /* component max. value */
						float invcompmax=1.0/compmax;
						int fcnt=0; //count vertices in this primitive

			egi_dpstd("Vertice in this primitive: %d\n", vtxCount-Vsdx);

					    	/* 11.3.4.2.1.2.3 Texture coord: VEC2 float Textrue coordinates */
						/* Get and assign vtxList[].u/v */
						for(int j=Vsdx; j<vtxCount; j++) {  /* traverse vertice index in this primitive */
						    if(compsize==4) {  //component type: float

							/***                  !!!--- CAUTION ---!!!
							 * Temp. save to vtxList[].u/v, later copy to mtlList[].triListUV_ke.
							 * see 11.3.4.2.1.2.4
							 */

							 trimesh->vtxList[j].u=fdat[fcnt*2];
							/* Hahaha... same UV coord direction as EGI E3D :) */
							 trimesh->vtxList[j].v=fdat[fcnt*2+1];

			//				printf("vtxList[%d].u/v:%f,%f\n",j,vtxList[j].u,vtxList[j].v);

			/* TEST: ---------- */
			if( trimesh->vtxList[j].u>1.0f || trimesh->vtxList[j].v>1.0f || trimesh->vtxList[j].u<0.0f || trimesh->vtxList[j].v<0.0f)
					egi_dpstd("For triListUV_ke: vtxList[].u/v(%f,%f) is NOT within [0 1]!\n",
									trimesh->vtxList[j].u, trimesh->vtxList[j].v);

							/* Try to adjust u/v to within [0 1]; TODO sampler NOT HERE! */
						    }
						    else if(compsize<4) { //component type: unsinged byte normalized, unsinged short normalized
							egi_dpstd(DBG_CYAN"TexCoordUV component type is NOT float!\n"DBG_RESET);
							/* componet: u */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2)*compsize+kk];  //1byte incremental
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							trimesh->vtxList[j].u=uind*invcompmax;

							/* component: v */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2+1)*compsize+kk];
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							trimesh->vtxList[j].v=uind*invcompmax;
						    }

						    fcnt++; /* count vertices */

						} /* End for(j) */

						/* 11.3.4.2.1.2.4  TODO: NOW E3D_TriMesh::renderMesh() reads UV from triList[].vtx[].u/v */
						int vtxind;
				  		/* For img_ke: Store uv mtlList[].triListUV_ke  nn--- see 11.3.4.2.1.1 */
						for(int j=Tsdx; j<triCount; j++) {
						   for(int jj=0; jj<3; jj++) {
							vtxind=trimesh->triList[j].vtx[jj].index;
							if(vtxind<0 || vtxind>=vtxCount)
								return -1;

							/* triGroupUV corresponding to triList[].vtx[] */
							trimesh->mtlList[mtlID].triListUV_ke[j].u[jj]=trimesh->vtxList[vtxind].u;
							trimesh->mtlList[mtlID].triListUV_ke[j].v[jj]=trimesh->vtxList[vtxind].v;
						   }
						}
					    }
					}

				} /* END 11.3.4.2 Assign uv for img_ke */

/////////////////////////////////////////////////////////////////////////////////////////////////

#if 0 //////<<<<<<<<<<< TEST_BREAK


///////////////////////////////////////////  uv for img_kn HK2022-01-24  ///////////////////////////////////////////

				/* 11.3.4.1A Check img_kn */
                    		/* see 10a. for mtList[].img_kn=glImages[].imgbuf */
				if(mtlList[materialIndex].img_kn==NULL) /* NOW: glImages[].imgbuf is NULL! */
 				        egi_dpstd(DBG_RED"mtList[materialIndex=%d].img_kn==NULL! Ignore triList[].vtx[].u/v .\n"DBG_RESET,
							materialIndex);

				/* 11.3.4.2 Assign mtList[].triListUV_kn */
		                else if(  mtlList[materialIndex].img_kn!=NULL &&
				     glMaterials[materialIndex].normalTexture.index>=0 ) {

					/* 11.3.4.2.0 Resize mtList[].triListUV_kn to tcnt */
					mtlList[materialIndex].triListUV_kn.resize(tcnt); //NOW: tcnt==triCount

				        egi_dpstd(DBG_YELLOW"Assign uv for img_kn, to mtList[].triListUV_kn...\n"DBG_RESET);

                   			int textureIndex=glMaterials[materialIndex].normalTexture.index;

					/* 11.3.4.2.1 If texture image exists */
                   			if(textureIndex>=0 && glTextures[textureIndex].imageIndex>=0 ) {
                        		    /* see 10a. for mtList[].img_kn=glImages[].imgbuf */
					    //if(mtList[materialIndex].img_kn==NULL) /* NOW: glImages[].imgbuf is NULL! Ownership changed.*/
					    /* 11.3.4.2.1.1 */
					    int nn=glMaterials[materialIndex].normalTexture.texCoord;
					    //assume nn is OK, integrity checked in E3D_glLoadTextureInfo()
					    int texcoord=glMeshes[m].primitives[n].attributes.texcoord[nn]; //as Accessors index
					    if(texcoord<0)
					    	egi_dpstd(DBG_RED"--- img_kn texcoord(accIndex)=%d<0!\n"DBG_RESET,texcoord);
					    /* 11.3.4.2.1.2 */
					    if( texcoord >-1 && texcoord < (int)glAccessors.size() ) {

egi_dpstd(DBG_YELLOW"Read img_kn u/v from glAccessors[texcoord(accindx)=%d], elemtype: %s, count=%d, elemsize=%d\n"DBG_RESET,
				texcoord, glAccessors[texcoord].type.c_str(), glAccessors[texcoord].count, glAccessors[texcoord].elemsize);

						/* 11.3.4.2.1.2.1 TODO: byteStripe of glAccessors[].data NOT considered */
						fdat=(float *)glAccessors[texcoord].data;

						/* Check element type */
						if(glAccessors[texcoord].type.compare("VEC2")) {
							egi_dpstd(DBG_RED"Critical: glAccessors[texcoord=%d].type:'%s', it's is NOT VEC2!\n"DBG_RESET,
									texcoord, glAccessors[texcoord].type.c_str());

							/* Clear vtxList/triList */
							vcnt=0; tcnt=0;  triGroupList.clear();

							return -1;
						}

						/* 11.3.4.2.1.2.2 */
			                        /* Get component size. component types: float, unsinged byte normalized, unsinged short normalized. */
                        			int compsize=glAccessors[texcoord].elemsize/2; /* for VEC2, number_of_components=2 */
						int compmax=(1<<(compsize<<3))-1; /* component max. value */
						float invcompmax=1.0/compmax;
						int fcnt=0; //count vertices in this primitive

			egi_dpstd("Vertice in this primitive: %d\n", vtxCount-Vsdx);

					    	/* 11.3.4.2.1.2.3 Texture coord: VEC2 float Textrue coordinates */
						/* Get and assign vtxList[].u/v */
						for(int j=Vsdx; j<vtxCount; j++) {  /* traverse vertice index in this primitive */
						    if(compsize==4) {  //component type: float
							 vtxList[j].u=fdat[fcnt*2];
							/* Hahaha... same UV coord direction as EGI E3D :) */
							 vtxList[j].v=fdat[fcnt*2+1];

			//				printf("vtxList[%d].u/v:%f,%f\n",j,vtxList[j].u,vtxList[j].v);

				/* TEST: ---------- uv NOT within [0 1] */
				if(vtxList[j].u>1.0f || vtxList[j].v>1.0f || vtxList[j].u<0.0f || vtxList[j].v<0.0f)
					egi_dpstd("For triListUV_kn: vtxList[].u/v(%f,%f) is NOT within [0 1]!\n",vtxList[j].u,vtxList[j].v);

							/* Try to adjust u/v to within [0 1]; TODO sampler. NOT HERE! */

						    }
						    else if(compsize<4) { //component type: unsinged byte normalized, unsinged short normalized
							egi_dpstd(DBG_CYAN"TexCoordUV component type is NOT float!\n"DBG_RESET);
							/* componet: u */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2)*compsize+kk];  //1byte incremental
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							vtxList[j].u=uind*invcompmax;

							/* component: v */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2+1)*compsize+kk];
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							vtxList[j].v=uind*invcompmax;
						    }

						    fcnt++; /* count vertices */

						} /* End for(j) */

						/* 11.3.4.2.1.2.4  TODO: NOW E3D_TriMesh::renderMesh() reads UV from triList[].vtx[].u/v */
						int vtxind;
				  		/* For img_kn: Store uv mtlList[].triListUV_kn  nn--- see 11.3.4.2.1.1 */
						for(int j=Tsdx; j<triCount; j++) {
						   for(int jj=0; jj<3; jj++) {
							vtxind=triList[j].vtx[jj].index;
							if(vtxind<0 || vtxind>=vtxCount)
								return -1;

							/* triGroupUV corresponding to triList[].vtx[] */
							mtlList[materialIndex].triListUV_kn[j].u[jj]=vtxList[vtxind].u;
							mtlList[materialIndex].triListUV_kn[j].v[jj]=vtxList[vtxind].v;
						   }
						}
					    }
					}

				} /* END 11.3.4.2 Assign uv for img_kn */
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif  /////<<<<<<<<<<<<<<<< TEST_BREAK

			}  /* END 11.3.4  Set material for trigroup */

			/* 11.3.4 Load morph weights from primitie to corresponding trimesh.trigroup */
			size_t wsize=glMeshes[m].mweights.size();
			size_t msize=glMeshes[m].primitives[n].morphs.size();
			int positionAccIndex, normalAccIndex;
			//float *fdat; defined at 11.
			if(msize>0 ) {
				/* Reset trigroup morphs accordingly */
				trimesh->triGroupList[tgCount].morphs.resize(msize);

				for(size_t mk=0; mk<msize; mk++) {
				   /* POSITION   for each primitive.morphs[]. Displacement for each primitive vertex position. */
				   positionAccIndex = glMeshes[m].primitives[n].morphs[mk].positionAccIndex;
				   if( positionAccIndex >-1 ) {
				   	/* Check accessor data */
					if( glAccessors[positionAccIndex].componentType!=5126) { /* component: float */
						egi_dpstd(DBG_RED"morph POSITION accessor data type error!\n"DBG_RESET);
					}
					else if( glAccessors[positionAccIndex].count < primitiveVtxCnt) { /* element: VE3 */
						egi_dpstd(DBG_RED"morph POSITION accessor data length error!\n"DBG_RESET);
					}
					else {
						/* Get float data pointer */
						fdat=(float *)glAccessors[positionAccIndex].data;

						/* Resize  trigroup.morphs.positions. here triGroupList[].morphs[].postions[] is E3D_Vector array */
						trimesh->triGroupList[tgCount].morphs[mk].positions.resize(primitiveVtxCnt);

						/* Assign trigroup.morphs.positions */
						for(int pk=0; pk<primitiveVtxCnt; pk++) {  /* Reas 3 components(float) each time */
							trimesh->triGroupList[tgCount].morphs[mk].positions[pk].x= *(fdat+pk*3);
							trimesh->triGroupList[tgCount].morphs[mk].positions[pk].y= *(fdat+pk*3+1);
							trimesh->triGroupList[tgCount].morphs[mk].positions[pk].z= *(fdat+pk*3+2);
						}
					}
				   }
				   /* NORMAL  for each primitive.morphs[]. Displacement for each primitive vertex normal. */
				   normalAccIndex = glMeshes[m].primitives[n].morphs[mk].normalAccIndex;
				   if( normalAccIndex >-1 ) {
				   	/* Check accessor data */
					if( glAccessors[normalAccIndex].componentType!=5126) { /* component: float */
						egi_dpstd(DBG_RED"morph NORMAL accessor data type error!\n"DBG_RESET);
					}
					else if( glAccessors[normalAccIndex].count < primitiveVtxCnt) {  /* Element VEC3 */
						egi_dpstd(DBG_RED"morph NORMAL accessor data length error!\n"DBG_RESET);
					}
					else {
						/* Get float data pointer */
						fdat=(float *)glAccessors[normalAccIndex].data;

						/* Resize  trigroup.morphs.normals. */
						trimesh->triGroupList[tgCount].morphs[mk].normals.resize(primitiveVtxCnt);

						/* Assign trigroup.morphs.normals */
						for(int pk=0; pk<primitiveVtxCnt; pk++) {  /* Reas 3 components(float) each time */
							trimesh->triGroupList[tgCount].morphs[mk].normals[pk].x= *(fdat+pk*3);
							trimesh->triGroupList[tgCount].morphs[mk].normals[pk].y= *(fdat+pk*3+1);
							trimesh->triGroupList[tgCount].morphs[mk].normals[pk].z= *(fdat+pk*3+2);
						}
					}

                                   }

				   /* TODO: TANGENT,TEXCOORD,COLOR */
				}
			}


#if 0 /* TEST: Hide triGroup which has no baseColorFactor defined ------------------- */
			if(triGroupList[tgCount].mtlID>-1) {
			    if(!glMaterials[triGroupList[tgCount].mtlID].pbrMetallicRoughness.baseColorFactor_defined)
                        	triGroupList[tgCount].hidden=true;
			}
			/* OR has no mtlID, USUally not */
			if(triGroupList[tgCount].mtlID<0)
				triGroupList[tgCount].hidden=true;
#endif //---------------------------------------------------------------------------

			/* 11.3.5 tgCount incremental at last! */
			tgCount +=1;

		} /* END if(mode==4 && accIndex>=0 ) */


	    } /* End 11.0 for(n) traverse primitives */

	    /* 11.1 update triface mormals */
	    trimesh-> updateAllTriNormals();

	    /* 11.2 Summit for each mesh */
	    egi_dpstd(DBG_MAGENTA"      --- triMeshList[%zu] Summit ---\n"DBG_RESET, m);
	    egi_dpstd("  Pre_count %d triangles, %d vertices, and %d triGroups \n", triCountBk, vtxCountBk, tgCountBk);
	    egi_dpstd("  Totally read %d triangles, %d vertices, and %d triGroups \n", triCount, vtxCount, tgCount);
	    egi_dpstd("  Space with tCapacity=%d, vCapacity=%d\n", trimesh->triCapacity(), trimesh->vtxCapacity());
	    egi_dpstd("  backFaceOn: %s\n",  trimesh->backFaceOn ? "True":"False");
	    egi_dpstd("  Materials:\n");
	  for( size_t k=0; k < trimesh->mtlList.size(); k++) {
	    egi_dpstd("      name: '%s'\n", trimesh->mtlList[k].name.c_str());
	    egi_dpstd("      img_kd: %s, img_ke: %s, img_kn: %s\n",
		      trimesh->mtlList[k].img_kd?"YES":"NO", trimesh->mtlList[k].img_ke?"YES":"NO",trimesh->mtlList[k].img_kn?"YES":"NO" );
          }

          for( size_t k=0; k < trimesh->triGroupList.size(); k++) {
		egi_dpstd(DBG_MAGENTA"  triMeshList[%zu].triGroupList[%zu]\n"DBG_RESET, m, k);
		egi_dpstd("     name: %s\n",trimesh->triGroupList[k].name.c_str());
		egi_dpstd("     tricnt: %d\n",trimesh->triGroupList[k].tricnt);
		egi_dpstd("     vtxcnt: %d\n",trimesh->triGroupList[k].vtxcnt);
		egi_dpstd("     mtlID: %d\n",trimesh->triGroupList[k].mtlID);
		egi_dpstd("     backFaceON: %s\n", trimesh->triGroupList[k].backFaceOn ? "True":"False");
	  }


	} /* End 11.-1 for(m) traverse meshes */


#if 1	/* TEST: ---- check results triMeshList[] */
	for(size_t k=0; k<triMeshList.size(); k++) {

   #if 0 /* TEST: ----- Add one instatnce for each triMeshList.  */
		addMeshInstance(*triMeshList[k], "_instance");
   #endif

		/* Print mtlList */
		for(size_t kk=0; kk<triMeshList[k]->mtlList.size(); kk++) {
			egi_dpstd(DBG_MAGENTA"      --- triMeshList[%zu]->mtlList[%zu] ---\n"DBG_RESET, k,kk);
			triMeshList[k]->mtlList[kk].print();
		}

   #if 0  /* TEST: ------- vtx UVs */
		for(int j=0; j < triMeshList[k]->tcnt; j++) {
		    for(size_t jj=0; jj<3; jj++) {
                         printf("triMeshList[%d]->triList[%d].vtx[%d].u/v:%f,%f\n",
                                 k, j, jj, triMeshList[k]->triList[j].vtx[jj].u, triMeshList[k]->triList[j].vtx[jj].v);
		    }
		}
   #endif

   #if 0 /* TEST: print out -------- vtx Normals */
		for(int j=0; j < triMeshList[k]->vcnt; j++) {
			 triMeshList[k]->vtxList[j].normal.normalize();
                          printf("triMeshList[%d]->vtxList[%d].normal: %f,%f,%f\n", k,j,
			         triMeshList[k]->vtxList[j].normal.x, triMeshList[k]->vtxList[j].normal.y, triMeshList[k]->vtxList[j].normal.z);
		}
   #endif

   #if 0 /* TEST: print out triGroupList[].skins -------------------- */
	   if( glMeshes[k].skinIndex >=0 ) {
	        egi_dpstd(DBG_MAGENTA"	--- triMeshList[%zu].skins ---\n"DBG_RESET, k);
	     #if 0 ///////////////////
		printf("	OK, triMeshList[%zu]  has skin defined!\n", k);
	     #else ////////////////////
		for(size_t i=0; i<triMeshList[k]->triGroupList.size(); i++) {
		    printf("triGroupList[%zu]:\n", i);
		    for(size_t j=0; j<triMeshList[k]->triGroupList[i].vtxcnt; j++) {
			printf("    vtx[%d]: weights[%f, %f, %f, %f], jointNodes[%d, %d, %d, %d]\n",
					triMeshList[k]->triGroupList[i].stidx+j,
					triMeshList[k]->triGroupList[i].skins[j].weights[0],
					triMeshList[k]->triGroupList[i].skins[j].weights[1],
					triMeshList[k]->triGroupList[i].skins[j].weights[2],
					triMeshList[k]->triGroupList[i].skins[j].weights[3],
					triMeshList[k]->triGroupList[i].skins[j].jointNodes[0],
					triMeshList[k]->triGroupList[i].skins[j].jointNodes[1],
					triMeshList[k]->triGroupList[i].skins[j].jointNodes[2],
					triMeshList[k]->triGroupList[i].skins[j].jointNodes[3]    );
		    }
		}
	     #endif //////////////////////
	   }
	   else
		printf("   glMeshes[%d].skinIndex=%d, No skin.\n", k, glMeshes[k].skinIndex);

   #endif

   #if 0 /* TEST: -------- backFaceOn */
		printf("triMeshList[0]->backFaceOn: %s\n", triMeshList[0]->backFaceOn ? "True" : "False");
		printf("triMeshList[0]->triGroupList[0].backFaceOn: %s\n", triMeshList[0]->triGroupList[0].backFaceOn ? "True" : "False");
   #endif


	}
#endif



	/* 12. Load animation data into glNodes.animQtsStacks and glNodes.animMorphWtsStacks
	 * Note:
		1. TODO NOW only extract glAnimations[0]

	  glAnimation ---> glAnimChannels  ---> glAnimChanTarget ---> node and TRS type.
           		                |
                        	        |---> glAnimChanSampler ---> accessor indices for keyframe timestamps and TRS data.
	 ***/

        /* Resize glNodes[].animQtsStacks and glNodes[].animMorphwtsStacks.
	 * Note: Allocate capacity for both items, though there's data ONLY for animQts or animMorphwts!
	 */
	animTotal=glAnimations.size();
	if(animTotal>0) {
		for(size_t ka=0; ka<glNodes.size(); ka++) {
			glNodes[ka].animQtsStacks.resize(animTotal);
			glNodes[ka].animMorphwtsStacks.resize(animTotal);
		}
	}

	egi_dpstd(DBG_MAGENTA"  --- Start to load animation data into glNodes.animQtsStacks/animMorphWtsStack...\n"DBG_RESET);
	egi_dpstd("  glAnimations.size()=%zu\n",glAnimations.size());
	sleep(2);

	for( size_t m=0; m<glAnimations.size(); m++)
	{
/* TODO: NOW ONLY first ANIMATION */
//		if(m==1)break; /* <--------------------ONLY FIRST ANIMATION */

		int node;  /* node index */
		int pathType; /* TRS type */
		int samplerIndex;
		int input,output;  /* input/output as index of accessor to timestamps and SRT data */

		int mwsize;	   /* size of mesh.weights[] */
		vector<float> weights; /* weights.size()=mwsize, resize see 12.1.18 */

		float ts;    /* timestamp */
		E3D_Vector sxyz;  /* Scale */
		E3D_Quaternion q; /* Rotation */
		E3D_Vector txyz;  /* Translation */

		egi_dpstd(DBG_MAGENTA"Read animation data for glAnimation_%zu/(%zu-1) '%s' ...\n"DBG_RESET,
									m, glAnimations.size(), glAnimations[m].name.c_str());
		sleep(1);

		/* 12.1 Traverse channels, each channel refers to a node */
		for( size_t n=0; n<glAnimations[m].channels.size(); n++) {

			/* 12.1.1 Get node,pathType,samplerIndex */
			node=glAnimations[m].channels[n].target.node;
			pathType=glAnimations[m].channels[n].target.pathType;
			samplerIndex=glAnimations[m].channels[n].samplerIndex;

			/* 12.1.1A If node is a mesh, get size of mesh.weights */
			mwsize=0; /* Reset first */
			if(glNodes[node].type==glNode_Mesh) {
				if(glNodes[node].meshIndex>=0 && glNodes[node].meshIndex<(int)glMeshes.size() )
					mwsize=glMeshes[glNodes[node].meshIndex].mweights.size();
				//else
				//	mwsize=0;
			}
//			egi_dpstd("glAnimations[%d].channels[%d].target.node: %d\n",m,n, node);
			if(mwsize>0) /* If has morph weights */
			    egi_dpstd("glAnimations[%zu].channels[%zu]: Mesh Morph weights.size=%d\n",m,n,mwsize);

			/* 12.1.1B resize weights according to glMeshes[].mweights.size() */
			if(mwsize>0) {
				weights.resize(mwsize);
			}

			/* 12.1.2 Check integrity */
			if( node<0 || node> (int)glNodes.size()-1
			    || pathType<0 || pathType >3
			    || samplerIndex<0 || samplerIndex >= (int)glAnimations[m].samplers.size() )
			{
				egi_dpstd(DBG_RED"Integrity error! node=%d,pathType=%d,samplerIndex=%d\n"DBG_RESET,
								node, pathType, samplerIndex );
				continue; /* Ignore this channel */
			}

			/* 12.1.3 Get accessor index */
			input=glAnimations[m].samplers[samplerIndex].input;
			output=glAnimations[m].samplers[samplerIndex].output;
			if(input<0 || input>=(int)glAccessors.size()
			   || output<0 || output>=(int)glAccessors.size() )
			{
				egi_dpstd(DBG_RED"Accessor index error! input=%d, ouput=%d, glAccessors.size=%zu\n"DBG_RESET,
							input,output, glAccessors.size() );

				continue; /* Ignore this channel */
			}

#if 1 			/* 12.1.4 Check input/output elements number--- NOT nessary! NOT here */
			if( pathType!=ANIMPATH_WEIGHTS && glAccessors[input].count != glAccessors[output].count ) {
				egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: input/output DO NOT has same glAccssors[].count!\n"DBG_RESET, m,n);
				egi_dpstd(DBG_RED"glAccessors[input].count=%d, glAccessors[output].count=%d\n"DBG_RESET,
							glAccessors[input].count, glAccessors[output].count);
				continue; /* Ignore this channel */
			}
#endif
			/* 12.1.5 Check input/output component type */
			/* TODO: Only support float component_type, signed byte normalized/unsigned byte normalized ... NOT supported */
			if( glAccessors[input].componentType != 5126 || glAccessors[output].componentType != 5126 ) {
				egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: input/output componentType is NOT float!\n"DBG_RESET, m,n);
				continue; /* Ignore this channel */
			}

			/* 12.1.6. Check input element type */
			if( glAccessors[input].type.compare("SCALAR") ) {
				egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: input element type is NOT SCALAR!\n"DBG_RESET,m,n);
				continue; /* Ignore this channel */
			}
			/* 12.1.7 Check output element type */
			bool dataError=false;
//			egi_dpstd(DBG_YELLOW"glAnimations[%d].channels[%d]: pathType=%d\n"DBG_RESET, m,n, pathType);
			switch (pathType) {
				case ANIMPATH_WEIGHTS:
				   if( glAccessors[output].type.compare("SCALAR") ) {
                                   	egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: pathtype 'weights' data is NOT SCALAR!\n"DBG_RESET,m,n);
				   	dataError=true;
				   }
				   break;
				case ANIMPATH_TRANSLATION:
				   if( glAccessors[output].type.compare("VEC3") ) {
                                         egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: pathtype 'translation' data is NOT VEC3!\n"DBG_RESET,m,n);
	                                 dataError=true;
				   }
                                   break;
				case ANIMPATH_ROTATION:
				   if( glAccessors[output].type.compare("VEC4") ) {
                                   	egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: pathtype 'rotation' data is NOT VEC4!\n"DBG_RESET,m,n);
	                                dataError=true;
				   }
                                   break;
				case ANIMPATH_SCALE:
				   if( glAccessors[output].type.compare("VEC3") ) {
                                        egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: pathtype 'scale' data is NOT VEC3!\n"DBG_RESET,m,n);
	                                dataError=true;
				   }
                                   break;
				default: /* pathtype error, OK check in  12.1.2 */
				   egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: pathtype=%d is NOT supported!\n"DBG_RESET,m,n,pathType);
				   dataError=true;
				   break;
			}

			/* 12.1.8 Ignor if dataError */
			if(dataError)
				continue; /* Ignore this channel */

			/* 12.1.9 Get data pointer */
			float *inputData=(float *)glAccessors[input].data;  /* component float ONLY */
			float *outputData=(float *)glAccessors[output].data; /* component float ONLY */

if(glAccessors[input].count != glAccessors[output].count)
	printf(DBG_RED" glAccessors[(%d)input].count != glAccessors[(%d)output].count!\n"DBG_RESET, input, output);

//printf(DBG_MAGENTA"glAnimations[%d].channels[%d]: glAccessors[input].componentType=%d! glAccessors[output].componentType=%d\n"DBG_RESET,
//			 		m, n, glAccessors[input].componentType, glAccessors[output].componentType);

			if(glAccessors[output].normalized) {
printf(DBG_RED"glAccessors[%d].normalized! componentType=%d\n"DBG_RESET, output, glAccessors[output].componentType);
			}

			/* 12.1.10 Get TRS and Morph_Weiths data */
			for( int cnt=0; cnt<glAccessors[input].count; cnt++) {  /* Frame count */
				/* 12.1.10.1 Get timestamp value */
				ts=inputData[cnt];

				/* 12.1.10.1a Update glNodes[node].animQtsStacks[m].tsmin/tsmax HK2023-04-06/07 */
				if(pathType==ANIMPATH_WEIGHTS) {
				    if(ts > glNodes[node].animMorphwtsStacks[m].tsmax)
					glNodes[node].animMorphwtsStacks[m].tsmax=ts;
				    else if(ts < glNodes[node].animMorphwtsStacks[m].tsmin)
					glNodes[node].animMorphwtsStacks[m].tsmin=ts;
				}
				else {   /* Case SRT Animations */
				    if(ts > glNodes[node].animQtsStacks[m].tsmax)
					glNodes[node].animQtsStacks[m].tsmax=ts;
				    else if(ts < glNodes[node].animQtsStacks[m].tsmin)
					glNodes[node].animQtsStacks[m].tsmin=ts;
				}

				/* 12.1.10.2 Get SRT parts and morph_weights */
				switch (pathType) {
					case ANIMPATH_WEIGHTS:
						/* MUST be same size as glMesh.weights.size(): mwsize */
						if(glAccessors[output].count < mwsize*glAccessors[input].count ) {
							egi_dpstd(DBG_RED"glAccessors[output].count NOT enought for weigths array!\n"DBG_RESET);
							break;
						}

						/* Read out weights for one frame */
						for(int kw=0; kw<mwsize; kw++) {
							weights[kw]=outputData[mwsize*cnt+kw];
						}

						/* Insert frame weights to node.animMorphwts */
						glNodes[node].animMorphwtsStacks[m].insert(ts, weights);

						break;
					case ANIMPATH_TRANSLATION:
						/* [tx ty tz] */
						txyz.x=outputData[3*cnt];
						txyz.y=outputData[3*cnt+1];
						txyz.z=outputData[3*cnt+2];

						/* Insert to node.animQts */
						glNodes[node].animQtsStacks[m].insertSRT(ts, "T", sxyz, q, txyz);

						break;
					case ANIMPATH_ROTATION:
						/* [x y z w] */
						q.x=outputData[4*cnt];
						q.y=outputData[4*cnt+1];
						q.z=outputData[4*cnt+2];
						q.w=outputData[4*cnt+3];

						/* Insert to node.animQts */
						glNodes[node].animQtsStacks[m].insertSRT(ts, "R", sxyz, q, txyz);
						break;

					case ANIMPATH_SCALE:
						/* [sx sy sz] */
						sxyz.x=outputData[3*cnt];
						sxyz.y=outputData[3*cnt+1];
						sxyz.z=outputData[3*cnt+2];

						/* Insert to node.animQts */
						glNodes[node].animQtsStacks[m].insertSRT(ts, "S", sxyz, q, txyz);
						break;
				}
			}

		} /* End 12.1 for(n) */

		/* 12.2 TODO: Check and rectify animation animQtsStacks[].qts 
			--- CROSS CHECK with loadGLB():12.2 ---
		 */

#if 1 /* TEST: print out glNodes[k].animQtsStacks[m] ------------- */
        	egi_dpstd(DBG_MAGENTA"      --- Animation[%zu] Summit ---\n"DBG_RESET, m);
		printf("   --- tsmin=%f, tsmax=%f ---\n", tsmin, tsmax);
		for(size_t k=0; k<glNodes.size(); k++) {
			printf("   --- glNodes[%zu].animQtsStacks[%zu] ---\n", k, m);
			if(!glNodes[k].animQtsStacks[m].ts.size()) printf("      Empty!\n");
			else glNodes[k].animQtsStacks[m].print();

			printf("   --- glNodes[%zu].animMorphwtsStacks[%zu] ---\n", k,m);
			if(!glNodes[k].animMorphwtsStacks[m].ts.size()) printf("      Empty!\n");
			else glNodes[k].animMorphwtsStacks[m].print();

		}

#endif

	} /* End 12. for(m) */



END_FUNC:  /* Note:   !!!---  Caution ---!!!
	    *  After 11.3.4.1 (assign mtlList[].img_xx), any case with ret<0 MUST jump here to
            *  transfer Ownership of all glImages[].imgbuf to THIS.imgbufs!
            */
        if(ret!=0) {
		egi_dpstd(DBG_RED"Error happens when import glTF into E3D_Scene!\n"DBG_RESET);
	}

	/* Free */
	egi_fmap_free(&fmap);

	/* !!! ---CAUTION --- !!!  Transfer Ownership to  scene->imgbufs, see TODO note 3. */
	for(size_t k; k<glImages.size(); k++) {
		imgbufs.push_back(glImages[k].imgbuf);

		if(imgbufs[k])
		    egi_dpstd("glImages[%zu].imgbuf W*H: %d*%d\n", k, imgbufs[k]->width, imgbufs[k]->height);
		else
		    egi_dpstd(DBG_YELLOW"glImages[%zu].imgbuf is NULL!\n"DBG_RESET,k);
	}

	/* Reset all imgbuf in glImages before exit, Ownership already transfered to scene->imgbufs */
	for(size_t k; k<glImages.size(); k++) {
		glImages[k].imgbuf=NULL;
	}

	return ret;

}


/*------------------------------------------------------
Load a GLB(binary glTF) file into the E3D_Scene.
A glb file is stored in little endian.


                 ===== GBL Layout =====

A GLB file is stored in little endian, it consists of:
1. 12-byte header:
        magic(uint32)+version(uint32)+length(uint32)
2. Chunk 0(JSON):
	chunkLength(unit32)+chunkType(unit32)+chunkData(ubyte[])
3. chunk 1(Binary buffer):
	chunkLength(uint32)+chunType(uint32)+chunkData(ubyte[])
4. Other_selfdefined_chunks.



Note:
1. Refer to  E3D_Scene::loadGLTF() for more details.
2. A buffer with data provided by the GLB-stored BIN chunk,
   MUST be the first element of buffers array and it MUST
   have its buffer.uri property undefined.
3. If a GLB-stored BIN buffer is defined, then MMAP fmap of
   the GLB file will be carried by glBuffer[0].fmap, until
   it calls ~E3D_glBuffer().
4. Except above mentioned, other codes are SAME as loadGLTF().

TODO:
1. NOW ONLY support chunk0 and chunk1, Other USER defined
   chunks are ignored!

@fgl:	Path to a glTF Json file.

Return:
	0	OK
	<0	Fails
-------------------------------------------------------*/
int E3D_Scene::loadGLB(const string & fgl)
{
	int ret=0;
	char *pjson;
	string key;
	string strValue;

        vector<E3D_glBuffer> glBuffers;
        vector<E3D_glBufferView> glBufferViews;
        vector<E3D_glAccessor> glAccessors;
        vector<E3D_glMesh> glMeshes;
	vector<E3D_glMaterial> glMaterials;
	vector<E3D_glTexture> glTextures;
	vector<E3D_glImage> glImages;
	vector<E3D_glSampler> glSamplers;

	vector<E3D_glSkin> glSkins;
	vector<E3D_glAnimation> glAnimations;

	/* --- E3D_Scene Memebers --- */
//	vector<E3D_glNode> glNodes;
//	vector<E3D_glScene> glScenes;

	int triCount=0; /* Count total triangles */
	int vtxCount=0; /* Count total vertices */
	int tgCount=0;  /* Count total triGroup */

	int triCountBk,vtxCountBk,tgCountBk;

	/* GLB file header */
	struct glb_file_header {
		uint32_t magic;
		uint32_t version;
		uint32_t length;
	} GlbFileHeader;

	/* Chunk length */
	uint32_t chunk0_length;  /* Length of chunkdata */
	uint32_t chunk0_type;    /* JSON */
	uint32_t chunk1_length;  /* Length of chunkdata */
	uint32_t  chunk1_type;   /* Binary buffer */


	/* Check whether file exists */
	if( access(fgl.c_str(), F_OK) !=0 ) {
		egi_dpstd(DBG_RED"File '%s' does NOT exist!\n"DBG_RESET, fgl.c_str());
		return -1;
	}

	/* 0. Check to ensure THIS is an empty E3D_Scene */
	if(triMeshList.size()>0 || meshInstanceList.size()>0 ) {
		egi_dpstd(DBG_RED"THIS E3D_Scene must be empty, triMeshList/meshInstanceList is NOT empty!\n"DBG_RESET);
		return -1;
	}
	if(glNodes.size()>0 || glScenes.size()>0) {
		egi_dpstd(DBG_RED"THIS E3D_Scene must be empty, glNodes/glScene is NOT empty!\n"DBG_RESET);
                return -1;
	}


////////////////// Load  GLB file to glBuffers/glBufferViews/glAccessors/glMeshes /////////////


	/* 1. Mmap glTF Json file */
	EGI_FILEMMAP *fmap=NULL;  /* Ownership will be transfered to glBuffer[0].fmap. */
	fmap=egi_fmap_create(fgl.c_str(), 0, PROT_READ, MAP_SHARED);
	if(fmap==NULL)
		return -1;

	/* 1A. Read GLB header */
	if(fmap->fsize < (off_t)sizeof(GlbFileHeader)) {
		egi_dpstd(DBG_RED"GLB file ERROR!\n"DBG_RESET);
		egi_fmap_free(&fmap);
		return -1;
	}
	memcpy(&GlbFileHeader, fmap->fp, sizeof(GlbFileHeader));
	egi_dpstd("Magic: 0x%08X, Version: 0x%08X, Length: %uBytes\n",
					GlbFileHeader.magic, GlbFileHeader.version, GlbFileHeader.length);
	/* Magic words: 0x46546C67 */

	/* 1B. Check file length */
	if(GlbFileHeader.length!=fmap->fsize) {
		egi_dpstd(DBG_RED"GLB file length ERROR!\n"DBG_RESET);
		egi_fmap_free(&fmap);
		return -1;
	}

	/* 1C. Get chunk0_length and chunk1_legnth */
	memcpy(&chunk0_length, fmap->fp+12, sizeof(chunk0_length));
	if(chunk0_length+24>fmap->fsize) {
		egi_dpstd(DBG_RED"chunk0_length ERROR!\n"DBG_RESET);
		egi_fmap_free(&fmap);
		return -1;
	}
	memcpy(&chunk1_length, fmap->fp+20+chunk0_length, sizeof(chunk1_length));
	if(chunk1_length+chunk0_length+28 != fmap->fsize) {
		egi_dpstd(DBG_RED"chunk0_length(%u)+chunk1_length(%u)+28 != fmap->fsize(%jd)!\n"DBG_RESET,
					chunk0_length, chunk1_length, fmap->fsize);
		egi_fmap_free(&fmap);
		return -1;
	}
	egi_dpstd("chunk0_length: %dBytes, chunk1_length: %dBytes\n", chunk0_length, chunk1_length);


	/* 1D. Get chunkTypes */
	memcpy(&chunk0_type, fmap->fp+16, sizeof(chunk0_type));
	memcpy(&chunk1_type, fmap->fp+24+chunk0_length, sizeof(chunk1_type));
	egi_dpstd(DBG_GREEN"Chunk0_type: 0x%08X,   chunk1_type: 0x%08X\n"DBG_RESET, chunk0_type, chunk1_type);
	/* Type JSON: 0x4E4F534A, Type BIN: 0x004E4942 */
	/* Note: Client implementations MUST ignore chunks with unknown types to enable glTF extensions
         * to reference additional chunks with new types following the first two chunks.
	 */

	/* 1E. Get psjon */
	pjson=fmap->fp+(12+8);

#if 1 /* TEST: write json to tmp. file */
	remove("/tmp/__glb__.gltf");
	egi_append_file("/tmp/__glb__.gltf", pjson,chunk0_length);
#endif

	/* 2. Parse Top_Level glTF Jobject arrays, and load to respective glObjects */
	while(  pjson-fmap->fp < (20+(int)chunk0_length)
		&& (pjson=E3D_glReadJsonKey(pjson, key, strValue))  ) {

		//cout << DBG_GRAY"Key: "<< key << DBG_RESET << endl;
		//cout << DBG_GRAY"Value: "DBG_RESET<< strValue << endl;

		/* Case glTF Jobject: buffers */
		if(!key.compare("buffers")) {
			printf("\n=== buffers ===\n");

			/* Load glBuffers as per Json descript */
			if( E3D_glLoadBuffers(strValue, glBuffers)<0 )
				return -1;

			egi_dpstd(DBG_GREEN"Totally %zu E3D_glBuffers loaded.\n"DBG_RESET, glBuffers.size());
			char strtmp[128];
			size_t strlen;
			for(size_t k=0; k<glBuffers.size(); k++) {
				strlen=glBuffers[k].uri.copy(strtmp,64,0);
				strtmp[strlen]='\0';
				if(glBuffers[k].uri.size()>64) 	strcpy(strtmp+64, "...");
				printf("glBuffers[%zu]: byteLength=%d, uri='%s'\n", k, glBuffers[k].byteLength, strtmp);
				/* Check buffer data */
				if(glBuffers[k].byteLength>0 && glBuffers[k].data==NULL && glBuffers[k].uri.empty() && k==0) {
					/* This is the ONLY GLB-stored BIN chunk.
				         * A buffer with data provided by the GLB-stored BIN chunk, MUST be the first
					 * element of buffers array and it MUST have its buffer.uri property undefined.
					 */

					/* Assign data, here k==0 */
					glBuffers[k].data=(unsigned char*)fmap->fp+28+chunk0_length;
					glBuffers[k].fmap=fmap;
					glBuffers[k].GLBstored=true;

					//egi_dpstd(DBG_GREEN"glBuffers[%d].data==NULL, while byteLength>0!\n"DBG_RESET, k);
					//return -1;
				}
			}
		}
		/* Case glTF Jobject: bufferViews */
		else if(!key.compare("bufferViews")) {
			printf("\n=== bufferViews ===\n");

			/* Load data to glBufferViews */
			if( E3D_glLoadBufferViews(strValue, glBufferViews)<0 )
				return -1;

			printf(DBG_YELLOW"Totally %zu E3D_glBufferViews loaded.\n"DBG_RESET, glBufferViews.size());
			for(size_t k=0; k<glBufferViews.size(); k++) {
			    printf("glBufferViews[%zu]: name='%s', bufferIndex=%d, byteOffset=%d, byteLength=%d, byteStride=%d, target=%d.\n",
				k, glBufferViews[k].name.c_str(),
				glBufferViews[k].bufferIndex, glBufferViews[k].byteOffset, glBufferViews[k].byteLength,
				glBufferViews[k].byteStride, glBufferViews[k].target);
			}
		}
		/* Case glTF Jobject: accessors */
		else if(!key.compare("accessors")) {
			printf("\n=== accessors ===\n");

			/* Load data to glAccessors */
			if( E3D_glLoadAccessors(strValue, glAccessors)<0 )
				return -1;

#if 0 /////////////  See 4a. ///////////////////////////////////
			printf(DBG_YELLOW"Totally %zu E3D_glAccessors loaded.\n"DBG_RESET, glAccessors.size());
			for(size_t k=0; k<glAccessors.size(); k++) {
		    printf("glAccessors[%zu]: name='%s', type='%s', count=%d, bufferViewIndex=%d, byteOffset=%d, componentType=%d, normalized=%s.\n",
				     k, glAccessors[k].name.c_str(), glAccessors[k].type.c_str(), glAccessors[k].count,
				     glAccessors[k].bufferViewIndex, glAccessors[k].byteOffset, glAccessors[k].componentType,
				     glAccessors[k].normalized?"Yes":"No" );
data NOT ok...	   printf(" byteStride=%d\n", glBufferViews[glAccessors[k].bufferViewIndex].byteStride);
			}
#endif /////////////////////////////////////////////////
		}
		/* Case glTF Jobject: meshes */
		else if(!key.compare("meshes")) {
			printf("\n=== meshes ===\n");

			/* Load data to glMeshes */
			if( E3D_glLoadMeshes(strValue, glMeshes)<0 )
				return -1;

			printf(DBG_GREEN"Totally %zu E3D_glMeshes loaded.\n"DBG_RESET, glMeshes.size());
			for(size_t k=0; k<glMeshes.size(); k++) {
				printf(DBG_MAGENTA"\n    --- mesh_%zu ---\n"DBG_RESET, k);
				printf("name: %s\n",glMeshes[k].name.c_str());
				printf("mweight.size: %zu\n", glMeshes[k].mweights.size());
				for(size_t j=0; j<glMeshes[k].primitives.size(); j++) {
					printf(DBG_MAGENTA"\n     (primitive_%zu)\n"DBG_RESET, j);
					int mode =glMeshes[k].primitives[j].mode;
					printf("mode:%d for '%s' \n",mode, (mode>=0&&mode<8)?glPrimitiveModeName[mode].c_str():"Unknown");
					printf("indices(AccIndex):%d\n",glMeshes[k].primitives[j].indicesAccIndex);
					printf("material(Index):%d\n",glMeshes[k].primitives[j].materialIndex);

					/* attributes */
					printf("Attributes:\n");
					printf("   POSITION(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.positionAccIndex);
					printf("   NORMAL(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.normalAccIndex);
					printf("   TANGENT(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.tangentAccIndex);
				     for(int in=0; in<PRIMITIVE_ATTR_MAX_TEXCOORD; in++) {
					if(glMeshes[k].primitives[j].attributes.texcoord[in]>-1)
					   printf("   TEXCOORD_%d=(AccIndex)%d\n",in, glMeshes[k].primitives[j].attributes.texcoord[in]);
				     }
				     for(int in=0; in<PRIMITIVE_ATTR_MAX_COLOR; in++) {
					if(glMeshes[k].primitives[j].attributes.color[in]>-1)
					   printf("   COLOR_%d=(AccIndex)%d\n",in, glMeshes[k].primitives[j].attributes.color[in]);
				     }
					printf("   (skin)JOINTS_0(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.joints_0);
					printf("   (skin)WEIGHTS_0(AccIndex)%d\n",glMeshes[k].primitives[j].attributes.weights_0);

					/* morph targets HK2023-02-27 */
					printf("Morph targets: %s\n",glMeshes[k].primitives[j].morphs.size()?"":"None");
				     for(size_t in=0; in<glMeshes[k].primitives[j].morphs.size(); in++) {
					printf("      --- morphs[%zu] ---\n", in);
					printf("      POSITION(AccIndex) %d\n", glMeshes[k].primitives[j].morphs[in].positionAccIndex);
					printf("      NORMAL(AccIndex) %d\n", glMeshes[k].primitives[j].morphs[in].normalAccIndex);
					printf("      TANGENT/TEXCOORD/COLOR todo\n");
				     }
				}
			}
		}
		/* Case glTF Jobject: materials */
		else if(!key.compare("materials")) {
			printf("\n=== materials ===\n");
			//printf("%s\n",strValue.c_str());

			/* Load data to glMeshes */
			if( E3D_glLoadMaterials(strValue, glMaterials)<0 )
				return -1;

/* TODO: diffuseTexture in extension, and NO baseColorTexture
	  [{"pbrMetallicRoughness":{},"normalTexture":{"index":2},"occlusionTexture":{"index":3},"doubleSided":true,
	  "extensions":{"KHR_materials_pbrSpecularGlossiness":{"diffuseTexture":{"index":0},"specularGlossinessTexture":{"index":1}}}}]
 */
			char *pt=NULL;
			if((pt=(char *)strstr(strValue.c_str(),"diffuseTexture"))) {
			    if((pt=strstr(pt+14+2,":") ))
				glMaterials[0].pbrMetallicRoughness.baseColorTexture.index=atoi(pt+1);
			}


			printf(DBG_GREEN"Totally %zu E3D_glMaterials loaded.\n"DBG_RESET, glMaterials.size());

			for(size_t k=0; k<glMaterials.size(); k++) {
				printf(DBG_MAGENTA"\n    --- material_%zu ---\n"DBG_RESET, k);
				printf("name: %s\n",glMaterials[k].name.c_str());
				printf("doubleSided: %s\n", glMaterials[k].doubleSided ? "True" : "False");
				printf("alphaMode: %s\n", glMaterials[k].alphaMode.c_str());
				printf("alphaCutoff: %f\n", glMaterials[k].alphaCutoff);
				printf("emissiveFactor: [%f,%f,%f]\n",
						glMaterials[k].emissiveFactor.x,
						glMaterials[k].emissiveFactor.y,
						glMaterials[k].emissiveFactor.z
				);
				printf("pbrMetallicRoughness:\n");
				printf("    baseColorFactor: [%f,%f,%f,%f]\n",
						glMaterials[k].pbrMetallicRoughness.baseColorFactor.x,
						glMaterials[k].pbrMetallicRoughness.baseColorFactor.y,
						glMaterials[k].pbrMetallicRoughness.baseColorFactor.z,
						glMaterials[k].pbrMetallicRoughness.baseColorFactor.w
				);
				printf("    baseColorTexture:\n");
				printf("         index:%d,  texCoord:%d\n",
						glMaterials[k].pbrMetallicRoughness.baseColorTexture.index,
						glMaterials[k].pbrMetallicRoughness.baseColorTexture.texCoord
				);
				printf("    metallicFactor: %f\n",glMaterials[k].pbrMetallicRoughness.metallicFactor);
				printf("    roughnessFactor: %f\n",glMaterials[k].pbrMetallicRoughness.roughnessFactor);
				printf("    metallicRoughnessTexture: TODO\n");

				printf("emissiveTexture: %s\n", glMaterials[k].emissiveTexture.index>-1?"":"None");
				if(glMaterials[k].emissiveTexture.index>-1) {
					printf("         index:%d,  texCoord:%d\n",
						glMaterials[k].emissiveTexture.index,
						glMaterials[k].emissiveTexture.texCoord
					);
				}

				printf("normalTexture: %s\n", glMaterials[k].normalTexture.index>-1?"":"None");
				if(glMaterials[k].normalTexture.index>-1) {
					printf("         index:%d,  texCoord:%d, scale:%f\n",
						glMaterials[k].normalTexture.index,
						glMaterials[k].normalTexture.texCoord,
						glMaterials[k].normalTexture.scale
					);
				}

				printf("occlutionTexture: TODO\n");
			}

		}
		/* Case glTF Jobject: textures */
		else if(!key.compare("textures")) {
			printf("\n=== textures ===\n");
			if( E3D_glLoadTextures(strValue, glTextures)<0 )
				return -1;

			printf(DBG_GREEN"Totally %zu E3D_glTextures loaded.\n"DBG_RESET, glTextures.size());
			for(size_t k=0; k<glTextures.size(); k++) {
				printf("glTextures[%zu]: name'%s', sampler(Index)=%d, image(Index)=%d\n",
						k,glTextures[k].name.c_str(), glTextures[k].samplerIndex, glTextures[k].imageIndex);
			}
		}
		/* Case glTF Jobject: images */
		else if(!key.compare("images")) {
			printf("\n=== images ===\n");
			if( E3D_glLoadImages(strValue, glImages)<0 ) {
				//return -1;
				printf(DBG_RED"E3D_glLoadImages() fails!\n"DBG_RESET);
			}

			printf(DBG_GREEN"Totally %zu E3D_glImages need to load.\n"DBG_RESET, glImages.size());
                        char strtmp[128];
			size_t strlen;
			for(size_t k=0; k<glImages.size(); k++) {
				/* HK2023-01-03 */
				strlen=glImages[k].uri.copy(strtmp, 64, 0);
				strtmp[strlen]='\0';
				if(glImages[k].uri.size()>64) strcpy(strtmp+64, "...");

				printf(DBG_MAGENTA"\n    --- image_%zu---\n"DBG_RESET, k);
				printf("name: %s\n", glImages[k].name.c_str());
				printf("uri: %s\n", strtmp);
				printf("IsDataURI: %s\n", glImages[k].IsDataURI?"Yes":"No");
				printf("mimeType: %s\n", glImages[k].mimeType.c_str());
				printf("bufferView(Index): %d\n", glImages[k].bufferViewIndex);
			}

#if 0 /* Disable img_kn in renderMesh() XXX TEST: To keep img_kd only, TODO img_kn/img_ke -------------- */
	glImages.resize(1);
#endif
		}

#if 0		/* Case glTF Jobject: samplers TODO */
		else if(!key.compare("samplers")) {
			printf("\n=== samplers ===\n");
			if( E3D_glLoadSampler(strValue, glSampler)<0 )
                                return -1;

                        printf(DBG_GREEN"Totally %zu E3D_glSamplers loaded.\n"DBG_RESET, glSamplers.size());
                        for(size_t k=0; k<glSamplers.size(); k++) {
                                printf(DBG_MAGENTA"\n    --- sampler_%d---\n"DBG_RESET, k);
                                printf("name: %s\n", glSamplers[k].name.c_str());
				printf("wrapS: %d\n",glSamplers[k].wrapS);
				printf("wrapT: %d\n",glSamplers[k].wrapT);
				printf("magFilter: %d\n",glSamplers[k].magFilter);
				printf("minFilter: %d\n",glSamplers[k].minFilter);
                        }

		}
#endif
		/* Case glTF Jobject: nodes */
		else if(!key.compare("nodes")) {
			printf("\n=== nodes ===\n");
			if( E3D_glLoadNodes(strValue, glNodes)<0 ) {
				//return -1;
				printf(DBG_RED"E3D_glLoadNodes() fails!\n"DBG_RESET);
			}

			printf(DBG_GREEN"Totally %zu E3D_glNodes loaded.\n"DBG_RESET, glNodes.size());
			for(size_t k=0; k<glNodes.size(); k++) {
				printf(DBG_MAGENTA"\n    --- node_%zu---\n"DBG_RESET, k);
				printf("name: %s\n", glNodes[k].name.c_str());
				printf("type: %s\n", _strNodeTypes[glNodes[k].type]);
				printf("Matrix_or_qt:\n");
				if(glNodes[k].matrix_defined)
					glNodes[k].matrix.print();
				else
					glNodes[k].qt.print();
				printf("pmat: \n"); glNodes[k].pmat.print("pmat"); /* HK2023-04-11 */
				printf("Children: ");
				for(size_t kc=0; kc<glNodes[k].children.size(); kc++)
					printf(" %d", glNodes[k].children[kc]);
				printf("\n");
			}
		}
		/* Case glTF Jobject: skins */
		else if(!key.compare("skins")) {
			printf("\n=== skins ===\n");
			if( E3D_glLoadSkins(strValue, glSkins)<0 ) {
				//return -1;
				printf(DBG_RED"E3D_glLoadSkins() fails!\n"DBG_RESET);
			}

			printf(DBG_GREEN"Totally %zu E3D_glSkins loaded.\n"DBG_RESET, glSkins.size());
			for(size_t k=0; k<glSkins.size(); k++) {
				printf(DBG_MAGENTA"\n    --- skins[%zu]---\n"DBG_RESET, k);
				printf("name: %s\n", glSkins[k].name.c_str());
				printf("invbMatsAccIndex: %d\n", glSkins[k].invbMatsAccIndex);
				if(glSkins[k].skeleton<0)
				     printf("skeleton(root): none\n");
				else
				     printf("skeleton(root): %d\n", glSkins[k].skeleton);
				printf("Total %zu joint_nodes.\n",glSkins[k].joints.size());
				printf("Joints nodes: [");
				for(size_t kk=0; kk<glSkins[k].joints.size(); kk++)
					printf(" %d ", glSkins[k].joints[kk]);
				printf("]\n");
			}
		}
		/* Case glTF Jobject: scenes */
		else if(!key.compare("scenes")) {
			printf("\n=== scenes ===\n");
			if( E3D_glLoadScenes(strValue, glScenes)<0 ) {
				//return -1;
				printf(DBG_RED"E3D_glLoadScenes() fails!\n"DBG_RESET);
			}

			printf(DBG_GREEN"Totally %zu E3D_glScenes loaded.\n"DBG_RESET, glScenes.size());
			for(size_t k=0; k<glScenes.size(); k++) {
				printf(DBG_MAGENTA"\n    --- scene_%zu---\n"DBG_RESET, k);
				printf("name: %s\n", glScenes[k].name.c_str());
				printf("Root node indices: ");
				for(size_t kk=0; kk<glScenes[k].rootNodeIndices.size(); kk++)
					printf("%d ", glScenes[k].rootNodeIndices[kk]);
				printf("\n");
			}
		}
		/* Case glTF Jobject: animations */
		else if(!key.compare("animations")) {
			printf("\n=== animations ===\n");
			if( E3D_glLoadAnimations(strValue, glAnimations)<0 ) {
				//return -1;
				printf(DBG_RED"E3D_glLoadAnimations() fails!\n"DBG_RESET);
			}

			printf(DBG_GREEN"Totally %zu E3D_glAnimations loaded.\n"DBG_RESET, glAnimations.size());
			for(size_t k=0; k<glAnimations.size(); k++) {
				printf(DBG_MAGENTA"\n    --- animation_%zu---\n"DBG_RESET, k);
				printf("name: %s\n", glAnimations[k].name.c_str());
				for(size_t kk=0; kk<glAnimations[k].channels.size(); kk++) {
					printf("Channels[%zu]: \n", kk);
					printf("   Sampler index: %d", glAnimations[k].channels[kk].samplerIndex);
					printf("   target node: %d, path: '%s'\n",
						 glAnimations[k].channels[kk].target.node, glAnimations[k].channels[kk].target.path.c_str());
				}
				for(size_t kk=0; kk<glAnimations[k].samplers.size(); kk++) {
					printf("Samplers[%zu]: \n", kk);
					printf("   input(accIndex): %d\n", glAnimations[k].samplers[kk].input);
					printf("   output(accIndex): %d\n", glAnimations[k].samplers[kk].output);
					printf("   interpolation: %d\n", glAnimations[k].samplers[kk].interpolation);
				}
			}
		}
		/* Case glTF Jobject: meshes */
		else
			egi_dpstd(DBG_YELLOW"Unparsed KEY/VALUE pair:\n  key: %s, value: %s\n"DBG_RESET, key.c_str(), strValue.c_str());

	}

	/* 3. Link buffers.data to bufferViews.data */
	for( size_t k=0; k<glBufferViews.size(); k++ ) {
		int index; /* index to glBuffers[] */
		int offset;
		int length;
		int stride;

		/* Check data integrity */
		index=glBufferViews[k].bufferIndex;
		if(index<0) {
			egi_dpstd(DBG_RED"glBufferViews[%zu].bufferIndex<0! give up data linking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check glBuffers[].data */
		if(glBuffers[index].data==NULL) {
			egi_dpstd(DBG_RED"glBuffers[%zu].data==NULL, give up data lingking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check offset */
		offset=glBufferViews[k].byteOffset;
		if(offset<0) {
			egi_dpstd(DBG_RED"glBufferViews[%zu].byteOffset<0! give up data lingking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check length */
		length=glBufferViews[k].byteLength;
		if(length<0) {
			egi_dpstd(DBG_RED"glBufferViews[%zu].byteLength<0! give up data lingking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check stride, byteStride is ONLY for vertex attribute data */
		stride=glBufferViews[k].byteStride;
		if(stride!=0 && (stride<4 || stride>252) ) {   /* byteStride: Minimum: >= 4, Maximum: <= 252 */
			egi_dpstd(DBG_RED"glBufferViews[%zu].byteStride=%d, without[4, 252], give up data lingking.\n"DBG_RESET, k, stride);
			//continue;
			return -1;
		}
		/* Check data length. */
		if(offset+length>glBuffers[index].byteLength) {
			egi_dpstd(DBG_RED"glBufferViews[%zu].byteOffset+byteLength > glBuffers[%d].byteLength, give up data lingking.\n"DBG_RESET,
					k, index);
			//continue;
			return -1;
		}
		/* TODO: Consider byteStride */
	/* bufferView.length >= accessor.byteOffset + EFFECTIVE_BYTE_STRIDE * (accessor.count - 1) + SIZE_OF_COMPONENT * NUMBER_OF_COMPONENTS */

		/* Link glBufferViews.data to glBuffers.data */
		glBufferViews[k].data = glBuffers[index].data+offset;
	}


	/* start = accessor.byteOffset + accessor.bufferView.byteOffset */

	/* 4. Link buffersViews.data to accessors.data */
	for( size_t k=0; k<glAccessors.size(); k++ ) {
		int index;   /* index to glBufferViews[] */
		int offset;
		int length; /* SIZE_OF_COMPONENT * NUMBER_OF_COMPONENTS */
		int esize;  /* Element size, in bytes */
		//int componentType; /* 5120-5126, NO 5124 */
		//bool normalized;
		int count;   /* Number of elements referenced by the accessor. */
		//string type;  /* SCALAR, or VEC2-4, or MAT2-4 */

		/* Check data integrity */
		index=glAccessors[k].bufferViewIndex;
		if(index<0) {
			egi_dpstd(DBG_RED"glAccessors[%zu].bufferViewIndex<0! give up data linking.\n"DBG_RESET,k);
			//continue;
			return -1;
		}
		/* Check offset */
		offset=glAccessors[k].byteOffset;
		if(offset<0) {
			egi_dpstd(DBG_RED"glAccessors[%zu].byteOffset<0! give up data linking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check count */
		count=glAccessors[k].count;
		if(count<0) {
			egi_dpstd(DBG_RED"glAccessors[%zu].count<0! give up data linking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check element size */
		esize=glAccessors[k].elemsize; //glElementSize(glAccessors[k].type, glAccessors[k].componentType);
		if(esize<0) {
			egi_dpstd(DBG_RED"glAccessors[%zu] element bytes<0! give up data linking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* If byteStride is dfined, it MUST be a multiple of the size of the accessor’s component type. */
		int stride=glBufferViews[index].byteStride; /* stride already checked at 3. */
		int compsize=glElementSize("SCALAR", glAccessors[k].componentType); /* SCALAR has 1 component */
		if(stride>0 && compsize>0) {
			if(stride%compsize) {
				egi_dpstd(DBG_RED"glAccessors[%zu]: stride(%d)%%compsize(%d)!=0, give up data linking.\n"DBG_RESET, k,stride,compsize);
				return -1;
			}
		}

		/* Check length */
		length=count*esize;
		if(length<0) {
			egi_dpstd(DBG_RED"glAccessors[%zu] data length<=0! give up data linking.\n"DBG_RESET, k);
			//continue;
			return -1;
		}
		/* Check data length. */
		if(glBufferViews[index].byteLength < offset+length) {
			egi_dpstd(DBG_RED"glAccessors[%zu].byteOffset+byteLength > glBufferViews[%d].byteLength, give up data lingking.\n"DBG_RESET,
					k, index);
			//continue;
			return -1;
		}

		/* Check data length, consider byteStride */
		//egi_dpstd(DBG_GRAY"glBufferViews[%d].byteStride=%dBs, element_size=%dBs, element_count=%d\n"DBG_RESET,
		//		index, glBufferViews[index].byteStride, esize, count);
	/* bufferView.length >= accessor.byteOffset + EFFECTIVE_BYTE_STRIDE * (accessor.count - 1) + SIZE_OF_COMPONENT * NUMBER_OF_COMPONENTS */
		//if( glBufferViews[index].byteLength < offset+(glBufferViews[index].byteStride-esize)*(count-1)+length ) {
		if( glBufferViews[index].byteLength < offset+(stride-esize)*(count-1)+length ) {
 		    egi_dpstd(DBG_RED"glBufferViews[%d].byteLength insufficient for glAccessors[%zu]! give up data lingking.\n"DBG_RESET, index, k);
		    //continue;
		    return -1;
		}

		/* Link glAccessors.data to glBufferViews.data */
		glAccessors[k].data = glBufferViews[index].data+offset;
		/* Assign byteStride HK2023-03-12 */
		glAccessors[k].byteStride=stride;
	}
	/* 4a. */
	printf(DBG_YELLOW"Totally %zu E3D_glAccessors loaded.\n"DBG_RESET, glAccessors.size());
	for(size_t k=0; k<glAccessors.size(); k++) {
             printf("glAccessors[%zu]: name='%s', type='%s', count=%d, bufferViewIndex=%d, byteOffset=%d, componentType=%d, normalized=%s.\n",
		     k, glAccessors[k].name.c_str(), glAccessors[k].type.c_str(), glAccessors[k].count,
		     glAccessors[k].bufferViewIndex, glAccessors[k].byteOffset, glAccessors[k].componentType,
		     glAccessors[k].normalized?"Yes":"No" );
	     printf("         byteStride=%d\n", glBufferViews[glAccessors[k].bufferViewIndex].byteStride);
	}


	/* 4b. Load glImages[].imgbuf from glBufferViews[].data, ONLY when its bufferViewIndex>0 and mimeType='image/jpeg' OR 'image/png'
	       See E3D_glLoadImages(): If imgbuf is loaded from a file uri OR embedded uri-data.
	 */
	for( size_t k=0; k<glImages.size(); k++ ) {
	   int bvindex=glImages[k].bufferViewIndex;

	   if(glImages[k].imgbuf==NULL	/* In case it's been loaded from an image file URI, see at E3D_glLoadImages() */
		&& bvindex>=0 && bvindex<(int)glBufferViews.size() )
	   {
		egi_dpstd(DBG_YELLOW"Try to load glImages[%zu].imgbuf from bufferViews[%d]...\n"DBG_RESET, k, bvindex);
	    	if(!glImages[k].mimeType.compare("image/jpeg")) {
			glImages[k].imgbuf=egi_imgbuf_readJpgBuffer(glBufferViews[bvindex].data, glBufferViews[bvindex].byteLength);
		   	if(glImages[k].imgbuf==NULL) {
				egi_dpstd(DBG_RED"glImages[%zu]: Fail to readJpgBuffer to imgbuf!\n"DBG_RESET, k);
				//return -1;
		   	}
	    	}
            	else if(!glImages[k].mimeType.compare("image/png")) {
            		//egi_dpstd(DBG_RED"glImages[%d]: mimeType 'image/png' is NOT supported yet.\n"DBG_RESET, k);
		   	glImages[k].imgbuf=egi_imgbuf_readPngBuffer(glBufferViews[bvindex].data, glBufferViews[bvindex].byteLength);
		   	if(glImages[k].imgbuf==NULL) {
				egi_dpstd(DBG_RED"glImages[%zu]: Fail to readJpngBuffer to imgbuf!\n"DBG_RESET, k);
				//return -1;
		   	}
            	}
            	else {
                	egi_dpstd(DBG_RED"glImages[%zu]: mimeType error for '%s'.\n"DBG_RESET, k, glImages[k].mimeType.c_str());
                	//return -1;
            	}
	   }
	}

//////////////////////// END: load GLB file to glBuffers/glBufferViews/glAccessors/glMeshes... ////////////////////
//exit(0);

	/* XXX 5. Count all trianlges and vertices */
        /* XXX 6. Clear vtxList[] and triList[], E3D_TriMesh() may allocate them.  */
        /* XXX 7. Allocate/init vtxList[] */
        /* XXX 8. Allocate/init triList[] */
	/* XXX 9. Resize triGroupList */
	/* XXX 10. Reset triCount/vtxCount */
	/* XXX 10a. Load Materils */

        /* 8. Check glNodes and set glMesh[].skinIndex accordingly. */
	for( size_t n=0; n<glNodes.size(); n++) {
  	    if(glNodes[n].type==glNode_Skin) {
		if( glNodes[n].skinMeshIndex>=0 && glNodes[n].skinMeshIndex < (int)glMeshes.size() ) {
		    if( glNodes[n].skinIndex>=0 && glNodes[n].skinIndex < (int)glSkins.size() ) {
			/* Finally.... */
			glMeshes[glNodes[n].skinMeshIndex].skinIndex = glNodes[n].skinIndex;
		    }
		    else
		        egi_dpstd(DBG_RED"glNodes[%zu].skinIndex=%d, NOT valid within [0 %zu) (glSkins.size) \n"DBG_RESET,
								n,glNodes[n].skinIndex, glSkins.size() );
		}
		else
		    egi_dpstd(DBG_RED"glNodes[%zu].skinMeshIndex=%d, NOT valid within [0 %zu) (glMeshes.size) \n"DBG_RESET,
								n,glNodes[n].skinMeshIndex, glMeshes.size() );
	    }
	}

	/* 9. Load skin data into glNodes.IsJoint and glNodes.invbMat */
printf(" ----------- xxxxxxxxxx ------------ \n");
	for( size_t  n=0; n<glSkins.size(); n++) { /* NOW one mesh is ONLY for one skin. */
	   egi_dpstd(DBG_MAGENTA"\n    --- glSkin[%zu]: nodes.inverseBindMatrices ---\n"DBG_RESET, n);

	   /* 9.1 Get data pointer to invbMat */
	   float *imdata=NULL; ///<---- { NULL }
	   int invbMatsAccIndex=glSkins[n].invbMatsAccIndex;
	   if( invbMatsAccIndex >=0 && invbMatsAccIndex <(int)glAccessors.size() ) {
	        if( !glAccessors[invbMatsAccIndex].type.compare("MAT4")
		    && glAccessors[invbMatsAccIndex].count >= (int)glSkins[n].joints.size() ) {

			imdata=(float *)glAccessors[invbMatsAccIndex].data;
		}
	   }
	   if( glAccessors[invbMatsAccIndex].componentType!=5126 )
		egi_dpstd(DBG_RED"glAccessors[invbMatsAccIndex].componentType!=5126(float)!\n"DBG_RESET);

	   /* 9.2 Assign glNodes.IsJoint and glNodes.invbMat */
	   for( size_t nn=0; nn<glSkins[n].joints.size(); nn++) {
		int jnode=glSkins[n].joints[nn];
		printf("glSkins[%zu].joints[%zu]=%d\n", n,nn, glSkins[n].joints[nn]);

		/* Check if jnode is reasonale */
		if(jnode<0 || jnode >= (int)glNodes.size())
			continue;

		/* TBD&TODO: NOW, a node can ONLY assign to one skin! */
		if(glNodes[jnode].IsJoint) {
			egi_dpstd(DBG_RED"glNodes[%d] is ALREADY assigned as a joint to glSkin[%d]!(now nn=%zu)\n"DBG_RESET,
							jnode, glNodes[jnode].jointSkinIndex, nn);
			continue;
		}

		/* Set IsJoint and skinIndex*/
		glNodes[jnode].IsJoint =true;
		glNodes[jnode].jointSkinIndex =n; /* index of skins */

		/* Set invbMat */
		if( imdata!=NULL ) {
printf("Extract inverseBindMatrices for glNodes[%d]....\n",jnode);
                      /*** Extract pmat[4*3] from 4*4 matrix
		       *   Note: Accessors of matrix type have data stored in column-major order! (glTF column-major is E3D row-major)
		       */

                      for(int ni=0; ni<4; ni++)  {
                          for(int nj=0; nj<3; nj++) {
                                      glNodes[jnode].invbMat.pmat[ni*3+nj]=imdata[(nn<<4)+(ni<<2)+nj]; // = [nn*16+ni*4+nj];
			 }
                      }
		      /* The fourth row of each matrix MUST be set to [0.0, 0.0, 0.0, 1.0]. */

/* TEST: -------- */
		     glNodes[jnode].invbMat.print();

		     /* To print vx/vy/vz HK2023-04-21 */
		     float vx,vy,vz;
		     vx= glNodes[jnode].invbMat.pmat[0]*glNodes[jnode].invbMat.pmat[0]
			+glNodes[jnode].invbMat.pmat[1]*glNodes[jnode].invbMat.pmat[1]
			+glNodes[jnode].invbMat.pmat[2]*glNodes[jnode].invbMat.pmat[2];
		     vx=sqrtf(vx);

		     vy= glNodes[jnode].invbMat.pmat[3]*glNodes[jnode].invbMat.pmat[3]
			+glNodes[jnode].invbMat.pmat[4]*glNodes[jnode].invbMat.pmat[4]
			+glNodes[jnode].invbMat.pmat[5]*glNodes[jnode].invbMat.pmat[5];
		     vy=sqrtf(vy);

		     vz= glNodes[jnode].invbMat.pmat[6]*glNodes[jnode].invbMat.pmat[6]
			+glNodes[jnode].invbMat.pmat[7]*glNodes[jnode].invbMat.pmat[7]
			+glNodes[jnode].invbMat.pmat[8]*glNodes[jnode].invbMat.pmat[8];
		     vz=sqrtf(vz);
		     printf("Modulus(vx,vy,vz): %f,%f,%f\n", vx,vy,vz);
		}
		else
		     egi_dpstd(DBG_RED"imdata=NULL! inverseBindMatrices are NOT available!\n"DBG_RESET);

	   }
	}

	/* 10. Reserve triMeshList first.  XXX to prevent unexpectd copying/releasing during auto capacity adjusting.XXX */
	triMeshList.reserve(glMeshes.size());

	/* 11. Load vtxList[], triList[] and triGroupList[] */
	int accIndex; /* Index for glAccessors[] */
	int mode;
	unsigned int vindex[3];  /* vertex index for tirangle */
	unsigned int uind;	/* To store assmebled index number */
	unsigned int utmp;
	unsigned int *udat;
				/* For triangle vertexIndex.
				 * When primitives.indices is defined. the accessor MUST have SCALAR element_type and
				 * an unsigned integer component_type.
				 *    			!!!--- CAUTION ---!!!
				 * Above "unsigned integer" includes: unsigned int, unsigned short, or unsigned char
		 		 */
	float *fdat;	/* 1. For veterx position(XYZ). element_type is VEC3, and component_type is float.
			 * 2. For veterx normal(xyz). element_type is VEC3, and component_type is float.
			 * 3. For u/v, element_type is VEC2, and component_types: float/unsigned_byte_normalized/unsinged_short_normalized
			 */
	int Vsdx;  /* Set as starting index for vtxList[], before reading each primitive */
	int Tsdx;  /* Set as starting index for triList[], before reading each primitive */
	int primitiveVtxCnt=0; /* Vertex count for a primitive, reset at 11.1A */

	vector<int> glMaterialsInMesh; /* glMaterialID list used in a trimesh */

	/* 11.-1 Traverse all meshes and primitives, to load data to vtxList[], triList[], triGroupList[], */
	for(size_t m=0; m<glMeshes.size(); m++) {
	    egi_dpstd(DBG_ORANGE"\n\nRead glMeshes[%zu]...\n"DBG_RESET, m);
	    int  primTriCnt=0;

	    /* 11.-1.1 Clear materialIDs */
	    glMaterialsInMesh.clear();

	    /* 11.-1.2 Compute tCapacity and vCapacity in glMeshes[] */
	    triCount=0; vtxCount=0; tgCount=0;
            for(size_t i=0; i<glMeshes[m].primitives.size(); i++) {
                int accID;
            	if( glMeshes[m].primitives[i].mode==4 ) { /* 4 for triangle */
                	/* Triangles */
                        accID=glMeshes[m].primitives[i].indicesAccIndex;
                        /* Each consecutive set of three vertices defines a single triangle primitive */
                        triCount += glAccessors[accID].count/3;

                        /* Vertices */
                        accID=glMeshes[m].primitives[i].attributes.positionAccIndex;
                        vtxCount += glAccessors[accID].count;

                        /* triGroup */
                        tgCount +=1;
                }
	        egi_dpstd("glMeshes[%zu].primitive[%zu] contains: triCount=%d, vtxCount=%d, tgCount=%d\n", m,i, triCount, vtxCount, tgCount);
	    }

	    /* 11.-1.3 Add a new TriMesh in triMeshList[], as corresponds with glMeshes[m] */
	    E3D_TriMesh *trimesh = new E3D_TriMesh(vtxCount, triCount); /* init vCapacity, tCapacity */
	    if(trimesh==NULL) {
		egi_dpstd(DBG_RED"Fail to allocate a new E3D_TriMesh!\n"DBG_RESET);
		return -1; //???? TODO
	    }
            /* 11.-1.4 Resize triGroupList */
            trimesh->triGroupList.resize(tgCount);
	    for(size_t kt=0; kt < trimesh->triGroupList.size(); kt++) /* HK2023-03-06 Necessary? */
			trimesh->triGroupList[kt].setDefaults();

	    /* 11.-1.4A Reserve enough capacity for trimesh->mtlList, See cautions at 11.3.4.1 */
	    int tmpIndex=-1;
	    int meshMtlCnt=0; /* Count of materials in a mesh, no repeating materialIndex. */
	    /* 11.-1.4A.1 Count all materials in a mesh */
	    for(size_t i=0; i<glMeshes[m].primitives.size(); i++) {
		if(glMeshes[m].primitives[i].materialIndex>=0) {
			tmpIndex=-1; /* as NOT found */
			/* Search in glMaterialsInMesh if same materialIndex already exists */
			for(size_t mk=0; mk<glMaterialsInMesh.size(); mk++) {
				if(glMeshes[m].primitives[i].materialIndex == glMaterialsInMesh[mk]) {
					tmpIndex=glMaterialsInMesh[mk]; /* Found same in glMaterialsInMesh */
				}
			}
			/* NOT found in glMaterialsInMesh[] */
			if(tmpIndex<0) {
				meshMtlCnt +=1;
			}
		}
	    }
	    /* 11.-1.4A.2 Reserve mtlList */
	    trimesh->mtlList.reserve(meshMtlCnt);

	    /* 11.-1.4B Clear materialIDs again. */
	    glMaterialsInMesh.clear();

	    /* 11.-1.5 Push back trimesh  !!! --- CAUTION --- !!! triMeshList capacity is reversed at 10. */
	    triMeshList.push_back(trimesh);

	    egi_dpstd("a E3D_TriMesh with tCapacity=%d, vCapacity=%d, triGroupList.size=%zu is created for glMeshes[%zu]!\n",
					trimesh->tCapacity, trimesh->vCapacity, trimesh->triGroupList.size(), m);

	    /* 11.-1.6 Reset triCount/vtxCount/tgCount for each mesh. */
	    triCountBk=triCount; 	triCount=0;
	    vtxCountBk=vtxCount; 	vtxCount=0;
	    tgCountBk=tgCount;  	tgCount=0;

	    /* 11.-1.6a Assign name HK2023-04-11 */
	    trimesh->name = glMeshes[m].name;

	    /* 11.-1.7 Assign mesh.mweights */
	    trimesh->mweights = glMeshes[m].mweights;

	    /* 11.0 Walk through all primitives in the mesh */
	    for(size_t n=0; n<glMeshes[m].primitives.size(); n++) {
	    	egi_dpstd(DBG_GREEN"   Read glMeshes[%zu].primitives[%zu]...\n"DBG_RESET, m, n);

		mode=glMeshes[m].primitives[n].mode;
		if(mode==4) primTriCnt++;

/* TODO: NOW only TRIANGLES: ----------------- Following for mode==4 (triangles) ONLY */

		/* 11.1 Save triGroupList[] starting index here! */
		Vsdx=vtxCount;
		Tsdx=triCount;
		egi_dpstd("Starting vtxIndex=%d\n", vtxCount);

		/* 11.1A Reset */
		primitiveVtxCnt =0 ;

		/* 11.2 Read glMesh.primitives.attributes.POSITION into vtxList[] */
		accIndex=glMeshes[m].primitives[n].attributes.positionAccIndex;
		if(mode==4 && accIndex>=0 ) {
			/* 11.2.0 Check data */
                        if(glAccessors[accIndex].data==NULL || glAccessors[accIndex].type.compare("VEC3") ) {  /* HK2023-03-25 */
                                egi_dpstd(DBG_RED"11.2.0: glAccessors[%d].data is NULL OR type is not VEC3! Abort.\n"DBG_RESET, accIndex);
                                return -1;
                        }

			/* 11.2.1 Get pointer to float type */
			fdat=(float *)glAccessors[accIndex].data;
                        int _dstride=glAccessors[accIndex].byteStride;
                        if(_dstride !=0 && _dstride!=sizeof(float)*3 )
                      		egi_dpstd(DBG_RED"_dstride !=0 && _dstride!=sizeof(float)*3!\n"DBG_RED);

			/* 11.2.1a Get primitiveVtxCnt */
			primitiveVtxCnt=glAccessors[accIndex].count; /* VEC3 */

			/* 11.2.2 Read in vtxList[] */
			egi_dpstd(DBG_YELLOW"Read vtxList[] from glAccessors[%d], VEC3 count=%d\n"DBG_RESET, accIndex, glAccessors[accIndex].count);
			for(int j=0; j<glAccessors[accIndex].count; j++) {  /* count is number of ELEMENTs(VEC3) */
				/* Read XYZ as fully packed. TODO: consider byteStride */
				//trimesh->vtxList[vtxCount].assign(fdat[3*j], fdat[3*j+1], fdat[3*j+2]);
                                if(_dstride)
                                   trimesh->vtxList[vtxCount].assign( fdat[(_dstride>>2)*j],    /* float is 4bytes */
                                                                      fdat[(_dstride>>2)*j+1],
                                                                      fdat[(_dstride>>2)*j+2] );
                                else
                                   trimesh->vtxList[vtxCount].assign(fdat[3*j], fdat[3*j+1], fdat[3*j+2]);
//				printf("vtxList[%d](XYZ): %f,%f,%f\n", vtxCount, fdat[3*j], fdat[3*j+1], fdat[3*j+2]);

				vtxCount++;
				trimesh->vcnt=vtxCount;
			}

		   	/* 11.2.3 Read glMesh.primitives.attributes.NORMAL into vtxList[] */
		   	int normalAccIndex=glMeshes[m].primitives[n].attributes.normalAccIndex;
		   	if(normalAccIndex>=0 ) {
			    /* Check element_type and component_type for NORMAL data */
			    if(glAccessors[normalAccIndex].type.compare("VEC3"))
			       egi_dpstd(DBG_RED"glMeshes[%zu].primitives[%zu] normal_data: glAccessror[%d] element is NOT 'VEC3'!\n"DBG_RESET,
				   		m,n, normalAccIndex);
			    if(glAccessors[normalAccIndex].componentType!=5126)
			       egi_dpstd(DBG_RED"glMeshes[%zu].primitives[%zu] normal_data: glAccessror[%d] component is NOT 'FLOAT'!\n"DBG_RESET,
				  		m,n, normalAccIndex);


				/* Get pointer to float type */
				fdat=(float *)glAccessors[normalAccIndex].data;

				/* Read in vtxList[].normal */
				egi_dpstd(DBG_YELLOW"Read vtxList[].normal from glAccessors[%d], VEC3 count=%d\n"DBG_RESET,
											normalAccIndex, glAccessors[normalAccIndex].count);
				/* Check vertex POSITION count and NORMAL count. */
				if(glAccessors[accIndex].count != glAccessors[normalAccIndex].count ) {
				   egi_dpstd(DBG_RED"POSITION glAccessors[%d].count!= NORMAL glAccessors[%d].count !\n"DBG_RESET,
								     accIndex, normalAccIndex);
				}
				else {
				   /* Read in vtxList[].normal */
				   for(int j=0; j<glAccessors[normalAccIndex].count; j++) {  /* count is number of ELEMENTs(VEC3) */
					/* Assume they are normalized VEC3 */
					trimesh->vtxList[Vsdx+j].normal.assign(fdat[3*j], fdat[3*j+1], fdat[3*j+2]);

//printf("vtxList[%d].normal: %f,%f,%f\n", Vsdx+j, fdat[3*j], fdat[3*j+1], fdat[3*j+2]);
				   }
				}
			}
		}

		/* 11.3 Read glMesh.primitivies.indices into triList[] */
		accIndex=glMeshes[m].primitives[n].indicesAccIndex;
		if( mode==4 && accIndex>=0 && accIndex<(int)glAccessors.size() ) {
			/* 11.3.0 Check data */
			if(glAccessors[accIndex].data==NULL) {
				egi_dpstd(DBG_RED"glAccessors[%d].data is NULL! Abort.\n"DBG_RESET,accIndex);
				return -1;
			}

			/* 11.3.1 Get component size */
			int compsize=glAccessors[accIndex].elemsize; /* for SCALAR, number_of_components=1 */

			/* triGroupList[] starting index */
			//Vsdx=vtxCount; NOT HERE! see at 11.1

			/* 11.3.2 Read this primitive into triList[] */
			egi_dpstd(DBG_YELLOW"Read triList[] from glAccessors[%d], SCALAR count=%d\n"DBG_RESET, accIndex, glAccessors[accIndex].count);
			for(int j=0; j<glAccessors[accIndex].count/3; j++) { /* 3 vertices for each triangle */

#if 1  //////////////// For any elemsize  /////////////////
				/* Read 3 vtx indices as fully packed.  TODO: consider byteStride  */
				for(int ii=0; ii<3; ii++) { /* 3 verter index for one triangle */
					uind=0;
					for(int jj=0; jj<compsize; jj++) { /* component bytes */
						/* read byte by byte. littlendia. */
						utmp=glAccessors[accIndex].data[(3*j+ii)*compsize+jj];
						uind += utmp<<(jj<<3); //jj*8
					}
					vindex[ii]=uind;

				#if 0	/* Check */
					if( Vsdx+(int)vindex[ii] >= vtxCount ) {
						egi_dpstd(DBG_RED"Vsdx(%d)+vtxIndex(%d) >= %d(vtxTotal)!\n"DBG_RESET,
										Vsdx,Vsdx+vindex[ii], vtxCount);
						return -1;
					}
				#endif

				}
				/* Assign to triList[] */
				trimesh->triList[triCount].assignVtxIndx(Vsdx+vindex[0], Vsdx+vindex[1], Vsdx+vindex[2]);
				//printf("triList[%d](index012): %d,%d,%d\n", triCount, Vsdx+vindex[0], Vsdx+vindex[1], Vsdx+vindex[2]);

#else ///////////////// For elemisze=4bytes ONLY //////////////
			        //udat pointer to ...
				udat=(unsigned int *)glAccessors[accIndex].data;
				trimesh->triList[triCount].assignVtxIndx(Vsdx+udat[3*j], Vsdx+udat[3*j+1], Vsdx+udat[3*j+2]);
				printf("triList[%d](index012): %d,%d,%d\n", triCount, Vsdx+udat[3*j], Vsdx+udat[3*j+1], Vsdx+udat[3*j+2]);
#endif
				/* triCount incremental */
				triCount++;
				trimesh->tcnt=triCount;
			}

			/* 11.3.3 Update TriGroupList. One meshes.primitive for one triGroup */
		        trimesh->triGroupList[tgCount].tricnt=glAccessors[accIndex].count/3; //HK2022-12-22: tricnt replaces tcnt
        		trimesh->triGroupList[tgCount].stidx=Tsdx;
        		trimesh->triGroupList[tgCount].etidx=Tsdx+glAccessors[accIndex].count/3; /* Caution!!! etidx is NOT the end index! */

			    /* HK2023-03-01 */
			trimesh->triGroupList[tgCount].vtxcnt=primitiveVtxCnt; //!!! glAccessors[accIndex].count; accIndex NOW is indicesAccIndex
			trimesh->triGroupList[tgCount].stvtxidx=Vsdx;
			trimesh->triGroupList[tgCount].edvtxidx=Vsdx+primitiveVtxCnt; /* Caution!!! edvtxidx is NOT the end index! */
			   /* glPrimitive has no param name! */
        		trimesh->triGroupList[tgCount].name=glMeshes[m].name; /* Assume ONLY 1 trianlge_primitive(mode==4) in primitives[] */
			/* If mesh has more than 1 primitive, then triGroup_name = mesh_name+append_num */
			if(glMeshes[m].primitives.size()>1) {
				char buff[32];
				buff[31]=0;
				sprintf(buff,"_%zu",n);
				trimesh->triGroupList[tgCount].name.append(buff);
			}

			/* 11.3.3A  Assign TriGroupList[].skins  */
			int jointsAccIndex=glMeshes[m].primitives[n].attributes.joints_0;
			int weightsAccIndex=glMeshes[m].primitives[n].attributes.weights_0;
			float *wtdata=NULL;  /* For weights */
			unsigned char  *jtdata0=NULL;  /* Forr joint indices */
			unsigned short *jtdata1=NULL;

			/* If has skin defined */
			if( glMeshes[m].skinIndex >=0   /* glMeshes[].skinIndx is set at 8. */
			   && jointsAccIndex >=0 && jointsAccIndex <(int)glAccessors.size()
			   && weightsAccIndex >=0 && weightsAccIndex <(int)glAccessors.size() )
			{
			   egi_dpstd(DBG_YELLOW"Read skin data for glMeshes[%zu].primitive[%zu], joints_0=%d, weights_0=%d.\n"DBG_RESET,
						m, n, jointsAccIndex, weightsAccIndex);

			   /* .S1 Check if accessors have valid data */
			   if(  !glAccessors[jointsAccIndex].type.compare("VEC4")
				  && glAccessors[jointsAccIndex].count >= primitiveVtxCnt
			     && !glAccessors[weightsAccIndex].type.compare("VEC4")
                                  && glAccessors[weightsAccIndex].count >= primitiveVtxCnt )
			   {
			      /*** Note:
                                 1. glAccessors[jointsAccIndex] componentType must be: unsigned byte(5121) or unsigned short(5123).
                                 2. glAccessors[weightsAccIndex] componentType must be: float(5126), or normalized unsigned byte(5121),
                                    or normalized unsigned short(5123).
			       */
      egi_dpstd(DBG_MAGENTA"glAccessors[weightsAccIndex].componentType=%d(5126), glAccessors[jointsAccIndex].componentType=%d(5121or5123)\n"DBG_RESET,
					  glAccessors[weightsAccIndex].componentType, glAccessors[jointsAccIndex].componentType );

			     /* TODO: If glAccessors[weightsAccIndex].normalized */
 			     if(glAccessors[weightsAccIndex].normalized)
      				 egi_dpstd(DBG_RED"glAccessors[%d for weightsAccIndex] is normalized!\n"DBG_RESET,weightsAccIndex);

			      /* .S1.1 NOW ONLY support float component for weigths */
			      if( glAccessors[weightsAccIndex].componentType == 5126
			         && ( glAccessors[jointsAccIndex].componentType == 5121 || glAccessors[jointsAccIndex].componentType == 5123 ) )
			      {
				/* .S1.1.1 Get pinter to wtdata/jtdata */
				wtdata=(float *)glAccessors[weightsAccIndex].data;

				jtdata0=NULL; jtdata1=NULL;
				if( glAccessors[jointsAccIndex].componentType == 5121 ) /* unsigned byte */
					jtdata0=(unsigned char *)glAccessors[jointsAccIndex].data;
				else /* componentType == 5123 */  /* Unsigned short */
					jtdata1=(unsigned short *)glAccessors[jointsAccIndex].data;

				/* .S1.1.2 Resize triGroupList[].skins */
				trimesh->triGroupList[tgCount].skins.resize(primitiveVtxCnt);

				/* .S1.1.3 Each primitive vetex has skin data: wegiths and joints */
				int   skinIndex=glMeshes[m].skinIndex;
				float _weight;  /* skin weight */
				int _joint;	/* joint index */
				int _jointNode; /* node index */
				int  wstride=glAccessors[weightsAccIndex].byteStride;
				int  jstride=glAccessors[jointsAccIndex].byteStride;

				for( int is=0; is<primitiveVtxCnt; is++) {
				   /* .S1.1.3.1 Each vertex has Max. 4 influencing joints */

				   for( int js=0; js<4; js++ ) {

				 	/* .S1.1.3.1.1 Get triGroupList[].skins.weights[4] */
					if(wstride)
					     _weight=*(wtdata+wstride/4*is+js);  //sizeof(float)=4
					else
					     _weight=*(wtdata+4*is+js);
					trimesh->triGroupList[tgCount].skins[is].weights.push_back(_weight);

					/* .S1.1.3.1.2 Get _joint from accessor data */
					if(jtdata0) { /* jtdata0: unsigned byte */
					     if(jstride)
						 _joint=*(jtdata0+jstride/1*is+js);
					     else
					         _joint=*(jtdata0+4*is+js); /* index to glSkin.joints[] */
					}
					else { /* jtdata1: unsigned short */
					     if(jstride)
						  _joint=*(jtdata1+jstride/2*is+js); //sizeof(unsign short)=2
					     else
						 _joint=*(jtdata1+4*is+js);
					}

					/* .S1.1.3.1.3 Get triGroupList[].skins.jointNodes[4] */
					if(_joint>=0 &&  _joint<(int)glSkins[skinIndex].joints.size() ) {
					    /* Covert glSkin.joints[] index to glNodes[] index */
					    _jointNode=glSkins[skinIndex].joints[_joint];

					    if( _jointNode>=0 && _jointNode<(int)glNodes.size() ) {
						 /* Finnaly...*/
						trimesh->triGroupList[tgCount].skins[is].jointNodes.push_back(_jointNode);
					    }
					    /* Whatever */
					    else {
					        trimesh->triGroupList[tgCount].skins[is].jointNodes.push_back(0);
						egi_dpstd(DBG_RED"_jointNode=%d, NOT within [0  %zu)(glNodes.size)!\n"DBG_RESET,
										_jointNode, glNodes.size());
					    }
					}
					/* .S1.1.3.1.4 Whatever */
					else {
					    /* Whatever */
					    trimesh->triGroupList[tgCount].skins[is].jointNodes.push_back(0);

					    egi_dpstd(DBG_RED"_joint=%d, NOT within [0  %zu)(glSkins[%d].joints.size)!\n"DBG_RESET,
									_joint, glSkins[skinIndex].joints.size(), skinIndex);
					}

				   }/* END for(js) */

				}/* END for(is) */

			    } /* END .S1.1 */
			    else {
				egi_dpstd(DBG_RED"glAccessors[weightsAccIndex].compentType is NOT float, not supported yet!"DBG_RESET);
			    }
			  } /* END .S1 */
			}
			/* No skin defined */
			else {
			   egi_dpstd(DBG_GRAY"NO skin data for glMeshes[%zu].primitive[%zu], skinIndex=%d, joints_0=%d, weights_0=%d.\n"DBG_RESET,
						m, n,  glMeshes[m].skinIndex, jointsAccIndex, weightsAccIndex);
			}

			/* 11.3.4 Assign material ID for the triGroup. Also see 10a. glMaterials[] ---> THIS.mtlList[] */
			int materialIndex=glMeshes[m].primitives[n].materialIndex;  //materialIndex = THIS.mtlID
			if( materialIndex >-1 && materialIndex<(int)glMaterials.size() ) { //same index glMaterials[]-->THIS.mtlList[]
				egi_dpstd(DBG_YELLOW"Assign triGroupList[].mtlID ...\n"DBG_RESET);

			        int baseColorTextureIndex;
			        int emissiveTextureIndex;
			        int normalTextureIndex;

				/* 11.3.4.0 Check if material with materialIndex has already been mapped into triMesh->mtList */
				bool mtID_found=false; /* whether this glMaterial already imported into trimesh->mtList */
				size_t mk;
				for(mk=0; mk<glMaterialsInMesh.size(); mk++) {
					if(materialIndex==glMaterialsInMesh[mk]) {
						mtID_found=true;

						/* Assign triGroupList[].mtlID */
						trimesh->triGroupList[tgCount].mtlID=mk;
						break;
					}
				}

				/* 11.3.4.1 Add a new item into trimesh->mtlList[] */
			    if(mtID_found==false) {
				int id;  //--->mtID;
			        //int baseColorTextureIndex;
			        //int emissiveTextureIndex;
			        //int normalTextureIndex;

				/* Push materialIndex into glMaterialInMesh, as corresponding with id. */
				glMaterialsInMesh.push_back(materialIndex);

			        /* One more item in mtlList */
				E3D_Material material;

				/*** 			!!!--- CAUTION ---!!!
				  material has pointer members, mtlList MUST reserve enough capacity to prevent unexpected
				  reallocation/realse of pointers when vector size grows. see 11.-1.4A
				 */
				trimesh->mtlList.push_back(material);

				/* !!! Operated with trimesh->mtllist.back(), NOT material! for it has pointer memeber !!! */
				id=trimesh->mtlList.size()-1;

				/* M1. Assign mtlList[].name */
		                trimesh->mtlList[id].name = glMaterials[materialIndex].name;

				egi_dpstd(DBG_MAGENTA"Load glMaterial[%d] '%s' into trimesh->mtlList[%d]\n"DBG_RESET,
						 	materialIndex, glMaterials[materialIndex].name.c_str(), id);

				/* M1A. Assign mtlList[].doubleSided HK2023-03-11 */
				trimesh->mtlList[id].doubleSided = glMaterials[materialIndex].doubleSided;

                		/* M2. Assign mtlList[].kd */
		                if(glMaterials[materialIndex].pbrMetallicRoughness.baseColorFactor_defined) {
                			trimesh->mtlList[id].kd.x= glMaterials[materialIndex].pbrMetallicRoughness.baseColorFactor.x;
		                    	trimesh->mtlList[id].kd.y= glMaterials[materialIndex].pbrMetallicRoughness.baseColorFactor.y;
                			trimesh->mtlList[id].kd.z= glMaterials[materialIndex].pbrMetallicRoughness.baseColorFactor.z;
					//TODO: ALPAH component
                		}
		                /* ELSE baseColorFactor undefined: keep E3D_Material default kd value. */

                		/* M3. Assign mtlList[].img_kd */
		                baseColorTextureIndex=glMaterials[materialIndex].pbrMetallicRoughness.baseColorTexture.index;
                		//if(glMaterials[k].pbrMetallicRoughness.baseColorTexture_defined) {
		                //baseColorTextureIndex >=0 for baseColorTexture_defined
                		if(baseColorTextureIndex>=0 && glTextures[baseColorTextureIndex].imageIndex>=0 ) {
					egi_dpstd(DBG_MAGENTA"Assign glImages[%d].imbuf to trimesh->mtlList[%d].img_kd\n"DBG_RESET,
							glTextures[baseColorTextureIndex].imageIndex, id);
		                        /*---- !!! CAUTION !!! Ownership transfers.  ---*/
                		        trimesh->mtlList[id].img_kd=glImages[glTextures[baseColorTextureIndex].imageIndex].imgbuf;
		                        /* glImages[glTextures[baseColorTextureIndex].imageIndex].imgbuf=NULL; see func TODO note.
					   It may be refered by other meshes, so do no reset to NULL.
					 */
					if(trimesh->mtlList[id].img_kd)
					    egi_dpstd("imgbuf W*H: %d*%d\n",
							trimesh->mtlList[id].img_kd->width, trimesh->mtlList[id].img_kd->height);
					else
				             egi_dpstd(DBG_RED"glImages[%d].imbuf is NOT loaded!\n"DBG_RESET,
										glTextures[baseColorTextureIndex].imageIndex);
                		        /* Note: Assign triGroupList[].vtx.u/v in 11.3.4 */
                		}

		                /* M4. Assign mtlList[].img_ke */
                		emissiveTextureIndex=glMaterials[materialIndex].emissiveTexture.index;
		                //emissvieTextureIndex >=0 for emissiveTexture_defined
                		if(emissiveTextureIndex>=0 && glTextures[emissiveTextureIndex].imageIndex>=0 ) {
		                        /*---- !!! CAUTION !!! Ownership transfers.  --- */
                		        trimesh->mtlList[id].img_ke=glImages[glTextures[emissiveTextureIndex].imageIndex].imgbuf;
		                        /* glImages[glTextures[emissiveTextureIndex].imageIndex].imgbuf=NULL; see TODO note.
					  It may be refered by other meshes, so do no reset to NULL.
                                         */

                		        /* Note: Assign triGroupList[].vtx.u/v in 11.3.4 */
                		}
				/* M5. Assign mtlList[].img_kn */
			        normalTextureIndex=glMaterials[materialIndex].normalTexture.index;
		                if(normalTextureIndex>=0 && glTextures[normalTextureIndex].imageIndex>=0 ) {
                		        /*---- !!! CAUTION !!! Ownership transfers.  ---*/
		                        trimesh->mtlList[id].img_kn=glImages[glTextures[normalTextureIndex].imageIndex].imgbuf;
                		        /* glImages[glTextures[normalTextureIndex].imageIndex].imgbuf=NULL; see TODO note.
					 It may be refered by other meshes, so do no reset to NULL.
                                         */

		                        /* Assign kn_scale */
                		        trimesh->mtlList[id].kn_scale=glMaterials[materialIndex].normalTexture.scale;
                		}

				/* M6. Assign triGroupList[].mtlID */
				trimesh->triGroupList[tgCount].mtlID=id;

			    } /* END if(mtID_found==false) */


///////////////////////////////////////////  uv for img kd  ///////////////////////////////////////////
				/* 11.3.4.1A Check img_kd */
                    		/* see 11.3.4.1 M3. Assign mtlList[].img_kd */
				//if( trimesh->mtlList[materialIndex].img_kd==NULL)
				if( baseColorTextureIndex <0 )
 				        egi_dpstd(DBG_RED"baseColorTextureIndex <0! Ignore triList[].vtx[].u/v .\n"DBG_RESET);

				/* 11.3.4.2 Assign triList[].vtx.u/v */
		                //else if(  trimesh->mtlList[materialIndex].img_kd!=NULL &&
				         //glMaterials[materialIndex].pbrMetallicRoughness.baseColorTexture.index>=0 ) {
				else {
				        egi_dpstd(DBG_YELLOW"Assign uv for img_kd, to triList[].vtx[].uv...\n"DBG_RESET);

                   			int textureIndex=glMaterials[materialIndex].pbrMetallicRoughness.baseColorTexture.index;

					/* 11.3.4.2.1 If texture image exists */
                   			if(textureIndex>=0 && glTextures[textureIndex].imageIndex>=0 ) {
					    /* 11.3.4.2.1.1  HK2023-01-03 */
					    int nn=glMaterials[materialIndex].pbrMetallicRoughness.baseColorTexture.texCoord;

					    //assume nn is OK, integrity checked in E3D_glLoadTextureInfo()
					    int texcoord=glMeshes[m].primitives[n].attributes.texcoord[nn]; //as Accessors index

					    /* 11.3.4.2.1.2 */
					    if( texcoord >-1 && texcoord < (int)glAccessors.size() ) {

egi_dpstd(DBG_YELLOW"Read img_kd u/v from glAccessors[texcoord(accindx)=%d], elemtype: %s, count=%d, elemsize=%d\n"DBG_RESET,
				texcoord, glAccessors[texcoord].type.c_str(), glAccessors[texcoord].count, glAccessors[texcoord].elemsize);

						/* 11.3.4.2.1.2.1 TODO: byteStripe of glAccessors[].data NOT considered */
						fdat=(float *)glAccessors[texcoord].data;

						/* Check element type */
						if(glAccessors[texcoord].type.compare("VEC2")) {
							egi_dpstd(DBG_RED"Critical: glAccessors[texcoord=%d].type:'%s', it's is NOT VEC2!\n"DBG_RESET,
									texcoord, glAccessors[texcoord].type.c_str());

							/* Clear vtxList/triList */
							trimesh->vcnt=0; trimesh->tcnt=0;  trimesh->triGroupList.clear();

							ret=-1;
							goto END_FUNC;
						}

						/* 11.3.4.2.1.2.2 */
			                        /* Get component size. component types: float, unsinged byte normalized, unsinged short normalized. */
                        			int compsize=glAccessors[texcoord].elemsize/2; /* for VEC2, number_of_components=2 */
						int compmax=(1<<(compsize<<3))-1; /* component max. value */
						float invcompmax=1.0/compmax;
						int fcnt=0; //count vertices in this primitive

			egi_dpstd("Vertice in this primitive: %d\n", vtxCount-Vsdx);  /* vtxCount updated for this primitive at 11.2.2 */

					    	/* 11.3.4.2.1.2.3 Texture coord: VEC2 float Textrue coordinates */
						/* Get and assign vtxList[].u/v */
						for(int j=Vsdx; j<vtxCount; j++) {  /* vtxCount updated for this primitive at 11.2.2 */
						    if(compsize==4) {  //component type: float
							 trimesh->vtxList[j].u=fdat[fcnt*2];
							/* Hahaha... same UV coord direction as EGI E3D :) */
							 trimesh->vtxList[j].v=fdat[fcnt*2+1];

						//	printf("trimesh->vtxList[%d].u/v:%f,%f\n",j, trimesh->vtxList[j].u, trimesh->vtxList[j].v);


			#if 0 /* TEST: --------- DO NOT addjust uv here! AFT pixels of triangle get interpolated uv....   ----- */
			if(trimesh->vtxList[j].u>1.0f || trimesh->vtxList[j].v>1.0f || trimesh->vtxList[j].u<0.0f || trimesh->vtxList[j].v<0.0f)
			{
				egi_dpstd(DBG_RED"For img_kd triList[].vtx[].uv: vtxList[].u/v(%f,%f) is NOT within [0 1]!\n"DBG_RESET,
										trimesh->vtxList[j].u, trimesh->vtxList[j].v);
				/* Try to adjust u/v to within [0 1); NOT HERE!!~ */
				 XXX E3D_wrapUV(trimesh->vtxList[j].u, trimesh->vtxList[j].v,0);
			}
			#endif

						    }
						    else if(compsize<4) { //component type: unsinged byte normalized, unsinged short normalized
							egi_dpstd(DBG_CYAN"TexCoordUV component type NOT float!\n"DBG_RESET);
							/* componet: u */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2)*compsize+kk];  //1byte incremental
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							trimesh->vtxList[j].u=uind*invcompmax;

							/* component: v */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2+1)*compsize+kk];
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							trimesh->vtxList[j].v=uind*invcompmax;
						    }

						    fcnt++; /* count vertices */

						} /* End for(j) */

						/* 11.3.4.2.1.2.4  TODO: NOW E3D_TriMesh::renderMesh() reads UV from triList[].vtx[].u/v */
						int vtxind;
				  		/* For img_kd: Store uv at triList[].vtx[].u/v  nn--- see 11.3.4.2.1.1 */
						for(int j=Tsdx; j<triCount; j++) {  /* triCount updated for this primitive at 11.3.2 */
						   for(int jj=0; jj<3; jj++) {
							vtxind=trimesh->triList[j].vtx[jj].index;
							if( vtxind<0 || vtxind>=trimesh->vcnt ) { // vtxCount) {
  egi_dpstd(DBG_RED"Error in parsing glMeshes[%zu].primitive[%zu]: vtxind out of range!\n"DBG_RESET, m,n);
  egi_dpstd(DBG_RED"Stop reading glMesh, with triMeshList.size()=%zu\n"DBG_RESET,triMeshList.size());
								vtxind=0;

								trimesh->triGroupList.clear(); /* Clear this trimesh */
								ret=-1;
								goto END_FUNC;
							}

							trimesh->triList[j].vtx[jj].u=trimesh->vtxList[vtxind].u;
							trimesh->triList[j].vtx[jj].v=trimesh->vtxList[vtxind].v;

						//	printf("trimesh->triList[%d].vtx[%d].u/v:%f,%f\n",
						//		j, jj, trimesh->triList[j].vtx[jj].u, trimesh->triList[j].vtx[jj].v);
						   }
						}
					    }
					}

				} /* END 11.3.4.2 Assign img_kd : triList[].vtx[].u/v */

///////////////////////////////////////////  uv for img_ke  ///////////////////////////////////////////

				/* 11.3.4.1A Check img_ke */
                    		/* see 10a. for mtList[].img_ke=glImages[].imgbuf */
				//if(mtlList[materialIndex].img_ke==NULL) /* NOW: glImages[].imgbuf is NULL! */
				if( emissiveTextureIndex <0 )
					egi_dpstd(DBG_RED"emissiveTextureIndex <0! Ignore mtlList[].triListUV_ke.\n"DBG_RESET);

				/* 11.3.4.2 Assign mtList[].triListUV_ke */
		                //else if(  mtlList[materialIndex].img_ke!=NULL &&
				     //glMaterials[materialIndex].emissiveTexture.index>=0 ) {
				//else if( trimesh->triGroupList[tgCount].mtlID >=0 ) {  //ALWAYS OK!
				else {
					/* 11.3.4.2.0 Resize mtList[].triListUV_ke to tcnt */
					int mtlID=trimesh->triGroupList[tgCount].mtlID;
					int tricnt=trimesh->triGroupList[tgCount].tricnt;
					trimesh->mtlList[mtlID].triListUV_ke.resize(tricnt);

				        egi_dpstd(DBG_YELLOW"Assign uv for img_ke, to mtList[].triListUV_ke...\n"DBG_RESET);

                   			int textureIndex=glMaterials[materialIndex].emissiveTexture.index;

					/* 11.3.4.2.1 If texture image exists */
                   			if(textureIndex>=0 && glTextures[textureIndex].imageIndex>=0 ) {
                        		    /* see 10a. for mtList[].img_kd=glImages[].imgbuf */
					    //if(mtList[materialIndex].img_kd==NULL) /* NOW: glImages[].imgbuf is NULL! Ownership changed.*/
					    /* 11.3.4.2.1.1  HK2023-01-03 */
					    int nn=glMaterials[materialIndex].emissiveTexture.texCoord;
					    //assume nn is OK, integrity checked in E3D_glLoadTextureInfo()
					    int texcoord=glMeshes[m].primitives[n].attributes.texcoord[nn]; //as Accessors index
					    if(texcoord<0)
					    	egi_dpstd(DBG_RED"--- img_ke texcoord(accIndex)=%d<0!\n"DBG_RESET,texcoord);
					    /* 11.3.4.2.1.2 */
					    if( texcoord >-1 && texcoord < (int)glAccessors.size() ) {

egi_dpstd(DBG_YELLOW"Read img_ke u/v from glAccessors[texcoord(accindx)=%d], elemtype: %s, count=%d, elemsize=%d\n"DBG_RESET,
				texcoord, glAccessors[texcoord].type.c_str(), glAccessors[texcoord].count, glAccessors[texcoord].elemsize);

						/* 11.3.4.2.1.2.1 TODO: byteStripe of glAccessors[].data NOT considered */
						fdat=(float *)glAccessors[texcoord].data;

						/* Check element type */
						if(glAccessors[texcoord].type.compare("VEC2")) {
							egi_dpstd(DBG_RED"Critical: glAccessors[texcoord=%d].type:'%s', it's is NOT VEC2!\n"DBG_RESET,
									texcoord, glAccessors[texcoord].type.c_str());

							/* Clear vtxList/triList */
							trimesh->vcnt=0; trimesh->tcnt=0;  trimesh->triGroupList.clear();

							ret=-1;
							goto END_FUNC;
						}

						/* 11.3.4.2.1.2.2 */
			                        /* Get component size. component types: float, unsinged byte normalized, unsinged short normalized. */
                        			int compsize=glAccessors[texcoord].elemsize/2; /* for VEC2, number_of_components=2 */
						int compmax=(1<<(compsize<<3))-1; /* component max. value */
						float invcompmax=1.0/compmax;
						int fcnt=0; //count vertices in this primitive

			egi_dpstd("Vertice in this primitive: %d\n", vtxCount-Vsdx);

					    	/* 11.3.4.2.1.2.3 Texture coord: VEC2 float Textrue coordinates */
						/* Get and assign vtxList[].u/v */
						for(int j=Vsdx; j<vtxCount; j++) {  /* traverse vertice index in this primitive */
						    if(compsize==4) {  //component type: float

							/***                  !!!--- CAUTION ---!!!
							 * Temp. save to vtxList[].u/v, later copy to mtlList[].triListUV_ke.
							 * see 11.3.4.2.1.2.4
							 */

							 trimesh->vtxList[j].u=fdat[fcnt*2];
							/* Hahaha... same UV coord direction as EGI E3D :) */
							 trimesh->vtxList[j].v=fdat[fcnt*2+1];

			//				printf("vtxList[%d].u/v:%f,%f\n",j,vtxList[j].u,vtxList[j].v);

			/* TEST: ---------- */
			if( trimesh->vtxList[j].u>1.0f || trimesh->vtxList[j].v>1.0f || trimesh->vtxList[j].u<0.0f || trimesh->vtxList[j].v<0.0f)
					egi_dpstd("For triListUV_ke: vtxList[].u/v(%f,%f) is NOT within [0 1]!\n",
									trimesh->vtxList[j].u, trimesh->vtxList[j].v);

							/* Try to adjust u/v to within [0 1]; TODO sampler NOT HERE! */
						    }
						    else if(compsize<4) { //component type: unsinged byte normalized, unsinged short normalized
							egi_dpstd(DBG_CYAN"TexCoordUV component type is NOT float!\n"DBG_RESET);
							/* componet: u */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2)*compsize+kk];  //1byte incremental
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							trimesh->vtxList[j].u=uind*invcompmax;

							/* component: v */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2+1)*compsize+kk];
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							trimesh->vtxList[j].v=uind*invcompmax;
						    }

						    fcnt++; /* count vertices */

						} /* End for(j) */

						/* 11.3.4.2.1.2.4  TODO: NOW E3D_TriMesh::renderMesh() reads UV from triList[].vtx[].u/v */
						int vtxind;
				  		/* For img_ke: Store uv mtlList[].triListUV_ke  nn--- see 11.3.4.2.1.1 */
						for(int j=Tsdx; j<triCount; j++) {
						   for(int jj=0; jj<3; jj++) {
							vtxind=trimesh->triList[j].vtx[jj].index;
							if(vtxind<0 || vtxind>=vtxCount)
								return -1;

							/* triGroupUV corresponding to triList[].vtx[] */
							trimesh->mtlList[mtlID].triListUV_ke[j].u[jj]=trimesh->vtxList[vtxind].u;
							trimesh->mtlList[mtlID].triListUV_ke[j].v[jj]=trimesh->vtxList[vtxind].v;
						   }
						}
					    }
					}

				} /* END 11.3.4.2 Assign uv for img_ke */

/////////////////////////////////////////////////////////////////////////////////////////////////

#if 0 //////<<<<<<<<<<< TEST_BREAK:  Assume img_kn use the SAME uv TexCoord as img_kd's

///////////////////////////////////////////  uv for img_kn HK2022-01-24  ///////////////////////////////////////////

				/* 11.3.4.1A Check img_kn */
                    		/* see 10a. for mtList[].img_kn=glImages[].imgbuf */
				if(trimesh->mtlList[materialIndex].img_kn==NULL) /* NOW: glImages[].imgbuf is NULL! */
 				        egi_dpstd(DBG_RED"mtList[materialIndex=%d].img_kn==NULL! Ignore triList[].vtx[].u/v .\n"DBG_RESET,
							materialIndex);

				/* 11.3.4.2 Assign mtList[].triListUV_kn */
		                else if( trimesh->mtlList[materialIndex].img_kn!=NULL &&
				         glMaterials[materialIndex].normalTexture.index>=0 ) {

					/* 11.3.4.2.0 Resize mtList[].triListUV_kn to tcnt */
					int mtlID=trimesh->triGroupList[tgCount].mtlID;
					int tricnt=trimesh->triGroupList[tgCount].tricnt;
					trimesh->mtlList[mtlID].triListUV_kn.resize(tricnt);

				        egi_dpstd(DBG_YELLOW"Assign uv for img_kn, to mtList[].triListUV_kn...\n"DBG_RESET);

                   			int textureIndex=glMaterials[materialIndex].normalTexture.index;

					/* 11.3.4.2.1 If texture image exists */
                   			if(textureIndex>=0 && glTextures[textureIndex].imageIndex>=0 ) {
                        		    /* see 10a. for mtList[].img_kn=glImages[].imgbuf */
					    //if(mtList[materialIndex].img_kn==NULL) /* NOW: glImages[].imgbuf is NULL! Ownership changed.*/
					    /* 11.3.4.2.1.1 */
					    int nn=glMaterials[materialIndex].normalTexture.texCoord;
					    //assume nn is OK, integrity checked in E3D_glLoadTextureInfo()
					    int texcoord=glMeshes[m].primitives[n].attributes.texcoord[nn]; //as Accessors index
					    if(texcoord<0)
					    	egi_dpstd(DBG_RED"--- img_kn texcoord(accIndex)=%d<0!\n"DBG_RESET,texcoord);
					    /* 11.3.4.2.1.2 */
					    if( texcoord >-1 && texcoord < (int)glAccessors.size() ) {

egi_dpstd(DBG_YELLOW"Read img_kn u/v from glAccessors[texcoord(accindx)=%d], elemtype: %s, count=%d, elemsize=%d\n"DBG_RESET,
				texcoord, glAccessors[texcoord].type.c_str(), glAccessors[texcoord].count, glAccessors[texcoord].elemsize);

						/* 11.3.4.2.1.2.1 TODO: byteStripe of glAccessors[].data NOT considered */
						fdat=(float *)glAccessors[texcoord].data;

						/* Check element type */
						if(glAccessors[texcoord].type.compare("VEC2")) {
							egi_dpstd(DBG_RED"Critical: glAccessors[texcoord=%d].type:'%s', it's is NOT VEC2!\n"DBG_RESET,
									texcoord, glAccessors[texcoord].type.c_str());

							/* Clear vtxList/triList */
							trimesh->vcnt=0; trimesh->tcnt=0;  trimesh->triGroupList.clear();
							ret=-1; goto END_FUNC;
						}

						/* 11.3.4.2.1.2.2 */
			                        /* Get component size. component types: float, unsinged byte normalized, unsinged short normalized. */
                        			int compsize=glAccessors[texcoord].elemsize/2; /* for VEC2, number_of_components=2 */
						int compmax=(1<<(compsize<<3))-1; /* component max. value */
						float invcompmax=1.0/compmax;
						int fcnt=0; //count vertices in this primitive

			egi_dpstd("Vertice in this primitive: %d\n", vtxCount-Vsdx);

					    	/* 11.3.4.2.1.2.3 Texture coord: VEC2 float Textrue coordinates */
						/* Get and assign vtxList[].u/v */
						for(int j=Vsdx; j<vtxCount; j++) {  /* traverse vertice index in this primitive */
						    if(compsize==4) {  //component type: float
							 trimesh->vtxList[j].u=fdat[fcnt*2];
							/* Hahaha... same UV coord direction as EGI E3D :) */
							 trimesh->vtxList[j].v=fdat[fcnt*2+1];

			//				printf("vtxList[%d].u/v:%f,%f\n",j,vtxList[j].u,vtxList[j].v);

				/* TEST: ---------- uv NOT within [0 1] */
			if( trimesh->vtxList[j].u>1.0f || trimesh->vtxList[j].v>1.0f || trimesh->vtxList[j].u<0.0f || trimesh->vtxList[j].v<0.0f)
					egi_dpstd("For triListUV_ke: vtxList[].u/v(%f,%f) is NOT within [0 1]!\n",
									trimesh->vtxList[j].u, trimesh->vtxList[j].v);

							/* Try to adjust u/v to within [0 1]; TODO sampler NOT HERE! */

						    }
						    else if(compsize<4) { //component type: unsinged byte normalized, unsinged short normalized
							egi_dpstd(DBG_CYAN"TexCoordUV component type is NOT float!\n"DBG_RESET);
							/* componet: u */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2)*compsize+kk];  //1byte incremental
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							trimesh->vtxList[j].u=uind*invcompmax;

							/* component: v */
							for(int kk=0; kk<compsize; kk++) {
							   uind=0;
                                                	   utmp=glAccessors[texcoord].data[(fcnt*2+1)*compsize+kk];
							   /* little endian */
                                                	   uind += utmp<<(kk<<3); //kk*8
							}
							trimesh->vtxList[j].v=uind*invcompmax;
						    }

						    fcnt++; /* count vertices */

						} /* End for(j) */

						/* 11.3.4.2.1.2.4  TODO: NOW E3D_TriMesh::renderMesh() reads UV from triList[].vtx[].u/v */
						int vtxind;
				  		/* For img_kn: Store uv mtlList[].triListUV_kn  nn--- see 11.3.4.2.1.1 */
						for(int j=Tsdx; j<triCount; j++) {
						   for(int jj=0; jj<3; jj++) {
							vtxind=trimesh->triList[j].vtx[jj].index;
							if(vtxind<0 || vtxind>=vtxCount)
								return -1;

							/* triGroupUV corresponding to triList[].vtx[] */
							trimesh->mtlList[materialIndex].triListUV_kn[j].u[jj]=trimesh->vtxList[vtxind].u;
							trimesh->mtlList[materialIndex].triListUV_kn[j].v[jj]=trimesh->vtxList[vtxind].v;
						   }
						}
					    }
					}

				} /* END 11.3.4.2 Assign uv for img_kn */
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif  /////<<<<<<<<<<<<<<<< TEST_BREAK

			}  /* END 11.3.4  Set material for trigroup */

			/* 11.3.4 Load morph weights from primitie to corresponding trimesh.trigroup */
			size_t wsize=glMeshes[m].mweights.size();
			size_t msize=glMeshes[m].primitives[n].morphs.size();
			int positionAccIndex, normalAccIndex;
			//float *fdat; defined at 11.
			if(msize>0 ) {
				/* Reset trigroup morphs accordingly */
				trimesh->triGroupList[tgCount].morphs.resize(msize);

				for(size_t mk=0; mk<msize; mk++) {
				   /* POSITION   for each primitive.morphs[]. Displacement for each primitive vertex position. */
				   positionAccIndex = glMeshes[m].primitives[n].morphs[mk].positionAccIndex;
				   if( positionAccIndex >-1 ) {
				   	/* Check accessor data */
					if( glAccessors[positionAccIndex].componentType!=5126) { /* component: float */
						egi_dpstd(DBG_RED"morph POSITION accessor data type error!\n"DBG_RESET);
					}
					else if( glAccessors[positionAccIndex].count < primitiveVtxCnt) { /* element: VE3 */
						egi_dpstd(DBG_RED"morph POSITION accessor data length error!\n"DBG_RESET);
					}
					else {
						/* Get float data pointer */
						fdat=(float *)glAccessors[positionAccIndex].data;

						/* Resize  trigroup.morphs.positions. here triGroupList[].morphs[].postions[] is E3D_Vector array */
						trimesh->triGroupList[tgCount].morphs[mk].positions.resize(primitiveVtxCnt);

						/* Assign trigroup.morphs.positions */
						for(int pk=0; pk<primitiveVtxCnt; pk++) {  /* Reas 3 components(float) each time */
							trimesh->triGroupList[tgCount].morphs[mk].positions[pk].x= *(fdat+pk*3);
							trimesh->triGroupList[tgCount].morphs[mk].positions[pk].y= *(fdat+pk*3+1);
							trimesh->triGroupList[tgCount].morphs[mk].positions[pk].z= *(fdat+pk*3+2);
						}
					}
				   }
				   /* NORMAL  for each primitive.morphs[]. Displacement for each primitive vertex normal. */
				   normalAccIndex = glMeshes[m].primitives[n].morphs[mk].normalAccIndex;
				   if( normalAccIndex >-1 ) {
				   	/* Check accessor data */
					if( glAccessors[normalAccIndex].componentType!=5126) { /* component: float */
						egi_dpstd(DBG_RED"morph NORMAL accessor data type error!\n"DBG_RESET);
					}
					else if( glAccessors[normalAccIndex].count < primitiveVtxCnt) {  /* Element VEC3 */
						egi_dpstd(DBG_RED"morph NORMAL accessor data length error!\n"DBG_RESET);
					}
					else {
						/* Get float data pointer */
						fdat=(float *)glAccessors[normalAccIndex].data;

						/* Resize  trigroup.morphs.normals. */
						trimesh->triGroupList[tgCount].morphs[mk].normals.resize(primitiveVtxCnt);

						/* Assign trigroup.morphs.normals */
						for(int pk=0; pk<primitiveVtxCnt; pk++) {  /* Reas 3 components(float) each time */
							trimesh->triGroupList[tgCount].morphs[mk].normals[pk].x= *(fdat+pk*3);
							trimesh->triGroupList[tgCount].morphs[mk].normals[pk].y= *(fdat+pk*3+1);
							trimesh->triGroupList[tgCount].morphs[mk].normals[pk].z= *(fdat+pk*3+2);
						}
					}

                                   }

				   /* TODO: TANGENT,TEXCOORD,COLOR */
				}
			}


#if 0 /* TEST: Hide triGroup which has no baseColorFactor defined ------------------- */
			if(triGroupList[tgCount].mtlID>-1) {
			    if(!glMaterials[triGroupList[tgCount].mtlID].pbrMetallicRoughness.baseColorFactor_defined)
                        	triGroupList[tgCount].hidden=true;
			}
			/* OR has no mtlID, USUally not */
			if(triGroupList[tgCount].mtlID<0)
				triGroupList[tgCount].hidden=true;
#endif //---------------------------------------------------------------------------

			/* 11.3.5 tgCount incremental at last! */
			tgCount +=1;

		} /* END if(mode==4 && accIndex>=0 ) */


	    } /* End 11.0 for(n) traverse primitives */


	    /* 11.1 update triface mormals */
	    trimesh->updateAllTriNormals();

	    /* 11.2 Summit for each mesh */
	    egi_dpstd(DBG_MAGENTA"      --- triMeshList[%zu] Summit ---\n"DBG_RESET, m);
	    egi_dpstd("  Pre_count %d triangles, %d vertices, and %d triGroups \n", triCountBk, vtxCountBk, tgCountBk);
	    egi_dpstd("  Totally read %d triangles, %d vertices, and %d triGroups \n", triCount, vtxCount, tgCount);
	    egi_dpstd("  Space with tCapacity=%d, vCapacity=%d\n", trimesh->triCapacity(), trimesh->vtxCapacity());
	    egi_dpstd("  backFaceOn: %s\n",  trimesh->backFaceOn ? "True":"False");
	    egi_dpstd("  Materials:\n");
	  for( size_t k=0; k < trimesh->mtlList.size(); k++) {
	    egi_dpstd("      name: '%s'\n", trimesh->mtlList[k].name.c_str());
	    egi_dpstd("      img_kd: %s, img_ke: %s, img_kn: %s\n",
		      trimesh->mtlList[k].img_kd?"YES":"NO", trimesh->mtlList[k].img_ke?"YES":"NO",trimesh->mtlList[k].img_kn?"YES":"NO" );
          }

          for( size_t k=0; k < trimesh->triGroupList.size(); k++) {
		egi_dpstd(DBG_MAGENTA"  triMeshList[%zu].triGroupList[%zu]\n"DBG_RESET, m, k);
		egi_dpstd("     name: %s\n",trimesh->triGroupList[k].name.c_str());
		egi_dpstd("     tricnt: %d\n",trimesh->triGroupList[k].tricnt);
		egi_dpstd("     vtxcnt: %d\n",trimesh->triGroupList[k].vtxcnt);
		egi_dpstd("     mtlID: %d\n",trimesh->triGroupList[k].mtlID);
		egi_dpstd("     backFaceON: %s\n", trimesh->triGroupList[k].backFaceOn ? "True":"False");
	  }


	} /* End 11.-1 for(m) traverse meshes */


#if 1	/* TEST: ---- check results triMeshList[] */
	for(size_t k=0; k<triMeshList.size(); k++) {

   #if 0 /* TEST: ----- Add one instatnce for each triMeshList.  */
		addMeshInstance(*triMeshList[k], "_instance");
   #endif

		/* Print mtlList */
		for(size_t kk=0; kk<triMeshList[k]->mtlList.size(); kk++) {
			egi_dpstd(DBG_MAGENTA"      --- triMeshList[%zu]->mtlList[%zu] ---\n"DBG_RESET, k,kk);
			triMeshList[k]->mtlList[kk].print();
		}

   #if 0  /* TEST: ------- vtx UVs */
		for(int j=0; j < triMeshList[k]->tcnt; j++) {
		    for(size_t jj=0; jj<3; jj++) {
                         printf("triMeshList[%zu]->triList[%zu].vtx[%d].u/v:%f,%f\n",
                                 k, j, jj, triMeshList[k]->triList[j].vtx[jj].u, triMeshList[k]->triList[j].vtx[jj].v);
		    }
		}
   #endif

   #if 0 /* TEST: print out -------- vtx Normals */
		for(int j=0; j < triMeshList[k]->vcnt; j++) {
			 triMeshList[k]->vtxList[j].normal.normalize();
                          printf("triMeshList[%zu]->vtxList[%zu].normal: %f,%f,%f\n", k,j,
			         triMeshList[k]->vtxList[j].normal.x, triMeshList[k]->vtxList[j].normal.y, triMeshList[k]->vtxList[j].normal.z);
		}
   #endif

   #if 1 /* TEST: print out triGroupList[].skins -------------------- */
	   if( glMeshes[k].skinIndex >=0 ) {
	        egi_dpstd(DBG_MAGENTA"	--- triMeshList[%zu].skins ---\n"DBG_RESET, k);
	     #if 1 ///////////////////
		printf("	OK, triMeshList[%zu]  has skin defined!\n", k);
	     #else ////////////////////
		for(size_t i=0; i<triMeshList[k]->triGroupList.size(); i++) {
		    printf("triGroupList[%zu]:\n", i);
		    for(size_t j=0; j<triMeshList[k]->triGroupList[i].vtxcnt; j++) {
			printf("    vtx[%d]: weights[%f, %f, %f, %f], jointNodes[%d, %d, %d, %d]\n",
					triMeshList[k]->triGroupList[i].stidx+j,
					triMeshList[k]->triGroupList[i].skins[j].weights[0],
					triMeshList[k]->triGroupList[i].skins[j].weights[1],
					triMeshList[k]->triGroupList[i].skins[j].weights[2],
					triMeshList[k]->triGroupList[i].skins[j].weights[3],
					triMeshList[k]->triGroupList[i].skins[j].jointNodes[0],
					triMeshList[k]->triGroupList[i].skins[j].jointNodes[1],
					triMeshList[k]->triGroupList[i].skins[j].jointNodes[2],
					triMeshList[k]->triGroupList[i].skins[j].jointNodes[3]    );
		    }
		}
	     #endif //////////////////////
	   }
	   else
		printf("   glMeshes[%zu].skinIndex=%d, No skin.\n", k, glMeshes[k].skinIndex);

   #endif

   #if 0 /* TEST: -------- backFaceOn */
		printf("triMeshList[0]->backFaceOn: %s\n", triMeshList[0]->backFaceOn ? "True" : "False");
		printf("triMeshList[0]->triGroupList[0].backFaceOn: %s\n", triMeshList[0]->triGroupList[0].backFaceOn ? "True" : "False");
   #endif


	}
#endif



	/* 12. Load animation data into glNodes.animQtsStacks and glNodes.animMorphWtsStacks
	 * Note:
		1. TODO NOW only extract glAnimations[0]

	  glAnimation ---> glAnimChannels  ---> glAnimChanTarget ---> node and TRS type.
           		                |
                        	        |---> glAnimChanSampler ---> accessor indices for keyframe timestamps and TRS data.
	 ***/

        /* Resize glNodes[].animQtsStacks and glNodes[].animMorphwtsStacks
	 * Note: Allocate capacity for both items, though there's data ONLY for animQts or animMorphwts!
	 */
	animTotal=glAnimations.size();
	if(animTotal>0) {
		for(size_t ka=0; ka<glNodes.size(); ka++) {
			glNodes[ka].animQtsStacks.resize(animTotal);
			glNodes[ka].animMorphwtsStacks.resize(animTotal);
		}
	}


	egi_dpstd(DBG_MAGENTA"  --- Start to load animation data into glNodes.animQtsStacks/animMorphWtsStacks...\n"DBG_RESET);
	egi_dpstd("  glAnimations.size()=%zu\n",glAnimations.size());

	for( size_t m=0; m<glAnimations.size(); m++)
	{
#if 1 /* TODO: NOW ONLY first ANIMATION */
		if(m==1)break; /* <--------------------ONLY FIRST ANIMATION */
#endif

		int node;  /* node index */
		int pathType; /* TRS type */
		int samplerIndex;
		int input,output;  /* input/output as index of accessor to timestamps and SRT data */

		int mwsize;	   /* size of mesh.weights[] */
		vector<float> weights; /* weights.size()=mwsize, resize see 12.1.18 */

		float ts;    /* timestamp */
		E3D_Vector sxyz;  /* Scale */
		E3D_Quaternion q; /* Rotation */
		E3D_Vector txyz;  /* Translation */

		egi_dpstd(DBG_MAGENTA"Read animation data for glAnimation_%zu/(%zu-1) '%s' ...\n"DBG_RESET,
									m, glAnimations.size(), glAnimations[m].name.c_str());
		sleep(1);

		/* 12.1 Traverse channels, each channel refers to a node */
		for( size_t n=0; n<glAnimations[m].channels.size(); n++) {

			/* 12.1.1 Get node,pathType,samplerIndex */
			node=glAnimations[m].channels[n].target.node;
			pathType=glAnimations[m].channels[n].target.pathType;
			samplerIndex=glAnimations[m].channels[n].samplerIndex;

			/* 12.1.1A If node is a mesh, get size of mesh.weights */
			mwsize=0; /* Reset first */
			if(glNodes[node].type==glNode_Mesh) {
				if(glNodes[node].meshIndex>=0 && glNodes[node].meshIndex<(int)glMeshes.size() )
					mwsize=glMeshes[glNodes[node].meshIndex].mweights.size();
				//else
				//	mwsize=0;
			}
//			egi_dpstd("glAnimations[%d].channels[%d].target.node: %d\n",m,n, node);
			if(mwsize>0) /* If has morph weights */
			    egi_dpstd("glAnimations[%zu].channels[%zu]: Mesh Morph weights.size=%d\n",m,n,mwsize);

			/* 12.1.1B resize weights according to glMeshes[].mweights.size() */
			if(mwsize>0) {
				weights.resize(mwsize);
			}

			/* 12.1.2 Check integrity */
			if( node<0 || node> (int)glNodes.size()-1
			    || pathType<0 || pathType >3
			    || samplerIndex<0 || samplerIndex >= (int)glAnimations[m].samplers.size() )
			{
				egi_dpstd(DBG_RED"Integrity error! node=%d,pathType=%d,samplerIndex=%d\n"DBG_RESET,
								node, pathType, samplerIndex );
				continue; /* Ignore this channel */
			}

			/* 12.1.3 Get accessor index */
			input=glAnimations[m].samplers[samplerIndex].input;
			output=glAnimations[m].samplers[samplerIndex].output;
			if(input<0 || input>=(int)glAccessors.size()
			   || output<0 || output>=(int)glAccessors.size() )
			{
				egi_dpstd(DBG_RED"Accessor index error! input=%d, ouput=%d, glAccessors.size=%zu\n"DBG_RESET,
							input,output, glAccessors.size() );
				continue; /* Ignore this channel */
			}

#if 1 			/* 12.1.4 Check input/output elements number--- NOT nessary! NOT here */
			if( pathType!=ANIMPATH_WEIGHTS && glAccessors[input].count != glAccessors[output].count ) {
				egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: input/output DO NOT has same glAccssors[].count!\n"DBG_RESET, m,n);
				egi_dpstd(DBG_RED"glAccessors[input].count=%d, glAccessors[output].count=%d\n"DBG_RESET,
							glAccessors[input].count, glAccessors[output].count);
				continue; /* Ignore this channel */
			}
#endif
			/* 12.1.5 Check input/output component type */
			/* TODO: Only support float component_type, signed byte normalized/unsigned byte normalized ... NOT supported */
			if( glAccessors[input].componentType != 5126 || glAccessors[output].componentType != 5126 ) {
				egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: input/output componentType is NOT float!\n"DBG_RESET, m,n);
				continue; /* Ignore this channel */
			}

			/* 12.1.6. Check input element type */
			if( glAccessors[input].type.compare("SCALAR") ) {
				egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: input element type is NOT SCALAR!\n"DBG_RESET,m,n);
				continue; /* Ignore this channel */
			}
			/* 12.1.7 Check output element type */
			bool dataError=false;
//			egi_dpstd(DBG_YELLOW"glAnimations[%d].channels[%d]: pathType=%d\n"DBG_RESET, m,n, pathType);
			switch (pathType) {
				case ANIMPATH_WEIGHTS:
				   if( glAccessors[output].type.compare("SCALAR") ) {
                                   	egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: pathtype 'weights' data is NOT SCALAR!\n"DBG_RESET,m,n);
				   	dataError=true;
				   }
				   break;
				case ANIMPATH_TRANSLATION:
				   if( glAccessors[output].type.compare("VEC3") ) {
                                         egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: pathtype 'translation' data is NOT VEC3!\n"DBG_RESET,m,n);
	                                 dataError=true;
				   }
                                   break;
				case ANIMPATH_ROTATION:
				   if( glAccessors[output].type.compare("VEC4") ) {
                                   	egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: pathtype 'rotation' data is NOT VEC4!\n"DBG_RESET,m,n);
	                                dataError=true;
				   }
                                   break;
				case ANIMPATH_SCALE:
				   if( glAccessors[output].type.compare("VEC3") ) {
                                        egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: pathtype 'scale' data is NOT VEC3!\n"DBG_RESET,m,n);
	                                dataError=true;
				   }
                                   break;
				default: /* pathtype error, OK check in  12.1.2 */
				   egi_dpstd(DBG_RED"glAnimations[%zu].channels[%zu]: pathtype=%d is NOT supported!\n"DBG_RESET,m,n,pathType);
				   dataError=true;
				   break;
			}

			/* 12.1.8 Ignor if dataError */
			if(dataError)
				continue; /* Ignore this channel */

			/* 12.1.9 Get data pointer */
			float *inputData=(float *)glAccessors[input].data;  /* component float ONLY */
			float *outputData=(float *)glAccessors[output].data; /* component float ONLY */

			if(glAccessors[output].normalized) {
printf(DBG_RED"glAccessors[%d].normalized! componentType=%d\n"DBG_RESET, output, glAccessors[output].componentType);
			}

			if( glAccessors[input].componentType!=5126 || glAccessors[output].componentType!=5126 ) {
printf(DBG_RED"glAccessors[].type is NOT float(5126)! glAccessors[input].componentType=%d, glAccessors[output].componentType=%d\n"DBG_RESET,
				glAccessors[input].componentType, glAccessors[output].componentType);
			}

			if( pathType!=ANIMPATH_WEIGHTS && (glAccessors[input].count != glAccessors[output].count) ) {
printf(DBG_RED"glAccessors[input].count(%d) != glAccessors[output].count(%d)\n"DBG_RESET,glAccessors[input].count, glAccessors[output].count);
			}

			/* 12.1.10 Get TRS and Morph_Weiths data */
			for( int cnt=0; cnt<glAccessors[input].count; cnt++) {  /* Frame count */
				/* 12.1.10.1 Get timestamp value */
				ts=inputData[cnt];

				/* 12.1.10.1a Update glNodes[node].animQtsStacks[m].tsmin/tsmax HK2023-04-06/07 */
				if(pathType==ANIMPATH_WEIGHTS) {
				    if(ts > glNodes[node].animMorphwtsStacks[m].tsmax)
					glNodes[node].animMorphwtsStacks[m].tsmax=ts;
				    else if(ts < glNodes[node].animMorphwtsStacks[m].tsmin)
					glNodes[node].animMorphwtsStacks[m].tsmin=ts;
				}
				else {   /* Case SRT Animations */
				    if(ts > glNodes[node].animQtsStacks[m].tsmax)
					glNodes[node].animQtsStacks[m].tsmax=ts;
				    else if(ts < glNodes[node].animQtsStacks[m].tsmin)
					glNodes[node].animQtsStacks[m].tsmin=ts;
				}

				/* 12.1.10.2 Get SRT parts and morph_weights */
				switch (pathType) {
					case ANIMPATH_WEIGHTS:
						/* MUST be same size as glMesh.weights.size(): mwsize */
						if(glAccessors[output].count < mwsize*glAccessors[input].count ) {
							egi_dpstd(DBG_RED"glAccessors[output].count NOT enought for weigths array!\n"DBG_RESET);
							break;
						}

						/* Read out weights for one frame */
						for(int kw=0; kw<mwsize; kw++) {
							weights[kw]=outputData[mwsize*cnt+kw];
						}

						/* Insert frame weights to node.animMorphwtsStacks */
						glNodes[node].animMorphwtsStacks[m].insert(ts, weights);

						break;
					case ANIMPATH_TRANSLATION:
						/* [tx ty tz] */
						txyz.x=outputData[3*cnt];
						txyz.y=outputData[3*cnt+1];
						txyz.z=outputData[3*cnt+2];

						/* Insert to node.animQts */
						glNodes[node].animQtsStacks[m].insertSRT(ts, "T", sxyz, q, txyz);

						break;
					case ANIMPATH_ROTATION:
						/* [x y z w] */
						q.x=outputData[4*cnt];
						q.y=outputData[4*cnt+1];
						q.z=outputData[4*cnt+2];
						q.w=outputData[4*cnt+3];

						/* Insert to node.animQts */
						glNodes[node].animQtsStacks[m].insertSRT(ts, "R", sxyz, q, txyz);
						break;
					case ANIMPATH_SCALE:
						/* [sx sy sz] */
						sxyz.x=outputData[3*cnt];
						sxyz.y=outputData[3*cnt+1];
						sxyz.z=outputData[3*cnt+2];

						/* Insert to node.animQts */
						glNodes[node].animQtsStacks[m].insertSRT(ts, "S", sxyz, q, txyz);
						break;
				}

			}

		} /* End 12.1 for(n) */


#if 1 	 	/* 12.2 Check and rectify animation animQtsStacks[].qts ----------- HK2023-04-11
		        --- CROSS CHECK with loadGLTF():12.2 ---
		 */

egi_dpstd("Try to check and recitify animQtsStacks[%zu].qts data...\n", m); sleep(1);

		for(size_t k=0; k<glNodes.size(); k++) {
			/* XXX 12.2.1 Disable nonjoint nodes
			   --- Also CROSS-CHECK to Scene::updateNodeGJmat().1.2.2 ---
			 */
			//if(!glNodes[k].IsJoint )  Also for non_joint nodes!
			//	continue;

			/* 12.2.2 If animQtsStacks[].qts is empty, insert qt at 0.0f */
			if(glNodes[k].animQtsStacks[m].qts.empty()) {
				E3D_Quatrix qt;
				if(glNodes[k].SRT_defined) {
                                	qt=glNodes[k].qt;
					glNodes[k].animQtsStacks[m].insert(0.0f, qt);
				}
				else {  /* TODO: to avoid scaleMat in pmat,  qts[0] NOT fromMatrix pmat!
					 * CROSS-CHEK updateNodeGJmat() 1.2.1/1.2.2  HK2023-04-21 */
					egi_dpstd(DBG_RED"NOT convert to qt, to avoid scaleMat in pamt!\n"DBG_RESET);
					//qt.fromMatrix(glNodes[k].pmat);
					//glNodes[k].animQtsStacks[m].insert(0.0f, qt);
				}
			}
			/* 12.2.3 animQtsStacks[].qts is NOT empty */
			else {
   #if 0 //////////////////////////////* 12.2.3.1 Adjust animQtsStacks[].qts.tx/ty/tz */
				if(  fabsf(glNodes[k].animQtsStacks[m].qts[0].tx)<VPRODUCT_NEARZERO  //VPRODUCT_EPSILON, NEARZERO is more accurate!
					   && fabsf(glNodes[k].animQtsStacks[m].qts[0].ty)<VPRODUCT_NEARZERO  //VPRODUCT_EPSILON
					   && fabsf(glNodes[k].animQtsStacks[m].qts[0].tz)<VPRODUCT_NEARZERO  //VPRODUCT_EPSILON
				)
				//if(glNodes[k].IsJoint)  /* Whatever, all qts.tx/ty/tz to be replaced by pmat */
				{
				    if( !strstr(glNodes[k].name.c_str(), "hips")    /* Hips/pelvis has moving data */
				        && !strstr(glNodes[k].name.c_str(), "Hips")
					&& !strstr(glNodes[k].name.c_str(), "Pelvis")
					&& !strstr(glNodes[k].name.c_str(), "pelvis")
					&& !strstr(glNodes[k].name.c_str(), "rootJoint")
				      )
				    {
				    	/* substitue animQtsStacks[].qts.tx/ty/tz with pmat.tx/ty.tz */
		    	    		for(size_t kk=0; kk<glNodes[k].animQtsStacks[m].qts.size(); kk++)  {
				    		/* Replace qt.tx/ty/tz only */
						glNodes[k].animQtsStacks[m].qts[kk].tx = glNodes[k].pmat.pmat[9];
						glNodes[k].animQtsStacks[m].qts[kk].ty = glNodes[k].pmat.pmat[10];
						glNodes[k].animQtsStacks[m].qts[kk].tz = glNodes[k].pmat.pmat[11];

						/* Replace qts[0] with pmat */
						//XXX if(kk==0) {
						//XXX	glNodes[k].animQtsStacks[m].qts[0].fromMatrix(glNodes[k].pmat);
						//XXX }
			    		}
				    }
			        }
   #else //////////////////////////////* 12.2.3.1 Adjust animQtsStacks[].qts.tx/ty/tz */
			        /* Rectify animQtsStacks[].qts.tx/ty/tz */
				for(size_t kk=0; kk<glNodes[k].animQtsStacks[m].qts.size(); kk++)  {
				    /* TODO??? Hips/pelvis keeps moving data?? */
				    if( strstr(glNodes[k].name.c_str(), "hips")
				        && strstr(glNodes[k].name.c_str(), "Hips")
					&& strstr(glNodes[k].name.c_str(), "Pelvis")
					&& strstr(glNodes[k].name.c_str(), "pelvis")
					&& strstr(glNodes[k].name.c_str(), "rootJoint")
				      ) {
					continue;
				     }

				    /* Smooth some 0s */
				    if( fabsf(glNodes[k].animQtsStacks[m].qts[kk].tx)<VPRODUCT_NEARZERO  //VPRODUCT_EPSILON > NEARZERO
				        && fabsf(glNodes[k].animQtsStacks[m].qts[kk].ty)<VPRODUCT_NEARZERO
					&& fabsf(glNodes[k].animQtsStacks[m].qts[kk].tz)<VPRODUCT_NEARZERO
				    ) {
					egi_dpstd(DBG_YELLOW"Rectify glNodes[%zu].animQtsStacks[%zu].qts[%zu].tx/ty/tz...\n"DBG_RESET, k,m,kk);

					if(kk==0) {
	                                	/* Replace qt.txyz with pmat.txyz */
        	                                glNodes[k].animQtsStacks[m].qts[kk].tx = glNodes[k].pmat.pmat[9];
                	                        glNodes[k].animQtsStacks[m].qts[kk].ty = glNodes[k].pmat.pmat[10];
                        	                glNodes[k].animQtsStacks[m].qts[kk].tz = glNodes[k].pmat.pmat[11];
					}
					else {
						/* Use last data, of qts[kk-1] */
						glNodes[k].animQtsStacks[m].qts[kk].tx = glNodes[k].animQtsStacks[m].qts[kk-1].tx;
						glNodes[k].animQtsStacks[m].qts[kk].ty = glNodes[k].animQtsStacks[m].qts[kk-1].ty;
						glNodes[k].animQtsStacks[m].qts[kk].tz = glNodes[k].animQtsStacks[m].qts[kk-1].tz;
					}
				    }
				}

   #endif

   				/* 12.2.3.2 Adjust animQtsStacks[].qts.q:  when qts[kk].q.w ==1.0/-1.0, replace qts[kk].q with qts[kk-1].q */
			        if( glNodes[k].animQtsStacks[m].qts.size()>1)
				{
					E3D_Quatrix qt;
					if(glNodes[k].SRT_defined)
						qt=glNodes[k].qt;
					else { /* TODO: avoid scaleMat in pmat */
						egi_dpstd(DBG_RED"Todo: avoid scaleMat in pmat! for glNodes[%zu]\n"DBG_RESET, k);
						qt.fromMatrix(glNodes[k].pmat);
					}

					/* For kk=0 */
					if(fabsf(fabsf(glNodes[k].animQtsStacks[m].qts[0].q.w)-1.0)<VPRODUCT_NEARZERO) { //EPSILON
					      egi_dpstd(DBG_YELLOW"Rectify glNodes[%zu].animQtsStacks[%zu].qts[0].q...\n"DBG_RESET, k,m);
					      glNodes[k].animQtsStacks[m].qts[0].q=qt.q;
					}

		    	    		for(size_t kk=1; kk<glNodes[k].animQtsStacks[m].qts.size(); kk++) { /* from kk=1 */
					    	/* Replace with last qts[].q */
					    	if(fabsf(fabsf(glNodes[k].animQtsStacks[m].qts[kk].q.w)-1.0)<VPRODUCT_NEARZERO)  { //EPSILON)
					             egi_dpstd(DBG_YELLOW"Rectify glNodes[%zu].animQtsStacks[%zu].qts[%zu].q...\n"DBG_RESET, k,m,kk);
						     glNodes[k].animQtsStacks[m].qts[kk].q=glNodes[k].animQtsStacks[m].qts[kk-1].q;
						}
					}

				}

				/* 12.2.3.3 If ONLY qts.size==1, Replace qt then. */
				else if( glNodes[k].animQtsStacks[m].qts.size()==1 ) {
                                	/* Replace qt */
                                        if(glNodes[k].SRT_defined)
						glNodes[k].animQtsStacks[m].qts[0]=glNodes[k].qt;
					else { /* TODO: avoid scaleMat in pmat */
						egi_dpstd(DBG_RED"Todo: avoid scaleMat in pmat! for glNodes[%zu]\n"DBG_RESET, k);
                                        	glNodes[k].animQtsStacks[m].qts[0].fromMatrix(glNodes[k].pmat);
					}
				}

			} /* END else nimQtsStacks[].qts is NOT empty */

                   #if 0
			/* TEST: ------- 12.2.4 If qts[0]!= pmat, then convert all qts.  HK2023-04-21
			 * amat'[0]=xmat*pmat,  amat'[n]=xmat*amat[n] ===> to get amat[] as amat[0]=pmat
			 * xmat=amat'[0]*invpmat,  amat[n]=invxmat*amat'[n]
			 */
			E3D_RTMatrix invpmat, invxmat, amat, smat;
			invpmat=glNodes[k].pmat; /* =pmat */
			invpmat.inverse(); /* NOW invpmat */
			glNodes[k].animQtsStacks[m].qts[0].toMatrix(invxmat); /* =amat'[0] */
			invxmat=invxmat*invpmat; /* =xmat (xmat=amat'[0]*invpmat) */
			invxmat.inverse(); /* NOW invxmat */
			for(size_t kk=0; kk<glNodes[k].animQtsStacks[m].qts.size(); kk++) {
				glNodes[k].animQtsStacks[m].qts[kk].toMatrix(amat,smat); /* amat'[n]=smat*amat */

				/* smat as pure scale mat, then amat=invxmat*smat*amat=invxmat*amat*smat */
				amat=invxmat*amat;  /* amat[n]=invxmat*amat'[n]=invxmat*amat*smat, here leave smat. */

				/* amat[n]=invxmat*amat, here leave smat. */
				glNodes[k].animQtsStacks[m].qts[kk].fromMatrix(amat);

				/* +smat.   Acutually, fromMatrix() keeps sx/sy/sz unchanged! */
			 	glNodes[k].animQtsStacks[m].qts[kk].sx=smat.pmat[0]; /* amat[n] + smat */
			 	glNodes[k].animQtsStacks[m].qts[kk].sy=smat.pmat[4];
			 	glNodes[k].animQtsStacks[m].qts[kk].sz=smat.pmat[8];
			}
		  #endif

		} /* END for(k<lgNodes.size()) */

egi_dpstd("\n OK, finish rectifying/adjusting animQtsStacks[%zu].qts\n", m); sleep(2);
#endif


#if 1 /* TEST: glNodes[].animQtsStacks and glNodes[].animMorphwts ------------------- */
  {
		E3D_Quatrix qt;
		E3D_RTMatrix tmat,xmat;

  #if 1 /* TEST: print out glNodes[k].animQtsStacks[m] ------------- */
        	egi_dpstd(DBG_MAGENTA"      --- Animation[%zu] Summit ---\n"DBG_RESET, m);
		printf("   --- tsmin=%f, tsmax=%f ---\n", tsmin, tsmax);
		for(size_t k=0; k<glNodes.size(); k++) {

/* TEST for dancesteps ------- */
//if(k<96) continue;

			printf("   --- glNodes[%zu].animQtsStacks[%zu] ---\n", k,m);
			if(!glNodes[k].animQtsStacks[m].ts.size()) printf("      Empty!\n");
			else {
				glNodes[k].animQtsStacks[m].print();

	#if 1 /* TEST: ------------------ */
				qt=glNodes[k].animQtsStacks[m].qts[0];
				qt.toMatrix(tmat);
				xmat=glNodes[k].pmat;
				xmat.inverse();

				tmat=tmat*xmat;
				tmat.print("===amat[0]*InvPmat===");
	#endif
			}


			printf("   --- glNodes[%zu].animMorphwtsStacks[%zu] ---\n", k,m);
			if(!glNodes[k].animMorphwtsStacks[m].ts.size()) printf("      Empty!\n");
			else glNodes[k].animMorphwtsStacks[m].print();

		}
  #endif

  }
#endif

	} /* End 12. for(m) */


	/* 13. Gamma correction for sRGB imgbuf  ...HK2023-04-24
	 * 		!!!--- CAUTION ---!!!
	 *   Ownership already transfered to scene->imgbufs
	 */
#if 0
	for(size_t k=0; k<glImages.size(); k++) {
		egi_imgbuf_gammaCorrect(glImages[k].imgbuf, 1.0/2.2);
	}
#endif

END_FUNC:

#if 0 /* XXX TEST: ----- re-compute all glNodes[].invbMat HK2023-04-03 */
	rebuildNodeInvbMats(0, _identityMat);

	for(size_t k=0; k<glSkins[0].joints.size(); k++) {
		printf(" --- glNodes[%d].invbMat rebuilt ---\n", glSkins[0].joints[k]);
		glNodes[glSkins[0].joints[k]].invbMat.print();
	}
#endif


	  /* Note:   !!!---  Caution ---!!!
	    *  After 11.3.4.1 (assign mtlList[].img_xx), any case with ret<0 MUST jump here to
            *  transfer Ownership of all glImages[].imgbuf to THIS.imgbufs!
            */
        if(ret!=0) {
		egi_dpstd(DBG_RED"Error happens when import glTF into E3D_Scene!\n"DBG_RESET);
	}

	/* Free fmap */
	//DO NOT! fmap->fp is carried by glBuffer[0].fmap	egi_fmap_free(&fmap);

	/* !!! ---CAUTION --- !!!  Transfer Ownership to  scene->imgbufs, see TODO note 3. */
	for(size_t k; k<glImages.size(); k++) {
		imgbufs.push_back(glImages[k].imgbuf);

		if(imgbufs[k])
		    egi_dpstd("glImages[%zu].imgbuf W*H: %d*%d\n", k, imgbufs[k]->width, imgbufs[k]->height);
		else
		    egi_dpstd(DBG_YELLOW"glImages[%zu].imgbuf is NULL!\n"DBG_RESET,k);
	}

	/* Reset all imgbuf in glImages before exit, Ownership already transfered to scene->imgbufs */
	for(size_t k; k<glImages.size(); k++) {
		glImages[k].imgbuf=NULL;
	}

	return ret;
}

