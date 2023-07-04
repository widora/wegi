/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Neural Network Description:
   Input(64) |<<<    Hidden_Layer(64i,20o)    >>>|<<<   Hidden_Layer(20i,20o)  >>>|<<<  Output_Layer(20i,10o)  >>>|

Reference:
1. Neural Network Training Model:
   https://peterroelants.github.io/posts/neural-network-implementation-part05/

2. MNIST handwrriten digit database:
   http://yann.lecun.com/exdb/mnist/

3. scikit-learn digit data:
   import sklearn
   from sklearn import datasets
   digits = datasets.load_digits()
   print(digits.data[k])

Some hints:
1. As number of layers increases, learn rate shall be smaller.
2. Simple mind learn simple thing.

Journal:
2023-06-29: Create the file.


Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------------*/
#include <stdint.h>
#include <xpt2046.h>
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_cstring.h>
#include <egi_FTsymbol.h>
#include <endian.h>

#include "digits_imgdata.h"
#include "nnc.h"
#include "actfs.h"
#include "digits_imgdata.h"


int main(void)
{



 /* <<<<<<  EGI general init  >>>>>> */

  /* Start sys tick */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();

  #if 0
  /* Start egi log */
  printf("egi_init_log()...\n");
  if(egi_init_log("/mmc/log_scrollinput")!=0) {
        printf("Fail to init egi logger, quit.\n");
        return -1;
  }
  #endif

  #if 0
  /* Load symbol pages */
  printf("FTsymbol_load_allpages()...\n");
  if(FTsymbol_load_allpages() !=0) /* FT derived sympg_ascii */
        printf("Fail to load FTsym pages! go on anyway...\n");
  printf("symbol_load_allpages()...\n");
  if(symbol_load_allpages() !=0) {
        printf("Fail to load sym pages, quit.\n");
        return -1;
  }
  #endif

  /* Load freetype fonts */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
        printf("Fail to load FT sysfonts, quit.\n");
        return -1;
  }
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
        printf("Fail to load FT appfonts, quit.\n");
        return -1;
  }

  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;

  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
        return -1;

  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,true);
  fb_position_rotate(&gv_fb_dev, 0);  /* relative to touch_coord */

  /* Create a fbimg to link to FB data */
  EGI_IMGBUF *fbimg=NULL;
  fbimg=egi_imgbuf_alloc();
  if(fbimg==NULL) return -1;
  fbimg->width=gv_fb_dev.pos_xres;
  fbimg->height=gv_fb_dev.pos_yres;
  fbimg->imgbuf=(EGI_16BIT_COLOR*)gv_fb_dev.map_fb;

#if 0/* TEST: ------ Reset xpt factors aft. egi_start_touchread()! --------- */
  sleep(1); /* Just wait to let egi_start_touchread() finish calling xpt_set_factors() */
  xpt_set_factors(2.1667, 3.0, 60, 56);
  //factX=2.166667, factY=3.000000  tbaseX=60 tbaseY=56
  //factX=2.194805, factY=3.000000  tbaseX=61 tbaseY=57
#endif

 /* <<<<<  End of EGI general init  >>>>>> */




	                  /*-----------------------------------
        	                 Neurial Network Training
                	   ----------------------------------*/

	#define ERR_LIMIT	0.001 //0.001

	int i,j,k;
	int loop=0;	/* while loops */

	int wi_cellnum=20; /* wi layer cell number */
	int wm_cellnum=20; /* wm layer cell number */
	int wo_cellnum=10; /* wo layer cell number */

	int wi_inpnum=64; /* number of input data for each input/hidden nvcell */
	int wm_inpnum=20; /* number of input data for each middle nvcell */
	int wo_inpnum=20; /* number of input data for each output nvcell */

	int count=0;  	   /* train counts */
	double err, batch_err, mean_err;
	const int  DATA_MERRS_MAX=300;
	double data_merrs[DATA_MERRS_MAX]; /* To store mean_err of each batch train */
	double max_merr=0.0f;
	int ns=500; /* batch size;  input samples (numbers + teacher value) for each batch training */
	int nb;     /* number of batches */

	bool gradient_checked=false;

	/* To link it to input_layer->pins */
	double data_input[64];
	double data_target[10];	/* one_hot target values. ONLY one '1' and others are '0'. */

	char strtmp[32];
	int TEST_DIGIT;

	/* Neural network symbol image */
	EGI_IMGBUF *nnimg=NULL;

        /* Handwritten Digit image, from MNIST or Sklearn data. */
        EGI_IMGBUF *hdimg=NULL;
        EGI_IMGBUF *hdsimg=NULL; /* stretched */
	EGI_16BIT_COLOR tcolor;

#if 1 /* TEST: ----------- */
        clear_screen(&gv_fb_dev,WEGI_COLOR_DARKOCEAN);

	/* Antialias ON, to apply fdraw_line(). */
        gv_fb_dev.antialias_on=true;

	/* Draw Neural Network Symbols */
	fbset_color(WEGI_COLOR_WHITE);//LTBLUE);
	//for(i=0; i<12; i++)
	//	fdraw_circle(&gv_fb_dev, 30, 20+i*8, 12);
	for(i=0; i<20; i++)
		fdraw_circle(&gv_fb_dev, 30, 20+i*(88.0f/19), 12);

	fbset_color(WEGI_COLOR_WHITE); //LTGREEN);
	//for(i=0; i<12; i++)
	//	fdraw_circle(&gv_fb_dev, 30+50, 20+i*8, 12);
	for(i=0; i<20; i++)
		fdraw_circle(&gv_fb_dev, 30+50, 20+i*(88.0f/19), 12);

	fbset_color(WEGI_COLOR_WHITE); //LTYELLOW);
	//for(i=0; i<6; i++)
	//	fdraw_circle(&gv_fb_dev, 30+50*2, 20+25+i*8, 12);
	for(i=0; i<10; i++)
		fdraw_circle(&gv_fb_dev, 30+50*2, 20+25+i*(40.0f/9), 12);


	fbset_color(WEGI_COLOR_LTBLUE);
	for(i=0; i<6; i++) {
		for(j=0; j<12; j++) {
			//draw_line_simple(&gv_fb_dev, 20, 20+i*8, 20+50, 20+j*8);
			fdraw_line(&gv_fb_dev, 0, 44+i*8, 30, 20+j*8);
		}
	}


	fbset_color(WEGI_COLOR_LTGREEN);
	for(i=0; i<12; i++) {
		for(j=0; j<12; j++) {
			//draw_line_simple(&gv_fb_dev, 20, 20+i*8, 20+50, 20+j*8);
			fdraw_line(&gv_fb_dev, 30, 20+i*8, 30+50, 20+j*8);
		}
	}
	fbset_color(WEGI_COLOR_LTYELLOW);
	for(i=0; i<12; i++) {
		for(j=0; j<6; j++) {
			//draw_line_simple(&gv_fb_dev, 20+50, 20+i*8, 20+50*2, 20+25+j*8);
			fdraw_line(&gv_fb_dev, 30+50, 20+i*8, 30+50*2, 20+25+j*8);
		}
	}

	#if 0
	fbset_color(WEGI_COLOR_LTGREEN);
	draw_line_simple(&gv_fb_dev, 30, 20, 30+50, 20+11*8);
	draw_line_simple(&gv_fb_dev, 30, 20+11*8, 30+50, 20);

	fbset_color(WEGI_COLOR_LTYELLOW);
	draw_line_simple(&gv_fb_dev, 30+50, 20, 30+50*2, 20+25+5*8);
	draw_line_simple(&gv_fb_dev, 30+50, 20+11*8, 30+50*2, 20+25);
	#endif

	/* Antialias OFF, to apply fdraw_line(). */
        gv_fb_dev.antialias_on=true;

	/* Copy fbimg to nnimg */
//	nnimg=egi_imgbuf_blockCopy(fbimg, 0,0, 145,128);
	nnimg=egi_imgbuf_blockCopy(fbimg, 0, 5, 118, 145); //180, 118);

//	exit(1);
#endif ///////////////////////////////////////


        /* Clear screen first */
        clear_screen(&gv_fb_dev,WEGI_COLOR_LTYELLOW); //WHITE); //BLACK); //DARKGRAY);



        /* Write neural network model description */
        draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY2, 0,0, 320-1, 20-1);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        15, 15, (unsigned char *)"3-Layer Neural Network Training",
                                        //(unsigned char *)"NN(64i,20o)+NN(20i,20o)+NN(20i,10o)",    /* fw,fh, pstr */
                                        320, 1, 4,                                  /* pixpl, lines, gap */
                                        25, 0,                                    /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255,       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        15, 15, (unsigned char *)"神经网络: 64(in)->20->20->10(out)",
                                        //(unsigned char *)"NN(64i,20o)+NN(20i,20o)+NN(20i,10o)",    /* fw,fh, pstr */
                                        320, 1, 4,                                  /* pixpl, lines, gap */
                                        35, 20,                                    /* x0,y0, */
                                        WEGI_COLOR_BLACK, -1, 250,       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

       	/* Clear plot area */
	fbset_color(WEGI_COLOR_DARKOCEAN); //BLACK);
	draw_filled_rect(&gv_fb_dev, 0, 40-1, 320-1,240-1);


do {  /* test while */

	/*  <<<<<<<<<<<<<<<<<  Create Neurial Network >>>>>>>>>>>>>  */

	/* 1. create an input nvlayer */
	NVCELL *wi_tempcell=new_nvcell(wi_inpnum,NULL, data_input, NULL,0, func_ReLU); //func_TanSigmoid); /* input cell */
        NVLAYER *wi_layer=new_nvlayer(wi_cellnum,wi_tempcell, false);

	/* 2. create a mid nvlayer */
	NVCELL *wm_tempcell=new_nvcell(wm_inpnum,wi_layer->nvcells,NULL,NULL,0, func_ReLU); //sigmoid); //func_ReLU); //func_TanSigmoid); //sigmoid); /* input cell */
        NVLAYER *wm_layer=new_nvlayer(wm_cellnum,wm_tempcell, false);

	/* 3. create an output nvlayer */
	NVCELL *wo_tempcell=new_nvcell(wo_inpnum, wm_layer->nvcells, NULL,NULL,0, NULL); //NULL); //func_TanSigmoid);//sigmoid); /* input cell */
        NVLAYER *wo_layer=new_nvlayer(wo_cellnum,wo_tempcell, true); /* false , we do not need douts, or no layer transfunc defined */
	wo_layer->transfunc = func_softmax;

	/* 4. Create an nerve net */
	NVNET *nnet=new_nvnet(3); /* 3 layers inside */
	nnet->nvlayers[0]=wi_layer;
	nnet->nvlayers[1]=wm_layer;
	nnet->nvlayers[2]=wo_layer;

	/* 5. Init params */
	nvnet_init_params(nnet);

	/*  <<<<<<<<<<<<<<<<<  Neurial Network Learning Process  >>>>>>>>>>>>>  */

	/* 6. Set learning_rate and  momentum friction */
	//nnc_set_param(10, 0.0);//0.03); /* set learn rate (double learn_rate, double dmmt) */
	nnc_set_mfrict(0.1);
	/* NOTE:
	 * NO MORE USE if call nvnet_update_params()/nvnet_mmtupdate_params() later.
	 *  Call nvnet_update_params(nnet, learn_rate), OR  nvnet_mmtupdate_params(nnet, learn_rate, mfrict)
	 */

	/* 7. Init err value */
	//batch_err=10; /* give an init value to trigger while() */
	mean_err=10;
	max_merr=0.0f;

  	printf("NN model starts learning ...\n");

	/* 8. Batch training */
	count=0;

	gradient_checked=false;
  	//while( count<10 || (mean_err > ERR_LIMIT && count<3000 ) )
  	while( mean_err > ERR_LIMIT || count<300 )
  	{
		/* W.1  Reset batch err */
		batch_err=0.0;

		/* W.2 batch learning */
	        for(nb=0; nb< DIGITS_IMGTOTAL/ns; nb++) {

		    /* Run all samples in the batch */
		    for(i=0; i<ns; i++) {
			//printf("\n	=== %dth_train, batch item %d/%d ===\n", count+1, i+1, ns);
			/* R1. update ONE sample for input: data_input, data_target */
			//memcpy(data_input, pin+4*i, 3*sizeof(double));
			for(k=0; k<64; k++)
				data_input[k]=digits_imgdata[nb*ns+i][k];      //const int digits_imgdata[500][64]
			for(k=0; k<10; k++)
				data_target[k] = (k==digits_targets[nb*ns+i] ? 1.0 : 0.0);


			/* R2. nvnet feed forward, accumlate err values for batch error
			   NOTICE: The output layer has transfunc defined, as func_softmax().
			 */
			err = nvnet_feed_forward(nnet, data_target, func_lossCrossEntropy); //func_lossMSE);
			if(isnan(err) || isinf(err) ) { /* If NAN */
				printf("Return err is nan or inf! Too big learn_rate?\n");
				exit(1);
			}
			//else
			//	printf("feed_forward err=%f\n",err);

			batch_err +=err;
			//printf("nvnet_feed_forward get err=%f\n", err);

			/* R3. nvnet feed backward, update cell->derrs */
			nvnet_feed_backward(nnet);

#if 0 //////////////////////////////
			/* R4. check gradient just before updating params */
			//if( !gradient_checked && count>10 && i==3 ) {
			if( !gradient_checked && i==ns-1 ) {
				if( nvnet_check_gradient(nnet, data_target, func_lossMSE) < 0) {
					printf("Gradient_check failed!\n");
					exit(-1);
				}
				else {
					gradient_checked=true;
				}
				nvnet_print_params(nnet);
				//sleep(2);
			}
#endif  /////////////////////////////////

			/* R5. update params after feedback(backpropagation) computation */
			//nvnet_mmtupdate_params(nnet, 0.002); //0.01);
			nvnet_update_params(nnet, 0.001); /* learn_rate */
			/* Note: Big learn_rate(>0.01, example 0.1) will cause all -nan results and quit trainning! */

			/* R6. Display running bar */
			//draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_PINK, 50,240-50, 50+(320-100)*(i+1)/ns, 240-50+6);
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_PINK, 50,60, 50+(320-100)*(i+1)/ns, 60+6);

		    } /* for(i) */

		} /* for(nb) */

		/* W3. Mean err for batch training */
		mean_err = batch_err/ns;
		printf("batch_err=%f, mean_err=%f\n", batch_err, mean_err);
		if(mean_err>max_merr)
			max_merr=mean_err;

		/* W3a. Store mean_err */
		data_merrs[count]=mean_err;

		/* W4. Count training */
		count++;

		/* ----- Update mean_err diagram ----- */

        	/* Clear plot area */
		fbset_color(WEGI_COLOR_DARKOCEAN); //BLACK);
		draw_filled_rect(&gv_fb_dev, 0, 40-1, 320-1,240-1);


#if 1		/* ---------- Hot Line TEST ----------- */
		//const int TEST_DIGIT=8;

		TEST_DIGIT= mat_random_range(DIGITS_IMGTOTAL);

		for(k=0; k<64; k++)
			data_input[k]=digits_imgdata[TEST_DIGIT][k];
		for(k=0; k<10; k++)
			data_target[k] = (k==digits_targets[TEST_DIGIT] ? 1.0 : 0.0);

	        /* Fill hdimg according to digitdata[] */
	        egi_imgbuf_free2(&hdimg);
	        hdimg=egi_imgbuf_createWithoutAlpha(8, 8, WEGI_COLOR_WHITE);
		if(hdimg==NULL) exit(1);
        	for(k=0; k<8*8; k++) {
	                if(k%(8)==0) printf("\n");
	                printf("%d ", digits_imgdata[TEST_DIGIT][k]);

	                tcolor=egi_colorLuma_adjust(WEGI_COLOR_WHITE,
						(1.0*digits_imgdata[TEST_DIGIT][k]/16.0-1.0)*egi_color_getY(WEGI_COLOR_WHITE));
        	        hdimg->imgbuf[k] = tcolor;
        	}
		printf("\n");


		/* Stretch hdimg to 40x40 */
        	//egi_imgbuf_free2(&hdsimg);
        	egi_imgbuf_free2(&hdsimg);
        	hdsimg=egi_imgbuf_stretch(hdimg, 40, 40);
		if(hdsimg==NULL) exit(2);

                /* Flip white/black  */
                for(k=0; k< hdsimg->width*hdsimg->height; k++) {
                           tcolor=egi_colorLuma_adjust(WEGI_COLOR_WHITE,  (-0.3)*egi_color_getY(hdsimg->imgbuf[k]));
                           hdsimg->imgbuf[k] = tcolor;
                }

		/* Display nn SYMBOLE image */
		egi_subimg_writeFB(nnimg, &gv_fb_dev, 0, -1, 100, 70);

		/* Display hdsimg */
		egi_subimg_writeFB(hdsimg, &gv_fb_dev, 0, -1, 60, 90+20);


		/* Do predict */
		#if 0
		err = nvnet_feed_forward(nnet, data_target, func_lossCrossEntropy);
		if(isnan(err) || isinf(err) ) { /* If NAN */
			printf("Return err is nan or inf! Too big learn_rate?\n");
			exit(1);
		}
		printf("err=%f, ", err);
		#else
		nvnet_feed_forward(nnet, NULL, NULL);
		#endif

		/* print result, display predict results. */
		printf("Predict: ");
		for(i=0;i<10;i++) {
			printf("%f ", wo_layer->douts[i]);
			sprintf(strtmp,"%d",i);
	        	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        45, 45, (unsigned char *)strtmp,		/* fw,fh, str */
                                        320, 1, 4,                                  /* pixpl, lines, gap */
                                        150+95, 91+10,                                    /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 255*wo_layer->douts[i],       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
		}
		printf("\n");

#endif


		#if 0 /* Wrtie current loops */
		sprintf(strtmp,"loops=%d", loop);
        	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        16, 16, (unsigned char *)strtmp,		/* fw,fh, str */
                                        320, 1, 4,                                  /* pixpl, lines, gap */
                                        320-250, 45,                                    /* x0,y0, */
                                        WEGI_COLOR_ORANGE, -1, 255,       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
		#endif


		/* Write current trains, mean_err */
		snprintf(strtmp, sizeof(strtmp),"loops=%d, epochs=%d, err=%.5f", loop, count, mean_err);
        	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        16, 16, (unsigned char *)strtmp,		/* fw,fh, str */
                                        320, 1, 4,                                  /* pixpl, lines, gap */
                                        30, 40,                                    /* x0,y0, */
                                        WEGI_COLOR_ORANGE, -1, 255,       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

		/* Draw lines */
		printf("count=%d, max_merr=%f\n", count, max_merr);
		if(count>1) {
		    fbset_color(WEGI_COLOR_YELLOW);
		    for(j=0; j<count-1; j++) {
			draw_line_simple(&gv_fb_dev, 10+j,   240-(200*data_merrs[j]/max_merr),
						     10+j+1, 240-(200*data_merrs[j+1]/max_merr) );
		    }

		    /* Tag err at last point */
		    sprintf(strtmp,"%.5f", mean_err);
	            FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        16, 16, (unsigned char *)strtmp,		/* fw,fh, str */
                                        320, 1, 4,                                  /* pixpl, lines, gap */
                                        10+j +5, 240-(200*data_merrs[j]/max_merr) -20, /* x0,y0, */
                                        WEGI_COLOR_ORANGE, -1, 255,       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

		}

		/* ------ End mean_err diagram ------ */

		/* W5. Check for data_merrs[] capability */
		if(count>=DATA_MERRS_MAX)
			break;

//		if( (count&(32-1)) == 0)
			printf("	%dth learning, mean_err=%0.8f \n",count, mean_err);

		/* Relieve CPU */
		usleep(500000);

/* TEST: ----------------- */
//exit(0);

  	}

	/* 9. Print results */
	printf("	%dth learning, mean_err=%0.8f \n",count, mean_err);
	printf("Finish %d times batch learning!. \n",count);

	/* 10. Print params */
	nvnet_print_params(nnet);

#if 0 ///////////////////////////////////////////
	/* 11. ----- Check gradient again ----- */
	i=2;

	//memcpy(data_input, pin+4*i, 3*sizeof(double));
	/* Update ONE sample:  data_input and data_target */
	for(k=0; k<64; k++)
		data_input[k]=digits_imgdata[i][k];      //const int digits_imgdata[500][64]
	for(k=0; k<10; k++)
		data_target[k] = (k==digits_targets[i] ? 1.0 : 0.0);


	nvnet_feed_forward(nnet, data_target,func_lossMSE);
	nvnet_feed_backward(nnet);
	nvnet_check_gradient(nnet, data_target, func_lossMSE);

#endif /////////////////////////////////////////


/*  <<<<<<<<<<<<<<<<<  Test Learned NN Model  >>>>>>>>>>>>>  */
	int errcnt=0;
	printf("\n----------- Test learned NN Model -----------\n");
	//for(i=0;i<ns;i++)
	for(i=0; i<DIGITS_IMGTOTAL; i++)
    	{
		/* update data_input */
                for(k=0; k<64; k++)
                         data_input[k]=digits_imgdata[i][k];      //const int digits_imgdata[500][64]
                for(k=0; k<10; k++)
                         data_target[k] = (k==digits_targets[i] ? 1.0 : 0.0);


		/* feed forward wi->wm->wo layer */
		nvlayer_feed_forward(wi_layer);
		nvlayer_feed_forward(wm_layer);
		nvlayer_feed_forward(wo_layer);  /* Apply nvlayer transfer function */

		/* print result */
		//printf("Input: ");
		//for(j=0;j<wi_inpnum;j++)
		//	printf("%lf ",data_input[j]);
		//printf("\n");
		//printf("output: %lf \n",wo_layer->nvcells[0]->dout);

		/* Print result */
		printf("Output: ");
		for(k=0; k< wo_layer->nc; k++)
			printf("%f, ",wo_layer->douts[k]);
		printf("    ");
		printf("Target: %d ",digits_targets[i]);
		if(wo_layer->douts[ digits_targets[i] ] >0.5)
			printf(" OK\n");
		else {
			printf(" Fails\n");
			errcnt++;
		}
	}
	printf("Err/Total: %d/%d\n", errcnt, DIGITS_IMGTOTAL);

	loop++;
	printf("------------  loop=%d, training count=%d ------------\n", loop, count);

	sleep(1);
/*  <<<<<<<<<<<<<<<<<  Destroy NN Model >>>>>>>>>>>>>  */

	free_nvcell(wi_tempcell);
	free_nvcell(wm_tempcell);
	free_nvcell(wo_tempcell);
/*
	free_nvlayer(wi_layer);
	free_nvlayer(wm_layer);
	free_nvlayer(wo_layer);
*/
	free_nvnet(nnet); /* free nvnet also free its nvlayers and nvcells inside */

	usleep(100000);

	printf("Ok, finish nnc.\n");
//	break;

} while(1); /* end test while */


 /* ----- MY release ---- */
// egi_fmap_free(&fmap);


 /* <<<<<  EGI general release   >>>>>> */

 printf("FTsymbol_release_allfonts()...\n");
 FTsymbol_release_allfonts(); /* release sysfonts and appfonts */
 printf("symbol_release_allpages()...\n");
 symbol_release_allpages(); /* release all symbol pages*/
 printf("FTsymbol_release_allpage()...\n");
 FTsymbol_release_allpages(); /* release FT derived symbol pages: sympg_ascii */

 printf("fb_filo_flush() and release_fbdev()...\n");
 fb_filo_flush(&gv_fb_dev);
 release_fbdev(&gv_fb_dev);
 #if 0
 printf("release virtual fbdev...\n");
 release_virt_fbdev(&vfb);
 #endif
 printf("egi_end_touchread()...\n");
 egi_end_touchread();
 #if 0
 printf("egi_quit_log()...\n");
 egi_quit_log();
 #endif

 printf("<-------  END  ------>\n");

	return 0;
}


