/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of displaying input TXT on screen, and a simple way of
extracting key words in TXT to control 3 lights .
!!! For 'One Server One Client' scenario only !!!

Note:
1. Run server process in back ground.
   (If '/dev/shm/writetxt_ctrl' exists already, it will remove it first!)
        ./test_writetxt >/dev/null 2>&1 &
2. Run a client process to send TXT to server for displaying:
        ./test_writetxt -t 3 "Hello world!世界你好！"
3. To quit the server process:
        ./test_writext -q
4. It uses shared memory as IPC to communicate between the server and
   the client process.
5. To incoorperate:
   autorec  /tmp/asr_snd.pcm  ( autorec ---> asr_pcm.sh ---> writetxt )

Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <getopt.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_gif.h"
#include "egi_shmem.h"

#define SHMEM_SIZE 	(1024*4)
#define TXTDATA_OFFSET	1024
#define TXTDATA_SIZE 	1024

int main(int argc, char **argv)
{
	int ts=1; /* Default displaying for 1 second */
	int opt;
	bool sigstop=false;
	char *ptxt=NULL;
	char *pdata=NULL;

	if(argc < 1)
		return 2;

        /* parse input option */
        while( (opt=getopt(argc,argv,"ht:q"))!=-1)
        {
                switch(opt)
                {
                       	case 'h':
                           printf("usage:  %s [-ht:q] text \n", argv[0]);
                           printf("         -h   help \n");
			   printf("	    -q   to end the server process.\n");
                           printf("         -t   holdon time, in seconds.\n");
                           printf("         text Text for displaying \n");
			   printf(" Run server process in back ground: test_writetxt >/dev/null 2>&1 & \n" );
			   printf(" Run a client process to send TXT to server for displaying: test_writetxt -t 3 世界你好！\n");                           return 0;
		 	case 't':
			   ts=atoi(optarg);
			   printf("ts=%d\n",ts);
			   break;
                       	case 'q':
                           printf(" Signal to quit text displaying process...\n");
                           sigstop=true;
                           break;
                       	default:
                           break;
                }
        }
	if(optind<argc) {
		ptxt=argv[optind];
		printf("Input TXT: %s\n",ptxt);
	}

        /* ------------ SHM_MSG communication ---------- */
        EGI_SHMEM shm_msg= {
          .shm_name="writetxt_ctrl",
          .shm_size=SHMEM_SIZE,
        };

        if( egi_shmem_open(&shm_msg) !=0 ) {
                printf("Fail to open shm_msg!\n");
                exit(1);
        }
	/* Assign shm data starting address */
	pdata=shm_msg.shm_map+TXTDATA_OFFSET;
	memset( shm_msg.msg_data->msg,0,64 );
	memset( pdata, 0, TXTDATA_SIZE); /* clear data block */

        /* shm_msg communictate */
        if( shm_msg.msg_data->active ) {         /* If msg_data is ACTIVE already */
                if(shm_msg.msg_data->msg[0] != '\0')
                        printf("Msg in shared mem '%s' is: %s\n", shm_msg.shm_name, shm_msg.msg_data->msg);
                if( ptxt != NULL ) {
			shm_msg.msg_data->signum=ts;
			strncpy( shm_msg.msg_data->msg, "New data available", 64);
                        strcpy( pdata, ptxt);
                }

                /* ---- signal to quit the main process ----- */
                if( sigstop ) {
                        printf("Signal to stop and wait....\n");
                        while( shm_msg.msg_data->active) {
                                shm_msg.msg_data->sigstop=true;
                                usleep(100000);
                        }
		}
               	egi_shmem_close(&shm_msg);
               	exit(1);
        }
        else {                                  /* ELSE if not active, activate it....  */
                shm_msg.msg_data->active=true;
                printf("You are the first to handle this mem!\n");
        }


        /* <<<<<  EGI general init  >>>>>> */
#if 0
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */

        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_gif") != 0) {        /* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
#endif

#if 1
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {       /* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }

        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }
#endif

        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;

#if 0
        printf("start touchread thread...\n");
        egi_start_touchread();                  /* start touch read thread */
#endif
        /* <<<<------------------  End EGI Init  ----------------->>>> */

	/* Direct FB */
	fb_set_directFB(&gv_fb_dev, true);
	/* set pos_rotation */
	gv_fb_dev.pos_rotate=3;


	printf("Start loop ...\n");
while(1) {

	/* check EGI_SHM_MSG */
        if(shm_msg.msg_data->sigstop ) {
        	printf(" ---------- shm_msg: sigstop received! \n");
                /* reset data */
                shm_msg.msg_data->signum=0;
                shm_msg.msg_data->active=false;
                shm_msg.msg_data->sigstop=false;
                break;
        }

	/* Check txt data for display  */
	if( pdata[0]=='\0') {
		printf(" --- \n");
		usleep(200000);
		continue;
	} else {
		printf("shm_msg.msg_data->signum=%d\n", shm_msg.msg_data->signum);
		printf("shm_msg.msg_data->msg: %s\n", shm_msg.msg_data->msg);
        	printf("shm priv data: %s\n", pdata);
	}

	/* Simple NLU: parse input txt data and extract key words */
	/*  Light size 80x80 */
	if(strstr(pdata,"开")) {
	    if(strstr(pdata,"灯")) {
		if( strstr(pdata,"红") ) {
			printf("Turn on red light!\n");
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_RED, 20, 140, 20+80, 140+80);
		}
		if(strstr(pdata,"绿") ) {
			printf("Turn on green light!\n");
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GREEN, 20+100, 140, 20+100+80, 140+80);
		}
		if(strstr(pdata,"蓝") ) {
			printf("Turn on blue light!\n");
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_BLUE, 20+200, 140, 20+200+80, 140+80);
		}
		if(strstr(pdata,"所有") || strstr(pdata,"全部")) {
			printf("Turn on all lights!\n");
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_RED, 20, 140, 20+80, 140+80);
                        draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GREEN, 20+100, 140, 20+100+80, 140+80);
                        draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_BLUE, 20+200, 140, 20+200+80, 140+80);
		}
	    }
	}
	if(strstr(pdata,"关")) {
	    if(strstr(pdata,"灯")) {
		if( strstr(pdata,"红") ) {
			printf("Turn off red light!\n");
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY5, 20, 140, 20+80, 140+80);
		}
		if(strstr(pdata,"绿") ) {
			printf("Turn off green light!\n");
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY5, 20+100, 140, 20+100+80, 140+80);
		}
		if(strstr(pdata,"蓝") ) {
			printf("Turn off blue light!\n");
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY5, 20+200, 140, 20+200+80, 140+80);
		}
		if(strstr(pdata,"所有") || strstr(pdata,"全部")) {
			printf("Turn on all lights!\n");
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY5, 20, 140, 20+80, 140+80);
                        draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY5, 20+100, 140, 20+100+80, 140+80);
                        draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY5, 20+200, 140, 20+200+80, 140+80);
		}
	    }
	}

	/* backup current screen */
        fb_page_saveToBuff(&gv_fb_dev, 0);

	FTsymbol_uft8strings_writeFB( 	&gv_fb_dev, egi_appfonts.bold,  	/* FBdev, fontface */
				      	30, 30,(const unsigned char *)pdata, 	/* fw,fh, pstr */
				      	320-10, 5, 4,                    	/* pixpl, lines, gap */
					10, 10,                           	/* x0,y0, */
                                     	WEGI_COLOR_RED, -1, -1,      /* fontcolor, transcolor,opaque */
                                     	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

	/* hold on */
	ts=shm_msg.msg_data->signum;
	sleep(ts);
	/* reset data */
	pdata[0]='\0';
	/* restore backed screen */
	fb_page_restoreFromBuff(&gv_fb_dev, 0);
}

	/* close shmem */
        egi_shmem_close(&shm_msg);

        /* <<<<<-----------------  EGI general release  ----------------->>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
        printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev); /* Flush FB filo if necessary */
        release_fbdev(&gv_fb_dev);
        printf("egi_end_touchread()...\n");
        egi_end_touchread();
        printf("egi_quit_log()...\n");
#if 0
        egi_quit_log();
        printf("<-------  END  ------>\n");
#endif
        return 0;
}
