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
#include "known.h"
#include "actions_known.h"

//
// registering commands
//

static request_t __action_known = {
	.match    = ".known",
	.callback = action_known,
	.man      = "parse regexp and show which known nick match",
	.hidden   = 0,
	.syntaxe  = ".known <regexp>",	
};

__registrar actions_known() {
	request_register(&__action_known);
}

//
// commands implementation
//

void action_known(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery, *parsed, *implode;
	const unsigned char *nick;
	list_t *nicklist;
	
	if(!action_parse_args(message, args))
		return;
	
	sqlquery = sqlite3_mprintf(
		"SELECT nick FROM stats                 "
		"WHERE nick REGEXP '%q' AND chan = '%q' ",
		args, message->chan
	);
	
	nicklist = list_init(NULL);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] action/count: sql error\n");
	
	while(sqlite3_step(stmt) == SQLITE_ROW) {
		nick = sqlite3_column_text(stmt, 0);		
		parsed = anti_hl_alloc((char *) nick);
		list_append(nicklist, parsed, parsed);
	}
	
	implode = list_implode(nicklist, 10);
	irc_privmsg(message->chan, implode);
	
	list_free(nicklist);
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}
