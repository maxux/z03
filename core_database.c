/* z03 - small bot with some network features - sqlite3 database file
 * Author: Daniel Maxime (maxux.unix@gmail.com)
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sqlite3.h>
#include "bot.h"
#include "core_init.h"
#include "core_database.h"

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

unsigned int db_sqlite_num_rows(sqlite3_stmt *stmt) {
	unsigned int nbrows = 0;
	
	while(sqlite3_step(stmt) != SQLITE_DONE)
		nbrows++;
	
	sqlite3_finalize(stmt);
	
	return nbrows;
}