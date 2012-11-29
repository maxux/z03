#ifndef __Z03_CORE_INIT_H
	#define __Z03_CORE_INIT_H
	
	#include <time.h>
	
	#define MAXBUFF			4096
	
	typedef struct codemap_t {
		char *filename;        /* library filename */
		void *handler;         /* library handler */
		void (*main)(char *, char *);  /* main library function */
		void (*destruct)(void);        /* unlinking threads, ... */
		
	} codemap_t;
	
	typedef struct ircmessage_t {
		char nick[32];
		char chan[32];
		
		char *message;
		
		char *command;
		char *args;
		
	} ircmessage_t;
	
	typedef struct global_core_t {
		time_t startup_time;
		
		time_t rehash_time;
		unsigned int rehash_count;
		
	} global_core_t;
	
	typedef struct whois_t {
		char *host;
		char *ip;
		
	} whois_t;
	
	extern int sockfd;
	extern global_core_t global_core;
	
	int init_socket(char *server, int port);
	void raw_socket(int sockfd, char *message);
	int read_socket(int sockfd, char *data, char *next);
	char *skip_server(char *data);
	
	void handle_private_message(char *data);
#endif
