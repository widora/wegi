/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Reference:
https://peterroelants.github.io/posts/neural-network-implementation-part05/

Journal:
2023-05-17: Create the file.

Midas Zhou
知之者不如好之者好之者不如乐之者
-----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <sys/time.h>
#include <string.h>

#include "nnc.h"
#include "actfs.h"
#include "params.h"
#include "nnc_digits.h"


/*---------------------------------------
To predict a digit number in a 8x8
gray image.

@imgdata: 8x8 gray value of an image.
          gray value [0 16]

@dout:	  To pass output data for all layers.
	  layer0_nout+layer1_nout+layer1_nout

		  !!! CAUTION !!!
	 //The caller must free mem of dout after use.
	 The caller MUST allocate enough mem space for it.
Return:
	<0 	Fails, or possibility <0.5
	[0 9]	OK
----------------------------------------*/
int nnc_predictDigit(double *imgdata, float *dout)
{
	int j;
	int digit=-1;  /* -1 as fails */

	if(imgdata==NULL)
		return -1;

	const int layer0_nin=64, layer0_nout=20;
	const int layer1_nin=20, layer1_nout=20;
	const int layer2_nin=20, layer2_nout=10;

	int dcnt=0;

	/* Allocate dout */
	//dout=calloc(layer0_nout+layer1_nout+layer2_nout, sizeof(float));
	if(dout==NULL)
		return -1;

        /* 1. create an input nvlayer */
        NVCELL *tempcell_input=new_nvcell(layer0_nin, NULL, imgdata, NULL, 0, func_sigmoid); /* input cell */
        NVLAYER *layer0=new_nvlayer(layer0_nout, tempcell_input);

        /* 2. create a hidden nvlayer */
        NVCELL *tempcell_hidden=new_nvcell(layer1_nin, layer0->nvcells,NULL,NULL,0, func_sigmoid);//sigmoid); /* input cell */
        NVLAYER *layer1=new_nvlayer(layer1_nout, tempcell_hidden);

        /* 3. create an output nvlayer */
        NVCELL *tempcell_output=new_nvcell(layer2_nin, layer1->nvcells, NULL,NULL,0, NULL);//ReLU);//sigmoid); /* input cell */
        NVLAYER *layer2=new_nvlayer(layer2_nout, tempcell_output);
	layer2->transfunc = func_softmax;

        /* 4. create an nerve net */
        NVNET *nnet=new_nvnet(3); /* 3 layers inside */
        nnet->nvlayers[0]=layer0;
        nnet->nvlayers[1]=layer1;
        nnet->nvlayers[2]=layer2;

        /* 5. init params */
        //nvnet_init_params(nnet);

	/* 5. Load params */
	nvlayer_load_params(layer0, param_weights0, param_bias0);
	nvlayer_load_params(layer1, param_weights1, param_bias1);
	nvlayer_load_params(layer2, param_weights2, param_bias2);

	/* 6. Feed forward to predict result */
        nvnet_feed_forward(nnet, NULL, func_lossMSE); /* NULL: no targets */

	/* Print output  (64) */
        printf("Input: \n");
        for(j=0;j< layer0->nvcells[0]->nin;j++) {
        	printf("%02d ", (int)imgdata[j]);
		if( (j+1)%8==0 )
			printf("\n");
	}
        printf("\n");

	printf("output of nvlayers[0]: \n");
	for(j=0; j< layer0->nc; j++) {  /* all connect to next layer cells */
		printf("%f ", layer0->nvcells[j]->dout);

		dout[dcnt++]=layer0->nvcells[j]->dout;
	}

	printf("\n");
	printf("output of nvlayers[1]: \n");
	for(j=0; j< layer1->nc; j++) {   /* all connect to next layer cells */
		printf("%f ", layer1->nvcells[j]->dout);

		dout[dcnt++]=layer1->nvcells[j]->dout;
	}
	printf("\n");

        printf("output of nvlayers[2]: \n");
	float possibility=0.65f;  /* threshold possiblity */
        for(j=0; j< layer2->nc; j++) {
		if( layer2->nvcells[j]->dout > possibility) {
			possibility=layer2->nvcells[j]->dout;
			digit=j;
		}
        	printf("%lf ",layer2->nvcells[j]->dout);

		dout[dcnt++]=layer2->nvcells[j]->dout;

	}
        printf("\n");

        printf("Predict: %d \n", digit);

	/* Free */
        free_nvcell(tempcell_input);
        free_nvcell(tempcell_hidden);
        free_nvcell(tempcell_output);

 	free_nvnet(nnet); /* free nvnet also free its nvlayers and nvcells inside */


	return digit;
}
