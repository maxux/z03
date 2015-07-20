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
#include <pthread.h>

#define __LIB_CORE_C
#include "../common/bot.h"

#include "../core/init.h"
#include "database.h"
#include "list.h"
#include "core.h"
#include "entities.h"
#include "downloader.h"
#include "urlmanager.h"
#include "logs.h"
#include "ircmisc.h"
#include "run.h"
#include "stats.h"
#include "known.h"
#include "periodic.h"

#include "actions.h"
#include "actions_services.h"
#include "actions_whatcd.h"

global_lib_t global_lib = {
	.channels = NULL,
	.threads  = NULL,
	.commands = NULL,
};

pthread_t __periodic_thread;

// signals
void lib_sighandler(int signal) {
	switch(signal) {
		case SIGUSR1:
			pthread_mutex_unlock(&global_core->mutex_ssl);
			topic_update();
			stats_daily_update();
		break;
		
		case SIGINT:
			exit(EXIT_SUCCESS);
		break;
	}
}

/*
 * Channels
 */
static void irc_joinall() {
	unsigned int i;
	char buffer[128];
	
	for(i = 0; i < sizeof(IRC_CHANNEL) / sizeof(char *); i++) {
		sprintf(buffer, "JOIN %s", IRC_CHANNEL[i]);
		raw_socket(buffer);
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
	char *nick, *username, *host;
	channel_t *channel = NULL;
	
	if(!irc_extract_userdata(data, &nick, &username, &host)) {
		printf("[-] lib/prehandle: extract data info failed\n");
		return 1;
	}
	
	free(username);
	
	/* Extracting nick */
	zsnprintf(message->nick, "%s", nick);
	zsnprintf(message->host, "%s", host);
	
	free(nick);
	free(host);
	
	/* Highlight protection */
	strcpy(message->nickhl, message->nick);
	anti_hl(message->nickhl);
	
	// extract channel
	// skip_server twice (skipping 2 words)
	data = skip_server(data);
	data = skip_server(data);
	extract_chan(data, message->chan, sizeof(message->chan));
	
	#ifdef ENABLE_NICK_MAX_LENGTH
	if(nick_length_check(message->nick, message->chan))
		return 1;
	#endif
	
	if(!(message->channel = list_search(global_lib.channels, message->chan))) {
		printf("[-] lib/prehandle: cannot find channel, reloading.\n");
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
	
	printf("[+] lib/handlenick: new nick: <%s>\n", nick);
}

void handle_join(char *data) {
	char *nick = NULL, *username, *host = NULL, *chan;
	nick_t *nickdata;
	channel_t *channel;
	
	chan = skip_header(data);
	
	/* Extracting data */
	if(!irc_extract_userdata(data, &nick, &username, &host)) {
		printf("[-] lib/join: extract data info failed\n");
		return;
	}
	
	printf("[+] lib/join: nick: <%s>, username: <%s>, host: <%s>\n", nick, username, host);
	
	/* Check Nick Length */
	if(!nick_length_check(nick, chan)) {
		/* Checking if it's the bot itself */
		if(!strcmp(nick, IRC_NICK))
			list_append(global_lib.channels, chan, stats_channel_load(chan));
		
		// check if there is pending notes
		__action_notes_checknew(chan, nick);
		
		if((channel = list_search(global_lib.channels, chan))) {
			printf("[+] lib/join: setting %s online\n", nick);
			
			if((nickdata = list_search(channel->nicks, nick)))
				nickdata->online = 1;
		}
	}
	
	// check if the user is known
	// __action_known_add(nick, username, host, chan);
	free(username);
	free(host);
	free(nick);
}

void handle_new_topic(char *data) {
	char *chan, buffer[256];
	
	data = skip_server(data);
	chan = string_index(data, 1);
	
	printf("[+] topic: new topic for <%s>, requesting info\n", chan);
	
	zsnprintf(buffer, "TOPIC %s", chan);
	raw_socket(buffer);
}

void handle_topic(char *data) {
	char *chan, *topic;
	channel_t *channel;
	
	data = skip_server(data);
	chan = string_index(data, 2);
	topic = skip_header(data);
	
	printf("[+] topic <%s>: %s\n", chan, topic);
	
	if((channel = list_search(global_lib.channels, chan))) {
		// updating topic
		free(channel->topic);
		channel->topic = strdup(topic);
		
		printf("[+] topic: channel found, topic set: %s\n", channel->topic);
	}
	
	free(chan);
}

//
// FIXME: private function for custom topic set
//
void topic_update() {
	char *chan = "#inpres";
	char *topic, buffer[2048], request[2440];
	char *begin, *start, *next;
	channel_t *channel;
	time_t now;
	struct tm reach;
	double seconds, days;
	
	if(!(channel = list_search(global_lib.channels, chan))) {
		printf("[-] topic: update: channel <%s> not found\n", chan);
		return;
	}
	
	if(!channel->topic) {
		printf("[-] topic: update: i don't have the topic right now, sorry\n");
		return;
	}
	
	topic = channel->topic;
	printf("[+] topic: channel found, current topic: %s\n", channel->topic);
	
	// find pattern
	if(!(start = strstr(topic, "Dour J-"))) {
		printf("[-] topic: update: pattern not found\n");
		return;
	}
	
	begin = strndup(topic, start - topic);
	
	if(!(next = strchr(start + 7, ' '))) {
		printf("[-] topic: update: end pattern not found ?!\n");
		return;
	}
	
	// set new date countdown
	time(&now);
	reach = *localtime(&now);
	
	reach.tm_hour = 0;
	reach.tm_min = 0;
	reach.tm_sec = 0;
	
	// 15 july
	reach.tm_mon = 6;
	reach.tm_mday = 15;
	
	seconds = difftime(mktime(&reach), now);
	days = seconds / (60 * 60 * 24);
	
	// building the new topic
	sprintf(buffer, "%sDour J-%.0f%s", begin, days, next);
	free(begin);
	
	// clearing previous one and set the new one
	free(channel->topic);
	channel->topic = strdup(buffer);
	
	// send to the server
	sprintf(request, "TOPIC %s :%s", chan, channel->topic);
	raw_socket(request);
	
	printf("[+] topic: new topic: %s\n", channel->topic);
}

void handle_kick(char *data) {
	char *nick, *chan, query[512];
	
	if(!(nick = string_index(data, 3)))
		return;
	
	if(!strcmp(nick, IRC_NICK)) {
		if((chan = string_index(data, 2))) {
			printf("[-] kick: bot kicked from %s, rejoins...\n", chan);
			
			zsnprintf(query, "JOIN %s", chan);
			raw_socket(query);
			
			free(chan);
		}
	}	
	
	free(nick);
}

void handle_part(char *data) {
	char *nick = NULL, *username, *host = NULL, *chan;
	channel_t *channel;
	nick_t *nickdata;
	
	chan = string_index(data, 2);
	
	/* Extracting data */
	if(!irc_extract_userdata(data, &nick, &username, &host)) {
		printf("[-] lib/part: extract data info failed\n");
		free(chan);
		return;
	}
	
	printf("[+] lib/part: channel: %s, nick: %s\n", chan, nick);
	
	/* Checking if it's the bot itself */
	if(!strcmp(nick, IRC_NICK)) {
		printf("[+] lib/part: cleaning %s environment...\n", chan);
		list_remove(global_lib.channels, chan);
		
	} else if((channel = list_search(global_lib.channels, chan))) {
		printf("[+] lib/part: setting %s from %s: offline\n", nick, chan);
		if((nickdata = list_search(channel->nicks, nick)))
			nickdata->online = 0;
	}
	
	free(chan);
	free(username);
	free(host);
	free(nick);
}

void handle_quit(char *data) {
	char *nick = NULL, *username, *host = NULL;
	nick_t *nickdata;
	list_node_t *node;
	
	/* Extracting data */
	if(!irc_extract_userdata(data, &nick, &username, &host)) {
		printf("[-] lib/quit: extract data info failed\n");
		return;
	}
	
	printf("[+] lib/quit: Nick: <%s>, Username: <%s>, Host: <%s>\n", nick, username, host);
	
	node = global_lib.channels->nodes;
	while(node) {
		printf("[+] lib/quit: removing %s from %s\n", nick, node->name);
		if((nickdata = list_search(((channel_t *) node->data)->nicks, nick)))
			nickdata->online = 0;
		
		node = node->next;
	}
	
	free(username);
	free(host);
	free(nick);
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
	
	if(isadmin(remote)) {
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

void *command_thread(void *_thread) {
	thread_cmd_t *thread = (thread_cmd_t *) _thread;
	
	thread->message->request->callback(thread->message, thread->args);
	
	// freeing process
	free(thread->message->command);
	free(thread->message);
	free(thread->args);
	
	list_remove(global_lib.threads, thread->myid);
	free(thread);
	
	return _thread;
}

void command_prepare_thread(ircmessage_t *message, char *args) {
	thread_cmd_t *thread;
	
	// copy ircmessage_t element
	thread = (thread_cmd_t *) malloc(sizeof(thread_cmd_t));
	thread->message = (ircmessage_t *) malloc(sizeof(ircmessage_t));
	memcpy(thread->message, message, sizeof(ircmessage_t));
	
	// reallocating dynamic contents
	thread->message->command = strdup(message->command);
	thread->args = strdup(args);
	
	zsnprintf(thread->myid, "%p", (void *) &thread->thread);
	
	if(!pthread_create(&thread->thread, NULL, command_thread, thread)) {
		printf("[+] core/thread: created\n");
		list_append(global_lib.threads, thread->myid, thread);
		pthread_detach(thread->thread);
		
	} else perror("[-] core/thread");
}

int handle_commands(char *content, ircmessage_t *message) {
	unsigned int callback_count = 0;
	char *callback_temp = NULL;
	char *command, *match;
	request_t *callback_request = NULL;
	request_t *request;
	
	if(*content == ' ')
		return 1;
	
	if((match = strchr(content, ' '))) {
		command = strndup(content, match - content);
		match++;
		
	} else {
		command = strdup(content);
		match = "";
	}
	
	list_foreach(global_lib.commands, node) {
		request = (request_t *) node->data;
		
		if(!strncasecmp(command, request->match, strlen(command))) {
			printf("[+] commands: match for: <%s>\n", request->match);
			callback_count++;
			callback_temp    = match;
			callback_request = request;
			
			/* check for exact matching */
			if(!strcmp(command, request->match)) {
				callback_count = 1;
				break;
			}
		}
	}
	
	free(command);
	
	if(callback_count == 0)
		return 0;
	
	if(callback_count == 1) {
		message->command = content;
		message->request = callback_request;
		command_prepare_thread(message, callback_temp);
		return 1;
		
	} else irc_privmsg(message->chan, "Ambiguous command name, be more precise.");
	
	return 0;
}

static char *__question_answers[] = {"Yes.", "No.", "Maybe."};

int handle_precommands(char *content, ircmessage_t *message) {
	char *temp;
	
	/* special check for BELL */
	if(strchr(content, '\x07')) {
		irc_kick(message->chan, message->nick, "Please, do not use BELL on this chan, fucking biatch !");
		return 1;
	}
	
	/* useless or easteregg */
	if(strstr(content, "mére")) {
		irc_kick(message->chan, message->nick, "ta mére ouais");
		return 0;
	}
	
	if(strstr(content, "une kernel") || strstr(content, "la kernel") || strstr(content, "kernelle")) {
		irc_kick(message->chan, message->nick, "On dit *un* *kernel*, espèce d'yllaytray");
		return 0;
	}
	
	if(strstr(content, "la nutella") || strstr(content, "la wifi")) {
		irc_kick(message->chan, message->nick, "Apprends à écrire, yllètrayé");
		return 0;
	}
	
	if(strstr(content, "sait passé")) {
		irc_kick(message->chan, message->nick, "Aprans a ékrir èspaisse d'illètré");
		return 0;
	}
	
	if(strstr(content, "twitch.tv/t4g1") && !strcmp(message->nick, "T4g1")) {
		irc_kick(message->chan, message->nick, "Nope");
		return 0;
	}
	
	// ask question to bot
	if(!strncmp(content, IRC_NICK, sizeof(IRC_NICK) - 1) && content[strlen(content) - 1] == '?') {
		irc_privmsg(message->chan, __question_answers[rand() % (sizeof(__question_answers) / sizeof(char *))]);
		return 1;
	}
	
	// end with ":D"
	temp = content + strlen(content) - 2;
	if(!strcmp(temp, ":D") || !strcmp(temp, "xD")) {
		if((rand() % 21) == 16)
			irc_privmsg(message->chan, ":D");
			
		return 0;
	}
	
	return 0;
}

int handle_message(char *data, ircmessage_t *message) {
	char *content, *temp;
	char *url, *trueurl;
	nick_t *nick;
	char buffer[256];
	time_t now;
	
	content = skip_server(data) + 1;
	
	// security check and easteregg/useless commands
	if(handle_precommands(content, message))
		return 0;
	
	// updating channel lines count
	if(progression_match(++message->channel->lines)) {
		zsnprintf(buffer, "Well done, we just reached %zu lines !\n", message->channel->lines);
		irc_privmsg(message->chan, buffer);
	}
	
	// checking bot commands, before stats management
	// this cause a better response time
	handle_commands(content, message);
	
	db_sqlite_simple_query(sqlite_db, "BEGIN TRANSACTION");
	
	/* Updating nick lines count */
	if((nick = list_search(message->channel->nicks, message->nick))) {
		time(&now);
		
		// update nick lines
		if(progression_match(++nick->lines)) {
			zsnprintf(buffer,
			          "Hey, %s just reached %zu lines (%.2f%% of %s) !\n", 
				  message->nickhl, nick->lines,
				  ((double) nick->lines / message->channel->lines) * 100,
				  message->chan
			);
				
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
			
		printf("[ ] lib/message: %s/%s, lines: %zu/%zu, words: %zu, lasttime: %ld\n",
		       message->chan, message->nickhl, nick->lines, message->channel->lines,
		       nick->words, nick->lasttime);
		
		#ifdef ENABLE_NICK_LASTTIME
		if(nick->lasttime < now - NICK_LASTTIME) {
			temp = time_elapsed(now - nick->lasttime);
			zsnprintf(buffer, "%s had not been seen on this channel for %s", message->nickhl, temp);
			irc_privmsg(message->chan, buffer);
			free(temp);
		}
		#endif
		
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
			zsnprintf(buffer, "%s is the %uth nick on this channel",
			                  message->nickhl, message->channel->nicks->length);
			
			irc_privmsg(message->chan, buffer);
		}
	}
	
	// saving log
	log_privmsg(message->chan, message->nick, content);
	
	// commit sql
	db_sqlite_simple_query(sqlite_db, "END TRANSACTION");
	
	// copy data to a modifiable string
	temp = strdup(data);
	
	// checking if url exists
	while((url = strstr(temp, "http://")) || (url = strstr(temp, "https://"))) {
		printf("[+] url found: %s\n", url);
		
		if((trueurl = extract_url(url))) {
			printf("[+] url cleared found: %s\n", trueurl);
			
			message->request = &__url_request;
			message->command = ".http";
			command_prepare_thread(message, trueurl);
			
			// clearing url from base string
			memset(url, ' ', strlen(trueurl));
			
			free(trueurl);
			
		} else fprintf(stderr, "[-] URL: Cannot extact url\n");
	}
	
	free(temp);
	
	return 1;
}

void handle_notice(char *data, ircmessage_t *message) {
	char *nick = NULL, *username, *host = NULL, *payload;
	
	/* Extracting data */
	if(!irc_extract_userdata(data, &nick, &username, &host)) {
		printf("[-] lib/part: extract data info failed\n");
		return;
	}
	
	free(username);
	free(host);
	
	zsnprintf(message->nick, "%s", nick);
	payload = strchr(data, ':') + 1;
	
	printf("[+] lib/notice: from: %s: %s\n", nick, payload);
	
	if(strstr(data, "You are now identified") && !strcasecmp(nick, "nickserv"))
		irc_joinall();
	
	if(!strncmp(payload, ".whatcd ", 8))
		action_whatcd(message, payload + 8);
	
	free(nick);
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

	if(!strncmp(data, "PING", 4)) {
		data[1] = 'O';		/* pOng */
		raw_socket(data);
		return;
	}
	
	if(!strncmp(request, "NOTICE", 6)) {
		handle_notice(data + 1, &message);
		return;
	}
	
	if(!strncmp(request, "JOIN", 4)) {
		handle_join(data + 1);
		return;
	}
	
	if(!strncmp(request, "PART", 4)) {
		handle_part(data + 1);
		return;
	}
	
	if(!strncmp(request, "QUIT", 4)) {
		handle_quit(data + 1);
		return;
	}
	
	if(!strncmp(request, "NICK", 4)) {
		handle_nick(data + 1);
		return;
	}
	
	if(!strncmp(request, "KICK", 4)) {
		handle_kick(data + 1);
		return;
	}
	
	if(!strncmp(request, "TOPIC", 5)) {
		handle_new_topic(data + 1);
		return;
	}
	
	if(!strncmp(request, "332", 3)) {
		handle_topic(data + 1);
		return;
	}
	
	if(!strncmp(request, "376", 3) || !strncmp(request, "422", 3)) {
		if(IRC_NICKSERV) {
			raw_socket("PRIVMSG NickServ :IDENTIFY " IRC_NICKSERV_PASS);
			
			// little sleep to wait vhost to be applicated
			usleep(500000);
		
		} else irc_joinall();
		
		return;
	}
}

//
// core inner handlers
//
void nick_free(void *data) {
	free(data);
}

static void channel_free(void *data) {
	channel_t *channel = (channel_t *) data;
	list_free(channel->nicks);
	free(channel);
}

void lib_construct() {
	global_lib.channels = list_init(channel_free);
	global_lib.threads  = list_init(NULL);
	
	// init libcurl
	curl_global_init(CURL_GLOBAL_ALL);
	
	// opening sqlite
	sqlite_db = db_sqlite_init();
	
	if(global_core->auth) {
		// loading all stats
		stats_load_all(global_lib.channels);
	}
	
	// grabbing SIGUSR1 for daily update
	signal_intercept(SIGUSR1, lib_sighandler);
	signal_intercept(SIGINT, lib_sighandler);
	
	// starting timing thread
	printf("[+] lib: init deferred threads\n");
	pthread_create(&__periodic_thread, NULL, periodic_each_minutes, NULL);
}

void lib_destruct() {	
	// cleaning threads
	pthread_cancel(__periodic_thread);
	
	printf("[+] lib: waiting theads...\n");
	pthread_join(__periodic_thread, NULL);
	
	list_foreach(global_lib.threads, node) {
		// pthread_cancel(((thread_cmd_t *) node)->thread);
		pthread_join(((thread_cmd_t *) node)->thread, NULL);
	}
	
	// freeing global lists
	printf("[+] freeing channels...\n");
	list_free(global_lib.channels);
	
	printf("[+] freeing threads...\n");
	list_free(global_lib.threads);
	
	// closing sqitite
	db_sqlite_close(sqlite_db);
}

//
// administrator
//
int isadmin(char *host) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	int admin = 0;
	
	/* select backlog */
	sqlquery = sqlite3_mprintf(
		"SELECT * FROM admin WHERE host = '%q'",
		host
	);

	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] lib/admin: sql select error\n");
	
	if(sqlite3_step(stmt) == SQLITE_ROW)
		admin = 1;
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return admin;
}
