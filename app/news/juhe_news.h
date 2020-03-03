/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------------*/
#ifndef __JUHE_NEWS_H__
#define __JUHE_NEWS_H__
#include <stdio.h>
#include <json-c/json.h>
#include <json-c/json_object.h>
#include "egi_filo.h"

/* Callback functions for libcurl API */
size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp);
size_t download_callback(void *ptr, size_t size, size_t nmemb, void *stream);

/* Functions */
int 		juhe_get_errorCode(const char *strinput);
int 		juhe_get_totalItemNum(const char *strinput);
char* 		juhe_dupGet_elemValue(const char *strinput, int index, const char *strkey);
EGI_FILO* 	juhe_get_newsFilo(const char* url);
void  		print_json_object(const json_object *json);
int 		juhe_save_charBuff(const char *fpath, const char *buff);
int 		juhe_fill_charBuff(const char *fpath, char *buff, int size);

#endif
