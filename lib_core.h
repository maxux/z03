#ifndef __Z03_LIB_CORE_H
	#define __Z03_LIB_CORE_H
	
	#define OUTPUT_PATH		"/var/www/perso/imagespawn/data/"
	// #define OUTPUT_PATH		"/tmp/spawn/"
	
	typedef struct request_t {
		char *match;
		void (*callback)(char *, char*);
		
	} request_t;
	
	typedef struct global_lib_t {
		void *dummy;
		
	} global_lib_t;
	
	extern unsigned int __request_count;
	extern request_t __request[];
	
	void main_core(char *data, char *request);
	
	char *extract_chan(char *data, char *destination, size_t size);
	size_t extract_nick(char *data, char *destination, size_t size);
	
	int nick_length_check(char *nick, char *channel);
	
	int pre_handle(char *data, ircmessage_t *message);
	int handle_message(char *data, ircmessage_t *message);
	void handle_nick(char *data);
	void handle_join(char *data);
#endif
