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

	---- CAUTION ----
  This is a recursive function!

Return:
	!NULL	OK
	NULL	Fails
------------------------------------------*/
void BMT_deleteNode(E3DS_BMTreeNode *node)
{
	E3DS_BMTreeNode *delChild, *nextChild;

	if(node==NULL) return;

	/* Delete all children nodes */
	delChild=node->firstChild;
	while(delChild) {
		/* Get nextChild */
		nextChild=delChild->nextSibling;

		/* Delete delChild */
		BMT_deleteNode(delChild);

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
	BMT_deleteNode(root);
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
	BMT_deleteNode(node);
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
Update nodePtrList + nodeGmatList
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


