/*---------------  jpgshow.c  --------------------
-------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "egi_bmpjpg.h"

int main(int argc, char **argv)
{
	int ret=0;
       	FBDEV fr_dev;

        /* prepare fb device */
        fr_dev.fdfd=-1;
        init_dev(&fr_dev);

	ret=show_jpg(argv[1],&fr_dev,0,0,0);/* 0-BALCK_ON, 1-BLACK_OFF */
	if(ret!=0)
		printf("jpgshow() fails!\n");

        /* delete fb map and close fb dev */
        release_dev(&fr_dev);

        return ret;
}

