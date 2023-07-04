/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
-----------------------------------------------------------------------*/
#ifndef __NNC_H__
#define __NNC_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct nerve_cell  NVCELL; 	/* neuron, or nerve cell */
typedef struct nerve_layer NVLAYER;
typedef struct nerve_net   NVNET;

struct nerve_cell
{
	unsigned int nin; 		/* number of dendrite receivers */
	NVCELL * const *incells; 	/* array of input cells, whose outputs are inputs for this cell
				  	   Only a pointer, will NOT allocate mem space.
					 */
	double *din; 			/* (data*nin)  (h^L-1) array of input data, mem space NOT to be allocated.
					 *  If the cell is in the input layer, din points to input data.
					 *  Else NO USE!  data fetch from incells[]->dout
					 */
	double *dw;  			/* (w*nin) array of weights, mem sapce allocated in new_nvcell().  */
	double dv;			/* (b) bias value */
	double dsum;			/* (u) sum of x1*w1+x2*w2+x3*w3+....+xn*wn -dv*/
	double (*transfunc)(double, double, int); /* pointer to a NVCELL transfer function (also include derivative part)
						   * If NULL, then f(x)=x! as nvcell->dout=nvcell->dsum <-------- NOTICED~!!!

									!!!---- CAUTION ----!!!
						      If the layer has transfunc defined(softMax etc.), then all
 						      layer->nvcells SHOULD NOT have transfunc defined!
						      If both transfuncs are defined(both nonlinear functions), usually
						      learning process will fail!
						   */

	double dout;			/* (h^L) dz, output value, dout=transfunc(dsum-dv)
					 * If transfunc==NULL, then dout==dsum!!!   see in nvcell_feed_forward()
					 */
	double derr;			/* current nvcell's dE/du, updated in feedback process.
					  In backpropagation, it temporarily stores dE/dh(=next layer' dE/dxi). then dE/du=dE/dh*f'(u).
			   		  For non_output layer, after feedback calculation:
				derr=dE/du=f'(u)*SUM(dE/du*w), wereh SUM(dE/du*w)=backfeeded(back propagated) from the next layer cell
			   		  For output layer, after feedback calculation:
					  derr=dE/du=f'(u)*L'(h)
					*/
};

struct nerve_layer
{
	unsigned int nc;	/* number of NVCELL in the layer */
	NVCELL **nvcells;	/* array of nerve cells in the layer */

	int (*transfunc)(NVLAYER*, int);  /* pointer to a NVLAYER transfer function (also include derivative part)
                                            * NOW: It's ONLY for the output layer, For example, to apply softMax function.
                                            * If NULL, then f(x)=x! as nvcell->dout=nvcell->dsum

								!!!---- CAUTION ----!!!
					      If the layer has transfunc defined(softMax etc.), then all
 					      layer->nvcells SHOULD NOT have transfunc defined!
					      If both transfuncs are defined(both nonlinear functions), usually
					      learning process will fail!

                                            */
	double *douts;		/* For the output layer, to store transfunc results! HK2023-06-19
				 * In this case, nvcell->dout stores u value.
				 */
};


struct nerve_net
{
	unsigned int nl;			/* number of NVLAYER in the net */
	NVLAYER * *nvlayers;     /*  array of nervers for the net */

	unsigned long np;	/* total numbers of params in the net */
	double *params;		/* for buffing params of all cells in the nvnet,
				 * WARNING: write and read MUST follow the same sequence!!!
				 * params are buffed cell by cell, in order of: dw[], dv, dsum, dout, derr
				*/
	unsigned long nmp;	/* total nmbers of mmts of all params, dw[] and dv */
	double *mmts; 		/* momentums of all corresponding params
				 * WARNING: write and read MUST follow the same sequence!!!
				 */
};


/* Function declaration */
/* nvcell */
NVCELL * new_nvcell( unsigned int nin, NVCELL * const *incells,
			double *din, double *dw, double bias, double (*transfer)(double, double, int ) );
void free_nvcell(NVCELL *ncell);
int nvcell_rand_dwv(NVCELL *ncell);
int nvcell_feed_forward(NVCELL *nvcell);
int nvcell_feed_backward(NVCELL *nvcell);
int nvcell_input_data(NVCELL *cell, double *data);

/* nvlayers */
NVLAYER *new_nvlayer(unsigned int nc, const NVCELL *template_cell, bool layerTransfuncDefined);
void free_nvlayer(NVLAYER *layer);
int nvlayer_load_params(NVLAYER *layer, double *weights, double *bias);
int nvlayer_link_inputdata(NVLAYER *layer, double *data);
int nvlayer_feed_forward(NVLAYER *layer);
double nvlayer_mean_loss(NVLAYER *outlayer, const double *tv,
                        double (*loss_func)(double out, const double tv, int token) );
int nvlayer_feed_backward(NVLAYER *layer);


/* nvnet */
NVNET *new_nvnet(unsigned int nl);
//int nvnet_feed_forward(NVNET *nnet);
int nvnet_init_params(NVNET *nnet);
double nvnet_feed_forward(NVNET *nnet, const double *tv,
                          double (*loss_func)(double, const double, int) );
int nvnet_feed_backward(NVNET *nnet);

int nvnet_update_params(NVNET *nnet, double rate);
//int nvnet_mmtupdate_params(NVNET *nnet, double rate);
int nvnet_mmtupdate_params(NVNET *nnet, double rate, double mfrict);

int nvnet_buff_params(NVNET *nnet);
int nvnet_restore_params(NVNET *nnet);
int nvnet_check_gradient(NVNET *nnet, const double *tv,
                        double (*loss_func)(double, const double, int) );
void free_nvnet(NVNET *nnet);

/* set param */
//void  nnc_set_param(double learn_rate);
void  nnc_set_param(double learn_rate, double dmfric);
void nnc_set_learnrate(double learn_rate);
void nnc_set_mfrict(double mfric);
double random_btwone(void);

/* print params */
void nvcell_print_params(const NVCELL *nvcell);
void nvlayer_print_params(const NVLAYER *layer);
void nvnet_print_params(const NVNET *nnet);


/* loss func */
double func_lossMSE(double out, const double tv, int token);
double func_lossCrossEntropy(double out, double tv, int token);
//double func_lossCrossEntropy(double *out, double *tv, int classes, int token);

/* others */
bool  gradient_isclose(double da, double db);

/* NVLAYER transfer/activation functions */
int func_softmax(NVLAYER *layer, int token);

#endif
