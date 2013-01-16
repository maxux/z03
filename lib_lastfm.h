#ifndef __LIB_LASTFM_ZO3_H
	#define __LIB_LASTFM_ZO3_H
	
	typedef enum lastfm_type_t {
		UNKNOWN,
		NOW_PLAYING,
		LAST_PLAYED,
		ERROR
		
	} lastfm_type_t;
	
	typedef struct lastfm_track_t {
		enum lastfm_type_t type;
		unsigned int total;
		time_t date;
		char *artist;
		char *title;
		char *album;
		
		char *error;
		
	} lastfm_track_t;
	
	typedef struct lastfm_request_t {
		char *reply;
		char *error;
		
	} lastfm_request_t;
	
	typedef struct lastfm_t {
		char *apikey;
		char *apisecret;
		char *token;
		char *session;
		
		struct lastfm_track_t *track;
				
	} lastfm_t;
	
	lastfm_t *lastfm_new(char *apikey, char *apisecret);
	void lastfm_free(lastfm_t *lastfm);
	
	lastfm_request_t *lastfm_request_new();
	void lastfm_request_free(lastfm_request_t *request);
	
	lastfm_request_t *lastfm_getplaying(lastfm_t *lastfm, lastfm_request_t *request, char *user);
	
	lastfm_request_t *lastfm_api_gettoken(lastfm_t *lastfm, lastfm_request_t *request);
	char *lastfm_api_authorize(lastfm_t *lastfm);
	
	lastfm_request_t *lastfm_api_getsession(lastfm_t *lastfm, lastfm_request_t *request);
	lastfm_request_t *lastfm_api_love(lastfm_t *lastfm, lastfm_request_t *request);
	
	#define lastfm_url(x, ...)     snprintf(x, sizeof(x), LASTFM_API_ROOT __VA_ARGS__)
#endif
