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
// #include "ircmisc.h"
#include "google.h"
#include "wiki.h"
#include "actions_wiki.h"

//
// registering commands
//

static request_t __action_wi = {
	.match    = ".wi",
	.callback = action_wiki,
	.man      = "summary a wiki's international article",
	.hidden   = 0,
	.syntaxe  = ".wi <lang> <keywords>",
};

static request_t __action_wiki = {
	.match    = ".wiki",
	.callback = action_wiki,
	.man      = "summary an english wikipedia's article",
	.hidden   = 0,
	.syntaxe  = ".wiki <keywords>",
};

__registrar actions_wiki() {
	request_register(&__action_wi);
	request_register(&__action_wiki);
}

//
// commands implementation
//

void action_wiki(ircmessage_t *message, char *args) {
	google_search_t *google;
	char lang[64] = {0};
	char *data = NULL, *match;
	char reply[1024];
	size_t matchlen;
	
	if(!action_parse_args(message, args))
		return;
	
	/* Wiki International */
	if(!strncmp(message->command, ".wi ", 4)) {
		if(!(match = strchr(args, ' ')) || (match - args >= (signed) sizeof(lang))) {
			irc_privmsg(message->chan, "Wiki Intl: wrong arguments");
			return;
		}
		
		matchlen = match - args;
		
		strncpy(lang, args, matchlen);
		lang[matchlen] = '\0';
		
		args += matchlen + 1;
		
	} else strcpy(lang, "en");
	
	/* Using 'reply' for request and answer */
	printf("[+] Wikipedia: request (lang: %s): %s\n", lang, args);
	
	zsnprintf(reply, "site:%s.wikipedia.org %s", lang, args);
	google = google_search(reply);
	
	if(google->length) {
		if((data = wiki_head(google->result[0].url))) {			
			if(strlen(data) > 290) {
				match = data + 280;
				while(*match && *match != ' ')
					match--;
					
				strcpy(match, " [...]");
			}
			
			zsnprintf(reply, "Wiki (%s): %s [%s]",
			                 lang, data, google->result[0].url);
			irc_privmsg(message->chan, reply);
			
		} else irc_privmsg(message->chan, "Wiki: cannot grab data from wikipedia");		
	} else irc_privmsg(message->chan, "Wiki: not found");
	
	free(data);
	google_free(google);
}
