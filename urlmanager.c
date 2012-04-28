#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netdb.h>
#include <curl/curl.h>
#include <ctype.h>
#include <sqlite3.h>
#include "imagespawn.h"
#include "bot.h"
#include "entities.h"
#include "database.h"
#include "urlmanager.h"

char *__host_ignore[] = {
	"www.maxux.net/",
	"paste.maxux.net/",
	"git.maxux.net/"
};

char * clean_filename(char *file) {
	int i = 0;
	
	while(*(file + i)) {
		if(*(file + i) == ':' || *(file + i) == '/' || *(file + i) == '#' || *(file + i) == '?')
			*(file + i) = '_';
		
		i++;
	}
	
	return file;
}

char * trim(char *str, unsigned int len) {
	char *read, *write;
	unsigned int i;
	
	read  = str;
	write = str;
	
	/* Strip \n */
	for(i = 0; i < len; i++) {
		if(*read != '\n') {
			*write = *read;
			write++;
		}
		
		read++;
	}
	
	/* New line limit */
	*write = '\0';
	
	/* Trim spaces before/after */
	read  = str;
	write = str;
	
	/* Before */
	while(isspace(*read))
		read++;
	
	/* Copy */
	while(*read)
		*write++ = *read++;
	
	/* After */
	while(isspace(*write))
		write--;
		
	*write-- = '\0';
	
	return str;
}

char * anti_hl(char *nick) {
	nick[strlen(nick) - 1] = '_';
	return nick;
}

char *extract_url(char *url) {
	int i = 0;
	char *out;
	
	while(*(url + i) && *(url + i) != ' ')
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
		
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, data);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_validate);
		
		curl_easy_setopt(curl, CURLOPT_HEADER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, CURL_USERAGENT);
		
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
		/* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); */
		
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		
	} else return 1;
	
	return 0;
}




int handle_url(char *nick, char *url) {
	sqlite3_stmt *stmt;
	char *sqlquery, msg[256], op[32];
	const unsigned char *_op = NULL;
	int row, id = -1, hit = 0;
	char skip_analyse = 0;
	
	printf("[+] URL Parser: %s\n", url);
		
	if(strlen(url) > 256) {
		printf("[-] URL Parser: URL too long...\n");
		return 2;
	}
	
	
	/*
	 *   Quering database
	 */
	sqlquery = sqlite3_mprintf("SELECT id, nick, hit FROM url WHERE url = '%q'", url);
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW) {
			id  = sqlite3_column_int(stmt, 0);
			_op  = sqlite3_column_text(stmt, 1);
			hit  = sqlite3_column_int(stmt, 2);
			
			strncpy(op, (char*) _op, sizeof(op));
			op[sizeof(op) - 1] = '\0';
		}
	}
	
	/* Updating Database */
	if(id > -1) {
		/* Repost ! */
		if(strncmp(nick, op, strlen(op))) {
			skip_analyse = 1;
			
			sprintf(msg, "PRIVMSG " IRC_CHANNEL " :Repost, OP is %s, hit %d times.", anti_hl(op), hit + 1);
			raw_socket(sockfd, msg);
			
		} else skip_analyse = 0;
		
		printf("[+] URL Parser: URL already on database, updating...\n");
		sqlquery = sqlite3_mprintf("UPDATE url SET hit = hit + 1 WHERE id = %d", id);
		
	} else {
		printf("[+] URL Parser: New url, inserting...\n");
		sqlquery = sqlite3_mprintf("INSERT INTO url (nick, url, hit) VALUES ('%q', '%q', 1)", nick, url);
	}
	
	if(!db_simple_query(sqlite_db, sqlquery))
		printf("[-] URL Parser: cannot update db\n");
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	
	/*
	 *   Dispatching url handling if not a repost
	 */
	return (!skip_analyse) ? handle_url_dispatch(url) : 0;
}

int handle_url_dispatch(char *url) {
	curl_data_t curl;
	char title[256];
	char request[384];
	unsigned int i;
	
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
		for(i = 0; i < sizeof(__host_ignore) / sizeof(char*); i++) {
			if(strstr(url, __host_ignore[i])) {
				free(curl.data);
				return 3;
			}
		}
		
		/* Extract title */
		if(url_extract_title(curl.data, title, sizeof(title))) {
			decode_html_entities_utf8(title, NULL);
			
			sprintf(request, "PRIVMSG " IRC_CHANNEL " :URL: %s", title);
			raw_socket(sockfd, request);
			
		} else printf("[-] URL: Cannot extract title\n");
		
	} else if(curl.type == IMAGE_ALL) {
		handle_url_image(url, &curl);
		
	}
	
	free(curl.data);
	
	return 0;
}

int handle_url_html(char *url, char *nick) {
	nick = NULL;
	
	// if(((youtube = strstr(url, "youtube.com/")) || (youtube = strstr(url, "youtu.be/"))) && (youtube - url) < 15) {
		handle_url_title(url);
		// return 0;
	// }
	
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
	// remove(filename);
	
	return 0;
}

int url_extract_title(char *body, char *title, size_t maxsize) {
	char *read, *write;
	unsigned int len;
	
	if((read = strstr(body, "<title>"))) {
		write = read + 7;
		
		/* Calculing length */
		while(*write && *write != '<')
			write++;
		
		len = write - read - 7;
		
		if(len > maxsize)
			len = maxsize;
		
		/* Copying title */
		strncpy(title, read + 7, len);
		title[len] = '\0';
		
		/* Stripping carriege return */
		trim(title, len);
		printf("<%s>\n", title);
		
		printf("[+] Title: %s\n", title);
		
	} else return 0;
	
	return 1;
}

void handle_url_title(char *url) {
	curl_data_t curl;
	char title[256];
	char request[384];
	
	curl_download(url, &curl);
	// printf("%s\n", curl.data);

	if(curl.length && url_extract_title(curl.data, title, sizeof(title))) {
		decode_html_entities_utf8(title, NULL);
		
		sprintf(request, "PRIVMSG " IRC_CHANNEL " :URL: %s", title);
		raw_socket(sockfd, request);
		
	} else printf("[-] URL Title: Cannot extract url\n");
	
	free(curl.data);
}
