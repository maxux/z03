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
#include "actions_bored.h"

//
// registering commands
//

static request_t __action_bored = {
	.match    = ".bored",
	.callback = action_bored,
	.man      = "pick randomly an url into the database. Have fun!",
	.hidden   = 0,
	.syntaxe  = ".bored",	
};

__registrar actions_bored() {
	request_register(&__action_bored);
}

//
// commands implementation
//

void action_bored(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char *output = NULL, *title, *url, *nick;
	time_t time;
	struct tm * timeinfo;
	char date[128], *url_nick;
	int row, len;
	char *titlehl;
	
/*	if(!action_parse_args(message, args))
		return;
*/

	sqlquery = sqlite3_mprintf(
		"SELECT url, title, nick, time "
		"FROM url WHERE chan = '%q' "
		"ORDER BY RANDOM() LIMIT 1",
		message->chan
	);

	//	Faire une fonction qui formatte la sortie, ca éviterais de réutiliser deux fois le même code

	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {

		while((row = sqlite3_step(stmt)) == SQLITE_ROW) {	//	TODO: the loop here seem useless (only one answer)
			url   = (char *) sqlite3_column_text(stmt, 0);
			title = (char *) sqlite3_column_text(stmt, 1);
			nick  = (char *) sqlite3_column_text(stmt, 2);
			time  = (time_t) sqlite3_column_int(stmt, 3);
			
			timeinfo = localtime(&time);
			strftime(date, sizeof(date), "%d/%m/%Y %X", timeinfo);
			
			if(!title)
				title = "Unknown title";				
			
			url_nick = (char *) malloc(sizeof(char) * strlen((char *) nick) + 4);
			strcpy(url_nick, nick);
			
			if(!(titlehl = anti_hl_each_words(title, strlen(title), UTF_8)))
				continue;
				
			len = strlen(url) + strlen(title) + strlen(nick) + 128;
			output = (char *) malloc(sizeof(char) * len);
			
			snprintf(output, len, "[%s] <%s> %s | %s", date, anti_hl(url_nick), url, titlehl);
			irc_privmsg(message->chan, output);
			
			free(titlehl);
			free(output);
			free(url_nick);
		}
	
	} else fprintf(stderr, "[-] Action/URL: SQL Error\n");
	
	if(!output)
		irc_privmsg(message->chan, "No match found");
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}
