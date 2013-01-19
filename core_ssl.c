/* z03 - small bot with some network features - ssl core
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
#include "core_init.h"

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "core_ssl.h"

ssl_socket_t *init_socket_ssl(int sockfd, ssl_socket_t *ssl) {
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

int ssl_read(ssl_socket_t *ssl, char *data, int max) {
	if(ssl)
		return SSL_read(ssl->socket, data, max);
		
	else return 0;
}

int ssl_write(ssl_socket_t *ssl, char *data) {
	if(ssl)
		return SSL_write(ssl->socket, data, strlen(data));
		
	else return 0;
}

void ssl_close(ssl_socket_t *ssl) {
	if(ssl->socket) {
		SSL_shutdown(ssl->socket);
		SSL_free(ssl->socket);
	}
	
	if(ssl->sslContext)
		SSL_CTX_free(ssl->sslContext);

	free(ssl);
}

void ssl_error() {
	ERR_print_errors_fp(stderr);
}
