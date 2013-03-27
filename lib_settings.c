/* z03 - small bot with some network features - irc miscallinious functions (anti highlights, ...)
 * Author: Daniel Maxime (root@maxux.net)
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
#include <string.h>
#include <jansson.h>
#include "bot.h"
#include "core_init.h"
#include "lib_database.h"
#include "lib_list.h"
#include "lib_core.h"
#include "lib_settings.h"

int settings_set(char *nick, char *key, char *value, settings_view view) {
	char *sqlquery;
	int retcode;
	
	sqlquery = sqlite3_mprintf(
		"REPLACE INTO settings (nick, key, value, private) "
		"VALUES ('%q', LOWER('%q'), LOWER('%q'), '%d')",
		nick, key, value, view
	);
	
	if(!db_sqlite_simple_query(sqlite_db, sqlquery)) {
		fprintf(stderr, "[-] settings/set: cannot insert data\n");
		retcode = 1;
	
	} else retcode = 0;
	
	/* Clearing */
	sqlite3_free(sqlquery);
	
	return retcode;
}

char *settings_get(char *nick, char *key, settings_view view) {
	sqlite3_stmt *stmt;
	char *sqlquery, *row_value = NULL;
	int row;
	
	if(view == PRIVATE)
		sqlquery = sqlite3_mprintf(
			"SELECT value FROM settings             "
			"WHERE nick = '%q' AND key = LOWER('%q')",
			nick, key
		);
		
	else sqlquery = sqlite3_mprintf(
		"SELECT value FROM settings "
		"WHERE nick = '%q'          "
		"  AND key = LOWER('%q')    "
		"  AND private = 0          ", 
		nick, key
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW)
			row_value = strdup((char *) sqlite3_column_text(stmt, 0));
	
	} else fprintf(stderr, "[-] settings/get: cannot select data\n");
	
	/* Clearing */
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return row_value;
}

int settings_unset(char *nick, char *key) {
	char *sqlquery;
	int retcode;
	
	sqlquery = sqlite3_mprintf("DELETE FROM settings WHERE nick = '%q' AND key = LOWER('%q')", nick, key);
	
	if(!db_sqlite_simple_query(sqlite_db, sqlquery)) {
		fprintf(stderr, "[-] settings/set: cannot insert data\n");
		retcode = 1;
	
	} else retcode = 0;
	
	/* Clearing */
	sqlite3_free(sqlquery);
	
	return retcode;
}

settings_view settings_getview(char *nick, char *key) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	settings_view view = PUBLIC;
	int row;
	
	sqlquery = sqlite3_mprintf(
		"SELECT private FROM settings          "
		"WHERE nick = '%q' AND key = LOWER('%q')",
		nick, key
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW)
			view = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] settings/get: cannot select data\n");
	
	/* Clearing */
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return view;
}
