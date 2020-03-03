/*--------------------------  iot_client.c  ---------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


1. A simple example to connect BIGIOT and control a bulb icon.
2. IoT talk syntax is according to BigIot Protocol: www.bigiot.net/help/1.html

3. More: iot_client works as an immediate layer between IoT server and EGI modules.
	3.1 One thread for sending and one for receiving.
	3.2 A recv_buff[] and a send_buff[].
	3.3 A parse thread, pull out recv_buff[], unwrap and forward to other modules.
	3.4 A wrap thread, wrap other modules's data to json string and push to send_buff[].
	what's more....

4. For one socket, there shall be only one recv_thread and one send_thread running at the same time.

5. WARNING!!! Consider to balance between egi_sleep(), tm_delayms() and egi_log();

			------	(( Glossary ))  ------

MTU:	    A maximum transmission unit(MTU) is the largest packet or frame size,IP uses MTU to determine
   	    the maximum size of each paket in the transmission. Max. 1500-bytes for internet.
	    A compelet packet:  Capsule(head and tail)+IP Header(20bytes)+TCP Header(20bytes)+Payload( )=Max.1500 bytes.

TCP MSS:    Maximum Segment Size is the payload of a TCP packet.
	    A complete TCP packet:  TCP Header(20bytes) + TCP MSS=Max.1480 bytes.
	    < ***** > IPv4 is required to handle Min MSS of 536 bytes, while IPv6 is 1220 bytes.


TODO:
1. Calling recv() may return several IoT commands from socket buffer at one time, especailly in heavy load condition.
   so need to separate them.
   recv() may return serveral sessions of BIGIOT command at one time, and fill log buffer full(254+1+1):
    //////  and cause log buff overflow !!!! \\\\\\\\
   [2019-03-12 10:21:52] [LOGLV_INFO] Message from the server: {"M":"say","ID":"Pc0a809a00a2900000b2b","NAME":"guest","C":"down","T":"1552357312"}
{"M":"say","ID":"Pc0a809a00a2900000b2b","NAME":"guest","C":"down","T":"1552357312"}
{"M":"say","ID":"Pc0a809a0[2019-03-12 10:22:03] [LOGLV_INFO] Message from the server: {"M":"say","ID":"Pc0a809a00a2900000b2b","NAME":"guest","C":"down","T":"1552357323"}
    ///// put recv() string to a buff......///
2. Thread iot_update_data() may exit sometime.
3. Check integrity of received message.
4. Set TIMEOUT for recv() and send()
5. Use select

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>
#include <malloc.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <json.h>
#include <json_object.h>
#include <printbuf.h>
#include <ctype.h> /* isdigit */
#include <sys/resource.h>
#include "egi_iotclient.h"
#include "egi.h"
#include "egi_debug.h"
#include "egi_log.h"
#include "egi_page.h"
#include "egi_color.h"
#include "egi_timer.h"
#include "egi_cstring.h"
#include "egi_iwinfo.h"

/* IOT data interface ID */
#define IOT_DATAINF_LOAD	546 /* CPU load */
#define IOT_DATAINF_WSPEED	961 /* network traffic, income */
#define IOT_DATAINF_VMSIZE	430 /* vm size of self */

#define IOT_HEARTBEAT_INTERVAL	30	/*in second, heart beat interval, Min 10s for status inquiry */
#define IOT_RECONNECT_INTERVAL	15	/*in second, reconnect interval, for status inquiry, Min. 10s  */

#define BUFFSIZE 		2048
#define BULB_OFF_COLOR 		0x000000 	/* default color for bulb turn_off */
#define BULB_ON_COLOR 		egi_color_random(medium) //0xDDDDDD   	/* default color for bulb turn_on */
#define BULB_COLOR_STEP 	0x111111	/* step for turn up and turn down */
#define SUBCOLOR_UP_LIMIT	0xFFFFFF 	/* up limit value for bulb light color */
#define SUBCOLOR_DOWN_LIMIT	0x666666 	/* down limit value for bulb light color */


//#define IOT_HOME_BULB

static int sockfd;
static struct sockaddr_in svr_addr;
static char recv_buf[BUFFSIZE]={0}; /* for recv() buff */

static EGI_16BIT_COLOR bulb_color;
static EGI_16BIT_COLOR subcolor;

static bool bulb_off=false; /* default bulb status */
static const char *bulb_status[2]={"ON","OFF"};
static bool keep_heart_beat __attribute__((__unused__)) =false; /* token for keepalive thread */

/*
  1. After receive command from the server, push it to incmd[] and confirm server for the receipt.
  2. After incmd[] executed, set flag incmd_executed to be true.
  3. Reply server to confirm result of execution.
*/
struct egi_iotdata
{
	//iot_status status;
	bool	connected;
	char 	incmd[64];
	bool	incmd_executed; /* set to false once after renew incmd[] */
};


/* json string templates as per BIGIOT protocol */
#define TEMPLATE_MARGIN		100 /* more bytes that may be added into the following json templates strings */

static char server_ip[32];
static int  server_port;
static char device_id[64];  /*{0}, device_id as string */
static char device_key[64]; /*{0},  device_key as string */

const static char *strjson_checkin_template=
			"{\"M\":\"checkin\"}";
const static char *strjson_keepalive_template __attribute__((__unused__))=
			"{\"M\":\"beat\"}\n";
const static char *strjson_reply_template=
			"{\"M\":\"say\",\"C\":\"Hello from Widora_NEO\",\"SIGN\":\"Widora_NEO\"}";//\n";
const static char *strjson_check_online_template __attribute__((__unused__))=
			"{\"M\":\"isOL\",\"ID\":[\"xx1\"...]}\n";
const static char *strjson_check_status_template=
			"{\"M\":\"status\"}\n"; /* return key M value:'connected' or 'checked' */
const static char *strjson_update_template=
			"{\"M\":\"update\"}"; /* to insert: "ID":id and \"V\":{\"id1\":\"value1\"}}\n"; */


/* Functions declaration */
const static char *iot_getkey_pstrval(json_object *json, const char* key);
static json_object * iot_new_datajson(const int *id, const double *value, int num);
static void iot_keepalive(void);
static int iot_connect_checkin(void);
static inline int iot_send(int *send_ret, const char *strmsg);
static inline int iot_recv(int *recv_ret);
static inline int iot_status(void);


/*-----------------------------------------------------------------------
Get a pionter to a !!!!string type key value from an input json object.

json:	a json object.
key:	key name of a json object

return:
	a pointer to string	OK
	NULL			Fail
------------------------------------------------------------------------*/
const static char *iot_getkey_pstrval(json_object *json, const char* key)
{
	json_object *json_item=NULL;

	/* 1. check input param */
	if( json==NULL || key==NULL) {
		EGI_PDEBUG(DBG_IOT,"input params invalid!\n");
		return NULL;
	}

	/* 2. get item object */
	if( json_object_object_get_ex(json, key, &json_item)==false ) {/* return pointer */
		//EGI_PDEBUG(DBG_IOT,"Fail to get value of key '%s' from the input json object.\n",key);
		return NULL;
	}
	else {
		//EGI_PDEBUG(DBG_IOT,"object key '%s' with value '%s'\n",
		//					key, json_object_get_string(json_item));
	}
	/* 3. check json object type */
	if(!json_object_is_type(json_item, json_type_string)) {
		EGI_PDEBUG(DBG_IOT,"Error, key '%s' json obj is NOT a string type object!\n",key );
		return NULL;
	}

	/* 4. get a pointer to string value */
 	return json_object_get_string(json_item); /* return a pointer */
}

/*-----------------------------------------------------------------------------
Assembly a cjson object by adding groups of keys(IDs) and Values(string type),
result json obj is like {"id1":"value1","id2":"value2",.... }.

Note: call json_object_put() to free it after use.

Retrun:
	NULL 	fails
--------------------------------------------------------------------------------*/
static json_object * iot_new_datajson(const int *id, const double *value, int num)
{
	int k;
	/* buff for ID string:
         * for int32, its Max decimal value has 11 digits, and int64 has 19 digits
         */
	char strid[32];
	char strval[32];

	/* 1. get a new json_object_type obj */
	json_object *json=json_object_new_object();
	if(json==NULL)
		return NULL;

	/* 2. insert Keys and Values into the json object */
	for(k=0; k<num; k++) {
		memset(strid,0,32);
		sprintf(strid,"%d",id[k]);
		memset(strval,0,32);
		sprintf(strval,"%3.2f",value[k]);
		json_object_object_add(json, strid, json_object_new_string(strval));
	}
	//EGI_PDEBUG(DBG_IOT,"%s: json created: %s\n",__func__,json_object_to_json_string(json));

	return json;
}



/*--------------------  A thread function   --------------------------

1. Send status inquiry string to keep connection with server.

Note:
   The function will not receive server replay message, just leave it
   to other threads to confirm.


Interval Min. 10 seconds as per protocol

-------------------------------------------------------------------*/
static void iot_keepalive(void)
{
	int ret;
	time_t t;
	struct tm *tm;

	while(1)
	{
		/* idle time */
		//tm_delayms(IOT_HEARTBEAT_INTERVAL*1000);
//		EGI_PLOG(LOGLV_WARN,"---------- start egi_sleep %d [%lld] ------------\n",
//							IOT_HEARTBEAT_INTERVAL, tm_get_tmstampms()/1000 );
		egi_sleep(0,IOT_HEARTBEAT_INTERVAL,0);
//		EGI_PLOG(LOGLV_WARN,"---------- end egi_sleep() [%lld] ----------\n", tm_get_tmstampms()/1000 );

		/* check iot network status */
		ret=iot_send(&ret, strjson_check_status_template);
		if(ret==0) {
			//EGI_PDEBUG(DBG_IOT,"heart_beat msg is sent out.\n");
		        /* get time stamp */
        		t=time(NULL);
        		tm=localtime(&t);
        		/* time stamp and msg */
        		EGI_PLOG(LOGLV_INFO,"[%d-%02d-%02d %02d:%02d:%02d] heart_beat msg is sent out.\n",
                                tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour, tm->tm_min,tm->tm_sec );
		}
		else
			EGI_PDEBUG(DBG_IOT,"Fail to send out heart_beat msg.\n");
	}
}


/*--------------  A Thread Function  ------------------
Update BIGIOT interface data by sending json string

Return:
	0	OK
	<0	fails
---------------------------------------------------------------------*/
static void iot_update_data(void)
{
	/* ID and DATA for BIGIOT data interface */
	int id[3]={ IOT_DATAINF_LOAD, IOT_DATAINF_WSPEED, IOT_DATAINF_VMSIZE};	/* interface ID */
	double data[3];
	struct rusage	r_usage;
	long maxrss;
	int ws;	/* wifi speed bytes/s */
	char sendbuff[BUFFSIZE]={0}; /* for recv() buff */
	int ret=0;
	/* open /porc/loadavg to get load value */
        double load=0.0;
        int fd;
        char strload[5]={0}; /* read in 4 byte */

	json_object *json_data;
	json_object *json_update;

	/* prepare update template */
	/* !!!!!WARNING: for an object_type json object containing another object_type json object,
	 * To call json_object_object_add() 2nd time results in segmentation fault!
	 * You need to put it and then reinitialize it.
	 */
#if 0
	json_object *json_update=json_tokener_parse(strjson_update_template);
	if(json_update == NULL) {
		EGI_PLOG(LOGLV_ERROR,"egi_iotclient: fail to prepare json_update.\n");
		pthread_exit(0);
	}
#endif

	while(1)
	{
	        /* 1. open proc file and read avgload */
       		fd=open("/proc/loadavg", O_RDONLY|O_CLOEXEC);
	        if(fd<0) {
        	        EGI_PLOG(LOGLV_ERROR,"%s: fail to open /proc/loadavg!\n",__func__);
			egi_sleep(0,1,0);
               		continue;
        	}
                lseek(fd,0,SEEK_SET);
                read(fd,strload,4);
                load=atof(strload);/* for symmic_cpuload[], index from 0 to 5 */

		/* 2. get wifi actual speed bytes/s */
		EGI_PLOG(LOGLV_INFO,"---------  trap into iw_get_speed( )  -------- >>>>> \n");
		iw_get_speed(&ws);
		EGI_PLOG(LOGLV_INFO,"---------  get out of iw_get_speed( )  ---------- <<<<< \n");

		/* 3. get VM size */
		getrusage(RUSAGE_SELF,&r_usage);
                maxrss=r_usage.ru_maxrss;
	       //long   ru_maxrss;        /* maximum resident set size */
               //long   ru_ixrss;         /* integral shared memory size */
               //long   ru_idrss;         /* integral unshared data size */
               //long   ru_isrss;         /* integral unshared stack size */
		EGI_PLOG(LOGLV_INFO,"xxxxxxxxxx   maxrss=%ld, ixrss=%ld, idrss=%ld, isrss=%ld    xxxxxxxxx\n",
  				    r_usage.ru_maxrss,r_usage.ru_ixrss, r_usage.ru_idrss, r_usage.ru_isrss);

		/* 3. prepare data json */
		data[0]=load;
		data[1]=ws; /* get average speed value */
		data[2]=(double)maxrss;
		EGI_PLOG(LOGLV_INFO,"raw data for json_data: load=%lf, ws=%lf, maxrss=%lf. \n",
										data[0],data[1],data[2] );

		/* 4. create data json */
 		json_data=iot_new_datajson( (const int *)id, data, 3);
		if(json_data==NULL) {
		    EGI_PLOG(LOGLV_ERROR,"iot_new_datajson() fail to create data json! retry...\n");
		    egi_sleep(0,0,500);
		    continue;
		}

		/* 5. prepare template json !!!! object_object, re_update !!!! */
		json_update=json_tokener_parse(strjson_update_template);
		json_object_object_add(json_update,"ID",json_object_new_string(device_id));

		/* 6. insert data json to update template json */
		json_object_object_del(json_update,"V");
		json_object_object_add(json_update,"V",json_data);
		EGI_PLOG(LOGLV_CRITICAL,"Finish creating update_json: \n%s\n",
					json_object_to_json_string_ext(json_update,
					JSON_C_TO_STRING_PRETTY)); /* ret pointer only */
		/* add tail '\n' */
		memset(sendbuff,0,sizeof(sendbuff));
		sprintf(sendbuff,"%s\n",json_object_to_json_string(json_update));

		/* 7. send to iot */
		if (iot_send(&ret, sendbuff) != 0)
			EGI_PLOG(LOGLV_WARN,"%s: fail to iot_send() updata message, ret=%d.\n",
											__func__, ret);
		/* 8. release json_data */
		if(json_object_put(json_data) != 1)
			EGI_PLOG(LOGLV_ERROR,"Fail to json_object_put() json_data!\n");
		if(json_object_put(json_update) != 1)
			EGI_PLOG(LOGLV_ERROR,"Fail to json_object_put() json_update!\n");

		/* 9. delay and refresh value */
		close(fd);
		//tm_delayms(6000);
		egi_sleep(0, ( 6>IW_TRAFFIC_SAMPLE_SEC ? (6-IW_TRAFFIC_SAMPLE_SEC):1 ), 0);
	}

}


/*--------------------------------------------------------
1. Close and set up socket FD
2. Connect to IoT server
3. Check in.

NOTE: Function loops trying to check into BigIot untill it
      succeeds.

Return:
	0	OK
	<O	fails
---------------------------------------------------------*/
static int iot_connect_checkin(void)
{
   int ret;
   char pval[32]={0};
//   char psvr[64]={0}; /* server ip */
//   int port=0;	      /* server port */
   //char device_id[64]={0};  /* device_id as string */
   //char pkey[64]={0}; /* device_key as string */

   json_object *json_checkin;
   int len;
   const char *pstr;
   char *strjson_checkin=NULL;

   /* get server addr and port  from config file */
   memset(device_id,0,sizeof(server_ip));
   if ( egi_get_config_value("IOT_CLIENT","server_ip",server_ip) != 0)
	return -1;
   if ( egi_get_config_value("IOT_CLIENT","server_port",pval) != 0)
	return -1;
   server_port=atoi(pval);

   /* get BIGIOT id and key from config file */
   memset(device_id,0,sizeof(device_id));
   memset(device_key,0,sizeof(device_key));
   if ( egi_get_config_value("IOT_CLIENT", "device_id", device_id) !=0 )
	return -1;

   if ( egi_get_config_value("IOT_CLIENT", "device_key", device_key) !=0 )
	return -1;

   /* create checkin json */
   if( (json_checkin=json_tokener_parse(strjson_checkin_template))==NULL)
		return -2;

   json_object_object_add(json_checkin,"ID",json_object_new_string(device_id));
   json_object_object_add(json_checkin,"K",json_object_new_string(device_key));
   EGI_PLOG(LOGLV_CRITICAL,"%s:Finish creating json_checkin: \n%s\n", __func__,
                                        json_object_to_json_string_ext(json_checkin,
                                        JSON_C_TO_STRING_PRETTY) );  /* ret pointer only */

    /* creat checkin json string */
    pstr=json_object_to_json_string(json_checkin);
    len=strlen(pstr);
    strjson_checkin=calloc(len+2,1); /* 2, '\n'+\'0' */
    if( strjson_checkin==NULL ) {
	json_object_put(json_checkin);
	return -3;
    }
    sprintf(strjson_checkin,"%s\n",pstr);

   /* loop trying .... */
   for(;;)
   {
	EGI_PLOG(LOGLV_CRITICAL,"------ Start connect and check into ther server ------\n");

	/* 1. create a socket file descriptor */
	if( close(sockfd) != 0 ) { /*try close it first */
		EGI_PLOG(LOGLV_ERROR,"%s :: close(): %s \n",__func__, strerror(errno));
		/* go on whatever */
	}
	sockfd=socket(AF_INET,SOCK_STREAM,0);
	if(sockfd<0) {
		perror("try to create socket file descriptor");
		EGI_PLOG(LOGLV_ERROR,"%s :: socket(): %s \n",__func__,strerror(errno));
		continue;
	}
	/* 2. set IOT server address  */
	svr_addr.sin_family=AF_INET;
	svr_addr.sin_port=htons(server_port);
	svr_addr.sin_addr.s_addr=inet_addr(server_ip);
	bzero(&(svr_addr.sin_zero),8);
   	/* 3. connect to socket. */
	ret=connect(sockfd,(struct sockaddr *)&svr_addr, sizeof(struct sockaddr));
	if(ret<0) {
		EGI_PLOG(LOGLV_ERROR,"%s :: connect(): %s \n",__func__,strerror(errno));
		egi_sleep(0,0,500);
		continue;
	}
	EGI_PLOG(LOGLV_CRITICAL,"%s: Succeed to connect to the BIGIOT socket!\n",__func__);
	/* 4. receive response msg from the server */
	memset(recv_buf,0,sizeof(recv_buf));
	ret=recv(sockfd, recv_buf, BUFFSIZE-1, 0);
	if(ret<=0) {
		EGI_PLOG(LOGLV_ERROR,"%s :: recv(): %s \n",__func__,strerror(errno));
		egi_sleep(0,0,500);
		continue;
	}
	EGI_PLOG(LOGLV_CRITICAL,"Reply from the server: %s\n",recv_buf);

	/* 5. check into BIGIO */
	EGI_PLOG(LOGLV_CRITICAL,"Start sending CheckIn msg to BIGIOT...\n");
	ret=send(sockfd, strjson_checkin, strlen(strjson_checkin), MSG_NOSIGNAL);
	if(ret<=0) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to send login request to BIGIOT:%s\n"
								,__func__,strerror(errno));
		egi_sleep(0,0,500);
		continue;
	}
	else {
		EGI_PLOG(LOGLV_CRITICAL,"Checkin msg has been sent to BIGIOT.\n");
	}
	/* 6. wait for reply from the server */
	memset(recv_buf,0,sizeof(recv_buf));
	ret=recv(sockfd, recv_buf, BUFFSIZE-1, 0);
	if(ret<=0) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to recv() CheckIn confirm msg from BIGIOT, %s\n"
								,__func__,strerror(errno));
		egi_sleep(0,0,500);
		continue;
	}
	/* confirm checkin */
	else if( strstr(recv_buf,"checkinok") == NULL ) {
		EGI_PLOG(LOGLV_ERROR,"Checkin confirm msg is NOT received. retry after INTERVAL seconds...\n");
	        //tm_delayms(IOT_RECONNECT_INTERVAL*1000);
		egi_sleep(0,IOT_RECONNECT_INTERVAL,0);
		continue;
	}
	else {
		EGI_PLOG(LOGLV_CRITICAL,"EGI_IOT: Checkin confirm msg is received.\n");
	}
	EGI_PLOG(LOGLV_CRITICAL,"CheckIn reply from the server: %s\n",recv_buf);

	/* 7. finally break the loop */
	break;

    }/* loop end */

	free(strjson_checkin);
 	json_object_put(json_checkin);
	return 0;
}



/*--------------------------------------------------------------
1. send msg to IoT server by a BLOCKING socket FD.

Param:
	send_ret:  pointer to return value of send() call.
	strmsg:	   pointer to a message string.

Return:
	0	OK.
	<0	send() fails.
--------------------------------------------------------------*/
static inline int iot_send(int *send_ret, const char *strmsg)
{
	int ret;

	/* check strmsg */
	if( strlen(strmsg)==0 || strmsg==NULL )
		return -1;
	/* send by a blocking socke FD, */
	ret=send(sockfd, strmsg, strlen(strmsg), MSG_CONFIRM|MSG_NOSIGNAL);
	/* pass ret to the caller */
	if(send_ret != NULL)
		*send_ret=ret;
	/* check result */
	if(ret<0) {
		EGI_PLOG(LOGLV_ERROR,"iot_send: Call send() error, %s\n", strerror(errno));
		return -2;
	}
	else
		EGI_PDEBUG(DBG_IOT,"Succeed to send msg to the server.\n");

	return 0;
}

/*-----------------------------------------------------------------
1. Receive msg from IoT server by a BLOCKING socket FD.
2. Check data integrity then by verifying its head and end token.
3. Received string are stored in recv_buf[].

Param:
	recv_ret:  pointer to return value of recv() call.

Return:
	0	OK
	<0	recv() fails
	>0	invalid IoT message received
------------------------------------------------------------------*/
static inline int iot_recv(int *recv_ret)
{
	int ret;
	int len;

        /* clear buff */
        memset(recv_buf,0,sizeof(recv_buf));

	/* receive with BLOCKING socket */
        ret=recv(sockfd, recv_buf, BUFFSIZE-1, 0);

	/* pass ret */
	if(recv_ret != NULL)
		*recv_ret=ret;

	/* return error */
        if(ret<=0)
                return -1;

	/* check integrity :: by head and end token */
	len=strlen(recv_buf);
	if( ( recv_buf[0] != '{' ) || (recv_buf[len-1] != '\n') )
	{
		EGI_PLOG(LOGLV_ERROR,"%s: ********* Invalid BigIoT message received: %s ******** \n",__func__,recv_buf);
		return 1;
	}

	return 0;
}

/*-------------------------------------------------------
Check IoT network status

Return:
	2	received data invalid
	1	Connected, but not checkin.
	0	OK, checkin.
	-1	Fail to call send()
	-2	Fail to call recv()
-------------------------------------------------------*/
static inline int iot_status(void)
{
	int ret;

	if( iot_send(&ret, strjson_check_status_template) !=0 )
		return -1;

	if( iot_recv(&ret) !=0 )
		return -2;

	if(strcmp(recv_buf,"checked")==0)
		return 0;

	else if(strcmp(recv_buf,"connected")==0)
		return 1;

	else  /*received invalid data */
		return 2;
}


/*----------------  Page Runner Function  ---------------------
Note: runner's host page may exit, so check *page to get status
BIGIOT client
--------------------------------------------------------------*/
void egi_iotclient(EGI_PAGE *page)
{
	int ret;
	int jsret;
	int k=0;

	json_object *json=NULL; /* sock received json */
	json_object *json_reply=NULL; /* json for reply */
	//json_object *json_item=NULL;

	struct printbuf*   pjbuf=printbuf_new(); /* for json string */

	char  keyC_buf[128]={0}; /* for key 'C' reply string*/

	char *strreply=NULL;
	const char *pstrIDval=NULL; /* key ID string pointer */
	const char *pstrCval=NULL; /* key C string pointer*/
	const char *pstrtmp=NULL;

	pthread_t  	pthd_keepalive;
	pthread_t	pthd_update; /* update BIGIOT interface data */


#ifdef IOT_HOME_BULB

 	EGI_PDEBUG(DBG_PAGE,"page '%s': runner thread egi_iotclient() is activated!.\n"
                                                                                ,page->ebox->tag);
	/* magic operation */
//	mallopt(M_MMAP_THRESHOLD,0);

	/* get related ebox form the page, id number for the IoT button */
	EGI_EBOX *iotbtn=egi_page_pickebox(page, type_btn, 7);
	if(iotbtn == NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to pick the IoT button in page '%s'\n.",
								__func__,  page->ebox->tag);
		return;
	}
	bulb_color=egi_color_random(medium);
//	egi_btnbox_setsubcolor(iotbtn, COLOR_24TO16BITS(BULB_ON_COLOR)); /* set subcolor */
	egi_btnbox_setsubcolor(iotbtn, bulb_color); /* set subcolor */
//	subcolor=BULB_ON_COLOR;
	egi_ebox_needrefresh(iotbtn);
	//egi_page_flag_needrefresh(page); /* !!!SLOW!!!, put flag, let page routine to do the refresh job */
	egi_ebox_refresh(iotbtn);
#endif

	/* 	1. prepare jsons with template string */
	json_reply=json_tokener_parse(strjson_reply_template);
	if(json_reply == NULL) {
		EGI_PLOG(LOGLV_ERROR,"egi_iotclient: fail to prepare json_reply.\n");
		return;
	}

	/* 	2. Set up socket, connect and check into BigIot.net */
	if( iot_connect_checkin() !=0 )
		return;

#if 1
	/* 	3.1 Launch keepalive thread, BIGIOT */
	if( pthread_create(&pthd_keepalive, NULL, (void *)iot_keepalive, NULL) !=0 ) {
                EGI_PLOG(LOGLV_ERROR,"Fail to create iot_keepalive thread!\n");
                goto fail;
        }
	else
		EGI_PDEBUG(DBG_IOT,"Create bigiot_keepavlie thread successfully!\n");

	/* 	3.2 Launch update_data thread, BIGIOT */
	if( pthread_create(&pthd_update, NULL, (void *)iot_update_data, NULL) !=0 ) {
                EGI_PLOG(LOGLV_ERROR,"Fail to create iot_update_data thread!\n");
                goto fail;
        }
	else
		EGI_PDEBUG(DBG_IOT,"Create iot_update_data thread successfully!\n");
#endif


	/* 	4. ------------  IoT Talk Loop:  loop recv and send processing  ---------- */
	while(1)
	{
		if( iot_recv(&ret)==0 )
		{
			EGI_PLOG(LOGLV_INFO,"Message from the server: %s\n",recv_buf);

			/* 4.1 parse received string for json */
			json=json_tokener_parse(recv_buf);/* _parse() creates a new json obj */
			if(!json) {
				EGI_PLOG(LOGLV_WARN,"egi_iotclient: Fail to parse received string by json_tokener_parse()!\n");
				continue;
			}
			else /* extract key items and values */
			{
				//EGI_PDEBUG(DBG_IOT,"Message from IOT server: %s\n",
				//	json_object_to_json_string_ext(json,JSON_C_TO_STRING_PRETTY));

/* ---- key 'ID': needed to identify the Visitor ---- */
				/* 4.2 get a key value from a string object, ret a pointer only. */
				pstrIDval=iot_getkey_pstrval(json, "ID");
				/* Need to check pstrIDvall ????? */
				if(pstrIDval==NULL) /* if NO ID, deem it as illegal */
				 {
//				  	 EGI_PLOG(LOGLV_WARN, "egi_iotclient: key 'ID' not found in recv_buf, continue...\n");
					 json_object_put(json);
				   	 continue;
				 }
				/* 4.3 renew reply_json's ID value: with visitor's ID value
				 * delete old "ID" key, then add own ID for reply
				 * add a json_obj1 to another json_obj2, you just pass the ownership, and shall
				 * not put obj1. obj1 will be freed when you put obj2.
				 */

				json_object_object_del(json_reply, "ID");
				json_object_object_add(json_reply, "ID", json_object_new_string(pstrIDval)); /* with strdup() inside */
				/* NOTE: json_object_new_string() with strdup() inside */

/* ---- key 'C': to control BULB ---- */
				/* 4.4 get a pointer to key 'C'(command) string value, and parse the string  */
				pstrCval=iot_getkey_pstrval(json, "C");/* get visiotr's Command value */
				if(pstrCval != NULL)
				{
					EGI_PDEBUG(DBG_IOT,"receive command: %s\n",pstrCval);
					/* parse command string */
					if(strcmp(pstrCval,"offOn")==0)
					{
						EGI_PDEBUG(DBG_IOT,"Execute command 'offOn' ....\n");
						/* toggle the bulb color */
						bulb_off=!bulb_off;
						if(bulb_off) {
							EGI_PDEBUG(DBG_IOT,"Switch bulb OFF \n");
							//subcolor=BULB_OFF_COLOR;
							bulb_color=BULB_OFF_COLOR;
							subcolor=bulb_color;
						}
						else {
							EGI_PDEBUG(DBG_IOT,"Switch bulb ON \n");
							//subcolor=BULB_ON_COLOR; /* mild white */
							bulb_color=egi_color_random(color_medium);
							subcolor=bulb_color;
						}

					}
					/* parse command 'plus' and 'up' */
					if( !bulb_off && (  (strcmp(pstrCval,"plus")==0)
							    || (strcmp(pstrCval,"up")==0)  ) )
					{
  					    	EGI_PDEBUG(DBG_IOT,"Execute command 'plus/up' ....\n");
						/* up limit value */
					    	//if(  subcolor <  SUBCOLOR_UP_LIMIT ) {
						//	subcolor += BULB_COLOR_STEP;
					    	//}
						k +=2;
						subcolor= egi_colorbrt_adjust(bulb_color,k);
						printf("---------bulb_color: 0x%02X,  subcolor: 0x%02X------\n",
										bulb_color, subcolor);

					}
					/* parse command 'minus' and 'down' */
					else if( !bulb_off && (  (strcmp(pstrCval,"minus")==0)
						         ||(strcmp(pstrCval,"down")==0)  ) )
					{
				    		EGI_PDEBUG(DBG_IOT,"Execute command 'minus/down' ....\n");
					#if 0
				    		if( subcolor < SUBCOLOR_DOWN_LIMIT ) {
							subcolor=SUBCOLOR_DOWN_LIMIT; /* low limit */
						}
				    		else {
							subcolor -= BULB_COLOR_STEP;
				    		}
					#endif
						k -= 2;
						subcolor=egi_colorbrt_adjust(bulb_color,k);
						printf("---------bulb_color: 0x%02X,  subcolor: 0x%02X------\n",
										bulb_color, subcolor);
					}

#ifdef IOT_HOME_BULB
					/* if digit, set as tag */
					else if( !bulb_off && isdigit(pstrCval[0]) )
					{
						egi_ebox_settag(iotbtn, pstrCval);
					}

					//printf("subcolor=0x%08X \n",subcolor);
					/* set subcolor to iotbtn */
					egi_btnbox_setsubcolor(iotbtn, subcolor);//COLOR_24TO16BITS(subcolor)); //iotbtn_subcolor);
					/* refresh iotbtn */
					egi_ebox_needrefresh(iotbtn);
					//egi_page_flag_needrefresh(page); /* !!!SLOW!!!! let page routine to do the refresh job */
					egi_ebox_refresh(iotbtn);
#endif

				} /* end of (pstrCval != NULL) */

/* ---- Create reply_json  ---- */
				/* 4.4 renew reply_json key 'C' value: with message string for reply */
				memset(keyC_buf,0,sizeof(keyC_buf));
				sprintf(keyC_buf,"'%s', bulb: %s, light: 0x%06X",
								pstrCval, bulb_status[bulb_off], subcolor);
				json_object_object_del(json_reply, "C");
				json_object_object_add(json_reply,"C", json_object_new_string(keyC_buf));
				/* 4.5 prepare reply string for socket */
				pstrtmp=json_object_to_json_string(json_reply);/* json..() return a pointer */
				strreply=(char *)malloc(strlen(pstrtmp)+TEMPLATE_MARGIN+2); /* at least +2 for '/n/0' */
				if(strreply==NULL)
				{
					EGI_PLOG(LOGLV_ERROR,"%s: fail to malloc strreply.\n",__func__);
					goto fail;
				}
				memset(strreply,0,strlen(pstrtmp)+TEMPLATE_MARGIN+2);
				sprintf(strreply, "%s\n",
					 json_object_to_json_string_ext(json_reply,JSON_C_TO_STRING_NOZERO));
				EGI_PDEBUG(DBG_IOT,"reply json string: %s\n",strreply);

				/* 4.6 send reply string by sockfd, to reply the visitor */
				iot_send(&ret,strreply);

			} /* end of else (json) < parse msg > */

			/* clear arena for next IoT talk */
			/* defre and free json objects */
			if(strreply !=NULL )
				free(strreply);

			//if( (jsret=json_object_put(json_item)) !=1)
			//	EGI_PLOG(LOGLV_ERROR,"%s: Fail to put(free) json_item!, jsret=%d \n",
			//								__func__, jsret);

			if( (jsret=json_object_put(json)) != 1) /* func with if(json) inside  */
				EGI_PLOG(LOGLV_ERROR,"%s: Fail to put(free) json!, jsret=%d \n",
											__func__, jsret);

		} /* end of if(iot_recv(&ret)==0) */
		else if(ret==0) /* socket disconnected, reconnect again. */
		{
			EGI_PLOG(LOGLV_ERROR,"%s: recv() ret=0 \n",__func__ );
			/* trap into loop of connect&checkin untill it succeeds. */
			iot_connect_checkin();
		}
		else /* ret<0 */
		{
			EGI_PLOG(LOGLV_ERROR,"%s: recv() error, %s\n",__func__, strerror(errno));
			if(ret==EBADF) /* invalid sock FD */
				EGI_PLOG(LOGLV_ERROR,"%s: Invalid socket file descriptor!\n",__func__);
			if(ret==ENOTSOCK) /* invalid sock FD */
				EGI_PLOG(LOGLV_ERROR,"%s: The file descriptor sockfd does not refer to a socket!\n",__func__);
		}

		//tm_delayms(IOT_CLIENT_);
		egi_sleep(0,0,200);

	} /* end of while() */

	/* joint threads */
	pthread_join(pthd_keepalive,NULL);
	pthread_join(pthd_update,NULL);

fail:
	/* free var */
	if(strreply != NULL )
		free(strreply);
	/* release json object */
	json_object_put(json); /* func with if(json) inside */
	//// put other json objs //////

	/* free printbuf */
	printbuf_free(pjbuf);

	/* close socket FD */
	close(sockfd);

	return ;
}
