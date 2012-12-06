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
#include "bot.h"
#include "core_init.h"
#include "lib_core.h"
#include "lib_urlmanager.h"
#include "lib_run.h"
#include "lib_ircmisc.h"

void lib_run_thread(int sock, ircmessage_t *message) {
	char buffer[1024], length = 0, *match, *print;
	size_t rlen;
	
	buffer[0] = '\0';
	
	while((rlen = recv(sock, buffer, sizeof(buffer), 0))) {
		buffer[rlen] = '\0';
		print = buffer;
		
		if(strchr(buffer, 0x07)) {
			irc_privmsg(message->chan, "Bell detected, closing thread.");
			break;
		}
		
		/* Multiple lines */
		while((match = strchr(print, '\n'))) {
			*match = '\0';
			
			if(strlen(print) > 180)
				strcpy(print + 172, " [...]");
				
			irc_privmsg(message->chan, print);
			
			if(length++ > 4) {
				irc_privmsg(message->chan, "Output truncated. Too verbose.");
				return;
			}
			
			print = match + 1;
			rlen = 0;
		}
		
		/* Single Line */
		if(rlen) {
			if(strlen(buffer) > 180)
				strcpy(buffer + 172, " [...]");
				
			irc_privmsg(message->chan, buffer);
			
			if(length++ > 4) {
				irc_privmsg(message->chan, "Output truncated. Too verbose.");
				return;
			}
		}
	}
	
	/* Clearing */
	close(sock);
	free(message);
}

void lib_run_init(ircmessage_t *msg, char *code, action_run_lang_t lang) {
	char buffer[2048], fd1[32], fd2[32];
	char *prefix[] = {"C ", "PY ", "HS "};
	int fd;
	ircmessage_t *message;
	
	if((fd = init_socket(SRV_RUN_CLIENT, SRV_RUN_PORT)) < 0) {
		irc_privmsg(message->chan, "Cannot connect the build machine");
		return;
	}
	
	/* Linking message */
	if(!(message = (ircmessage_t*) malloc(sizeof(ircmessage_t))))
		return;
	
	strcpy(message->chan, msg->chan);
	
	/* Starting */
	snprintf(buffer, sizeof(buffer), "%s%s", prefix[lang], code);	
	
	/* Sending code */
	if(send(fd, buffer, strlen(buffer), 0) == -1) {
		perror("[-] send");
		return;
	}
	
	if(fork() <= 0) {
		sprintf(fd1, "%d", sockfd);
		sprintf(fd2, "%d", fd);
		
		execl("execute-daemon/execute-client", "execute-client", fd1, fd2, message->chan, NULL);
		
	} else printf("[+] Forked, see ya soon biatch.\n");
}