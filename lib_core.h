#ifndef __Z03_LIB_CORE_H
	#define __Z03_LIB_CORE_H
	
	#include <time.h>
	#include <string.h>
	#include "lib_list.h"
	
	#define OUTPUT_PATH		"/var/www/perso/imagespawn/data/"
	
	typedef struct nick_t {
		size_t lines;
		
	} nick_t;
	
	typedef struct channel_t {
		/* Channel Data */
		time_t last_chart_request;
		time_t last_backurl_request;
		time_t last_backlog_request;
		
		list_t *nicks;
		
		size_t lines;
		
	} channel_t;
	
	typedef struct ircmessage_t {
		char nick[32];
		char nickhl[36];
		
		/* Warning: not clear:
			- chan is channel name (retrocompatibility)
			- channel is channel_t object
		*/
		char chan[64];
		struct channel_t *channel;
		
		char *message;
		
		char *command;
		char *args;
		
	} ircmessage_t;
	
	typedef struct request_t {
		char *match;
		void (*callback)(ircmessage_t *, char *);
		char *man;
		
	} request_t;
	
	
	
	typedef struct global_lib_t {
		struct list_t *channels;
		
	} global_lib_t;
	
	/* Action Request */
	extern unsigned int __request_count;
	extern request_t __request[];
	
	/* Global Data */
	extern global_lib_t global_lib;
	
	
	char *match_prefix(char *data, char *match);
	
	void main_construct(void);
	void main_core(char *data, char *request);
	void main_destruct(void);
	
	char *extract_chan(char *data, char *destination, size_t size);
	size_t extract_nick(char *data, char *destination, size_t size);
	
	int nick_length_check(char *nick, char *channel);
	
	int pre_handle(char *data, ircmessage_t *message);
	int handle_message(char *data, ircmessage_t *message);
	void handle_nick(char *data);
	void handle_join(char *data);
	
	void irc_kick(char *chan, char *nick, char *reason);
	
	#define zsnprintf(x, ...) snprintf(x, sizeof(x), __VA_ARGS__)
#endif
