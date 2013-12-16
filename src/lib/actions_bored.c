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
#include "downloader.h"
#include "urlmanager.h"
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

static request_t __action_boiler = {
	.match    = ".br",
	.callback = action_boiler,
	.man      = "pick randomly a boiler-room-url into the database",
	.hidden   = 0,
	.syntaxe  = ".br",
};

__registrar actions_bored() {
	request_register(&__action_bored);
	request_register(&__action_boiler);
}

//
// commands implementation
//

void action_bored(ircmessage_t *message, char *args) {
	char *sqlquery;
	(void) args;
	
	sqlquery = sqlite3_mprintf(
		"SELECT url, title, nick, time "
		"FROM url WHERE chan = '%q' "
		"ORDER BY RANDOM() LIMIT 1",
		message->chan
	);
	
	url_format_log(sqlquery, message);
	sqlite3_free(sqlquery);
}

void action_boiler(ircmessage_t *message, char *args) {
	char *sqlquery;
	(void) args;

	sqlquery = sqlite3_mprintf(
		"SELECT url, title, nick, time       "
		"FROM url WHERE chan = '%q'          "
		" AND UPPER(title) LIKE '%%BOILER%%' "
		"ORDER BY RANDOM() LIMIT 1           ",
		message->chan
	);

	url_format_log(sqlquery, message);
	sqlite3_free(sqlquery);
}
