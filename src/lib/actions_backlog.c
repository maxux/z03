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
#include "chart.h"
#include "ircmisc.h"
#include "actions_backlog.h"

//
// registering commands
//

static request_t __action_backlog = {
	.match    = ".backlog",
	.callback = action_backlog,
	.man      = "print last lines",
	.hidden   = 0,
	.syntaxe  = ".backlog, .backlog <nick>",
};

__registrar actions_backlog() {
	request_register(&__action_backlog);
}

//
// commands implementation
//

void action_backlog(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt1, *stmt2;
	char *sqlquery1, *sqlquery2, *msg;
	const unsigned char *row_nick, *row_msg;
	time_t row_time;
	struct tm * timeinfo;
	char date[128], *log_nick;
	char temp[256];
	unsigned int nrows, found = 0;
	size_t len, row;
	
	if(strlen((args = action_check_args(args)))) {
		sqlquery1 = NULL;
		
		sqlquery2 = sqlite3_mprintf(
			"SELECT nick, timestamp, message FROM "
			"  (SELECT nick, timestamp, message FROM logs "
			"   WHERE chan = '%q' AND nick = '%q' "
			"   ORDER BY id DESC LIMIT 4) "
			"ORDER BY timestamp ASC",
			message->chan, args
		);
				   
	} else {
		/* if(time(NULL) - (60 * 10) < message->channel->last_backlog_request) {
			irc_privmsg(message->chan, "Avoiding flood, bitch !");
			return;
			
		} else message->channel->last_backlog_request = time(NULL); */
		
		sqlquery1 = sqlite3_mprintf(
			"   SELECT COUNT(*) FROM logs               "
			"   WHERE chan = '%q' AND timestamp > (     "
			"      SELECT MAX(timestamp)                "
			"      FROM logs                            "
			"      WHERE chan = '%q' AND nick = '%q'    "
			"      AND timestamp < %d                   "
			"   )                                       ",
			message->chan, message->chan, message->nick, time(NULL) - 2
		);
		
		sqlquery2 = sqlite3_mprintf(
			"SELECT nick, timestamp, message FROM (     "
			"   SELECT nick, timestamp, message         "
			"   FROM logs                               "
			"   WHERE chan = '%q'                       "
			"   ORDER BY timestamp DESC LIMIT 20        "
			") ORDER BY timestamp ASC                   ",
			message->chan, message->chan, message->nick, time(NULL) - 2
		);
	}
	
	if((stmt2 = db_sqlite_select_query(sqlite_db, sqlquery2))) {
		if(sqlquery1) {
			if((stmt1 = db_sqlite_select_query(sqlite_db, sqlquery1))) {
				nrows = sqlite3_column_int(stmt1, 0);
				if(nrows > 1) {
					zsnprintf(temp, "%s: %u lines since last talk, noticing last 20 lines...", 
							message->nickhl, nrows);

				} else zsnprintf(temp, "%s: noticing last 20 lines...",  message->nickhl);
				
				irc_privmsg(message->chan, temp);
				
			} else fprintf(stderr, "[-] backlog: cannot select\n");
			
			sqlite3_finalize(stmt1);
			sqlite3_free(sqlquery1);
		}
	
		while((row = sqlite3_step(stmt2)) == SQLITE_ROW) {
			found = 1;
			
			row_nick  = sqlite3_column_text(stmt2, 0);
			row_time  = sqlite3_column_int(stmt2, 1);
			row_msg   = sqlite3_column_text(stmt2, 2);
			
			/* Date formating */
			timeinfo = localtime(&row_time);
			strftime(date, sizeof(date), "%d/%m/%Y %X", timeinfo);
			
			log_nick = (char *) malloc(sizeof(char) * strlen((char *) row_nick) + 4);
			strcpy(log_nick, (char *) row_nick);
			
			len = strlen(log_nick) * strlen((char *) row_msg) + 64;
			msg = (char *) malloc(sizeof(char) * len + 1);
				
			snprintf(msg, len, "[%s] <%s> %s", date, anti_hl(log_nick), row_msg);
			
			if(sqlquery1)
				irc_notice(message->nick, msg);
				
			else irc_privmsg(message->chan, msg);
			
			free(log_nick);
			free(msg);
		}
	
	} else fprintf(stderr, "[-] URL Parser: cannot select logs\n");
	
	if(!found) {
		zsnprintf(temp, "No match found for <%s>", args);
		irc_privmsg(message->chan, temp);
	}
	
	/* Clearing */
	sqlite3_finalize(stmt2);
	sqlite3_free(sqlquery2);
}
