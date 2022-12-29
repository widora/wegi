/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


		A libcurl http helper
Refer to:  https://curl.haxx.se/libcurl/c/

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
-----------------------------------------------------------------*/
#ifndef __EGI_HTTPS__
#define __EGI_HTTPS__

#include <stdio.h>
#include <curl/curl.h>

/*  erase '__' to use http instead */
//#define SKIP_PEER_VERIFICATION
//#define SKIP_HOSTNAME_VERIFICATION

/* Https Options */
#define HTTPS_OPTION_DEFAULT            	(0)
#define HTTPS_SKIP_PEER_VERIFICATION            (1<<0)
#define HTTPS_SKIP_HOSTNAME_VERIFICATION        (1<<1)
/* Download and save by appending to the end of file, Otherwise truncate file to 0 before saving */
#define HTTPS_DOWNLOAD_SAVE_APPEND        	(1<<2)
#define HTTPS_ENABLE_REDIRECT			(1<<3)

#define CURL_RETDATA_BUFF_SIZE  (512*1024)  /* CURL RETURNED DATA BUFFER SIZE */

/* a callback function for CURL to handle returned data */
typedef size_t (* curlget_callback_t)(void *ptr, size_t size, size_t nmemb, void *userp);

/*** Examples:
	static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp)
	{
		strcat(userp,ptr);
		return size*nmemb;
	}

	static size_t download_callback(void *ptr, size_t size, size_t nmemb, void *stream)
	{
	       size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
	       return written;
	}
*/

extern char https_easy_buff[];
//static size_t easy_callback_copyToBuffer(void *ptr, size_t size, size_t nmemb, void *userp);

int https_curl_request(int opt, unsigned int trys, unsigned int timeout,
		       char *request, char *reply_buff, void *data,
			curlget_callback_t get_callback);

//static size_t easy_callback_writeToFile(void *ptr, size_t size, size_t nmemb, void *stream);
int https_easy_download(int opt, unsigned int trys, unsigned int timeout,
			const char *file_url, const char *file_save,  void *data,
                        curlget_callback_t write_callback );

int https_easy_download2( int opt, unsigned int trys,
                          const char *file_url, const char *file_save,   void *data,
                          curlget_callback_t write_callback );

int https_easy_stream( int opt, unsigned int trys, unsigned int timeout,
                       const char *stream_url, int nr, const char **reqs, void *data,
                       curlget_callback_t write_callback, curlget_callback_t header_callback );

/* ----- MultiThread Easy Download ----- */
size_t https_easy_getFileSize(const char *file_url, int opt);
//static size_t callback_mThreadWriteToFile(void *ptr, size_t size, size_t nmemb, void *arg)
//void *thread_easy_download(void *arg)
int https_easy_mThreadDownload(int opt, unsigned int nthreads, unsigned int trys, unsigned int timeout,
                                const char *file_url, const char *file_save );

int https_easy_mURLDownload( unsigned int n, const char **urls, const char *file_save,
                             int opt, unsigned int trys, unsigned int timeout );

#endif
