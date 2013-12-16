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
#include "actions_url.h"

//
// registering commands
//

static request_t __action_url = {
	.match    = ".url",
	.callback = action_url,
	.man      = "search on url database, by url or title. Use % as wildcard",
	.hidden   = 0,
	.syntaxe  = ".url <keywords> (use % as wildcard)",	
};

__registrar actions_url() {
	request_register(&__action_url);
}

//
// commands implementation
//

void action_url(ircmessage_t *message, char *args) {
	char *sqlquery;
	
	if(!action_parse_args(message, args))
		return;
	
	sqlquery = sqlite3_mprintf(
		"SELECT url, title, nick, time "
		"FROM url WHERE (url LIKE '%%%q%%' OR title LIKE '%%%q%%') "
		"           AND chan = '%q' "
		"ORDER BY time DESC LIMIT 5",
		args, args, message->chan
	);
	
	url_format_log(sqlquery, message);
	sqlite3_free(sqlquery);
}
