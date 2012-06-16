#ifndef __Z03_CORE_INIT_H
	#define __Z03_CORE_INIT_H
	
	#define MAXBUFF			4096
	
	typedef struct codemap_t {
		char *filename;        /* library filename */
		void *handler;         /* library handler */
		void (*main)(char *, char *);  /* main library function */
		
	} codemap_t;
	
	typedef struct ircmessage_t {
		char nick[32];
		char chan[32];
		
		char *message;
		
		char *command;
		char *args;
		
	} ircmessage_t;
	
	extern int sockfd;
	
	int init_irc_socket(char *server, int port);
	void raw_socket(int sockfd, char *message);
	int read_socket(int sockfd, char *data, char *next);
	char *skip_server(char *data);
	
	void handle_private_message(char *data);
#endif
