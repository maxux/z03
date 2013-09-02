#ifndef __ZO3_LIB_URL
	#define __ZO3_LIB_URL
	
	#include <curl/curl.h>
	#define CURL_USERAGENT		"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/29.0.1547.22 Safari/537.36"
	#define CURL_USERAGENT_LEGACY	"curl/7.25.0 (i686-pc-linux-gnu) libcurl/7.25.0"
	#define CURL_WARN_SIZE          10 * 1024 * 1024        /* in bytes */
	
	/* Title Host ignore */
	extern char *__host_ignore[];
	
	extern request_t __url_request;
	
	char *extract_url(char *url);	
	void url_manager(ircmessage_t *message, char *args);
	int url_error(int errcode, curl_data_t *curl);
	
	int url_magic(curl_data_t *curl, ircmessage_t *message);
#endif
