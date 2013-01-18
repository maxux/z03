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

void __action_notes_checknew(char *chan, char *nick) {
	sqlite3_stmt *stmt;
	char output[1024], timestring[64];
	char *sqlquery, *fnick, *message;
	struct tm * timeinfo;
	time_t ts;

	/* Checking notes */
	sqlquery = sqlite3_mprintf("SELECT fnick, message, ts FROM notes WHERE tnick = '%q' AND chan = '%q' AND seen = 0", nick, chan);
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			fnick   = (char *) sqlite3_column_text(stmt, 0);
			message = (char *) sqlite3_column_text(stmt, 1);

			if((ts = sqlite3_column_int(stmt, 2)) > 0) {
				timeinfo = localtime(&ts);
				sprintf(timestring, "%02d/%02d/%02d %02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon + 1, (timeinfo->tm_year + 1900 - 2000), timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

			} else strcpy(timestring, "unknown");

			snprintf(output, sizeof(output), "PRIVMSG %s :┌── [%s] %s sent a message to %s", chan, timestring, fnick, nick);
			raw_socket(output);

			snprintf(output, sizeof(output), "PRIVMSG %s :└─> %s", chan, message);
			raw_socket(output);
		}

		sqlite3_free(sqlquery);
		sqlite3_finalize(stmt);

		sqlquery = sqlite3_mprintf("UPDATE notes SET seen = 1 WHERE tnick = '%q' AND chan = '%q'", nick, chan);
		if(!db_simple_query(sqlite_db, sqlquery))
			printf("[-] lib/notes: cannot mark as read\n");

	} else fprintf(stderr, "[-] lib/notes: sql error\n");

	/* Clearing */
	sqlite3_free(sqlquery);
}

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
		while((row = sqlite3_step(stmt)) == SQLITE_ROW) {
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
