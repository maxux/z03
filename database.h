#ifndef __IMAGESPAWN_SQLITE
	#define __IMAGESPAWN_SQLITE

	#include <sqlite3.h>
	
	// #define SQL_DATABASE_FILE	"sp0wimg-test.sqlite3"
	#define SQL_DATABASE_FILE	"sp0wimg.sqlite3"
	
	extern sqlite3 *sqlite_db;
	
	sqlite3 * db_sqlite_init();
	sqlite3_stmt * db_select_query(sqlite3 *db, char *sql);
	int db_simple_query(sqlite3 *db, char *sql);
	int db_sqlite_parse(sqlite3 *db);
#endif
