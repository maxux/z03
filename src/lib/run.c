/* z03 - small bot with some network features - irc channel bot actions
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include "../common/bot.h"
#include "../core/init.h"
#include "list.h"
#include "core.h"
#include "run.h"
#include "ircmisc.h"

typedef struct lib_run_data_t {
	pthread_t thread;
	ircmessage_t *message;
	int codefd;
	
} lib_run_data_t;

void lib_run_privmsg(ircmessage_t *message, char *data) {
	char buffer[1500];
	zsnprintf(buffer, "[%p] %s", (void *) message, data);
	irc_privmsg(message->chan, buffer);
}

// void lib_run_fork(ircmessage_t *message, int codefd) {
void *lib_run_fork(void *_data) {
	lib_run_data_t *data = (lib_run_data_t *) _data;
	struct timeval tv = {tv.tv_sec = 0, tv.tv_usec = 0};
	char buffer[1024], length = 0;
	char *match = NULL, *print;
	size_t rlen;
	
	printf("[+] lib/run: thread %u: starting...\n", (unsigned int) pthread_self());
	
	buffer[rlen] = '\0';
	while((rlen = recv(data->codefd, buffer, sizeof(buffer), 0))) {
		buffer[rlen] = '\0';
		printf("[ ] lib/run: raw: <%s>\n", buffer);
		
		if(strchr(buffer, 0x07)) {
			lib_run_privmsg(data->message, "Bell detected, closing thread.");
			break;
		}
		
		/* multiple lines */
		print = buffer;
		while((match = strchr(print, '\n')) || rlen) {
			if(match)
				*match = '\0';
			
			if(strlen(print) > 180)
				strcpy(print + 172, " [...]");
				
			lib_run_privmsg(data->message, print);
			
			if(!strcmp(data->message->chan, "#test") && length > 8) {
				lib_run_privmsg(data->message, "Output truncated. Too verbose.");
				goto eot;
			}
			
			if(strcmp(data->message->chan, "#test") && length > 1)
				goto eot;
			
			if(match)
				print = match + 1;
			
			rlen = 0;
			length++;
		}
	}
	
	eot:
		close(data->codefd);
		free(data->message);
		free(data);
	
	printf("[+] lib/run: thread %u: closing...\n", (unsigned int) pthread_self());
	
	// removing client from list and restoring timeout if last client
	pthread_mutex_lock(&global_core->mutex_client);
	if(!--global_core->extraclient) {
		tv.tv_sec = SOCKET_TIMEOUT;
		
		if(setsockopt(ssl->sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(tv)))
			diep("[-] setsockopt: SO_RCVTIMEO");
	}
	pthread_mutex_unlock(&global_core->mutex_client);
	
	return data;
}

void lib_run_init(ircmessage_t *msg, char *code, action_run_lang_t lang) {
	char buffer[2048];
	char *prefix[] = {"C ", "PY ", "HS ", "PHP "};
	lib_run_data_t *data;
	int fd;
	
	if((fd = init_socket(SRV_RUN_CLIENT, SRV_RUN_PORT)) < 0) {
		irc_privmsg(msg->chan, "Build machine seems te be down");
		return;
	}
	
	/* creating environment */
	if(!(data = malloc(sizeof(lib_run_data_t))))
		return;
	
	/* linking message */
	if(!(data->message = (ircmessage_t*) malloc(sizeof(ircmessage_t))))
		return;
	
	strcpy(data->message->chan, msg->chan);
	
	/* starting request */
	snprintf(buffer, sizeof(buffer), "%s%s", prefix[lang], code);	
	
	/* Sending code */
	if(send(fd, buffer, strlen(buffer), 0) < 0) {
		perror("[-] send");
		return;
	}
	
	data->codefd = fd;
	global_core->extraclient++;
	
	/* threading operation */
	printf("[+] lib/run: starting thread\n");
	pthread_create(&data->thread, NULL, lib_run_fork, data);
	pthread_detach(data->thread);
}
