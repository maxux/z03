#ifndef __Z03_CORE_SSL_HEADER
	#define __Z03_CORE_SSL_HEADER

	#include <openssl/ssl.h>

	typedef struct {
		int sockfd;
		SSL *socket;
		SSL_CTX *sslContext;
		
	} ssl_socket_t;
	
	
	ssl_socket_t *ssl_init(int sockfd);
	int ssl_read(ssl_socket_t *ssl, char *data, int max);
	int ssl_write(ssl_socket_t *ssl, char *data);
	void ssl_close(ssl_socket_t *ssl);
#endif
