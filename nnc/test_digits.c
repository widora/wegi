/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.



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
//#include "params.h"
#include "nnc_digits.h"

double data_input[10][64]=
{
/////// 0 //////
  0.,   0.,   0.,  12.,  11.,   0.,   0.,   0.,   0.,   0.,  12.,
 12.,   9.,  10.,   0.,   0.,   0.,   2.,  16.,   2.,   1.,  11.,
  1.,   0.,   0.,   1.,  15.,   0.,   0.,   5.,   8.,   0.,   0.,
  2.,  14.,   0.,   0.,   5.,  10.,   0.,   0.,   0.,  13.,   2.,
  0.,   2.,  13.,   0.,   0.,   0.,   7.,   9.,   0.,   7.,  11.,
  0.,   0.,   0.,   0.,  11.,  13.,  16.,   2.,   0.,

/////// 1 //////
   0.,   0.,  13.,  14.,  10.,   2.,   0.,   0.,   0.,   0.,   6.,
  16.,  16.,  16.,   0.,   0.,   0.,   0.,   0.,  16.,  16.,  16.,
   4.,   0.,   0.,   0.,   4.,  16.,  16.,  14.,   2.,   0.,   0.,
   0.,   8.,  16.,  16.,   7.,   0.,   0.,   0.,   3.,  15.,  16.,
  16.,   4.,   0.,   0.,   0.,   1.,  16.,  16.,  14.,   1.,   0.,
   0.,   0.,   0.,  14.,  16.,  13.,   3.,   0.,   0.,

/////// 2 //////
  0.,   3.,  15.,  16.,   6.,   0.,   0.,   0.,   0.,  11.,  15.,
 12.,  15.,   0.,   0.,   0.,   0.,   2.,   2.,   2.,  16.,   4.,
  0.,   0.,   0.,   0.,   0.,   0.,  16.,   4.,   0.,   0.,   0.,
  0.,   0.,   5.,  16.,   1.,   0.,   0.,   0.,   0.,   0.,  11.,
 15.,   4.,   1.,   0.,   0.,   1.,  10.,  16.,  16.,  16.,  11.,
  0.,   0.,   4.,  16.,  14.,  12.,   8.,   3.,   0.,

/////// 3 ///////
  0.,   0.,   9.,  16.,  16.,  10.,   0.,   0.,   0.,   0.,   9.,
  9.,   9.,  15.,   0.,   0.,   0.,   0.,   0.,   0.,   6.,  14.,
  0.,   0.,   0.,   0.,   0.,   2.,  15.,   7.,   0.,   0.,   0.,
  0.,   1.,  14.,  16.,   4.,   0.,   0.,   0.,   0.,   5.,  16.,
 16.,   8.,   0.,   0.,   0.,   0.,   0.,   6.,  16.,   4.,   0.,
  0.,   0.,   0.,  11.,  16.,  12.,   0.,   0.,   0.,

/////// 4 ///////
  0.,   0.,   0.,   3.,  16.,   5.,   0.,   0.,   0.,   0.,   3.,
 14.,  10.,   0.,   9.,  11.,   0.,   1.,  13.,  11.,   0.,   2.,
 15.,   8.,   0.,   7.,  16.,   9.,  11.,  16.,  15.,   1.,   0.,
  6.,  15.,  13.,  12.,  16.,   9.,   0.,   0.,   0.,   0.,   0.,
  8.,  15.,   2.,   0.,   0.,   0.,   0.,   1.,  15.,   7.,   0.,
  0.,   0.,   0.,   0.,   5.,  15.,   2.,   0.,   0.,

/////// 5 ///////
   0., 8., 16., 12., 15., 16., 7., 0.,
   0.,  13.,  16.,  14.,   6.,   4.,   1.,   0.,
   0.,  12.,  10.,   0.,   0.,   0.,   0.,   0.,
   0.,   3.,  16.,  10.,   0.,   0.,   0.,   0.,
   0.,   0.,   6.,  15.,   9.,   0.,   0.,   0.,
   0.,   0.,   0.,   4.,  16.,   2.,   0.,   0.,
   0.,   1.,   4.,   6.,  16.,   5.,   0.,   0.,
   0.,   7.,  16.,  16.,  10.,   0.,   0.,   0.,

/////// 6 //////
  0.,   0.,   3.,  11.,   0.,   0.,   0.,   0.,   0.,   0.,  12.,
 11.,   0.,   0.,   0.,   0.,   0.,   1.,  14.,   1.,   0.,   0.,
  0.,   0.,   0.,   2.,  15.,   0.,   0.,   0.,   0.,   0.,   0.,
  4.,  15.,  15.,  16.,  15.,   2.,   0.,   0.,   1.,  16.,   8.,
  4.,   8.,  11.,   0.,   0.,   1.,  16.,  11.,   7.,  10.,  12.,
  0.,   0.,   0.,   5.,  10.,  12.,  15.,   7.,   0.,

/////// 7 ///////
  0.,   0.,   6.,  16.,  16.,   3.,   0.,   0.,   0.,   0.,   8.,
 16.,  16.,  12.,   0.,   0.,   0.,   0.,   0.,   4.,  15.,  11.,
  0.,   0.,   0.,   0.,   6.,  16.,  16.,  16.,  13.,   0.,   0.,
  0.,  11.,  16.,  16.,   5.,   1.,   0.,   0.,   0.,   0.,  14.,
  7.,   0.,   0.,   0.,   0.,   0.,   4.,  16.,   1.,   0.,   0.,
  0.,   0.,   0.,  11.,  11.,   0.,   0.,   0.,   0.,

/////// 8 ///////
  0.,   0.,   2.,  16.,  10.,   1.,   0.,   0.,   0.,   0.,   7.,
 16.,  16.,  12.,   0.,   0.,   0.,   0.,   3.,  16.,  16.,  15.,
  0.,   0.,   0.,   0.,   2.,  16.,  14.,   0.,   0.,   0.,   0.,
  0.,   8.,  15.,  16.,   6.,   0.,   0.,   0.,   0.,  13.,   8.,
  9.,  13.,   0.,   0.,   0.,   0.,  12.,  10.,   7.,  16.,   0.,
  0.,   0.,   0.,   3.,  13.,  15.,  10.,   0.,   0.,

/////// 9 //////
  0.,   1.,  12.,  16.,  10.,   1.,   0.,   0.,   0.,   8.,  12.,
  3.,  11.,   8.,   0.,   0.,   0.,  12.,  13.,   6.,  12.,   8.,
  0.,   0.,   0.,   3.,  15.,  16.,  16.,  16.,   1.,   0.,   0.,
  0.,   0.,   0.,   0.,  13.,   6.,   0.,   0.,   0.,   0.,   0.,
  0.,   6.,  11.,   0.,   0.,   0.,  13.,   0.,   0.,   5.,  12.,
  0.,   0.,   0.,  12.,  16.,  16.,  16.,   8.,   0.,
};


int main(void)
{
        float r;
	int dindex;

        /* Select random digit image data*/
        struct timeval tmval;
        gettimeofday(&tmval,NULL);
        srand(tmval.tv_usec);
        r=rand();

        dindex=round(r*10/RAND_MAX);
        if(dindex>9)dindex=9;


	printf(" ---- Predict: %d ------\n",
		nnc_predictDigit(&data_input[dindex][0]) );

	return 0;
}

#if 0 //////////////////////////////////
int main(void)
{
	int j;
	int dindex=0;
	float r;
	/* Select random digit image data*/
        struct timeval tmval;
        gettimeofday(&tmval,NULL);
        srand(tmval.tv_usec);
	r=rand();
        dindex=round(r*10/RAND_MAX);
	if(dindex>9)dindex=9;


        /* 1. create an input nvlayer */
        NVCELL *tempcell_input=new_nvcell(64, NULL, &data_input[dindex][0], NULL, 0, func_sigmoid); /* input cell */
        NVLAYER *layer0=new_nvlayer(20, tempcell_input);

        /* 2. create a hidden nvlayer */
        NVCELL *tempcell_hidden=new_nvcell(20, layer0->nvcells,NULL,NULL,0, func_sigmoid);//sigmoid); /* input cell */
        NVLAYER *layer1=new_nvlayer(20, tempcell_hidden);

        /* 3. create an output nvlayer */
        NVCELL *tempcell_output=new_nvcell(20, layer1->nvcells, NULL,NULL,0, NULL);//ReLU);//sigmoid); /* input cell */
        NVLAYER *layer2=new_nvlayer(10, tempcell_output);
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

/* LOOP TEST: ------------------------------------------------ */
while(1) {
    /* Random dindex */
    gettimeofday(&tmval,NULL);
    srand(tmval.tv_usec);
    r=rand();
    dindex=round(r*10/RAND_MAX);
    if(dindex>9)dindex=9;

    /* Reload input data to input layer */
    nvlayer_link_inputdata(layer0, &data_input[dindex][0]);


	/* 6. Feed forward to predict result */
        nvnet_feed_forward(nnet, NULL, func_lossMSE); /* NULL: no targets */

	if(nnet->nvlayers[2]->transfunc != NULL)
		printf("nnet->nvlayers[2]->transfunc != NULL\n");

	/* Print output */
        printf("Input: \n");
        for(j=0;j< layer0->nvcells[0]->nin;j++) {
        	printf("%02d ", (int)data_input[dindex][j]);
		if( (j+1)%8==0 )
			printf("\n");
	}
        printf("\n");
        printf("output: \n");
	float possibility=0.0f;
	int digit=0;
        for(j=0;j< layer2->nc; j++) {
		if( layer2->nvcells[j]->dout > possibility) {
			possibility=layer2->nvcells[j]->dout;
			digit=j;
		}
        	printf("%lf ",layer2->nvcells[j]->dout);
	}
        printf("\n");
        printf("Digit: %d, Predict: %d \n", dindex, digit);

usleep(10000);
} /* END while() ---------------------------------- */

	/* Free */
        free_nvcell(tempcell_input);
        free_nvcell(tempcell_hidden);
        free_nvcell(tempcell_output);

 	free_nvnet(nnet); /* free nvnet also free its nvlayers and nvcells inside */


	return 0;
}
#endif //////////////////////////
