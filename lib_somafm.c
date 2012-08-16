#include <stdio.h>
#include <string.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <curl/curl.h>
#include "lib_somafm.h"
#include "bot.h"
#include "core_init.h"
#include "lib_core.h"
#include "lib_urlmanager.h"

#include "lib_actions.h"
#include "lib_ircmisc.h"

somafm_station_t somafm_stations[] = {
	{.ref = "poptron",        .name = "PopTron"},
	{.ref = "groovesalad",    .name = "Groove Salad"},
	{.ref = "indiepop",       .name = "Indie Pop Rocks!"},
};

unsigned int somafm_stations_count = sizeof(somafm_stations) / sizeof(somafm_station_t);

char * somafm_station_list() {
	char *list;
	unsigned int i, len = 0;
	
	for(i = 0; i < somafm_stations_count; i++)
		len += strlen(somafm_stations[i].ref) + 1;
	
	list = (char*) malloc(sizeof(char) * len + 1);
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
	curl_data_t curl;
	char temp[512];
	
	// downloading page
	sprintf(temp, "http://somafm.com/%s/", station->ref);
	
	if(curl_download(temp, &curl))
		return 1;
	
	if(!curl.length)
		return 1;

	// loading xml
	doc = (xmlDoc *) htmlReadMemory(curl.data, strlen(curl.data), "/", "utf-8", HTML_PARSE_NOERROR);
	
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
	raw_socket(sockfd, temp);

	// freeing all stuff
	freeme:
		xmlXPathFreeObject(xpathObj);
		xmlXPathFreeContext(ctx);
		xmlFreeDoc(doc);
		free(curl.data);
	
	return 0;
}

int main(void) {
	printf("%s\n", somafm_station_list());
}
