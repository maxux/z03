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
#include <arpa/inet.h>
#include <netdb.h>
#include "../common/bot.h"
#include "../core/init.h"
#include "core.h"
#include "actions.h"
#include "ircmisc.h"
#include "actions_binutils.h"

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

void action_time(ircmessage_t *message, char *args) {
	time_t rawtime;
	struct tm *timeinfo;
	char date[128];
	(void) args;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(date, sizeof(date), TIME_FORMAT, timeinfo);	
	irc_privmsg(message->chan, date);
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