#ifndef __Z03_LIB_WHOIS_H
	#define __Z03_LIB_WHOIS_H
	
	typedef struct whois_t {
		char *host;
		char *ip;
		char *modes;
		
	} whois_t;
	
	whois_t *whois_init();
	whois_t *irc_whois(char *nick);
	void whois_free(whois_t *whois);
#endif
