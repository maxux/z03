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
#include <arpa/inet.h>
#include "../common/bot.h"
#include "../core/init.h"
#include "database.h"
#include "core.h"
#include "actions.h"
#include "actions_dns.h"

//
// registering commands
//

static request_t __action_dns = {
	.match    = ".dns",
	.callback = action_dns,
	.man      = "resolve a dns name address",
	.hidden   = 0,
	.syntaxe  = ".dns <hostname>",
};

static request_t __action_rdns = {
	.match    = ".rdns",
	.callback = action_rdns,
	.man      = "resolve a reverse dns address",
	.hidden   = 0,
	.syntaxe  = ".rdns <ipv4 address>",
};

__registrar actions_dns() {
	request_register(&__action_dns);
	request_register(&__action_rdns);
}

//
// commands implementation
//

void action_dns(ircmessage_t *message, char *args) {
	struct hostent *he;
	char buffer[256];
	char *ipbuf;
	
	if(!action_parse_args(message, args))
		return;
		
	if((he = gethostbyname(args)) == NULL) {
		zsnprintf(buffer, "Cannot resolve host %s", args);
		irc_privmsg(message->chan, buffer);
		return;
	}
	
	ipbuf = inet_ntoa(*((struct in_addr *) he->h_addr_list[0]));
	
	zsnprintf(buffer, "%s -> %s", args, ipbuf);
	irc_privmsg(message->chan, buffer);
}

void action_rdns(ircmessage_t *message, char *args) {
	struct hostent *he;
	char buffer[256];
	in_addr_t ip;
	
	if(!action_parse_args(message, args))
		return;
	
	if((ip = inet_addr(args)) == INADDR_NONE) {
		zsnprintf(buffer, "Invalid address <%s>", args);
		irc_privmsg(message->chan, buffer);
		return;
	}
		
	if((he = gethostbyaddr(&ip, sizeof(in_addr_t), AF_INET)) == NULL) {
		zsnprintf(buffer, "Cannot resolve host %s", args);
		irc_privmsg(message->chan, buffer);
		return;
	}
	
	zsnprintf(buffer, "%s -> %s", args, he->h_name);
	irc_privmsg(message->chan, buffer);
}
