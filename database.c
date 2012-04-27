#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sqlite3.h>
#include "imagespawn.h"
#include "bot.h"
#include "database.h"

sqlite3 *sqlite_db;

/*
 * SQLite Management
 */
sqlite3 * db_sqlite_init() {
	sqlite3 *db;
	
	printf("[+] SQLite: Initializing <%s>\n", SQL_DATABASE_FILE);
	
	if(sqlite3_open(SQL_DATABASE_FILE, &db) != SQLITE_OK) {
		fprintf(stderr, "[+] SQLite: cannot open sqlite databse: <%s>\n", sqlite3_errmsg(db));
		return NULL;
	}
	
	return db;
}

sqlite3_stmt * db_select_query(sqlite3 *db, char *sql) {
	sqlite3_stmt *stmt;
	
	/* Debug SQL */
	printf("[+] SQLite: <%s>\n", sql);
	
	/* Query */
	if(sqlite3_prepare_v2(db, sql, strlen(sql) + 1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "[+] SQLite: query <%s> failed: %s\n", sql, sqlite3_errmsg(db));
		return NULL;
	}
	
	return stmt;
}

int db_simple_query(sqlite3 *db, char *sql) {
	/* Debug SQL */
	printf("[+] SQLite: <%s>\n", sql);
	
	/* Query */
	if(sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
		fprintf(stderr, "[+] SQLite: query <%s> failed: %s\n", sql, sqlite3_errmsg(db));
		return 0;
	}
	
	return 1;
}


int db_sqlite_parse(sqlite3 *db) {
	sqlite3_stmt *stmt;
	int i, row;
	
	/* Reading table content */
	if((stmt = db_select_query(db, "SELECT * FROM url")) == NULL)
		return 0;
	
	/* Counting... */
	i = 0;
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW)
			i++;
	}
	
	/* Clearing */
	sqlite3_finalize(stmt);
	
	printf("[+] SQLite: url parsed: %d rows returned\n", i);
	
	return 1;
}
