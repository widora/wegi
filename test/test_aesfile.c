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
	./test_aesfile -d -i text.lock -o text -p "hello world"
	 ( in_file and out_file name may be the same! )

Note:
1. Standard and parameters.
	   Key Size	 Block Size    Number of Rounds
	  (Nk words)     (Nb words)        (Nr)
AES-128	      4               4             10
AES-192       6               4             12
AES-256       8               4             14


2. File size < 2G, for MMAP limit.
3. In_file and out_file may be the same.
4. Encrypt and Decrypt is reversible/symmetrical to each other.
   So, you may use '-d' to encrypt and '-e' to decrypt....
5. MT7688 speed: ~1/3MBps for encryption;  ~1/5MBpsfor decryption.

	--- TEST Results ---
File: '1280x720r30.avi', size: 448981910Bs,
Hash digest: B64185F4C51D5B0F165D1F23B41761F3015B5D128A2AF8663B0ED32DA94F9ECF  Hash cost time: 117681952us
File: '1280x720r30.avi.dec', size: 448981910Bs,
Hash digest: B64185F4C51D5B0F165D1F23B41761F3015B5D128A2AF8663B0ED32DA94F9ECF  Hash cost time: 115208456us
Encrypt from Thu Dec 17 19:19:48 CST 2020 to Thu Dec 17 19:43:40 CST 2020
Decrypt from Thu Dec 17 19:43:40 CST 2020 to Thu Dec 17 20:21:24 CST 2020

File: '1280x720r30.avi', size: 448981910Bs,
Hash digest: B64185F4C51D5B0F165D1F23B41761F3015B5D128A2AF8663B0ED32DA94F9ECF Hash cost time: 117409475us
File: '1280x720r30.avi.dec', size: 448981910Bs,
Hash digest: B64185F4C51D5B0F165D1F23B41761F3015B5D128A2AF8663B0ED32DA94F9ECF Hash cost time: 115281359us
Encrypt from Thu Dec 17 20:25:21 CST 2020 to Thu Dec 17 20:49:45 CST 2020
Decrypt from Thu Dec 17 20:49:45 CST 2020 to Thu Dec 17 21:28:03 CST 2020

File: '1280x720r30.avi', size: 448981910Bs,
Hash digest: B64185F4C51D5B0F165D1F23B41761F3015B5D128A2AF8663B0ED32DA94F9ECF Hash cost time: 117307367us
File: '1280x720r30.avi.dec', size: 448981910Bs,
Hash digest: B64185F4C51D5B0F165D1F23B41761F3015B5D128A2AF8663B0ED32DA94F9ECF Hash cost time: 115097046us
Encrypt from Thu Dec 17 21:32:01 CST 2020 to Thu Dec 17 21:56:28 CST 2020
Decrypt from Thu Dec 17 21:56:28 CST 2020 to Thu Dec 17 22:34:43 CST 2020


TODO:
1. Encrypt/decrypt tail data treatment!
2. AES-128/192/256 selection.

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
	int64_t i,j,k;
	int opt;
	bool do_encrypt=true;

	char *fpath_in=NULL;
	EGI_FILEMMAP *fmap_in=NULL;
	char fpath_out[EGI_PATH_MAX+EGI_NAME_MAX]={0};
	EGI_FILEMMAP *fmap_out=NULL;

	char password[64+1]={0};	/* Though SHA256 result is 32bytes */

  const uint8_t Nb=4;		    /* Block size, 4/4/4 for AES-128/192/256 */
	uint8_t Nk=8;		    /* column number, as of 4xNk, 4/6/8 for AES-128/192/256 */
	uint8_t Nr=14;		    /* Number of rounds, 10/12/14 for AES-128/192/256 */
  	uint8_t state[4*4];	    /* State array --- ROW order -- */
	uint8_t tmpstate[4*4];	    /* Temp. state */
	uint8_t tail_size;	    /* The tail(or the last state) of a data */
	uint64_t ns;		    /* Total number of states */
	uint32_t *keywords=NULL;    /* Round keys in words, total number of keys: Nb*(Nr+1). */

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
	/* NOTE:
	 * WRITE for tail decrypt,  TODO: With PROT_WRITE, cannot allocate memory for big file in /mmc!??
	 * Use fwrite()/write() instead.
	 */
	fmap_in=egi_fmap_create(fpath_in, 0, PROT_READ, MAP_PRIVATE);
	if(fmap_in==NULL) {
		printf("Fail to mmap file '%s'.\n", fpath_in);
		exit(EXIT_FAILURE);
	}
	/* check size */
	if(fmap_in->fsize<16) {
		printf("Input file size at least to be 16bytes!\n");
		egi_fmap_free(&fmap_in);
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
	uint8_t inkey[Nb*Nk];
	/* Use your own init_hv/kv if necessary~  */
	mat_sha256_digest((const uint8_t *)password, strlen(password), NULL, NULL, (uint32_t *)inkey, NULL);
	printf("SHA256 inkey: ");
	for(i=0; i<Nk; i++)
		printf("%04X",inkey[i]);
	printf("\n");
	#endif

	/* 5. Generate Nr+1 round_keys(derived from inkey), to save in keywords. */
	aes_ExpRoundKeys(Nr, Nk, inkey, keywords);


#if 0 ////////////////////   TAIL OPTION 1 : Keep tail data unencrypted  //////////////////////

	/* 6. Calculate total state blocks needed. */
	ns=fmap_in->fsize/16;
	tail_size=fmap_in->fsize&0b1111;
	printf("fsize=%lluBytes, ns=%llu, tail_size=%dBytes\n", fmap_in->fsize, ns, tail_size);

	/* 7. Do encrypt or decrypt */
 /* -----> DO ENCRYPTION */
	if( do_encrypt ) {
		printf("Start encrypting...\n");
		/* Encrypt each state */
		for(i=0,k=0; i<ns; i++, k++) {
			/* Fill into state array: from colum[0] to colum[3] */
			//bzero(state,16);
			aes_DataToState( (uint8_t *)fmap_in->fp+i*16, state);

			/* Encryp state */
			aes_EncryptState(Nr, Nk, keywords, state);

			/* Write to fmap_out: read out from state[] column by column*/
			aes_StateToData(state, (uint8_t *)fmap_out->fp+i*16);

			/* Print msg very 10k states */
			if( k==10000 ) {
				k=0;
				printf("for(i): %llu/%llu --%%%f.1--\r", i, ns-1,(1.0*i+1)/ns);fflush(stdout);
			}
		}

		/* --- Tail data treatment --- */
		/* Just copy tail_size */
		for(i=0; i<tail_size; i++)
			fmap_out->fp[ns*16+i]=fmap_in->fp[ns*16+i];

		printf("\nFinish encrypting '%s' to '%s', with Round_Nr=%d, KeySize_Nk=%d, States_ns=%llu.\n",
		        fpath_in, fpath_out, Nr, Nk, ns);
	}
 /* -----> DO DECRYPTION */
	else { /* do_decrypt */
		printf("Start decrypting...\n");
		/* Decrypt each state */
		for(i=0,k=0; i<ns; i++,k++) {
			/* Read from state array: write to state[] from column[0] to column[3] */
			//bzero(state,16);
			aes_DataToState( (uint8_t *)fmap_in->fp+i*16, state);

			/* Decrypt state  */
			aes_DecryptState(Nr, Nk, keywords, state);

			/* Write to fmap_out: read out from state[] column by column*/
			aes_StateToData(state,(uint8_t *)fmap_out->fp+i*16);

			/* Print msg every 10k states */
			if( k==10000 ) {
				k=0;
				printf("for(i): %llu/%llu --%%%f.1--\r", i, ns-1,(1.0*i+1)/ns);fflush(stdout);
			}
		}

		/* Just copy tail_size */
		for(i=0; i<tail_size; i++)
			fmap_out->fp[ns*16+i]=fmap_in->fp[ns*16+i];

		printf("\nFinish decrypting '%s' to '%s', with Round_Nr=%d, KeySize_Nk=%d, States_ns=%llu.\n",
			fpath_in, fpath_out, Nr, Nk, ns );
	}

#else  ////////////////////   TAIL OPTION 2 : Roll back data and make tail a complete state of 16bytes  /////////////////////

	/* 6. Calculate total state blocks needed. */
	ns=fmap_in->fsize/16;
	tail_size=fmap_in->fsize&0b1111;
	if(tail_size)
		ns+=1;

	printf("fsize=%lluBytes, ns=%llu, tail_size=%dBytes\n", fmap_in->fsize, ns, tail_size);

	/* 7. Do encrypt or decrypt */
 /* -----> DO ENCRYPTION */
	if( do_encrypt ) {
		printf("Start encrypting...\n");
		/* Encrypt each state in sequence */
		for(i=0,k=0; i<ns; i++, k++) {
			/* 1. Fill into state array: from colum[0] to colum[3] */
			bzero(state,16);
			if( i < ns-1 || tail_size==0 )
				aes_DataToState( (uint8_t *)fmap_in->fp+i*16, state);
			else { /* tail data */
			        for(j=0; j<16-tail_size; j++)
					/* Roll in previous ENCRYPTED data */
		                	state[(j%4)*4+j/4]=*((uint8_t *)(fmap_out->fp)+(fmap_out->fsize)-16+j);
			        for(j=16-tail_size; j<16; j++)
		                	state[(j%4)*4+j/4]=*((uint8_t *)(fmap_in->fp)+(fmap_in->fsize)-16+j);
			}

			/* 2. Encryp state */
			aes_EncryptState(Nr, Nk, keywords, state);

			/* 3. Write to fmap_out: read out from state[] column by column*/
			if( i < ns-1 || tail_size==0 )
				aes_StateToData(state, (uint8_t *)fmap_out->fp+i*16);
			else {
				/* Tail state, parts of previou state encrypted twice. */
				aes_StateToData(state, (uint8_t *)(fmap_out->fp)+(fmap_out->fsize)-16);
			}

			/* 4. Print msg very 10k states */
			if( k==10000 ) {
				k=0;
				printf("for(i): %llu/%llu --%%%f.1--\r", i, ns-1,(1.0*i+1)/ns);fflush(stdout);
			}
		}

		printf("\nFinish encrypting '%s' to '%s', with Round_Nr=%d, KeySize_Nk=%d, States_ns=%llu.\n",
		        fpath_in, fpath_out, Nr, Nk, ns);
	}
 /* -----> DO DECRYPTION */
	else { /* do_decrypt */
		printf("Start decrypting...\n");
		/* Decrypt from the tail state...! */
		for(i=ns-1,k=0; i>=0; i--,k++) {
			/* 1. Read from state array: write to state[] from column[0] to column[3] */
			bzero(state,16);
			if(i==ns-1 && tail_size)  /* Decrypt the tail state first! */
				aes_DataToState( (uint8_t *)(fmap_in->fp)+(fmap_in->fsize)-16, state);
			/* Put part of previously encrpted tmpstate to state */
			else if(i==ns-2 && tail_size) {
				aes_DataToState( (uint8_t *)(fmap_in->fp)+i*16, state);
				/* Replace aft. part of state with fore. part of tmpstate */
				for(j=0; j<16-tail_size; j++)
					state[((j+tail_size)%4)*4+(j+tail_size)/4]=tmpstate[(j%4)*4+j/4];
			}
			else {
				aes_DataToState( (uint8_t *)(fmap_in->fp)+i*16, state);
			}

			/* 2. Decrypt state  */
			aes_DecryptState(Nr, Nk, keywords, state);

			/* 3. Write to fmap_out: read out from state[] column by column*/
			if(i==ns-1 && tail_size ) { /* The last tail state */
			        for(j=0; j<16-tail_size; j++) {
					/* Save to temp state, to put back to state before next decryption, see above */
					tmpstate[(j%4)*4+j/4]=state[(j%4)*4+j/4];
					/* Roll out to previous ENCRYPTED data! */
		                	//fmap_in->fp[fmap_in->fsize-16+j]=state[(j%4)*4+j/4];
				}
			        for(j=16-tail_size; j<16; j++)
					fmap_out->fp[fmap_out->fsize-16+j]=state[(j%4)*4+j/4];
			}
			else {
				aes_StateToData(state, (uint8_t *)(fmap_out->fp)+i*16);
			}

			/* 4. Print msg every 10k states */
			if( k==10000 ) {
				k=0;
				printf("for(i): %llu/%llu --%%%f.1--\r", i, ns-1,(1.0*i+1)/ns);fflush(stdout);
			}
		}

		printf("\nFinish decrypting '%s' to '%s', with Round_Nr=%d, KeySize_Nk=%d, States_ns=%llu.\n",
			fpath_in, fpath_out, Nr, Nk, ns );
	}

#endif /////////// END TAIL OPTIONS //////////

	/* 8. Free resource */
	egi_fmap_free(&fmap_in);
	egi_fmap_msync(fmap_out);
	egi_fmap_free(&fmap_out);
	free(keywords);

	return 0;
}

