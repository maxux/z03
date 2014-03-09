#ifndef __IRCMISC_H
	#define __IRCMISC_H
	
	#include "downloader.h"

	char *clean_filename(char *file);
	
	char *ltrim(char *str);
	char *rtrim(char *str);
	char *crlftrim(char *str);
	
	char *space_encode(char *str);
	char *url_encode(char *url);
	
	char *string_index(char *str, unsigned int index);
	void intswap(int *a, int *b);
	
	char *anti_hl(char *nick);
	char *anti_hl_alloc(char *nick);
	char *anti_hl_each_words(char *str, size_t len, charset_t charset);
	
	char *time_elapsed(time_t time);
	
	char *irc_mstrncpy(char *src, size_t len);
	char *skip_header(char *data);
	
	int irc_extract_userdata(char *data, char **nick, char **username, char **host);
	
	int progression_match(size_t value);
	
	size_t words_count(char *str);
	
	time_t today();
	
	char *list_nick_implode(list_t *list);
	
	char *md5_ascii(char *source);
	char *sha1_string(unsigned char *sha1_hexa, char *sha1_char);
	
	int file_write(const char *filename, char *buffer, size_t length);
	
	char *spacetrunc(char *str, size_t maxlen);
#endif
