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
#include "bot.h"
#include "core_init.h"
#include "lib_core.h"
#include "lib_actions.h"
#include "lib_ircmisc.h"


int action_parse_args(ircmessage_t *message, char *args) {
	if(!*args) {
		irc_privmsg(message->chan, "Missing arguments");
		return 0;
		
	} else short_trim(args);
	
	return 1;
}

void action_help(ircmessage_t *message, char *args) {
	char list[512], buffer[768];
	unsigned int i;
	(void) args;
	
	// clearing list
	list[0] = '\0';
	
	for(i = 0; i < __request_count; i++) {
		if(!__request[i].hidden) {
			strcat(list, __request[i].match);
			strcat(list, " ");
		}

		if((!(i % 15) && i > 0) || i == __request_count - 1) {
			zsnprintf(buffer, "Commands: %s", list);
			irc_privmsg(message->chan, buffer);

			// reset list
			list[0] = '\0';
		}
	}
}

void action_man(ircmessage_t *message, char *args) {
	char buffer[512];
	unsigned int i;

	if(!action_parse_args(message, args))
		return;
	
	for(i = 0; i < __request_count; i++) {
		if(match_prefix(args, __request[i].match + 1) && !__request[i].hidden) {
			zsnprintf(buffer, "%s: %s", args, __request[i].man);
			irc_privmsg(message->chan, buffer);
			return;
		}
	}
}
