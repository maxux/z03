/* z03 - services like nickserv, ...
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
#include "bot.h"
#include "core_init.h"
#include "lib_database.h"
#include "lib_core.h"
#include "lib_chart.h"
#include "lib_actions_services.h"

void action_notes(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery, *msg;
	int row, count = 0;
	
	if(!*args || !(msg = strchr(args, ' '))) {
		irc_privmsg(message->chan, "Missing arguments");
		return;
	}
	
	*(msg++) = '\0';
	
	sqlquery = sqlite3_mprintf(
		"SELECT COUNT(id) FROM notes "
		"WHERE chan = '%q' AND tnick = '%q' AND seen = 0", 
		message->chan, args
	);
	
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
			if((count = sqlite3_column_int(stmt, 0)) >= MAX_NOTES)
				irc_privmsg(message->chan, "Message queue full");
		}
	
	} else fprintf(stderr, "[-] Action/URL: SQL Error\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	if(count >= MAX_NOTES)
		return;
	
	/* Inserting */
	sqlquery = sqlite3_mprintf(
		"INSERT INTO notes (fnick, tnick, chan, message, seen, ts) "
		"VALUES ('%q', '%q', '%q', '%q', 0, %u)",
		message->nick, args, message->chan, msg, time(NULL)
	);
	
	if(db_simple_query(sqlite_db, sqlquery)) {
		irc_privmsg(message->chan, "Message saved");
		
	} else irc_privmsg(message->chan, "Cannot sent your message. Try again later.");
	
	sqlite3_free(sqlquery);
}
