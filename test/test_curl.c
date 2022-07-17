/*--------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Journal:
2022-07-16: Create the file.

Midas Zhou
知之者不如好之者好之者不如乐之者
--------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "egi_https.h"
#include "egi_timer.h"

int main(int argc, char **argv)
{



  #if 0 ////////////////// https_easy_getFileSize() ////////////////
	long fsize;

    while(1) {

	fsize=https_easy_getFileSize(argv[1]);
  	printf("Remote file size: %ldBytes\n\n", fsize);

   	usleep(200000);
    }
  #endif //////////////////////////////////////////////////////////////


  #if 1 ////////////////// https_easy_mThreadDownload() ////////////////

	struct timeval tms,tme;
        int opt;
	int nthreads=2;
	int timeout=30;
	char *url=NULL;
        while( (opt=getopt(argc,argv,"hn:t:"))!=-1 ) {
                switch(opt) {
                        case 'h':
				printf("%s: -hn:t:\n", argv[0]);
                                exit(0);
                                break;
                        case 'n':
                                nthreads=atoi(optarg);
                                break;
                        case 's':
                                timeout=atoi(optarg);
				break;
		}
	}
        if( optind < argc ) {
                url=argv[optind];
        } else {
                printf("No URL!\n");
                exit(1);
        }

	gettimeofday(&tms, NULL);
       //int https_easy_mThreadDownload(int opt, unsigned int nthreads, unsigned int trys, unsigned int timeout,
       //                       const char *file_url, const char *file_save );

	
	https_easy_mThreadDownload( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION, nthreads, 0, timeout,
				    url, "/tmp/multTD.ts" );
	gettimeofday(&tme, NULL);
	fprintf(stderr, "Cost time: %ldms\n", tm_diffus(tms,tme)/1000);

  #endif //////////////////////////////////////////////////////////////

   return 0;
}
