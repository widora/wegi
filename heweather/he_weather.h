/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example for www.heweahter.com https interface.

Midas Zhou
-------------------------------------------------------------------*/
#ifndef __HE_WEATHER__
#define __HE_WEATHER__

#include <stdio.h>
#include "egi_image.h"

#define _SKIP_PEER_VERIFICATION
#define _SKIP_HOSTNAME_VERIFICATION

#define HEWEATHER_NOW		0
#define HEWEATHER_FORECAST	1
#define HEWEATHER_HOURLY	2
#define HEWEATHER_LIFESTYLE	3
#define HEWEATHER_FORECAST_DAYS	3

#define HEWEATHER_ICON_PATH	"/mmc/heweather" /* png icons */

enum heweather_data_type {
	data_now=0,
	data_forecast,
	data_hourly,
	data_lifestyle
};

/* Use static mem may be GOOD */
typedef struct  heweather_data {
	char		*location;		/* Location */

	char		*cond_code;	/* current weather condition code */
	char		*cond_code_d;	/* forecast day cond_code */
	char		*cond_code_n;	/* forecast night cond_code */

	EGI_IMGBUF	*eimg; 		/* weather icon image data loaded from icon_path */

	char		*cond_txt;	/* current weather condition txt */
	char		*cond_txt_d;	/* forecast day cond_txt */
	char		*cond_txt_n;	/* forecast night cond_txt */

	char		*wind_dir;	/* wind direction */
	int		wind_deg;	/* wind degree 0-360 */
	char		*wind_scale;	/* wind scale 1-12 */
	int		wind_speed;	/* wind speed in kilometer/hour */

	int		temp;  		/* current temperature */
	int		temp_max;	/* forecast Temp. */
	int		temp_min;

	int		hum;   		/* current humidity */
	int		hum_max;	/* forecast Hum. */
	int		hum_min;
}EGI_WEATHER_DATA;

/***
 *  Now:  0;
 *  Forcast:
 *	 1=today;  2=tomorrow; 3=the day aft. tomorrow
 */
extern EGI_WEATHER_DATA weather_data[1+HEWEATHER_FORECAST_DAYS];

/* function */
char * heweather_get_objitem(const char *strinput, const char *strsect, const char *strkey);
char * heweather_get_forecast(const char *strinput, int index, const char *strkey);
void   heweather_data_clear(EGI_WEATHER_DATA *weather_data);
int heweather_httpget_data(enum heweather_data_type data_type, const char *location);


#endif
