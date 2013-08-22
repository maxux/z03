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
#include <stdlib.h>
#include "downloader.h"
#include "shurl.h"

static char *baseurl = "http://x.maxux.net/index.php?url=";

char *shurl(char *url) {
	char *request;
	curl_data_t *curl;
	size_t len;
	
	curl = curl_data_new();
	
	len = (strlen(baseurl) + strlen(url)) + 8;
	if(!(request = (char *) malloc(sizeof(char) * len)))
		return NULL;
	
	sprintf(request, "%s%s", baseurl, url);
	
	if(curl_download_text(request, curl)) {
		free(request);
		return NULL;
	}
	
	if(curl->length > len)
		curl->length = len - 1;
	
	strncpy(request, curl->data, curl->length);
	request[curl->length] = '\0';
	
	curl_data_free(curl);
	
	printf("[+] urlmanager/shurl: <%s>\n", request);
	
	return request;
}
