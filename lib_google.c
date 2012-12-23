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

int google_search(char *chan, char *search, int max) {
	xmlDoc *doc = NULL;
	xmlXPathContext *ctx = NULL;
	xmlXPathObject *xpathObj = NULL;
	xmlNode *node = NULL;
	int i, x;
	curl_data_t curl;
	char *url;
	char answer[512], *nurl;
	char *baseurl = "https://www.google.com/search?hl=en&q=";
	
	url = (char*) malloc(sizeof(char) * strlen(search) + strlen(baseurl) + 8);
	sprintf(url, "%s%s", baseurl, space_encode(search));
	
	if(curl_download(url, &curl, 0) || !curl.length) {
		free(url);
		return 1;
	}
	
	doc = (xmlDoc *) htmlReadMemory(curl.data, strlen(curl.data), "/", "utf-8", HTML_PARSE_NOERROR);
	
	// creating xpath request
	ctx = xmlXPathNewContext(doc);
	xpathObj = xmlXPathEvalExpression((const xmlChar *) "//a[@class='l']", ctx);
	
	if(!xmlXPathNodeSetIsEmpty(xpathObj->nodesetval)) {
		x = (xpathObj->nodesetval->nodeNr > max) ? max : xpathObj->nodesetval->nodeNr;
		
		// Cleaning this url
		free(url);
		
		for(i = 0; i < x; i++) {
			node = xpathObj->nodesetval->nodeTab[i];
			nurl = (char *) xmlGetProp(node, (unsigned char *) "href");
			
			if(!(url = shurl(nurl))) {
				printf("[-] Google: failed to shurl\n");
				continue;
			}
			
			snprintf(answer, sizeof(answer), "PRIVMSG %s :%d) %s: %s", chan, i + 1, (char *) xmlNodeGetContent(node), url);
			raw_socket(sockfd, answer);
			
			free(url);
		}
		
		url = NULL;
		
	} else {
		snprintf(answer, sizeof(answer), "PRIVMSG %s :No match found.", chan);
		raw_socket(sockfd, answer);
		
		return 2;
	}

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(ctx);
	xmlFreeDoc(doc);
	free(curl.data);
	free(url);
	
	return 0;
}
