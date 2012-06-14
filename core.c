/* z03 - small bot with some network features
 * Author: Daniel Maxime (maxux.unix@gmail.com)
 * Contributor: Darky, mortecouille, RaphaelJ, somic, ghilan
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
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netdb.h>
#include <curl/curl.h>
#include <ctype.h>
#include "bot.h"
#include "core.h"
#include "actions.h"
#include "entities.h"
#include "database.h"
#include "urlmanager.h"

request_t __request[] = {
	{.match = ".weather",  .callback = action_weather},
	{.match = ".ping",     .callback = action_ping},
	{.match = ".time",     .callback = action_time},
	{.match = ".rand",     .callback = action_random},
	{.match = ".stats",    .callback = action_stats},
	{.match = ".chart",    .callback = action_chart},
	{.match = ".help",     .callback = action_help},
};

unsigned int __request_count = sizeof(__request) / sizeof(request_t);

int sockfd;

void diep(char *str) {
	perror(str);
	exit(EXIT_FAILURE);
}

/*
 * Chat Handling
 */

int nick_length_check(char *nick, char *channel) {
	char raw[128];
	
	if(strlen(nick) > NICK_MAX_LENGTH) {
		printf("<> Nick too long\n");
		
		/* Ban Nick */
		sprintf(raw, "MODE %s +b %s!*@*", channel, nick);
		raw_socket(sockfd, raw);
		
		/* Kick Nick */
		sprintf(raw, "KICK %s %s :Nick too long (limit: %d). Please change your nick.", channel, nick, NICK_MAX_LENGTH);
		raw_socket(sockfd, raw);
		
		return 1;
	}
	
	return 0;
}

int pre_handle(char *data, char *nick, size_t nick_size) {
	extract_nick(data, nick, nick_size);
	
	/* Check Nick Length */
	if(nick_length_check(nick, IRC_CHANNEL))
		return 0;
	
	/* More Stuff Here */
	
	return 0;
}

void handle_nick(char *data) {
	char *nick = data;
	unsigned int i = 0;
	
	
	while(*(nick + i) && *(nick + i) != ':')
		nick++;
	
	/* Skipping ':' */
	nick++;
	
	/* Check Nick Length */
	if(nick_length_check(nick, IRC_CHANNEL))
		return;
	
	/* More Stuff Here */
}

void handle_join(char *data) {
	char nick[32];
	
	extract_nick(data, nick, sizeof(nick));
	
	/* Check Nick Length */
	if(nick_length_check(nick, IRC_CHANNEL))
		return;
}

char * match_prefix(char *data, char *match) {
	size_t size;
	
	size = strlen(match);
	if(!strncmp(data, match, size)) {
		/* Arguments */
		if(*(data + size) == ' ')
			return data + size + 1;
		
		/* No arguments */
		else if(*(data + size) == '\0')
			return data + size;
		
		/* Not exact match */
		else return NULL;
		
	} else return NULL;
}

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

int handle_message(char *message, char *nick) {
	char chan[32];
	char *content, *temp;
	unsigned int i;
	char *url, *trueurl;
	
	content = extract_chan(message, chan, sizeof(chan));
	
	for(i = 0; i < __request_count; i++) {
		if((temp = match_prefix(content, __request[i].match))) {
			__request[i].callback(chan, temp);
			return 0;
		}
	}
	
	if((url = strstr(message, "http://")) || (url = strstr(message, "https://"))) {
		if((trueurl = extract_url(url))) {
			// extract_nick(data + 1, nick, sizeof(nick));
			handle_url(nick, trueurl);
			
			free(trueurl);
			
		} else fprintf(stderr, "[-] URL: Cannot extact url\n");		
	}
	
	return 1;
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
		
		if((rlen = recv(sockfd, buff, MAXBUFF, 0)) < 0)
			diep("recv");
			
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

char *extract_chan(char *data, char *destination, size_t size) {
	size_t len = 0;
	size_t max = 0;
	
	while(*(data + len) && *(data + len) != ' ')
		len++;
	
	max = (len > size) ? size : len;
	strncpy(destination, data, max);
	
	destination[max] = '\0';
	
	return data + len + 2;
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
	char *request, nick[32];
	
	printf("[+] Core: Starting...\n");
	
	sqlite_db = db_sqlite_init();
	if(!db_sqlite_parse(sqlite_db))
		return 1;
	
	sockfd = init_irc_socket(IRC_SERVER, IRC_PORT);
	
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
		
		if(!strncmp(request, "JOIN", 4)) {
			handle_join(data + 1);
			continue;
		}
		
		if(!strncmp(request, "NICK", 4)) {
			handle_nick(data + 1);
			continue;
		}
		
		if(!strncmp(request, "PRIVMSG #", 9)) {
			pre_handle(data + 1, nick, sizeof(nick));
			handle_message(skip_server(request), nick);	
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
