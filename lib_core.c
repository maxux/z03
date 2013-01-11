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

#define __LIB_CORE_C
#include "bot.h"

#include "core_init.h"
#include "lib_database.h"
#include "lib_list.h"
#include "lib_core.h"
#include "lib_actions.h"
#include "lib_entities.h"
#include "lib_urlmanager.h"
#include "lib_logs.h"
#include "lib_ircmisc.h"
#include "lib_run.h"

request_t __request[] = {
	{.match = ".weather",  .callback = action_weather,     .man = "print weather information: .weather list, .weather [station]"},
	{.match = ".ping",     .callback = action_ping,        .man = "ping request/reply"},
	{.match = ".time",     .callback = action_time,        .man = "print the time"},
	{.match = ".rand",     .callback = action_random,      .man = "random generator: .rand, .rand max, .rand min max"},
	{.match = ".stats",    .callback = action_stats,       .man = "print url statistics"},
	{.match = ".chart",    .callback = action_chart,       .man = "print a chart about url usage"},
	{.match = ".backurl",  .callback = action_backlog_url, .man = "print the lastest urls posted on the chan"},
	{.match = ".uptime",   .callback = action_uptime,      .man = "print the bot's uptime"},
	{.match = ".somafm",   .callback = action_somafm,      .man = "print the current track on SomaFM radio: .somafm list, .somafm station"},
	{.match = ".dns",      .callback = action_dns,         .man = "resolve a dns name address: .dns domain-name"},
	{.match = ".count",    .callback = action_count,       .man = "print the number of line posted by a given nick: .count nick"},
	{.match = ".known",    .callback = action_known,       .man = "check if a given nick is already known, by hostname: .known nick"},
	{.match = ".url",      .callback = action_url,         .man = "search on url database, by url or title. Use % as wildcard. ie: .url gang%youtube"},
	{.match = ".goo",      .callback = action_google,      .man = "search on Google, print the first result: .goo keywords"},
	{.match = ".google",   .callback = action_google,      .man = "search on Google, print the 3 firsts result: .google keywords"},
	{.match = ".help",     .callback = action_help,        .man = "print the list of all the commands available"},
	{.match = ".man",      .callback = action_man,         .man = "print 'man page' of a given bot command: .man command"},
	{.match = ".note",     .callback = action_notes,       .man = "leave a message to someone, will be sent when connecting."},
	{.match = ".c",        .callback = action_run_c,       .man = "compile and run c code, from arguments: .run printf(\"Hello world\\n\");"},
	{.match = ".py",       .callback = action_run_py,      .man = "compile and run inline python code, from arguments: .py print('Hello world')"},
	{.match = ".hs",       .callback = action_run_hs,      .man = "compile and run inline haskell code, from arguments: .hs print \"Hello\""},
	{.match = ".php",      .callback = action_run_php,     .man = "compile and run inline php code, from arguments: .php echo \"Hello\";"},
	{.match = ".backlog",  .callback = action_backlog,     .man = "print last lines: .backlog [nick]"},
	{.match = ".wi",       .callback = action_wiki,        .man = "summary an english wikipedia's article: .wiki keywords"},
	{.match = ".wiki",     .callback = action_wiki,        .man = "summary a wiki's international article: .wiki lang keywords"},
	{.match = ".set",      .callback = action_set,         .man = "set a variable value: .set var1 val1"},
	{.match = ".unset",    .callback = action_unset,       .man = "unset a variable value: .unset var1"},
	{.match = ".fm",       .callback = action_lastfm,      .man = "print now playing lastfm title: .fm [username]"},
	{.match = ".fmlove",   .callback = action_lastfmlove,  .man = "love your current track on last.fm"},
};

unsigned int __request_count = sizeof(__request) / sizeof(request_t);

global_lib_t global_lib = {
	.channels = NULL,
};

/*
 * Channel Handling
 */
channel_t * channel_load(char *chan) {
	sqlite3_stmt *stmt;
	char *sqlquery, *nickname;
	int row;
	channel_t *channel;
	nick_t *nick;
	char buffer[512];
	
	printf("[+] Lib/Channel: creating <%s> environment...\n", chan);
		
	if(!(channel = (channel_t*) calloc(1, sizeof(channel_t))))
		diep("malloc");
	
	/* Reading channel stats */
	sqlquery = sqlite3_mprintf("SELECT count(id) FROM logs WHERE chan = '%q'", chan);
	printf("[ ] Lib/Channel: loading channel length...\n");
		
	if((stmt = db_select_query(sqlite_db, sqlquery)))
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW)
			channel->lines = (size_t) sqlite3_column_int(stmt, 0);
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	/* Creating nick list */
	channel->nicks = list_init(NULL);
	
	/* Reading nick list */
	sqlquery = sqlite3_mprintf("SELECT nick, count(id) FROM logs WHERE chan = '%q' GROUP BY nick", chan);
	printf("[ ] Lib/Channel: loading nick list...\n");
		
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
			if(!(nick = (nick_t*) malloc(sizeof(nick_t))))
				diep("malloc");
				
			nickname    = (char *) sqlite3_column_text(stmt, 0);
			nick->lines = (size_t) sqlite3_column_int(stmt, 1);
			
			list_append(channel->nicks, nickname, nick);
		}
	}
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	list_append(global_lib.channels, chan, channel);
	
	snprintf(buffer, sizeof(buffer), "[table rehashed: got %u total lines for %u nicks on database]", channel->lines, channel->nicks->length);
	irc_privmsg(chan, buffer);
	
	return channel;
}

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
		printf("[-] Lib/PreHandle: cannot find channel, reloading.\n");
		message->channel = channel_load(message->chan);
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
	char *fnick, *message;
	char *sqlquery, *list;
	char output[1024], timestring[64];
	struct tm * timeinfo;
	int row;
	time_t ts;
	
	chan = skip_header(data);
	
	/* Extracting data */
	if(!irc_extract_userdata(data, &nick, &username, &host)) {
		printf("[-] Lib/Join: Extract data info failed\n");
		return;
	}
	
	printf("[+] Lib/Join: Nick: <%s>, Username: <%s>, Host: <%s>\n", nick, username, host);
	
	/* Check Nick Length */
	if(nick_length_check(nick, chan))
		return;
	
	/* Checking if it's the bot itself */
	if(!strcmp(nick, IRC_NICK))
		channel_load(chan);
	
	/* Insert to db */
	sqlquery = sqlite3_mprintf("INSERT INTO hosts (nick, username, host, chan) VALUES ('%q', '%q', '%q', '%q')", nick, username, host, chan);

	if(db_simple_query(sqlite_db, sqlquery)) {
		if((list = irc_knownuser(nick, host))) {
			snprintf(output, sizeof(output), "PRIVMSG %s :%s (host: %s) is also known as: %s", chan, nick, host, list);
			raw_socket(sockfd, output);
			free(list);
		}
		
	} else printf("[-] Lib/Join: cannot update db, probably because nick already exists.\n");
		
	/* Clearing */
	sqlite3_free(sqlquery);
	free(username);
	
	/* Checking notes */	
	sqlquery = sqlite3_mprintf("SELECT fnick, message, ts FROM notes WHERE tnick = '%q' AND chan = '%q' AND seen = 0", nick, chan);
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
			fnick   = (char *) sqlite3_column_text(stmt, 0);
			message = (char *) sqlite3_column_text(stmt, 1);
			
			if((ts = sqlite3_column_int(stmt, 2)) > 0) {
				timeinfo = localtime(&ts);
				sprintf(timestring, "%02d/%02d/%02d %02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon + 1, (timeinfo->tm_year + 1900 - 2000), timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
				
			} else strcpy(timestring, "unknown");
				
			snprintf(output, sizeof(output), "PRIVMSG %s :┌── [%s] %s sent a message to %s", chan, timestring, fnick, nick);
			raw_socket(sockfd, output);
			
			snprintf(output, sizeof(output), "PRIVMSG %s :└─> %s", chan, message);
			raw_socket(sockfd, output);
		}
		
		sqlite3_free(sqlquery);
		sqlite3_finalize(stmt);
		
		sqlquery = sqlite3_mprintf("UPDATE notes SET seen = 1 WHERE tnick = '%q' AND chan = '%q'", nick, chan);
		if(!db_simple_query(sqlite_db, sqlquery))
			printf("[-] Lib/Join: cannot mark as read\n");
	
	} else fprintf(stderr, "[-] Action/Notes: SQL Error\n");
	
	/* Clearing */
	sqlite3_free(sqlquery);
	
	free(host);
	free(nick);
}

void handle_part(char *data) {
	char *nick = NULL, *username, *host = NULL, *chan;
	
	if(!(chan = strstr(data, "PART #"))) {
		printf("[-] Lib/Part: cannot get channel\n");
		return;
	}
	
	chan += 5;
	
	/* Extracting data */
	if(!irc_extract_userdata(data, &nick, &username, &host)) {
		printf("[-] Lib/Part: Extract data info failed\n");
		return;
	}
	
	printf("[+] Lib/Part: Nick: <%s>, Username: <%s>, Host: <%s>\n", nick, username, host);
	
	/* Checking if it's the bot itself */
	if(!strcmp(nick, IRC_NICK)) {
		printf("[+] Lib/Part: cleaning <%s> environment...\n", chan);
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
	nick_t *nick;
	char buffer[256];
	
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
		if(progression_match(++nick->lines)) {
			snprintf(buffer, sizeof(buffer), "Hey, %s just reached %u lines (%.2f%% of %s) !\n", 
					message->nickhl, nick->lines, ((double)nick->lines / message->channel->lines) * 100, message->chan);
				
			irc_privmsg(message->chan, buffer);
		}
			
		printf("[ ] Lib/Message: %s/%s: %u (%u)\n", message->chan, message->nickhl, nick->lines, message->channel->lines);
	
	/* New user, adding it */
	} else {
		if(!(nick = (nick_t*) malloc(sizeof(nick_t))))
			diep("malloc");
		
		nick->lines = 1;
		list_append(message->channel->nicks, message->nick, nick);
		
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
		raw_socket(sockfd, buffer);
	}
}

void main_core(char *data, char *request) {
	ircmessage_t message;

	if(!strncmp(data, "PING", 4)) {
		data[1] = 'O';		/* pOng */
		raw_socket(sockfd, data);
		return;
	}
	
	if(!strncmp(request, "376", 3)) {
		if(IRC_NICKSERV) {
			raw_socket(sockfd, "PRIVMSG NickServ :IDENTIFY " IRC_NICKSERV_PASS);
		
		/* if(IRC_OPER)
			raw_socket(sockfd, "OPER " IRC_NICK " " IRC_OPER_PASS); */
		
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
		handle_private_message(data + 1);
		return;
	}
}

void main_construct(void) {
	global_lib.channels = list_init(NULL);
	
	// opening sqlite
	sqlite_db = db_sqlite_init();
	if(!db_sqlite_parse(sqlite_db))
		fprintf(stderr, "[-] lib: error: cannot parse database !\n");
}

void main_destruct(void) {
	// closing sqitite
	db_sqlite_close(sqlite_db);
}
