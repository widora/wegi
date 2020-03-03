/*--------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to convert a normal file to an OBJ file, then compile with
that OBJ file to retrieve the original file.

Steps:
1. Convert a JPG file to an OBJ file pridata.o:
   xxx_objcopy -I binary -O elf32-tradlittlemips -B mips lake.jpg pridata.o
   ( xxx as cross_compile toochain  mipsel-openwrt-linux )

   !!!Note: Put the file in current directory, or the symbol name will be too long !!!
   	../open_objcopy -I binary -O elf32-tradlittlemips -B mips ../data/lake.jpg pridata.o
   Result in:
	0000000 g       .data	00000000 _binary____data_lake_jpg_start
	00003728 g       .data	00000000 _binary____data_lake_jpg_end
	00003728 g       *ABS*	00000000 _binary____data_lake_jpg_size


2. Compile with above OBJ file:
   xxx_gcc -o test_elf test_elf.c pridata.o

3. Run test_elf in Windora_NEO to retrieve the original JPG file.


Midas Zhou
---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
//#include <stdint.h>
//#include <elf.h>

#if 1 /////////////////////////////////////////////////////////
/* Note: All symbol value are parsed as address after ld */
extern char _binary_lake_jpg_start[];
extern char _binary_lake_jpg_end[];
extern const char _binary_lake_jpg_size[];

int main(void)
{
	FILE *fp;
	unsigned long size;

	printf("offset: _binary_lake_jpg_start=%p \n", _binary_lake_jpg_start );
	printf("offset: _binary_lake_jpg_end=%p \n", _binary_lake_jpg_end );
	printf("_binary_lake_jpg_size=%p \n", _binary_lake_jpg_size );
	printf("size=%d, or 0x%x \n", (int)_binary_lake_jpg_size, (int)_binary_lake_jpg_size);

	size=(int)_binary_lake_jpg_size;


	fp=fopen("/tmp/lake.jpg", "w");
	if(fp == NULL ) {
		perror("open file");
		exit(-1);
	}


	fwrite( (char *)_binary_lake_jpg_start, size, 1, fp);

	fclose(fp);

	return 0;
}

#else	//////////////////////////////////////////////////////////

extern char privdata[];

int main(void)
{
	FILE *fp;
	unsigned long size;

	printf("offset: privdata=%p \n", privdata );

	size=14120;

	fp=fopen("/tmp/lake.jpg", "w");
	if(fp == NULL ) {
		perror("open file");
		exit(-1);
	}


	fwrite( (char *)privdata, size, 1, fp);

	fclose(fp);

	return 0;
}

#endif ///////////////////////////////////////////////////
