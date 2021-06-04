/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/

#ifndef __EGI_IWINFO__
#define __EGI_IWINFO__


#define IW_TRAFFIC_SAMPLE_SEC 5 /* in second, time for measuring traffic rate, used to get an average value */

int iw_get_rssi(const char *ifname, int *rssi);
int iw_get_speed(int *ws, const char* strifname);
int iw_http_request(char *host, char *request, char *reply);
int iw_read_traffic(const char* strifname, unsigned long long *recv, unsigned long long *trans);
int iw_read_cpuload(float *loadavg);
int iw_get_clients(const char *ifname);
bool iw_is_running(const char *ifname);
int iw_ifup(const char *ifname);
int iw_ifdown(const char *ifname);
void iwpriv(const char *name, const char *key, const char *val);
#endif
