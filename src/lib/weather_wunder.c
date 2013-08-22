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
#include <string.h>
#include <jansson.h>
#include "../common/private.h"
#include "core.h"
#include "downloader.h"
#include "weather.h"
#include "weather_wunder.h"
#include "actions.h"
#include "ircmisc.h"

static json_t *wunder_json_load(char *json, size_t length) {
	json_t *root;
	json_error_t error;
	
	if(!(root = json_loadb(json, length, 0, &error))) {
		fprintf(stderr, "[-] wunder/json: parsing error: line %d: %s\n", error.line, error.text);
		return NULL;
	}
	
	if(!json_is_object(root)) {
		fprintf(stderr, "[-] wunder/json: root is not an object\n");
		return NULL;
	}
	
	return root;
}

static char *wunder_json_checkerror(json_t *root) {
	json_t *error;
	
	root = json_object_get(root, "response");
	if(!json_is_object(root)) {
		fprintf(stderr, "[-] wunder/json: response is not an object\n");
		return NULL;
	}
	
	/* Checking for error */
	error = json_object_get(root, "error");
	if(json_is_object(error)) {
		fprintf(stderr, "[-] wunder: error detected\n");
		
		if((json_string_value(json_object_get(error, "description"))))
			return strdup(json_string_value(json_object_get(error, "description")));
	}
	
	return NULL;
}

static int wunder_parse(json_t *root, weather_data_t *weather) {
	json_t *node, *subnode;
	
	node = json_object_get(root, "current_observation");
	if(!json_is_object(node)) {
		fprintf(stderr, "[-] wunder/parse: cannot get current_observation\n");
		return 0;
	}
	
	/* grab location */	
	subnode = json_object_get(node, "display_location");
	if(!json_is_object(subnode)) {
		fprintf(stderr, "[-] wunder/parse: cannot get display_location\n");
		return 0;
	}
	
	if(!(json_string_value(json_object_get(subnode, "full")))) {
		fprintf(stderr, "[-] wunder/parse: cannot get full\n");
		return 0;
	}
	
	strncpy(
		weather->location,
		json_string_value(json_object_get(subnode, "full")),
		sizeof(weather->location)
	);
	
	/* grab temperature */
	if(!(json_is_number(json_object_get(node, "temp_c")))) {
		fprintf(stderr, "[-] wunder/parse: cannot get temp_c\n");
		return 0;
	}
	
	if(json_is_real(json_object_get(node, "temp_c")))
		weather->temp = json_real_value(json_object_get(node, "temp_c"));
	
	else weather->temp = (double) json_integer_value(json_object_get(node, "temp_c"));
	
	/* grab humidity */
	if(!(json_string_value(json_object_get(node, "relative_humidity")))) {
		fprintf(stderr, "[-] wunder/parse: cannot get relative_humidity\n");
		return 0;
	}
	
	weather->humidity = atof(json_string_value(json_object_get(node, "relative_humidity")));
	
	/* grab date */
	if(!(json_string_value(json_object_get(node, "observation_time_rfc822")))) {
		fprintf(stderr, "[-] wunder/parse: cannot get observation_time_rfc822\n");
		return 0;
	}
	
	strncpy(
		weather->date,
		json_string_value(json_object_get(node, "observation_time_rfc822")),
		sizeof(weather->date)
	);
	
	return 1;
}

int wunder_handle(char *chan, char *location, curl_data_t *curl) {
	char *error, temp[1024];
	json_t *root;
	weather_data_t weather;
	
	/* Downloading page */
	sprintf(temp, "http://api.wunderground.com/api/" WUNDERGROUND_KEY "/conditions/q/%s.json", location);
	
	if(curl_download_text(temp, curl))
		return 1;
	
	if(!curl->length)
		return 1;
		
	printf("[ ] wunder: json dump: <%s>\n", curl->data);
	
	/* loading json */
	if(!(root = wunder_json_load(curl->data, strlen(curl->data)))) {
		weather_error(chan, "Cannot download weather information");
		return 1;
	}
	
	if((error = wunder_json_checkerror(root))) {
		weather_error(chan, error);
		free(error);
		return 1;
	}
	
	if(!wunder_parse(root, &weather)) {
		weather_error(chan, "Error during JSON data read");
		return 1;
	}
	
	weather_print(chan, &weather);
	
	return 0;
}
