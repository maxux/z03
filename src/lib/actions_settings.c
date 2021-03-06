/* z03 - keep user settings on database (preferences, id, ...)
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
#include "settings.h"
#include "actions_settings.h"

//
// registering commands
//

static request_t __action_set = {
	.match    = ".set",
	.callback = action_set,
	.man      = "set a variable value",
	.hidden   = 0,
	.syntaxe  = ".set <variable name> <value>",
};

static request_t __action_get = {
	.match    = ".get",
	.callback = action_get,
	.man      = "get a variable value",
	.hidden   = 0,
	.syntaxe  = ".get <nick> <variable name>, .get <variable name>",
};

static request_t __action_unset = {
	.match    = ".unset",
	.callback = action_unset,
	.man      = "unset a variable value",
	.hidden   = 0,
	.syntaxe  = ".unset <variable name>",
};

__registrar actions_settings() {
	request_register(&__action_set);
	request_register(&__action_get);
	request_register(&__action_unset);
}

//
// commands implementation
//

void action_set(ircmessage_t *message, char *args) {
	char *match, *key, answer[512];
	
	if(!action_parse_args(message, args))
		return;
	
	if(!(match = strchr(args, ' '))) {
		irc_privmsg(message->chan, "Wrong argument");
		return;
	}
	
	key = strdup(args);
	key[match - args] = '\0';
	
	if(settings_getview(message->nick, key) == PRIVATE) {
		zsnprintf(answer, "%s.%s is a private value", message->nickhl, key);
		
	} else {
		settings_set(message->nick, key, match + 1, PUBLIC);
		zsnprintf(answer, "%s.%s = %s", message->nickhl, key, match + 1);
	}
	
	
	irc_privmsg(message->chan, answer);
	
	free(key);
}

void action_get(ircmessage_t *message, char *args) {
	char *match, *key = NULL, *value, answer[512];
	
	if(!action_parse_args(message, args))
		return;
	
	// someone else
	if((match = strchr(args, ' '))) {
		key = strdup(args);
		key[match - args] = '\0';
		
		if((value = settings_get(key, match + 1, PUBLIC)))
			zsnprintf(answer, "%s.%s = %s", key, match + 1, value);
			
		else zsnprintf(answer, "%s.%s is not set", key, match + 1);
		
	// itself
	} else if((value = settings_get(message->nick, args, PUBLIC)))
		zsnprintf(answer, "%s.%s = %s", message->nickhl, args, value);
			
	else zsnprintf(answer, "%s.%s is not set", message->nickhl, args);
	
	irc_privmsg(message->chan, answer);
	free(value);
	free(key);
}


void action_unset(ircmessage_t *message, char *args) {
	char answer[512];
	
	if(!action_parse_args(message, args))
		return;
	
	settings_unset(message->nick, args);
	
	zsnprintf(answer, "%s.%s unset", message->nickhl, args);
	irc_privmsg(message->chan, answer);
}
