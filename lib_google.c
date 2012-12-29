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
#include "lib_google.h"
#include "bot.h"
#include "core_init.h"
#include "lib_list.h"
#include "lib_core.h"
#include "lib_urlmanager.h"
#include "lib_actions.h"
#include "lib_ircmisc.h"

char *baseurl = "https://www.google.com/search?hl=en&q=";

google_search_t * google_search(char *keywords) {
	curl_data_t curl;
	google_search_t *search;
	xmlDoc *doc = NULL;
	xmlXPathContext *ctx = NULL;
	xmlXPathObject *xpathObj = NULL;
	xmlNode *node = NULL;
	char url[2048];
	int i;
	
	snprintf(url, sizeof(url), "%s%s", baseurl, space_encode(keywords));
	
	if(curl_download(url, &curl, 0) || !curl.length)
		return NULL;
	
	doc = (xmlDoc *) htmlReadMemory(curl.data, strlen(curl.data), "/", "utf-8", HTML_PARSE_NOERROR);
	
	/* creating xpath request */
	ctx = xmlXPathNewContext(doc);
	xpathObj = xmlXPathEvalExpression((const xmlChar *) "//a[@class='l']", ctx);
	
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
	free(curl.data);
	
	return search;
}

void google_free(google_search_t *search) {
	unsigned int i;

	for(i = 0; i < search->length; i++) {
		free(search->result[i].title);
		free(search->result[i].url);
	}

	free(search);
}
