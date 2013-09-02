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
#include "list.h"

//
// registering commands
//

static request_t __action_help = {
	.match    = ".help",
	.callback = action_help,
	.man      = "print the list of all the commands available",
	.hidden   = 0,
	.syntaxe  = "",
};

static request_t __action_man = {
	.match    = ".man",
	.callback = action_man,
	.man      = "print 'man page' of a given bot command",
	.hidden   = 0,
	.syntaxe  = ".man <command name> (without prefix)",
};

__registrar actions_root() {
	request_register(&__action_help);
	request_register(&__action_man);
}

//
// commands implementation
//

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
	request_t *request;
	char list[512], buffer[768];
	unsigned int length;
	(void) args;
	
	// clearing list
	list[0] = '\0';
	length  = 0;
	
	list_foreach(global_lib.commands, node) {
		request = (request_t *) node->data;
	
		if(!request->hidden) {
			strcat(list, request->match);
			strcat(list, " ");
			length += strlen(request->match);
		}
		
		if(length >= 60 || !node->next) {
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
	request_t *request;
	char buffer[512];
	
	if(!action_parse_args(message, args))
		return;
	
	list_foreach(global_lib.commands, node) {
		request = (request_t *) node->data;
		
		if(!strcmp(args, request->match + 1) && !request->hidden) {
			zsnprintf(buffer, "%s: %s", args, request->man);
			irc_privmsg(message->chan, buffer);
			
			if(*(request->syntaxe)) {
				zsnprintf(buffer, "Syntaxe: %s", request->syntaxe);
				irc_privmsg(message->chan, buffer);
			}
			
			return;
		}
	}
}
