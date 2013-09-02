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
#include "ircmisc.h"

char *wiki_head(char *url) {
	curl_data_t *curl;
	xmlDoc *doc = NULL;
	xmlXPathContext *ctx = NULL;
	xmlXPathObject *xpathObj = NULL;
	char *text = NULL;
	
	curl = curl_data_new();
	
	if(curl_download_text(url, curl))
		return NULL;
	
	doc = (xmlDoc *) htmlReadMemory(curl->data, strlen(curl->data), "/", "utf-8", HTML_PARSE_NOERROR);
	
	/* creating xpath request */
	ctx = xmlXPathNewContext(doc);
	xpathObj = xmlXPathEvalExpression((const xmlChar *) "//div[@id='mw-content-text']/p", ctx);
	
	if(!xmlXPathNodeSetIsEmpty(xpathObj->nodesetval)) {
		if(xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]))
			text = strdup((char *) xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]));	
	}

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(ctx);
	xmlFreeDoc(doc);
	curl_data_free(curl);
	
	return text;
}
