/* z03 - small bot with some network features - url handling/mirroring/management
 * Author: Daniel Maxime (maxux.unix@gmail.com)
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
#include "core.h"
#include "bot.h"
#include "entities.h"
#include "database.h"
#include "urlmanager.h"
#include "ircmisc.h"


char *__host_ignore[] = {NULL
	/* "www.maxux.net/",
	"paste.maxux.net/",
	"git.maxux.net/" */
};

char *extract_url(char *url) {
	int i = 0;
	char *out;
	
	// TODO: Matching (xxx)
	while(*(url + i) && *(url + i) != ' ' && *(url + i) != ')')
		i++;
	
	if(!(out = (char*) malloc(sizeof(char) * i + 1)))
		return NULL;
	
	strncpy(out, url, i);
	out[i] = '\0';
	
	return out;
}

size_t curl_header_validate(char *ptr, size_t size, size_t nmemb, void *userdata) {
	curl_data_t *curl = (curl_data_t*) userdata;
	
	if(!(size * nmemb))
		return 0;
	
	if(!strncmp(ptr, "Content-Type: ", 14) || !strncmp(ptr, "Content-type: ", 14)) {
		if(!strncmp(ptr + 14, "image/", 6)) {
			printf("[+] CURL/Header: image mime type detected\n");
			curl->type = IMAGE_ALL;
			
			return size * nmemb;
		}
		
		if(!strncmp(ptr + 14, "text/html", 9)) {
			printf("[+] CURL/Header: text/html mime type detected\n");
			curl->type = TEXT_HTML;
			
			return size * nmemb;
		}
		
		/* ignoring anything else */
		curl->type = UNKNOWN;
		printf("[-] CURL/Header: unknown mime type, reject it.\n");
		
		return 0;
	}

	/* Return required by libcurl */
	return size * nmemb;
}

size_t curl_body(char *ptr, size_t size, size_t nmemb, void *userdata) {
	curl_data_t *curl = (curl_data_t*) userdata;
	size_t prev;
	
	prev = curl->length;
	curl->length += (size * nmemb);
	
	if(curl->length > CURL_MAX_SIZE)
		return 0;
	
	/* Resize data */
	curl->data  = (char*) realloc(curl->data, (curl->length + 32));
	
	/* Appending data */
	memcpy(curl->data + prev, ptr, size * nmemb);
	
	/* Return required by libcurl */
	return size * nmemb;
}

int curl_download(char *url, curl_data_t *data) {
	CURL *curl;
	CURLcode res;
	
	curl = curl_easy_init();
	
	data->data   = NULL;
	data->type   = UNKNOWN;
	data->length = 0;
	
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
		curl_easy_setopt(curl, CURLOPT_USERAGENT, CURL_USERAGENT);
		
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
		/* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); */
		
		res = curl_easy_perform(curl);
		
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &(data->code));
		printf("[ ] CURL: HTTP_REPONSE_CODE: %ld\n", data->code);
		
		curl_easy_cleanup(curl);
		
	} else return 1;
	
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
			sprintf(msg, "PRIVMSG %s :Repost (url match), OP is %s (%s), hit %d times.", chan, anti_hl(op), timestring, hit + 1);
		break;
		
		case CHECKSUM_MATCH:
			snprintf(msg, sizeof(msg), "PRIVMSG %s :Repost (checksum match), OP is %s (%s), hit %d times. URL waz: %s", chan, anti_hl(op), timestring, hit + 1, url);
		break;
		
		case TITLE_MATCH:
			snprintf(msg, sizeof(msg), "PRIVMSG %s :Repost (title match), OP is %s (%s), hit %d times. URL waz: %s", chan, anti_hl(op), timestring, hit + 1, url);
		break;
	}

	raw_socket(sockfd, msg);
}

void check_round_count() {
	sqlite3_stmt *stmt;
	char *sqlquery, msg[128];
	int row, urls = -1;

	sqlquery = "SELECT count(id) FROM url";
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW)
			urls = sqlite3_column_int(stmt, 0);
	}
	
	printf("[ ] URL: %d\n", urls);
	if(urls % 500 == 0) {
		sprintf(msg, "PRIVMSG " IRC_CHANNEL " :We just reached %d urls !", urls);
		raw_socket(sockfd, msg);
	}
	
	sqlite3_finalize(stmt);
}

int handle_url(char *nick, char *url) {
	sqlite3_stmt *stmt;
	char *sqlquery, op[48];
	const unsigned char *_op = NULL;
	int row, id = -1, hit = 0;
	char skip_analyse = 0;
	
	time_t ts = 0;
	
	
	printf("[+] URL Parser: %s\n", url);
		
	if(strlen(url) > 256) {
		printf("[-] URL Parser: URL too long...\n");
		return 2;
	}
	
	
	/*
	 *   Quering database
	 */
	sqlquery = sqlite3_mprintf("SELECT id, nick, hit, time FROM url WHERE url = '%q'", url);
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
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
		/* Repost ! */
		if(strncmp(nick, op, strlen(op))) {
			skip_analyse = 1;
			handle_repost(URL_MATCH, NULL, IRC_CHANNEL, op, ts, hit);
			
		} else skip_analyse = 0;
		
		printf("[+] URL Parser: URL already on database, updating...\n");
		sqlquery = sqlite3_mprintf("UPDATE url SET hit = hit + 1 WHERE id = %d", id);
		
	} else {
		printf("[+] URL Parser: New url, inserting...\n");
		sqlquery = sqlite3_mprintf("INSERT INTO url (nick, url, hit, time) VALUES ('%q', '%q', 1, %d)", nick, url, time(NULL));
	}
	
	if(!db_simple_query(sqlite_db, sqlquery))
		printf("[-] URL Parser: cannot update db\n");
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	check_round_count();
	
	/*
	 *   Dispatching url handling if not a repost
	 */
	return (!skip_analyse) ? handle_url_dispatch(url, op) : 0;
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

int handle_url_dispatch(char *url, char *post_nick) {
	curl_data_t curl;
	char *title = NULL, *stripped = NULL;
	char *strcode;
	unsigned int len;
	// unsigned char i;
	char *sqlquery, *request;
	sqlite3_stmt *stmt;
	
	unsigned char sha1_hexa[SHA_DIGEST_LENGTH];
	char sha1_char[(SHA_DIGEST_LENGTH * 2) + 1];
	
	const unsigned char *sqlurl = NULL, *nick = NULL;
	time_t ts = 0;
	int hit = -1, row;
	char notif_it = 1;
	
	
	if(curl_download(url, &curl))
		return 1;
		
	printf("[+] Downloaded Length: %d\n", curl.length);
	printf("[+] Downloaded Type  : %d\n", curl.type);
	// printf("%s\n", curl.data);
	
	if(!curl.length || curl.type == UNKNOWN) {
		/* realloc should not be occured, but not sure... */
		free(curl.data);
		return 2;
	}

	if(curl.type == TEXT_HTML) {
		/* Checking ignored hosts */
		/* for(i = 0; i < sizeof(__host_ignore) / sizeof(char*); i++) {
			if(strstr(url, __host_ignore[i])) {
				free(curl.data);
				return 3;
			}
		} */
		
		/* Extract title */
		if((title = url_extract_title(curl.data, title)) != NULL) {
			decode_html_entities_utf8(title, NULL);
			
			len = strlen(title) + 256;
			request = (char*) malloc(sizeof(char) * len);
			
			if(curl.code == 404)
				strcode = " (Error 404)";
				
			else strcode = "";
			
			/* Check Title Repost */
			/* sqlquery = sqlite3_mprintf("SELECT url, nick, time, hit FROM url WHERE title = '%q'", title);
			if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
				fprintf(stderr, "[-] URL Parser: cannot select url\n");
			
			while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
				if(row == SQLITE_ROW) {
					// Skip repost from same nick
					nick   = sqlite3_column_text(stmt, 1);
					if(!strcmp((char *) nick, post_nick))
						continue;
					
					sqlurl = sqlite3_column_text(stmt, 0);
					ts     = sqlite3_column_int(stmt, 2);
					hit    = sqlite3_column_int(stmt, 3);
					
					if(curl.code == 200)
						handle_repost(TITLE_MATCH, (char *) sqlurl, IRC_CHANNEL, (char *) nick, ts, hit);
					
					notif_it = 0;
				}
				
				break;
			}
			
			// Clearing
			sqlite3_free(sqlquery);
			sqlite3_finalize(stmt);
			*/
			
			/* Notify Title */
			if(notif_it) {
				stripped = anti_hl_each_words(title, strlen(title));
				
				snprintf(request, len, "PRIVMSG " IRC_CHANNEL " :URL%s: %s", strcode, stripped);
				raw_socket(sockfd, request);
				
				free(stripped);
			}
			
			sqlquery = sqlite3_mprintf("UPDATE url SET title = '%q' WHERE url = '%q'", title, url);
			if(!db_simple_query(sqlite_db, sqlquery))
				printf("[-] URL Parser: cannot update db\n");
			
			/* Clearing */
			sqlite3_free(sqlquery);
			free(title);
			free(request);
			
		} else printf("[-] URL: Cannot extract title\n");
		
	} else if(curl.type == IMAGE_ALL) {
		if(!handle_url_image(url, &curl)) {
			/* Calculing data checksum */
			SHA1((unsigned char *) curl.data, curl.length, sha1_hexa);
			
			sqlquery = sqlite3_mprintf("UPDATE url SET sha1 = '%s' WHERE url = '%q'", sha1_string(sha1_hexa, sha1_char), url);
			if(!db_simple_query(sqlite_db, sqlquery))
				printf("[-] URL Parser: cannot update db\n");
				
			/* Clearing */
			sqlite3_free(sqlquery);
			
			/* Checking checksum repost */
			sqlquery = sqlite3_mprintf("SELECT url, nick, time, hit FROM url WHERE sha1 = '%s' AND url != '%q'", sha1_char, url);
			if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
				fprintf(stderr, "[-] URL Parser: cannot select url\n");
			
			while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
				if(row == SQLITE_ROW) {
					/* Skip repost from same nick */
					nick   = sqlite3_column_text(stmt, 1);
					if(!strcmp((char *) nick, post_nick))
						continue;
						
					sqlurl = sqlite3_column_text(stmt, 0);
					ts     = sqlite3_column_int(stmt, 2);
					hit    = sqlite3_column_int(stmt, 3);
					
					handle_repost(CHECKSUM_MATCH, (char *) sqlurl, IRC_CHANNEL, (char *) nick, ts, hit);
				}
				
				break;
			}
			
			/* Clearing */
			sqlite3_free(sqlquery);
			sqlite3_finalize(stmt);
		}
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
		
	fp = fopen(filename, "w");
	if(!fp) {
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
	char *read, *write;
	unsigned int len;
	
	if((read = strstr(body, "<title>")) || (read = strstr(body, "<TITLE>"))) {
		write = read + 7;
		
		/* Calculing length */
		while(*write && *write != '<')
			write++;
		
		len = write - read - 7;
		title = (char*) malloc(sizeof(char) * len + 1);
		
		/* Copying title */
		strncpy(title, read + 7, len);
		title[len] = '\0';
		
		if(strlen(title) > 450)
			strcpy(title + 440, " [...]");
		
		/* Stripping carriege return */
		trim(title, strlen(title));
		printf("[+] Title: <%s>\n", title);
		
	} else return NULL;
	
	return title;
}
