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
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include "lib_weather.h"
#include "bot.h"
#include "core_init.h"
#include "lib_core.h"
#include "lib_urlmanager.h"

#include "lib_actions.h"
#include "lib_ircmisc.h"

weather_station_t weather_stations[] = {
	{.id = 29,  .ref = "liege",   .type = STATION_STATION, .name = "Thier-à-Liège"},
	{.id = 96,  .ref = "slins",   .type = STATION_STATION, .name = "Slins"},
	{.id = 125, .ref = "oupeye",  .type = STATION_STATION, .name = "Oupeye"},
	{.id = 14,  .ref = "lille",   .type = STATION_METAR,   .name = "Lille (France)"},
	{.id = 77,  .ref = "knokke",  .type = STATION_STATION, .name = "Knokke"},
	{.id = 80,  .ref = "seraing", .type = STATION_STATION, .name = "Boncelles (Seraing)"},
	{.id = 106, .ref = "namur",   .type = STATION_STATION, .name = "Floriffoux (Namur)"},
	{.id = 48,  .ref = "spa",     .type = STATION_STATION, .name = "Spa"},
	{.id = 13,  .ref = "beauve",  .type = STATION_METAR,   .name = "Beauvechain"},
};

unsigned int weather_stations_count = sizeof(weather_stations) / sizeof(weather_station_t);

char *__weather_internal_station_url[] = {
	"http://www.meteobelgique.be/observation/station-meteo.html?staticfile=realtime-datametar.php&Itemid=69&id=%d",
	"http://www.meteobelgique.be/observation/station-meteo.html?staticfile=realtime-datastation.php&Itemid=69&id=%d&lg=2"
};

char * weather_station_list() {
	char *list;
	unsigned int i, len = 0;
	
	for(i = 0; i < weather_stations_count; i++)
		len += strlen(weather_stations[i].ref) + 1;
	
	list = (char*) malloc(sizeof(char) * len + 1);
	if(!list)
		return NULL;
	
	*list = '\0';
	
	for(i = 0; i < weather_stations_count; i++) {
		strcat(list, weather_stations[i].ref);
		strcat(list, " ");
	}
	
	/* Remove last space */
	*(list + len - 1) = '\0';
	
	return list;
}

void weather_parse(const char *data, weather_data_t *weather) {
	char *temp;
	double *temp_output;
	char *temp_time_output;
	
	if(!strncmp(data, "Températures", 13)) {
		// Selecting right variable
		if(!strncmp(data, "Températures min", 17)) {
			temp_output      = &weather->temp_min;
			temp_time_output = weather->temp_min_time;

		} else {
			temp_output      = &weather->temp_max;
			temp_time_output = weather->temp_max_time;
		}

		// Writing
		*temp_output = atof(data + 26);

		if((temp = strstr(data, "à"))) {
			strncpy(temp_time_output, temp + 3, 5);
			temp_time_output[5] = '\0';

		} else *temp_time_output = '\0';

	} else if(!strncmp(data, "Température :", 14))
		weather->temp = atof(data + 15);

	else if(!strncmp(data, "Vitesse du vent :", 15))
		weather->wind_speed = atof(data + 16);
	
	else if(!strncmp(data, "Vitesse :", 9))
		weather->wind_speed = atof(data + 10);
	
	else if(!strncmp(data, "Humidité :", 11))
		weather->humidity = atoi(data + 12);
}

int weather_handle(char *chan, weather_station_t *station) {
	xmlDoc *doc = NULL;
	xmlXPathContext *ctx = NULL;
	xmlXPathObject *xpathObj = NULL;
	xmlNode *node = NULL;
	int i, n;
	curl_data_t curl;
	char temp[512];
	weather_data_t weather = {
		.temp_min_time = {'\0'},
		.temp_max_time = {'\0'},
	};
	
	/* Downloading page */
	sprintf(temp, __weather_internal_station_url[station->type], station->id);
	
	if(curl_download(temp, &curl, 0))
		return 1;
	
	if(!curl.length)
		return 1;

	// loading xml
	doc = (xmlDoc *) htmlReadMemory(curl.data, strlen(curl.data), "/", "utf-8", HTML_PARSE_NOERROR);
	
	// creating xpath request
	ctx = xmlXPathNewContext(doc);
	xpathObj = xmlXPathEvalExpression((const xmlChar *) "//ul[@class='arrow']/li", ctx);
	
	if(xmlXPathNodeSetIsEmpty(xpathObj->nodesetval)) {
		snprintf(temp, sizeof(temp), "PRIVMSG %s :Station information are currently unavailable.", chan);
		raw_socket(sockfd, temp);
		
		printf("[-] XPath: No values\n");
		goto freeme;
	}
	
	n = xpathObj->nodesetval->nodeNr;

	// reading each nodes (ul li)
	for(i = 0; i < n; i++) {
		node = xpathObj->nodesetval->nodeTab[i];
		weather_parse((char *) node->children->content, &weather);
	}

	// reading date node
	xmlXPathFreeObject(xpathObj);
	xpathObj = xmlXPathEvalExpression((unsigned const char*) "//h3[@class='MBlegend-title']", ctx);
	
	if(xmlXPathNodeSetIsEmpty(xpathObj->nodesetval)) {
		printf("[-] XPath: No values\n");
		goto freeme;
	}
	
	n = xpathObj->nodesetval->nodeNr;
	
	for(i = 0; i < n; i++) {
		node = xpathObj->nodesetval->nodeTab[i];
		strcpy(weather.date, (char *) node->children->content + 11);
		break; // just handle the first element
	}
	
	// building response	
	sprintf(temp, "PRIVMSG %s :%s: température: %.1f°C, humidité: %d%% [%s]", chan, station->name, weather.temp, weather.humidity, weather.date);
	raw_socket(sockfd, temp);
	
	// freeing all stuff
	freeme:
		xmlXPathFreeObject(xpathObj);
		xmlXPathFreeContext(ctx);
		xmlFreeDoc(doc);
		free(curl.data);
	
	return 0;
}
