#ifndef __IRCMISC_H
	#define __IRCMISC_H

	char * clean_filename(char *file);
	char * trim(char *str, unsigned int len);
	char * short_trim(char *str);
	
	char * anti_hl(char *nick);
	char * anti_hl_each_words(char *str, size_t len, charset_t charset);
	
	char * time_elapsed(time_t time);
#endif
