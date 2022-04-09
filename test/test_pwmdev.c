/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Refrence: http://blog.csdn.net/qianrushizaixian/article/details/46536005

An example to set/adjust PWM for LCD backlight.

Journal:
	2022-04-08: Create the file.


Midas Zhou
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>

#include "egi_debug.h"

int egi_adjust_pwm(int pwmnum, int duty);

int main(int argc, char **argv)
{
	int duty=0;
	int step=1;

 while(1) {
	egi_adjust_pwm(0, duty);
	usleep(1500);

	if(duty==100) step=-1;
	if(duty==0) step=1;

	duty +=step;
 }

	if( argc>1 )
		duty=atoi(argv[1]);
	else
		duty=50;

	egi_adjust_pwm(0,duty);

	return 0;
}


/*--------------------------------------------
Adjust PWM duty cycle.

@pwmnum:  PWM number.
	  0 0R 1.
@duty:    Percentage value of duty cycle.
          [0 100]

Return:
	0	OK
	<0	Fail
----------------------------------------------*/
#include "/home/midas-zhou/Ctest/kmods/soopwm/sooall_pwm.h"
int egi_adjust_pwm(int pwmnum, int duty)
{
	int pwm_fd;
	struct pwm_cfg  cfg;
	int ret;

	/* Open PWM dev */
	pwm_fd = open("/dev/"PWM_DEV_NAME, O_RDWR);
	if (pwm_fd < 0) {
		egi_dperr("open pwm failed");
		return -1;
	}

	/* Limt */
	if(duty<0)duty=0;

	/* Set PWM parameters for old/classic mode. */
	cfg.no        =   pwmnum?1:0;  	 /* 0:pwm0, 1:pwm1 */
	cfg.clksrc    =   PWM_CLK_40MHZ; /* 40MHZ or 100KHZ */
	cfg.clkdiv    =   PWM_CLK_DIV4;  /* DIV2 40/2=20MHZ */
	cfg.old_pwm_mode =true;    	 /* TRUE: old mode; FALSE: new mode */
	cfg.idelval   =   0;
	cfg.guardval  =   0;
	cfg.guarddur  =   0;
	cfg.wavenum   =   0;  		 /* forever loop */
	cfg.datawidth =   100;           /* limit 2^13-1=8191 */
	cfg.threshold =   duty;		 /* Duty cycle */

	/*** Check period
	 *  100KHz CLK: period=1000/100(KHZ)*(DIV(1-128))*datawidth   (us)
	 *  40MHz CLK: period=1000/40(MHz)*(DIV)*datawidth            (ns)
	 */
	if(cfg.old_pwm_mode == true) {
           if(cfg.clksrc == PWM_CLK_100KHZ)
		egi_dpstd("Set PWM period=%d us\n",(int)(1000.0/100.0*(1<<cfg.clkdiv)*cfg.datawidth));
           else if(cfg.clksrc == PWM_CLK_40MHZ)
		egi_dpstd("Set PWM period=%d ns\n",(int)(1000.0/40.0*(1<<cfg.clkdiv)*cfg.datawidth));
         }
	else  /* NEW mode */
		egi_dpstd("Set PWM senddata0: %#08x  senddata1: %#08x \n",cfg.senddata0,cfg.senddata1);

	/* Configure and enable PWM */
	if( ioctl(pwm_fd, PWM_CONFIGURE, &cfg)<0 || ioctl(pwm_fd, PWM_ENABLE, &cfg)<0 )
		ret=-1;

	close(pwm_fd);

	return ret;
}
