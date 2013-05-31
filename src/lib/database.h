#ifndef __Z03_SQLITE
	#define __Z03_SQLITE

	#include <sqlite3.h>
	
	#ifdef __DEBUG__
		#define SQL_DATABASE_FILE	"sp0wimg-test.sqlite3"
	#else
		#define SQL_DATABASE_FILE	"sp0wimg.sqlite3"
	#endif
	
	extern sqlite3 *sqlite_db;
	
	sqlite3 *db_sqlite_init();
	int db_sqlite_close(sqlite3 *db);
	
	sqlite3_stmt *db_sqlite_select_query(sqlite3 *db, char *sql);
	int db_sqlite_simple_query(sqlite3 *db, char *sql);
	unsigned int db_sqlite_num_rows(sqlite3_stmt *stmt);
#endif
