/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to parse and download AAC m3u8 file.

TODO:
1. For some m3u8 address, it sometimes exits accidently when calling https_easy_download().

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <egi_https.h>
#include <egi_timer.h>
#include <egi_log.h>

int curl_nwrite;
size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *userp);
void parse_m3u8list(char *strm3u);
char aacLastURL[1024];  /* The last URL of downloaded AAC file */

int main(int argc, char **argv)
{
	char buff[CURL_RETDATA_BUFF_SIZE];
	char strRequest[256+64];

/* TODO test */
//	cstr_split_nstr(strm3u, split, )

#if 0	/* Start egi log */
  	if(egi_init_log("/mmc/test_http.log") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
  	}
	EGI_PLOG(LOGLV_INFO,"%s: Start logging...", argv[0]);
#endif

	/* For http,  conflict with curl?? */
	printf("start egitick...\n");
	tm_start_egitick();

	/* prepare GET request string */
        memset(strRequest,0,sizeof(strRequest));
        strcat(strRequest,argv[1]);
        //printf("Request:%s\n", strRequest);

while(1) {
	/* Https GET request */
	EGI_PLOG(LOGLV_INFO,"Start https curl request...");
        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION,
				strRequest, buff, NULL, curl_callback) !=0 )
	{
		EGI_PLOG(LOGLV_ERROR, "Fail to call https_curl_request()! try again...");
		/* Try again */
		sleep(1);
		continue;
		EGI_PLOG(LOGLV_ERROR, "Try https_curl_request() again...");
		//exit(EXIT_FAILURE);
	}
        printf("        --- Http GET Reply ---\n %s\n",buff);

	/* Parse content */
	EGI_PLOG(LOGLV_INFO,"Start parse m3u8list...");
	parse_m3u8list(buff);

	/* Sleep of TARGETDURATION */
	sleep(7/2);
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

        written = fwrite(ptr, size,  nmemb, (FILE *)stream);
        //printf("%s: written size=%zd\n",__func__, written);

	if(written<0) {
		EGI_PLOG(LOGLV_ERROR,"xxxxx  Curl callback:  fwrite error!  written<0!  xxxxx");
		written=0;
	}
	curl_nwrite += written;

	EGI_PLOG(LOGLV_CRITICAL," curl_nwritten=%d ", curl_nwrite);
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
	char aacURL[1024]={0};

	/* Get m3u8 params: VERSION,TARGETDURATION,SEQUENCE,EXTINF .....  */

	/* Parse AAC URL List */
        ps=strtok(strm3u, delim);
        for(k=0; ps!=NULL; k++) {
		if( strstr(ps,"aac") && strstr(ps,"//") ) {
			EGI_PLOG(LOGLV_INFO, "AAC URL: '%s'.",ps);

			/* Get right URL for AAC file */
			memset(aacURL,0,sizeof(aacURL));
			if( strstr(ps,"http:")==NULL ) {
				strcat(aacURL, "http:");
				strncat(aacURL, ps, sizeof(aacURL)-1-strlen("http:"));
			}
			else {
				strncat(aacURL, ps, sizeof(aacURL)-1);
			}

			/* 1. Check and download a.stream */
			if( access("/tmp/a.stream",F_OK)!=0 && strcmp(aacURL, aacLastURL) > 0) {
				/* Download AAC */
				curl_nwrite=0;
				EGI_PLOG(LOGLV_INFO, "Downloading a.stream AAC from: %s",  aacURL);
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION,
							 aacURL, "/tmp/a.stream", NULL, download_callback) !=0 )
				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download a.stream from '%s'.", aacURL);
					remove("/tmp/a.stream");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/a.stream.");
				} else {
					/* Update aacLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, a.stream updated! curl_nwrite=%d", curl_nwrite);
					strncpy( aacLastURL, aacURL, sizeof(aacLastURL)-1 );
				}
			}

			/* 2. Check and download b.stream */
			else if( access("/tmp/b.stream",F_OK)!=0 && strcmp(aacURL, aacLastURL) > 0) {
				/* Download AAC */
				curl_nwrite=0;
				EGI_PLOG(LOGLV_INFO, "Downloading b.stream AAC from: %s",  aacURL);
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION,
							 aacURL, "/tmp/b.stream", NULL, download_callback) !=0 )
				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download b.stream from '%s'.", aacURL);
					remove("/tmp/b.stream");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/b.stream.");
				} else {
					/* Update aacLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, b.stream updated! curl_nwrite=%d", curl_nwrite);
					strncpy( aacLastURL, aacURL, sizeof(aacLastURL)-1 );
				}
			}
			/* 3.!!! Sometimes URL time stamp skips and  is NOT consistent with former one, strcmp() will fail!???  */
			else if( access("/tmp/a.stream",F_OK)!=0  && access("/tmp/b.stream",F_OK)!=0 ) {
				/* Download AAC */
				curl_nwrite=0;
				EGI_PLOG(LOGLV_ERROR, "Case 3: a.stream & b.stream both missing! downloading a.stream AAC from: %s",  aacURL);
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION,
							 aacURL, "/tmp/a.stream", NULL, download_callback) !=0 )
				{
                        		EGI_PLOG(LOGLV_ERROR, "Case 3: Fail to easy_download a.stream from '%s'.", aacURL);
					remove("/tmp/a.stream");
					EGI_PLOG(LOGLV_INFO, "Case 3: Finsh remove /tmp/a.stream.");
				} else {
					/* Update aacLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, a.stream updated! curl_nwrite=%d", curl_nwrite);
					strncpy( aacLastURL, aacURL, sizeof(aacLastURL)-1 );
				}
			}
			/* 4. Both OK */
			else
				EGI_PLOG(LOGLV_CRITICAL, "a.stream and b.stream both available!\n");
		}

		/* Get next URL */
		ps=strtok(NULL, delim);

	}
}
