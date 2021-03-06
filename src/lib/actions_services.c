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
#include "../common/bot.h"
#include "../core/init.h"
#include "database.h"
#include "core.h"
#include "chart.h"
#include "ircmisc.h"
#include "actions.h"
#include "actions_services.h"

//
// registering commands
//

static request_t __action_note = {
	.match    = ".note",
	.callback = action_notes,
	.man      = "leave a message to someone, will be sent when connecting.",
	.hidden   = 0,
	.syntaxe  = ".note, .note <nick> <message>",
};

static request_t __action_delay = {
	.match    = ".delay",
	.callback = action_delay,
	.man      = "alarm timed notification",
	.hidden   = 0,
	.syntaxe  = ".delay <1-2880 minutes> <message>",
};

__registrar actions_services() {
	request_register(&__action_note);
	request_register(&__action_delay);
}

//
// commands implementation
//

void __action_notes_checknew(char *chan, char *nick) {
	sqlite3_stmt *stmt;
	char output[1024], timestring[64];
	char *sqlquery, *fnick, *message, *host;
	struct tm * timeinfo;
	time_t ts;
	
	/* Checking notes */
	sqlquery = sqlite3_mprintf(
		"SELECT fnick, message, ts, host FROM notes     "
		"WHERE tnick = '%q' AND chan = '%q' AND seen = 0",
		nick, chan
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			fnick   = strdup((char *) sqlite3_column_text(stmt, 0));
			message = (char *) sqlite3_column_text(stmt, 1);
			host    = (char *) sqlite3_column_text(stmt, 3);
			
			if((ts = sqlite3_column_int(stmt, 2)) > 0) {
				timeinfo = localtime(&ts);
				sprintf(timestring, "%02d/%02d/%02d %02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon + 1, (timeinfo->tm_year + 1900 - 2000), timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
				
			} else strcpy(timestring, "unknown");
			
			fnick = (char *) realloc(fnick, (sizeof(char) * strlen(fnick) + 16));
			
			zsnprintf(output, "┌── [%s] %s (%s) sent you a message, %s", timestring, anti_hl(fnick), host, nick);
			irc_privmsg(chan, output);
			
			zsnprintf(output, "└─> %s: %s", nick, message);
			irc_privmsg(chan, output);
			
			free(fnick);
		}
		
		sqlite3_free(sqlquery);
		sqlite3_finalize(stmt);
		
		sqlquery = sqlite3_mprintf("UPDATE notes SET seen = 1 WHERE tnick = '%q' AND chan = '%q'", nick, chan);
		if(!db_sqlite_simple_query(sqlite_db, sqlquery))
			printf("[-] lib/notes: cannot mark as read\n");
	
	} else fprintf(stderr, "[-] lib/notes: sql error\n");
	
	/* Clearing */
	sqlite3_free(sqlquery);
}

void action_notes(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery, *msg;
	int row, count = 0;
	
	/* read our own notes */
	if(!*args) {
		__action_notes_checknew(message->chan, message->nick);
		return;
	}
	
	if(!(msg = strchr(args, ' '))) {
		action_missing_args(message);
		return;
	}
	
	*(msg++) = '\0';
	
	sqlquery = sqlite3_mprintf(
		"SELECT COUNT(id) FROM notes "
		"WHERE chan = '%q' AND tnick = '%q' AND seen = 0", 
		message->chan, args
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
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
		"INSERT INTO notes (fnick, tnick, chan, message, seen, ts, host) "
		"VALUES ('%q', '%q', '%q', '%q', 0, %u, '%q')",
		message->nick, args, message->chan, msg, time(NULL), message->host
	);
	
	if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
		irc_privmsg(message->chan, "Message saved");
		
	} else irc_privmsg(message->chan, "Cannot sent your message. Try again later.");
	
	sqlite3_free(sqlquery);
}

void action_delay(ircmessage_t *message, char *args) {
	char *msg, *sqlquery, date[128];
	time_t timestamp;
	int timevalue;
	struct tm *timeinfo;
	
	if(!action_parse_args(message, args))
		return;
	
	if(!(msg = strchr(args, ' ')) || (timevalue = atoi(args)) <= 0) {
		action_missing_args(message);
		return;
	}
	
	if(timevalue > 2880) {
		irc_privmsg(message->chan, "Delay out of range");
		return;
	}
	
	/* time checking and settings */
	timestamp = time(NULL) + (timevalue * 60);
	timeinfo = localtime(&timestamp);
	strftime(date, sizeof(date), "Alarm set to: " TIME_FORMAT, timeinfo);
	
	/* building sql query */
	sqlquery = sqlite3_mprintf(
		"INSERT INTO delay (timestamp, nick, chan, message, finished) "
		"VALUES ('%ld', '%q', '%q', '%q', 0)",
		timestamp, message->nick, message->chan, msg + 1
	);
	
	if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
		irc_privmsg(message->chan, date);
		
	} else irc_privmsg(message->chan, "Cannot program your alarm. Try again later.");
	
	sqlite3_free(sqlquery);
}
