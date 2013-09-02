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
#include "../common/bot.h"
#include "../core/init.h"
#include "database.h"
#include "core.h"
#include "actions.h"
#include "ircmisc.h"
#include "weather.h"
#include "settings.h"
#include "weather_meteobe.h"
#include "weather_wunder.h"
#include "actions_weather.h"

//
// registering commands
//

static request_t __action_weather = {
	.match    = ".weather",
	.callback = action_weather,
	.man      = "print weather information from meteobelgique: .weather list, .weather [station]",
	.hidden   = 0,
	.syntaxe  = ".weather <station>, .weather list (setting variable: weather)",
};

static request_t __action_wunder = {
	.match    = ".wunder",
	.callback = action_wunder,
	.man      = "print weather information from wunderground",
	.hidden   = 0,
	.syntaxe  = ".wunder <country/city> (setting variable: wunder)",
};

__registrar actions_weather() {
	request_register(&__action_weather);
	request_register(&__action_wunder);
}

//
// commands implementation
//

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
				
			zsnprintf(
				cmdline,
				"Stations list: %s (Default: %s)",
				list,
				weather_stations[weather_default_station].ref
			);
			
			irc_privmsg(message->chan, cmdline);
			
			free(list);
			
			return;
		}
		
		/* Searching station */
		id = weather_get_station(args);
		
	} else if((value = settings_get(message->nick, "weather", PUBLIC))) {
			id = weather_get_station(value);
			free(value);
			
	} else id = weather_default_station;
	
	weather_handle(message->chan, (weather_stations + id));
}

void action_wunder(ircmessage_t *message, char *args) {
	char *value;
	curl_data_t *curl;
	
	if(!*args) {
		if(!(value = settings_get(message->nick, "wunder", PUBLIC))) {
			action_missing_args(message);
			return;
		}
			
	} else value = strdup(args);
	
	curl = curl_data_new();
	wunder_handle(message->chan, value, curl);
	curl_data_free(curl);	
	
	free(value);
}
