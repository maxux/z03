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
#include "init.h"

#include <pthread.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>

#include "ssl.h"

static pthread_mutex_t *lockarray;

/* openssl callback for thread safe libcurl */
static void lock_callback(int mode, int type, char *file, int line) {
	(void) file;
	(void) line;
	
	if(mode & CRYPTO_LOCK)
		pthread_mutex_lock(&(lockarray[type]));
		
	else  pthread_mutex_unlock(&(lockarray[type]));
}

static unsigned long thread_id(void) {
	unsigned long ret;

	ret = (unsigned long) pthread_self();
	return ret;
}

static void init_locks(void) {
	int i;

	lockarray = (pthread_mutex_t *) OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
	
	for(i = 0; i < CRYPTO_num_locks(); i++)
		pthread_mutex_init(&(lockarray[i]), NULL);

	CRYPTO_set_id_callback((unsigned long (*)()) thread_id);
	CRYPTO_set_locking_callback((void (*)()) lock_callback);
}

static void kill_locks(void) {
	int i;

	CRYPTO_set_locking_callback(NULL);
	
	for(i = 0; i < CRYPTO_num_locks(); i++)
		pthread_mutex_destroy(&(lockarray[i]));

	OPENSSL_free(lockarray);
}

ssl_socket_t *init_socket_ssl(int sockfd, ssl_socket_t *ssl) {
	ssl->socket     = NULL;
	ssl->sslContext = NULL;
	ssl->sockfd = sockfd;
	
	// register the error strings for libcrypto & libssl
	SSL_load_error_strings();	
	
	// register the available ciphers and digests
	SSL_library_init();
	
	init_locks();

	// new context saying we are a client, and using SSL 2 or 3
	ssl->sslContext = SSL_CTX_new(SSLv23_client_method());
	if(ssl->sslContext == NULL)
		ERR_print_errors_fp(stderr);

	// create an SSL struct for the connection
	ssl->socket = SSL_new(ssl->sslContext);
	if(ssl->socket == NULL)
		ERR_print_errors_fp(stderr);

	// connect the SSL struct to our connection
	if(!SSL_set_fd(ssl->socket, ssl->sockfd))
		ERR_print_errors_fp(stderr);

	// initiate SSL handshake
	if(SSL_connect(ssl->socket) != 1)
		ERR_print_errors_fp(stderr);
	
	// avoid manuel ssl retry
	SSL_set_mode(ssl->socket, SSL_MODE_AUTO_RETRY);
	
	pthread_mutex_init(&ssl->mutwrite, NULL);

	return ssl;
}

int ssl_read(ssl_socket_t *ssl, char *data, int max) {
	int errcode, error;
	char errstr[1024];
	
	if(!ssl)
		return 0;
		
	while((errcode = SSL_read(ssl->socket, data, max)) < 0) {
		error = SSL_get_error(ssl->socket, errcode);
		
		if(error == SSL_ERROR_SYSCALL)
			perror("[-] core: ssl read");
		
		if(error == SSL_ERROR_WANT_READ) {
			usleep(10000);
			continue;
		}
		
		printf("[-] core: ssl read: %d, %s\n", error, ERR_error_string(error, errstr));
		
		break;
	}
	
	return errcode;
}

int ssl_write(ssl_socket_t *ssl, char *data) {
	int errcode, error;
	char errstr[1024];
	
	if(!ssl)
		return 0;
	
	pthread_mutex_lock(&ssl->mutwrite);
	
	while((errcode = SSL_write(ssl->socket, data, strlen(data))) < 0) {
		error = SSL_get_error(ssl->socket, errcode);
		
		if(error == SSL_ERROR_WANT_WRITE) {
			usleep(10000);
			continue;
		}
		
		if(error == SSL_ERROR_SYSCALL)
			perror("[-] core: ssl write");
		
		printf("[-] core: ssl write: %d, %s\n", error, ERR_error_string(error, errstr));
		
		break;
	}
	
	pthread_mutex_unlock(&ssl->mutwrite);
	
	return errcode;
}

void ssl_close(ssl_socket_t *ssl) {
	if(ssl->socket) {
		SSL_shutdown(ssl->socket);
		SSL_free(ssl->socket);
	}
	
	if(ssl->sslContext)
		SSL_CTX_free(ssl->sslContext);

	free(ssl);
	kill_locks();
}

void ssl_error() {
	ERR_print_errors_fp(stderr);
}
