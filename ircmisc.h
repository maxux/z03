#ifndef __IRCMISC_H
	#define __IRCMISC_H

	char * clean_filename(char *file);
	char * trim(char *str, unsigned int len);
	
	char * anti_hl(char *nick);
	char * anti_hl_each_words(char *str, size_t len);
#endif
