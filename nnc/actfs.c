
/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------------*/
#include "actfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>



/*-----------------------------------------------
 * Note:
 *	A step transfer function.
 * Params:
 * 	@u	input param for transfer function;
 * Return:
 *		a double.
------------------------------------------------*/
double func_step(double x, double f, int token)
{
   /* Normal func */
   if(token==NORMAL_FUNC) {
	if(x>=0)
		return 1.0;
	else
		return 0.0;
   }
   /* Derivative func */
   else {
	//printf("%s: Derivative function NOT defined!!! \n",__func__);
	return 0; ///////////////////////////////// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   }
}


/*-------------------------------------------------------------------
 * Note:
 *	A Log-Sigmoid(logistic) function:
 *      	f(x)=1/(1+exp(-ax)).
 *		f'(x)=a*f(x)*[1-f(-x)].
 *	Here take a=1.
 * Params:
 * 	@x,f	input param for function;
 *		if token==0:  x,    for sigmoid f(x)
 *		if token!=0:  f=f(x), f'(x)=f(x)(1-f(-x)), where a=1.
 *	@token	0 ---normal func; 1 ---derivative func
 * Return:
 *		a double.
---------------------------------------------------------------------*/
double func_sigmoid(double x, double f, int token)
{
   /* Normal func */
   if(token==NORMAL_FUNC) {
	return 1.0/(1.0+exp(-x));
   }
   /* if DERIVATIVE_FUNC Derivative func
    * Note: input of f'(u) is f(u), NOT u!!
    */
   else  {
	return f*(1-f);
  }
}


/*-------------------------------------------------------------------
 * Note:
 *	A Tan-Sigmoid function:
 *      	f(x)=2/(1+exp(-ax))-1
 *		f'(x)=a*[1-f(x)^2]/2
 *	Here take a=2.
 * Params:
 * 	@x,f	input param for function;
 *		if token==0: x       for Tan-Sigmoid f(x)
 *		if token!=0: f=f(x)  for f'(x)=1-f(x)^2  where a=2
 *	@token	0 ---normal func; 1 ---derivative func
 * Return:
 *		a double.
---------------------------------------------------------------------*/
double func_TanSigmoid(double x, double f, int token)
{
   double a=2.0;

   /* Normal func */
   if(token==NORMAL_FUNC) {
	return 2.0/(1.0+exp(-a*x))-1.0;
   }
   /* if DERIVATIVE_FUNC, Derivative func
    * Note: input of f'(u) is f(u), NOT u!!
    */
   else  {
	return a*(1-f*f)/2.0;
  }

}


/*-------------------------------------------------------------------
 * Note:
 *	A Rectified Linear Unit (ReLU) function:
 *      	f(x)=0  for x<0
 *      	f(x) =x  for x>=0
 *		f'(x)=0 for x<0
 *		f'(x)=1 for x>=0
 *
 *	Here take a=2.
 * Params:
 * 	@x,f=f(x)  input param for function;
 *	@token	0 ---normal func; 1 ---derivative func
 * Return:
 *		a double.
---------------------------------------------------------------------*/
double func_ReLU(double x, double f, int token)
{
   /* Normal func */
   if(token==NORMAL_FUNC) {
	if(x<0)
		return 0.0;
	else
		return x;
   }
   /* if DERIVATIVE_FUNC, Derivative func */
   else  {
	if(x<0) return 0.0;
	else	return 1.0;
  }
}


/*-------------------------------------------------------------------
 * Note:
 *	A Parameteric Rectified Linear Unit (ReLU) function:
 *      	f(x)=ax  for x<0  a(0-1)
 *      	f(x)=x   for x>=0
 *		f'(x)=a for x<0
 *		f'(x)=1 for x>=0
 *
 * Params:
 * 	@x,f=f(x)  input param for function;
 *	@token	0 ---normal func; 1 ---derivative func
 * Return:
 *		a double.
---------------------------------------------------------------------*/
double func_PReLU(double x, double f, int token)
{
    double a=0.5;

   /* Normal func */
   if(token==NORMAL_FUNC) {
	if(x<0)
		return a*x;
	else
		return x;
   }
   /* if DERIVATIVE_FUNC, Derivative func */
   else  {
	if(x<0) return a;
	else	return 1.0;
  }
}

