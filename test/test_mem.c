#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <malloc.h>


int main(void)
{
	struct mallinfo	minfo;

	/* print malloc info */
	minfo=mallinfo();
	printf("number of free chunks: 		%d\n", minfo.ordblks);  /* 512-128K */
	printf("number of fastbin blocks: 	%d\n", minfo.smblks);	/* 80-512 bytes */
	printf("space available in freed fastbin blocks: 	%d\n", minfo.fsmblks);	/* 0-80 bytes */

	/* print malloc status */
	malloc_stats(NULL);

	return 0;
}
