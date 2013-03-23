#ifndef __IRCMISC_H
	#define __IRCMISC_H
	
	#include "lib_urlmanager.h"

	char *clean_filename(char *file);
	
	char *ltrim(char *str);
	char *rtrim(char *str);
	char *crlftrim(char *str);
	
	char *space_encode(char *str);
	char *string_index(char *str, unsigned int index);
	void intswap(int *a, int *b);
	
	char *anti_hl(char *nick);
	char *anti_hl_each_words(char *str, size_t len, charset_t charset);
	
	char *time_elapsed(time_t time);
	
	char *irc_mstrncpy(char *src, size_t len);
	char *skip_header(char *data);
	
	int irc_extract_userdata(char *data, char **nick, char **username, char **host);
	
	int progression_match(size_t value);
	
	char *md5ascii(char *source);
	size_t words_count(char *str);
	
	time_t today();
#endif
