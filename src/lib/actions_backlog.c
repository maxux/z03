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
	.man      = "notice last lines from chan/someone ('public' flag send text to channel)",
	.hidden   = 0,
	.syntaxe  = ".backlog, .backlog <nick> [public]",
};

__registrar actions_backlog() {
	request_register(&__action_backlog);
}

//
// commands implementation
//

void action_backlog(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery, *msg, *match, *asknick = NULL;
	const unsigned char *row_nick, *row_msg;
	time_t row_time;
	struct tm * timeinfo;
	char date[128], *log_nick;
	char temp[256];
	unsigned int found = 0;
	size_t len, row;
	char public = 0;
	
	if(strlen((args = action_check_args(args)))) {
		if((match = strchr(args, ' '))) {
			if(!strcasecmp(match + 1, "public"))
				public = 1;
			
			// isolating nick
			asknick = strndup(args, match - args);
		
		// duplicate full-args		
		} else asknick = strdup(args);
		
		sqlquery = sqlite3_mprintf(
			"SELECT nick, timestamp, message FROM "
			"  (SELECT nick, timestamp, message FROM logs "
			"   WHERE chan = '%q' AND nick = '%q' "
			"   ORDER BY id DESC LIMIT 10) "
			"ORDER BY timestamp ASC",
			message->chan, asknick
		);
				   
	} else {
		sqlquery = sqlite3_mprintf(
			"SELECT nick, timestamp, message, id FROM (   "
			"   SELECT nick, timestamp, message, id       "
			"   FROM logs                                 "
			"   WHERE chan = '%q'                         "
			"   ORDER BY timestamp DESC, id DESC LIMIT 20 "
			") ORDER BY timestamp ASC, id ASC             ",
			message->chan, message->chan, message->nick, time(NULL) - 2
		);
	}
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) == SQLITE_ROW) {
			found = 1;
			
			row_nick  = sqlite3_column_text(stmt, 0);
			row_time  = sqlite3_column_int(stmt, 1);
			row_msg   = sqlite3_column_text(stmt, 2);
			
			/* Date formating */
			timeinfo = localtime(&row_time);
			strftime(date, sizeof(date), "%d/%m/%Y %X", timeinfo);
			
			log_nick = (char *) malloc(sizeof(char) * strlen((char *) row_nick) + 4);
			strcpy(log_nick, (char *) row_nick);
			
			len = strlen(log_nick) * strlen((char *) row_msg) + 64;
			msg = (char *) malloc(sizeof(char) * len + 1);
				
			snprintf(msg, len, "[%s] <%s> %s", date, anti_hl(log_nick), row_msg);
			
			if(public)
				irc_privmsg(message->chan, msg);
				
			else irc_notice(message->nick, msg);
			
			free(log_nick);
			free(msg);
		}
	
	} else fprintf(stderr, "[-] URL Parser: cannot select logs\n");
	
	if(!found) {
		zsnprintf(temp, "No match found for <%s>", (asknick) ? asknick : args);
		irc_privmsg(message->chan, temp);
	}
	
	if(found && !public) {
		zsnprintf(temp, "Backlog noticed");
		irc_privmsg(message->chan, temp);
	}
	
	/* Clearing */
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
}
