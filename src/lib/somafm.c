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
#include "somafm.h"
#include "../common/bot.h"
#include "../core/init.h"
#include "list.h"
#include "core.h"
#include "urlmanager.h"

#include "actions.h"
#include "ircmisc.h"

somafm_station_t somafm_stations[] = {
	{.ref = "poptron",        .name = "PopTron"},
	{.ref = "groovesalad",    .name = "Groove Salad"},
	{.ref = "indiepop",       .name = "Indie Pop Rocks!"},
	{.ref = "sonicuniverse",  .name = "Sonic Universe"},
	{.ref = "dubstep",        .name = "Dub Step Beyond"},
};

unsigned int somafm_stations_count = sizeof(somafm_stations) / sizeof(somafm_station_t);

char * somafm_station_list() {
	char *list;
	unsigned int i, len = 0;
	
	for(i = 0; i < somafm_stations_count; i++)
		len += strlen(somafm_stations[i].ref) + 1;
	
	list = (char *) malloc(sizeof(char) * len + 1);
	if(!list)
		return NULL;
	
	*list = '\0';
	
	for(i = 0; i < somafm_stations_count; i++) {
		strcat(list, somafm_stations[i].ref);
		strcat(list, " ");
	}
	
	/* Remove last space */
	*(list + len - 1) = '\0';
	
	return list;
}

int somafm_handle(char *chan, somafm_station_t *station) {
	xmlDoc *doc = NULL;
	xmlXPathContext *ctx = NULL;
	xmlXPathObject *xpathObj = NULL;
	xmlNode *node = NULL;
	curl_data_t *curl;
	char temp[512];
	
	curl = curl_data_new();
	
	// downloading page
	sprintf(temp, "http://somafm.com/%s/", station->ref);
	
	if(curl_download_text(temp, curl))
		return 1;
	
	if(!curl->length)
		return 1;

	// loading xml
	doc = (xmlDoc *) htmlReadMemory(curl->data, strlen(curl->data), "/", "utf-8", HTML_PARSE_NOERROR);
	
	// creating xpath request
	ctx = xmlXPathNewContext(doc);
	xpathObj = xmlXPathEvalExpression((const xmlChar *) "//span[@class='playing']/a", ctx);
	
	if(xmlXPathNodeSetIsEmpty(xpathObj->nodesetval)) {
		printf("[-] XPath: No values\n");
		goto freeme;
	}
	
	node = xpathObj->nodesetval->nodeTab[0];
	printf("[+] SomaFM: <%s>\n", (char *) node->children->content);
	
	// building response
	sprintf(temp, "PRIVMSG %s :[SomaFM/%s] Now playing: %s", chan, station->name, (char *) node->children->content);
	raw_socket(temp);

	// freeing all stuff
	freeme:
		xmlXPathFreeObject(xpathObj);
		xmlXPathFreeContext(ctx);
		xmlFreeDoc(doc);
		curl_data_free(curl);
	
	return 0;
}

int main(void) {
	printf("%s\n", somafm_station_list());
}
