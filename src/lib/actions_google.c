/* z03 - webservices like weather, radio station, google, wikipedia, ...
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
#include "shurl.h"
#include "google.h"
#include "actions_google.h"

//
// registering commands
//

static request_t __action_goo = {
	.match    = ".goo",
	.callback = action_google,
	.man      = "search on Google, print the first result",
	.hidden   = 0,
	.syntaxe  = ".goo <keywords>",
};

static request_t __action_google = {
	.match    = ".google",
	.callback = action_google,
	.man      = "search on Google, print the 3 firsts result",
	.hidden   = 0,
	.syntaxe  = ".google <keywords>",
};

static request_t __action_calc = {
	.match    = ".calc",
	.callback = action_calc,
	.man      = "google calculator query",
	.hidden   = 0,
	.syntaxe  = ".calc <query>",
};

__registrar actions_google() {
	request_register(&__action_goo);
	request_register(&__action_google);
	request_register(&__action_calc);
}

//
// commands implementation
//

void action_google(ircmessage_t *message, char *args) {
	unsigned int max;
	char msg[1024], *url;
	google_search_t *google;
	unsigned int i;
	
	if(!action_parse_args(message, args))
		return;
	
	if(!(google = google_search(args))) {
		irc_privmsg(message->chan, "Cannot download request, please try again");
		return;
	}
	
	if(google->length) {
		if(!strncmp(message->command, ".google", 7))
			max = (google->length < 3) ? google->length : 3;
		
		else max = 1;
		
		for(i = 0; i < max; i++) {
			if(!(url = shurl(google->result[i].url)))
				url = google->result[i].url;
			
			zsnprintf(msg, "%u) %s | %s", i + 1, google->result[i].title, url);
			irc_privmsg(message->chan, msg);
			free(url);
		}
		
	} else irc_privmsg(message->chan, "No result");
	
	google_free(google);
}

void action_calc(ircmessage_t *message, char *args) {
	char *answer;
	
	if(!action_parse_args(message, args))
		return;
	
	if((answer = google_calc(args))) {
		irc_privmsg(message->chan, answer);
		free(answer);
		
	} else irc_privmsg(message->chan, "No result from Google Calculator");
}
