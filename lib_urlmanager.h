#ifndef __IMAGESPAWN_URL
	#define __IMAGESPAWN_URL
	
	#include <curl/curl.h>
	#define CURL_USERAGENT		"Mozilla/5.0 (Windows NT 6.1; WOW64; rv:9.0.1; spawnimg) Gecko/20100101 Firefox/9.0.1"
	#define CURL_USERAGENT_LEGACY	"curl/7.25.0 (i686-pc-linux-gnu) libcurl/7.25.0"
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
		char forcedl;
		char *cookie;
		
	} curl_data_t;
	
	typedef struct host_cookies_t {
		char *host;
		char *cookie;
		
	} host_cookies_t;
	
	char *extract_url(char *url);
	
	curl_data_t *curl_data_new();
	void curl_data_free(curl_data_t *data);
	
	size_t curl_header_validate(char *ptr, size_t size, size_t nmemb, void *userdata);
	size_t curl_body(char *ptr, size_t size, size_t nmemb, void *userdata);
	char * curl_cookie(char *url);
	int curl_download(char *url, curl_data_t *data, char forcedl);
	int curl_download_post(char *url, curl_data_t *data, char *post);
	int curl_download_text(char *url, curl_data_t *data);
	int curl_download_text_post(char *url, curl_data_t *data, char *post);
	
	char * repost();
	
	int handle_url(ircmessage_t *message, char *url);
	int handle_url_dispatch(char *url, ircmessage_t *message, char already_match);
	int handle_url_image(char *url, curl_data_t *curl);
	
	char * url_extract_title(char *body, char *title);
	enum charset_t url_extract_charset(char *body);
	
	void handle_url_title(char *url);
	char * shurl(char *url);
	
	// GNU Fix
	extern char * strcasestr(const char *, const char *);
#endif
