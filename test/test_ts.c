/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of extracting audio data from Transport Stream.

Reference:
  1. https://blog.csdn.net/leek5533/article/details/104993932
  2. ISO/IEC13818-1 Information technology — Generic coding of moving
	       pictures and associated audio information: Systems


Abbreviations
   PAT --- Program Association Table, list Program_Number and related PMT PID.
   PMT --- Program Map Table, list Elementary Stream type and PID.
	   If ONLY one program, then PMT maybe omitted!??? <--------
   CAT --- Condition Access Table
   NIT --- Network Information Table

   PSI --- Program Specific Information. (PAT,PMT,CAT,...etc)
   PES --- Packetized Elementary Stream

Journal:
2022-06-30: Create the file.
2022-07-01/02:
	  1. Extract ES and save to fout.
	  2. Transfer egi_extract_aac_from_ts() to egi_utils.c

Midas Zhou
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <egi_debug.h>
#include <egi_cstring.h>
#include <egi_utils.h>


/*----------------------------
           Main
-----------------------------*/
int main(int argc, char **argv)
{

	egi_extract_aac_from_ts(argv[1], "/tmp/ts.aac");

	return 0;
}


