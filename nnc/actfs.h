/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
-----------------------------------------------------------------------*/
#ifndef __ACTFS_H__
#define __ACTFS_H__

#define DERIVATIVE_FUNC	1  /* to switch to derivative calculation in a function */
#define NORMAL_FUNC	0

double func_step(double x, double f, int token);
double func_sigmoid(double x, double f, int token);
double func_TanSigmoid(double x, double f, int token);
double func_ReLU(double x, double f, int token);
double func_PReLU(double x, double f, int token);

#endif
