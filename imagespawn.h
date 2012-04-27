#ifndef __HEADER_IMAGESPAWN_H	
	#define __HEADER_IMAGESPAWN_H
	
	#define MAXBUFF			4096
	
	#define OUTPUT_PATH		"/var/www/perso/imagespawn/data/"
	// #define OUTPUT_PATH		"/tmp/spawn/"
	
	extern int sockfd;
	
	int init_irc_socket(char *server, int port);
	void raw_socket(int sockfd, char *message);
	int read_socket(int sockfd, char *data, char *next);
	char *skip_server(char *data);
	
	void handle_stats(char *data);
#endif
