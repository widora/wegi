/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

		A libcurl http helper
Refer to:  https://curl.haxx.se/libcurl/c/

Note:
1. Unknown SSL protocol error:
   Old version of CURL and OpenSSL do not support some new SSL
   protocols(with elliptic-curve key etc.), Update to CURL 7.68
   and OpenSSL 1.1.1b...
2. Repeated call to curl_global_init and curl_global_cleanup should
   be avoided.
3. (MT7688 OPenwrt) A BLUETooth USB dongle (for mouse/keyboard etc.)
   MAY (clog/hamper) (CURL/http/wifi?) connecting/downloading!
   Just unplug it if such case arises!

Jurnal:
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
2021-09-29:
	1.https_curl_request()/_easy_download(): Append HTTP request header.
2021-11-11:
	1. curl_easy_strerror()
	2. Add https_easy_download2()
2021-11-14:
        1. Add https_easy_stream()
2021-11-20:
	1. https_easy_stream( ..nr, reqs..): additional request lines.
2021-11-24:
	1. https_curl_request(): add param timeout.
	2. https_easy_download(): add param timeout.

2021-11-30:
	1.Add CURLOPT_ACCEPT_ENCODING to enables automatic decompression of HTTP downloads.
		https_curl_request()
2021-12-02:
	1. https_easy_download(): Enable CURLOPT_ACCEPT_ENCODING.
2021-12-15:
	1. https_curl_request():
	   1.1 Add header append: Content-Type:charset=UTF-8
	   1.2 curl_slist_free_all(header)
2021-12-20:
	1. https_easy_download(): opt to save by appending to the file.
2021-12-21:
	1. https_curl_request(): opt to follow HTTP 3xx redirects.
2021-12-27:
	1. Check if curl_global_init() does NOT return CURLE_OK.
	2. Each time curl_easy_init() fails, call curl_global_cleanup() then.
2021-12-28:
	1. Set header to NULL after curl_slist_free_all(header)!
2022-01-01:
	1. https_easy_download():  Enable option CURLOPT_HTTPGET.
2022-03-21:
	1. Add fflush(fb) before fsync(fileno(fp));

TODO:
XXX 1. fstat()/stat() MAY get shorter/longer filesize sometime? Not same as doubleinfo, OR curl BUG?
    compressed data size  ---OK, compression data.

XXX 2. After looping curl_global_init() +..+ curl_global_cleanup() with enough circle times, it will
   unexpectedly throw out 'Aborted' and exit just at calling curl_global_init()!

3. Set TCP KEEPALIVE option, and DO NOT clean up curl at end of each session,
   try to REUSE curl, to SAVE init time. ----> for streams only.
   MidasHK_2022_04_01

4. To check HTTP response status code! --- use void *data to pass out.
   Downloading session may still fail though function returns 0!
   Example: Status code 404, Resource NOT found.  MidasHK_2022_04_01


Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
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
#include "egi_debug.h"
#include "egi_utils.h"

#define EGI_CURL_TIMEOUT	3  /* 15 in seconds. tricky for M3U, abt. SegDuration/3trys */

/*--------------------------------
This is from CURL lib/strerror.c
---------------------------------*/
const char *curl_easy_strerror(CURLcode error)
{
  switch (error) {
  case CURLE_OK:
    return "No error";

  case CURLE_UNSUPPORTED_PROTOCOL:
    return "Unsupported protocol";

  case CURLE_FAILED_INIT:
    return "Failed initialization";

  case CURLE_URL_MALFORMAT:
    return "URL using bad/illegal format or missing URL";

  case CURLE_NOT_BUILT_IN:
    return "A requested feature, protocol or option was not found built-in in"
      " this libcurl due to a build-time decision.";

  case CURLE_COULDNT_RESOLVE_PROXY:
    return "Couldn't resolve proxy name";

  case CURLE_COULDNT_RESOLVE_HOST:
    return "Couldn't resolve host name";

  case CURLE_COULDNT_CONNECT:
    return "Couldn't connect to server";

  case CURLE_FTP_WEIRD_SERVER_REPLY:
    return "FTP: weird server reply";

  case CURLE_REMOTE_ACCESS_DENIED:
    return "Access denied to remote resource";

  case CURLE_FTP_ACCEPT_FAILED:
    return "FTP: The server failed to connect to data port";

  case CURLE_FTP_ACCEPT_TIMEOUT:
    return "FTP: Accepting server connect has timed out";

  case CURLE_FTP_PRET_FAILED:
    return "FTP: The server did not accept the PRET command.";

  case CURLE_FTP_WEIRD_PASS_REPLY:
    return "FTP: unknown PASS reply";

  case CURLE_FTP_WEIRD_PASV_REPLY:
    return "FTP: unknown PASV reply";

  case CURLE_FTP_WEIRD_227_FORMAT:
    return "FTP: unknown 227 response format";

  case CURLE_FTP_CANT_GET_HOST:
    return "FTP: can't figure out the host in the PASV response";

  case CURLE_HTTP2:
    return "Error in the HTTP2 framing layer";

  case CURLE_FTP_COULDNT_SET_TYPE:
    return "FTP: couldn't set file type";

  case CURLE_PARTIAL_FILE:
    return "Transferred a partial file";

  case CURLE_FTP_COULDNT_RETR_FILE:
    return "FTP: couldn't retrieve (RETR failed) the specified file";

  case CURLE_QUOTE_ERROR:
    return "Quote command returned error";

  case CURLE_HTTP_RETURNED_ERROR:
    return "HTTP response code said error";

  case CURLE_WRITE_ERROR:
    return "Failed writing received data to disk/application";

  case CURLE_UPLOAD_FAILED:
    return "Upload failed (at start/before it took off)";

  case CURLE_READ_ERROR:
    return "Failed to open/read local data from file/application";

  case CURLE_OUT_OF_MEMORY:
    return "Out of memory";

  case CURLE_OPERATION_TIMEDOUT:
    return "Timeout was reached";

  case CURLE_FTP_PORT_FAILED:
    return "FTP: command PORT failed";

  case CURLE_FTP_COULDNT_USE_REST:
    return "FTP: command REST failed";

  case CURLE_RANGE_ERROR:
    return "Requested range was not delivered by the server";

  case CURLE_HTTP_POST_ERROR:
    return "Internal problem setting up the POST";

  case CURLE_SSL_CONNECT_ERROR:
    return "SSL connect error";

  case CURLE_BAD_DOWNLOAD_RESUME:
    return "Couldn't resume download";

  case CURLE_FILE_COULDNT_READ_FILE:
    return "Couldn't read a file:// file";

  case CURLE_LDAP_CANNOT_BIND:
    return "LDAP: cannot bind";

  case CURLE_LDAP_SEARCH_FAILED:
    return "LDAP: search failed";

  case CURLE_FUNCTION_NOT_FOUND:
    return "A required function in the library was not found";

  case CURLE_ABORTED_BY_CALLBACK:
    return "Operation was aborted by an application callback";

  case CURLE_BAD_FUNCTION_ARGUMENT:
    return "A libcurl function was given a bad argument";

  case CURLE_INTERFACE_FAILED:
    return "Failed binding local connection end";

  case CURLE_TOO_MANY_REDIRECTS :
    return "Number of redirects hit maximum amount";

  case CURLE_UNKNOWN_OPTION:
    return "An unknown option was passed in to libcurl";

  case CURLE_TELNET_OPTION_SYNTAX :
    return "Malformed telnet option";

  case CURLE_PEER_FAILED_VERIFICATION:
    return "SSL peer certificate or SSH remote key was not OK";

  case CURLE_GOT_NOTHING:
    return "Server returned nothing (no headers, no data)";

  case CURLE_SSL_ENGINE_NOTFOUND:
    return "SSL crypto engine not found";

  case CURLE_SSL_ENGINE_SETFAILED:
    return "Can not set SSL crypto engine as default";

  case CURLE_SSL_ENGINE_INITFAILED:
    return "Failed to initialise SSL crypto engine";

  case CURLE_SEND_ERROR:
    return "Failed sending data to the peer";

  case CURLE_RECV_ERROR:
    return "Failure when receiving data from the peer";

  case CURLE_SSL_CERTPROBLEM:
    return "Problem with the local SSL certificate";

  case CURLE_SSL_CIPHER:
    return "Couldn't use specified SSL cipher";

  case CURLE_SSL_CACERT:
    return "Peer certificate cannot be authenticated with given CA "
      "certificates";

  case CURLE_SSL_CACERT_BADFILE:
    return "Problem with the SSL CA cert (path? access rights?)";

  case CURLE_BAD_CONTENT_ENCODING:
    return "Unrecognized or bad HTTP Content or Transfer-Encoding";

  case CURLE_LDAP_INVALID_URL:
    return "Invalid LDAP URL";

  case CURLE_FILESIZE_EXCEEDED:
    return "Maximum file size exceeded";

  case CURLE_USE_SSL_FAILED:
    return "Requested SSL level failed";

  case CURLE_SSL_SHUTDOWN_FAILED:
    return "Failed to shut down the SSL connection";

  case CURLE_SSL_CRL_BADFILE:
    return "Failed to load CRL file (path? access rights?, format?)";

  case CURLE_SSL_ISSUER_ERROR:
    return "Issuer check against peer certificate failed";

  case CURLE_SEND_FAIL_REWIND:
    return "Send failed since rewinding of the data stream failed";

  case CURLE_LOGIN_DENIED:
    return "Login denied";

  case CURLE_TFTP_NOTFOUND:
    return "TFTP: File Not Found";

  case CURLE_TFTP_PERM:
    return "TFTP: Access Violation";

  case CURLE_REMOTE_DISK_FULL:
    return "Disk full or allocation exceeded";

  case CURLE_TFTP_ILLEGAL:
    return "TFTP: Illegal operation";

  case CURLE_TFTP_UNKNOWNID:
    return "TFTP: Unknown transfer ID";

  case CURLE_REMOTE_FILE_EXISTS:
    return "Remote file already exists";

  case CURLE_TFTP_NOSUCHUSER:
    return "TFTP: No such user";

  case CURLE_CONV_FAILED:
    return "Conversion failed";

  case CURLE_CONV_REQD:
    return "Caller must register CURLOPT_CONV_ callback options";

  case CURLE_REMOTE_FILE_NOT_FOUND:
    return "Remote file not found";

  case CURLE_SSH:
    return "Error in the SSH layer";

  case CURLE_AGAIN:
    return "Socket not ready for send/recv";

  case CURLE_RTSP_CSEQ_ERROR:
    return "RTSP CSeq mismatch or invalid CSeq";

  case CURLE_RTSP_SESSION_ERROR:
    return "RTSP session error";

  case CURLE_FTP_BAD_FILE_LIST:
    return "Unable to parse FTP file list";

  case CURLE_CHUNK_FAILED:
    return "Chunk callback failed";

  case CURLE_NO_CONNECTION_AVAILABLE:
    return "The max connection limit is reached";

  case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
    return "SSL public key does not match pinned public key";

  /* error codes not used by current libcurl */
  case CURLE_OBSOLETE20:
  case CURLE_OBSOLETE24:
  case CURLE_OBSOLETE29:
  case CURLE_OBSOLETE32:
  case CURLE_OBSOLETE40:
  case CURLE_OBSOLETE44:
  case CURLE_OBSOLETE46:
  case CURLE_OBSOLETE50:
  case CURLE_OBSOLETE57:
  case CURL_LAST:
  break;

  }

  return "Unknown error";
}

/*------------------------------------------------------------------------------
			HTTPS request by libcurl

Note: You must have installed ca-certificates before call curl https, or
      define SKIP_PEER/HOSTNAME_VERIFICATION to use http instead.

@opt:			Http options, Use '|' to combine.
			HTTPS_SKIP_PEER_VERIFICATION
			HTTPS_SKIP_HOSTNAME_VERIFICATION

@trys:			Max. try times.
			If trys==0; unlimit.
@timeout:		Timeout in seconds.
@request:	request string
@reply_buff:	Buffer for returned reply string, the Caller must ensure enough space.

				!!! --- CAUTION --- !!!
		Insufficient memspace causes segmentation_fault and other strange things.
		In the get_callback() function, it MUST check available space left
		if reply_buff[], for each chunck session.
		Example:
		   static size_t get_callback(void *ptr, size_t size, size_t nmemb, void *userp)
		   {
			int xlen=strlen(ptr)+strlen(reply_buff);
        		if( xlen >= sizeof(reply_buff)-1) {
				strncat(userp, ptr, sizeof(reply_buff)-1-strlen(reply_buff));
				....  // OR: CURL_RETDATA_BUFF_SIZE-1-strlen(..)
			}
			else {
				strcat(userp, ptr);
				....
			}
		   }

@data:		TODO: if any more data needed

		!!! CURL will disable egi tick timer? !!!
Return:
	0	Ok
	<0	Fails
--------------------------------------------------------------------------------*/
int https_curl_request( int opt, unsigned int trys, unsigned int timeout, const char *request, char *reply_buff,
			void *data, curlget_callback_t get_callback )
{
	int i;
	int ret=0;
  	CURL *curl;
  	CURLcode res=CURLE_OK;
	double doubleinfo=0;
	long   longinfo=0;
	struct curl_slist *header=NULL;

 /* Try Max. sessions. TODO: tm_delay() not accurate, delay time too short!  */
 for(i=0; i<trys || trys==0; i++)
 {
	/* init curl */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init ...",__func__);
	if( curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK ) {
		EGI_PLOG(LOGLV_ERROR, "%s: curl_global_init() fails!",__func__);
  		curl_global_cleanup();
		return -1;
	}

	EGI_PLOG(LOGLV_INFO, "%s: start curl_easy_init...",__func__);
	curl = curl_easy_init();
	if(curl==NULL) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl!",__func__);
  		curl_global_cleanup();
		return -1;
	}

	/* Set curl options */
	EGI_PLOG(LOGLV_INFO, "%s: start to set curl options ...",__func__);
	curl_easy_setopt(curl, CURLOPT_URL, request);		 	 /* set request URL */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); //1L		 /* 1 print more detail */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout); //EGI_CURL_TIMEOUT);	 /* set timeout */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_callback);     /* set write_callback */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, reply_buff); 		 /* set data dest for write_callback */

	/* User options */
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, (opt&HTTPS_ENABLE_REDIRECT)?1L:0L); /* Enable redirect */

	/* TODO:  unsupported compression method may produce corrupted data! */
	/* "" --- Containing all built-in supported encodings. */
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

	/* Append HTTP request header */
	header=curl_slist_append(header,"User-Agent: CURL(Linux; Egi)");
//	header=curl_slist_append(header,"User-Agent: Mozilla/5.0(Linux; Android 8.0)");
	header=curl_slist_append(header,"Accept-Charset: utf-8");
//	header=curl_slist_append(header,"Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER,header);

	/* Param opt set */
	if(HTTPS_SKIP_PEER_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	/* Perform the request, res will get the return code */
	EGI_PLOG(LOGLV_CRITICAL, "%s: Try [%d]th curl_easy_perform()... ", __func__, i);
	if(  (res=curl_easy_perform(curl)) !=CURLE_OK ) {
	       egi_dpstd("curl_easy_perform() failed! res=%d, Err'%s'\n",res, curl_easy_strerror(res));
		ret=-2;
		printf("%s: tm_delayms..\n",__func__);
		tm_delayms(200);
		printf("%s: try again...\n",__func__);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}

	/* 				--- Check session info. ---
	 ***  Note: curl_easy_perform() may result in CURLE_OK, but curl_easy_getinfo() may still fail!
	 */
/* TEST: ---> */
	EGI_PLOG(LOGLV_CRITICAL, "%s: Start to get response code...", __func__);
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &longinfo) ) {
		EGI_PLOG(LOGLV_CRITICAL,"%s: get response code '%ld'.",__func__, longinfo);
		if(longinfo==0) {
			egi_dpstd("No response from the peer/server!\n");
			ret=-2;
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
	}

#if 0 ///////////////// Decompressed data lenght MAY NOT same as doubleinfo size ///////////////
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
		else {
			ret=0;
		  	/* ----- OK! End Session ----- */
		}
	}
	else { 	/* Getinfo fails */
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to easy getinfo CURLINFO_CONTENT_LENGTH_DOWNLOAD!", __func__);
		ret=-5;
		tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}
#endif //////////////////////

CURL_CLEANUP:
	/* always cleanup */
	curl_slist_free_all(header);
	header=NULL;
	curl_easy_cleanup(curl);
	curl=NULL;
  	curl_global_cleanup();

	/* if succeeds --- OK --- */
	if(ret==0)
		break;

 } /* End: try Max.3 times */

	if(ret!=0)
		EGI_PLOG(LOGLV_ERROR,"%s: Fail after try %d times!", __func__, i);


	return ret;
}


/*-----------------------------------------------
 A default callback for https_easy_download().
------------------------------------------------*/
static size_t easy_callback_writeToFile(void *ptr, size_t size, size_t nmemb, void *stream)
{
   size_t chunksize=size*nmemb;
   size_t written=0;

   if(stream) {
        written = fwrite(ptr, size,  nmemb, (FILE *)stream);
        //printf("%s: written size=%zd\n",__func__, written);
        if(written<0) {
                EGI_PLOG(LOGLV_ERROR,"xxxxx  Curl callback:  fwrite error!  written<0!  xxxxx");
                written=0;
        }
	else if(written!=chunksize) {
                EGI_PLOG(LOGLV_ERROR,"xxxxx  Curl callback:  fwrite error!  write %uz of datasize %uz!  xxxxx", written, chunksize);
	}
   }

   //return written;
   return chunksize; /* if(retsize != size*nmemeb) curl will fails!? */
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
@trys:			Max. try times.
			If trys==0; unlimit.
@file_url:		file url for downloading.
@file_save:		file path for saving received file.
@data:			TODO: if more data needed
@write_callback:	Callback for writing received data

		!!! CURL will disable egi tick timer? !!!

Return:
	0	ok
	<0	fails
-------------------------------------------------------------------------------*/
int https_easy_download( int opt, unsigned int trys, unsigned int timeout,
			 const char *file_url, const char *file_save,   void *data,
			 curlget_callback_t write_callback )
{
	int i;
	int ret=0;
  	CURL *curl=NULL;
  	CURLcode res;
	double doubleinfo=0;
	long longinfo=0;
	FILE *fp;	/* FILE to save received data */
	int sizedown;
	size_t ofsize;
	struct curl_slist *header=NULL;

	/* check input */
	if(file_url==NULL || file_save==NULL)
		return -1;

	/* Open file for saving file */
	if(opt&HTTPS_DOWNLOAD_SAVE_APPEND) {
	    fp=fopen(file_save,"ab");
	    /* Get original file size */
	    ofsize=egi_get_fileSize(file_save);
	}
	else
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

 /* Try Max. tru times */
 for(i=0; i<trys || trys==0; i++)
 {
	/* init curl */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init and easy init...",__func__);
        if( curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK ) {
                EGI_PLOG(LOGLV_ERROR, "%s: curl_global_init() fails!",__func__);
  		curl_global_cleanup();
		return -1;
        }

	curl = curl_easy_init();
	if(curl==NULL) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl!",__func__);
		ret=-2;
  		curl_global_cleanup();
		goto CURL_END;
	}

	/***   ---  Set options for CURL  ---   ***/
	/* set download file URL */
	curl_easy_setopt(curl, CURLOPT_URL, file_url);

#if 0   /* Setopt TCP KEEPALIVE */
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
#endif

	/*  1L --print more detail,  0L --disbale */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L); //1L
   	/*  1L --disable progress meter, 0L --enable and disable debug output  */
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	/*  1L --enable redirection */
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	/*  set timeout */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout); //EGI_CURL_TIMEOUT);

	/* TODO:  unsupported compression method may produce corrupted data! */
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");		 /* enables automatic decompression of HTTP downloads */

	/* Force the HTTP request to get back to using GET. in case the serve NOT provide Access-Control-Allow-Methods! */
	curl_easy_setopt(curl,CURLOPT_HTTPGET, 1L);

	/* For name lookup timed out error: use ipv4 as default */
	//  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

	/* set write_callback to write/save received data  */
	if(write_callback)
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	else /* default */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, easy_callback_writeToFile);

	/* Append HTTP request header */
	header=curl_slist_append(header,"User-Agent: CURL(Linux; Egi)");
	//header=curl_slist_append(header,"User-Agent: Mozilla/5.0(Linux; Android 8.0)");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);

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
		//if(res ==  CURLE_URL_MALFORMAT )
		//	EGI_PLOG(LOGLV_ERROR,"%s: curl_easy_perform() failed because of URL malformat!", __func__);
		//else if( res == CURLE_OPERATION_TIMEDOUT )  /* res==28 */
		//	EGI_PLOG(LOGLV_ERROR,"%s: curl_easy_perform() Timeout!", __func__);
		//else
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
/* TEST: ---> */
	EGI_PLOG(LOGLV_CRITICAL, "%s: Start to get response code...", __func__);
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &longinfo) ) {
		EGI_PLOG(LOGLV_CRITICAL,"%s: get response code '%ld'.",__func__, longinfo);
		if(longinfo==0) {
			egi_dpstd("No response from the peer/server!\n");
			ret=-2;
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
	}

	/* TEST: CURLINFO_SIZE_DOWNLOAD */
/* TEST: ---> */
	EGI_PLOG(LOGLV_CRITICAL, "%s: Start to get sizedown...", __func__);
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &doubleinfo) ) {
		  	EGI_PLOG(LOGLV_CRITICAL,"%s: Curlinfo_size_download=%d!",  __func__, (int)doubleinfo);
			sizedown=(int)doubleinfo;
	}

#if 0 /////////////////////   Decompressed, st_size MAY NOT as doubleinfo size ///////////////////////////////

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
			ret=0;
		  	break; 	/* ----- OK ----- */
		}
	}
	else {	/* Getinfo fails */
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to easy getinfo CURLINFO_CONTENT_LENGTH_DOWNLOAD!", __func__);
		ret=-6;
		//tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}
#endif //////////////////////////////////////////////

CURL_CLEANUP:
	/* If succeeds, break loop! */
	if(ret==0)
		break;

	egi_dpstd("Curl clean up before try again... the file will be truncated to 0 or ofsize!\n");

	/* If OK, it should NOT carry out following jobs ... */
	if(opt&HTTPS_DOWNLOAD_SAVE_APPEND) {
		truncate(file_save, ofsize);
		fseek(fp, ofsize, SEEK_SET); /* truncate will NOT reset file offset! */
	}
	else {
		truncate(file_save, 0);
		rewind(fp);	/* truncate will NOT reset file offset! */
	}

	tm_delayms(200);

	/* Always cleanup before loop back to init curl */
	curl_slist_free_all(header);
	header=NULL;
	curl_easy_cleanup(curl);
	curl=NULL; /* OR curl_easy_cleanup() will crash at last! */
  	curl_global_cleanup();
 } /* End: try Max times */

	/* Check result, If it fails, truncate filesize to 0 or ofsize before quit!    ??? NOT happens!! */
	if(ret!=0) {
		EGI_PLOG(LOGLV_ERROR, "%s:  Download fails, re_truncate file size to 0 or ofsize",__func__);

		/* Resize/truncate filesize to 0! */
		if(opt&HTTPS_DOWNLOAD_SAVE_APPEND) {
		     if(truncate(file_save, ofsize)!=0)
			EGI_PLOG(LOGLV_ERROR, "%s: [download fails] Fail to truncate file size to ofsize!",__func__);
		}
		else if( truncate(file_save, 0)!=0 ) {
			EGI_PLOG(LOGLV_ERROR, "%s: [download fails] Fail to truncate file size to 0!",__func__);
		}

		/* Make sure to flush metadata again! */
		fflush(fp); // fflush first :>>>
		if( fsync(fileno(fp)) !=0 ) {
			EGI_PLOG(LOGLV_ERROR,"%s: [download fails] Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
		}

		goto CURL_END;
	}

	/* NOW: ret==0 */

	/* Make sure to flush metadata before recheck file size! necessary? */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start fsync file...\n", __func__);
	fflush(fp); // fflush first :>>>
	if( fsync(fileno(fp)) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
	}


#if 0 /////////////////////   Decompressed, st_size MAY NOT as doubleinfo size ///////////////////////////////
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
			fflush(fp);
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
			fflush(fp);
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
#endif ////////////////////////////////////////

CURL_END:

	/* Unlock, advisory locks only */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start flock file...\n",__func__);
	if( flock(fileno(fp),LOCK_UN) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to un_flock '%s', Err'%s'", __func__, file_save, strerror(errno));
		/* Go on .. */
	}

	/* Close file */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start fclose(fp)...\n",__func__);
	if(fclose(fp)!=0)
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to fclose '%s', Err'%s'", __func__, file_save, strerror(errno));

	/* Clean up */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start curl clean...\n",__func__);
	if(curl != NULL) {
		curl_slist_free_all(header); header=NULL;
		curl_easy_cleanup(curl); curl=NULL;
  		curl_global_cleanup();
	}

	return ret;
}

/*-------------------------------------------------------------------------------
Download https stream data, and save them into separated files! Size of each file
can be controlled by the Caller in write_callbck.

Note:
1. Assume that the remote object/file is a stream in chunk data.
2. Set a big timeout value to avoid frequent breakout and clean_up of easy_perform,
   and a new round(new file) of easy_perform may receive repeated data at first.

-----------------------------------------------------------------------------*/
int https_easy_download2( int opt, unsigned int trys,
			  const char *file_url, const char *file_save,   void *data,
			  curlget_callback_t write_callback )
{
	int i;
	int ret=0;
  	CURL *curl=NULL;
  	CURLcode res;
	double doubleinfo=0;
	long filetime;
	FILE *fp;	/* FILE to save received data */
	int sizedown;
	struct curl_slist *header=NULL;

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

 /* Try Max. trys.. */
 for(i=0; i<trys || trys==0; i++)
 {
	/* init curl */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init and easy init...",__func__);
        if( curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK ) {
                EGI_PLOG(LOGLV_ERROR, "%s: curl_global_init() fails!",__func__);
  		curl_global_cleanup();
		return -1;
        }

	curl = curl_easy_init();
	if(curl==NULL) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl!",__func__);
  		curl_global_cleanup();
		ret=-2;
		goto CURL_END;
	}

	/***   ---  Set options for CURL  ---   ***/
	/* set download file URL */
	curl_easy_setopt(curl, CURLOPT_URL, file_url);
	//curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

#if 0	/* Setopt TCP KEEPALIVE */
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
#endif

	/* Just request head, without body */
//	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
//	curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL); /* use selfdefined data as headerdata */

	/* Callback when get Header data  */
//	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

	/* Setopt for file time */
	curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);

	/*  1L --print more detail,  0L --disbale */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L); //1L
   	/*  1L --disable progress meter, 0L --enable and disable debug output  */
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	/*  1L --enable redirection */
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	/*  set timeout. 0--never timeout, Give it a value, to prevent is from stucking in! */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60); //EGI_CURL_TIMEOUT);

	/* TODO:  unsupported compression method may produce corrupted data! */
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

	/* Set CA bundle */
	//curl_easy_setopt(curl, CURLOPT_CAINFO, "ca-bundle.crt");

	/* For name lookup timed out error: use ipv4 as default */
	//curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

	/* set write_callback to write/save received data  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

	/* Append HTTP request header */
	header=curl_slist_append(header,"User-Agent: CURL(Linux; Egi)");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER,header);

	/* Param opt set */
	if(HTTPS_SKIP_PEER_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);


	/* Set data destination for write_callback */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

	EGI_PLOG(LOGLV_CRITICAL, "%s: Try [%d]th curl_easy_perform()... ", __func__, i);

/* Perform to download data. */
while(1) {
	/* then download body */
	curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);

	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		/* Check Curl config: #ifdef CURL_DISABLE_VERBOSE_STRINGS! */
		EGI_PLOG(LOGLV_ERROR,"%s: res=%d, curl_easy_perform() failed! Err'%s'", __func__, res, curl_easy_strerror(res));

		ret=-3;

		if(res == CURLE_OPERATION_TIMEDOUT)
			continue;

		else if(res == CURLE_WRITE_ERROR)
			goto CURL_END;
	}
	/* NOW: CURLE_OK, all data received. reset ret! */
	else
		ret=0;


	/* 				--- Check session info. ---
	 *** Note:
	 *   1. curl_easy_perform() may result in CURLE_OK, but curl_easy_getinfo() may still fail!
	 *   2. Testing results: CURLINFO_SIZE_DOWNLOAD and CURLINFO_CONTENT_LENGTH_DOWNLOAD get the same doubleinfo!
	 */
	/* CURLINFO_FILETIME */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_FILETIME, &filetime) ) {
		if(filetime<0)
			EGI_PLOG(LOGLV_CRITICAL, "%s: CURLINFO_FILETIME  <0!", __func__ );
		else {
			time_t ftime=(time_t)filetime;
			egi_dpstd(">>>>>>>> File time: %s\n", ctime(&ftime));
		}
	}
	else
		EGI_PLOG(LOGLV_CRITICAL, "%s: Fail to easy_getinfo CURLINFO_FILETIME!", __func__ );

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
			ret=0;
		  	break; 	/* ----- OK ----- */
		}
	}
	else {	/* Getinfo fails */
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to easy getinfo CURLINFO_CONTENT_LENGTH_DOWNLOAD!", __func__);
		ret=-6;
		//tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}

} /* END WHILE */


CURL_CLEANUP:
	/* If succeeds, break loop! */
	if(ret==0)
		break;

	egi_dpstd("Curl clean up before try again... the file will be truncated to 0!\n");
	/* If OK, it should NOT carry out following jobs ... */
	truncate(file_save, 0);
	rewind(fp);	/* truncate will NOT reset file offset! */
	tm_delayms(200);

	/* Always cleanup before loop back to init curl */
	curl_slist_free_all(header);
	header=NULL;
	curl_easy_cleanup(curl);
	curl=NULL; /* OR curl_easy_cleanup() will crash at last! */
  	curl_global_cleanup();

 } /* End: try Max. TIMEOUTs */


#if 0	/* Check result, If it fails, truncate filesize to 0 before quit!    ??? NOT happens!! */
	if(ret!=0) {
		EGI_PLOG(LOGLV_ERROR, "%s: Download fails, re_truncate file size to 0...",__func__);

		/* Resize/truncate filesize to 0! */
		if( truncate(file_save, 0)!=0 ) {
			EGI_PLOG(LOGLV_ERROR, "%s: [download fails] Fail to truncate file size to 0!",__func__);
		}

		/* Make sure to flush metadata again! */
		fflush(fp);
		if( fsync(fileno(fp)) !=0 ) {
			EGI_PLOG(LOGLV_ERROR,"%s: [download fails] Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
		}

		goto CURL_END;
	}
#endif //////////////////////

	/* NOW: ret==0 */

	/* Make sure to flush metadata before recheck file size! necessary? */
	fflush(fp);
	if( fsync(fileno(fp)) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
	}


#if 0  ///////////////// NOT for stream data!  ////////////
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
			fflush(fp);
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
			fflush(fp);
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
#endif  //////////////////////////////////////////////////


CURL_END:
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
		curl_slist_free_all(header); header=NULL;
		curl_easy_cleanup(curl); curl=NULL;
  		curl_global_cleanup();
	}

	return ret;
}


/*--------------------------------------------------------------------
Download https data as continuous stream.

@opt:			Http options, Use '|' to combine.
			HTTPS_SKIP_PEER_VERIFICATION
			HTTPS_SKIP_HOSTNAME_VERIFICATION
@trys:			Max. try times.
			If trys==0; unlimit.
@timeout:		Timeout in seconds.
			A proper timeout value matching with RingBuffer
			makes stream play smoothly.
			Too small value may result in receiving repeated data.
			Too big value cause to break stream playing often.
@stream_url:		URL of the stream
@nr:			Items of reqs[].
@reqs			Pointers to addtional HTTP request lines
@data:			Data destination for write_callback
@write_callback:	Callback for writing received data
@header_callback	Callback for each head line replied.

Note:
1. Assume that the remote object/file is a stream in chunk data.

----------------------------------------------------------------------*/
int https_easy_stream( int opt, unsigned int trys, unsigned int timeout,
			 const char *stream_url, int nr, const char **reqs, void *data,
			 curlget_callback_t write_callback, curlget_callback_t header_callback )
{
	int i,k;
	int ret=0;
  	CURL *curl=NULL;
  	CURLcode res;
	double doubleinfo=0;
	long longinfo=0;
	long filetime;
	int sizedown;
	struct curl_slist *header=NULL;

	/* Check input */
	if(stream_url==NULL)
		return -1;

 /* Try Max. trys.. */
 for(i=0; i<trys || trys==0; i++)
 {
	/* init curl */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init and easy init...",__func__);
        if( curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK ) {
                EGI_PLOG(LOGLV_ERROR, "%s: curl_global_init() fails!",__func__);
  		curl_global_cleanup();
		return -1;
        }

	curl = curl_easy_init();
	if(curl==NULL) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl!",__func__);
		ret=-2;
  		curl_global_cleanup();
		goto CURL_END;
	}

	/***   ---  Set options for CURL  ---   ***/
	/* set download file URL */
	curl_easy_setopt(curl, CURLOPT_URL, stream_url);
	//curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

#if 0	/* Setopt TCP KEEPALIVE */
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
#endif

	/* Just request head, without body */
//	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
//	curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL); /* use selfdefined data as headerdata */

	/* Callback when get Header data  */
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

	/* Setopt for file time */
	curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);

	/*  1L --print more detail,  0L --disbale */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L); //1L
   	/*  1L --disable progress meter, 0L --enable and disable debug output  */
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	/*  1L --enable redirection */
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	/*  set timeout. 0--never timeout. give it a value, to prevent it from stucking in. */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

	/* TODO:  unsupported compression method may produce corrupted data! */
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

	/* Set CA bundle */
	//curl_easy_setopt(curl, CURLOPT_CAINFO, "ca-bundle.crt");

	/* For name lookup timed out error: use ipv4 as default */
	//curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

	/* set write_callback to write/save received data  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

	/* Append HTTP request header */
	header=curl_slist_append(header,"User-Agent: CURL(Linux; Egi)");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER,header);

	/* User input request header lines */
	if(nr>0) {
	   for(k=0; k<nr; k++) {
		//header=curl_slist_append(header,"Icy-MetaData:1");
		header=curl_slist_append(header, reqs[k]);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER,header);
	   }
	}

	/* Param opt set */
	if(HTTPS_SKIP_PEER_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	/* Set data destination for write_callback */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

	EGI_PLOG(LOGLV_CRITICAL, "%s: Try [%d]th curl_easy_perform()... ", __func__, i);

/* Perform to download data. */
while(1) {

	curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);

	/* Receive stream data without break. */
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		/* Check Curl config: #ifdef CURL_DISABLE_VERBOSE_STRINGS! */
		EGI_PLOG(LOGLV_ERROR,"%s: res=%d, curl_easy_perform() failed! Err'%s'", __func__, res, curl_easy_strerror(res));
		ret=-3;

		if(res == CURLE_OPERATION_TIMEDOUT)
			continue;
		else if(res == CURLE_WRITE_ERROR)
			goto CURL_END;
	}
	/* NOW: CURLE_OK. All stream data received! */
	else
		ret=0;

	/* 				--- Check session info. ---
	 *** Note:
	 *   1. curl_easy_perform() may result in CURLE_OK, but curl_easy_getinfo() may still fail!
	 *   2. Testing results: CURLINFO_SIZE_DOWNLOAD and CURLINFO_CONTENT_LENGTH_DOWNLOAD get the same doubleinfo!
	 */

	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &longinfo) ) {
		EGI_PLOG(LOGLV_CRITICAL,"%s: get response code '%ld'.",__func__, longinfo);
		if(longinfo==0) {
			egi_dpstd("No response from the peer/server!\n");
			ret=-2;
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
	}
	/* TEST: CURLINFO_SIZE_DOWNLOAD */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &doubleinfo) ) {
		  	EGI_PLOG(LOGLV_CRITICAL,"%s: Curlinfo_size_download=%d!",  __func__, (int)doubleinfo);
			sizedown=(int)doubleinfo;
	}


#if 0 /////////////////////  Decompressed data MAY NOT as doubleinfo size ///////////////////////////////

	/* CURLINFO_FILETIME */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_FILETIME, &filetime) ) {
		if(filetime<0)
			EGI_PLOG(LOGLV_CRITICAL, "%s: CURLINFO_FILETIME  <0!", __func__ );
		else {
			time_t ftime=(time_t)filetime;
			egi_dpstd(">>>>>>>> File time: %s\n", ctime(&ftime));
		}
	}
	else
		EGI_PLOG(LOGLV_CRITICAL, "%s: Fail to easy_getinfo CURLINFO_FILETIME!", __func__ );

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
			ret=0;
		  	break; 	/* ----- OK ----- */
		}
	}
	else {	/* Getinfo fails */
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to easy getinfo CURLINFO_CONTENT_LENGTH_DOWNLOAD!", __func__);
		ret=-6;
		//tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}
#endif /////////////////////////////////////////////////////////////////////


} /* END WHILE */

CURL_CLEANUP:
	/* If succeeds, break loop! */
	if(ret==0)
		break;

	/* If OK, it should NOT carry out following jobs ... */
	egi_dpstd("Curl clean up before try again...\n");

	/* Always cleanup before loop back to init curl */
	curl_slist_free_all(header);
	header=NULL;
	curl_easy_cleanup(curl);
	curl=NULL; /* OR curl_easy_cleanup() will crash at last! */
  	curl_global_cleanup();

 } /* End: try Max. TIMEOUTs */


CURL_END:
	/* Clean up */
	if(curl != NULL) {
		curl_slist_free_all(header); header=NULL;
		curl_easy_cleanup(curl); curl=NULL;
  		curl_global_cleanup();
	}

	return ret;
}
