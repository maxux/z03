#ifndef __Z03_LIB_DOWNLOADER
	#define __Z03_LIB_DOWNLOADER
	
	#include <curl/curl.h>
	#define CURL_USERAGENT		"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/29.0.1547.22 Safari/537.36"
	#define CURL_USERAGENT_LEGACY	"curl/7.25.0 (i686-pc-linux-gnu) libcurl/7.25.0"
	#define CURL_MAX_SIZE		25 * 1024 * 1024        /* in bytes */
	
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
		char *url;                     // original url
		char *data;                    // output data
		size_t length;                 // output data length
		size_t http_length;            // http Content-Length value
		long code;                     // http code (200, 404, ...)
		char *http_type;               // http Content-Type value
		enum document_type_t type;     // parsed Content-Type
		enum charset_t charset;        // parsed Content-Type charset
		CURLcode curlcode;             // curl return error code
		char forcedl;                  // flag to force download
		char *cookie;                  // optional cookie
		
		// body callback wrapper
		size_t (*body_callback)(char *, size_t, size_t, void *);
		
	} curl_data_t;
	
	typedef struct host_cookies_t {
		char *host;
		char *cookie;
		
	} host_cookies_t;
	
	curl_data_t *curl_data_new();
	void curl_data_free(curl_data_t *data);
	
	int curl_download(char *url, curl_data_t *data, char forcedl);
	int curl_download_nobody(char *url, curl_data_t *data, char forcedl);
	int curl_download_post(char *url, curl_data_t *data, char *post);
	int curl_download_text(char *url, curl_data_t *data);
	int curl_download_text_post(char *url, curl_data_t *data, char *post);
	
	charset_t curl_extract_charset(char *line);
	char *curl_gethost(char *url);
#endif
