#ifndef __LIB_LASTFM_ZO3_H
	#define __LIB_LASTFM_ZO3_H
	
	typedef enum lastfm_type_t {
		UNKNOWN,
		NOW_PLAYING,
		LAST_PLAYED,
		ERROR
		
	} lastfm_type_t;
	
	typedef struct lastfm_t {
		enum lastfm_type_t type;
		unsigned int total;
		char *date;
		char *message;
		char *artist;
		char *title;
		char *album;
		
	} lastfm_t;
	
	lastfm_t * lastfm_new();
	void lastfm_free(lastfm_t *lastfm);
	
	lastfm_t * lastfm_getplaying(char *user);
#endif
