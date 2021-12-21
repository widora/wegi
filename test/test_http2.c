/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to parse and download AAC m3u8 file.

TODO:
1. For some m3u8 address, it sometimes exits accidently when calling https_easy_download().

Journal:
2021-12-18:
	Create the file by copying test_http.c
2021-12-20:
	1. parse_m3u8list(): to download and play segment vides.

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
#include <egi_utils.h>
#include <egi_cstring.h>
#include <egi_procman.h>

size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *userp);
void parse_m3u8list(char *strm3u);

char *dirURL;  /* The last dirURL */
char *m3uURL;
char buff[CURL_RETDATA_BUFF_SIZE];

int main(int argc, char **argv)
{
	char strRequest[256+64];

	if(argc<2) {
		printf("Please provide m3uURL!\n");
		exit(0);
	}

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
        EGI_PLOG(LOGLV_CRITICAL,"Http request head:\n %s\n", strRequest);

	/* m3u URL */
	m3uURL = strRequest;

  while(1) {
	/* 1. Reset buff */
	//memset(buff, 0, sizeof(buff));
	buff[0]=0; /* OK??? */

	/* 2. Https GET request */
	EGI_PLOG(LOGLV_INFO,"Start https curl request...");
        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION|HTTPS_ENABLE_REDIRECT,
				5,30, strRequest, buff, NULL, curl_callback) !=0 )
	{
		EGI_PLOG(LOGLV_ERROR, "Fail to call https_curl_request()! try again...");
		/* Try again */
		sleep(1);
		continue;
		//EGI_PLOG(LOGLV_ERROR, "Try https_curl_request() again...");
		//exit(EXIT_FAILURE);
	}
        printf("        --- Http GET Reply ---\n %s\n",buff);
	EGI_PLOG(LOGLV_INFO,"Http curl get: %s\n", buff);

	/* 3. Parse content */
	EGI_PLOG(LOGLV_INFO,"Start parse m3u8list...");
	parse_m3u8list(buff);

	/* Sleep of TARGETDURATION */
//	sleep(7/2);
  }

	exit(EXIT_SUCCESS);
}

/*-----------------------------------------------
 A callback function to deal with replied data.
------------------------------------------------*/
size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
        int blen=strlen(buff);
        int plen=strlen(ptr);
        int xlen=plen+blen;

        /* MUST check buff[] capacity! */
        if( xlen >= sizeof(buff)-1) {   // OR: CURL_RETDATA_BUFF_SIZE-1-strlen(..)
                /* "If src contains n or more bytes, strncat() writes n+1 bytes to dest
                 * (n from src plus the terminating null byte." ---man strncat
                 */
                fprintf(stderr,"\033[0;31;40m strlen(buff)=%d, strlen(buff)+strlen(ptr)=%d >=sizeof(buff)-1! \e[0m\n",
                                                        blen, xlen);
                //exit(0);  /* YES! */

                strncat(userp, ptr, sizeof(buff)-1-blen); /* 1 for '\0' by strncat */
                //strncat(buff, ptr, sizeof(buff)-1);

	    	EGI_PLOG(LOGLV_TEST, "%dBytes data strncat to userp!", sizeof(buff)-1-blen);
        }
        else {
            #if 1 /* strcat */
                strcat(userp, ptr);
                //buff[sizeof(buff)-1]='\0';  /* <---------- return data maybe error!!??  */
	    	EGI_PLOG(LOGLV_TEST, "%dBytes data strcat to userp!", plen);
            #else /* memcpy */
                memcpy(userp, ptr, size*nmemb); // NOPE for compressed data?!!!
                userp[size*nmemb]='\0';
            #endif

        }

        return size*nmemb; /* Return size*nmemb OR curl will fails! */
}


/*-----------------------------------
Parse m3u8 content

Note:
1. Content in strm3u will be changed!
-----------------------------------*/
void parse_m3u8list(char *strm3u)
{
	int k;
	int cnt;
	int dscnt;
	char *ps;
	const char *delim="\r\n";
	char tsURL[8][EGI_URL_MAX];  /* segment URL */
	char *sfname[8];  /* segment file name */

	/* Get m3u8 params: VERSION,TARGETDURATION,SEQUENCE,EXTINF .....  */

	/* Parse segment URL List */
	cnt=0;
        ps=strtok(strm3u, delim);
        while(ps!=NULL && cnt<6) {
	   /* Get .ts file */
	   if( strstr(ps,".ts")!=NULL ) {
		/* Get the last subdir URL */
		/* (myURL, protocol, host, port, path, filename, dir,dirURL) */
		cstr_parse_URL(m3uURL, NULL, NULL, NULL, NULL, NULL, NULL, &dirURL);

		/* Assemble URL for the time segement file */
		tsURL[cnt][0]=0;
		strcat(tsURL[cnt], dirURL);
		strcat(tsURL[cnt], ps);

		printf("tsURL[%d]: %s\n", cnt, tsURL[cnt]);

		/* Free dirURL */
		egi_free_char(&dirURL);

		/* Count ts files */
		cnt++;
	   }

	   /* Go on */
	   ps=strtok(NULL, delim);
	}

	/* Download segment files */
	remove("/tmp/vall.mp4");
	dscnt=0;
	for(k=0; k<cnt; k++) {
		if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION|HTTPS_DOWNLOAD_SAVE_APPEND,
					 3, 30,
                			 //tsURL[k], tsSavePath[k], NULL, download_callback) ==0 )
                			 tsURL[k], "/tmp/vall.mp4", NULL, NULL) ==0 )  /* Use default callback write func. */
                {
			printf("Ok, succeed to download segment from '%s'!\n", tsURL[k]);
			dscnt++;
		}
		else
			printf("Fail to download segment from '%s'!\n", tsURL[k]);
	}
	printf("Totally %d segments downloade!\n", dscnt);

        /* Convert video to MPEG */
	printf("Start to conver video format...\n");
        //egi_system("ffmpeg -y -i /tmp/vall.mp4 -f mpeg -s 320x240 -r 20 /tmp/vall.avi");
        egi_system("ffmpeg -y -i /tmp/vall.mp4 -f mpeg -s 320x240 -r 20 -b:v 300 /tmp/vall.avi");

	/* Play video */
	printf("Start to play video...\n");
        egi_system("/tmp/MPlayer -ac mad -vo fbdev -framedrop -brightness 18 /tmp/vall.avi");
	printf("Finish playing video!\n");
}
