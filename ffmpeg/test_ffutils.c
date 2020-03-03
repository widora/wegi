#include <stdio.h>
//#include "ff_utils.h"
#include "egi_ffplay.h"

int main(void)
{
	int i;

  do{

	if( egi_init_ffplayCtx("/mmc", "mp3, avi, jpg")) {
		printf("Fail to call egi_init_ffplayCtx() .\n");
		return -1;
	}

	printf("Print totally %d fpath...\n", FFplay_Ctx->ftotal);
	for(i=0; i < FFplay_Ctx->ftotal; i++)
		printf("%s\n",(FFplay_Ctx->fpath)[i]);

	usleep(100000);

	egi_free_ffplayCtx();

  }while(0);

  return 0;
}
