/*---------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License version 2 as published by the Free Software
Foundation.

AES-128-CBC encrypto/decrypto with openssl API.

Usage:
        ./test_aescbc -e -i test.txt  -o test.en
        ./test_aescbc -d -i test.en  -o test.de

as comparing with openssl command:
openssl enc -aes-128-cbc -e -in test.txt -out test.en -iv 30303030303030303030303061c9391a -K 0fc6a7c07620b809a5ae77e96863352b
openssl enc -aes-128-cbc -d -in test.en -out test.de -iv 30303030303030303030303061c9391a -K 0fc6a7c07620b809a5ae77e96863352b


Reference:
https://www.cnblogs.com/ylgcgbd/p/14897039.html


Note:
1. 16==AES_BLOCK_SIZE, 14==AES_MAXNR, as defined in <openssl/aes.h>.
2. AES_cbc_encrypt() implements zero padding mode by default, otherwise the caller
   MUST do the padding with desired mode(PKCS7 etc.) before calling the function.

			( --- Default as zero padding --- )
Example: Notice the 0_tails in the decrypted file.
	root@Widora:/tmp# hexdump -C test.txt
	00000000  31 32 33 34 35 36 37 38  0a                       |12345678.|
	00000009

	./test_aescbc -e -i test.txt  -o test.en
	./test_aescbc -d -i test.en  -o test.de

	root@Widora:/tmp# hexdump -C test.de
	00000000  31 32 33 34 35 36 37 38  0a 00 00 00 00 00 00 00  |12345678........|
	00000010

  ----- Compare with openssl ----- Note: openssl NOT support input lenthg<16Bs!
  openssl enc -aes-128-cbc -d -in test2.en -out test2.de -iv 30303030303030303030303061c9391a -K 0fc6a7c07620b809a5ae77e96863352b
  bad decrypt
  1997751368:error:06065064:lib(6):func(101):reason(100):NA:0:

2. Here we implement 'PKCS7' padding mechanism. It means IF the input file has length of
   multiples of 16(block_size), the padding length will be (16)bytes, NOT (0)bytes!!!
   OR, the padding lenght will be (16-length%16)bytes, and it's just the
   number of bytes added at tail to fillup datasize to be multiples of 16(block_size).

				(---Show Paddings---)
root@Widora:/tmp# ./test_aescbc -d -i test2.en -o test2.de
root@Widora:/tmp# cat test2.de
1234567890abcdef12345678
root@Widora:/tmp# hexdump -C test2.de
00000000  31 32 33 34 35 36 37 38  39 30 61 62 63 64 65 66  |1234567890abcdef|
00000010  31 32 33 34 35 36 37 38  0a 07 07 07 07 07 07 07  |12345678........|   <---- 7bytes padding.
00000020
root@Widora:/tmp# rm test2.de
rm: remove 'test2.de'? y
root@Widora:/tmp# openssl enc -aes-128-cbc -d -in test2.en -out test2.de -iv 30303030303030303030303061c9391a -K 0fc6a7c07620b809a5ae77e96863352b
root@Widora:/tmp# hexdump -C test2.de
00000000  31 32 33 34 35 36 37 38  39 30 61 62 63 64 65 66  |1234567890abcdef|
00000010  31 32 33 34 35 36 37 38  0a                       |12345678.|
00000019


TODO:
1. Return 0 length file if fails.

Journal:
2021-12-28: Create the file.


Midas Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <openssl/aes.h>
#include <egi_cstring.h>
#include <egi_utils.h>

const char *fpath_in; //="./cryp5.mp4";
const char *fpath_out; //="./decryp.mp4";

/* strlen of strKey or strIV MUST >=2*16 */
char *strKey="0fc6a7c07620b809a5ae77e96863352b__--";    /* Key string */
char *strIV="30303030303030303030303061c9391a==";	/* Init vector string */

void print_help(const char *name)
{
        printf("Usage: %s [-hedi:o:]\n", name);
        printf("-h     AES-128-CBC mode encrypto/decrypto.\n");
        printf("-e     Encrypt(default)\n");
        printf("-d     Decrypt\n");
	printf("-i:    In file path\n");
	printf("-o:    Out file path\n");
}

/*---------------------------
       	      MAIN
----------------------------*/
int main(int argc, char **argv)
{
	/* 16 == AES_BLOCK_SIZE */

        EGI_FILEMMAP *fmap_in=NULL;
        EGI_FILEMMAP *fmap_out=NULL;

	AES_KEY enckey;
	AES_KEY deckey;
	unsigned char tmpiv[16];

	unsigned char *ukey;
	unsigned char *uiv;

	int encode=AES_ENCRYPT;
	size_t length;  /* For dencrypt: it's input file size at first, then erase padding and adjust it.
		 	 * For encrypt: it's the output file size.
			 */

        /* Parse input option */
        int opt;
        while( (opt=getopt(argc,argv,"hedi:o:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
                                break;
                        case 'e':
				encode=AES_ENCRYPT;
                                break;
                        case 'd':
				encode=AES_DECRYPT;
				break;
			case 'i':
				fpath_in=optarg;
				break;
			case 'o':
				fpath_out=optarg;
				break;
		}
	}

	/* Check input file path */
	if(fpath_in==NULL || fpath_out==NULL) {
		printf("Please provide in/out file path!\n");
		exit(0);
	}

        /* Fmmap in file */
        fmap_in=egi_fmap_create(fpath_in, 0, PROT_READ, MAP_SHARED);
        if(fmap_in==NULL) {
                printf("Fail to mmap file '%s'.\n", fpath_in);
                exit(EXIT_FAILURE);
        }

	/* 1. Compute out file length, considering padding. */
	if(encode==AES_ENCRYPT)
		length=(fmap_in->fsize/16+1)*16; /* PKCS7 padding */
	/* 2. Check file size for AES_DECRYPT */
	else if( fmap_in->fsize%16 !=0 ) {
		printf("Data size error!\nFor AES-CBC-128 decrypt, input data size MUST be multiples of 16Bs!\n");
		exit(EXIT_FAILURE);
	}
	else
		length=fmap_in->fsize;

	/* 3. Fmmap out file */
        fmap_out=egi_fmap_create(fpath_out, length, PROT_READ|PROT_WRITE, MAP_SHARED);
        if(fmap_out==NULL) {
                printf("Fail to mmap file '%s'.\n", fpath_out);
                exit(EXIT_FAILURE);
        }

	/* 4. Convert string to HEX */
	if(strlen(strKey)<16*2 || strlen(strIV)<16*2) {
		printf("strKey and strIV MUST has len>=16*2!\n");
		exit(EXIT_FAILURE);
	}
	ukey=egi_str2hex(strKey, 16);
	uiv=egi_str2hex(strIV, 16);
	if(ukey==NULL||uiv==NULL)
		exit(EXIT_FAILURE);

/* TEST:  egi_str2hex()/egi_hex2str() ----------- */
	int i;
	char *tmp;
	printf("ukey[]: ");
	for(i=0; i<16; i++)
		printf("%02x", ukey[i]);
	printf("\n");
	free(tmp);
	printf("Hex2str(ukey): %s\n", tmp=egi_hex2str(ukey, 16));

	printf("uiv[]: ");
	for(i=0; i<16; i++)
		printf("%02x", uiv[i]);
	printf("\n");
	printf("Hex2str(uiv): %s\n", tmp=egi_hex2str(uiv, 16));
	free(tmp);
/*------------*/

	/* 5. Prep encrypt_key/decrypt_key */
	if(encode==AES_ENCRYPT)
		AES_set_encrypt_key(ukey, 128, &enckey);
	else
		AES_set_decrypt_key(ukey, 128, &deckey);

	/* 6. Prepare decrypt init vector */
	memcpy(tmpiv, uiv, 16);

	/* 7. Separate multiples_of_16 and remainder from data size.  16=AES_BLOCK_SIZE */
	int lenm16=(fmap_in->fsize/16)*16; /* PART1: Multipls of 16. */
	int lenr16=fmap_in->fsize%16;	   /* PART2: Remainder */
	printf("lenm16=%dBs, lenr16=%dBs\n", lenm16, lenr16);

	/* 8. [ENCRYPT+DECRYPT]:  PART1 (lenm16 part). notice that for decrypto, lenr16==0! */
	AES_cbc_encrypt( (unsigned char *)fmap_in->fp,
			 (unsigned char *)fmap_out->fp, lenm16,
			 encode==AES_ENCRYPT?(&enckey):(&deckey), tmpiv, encode);  /* tmpiv is always changing! */

	/* 9. [ENCRYPT]: PKCS7 padding,  padding with 16Bs(block_size) if remainder is 0bytes. */
	if(encode==AES_ENCRYPT) {
		unsigned char tmp[16]; /* 16: AES_BLOCK_SIZE */

		/* 9.1 Remainder data, padding with of number of bytes added, as 16-lenr16. */
		if(lenr16==0) {   /* 16==AES_BLOCK_SIZE */
			lenr16=16;  /* <---- pad whole block size here with (16)!!! */
			memset(tmp, (unsigned char )(16), sizeof(tmp));
			// remainder==0
		}
		else {
			memset(tmp, (unsigned char )(16-lenr16), sizeof(tmp)); /* <--- padding with (16-lenr16) */
			memcpy(tmp, (unsigned char *)fmap_in->fp+lenm16, lenr16);
		}

		/* 9.2 ENCRYPT: PART2 (lenr16+padding part). notice that for decrypto, lenr16==0! */
	 	/* (*in,  *out,  length, *key, *ivec, enc) */
		AES_cbc_encrypt( tmp, (unsigned char *)fmap_out->fp+lenm16, 16, /* NOT lenr16!! AES_cbc_encrypt() pad with 0 ONLY! */
				 encode==AES_ENCRYPT?(&enckey):(&deckey), tmpiv, encode);  /* tmpiv is always changing! */
	}
	/* 10. [DECRYPT]: PKS5/7 padding, erase padding bytes.. */
	else if(encode==AES_DECRYPT) {
		/* The last byte is padding size */
		length -=(int)fmap_out->fp[lenm16-1];  /* <---- adjust file length  */
		egi_fmap_resize(fmap_out, length);
	}

	/* 11. sync fmap */
	egi_fmap_msync(fmap_out);


	/* Free */
	egi_fmap_free(&fmap_in);
	egi_fmap_free(&fmap_out);

	free(ukey);
	free(uiv);

	return 0;
}
