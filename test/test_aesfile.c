/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test for AES encryption (RIJNDAEL symmetric key encryption algorithm).
Reference:
	1. Advanced Encryption Standard (AES) (FIPS PUB 197)
	2. Advanced Encryption Standard by Example  (by Adam Berent)

Usage Example:
	./test_aesfile -e -i text -o text.lock -p "hello world"
	./test_aesfile -d -i text.lock -o text -p "hello wordl"

Note:
1. Standard and parameters.
	   Key Size	 Block Size    Number of Rounds
	  (Nk words)     (Nb words)        (Nr)
AES-128	      4               4             10
AES-192       6               4             12
AES-256       8               4             14

2. File size < 2G, for MMAP limit.


Midas Zhou
https://github.com/widora/wegi
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <egi_aes.h>
#include <egi_utils.h>
#include <egi_math.h>

/* Print help */
void print_help(const char* cmd)
{
        printf("Usage: %s [-hfi:o:edp:] [FILE] \n", cmd);
        printf("        -h   This help\n");
        printf("        -i:  In file. (Not necessary) \n");
	printf("	     If omit, use [FILE].\n");
        printf("        -o:  Out file. (Not necessary) \n");
	printf("	     If omit, append '.enc' or '.dec' to the input file name.\n");
        printf("        -e   Do encrypt (Default).\n");
        printf("        -d   Do decrypt.\n");
	printf("	-p   Password \n");
	printf("     [FILE]  Full file path.\n");
}


int main(int argc, char **argv)
{
	int i;
	int opt;
	bool do_encrypt=true;

	char *fpath_in=NULL;
	EGI_FILEMMAP *fmap_in=NULL;
	char fpath_out[EGI_PATH_MAX+EGI_NAME_MAX]={0};
	EGI_FILEMMAP *fmap_out=NULL;

	char password[64+1]={0};	/* Though SHA256 result is 32bytes */

	uint8_t Nk=4;		    /* column number, as of 4xNk, 4/6/8 for AES-128/192/256 */
  const uint8_t Nb=4;		    /* Block size, 4/4/4 for AES-128/192/256 */
	uint8_t Nr=10;		    /* Number of rounds, 10/12/14 for AES-128/192/256 */
  	uint8_t state[4*4];	    /* State array --- ROW order -- */
	uint64_t ns;		    /* Total number of states */
	uint32_t *keywords=NULL;    /* Round keys, in word. */

	const uint8_t input_msg[]= {
		0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
	};

        /* 1. Parse input option */
        while( (opt=getopt(argc,argv,"hi:o:edp:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
                                break;
                        case 'i':
				fpath_in=optarg;
                                break;
                        case 'o':
				strncpy(fpath_out, optarg, sizeof(fpath_out)-1);
                                break;
			case 'e':
				do_encrypt=true; /* Default */
				break;
			case 'd':
				do_encrypt=false;
				break;
			case 'p':
				strncpy(password, optarg,sizeof(password)-1);
				break;
		}
	}

	/* Check password */
	if( strlen(password) == 0 ) {
		printf("Please enter password! by option -p: \n");
		exit(EXIT_FAILURE);
	}

	/* Check in_file path. */
        if( fpath_in == NULL && optind < argc )
		fpath_in=argv[optind];

	/* Check out_file path. */
	if( strlen(fpath_out) == 0 ) {
		if( do_encrypt )
			sprintf(fpath_out,"%s.enc",fpath_in);
		else
			sprintf(fpath_out,"%s.dec",fpath_in);
	}

	/* 2. Fmmap in/out file */
	fmap_in=egi_fmap_create(fpath_in, 0, PROT_READ, MAP_PRIVATE);
	if(fmap_in==NULL) {
		printf("Fail to mmap file '%s'.\n", fpath_in);
		exit(EXIT_FAILURE);
	}

	fmap_out=egi_fmap_create(fpath_out, fmap_in->fsize, PROT_WRITE, MAP_SHARED);
	if(fmap_out==NULL) {
		printf("Fail to mmap file '%s'.\n", fpath_out);
		exit(EXIT_FAILURE);
	}

	/* 3. Allocate keywords[], depending on Nb and Nr. */
	keywords=calloc(1, Nb*(Nr+1));
	if(keywords==NULL)
		exit(EXIT_FAILURE);

  	/* 4. Generate inkey from password : ( With Your Private Method ) */
	#if 0 /* A test inkey */
        const uint8_t inkey[4*8]= {  /* Nb*Nk */
                0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
                0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
        };
	#else
	uint8_t inkey[4*8];
	/* Use your own init_hv/kv if necessary~  */
	mat_sha256_digest((const uint8_t *)password, strlen(password), NULL, NULL, (uint32_t *)inkey, NULL);
	#endif

	/* 5. Generate Nr+1 round_keys(derived from inkey), to save in keywords. */
	aes_ExpRoundKeys(Nr, Nk, inkey, keywords);

	/* 6. Calculate total state blocks. */
	ns=(fmap_in->fsize+15)/16;

	/* 7. Do encrypt or decrypt */
	if( do_encrypt ) {
		/* Encrypt each state */
		for(i=0; i<ns; i++) {
			/* Fill into state array: from colum[0] to colum[3] */
			bzero(state,16);
			aes_DataToState( (uint8_t *)fmap_in->fp+i*16,state);

			/* Encryp state */
			aes_EncryptState(Nr, Nk, keywords, state);

			/* Write to fmap_out: read out from state[] column by column*/
			aes_StateToData(state,(uint8_t *)fmap_out->fp+i*16);
		}

		printf("Finish encrypting '%s' to '%s', with Round_Nr=%d, KeySize_Nk=%d, States_ns=%llu.\n",
		        fpath_in, fpath_out, Nr, Nk, ns);
	}
	else { /* do_decrypt */
		/* Decrypt each state */
		for(i=0; i<ns; i++) {
			/* Fill into state array: write to state[] from column[0] to column[3] */
			bzero(state,16);
			aes_DataToState( (uint8_t *)fmap_in->fp+i*16,state);

			/* Decrypt state  */
			aes_DecryptState(Nr, Nk, keywords, state);

			/* Write to fmap_out: read out from state[] column by column*/
			aes_StateToData(state,(uint8_t *)fmap_out->fp+i*16);
		}

		printf("Finish decrypting '%s' to '%s', with Round_Nr=%d, KeySize_Nk=%d, States_ns=%llu.\n",
			fpath_in, fpath_out, Nr, Nk, ns );
	}

	/* 8. Free resource */
	egi_fmap_free(&fmap_in);
	egi_fmap_free(&fmap_out);
	free(keywords);

	return 0;
}

