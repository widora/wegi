/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example for www.heweahter.com https interface.

Midas Zhou
-------------------------------------------------------------------*/
#include <stdio.h>
#include <curl/curl.h>
#include <string.h>
#include <json-c/json.h>
#include <json-c/json_object.h>
#include "egi_common.h"
#include "he_weather.h"
#include "egi_FTsymbol.h"

EGI_WEATHER_DATA weather_data[1+HEWEATHER_FORECAST_DAYS]; /* 0=now; forcast: 1=today; 2=tomorrow; 3=the day aft. tomorrow */

static char strrequest_mode[4][256]=
{
	"https://free-api.heweather.net/s6/weather/now?", 		//location=shanghai&key=",
	"https://free-api.heweather.net/s6/weather/forecast?",
	"https://free-api.heweather.net/s6/weather/hourly?",
	"https://free-api.heweather.net/s6/weather/lifestyle?"
};
static char strkey[256];
static char buff[32*1024]; /* for curl return */

#if 0
char *json_type_str[]=
{
     "json_type_null",
     "json_type_boolean",
     "json_type_double",
     "json_type_int",
     "json_type_object",
     "json_type_array",
     "json_type_string"
};
#endif

/*---------------------------------------------------
 	Clear data in EGI_WEATHER_DATA
Free ->eimg and ->cond_code

Note: weather_data is static now!!!
------------------------------------------------------*/
void heweather_data_clear(EGI_WEATHER_DATA *weather_data)
{
	if(weather_data!=NULL) {
		/* free eimg data */
		if(weather_data->eimg !=NULL) {
			 /* clear data only!!! */
			 egi_imgbuf_cleardata(weather_data->eimg);
		}
		/* free location string */
		if(weather_data->location !=NULL) {
			free(weather_data->location);
			weather_data->location=NULL;
		}
		/* free cond_code */
		if(weather_data->cond_code !=NULL) {
			free(weather_data->cond_code);
			weather_data->cond_code=NULL;
		}
		if(weather_data->cond_code_d !=NULL) {
			free(weather_data->cond_code_d);
			weather_data->cond_code_d=NULL;
		}
		if(weather_data->cond_code_n !=NULL) {
			free(weather_data->cond_code_n);
			weather_data->cond_code_n=NULL;
		}
		/* free cond_txt */
		if(weather_data->cond_txt !=NULL) {
			free(weather_data->cond_txt);
			weather_data->cond_txt=NULL;
		}
		if(weather_data->cond_txt_d !=NULL) {
			free(weather_data->cond_txt_d);
			weather_data->cond_txt_d=NULL;
		}
		if(weather_data->cond_txt_n !=NULL) {
			free(weather_data->cond_txt_n);
			weather_data->cond_txt_n=NULL;
		}
		/* free wind data */
		if(weather_data->wind_dir !=NULL) {
			free(weather_data->wind_dir);
			weather_data->wind_dir=NULL;
		}
		if(weather_data->wind_scale !=NULL) {
			free(weather_data->wind_scale);
			weather_data->wind_scale=NULL;
		}


	}
}

/*---------------------------------------------
A callback function to deal with replied data.
----------------------------------------------*/
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	strcat(userp,ptr);
	return size*nmemb;
}


/*-----------------------------------------------------------------------------------------------
Parse HeWeather string and return string pointer to value of specified strkey, or to the value of
specified section if strkey is NULL.

Note:
	1. Strkey available only for non_array Sections. ---"basic","update","status","now"
	   However, strkey in array object "daily_forcast" CAN NOT be extracted by this function.
	   call heweather_get_arrayitem() instead.
	2. !!! Don't forget to free the returned string pointer !!!

@strinput	input HeWeather string
@strsect	section name of requested string. Example: "basic","update","now","forcast"
@strkey	        key name of the item in section. Example: "cond_code","tmp",...
		Note: if strkey==NULL, then return string pointer to section objet.

Return:
	0	ok
	<0	fails
----------------------------------------------------------------------------------------------*/
char* heweather_get_objitem(const char *strinput, const char *strsect, const char *strkey)
{
	char *pt;

	json_object *json_input=NULL;
	json_object *json_array=NULL;
	json_object *json_data=NULL;
        json_object *json_sect=NULL;
	json_object *json_get=NULL;

	/* parse returned string */
	json_input=json_tokener_parse(strinput);
	if(json_input==NULL) return NULL;

	/* strip to get section obj */
	json_object_object_get_ex(json_input,"HeWeather6",&json_array);
	if(json_array==NULL)return NULL;
	json_data=json_object_array_get_idx(json_array,0);
	if(json_data==NULL)return NULL;
	json_object_object_get_ex(json_data, strsect, &json_sect); /* get strsect object */
	if(json_sect==NULL)return NULL;

	/* if strkey, get key obj */
	if(strkey!=NULL) {
		json_object_object_get_ex(json_sect, strkey, &json_get);
		if(json_get==NULL)  return NULL;
	} else {
		json_get=json_sect;
	}

	/* Get pointer to the item string */
	pt=strdup((char *)json_object_get_string(json_get));

	/* free input object */
	json_object_put(json_input);


	return pt;
}


/*---------------------------------------------------------------------------------------------------
Parse HeWeather string and return string pointer to value of specified strkey in forecast array[index]
Note:
	1. !!! Don't forget to free the returned string pointer !!!

@strinput	input HeWeather string
@index		index of forecast json array object, for 3days, 0-2
@strkey	        key name of the item in forecast array, Example:'cond_code_d','cond_code_n','data','hum'

Return:
	0	ok
	<0	fails
--------------------------------------------------------------------------------------------------*/
char* heweather_get_forecast(const char *strinput, int index, const char *strkey)
{
	char *pt;

	json_object *json_input=NULL;
	json_object *json_array=NULL;
	json_object *json_data=NULL;
        json_object *json_sect=NULL;
	json_object *json_get=NULL;

	/* set forcast index limit, 3 days */
	if(index<0 || index>2)
		return NULL;

	/* parse returned string */
	json_input=json_tokener_parse(strinput);
	if(json_input==NULL) return NULL;

	/* strip to get section obj */
	json_object_object_get_ex(json_input,"HeWeather6",&json_array);
	if(json_array==NULL)return NULL;
	json_data=json_object_array_get_idx(json_array,0);
	if(json_data==NULL)return NULL;
	json_object_object_get_ex(json_data, "daily_forecast", &json_sect); /* get strsect object */
	if(json_sect==NULL)return NULL;
	json_array=json_object_array_get_idx(json_sect,index);

	/* if strkey, get key obj in array[index] */
	if(strkey!=NULL) {
		json_object_object_get_ex(json_array, strkey, &json_get);
		if(json_get==NULL)  return NULL;
	} else {
		return NULL;
	}

	/* Get pointer to the item string */
	pt=strdup((char *)json_object_get_string(json_get));

	/* free input object */
	json_object_put(json_input);

	return pt;
}



/*--------------------------------------------------------------------
Get weather data from HeWeather.com

Note:
1. If hwdata[]->eimg is NULL, then egi_imgbuf_alloc() will be called to allocatge
   it.
2. If hwdata[]->eimg is NOT null, then egi_imgbuf_cleardata() will be called
   to empty its old data before put new data into it.
3. Do NOT forget to free

@data_type:	weather data type to be requested
@location:	location in string

Return:
	0	Ok
	<0	Fails
----------------------------------------------------------------------*/
int heweather_httpget_data(enum heweather_data_type data_type, const char *location)
{
  	int i=0;
	char *pstr=NULL;
	char strrequest[256+64]={0};
	char strpath[256];
	char strtemp[256];
	int  temp;
	char strhum[16];
	int  hum;
	char *cond_txt;

	/* read key from EGI config file */
	egi_get_config_value("EGI_WEATHER", "key", strkey);
	strcat(strrequest,strrequest_mode[data_type]);
	strcat(strrequest,"location=");
	strcat(strrequest,location);
	strcat(strrequest,"&key=");
	strcat(strrequest,strkey);
//	sprintf(strrequest,"%s%s", strrequest_mode[data_type], strkey);
	printf("strrequest:%s\n", strrequest);

	/* Get request */
	memset(buff,0,sizeof(buff));
	if(https_curl_request(strrequest, buff, NULL, curlget_callback)!=0) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to call https_curl_request()!\n",__func__);
		return -1;
	}
	printf("%s: get curl reply: %s\n",__func__, buff);

	/* Extract section item 'status' string */
	pstr=heweather_get_objitem(buff,"status",NULL);
	if(pstr) {
		printf("status: %s\n",pstr);
		free(pstr); pstr=NULL;
	}

/*---------   (  PARSE Request_NOW  )   ------------
{"HeWeather6":[ {"basic":{"cid":"CN101020100","location":"上海","parent_city":"上海","admin_area":"上海",
                  	    "cnty":"中国","lat":"31.23170662","lon":"121.47264099","tz":"+8.00"},
                 "update":{"loc":"2019-06-03 15:09","utc":"2019-06-03 07:09"},
  		 "status":"ok",
                 "now":{"cloud":"0","cond_code":"101","cond_txt":"多云","fl":"31","hum":"48","pcpn":"0.0",
			 "pres":"1005", "tmp":"29","vis":"16","wind_deg":"1","wind_dir":"北风","wind_sc":"0",
		         "wind_spd":"1"}
  	        }
	      ]
*/
  if(data_type==data_now)
  {
	/* Always do! empty old data if any, some strdup() data will be cleared  */
	heweather_data_clear(&weather_data[0]);

	/* put location */
	pstr=heweather_get_objitem(buff, "basic","location");
	if(pstr != NULL) {
		weather_data[0].location=pstr;
		pstr=NULL;
	}

	/* Extract key item 'cond_code' in section 'now' */
	pstr=heweather_get_objitem(buff, "now","cond_code");
	if(pstr!=NULL) {
		weather_data[0].cond_code=pstr;
		sprintf(strpath,"%s/%s.png",HEWEATHER_ICON_PATH,pstr);
		pstr=NULL;
	}

	/* Extract key item 'cond_txt' in section 'now' */
	pstr=heweather_get_objitem(buff, "now","cond_txt");
	if(pstr !=NULL) {
		weather_data[0].cond_txt=pstr;
		pstr=NULL;
	}

	/* Extract key item 'wind_dir' in section 'now' */
	pstr=heweather_get_objitem(buff, "now","wind_dir");
	if(pstr !=NULL) {
		weather_data[0].wind_dir=pstr;
		pstr=NULL;
	}

	/* Extract key item 'wind_sc' in section 'now' */
	pstr=heweather_get_objitem(buff, "now","wind_sc");
	if(pstr !=NULL) {
		weather_data[0].wind_scale=pstr;
		pstr=NULL;
	}

	/* Extract key item 'tmp' in section 'now' */
	pstr=heweather_get_objitem(buff,"now", "tmp");
	if(pstr!=NULL) {
		weather_data[0].temp=atoi(pstr);
		free(pstr); pstr=NULL;
	}

	/* Extract key item 'hum' in section 'now' */
	pstr=heweather_get_objitem(buff, "now", "hum");
	if(pstr!=NULL) {
		weather_data[0].hum=atoi(pstr);
		free(pstr); pstr=NULL;
	}

	/* create a new EGI_IMGBUF if NULL */
	if(weather_data[0].eimg==NULL) {
		weather_data[0].eimg=egi_imgbuf_alloc();
		if(weather_data[0].eimg==NULL) {
			printf("%s: fail to call egi_imgbuf_alloc()!\n",__func__);
			return -2;
		}
	}

	/* load png file acoordingly */
   	if( egi_imgbuf_loadpng(strpath, weather_data[0].eimg ) !=0 ) {   /* mutex inside, imgbuf renewed */
		printf("%s: Fail to loadpng %s!\n", __func__, strpath);
		return -3;
   	}
 	printf("icon path: %s\n",weather_data[0].cond_code);
	printf("cond_txt: %s\n",weather_data[0].cond_txt);
	printf("温度%d 湿度%d\n",weather_data[0].temp, weather_data[0].hum);
        printf("%s: ---OK---!\n", __func__);

   }

/*---------   (  PARSE Request_FORECAST  )   ------------

{"HeWeather6":[ { "basic": {"cid":"CN101020100","location":"上海","parent_city":"上海","admin_area":"上海",
			      "cnty":"中国","lat":"31.23170662","lon":"121.47264099","tz":"+8.00"},
		   "update":{"loc":"2019-06-07 16:57","utc":"2019-06-07 08:57"},
		   "status":"ok",
		   "daily_forecast":[ { "cond_code_d":"101","cond_code_n":"101","cond_txt_d":"多云","cond_txt_n":"多云",
					"date":"2019-06-07","hum":"70","mr":"08:23","ms":"22:37","pcpn":"5.0","pop":"79","pres":"1008","sr":"04:49","ss":"18:56","tmp_max":"28","tmp_min":"20","uv_index":"3","vis":"25","wind_deg":"359","wind_dir":"北风","wind_sc":"1-2","wind_spd":"3"},
				      { "cond_code_d":"101","cond_code_n":"101","cond_txt_d":"多云","cond_txt_n":"多云",
				        "date":"2019-06-08","hum":"76","mr":"09:30","ms":"23:24","pcpn":"0.0","pop":"24","pres":"1003","sr":"04:49","ss":"18:57","tmp_max":"29","tmp_min":"21","uv_index":"3","vis":"25","wind_deg":"129","wind_dir":"东南风","wind_sc":"3-4","wind_spd":"24"},
				      { "cond_code_d":"101","cond_code_n":"101","cond_txt_d":"多云","cond_txt_n":"多云",
					"date":"2019-06-09","hum":"87","mr":"10:36","ms":"00:00","pcpn":"0.0","pop":"1","pres":"1001","sr":"04:49","ss":"18:57","tmp_max":"30","tmp_min":"21","uv_index":"6","vis":"24","wind_deg":"146","wind_dir":"东南风","wind_sc":"3-4","wind_spd":"13"}
				    ]
                  }
                ]
  }
*/

  else if(data_type==data_forecast)
  {
	printf("---Daily_forecast--- \n");
	/* forecast today, tommorrow, the day aft. tomorrow */
	for(i=1;i<4;i++) {

        	/* Always do! empty old data if any, some strdup() data will be cleared  */
        	heweather_data_clear(&weather_data[i]);

		/* Get location */
		pstr=heweather_get_objitem(buff, "basic","location");
        	if(pstr != NULL) {
                	weather_data[i].location=pstr;
			pstr=NULL;
		}
		printf("location: %s\n", weather_data[i].location);

		/* Extract key item 'cond_code_d' as for cond_code */
		pstr=heweather_get_forecast(buff,i-1,"cond_code_d");
		if(pstr!=NULL) {
			weather_data[i].cond_code=pstr; /* !!! to free by heweather_data_clear() later */
			sprintf(strpath,"%s/%s.png",HEWEATHER_ICON_PATH,pstr);
			pstr=NULL;
		}

		/* Extract key item 'cond_txt_d' as for cond_txt */
		memset(strtemp,0,sizeof(strtemp));
		pstr=heweather_get_forecast(buff,i-1,"cond_txt_d");
		if(pstr!=NULL)	{
			strcat(strtemp,pstr);
			free(pstr); pstr=NULL;
		}
		pstr=heweather_get_forecast(buff,i-1,"cond_txt_n");
		if(pstr!=NULL)	{
			strcat(strtemp,"/");
			strcat(strtemp,pstr);
			free(pstr); pstr=NULL;
		}
		weather_data[i].cond_txt=strdup(strtemp);

		/* Extract key item 'wind_dir' in section 'now' */
		pstr=heweather_get_forecast(buff,i-1,"wind_dir");
		if(pstr!=NULL) {
			weather_data[i].wind_dir=pstr;
			pstr=NULL;
		}

		/* Extract key item 'wind_sc' in section 'now' */
		pstr=heweather_get_forecast(buff,i-1,"wind_sc");
		if(pstr !=NULL) {
			weather_data[i].wind_scale=pstr;
		}

		/* Extract key item for temperature */
		pstr=heweather_get_forecast(buff,i-1,"tmp_max");
		if(pstr!=NULL) {
			weather_data[i].temp_max=atoi(pstr);
			free(pstr); pstr=NULL;
		}
		pstr=heweather_get_forecast(buff,i-1,"tmp_min");
		if(pstr!=NULL) {
			weather_data[i].temp_min=atoi(pstr);
			free(pstr); pstr=NULL;
		}

		/* Extract key item for humidity */
		pstr=heweather_get_forecast(buff,i-1,"hum");
		if(pstr!=NULL) {
			weather_data[i].hum=atoi(pstr);
			free(pstr); pstr=NULL;
		}


	        /* create a new EGI_IMGBUF if NULL */
        	if(weather_data[i].eimg==NULL) {
                	weather_data[i].eimg=egi_imgbuf_alloc();
	                if(weather_data[i].eimg==NULL) {
        	                printf("%s: fail to call egi_imgbuf_alloc()!\n",__func__);
                	        return -2;
                	}
        	}

	        /* load png file acoordingly */
        	if( egi_imgbuf_loadpng(strpath, weather_data[i].eimg ) !=0 ) {   /* mutex inside, imgbuf renewed */
                	printf("%s: Fail to loadpng %s!\n", __func__, strpath);
                	return -3;
        	}
	        printf("icon path: %s\n", strpath);
        	printf("cond_txt: %s\n",weather_data[i].cond_txt);
	        printf("温度 %d/%dC, 湿度 %d%% \n",weather_data[i].temp_max,
							weather_data[i].temp_min, weather_data[i].hum);
        	printf("%s: ---OK---!\n", __func__);


	}
	printf("\n");


  }


/*---------   (  PARSE Request_Hourly  )   ------------

*/

  return 0;
}
