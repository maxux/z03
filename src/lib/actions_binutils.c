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
#include "../common/bot.h"
#include "../core/init.h"
#include "core.h"
#include "actions.h"
#include "ircmisc.h"
#include "actions_binutils.h"

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

__registrar actions_binutils() {
	request_register(&__action_time);
	request_register(&__action_uptime);
	request_register(&__action_rand);
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
