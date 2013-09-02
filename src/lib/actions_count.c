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
#include "actions_count.h"

//
// registering commands
//

static request_t __action_count = {
	.match    = ".count",
	.callback = action_count,
	.man      = "print some statistics about someone",
	.hidden   = 0,
	.syntaxe  = ".count, .count <nick>",	
};

__registrar actions_count() {
	request_register(&__action_count);
}

//
// commands implementation
//

void action_count(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery, *nick, nicked[64];
	char output[256];
	int words, lines, twords, tlines;
	time_t since;
	struct tm * timeinfo;
	char found = 0, date[128];
	
	if(!strlen((args = action_check_args(args))))
		nick = message->nick;
		
	else nick = args;
	
	sqlquery = sqlite3_mprintf(
		"SELECT SUM(words), SUM(lines),             "
		"    (SELECT SUM(words) FROM stats          "
		"    WHERE chan = '%q') as twords,          "
		"    (SELECT COUNT(*) FROM logs             "
		"    WHERE chan = '%q') as tlines,          "
		"    (SELECT timestamp FROM logs            "
		"    WHERE chan = '%q' AND nick REGEXP '%q' "
		"    ORDER BY timestamp ASC LIMIT 1) as si  "
		"FROM stats                                 "
		"WHERE nick REGEXP '%q' AND chan = '%q'      ",
		message->chan, message->chan, message->chan, nick, nick, message->chan
	);
	                           
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] action/count: sql error\n");
	
	while(sqlite3_step(stmt) == SQLITE_ROW) {
		words  = sqlite3_column_int(stmt, 0);
		
		if(words) {
			found = 1;
			
		} else break;
			
		lines  = sqlite3_column_int(stmt, 1);
		twords = sqlite3_column_int(stmt, 2);
		tlines = sqlite3_column_int(stmt, 3);
		since  = (time_t) sqlite3_column_int(stmt, 4);
		
		timeinfo = localtime(&since);
		strftime(date, sizeof(date), "%d/%m/%Y", timeinfo);
		
		printf("[ ] action/count: %d / %d / %d / %d / %lu\n", words, lines, twords, tlines, since);
		
		// avoid divide by zero
		if(tlines && twords) {
			if(*args) {
				strcpy(nicked, args);
				anti_hl(nicked);
				
			} else strcpy(nicked, message->nickhl);
		
			zsnprintf(output,
			          "%s: %d (%.2f%% of %d) lines and %d (%.2f%% of %d) words (avg: %.2f words per lines) since %s",
				  nicked,
				  lines, ((float) lines / tlines) * 100, tlines,
				  words, ((float) words / twords) * 100, twords,
				  ((float) words / lines),
				  date
			);
			                  
			irc_privmsg(message->chan, output);
		}
	}
	
	if(!found) {
		zsnprintf(output, "No match found for <%s>", nick);
		irc_privmsg(message->chan, output);
	}
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}
