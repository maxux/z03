#ifndef __IRCMISC_H
	#define __IRCMISC_H

	char * clean_filename(char *file);
	char * trim(char *str, unsigned int len);
	char * short_trim(char *str);
	char * space_encode(char *str);
	
	char * anti_hl(char *nick);
	char * anti_hl_each_words(char *str, size_t len, charset_t charset);
	
	char * time_elapsed(time_t time);
	
	char * irc_mstrncpy(char *src, size_t len);
	char * skip_header(char *data);
	
	int irc_extract_userdata(char *data, char **nick, char **username, char **host);
	
	char * irc_knownuser(char *nick, char *host);
	
	whois_t * whois_init();
	whois_t * irc_whois(char *nick);
	void whois_free(whois_t *whois);
	
	void irc_privmsg(char *chan, char *message);
#endif
