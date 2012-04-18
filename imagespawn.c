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

int sockfd;
sqlite3 *sqlite_db;

char *repost_msg[] = {"Repost", "Repost, n00b", "-> GTFO"};

/*
 * SQLite Management
 */
sqlite3 * db_sqlite_init() {
	sqlite3 *db;
	
	printf("[+] SQLite: Initializing <%s>\n", SQL_DATABASE_FILE);
	
	if(sqlite3_open(SQL_DATABASE_FILE, &db) != SQLITE_OK) {
		fprintf(stderr, "[+] SQLite: cannot open sqlite databse: <%s>\n", sqlite3_errmsg(db));
		return NULL;
	}
	
	return db;
}

sqlite3_stmt * db_select_query(sqlite3 *db, char *sql) {
	sqlite3_stmt *stmt;
	
	/* Debug SQL */
	printf("[+] SQLite: <%s>\n", sql);
	
	/* Query */
	if(sqlite3_prepare_v2(db, sql, strlen(sql) + 1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "[+] SQLite: query <%s> failed: %s\n", sql, sqlite3_errmsg(db));
		return NULL;
	}
	
	return stmt;
}

int db_simple_query(sqlite3 *db, char *sql) {
	/* Debug SQL */
	printf("[+] SQLite: <%s>\n", sql);
	
	/* Query */
	if(sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
		fprintf(stderr, "[+] SQLite: query <%s> failed: %s\n", sql, sqlite3_errmsg(db));
		return 0;
	}
	
	return 1;
}


int db_sqlite_parse(sqlite3 *db) {
	sqlite3_stmt *stmt;
	int i, row;
	
	/* Reading table content */
	if((stmt = db_select_query(db, "SELECT * FROM url")) == NULL)
		return 0;
	
	/* Counting... */
	i = 0;
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW)
			i++;
	}
	
	/* Clearing */
	sqlite3_finalize(stmt);
	
	printf("[+] SQLite: url parsed: %d rows returned\n", i);
	
	return 1;
}

/*
 * cURL
 */

size_t curl_header(char *ptr, size_t size, size_t nmemb, void *userdata) {
	userdata = NULL;
	
	if(!(size * nmemb))
		return 0;
	
	if(!strncmp(ptr, "Content-Type: ", 14) || !strncmp(ptr, "Content-type: ", 14)) {
		printf("[+] Image/CURL: %s", ptr);
		
		if(strncmp(ptr, "Content-Type: image/", 20) != 0 && strncmp(ptr, "Content-type: image/", 20) != 0) {
			printf("[+] Image/CURL: Ignoring file...\n");
			return 0;
		}
	}

	/* Return required by libcurl */
	return size * nmemb;
}

size_t curl_body(char *ptr, size_t size, size_t nmemb, void *userdata) {
	curl_data_t *curl = (curl_data_t*) userdata;
	
	curl->length += (size * nmemb);
	
	/* Resize data */
	if(!curl->data) {
		curl->data  = (char*) malloc(sizeof(char) * (curl->length + 32));
		*curl->data = '\0';
		
	} else curl->data  = (char*) realloc(curl->data, (curl->length + 32));
	
	/* Appending data */
	strncat(curl->data, ptr, (size * nmemb));
	*(curl->data + curl->length) = '\0';
	
	/* Return required by libcurl */
	return size * nmemb;
}

int curl_download(char *url, curl_data_t *data) {
	CURL *curl;
	CURLcode res;
	
	curl = curl_easy_init();
	
	data->data   = NULL;
	data->length = 0;
	
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_body);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		
		curl_easy_setopt(curl, CURLOPT_HEADER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, CURL_USERAGENT);
		
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
		
		// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
		
		res = curl_easy_perform(curl);

		curl_easy_cleanup(curl);
		
	} else return 1;
	
	return 0;
}

int handle_url(char *nick, char *url) {
	FILE *fp;
	CURL *curl;
	CURLcode res;
	sqlite3_stmt *stmt;
	char filename[512], temp[256], *youtube;
	char *sqlquery, msg[256], op[32];
	const unsigned char *_op = NULL;
	int row, id = -1, hit = 0;
	
	printf("[+] URL Parser: %s\n", url);
		
	if(strlen(url) > 256) {
		printf("[-] URL Parser: URL too long...\n");
		return 2;
	}
	
	
	/* Quering database */
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
		sprintf(msg, "PRIVMSG " IRC_CHANNEL " :Repost, OP is %s, hit %d times.", anti_hl(op), hit + 1);
		raw_socket(sockfd, msg);
		
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
	
	
	
	/* Check YouTube URL */
	if(((youtube = strstr(url, "youtube.com/")) || (youtube = strstr(url, "youtu.be/"))) && (youtube - url) < 15) {
		handle_youtube(url);
		return 0;
	}
	
	/* Keep going on Image Support */
	curl = curl_easy_init();
	strcpy(temp, url + 7);
	clean_filename(temp);
	
	/* Open File */
	sprintf(filename, "%s/%s", OUTPUT_PATH, temp);
	
	/* Check repost */
	fp = fopen(filename, "r");
	if(fp) {
		printf("[+] Image: Repost\n");
		
		/* sprintf(filename, "PRIVMSG %s :%s", IRC_CHANNEL, repost());
		raw_socket(sockfd, filename); */
		
		fclose(fp);
		return 3;
	}
	
	fp = fopen(filename, "w");
	if(!fp) {
		perror(filename);
		return 1;
	}
	
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		
		// curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fp);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header);
		
		curl_easy_setopt(curl, CURLOPT_HEADER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, CURL_USERAGENT);
		
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
		
		// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
		
		res = curl_easy_perform(curl);

		curl_easy_cleanup(curl);
		
	} else return 1;
	
	fclose(fp);
	
	if(res == CURLE_OK) {
		printf("[+] Image/CURL: File saved (%s)\n", temp);
		
	} else remove(filename);
	
	return 0;
}

/*
 * YouTube
 */
int youtube_extract_title(char *body, char *title) {
	char *read, *write;
	unsigned int len;
	
	if((read = strstr(body, "<title>"))) {
		write = read + 7;
		// printf(">> %s\n", write);
		
		/* Calculing length */
		while(*write && *write != '<')
			write++;
		
		len = write - read - 7;
		
		/* Copying title */
		strncpy(title, read + 7, len);
		title[len] = '\0';
	
		/* Removing " - YouTube" */
		read  = title;
		write = strstr(read, " - YouTube");
		
		if(write) {		
			title[write - read] = '\0';
			len = strlen(title);
		}
		
		/* Stripping carriege return */
		trim(title, len);
		
		printf("[+] Title: %s\n", title);
		
	} else return 0;
	
	return 1;
}

void handle_youtube(char *url) {
	curl_data_t curl;
	char title[256];
	char request[384];
	
	curl_download(url, &curl);
	// printf("%s\n", curl.data);

	if(curl.length && youtube_extract_title(curl.data, title)) {
		sprintf(request, "PRIVMSG " IRC_CHANNEL " :YouTube: %s", title);
		raw_socket(sockfd, request);
		
	} else printf("[-] YouTube: Cannot extract url\n");
	
	free(curl.data);
}


/*
 * Misc
 */
void debug(char *d) {
	int i;
	
	for(i = 0; i < 100; i++)
		printf("%c ", (unsigned char) *(d + i));
	
	printf("\n");
}

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
	
	/* Trim spaces before */
	read  = str;
	write = str;
	
	while(isspace(*read))
		read++;
	
	while(*read)
		*write++ = *read++;
	
	write--;
	while(isspace(*write))
		*write-- = '\0';
	
	return str;
}

char * anti_hl(char *nick) {
	nick[strlen(nick) - 1] = '_';
	return nick;
}

/*
 * Image Handling
 */
char * repost() {
	return repost_msg[rand() % sizeof(repost_msg) / sizeof(char*)];
}


/*
 * Chat Handling
 */
void handle_private_message(char *data) {
	char remote[128], *request;
	char *diff = NULL;
	unsigned char length;
	
	if((diff = strstr(data, "PRIVMSG"))) {
		length = diff - data - 1;
		
		strncpy(remote, data, length);
		remote[length] = '\0';
		
	} else return;
	
	if(!strcmp(remote, IRC_ADMIN_HOST)) {
		request = strstr(data, ":");
		
		if(request++) {
			printf("[+] Admin <%s> request: <%s>\n", remote, request);
			raw_socket(sockfd, request);
		}
		
	} else printf("[-] Host <%s> is not admin\n", remote);
}

/*
 * IRC Protocol
 */
int init_irc_socket(char *server, int port) {
	int sockfd = -1, connresult;
	struct sockaddr_in server_addr;
	struct hostent *he;
	
	/* Resolving name */
	if((he = gethostbyname(server)) == NULL)
		perror("[-] IRC: gethostbyname");
	
	bcopy(he->h_addr, &server_addr.sin_addr, he->h_length);

	server_addr.sin_family = AF_INET; 
	server_addr.sin_port = htons(port);

	/* Creating Socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		perror("[-] IRC: socket");

	/* Init Connection */
	connresult = connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
	
	if(connresult < 0)
		perror("[-] IRC: connect");
	
	return sockfd;
}

void raw_socket(int sockfd, char *message) {
	char *sending = (char*) malloc(sizeof(char*) * strlen(message) + 3);
	
	printf("[+] IRC: << %s\n", message);
	
	strcpy(sending, message);
	strcat(sending, "\r\n");
	
	if(send(sockfd, sending, strlen(sending), 0) == -1)
		perror("[-] IRC: send");
	
	free(sending);
}

int read_socket(int sockfd, char *data, char *next) {
	char buff[MAXBUFF];
	int rlen, i, tlen;
	char *temp = NULL;
	
	buff[0] = '\0';		// Be sure that buff is empty
	data[0] = '\0';		// Be sure that data is empty
	
	while(1)  {
		free(temp);
		temp = (char*) malloc(sizeof(char*) * (strlen(next) + MAXBUFF));
		
		tlen = sprintf(temp, "%s%s", next, buff);
		
		for(i = 0; i < tlen; i++) {			// Checking if next (+buff), there is not \r\n
			if(temp[i] == '\r' && temp[i+1] == '\n') {
				strncpy(data, temp, i);		// Saving Current Data
				data[i] = '\0';
				
				if(temp[i+2] != '\0' && temp[i+1] != '\0' && temp[i] != '\0') {		// If the paquet is not finished, saving the rest
					strncpy(next, temp+i+2, tlen - i);
					
				} else next[0] = '\0';
				
				free(temp);
				return 0;
			}
		}
		
		rlen = recv(sockfd, buff, MAXBUFF, 0);
		buff[rlen] = '\0';
	}
	
	return 0;
}

char *skip_server(char *data) {
	int i, j;
	
	j = strlen(data);
	for(i = 0; i < j; i++)
		if(*(data+i) == ' ')
			return data + i + 1;
	
	return NULL;
}

size_t extract_nick(char *data, char *destination, size_t size) {
	size_t len = 0;
	size_t max = 0;
	
	while(*(data + len) && *(data + len) != '!')
		len++;
	
	max = (len > size) ? size : len;
	strncpy(destination, data, max);
	
	destination[max] = '\0';
	
	return len;
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

int main(void) {
	char *data = (char*) malloc(sizeof(char*) * (2 * MAXBUFF));
	char *next = (char*) malloc(sizeof(char*) * (2 * MAXBUFF));
	char *request, *url, *trueurl, nick[32];
	
	printf("[+] Core: Starting...\n");
	
	sqlite_db = db_sqlite_init();
	if(!db_sqlite_parse(sqlite_db))
		return 1;
	
	sockfd = init_irc_socket("192.168.20.1", 6667);
	
	while(1) {
		read_socket(sockfd, data, next);
		printf("[ ] IRC: >> %s\n", data);
		
		if((request = skip_server(data)) == NULL) {
			printf("[-] IRC: Something wrong with protocol...\n");
			continue;
		}
		
		if(!strncmp(request, "NOTICE AUTH", 11)) {
			raw_socket(sockfd, "NICK " IRC_NICK);
			raw_socket(sockfd, "USER " IRC_USERNAME " " IRC_USERNAME " " IRC_USERNAME " :" IRC_REALNAME);
			break;
		}
	}
	
	while(1) {
		read_socket(sockfd, data, next);
		printf("[ ] IRC: >> %s\n", data);
		
		if((request = skip_server(data)) == NULL) {
			printf("[-] IRC: Something wrong with protocol...\n");
			continue;
		}

		if(!strncmp(data, "PING", 4)) {
			data[1] = 'O';		/* pOng */
			raw_socket(sockfd, data);
			continue;
		}
		
		if(!strncmp(request, "376", 3)) {
			if(IRC_NICKSERV)
				raw_socket(sockfd, "PRIVMSG NickServ :IDENTIFY " IRC_NICKSERV_PASS);
			
			else raw_socket(sockfd, "JOIN " IRC_CHANNEL);
			
			continue;
		}
		
		if(!strncmp(request, "MODE", 4)) {
			/* Bot identified */
			if(!strncmp(request, "MODE " IRC_NICK " :+r", 9 + sizeof(IRC_NICK)) && IRC_NICKSERV)
				raw_socket(sockfd, "JOIN " IRC_CHANNEL);
		}
		
		if(!strncmp(request, "PRIVMSG #", 9)) {
			if((url = strstr(request + 8, "http://")) || (url = strstr(request + 8, "https://"))) {
				if(!(trueurl = extract_url(url))) {
					fprintf(stderr, "[-] URL: Cannot extact url\n");
					continue;
				}
				
				extract_nick(data + 1, nick, sizeof(nick));
				handle_url(nick, trueurl);
				
				free(trueurl);
			}
			
			continue;
		}
		
		if(!strncmp(request, "PRIVMSG " IRC_NICK, sizeof("PRIVMSG " IRC_NICK) - 1)) {
			handle_private_message(data + 1);
			continue;
		}
	}
	 
	free(data);
	free(next);
	
	return 0;
}
