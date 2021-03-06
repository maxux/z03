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
#include <magic.h>
#include <pthread.h>
#include "../common/bot.h"
#include "../core/init.h"
#include "database.h"
#include "list.h"
#include "core.h"
#include "entities.h"
#include "downloader.h"
#include "urlmanager.h"
#include "urlrepost.h"
#include "ircmisc.h"
#include "stats.h"
#include "magic.h"

request_t __url_request = {
	.match    = "",
	.callback = url_manager,
	.man      = "",
	.hidden   = 1,
	.syntaxe  = "",
};

//
// return a malloc'ed url from a string
// the string must begin by "http(s)://" 
//
char *extract_url(char *url) {
	int i = 0, braks = 0;
	char *out;
	
	// TODO: Matching (xxx)
	while(*(url + i) && *(url + i) != ' ' && !braks) {
		/* if(*(url + i) != '(')
			braks++;
		
		else if(*(url + i) != ')')
			braks--; */
			
		i++;
	}
	
	if(!(out = (char *) malloc(sizeof(char) * i + 1)))
		return NULL;
	
	strncpy(out, url, i);
	out[i] = '\0';
	
	return out;
}

//
// return the host part of an url
// string must be free'd after use
//
char *extract_host(char *url) {
	char *from, *to;
	
	if(!(from = strchr(url, '/')))
		return NULL;
	
	if(*(from + 1) == '/')
		from += 2;
	
	if(!(to = strchr(from, '/')))
		return NULL;
	
	return strndup(from, to - from);
}

//
// grab <title></title> from a html page
// don't use libxml to works with crappy invalid html page
// 
static char *url_extract_title(char *body) {
	char *read, *end;
	char *title;
	
	if((read = strcasestr(body, "<title"))) {
		while(*read != '>')
			read++;
		
		read = ltrim(++read);
			
		if(!(end = strstr(read, "</")))
			return NULL;
		
		title = strndup(read, end - read);
		title = rtrim(crlftrim(title));
		
		if(!strlen(title)) {
			free(title);
			return NULL;
		}
		
		printf("[+] urlmanager/rawtitle: <%s>\n", title);
		
	} else return NULL;
	
	return title;
}

//
// build a magic file string from libmagic, see magic.
//
int url_magic(curl_data_t *curl, ircmessage_t *message) {
	char *magicstr = NULL;
	char buffer[1024];
	size_t length;
	
	// magic file
	magicstr = magic(curl->data, curl->length);
	
	length = ((!curl->http_length) ? curl->length : curl->http_length);
	
	if(length < 1024 * 2048)
		zsnprintf(buffer, "File: [%.2f Ko, %s]", length / 1024.0, magicstr);
		
	else zsnprintf(buffer, "File: [%.2f Mo, %s]", length / 1024 / 1024.0, magicstr);
	
	irc_privmsg(message->chan, buffer);
	
	free(magicstr);
	
	return 1;
}

//
// download image in memory, process a checksum in ram
// magic file and saving on disk for mirroring
//
static int url_process_image(char *url, ircmessage_t *message, repost_t *repost) {
	curl_data_t *curl;
	char filename[512], temp[512], *sqlquery;
	int length;

	curl = curl_data_new();
	
	if(curl_download(url, curl, 0)) // force dl is useless with custom wrapper
		fprintf(stderr, "[-] urlmanager/image: something wrong with download\n");
	
	if(curl->data && curl->length) {	 
		strcpy(temp, url + 7);
		clean_filename(temp);
		
		// building filename
		sprintf(filename, "%s/%s", MIRROR_PATH, temp);
		printf("[+] urlmanager/image: %s\n", filename);		
	
		length = file_write(filename, curl->data, curl->length);		
		printf("[+] urlmanager/image: file saved: %d bytes\n", length);
		
		// updating database
		sqlquery = sqlite3_mprintf(
			"INSERT INTO images (nick, chan, url, date, filename, size) VALUES "
			"('%q', '%q', '%q', '%u', '%q', '%u')",
			message->nick, message->chan, url, time(NULL), temp, length
		);
		
		if(!db_sqlite_simple_query(sqlite_db, sqlquery))
			printf("[-] urlmanager/mirroring: cannot insert image\n");
		
		sqlite3_free(sqlquery);
		
		url_magic(curl, message);
		url_repost_advanced(curl, message, repost);
	}
	
	// free stuff
	curl_data_free(curl);
	
	return 0;
}

static int url_process_html(char *url, ircmessage_t *message, repost_t *repost) {
	curl_data_t *curl;
	char *title = NULL, *print = NULL;
	char request[256];
	char *sqlquery;
	
	curl = curl_data_new();
	
	if(curl_download(url, curl, 0)) // force dl is useless with custom wrapper
		fprintf(stderr, "[-] urlmanager/html: something wrong with download\n");
	
	if(curl->data && curl->length && (title = url_extract_title(curl->data))) {
		// useless/easteregg check
		if(!strncasecmp(message->nick, "malabar", 7) && 
		   (strstr(title, "Boiler Room") || strstr(title, "BOILER ROOM")))
			irc_kick(message->chan, message->nick, "ON S'EN *BRANLE* DE TA PUTAIN DE BOILER ROOM");
		
		// if charset is not set on http header, trying to find it
		// by reading html content (ie: <meta> tag)
		if(curl->charset == UNKNOWN_CHARSET) {
			curl->charset = curl_extract_charset(curl->data);
			printf("[+] urlmanager/charset fixed: %d\n", curl->charset);
		}
		
		// setting title repost, duplicating
		repost->title = strdup(title);
		
		// highlight preventing on title
		print = anti_hl_each_words(title, strlen(title), curl->charset);
		decode_html_entities_utf8(print, NULL);
		
		// truncate too long title
		if(strlen(print) > 250)
			spacetrunc(print, 250);
		
		// print title with error code if any
		if(curl->code != 200) {
			zsnprintf(request, "URL (Error: %ld): %s", curl->code, print);
			
		} else zsnprintf(request, "URL: %s", print);
		
		irc_privmsg(message->chan, request);
		free(print);
		
		// updating title on database
		sqlquery = sqlite3_mprintf(
			"UPDATE url SET title = '%q' WHERE url = '%q'",
			title, url
		);
		
		if(!db_sqlite_simple_query(sqlite_db, sqlquery))
			printf("[-] urlmanager/title: cannot update title on db\n");
		
		sqlite3_free(sqlquery);
		free(title);
		
		if(curl->code == 200)
			url_repost_advanced(curl, message, repost);
		
	} else fprintf(stderr, "[-] urlmanager/title: cannot extract title\n");
	
	// free stuff
	curl_data_free(curl);
	
	return 0;
}

//
// handler for unknown process, wrapper for libcurl
// downloading 256 ko of data, then magic it
//
static size_t url_process_unknown_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
	curl_data_t *curl = (curl_data_t*) userdata;
	size_t prev;
	
	prev = curl->length;
	curl->length += (size * nmemb);
	
	/* Resize data */
	curl->data  = (char *) realloc(curl->data, (curl->length + 1));
	memcpy(curl->data + prev, ptr, size * nmemb);
	
	if(curl->length >= 256 * 1024)
		return 0;
	
	return size * nmemb;
}

//
// process download of UNKNOWN type (not image or html)
//
static int url_process_unknown(char *url, ircmessage_t *message, repost_t *repost) {
	curl_data_t *curl;
	(void) repost;
	
	curl = curl_data_new();
	curl->body_callback = url_process_unknown_callback;
	
	if(curl_download(url, curl, 42)) // force dl is useless with custom wrapper
		fprintf(stderr, "[-] urlmanager/unknown: something wrong with download\n");
	
	if(curl->data && curl->length)
		url_magic(curl, message);
	
	// free stuff
	curl_data_free(curl);
	
	return 0;
}

//
// just wrap error avoiding lot of line
//
int url_error(int errcode, curl_data_t *curl) {
	curl_data_free(curl);
	return errcode;
}

//
// make a first HTTP HEAD request, to read header information
// on base on header, dispatch code to image/html/unknown (binary)
//
static int url_process(char *url, ircmessage_t *message, repost_t *repost) {
	curl_data_t *curl;
	document_type_t type;
	char buffer[1024];
	
	curl = curl_data_new();
	
	// reading request header
	if(curl_download_nobody(url, curl, 0))
		printf("[-] Warning: special download\n");
		
	printf("[+] urlmanager/url httplength: %zu\n", curl->http_length);
	printf("[+] urlmanager/url type: %d\n", curl->type);
	
	// curl/http error (404, 401, ...), nothing interresting is downloaded
	if(curl->curlcode != CURLE_OK && curl->curlcode != CURLE_WRITE_ERROR) {
		zsnprintf(buffer, "URL (Error %d): %s", curl->curlcode, curl_easy_strerror(curl->curlcode));
		irc_privmsg(message->chan, buffer);
		return url_error(1, curl);
	}
	
	if((curl->type == IMAGE_ALL || curl->type == WEBM) && curl->http_length) {
		// warning on big image
		if(curl->http_length > CURL_WARN_SIZE && curl->http_length < CURL_MAX_SIZE) {
			zsnprintf(buffer, "[Warning: image size: %.2f Mo, mirroring anyway...]",
					  curl->http_length / 1024 / 1024.0);
					
			irc_privmsg(message->chan, buffer);
		
		// file too large, will not be downloaded
		// skipping repost process because checksum will not be made
		} else if(curl->http_length > CURL_MAX_SIZE) {
			zsnprintf(buffer, "[Warning: image size: %.2f Mo, this file will not be mirrored]",
					  curl->http_length / 1024 / 1024.0);
					
			irc_privmsg(message->chan, buffer);
			
			return url_error(1, curl);
		}
		
	} else fprintf(stderr, "[-] urlmanager/url httplength is not set, deal with it.\n");
	
	type = curl->type;
	curl_data_free(curl);
	
	/* dispatching process */
	if(type == IMAGE_ALL || type == WEBM) {
		#ifdef ENABLE_MIRRORING
		return url_process_image(url, message, repost);
		#else
		return url_process_unknown(url, message, repost);
		#endif
	}
		
	if(type == TEXT_HTML)
		return url_process_html(url, message, repost);
		
	if(type == UNKNOWN_TYPE)
		return url_process_unknown(url, message, repost);
	
	return 0;
}

//
// main point of url handler, check the url, load initial repost information
// make stats counting then process the request to curl parsing
//
void url_manager(ircmessage_t *message, char *args) {
	char buffer[1024], *url;
	int urlcount;
	repost_t *repost;
	(void) args;
	
	url = args;
	
	printf("[+] urlmanager/parser: %s\n", url);
		
	if(strlen(url) > 256) {
		printf("[-] urlmanager/parser: url too long...\n");
		return;
	}
	
	// creating repost node
	repost = repost_new();
	repost->url = strdup(url);
	url_repost_hit(url, message, repost);
	
	// check for round url count
	if(!((urlcount = stats_url_count(message->chan)) % 500)) {
		zsnprintf(buffer, "We just reached %d urls !", urlcount);
		irc_privmsg(message->chan, buffer);
	}
	
	// dispatching url
	url_process(url, message, repost);
	
	// freeing stuff
	repost_free(repost);
}


int url_format_log(char *sqlquery, ircmessage_t *message) {
	sqlite3_stmt *stmt;
	char *output = NULL, *title, *url, *nick;
	time_t time;
	struct tm * timeinfo;
	char date[128], *url_nick;
	int row, len, count = 0;
	char *titlehl;

	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) == SQLITE_ROW) {
			url   = (char *) sqlite3_column_text(stmt, 0);
			title = (char *) sqlite3_column_text(stmt, 1);
			nick  = (char *) sqlite3_column_text(stmt, 2);
			time  = (time_t) sqlite3_column_int(stmt, 3);
			
			timeinfo = localtime(&time);
			strftime(date, sizeof(date), "%d/%m/%Y %X", timeinfo);
			
			if(!title)
				title = "Unknown title";				
			
			url_nick = (char *) malloc(sizeof(char) * strlen((char *) nick) + 8);
			strcpy(url_nick, nick);
			
			if(!(titlehl = anti_hl_each_words(title, strlen(title), UTF_8)))
				continue;
				
			len = strlen(url) + strlen(title) + strlen(nick) + 128;
			output = (char *) malloc(sizeof(char) * len);
			
			snprintf(output, len, "[%s] <%s> %s | %s", date, anti_hl(url_nick), url, titlehl);
			irc_privmsg(message->chan, output);
			
			free(titlehl);
			free(output);
			free(url_nick);
			
			count++;
		}
	
	} else irc_privmsg(message->chan, "Something went wrong with the sql query, sorry :/");
	
	if(!output)
		irc_privmsg(message->chan, "No match found");
	
	sqlite3_finalize(stmt);
	
	return count;
}

//
// return a copy of a well formated url from raw html code
//
char *url_ender(char *src) {
	char *end;
	
	if(!(end = strchr(src, '"')))
		return NULL;
	
	return strndup(src, end - src);
}
