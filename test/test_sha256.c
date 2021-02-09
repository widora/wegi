/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test for SHA-256 hashing/digesting.

Speed: ~6MBps for files at tmp mem.

Midas Zhou
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <egi_math.h>
#include <egi_timer.h>
#include <egi_utils.h>

/*---------------------------
        Print help
-----------------------------*/
void print_help(const char* cmd)
{
        printf("Usage: %s [-hf:l:] msg \n", cmd);
        printf("        -h   Help \n");
        printf("        -f:  File path. then ignore input msg if any.\n");
	printf("        -l:  Test loop times, default 1. \n");
	printf("        msg  Input string. if file path is provided, then ignore. \n");
}

/*----------------
      Main()
----------------*/
int main(int argc, char **argv)
{
	int k=0;
	//unsigned char *input="hello world";
	  //B94D27B9934D3E08A52E52D7DA7DABFAC484EFE37A5380EE9088F7ACE2EFCDE9

	//unsigned char *input=(unsigned char *)"widora歪朵拉";
	  //F18D00D14163D1EF14E07C37BBF0A97F8C2444BF599082122FD2B1F11F496DDB

	//unsigned char *input=(unsigned char *)"［将行反忤之术，必须先定计谋，然后行之，又用飞钳之术以弥缝之。］"
	  //4AA4C5649636C3680859C26FDB813E73CC22903083413BA48126AB9903BDD4BD

	unsigned char *input=(unsigned char *)"［用之于人，谓用飞钳之术于诸侯也。量智能、料气势者，亦欲知其智谋能否也。枢所以主门之动静，机所以主弩之放发，言既知其诸侯智谋能否，然后立法镇其动静，制其放发，犹枢之于门，机之于弩，或先而迎之，或后而随之，皆钳其情以和之，用其意以宜之。如此则诸侯之权，可得而执，己之恩又得而固，故曰：飞钳之缀也。谓用飞钳之术连于人也。］";
	  //0108CC2172E8ACE880C65DA4F1ADDD854C5087D4277A8C9224F4B407037C7130

	char digest[8*8+1]={0};
	uint32_t hv[8];
	uint32_t len=0;

	int opt;
	int nloops=1;
	const char* fpath=NULL;
	EGI_CLOCK eclock={0};
	EGI_FILEMMAP *fmap=NULL;

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hf:l:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
                                break;
                        case 'f':
				fpath=optarg;
                                break;
			case 'l':
				nloops=atoi(optarg);
				if(nloops<1) nloops=1;
				break;
		}
	}

	/* Use builtin default test string */
	if( argc<2 ) {
		len=strlen((char *)input);
	}
	/* If no fpath, then check input string */
	else if(fpath==NULL) {
		if( optind >= argc ) {
                	print_help(argv[0]);
			exit(0);
		}

		input=(unsigned char *)argv[optind];
		len=strlen((char *)input);
	}
	/* Input file */
	else if(fpath!=NULL) {
		fmap=egi_fmap_create(fpath, 0, PROT_READ, MAP_PRIVATE);
		if(fmap==NULL) exit(0);

		input=(unsigned char *)fmap->fp;
		len=fmap->fsize;
	}
	else {
               	print_help(argv[0]);
		exit(0);
	}

	//printf("Input: %s\n", input);

 do {  /*----------------- LOOP TEST ---------------- */
//	printf("\t--- k=%d ---\n", k++);
//	printf("Start hashing...\n");
	egi_clock_start(&eclock);
        mat_sha256_digest(input, len, NULL, NULL, hv, digest);
	egi_clock_stop(&eclock);
//	printf("File: '%s', size: %dBs, Hash digest: %s\n", fpath, len, digest);
//	printf("Cost time: %ldus\n", egi_clock_readCostUsec(&eclock));

 } while(--nloops); /* END:Loop test */

	printf("%s\n", digest);

	/* Free */
	egi_fmap_free(&fmap);

	return 0;
}


