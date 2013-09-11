/* z03 - small bot with some network features - url handling/mirroring/management
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netdb.h>
#include <curl/curl.h>
#include <ctype.h>
#include <sqlite3.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include <pthread.h>
#include "../common/bot.h"
#include "../core/init.h"
#include "list.h"
#include "core.h"
#include "entities.h"
#include "downloader.h"
#include "ircmisc.h"

char *__user_agent_hosts[] = {
	"t.co", "gks.gs", "google.com"
};

host_cookies_t __host_cookies[] = {
	{.host = "facebook.com",    .cookie = PRIVATE_FBCOOK},
	{.host = "what.cd",         .cookie = PRIVATE_WHATCD},
	{.host = "gks.gs",          .cookie = PRIVATE_GKS},
	{.host = NULL,              .cookie = NULL},
};

char *curl_gethost(char *url) {
	char *buffer, *match, *left;
	
	/* Extracting Host */
	if((match = strstr(url, "//"))) {
		/* Skipping // */
		match += 2;
		
		if(!(left = strchr(match, '/')))
			return NULL;
		
		if(!(buffer = (char *) malloc(sizeof(char) * (left - match) + 1)))
			return NULL;
			
		snprintf(buffer, left - match + 1, "%s", match);
		return buffer;
		
	} else return NULL;
}

static char *curl_useragent(char *url) {
	char *host, *useragent;
	unsigned int i;
	
	useragent = CURL_USERAGENT;
	
	/* Checking host */
	if((host = curl_gethost(url))) {
		printf("[+] urlmanager/host: <%s>\n", host);
		
		for(i = 0; i < sizeof(__user_agent_hosts) / sizeof(char *); i++) {
			if(!strcmp(host, __user_agent_hosts[i]))
				useragent = CURL_USERAGENT_LEGACY;
		}
			
		free(host);
	}
	
	printf("[+] urlmanager/useragent: <%s>\n", useragent);
	
	return useragent;
}

static char *curl_cookie(char *url) {
	char *host, *cookie = NULL;
	int i;
	
	if(!(host = curl_gethost(url)))
		return NULL;
	
	for(i = 0; __host_cookies[i].host; i++) {
		if(strstr(host, __host_cookies[i].host)) {
			printf("[ ] urlmanager/cookie: cookie %s (%s) found\n", __host_cookies[i].host, host);
			cookie = __host_cookies[i].cookie;
			break;
		}
	}
		
	free(host);
	
	return cookie;
}

charset_t curl_extract_charset(char *line) {
	char *charset;
	
	if((charset = strcasestr(line, " charset="))) {
		// skipping match
		charset += 9;
		
		if(*charset == '"')
			charset++;
		
		if(!strncasecmp(charset, "iso-8859", 8))
			return ISO_8859;
		
		if(!strncasecmp(charset, "utf-8", 5))
			return UTF_8;
		
		if(!strncasecmp(charset, "windows-1252", 5))
			return WIN_1252;
		
		printf("[ ] urlmanager/charset: unknown: <%s>\n", charset);
	}
	
	return UNKNOWN_CHARSET;
}

size_t curl_header_validate(char *ptr, size_t size, size_t nmemb, void *userdata) {
	curl_data_t *curl = (curl_data_t*) userdata;
	size_t len;
	
	if(!(size * nmemb))
		return 0;
	
	printf("[ ] urlmanager/header: %s", ptr);
	
	if(!strncasecmp(ptr, "Content-Type: ", 14)) {
		free(curl->http_type);
		curl->http_type = NULL;
			
		if(!strncmp(ptr + 14, "image/", 6)) {
			printf("[+] urlmanager/header: <image> mime type detected\n");
			curl->type = IMAGE_ALL;
			
			return size * nmemb;
		}
		
		// reading charset
		curl->charset = curl_extract_charset(ptr + 14);
		printf("[+] urlmanager/header: charset: %d\n", curl->charset);
		
		if(!strncmp(ptr + 14, "text/html", 9)) {
			printf("[+] urlmanager/header: <text/html> mime type detected\n");
			curl->type = TEXT_HTML;
			
			return size * nmemb;
		}
		
		printf("[ ] urlmanager/header: match: %s", ptr);
		
		// ignoring anything else
		curl->type = UNKNOWN_TYPE;
		
		len = strlen(ptr + 14);
		curl->http_type = (char *) malloc(sizeof(char) * len + 1);
		
		strcpy(curl->http_type, ptr + 14);
		curl->http_type[len - 2] = '\0';
		
		printf("[-] urlmanager/header: unhandled mime type: <%s>\n", curl->http_type);
	}
	
	if(!strncasecmp(ptr, "Content-Length: ", 16)) {
		curl->http_length = atoll(ptr + 16);
		printf("[+] urlmanager/header: length %zu bytes\n", curl->http_length);
	}

	/* Return required by libcurl */
	return size * nmemb;
}

size_t curl_body(char *ptr, size_t size, size_t nmemb, void *userdata) {
	curl_data_t *curl = (curl_data_t*) userdata;
	size_t prev;
	
	if(!curl->forcedl && curl->http_length > CURL_MAX_SIZE)
		return 0;
	
	prev = curl->length;
	curl->length += (size * nmemb);
	
	// Do not download if too large or a unhandled mime
	if(!curl->forcedl && (curl->length > CURL_MAX_SIZE || curl->http_type))
		return 0;
	
	/* Resize data */
	curl->data = (char *) realloc(curl->data, (curl->length + 1));
	
	/* Appending data */
	memcpy(curl->data + prev, ptr, size * nmemb);
	
	/* Return required by libcurl */
	return size * nmemb;
}

curl_data_t *curl_data_new() {
	curl_data_t *data;
	
	if(!(data = (curl_data_t *) calloc(1, sizeof(curl_data_t))))
		return NULL;
	
	data->type    = UNKNOWN_TYPE;
	data->charset = UNKNOWN_CHARSET;
	
	return data;
}

void curl_data_free(curl_data_t *data) {
	free(data->data);
	free(data->http_type);
	free(data);
}

static int curl_download_process(char *url, curl_data_t *data, char forcedl, char *post, char head) {
	CURL *curl;
	char *useragent = CURL_USERAGENT;
	char *cookie;
	
	curl = curl_easy_init();

	// FIXME: set it in other way
	data->forcedl = forcedl;
	data->url     = url;
	
	printf("[+] urlmanager/curl: %s, force dl: %d\n", url, data->forcedl);
	
	useragent = curl_useragent(url);
	
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		
		if(head)
			curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
			
			
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
		
		if(data->body_callback)
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, data->body_callback);
			
		else curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_body);

		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10);
		
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, data);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_validate);
		
		curl_easy_setopt(curl, CURLOPT_HEADER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, useragent);
		
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180); // 180 seconds at 4 Mo/s = 720 Mo of data
		/* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); */
		
		if(post) {
			curl_easy_setopt(curl, CURLOPT_POST, 1);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
			printf("[+] urlmanager/post: <%s>\n", post);
		}
		
		/* Checking Host for specific Cookies */
		if(data->cookie) {
			printf("[ ] urlmanager/cookie: %s\n", data->cookie);
			curl_easy_setopt(curl, CURLOPT_COOKIE, data->cookie);
			
		} else if((cookie = curl_cookie(url)))
			curl_easy_setopt(curl, CURLOPT_COOKIE, cookie);
		
		data->curlcode = curl_easy_perform(curl);
		
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &(data->code));
		printf("[ ] urlmanager/curl: code: %ld\n", data->code);
		
		curl_easy_cleanup(curl);
		
		if(!data->data && !data->http_length) {
			fprintf(stderr, "[-] urlmanager/curl: data is empty.\n");
			return 1;
		}
		
		// FIXME
		/* if(data->type == TEXT_HTML) {
			data->charset = url_extract_charset(data->data);
			printf("[ ] urlmanager/curl: charset: %d\n", data->charset);
		} */
		
	} else return 1;
	
	return 0;
}

/* wrapper */
int curl_download(char *url, curl_data_t *data, char forcedl) {
	return curl_download_process(url, data, forcedl, NULL, 0);
}

int curl_download_nobody(char *url, curl_data_t *data, char forcedl) {
	return curl_download_process(url, data, forcedl, NULL, 1);
}

int curl_download_post(char *url, curl_data_t *data, char *post) {
	return curl_download_process(url, data, 1, post, 0);
}

int curl_download_text(char *url, curl_data_t *data) {
	if(curl_download(url, data, 1) || !data->length)
		return 1;
	
	data->data[data->length] = '\0';	
	return 0;
}

int curl_download_text_post(char *url, curl_data_t *data, char *post) {
	if(curl_download_post(url, data, post) || !data->length)
		return 1;
	
	data->data[data->length] = '\0';	
	return 0;
}
