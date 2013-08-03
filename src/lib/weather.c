/* z03 - small bot with some network features - irc miscallinious functions (anti highlights, ...)
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
#include "weather.h"
#include "core.h"
#include "urlmanager.h"
#include "actions.h"
#include "ircmisc.h"

void weather_error(char *chan, char *message) {
	irc_privmsg(chan, message);
}

void weather_print(char *chan, weather_data_t *weather) {
	char temp[1024];
	
	// building response	
	zsnprintf(
		temp,
		"%s: temperature: %.1f°C, humidity: %.1f%% [%s]",
		weather->location, weather->temp, weather->humidity, weather->date
	);
	
	irc_privmsg(chan, temp);
}
