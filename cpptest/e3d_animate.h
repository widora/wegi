/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

BVH: 

Journal:
2022-10-26:
	1. Create the file
	2. Add E3DS_BMTreeNode and functions:
		BMT_createRootNode(), BMT_addChildNode(), BMT_deleteNode()
	3. Add class E3D_BMatrixTree and the constructor.
2022-10-27:
	1. Add E3D_BMatrixTree::addChildNode(), E3D_BMatrixTree::deleteNode().
	2. Add E3D_BMatrixTree::updateNodePtrList()

Midas Zhou
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#ifndef __E3D_ANIMATE_H__
#define __E3D_ANIMATE_H__

#include <queue>
#include <vector>
#include <string>

#include "e3d_trimesh.h"


/*---------------------------------
   Class E3D_AnimQuatrices
   ( Animating quatrices )
---------------------------------*/
class E3D_AnimQuatrices {
public:
	/* Time point and corresponding Quatrix */
	vector<float>		ts;	/* time points, [0 1], sort in Ascending Order */
	vector<E3D_Quatrix>  	qts;    /* Quatrix at the time point */

        /* Constructor */
        //E3D_AnimQuatrices ();

        /* Destructor */
        //~E3D_AnimQuatrices ();

	/* Function: print */
	void print(const char *name=NULL) const;

	/* Function: insert and save quatrix at time point t */
	void insert(float t, const E3D_Quatrix &qt);

	/* Function: interpolate at time point t and return result to qt */
	void interp(float t, E3D_Quatrix &qt) const;
};


/*---------------------------------------------------
   Struct  E3DS_BMTreeNode
   ( Bone Matrix Tree Node )

A bone(tree) growing direction defined as Z direction
of the local COORD. --- BUT, NOT Necessary!


Note:
1. In c++ struct is nearly same as class
   Use 'new' NOT 'malloc/alloc' to allocate
   a struct.
2. A ROOT node bone is ACTUALLY a (ORIGIN) point!
   and its blen is ALWAYS 0!

----------------------------------------------------*/
typedef struct _BoneMatrix_TreeNode E3DS_BMTreeNode;
struct _BoneMatrix_TreeNode {
	/* Node tree pointers */
	E3DS_BMTreeNode* parent;  	/* =NULL: mark as the root node */
	E3DS_BMTreeNode* firstChild;
	E3DS_BMTreeNode* prevSibling;
	E3DS_BMTreeNode* nextSibling;

	/* Node data */
	float blen;		/* Bone Length.
				 * For the root: it's 0.0f
				 * Others if blen=0.0f, then it's NOT allowed to attached any points??
				 */
	E3D_Vector bv;		/* This is for BMTREE_TYPE_VBH only , indicates the Bone Vector(or growing direction of the bone.)
				 * init as 0,0,0
				 */


	E3D_RTMatrix pmat;	/* Presetting Transform matrix, for the object's initial orientation.
				 * 1. For node COORD Frame: transform from its parent COORD to this COORD
							!!! --- THIS IS IMPORTANT --- !!!
				      		        Conception for creating a new bone
				   For MBTREE_TYPE_E3D:
				      Bone direction MUST align with local COORD_Z axis, and local COORDs NOT necessary to
				      parallel with Global COORD.
				      A new bone is first created under its parent COORD, starting from parent COORD_Origin, with
   				      given BoneLength along parent COORD_Z, then it is transfered by pmat to its own position/orientation.

				   For MBTREE_TYPE_BVH:
				      Initial local COORDs MUST all parallel with Global COORD, bone direction NOT necessary to
				      align with any axis!
				      A new bone is first created under its parent COORD, starting from parent COORD_Origin, with
				      give BoneLength along direction(vector) bv, then it is transfered(offsetXYZ usually) by pmat to its
				      own position/orientation.

				 * 2. For attached points: transform coordinates under this COORD to under its parent's COORD
				 * 3. For the root: transfrom from the Global/Eye COORD to this root COORD.
		 					!!! CAVEAT !!!
				 * 4. NORMALLy, the translation of pmat is along its parent's bone(blen) legnth direction
				 */

	E3D_RTMatrix amat;	/* Acting transform matrix, to apply on (relative to)pmat, stands for object's animation.
				 *  0. node->gmat=node->amat*node->pmat*(node->parent->gmat)
				 *  1. Usually amat will be input or interpolated from anitQts for each motion frame.
				 * 		--- CAUTION ---
				 *  2. amat will be replaced/updated after calling updateTreeOrients()!
				 */
	//E3D_RTMatrix adjustmat; /* User adjusting matrix, to apply on amat */

	E3D_RTMatrix gmat;	/* Globalizing transform matrix, to transform coordinates under this node COORD to under Global COORD
				 *		--- CAUTION ---
				 *  Usually gmat is correct ONLY after immediate whole tree computation.
				 *  E3D_BMatrixTree::updateTreeGmats() OR updateNodePtrList()
				 */

	E3D_AnimQuatrices  animQts; /* Animating Quatrices for KeyFrames, interpolate to assign to amat!
				     * TBD&TODO: check and normalize quaternion before converting to/from matrix. ???
				     */
};
E3DS_BMTreeNode *BMT_createRootNode(void);
E3DS_BMTreeNode *BMT_addChildNode(E3DS_BMTreeNode* node, float boneLen);
//NO addSiblingNode(), as there MUST be a ROOT NODE!
void BMT_deleteTreeNodes(E3DS_BMTreeNode *node); /* Delet the node and all its children */
void BMT_updateTreeGmats(E3DS_BMTreeNode *node); /* Update all gmat for the node and its children */
void BMT_updateTreeAmats(E3DS_BMTreeNode *node); /* Update all amat for the node and its children */


/*---------------------------------
       Class  E3D_BMatrixTree
RTMatrix tree for bones.
---------------------------------*/
#define BMTREE_TYPE_E3D		0	/* Default */
#define BMTREE_TYPE_VBH		1
#define BMTREE_TYPE_NAMCO	2
class E3D_BMatrixTree {
public:
        /* Constructor */
        E3D_BMatrixTree ();
	E3D_BMatrixTree(const char *fbvh); /* To create BMatrixTree by reading BVH data */
	E3D_BMatrixTree(const char *fbvh, int namco);  /* To create BMatrixTree by reading Bandai_Namco BVH data */

        /* Destructor */
        ~E3D_BMatrixTree ();

	/* Function: add a child to the node. */
	E3DS_BMTreeNode *addChildNode(E3DS_BMTreeNode *node, float boneLen=0.0f);

	/* Function: Delete the node and all its children */
	void deleteNode(E3DS_BMTreeNode* node);

////////////////* TEST Function: Create a BoneMatrixTree for an object *//////////
	void createCraneBMTree();
	void createHandBMTree();
	void createBoxmanBMTree();
/////////////////////////////////////////////////////////////////////////////////


	/* Function: Get LevelOrder list of all nodes(pointers) to root/subroot, including root node. */
	vector<E3DS_BMTreeNode*> getTreeNodePtrList(const E3DS_BMTreeNode *root) const;

	/* Function: update gmats for the node and its children (or as subTree) */
	/* compute transformation matrix which will transfer ponit coordinates under this node COORD to under Global/Eye COORD */
	void updateTreeGmats(); /* If only need to update gmats of a subtree, call BMT_updateTreeGmats(node) */

	/* Function: Update nodePtrList(LeverOrder)+nodeGmatList */
	void updateNodePtrList();

	/* Function: Reset all amat to identity */
	void resetTreeAmats();

	/* Function: Insert node aminating quatrix */
	void insertNodeAminQt(E3DS_BMTreeNode *node, float t, const E3D_Quatrix &qt);

	/* Interpolate to update all amats with with given time point t[0 1] */
	void interpTreeAmats(float t);

	/* Function: resetNodeAmats()+updateTreeGmats() */
	void resetTreeOrients();

	/* Function: interpNodeAmats()+updateTreeGmats() */
	void interpTreeOrients(float t);

public:
	int			  type;  /* BMTree Type:
					    BMTREE_TYPE_E3D(=0 default): Bone local COORD_Z always aligns with bone growing direction.
					    BMTREE_TYPE_VBH: Bone local COORD_XYZ aligns with golbal COORD_XYZ initially!
					    BMTREE_TYPE_NAMCO: BadaiNamco format BVH.
					  */

	E3DS_BMTreeNode		  *root;  	/* MUST NOT be NULL! */

	/* --- LevelOrder Data --- */
	vector<E3DS_BMTreeNode*>  nodePtrList; 	/* Node pointer list, sort in Level_Order. */


};

#endif

