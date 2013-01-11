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
#include "bot.h"
#include "core_init.h"
#include "lib_database.h"
#include "lib_core.h"
#include "lib_actions.h"
// #include "lib_ircmisc.h"
#include "lib_settings.h"
#include "lib_actions_settings.h"

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
	
	settings_set(message->nick, key, match + 1);
	
	zsnprintf(answer, "%s.%s = %s", message->nickhl, key, match + 1);
	irc_privmsg(message->chan, answer);
	
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
