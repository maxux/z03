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
#include "imagespawn.h"
#include "bot.h"
#include "entities.h"
#include "database.h"
#include "urlmanager.h"

int sockfd;

void handle_stats(char *data) {
	sqlite3_stmt *stmt;
	char *sqlquery = "SELECT count(id) FROM url", msg[256];
	int count = 0, row;
	
	// Fix warning
	data = NULL;
	
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW)
			count  = sqlite3_column_int(stmt, 0);
	}
	
	sprintf(msg, "PRIVMSG " IRC_CHANNEL " :Got %d url on database", count);
	raw_socket(sockfd, msg);
	
	/* Clearing */
	sqlite3_finalize(stmt);
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

void handle_message(char *data) {
	char *message;
	
	message = data;
	while(*message && *message != ':')
		message++;
	
	if(!strncmp(++message, ".stats", 6))
		handle_stats(data);
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
				
				continue;
			}
			
			handle_message(data + 1);
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
