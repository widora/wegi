/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

		A libcurl http helper
Refer to:  https://curl.haxx.se/libcurl/c/

Jurnal
2021-02-09:
	1. https_easy_download(): Add input param 'int opt' to replace
           definition of SKIP_xxx_VERIFICATION.
2021-02-10:
	1. https_curl_request(): Add input param 'int opt' to replace
           definition of SKIP_xxx_VERIFICATION.
2021-02-13:
	1. https_easy_download(): Add flock(,LOCK_EX) during downloading...
	   			  Add fsync() after fwrite() all data!
				  Add truncate() if filesize not right.
2021-02-21:
	1. https_easy_download(): Re_truncate file to 0 if download fails.
2021-03-23:
	1. Reset curl to NULL after curl_easy_cleanup(curl) !!!


Midas Zhou
midaszhou@yahoo.com
--------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>
#include <curl/curl.h>
#include "egi_https.h"
#include "egi_log.h"
#include "egi_timer.h"

#define EGI_CURL_TIMEOUT	15   /* in seconds */

/*------------------------------------------------------------------------------
			HTTPS request by libcurl

Note: You must have installed ca-certificates before call curl https, or
      define SKIP_PEER/HOSTNAME_VERIFICATION to use http instead.

@request:	request string
@reply_buff:	returned reply string buffer, the Caller must ensure enough space.
@data:		TODO: if any more data needed

		!!! CURL will disable egi tick timer? !!!
Return:
	0	ok
	<0	fails
--------------------------------------------------------------------------------*/
int https_curl_request(int opt, const char *request, char *reply_buff, void *data,
								curlget_callback_t get_callback)
{
	int i;
	int ret=0;
  	CURL *curl;
  	CURLcode res=CURLE_OK;
	double doubleinfo=0;

 /* Try Max. 3 sessions. TODO: tm_delay() not accurate, delay time too short!  */
 for(i=0; i<3; i++)
 {
	/* init curl */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init and easy init...",__func__);
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
	if(curl==NULL) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl!",__func__);
		return -1;
	}

	/* set curl option */
	curl_easy_setopt(curl, CURLOPT_URL, request);		 	 /* set request URL */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L); //1L		 /* 1 print more detail */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, EGI_CURL_TIMEOUT);	 /* set timeout */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_callback);     /* set write_callback */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, reply_buff); 		 /* set data dest for write_callback */

	/* Param opt set */
	if(HTTPS_SKIP_PEER_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	/* Perform the request, res will get the return code */
	EGI_PLOG(LOGLV_CRITICAL, "%s: Try [%d]th curl_easy_perform()... ", __func__, i);
	if(CURLE_OK != curl_easy_perform(curl) ) {
		printf("%s: curl_easy_perform() failed! Err'%s'\n", __func__, curl_easy_strerror(res));
		ret=-2;
		printf("%s: tm_delayms..\n",__func__);
		tm_delayms(200);
		printf("%s: try again...\n",__func__);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}

	/* 				--- Check session info. ---
	 ***  Note: curl_easy_perform() may result in CURLE_OK, but curl_easy_getinfo() may still fail!
	 */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &doubleinfo) ) {
		printf("%s: CURLINFO_CONTENT_LENGTH_DOWNLOAD = %.0f \n", __func__, doubleinfo);
		if( (int)doubleinfo > CURL_RETDATA_BUFF_SIZE )
		{
		  EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length > CURL_RETDATA_BUFF_SIZE!",
												   __func__);
			ret=-3;
			break;	/* BUFF overflow! */
		}
		/* Content length MAY BE invalid!!! */
		else if( (int)doubleinfo < 0 ) {
		  	EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length < 0!",  __func__);
			ret=-4;
			tm_delayms(200);
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
		//else
		//  	break; 	/* ----- OK! End Session ----- */
	}
	else { 	/* Getinfo fails */
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to easy getinfo CURLINFO_CONTENT_LENGTH_DOWNLOAD!", __func__);
		ret=-5;
		tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}

CURL_CLEANUP:
	/* always cleanup */
	curl_easy_cleanup(curl);
	curl=NULL;
  	curl_global_cleanup();

	/* if succeeds --- OK --- */
	if(ret==0)
		break;

 } /* End: try Max.3 times */

	if(ret!=0 && i==3)
		EGI_PLOG(LOGLV_ERROR,"%s: Fail after try %d times!", __func__, i);

	return ret;
}



/*----------------------------------------------------------------------------
			HTTPS request by libcurl
Note:
1. You must have installed ca-certificates before call curl https, or
   define SKIP_PEER/HOSTNAME_VERIFICATION to use http instead.
2. --- !!! WARNING !!! ---
   Filesize of fila_save will be truncated to 0 if it fails!
   The caller MAY need to remove file_save file in case https_easy_download fails!
   OR the file will remain!

@opt:			Http options, Use '|' to combine.
			HTTPS_SKIP_PEER_VERIFICATION
			HTTPS_SKIP_HOSTNAME_VERIFICATION

@file_url:		file url for downloading.
@file_save:		file path for saving received file.
@data:			TODO: if any more data needed
@write_callback:	Callback for writing received data

		!!! CURL will disable egi tick timer? !!!

Return:
	0	ok
	<0	fails
-------------------------------------------------------------------------------*/
int https_easy_download(int opt, const char *file_url, const char *file_save,   void *data,
							      curlget_callback_t write_callback )
{
	int i;
	int ret=0;
  	CURL *curl=NULL;
  	CURLcode res;
	double doubleinfo=0;
	FILE *fp;	/* FILE to save received data */
	int sizedown;

	/* check input */
	if(file_url==NULL || file_save==NULL)
		return -1;

	/* Open file for saving file */
	fp=fopen(file_save,"wb");
	if(fp==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: open file '%s', Err'%s'", __func__, file_save, strerror(errno));
		return -1;
	}

	/* Put lock, advisory locks only. TODO: it doesn't work???  */
	if( flock(fileno(fp),LOCK_EX) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to flock '%s', Err'%s'", __func__, file_save, strerror(errno));
		/* Go on .. */
	}

 /* Try Max. 3 times */
 for(i=0; i<3; i++)
 {
	/* init curl */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init and easy init...",__func__);
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
	if(curl==NULL) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl!",__func__);
		ret=-2;
		goto CURL_FAIL;
	}

	/***   ---  Set options for CURL  ---   ***/
	/* set download file URL */
	curl_easy_setopt(curl, CURLOPT_URL, file_url);
	/*  1L --print more detail,  0L --disbale */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L); //1L
   	/*  1L --disable progress meter, 0L --enable and disable debug output  */
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	/*  1L --enable redirection */
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	/*  set timeout */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, EGI_CURL_TIMEOUT);

	/* For name lookup timed out error: use ipv4 as default */
	//  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

	/* set write_callback to write/save received data  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

	/* Param opt set */
	if(HTTPS_SKIP_PEER_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	//curl_easy_setopt(curl, CURLOPT_CAINFO, "ca-bundle.crt");

	/* Set data destination for write_callback */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

	EGI_PLOG(LOGLV_CRITICAL, "%s: Try [%d]th curl_easy_perform()... ", __func__, i);
	/* Perform the request, res will get the return code */
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		/* Check Curl config: #ifdef CURL_DISABLE_VERBOSE_STRINGS! */
		if(res ==  CURLE_URL_MALFORMAT )
			EGI_PLOG(LOGLV_ERROR,"%s: curl_easy_perform() failed because of URL malformat!", __func__);
		else if( res == CURLE_OPERATION_TIMEDOUT )  /* res==28 */
			EGI_PLOG(LOGLV_ERROR,"%s: curl_easy_perform() Timeout!", __func__);
		else
			EGI_PLOG(LOGLV_ERROR,"%s: res=%d, curl_easy_perform() failed! Err'%s'", __func__, res, curl_easy_strerror(res));
		ret=-3;
		//tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}

	/* NOW: CURLE_OK, reset ret! */
	ret=0;

	/* 				--- Check session info. ---
	 *** Note:
	 *   1. curl_easy_perform() may result in CURLE_OK, but curl_easy_getinfo() may still fail!
	 *   2. Testing results: CURLINFO_SIZE_DOWNLOAD and CURLINFO_CONTENT_LENGTH_DOWNLOAD get the same doubleinfo!
	 */
	/* TEST: CURLINFO_SIZE_DOWNLOAD */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &doubleinfo) ) {
		  	EGI_PLOG(LOGLV_CRITICAL,"%s: Curlinfo_size_download=%d!",  __func__, (int)doubleinfo);
			sizedown=(int)doubleinfo;
	}

	/* CURLINFO_CONTENT_LENGTH_DOWNLOAD */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &doubleinfo) ) {
		//printf("%s: CURLINFO_CONTENT_LENGTH_DOWNLOAD = %.0f \n", __func__, doubleinfo);
		if( (int)doubleinfo > CURL_RETDATA_BUFF_SIZE )
		{
		  EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length=%d > CURL_RETDATA_BUFF_SIZE!",
  									      __func__, (int)doubleinfo);
			ret=-4;
			break;	/* BUFF overflow! */
		}
		/* Content length MAY BE invalid!!! */
		else if( (int)doubleinfo < 0 ) {
		  	EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length < 0!",  __func__);
			ret=-5;
			//tm_delayms(200);
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
		else {
		  	EGI_PLOG(LOGLV_CRITICAL,"%s: Curl download content length =%d!",  __func__, (int)doubleinfo);
		  	break; 	/* ----- OK ----- */
		}
	}
	else {	/* Getinfo fails */
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to easy getinfo CURLINFO_CONTENT_LENGTH_DOWNLOAD!", __func__);
		ret=-6;
		//tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}

CURL_CLEANUP:
	/* If succeeds, break loop! */
	if(ret==0)
		break;

	/* If OK, it should NOT carry out following jobs ... */
	truncate(file_save, 0);
	rewind(fp);	/* truncate will NOT reset file offset! */
	tm_delayms(200);

	/* Always cleanup before loop back to init curl */
	curl_easy_cleanup(curl);
	curl=NULL; /* OR curl_easy_cleanup() will crash at last! */
  	curl_global_cleanup();
 } /* End: try Max.3 times */


	/* Check result, If it fails, truncate filesize to 0 before quit!    ??? NOT happens!! */
	if(ret!=0) {
		EGI_PLOG(LOGLV_ERROR, "%s: Download fails, re_truncate file size to 0...",__func__);

		/* Resize/truncate filesize to 0! */
		if( truncate(file_save, 0)!=0 ) {
			EGI_PLOG(LOGLV_ERROR, "%s: [download fails] Fail to truncate file size to 0!",__func__);
		}

		/* Make sure to flush metadata again! */
		if( fsync(fileno(fp)) !=0 ) {
			EGI_PLOG(LOGLV_ERROR,"%s: [download fails] Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
		}

		goto CURL_FAIL;
	}

	/* NOW: ret==0 */

	/* Make sure to flush metadata before recheck file size! necessary? */
	if( fsync(fileno(fp)) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
	}

	/*** Check date length again. and try to adjust it.
	 *  1. fstat()/stat() MAY get shorter/longer filesize sometime? Not same as doubleinfo, OR curl BUG?
	 *  2. However, written bytes sum_up in write_callback function gets same as stat file size!
	 */
        struct stat     sb;

        //if( fstat(fileno(fp), &sb)<0 ) {
        if( stat(file_save, &sb)<0 ) {
                EGI_PLOG(LOGLV_ERROR, "%s: Fail call stat() for file '%s'. ERR:%s\n", __func__, file_save, strerror(errno));
        } else {
		/* !!! WARNING !!!    [st_size > doubleinfo] OR [st_size < doubleinfo], all possible! */

		if( sb.st_size != (int)doubleinfo ) {

	#if 0 ////////   A. Try to adjust final file size according to doubleinfo   ////////////////
			EGI_PLOG(LOGLV_ERROR,"%s: stat file size(%lld) is NOT same as content length downloaded (%d)! Try adjust...",
									__func__, sb.st_size, (int)doubleinfo);

			/* Resize/truncate to the same as curl downloaded size, If it's shorter, then to be paddled with 0! */
			if( truncate(file_save, (int)doubleinfo )!=0 ) {
				EGI_PLOG(LOGLV_ERROR, "%s: [fsize error] Fail to truncate file size to the same as download size!",__func__);
			}

			/* Make sure to flush metadata again! */
			if( fsync(fileno(fp)) !=0 ) {
				EGI_PLOG(LOGLV_ERROR,"%s: [fsize error] Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
			}

	#else  /////////  B. Just let it fail! Resize file_save to 0!   ///////////////
			EGI_PLOG(LOGLV_ERROR,"%s: stat file size(%lld) is NOT same as content length downloaded size(%d)! sizedown(%d). Fails.",
									__func__, sb.st_size, (int)doubleinfo, sizedown);
			ret=-10;

			/* Retry if i<3 ???? re_enter into for() ... OK ???  */
			if(i<3) {
							/* Most(all) case: i=0! */
				EGI_PLOG(LOGLV_CRITICAL,"%s: i=%d, i<3, filesize != sizedown, retry...", __func__, i);
				goto CURL_CLEANUP; 	/* To truncate file to 0 and reset offset. */
			}

			/* Resize/truncate filesize to 0! */
			if( truncate(file_save, 0)!=0 ) {
				EGI_PLOG(LOGLV_ERROR, "%s: [fsize error] Fail to truncate file size to 0!",__func__);
			}
			/* Make sure to flush metadata again! */
			if( fsync(fileno(fp)) !=0 ) {
				EGI_PLOG(LOGLV_ERROR,"%s: [fsize error] Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
			}
	#endif  /////////////////////

		}
		/* (sb.st_size == (int)doubleinfo) */
		else
			EGI_PLOG(LOGLV_CRITICAL,"%s: File size(%lld) is same as content length downloaded size(%d)!",
									__func__, sb.st_size, (int)doubleinfo);
	}


CURL_FAIL:

	/* Unlock, advisory locks only */
	if( flock(fileno(fp),LOCK_UN) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to un_flock '%s', Err'%s'", __func__, file_save, strerror(errno));
		/* Go on .. */
	}

	/* Close file */
	if(fclose(fp)!=0)
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to fclose '%s', Err'%s'", __func__, file_save, strerror(errno));

	/* Clean up */
	if(curl != NULL) {
		curl_easy_cleanup(curl);
  		curl_global_cleanup();
	}

	return ret;
}
