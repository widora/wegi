/*--------------------------------------------------------
A test to draw Julia_Set and Manderbort_Set.

Reference: 孔令德(UID:486651102) @bilibili  计算机图形学
  	   例8-10: julia集合  例8-101: Mandelbrot集

Journal:
2021-08-08: Create the file.

Midas Zhou
---------------------------------------------------------*/
#include <math.h>
#include <stdio.h>
#include "egi_fbdev.h"
#include "egi_fbgeom.h"
#include "egi_FTsymbol.h"
#include "egi_math.h"

class CComplex
{
public:
	CComplex(void);
	virtual ~CComplex(void);
	CComplex(double real, double image);

	friend CComplex operator + (CComplex &c1, CComplex &c2);
	friend CComplex operator - (CComplex &c1, CComplex &c2);
	friend CComplex operator * (CComplex &c1, CComplex &c2);
	friend CComplex operator / (CComplex &c1, CComplex &c2);

	friend double Magnitude(CComplex c) {
		return sqrt(c.real*c.real+c.imag*c.imag);
	};

	void print(void) {
		printf("(%f, %fi)\n", real, imag);
	};

public:
	double real, imag;
};

CComplex::CComplex(void)
{
	real=0.0;
	imag=0.0;
}

CComplex::~CComplex(void) {};

CComplex::CComplex(double real, double image)
{
	this->real=real;
	this->imag=image;
}


CComplex operator + (CComplex &c1, CComplex &c2)
{
	CComplex c;
	c.real = c1.real+c2.real;
	c.imag = c1.imag+c2.imag;

	return c;
}

CComplex operator - (CComplex &c1, CComplex &c2)
{
        CComplex c;
        c.real = c1.real-c2.real;
        c.imag = c1.imag-c2.imag;

        return c;
}

CComplex operator * (CComplex &c1, CComplex &c2)
{
        CComplex c;
        c.real = c1.real*c2.real-c1.imag*c2.imag;
        c.imag = c1.real*c2.imag+c2.real*c1.imag;

        return c;
}

CComplex operator / (CComplex &c1, CComplex &c2)
{
        CComplex c;
	double  dd=c2.real*c2.real+c2.imag*c2.imag;

        c.real = (c1.real*c2.real+c1.imag*c2.imag)/dd;
        c.imag = (c2.real*c1.imag-c1.real*c2.imag)/dd;

        return c;
}


/*------- Julia set --------*/
void drawJuliaSet(FBDEV *fbdev, double Creal, double Cimag)
{
#ifdef LETS_NOTE
	int d=900;
	int xres=1200;
#else
	int d=300;
	int xres=320;
#endif
	double RezMin=-1.5, RezMax=1.5;  /* Real part LIMIT */
	double ImzMin=-1.5, ImzMax=1.5;  /* Image part LIMIT */

	double dx=(RezMax-RezMin)/d;	/* Step on real axis */
	double dy=(ImzMax-ImzMin)/d;	/* Step on image axis */

	int nMax=100; /* Max. iterations */
	int rMax=2;   /* threshold value! */
	double r;     /* mod(z) */
	CComplex z,c; /* complex z,c */
	CComplex zz;

	fbdev->pixcolor_on=true;

	for(int j=-xres/2; j<xres/2; j++) {
		for(int i=-xres/2; i<xres/2; i++) {
			c.real=Creal;  c.imag=Cimag;	/* fix c */
			z.real = i*dx; z.imag = j*dy; 	/* iterate z */
			for(int n=0; n<nMax; n++) {
				r=Magnitude(z);
				if(r>rMax)
					break;
				zz=z*z;
				z=zz + c;
				fbset_color2(fbdev, n*12000);
				draw_dot(fbdev, i+xres/2,j+xres/2 -xres/8);
			}
		}
	}
}


/*------- Julia set --------*/
void drawMandelbrotSet(FBDEV *fbdev)
{
#ifdef LETS_NOTE
	int d=1200;
	int xres=1000;
#else
	int d=300;
	int xres=320;
#endif

	double RezMin=-1.5, RezMax=1.5;  /* Real part LIMIT */
	double ImzMin=-1.5, ImzMax=1.5;  /* Image part LIMIT */

	double dx=(RezMax-RezMin)/d;	/* Step on real axis */
	double dy=(ImzMax-ImzMin)/d;	/* Step on image axis */

	int nMax=100; /* Max. iterations */
	int rMax=2;   /* threshold value! */
	double r;     /* mod(z) */
	CComplex z,c; /* complex z,c */
	CComplex zz;

	fbdev->pixcolor_on=true;

	for(int j=-xres/2; j<xres/2; j++) {
		for(int i=-xres/2 -xres/3; i<xres/2; i++) {
			c.real = i*dx;  c.imag = j*dy; 	/* iterate c */
			z.real=0.0;	z.imag=0.0;	/* fix z */
			for(int n=0; n<nMax; n++) {
				r=Magnitude(z);
				if(r>rMax)
					break;
				zz=z*z;
				z=zz + c;
				//fbset_color2(fbdev, n>=nMax-1?WEGI_COLOR_GRAY:WEGI_COLOR_BLACK); //n*1000);
				fbset_color2(fbdev, n>=50?WEGI_COLOR_BLACK:WEGI_COLOR_GRAY); //n*1000);
				draw_dot(fbdev, i+xres/2 +xres/6, j+xres/2-xres/8);
			}
		}
	}
}


/*==============
     MAIN
==============*/
int main()
{
	char strtmp[1024];
	double Creal, Cimag;
	int nrand;

	/* -----TEST: CComplex */
	CComplex compA(1,2);
	CComplex compB(100,200);
	CComplex compC;
	compA.print();
	compC=compA+compB;

        /* Initilize sys FBDEV */
        printf("init_fbdev()...\n");
        if(init_fbdev(&gv_fb_dev))
                return -1;

        /* Load freetype appfonts */
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev,true);
        fb_position_rotate(&gv_fb_dev,0);
        gv_fb_dev.pixcolor_on=true;


  while(1) { ////////////////////////////////////////////////////////
	//fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY);
	clear_screen(&gv_fb_dev, WEGI_COLOR_GRAY);


#if 1	/* Draw Julia Set */
	nrand=50+mat_random_range(400)+1; /* 50 ~ 450 */
	Creal=( nrand*( mat_random_range(2) ? 1.0:-1.0 ) )/500.0;     /* +-(50~450)/500: +- 0.1~0.9 */
	nrand=50+mat_random_range(400)+1; /* 50 ~ 450 */
	Cimag=( nrand*( mat_random_range(2) ? 1.0:-1.0 ) )/500.0;     /* +-(50~450)/500: +- 0.1~0.9 */

	drawJuliaSet(&gv_fb_dev, Creal, Cimag);

#else	/* Draw Mandelbrot Set */
	drawMandelbrotSet(&gv_fb_dev);
	return 0;
#endif

	/* Note */
	sprintf(strtmp,"Julia Set: %.4f, %.4f", Creal, Cimag);
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold, /* FBdev, fontface */
                                        16, 16,                         /* fw,fh */
                                        (UFT8_PCHAR)strtmp, /* pstr */
                                        300, 1, 0,                        /* pixpl, lines, fgap */
                                        5, 240-20,                            /* x0,y0, */
                                        WEGI_COLOR_WHITE, -1, 230,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */


	sleep(6);

  } ////////////////////////////////////////////////

	return 0;
}
