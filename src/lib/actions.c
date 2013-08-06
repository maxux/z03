/* z03 - actions root file
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
#include "core.h"
#include "actions.h"
#include "ircmisc.h"

char *action_check_args(char *args) {
	return ltrim(rtrim(args));
}

void action_missing_args(ircmessage_t *message) {
	char info[2048];
	
	zsnprintf(info, "Missing arguments. Command syntaxe: %s", message->request->syntaxe);
	irc_privmsg(message->chan, info);
}

char *action_parse_args(ircmessage_t *message, char *args) {
	if(!strlen(action_check_args(args))) {
		action_missing_args(message);
		return NULL;
	}
	
	return args;
}

void action_help(ircmessage_t *message, char *args) {
	char list[512], buffer[768];
	unsigned int i, length;
	(void) args;
	
	// clearing list
	list[0] = '\0';
	length  = 0;
	
	for(i = 0; i < __request_count; i++) {
		if(!__request[i].hidden) {
			strcat(list, __request[i].match);
			strcat(list, " ");
			length += strlen(__request[i].match);
		}
		
		if(length >= 60 || i == __request_count - 1) {
			if(*list) {
				zsnprintf(buffer, "Commands: %s", list);
				irc_privmsg(message->chan, buffer);
			}
			
			// reset list
			list[0] = '\0';
			length  = 0;
		}
	}
}

void action_man(ircmessage_t *message, char *args) {
	char buffer[512];
	unsigned int i;
	
	if(!action_parse_args(message, args))
		return;
	
	for(i = 0; i < __request_count; i++) {
		if(!strcmp(args, __request[i].match + 1) && !__request[i].hidden) {
			zsnprintf(buffer, "%s: %s", args, __request[i].man);
			irc_privmsg(message->chan, buffer);
			
			if(*(__request[i].syntaxe)) {
				zsnprintf(buffer, "Syntaxe: %s", __request[i].syntaxe);
				irc_privmsg(message->chan, buffer);
			}
			
			return;
		}
	}
}
