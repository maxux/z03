/* z03 - small bot with some network features - irc channel bot actions
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
// #include <unistd.h>
#include "bot.h"
#include "core_init.h"
#include "lib_core.h"
#include "lib_database.h"
#include "lib_list.h"
#include "lib_urlmanager.h"
#include "lib_actions.h"
#include "lib_chart.h"
#include "lib_ircmisc.h"
#include "lib_weather.h"
#include "lib_somafm.h"
#include "lib_google.h"
#include "lib_run.h"
#include "lib_wiki.h"
#include "lib_lastfm.h"
#include "lib_settings.h"

int action_parse_args(ircmessage_t *message, char *args) {
	if(!*args) {
		irc_privmsg(message->chan, "Missing arguments");
		return 0;
		
	} else short_trim(args);
	
	return 1;
}

void action_help(ircmessage_t *message, char *args) {
	char list[1024];
	unsigned int i;
	(void) args;
	
	sprintf(list, "PRIVMSG %s :Commands: ", message->chan);
	
	for(i = 0; i < __request_count; i++) {
		strcat(list, __request[i].match);
		strcat(list, " ");
	}
	
	raw_socket(sockfd, list);
}
