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
#include "bot.h"
#include "core_init.h"
#include "lib_database.h"
#include "lib_core.h"
#include "lib_actions.h"
#include "lib_ircmisc.h"
#include "lib_weather.h"
#include "lib_somafm.h"
#include "lib_google.h"
#include "lib_wiki.h"
#include "lib_settings.h"
#include "lib_actions_webservices.h"

void action_weather(ircmessage_t *message, char *args) {
	char cmdline[256], *list, *value;
	int id;
	
	/* Checking arguments */
	if(*args) {
		/* Building List */
		if(!strcmp(args, "list")) {
			list = weather_station_list();
			if(!list)
				return;
				
			zsnprintf(cmdline, "Stations list: %s (Default: %s)",
			                   list, weather_stations[weather_default_station].ref);
			irc_privmsg(message->chan, cmdline);
			
			free(list);
			
			return;
		}
		
		/* Searching station */
		id = weather_get_station(args);
		
	} else if((value = settings_get(message->nick, "weather"))) {
			id = weather_get_station(value);
			free(value);
			
	} else id = weather_default_station;
	
	weather_handle(message->chan, (weather_stations + id));
}

void action_somafm(ircmessage_t *message, char *args) {
	char cmdline[256], *list;
	unsigned int i;
	int id = 0;	// default
	
	/* Checking arguments */
	if(*args) {
		/* Building List */
		if(!strcmp(args, "list")) {
			list = somafm_station_list();
			if(!list)
				return;
				
			zsnprintf(cmdline, "Stations list: %s (Default: %s)",
			                   list, somafm_stations[id].ref);
			irc_privmsg(message->chan, cmdline);
			
			free(list);
			
			return;
		}
		
		/* Searching station */
		for(i = 0; i < somafm_stations_count; i++) {
			if(!strncmp(args, somafm_stations[i].ref, strlen(somafm_stations[i].ref))) {
				id = i;
				break;
			}
		}
	}
	
	somafm_handle(message->chan, (somafm_stations + id));
}


void action_wiki(ircmessage_t *message, char *args) {
	google_search_t *google;
	char lang[64] = {0};
	char *data = NULL, *match;
	char reply[1024];
	size_t matchlen;
	
	if(!action_parse_args(message, args))
		return;
	
	/* Wiki International */
	if(!strncmp(message->command, ".wiki", 5)) {
		if(!(match = strchr(args, ' ')) || (match - args >= (signed) sizeof(lang))) {
			irc_privmsg(message->chan, "Wiki Intl: wrong arguments");
			return;
		}
		
		matchlen = match - args;
		
		strncpy(lang, args, matchlen);
		lang[matchlen] = '\0';
		
		args += matchlen + 1;
		
	} else strcpy(lang, "en");
	
	/* Using 'reply' for request and answer */
	printf("[+] Wikipedia: request (lang: %s): %s\n", lang, args);
	
	zsnprintf(reply, "site:%s.wikipedia.org %s", lang, args);
	google = google_search(reply);
	
	if(google->length) {
		if((data = wiki_head(google->result[0].url))) {			
			if(strlen(data) > 290)
				strcpy(data + 280, " [...]");
			
			zsnprintf(reply, "Wiki (%s): %s [%s]",
			                 lang, data, google->result[0].url);
			irc_privmsg(message->chan, reply);
			
		} else irc_privmsg(message->chan, "Wiki: cannot grab data from wikipedia");		
	} else irc_privmsg(message->chan, "Wiki: not found");
	
	free(data);
	google_free(google);
}

void action_google(ircmessage_t *message, char *args) {
	unsigned int max;
	char msg[1024], *url;
	google_search_t *google;
	unsigned int i;
	
	if(!action_parse_args(message, args))
		return;
	
	google = google_search(args);	
	if(google->length) {
		if(!strncmp(message->command, ".google", 7))
			max = (google->length < 3) ? google->length : 3;
		
		else max = 1;
		
		for(i = 0; i < max; i++) {
			if(!(url = shurl(google->result[i].url)))
				url = google->result[i].url;
			
			zsnprintf(msg, "%u) %s | %s", i + 1, google->result[i].title, url);
			irc_privmsg(message->chan, msg);
			free(url);
		}
		
	} else irc_privmsg(message->chan, "No result");
	
	google_free(google);
}
