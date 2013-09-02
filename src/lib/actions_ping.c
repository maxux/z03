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
#include <netdb.h>
#include "../common/bot.h"
#include "../core/init.h"
#include "database.h"
#include "core.h"
#include "actions.h"
#include "actions_ping.h"

//
// registering commands
//

static request_t __action_ping = {
	.match    = ".ping",
	.callback = action_ping,
	.man      = "ping request/reply",
	.hidden   = 0,
	.syntaxe  = "",
};

__registrar actions_ping() {
	request_register(&__action_ping);
}

//
// commands implementation
//

void action_just_ping(ircmessage_t *message, char *args) {
	time_t t;
	struct tm *timeinfo;
	char buffer[128];
	(void) args;
	
	time(&t);
	timeinfo = localtime(&t);
	
	zsnprintf(buffer, "Pong. Ping request received at %02d:%02d:%02d.",
	                  timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	
	irc_privmsg(message->chan, buffer);
}

void action_ping(ircmessage_t *message, char *args) {
	struct hostent *he;
	char cmdline[1024], buffer[512];
	FILE *fp;
	
	if(!strlen((args = action_check_args(args)))) {
		action_just_ping(message, args);
		return;
	}		
	
	if((he = gethostbyname(args)) == NULL) {
		zsnprintf(buffer, "Cannot resolve host %s", args);
		irc_privmsg(message->chan, buffer);
		return;
	}
	
	// 
	zsnprintf(cmdline, "ping -W1 -c 3 -i 0.2 %s | tail -2", args);
	if(!(fp = popen(cmdline, "r"))) {
		perror("[-] popen");
		return;
	}
	
	while(fgets(buffer, sizeof(buffer), fp))
		irc_privmsg(message->chan, buffer);
	
	fclose(fp);
}
