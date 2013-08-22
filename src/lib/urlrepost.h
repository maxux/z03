#ifndef __ZO3_LIB_URLREPOST
	#define __ZO3_LIB_URLREPOST
	
	typedef enum repost_type_t {
		NOT_REPOST,        // not a repost
		NICK_IS_OP,        // current nick is the original poster
		URL_MATCH,         // the url match on database
		CHECKSUM_MATCH,    // data checksum match
		TITLE_MATCH,       // page title match
		
	} repost_type_t;
	
	typedef struct repost_t {
		int id;              // row id
		char *op;            // original poster
		char *url;           // original url
		time_t timestamp;    // original timestamp
		int hit;             // url hit count
		repost_type_t type;
		char *sha1;          // sha-1 checksum
		char *title;         // html title
		
	} repost_t;
	
	repost_t *repost_new();
	void repost_free(repost_t *repost);
	
	repost_t *url_repost_hit(char *url, ircmessage_t *message, repost_t *repost);
	repost_t *url_repost_advanced(curl_data_t *curl, ircmessage_t *message, repost_t *repost);
#endif
