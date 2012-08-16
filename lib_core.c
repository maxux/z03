/* z03 - small bot with some network features
 * Author: Daniel Maxime (root@maxux.net)
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
#include "core_init.h"
#include "core_database.h"
#include "lib_core.h"
#include "lib_actions.h"
#include "lib_entities.h"
#include "lib_urlmanager.h"
#include "lib_logs.h"

request_t __request[] = {
	{.match = ".weather",  .callback = action_weather},
	{.match = ".ping",     .callback = action_ping},
	{.match = ".time",     .callback = action_time},
	{.match = ".rand",     .callback = action_random},
	{.match = ".stats",    .callback = action_stats},
	{.match = ".chart",    .callback = action_chart},
	{.match = ".backurl",  .callback = action_backlog_url},
	{.match = ".uptime",   .callback = action_uptime},
	{.match = ".seen",     .callback = action_seen},
	{.match = ".somafm",   .callback = action_somafm},
	{.match = ".help",     .callback = action_help},
};

unsigned int __request_count = sizeof(__request) / sizeof(request_t);

/*
 * Chat Handling
 */
int nick_length_check(char *nick, char *channel) {
	char raw[128];
	
	if(strlen(nick) > NICK_MAX_LENGTH) {
		printf("[-] Nick too long\n");
		
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

int pre_handle(char *data, ircmessage_t *message) {
	// Extracting Nick
	extract_nick(data, message->nick, sizeof(message->nick));
	
	// Extracting Message Chan
	data = skip_server(data);
	data = skip_server(data);
	extract_chan(data, message->chan, sizeof(message->chan));
	
	/* Check Nick Length */
	if(nick_length_check(message->nick, message->chan))
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

void irc_kick(char *chan, char *nick, char *reason) {
	char *request;
	
	request = (char*) malloc(sizeof(char) * ((strlen(chan) + strlen(nick) + strlen(reason)) + 16));
	sprintf(request, "KICK %s %s :%s", chan, nick, reason);
	
	raw_socket(sockfd, request);
	free(request);
}

void handle_private_message(char *data) {
	printf("[ ] Delayed private message support: %s\n", data);
}

int handle_message(char *data, ircmessage_t *message) {
	char *content, *temp;
	unsigned int i;
	char *url, *trueurl;
	
	content = skip_server(data) + 1;
	
	/* Saving log */
	log_privmsg(message->chan, message->nick, content);
	
	/* Special Check for BELL */
	if(strchr(data, '\x07')) {
		irc_kick(message->chan, message->nick, "Please, do not use BELL on this chan, fucking biatch !");
		return 0;
	}
	
	for(i = 0; i < __request_count; i++) {
		if((temp = match_prefix(content, __request[i].match))) {
			__request[i].callback(message->chan, temp);
			return 0;
		}
	}
	
	if((url = strstr(data, "http://")) || (url = strstr(data, "https://"))) {
		if((trueurl = extract_url(url))) {
			handle_url(message, trueurl);
			free(trueurl);
			
		} else fprintf(stderr, "[-] URL: Cannot extact url\n");		
	}
	
	return 1;
}

/*
 * IRC Protocol
 */
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

void main_core(char *data, char *request) {
	ircmessage_t message;

	if(!strncmp(data, "PING", 4)) {
		data[1] = 'O';		/* pOng */
		raw_socket(sockfd, data);
		return;
	}
	
	if(!strncmp(request, "376", 3)) {
		if(IRC_NICKSERV)
			raw_socket(sockfd, "PRIVMSG NickServ :IDENTIFY " IRC_NICKSERV_PASS);
		
		else raw_socket(sockfd, "JOIN " IRC_CHANNEL);
		
		return;
	}
	
	if(!strncmp(request, "MODE", 4)) {
		/* Bot identified */
		if(!strncmp(request, "MODE " IRC_NICK " :+r", 9 + sizeof(IRC_NICK)) && IRC_NICKSERV)
			raw_socket(sockfd, "JOIN " IRC_CHANNEL);
	}
	
	if(!strncmp(request, "JOIN", 4)) {
		handle_join(data + 1);
		return;
	}
	
	if(!strncmp(request, "NICK", 4)) {
		handle_nick(data + 1);
		return;
	}
	
	if(!strncmp(request, "PRIVMSG #", 9)) {
		pre_handle(data + 1, &message);
		handle_message(skip_server(request), &message);	
		return;
	}
	
	if(!strncmp(request, "PRIVMSG " IRC_NICK, sizeof("PRIVMSG " IRC_NICK) - 1)) {
		handle_private_message(data + 1);
		return;
	}
}
