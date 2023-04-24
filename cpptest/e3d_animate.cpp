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
2022-11-03:
	1. Add E3D_AnimQuatrices and functions:
		printf(), insert(), interp()
	2. Rename: resetNodeAmats() ---> resetTreeAmats()   (Tree=AllNode)
	   Rename: resetNodeOrients() ---> resetTreeOrients()
2022-11-04:
	1. Add E3D_BMatrixTree::insertNodeAminQt(), interpTreeAmats(t), interpTreeOrients(t)
2022-11-06:
	1. Add E3D_BMatrixTree::createBoxmanBMTree()
2022-11-07:
	1. E3D_BMatrixTree::createBoxmanBMTree(): Test simple motion.
2022-11-08:
        1. E3D_BMatrixTree::createBoxmanBMTree(): Test motion squating.
2022-11-10:
	1. Add E3D_BMatrixTree::E3D_BMatrixTree(const char *fbvh)
2022-11-14:
	1. E3D_BMatrixTree::E3D_BMatrixTree(const char *fbvh): Add case that CHANNELS maybe 3 or 6.
2022-11-15:
	1.  Add E3D_BMatrixTree::E3D_BMatrixTree(const char *fbvh, int namco) for Bandai_Namco BVH format.
2022-11-21:
	1. E3D_BMatrixTree::E3D_BMatrixTree(const char *fbvh): Test for Baidan_Namco BVH.
2022-11-23:
	1. E3D_BMatrixTree::E3D_BMatrixTree(const char *fbvh): Get rotAxisToks[] for each bone node.
2022-11-24:
	1. E3D_BMatrixTree::E3D_BMatrixTree(const char *fbvh): Test for SecondLife(avatar_) BVH.
2022-11-29/20:
	1. E3D_BMatrixTree::E3D_BMatrixTree(const char *fbvh): Test for Adobe Mixamo BVH. (variable CHANNELS(6/3))
2023-02-23:
	1. Add E3D_AnimQuatrices::insertSRT().
2023-02-28:
	1. Add E3D_AnimMorphWeights::insert() and E3D_AnimMorphWeights::interp()
2023-04-06:
	1. Add E3D_AnimQuatrices::tsmin/tsmax and E3D_AnimQuatrices::E3D_AnimQuatrices()

Midas Zhou
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#include "e3d_animate.h"
#include <vector>
#include <sstream> //istringstrem
#include <iostream> //ifstream
#include <fstream>

using namespace std;

		/*--------------------------------
  		       Class E3D_AnimMorphwts
   		    ( Animating morph weights )
		---------------------------------*/
/*----------------------
	Print
-----------------------*/
void E3D_AnimMorphWeights::print(const char *name) const
{
	if(name)
		printf("  --- %s ---\n",name);

	for(size_t k=0; k<ts.size(); k++) {
		printf("Seq%zu: t=%f \n  %d weights: [ ", k, ts[k], mmsize);
		for(size_t kk=0; kk<mwts[k].size(); kk++) {
			printf("%f, ", mwts[k][kk]);
		} printf("]\n");
	}
}

/*----------------------------------------
Insert/save morph weights at time point t.

	 !!!--- CAUTION ---!!!
If frame t already exists(t==ts[k]), then
REPLACE/UPDATE its mwt[] value.

@t:         Time point, [0 1] OR Seconds
@weights:   morph weights for targets
	    It size MUST be same as target numbers.
------------------------------------------------*/
void E3D_AnimMorphWeights::insert(float t, const vector<float> weights)
{
	bool ImBig=false;

	/* Check size */
	if(weights.size()<1)
		return;

#if 0	/* TODO: Limit??? necessary??? GLTF t --seconds! */
	if( t<0.0f || t>1.0 )
		return;
#endif

	/* 1. No element in vecotr */
	if(ts.empty() ) {
		ts.push_back(t);
		mwts.push_back(weights);
		mmsize=weights.size();   /* First assign */
		return;
	}

	/* 1a. Check size */
	if(weights.size()!=mmsize) {
		egi_dpstd(DBG_RED"weights.size:%zu != mmsize:%zu\n", weights.size(), mmsize);
		return;
	}

	/* 2. Insert between two mwt in ascending order of ts */
	for(size_t k=0; k<ts.size(); k++) {
		if(ImBig) {
			/* Insert between two qts, before k */
			if(t<ts[k]) {
				ts.insert(ts.begin()+k,t);
				mwts.insert(mwts.begin()+k, weights);
				return; /* ---------> */
			}
			else if(t>ts[k])
				continue;
			else { /* t==ts[k], replace it! */
				// ts[k]=t; NOT necessary HK2023-02-23
				mwts[k]=weights;
				return; /* ---------> */
			}
		}
		else {  /* ImSmall, ---- this should happen ONLY once as we set ImBig=false at beginning */
			/* Insert before k=0 */
			if(t<ts[k]) { /* here k==0, as we assume ImBig=false at beginning */
				ts.insert(ts.begin()+k,t);
				mwts.insert(mwts.begin()+k, weights);
				return;  /* --------> */
			}
			else if(t>ts[k]) {
				ImBig=true;
				continue;
			}
			else { /* t==ts[k], replace it! here k==0 */
				// ts[k]=t; NOT necessary HK2023-02-23
				mwts[k]=weights;

				return;  /* --------> */
			}
		}
	}

	/* 3. NOW, it gets to the end. */
	ts.push_back(t);
	mwts.push_back(weights);

}


/*----------------------------------------------
Interpolate by time point t to get corresponding
morph weights.

1. "Before and after the provided input range, output MUST be clamped to the nearest end of the input range". --glTF Spek2.0

Note:
1. If ts.size()==0, return 0s vector.
2. If t<ts[0], then weights=mwts[0].
   If t>ts[last], then qt=mwts[last]

@t:    [0 1] OR seconds, Time point for the quatrix.
@qt:   Returned morph weights.
-----------------------------------------------*/
void E3D_AnimMorphWeights::interp(float t, vector<float> &weights) const
{
	bool ImBig=false;
	size_t k;
	float rt;

#if 0	/* TODO: Limit??? necessary??? */
	if( t<0.0f || t>1.0 )
		return;
#endif

	/* Check mwts */
	if(mmsize<1 || mwts.size()<1) {
		egi_dpstd(DBG_RED"mmsize<1 OR mwts.size<1\n"DBG_RESET);
		return;
	}

	/* Check wegiths.size() */
	if(weights.empty() || weights.size()!=mmsize )
		weights.resize(mmsize);

	/* 1. No element in vecotr, return 0 vector. */
	if(ts.empty()) {
		egi_dpstd("No element in ts[]/animQts[]!\n");
		weights.assign(weights.size(), 0.0f);
		return;
	}
//	egi_dpstd("ts.size()=%zu\n", ts.size());

	/* 2. Interpolate between two mwts in ascending order of ts */
	for(k=0; k<ts.size(); k++) {
		if(ImBig) {
			/* Interpolate between two mwts, before k */
			if(t<ts[k]) {
				rt=(t-ts[k-1])/(ts[k]-ts[k-1]);
				for(size_t j=0; j<mmsize; j++)
					weights[j]=mwts[k-1][j]+rt*(mwts[k][j]-mwts[k-1][j]);

				return; /* ---------> */
			}
			else if(t>ts[k])
				continue;
			else { /* t==ts[k] */
				rt=(t-ts[k-1])/(ts[k]-ts[k-1]);
				for(size_t j=0; j<mmsize; j++)
					weights[j]=mwts[k-1][j]+rt*(mwts[k][j]-mwts[k-1][j]);

				return; /* ---------> */
			}
		}
		else {  /* ImSmall, ---- this should happen ONLY once as we set ImBig=false at beginning */
			/* Interpolate at before k=0 */
			if(t<ts[k]) { /* here k==0, as we assume ImBig=false at beginning */
				weights=mwts[k]; /* Same as the first mwts[] */
				return;  /* --------> */
			}
			else if(t>ts[k]) {
				ImBig=true;
				continue;
			}
			else { /* t==ts[k], assign it. */
				weights=mwts[k]; //weights.assign(mwts[k].begin(), mwts[k].end());
				return;  /* --------> */
			}
		}
	}

	/* 3. NOW, it gets to the end, same as the last qts[]. */
	weights=mwts[k-1];
}

		/*---------------------------------
  		       Class E3D_AnimQuatrices
   		       ( Animating quatrices )
		---------------------------------*/

/*----------------------
     Constructor
-----------------------*/
E3D_AnimQuatrices::E3D_AnimQuatrices()
{
	tsmax=tsmin=0.0f;
}

/*----------------------
	Print
-----------------------*/
void E3D_AnimQuatrices::print(const char *name) const
{
	if(name)
		printf("  --- %s ---\n",name);

	for(size_t k=0; k<ts.size(); k++) {
		printf("Seq%zu: t=%f, quat[%f (%f, %f, %f)], txyz(%f,%f,%f), sxyz(%f,%f,%f).\n",
			k, ts[k],

			qts[k].q.w,  qts[k].q.x, qts[k].q.y, qts[k].q.z,
			qts[k].tx, qts[k].ty, qts[k].tz,
			qts[k].sx, qts[k].sy, qts[k].sz  /* HK2023-02-23 */
		);
	}
}

/*----------------------------------------
Insert/save quatrix at time point t

	 !!!--- CAUTION ---!!!
If frame t already exists(t==ts[k]), then
REPLACE/UPDATE its qts[] value.


@t:    [0 1] Time point for the quatrix. ---NOT necessary!
@qt:   The quatrix to be inserted.
-----------------------------------------*/
void E3D_AnimQuatrices::insert(float t, const E3D_Quatrix &qt)
{
	bool ImBig=false;

#if 0	/* TODO: Limit??? necessary??? GLTF t --seconds! */
	if( t<0.0f || t>1.0 )
		return;
#endif

	/* 1. No element in vecotr */
	if(ts.empty()) {
		ts.push_back(t);
		qts.push_back(qt);
		return;
	}

	/* 2. Insert between two qts in ascending order of ts */
	for(size_t k=0; k<ts.size(); k++) {
		if(ImBig) {
			/* Insert between two qts, before k */
			if(t<ts[k]) {
				ts.insert(ts.begin()+k,t);
				qts.insert(qts.begin()+k, qt);
				return; /* ---------> */
			}
			else if(t>ts[k])
				continue;
			else { /* t==ts[k], replace it! */
				// ts[k]=t; NOT necessary HK2023-02-23
				qts[k]=qt;
				return; /* ---------> */
			}
		}
		else {  /* ImSmall, ---- this should happen ONLY once as we set ImBig=false at beginning */
			/* Insert before k=0 */
			if(t<ts[k]) { /* here k==0, as we assume ImBig=false at beginning */
				ts.insert(ts.begin()+k,t);
				qts.insert(qts.begin()+k, qt);
				return;  /* --------> */
			}
			else if(t>ts[k]) {
				ImBig=true;
				continue;
			}
			else { /* t==ts[k], replace it! here k==0 */
				// ts[k]=t; NOT necessary HK2023-02-23
				qts[k]=qt;
				return;  /* --------> */
			}
		}
	}

	/* 3. NOW, it gets to the end. */
	ts.push_back(t);
	qts.push_back(qt);

}

/*----------------------------------------
Insert/save Scale/Rotation/Translation
at time point t.

         !!!--- CAUTION ---!!!
If frame t already exists (t==ts[k]), then
REPLACE/UPDATE part(s) of its qts[] (Scale
,Rotation,Translation) accordingly.


@t:     Time point for the quatrix. ---NOT necessary to be [0 1]
@toks:  A string combination of 'S','R','T', stands for
        inserting Scale/Rotation/Translation respectively.
@sxyz:  Scale X,Y,Z
@q:     Rotation in quarternion. (x,y,z,w)
@txyz:  Translation X,Y,Z
-----------------------------------------*/
void E3D_AnimQuatrices::insertSRT(float t, const char *toks,
				   const E3D_Vector &sxyz, const E3D_Quaternion &q, const E3D_Vector &txyz)
{
	bool ImBig=false;
	bool S_on=false, R_on=false, T_on=false;
	E3D_Quatrix qt;
	int k;

#if 0	/* TODO: Limit??? necessary??? GLTF t --seconds! */
	if( t<0.0f || t>1.0 )
		return;
#endif

	/***               !!!--- CAUTION ---!!!
	 * Operator '=' NOT reloaded for E3D_Quaternion and E3D_Quatrix yet!
	 */

	/* 0. Parse toks */
	if(toks==NULL) return;
	k=0;
	while(toks[k]) {
		if(toks[k]=='S')
			S_on=true;
		else if(toks[k]=='R')
			R_on=true;
		else if(toks[k]=='T')
                        T_on=true;

		/* Max 3 toks */
		k++;
		if(k==3) break;
	}
	/* No effect */
	if(!S_on && !R_on && !T_on)
		return;

	/* 1. No element in vecotr */
	if(ts.empty()) {

		/* Set qt accordingly */
		if(S_on) {
		      qt.sx=sxyz.x; qt.sy=sxyz.y; qt.sz=sxyz.z;
		}
		if(R_on) {
		      qt.q=q;
		}
		if(T_on) {
		      qt.tx=txyz.x; qt.ty=txyz.y; qt.tz=txyz.z;
		}

		/* Push back to qts */
		ts.push_back(t);
		qts.push_back(qt);
		return;
	}

	/* 2. Insert between two qts in ascending order of ts */
	for(size_t k=0; k<ts.size(); k++) {
		/* 2.1 */
		if(ImBig) {
			/* 2.1.1 Insert between two qts, before k */
			if(t<ts[k]) {
				/* 2.1.1.1 Set qt accordingly */
				if(S_on) {
		      			qt.sx=sxyz.x; qt.sy=sxyz.y; qt.sz=sxyz.z;
				}
				if(R_on) {
		      			qt.q=q;
				}
				if(T_on) {
		      			qt.tx=txyz.x; qt.ty=txyz.y; qt.tz=txyz.z;
				}

				/* 2.1.1.2 Insert qt at t */
				ts.insert(ts.begin()+k,t);
				qts.insert(qts.begin()+k, qt);
				return; /* ---------> */
			}
			/* 2.1.2 */
			else if(t>ts[k])
				continue;
			/* 2.1.3 */
			else { /* t==ts[k], replace/update part(s) of it! */
				/* 2.1.3.1 Copy first */
				qt=qts[k];

				/* 2.1.3.2 Modify part(s) of qt accordingly */
				if(S_on) {
		      			qt.sx=sxyz.x; qt.sy=sxyz.y; qt.sz=sxyz.z;
				}
				if(R_on) {
		      			qt.q=q;
				}
				if(T_on) {
		      			qt.tx=txyz.x; qt.ty=txyz.y; qt.tz=txyz.z;
				}

				/* 2.1.3.3 Update qt */
				//ts[k]=t; NOT necessary HK2023-02-23
				qts[k]=qt;
				return; /* ---------> */
			}
		}
		/* 2.2 */
		else {  /* ImSmall, ---- this should happen ONLY once as we set ImBig=false at beginning */
			/* 2.2.1 Insert before k=0 */
			if(t<ts[k]) { /* here k==0, as we assume ImBig=false at beginning */
				/* 2.2.1.1 Set qt accordingly */
				if(S_on) {
		      			qt.sx=sxyz.x; qt.sy=sxyz.y; qt.sz=sxyz.z;
				}
				if(R_on) {
		      			qt.q=q;
				}
				if(T_on) {
		      			qt.tx=txyz.x; qt.ty=txyz.y; qt.tz=txyz.z;
				}

				/* 2.2.1.2 Insert qt at t */
				ts.insert(ts.begin()+k,t);
				qts.insert(qts.begin()+k, qt);
				return;  /* --------> */
			}
			/* 2.2.2 */
			else if(t>ts[k]) {
				ImBig=true;
				continue;
			}
			/* 2.2.3 */
			else { /* t==ts[k], replace/update part(s) of it! here k==0 */
				/* 2.2.3.1 Copy first */
				qt=qts[k];

				/* 2.2.3.2 Modify part(s) of qt accordingly */
				if(S_on) {
		      			qt.sx=sxyz.x; qt.sy=sxyz.y; qt.sz=sxyz.z;
				}
				if(R_on) {
		      			qt.q=q;
				}
				if(T_on) {
		      			qt.tx=txyz.x; qt.ty=txyz.y; qt.tz=txyz.z;
				}

				/* 2.2.3.3 Update qt */
				// ts[k]=t; NOT necessary HK2023-02-23
				qts[k]=qt;
				return;  /* --------> */
			}
		}
	}

	/* 3. NOW, it gets to the end. */
	/* 3.1 Set qt accordingly */
	if(S_on) {
	      qt.sx=sxyz.x; qt.sy=sxyz.y; qt.sz=sxyz.z;
	}
	if(R_on) {
	      qt.q=q;
	}
	if(T_on) {
	      qt.tx=txyz.x; qt.ty=txyz.y; qt.tz=txyz.z;
	}
	/* 3.2 Push back at end of qts[] */
	ts.push_back(t);
	qts.push_back(qt);
}


/*----------------------------------------------
Interpolate by time point t to get corresponding
Quatrix.

Note:
1. "Before and after the provided input range, output MUST be clamped to the nearest end of the input range". --glTF Spek2.0

Note:
1. If ts.size()==0, return identity.
2. If t<ts[0], then qt=qts[0].
   If t>ts[last], then qt=qts[last]

@t:    [0 1] Time point for the quatrix.
@qt:   Returned quatrix
-----------------------------------------------*/
void E3D_AnimQuatrices::interp(float t, E3D_Quatrix &qt) const
{
	bool ImBig=false;
	size_t k;
	float rt;

#if 0	/* OK, t maybe<0; XXXTODO: Limit??? necessary??? */
	if( t<0.0f || t>1.0 )
		return;
#endif

//	egi_dpstd("....in...\n");

	/* 1. No element in vecotr, return identity. */
	if(ts.empty()) {  /* Hello! Haaa ... HK2022-11-04 */
		egi_dpstd("No element in ts[]/animQts[]!\n");
		//qt.q.identity();
		qt.identity(); /* HK2023-03-26 */
		return;
	}
//	egi_dpstd("ts.size()=%zu\n", ts.size());

	/* 2. Interpolate between two qts in ascending order of ts */
	for(k=0; k<ts.size(); k++) {
		if(ImBig) {
			/* Interpolate between two qts, before k */
			if(t<ts[k]) {
				rt=(t-ts[k-1])/(ts[k]-ts[k-1]);
				qt.interp(qts[k-1],qts[k], rt);
				return; /* ---------> */
			}
			else if(t>ts[k])
				continue;
			else { /* t==ts[k] */
				qt=qts[k];
				return; /* ---------> */
			}
		}
		else {  /* ImSmall, ---- this should happen ONLY once as we set ImBig=false at beginning */
			/* Interpolate at before k=0 */
			if(t<ts[k]) { /* here k==0, as we assume ImBig=false at beginning */
			     #if 1  /* Clamped */
				qt=qts[k]; /* Same as the first qts[] ... TODO: OR identity??? */
			     #else
				qt=_identityQt;
			     #endif
				return;  /* --------> */
			}
			else if(t>ts[k]) {
				ImBig=true;
				continue;
			}
			else { /* t==ts[k], here k==0 */
				qt=qts[k];
				return;  /* --------> */
			}
		}
	}

	/* 3. NOW, it gets to the end, same as the last qts[]. */
	qt=qts[k-1];

}




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
//	root = (E3DS_BMTreeNode*)calloc(1,sizeof(E3DS_BMTreeNode));
//	if(root==NULL) {
//		egi_dperr(DBG_RED"malloc fails"DBG_RESET);
//		return NULL;
//	}

	/* Add new childNode */
        try {
		root = new E3DS_BMTreeNode;
        }
        catch ( std::bad_alloc ) {
                egi_dpstd("Fail to allocate root node!\n");
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
//	childNode = (E3DS_BMTreeNode*) calloc(1,sizeof(E3DS_BMTreeNode));
//	if(childNode==NULL) {
//		egi_dperr(DBG_RED"malloc fails"DBG_RESET);
//		return NULL;
//	}

	/* Add new childNode */
        try {
		childNode = new E3DS_BMTreeNode;
        }
        catch ( std::bad_alloc ) {
                egi_dpstd("Fail to allocate a new childNode!\n");
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
	delete node;
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

#if 1 /* TEST ---------- HK2022-11-16 */
	while(!node->gmat.isOrthogonal()) {
        	//egi_dpstd(DBG_RED">>>>>> orthNormalize gmat... <<<<<< \n"DBG_RESET);
                node->gmat.orthNormalize();
		//if(!node->gmat.isOrthogonal())
		//	egi_dpstd(DBG_RED"___ Fail to orthNormalize ___ \n"DBG_RESET);
        }
#endif

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


/*------------------------------------------------------
To create BMatrixTree by reading BVH data

		=======  Typical BVH File Format  ======
HIERARCHY
ROOT Hips (hip)
{
	OFFSET  0.0 0.0 0.0
	CHANNELS 6 Xposition Yposition Zposition Zrotation Xrotation Yrotation
	JOINT LeftUpLeg
	{
		OFFSET ...
		CHANNELS 3 Zrotation Xrotation Yrotation
		JOINT Leftleg
		{
			... ...
			OFFET ...
			CHANNELS 3 ...
			End Site
			{
				OFFSET  x x x
			}
			... ...
		}
		... ...
	}
	JOINT RightUpLeg
	... ...
}
MOTION
Frames: 137
Frame Time: 0.016667
(data for each frame ) ....




--------- Bone Nodes: Bone_ROOT_Node, Bone_Hip_to_LeftUpLeg, Bone_LeftUpLeg_to_Leftleg, ....


		========= Secondlife:: avatar_xxx.bvh  ========
HIERARCHY
ROOT hip
{

	OFFSET	0.00  0.00  0.00
	CHANNELS 6 Xposition Yposition Zposition Xrotation Zrotation Yrotation
	JOINT abdomen
	{
		OFFSET	0.000000 0.000000 0.000000          <----------  !!! CAUTION !!! -----
		CHANNELS 3 Xrotation Zrotation Yrotation
		JOINT chest
		{
			OFFSET	0.000000 5.018152 -1.882228
			CHANNELS 3 Xrotation Zrotation Yrotation
			JOINT neckDummy
			{
		... ...
}



		=========  MIXAMO Rig BVH  ========

HIERARCHY
ROOT mixamorig:Hips
{
	OFFSET -0.000008 104.274872 1.554316
	CHANNELS 6 Xposition Yposition Zposition Xrotation Yrotation Zrotation
	JOINT mixamorig:Spine
	{
		OFFSET -0.000005 10.181595 0.131521
		CHANNELS 6 Xposition Yposition Zposition Xrotation Yrotation Zrotation
		JOINT mixamorig:Spine1
		{
			OFFSET -0.000000 9.893959 -1.470711
			CHANNELS 6 Xposition Yposition Zposition Xrotation Yrotation Zrotation
			JOINT mixamorig:Spine2
			{
				OFFSET -0.000000 9.220757 -1.370642
				CHANNELS 3 Xrotation Yrotation Zrotation   <------  !!! CAUTION !!! -----
				JOINT mixamorig:Neck
				{
					OFFSET 0.000001 16.740417 -2.049038
					CHANNELS 6 Xposition Yposition Zposition Xrotation Yrotation Zrotation
					JOINT mixamorig:Head
					{
	... ...
}


Refrence BVH files:
1. https://github.com/BandaiNamcoResearchInc/Bandai-Namco-Research-Motiondataset
2. https://wiki.secondlife.com/wiki/Internal_Animations
3. https://pages.cs.wisc.edu/~pingelm/ComputerAnimation/Motions/bvh/
4. mixamorig in https://www.mixamo.com


Note:
1. The BVH file defines a tree of joints('points'), while E3D_BMatrixTree
   defines a tree of 'bones'!
   				BVH COORD System
   RightHand COORD, Relative to a skeleton: Lefthand---COORD_X, Uppward---COORD_Y, Forward--COORD_Z

   				    For MBTREE_TYPE_E3D:
   Bone direction MUST align with local COORD_Z axis, and local COORDs NOT necessary to parallel with Global COORD.
				    For MBTREE_TYPE_BVH:
   Initial local COORDs MUST all parallel with Global COORD, bone direction NOT necessary to align with any axis!

2. 		!!!--- CAUTION ---!!!
   XXX Assum that each BVH joint(bone) has 3 CHANNELS(sequence: Zrotation Xrotation Yrotation). XXX
   XXX except the ROOT node, which has 6 CHANNELS.(sequence: Xposition Yposition Zposition Zrotation Xrotation Yrotation). XXX

   OK, Baidan_Namco BVH all have 6 CHANNELS, though XYZposition data is for NO USE for nonroot nodes.
   and MIXAMO Rig BVH has variable CHANNELS(6/3), some JOINTs have 6 channels, and others have 3 channels, it depends.

3. A 'JOINT' defines a E3D bone node, its 'CHANNELS' is defined before this 'JOINT', and its 'OFFSET'(as for bonelenth for
   this bone, and offset for next bone) is defined after this 'JOINT'.

5. In case of 'JOINT' and 'End site": add a new bone from parent to THIS joint point.
6. Assign E3DS_BMTreeNode.bv with OFFSET_xyz first, and convert to unit vector AFTER
   assign vb.xyz to next bone's pmat[9-11](Txyz);
7. Bones whose parent is ROOT are fixed with root node! they need NO
   CHANNEL values.
8. In VBH text, CHANNEL values are for the child bones linked with THIS joint,
   as a hinge for the child bones.
9. If a joint connects with more than 1 child bones, then they share
   the same CHANNEL value, see above 5.   <-------
10. At End_site, bnode MUST reset to NULL, to avoid 'push up last bnode before create a new bnode' at other cases.
------------------------------------------------------------*/
E3D_BMatrixTree::E3D_BMatrixTree(const char *fbvh)
{
	vector<E3DS_BMTreeNode*> branch;    /* To trace current branch of the BMTree */
	vector<E3DS_BMTreeNode*> nodestack; /* All BMTreeNode pointers are pushed in text sequence (PreOrder Traversal)
					     * this MUST be the same sequenece of frame data attached.
					     */
	vector<string> jointnames;	    /* Name of JOINT */

	/* XXX Assum that each joint(bone) has 3 CHANNELS(Zrotation Xrotation Yrotation),
	 * XXX and the ROOT has 6 CHANNELS.(Xposition Yposition Zposition Zrotation Xrotation Yrotation).
	 */
	vector<int>		 chanstack;	/* CHANNEL value stacks, in appearing sequence in the text.  */
	size_t 			 datacnt=0;    /* data(float type) number for one MOTION frame */
	vector<int>		 dataindex;    /* data start index for corresponding nodestack[] */

	vector<int>		 indexbranch;   /* to trace current dataIndex */
//	vector<int>		 chansbranch;	/* to trace number of nodechannels */
	vector<float>		 datagroup;    /* goup of data for one MOTION frame */
	int 			 lastindex;    /* Before indexbranch.pop_back, need to save as lastindex for later use.
						* In case a node has 2 or more branches, when it pop_back to the node,
						* it uses the lastindex to share same data.
					        */

//	int			lastchans; /* same as lastindex,
//					    * In case a node has 2 or more branches, when it pop_back to the node,
//					    * it uses the lastchans to share same CHANNELS.
//					    */

	E3DS_BMTreeNode *bnode=NULL;
	bool IsEndSite=false;
	bool IsRootNode=false;
	bool IsRootHips=true;   /* TRUE: Usually hip center is the ROOT,
				 * FALSE: Assume here as Baidan_Namco BVH.
				 */

	char rotAxisToks[4]={'Z','X','Y',0};  /* Rotation axis sequence tokens, 'ZXY' for most case. */
	vector<string> rotAxisTokStack;  /* To save Rotation axis sequence tokens for each bone. */
	vector<string> rotAxisTokBranch; /* To trace current rotAxisTok */
	string lastRotAxisTok;		 /* To save as last token in rotAxisTokBranch,
					  * In case a node has 2 or more branches, when it pop_back to the node,
                                          * it uses this to share same tokens.
                                          */

	char transAxisToks[4]={'X','Y','Z',0};  /* Translation axis sequence tokens, 'xyz' for most case. */
	vector<string> transAxisTokStack;  /* To save Translation axis sequence tokens for each bone. */
	vector<string> transAxisTokBranch; /* To trace current transAxisTok */
	string lastTransAxisTok;	   /* To save as last token in rotAxisTokBranch,
					  * In case a node has 2 or more branches, when it pop_back to the node,
                                          * it uses this to share same tokens.
                                          */

	int cntback=0;		/* This is to count number of '}'. when meet 'JOINT' reset to 0 aft use.
				 * --- As the length of the last sub_branch ---
				 */

	float blen;
	int  nodechans=6;  /* (non_ROOT) node channels. Assume 6 OR 3. */
	float offx=0.0f, offy=0.0f, offz=0.0f;

	int  frames; /* Total frames recorded */
	int framecnt=0;
	float tpf;   /* in second, time per frame */

	/* Set as BMTREE_TYP_VBH */
	type=BMTREE_TYPE_VBH;

	/* Open BVH data file */
	ifstream  fin(fbvh);
	string    txtline;
//	const char *ptr;
	size_t    tlcnt;
	size_t    pos;	/* string::size_type */

	/* Open file */
	if(!fin) {
		cerr<<"Fail to open "<<fbvh<<endl;
		return;
	}

printf(DBG_GREEN" ---- Start parsing ----\n"DBG_RESET);
	/* Parse each line */
	tlcnt=0;
	while(getline(fin, txtline)) {
		//cout<< DBG_YELLOW"txtline: "<<txtline<< "\n\n"DBG_RESET<< endl;
//		printf(DBG_YELLOW"txtline[%zu]:"DBG_RESET"\n %s\n", tlcnt, txtline.c_str());

		/* W1. Case key 'ROOT'  */
		if( (pos=txtline.find("ROOT")) !=txtline.npos ) {
			/* Push JOINT name */
			jointnames.push_back(txtline.substr(pos+4));

			printf("ROOT, pos=%zu\n",pos);
			IsRootNode=true;

			/* W1.1 Check if ROOT node is NOT hips center */
			if( (pos=txtline.find("Hip"))==txtline.npos && (pos=txtline.find("hip"))==txtline.npos )
			{
				IsRootHips=false; /* Root is NOT the hips center.  */
				type=BMTREE_TYPE_NAMCO;
			}

			/* W1.2 Create root node */
			root=BMT_createRootNode();

			/* W1.3 Push back to tracing branch */
			branch.push_back(root);
			nodestack.push_back(root);
			printf("Trancing branch.size=%zu\n", branch.size());

			/* W1.4 Push back axistok tracing branch ...
			   NOT here! we need CHANNELS from ROOT's firstChild ...SEE W2.1.2.1  */

			/* W1.5 <=======  Data index handling  =========> */
			dataindex.push_back(0);
			indexbranch.push_back(dataindex.back());

			nodechans=6;
			datacnt +=nodechans;
			lastindex=0;

			/* ---- */
			chanstack.push_back(6);
		}
		/* W2. Case key 'JOINT': Add a bone from parent to this joint!  and assign its parent(branch.back).pmat[9-11] */
		else if( (pos=txtline.find("JOINT")) !=txtline.npos ) {
			printf("JOINT, pos=%zu\n",pos);

			/* Push JOINT name */
			jointnames.push_back(txtline.substr(pos+5));

			/* W2.1 Push back last bone to tracing branch */
			if(bnode!=NULL) { /* Note, bnode reset to NULL aft push_back End_site bone */

				/* W2.1.1 Push last bnode */
				branch.push_back(bnode);
				nodestack.push_back(bnode);
				printf("Trancing branch.size=%zu\n", branch.size());

				/* <======= Data index handling  =========> */
				/* W2.1.2 Push rotAxisToks for the ROOT here first!!! */
				if(bnode->parent==root) {  /* Fixed with ROOT, need no channels */
					printf("The node has Root as its parent!\n");

					/* W2.1.2.1 For ROOT's direct firsChild:
					   -----> Push rotAxisToks same for the ROOT, check note at W1.4 */
					if(bnode->parent->firstChild==bnode) {
						rotAxisTokStack.push_back(rotAxisTokStack.back());
						rotAxisTokBranch.push_back(rotAxisTokStack.back());
						transAxisTokStack.push_back(transAxisTokStack.back());
						transAxisTokBranch.push_back(transAxisTokStack.back());
					}

					/* Same CHANNELS as ROOT, no need for dataindex. */
				}

				/* W2.1.3 If share the same CHANNEL data with siblings, see note 7. */
			 	if(bnode->parent->firstChild !=bnode ) {
					printf(">>> Share data with siblings: lastindex=%d, indexbranch.size=%zu, indexbranch.back()=%d\n",
							lastindex, indexbranch.size(), indexbranch.back() );

					dataindex.push_back(lastindex); /* HK2022-11-16 */
					indexbranch.push_back(dataindex.back());

					/* NOT push rotAxisToks and tranAxisTok here!!! See in W2.3.1 */

				}
				/* W2.1.4 Need 6/3 CHANNEL values */
				else
				{ /* The firstChild, Or the ONLY child.*/

					/* Note: for ROOT's direct child,  dataindex will be ignored later. as it shares with ROOT's. */
					dataindex.push_back(datacnt);
					indexbranch.push_back(dataindex.back());

					/* CHANNEL value for next bone index */
					//printf("indexbranch.push_back(), indexbranch.back()=%d\n", indexbranch.back());
					if( bnode->parent!=root ) { /* Root's child share data with ROOT node */
						/* nodechans should be previous CHANNEL value! */
						nodechans=chanstack[chanstack.size()-2];
						datacnt +=nodechans;
						printf(">>> datacnt +=%d, datacnt=%d \n",nodechans, datacnt);
					}
					else
						printf("ROOT child, >>> datacnt +=0, datacnt=%d \n",datacnt);
				}

#if 0 /* Test: ---------- */
				if(bnode->parent==root)
				   	printf("DataStartIndex for a root child, No need.\n");
				else
					printf("DataStartIndex for above node: %d.\n", dataindex.back());
#endif
				/* <======= END data index handling  =========> */

			} /* End push last bone node */


			/* W2.2 Add a bone from its parent to this joint */
			bnode=addChildNode(branch.back(), 0.0f); /* BoneLength unkown here, until we get next OFFSET */
			if(bnode==NULL) {
				egi_dpstd(DBG_RED"Fail to addChildNode!\n"DBG_RESET);
				return;
/* Retrun  <----------------------------- */
			}


			/* W2.3 As axisToks is present before this 'JOINT' */
			if(bnode->parent->firstChild !=bnode ) {
				//printf(">>> Share data with siblings: lastindex=%d, indexbranch.size=%zu, indexbranch.back()=%d\n",
				//			lastindex, indexbranch.size(), indexbranch.back() );

				/* NOT push dataindex here!!! See in W2.1.3 */

				/* W2.3.1 Push rotAxisToks same as the firstChild, No 'CHANNELS' definded before this 'JOINT' */
				rotAxisTokStack.push_back(lastRotAxisTok);
				rotAxisTokBranch.push_back(rotAxisTokStack.back());
				transAxisTokStack.push_back(lastTransAxisTok);
				transAxisTokBranch.push_back(transAxisTokStack.back());

				printf("--- rotAxisTokStack.back(): %s, transAxisTokStack.back(): %s ---\n",
						    rotAxisTokStack.back().c_str(), transAxisTokStack.back().c_str() );
			}

			/* W2.4 Local COORD offset from the parent */
			if(bnode->parent==root) { /* If ROOT's direct child, it's fixed with the ROOT. */
				bnode->pmat.pmat[9]=0.0f;
				bnode->pmat.pmat[10]=0.0f;
				bnode->pmat.pmat[11]=0.0f;

			}
			else {
				/* Local COORD as its parent bone Vector */
				bnode->pmat.pmat[9]=bnode->parent->bv.x;
				bnode->pmat.pmat[10]=bnode->parent->bv.y;
				bnode->pmat.pmat[11]=bnode->parent->bv.z;

				/* NOW: normalize paretn->bv */
				/* XXX NOT HERE! Each child node need it for pmat[9-11]!!! */
				// bnode->parent->bv.normalize();
			}

			/* W2.5 Reset count back */
			cntback=0;

		}
		/* W3. Case key 'OFFSET': set root.offxyz, set bnode.bv and bnode.blen. */
		else if( (pos=txtline.find("OFFSET")) !=txtline.npos ) {
			printf("OFFSET, pos=%zu, offset: %s\n",pos, txtline.substr(pos+6).c_str());

			/* W3.1 Read OFFSET xyz */
			sscanf(txtline.substr(pos+6).c_str(), "%f %f %f", &offx,&offy,&offz);
			printf(DBG_GREEN"___offxyz: %f,%f,%f\n"DBG_RESET, offx,offy,offz);

			/* W3.2 Assign root->pmat.pmat[9-11] */
			if(IsRootNode && root!=NULL ) {  /* root SHOULD already created at case 'ROOT' */
				/* Assign Offset_xyz to pmat.Txyz */
				root->pmat.pmat[9]=offx;
				root->pmat.pmat[10]=offy;
				root->pmat.pmat[11]=offz;

				/* Unmark */
				IsRootNode=false;
			}
			/* W3.3 Assign blen and bv */
			else if(bnode) { /* bnode SHOULD already created at case 'JOINT' */
				/* bone length */
				blen=sqrt(offx*offx+offy*offy+offz*offz);
				bnode->blen=blen;

				/* Check if ROOT is NOT the hips center */
				if(!IsRootHips && bnode->parent==root)
					bnode->blen=0.0f; /* DO NOT draw then bone */

				/* Note: this OFFSET is for NEXT bone */
				/* bone direction vector */
				bnode->bv.assign(offx,offy,offz);
			}

			/* W3.4 If the EndSite */
			if(IsEndSite && bnode!=NULL) {
				/* W3.4.1 Push back the end_site bone to tracing branck */
				branch.push_back(bnode);
				nodestack.push_back(bnode);
				printf("Trancing branch.size=%zu\n", branch.size());

				/* <======= Data index handling  =========> */
				/* W3.4.2 */
				if(bnode->parent==root) {  /* Fixed with ROOT, need no channels */
					printf("The node has Root as its parent!\n");
				}

				/* W3.4.3 If share the same CHANNEL values with siblings, see note 7. */
			 	if(bnode->parent->firstChild !=bnode ) {
					printf(">>> Share data with siblings: indexbranch.size=%zu, indexbranch.back()=%d\n",
							indexbranch.size(), indexbranch.back() );
					dataindex.push_back(lastindex); /* HK2022-11-16 */
					indexbranch.push_back(dataindex.back());
				}
				/* W3.4.4 Need 6/3 CHANNEL values */
				else {
					dataindex.push_back(datacnt);
					indexbranch.push_back(dataindex.back());
					if( bnode->parent!=root ) {
						nodechans =chanstack[chanstack.size()-1]; /* For EndSite bone */
						datacnt +=nodechans;
						printf(">>> datacnt +=%d, datacnt=%d\n", nodechans, datacnt);
					}
				}
#if 0 /* Test: ---------- */
				if(bnode->parent==root)
				   	printf("DataStartIndex for a root child, No need.\n");
				else
					printf("DataStartIndex for above node: %d.\n", dataindex.back());
#endif
				/* <======= END data index handling  =========> */


				/* W3.4.5 <<<<<<<<<<< Reset bnode here! >>>>>>>>>> */
				bnode=NULL;

				/* Unmark */
				// IsEndSite=false; NOT here, see at case '}' ...
			}


		}
		/* W4. Case key 'CHANNELS': This for the next bone! if this_bone.parent=root, then it's all fixed.  */
		else if( (pos=txtline.find("CHANNELS")) !=txtline.npos ) {
			//printf("CHANNELS found, pos=%zu\n",pos);

			/* W4.1 Get nodechans, usually 6 or 3. */
			sscanf(txtline.substr(pos+9).c_str(), "%d", &nodechans);
			printf("CHANNELs found, Read nodechans=%d\n", nodechans);

			/* Push to chanstack */
			chanstack.push_back(nodechans);

			int ps[3];
			/* W4.2 Get translation axis sequences */
			ps[0]=(int)txtline.find("Xposition");
			ps[1]=(int)txtline.find("Yposition");
			ps[2]=(int)txtline.find("Zposition");

			/* W4.3 XYZ MUST be presented together */
			if( (size_t)ps[0]!=txtline.npos && (size_t)ps[1]!=txtline.npos && (size_t)ps[2]!=txtline.npos) {
				/* W4.3.1 Sort in ascending order */
				mat_quick_sort(ps, 0, 2, 3);

				transAxisToks[0]=txtline[ps[0]];
				transAxisToks[1]=txtline[ps[1]];
				transAxisToks[2]=txtline[ps[2]];
				transAxisToks[3]=0;
				printf(" --- TransAxisToks: %s ----\n", transAxisToks);
				//exit(0);

				/* W4.3.2 Push to transAxisTokStack[] */
				transAxisTokStack.push_back(transAxisToks);
				transAxisTokBranch.push_back(transAxisTokStack.back());
			}
			/* W4.4 NO XYZ_postion presented */
			else {
				transAxisToks[0]=0;
				transAxisToks[1]='_';
				transAxisToks[2]='_';
				transAxisToks[3]=0;

				/* W4.3.2 Push to transAxisTokStack[] */
				transAxisTokStack.push_back(transAxisToks);
				transAxisTokBranch.push_back(transAxisTokStack.back());
			}

			/* W4.4 Get rotation axis sequences */
			ps[0]=(int)txtline.find("Xrotation");
			ps[1]=(int)txtline.find("Yrotation");
			ps[2]=(int)txtline.find("Zrotation");

			/* W4.5 XYZ MUST be presented together */
			if( (size_t)ps[0]!=txtline.npos && (size_t)ps[1]!=txtline.npos && (size_t)ps[2]!=txtline.npos) {
				/* W4.5.1 Sort in ascending order */
				mat_quick_sort(ps, 0, 2, 3);

				rotAxisToks[0]=txtline[ps[0]];
				rotAxisToks[1]=txtline[ps[1]];
				rotAxisToks[2]=txtline[ps[2]];
				rotAxisToks[3]=0;
				printf(" --- RotAxisToks: %s ----\n", rotAxisToks);
				//exit(0);

				/* W4.5.2  Push to rotAxisTokStack[] */
				rotAxisTokStack.push_back(rotAxisToks);
				rotAxisTokBranch.push_back(rotAxisTokStack.back());
			}
			/* W4.6 Keep same as previous in rotAxisToks.
				Note: XYZ_Rotation MUST be presented for each bone. */
			else {
				/* 4.6.1 Push to rotAxisTokStack[] */
				rotAxisTokStack.push_back(rotAxisToks);
				rotAxisTokBranch.push_back(rotAxisTokStack.back());
			}

		}
		/* W5. Case key 'End_Site': Push last bnode to branch, add the END bone. */
		else if( (pos=txtline.find("End Site")) !=txtline.npos ) {
			printf("JOINT found, pos=%zu\n",pos);

			/* Push JOINT name */
			jointnames.push_back("End_Site");

			/* W5.1 Set IsEndSite */
			IsEndSite=true;

			/* W5.2 Push back last bone to tracing branch */
			if(bnode!=NULL) { /* Note, bnode reset to NULL aft push_back End_site bone */

				/* W5.2.1 Push bnode */
				branch.push_back(bnode);
				nodestack.push_back(bnode);
				printf("Trancing branch.size=%zu\n", branch.size());

				/* <======= Data index handling  =========> */
				/* W5.2.2 */
				if(bnode->parent==root) {  /* Fixed with ROOT, need no channels */
					printf("The node has Root as its parent!\n");
				}

				/* W5.2.3 If share the same CHANNEL values with siblings, see note 7. */
			 	if(bnode->parent->firstChild !=bnode ) {
					printf(">>> Share data with siblings: indexbranch.size=%zu, indexbranch.back()=%d\n",
							indexbranch.size(), indexbranch.back() );
					dataindex.push_back(lastindex);/* HK2022-11-16 */
					indexbranch.push_back(dataindex.back());
				}
				/* W5.2.4 Need 6/3 CHANNEL values */
				else {
					dataindex.push_back(datacnt);
					indexbranch.push_back(dataindex.back());
					if( bnode->parent!=root ) {
						/* nodechans should be previous CHANNEL value! */
						nodechans=chanstack[chanstack.size()-2];
						datacnt +=nodechans;
						printf(">>> datacnt +=%d, datacnt=%d\n",nodechans, datacnt);
					}
				}
#if 0 /* Test: ---------- */
				if(bnode->parent==root)
				   	printf("DataStartIndex for a root child, No need.\n");
				else
					printf("DataStartIndex for above node: %d.\n", dataindex.back());
#endif
				/* <======= END data index handling  =========> */

			}

			/* W5.3 Add a bone from parent to this End_Site */
			bnode=addChildNode(branch.back(), 0.0f); /* BoneLength unkown here, until get OFFSET */
			if(bnode==NULL) {
				egi_dpstd(DBG_RED"Fail to addChildNode!\n"DBG_RESET);
				return;
/* Retrun  <----------------------------- */
			}

			/* W5.4 Local COORD offset from the parent */
			bnode->pmat.pmat[9]=bnode->parent->bv.x;
			bnode->pmat.pmat[10]=bnode->parent->bv.y;
			bnode->pmat.pmat[11]=bnode->parent->bv.z;

			/* NOW: normalize paretn->bv */
			/* XXX NOT HERE! Each child node need it for pmat[9-11]!!! */
                        //bnode->parent->bv.normalize();
		}
		/* W6. Case key '}': branch.pop_back() */
		else if( (pos=txtline.find("}")) !=txtline.npos ) {

			/* Reseet IsEndSite */
			if(IsEndSite) {
				IsEndSite=false;
			}

			/* Pop a node from the branch */
			branch.pop_back();
			if(branch.empty()) {
				printf("...tracing branch is empty NOW! End VBH Tree!?, datacnt=%zu .....\n", datacnt);
			}
			else {
				printf("Trancing branch.size=%zu (ROOT is also a bonenode)\n", branch.size());
				if(branch.back()->parent==NULL)
					printf("Get to the ROOT.\n");

			}


			/* Pop transAxisTokBranch and rotAxisTokBranch */
			printf("Trancing rotAxisTokBranch.size=%zu, transAxisTokBranch.size=%zu\n",
					rotAxisTokBranch.size(), transAxisTokBranch.size() );
			lastTransAxisTok=transAxisTokBranch.back();
			transAxisTokBranch.pop_back();

			lastRotAxisTok=rotAxisTokBranch.back();
			rotAxisTokBranch.pop_back();


			/* Count back */
			cntback +=1;

			/* <======= Data index handling  =========> */
			/* Need to save lastindex before pop_back */
			lastindex=indexbranch.back();
			/* Pop indexbranch */
			indexbranch.pop_back();
			printf("lastindex=%d, indexbranch.pop_back...\n", lastindex);

		}
		/* W7. Case key 'Frames:' Total frame data attached. */
		else if( (pos=txtline.find("Frames:")) !=txtline.npos ) {
			sscanf(txtline.substr(pos+7).c_str(), "%d", &frames);
			printf("Frames: %d\n", frames);
		}
		/* W8. Case key 'Frame Time:' Frame time in second. */
		else if( (pos=txtline.find("Frame Time:")) !=txtline.npos ) {
			sscanf(txtline.substr(pos+11).c_str(), "%f", &tpf);
			printf("Time per. frame: %f(s)\n", tpf);

/* <---------------------- break here */
			break;
		}

		tlcnt++;
	}

	/* Update nodePtrList in LevelOrder */
	updateNodePtrList();

	/* Normalize all 3DS_BMTreeNode::bv */
	for(size_t k=0; k<nodePtrList.size(); k++) {
		nodePtrList[k]->bv.normalize();
	}


#if 0 ////////////////////* Change BMTreeNode.pmat to let local COORD_Y as boneLength direction *////////////////////
	E3D_Vector axisY(0.0,1.0,0.0);
	for(size_t k=0; k<nodePtrList.size(); k++) {
		/*  Assign axisY rotation relative to parent axisY: BMTreeNode.pmat.pmat[0-8] */
		/*  Reset BMTreeNode.pmat.pmat[0-8], Only aixsY for bone length. */

		if(nodePtrList[k]->parent==NULL) {
			/* Rotation: The ROOT node, Local axis_Y as bone direction. init as align with Global axis_Y. */
			/* Translation: ok, alreay with assigned value OFFSET_xyz */
		}
		else if(nodePtrList[k]->parent==root) {
			/* Rotation: from parent axisY to this axisY */
		        nodePtrList[k]->pmat.setRotation(axisY, nodePtrList[k]->bv); /* Bone growing direction axisY */
			/* Translation: as parent blen=0.0 */
			nodePtrList[k]->pmat.setTranslation(0.0,0.0,0.0);
		}
		else {
			/* Rotation: from parent axisY to this axisY */
		        nodePtrList[k]->pmat.setRotation(nodePtrList[k]->parent->bv, nodePtrList[k]->bv);
			/* Translation: along parent bone length */
			nodePtrList[k]->pmat.setTranslation(0.0, nodePtrList[k]->parent->blen, 0.0);
		}
	}
#endif //////////////////////////////////////////////////////////////////////////////////////////

#if 1 /* TEST: Check nodestack and corresponding dataindex ------------- */
	printf(" ============= nodestack =========== \n");

	#if 0
	for(size_t k=0; k<nodestack.size(); k++) {
		nodestack[k]->bv.print(NULL);
	}
	#endif

	printf(" ============= node dataindex[] =========== \n");
	for(size_t k=0; k<nodestack.size(); k++) {
                printf(">>>>> Node[%d] '%s': corresponding frame dataindex: %d %s <<<<<\n",
				k, jointnames[k].c_str(), dataindex[k], nodestack[k]->parent==root?"(ROOT's child, Ignore!)":"");
        }

	printf(" ============= node axisTokStack[] =========== \n");
	for(size_t k=0; k<rotAxisTokStack.size(); k++) {
		printf(">>>>> Node[%d] '%s': Translation '%s', Rotation '%s'  <<<<<\n",
				k, jointnames[k].c_str(), transAxisTokStack[k].c_str(), rotAxisTokStack[k].c_str());
	}

//	sleep(2);
//	exit(0);
#endif


#if 0 /* TEST: ------------- */
	nodestack[1]->pmat.print("---nodestack[1].pamt---");

	printf("Total %zu bone nodes added!\n", nodePtrList.size());
	for(size_t k=0; k<nodePtrList.size(); k++) {
		printf(">>>>> Node[%d]: blen=%f, with dataindex: %d (no need for root's child) <<<<<\n", k, nodePtrList[k]->blen, dataindex[k]);
		nodePtrList[k]->bv.print(NULL);
	}
#endif


#if 1	/* Read all data into animQts[] for each BMTreeNode */
	char *pt;
	const char *delims=" 	\n\r\t"; /* space and tab */

	E3D_Quatrix qt;
	float angs[3];
	int index;
	float invfms=1.0/(frames-1);
	float PIpd=MATH_PI/180.0;

	framecnt=0;
	while(getline(fin, txtline)) {
//		printf("	=== framecnt:%d/[%d-1] ===\n", framecnt,frames);
//		printf(DBG_YELLOW"txtline[%zu]:"DBG_RESET"\n %s\n", tlcnt, txtline.c_str());

		/* R1. Read joint transform(RT) data for one frame */
		pt=strtok((char *)txtline.c_str(), delims);
		while(pt) {
			datagroup.push_back(atof(pt));
			pt=strtok(NULL,delims);
		}
//		printf(DBG_YELLOW" ------ datagroup.size=%zu -----\n"DBG_RESET, datagroup.size());

		#if 0 /* Print datagroup */
		for(size_t k=0; k<datagroup.size(); k++)
			printf("datagroup[%zu]: %f\n", k, datagroup[k]);
		#endif

		/* R2. Assign movment data to corresponding BMTreeNode.animQts */
		egi_dpstd("nodestack.size(+ROOT)=%zu, jointnames.size(+ROOT)=%zu, rotAxisTokStack.size=%zu, transAxisTokStack.size=%zu \n",
		nodestack.size(), jointnames.size(), rotAxisTokStack.size(), transAxisTokStack.size());
		for(size_t k=0; k<nodestack.size(); k++) {

			/* R2.1 The ROOT node: 6 Channels */
			if(k==0) {
printf("node[0]: T'%s'+R'%s', TransXYZ:%f,%f,%f RotAngs:%f,%f,%f\n", transAxisTokStack[0].c_str(), rotAxisTokStack[0].c_str(),
                                datagroup[0],datagroup[1],datagroup[2],
                                datagroup[3],datagroup[4],datagroup[5]);

				/* Sequence: Xposition Yposition Zposition Zrotation Xrotation Yrotation */
				qt.identity();

				/* First, set rotation */
				angs[0]=PIpd*datagroup[3];
				angs[1]=PIpd*datagroup[4];
				angs[2]=PIpd*datagroup[5];
				//qt.setIntriRotation("ZXY", angs);
				//qt.setIntriRotation(rotAxisToks, angs);
				qt.setIntriRotation(rotAxisTokStack[k].c_str(), angs);

				/* Translation After rotation! */
				qt.setTranslation(datagroup[0],datagroup[1],datagroup[2]);

				/* Insert to animQts */
				nodestack[k]->animQts.insert(1.0*framecnt*invfms, qt); //(1.0*k*/(frames-1), qt);
			}
			/* R2.2 Direct child of the ROOT. see as fixed with the ROOT node. */
			else if(nodestack[k]->parent==root) {
printf("node[%zu]: ROOT's child,  T%s+R%s, Fixed with ROOT.\n", k, transAxisTokStack[k].c_str(), rotAxisTokStack[k].c_str());
				//datagroup[index],datagroup[index+1],datagroup[index+2],
				//datagroup[index+3],datagroup[index+4],datagroup[index+5]);

				/* See as fixed  with the ROOT. NO need to insert an identity qt. */
				qt.identity();
				nodestack[k]->animQts.insert(1.0*framecnt*invfms, qt);
			}
			/* R2.2 Normal bone nodes: Note, some bone may share the same data in datagroup[] */
			else {
				/* qt and index */
				qt.identity();
				index=dataindex[k]; /* start data index for the node */

				/* Get node channels */
				nodechans=rotAxisTokStack[k].size()+transAxisTokStack[k].size();

				/* Case: 6 CHANNELS */
				if(nodechans==6) {
printf("node[%zu]: T'%s'+R'%s', TransXYZ:%f,%f,%f RotAngs:%f,%f,%f\n", k, transAxisTokStack[k].c_str(), rotAxisTokStack[k].c_str(),
				datagroup[index],datagroup[index+1],datagroup[index+2],
				datagroup[index+3],datagroup[index+4],datagroup[index+5]);

					/* Rotation Before translation */
					angs[0]=PIpd*datagroup[index+3];
					angs[1]=PIpd*datagroup[index+4];
					angs[2]=PIpd*datagroup[index+5];

					//qt.setIntriRotation("ZXY", angs);
					//qt.setIntriRotation(rotAxisToks, angs);
					qt.setIntriRotation(rotAxisTokStack[k].c_str(), angs);

					/* ---> For Baidan_Namco BVH, assign local origin XYZ movement to hips.  HK2022-11-22 */
					#if 1
					if(!IsRootHips && nodestack[k]->parent->parent==root) /* Hi...HK2022-12-01 */
					  	qt.setTranslation(datagroup[index],datagroup[index+1],datagroup[index+2]);
					#endif

					/* Translation After rotation! NOPE! ...Usually same as OFFSET */
					// qt.setTranslation(datagroup[index],datagroup[index+1],datagroup[index+2]);
				}
				/* Case: 3 CHANNELS.  Rotation_XYZ MUST present. */
				else if(nodechans==3) {
printf("node[%zu]: T'none'+R'%s', RotAngs:%f,%f,%f\n", k, rotAxisTokStack[k].c_str(),
				datagroup[index],datagroup[index+1],datagroup[index+2] );

					/* Rotation */
					angs[0]=PIpd*datagroup[index];
					angs[1]=PIpd*datagroup[index+1];
					angs[2]=PIpd*datagroup[index+2];

					//qt.setIntriRotation("ZXY", angs);
					//qt.setIntriRotation(rotAxisToks, angs);
					qt.setIntriRotation(rotAxisTokStack[k].c_str(), angs);
				}
				/* Case: Unexpected! */
				else {
					egi_dpstd(DBG_RED"Unexpected nodechans=%d for nodestack[%d]!\n"DBG_RESET, nodechans, k);
				}

				/* Insert to animQts */
				nodestack[k]->animQts.insert(1.0*framecnt*invfms, qt); //(1.0*k*/(frames-1), qt);
			}
		}

		/* Counter incremental */
		framecnt++;
		tlcnt++;

		/* Clear datagroup */
		datagroup.clear();

/* TEST: ----------- */
//exit(0);

	}
#endif

	fin.close();
	egi_dpstd(DBG_GREEN"Totally read %d of %d frames of animation data, each frame takes %f second.\n", framecnt, frames, tpf);

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
	E3D_Quatrix qt;
	E3D_RTMatrix mat;

        /* 2. Create BoneTree --- a line tree */
        float ang[3]={0.0,0.0,0.0};
	float s=50;
        float blen=7*s;


	/* 0. Root node */

        /* 1. 1st node */
        node=addChildNode(root, blen); /* node, boneLen */

        /* 2. 2nd node */
        node=addChildNode(node, blen);
        ang[0]=MATH_PI*30/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* 2.1 Animating Quatrices(KeyFrames) */
	qt.setRotation('Y',MATH_PI*0/180);  /* <--- upon pmat */
	node->animQts.insert(0.0f, qt);
	qt.setRotation('Y',MATH_PI*100/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('Y',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);

        /* 3. 3rd node */
        node=addChildNode(node, 0.6*blen);
        ang[0]=MATH_PI*90/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */
	/* 3.1 Animating Quatrices(KeyFrames) */
#if 1
	qt.setRotation('Y',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);

	qt.setRotation('Y',MATH_PI*30/180);
	node->animQts.insert(0.5f, qt);

	qt.setRotation('Y',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);
#else
	qt.identity();
	qt.setRotation('Y',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);

	ang[0]=MATH_PI*180/180;
	ang[1]=MATH_PI*90/180;
	mat.identity();
	mat.combIntriRotation("Lzy",ang);/* Rotation Z+Y  */
	qt.fromMatrix(mat);
	node->animQts.insert(0.5f, qt);

	ang[0]=MATH_PI*0/180;
	ang[1]=MATH_PI*0/180;
	mat.identity();
	mat.combIntriRotation("Lzy",ang); /* Rotation Z+Y */
	qt.fromMatrix(mat);
	node->animQts.insert(1.0f, qt);
#endif

        /* 4. 4th node */
        node=addChildNode(node, 0.5*blen);
        ang[0]=MATH_PI*40/180;
        node->pmat.combIntriRotation("Y",ang);
        node->pmat.setTranslation(0,0, node->parent->blen); /* <---- parent's blen */

	/* 4.1 Animating Quatrices */
	qt.setRotation('Y',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('Y',MATH_PI*95/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('Y',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);

        /* Finally: update NodePtrList + update tree Gmats */
        updateNodePtrList();
}

/*---------------- TEST --------------------
Create a BoneMatrixTree for a hand something.
Bone direction Z

	!!! --- CAUTION --- !!!
The initial E3D_BMatrixTree should ONLY
have ROOT node!

------------------------------------------*/
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

        /* Finally: update NodePtrList to update Gmat list */
        updateNodePtrList();
}


#if 0 //////////////////////////////
/*---------- TEST ------------------
Create a BoneMatrixTree for a boxman.
  Motion: Lift left_leg and right_arm

	!!! --- CAUTION --- !!!
The initial E3D_BMatrixTree should ONLY
have ROOT node!

-----------------------------------*/
void E3D_BMatrixTree::createBoxmanBMTree()
{
        E3DS_BMTreeNode* node;
	E3DS_BMTreeNode* crossNode; /* crossNode, joining with neck/spine/shoulderbones */
	E3D_Quatrix qt;
	E3D_RTMatrix mat;

        /* 2. Create BoneTree */
        float ang[3]={0.0,0.0,0.0};
	float s=50;
        float blen=7*s;

	/* 0. Root node */

/* Left Leg */
        /* 1. node[1]: hip bone  */
        node=addChildNode(root, 0.45*blen); /* node, boneLen */
        ang[0]=MATH_PI*-120/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,0); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) */


        /* 2. node[2]: thighbone  */
        node=addChildNode(node, 1.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*-(180-120 +7)/180;  /* 7Deg turn inside */
        node->pmat.setRotation('Y',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-110/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);

        /* 3. node[3]: shank bone  */
        node=addChildNode(node, 1.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*7/180; /* 7Deg turn outside */
        node->pmat.setRotation('Y',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*130/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);


        /* 4. node[4]: foot bone  */
        node=addChildNode(node, 0.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*-90/180;
        node->pmat.setRotation('X',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-35/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);


/* Right Leg */
        /* 5. node[5]: hip bone  */
        node=addChildNode(root, 0.45*blen); /* node, boneLen */
          ang[0]=MATH_PI*120/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,0); /* <--- parent's blen */

        /* 6. node[6]: thighbone  */
        node=addChildNode(node, 1.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*(180-120 +7)/180;  /* 7Deg turn inside */
        node->pmat.setRotation('Y',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 7. node[7]: shank bone  */
        node=addChildNode(node, 1.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*-7/180;	/* 7Deg turn outside */
        node->pmat.setRotation('Y',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 8. node[8]: foot bone  */
        node=addChildNode(node, 0.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*-90/180;
        node->pmat.setRotation('X',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

/* Spine to neck */
        /* 9. node[9]: spine_1  */
        node=addChildNode(root, 0.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('X', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 10. node[10]: spine_1A  */
        node=addChildNode(node, 0.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('X', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- turn backward */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*20/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);

        /* 11. node[11]: spine_2  */
        node=addChildNode(node, 0.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('X', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

	crossNode=node;

        /* 12. node[12]: spine_neck  */
        node=addChildNode(node, 0.25*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('X', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 13. node[13]: spine_head  */
        node=addChildNode(node, 0.75*blen); /* node, boneLen */
        ang[0]=MATH_PI*20/180;
        node->pmat.setRotation('X', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */


/* Left shoulder to arm */
        /* 14. node[14]: left_shoulder bone  */
        node=addChildNode(crossNode, 0.6*blen); /* node, boneLen */
        ang[0]=MATH_PI*-100/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 15. node[15]: left_upperarm bone  */
        node=addChildNode(node, 1.0*blen); /* node, boneLen */
        ang[0]=MATH_PI*-(180-100)/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */


        /* 16. node[16]: left_forearm bone  */
        node=addChildNode(node, 0.8*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 17. node[17]: left_hand bone  */
        node=addChildNode(node, 0.3*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */


/* Right shoulder to arm */
        /* 18. node[18]: right_shoulder bone  */
        node=addChildNode(crossNode, 0.6*blen); /* node, boneLen */
        ang[0]=MATH_PI*100/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 19. node[19]: right_upperarm bone  */
        node=addChildNode(node, 1.0*blen); /* node, boneLen */
        ang[0]=MATH_PI*(180-100)/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- arm level up */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-110/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);

        /* 20. node[20]: right_forearm bone  */
        node=addChildNode(node, 0.8*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
#if 0	/* Animating Quatrices(KeyFrames) --- contract biceps */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-150/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);
#else /* Animating Quatrices(KeyFrames) --- turn albow out, turn hand to chest  */
	qt.setRotation('Y',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('Y',MATH_PI*90/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('Y',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);
#endif


        /* 21. node[21]: right_hand bone  */
        node=addChildNode(node, 0.3*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* Finally: update NodePtrList to update Gmat list */
        updateNodePtrList();
}
#endif //////////////////////////////////////////////////////////////


/*-====--------- TEST ------------------
Create a BoneMatrixTree for a boxman.
	Motion: squating

	!!! --- CAUTION --- !!!
The initial E3D_BMatrixTree should ONLY
have ROOT node!

===-----------------------------------*/
void E3D_BMatrixTree::createBoxmanBMTree()
{
        E3DS_BMTreeNode* node;
	E3DS_BMTreeNode* crossNode; /* crossNode, joining with neck/spine/shoulderbones */
	E3DS_BMTreeNode* foot;
	E3D_Quatrix qt;
	E3D_RTMatrix mat;

        float ang[3]={0.0,0.0,0.0};
	float s=50;
        float blen=7*s;

	/* 0. Root node */
#if 0	/* Animating Quatrices(KeyFrames) */
	root->pmat.identity();
	qt.setTranslation(0.0, 0.0, 0.0);
	root->animQts.insert(0.0f, qt);
	/* The max. downward movement of the hip. */
	qt.setTranslation(0.0, 0.0, (1.5*sin(MATH_PI*55/180)-1.5*sin(MATH_PI*10/180))*blen -(1.5+1.5)*blen );
	root->animQts.insert(0.5f, qt);
	qt.setTranslation(0.0, 0.0, 0.0);
	root->animQts.insert(1.0f, qt);
#endif

/* Left Leg */
        /* 1. node[1]: hip bone  */
        node=addChildNode(root, 0.45*blen); /* node, boneLen */
        ang[0]=MATH_PI*-120/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,0); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) */
	/* none */

        /* 2. node[2]: thighbone  */
        node=addChildNode(node, 1.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*-(180-120 +7)/180;  /* 7Deg turn inside */
        node->pmat.setRotation('Y',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- squat down */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-110/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);

        /* 3. node[3]: shank bone  */
        node=addChildNode(node, 1.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*7/180; /* 7Deg turn outside */
        node->pmat.setRotation('Y',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- squat down */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*135/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);

        /* 4. node[4]: foot bone  */
        node=addChildNode(node, 0.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*-90/180;
        node->pmat.setRotation('X',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- squat down */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-35/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);


/* Right Leg */
        /* 5. node[5]: hip bone  */
        node=addChildNode(root, 0.45*blen); /* node, boneLen */
          ang[0]=MATH_PI*120/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,0); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) */
	/* none */


        /* 6. node[6]: thighbone  */
        node=addChildNode(node, 1.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*(180-120 +7)/180;  /* 7Deg turn inside */
        node->pmat.setRotation('Y',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- squat down */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-110/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);


        /* 7. node[7]: shank bone  */
        node=addChildNode(node, 1.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*-7/180;	/* 7Deg turn outside */
        node->pmat.setRotation('Y',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- squat down */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*135/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);

        /* 8. node[8]: foot bone  */
        node=addChildNode(node, 0.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*-90/180;
        node->pmat.setRotation('X',ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- squat down */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-35/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);

	foot=node;

/* Spine to neck */
        /* 9. node[9]: spine_1  */
        node=addChildNode(root, 0.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('X', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- squat down */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-10/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);


        /* 10. node[10]: spine_1A  */
        node=addChildNode(node, 0.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('X', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- turn forward */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-30/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);

        /* 11. node[11]: spine_2  */
        node=addChildNode(node, 0.5*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('X', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

	crossNode=node;

        /* 12. node[12]: spine_neck  */
        node=addChildNode(node, 0.25*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('X', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 13. node[13]: spine_head  */
        node=addChildNode(node, 0.75*blen); /* node, boneLen */
        ang[0]=MATH_PI*20/180;
        node->pmat.setRotation('X', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */


/* Left shoulder to arm */
        /* 14. node[14]: left_shoulder bone  */
        node=addChildNode(crossNode, 0.6*blen); /* node, boneLen */
        ang[0]=MATH_PI*-100/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 15. node[15]: left_upperarm bone  */
        node=addChildNode(node, 1.0*blen); /* node, boneLen */
        ang[0]=MATH_PI*-(180-100)/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- turn forward */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-130/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);


        /* 16. node[16]: left_forearm bone  */
        node=addChildNode(node, 0.8*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 17. node[17]: left_hand bone  */
        node=addChildNode(node, 0.3*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */


/* Right shoulder to arm */
        /* 18. node[18]: right_shoulder bone  */
        node=addChildNode(crossNode, 0.6*blen); /* node, boneLen */
        ang[0]=MATH_PI*100/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 19. node[19]: right_upperarm bone  */
        node=addChildNode(node, 1.0*blen); /* node, boneLen */
        ang[0]=MATH_PI*(180-100)/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */
	/* Animating Quatrices(KeyFrames) --- turn forward */
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(0.0f, qt);
	qt.setRotation('X',MATH_PI*-130/180);
	node->animQts.insert(0.5f, qt);
	qt.setRotation('X',MATH_PI*0/180);
	node->animQts.insert(1.0f, qt);

        /* 20. node[20]: right_forearm bone  */
        node=addChildNode(node, 0.8*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* 21. node[21]: right_hand bone  */
        node=addChildNode(node, 0.3*blen); /* node, boneLen */
        ang[0]=MATH_PI*0/180;
        node->pmat.setRotation('Y', ang[0]);
        node->pmat.setTranslation(0,0,node->parent->blen); /* <--- parent's blen */

        /* Finally: update NodePtrList to update Gmat list */
        updateNodePtrList();

/* TODO: To fix foot! default fix root */

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
	if(root==NULL) {
		egi_dpstd("root is NULL!\n");
		return;
	}

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
void E3D_BMatrixTree::resetTreeAmats()
{
	for(size_t k=0; k<nodePtrList.size(); k++)
		nodePtrList[k]->amat.identity();
}

/*------------------------------------------
Reset all amats as identity and update gmats
accordinly.
-------------------------------------------*/
void E3D_BMatrixTree::resetTreeOrients()
{
	resetTreeAmats();
	updateTreeGmats();
}

/*------------------------------------------
Insert t/quatrix to the node->aminQts[].
-------------------------------------------*/
void E3D_BMatrixTree::insertNodeAminQt(E3DS_BMTreeNode *node, float t, const E3D_Quatrix &qt)
{
	if(node==NULL)
		return;

	/* Insert to node E3D_AnimQuatrices animQts */
	node->animQts.insert(t, qt);
}

/*----------------------------------------------
Interpolate to update all amats with with given
time point t[0 1]
----------------------------------------------*/
void E3D_BMatrixTree::interpTreeAmats(float t)
{
	E3D_Quatrix qt;

	for(size_t k=0; k< nodePtrList.size(); k++) {
		qt.identity(); /* HK2022-11-08 */
		egi_dpstd("nodePtrList[%zu]->animQts.interp(t, qt)...\n",k);
		nodePtrList[k]->animQts.interp(t, qt); /* interpolate to get qt */
		qt.toMatrix(nodePtrList[k]->amat); /* qt to amat */
	}
}


/*----------------------------------------
interpNodeAmats()+updateTreeGmats()
-----------------------------------------*/
void E3D_BMatrixTree::interpTreeOrients(float t)
{
	interpTreeAmats(t);
	updateTreeGmats();
}



