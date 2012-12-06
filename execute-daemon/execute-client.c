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
#include <unistd.h>
#include "../bot.h"
#include "../core_init.h"
#include "../lib_core.h"
#include "../lib_urlmanager.h"
#include "../lib_run.h"
#include "../lib_ircmisc.h"

int sockfd;

void dies(int sock) {
	close(sock);
	exit(EXIT_SUCCESS);
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

void irc_privmsg(char *chan, char *message) {
	char buffer[2048];
	
	snprintf(buffer, sizeof(buffer), "PRIVMSG %s :%s", chan, message);
	raw_socket(sockfd, buffer);
}

int main(int argc, char *argv[]) {
	char buffer[1024], length = 0, *match, *print;
	char *chan;
	int sock;
	size_t rlen;
	
	if(argc < 4)
		return 1;
	
	sockfd = atoi(argv[1]);
	sock   = atoi(argv[2]);
	chan   = argv[3];
	
	printf("[+] > Forked: sockfd: %d, sock: %d, chan: %s\n", sockfd, sock, chan);
		
	buffer[0] = '\0';
	
	while((rlen = recv(sock, buffer, sizeof(buffer), 0))) {
		buffer[rlen] = '\0';
		print = buffer;
		
		if(strchr(buffer, 0x07)) {
			irc_privmsg(chan, "Bell detected, closing thread.");
			break;
		}
		
		/* Multiple lines */
		while((match = strchr(print, '\n'))) {
			*match = '\0';
			
			if(strlen(print) > 180)
				strcpy(print + 172, " [...]");
				
			irc_privmsg(chan, print);
			
			if(length++ > 4) {
				irc_privmsg(chan, "Output truncated. Too verbose.");
				dies(sock);
			}
			
			print = match + 1;
			rlen = 0;
		}
		
		/* Single Line */
		if(rlen) {
			if(strlen(buffer) > 180)
				strcpy(buffer + 172, " [...]");
				
			irc_privmsg(chan, buffer);
			
			if(length++ > 4) {
				irc_privmsg(chan, "Output truncated. Too verbose.");
				dies(sock);
			}
		}
	}
	
	/* Clearing */
	close(sock);
	
	return 0;
}
