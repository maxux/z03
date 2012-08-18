#ifndef __IMAGESPAWN_URL
	#define __IMAGESPAWN_URL
	
	#include <curl/curl.h>
	#define CURL_USERAGENT		"Mozilla/5.0 (Windows NT 6.1; WOW64; rv:9.0.1; spawnimg) Gecko/20100101 Firefox/9.0.1"
	#define CURL_MAX_SIZE		20 * 1024 * 1024	/* 20 Mo */
	
	/* Title Host ignore */
	extern char *__host_ignore[];
	
	typedef enum repost_type_t {
		URL_MATCH,
		CHECKSUM_MATCH,
		TITLE_MATCH,
		
	} repost_type_t;
	
	
	typedef enum document_type_t {
		UNKNOWN_TYPE,
		TEXT_HTML,
		IMAGE_ALL
		
	} document_type_t;
	
	typedef enum charset_t {
		UNKNOWN_CHARSET,
		UTF_8,
		ISO_8859,
		WIN_1252
		
	} charset_t;
	
	typedef struct curl_data_t {
		char *data;
		size_t http_length;
		size_t length;
		long code;
		char *http_type;
		enum document_type_t type;
		enum charset_t charset;
		CURLcode curlcode;
		
	} curl_data_t;
	
	char *extract_url(char *url);
	
	size_t curl_header_validate(char *ptr, size_t size, size_t nmemb, void *userdata);
	size_t curl_body(char *ptr, size_t size, size_t nmemb, void *userdata);
	
	int curl_download(char *url, curl_data_t *data);
	
	char * repost();
	
	int handle_url(ircmessage_t *message, char *url);
	int handle_url_dispatch(char *url, ircmessage_t *message, char already_match);
	int handle_url_image(char *url, curl_data_t *curl);
	
	char * url_extract_title(char *body, char *title);
	enum charset_t url_extract_charset(char *body);
	
	void handle_url_title(char *url);
	
	// GNU Fix
	extern char * strcasestr(const char *, const char *);
#endif
