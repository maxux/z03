#ifndef __Z03_CORE_SSL_HEADER
	#define __Z03_CORE_SSL_HEADER

	#include <openssl/ssl.h>
	#include <pthread.h>

	typedef struct {
		int sockfd;
		SSL *socket;
		SSL_CTX *sslContext;
		pthread_mutex_t mutwrite;
		
	} ssl_socket_t;
	
	
	ssl_socket_t *init_socket_ssl(int sockfd, ssl_socket_t *ssl);
	int ssl_read(ssl_socket_t *ssl, char *data, int max);
	int ssl_write(ssl_socket_t *ssl, char *data);
	void ssl_close(ssl_socket_t *ssl);
	void ssl_error();
#endif
