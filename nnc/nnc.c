/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Glossary/Concept:

1. transfer/output/activation function
   devtransfer:derivative function of transfer function
2. loss/error/cost
	loss  --- Loss/cost for one learning sample.
	error --- All err values summed up for all learning samples.
3. gradient function
4. feedbackward/backpropagation
5. batch-learn and online-learn

dlrate:	learning rate


TODO:
1. Store and deploy model.
2. For multiply output, change mean loss function !!!!!

Note:
1. a single neural cell with N inputs:
	1. Input:	 x1,x2,x3,.....xn
	2. Weigths:	 w1,w2,w3,.....wn
	3. Bias:	 bias value
	4. sum up:	 u=x1*w1+x2*w2+x3*w3+....+xn*wn -bias
	5. transfer/activation function:   f(u)
	6. activation output:	h=f(u)
 	7. 7.1 For non_output layer: Composite loss is defined as loss=dE/du=derr*f'(u)  <this is after feedback calculation>
	       Before feedback calculation, derr=SUM(dE/du*w) where E,u,w are of its downstream cells.
	   7.2 For outpout layer: derr=dE/du=L'(h)*f'(u) <this is after feedback calculation>
	       Before feedback calculation, derr=L'(h),to calcluate L'(h) also need t--teacher value
	8. Final loss/err: err=L(h), L(h) is loss functions. to calcluate L'(h) also need t--teacher value.
	9. derr: for the current nvcell's dE/dh.

2. Sometimes the err increases monotonously and NERVER converge, it's
   necessary to observe such condition and reset weights and bias
   for all cells.

3. new_nvlayer() will also alloc nvcells inside, while new_nvnet() will NOT alloc nvlayers inside !!!

4. nvnet_feed_forward() will call nvlayer_mean_loss( nnet->nvlayers[nnet->nl-1], tv, loss_func ) for the output layer
   and return mean_loss value. If NOT output layer, then return 0.0.

5. For output layer, if it has transfunc defined, the it result should be stored in layer->douts, NOT in nvcell->dout!
   nvcell->dout stores u value, as nvcell->transfunc is NULL in this case.

Journal:
2023-05-18:
   1. Add func_softmax(NVLAYER *layer, int token)
   2. Add nvlayer_load_params()
   3. Add nvlayer_link_inputdata()
2023-06-15:
   1. Add float func_lossCrossEntropy()
2023-06-19:
   1. Add 'douts' for struct nerve_layer, for storing output layer transfunc results.
   2. nvlayer_mean_loss(): Consider softmax+crossEntropy, its easy to comput combined derivative.
   3. nvcell_feed_backward(): If nvcell has NULL transfunc, then f(u)=u and f'(u)=1.
2023-06-25:
   1. nvnet_mmtupdate_params(nnet, rate): add parameter 'double mfrict'.

Midas Zhou
midaszhou@yahoo.com <---- Not in use.
-----------------------------------------------------------------------*/
#include "nnc.h"
#include "actfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>


/* to define limit value for gradient checking */
#define GRADIENT_ABS_LIMIT	0.00000001	/* absolue small value limit */
#define GRADIENT_COMP_LIMIT	0.0001		/* compared/percentage small value limit */


static double dlrate=20.0;       	/* default value, learning rate for all nvcells */
static double desp_params=0.00001;	/* small change value for computing numerical gradients of params*/
static double dmmt_fric=0.75;		/* friction rate for momentum updating algorithm */


/*---------------------------------------------
 * set parameters for NNC
@learn_rate:	learn rate
@mfric:		parameter for momentum friction
---------------------------------------------*/
void  nnc_set_param(double learn_rate, double mfric)
{
	dlrate=learn_rate;
	dmmt_fric=mfric;
}

void nnc_set_learnrate(double learn_rate)
{
	dlrate=learn_rate;
}

void nnc_set_mfrict(double mfric)
{
	dmmt_fric=mfric;
}


///////////////////////////     Nerve Cell/Layer/Net Concept     ///////////////////////

/*-----------------------------------------------------------------------
 * Create a nueron with given .
 *
 * Params:
 * 	@nin 	number of input data
 * 	@incells 	array of input cells,from which we get input data(din),
 *			if NULL use din as input data, which will be realized only in transfer calculation.
 *	@din	array of input data, if NOT null, use as input data.
 * 	@dw  	array of weights, double. if NULL, init with 0.
 * 	@valve 	valve value.
 * 	@transfer 	transfer function.
 * Return:
 *	pointer to a NCELL ...  OK
 *	NULL		   ...  fails
-------------------------------------------------------------------------*/
NVCELL * new_nvcell( unsigned int nin, NVCELL * const *incells,
				double *din, double *dw, double bias, double (*transfer)(double, double,int ) )
{
	int i;

	/* check input param */
	if(nin==0 ) {
		printf("Init a new NCELL: input param 'nin' error.\n");
		return NULL;
	}

	/* calloc ncell->din and dw */
	NVCELL *ncell=calloc(1,sizeof(NVCELL));
	if(ncell==NULL) {
		printf("Init a new NCELL: fail to calloc ncell.\n");
		return NULL;
	}
	ncell->dw=calloc(nin,sizeof(double));
	if(ncell->dw==NULL) {
		printf("Init a new NCELL: fail to calloc dw.\n");
		free(ncell);
		return NULL;
	}

	/* assign din and dw, or leave it for later assignment  */
	if(din !=NULL) {
		ncell->din=din; /* pass only a pointer to din */
	}
	if(dw !=NULL) {
		for(i=0;i<nin;i++) {
			ncell->dw[i]=dw[i];
//			printf("dw[%d]=%f \n",i, ncell->dw[i]);
		}
	}

	/* assign incells, valve and tranfer function */
	ncell->nin=nin;
	ncell->incells=incells;
	ncell->dv=bias;
	ncell->dsum=0.0;
	ncell->transfunc=transfer;
	ncell->dout=0.0;

	return ncell;
}

/*--------------------------------------
 * Params:
 * 	@ncell	pointer to a nerve cell.
--------------------------------------*/
void free_nvcell(NVCELL *ncell)
{
	if(ncell==NULL) return;

	if(ncell->dw != NULL)
		free(ncell->dw);

	free(ncell);

	ncell=NULL;
}

/*--------------------------------------
 * Initialize dw and dv for a nvcell
 *
--------------------------------------*/
int nvcell_rand_dwv(NVCELL *ncell)
{
	int i;

	if(ncell==NULL || ncell->dw==NULL)
		return -1;

	/* random dw */
	for(i=0; i < ncell->nin; i++)
		ncell->dw[i]=random_btwone();

	/* random dv */
	ncell->dv=random_btwone();

	return 0;
}


/*---------------------------------------------------------------------
 * Note:
 *	1. Create a new nerve layer with nc nvcells inside.
 *	2. All nvcells of the nvlayer are copied from the template_cell
 *	   so they have same param set inside.
 * Params:
 * 	@nc	number of nerve cells in the layer;
 *	@template_cell	a template cell for layer->nvcells.
 *	@layerTransfuncDefined	If it's the output layer, and layer->transfunc defined/needed.
 * Return:
 *	pointer to NVLAYER	OK
 *	NULL			fails
--------------------------------------------------------------------*/
NVLAYER *new_nvlayer(unsigned int nc, const NVCELL *template_cell, bool layerTransfuncDefined)
{
	int i,j;

	/* check input param */
	if(nc==0 ) {
		printf("Init a new NVLAYER: input param 'nc' error.\n");
		return NULL;
	}

	if( template_cell == NULL ) {
		printf("Init a new NVLAYER: template cell is NULL.\n");
		return NULL;
	}

	/* calloc NVLAYER */
	NVLAYER *layer=calloc(1,sizeof(NVLAYER));
	if(layer==NULL) {
		printf("Init a new NVLAYER: fail to calloc nvlayer!\n");
		return NULL;
	}

	/* calloc NVCELL* */
	layer->nvcells=calloc(nc, sizeof(NVCELL *));
	if(layer->nvcells==NULL) {
		free(layer);
		printf("Init a new NVLAYER: fail to calloc layer->nvcells!\n");
		return NULL;
	}

	/* calloc douts HK2023-06-19 */
	if(layerTransfuncDefined) {
		layer->douts=calloc(nc, sizeof(typeof(*layer->douts)));
		if(layer->douts==NULL) {
			free(layer->nvcells);
			free(layer);
			printf("Init a new NVLAYER: fail to calloc layer->douts!\n");
			return NULL;
		}
	}

	/* assign nc */
	layer->nc=nc;

	/* create nvcells as per template_cell */
	for(i=0;i<nc;i++) {
		layer->nvcells[i]=new_nvcell( template_cell->nin, template_cell->incells,
					      template_cell->din,template_cell->dw, template_cell->dv,
					      template_cell->transfunc
					);
		/* if fail, release all */
		if(layer->nvcells[i]==NULL) {
				printf("Init a new NVLAYER: fail to create new_nvcell!.\n");
				for(j=0;j<i;j++)
					free_nvcell(layer->nvcells[j]);

				free(layer->nvcells);
				free(layer);

				return NULL;
		}
	}

	return layer;
}


/*--------------------------------------
 * Params:
 * 	@layer	pointer to a nerve layer.
---------------------------------------*/
void free_nvlayer(NVLAYER *layer)
{
	int i;

	if(layer==NULL)
		return;

	if(layer->nvcells==NULL) {
		free(layer);
		layer=NULL;

		return;
	}

	/* else if nvcells is not NULL */
	for(i=0; i < layer->nc; i++) {
		if( layer->nvcells[i] != NULL)
			free_nvcell(layer->nvcells[i]);
	}


	free(layer->douts); /* HK2023-06-30 */
	free(layer->nvcells);
	free(layer);
	layer=NULL;
}


/*-----------------------------------------------
 * Create a NVNET and all nvlayers inside
 * Params:
 * 	@nl	number of nvlayers in the nvnet.
------------------------------------------------*/
NVNET *new_nvnet(unsigned int nl)
{
	/* check input param */
	if(nl==0 ) {
		printf("Init a new NVNET: input param 'nl' error.\n");
		return NULL;
	}

	/* calloc NVNET */
	NVNET *nnet=calloc(1,sizeof(NVNET));
	if(nnet==NULL) {
		printf("Init a new NVNET: fail to calloc nnet!\n");
		return NULL;
	}

	/* calloc nnet->nvlayers */
	nnet->nvlayers=calloc(nl, sizeof(NVLAYER *));
	if(nnet->nvlayers==NULL) {
		free(nnet);
		printf("Init a new NVNET: fail to calloc nnet->nvlayer!\n");
		return NULL;
	}

	/* assign nl */
	nnet->nl=nl;

	return nnet;
}

/*---------------------------------------------
 * Free a NVNET with array of nvlayers inside
 * Params:
 * 	@nnet	pointer to a nerve layer.
---------------------------------------------*/
void free_nvnet(NVNET *nnet)
{
	int i;

	if(nnet==NULL)
		return;

	if(nnet->nvlayers==NULL) {
		free(nnet);
		nnet=NULL;
		return;
	}

	/* free params buff */
	if(nnet->params != NULL) {
		free(nnet->params);
		nnet->params=NULL;
	}

	/* free mmts buff */
	if(nnet->mmts != NULL) {
		free(nnet->mmts);
		nnet->mmts=NULL;
	}

	/* free nvlayers inside */
	for(i=0;i < nnet->nl; i++) {
		if(nnet->nvlayers[i] != NULL)
			free_nvlayer(nnet->nvlayers[i]);
	}

	free(nnet->nvlayers);
	free(nnet);
	nnet=NULL;
}


///////////////////////////     Nerve Cell/Layer/Net Functions     ///////////////////////

/*----------------------------------------------
 * Note:
 *	A feed forward function for a nerve cell.
 * Params:
 * 	@nvcell		a nerve cell;
 * Return:
 *		0	OK
 *		<0	fails
-----------------------------------------------*/
int nvcell_feed_forward(NVCELL *nvcell)
{
	int i;

	/* check input param. OK nvcell MAY be NULL, see also 3.  HK2023-05-18 */
	if(nvcell==NULL )  // || nvcell->transfunc==NULL)
		return -1;

	/* 1. reset dsum and derr every time before feedforward */
	nvcell->dsum=0;
//	nvcell->derr=0; /* put this in nvnet_feed_back() */

	/* 2. Calculate sum of Xn*Wn */
	/* 2.1 Get input from a data array */
	if(nvcell->din != NULL) {
		for(i=0; i < nvcell->nin; i++) {
			nvcell->dsum += (nvcell->din[i]) * (nvcell->dw[i]);
		}
		/* applay dv */
		nvcell->dsum -= nvcell->dv;
	}
	/* 2.2 OR, get data from ahead nvcells' output */
	else if( nvcell->incells !=NULL ) {
		/* check data */
		if(nvcell->incells[0]==NULL) {
			printf("%s: nvcell->incells[x] unavailable! \n",__func__);
			return -2;
		}
		for(i=0; i < nvcell->nin; i++) {
			nvcell->dsum += (nvcell->incells[i]->dout) * (nvcell->dw[i]);
		}
		/* applay dv */
		nvcell->dsum -= nvcell->dv;
	}
	/* 2.3 OR, input data unavailable ! */
	else {
		printf("%s: input data unavailable!\n",__func__);
		return -3;
	}
//	printf(" dsum=%f, dv=%f \n",nvcell->dsum, nvcell->dv);

	/* 3. Calculate output with transfer/activation function */
	if(nvcell->transfunc)  /* HK2023-05-18 */
	     nvcell->dout=(*nvcell->transfunc)(nvcell->dsum, 0, NORMAL_FUNC);  /* f=0 */
	else  { /* No transfunc */
	    /* Example: at output layer, softMax applys on output layer->transfunc at nvlayer_feed_forward() */
	    nvcell->dout=nvcell->dsum;
	}

	return 0;
}


/*-------------------------------------------------------------------
 * Note:
 *	Calculate loss value from output cells.
 *      This func is to be called after each feed_forward calculaton.
 *
 * Params:
 * 	@nvcell		output nerve cells;
 *	@tv		array of target value;
 *
 * Return:	loss/err value of the last feed_forward calculation.
--------------------------------------------------------------------*/
double nvcell_calc_loss(NVCELL *outcells, const double *tv,
			double (*loss)(double out, const double tv) )
{

	/* check input param */
	if( outcells==NULL || loss==NULL || tv==NULL ) {
		printf("%s: input params invalid! \n",__func__);
		return 999999.9;
	}

	return loss(outcells->dout,*tv);
}


/*------------------------------------------------------
 * Note:
 *	1. A feed_backward/backpropagation function for a nerve cell.
 *	2. !!!Assume that the nvcell is well confiured and prepared,
 *	   and with result of last feed_forward calculation.
 * Params:
 * 	@nvcell		a nerve cell;
 *	@tv		a teacher's value of output nvcells;
 *			NULL for non_output nvcells
 * Return:
 *		0	OK
 *		<0	fails
--------------------------------------------------------*/
int nvcell_feed_backward(NVCELL *nvcell)
{
	int i;

	/* check input param */
	if(nvcell==NULL) // || nvcell->transfunc==NULL)
		return -1;


        /* 1. calculate composite dE/du */
	/* if it's output nvcell */
//	if(tv !=NULL )
// 	{
  /* ----P1: LOSS COMPOSITE for output nvcell : loss composite=dE/du=L'(h)*f'(u)
          after nvlayer_mean_loss() we get derr=L'(h). so derr=derr*f'(u):  */
//		nvcell->derr *= nvcell->transfunc(nvcell->dsum, nvcell->dout, DERIVATIVE_FUNC);
//	}
	/* else if it's non_output nvcell */
//	else
//	{
  /* ----P2: LOSS COMPOSITE for NON_output nvcell : loss composite=dE/du=derr*f'(u)=SUM(dE/du[L+1]*w[L+1])*f'(u);
         here,derr=SUM(dE/du[L+1]*w[L+1]) where E,u,w are of downstream cells,
	 assume that above derr has already been feeded backed from the next layer cells */
	if(*nvcell->transfunc)  /* HK2023-06-19 */
		nvcell->derr *= (*nvcell->transfunc)(nvcell->dsum, nvcell->dout, DERIVATIVE_FUNC);
	//else  /* No transfunc: f(u)=u, f'(u)=1.  Example: for the output layer, softMax+crossEntropy  */
	//	nvcell->derr *=1.0;
//	}

//	printf("%s,--- update dw and v --- \n", __func__ );

	/* 2. update weight by learning
	   For output cells:      dw += -rate*L'(h)*f'(u)*h[L-1], assume derr=L'(h)*f'(u) already
	   For non_output cells:  dw += -rate*dE/du*h[L-1],  derr=dE/du as in P1/P2.
	 */
	/* 2.1 If NOT input nvcells,  feedback to upstream cells */
	if( nvcell->incells !=NULL && nvcell->incells[0] != NULL)  {
		/* update dw */
		for(i=0; i< nvcell->nin; i++) {

		/* 3. feed back loss to previous nvcell, just take advantage of this for() loop */
   /* ---- LOSS BACKP_ROPAGATION FUNCTION : incell[x]_derr = SUM(dw*derr), sum of all next layer feedback error */
			/* feedback through dw[] to its corresponding upstream cells */
			nvcell->incells[i]->derr += (nvcell->dw[i])*(nvcell->derr);

#if 0 /* move to nvnet_update_params() */
			nvcell->dw[i] += -dlrate*(nvcell->incells[i]->dout)*(nvcell->derr); /* already put previout output in din[] */
#endif

		}
	}

#if 0 /* move to nvnet_update_params() */
	/* 2.2 ELSE: assume its an input nvcell, get din directly. */
	else if(nvcell->din !=NULL) {
		/* update dw */
		for(i=0; i< nvcell->nin; i++) {
			/* here derr=SUM(dE/du[L+1]*w[L+1])*f'(u) alread */
			nvcell->dw[i] += -dlrate*(nvcell->din[i])*(nvcell->derr); /* already put previout output in din[] */
		}
	}
	/* 2.3 Data error */
	else {
		printf("%s: nvcell->incells[x] or din[x] invalid!\n",__func__);
		return -2;
	}
#endif


#if 0 /* move to nvnet_update_params() */
	/* 4. update bias value by learning */
	nvcell->dv += -dlrate*(-1.0)*(nvcell->derr); /* bias deemed as a special kind of weight, with din[x]=-1 */
#endif

	return 0;
}



/*-----------------------------------------------------
 * Load weights/bias value for each nvcell in the layer
 *
 *		       !!! ---- CAUTION ---- !!!
 *        The caller MUST ensure enough data in weights/bias,
 *	  to avoid mem access error!
 ^
 * Params:
 * 	@layer:	   a nerve layer;
 *	@weights:  array of weights values
 *		   In order of nvcells:
		   Wc1,1 Wc1,2 ... Wc1,nin  Wc2,1 Wc2,2 ... Wc2,nin Wc3,1 Wc3,2 ... (total nin*nc)
 *      @bias:     array of bias values.
 		   bc1  bc2  bc3 ... (total nc)
 *
 * Return:
 *		0	OK
 *		<0	fails
-----------------------------------------------------*/
int nvlayer_load_params(NVLAYER *layer, double *weights, double *bias)
{
	int i,j;
	int cnt;

	if(layer==NULL || layer->nvcells==NULL)
		return -1;

        if(weights) {
	   cnt=0;
	   for(i=0; i< layer->nc; i++) {
		for(j=0; j< layer->nvcells[i]->nin; j++)
			layer->nvcells[i]->dw[j]=weights[cnt++];
	   }
	}

        if(bias) {
	   cnt=0;
	   for(i=0; i< layer->nc; i++) {
		layer->nvcells[i]->dv=bias[cnt++];
	   }
	}


	return 0;
}


/*-----------------------------------------------------
 * Link data array to each nvcell.din in the layer.
 *
 *	          !!! ---- CAUTION ---- !!!
 * The caller MUST ensure enough data to avoid mem access error!
 *
 * Params:
 * 	@layer:	   a nerve layer;
 *	@data:     Data array as input data for nvcell.din
 *
 * Return:
 *		0	OK
 *		<0	fails
-----------------------------------------------------*/
int nvlayer_link_inputdata(NVLAYER *layer, double *data)
{
	int i;

	if(layer==NULL || layer->nvcells==NULL)
		return -1;

        for(i=0; i< layer->nc; i++) {
		/* Link data to each nvcell.din */
		layer->nvcells[i]->din=data;
        }

	return 0;
}

/*----------------------------------------------
 * Note:
 *	A feed forward function for a nerve layer.
 * Params:
 * 	@layer	a nerve layer;
 * Return:
 *		0	OK
 *		<0	fails
-----------------------------------------------*/
int nvlayer_feed_forward(NVLAYER *layer)
{
	int i;
	int ret;

	/* check layer */
	if(layer==NULL || layer->nvcells==NULL)
		return -1;

	/* feed forward all nvcells in the layer */
	for(i=0; i< layer->nc; i++) {
		ret=nvcell_feed_forward(layer->nvcells[i]);
		if( ret !=0 ) return ret;
	}

	/* Apply nvlayer transfer function.  Example: output layer softmax  HK2023-05-18 */
	if( layer->transfunc !=NULL ) {
		ret=(*layer->transfunc)(layer, NORMAL_FUNC);
		if( ret!=0 ) return ret;
	}

	return 0;
}


/*-----------------------------------------------------------------------------
 *	WARN: The loss function MUST be a kind of MEAN loss type !!!!
 * 	Example: (1/N)*SUM[(tv-yi)^2], (1/N)*SUM[ ] is calculated in this func.
 *
 * Note:
 *  1.  Calculate MEAN loss value from output nerve cells. It calculates
 *	each output nvcells with dout and tv, then use number of nvcells
 *	to get a mean loss value.
 *  2.  It alsao calculate derr=L'(h)*f'(u)
 *  3.  This func MUST be called after each feed_forward calculaton.
 *  4.  Make sure that size of array tv is outlayer->nc.
 *
 * Params:
 * 	@outlayer	output nerve layer;
 *	@tv		array of teacher's value.
 *	@loss		MUST be a kind of MEAN loss function !!!
 *
 * Return:
 *		0	OK
 *		<0	fails
------------------------------------------------------------------------------*/
double nvlayer_mean_loss(NVLAYER *outlayer, const double *tv,
			double (*loss_func)(double out, const double tv, int token) )
{
	int i;
	//double err=0.0;
	double loss=0.0;
//	double mean_loss=0.0;

	/* check input param */
	if(outlayer==NULL || outlayer->nvcells==NULL|| tv==NULL ) {
		printf("%s: input params invalid! \n",__func__);
		return 999999.9;
	}
	else if(loss_func==NULL) {
		printf("%s: Loss function NOT defined! \n",__func__);
		return 999999.9;
	}

	/* 1. For softMax+lossCrossEntropy: softMas as outlayer transfer func.  HK2023-06-19 */
	if(loss_func==func_lossCrossEntropy ) {
	    if(outlayer->transfunc == func_softmax) {
           	/* for each output nvcell */
           	for(i=0; i< outlayer->nc; i++) {
			/* sum up each loss, Noticed: softMax()  results stored in outlayer->douts! */
			//loss += loss_func(outlayer->nvcells[i]->dout, tv[i], NORMAL_FUNC);
			loss += loss_func(outlayer->douts[i], tv[i], NORMAL_FUNC);

			/* Here we only calc L'(h), and put it in nvcell->derr,
                 	 * later in nvcell_feed_backward() : derr=L'(h)*f'(u)=derr*f'(u) as dE/du.dE/dh*dh/du
                 	 */
			/* For combined function cf(ui)=softmax+crossEntropy: cf'(u)= y[i]-tv[i]  */
			outlayer->nvcells[i]->derr = outlayer->douts[i]-tv[i]; /* Notice: not in nvcells[]->dout!!! */
		}
	    }
	    else {
			printf("%s: CrossEntropy MUST combine with sotfMax! \n",__func__);
			return 999999.9;
	    }
	}
	/* 2. Non_combine functions */
	else {

	   /* for each output nvcell */
	   for(i=0; i< outlayer->nc; i++) {
		/* sum up each loss */
		loss += loss_func(outlayer->nvcells[i]->dout, tv[i], NORMAL_FUNC);

		/* Here we only calc L'(h), and put it in nvcell->derr,
                 * later in nvcell_feed_backward() : derr=L'(h)*f'(u)=derr*f'(u) as dE/du.dE/dh*dh/du
                 */
		outlayer->nvcells[i]->derr = loss_func(outlayer->nvcells[i]->dout, tv[i], DERIVATIVE_FUNC);
	    }
	}

	/* get mean loss */
//	printf("%s: loss=%f\n", __func__, loss);
	return loss/(outlayer->nc);
}


/*----------------------------------------------
 * Note:
 *	A feed forward function for a nerve layer.
 * Params:
 * 	@layer	a nerve layer;
 *	@tv		a teacher's value of output nvcells;
 *			NULL for non_output nvcells
 * Return:
 *		0	OK
 *		<0	fails
-----------------------------------------------*/
int nvlayer_feed_backward(NVLAYER *layer)
{
	int i;
	int ret;

	/* check layer */
	if(layer==NULL || layer->nvcells==NULL)
		return -1;

	/* feed backward all nvcells in the layer */
	for(i=0; i< layer->nc; i++) {
		ret=nvcell_feed_backward(layer->nvcells[i]);
		if(ret !=0) return ret;
	}

	return 0;
}


/*-----------------------------------------
 * A feed forward function for a nerve NET.
 * Params:
 * 	@nnet		nerve net
 * Return:
 *		0	OK
 *		<0	fails
-----------------------------------------*/
int nvnet_init_params(NVNET *nnet)
{
	int i,j;
	NVCELL *cell;

	if( nnet==NULL || nnet->nl==0)
		return -1;

	/* rand dw[] and dv */
	for(i=0; i< nnet->nl; i++) {
		for(j=0; j< nnet->nvlayers[i]->nc; j++) {
			cell=nnet->nvlayers[i]->nvcells[j];
			if( nvcell_rand_dwv(cell) <0 ) /* rand dw dv */
				return -2;
			cell->dsum=0.0;
			cell->dout=0.0;
			cell->derr=0.0;
		}
	}

	return 0;
}


/*-----------------------------------------
 * A feed forward function for a nerve NET.
 * Params:
 * 	@nnet		nerve net
 *	@tv		array of teach value
 *	@loss_func	loss function
 *
 * Return:
 *		others 	OK
 *		a big value	fails
-----------------------------------------*/
double nvnet_feed_forward(NVNET *nnet, const double *tv,
			double (*loss_func)(double, const double, int) )
{
	int i;

	if( nnet==NULL || nnet->nl==0)
		return 999999.9;

	for(i=0; i< nnet->nl; i++) {
//		printf("%s: nvlayers[%d] feed forward...\n",__func__, i);
		nvlayer_feed_forward(nnet->nvlayers[i]);
	}

        /* Get final err */
	if(tv) {
         	return  nvlayer_mean_loss( nnet->nvlayers[nnet->nl-1], tv, loss_func );
	}
	else /* HK2023-05-17 */
		return 0.0f;
}


/*-----------------------------------------
 * A feed backward function for a nerve NET.
 * Params:
 * 	@nnet		nerve net
 * Return:
 *		0	OK
 *		<0	fails
-----------------------------------------*/
int nvnet_feed_backward(NVNET *nnet)
{
	int i,j;

	if( nnet==NULL || nnet->nl==0)
		return -1;

	/* clear derr in all cells,except the output cells by  */
	for(i=0; i< nnet->nl-1; i++) {  /*  ----- CAUTION ----  <nl-1 */
		/* but leave the output layer, as we already put derr=L'(h) there !!! */
		for(j=0; j< nnet->nvlayers[i]->nc; j++) {
			nnet->nvlayers[i]->nvcells[j]->derr=0.0;
		}
	}

	/* feed backward from output layer to input layer */
	for(i=nnet->nl-1; i>=0; i--) {
//		printf("%s: nvlayers[%d] feed_backward...\n", __func__, i);
		if(nvlayer_feed_backward(nnet->nvlayers[i]) !=0 ) {
			printf("%s: nvlayer[%d] feed backward fails!\n",__func__,i);
			return -2;
		}
	}

	return 0;
}



/*------------------------------------------------------------------------------
 *  Update all cells' params of a nvnet by simple gradient descent algorithm.
 *  For output cells:      dw += -rate*L'(h)*f'(u)*h[L-1],
 *       assume derr=L'(h)*f'(u) already computed by nvnet_feed_backward().
 *  For non_output cells:  dw += -rate*dE/du*h[L-1],
 *       assume derr=dE/du=f'(h)*SUM(dE/du*w)[L-1] already computed by nvnet_feed_backward().
 *
 * Params:
 * 	@nnet		nerve net
 *	@rate		learning rate
 * Return:
 *		0	OK
 *		<0	fails
---------------------------------------------------------------------------*/
int nvnet_update_params(NVNET *nnet, double rate)
{
	int i,j,k;
	NVCELL *cell;

	if( nnet==NULL || nnet->nl==0)
		return -1;

	for(i=0; i< nnet->nl; i++) {			/* traverse nvlayers */
	   for(j=0; j< nnet->nvlayers[i]->nc; j++) {	/* traverse nvcells */
	      cell=nnet->nvlayers[i]->nvcells[j];

	      /* 1. update dw[] += -LEARN_RATE*h[L-1]*derr */
	      for(k=0; k< cell->nin; k++)  {		/* traverse params */

		 if( cell->incells !=NULL && cell->incells[0] != NULL) {
			cell->dw[k] += -rate*(cell->incells[k]->dout)*(cell->derr);
		 }
		 else if( cell->din !=NULL ) {
			cell->dw[k] += -rate*(cell->din[k])*(cell->derr);
		 }
	 	 else {
			printf("%s: nvcell->incells[x] or din[x] invalid!\n",__func__);
			return -2;
		 }

	      }

	      /* 2. bias deemed as a special kind of weight, dv += -LEARN_RATE*dE/db,
	       *	dE/db=-f'(u)
               */
	      cell->dv += rate*(cell->derr); /* -rate*(-1.0)*(cell->derr) */

           }
	}

	return 0;
}



/*------------------------------------------------------------------------------
 *  Update all cells' params of a nvnet by momentum algorithm.
 *  For output cells:      dw += -rate*L'(h)*f'(u)*h[L-1],
 *       assume derr=L'(h)*f'(u) already computed by nvnet_feed_backward().
 *  For non_output cells:  dw += -rate*dE/du*h[L-1],
 *       assume derr=dE/du=f'(h)*SUM(dE/du*w)[L-1] already computed by nvnet_feed_backward().
 *
 *  Parameters updated by Momentum:
 *	M(i+1) = f*M(i)-r*dE/dP
 *      P(i+1) = P(i) + M(i+1)
 *
 *     where    M: momentum value; P: parameters, weigts, bias etc.
 *		f: friction;  r: learning rate;  dE/dP: dE/dw, dE/db ..etc.
 *
 * Params:
 * 	@nnet		nerve net
 *	@rate		learning rate
 *	@mfrict:	momentum friction parameter
 * Return:
 *		0	OK
 *		<0	fails
---------------------------------------------------------------------------*/
int nvnet_mmtupdate_params(NVNET *nnet, double rate, double mfrict)
{
	int i,j,k;
	int nmp;
	NVCELL *cell;

	if( nnet==NULL || nnet->nl==0)
		return -1;

	/* init nnet->mmts if NULL */
	if( nnet->mmts == NULL ) {
		nmp=0;
		/* get total number of mmts params */
		for(i=0; i< nnet->nl; i++) {	/* layers in the nvnet */
			for(j=0; j < nnet->nvlayers[i]->nc; j++) {	    /* cells in a layer */
				nmp += (nnet->nvlayers[i]->nvcells[j]->nin)+1; /* dw[], dv */
			}
		}
		/* allocate nnet->mmts and set all zero */
		nnet->mmts = calloc(nmp, sizeof(double));
		if(nnet->mmts==NULL) {
			printf("%s: fail to calloc nnet->mmts.\n",__func__);
			return -2;
		}

		nnet->nmp=nmp;
	}

	/* update params by momentum */
	nmp=0;
	for(i=0; i< nnet->nl; i++) {			/* traverse nvlayers */
	   for(j=0; j< nnet->nvlayers[i]->nc; j++) {	/* traverse nvcells */
	      /* For each neuron in the network */
	      cell=nnet->nvlayers[i]->nvcells[j];

	      /* 1. update dw[]  */
	      for(k=0; k< cell->nin; k++)  {		/* traverse params */
		if( cell->incells !=NULL && cell->incells[0] != NULL) {
			/* update momentum dw_mmt[i+1]=fric*dw_mmt[i]-rate*h[L-1]*derr
			   Notice: dE/dwi=dE/du*du/dwi=dE/du*xi=dE/du*h[L-1]=derr*h[L-1]
			 */
			//nnet->mmts[nmp]= dmmt_fric*nnet->mmts[nmp]-rate*(cell->incells[k]->dout)*(cell->derr);
			nnet->mmts[nmp]= mfrict*nnet->mmts[nmp]-rate*(cell->incells[k]->dout)*(cell->derr);
			/* update dw, dw[i+1]=dw[i]+dw_mmt[i+i] */
			cell->dw[k] += nnet->mmts[nmp];

			nmp++;
		}
		else if( cell->din !=NULL ) {
			//nnet->mmts[nmp]= dmmt_fric*nnet->mmts[nmp]-rate*(cell->din[k])*(cell->derr);
			nnet->mmts[nmp]= mfrict*nnet->mmts[nmp]-rate*(cell->din[k])*(cell->derr);
			cell->dw[k] += -rate*(cell->din[k])*(cell->derr);

			nmp++;
		}
		else {
			printf("%s: nvcell->incells[x] or din[x] invalid!\n",__func__);
			return -2;
		}
	     }  /* end of dw[] nin */

		/* 2. update dv, dv_mmt[i+1]=fric*dv_mmt[i]-rate*(-1.0)*derr */
		//nnet->mmts[nmp]= dmmt_fric*nnet->mmts[nmp]+rate*(cell->derr); /* -rate*(-1.0)*(cell->derr) */
		nnet->mmts[nmp]= mfrict*nnet->mmts[nmp]+rate*(cell->derr); /* -rate*(-1.0)*(cell->derr) */
		/* update dv, dv[i+1]=dv[i]+dv_mmt[i+i] */
		cell->dv += nnet->mmts[nmp];

		nmp++;
           }
	}

	return 0;
}


/*---------------------------------------------
 * Buff current params into nvnet->params.
 * params are buffed in order: dw[],dv,dsum,dout,derr.
 *
 * the net. all to be double type.
 *
 * Params:
 * 	@nnet		nerve net
 * Return:
 *		0	OK
 *		<0	fails
-----------------------------------------------*/
int nvnet_buff_params(NVNET *nnet)
{
	int i,j,k;
	int np; /* total numbers of params */
	NVCELL *cell;

	if( nnet==NULL || nnet->nl==0)
		return -1;

	/* if NULL, allocate nnet->params */
	if(nnet->params==NULL) {
		np=0;
		for(i=0; i< nnet->nl; i++) {	/* layers in the nvnet */
			for(j=0; j < nnet->nvlayers[i]->nc; j++) {	  /* cells in a layer */
				cell=nnet->nvlayers[i]->nvcells[j];
				np += cell->nin + 4; /* dw[], dv, dsum, dout, derr */
			}
		}

		nnet->params=calloc(np,sizeof(double));
		if(nnet->params==NULL) {
			printf("%s: fail to calloc nnet->params.\n",__func__);
			return -1;
		}

		nnet->np=np;
		printf("%s: space for %d double type params are allocated to nvnet->param.\n",__func__, np);
	}

	/* buff all params to nnet->params */
	np=0;
	for(i=0; i<nnet->nl; i++) {				  /* layers in the nvnet */
		for(j=0; j < nnet->nvlayers[i]->nc; j++) {	  /* cells in a layer */

			cell=nnet->nvlayers[i]->nvcells[j];
			/* buff dw[] */
			for(k=0; k< cell->nin; k++) {
				nnet->params[np++]=cell->dw[k];
			}
			/* buff dv, dsum, dout, derr; beware of the their order */
			nnet->params[np++]=cell->dv;
			nnet->params[np++]=cell->dsum;
			nnet->params[np++]=cell->dout;
			nnet->params[np++]=cell->derr;
		}
	}
//	printf("%s: %d params buff into nvnet->params.\n",__func__, np);

	return 0;
}


/*----------------------------------------------------
 * Restore params in nvnet->buff to its cells
 * params are buffed in order: dw[],dv,dsum,dout,derr.
 * the net. all to be double type.
 *
 * Params:
 * 	@nnet		nerve net
 * Return:
 *		0	OK
 *		<0	fails
-----------------------------------------------------*/
int nvnet_restore_params(NVNET *nnet)
{
	int i,j,k;
	int np; /* total numbers of params */
	NVCELL *cell;

	if( nnet==NULL || nnet->params==NULL || nnet->nl==0 )
		return -1;


	/* put all params to nnet->params */
	np=0;
	for(i=0; i< nnet->nl; i++) {				  /* layers in the nvnet */
		for(j=0; j < nnet->nvlayers[i]->nc; j++) {	  /* cells in a layer */

			cell=nnet->nvlayers[i]->nvcells[j];
			/* restore dw[] */
			for(k=0; k< cell->nin; k++) {
				cell->dw[k]=nnet->params[np++];
			}
			/* buff dv, dsum, dout, derr; beware of the their order */
			cell->dv=nnet->params[np++];
			cell->dsum=nnet->params[np++];
			cell->dout=nnet->params[np++];
			cell->derr=nnet->params[np++];
		}
	}

//	printf("%s: %d params in nvnet->params restored into cells.\n",__func__, np);

	return 0;
}


/*----------------------------------------------------------
 * 1. Check gradient of nnet after backpropagation computation
 *    and before updating params.
 *    Check numerical gradient of dE/dw and dE/dv with the
 *    backpropagation gradient respectively.
 * 2. Call this function just after feedback computation!!!
 *    that all cells can get its right params, including dout, derr etc.
 * 3. Input and teacher data for gradient checking shall be
 *    the same set as in the previous feedback computation. !!??
 *
 * Params:
 * 	@nnet		nerve net
 *	@tv		array of teacher value
 *	@loss_func	loss function
 * Return:
 *		0	OK
 *		<0	fails
---------------------------------------------------------*/
int nvnet_check_gradient(NVNET *nnet, const double *tv,
			double (*loss_func)(double, const double, int) )
{
	int i,j,k;
	NVCELL *cell;
	double  dgrt_back; /* backpropagation gradient */
	double  dgrt_num;  /* numerical gradient */
	double  err_plus; /* err result for dw plus samll changes */
	double  err_minus; /* err result for dw minus samll changes */

	if( nnet==NULL || nnet->nl==0 )
		return -1;

	/* buff all params */
	nvnet_buff_params(nnet);

	/* check gradient for each parameter */
	for(i=0; i< nnet->nl; i++) {					  /* traverse nvlayers */
	    printf("\nCheck gradient for nvlayer[%d]:\n",i);

	    for(j=0; j< nnet->nvlayers[i]->nc; j++) {			  /* traverse nvcells */
		cell=nnet->nvlayers[i]->nvcells[j];

		/* 1. dw[]---check dw[] gradient in the cell */
		for(k=0; k< cell->nin; k++) {

			/* 1.1 get dgrt_back: dE/dw=h^(L-1)*derr */
			nvnet_restore_params(nnet); /* restore params before calculate dgrt_back */
			if(i==0) {  //i==0 for input layer OR cell->din != NULL) { /* For input cells */
				dgrt_back=cell->derr*(cell->din[k]);
			}
			else if(cell->incells[k] != NULL) {	/* For non_input layers */
				dgrt_back=cell->derr*(cell->incells[k]->dout);
			}
			else
				return -2;		/* Fail */

			/* 1.2 calculate err_plus, with restored  params */
			cell->dw[k] += desp_params; /* plus a small change value */
			err_plus= nvnet_feed_forward(nnet, tv, loss_func);

			/* 1.3 restore params again and compute error with minus param */
			nvnet_restore_params(nnet);
			cell->dw[k] -= desp_params; /* minus a small change value */
			err_minus= nvnet_feed_forward(nnet, tv, loss_func);

			/* 1.4 get numerical gradient */
			dgrt_num=(err_plus-err_minus)/(2.0*desp_params);

			/* 1.5 compare results */
			printf("dw[%d]: dgrt_back=%1.10f,  dgrt_num=%1.10f. \n",k,dgrt_back,dgrt_num);
#if 1
			if( gradient_isclose(dgrt_num,dgrt_back)==false ) {
//			if( gradient_isclose(dgrt_num,dgrt_back)==false && cell->din==NULL) {
			   printf("Gradient check fails for dw[%d]: dgrt_back=%1.10f,  dgrt_num=%1.10f. \n",
									k,dgrt_back,dgrt_num);
				return -1;
			}
#endif
		}

	/* 2. dv---check dv gradient in the cell */
		/* 2.1 dv----get dgrt_back: dE/dv=-1.0*derr */
		nvnet_restore_params(nnet);
		dgrt_back=-1.0*cell->derr;

		cell->dv += desp_params; /* plus a small change value */
		err_plus= nvnet_feed_forward(nnet, tv, loss_func);

		/* 2.2 restore params and compute error with minus param */
		nvnet_restore_params(nnet);
		cell->dv -= desp_params; /* minus a small change value */
		err_minus= nvnet_feed_forward(nnet, tv, loss_func);

		/* 2.3 numerical gradient */
		dgrt_num=(err_plus-err_minus)/(2.0*desp_params);

		/* compare results */
		printf("dv: dgrt_back=%1.10f,  dgrt_num=%1.10f. \n",dgrt_back, dgrt_num);
#if 1
//		if( gradient_isclose(dgrt_num, dgrt_back)==false && cell->din==NULL) {
		if( gradient_isclose(dgrt_num,dgrt_back)==false ) {
  		         printf("Gradient check fails for dw[%d]: dgrt_back=%1.10f,  dgrt_num=%1.10f. \n",
									k,dgrt_back,dgrt_num);
			return -1;
		}
#endif

	    }
	}

	/* restore params at last */
	printf("\n	-------<<<   gradients checking passed!  >>>------- \n");
	nvnet_restore_params(nnet);

	return 0;
}



/*------------------------------------------------
 * Params:
 * 	@nnet	a well prepared/confiured nerve net;
 *		with input data in din.
 *	@tv	teacher's value
 * Return:
 *		0	OK
 *		<0	fails
------------------------------------------------*/
int nvcell_get_loss(NVNET *nnet, double tv)
{

	return 0;
}


/*------------------------------------------------------
 * Pass a pointer of data array to nvcell->din.
 * Params:
 * 	@cell		a nerve cell;
 *	@data		array of input data for cell.din
 * Return:
 *		0	OK
 *		<0	fails
-------------------------------------------------------*/
int  nvcell_input_data(NVCELL *cell, double *data)
{
	/* check input */
	if( cell==NULL || data==NULL )
		return -1;

//	if( cell->din !=NULL) {
//		printf("%s: WARNING!  nvcell->din is NOT empty!\n",__func__);
//	}

	/* set input data */
	cell->din = data;

	return 0;
}


///////////////////////////    Common Math     ///////////////////////

/*----------------------------------------------------
 * Generate a random double between -1 to 1 for dw[]
----------------------------------------------------*/
double random_btwone(void)
{
	double rnd;
        struct timeval tmval;

	/* rand seeds */
        gettimeofday(&tmval,NULL);
        srand(tmval.tv_usec);

	while( (rnd=(double)rand()/RAND_MAX) == 1.0 );
	rnd=rnd*2-1.0;

	return rnd;
}



////////////////////////      print params     //////////////////////////

/*------------------------------------------------------
 * Note:
 *	1. Print dw and dv of a nvcell
 * Params:
 * 	@nvcell		a nerve cell;
 *
--------------------------------------------------------*/
void nvcell_print_params(const NVCELL *nvcell)
{
	int i;

	if(nvcell==NULL)return;

	printf("	  dw[%d]: ",nvcell->nin);
	for( i=0; i < nvcell->nin; i++ ) {
		printf("  %f",nvcell->dw[i]);
	}
	printf("   dv: %f   \n",nvcell->dv);

}

/*------------------------------------------------------
 * Note:
 *	1. Print dw and dv of  nvcells in a layer.
 * Params:
 * 	@nvcell		a nerve cell;
 *
--------------------------------------------------------*/
void nvlayer_print_params(const NVLAYER *layer)
{
	int k;

	if(layer==NULL)return;

	for(k=0; k < layer->nc; k++) {
		printf("nvcell[%d] \n",k);
		nvcell_print_params(layer->nvcells[k]);
	}

}

/*------------------------------------------------------
 * Note:
 *	1. Print dw and dv of  nvcells in a nvnet.
 * Params:
 * 	@nnet		a nerve net;
 *
--------------------------------------------------------*/
void nvnet_print_params(const NVNET *nnet)
{
	int k;

	if(nnet==NULL) return;

	printf("\n      ----------- Print Model Params -----------\n");
	for(k=0; k<nnet->nl; k++) {
		printf("\n		--- nvlayer [%d] ---\n",k);
		nvlayer_print_params(nnet->nvlayers[k]);
	}

}


/////////////////////////      Loss Functions     ///////////////////////

/*------------------------- MSE Loss Function --------------------------------
 * Mean Squared Error Loss Function for each sample: (tv-yi)^2
 * 	 (1/N)*SUM[(tv-yi)^2],  1/N*SUM[ ... ] to be applied by the caller!!!
 * NOTE:
 *  1. For normal func: it returns  (tv-yi)^2.
 *  2. For derivative func: it returns  -2*(tv-yi).
 *
 * Params:
 * 	@out		nvcells  output value;
 *	@tv		teacher's value
 *	@token		NORMAL_FUNC or DERIVATIVE_FUNC
 * Return:
 *		0	OK
 *		<0	fails
------------------------------------------------------------------------------*/
double func_lossMSE(double out, const double tv, int token)
{
   /* Normal func */
   if(token==NORMAL_FUNC) {
        return (tv-out)*(tv-out);
   }
   /* if DERIVATIVE_FUNC Derivative func, dE/dh */
   else  {
        return -2.0*(tv-out);
  }

}


/*--------------------- Cross-entropy Loss Function ----------------------------
 * Cross-entropy Loss Function for each sample: -tv[i]*log(out[i])
 * For batch samples, the mean loss is:
 * 		-1/N*SUM{ tv[i]*log(out[i]) }  to be applied by the caller!!
 * NOTE:
 *  1. For normal func: it returns: -tv[i]*log(out[i])
 *  2. For derivative func: it returns:
 *
 *   		!!!--- CAUTION ---!!!
 *   The Caller MUST ensure mem of out/tv as per classes.
 *
 * Params:
 * 	@out		nvcells  output value (possiblity value for each class);
 *	@tv		teacher's value
 *xxx	@classes	Number of classes, or array size of out/tv.
 *	@token		NORMAL_FUNC or DERIVATIVE_FUNC
 * Return:
 *	A very big value	fails
------------------------------------------------------------------------------*/
double func_lossCrossEntropy(double out, double tv, int token)
{
   /* Normal func */
   if(token==NORMAL_FUNC) {
//	if(out<0.0001) printf("%s: out=%f, log(out)=%f\n",__func__, out, log(out));
	return  -tv*log(out);  /* log() is ln() */
   }
   /* For Derivative: It MUST combine with softMax or logSoftMax etc.
	1.  softMax + CorssEntropy:  dE/dh=dE/dz=yi-ti
        In this case, it compute in nvlayer_mean_loss()
    */
   else  {
	printf("%s: Only support NORMAL_FUNC!\n", __func__);
	return 999999.0;
  }
}

#if 0 ////////////////////////////////////////////
float func_lossCrossEntropy(float *out, float *tv, int classes, int token)
{
   if(out==NULL || tv==NULL)
	return  999999.0;

   int k;
   float loss=0.0f;

   /* Normal func */
   if(token==NORMAL_FUNC) {
	for(k=0; k<classes; k++) {
		loss -= tv[k]*log(out[k]);  /* log() is ln() */
	}

	return loss;
   }
   /* For Derivative: It MUST combine with softMax or logSoftMax etc.
	1.  softMax + CorssEntropy:  dE/dh=dE/dz=yi-ti
        In this case, it compute in nvlayer_mean_loss()
    */
   else  {
	printf("%s: Only support NORMAL_FUNC!\n", __func__);
	return 999999.0;
  }

}
#endif //////////////////////////////////////////



/*------------------------------------------------------
 * check two gradient value if they are close enough
 * Params:
 * 	@a,b		two gradient value
 * Return
 *	true	ok, two values are close enough.
 *	false	fails
--------------------------------------------------------*/
bool  gradient_isclose(double da, double db)
{
	double a=da>0?da:-da;
	double b=db>0?db:-db;
	double dcomp;

	/* 1. check absoute limit and compute dcomp */
	if(a > GRADIENT_ABS_LIMIT && b > 0 ) {
		dcomp=(da-db)/da;
		dcomp= dcomp>0 ? dcomp : -dcomp;
	}
	else if(b > GRADIENT_ABS_LIMIT && a > 0 ) {
		dcomp=(db-da)/db;
		dcomp= dcomp>0 ? dcomp : -dcomp;
	}
	else /* a,b <= GRADIENT_ABS_LIMIT */
		return true;

	/* 2. check comp limit */
	if( dcomp <= GRADIENT_COMP_LIMIT )
		return true;
	else
		return false;
}



/*--------------------------------------------
TODO: put into actfs.c

Softmax for NVLAYER: softmax(xi) = e^xi/Sum(e^x)
This function applys in nvlayer_feed_forward()!
after tranfer function!

	!!! --- CAUTION --- !!!
func_softmax is layer transfer function. Results are
stored in layer->douts[]. NOT in layer->nvcells[].dout!


@layer: NVLAYER to apply softmax
@token:  0 ---normal func; 1 ---derivative func

Return:
	0 	Ok
	<0	Fails
-------------------------------------------*/
int func_softmax(NVLAYER *layer, int token)
{
	int i;
	double fsum=0.0f;

        /* check layer */
        if(layer==NULL || layer->nvcells==NULL)
                return -1;

 	if(token==NORMAL_FUNC) {
        	/* Add up dout */
	        for(i=0; i< layer->nc; i++) {
			//layer->nvcells[i]->dout = exp(layer->nvcells[i]->dsum);
			//fsum += layer->nvcells[i]->dout;
			layer->douts[i] = exp(layer->nvcells[i]->dsum);  //dout==dsum
			fsum += layer->douts[i];
		}

#if 1  /* Test: fsum */
		if(isnan(fsum))
			printf("%s: fsum is nan!\n", __func__);
		else if(isinf(fsum))
			printf("%s: fsum is inf!\n", __func__);
		else if(fsum==0.0)
			printf("%s: fsum ==0.0!\n", __func__);
#endif

		/* Softmax value */
        	for(i=0; i< layer->nc; i++) {

			//layer->nvcells[i]->dout /= fsum;  // NOPE! TODO: It will dirt dout! for later chain derivative operation.
			layer->douts[i] /=fsum; /* Noticed~! softmax is layer transfer function!  result put in layer->douts[] */
//printf("%s: douts[%d]=%f\n", __func__, i, layer->douts[i]);
		}
	}
	else {  /* ERIVATIVE_FUNC. */
		/* TODO
		   Si = softmax(ai);
		   Sj = softmax(aj)

		   If(i==j) DSi/Daj=si*(1-sj)
		   Else     Dsi/Daj= -si*sj
		*/

		/* softMax+crossEntropy combined derivative, see in */
		printf("%s: NO derivative func for sotfmax!",__func__);

	}

	return 0;
}



