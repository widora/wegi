/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

              --- EGI 3D Scene and MeshInstance ---


Midas ZhouHK
-----------------------------------------------------------------*/
#ifndef __E3D_SCENE_H__
#define __E3D_SCENE_H__

#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>

#include "e3d_trimesh.h"
#include "e3d_glTF.h"


#if 0 /////////////////////////////////////////////
/*-------------------------------------------------------
        Class: E3D_MeshInstanceNodes
Nodes of MeshInstance, describle hierarchy of MeshInstances.
--------------------------------------------------------*/
class E3D_MeshInstanceNodes {
public:
	/* Constructor */
	E3D_MeshInstanceNodes();
	/* Destructor */
	~E3D_MeshInstanceNodes();


	E3D_MeshInstanceNodes* parent;
	E3D_MeshInstanceNodes* firstChild;
	E3D_MeshInstanceNodes* 

        E3DS_BMTreeNode* firstChild;
        E3DS_BMTreeNode* prevSibling;
        E3DS_BMTreeNode* nextSibling;
};
#endif ////////////////////////////////////////////


/*----------------------------------------------
        Class: E3D_MeshInstance
Instance of E3D_TriMesh.
----------------------------------------------*/
class E3D_MeshInstance {
public:
        /* Constructor */
        E3D_MeshInstance(E3D_TriMesh &refTriMesh, string pname=" ");
        E3D_MeshInstance(E3D_MeshInstance &refInstance, string pname=" ");

        /* Destructor */
        ~E3D_MeshInstance();

        /* Check if instance is completely out of the view frustum */
        bool aabbOutViewFrustum(const E3DS_ProjMatrix &projMatrix);  /* Rename 'isOut...' to 'aabbOut...'  HK2022-10-08 */
        bool aabbInViewFrustum(const E3DS_ProjMatrix &projMatrix);

        /* Render MeshInstance */
        void renderInstance(FBDEV *fbdev, const E3DS_ProjMatrix &projMatrix) const;

        /* Name HK2022-09-29 */
        string name;

        /* Reference E3D_TriMesh. refTriMesh->instanceCount will be modified! */
        E3D_TriMesh & refTriMesh;

        /* objmat matrix for the instance mesh. refTriMesh.objmat to be ignored then! */
        E3D_RTMatrix objmat;   /* Rotation+Translation */

//      E3D_RTMatrix objScaleMat;  /* Scale ONLY. --- OK, may be integrated in objmat */


        /* TriGroup omats. refTriMesh::triGroupList[].omat to be ignored then! TODO. */
        vector<E3D_RTMatrix> omats;

};


/*-------------------------------------------
              Class: E3D_Scene
Settings for an E3D scene.
-------------------------------------------*/
class E3D_Scene {
public:
        /* Constructor */
        E3D_Scene();

        /* Destructor. destroy all E3D_TriMesh in TriMeshList[] */
        ~E3D_Scene();

        /* Default ProjeMatrix  HK2022-12-29 */
        E3DS_ProjMatrix projMatrix;

        /* Import .obj into TriMeshList */
        void importObj(const char *fobj);

	/* Load glTF into THIS Scene */
	int loadGLTF(const string & fgl);
	int loadGLB(const string & fgl);

        /* Add instance */
        void addMeshInstance(E3D_TriMesh &refTriMesh, string pname=" ");
        void addMeshInstance(E3D_MeshInstance &refInstance, string pname=" ");
        /* Clear instance list. ONLY clear meshInstanceList! */
        void clearMeshInstanceList();

	/* === For animation === */
	void setAnimation(int animIndex); /* Set E3D_Scene::animIndex, limit E3D_Scene::animTotal */
	void interpNodeAmats(float t);  /* To update value of each THIS.glNodes[].amat by interpolating its glNode.animQts. */
	void interpNodeAmwts(float t);  /* To update value of each THIS.glNodes[].amwt by interpolating its glNode.animMorphwts. */

	/* Update node's(and its chirdren's) GJmats(gmats+jointMats) */
	void updateNodeGJmat(size_t node, bool pmatON, bool animON, const E3D_RTMatrix &parentGmat); /* --- Recursive --- */

	/* Update all nodes' GJmats(gmats+jointMats) for THIS.scenes[scene]
	    It calls updateNodeGJmat().
	    It will NOT interp to update amat,amwts etc..
	 */
	void updateNodeGJmats(size_t scene, bool pmatON, bool animON, const E3D_RTMatrix &parentGmat);

	/* void updateNodesMwts() is NOT necessary, as a morph takes place under a mesh's local COORD, and is NOT affected
	 by nodes' hierarchical transformation. */

	/* Compute glNode.invbMat for node and its chirldren. USUALLY for the skeleton(rootJoint) node. */
	void rebuildNodeInvbMats(size_t node, const E3D_RTMatrix &parentGmat);  /* --- Recursive --- */

        /* Render Scene.  To render each mesh instance in meshInstaneList[] */
        void renderScene(FBDEV *fbdev, const E3DS_ProjMatrix &projMatrix) const;

	/* Render Node. To render the node and it's chirdren nodes(mesh_type), >>>>> nodes' gmats also updated. <<<<< */
	void renderSceneNode( FBDEV *fbdev, const E3DS_ProjMatrix &_projMatrix, const E3D_RTMatrix &parentMat,  /* --- Recursive --- */
			      int node, bool animON, int shadeType); // XXX const,  glNodes[].gmat modified

       /* Render scene, To render all THIS.glScenes[scene].rootNodes(and their children). */
       void renderScene(FBDEV *fbdev, const E3DS_ProjMatrix &_projMatrix, const E3D_RTMatrix &parentGmat,
					int scene, bool pmatON, bool animON, int shadeType);



        vector<string>  fpathList;          /* List of fpath to the obj file */

        vector<E3D_TriMesh*>  triMeshList;  /* List of TriMesh  <--- Pointer --->  */
					    /* May be parsed and loaded from glFT nodes
					     * It MUST keep same index as glMeshes[]!  see E3D_Scene::loadGLTF()
					     */

/* For TEST: ---------- */
	bool  meshBackFaceOn;		    /* Default: false.
					     * See E3D_Scene::renderSceneNode() 2.2.7: assign refTriMesh->backFaceOn=meshBackFaceOn
					     */
	bool  skinMeshOff;		    /* Default: false. HK2023-04-18
					     * If true, DO NOT draw skin mesh, see at E3D_Scene::renderSceneNode() 2.2.8
					     */
	bool  skeletonLineOn;		    /* Default: false. HK2023-04-17
					     * If true, draw skeleton lines for joint_node in renderSceneNode()
					     */

        //vector<E3D_RTMatrix>  ObjMatList;   /* Corresponding to each TriMesh::objmat  NOPE! */
                                              /* Suppose that triMeshList->objmat ALWAYS is an identity matrix,
                                               * as triMeshList are FOR REFERENCE!
                                               */

        /* Instances List */
        vector<E3D_MeshInstance*> meshInstanceList; /* List of mesh instances
                                                     * Note:
                                                     * 1. Each time a fobj is imported, a MeshInstance is added in the list.
						       2. NOT FOR glTF: glTF parse glNodes to render each mesh.
                                                     */


	/* ---- Following for glTF derived scene ---- */
	vector<EGI_IMGBUF *> imgbufs;	/*** Pointers to all images used.
						  		!!!--- CAUTION ---!!!
					   Before releasing/deconstructing the scene, these imgbufs MUST be freed/released first,
					   then reset all imgbuf in triMeshList[]->mtList[] to NULL, so if there are more
					   than one trimesh holding the same imgbuf pointer, it will NOT be released repeatedly.
					   This is a workaround to settle the problem that meshes in glTF may share the same
					   material, and it's too cost to let each mesh hold a copy of all those images.
					 *
					 */

	/* glTF object nodes */
	vector<E3D_glNode> glNodes;	/* glTF nodes. (type: mesh, skin, camera, node)
					 * Note:
					 * 1. A glNode contains animation data at glNodes[].animQts[]. MAYBE more than 1 set of animation data.
					 *
					 */
	/* glTF animation. TODO: Relation between Scenes and Nodes??
	   time[0] >= 0.0, and strictly increasing values, i.e., time[n + 1] > time[n].
	 */
	float  tsmin;			/* Refere to E3D_Scene::setAnimation(): Update when reset animIndex.
					 * tsmin MAY be <0.0! Init to 0.0f.  XXX See loadGLTF/GLB() 12.1.10.1a.
					 * Reset to be the Min of all glNodes[].animQtsStacks[m].qt[].t
					 * 		!!!--- CAVEAT---!!!
					 *	ALWAYS call setAnimation() to update it.
					 */
	float  tsmax;			/* Refere to E3D_Scene::setAnimation(): Update when reset animIndex.
					 * Max. timestamp value for animation. Init to be 0.0f. XXX See loadGLTF/GLB() 12.1.10.1a.
					 * Reset to be the Max. of all glNodes[].animQtsStacks[m].qt[].t
					 * 		!!!--- CAVEAT---!!!
					 *	ALWAYS call setAnimation() to update it.
					 */

	int    animIndex;		/* Index of animations, maybe more than 1 animation data stored. Init to 0.
					 * 		!!!--- CAVEAT---!!!
					 *	ALWAYS call setAnimation() to update it.
					 */

	int    animTotal;		/* Total number of animation sets. HK2023-04-06 */

	/* glTF scenes, defined root node(s) in each glScene, TODO: NOW only glScene[0] is rendered. */
	vector<E3D_glScene> glScenes;

        /* TODO: */
        //List of vLights

private:

};


#endif
