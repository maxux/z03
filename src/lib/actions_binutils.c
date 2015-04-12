/* z03 - irc "binutils" (ping, time, ...) basic commands
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "../common/bot.h"
#include "../core/init.h"
#include "core.h"
#include "actions.h"
#include "ircmisc.h"
#include "actions_binutils.h"
#include "geoip.h"

//
// registering commands
//

static request_t __action_time = {
	.match    = ".time",
	.callback = action_time,
	.man      = "print the time",
	.hidden   = 0,
	.syntaxe  = "",
};

static request_t __action_uptime = {
	.match    = ".uptime",
	.callback = action_uptime,
	.man      = "print the bot's uptime",
	.hidden   = 0,
	.syntaxe  = "",
};

static request_t __action_rand = {
	.match    = ".rand",
	.callback = action_random,
	.man      = "random number generator",
	.hidden   = 0,
	.syntaxe  = ".rand, .rand <max>, .rand <min> <max>",
};

static request_t __action_nc = {
	.match    = ".nc",
	.callback = action_nc,
	.man      = "raw socket single line test",
	.hidden   = 0,
	.syntaxe  = ".nc host port [data]",
};

#ifdef ENABLE_GEOIP
static request_t __action_geoip = {
	.match    = ".geoip",
	.callback = action_geoip,
	.man      = "geoip information",
	.hidden   = 0,
	.syntaxe  = ".geoip <ip address>",
};
#endif

__registrar actions_binutils() {
	request_register(&__action_time);
	request_register(&__action_uptime);
	request_register(&__action_rand);
	request_register(&__action_nc);
	
	#ifdef ENABLE_GEOIP
	request_register(&__action_geoip);
	#endif
}

//
// commands implementation
//

void action_time(ircmessage_t *message, char *args) {
	time_t rawtime;
	struct tm *timeinfo;
	char date[128];
	(void) args;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(date, sizeof(date), TIME_FORMAT, timeinfo);	
	irc_privmsg(message->chan, date);
	// irc_privmsg(message->chan, "May the force be with you today...");
}

void action_uptime(ircmessage_t *message, char *args) {
	time_t now;
	char msg[256];
	char *uptime, *rehash;
	(void) args;
	
	now  = time(NULL);
		
	uptime = time_elapsed(now - global_core->startup_time);
	rehash = time_elapsed(now - global_core->rehash_time);
	
	zsnprintf(msg, "Bot uptime: %s (rehashed %d times, rehash uptime: %s)", 
	               uptime, global_core->rehash_count, rehash);
		
	free(uptime);
	free(rehash);
	
	irc_privmsg(message->chan, msg);
}

void action_random(ircmessage_t *message, char *args) {
	char buffer[512], *x;
	int random, min = 0, max = 100;
	
	if(strlen((args = action_check_args(args)))) {
		/* Min/Max or just Max */
		if((x = strrchr(args, ' '))) {
			printf("<%s> <%s>", args, x);
			min = (atoi(args) <= 0) ? 0   : atoi(args);
			max = (atoi(args) <= 0) ? 100 : atoi(x);
			
			if(max < min)
				intswap(&min, &max);
			
		} else max = (atoi(args) <= 0) ? 100 : atoi(args);
	}
	
	printf("[ ] Action/Random: min: %d, max: %d\n", min, max);
	
	random = (rand() % (max - min + 1) + min);
	
	zsnprintf(buffer, "Random (%d, %d): %d", min, max, random);
	irc_privmsg(message->chan, buffer);
}

#ifdef ENABLE_GEOIP
void action_geoip(ircmessage_t *message, char *args) {
	char *data;
	
	data = geoip(args);
	irc_privmsg(message->chan, data);
	
	free(data);
}
#endif

static int netcat(char *host, int port) {
	int sockfd;
	struct sockaddr_in addr_remote;
	struct hostent *hent;
	struct timeval timeout = {
		.tv_sec  = 5,
		.tv_usec = 0
	};

	// creating client socket
	addr_remote.sin_family = AF_INET;
	addr_remote.sin_port   = htons(port);
	
	// dns resolution
	if((hent = gethostbyname(host)) == NULL)
		return 0;
		
	memcpy(&addr_remote.sin_addr, hent->h_addr_list[0], hent->h_length);
	
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return 0;

	// connecting
	if(connect(sockfd, (const struct sockaddr *) &addr_remote, sizeof(addr_remote)) < 0)
		return 0;
	
	// setting timeout
	if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
		return 0;

	printf("[+] socket: connected\n");
	
	return sockfd;
}

void action_nc(ircmessage_t *message, char *args) {
	char buffer[64], response[768], *temp;
	char *host = NULL, *_port = NULL, *data = NULL;
	int port, sockfd, length, i, j;
	
	if(!action_parse_args(message, args))
		return;
	
	//
	// extracting arguments
	//
	if(!(host = string_index(args, 0)))
		return;
	
	if(!(_port = string_index(args, 1))) {
		free(host);
		return;
	}
	
	port = atoi(_port);
	
	// if data is filled
	if(strlen(args + strlen(host) + strlen(_port) + 2) > 0) {
		temp = args + strlen(host) + strlen(_port) + 2;
		
		// copy data to buffer, parsing some escape sequence
		for(i = 0, j = 0; *(temp + i); i++, j++) {
			if(temp[i] == '\\') {
				if(temp[i + 1] == 'n')
					buffer[j] = '\n';
				
				if(temp[i + 1] == 'r')
					buffer[j] = '\r';
				
				i++;
				
			} else buffer[j] = temp[i];
		}
		
		buffer[i] = '\0';
		data = buffer;
		
		printf("[+] socket: data: <%s>\n", buffer);
	}
	
	free(_port);
	
	//
	// connecting
	//
	if(!(sockfd = netcat(host, port))) {
		zsnprintf(response, "[-][%s:%d] %s", host, port, strerror(errno));
		irc_privmsg(message->chan, response);
		return;
	}
	
	//
	// sending data provided
	//
	if(data) {
		if(send(sockfd, data, strlen(data), 0) < 0) {
			zsnprintf(response, "[-][%s:%d] %s", host, port, strerror(errno));
			irc_privmsg(message->chan, response);
		}
	}
	
	//
	// reading socket
	//
	if((length = recv(sockfd, buffer, sizeof(buffer), 0)) >= 0) {
		buffer[length] = '\0';
		printf("[+] socket: buffer [length: %d]: <%s>\n", length, buffer);
		
		// keep only the first line
		if((temp = strchr(buffer, '\n')))
			*temp = '\0';
		
		zsnprintf(response, "[+][%s:%d] %s", host, port, buffer);
	
	} else zsnprintf(response, "[-][%s:%d] No data received (timeout)", host, port);
	
	irc_privmsg(message->chan, response);
	
	//
	// clearing
	//
	shutdown(sockfd, SHUT_RDWR);
	free(host);
}
