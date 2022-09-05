/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Reference:
	1. Advanced Encryption Standard (AES) (FIPS PUB 197)
	2. Advanced Encryption Standard by Example  (by Adam Berent)


Note:
1. Standard and parameters.
	   Key Size	 Block Size    Number of Rounds
	  (Nk words)     (Nb words)        (Nr)
AES-128	      4               4             10
AES-192       6               4             12
AES-256       8               4             14

TODO:
1. To utilize cryptograhpic hardware accelerator. (to install
   kmod-cryptodev to create /dev/crypto... )
2. egi_AES128CBC_encrypt() ONLY abt. 1/7 speed of AES_cbc128_encrypt()
   to optimize it...

Journal:
2022-01-01:
	1. Add AES_cbc128_encrypt().
2022-01-05:
	1. Add egi_AES128CBC_encrypt().

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
https://github.com/widora/wegi
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <egi_debug.h>
#include <egi_aes.h>
#include <egi_cstring.h>


/////////////////////////  OpenSSL API  //////////////////////////
#include <openssl/aes.h>

/*--------------------------------------------------------
Encrypt/decrypt data with AES_CBC_128 + PKCS7 padding, by
calling OpenSSL API AES_cbc_encrypt().

Note:
1. 16==AES_BLOCK_SIZE.
2. If outdata is created/mmaped with egi_fmap_create(), its
   initial size shall be the same as inszie, and later to
   call egi_fmap_resize() to adjust same as final outsize.

@indata: 	Input data
@insize: 	Input data size
@outdata: 	Output data
		!!!--- CAUTION ---!!!
		The Caller MUST ensure enough sapce.
@outsize:	Pointer to output data size
		If returned outsize is negative or ridiculous, it means
		the original data error, OR encryption method unsupported.
@ukey:  	The key
@uiv:   	The init vector
@encode:	0 as AES_DECRYPT
		!0 as AES_ENCRYPT
		//# define AES_ENCRYPT     1
		//# define AES_DECRYPT     0

Return:
	0	OK
	<0	Fails
-------------------------------------------------------------*/
int AES_cbc128_encrypt(unsigned char *indata, size_t insize,  unsigned char *outdata, size_t *outsize,
			const unsigned char *ukey, const unsigned char *uiv, int encode)
{
	AES_KEY enckey;
	AES_KEY deckey;
	unsigned char tmpiv[16];  /* varing IV, for each block_size operation. */

/* For TEST: ------- */
	char *strtmp=NULL;

	if(indata==NULL || outdata==NULL || ukey==NULL || uiv==NULL)
		return -1;


	/* 0. Check encode */
	if(encode!=0)
		encode=AES_ENCRYPT;
	else
		encode=AES_DECRYPT;

	/* 1. Prep encrypt_key/decrypt_key */
	if(encode==AES_ENCRYPT)
		AES_set_encrypt_key(ukey, 128, &enckey);
	else
		AES_set_decrypt_key(ukey, 128, &deckey);

	/* 2. Prepare decrypt init vector */
	memcpy(tmpiv, uiv, 16);

	/* 3. Separate multiples_of_16 and remainder from data size.  16=AES_BLOCK_SIZE */
	int lenm16=(insize/16)*16; /* PART1: Multipls of 16. */
	int lenr16=insize%16;	   /* PART2: Remainder */
//	printf("lenm16=%dBs, lenr16=%dBs\n", lenm16, lenr16);

#if 0 /* TEST iv---> */
	egi_dpstd("Init tmpiv[]: %s\n", strtmp=egi_hex2str((unsigned char *)tmpiv, 16));
	free(strtmp);
#endif

//	egi_dpstd("Start %s ...\n", encode==AES_ENCRYPT?"encrypting":"decrypting");

	/* 4. [ENCRYPT+DECRYPT]:  PART1 (lenm16 part). notice that for decrypto, lenr16==0! */
	AES_cbc_encrypt( (unsigned char *)indata,
			 (unsigned char *)outdata, lenm16,
			 encode==AES_ENCRYPT?(&enckey):(&deckey), tmpiv, encode);  /* tmpiv is always changing! */

#if 0 /* TEST iv---> */
	printf("Aft 8: tmpiv[]: %s\n", strtmp=egi_hex2str((unsigned char *)tmpiv, 16));
	free(strtmp);
#endif

	/* 5. [ENCRYPT]: PKCS7 padding,  padding with 16Bs(block_size) if remainder is 0bytes. */
	if(encode==AES_ENCRYPT) {
		unsigned char tmp[16]; /* 16: AES_BLOCK_SIZE */

		/* 5.1 Remainder data, padding with of number of bytes added, as 16-lenr16. */
		if(lenr16==0) {   /* 16==AES_BLOCK_SIZE */
			lenr16=16;  /* <---- pad whole block size here with (16)!!! */
			memset(tmp, (unsigned char )(16), sizeof(tmp));
			// remainder==0
		}
		else {
			memset(tmp, (unsigned char )(16-lenr16), sizeof(tmp)); /* <--- padding with (16-lenr16) */
			memcpy(tmp, (unsigned char *)indata+lenm16, lenr16); // remainder
		}

		/* 5.2 ENCRYPT: PART2 (lenr16+padding part). notice that for decrypto, lenr16==0! */
	 	/* (*in,  *out,  length, *key, *ivec, enc) */
		AES_cbc_encrypt( tmp, (unsigned char *)outdata+lenm16, 16, /* NOT lenr16!! AES_cbc_encrypt() pad with 0 ONLY! */
				 encode==AES_ENCRYPT?(&enckey):(&deckey), tmpiv, encode);  /* tmpiv is always changing! */

#if 0 /* TEST iv---> */
		printf("Aft 9.2: tmpiv[]: %s\n", strtmp=egi_hex2str((unsigned char *)tmpiv, 16));
		free(strtmp);
#endif

		/* 5.3 Cal. outsize */
		if(lenr16==0)
			*outsize=insize+16;
		else
			*outsize=insize+(16-lenr16);
	}
	/* 6. [DECRYPT]: PKS5/7 padding, erase padding bytes.. */
	else if(encode==AES_DECRYPT) {
		/* 6.1 Cal. outsize.  Value of the last byte is ALWAYS the padding size */
		*outsize=insize-(int)outdata[lenm16-1];  /* <---- adjust file length  */
	}

	return 0;
}


//////////////////////////////////  EGi  ////////////////////////////////

/* S_BOX */
static const uint8_t sbox[256] = {
/* 0     1    2      3     4    5     6     7      8     9     A     B     C     D     E     F  */
  0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
  0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
  0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
  0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
  0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
  0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
  0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
  0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
  0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
  0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
  0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
  0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
  0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
  0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
  0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
  0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
};

/* Reverse S_BOX */
static const uint8_t rsbox[256] = {
/* 0     1    2      3     4    5     6     7      8     9     A     B     C     D     E     F  */
  0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38, 0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB,
  0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87, 0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB,
  0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D, 0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
  0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2, 0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25,
  0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92,
  0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA, 0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
  0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A, 0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
  0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02, 0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B,
  0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA, 0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
  0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85, 0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E,
  0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89, 0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B,
  0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20, 0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
  0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31, 0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F,
  0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D, 0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
  0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0, 0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
  0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26, 0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D
};

static const uint8_t Etab[256]= {
/* 0     1      2    3     4     5     6     7      8     9     A     B     C     D     E     F  */
  0x01, 0x03, 0x05, 0x0F, 0x11, 0x33, 0x55, 0xFF, 0x1A, 0x2E, 0x72, 0x96, 0xA1, 0xF8, 0x13, 0x35,
  0x5F, 0xE1, 0x38, 0x48, 0xD8, 0x73, 0x95, 0xA4, 0xF7, 0x02, 0x06, 0x0A, 0x1E, 0x22, 0x66, 0xAA,
  0xE5, 0x34, 0x5C, 0xE4, 0x37, 0x59, 0xEB, 0x26, 0x6A, 0xBE, 0xD9, 0x70, 0x90, 0xAB, 0xE6, 0x31,
  0x53, 0xF5, 0x04, 0x0C, 0x14, 0x3C, 0x44, 0xCC, 0x4F, 0xD1, 0x68, 0xB8, 0xD3, 0x6E, 0xB2, 0xCD,
  0x4C, 0xD4, 0x67, 0xA9, 0xE0, 0x3B, 0x4D, 0xD7, 0x62, 0xA6, 0xF1, 0x08, 0x18, 0x28, 0x78, 0x88,
  0x83, 0x9E, 0xB9, 0xD0, 0x6B, 0xBD, 0xDC, 0x7F, 0x81, 0x98, 0xB3, 0xCE, 0x49, 0xDB, 0x76, 0x9A,
  0xB5, 0xC4, 0x57, 0xF9, 0x10, 0x30, 0x50, 0xF0, 0x0B, 0x1D, 0x27, 0x69, 0xBB, 0xD6, 0x61, 0xA3,
  0xFE, 0x19, 0x2B, 0x7D, 0x87, 0x92, 0xAD, 0xEC, 0x2F, 0x71, 0x93, 0xAE, 0xE9, 0x20, 0x60, 0xA0,
  0xFB, 0x16, 0x3A, 0x4E, 0xD2, 0x6D, 0xB7, 0xC2, 0x5D, 0xE7, 0x32, 0x56, 0xFA, 0x15, 0x3F, 0x41,
  0xC3, 0x5E, 0xE2, 0x3D, 0x47, 0xC9, 0x40, 0xC0, 0x5B, 0xED, 0x2C, 0x74, 0x9C, 0xBF, 0xDA, 0x75,
  0x9F, 0xBA, 0xD5, 0x64, 0xAC, 0xEF, 0x2A, 0x7E, 0x82, 0x9D, 0xBC, 0xDF, 0x7A, 0x8E, 0x89, 0x80,
  0x9B, 0xB6, 0xC1, 0x58, 0xE8, 0x23, 0x65, 0xAF, 0xEA, 0x25, 0x6F, 0xB1, 0xC8, 0x43, 0xC5, 0x54,
  0xFC, 0x1F, 0x21, 0x63, 0xA5, 0xF4, 0x07, 0x09, 0x1B, 0x2D, 0x77, 0x99, 0xB0, 0xCB, 0x46, 0xCA,
  0x45, 0xCF, 0x4A, 0xDE, 0x79, 0x8B, 0x86, 0x91, 0xA8, 0xE3, 0x3E, 0x42, 0xC6, 0x51, 0xF3, 0x0E,
  0x12, 0x36, 0x5A, 0xEE, 0x29, 0x7B, 0x8D, 0x8C, 0x8F, 0x8A, 0x85, 0x94, 0xA7, 0xF2, 0x0D, 0x17,
  0x39, 0x4B, 0xDD, 0x7C, 0x84, 0x97, 0xA2, 0xFD, 0x1C, 0x24, 0x6C, 0xB4, 0xC7, 0x52, 0xF6, 0x01
};

static const uint8_t Ltab[256]= {
/* 0     1      2    3     4     5     6     7      8     9     A     B     C     D     E     F  */
  0x0,  0x0,  0x19, 0x01, 0x32, 0x02, 0x1A, 0xC6, 0x4B, 0xC7, 0x1B, 0x68, 0x33, 0xEE, 0xDF, 0x03, // 0
  0x64, 0x04, 0xE0, 0x0E, 0x34, 0x8D, 0x81, 0xEF, 0x4C, 0x71, 0x08, 0xC8, 0xF8, 0x69, 0x1C, 0xC1, // 1
  0x7D, 0xC2, 0x1D, 0xB5, 0xF9, 0xB9, 0x27, 0x6A, 0x4D, 0xE4, 0xA6, 0x72, 0x9A, 0xC9, 0x09, 0x78, // 2
  0x65, 0x2F, 0x8A, 0x05, 0x21, 0x0F, 0xE1, 0x24, 0x12, 0xF0, 0x82, 0x45, 0x35, 0x93, 0xDA, 0x8E, // 3
  0x96, 0x8F, 0xDB, 0xBD, 0x36, 0xD0, 0xCE, 0x94, 0x13, 0x5C, 0xD2, 0xF1, 0x40, 0x46, 0x83, 0x38, // 4
  0x66, 0xDD, 0xFD, 0x30, 0xBF, 0x06, 0x8B, 0x62, 0xB3, 0x25, 0xE2, 0x98, 0x22, 0x88, 0x91, 0x10, // 5
  0x7E, 0x6E, 0x48, 0xC3, 0xA3, 0xB6, 0x1E, 0x42, 0x3A, 0x6B, 0x28, 0x54, 0xFA, 0x85, 0x3D, 0xBA, // 6
  0x2B, 0x79, 0x0A, 0x15, 0x9B, 0x9F, 0x5E, 0xCA, 0x4E, 0xD4, 0xAC, 0xE5, 0xF3, 0x73, 0xA7, 0x57, // 7
  0xAF, 0x58, 0xA8, 0x50, 0xF4, 0xEA, 0xD6, 0x74, 0x4F, 0xAE, 0xE9, 0xD5, 0xE7, 0xE6, 0xAD, 0xE8, // 8
  0x2C, 0xD7, 0x75, 0x7A, 0xEB, 0x16, 0x0B, 0xF5, 0x59, 0xCB, 0x5F, 0xB0, 0x9C, 0xA9, 0x51, 0xA0, // 9
  0x7F, 0x0C, 0xF6, 0x6F, 0x17, 0xC4, 0x49, 0xEC, 0xD8, 0x43, 0x1F, 0x2D, 0xA4, 0x76, 0x7B, 0xB7, // A
  0xCC, 0xBB, 0x3E, 0x5A, 0xFB, 0x60, 0xB1, 0x86, 0x3B, 0x52, 0xA1, 0x6C, 0xAA, 0x55, 0x29, 0x9D, // B
  0x97, 0xB2, 0x87, 0x90, 0x61, 0xBE, 0xDC, 0xFC, 0xBC, 0x95, 0xCF, 0xCD, 0x37, 0x3F, 0x5B, 0xD1, // C
  0x53, 0x39, 0x84, 0x3C, 0x41, 0xA2, 0x6D, 0x47, 0x14, 0x2A, 0x9E, 0x5D, 0x56, 0xF2, 0xD3, 0xAB, // D
  0x44, 0x11, 0x92, 0xD9, 0x23, 0x20, 0x2E, 0x89, 0xB4, 0x7C, 0xB8, 0x26, 0x77, 0x99, 0xE3, 0xA5, // E
  0x67, 0x4A, 0xED, 0xDE, 0xC5, 0x31, 0xFE, 0x18, 0x0D, 0x63, 0x8C, 0x80, 0xC0, 0xF7, 0x70, 0x07  // F
};

static const uint32_t Rcon[15]= {
  0x01000000,
  0x02000000,
  0x04000000,
  0x08000000,
  0x10000000,
  0x20000000,
  0x40000000,
  0x80000000,
  0x1B000000,
  0x36000000,
  0x6C000000,
  0xD8000000,
  0xAB000000,
  0x4D000000,
  0x9A000000
};

/*----------------------
Print a state[4*4]
-----------------------*/
void aes_PrintState(const uint8_t *s)
{
	int i,j;
	for(i=0; i<4; i++) {
		for(j=0; j<4; j++) {
			printf("%02x",s[i*4+j]);
			//printf("'%c'",s[i*4+j]); /* !!! A control key MAY erase previous chars on screen! !!! */
			printf(" ");
		}
		printf("\n");
	}
	printf("\n");
}


/* ---------------------------------------------
Write/Read data[] into/from a state[4*4]:


@data[16]:   Data in sequence of
	     a0a1a2a3a4a5a6a7a8.....a14a15
@iv[16]:     Same as above.
@state[4*4]: A state Matrix in sequence of
	     a0a4a8a12.....a11a15, which represents
	     a matrix of:
		a0 a4 a8  a12
		a1 a5 a9  a13
		a2 a6 a0  a14
		a3 a7 a11 a15

	!!!--- WARNING ---!!!
state[] INDEX is NOT in sequence!!!

Return:
	0	OK
	<0	Fails
-----------------------------------------------*/
inline int aes_DataToState(const uint8_t *data, uint8_t *state)
{
	int k;
//	if(data==NULL || state==NULL)
//		return -1;

	for(k=0; k<16; k++) {
        	//state[(k%4)*4+k/4]=data[k];
        	state[( (k&0x3)<<2 )+(k>>2)]=data[k];
	}

	return 0;
}
inline int aes_StateToData(const uint8_t *state, uint8_t *data)
{
	int k;
//	if(data==NULL || state==NULL)
//		return -1;

         for(k=0; k<16; k++) {
         	//data[k]=state[(k%4)*4+k/4];
        	data[k]=state[( (k&0x3)<<2 )+(k>>2)];
	}

	return 0;
}
inline int aes_StateXorIv(uint8_t *state, const uint8_t *iv)
{
	int k;
//	if(state==NULL || iv==NULL)
//		return -1;

	for(k=0; k<16; k++)
		state[k] = state[k]^iv[k];

	return 0;
}


/*------------------------------
Shift operation of the state.

@state[4*4]

Return:
	0	OK
	<0	Fails
-------------------------------*/
int aes_ShiftRows(uint8_t *state)
{
	int j,k;
	uint8_t tmp;

	if(state==NULL)
		return -1;

	for(k=0; k<4; k++) {
		/* each row shift k times */
		for(j=0; j<k; j++) {
			tmp=*(state+(k<<2));   //tmp=*(state+4*k);   /* save the first byte */
			memmove(state+(k<<2), state+(k<<2)+1, 3); //memmove(state+4*k, state+4*k+1, 3);
			*(state+(k<<2)+3)=tmp; // *(state+4*k+3)=tmp; /* set the last byte */
		}
	}
	return 0;
}

/*------------------------------
Shift operation of the state.

@state[4*4]

Return:
	0	OK
	<0	Fails
-------------------------------*/
int aes_InvShiftRows(uint8_t *state)
{
	int j,k;
	uint8_t tmp;

	if(state==NULL)
		return -1;

	for(k=0; k<4; k++) {
		/* each row shift k times */
		for(j=0; j<k; j++) {
			tmp=*(state+(k<<2)+3); //tmp=*(state+4*k+3); /* save the last byte */
			memmove(state+(k<<2)+1, state+(k<<2), 3); // memmove(state+4*k+1, state+4*k, 3);
			*(state+(k<<2))=tmp; // *(state+4*k)=tmp;   /* set the first byte */
		}
	}
	return 0;
}


/*-------------------------------------------------------------
Add round key to the state.

@Nr:	    Number of rounds, 10/12/14 for AES-128/192/256
@Nk:        Key length, in words.
@round:	    Current round number.
@state:	    Pointer to state.
@keywords[Nb*(Nr+1)]:   All round keys, in words.

Return:
	0	Ok
	<0	Fails
--------------------------------------------------------------*/
int aes_AddRoundKey(uint8_t Nr, uint8_t Nk, uint8_t round, uint8_t *state, const uint32_t *keywords)
{
	int k;
	if(state==NULL || keywords==NULL)
		return -1;

	for(k=0; k<4*4; k++)
		//state[k] = ( keywords[round*4+k%4]>>((3-k/4)*8) &0xFF )^state[k];
		state[k] = ( keywords[ (round<<2) +(k&0x3) ]>>((3-(k>>2))<<3) &0xFF )^state[k];

	return 0;
}


/*-------------------------------------------------------------------
Generate round keys.

@Nr:	    Number of rounds, 10/12/14 for AES-128/192/256
@Nk:        		Key length, in words.
@inkey[4*Nk]:  	        Original key, 4*Nk bytes, arranged row by row.
@keywords[Nb*(Nr+1)]:  Output keys, in words.  Nb*(Nr+1)
			one keywords(32 bytes) as one column of key_bytes(4 bytes)

Note:
1. The caller MUST ensure enough mem space of input params.

Return:
	0	Ok
	<0	Fails
---------------------------------------------------------------------*/
int aes_ExpRoundKeys(uint8_t Nr, uint8_t Nk, const uint8_t *inkey, uint32_t *keywords)
{
	int i;
	const int Nb=4;
	uint32_t temp;

	if(inkey==NULL || keywords==NULL)
		return -1;

	/* Re_arrange inkey to keywords, convert 4x8bytes each row_data to a 32bytes keyword, as a complex column_data. */
	for( i=0; i<Nk; i++ ) {
		keywords[i]=(inkey[4*i]<<24)+(inkey[4*i+1]<<16)+(inkey[4*i+2]<<8)+inkey[4*i+3];
	}

	/* Expend round keys */
	for(i=Nk; i<Nb*(Nr+1); i++) {
		temp=keywords[i-1];
		if( i%Nk==0 ) {
			/* RotWord */
			temp=( temp<<8 )+( temp>>24 );
			/* Subword */
			temp=(sbox[temp>>24]<<24) +(sbox[(temp>>16)&0xFF]<<16) +(sbox[(temp>>8)&0xFF]<<8)
				+sbox[temp&0xFF];
			/* temp=SubWord(RotWord(temp)) XOR Rcon[i/Nk-1] */
			temp=temp ^ Rcon[i/Nk-1];
		}
		else if (Nk>6 && i%Nk==4 ) {
			/* Subword */
			temp=(sbox[temp>>24]<<24) +(sbox[(temp>>16)&0xFF]<<16) +(sbox[(temp>>8)&0xFF]<<8)
                                +sbox[temp&0xFF];
		}

		/* Get keywords[i] */
		keywords[i]=keywords[i-Nk]^temp;
	}
#if 0	/* Print all keys */
	for(i=0; i<Nb*(Nr+1); i++)
		printf("keywords[%d]=0x%08X\n", i, keywords[i]);
#endif

	return 0;
}


/*----------------------------------------------------------------------
Generate round keys.

@Nr:	   		 Number of rounds, 10/12/14 for AES-128/192/256
@Nk:        		Key length, in words.
@keywordss[Nb*(Nr+1)]:   All round keys, in words.
@state[4*4]:		The state block.

Note:
1. The caller MUST ensure enough mem space of input params.

Return:
	0	Ok
	<0	Fails
------------------------------------------------------------------------*/
int aes_EncryptState(uint8_t Nr, uint8_t Nk, uint32_t *keywords, uint8_t *state)
{
	int i,k;
	uint8_t round;
	uint8_t mc[4];		    /* Temp. var */

	if(keywords==NULL || state==NULL)
		return -1;

	/* 1. Add round key */
//	printf(" --- Add Round_key ---\n");
        aes_AddRoundKey(Nr, Nk, 0, state, keywords);
//	aes_PrintState(state);

	 /* Run Nr round functions */
	 for( round=1; round<Nr; round++) {  /* Nr */

		/* 2. Substitue State Bytes with SBOX */
//		printf(" --- SubBytes() Round:%d ---\n",round);
		for(k=0; k<16; k++)
			state[k]=sbox[state[k]];
//		aes_PrintState(state);

		/* 3. Shift State Rows */
//		printf(" --- ShiftRows() Round:%d ---\n",round);
		aes_ShiftRows(state);
//		aes_PrintState(state);

		/* 4. Mix State Cloumns */
		/* Galois Field Multiplication, Multi_Matrix:
			2 3 1 1
			1 2 3 1
			1 1 2 3
			3 1 1 2
		   Note:
		   1. Any number multiplied by 1 is equal to the number itself.
		   2. Any number multiplied by 0 is 0!
		*/
//		printf(" --- MixColumn() Round:%d ---\n",round);
		for(i=0; i<4; i++) { /* i as column index */
	   		mc[0]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[2])%0xFF] )
  				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[3])%0xFF] )
				^state[i+8]^state[i+12];
			mc[1]=  state[i]
				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[2])%0xFF] )
				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[3])%0xFF] )
				^state[i+12];
			mc[2]=  state[i]^state[i+4]
				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[2])%0xFF] )
				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[3])%0xFF] );
			mc[3]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[3])%0xFF] )
				^state[i+4]^state[i+8]
				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[2])%0xFF] );

			state[i+0]=mc[0];
			state[i+4]=mc[1];
			state[i+8]=mc[2];
			state[i+12]=mc[3];
		}
//		aes_PrintState(state);

		/* 5. Add State with Round Key */
//		printf(" --- Add Round_key ---\n");
	        aes_AddRoundKey(Nr, Nk, round, state, keywords);
//		aes_PrintState(state);

   	} /* END Nr rounds */

	/* 6. Substitue State Bytes with SBOX */
//	printf(" --- SubBytes() Round:%d ---\n",round);
	for(k=0; k<16; k++)
		state[k]=sbox[state[k]];
//	aes_PrintState(state);

	/* 7. Shift State Rows */
//	printf(" --- ShiftRows() Round:%d ---\n",round);
	aes_ShiftRows(state);
//	aes_PrintState(state);

	/* 8. Add State with Round Key */
//	printf(" --- Add Round_key ---\n");
        aes_AddRoundKey(Nr, Nk, round, state, keywords);
//	aes_PrintState(state);

	return 0;
}


/*----------------------------------------------------------------------
Decrypt the state.

@Nr:	   		 Number of rounds, 10/12/14 for AES-128/192/256
@Nk:        		Key length, in words.
@keywordss[Nb*(Nr+1)]:  All round keys, in words.
@state[4*4]:		The state block.

Note:
1. The caller MUST ensure enough mem space of input params.

Return:
	0	Ok
	<0	Fails
------------------------------------------------------------------------*/
int aes_DecryptState(uint8_t Nr, uint8_t Nk, uint32_t *keywords, uint8_t *state)
{
	int i,k;
	uint8_t round;
	uint8_t mc[4];		    /* Temp. var */

	if(keywords==NULL || state==NULL)
		return -1;

	/* 1. Add round key */
//	printf(" --- Add Round_key ---\n");
        aes_AddRoundKey(Nr, Nk, Nr, state, keywords);  /* From Nr_th round */
//	aes_PrintState(state);

	 /* Run Nr round functions */
	 for( round=Nr-1; round>0; round--) {  /* round [Nr-1  1]  */

		/* 2. InvShift State Rows */
//		printf(" --- InvShiftRows() Round:%d ---\n",round);
		aes_InvShiftRows(state);
//		aes_PrintState(state);

		/* 3. (Inv)Substitue State Bytes with R_SBOX */
//		printf(" --- (Inv)SubBytes() Round:%d ---\n",round);
		for(k=0; k<16; k++)
			state[k]=rsbox[state[k]];
//		aes_PrintState(state);

		/* 4. Add State with Round Key */
//		printf(" --- Add Round_key ---\n");
	        aes_AddRoundKey(Nr, Nk, round, state, keywords);
//		aes_PrintState(state);

		/* 5. Inverse Mix State Cloumns */
		/* Galois Field Multiplication, Multi_Matrix:
			0x0E 0x0B 0x0D 0x09
			0x09 0x0E 0x0B 0x0D
			0x0D 0x09 0x0E 0x0B
			0x0B 0x0D 0x09 0x0E
		   Note:
		   1. Any number multiplied by 1 is equal to the number itself.
		   2. Any number multiplied by 0 is 0!
		*/
//		printf(" --- InvMixColumn() Round:%d ---\n",round);
		for(i=0; i<4; i++) { 	/* i as column index */
	   		mc[0]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[0x0E])%0xFF] )
  				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[0x0B])%0xFF] )
  				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[0x0D])%0xFF] )
  				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[0x09])%0xFF] );
	   		mc[1]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[0x09])%0xFF] )
  				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[0x0E])%0xFF] )
  				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[0x0B])%0xFF] )
  				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[0x0D])%0xFF] );
	   		mc[2]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[0x0D])%0xFF] )
  				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[0x09])%0xFF] )
  				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[0x0E])%0xFF] )
  				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[0x0B])%0xFF] );
	   		mc[3]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[0x0B])%0xFF] )
  				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[0x0D])%0xFF] )
  				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[0x09])%0xFF] )
  				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[0x0E])%0xFF] );

			state[i+0]=mc[0];
			state[i+4]=mc[1];
			state[i+8]=mc[2];
			state[i+12]=mc[3];
		}
//		aes_PrintState(state);


   	} /* END Nr rounds */

	/* 6. Inverse Shift State Rows */
//	printf(" --- InvShiftRows() Round:%d ---\n",round);
	aes_InvShiftRows(state);
//	aes_PrintState(state);

	/* 7. Substitue State Bytes with SBOX */
//	printf(" --- InvSubBytes() Round:%d ---\n",round);
	for(k=0; k<16; k++)
		state[k]=rsbox[state[k]];
//	aes_PrintState(state);

	/* 8. Add State with Round Key */
//	printf(" --- Add Round_key ---\n");
        aes_AddRoundKey(Nr, Nk, 0, state, keywords);
//	aes_PrintState(state);

	return 0;

}



/*----------------------------------------------------------
Encrypt/decrypt data with AES_128_CBC method + PKCS7 padding.
AES: 	  Advanced Encryption Standard
AES_128:  with a 128-bit key
CBC: 	  Cipher Block Chaining

Note:
1. 16==AES_BLOCK_SIZE.
2. If outdata is created/mmaped with egi_fmap_create(), its
   initial size shall be the same as inszie, and later to
   call egi_fmap_resize() to adjust same as final outsize.

@indata: 	Input data
@insize: 	Input data size
@outdata: 	Output data
		!!!--- CAUTION ---!!!
		The Caller MUST ensure enough sapce.
@outsize:	Pointer to output data size
                If returned outsize is negative or ridiculous, it means
                the original data error, OR encryption method unsupported.
@ukey:  	The 128bits key,
@uiv:   	The 128bits init vector
@encode:	0 as to decrypt
		!0 to encrypt.

Return:
	0	OK
	<0	Fails
-------------------------------------------------------------*/
int egi_AES128CBC_encrypt(uint8_t *indata, size_t insize,  uint8_t *outdata, size_t *outsize,
			const uint8_t ukey[16],  const uint8_t uiv[16], int encode)
{
	int i;
//	unsigned char tmpiv[16];  /* varing IV, for each block_size operation. */

  	const uint8_t Nb=4;	/* Block size(in words), 4/4/4 for AES-128/192/256 */
  	const uint8_t Nk=4;	/* Key size(in words), 4/6/8 for AES-128/192/256 */
  	const uint8_t Nr=10;	/* Number of rounds, 10/12/14 for AES-128/192/256 */

	uint8_t tmp[16];
	uint8_t state[16];
	uint8_t iv[16];
	uint32_t keywords[Nb*(Nr+1)];

	bool	do_encrypt=(encode==0?false:true);

	/* 1 Check input */
	if(indata==NULL || outdata==NULL || ukey==NULL || uiv==NULL)
		return -1;

	/* 2. Generate Nr+1 round_keys(derived from ukey), to save in keywords. */
	aes_ExpRoundKeys(Nr, Nk, ukey, keywords);

	/* 3. Separate multiples_of_16 and remainder from data size.  16=AES_BLOCK_SIZE */
	int ns=insize/16;	   /* Full block_size data */
	int lenm16=ns*16; 	   /* PART1: Multipls of 16. */
	int lenr16=insize%16;	   /* PART2: Remainder */
	printf("lenm16=%dBs, lenr16=%dBs\n", lenm16, lenr16);

	/* 3. Do encrypt or decrypt */
 /* -----> DO ENCRYPTION */
	if( do_encrypt ) {
		egi_dpstd("Start encrypting...\n");

		/* Init iv */
		//memcpy(iv, uiv, 16);
		aes_DataToState(uiv,iv); /* Notice first sequence! */

		/* Encrypt each state */
		for(i=0; i<ns; i++) {
			/* Fill into state array: from colum[0] to colum[3] */
			aes_DataToState( indata+(i<<4), state);

			/* State XOR Iv */
			aes_StateXorIv(state, iv);
			/* Encryp state */
			aes_EncryptState(Nr, Nk, keywords, state);
			/* Read out */
			aes_StateToData(state, outdata+(i<<4));
			/* Save result as for NEXT iv */
			memcpy(iv, state, 16); /* NOW iv,state same sequence */
		}

		/* Tail data treatment, PKCS7 padding.
		 * 1. If remainder is 0byte, add additional 16Bs(a block_size), and all fill with number (16).
		 * 2. Otherwise padding with additional (16-lenr16)Bs with number (16-lenr16),
		 */
		if(lenr16==0) {
			/* Pad a whole block with 16 */
			memset(state, 16, 16);

			/* State XOR Iv */
			aes_StateXorIv(state, iv);
			/* Encryp state */
			aes_EncryptState(Nr, Nk, keywords, state);
			/* Read out */
			aes_StateToData(state, outdata+(ns<<4));
			/* Save result as iv */
			memcpy(iv, state, 16);
		}
		else { /* lenr16!=0 */
			/* Fill remaining data to tmp */
			memcpy(tmp, indata+(ns<<4), lenr16);
			/* Padding with (16-lenr16) */
                        memset(tmp+lenr16, 16-lenr16,  16-lenr16);
			/* Fill into state array: from colum[0] to colum[3] */
			aes_DataToState(tmp, state);

			/* State XOR Iv */
			aes_StateXorIv(state, iv);
			/* Encryp state */
			aes_EncryptState(Nr, Nk, keywords, state);
			/* Read out */
			aes_StateToData(state, outdata+(ns<<4));
			/* Save result as iv */
			memcpy(iv, state, 16);
		}

		/* Update Outsize */
		*outsize = (ns+1)<<4;
	}

 /* -----> DO DECRYPTION */
	else {
		egi_dpstd("Start decrypting...\n");

		/* Check size */
		if( lenr16!=0 ) {
			egi_dpstd("Input decrypto data error! it MUST be multiples of 16!\n");
			return -1;
		}

		/* Init iv */
		//memcpy(iv, uiv, 16);
		aes_DataToState(uiv,iv); /* Notice first sequence! */

		/* Decrypt each state */
		for(i=0; i<ns; i++) {
			/* Fill into state array: from colum[0] to colum[3] */
			aes_DataToState( indata+(i<<4), state);
			/* Backup indata as for next iv */
			memcpy(tmp, indata+(i<<4), 16);

			/* Encryp state */
			aes_DecryptState(Nr, Nk, keywords, state);
			/* State XOR Iv */
			aes_StateXorIv(state, iv);
			/* Read out */
			aes_StateToData(state, outdata+(i<<4));

			/* Load prev indata/tmp for NEXT iv */
			aes_DataToState(tmp, iv);
			//memcpy(iv, tmp, 16); /* Same sequence */
		}

		/* Tail data treatment, PKCS7 padding.
		 * 1. If remainder is 0byte, add additional 16Bs(a block_size), and all fill with number (16).
		 * 2. Otherwise padding with additional (16-lenr16)Bs with number (16-lenr16)
		 *    padding with of number of bytes added, as 16-lenr16.
		 */

		/* Read last state to tmp */
		aes_StateToData(state, tmp);
		lenr16=16-tmp[15];

		/* Update Outsize */
		*outsize = insize-tmp[15]; /* The last byte always is the number of paddings */

		/* Check padding */
		for(i=15; i>lenr16-1; i--) {
			if( tmp[15]!=tmp[i] ) {
				egi_dpstd("Padding data error! fail to decrypt.\n");
				return -1;
			}
		}
	}


	return 0;
}
