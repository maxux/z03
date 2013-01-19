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
#include "bot.h"
#include "core_init.h"
#include "lib_database.h"
#include "lib_list.h"
#include "lib_core.h"
#include "lib_entities.h"
#include "lib_urlmanager.h"
#include "lib_ircmisc.h"

#define _GNU_SOURCE

char *__host_ignore[] = {NULL
	/* "www.maxux.net/",
	"paste.maxux.net/",
	"git.maxux.net/" */
};

host_cookies_t __host_cookies[] = {
	{.host = "facebook.com",    .cookie = PRIVATE_FBCOOK},
	{.host = "what.cd",         .cookie = PRIVATE_WHATCD},
	{.host = "gks.gs",          .cookie = PRIVATE_GKS},
	{.host = NULL,              .cookie = NULL},
};

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

char *curl_gethost(char *url) {
	char *buffer, *match, *left;
	
	/* Extracting Host */
	if((match = strstr(url, "//"))) {
		/* Skipping // */
		match += 2;
		
		if(!(left = strchr(match, '/')))
			return NULL;
		
		buffer = (char *) malloc(sizeof(char) * (left - match) + 1);
		snprintf(buffer, left - match + 1, "%s", match);
		
		return buffer;
		
	} else return NULL;
}

char *curl_useragent(char *url) {
	char *host, *useragent;
	
	useragent = CURL_USERAGENT;
	
	/* Checking host */
	if((host = curl_gethost(url))) {
		printf("[+] CURL/Init: host is <%s>\n", host);
		
		if(!strcmp(host, "t.co"))
			useragent = CURL_USERAGENT_LEGACY;
		
		if(!strcmp(host, "gks.gs"))
			useragent = CURL_USERAGENT_LEGACY;
			
		free(host);
	}
	
	printf("[+] CURL/UserAgent: <%s>\n", useragent);
	
	return useragent;
}

char * curl_cookie(char *url) {
	char *host, *cookie = NULL;
	int i;
	
	if((host = curl_gethost(url))) {
		for(i = 0; __host_cookies[i].host; i++) {
			if(strstr(host, __host_cookies[i].host)) {
				printf("[ ] URL/Cookie: special cookie for host %s (%s) found\n", __host_cookies[i].host, host);
				cookie = __host_cookies[i].cookie;
				break;
			}
		}
		
		free(host);
	}
	
	return cookie;
}

size_t curl_header_validate(char *ptr, size_t size, size_t nmemb, void *userdata) {
	curl_data_t *curl = (curl_data_t*) userdata;
	size_t len;
	
	if(!(size * nmemb))
		return 0;
	
	printf("[ ] CURL/Header: %s", ptr);
	
	if(!strncmp(ptr, "Content-Type: ", 14) || !strncmp(ptr, "Content-type: ", 14)) {
		free(curl->http_type);
		curl->http_type = NULL;
			
		if(!strncmp(ptr + 14, "image/", 6)) {
			printf("[+] CURL/Header: <image> mime type detected\n");
			curl->type = IMAGE_ALL;
			
			return size * nmemb;
		}
		
		if(!strncmp(ptr + 14, "text/html", 9)) {
			printf("[+] CURL/Header: <text/html> mime type detected\n");
			curl->type = TEXT_HTML;
			
			return size * nmemb;
		}
		
		printf("[ ] CURL/Header: match: %s", ptr);
		
		/* ignoring anything else */
		curl->type = UNKNOWN_TYPE;
		
		len = strlen(ptr + 14);
		curl->http_type = (char *) malloc(sizeof(char) * len + 1);
		
		strcpy(curl->http_type, ptr + 14);
		curl->http_type[len - 2] = '\0';
		
		printf("[-] CURL/Header: unhandled mime type: <%s>\n", curl->http_type);
	}
	
	if(!strncmp(ptr, "Content-Length: ", 16) || !strncmp(ptr, "Content-length: ", 16)) {
		curl->http_length = atoll(ptr + 16);
		printf("[+] CURL/Header: length %d bytes\n", curl->http_length);
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
	curl->data  = (char *) realloc(curl->data, (curl->length + 1));
	
	/* Appending data */
	memcpy(curl->data + prev, ptr, size * nmemb);
	
	/* Return required by libcurl */
	return size * nmemb;
}

static int curl_download_process(char *url, curl_data_t *data, char forcedl, char *post) {
	CURL *curl;
	char *useragent = CURL_USERAGENT;
	char *cookie;
	
	curl = curl_easy_init();
	
	data->data        = NULL;
	data->type        = UNKNOWN_TYPE;
	data->length      = 0;
	data->http_length = 0;
	data->charset     = UNKNOWN_CHARSET;
	data->http_type   = NULL;
	data->forcedl     = forcedl;
	
	printf("[+] CURL: %s\n", url);
	
	useragent = curl_useragent(url);
	
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_body);

		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10);
		
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, data);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_validate);
		
		curl_easy_setopt(curl, CURLOPT_HEADER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, useragent);
		
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		/* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); */
		
		if(post) {
			curl_easy_setopt(curl, CURLOPT_POST, 1);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
			printf("[+] CURL/Post: <%s>\n", post);
		}
		
		/* Checking Host for specific Cookies */
		if((cookie = curl_cookie(url)))
			curl_easy_setopt(curl, CURLOPT_COOKIE, cookie);
		
		data->curlcode = curl_easy_perform(curl);
		
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &(data->code));
		printf("[ ] CURL: code: %ld\n", data->code);
		
		curl_easy_cleanup(curl);
		
		if(!data->data && !data->http_length) {
			fprintf(stderr, "[-] CURL: data is empty.\n");
			return 1;
		}
		
		if(data->type == TEXT_HTML) {
			data->charset = url_extract_charset(data->data);
			printf("[ ] CURL: charset: %d\n", data->charset);
		}
		
	} else return 1;
	
	return 0;
}

/* wrapper */
int curl_download(char *url, curl_data_t *data, char forcedl) {
	return curl_download_process(url, data, forcedl, NULL);
}

int curl_download_post(char *url, curl_data_t *data, char *post) {
	return curl_download_process(url, data, 1, post);
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


void handle_repost(repost_type_t type, char *url, char *chan, char *nick, time_t ts, int hit) {
	char timestring[64];
	struct tm * timeinfo;
	char op[48], msg[512];
	
	strcpy(op, (char *) nick);
	
	if(ts != 0) {
		timeinfo = localtime(&ts);
		sprintf(timestring, "%02d/%02d/%02d %02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon + 1, (timeinfo->tm_year + 1900 - 2000), timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
		
	} else strcpy(timestring, "unknown");

	switch(type) {
		case URL_MATCH:
			sprintf(msg, "PRIVMSG %s :(url match, OP is %s (%s), hit %d times)", chan, anti_hl(op), timestring, hit + 1);
		break;
		
		case CHECKSUM_MATCH:
			snprintf(msg, sizeof(msg), "PRIVMSG %s :(checksum match, OP is %s (%s), hit %d times. URL waz: %s)", chan, anti_hl(op), timestring, hit + 1, url);
		break;
		
		case TITLE_MATCH:
			snprintf(msg, sizeof(msg), "PRIVMSG %s :(title match on database, OP is %s, at %s. URL waz: %s)", chan, anti_hl(op), timestring, url);
		break;
	}

	raw_socket(msg);
}

void check_round_count(ircmessage_t *message) {
	sqlite3_stmt *stmt;
	char *sqlquery, msg[128];
	int row, urls = -1;

	sqlquery = sqlite3_mprintf("SELECT count(id) FROM url WHERE chan = '%q'", message->chan);
	
	if((stmt = db_select_query(sqlite_db, sqlquery))) {		
		while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
			if(row == SQLITE_ROW)
				urls = sqlite3_column_int(stmt, 0);
		}
		
		printf("[ ] URL: %d\n", urls);
		if(urls % 500 == 0) {
			sprintf(msg, "PRIVMSG %s :We just reached %d urls !", message->chan, urls);
			raw_socket(msg);
		}
		
	} else fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
}

int handle_url(ircmessage_t *message, char *url) {
	sqlite3_stmt *stmt;
	char *sqlquery, op[48];
	const unsigned char *_op = NULL;
	int row, id = -1, hit = 0;
	char match = 0;
	time_t ts = 0;
	
	
	printf("[+] URL Parser: %s\n", url);
		
	if(strlen(url) > 256) {
		printf("[-] URL Parser: URL too long...\n");
		return 2;
	}
	
	
	/*
	 *   Quering database
	 */
	sqlquery = sqlite3_mprintf("SELECT id, nick, hit, time FROM url WHERE url = '%q' AND chan = '%q'", url, message->chan);
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL) {
		fprintf(stderr, "[-] URL Parser: cannot select url\n");
		return 1;
	}
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW) {
			id  = sqlite3_column_int(stmt, 0);
			_op = sqlite3_column_text(stmt, 1);
			hit = sqlite3_column_int(stmt, 2);
			ts  = sqlite3_column_int(stmt, 3);
			
			strncpy(op, (char *) _op, sizeof(op));
			op[sizeof(op) - 1] = '\0';
		}
	}
	
	/* Updating Database */
	if(id > -1) {
		if(strcmp(op, message->nick))
			handle_repost(URL_MATCH, NULL, message->chan, op, ts, hit);
			
		match = 1;
		
		printf("[+] URL Parser: URL already on database, updating...\n");
		sqlquery = sqlite3_mprintf("UPDATE url SET hit = hit + 1 WHERE id = %d", id);
		
	} else {
		printf("[+] URL Parser: New url, inserting...\n");
		sqlquery = sqlite3_mprintf("INSERT INTO url (nick, url, hit, time, chan) VALUES ('%q', '%q', 1, %d, '%q')", message->nick, url, time(NULL), message->chan);
	}
	
	if(!db_simple_query(sqlite_db, sqlquery))
		printf("[-] URL Parser: cannot update db\n");
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	check_round_count(message);
	
	/*
	 *   Dispatching url handling if not a repost
	 */
	return handle_url_dispatch(url, message, match);
}

char * sha1_string(unsigned char *sha1_hexa, char *sha1_char) {
	char sha1_hex[3];
	unsigned int i;
	
	*sha1_char = '\0';
	
	for(i = 0; i < SHA_DIGEST_LENGTH; i++) {
		sprintf(sha1_hex, "%02x", sha1_hexa[i]);
		strcat(sha1_char, sha1_hex);
	}
	
	return sha1_char;
}

int handle_url_dispatch(char *url, ircmessage_t *message, char already_match) {
	curl_data_t curl;
	char *title = NULL, *stripped = NULL;
	char *strcode, temp[256];
	unsigned int len;
	char *sqlquery, *request;
	sqlite3_stmt *stmt;
	
	unsigned char sha1_hexa[SHA_DIGEST_LENGTH];
	char sha1_char[(SHA_DIGEST_LENGTH * 2) + 1];
	
	const unsigned char *sqlurl = NULL, *nick = NULL;
	time_t ts = 0;
	int hit = -1, row;
	
	
	if(curl_download(url, &curl, 0))
		printf("[-] Warning: special download\n");
		
	printf("[+] Downloaded Length: %d\n", curl.length);
	printf("[+] Downloaded Type  : %d\n", curl.type);
	// printf("%s\n", curl.data);
	
	// Write error occure on data file
	if(curl.curlcode != CURLE_OK && curl.curlcode != CURLE_WRITE_ERROR) {
		zsnprintf(temp, "URL (Error %d): %s", curl.curlcode, curl_easy_strerror(curl.curlcode));
		irc_privmsg(message->chan, temp);

		free(curl.data);
		return 2;
	}

	if(!curl.data && !curl.http_length) {
		fprintf(stderr, "[-] URL/Dispatch: data is empty, this should not happen\n");
		return 2;
	}
	
	if(!curl.length && curl.type != UNKNOWN_TYPE) {
		/* realloc should not be occured, but not sure... */
		free(curl.data);
		return 2;
	}

	if(curl.type == TEXT_HTML) {
		/* Checking ignored hosts */
		/* for(i = 0; i < sizeof(__host_ignore) / sizeof(char *); i++) {
			if(strstr(url, __host_ignore[i])) {
				free(curl.data);
				return 3;
			}
		} */
		
		/* Extract title */
		if((title = url_extract_title(curl.data, title)) != NULL) {
			len = strlen(title) + 256;
			request = (char *) malloc(sizeof(char) * len);
			
			if(curl.code == 404)
				strcode = " (Error 404)";
				
			else strcode = "";
			
			/* Notify Title */
			stripped = anti_hl_each_words(title, strlen(title), curl.charset);
			decode_html_entities_utf8(stripped, NULL);
			
			snprintf(request, len, "PRIVMSG %s :URL%s: %s", message->chan, strcode, stripped);
			raw_socket(request);
			
			/* Check Title Repost */
			// redecoding entities for clear title update
			decode_html_entities_utf8(title, NULL);
			sqlquery = sqlite3_mprintf("SELECT url, nick, time, hit FROM url WHERE title = '%q' AND nick != '%q' AND chan = '%q' LIMIT 1", title, message->nick, message->chan);
			if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
				fprintf(stderr, "[-] URL Parser: cannot select url\n");
			
			while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
				if(row == SQLITE_ROW) {
					// Skip repost from same nick
					nick   = sqlite3_column_text(stmt, 1);					
					sqlurl = sqlite3_column_text(stmt, 0);
					ts     = sqlite3_column_int(stmt, 2);
					hit    = sqlite3_column_int(stmt, 3);
					
					if(curl.code == 200 && !already_match)
						handle_repost(TITLE_MATCH, (char *) sqlurl, message->chan, (char *) nick, ts, hit);
				}
				
				break;
			}
			
			// Clearing
			sqlite3_free(sqlquery);
			sqlite3_finalize(stmt);
			
			sqlquery = sqlite3_mprintf("UPDATE url SET title = '%q' WHERE url = '%q'", title, url);
			if(!db_simple_query(sqlite_db, sqlquery))
				printf("[-] URL Parser: cannot update db\n");
			
			/* Clearing */
			sqlite3_free(sqlquery);
			
			free(stripped);
			free(request);
			
		} else printf("[-] URL: Cannot extract title\n");
		
	} else if(curl.type == IMAGE_ALL) {
		if(curl.http_length > CURL_MAX_SIZE) {
			sprintf(temp, "PRIVMSG %s :(Warning: image size: %u Mo)", message->chan, curl.http_length / 1024 / 1024);
			raw_socket(temp);
			
			return 1;
		}
			
		if(!handle_url_image(url, &curl)) {
			/* Calculing data checksum */
			SHA1((unsigned char *) curl.data, curl.length, sha1_hexa);
			
			sqlquery = sqlite3_mprintf("UPDATE url SET sha1 = '%s' WHERE url = '%q'", sha1_string(sha1_hexa, sha1_char), url);
			if(!db_simple_query(sqlite_db, sqlquery))
				printf("[-] URL Parser: cannot update db\n");
				
			/* Clearing */
			sqlite3_free(sqlquery);
			
			/* Checking checksum repost */
			sqlquery = sqlite3_mprintf("SELECT url, nick, time, hit FROM url WHERE sha1 = '%s' AND url != '%q' AND chan = '%q'", sha1_char, url, message->chan);
			if((stmt = db_select_query(sqlite_db, sqlquery))) {
				while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
					if(row == SQLITE_ROW) {
						/* Skip repost from same nick */
						nick   = sqlite3_column_text(stmt, 1);
						
						if(!strcmp((char *) nick, message->nick))
							continue;
							
						sqlurl = sqlite3_column_text(stmt, 0);
						ts     = sqlite3_column_int(stmt, 2);
						hit    = sqlite3_column_int(stmt, 3);
						
						if(!already_match)
							handle_repost(CHECKSUM_MATCH, (char *) sqlurl, message->chan, (char *) nick, ts, hit);
					}
					
					break;
				}
			
			} else fprintf(stderr, "[-] URL Parser: cannot select url\n");
			
			/* Clearing */
			sqlite3_free(sqlquery);
			sqlite3_finalize(stmt);
		}
		
	} else if(curl.type == UNKNOWN_TYPE) {
		snprintf(temp, sizeof(temp), "PRIVMSG %s :Content: %s (%.2f Mo)", message->chan, curl.http_type, (double) curl.http_length / 1024 / 1024);
		raw_socket(temp);
		
		free(curl.http_type);
	}
	
	free(curl.data);
	
	return 0;
}

int handle_url_image(char *url, curl_data_t *curl) {
	FILE *fp;	
	char filename[512], temp[256];
	 
	/* Keep going on Image Support */	
	strcpy(temp, url + 7);
	clean_filename(temp);
	
	/* Open File */
	sprintf(filename, "%s/%s", OUTPUT_PATH, temp);
		
	if(!(fp = fopen(filename, "w"))) {
		perror(filename);
		return 1;
	}
	
	if(fwrite(curl->data, 1, curl->length, fp) != curl->length)
		perror("fwrite");
	
	fclose(fp);
	
	printf("[+] Image/CURL: File saved (%d / %s)\n", curl->length, temp);
	
	return 0;
}

char * url_extract_title(char *body, char *title) {
	char *read, *end;
	
	if((read = strcasestr(body, "<title"))) {
		while(*read != '>')
			read++;
			
		end = strcasestr(++read, "</title>");
		*end = '\0';
		
		title = rtrim(ltrim(crlftrim(read)));
		
		if(strlen(title) > 250)
			strcpy(title + 240, " [...]");
		
		if(!strlen(title))
			return NULL;
			
		printf("[+] Title: <%s>\n", title);
		
	} else return NULL;
	
	return title;
}

charset_t url_extract_charset(char *body) {
	char *charset, *limit;
	
	if((limit = strcasestr(body, "</head>"))) {
		if((charset = strcasestr(body, "charset="))) {
			if(charset > limit || strlen(charset) < 12)
				return UNKNOWN_CHARSET;
			
			/* Skipping beginning */
			charset += 8;
			
			if(!strncasecmp(charset, "iso-8859-1", 10) || !strncasecmp(charset + 1, "iso-8859-1", 10))
				return ISO_8859;
			
			if(!strncasecmp(charset, "utf-8", 5) || !strncasecmp(charset + 1, "utf-8", 5))
				return UTF_8;
			
			if(!strncasecmp(charset, "windows-1252", 5) || !strncasecmp(charset + 1, "windows-1252", 5))
				return WIN_1252;
			
			printf("[ ] Charset: Unknown: <%s>\n", charset);
		}
	}
	
	return UNKNOWN_CHARSET;
}

char * shurl(char *url) {
	char *baseurl = "http://x.maxux.net/index.php?url=", *request;
	curl_data_t curl;
	size_t len;
	
	len = (strlen(baseurl) + strlen(url)) + 8;
	request = (char *) malloc(sizeof(char) * len);
	
	sprintf(request, "%s%s", baseurl, url);
	
	if(curl_download_text(request, &curl)) {
		free(request);
		return NULL;
	}
	
	if(curl.length > len)
		curl.length = len - 1;
	
	strncpy(request, curl.data, curl.length);
	request[curl.length] = '\0';
	
	free(curl.data);
	
	printf("[+] Shurl: <%s>\n", request);
	
	return request;
}
