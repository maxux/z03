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
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../bot.h"
#include "../core_init.h"
#include "../lib_core.h"
#include "../lib_urlmanager.h"
#include "../lib_run.h"
#include "../lib_ircmisc.h"
#include "../core_ssl.h"

ssl_socket_t *ssl;
int sock, sockfd;

void dies(char *str) {
	close(sock);
	perror(str);
	exit(EXIT_FAILURE);
}

ssl_socket_t *ssl_init(int sockfd) {
	ssl_socket_t *ssl;

	if(!(ssl = (ssl_socket_t*) malloc(sizeof(ssl_socket_t))))
		dies("[-] malloc");

	ssl->socket     = NULL;
	ssl->sslContext = NULL;

	ssl->sockfd = sockfd;

	// Register the error strings for libcrypto & libssl
	SSL_load_error_strings();

	// Register the available ciphers and digests
	SSL_library_init();

	// New context saying we are a client, and using SSL 2 or 3
	ssl->sslContext = SSL_CTX_new(SSLv23_client_method());
	if(ssl->sslContext == NULL)
		ERR_print_errors_fp(stderr);

	// Create an SSL struct for the connection
	ssl->socket = SSL_new(ssl->sslContext);
	if(ssl->socket == NULL)
		ERR_print_errors_fp(stderr);

	// Connect the SSL struct to our connection
	if(!SSL_set_fd(ssl->socket, ssl->sockfd))
		ERR_print_errors_fp(stderr);

	// Initiate SSL handshake
	if(SSL_connect(ssl->socket) != 1)
		ERR_print_errors_fp(stderr);

	return ssl;
}

int ssl_write(ssl_socket_t *ssl, char *data) {
	if(ssl)
		return SSL_write(ssl->socket, data, strlen(data));

	else return 0;
}

void raw_socket(char *message) {
	char *sending = (char *) malloc(sizeof(char *) * strlen(message) + 3);
	
	printf("[+] execute-client: IRC: << %s\n", message);
	
	strcpy(sending, message);
	strcat(sending, "\r\n");
	
	if(ssl_write(ssl, sending) == -1)
		ERR_print_errors_fp(stderr);
	
	free(sending);
}

void irc_privmsg(char *chan, char *message) {
	char buffer[2048];
	
	snprintf(buffer, sizeof(buffer), "PRIVMSG %s :[%d] %s", chan, getpid(), message);
	raw_socket(buffer);
}

int main(int argc, char *argv[]) {
	char buffer[1024], length = 0, *match, *print;
	char *chan;
	size_t rlen;
	
	if(argc < 4)
		return 1;
	
	sockfd = atoi(argv[1]);
	sock   = atoi(argv[2]);
	chan   = argv[3];
	
	// ssl = ssl_init(sockfd);
	printf("[+] execute-client: forked: sockfd: %d, sock: %d, chan: %s\n", sockfd, sock, chan);
		
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
				dies("truncate");
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
				dies("truncate");
			}
		}
	}
	
	/* Clearing */
	close(sock);
	
	return 0;
}
