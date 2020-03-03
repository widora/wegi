/* -------------------------------------------------------------------------------------
  https://www.sqlite.org/quickstart.html
  See also the Introduction To The SQLite C/C++ Interface for an introductory overview
  and roadmap to the dozens of SQLite interface functions.
---------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sqlite3.h>

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
	int i;
	static bool token=false;

//    	for(i=0; i<argc; i++){
//      		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
//    	}
//    	printf("\n");

	/* print column name */
	if(token==false) {
	    	for(i=0; i<argc; i++)
      			printf("-----------");
    		printf("\n");

	    	for(i=0; i<argc; i++) {
      			printf("%-12s", azColName[i]);
	    	}
    		printf("\n");

	    	for(i=0; i<argc; i++)
      			printf("-----------");
    		printf("\n");

		token=true;
	}

	/* print argv  */
	for(i=0; i<argc; i++) {
		printf("%-12s", argv[i] ? argv[i] : "NULL");
	}
 	printf("\n");

    	return 0;
}


int main(int argc, char **argv)
{
    	sqlite3 *db;
    	char *zErrMsg = 0;
    	int rc;

	if( argc!=3 ) {
      		fprintf(stderr, "Usage: %s DATABASE SQL-STATEMENT\n", argv[0]);
	      	return(1);
    	}

 	rc = sqlite3_open(argv[1], &db);
    	if( rc ) {
	      	fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
      		sqlite3_close(db);
      		return(1);
    	}

    	rc = sqlite3_exec(db, argv[2], callback, 0, &zErrMsg);
    	if( rc!=SQLITE_OK ) {
	      fprintf(stderr, "SQL error: %s\n", zErrMsg);
	      sqlite3_free(zErrMsg);
    	}
	sqlite3_close(db);

    return 0;
}
