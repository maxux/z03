#ifndef __HEADER_IMAGESPAWN_H	
	#define __HEADER_IMAGESPAWN_H
	
	#define MAXBUFF			4096
	
	#define OUTPUT_PATH		"/var/www/perso/imagespawn/data/"
	// #define OUTPUT_PATH		"/tmp/spawn/"
	
	typedef struct request_t {
		char *match;
		void (*callback)(char *, char*);
		
	} request_t;
	
	extern unsigned int __request_count;
	extern request_t __request[];
	
	extern int sockfd;
	
	int init_irc_socket(char *server, int port);
	void raw_socket(int sockfd, char *message);
	int read_socket(int sockfd, char *data, char *next);
	char *skip_server(char *data);
	
	char *extract_chan(char *data, char *destination, size_t size);
	size_t extract_nick(char *data, char *destination, size_t size);
	
	int nick_length_check(char *nick, char *channel);
	
	int pre_handle(char *data, char *nick, size_t nick_size);
	int handle_message(char *message, char *nick);
	void handle_nick(char *data);
	void handle_join(char *data);
#endif
