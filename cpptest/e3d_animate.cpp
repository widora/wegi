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
	3. Create e3d_animate.cpp, as spin off from e3d_aniamte.h.
2022-10-30:
	1. Add E3D_BMatrixTree::createCraneBMTree()
2022-11-01:
	1. Rename BMT_deleteNode() to BMT_deleteTreeNodes().
	2. Add void BMT_updateTreeGmats()
	3. Add E3D_BMatrixTree::updateTreeGmats()
	4. Add E3D_BMatrixTree::createHandBMTree()
	5. Add E3D_BMatrixTree::resetNodeAmats()
	6. Add 4E3D_BMatrixTree::resetNodeOrients()

Midas Zhou
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#include "e3d_animate.h"

		/*---------------------------------
		       Struct  E3DS_BMTreeNode
		      ( Bone Matrix Tree Node )
		---------------------------------*/

/*------------------------------------------
Create/malloc/init a root node
Return:
	Ponter to the childNode:  OK
	NULL	Fails
------------------------------------------*/
E3DS_BMTreeNode *BMT_createRootNode(void)
{
	E3DS_BMTreeNode *root;

	/* Calloc root */
	root = (E3DS_BMTreeNode*) malloc(sizeof(E3DS_BMTreeNode));
	if(root==NULL) {
		egi_dperr(DBG_RED"malloc fails"DBG_RESET);
		return NULL;
	}

	/* Init bone length */
	root->blen=0.0f;

	/* Init link pointers */
	root->parent=NULL; 	/* Mark as root */
	root->firstChild=NULL;
	root->nextSibling=NULL;
	root->prevSibling=NULL;

	/* Init matrix */
	root->pmat.identity();
	root->amat.identity();

	return root;
}

/*------------------------------------------
Add a child to the node, as the last sibling
of node's Children.

@node:      Pointer to an E3DS_BMTreeNode,
	    under which an childe node will be added.
@boneLen:   Length of the bone.

Return:
	Ponter to the childNode:  OK
	NULL	Fails
------------------------------------------*/
E3DS_BMTreeNode *BMT_addChildNode(E3DS_BMTreeNode* node, float boneLen)
{
	E3DS_BMTreeNode *childNode, *lastChild;

	if(node==NULL)
		return NULL;

	/* Calloc childNode */
	childNode = (E3DS_BMTreeNode*) malloc(sizeof(E3DS_BMTreeNode));
	if(childNode==NULL) {
		egi_dperr(DBG_RED"malloc fails"DBG_RESET);
		return NULL;
	}

	/* Init bone length */
	childNode->blen=boneLen;

	/* Init link pointers */
	childNode->parent=node;
	childNode->firstChild=NULL;
	childNode->nextSibling=NULL; /* Add at last */

	/* Node has no child */
	if(node->firstChild==NULL) {
		node->firstChild=childNode;
		childNode->prevSibling=NULL;
	}
	/* Node has child, get the last child then. */
	else {
		lastChild=node->firstChild;
		while(lastChild->nextSibling)
			lastChild=lastChild->nextSibling;

		/* Assign as the last sibling of node's children */
		lastChild->nextSibling=childNode;
		childNode->prevSibling=lastChild;
	}

	/* Init matrixt */
	childNode->pmat.identity();
	childNode->amat.identity();

	/* OK */
	return childNode;
}


/*------------------------------------------
Delete the node and all its children.
(as a subTree)

	---- CAUTION ----
  This is a recursive function!

@node: A pointer to E3DS_BMTreeNode, all its
       children and itself are to be deleted.

Return:
	!NULL	OK
	NULL	Fails
------------------------------------------*/
void BMT_deleteTreeNodes(E3DS_BMTreeNode *node)
{
	E3DS_BMTreeNode *delChild, *nextChild;

	if(node==NULL) return;

	/* Delete all children nodes */
	delChild=node->firstChild;
	while(delChild) {
		/* Get nextChild */
		nextChild=delChild->nextSibling;

		/* Delete delChild */
		BMT_deleteTreeNodes(delChild);

		/* nextChild as delChild */
		delChild=nextChild;
	}

	/* Modify sibling links to Parent/prevSibling/nextSibling */
	if(node->parent) {
		/* Has no prevSiblings */
		if(node->parent->firstChild==node) {
			node->parent->firstChild=node->nextSibling;

			if(node->nextSibling)
				node->nextSibling->prevSibling=NULL;
		}
		else { /* Has prevSiblings */
			node->prevSibling->nextSibling=node->nextSibling;
			if(node->nextSibling)
				node->nextSibling->prevSibling=node->prevSibling;
		}

		egi_dpstd(DBG_YELLOW"Deleting a node, with bone len=%f!\n"DBG_RESET, node->blen);
	}
	/* This is the ROOT node */
	else {
		egi_dpstd(DBG_YELLOW"Deleting the root node!\n"DBG_RESET);
	}

	/* Delete itself */
	free(node);
}


/*-----------------------------------------------------
Update all gmat (globalizing matrix) of the node and its
child nodes. (as a subTree).

	---- CAUTION ----
  This is a recursive function!

@node: A pointer to E3DS_BMTreeNode
       gmat of itself and its children are to be updated.

Return:
	!NULL	OK
	NULL	Fails
-----------------------------------------------------*/
void BMT_updateTreeGmats(E3DS_BMTreeNode *node)
{
	E3DS_BMTreeNode *doChild, *nextChild;

	if(node==NULL) return;

	/* 1. First, refresh gmat of this node */
	if(node->parent)
        	node->gmat=node->amat*node->pmat*(node->parent->gmat);
        else /* for the root node */
                node->gmat=node->amat*node->pmat;

	/* 2. Then, update gmat for its children nodes */
	doChild=node->firstChild;
	while(doChild) {
		/* 2.1 Get nextChild */
		nextChild=doChild->nextSibling;

		/* 2.2 Update gmat for the doChild */
		BMT_updateTreeGmats(doChild);

		/* 2.3 nextChild as doChild */
		doChild=nextChild;
	}
}


		/*---------------------------------
       			Class  E3D_BMatrixTree
			RTMatrix tree for bones.
		---------------------------------*/


/* Constructor */
E3D_BMatrixTree::E3D_BMatrixTree()
{
	root=BMT_createRootNode();
}

/* Destructor */
E3D_BMatrixTree::~E3D_BMatrixTree()
{
	BMT_deleteTreeNodes(root);
}


/*-------------------------
Add a child to the node.
--------------------------*/
E3DS_BMTreeNode* E3D_BMatrixTree::addChildNode(E3DS_BMTreeNode *node, float boneLen)
{
	return BMT_addChildNode(node, boneLen);
}


/*-------------------------
Add a child to the node.
--------------------------*/
void E3D_BMatrixTree::deleteNode(E3DS_BMTreeNode*node)
{
	BMT_deleteTreeNodes(node);
}


/*---------- TEST ------------------
Create a BoneMatrixTree for a crane.
-----------------------------------*/
void E3D_BMatrixTree::createCraneBMTree()
{
        E3DS_BMTreeNode* node;

        /* 2. Create BoneTree --- a line tree */
        float ang[3]={0.0,0.0,0.0};
	float s=50;
        float blen=7*s;

	/* 0. Root node */

        /* 1. 1st node */
        node=addChildNode(root, blen); /* node, boneLen */

        /* 2. 2nd node */
        node=addChildNode(node, blen);
        ang[0]=MATH_PI*60/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 3. 3rd node */
        node=addChildNode(node, 0.6*blen);
        ang[0]=MATH_PI*90/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* 4. 4th node */
        node=addChildNode(node, 0.5*blen);
        ang[0]=MATH_PI*120/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* 5. update NodePtrList to update Gmat list */
        updateNodePtrList();
}

/*---------- TEST ------------------
Create a BoneMatrixTree for a hand something.
Bone direction Z
-----------------------------------*/
void E3D_BMatrixTree::createHandBMTree()
{
        E3DS_BMTreeNode *basenode; /* Base joined bone */
	E3DS_BMTreeNode *node;

        /* 2. Create BoneTree --- a line tree */
        float ang[3]={0.0,0.0,0.0};
	float s=50; /* side size */
        float blen=8*s;

	/* 0. Root node */

        /* Joint bone */
        basenode=addChildNode(root, 0.5*blen); /* node, boneLen */

/* Middle finger */
        /* M1. */
        node=addChildNode(basenode, blen);
        ang[0]=MATH_PI*5/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* M2. */
        node=addChildNode(node, 0.5*blen);
        ang[0]=MATH_PI*0/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* M3. */
        node=addChildNode(node, 0.3*blen);
        ang[0]=MATH_PI*-10/180;
        node->pmat.combIntriRotation("X",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* M4. */
        node=addChildNode(node, 0.2*blen);
        ang[0]=MATH_PI*0/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

/* The Index finger */
        /* I1. */
        node=addChildNode(basenode, blen);
        ang[0]=MATH_PI*20/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* I2. */
        node=addChildNode(node, 0.5*blen);
        ang[0]=MATH_PI*0/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* I3. */
        node=addChildNode(node, 0.3*blen);
        ang[0]=MATH_PI*-10/180;
        node->pmat.combIntriRotation("X",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* I4. */
        node=addChildNode(node, 0.2*blen);
        ang[0]=MATH_PI*0/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

/* The Ring finger */
        /* R1. */
        node=addChildNode(basenode, 0.9*blen);
        ang[0]=MATH_PI*-10/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* R2. */
        node=addChildNode(node, 0.5*blen);
        ang[0]=MATH_PI*0/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* R3. */
        node=addChildNode(node, 0.3*blen);
        ang[0]=MATH_PI*-10/180;
        node->pmat.combIntriRotation("X",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* R4. */
        node=addChildNode(node, 0.2*blen);
        ang[0]=MATH_PI*0/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

/* The Thumb finger */
        /* T1. */
        node=addChildNode(basenode, 0.7*blen);
        ang[0]=MATH_PI*70/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* T2. */
        node=addChildNode(node, 0.3*blen);
        ang[0]=MATH_PI*0/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* T3. */
        node=addChildNode(node, 0.2*blen);
        ang[0]=MATH_PI*0/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

/* The Little finger */
        /* L1. */
        node=addChildNode(basenode, 0.8*blen);
        ang[0]=MATH_PI*-20/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* L2. */
        node=addChildNode(node, 0.45*blen);
        ang[0]=MATH_PI*0/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* L3. */
        node=addChildNode(node, 0.2*blen);
        ang[0]=MATH_PI*-10/180;
        node->pmat.combIntriRotation("X",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* L4. */
        node=addChildNode(node, 0.2*blen);
        ang[0]=MATH_PI*0/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

        /* 5. update NodePtrList to update Gmat list */
        updateNodePtrList();
}


/*-------------------------------------------------------
Get/List all node pointers of a (sub)tree in LevelOrder.

	  !!! --- CAUTION --- !!!
	NODE.gmat NOT updated here!
-------------------------------------------------------*/
vector<E3DS_BMTreeNode*> E3D_BMatrixTree::getTreeNodePtrList(const E3DS_BMTreeNode *root) const
{
	vector<E3DS_BMTreeNode*> list;
	queue<E3DS_BMTreeNode*>  qu;
	E3DS_BMTreeNode *node, *child;

	if(root==NULL)
		return list;

	qu.push((E3DS_BMTreeNode *)root);
	while(!qu.empty()) {
		/* Traverse all level memeber */
		for(size_t k=0; k<qu.size(); k++) {
			node=qu.front(); /* Get first memeber int he queue */
			list.push_back(node); /* Push all level nodes to list */

			/* Add all child nodes (at the next level) to qu */
			child=node->firstChild;
			while(child) {
				qu.push(child); /* Push to queue end */
				child=child->nextSibling;
			}

			/* Discard the front node of the qu, prepare for next node */
			qu.pop();
		}

		/* NOW qu: all node of this level are discarded and replaced by nodes of next level. */
		/* NOW list: all nodes of this level are pushed back into the list. */
	}

	return list;
}

/*------------------------------------------
Update nodePtrList(LevelOrder) + nodeGmatList
-------------------------------------------*/
void E3D_BMatrixTree::updateNodePtrList()
{
	queue<E3DS_BMTreeNode*>  qu;
	E3DS_BMTreeNode *node, *child;
	E3D_RTMatrix gmat;

	/* Impossible */
	if(root==NULL) return;

	/* Clear nodePtrList */
	nodePtrList.clear();

	qu.push(root);
	while(!qu.empty()) {
		/* Traverse all level memeber */
		for(size_t k=0; k<qu.size(); k++) {
			node=qu.front(); /* Get first memeber int he queue */

			/* Update node.gmat */
			if(node->parent)
			     node->gmat=node->amat*node->pmat*(node->parent->gmat);
			else /* for the root node */
			     node->gmat=node->amat*node->pmat;

			nodePtrList.push_back(node);  /* <---------- Push all level nodes ptr to list */

			/* Add all child nodes (at the next level) to qu */
			child=node->firstChild;
			while(child) {
				qu.push(child); /* Push to queue end */
				child=child->nextSibling;
			}

			/* Discard the front node of the qu, prepare for next node */
			qu.pop();
		}

		/* NOW qu: all node of this level are discarded and replaced by nodes of next level. */
		/* NOW list: all nodes of this level are pushed back into the list. */
	}
}


/*------------------------------------------
Update all gmats.
All refere to BMT_updateTreeGmats(node).

Note: If ONLY update gamts of a subtree,
then call BMT_updateTreeGmats(node).
-------------------------------------------*/
void E3D_BMatrixTree::updateTreeGmats()
{
	BMT_updateTreeGmats(root);
}


/*------------------------------------------
Reset all node amats to identity RTMatrix.
-------------------------------------------*/
void E3D_BMatrixTree::resetNodeAmats()
{
	for(size_t k=0; k<nodePtrList.size(); k++)
		nodePtrList[k]->amat.identity();
}

/*------------------------------------------
Reset all amats as identity and update gmats
accordinly.
-------------------------------------------*/
void E3D_BMatrixTree::resetNodeOrients()
{
	resetNodeAmats();
	updateTreeGmats();
}
