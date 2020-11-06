/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdlib.h>
#include <unistd.h>
#include <egi_utils.h>
#include <egi_cstring.h>
#include <dirent.h>

char *path="/mmc";

int main(void)
{
	int i;
	DIR *dir;
	struct dirent *file;
	struct stat sb;
	char fullpath[EGI_PATH_MAX+EGI_NAME_MAX];
	char tmptxt[EGI_PATH_MAX+EGI_NAME_MAX+64];

	EGI_TXTGROUP *txtgroup=NULL;

#if 1	//////////////  TEST: BIGFILE size  /////////////////
	unsigned long long fsize;
	int tm_cost=38540; //ms

	// 43896 IPACKs, with each 65888 bytes, and a tail packet of 25231 bytes
	fsize=(unsigned long long)43896*65888+25231;

	printf("File is received, total cost time: %.2fs,  avg speed: %.2fKBs.\n", tm_cost/1000.0, fsize/1024.0/tm_cost*1000.0 );

#endif	/////////////////


//  while(1) { //////////////////////////  LOOP TEST  /////////////////////

	/* Create txtgroup */
	txtgroup=cstr_txtgroup_create(0,0);
	if(txtgroup==NULL) exit(1);

	/* Open dir */
        if(!(dir=opendir(path)))
		return -1;

        /* Get file names */
        while((file=readdir(dir))!=NULL) {
           	/* If NOT a regular file */
		snprintf(fullpath, EGI_PATH_MAX+EGI_NAME_MAX-1,"%s/%s",path ,file->d_name);
                if( stat(fullpath, &sb)<0 || !S_ISREG(sb.st_mode) ) {
			//printf("%s \n",fullpath);
                        continue;
		}
#if 1
		if( ((sb.st_size)>>20) > 0)
			//printf("%-22s  %.1fM\n", file->d_name, sb.st_size/1024.0/1024.0);
			snprintf(tmptxt,sizeof(tmptxt)-1,"%-22s  %.1fM", file->d_name, sb.st_size/1024.0/1024.0);
		else if ( ((sb.st_size)>>10) > 0)
			//printf("%-22s  %.1fk\n", file->d_name, sb.st_size/1024.0);
			snprintf(tmptxt,sizeof(tmptxt)-1,"%-22s  %.1fk", file->d_name, sb.st_size/1024.0);
		else
			//printf("%-22s  %ldB\n", file->d_name, sb.st_size);
			snprintf(tmptxt,sizeof(tmptxt)-1,"%-22s  %lldB", file->d_name, sb.st_size);
#endif
		//cstr_txtgroup_push(txtgroup, fullpath);
		cstr_txtgroup_push(txtgroup, tmptxt);

	}

	/* Print txtgroup */
	for(i=0; i< txtgroup->size; i++)
		printf("%d: %s\n", i, txtgroup->buff+txtgroup->offs[i]);

	cstr_txtgroup_free(&txtgroup);

	closedir(dir);

	usleep(50000);
//}  /////////////////////////// END LOOP TEST ///////////////

	return 0;
}
