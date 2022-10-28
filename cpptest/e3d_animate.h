/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


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

#include "e3d_trimesh.h"

/*---------------------------------
   Struct  E3DS_BMTreeNode
   ( Bone Matrix Tree Node )
---------------------------------*/
typedef struct TreeNode E3DS_BMTreeNode;
struct TreeNode {
	E3DS_BMTreeNode* parent;  	/* =NULL: mark as the root node */
	E3DS_BMTreeNode* firstChild;
	E3DS_BMTreeNode* prevSibling;
	E3DS_BMTreeNode* nextSibling;

	float blen;		/* Bone Length.
				 * For the root: it's 0.0f
				 * Others if blen=0.0f, then it's NOT allowed to attached any points??
				 */
	E3D_RTMatrix pmat;	/* Presetting Transform matrix, for the object's initial orientation.
				 * 1. For COORD Frame: transform from its parent COORD to this COORD
				 * 2. For attached points: transfrom coordinates under this COORD to under its parent's COORD
				 * 3. For the root: transfrom from the Global/Eye COORD to this root COORD.
				 * 4. NORMALLy, the translation of pmat is along its parent's bone(blen)  !!! CAVEAT !!!
				 */

	E3D_RTMatrix amat;	/* Acting transform matrix, To apply on pmat, stands for object's animation. */

	E3D_RTMatrix gmat;	/* gmat, to transform coordinates under this node COORD to under Global COORD
				 *		--- CAUTION ---
				 *  Usually gmat is correct ONLY after immediate whole tree computation.
				 *  E3D_BMatrixTree::updateNodePtrList() OR ...
				 */

};
E3DS_BMTreeNode *BMT_createRootNode(void);
E3DS_BMTreeNode *BMT_addChildNode(E3DS_BMTreeNode* node, float boneLen);
//NO addSiblingNode(), as there MUST be a ROOT NODE!
void BMT_deleteNode(E3DS_BMTreeNode*node);


/*---------------------------------
       Class  E3D_BMatrixTree
RTMatrix tree for bones.
---------------------------------*/
class E3D_BMatrixTree {
public:
        /* Constructor */
        E3D_BMatrixTree ();

        /* Destructor */
        ~E3D_BMatrixTree ();

	/* Function: add a child to the node. */
	E3DS_BMTreeNode *addChildNode(E3DS_BMTreeNode *node, float boneLen=0.0f);

	/* Function: Delete the node and all its children */
	void deleteNode(E3DS_BMTreeNode*node);

	/* Function: Get LevelOrder list of all nodes(pointers) to root/subroot, including root node. */
	vector<E3DS_BMTreeNode*> getTreeNodePtrList(const E3DS_BMTreeNode *root) const;

	/* Function: compute transformation matrix which will transfer ponit coordinates under this node COORD to under Global/Eye COORD */
	E3D_RTMatrix computeGmat(E3DS_BMTreeNode *node); /* Gmat --- Transform from Node COORD to under Global COORD */

	/* Fucntion: Update nodePtrList+nodeGmatList */
	void updateNodePtrList();

public:
	E3DS_BMTreeNode		  *root;  	/* MUST NOT be NULL! */

	/* --- LevelOrder Data --- */
	vector<E3DS_BMTreeNode*>  nodePtrList; 	/* Node pointer list, same sequence as nodeGmatLsit */
//	vector<E3D_RTMatrix>	  nodeGmatList; /* XXX see node.gmat  XXX Node Gmat list, same sequence as nodePtrList */
};

#endif
