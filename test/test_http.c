/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to parse and download AAC m3u8 file.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <egi_https.h>
#include <egi_timer.h>

size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *userp);
void parse_m3u8list(char *strm3u);
char aacLastURL[1024];  /* The last URL of downloaded AAC file */

int main(int argc, char **argv)
{
	char buff[CURL_RETDATA_BUFF_SIZE];
	char strRequest[256+64];

/* TODO test */
//	cstr_split_nstr(strm3u, split, )

	/* For http */
	tm_start_egitick();


	/* prepare GET request string */
        memset(strRequest,0,sizeof(strRequest));
        strcat(strRequest,argv[1]);
        //printf("Request:%s\n", strRequest);

while(1) {
	/* Https GET request */
        if( https_curl_request(strRequest, buff, NULL, curl_callback)!=0 ) {
		printf("Fail to call https_curl_request()!");
		exit(EXIT_FAILURE);
	}
        printf("        --- Http GET Reply ---\n %s\n",buff);

	/* Parse content */
	parse_m3u8list(buff);

	sleep(2);
}

	exit(EXIT_SUCCESS);
}

/*-----------------------------------------------
 A callback function to deal with replied data.
------------------------------------------------*/
size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
        size_t session_size=size*nmemb;

        //printf("%s: session_size=%zd\n",__func__, session_size);

	/* Just copy to user */
        strcat(userp, ptr);

        return session_size;
}

/*-----------------------------------------------
 A callback function for write data to file.
------------------------------------------------*/
size_t download_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
       size_t written;

        written = fwrite(ptr, size, nmemb, (FILE *)stream);
        //printf("%s: written size=%zd\n",__func__, written);

        return written;
}

/*-----------------------------------
Parse m3u8 content

Note:
1. Content in strm3u will be changed!
-----------------------------------*/
void parse_m3u8list(char *strm3u)
{
	int k;
	const char *delim="\r\n";
	char *ps;

	/* Get m3u8 params: VERSION,TARGETDURATION,SEQUENCE,EXTINF .....  */


	/* Parse AAC URL List */
        ps=strtok(strm3u, delim);
        for(k=0; ps!=NULL; k++) {
		if( strstr(ps,"aac")!=NULL ) {

			/* 1. Check and download a.stream */
			if( access("/tmp/a.stream",F_OK)!=0 && strcmp(ps, aacLastURL) > 0) {
				/* Download AAC */
				printf("Downloading AAC from: %s\n",  ps);
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION,
							 ps, "/tmp/a.stream", NULL, download_callback) !=0 )
				{
                        		printf("Fail to easy_download a.stream from '%s'.\n", ps);
					remove("/tmp/a.stream");
				} else {
					/* Update aacLastURL */
					printf("Ok, a.stream updated!\n");
					strncpy( aacLastURL, ps, sizeof(aacLastURL)-1 );
				}
			}

			/* 2. Check and download b.stream */
			else if( access("/tmp/b.stream",F_OK)!=0 && strcmp(ps, aacLastURL) > 0) {
				/* Download AAC */
				printf("Downloading AAC from: %s\n",  ps);
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION,
							 ps, "/tmp/b.stream", NULL, download_callback) !=0 )
				{
                        		printf("Fail to easy_download b.stream from '%s'.\n", ps);
					remove("/tmp/b.stream");
				} else {
					/* Update aacLastURL */
					printf("OK, b.stream updated!\n");
					strncpy( aacLastURL, ps, sizeof(aacLastURL)-1 );
				}
			}
			/* 3. Both OK */
			else
				printf("a.stream and b.stream both available!\n");
		}
		ps=strtok(NULL, delim);
	}
}
