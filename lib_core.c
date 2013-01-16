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
#include <time.h>
#include <signal.h>

#define __LIB_CORE_C
#include "bot.h"

#include "core_init.h"
#include "lib_database.h"
#include "lib_list.h"
#include "lib_core.h"
#include "lib_entities.h"
#include "lib_urlmanager.h"
#include "lib_logs.h"
#include "lib_ircmisc.h"
#include "lib_run.h"
#include "lib_stats.h"

#include "lib_actions.h"
#include "lib_actions_binutils.h"
#include "lib_actions_logs.h"
#include "lib_actions_services.h"
#include "lib_actions_lastfm.h"
#include "lib_actions_settings.h"
#include "lib_actions_webservices.h"
#include "lib_actions_run.h"
#include "lib_actions_useless.h"

// request_t __request= include
#include "lib_request_list.h"
unsigned int __request_count = sizeof(__request) / sizeof(request_t);

#define LASTTIME_CHECK     2 * 24 * 60 * 60 // 2 days

global_lib_t global_lib = {
	.channels = NULL,
};

/* Signals */
void lib_sighandler(int signal) {
	switch(signal) {
		case SIGUSR1:
			stats_daily_update();
		break;
	}
}

/*
 * Chat Handling
 */
void irc_privmsg(char *dest, char *message) {
	char buffer[2048];

	zsnprintf(buffer, "PRIVMSG %s :%s", dest, message);
	raw_socket(buffer);
}

void irc_notice(char *user, char *message) {
	char buffer[2048];

	zsnprintf(buffer, "NOTICE %s :%s", user, message);
	raw_socket(buffer);
}

int nick_length_check(char *nick, char *channel) {
	char raw[128];
	
	if(strlen(nick) > NICK_MAX_LENGTH) {
		printf("[-] Nick too long\n");
		
		/* Ban Nick */
		sprintf(raw, "MODE %s +b %s!*@*", channel, nick);
		raw_socket(raw);
		
		/* Kick Nick */
		sprintf(raw, "KICK %s %s :Nick too long (limit: %d). Please change your nick.", channel, nick, NICK_MAX_LENGTH);
		raw_socket(raw);
		
		return 1;
	}
	
	return 0;
}

int pre_handle(char *data, ircmessage_t *message) {
	channel_t *channel;
	/* Extracting nick */
	extract_nick(data, message->nick, sizeof(message->nick));
	
	/* Highlight protection */
	strcpy(message->nickhl, message->nick);
	anti_hl(message->nickhl);
	
	/* Extracting channel */
	data = skip_server(data);
	data = skip_server(data);
	extract_chan(data, message->chan, sizeof(message->chan));
	
	/* Check Nick Length */
	if(nick_length_check(message->nick, message->chan))
		return 0;
	
	if(!(message->channel = list_search(global_lib.channels, message->chan))) {
		printf("[-] lib/PreHandle: cannot find channel, reloading.\n");
		channel = stats_channel_load(message->chan);

		list_append(global_lib.channels, message->chan, channel);
		message->channel = channel;
	}
	
	return 0;
}

void handle_nick(char *data) {
	char *nick = data;
	unsigned int i = 0;
	
	
	while(*(nick + i) && *(nick + i) != ':')
		nick++;
	
	/* Skipping ':' */
	nick++;
}

void handle_join(char *data) {
	sqlite3_stmt *stmt;
	char *nick = NULL, *username, *host = NULL, *chan;
	char *fnick, *message, nick2[64];
	char *sqlquery, *list;
	char output[1024], timestring[64];
	struct tm * timeinfo;
	int row;
	time_t ts;
	
	chan = skip_header(data);
	
	/* Extracting data */
	if(!irc_extract_userdata(data, &nick, &username, &host)) {
		printf("[-] lib/Join: Extract data info failed\n");
		return;
	}
	
	printf("[+] lib/Join: Nick: <%s>, Username: <%s>, Host: <%s>\n", nick, username, host);
	
	/* Check Nick Length */
	if(nick_length_check(nick, chan))
		return;
	
	/* Checking if it's the bot itself */
	if(!strcmp(nick, IRC_NICK))
		list_append(global_lib.channels, chan, stats_channel_load(chan));
	
	/* Insert to db */
	sqlquery = sqlite3_mprintf("INSERT INTO hosts (nick, username, host, chan) VALUES ('%q', '%q', '%q', '%q')", nick, username, host, chan);

	if(db_simple_query(sqlite_db, sqlquery)) {
		if((list = irc_knownuser(nick, host))) {
			zsnprintf(nick2, "%s", nick);
			anti_hl(nick2);

			zsnprintf(output, "%s (host: %s) is also known as: %s", nick2, host, list);
			irc_privmsg(chan, output);

			free(list);
		}
		
	} else printf("[-] lib/Join: cannot update db, probably because nick already exists.\n");
		
	/* Clearing */
	sqlite3_free(sqlquery);
	free(username);
	
	/* Checking notes */	
	sqlquery = sqlite3_mprintf("SELECT fnick, message, ts FROM notes WHERE tnick = '%q' AND chan = '%q' AND seen = 0", nick, chan);
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) == SQLITE_ROW) {
			fnick   = (char *) sqlite3_column_text(stmt, 0);
			message = (char *) sqlite3_column_text(stmt, 1);
			
			if((ts = sqlite3_column_int(stmt, 2)) > 0) {
				timeinfo = localtime(&ts);
				sprintf(timestring, "%02d/%02d/%02d %02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon + 1, (timeinfo->tm_year + 1900 - 2000), timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
				
			} else strcpy(timestring, "unknown");
				
			snprintf(output, sizeof(output), "PRIVMSG %s :┌── [%s] %s sent a message to %s", chan, timestring, fnick, nick);
			raw_socket(output);
			
			snprintf(output, sizeof(output), "PRIVMSG %s :└─> %s", chan, message);
			raw_socket(output);
		}
		
		sqlite3_free(sqlquery);
		sqlite3_finalize(stmt);
		
		sqlquery = sqlite3_mprintf("UPDATE notes SET seen = 1 WHERE tnick = '%q' AND chan = '%q'", nick, chan);
		if(!db_simple_query(sqlite_db, sqlquery))
			printf("[-] lib/Join: cannot mark as read\n");
	
	} else fprintf(stderr, "[-] Action/Notes: SQL Error\n");
	
	/* Clearing */
	sqlite3_free(sqlquery);
	
	free(host);
	free(nick);
}

void handle_part(char *data) {
	char *nick = NULL, *username, *host = NULL, *chan;
	
	if(!(chan = strstr(data, "PART #"))) {
		printf("[-] lib/Part: cannot get channel\n");
		return;
	}
	
	chan += 5;
	
	/* Extracting data */
	if(!irc_extract_userdata(data, &nick, &username, &host)) {
		printf("[-] lib/Part: Extract data info failed\n");
		return;
	}
	
	printf("[+] lib/Part: Nick: <%s>, Username: <%s>, Host: <%s>\n", nick, username, host);
	
	/* Checking if it's the bot itself */
	if(!strcmp(nick, IRC_NICK)) {
		printf("[+] lib/Part: cleaning <%s> environment...\n", chan);
		list_remove(global_lib.channels, chan);
	}
	
	free(host);
	free(nick);
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
	
	request = (char *) malloc(sizeof(char) * ((strlen(chan) + strlen(nick) + strlen(reason)) + 16));
	sprintf(request, "KICK %s %s :%s", chan, nick, reason);
	
	raw_socket(request);
	free(request);
}

int handle_query(char *data) {
	char remote[256], *request;
	char *diff = NULL;
	unsigned char length;

	if((diff = strstr(data, "PRIVMSG"))) {
		length = diff - data - 1;

		strncpy(remote, data, length);
		remote[length] = '\0';

	} else return 0;

	if(!strcmp(remote, IRC_ADMIN_HOST)) {
		request = strstr(data, ":");

		if(request++) {
			printf("[ ] lib/admin: host <%s> request: <%s>\n", remote, request);

			if(!strncmp(request, ".rebuild", 7)) {
				printf("[+] lib/admin: rebuilding all...\n");
				stats_rebuild_all(global_lib.channels);
			}
		}

	} else printf("[-] lib/admin: host <%s> is not admin\n", remote);

	return 0;
}

int handle_message(char *data, ircmessage_t *message) {
	char *content, *temp;
	unsigned int i;
	char *url, *trueurl;
	nick_t *nick;
	char buffer[256];
	time_t now;
	
	content = skip_server(data) + 1;
	
	/* Saving log */
	log_privmsg(message->chan, message->nick, content);
	
	/* Special Check for BELL */
	if(strchr(data, '\x07')) {
		irc_kick(message->chan, message->nick, "Please, do not use BELL on this chan, fucking biatch !");
		return 0;
	}
	
	/* Updating channel lines count */
	if(progression_match(++message->channel->lines)) {
		snprintf(buffer, sizeof(buffer), "Well done, we just reached %u lines !\n", message->channel->lines);
		irc_privmsg(message->chan, buffer);
	}
	
	/* Updating nick lines count */
	if((nick = list_search(message->channel->nicks, message->nick))) {
		time(&now);

		// update nick lines
		if(progression_match(++nick->lines)) {
			snprintf(buffer, sizeof(buffer), "Hey, %s just reached %u lines (%.2f%% of %s) !\n", 
					message->nickhl, nick->lines, ((double) nick->lines / message->channel->lines) * 100, message->chan);
				
			irc_privmsg(message->chan, buffer);
		}

		if(!nick->words) {
			printf("[ ] lib/message: count of words not set, reading it...\n");
			nick->words = stats_get_words(message->nick, message->chan);
		}

		// update nick words
		nick->words += words_count(content);
		stats_update(message, nick, 0);

		// check last time
		if(!nick->lasttime) {
			printf("[ ] lib/message: lasttime not set, reading it\n");
			nick->lasttime = stats_get_lasttime(message->nick, message->chan);
		}
			
		printf("[ ] lib/message: %s/%s, lines: %u/%u, words: %u, lasttime: %ld\n",
		       message->chan, message->nickhl, nick->lines, message->channel->lines,
		       nick->words, nick->lasttime);

		if(nick->lasttime < now - LASTTIME_CHECK) {
			temp = time_elapsed(now - nick->lasttime);
			zsnprintf(buffer, "%s had not been seen on this channel for %s", message->nickhl, temp);
			irc_privmsg(message->chan, buffer);
			free(temp);
		}

		nick->lasttime = now;

	/* New user, adding it */
	} else {
		if(!(nick = (nick_t*) malloc(sizeof(nick_t))))
			diep("malloc");
		
		printf("[+] lib/message: new user <%s>\n", message->nick);

		nick->lines    = 1;
		nick->lasttime = time(NULL);
		nick->words    = words_count(content);

		list_append(message->channel->nicks, message->nick, nick);
		stats_update(message, nick, 1);
		
		if(!(message->channel->nicks->length % 100)) {
			snprintf(buffer, sizeof(buffer), "%s is the %uth nick on this channel", message->nickhl, message->channel->nicks->length);
			irc_privmsg(message->chan, buffer);
		}
	}
	
	for(i = 0; i < __request_count; i++) {
		if((temp = match_prefix(content, __request[i].match))) {
			message->command = content;
			__request[i].callback(message, temp);
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

void irc_joinall() {
	unsigned int i;
	char buffer[128];
	
	for(i = 0; i < sizeof(IRC_CHANNEL) / sizeof(char *); i++) {
		sprintf(buffer, "JOIN %s", IRC_CHANNEL[i]);
		raw_socket(buffer);
	}
}

void main_core(char *data, char *request) {
	ircmessage_t message;

	if(!strncmp(data, "PING", 4)) {
		data[1] = 'O';		/* pOng */
		raw_socket(data);
		return;
	}
	
	if(!strncmp(request, "376", 3)) {
		if(IRC_NICKSERV) {
			raw_socket("PRIVMSG NickServ :IDENTIFY " IRC_NICKSERV_PASS);
		
		/* if(IRC_OPER)
			raw_socket("OPER " IRC_NICK " " IRC_OPER_PASS); */
		
		} else irc_joinall();
		
		return;
	}
	
	if(!strncmp(request, "MODE", 4)) {
		/* Bot identified */
		if(!strncmp(request, "MODE " IRC_NICK " :+r", 9 + sizeof(IRC_NICK)) && IRC_NICKSERV)
			irc_joinall();
	}
	
	if(!strncmp(request, "JOIN", 4)) {
		handle_join(data + 1);
		return;
	}
	
	if(!strncmp(request, "PART", 4)) {
		handle_part(data + 1);
		return;
	}
	
	if(!strncmp(request, "NICK", 4)) {
		handle_nick(data + 1);
		return;
	}
	
	if(!strncmp(request, "PRIVMSG #", 9)) {
		if(pre_handle(data + 1, &message))
			return;
			
		handle_message(skip_server(request), &message);
		return;
	}
	
	if(!strncmp(request, "PRIVMSG " IRC_NICK, sizeof("PRIVMSG " IRC_NICK) - 1)) {
		handle_query(data + 1);
		return;
	}
}

void main_construct(void) {
	global_lib.channels = list_init(NULL);
	
	// opening sqlite
	sqlite_db = db_sqlite_init();

	if(global_core.auth) {
		// loading all stats
		stats_load_all(global_lib.channels);
	}

	// grabbing SIGUSR1 for daily update
	signal_intercept(SIGUSR1, lib_sighandler);
}

void main_destruct(void) {
	// closing sqitite
	db_sqlite_close(sqlite_db);
}
