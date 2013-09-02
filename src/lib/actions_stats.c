/* z03 - logs based actions (backlog, url search, ...)
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
#include <stdlib.h>
#include "../common/bot.h"
#include "../core/init.h"
#include "database.h"
#include "core.h"
#include "actions.h"
#include "ircmisc.h"
#include "actions_stats.h"

//
// registering commands
//

static request_t __action_stats = {
	.match    = ".stats",
	.callback = action_stats,
	.man      = "print url statistics",
	.hidden   = 0,
	.syntaxe  = "",
};

__registrar actions_stats() {
	request_register(&__action_stats);
}

//
// commands implementation
//

void action_stats(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char msg[256];
	int count = 0, cnick = 0, chits = 0;
	int lines = 0, words = 0;
	time_t timestamp;
	(void) args;
	
	sqlquery = sqlite3_mprintf(
		"SELECT COUNT(*), COUNT(DISTINCT nick), SUM(hit), "
		"   (SELECT SUM(lines) FROM stats WHERE chan = '%q') lines, "
		"   (SELECT SUM(words) FROM stats WHERE chan = '%q') words "
		"FROM url WHERE chan = '%q'",
		message->chan, message->chan, message->chan
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			count = sqlite3_column_int(stmt, 0);
			cnick = sqlite3_column_int(stmt, 1);
			chits = sqlite3_column_int(stmt, 2);
			lines = sqlite3_column_int(stmt, 3);
			words = sqlite3_column_int(stmt, 4);
		}
			
		zsnprintf(msg, "Got  : %d url for %d nicks and %d total hits, %d lines and %d words",
		               count, cnick, chits, lines, words);
		irc_privmsg(message->chan, msg);
	
	} else fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	/* Today */
	timestamp = today();
	
	sqlquery = sqlite3_mprintf(
		"SELECT COUNT(*) FROM logs "
		"WHERE chan = '%q' AND timestamp > %u GROUP BY nick",
		message->chan, timestamp
	);
	
	cnick = 0;
	lines = 0;
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			cnick++;
			lines += sqlite3_column_int(stmt, 0);
		}
	}
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	sqlquery = sqlite3_mprintf(
		"SELECT COUNT(*) FROM url "
		"WHERE chan = '%q' "
		"  AND time > %u",
		message->chan, timestamp
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW)
			count = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
			
	zsnprintf(msg, "Today: %d urls and %d lines for %d nicks", count, lines, cnick);
	irc_privmsg(message->chan, msg);	
}
