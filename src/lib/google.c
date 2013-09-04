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
#include "google.h"
#include "../common/bot.h"
#include "../core/init.h"
#include "list.h"
#include "core.h"
#include "downloader.h"
#include "actions.h"
#include "ircmisc.h"

static char *baseurlen = "https://www.google.com/search?hl=en&q=";
static char *baseurl   = "https://www.google.com/search?q=";

google_search_t * google_search(char *keywords) {
	curl_data_t *curl;
	google_search_t *search;
	xmlDoc *doc = NULL;
	xmlXPathContext *ctx = NULL;
	xmlXPathObject *xpathObj = NULL;
	xmlNode *node = NULL;
	char url[2048];
	int i;
	
	curl = curl_data_new();
	
	snprintf(url, sizeof(url), "%s%s", baseurlen, space_encode(keywords));
	
	if(curl_download_text(url, curl))
		return NULL;
	
	doc = (xmlDoc *) htmlReadMemory(curl->data, strlen(curl->data), "/", "utf-8", HTML_PARSE_NOERROR);
	
	/* creating xpath request */
	ctx = xmlXPathNewContext(doc);
	xpathObj = xmlXPathEvalExpression((const xmlChar *) "//li/div/h3/a", ctx);
	
	search = (google_search_t *) calloc(1, sizeof(google_search_t));
	
	if(!xmlXPathNodeSetIsEmpty(xpathObj->nodesetval)) {
		search->length = xpathObj->nodesetval->nodeNr;
		search->result = (google_result_t *) calloc(1, sizeof(google_result_t) * search->length);
		
		for(i = 0; i < xpathObj->nodesetval->nodeNr; i++) {
			node = xpathObj->nodesetval->nodeTab[i];
			
			if(xmlNodeGetContent(node))
				search->result[i].title = strdup((char *) xmlNodeGetContent(node));
			
			if(xmlGetProp(node, (unsigned char *) "href"))
				search->result[i].url   = strdup((char *) xmlGetProp(node, (unsigned char *) "href"));
		}
	}

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(ctx);	
	xmlFreeDoc(doc);
	curl_data_free(curl);
	
	return search;
}

static char *__calc_match1[] = {
	"//div[@class='vk_ans vk_bk']",
	"//div[@class='vk_bk vk_ans']",
	"//div[@class='vk_ans vk_bk curtgt']",
	"//span[@id='cwos']",
	NULL
};

static char *__calc_match2[] = {
	"//input[@id='ucw_rhs_d']",
	"//input[@id='pair_targ_input']",
	NULL
};

static char *google_calc_value(xmlXPathObject *xpathObj) {
	char *value = NULL;
	xmlNode *node = NULL;
	int i;
	
	for(i = 0; i < xpathObj->nodesetval->nodeNr; i++) {
		node = xpathObj->nodesetval->nodeTab[i];
		
		if(xmlNodeGetContent(node)) {
			value = strdup((char *) xmlNodeGetContent(node));
			printf("[+] google/calc: value: %s\n", value);
			return value;
		}
	}
	
	return NULL;
}

static char *google_calc_input_value(xmlXPathObject *xpathObj) {
	char *value = NULL;
	xmlNode *node = NULL;
	
	node = xpathObj->nodesetval->nodeTab[0];
	value = strdup((char *) xmlGetProp(node, (const xmlChar *) "value"));
	printf("[+] google/calc: value: %s\n", value);
	
	return value;
}

static char *google_calc_unit(xmlXPathObject *xpathObj, char *value) {
	xmlNode *node = NULL;
	char *unit = NULL, *final = NULL;
	
	node = xpathObj->nodesetval->nodeTab[0];
	
	if(xmlNodeGetContent(node))
		unit = strdup((char *) xmlNodeGetContent(node));
	
	if(value && unit) {
		final = (char *) malloc(sizeof(char) * (strlen(value) + strlen(unit) + 32));
		
		sprintf(final, "%s %s", value, unit);
		free(unit);
		free(value);
		
		printf("[+] google/calc: final: %s\n", final);
	}
	
	return final;
}

char *google_calc(char *keywords) {
	curl_data_t *curl;
	xmlDoc *doc = NULL;
	xmlXPathContext *ctx = NULL;
	xmlXPathObject *xpathObj = NULL;
	char url[2048], *request, **match;
	char *value = NULL;
	int i;
	
	curl = curl_data_new();
	
	if(!(request = url_encode(keywords)))
		return NULL;
		
	snprintf(url, sizeof(url), "%s%s", baseurl, request);
	free(request);
	
	if(curl_download_text(url, curl))
		return NULL;
	
	doc = (xmlDoc *) htmlReadMemory(curl->data, strlen(curl->data), "/", "utf-8", HTML_PARSE_NOERROR);
	
	/* creating xpath request */
	ctx = xmlXPathNewContext(doc);
	
	// trying first method of detection
	match = __calc_match1;
	i = 0;
	while(match[i]) {
		xpathObj = xmlXPathEvalExpression((const xmlChar *) match[i++], ctx);
		
		if(!xmlXPathNodeSetIsEmpty(xpathObj->nodesetval)) {
			value = google_calc_value(xpathObj);
			goto unit_check;
		}
	}
	
	// trying second method
	match = __calc_match2;
	i = 0;
	while(match[i]) {
		xpathObj = xmlXPathEvalExpression((const xmlChar *) match[i++], ctx);
		
		if(!xmlXPathNodeSetIsEmpty(xpathObj->nodesetval)) {
			value = google_calc_input_value(xpathObj);
			goto unit_check;
		}
	}
	
	unit_check:
	
	// grab the unit
	xpathObj = xmlXPathEvalExpression((const xmlChar *) "//select[@id='target_list']/option[@selected='1']", ctx);
	if(!xmlXPathNodeSetIsEmpty(xpathObj->nodesetval))
		value = google_calc_unit(xpathObj, value);

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(ctx);	
	xmlFreeDoc(doc);
	curl_data_free(curl);
	
	return value;
}

void google_free(google_search_t *search) {
	unsigned int i;
	
	for(i = 0; i < search->length; i++) {
		free(search->result[i].title);
		free(search->result[i].url);
	}
	
	free(search);
}
