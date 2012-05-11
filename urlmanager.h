#ifndef __IMAGESPAWN_URL
	#define __IMAGESPAWN_URL
	
	#define CURL_USERAGENT		"Mozilla/5.0 (Windows NT 6.1; WOW64; rv:9.0.1; spawnimg) Gecko/20100101 Firefox/9.0.1"
	#define CURL_MAX_SIZE		20 * 1024 * 1024	/* 20 Mo */
	
	/* Title Host ignore */
	extern char *__host_ignore[];	
	
	typedef enum document_type_t {
		UNKNOWN,
		TEXT_HTML,
		IMAGE_ALL
		
	} document_type_t;
	
	typedef struct curl_data_t {
		char *data;
		size_t length;
		enum document_type_t type;
		
	} curl_data_t;
	
	char *extract_url(char *url);
	
	size_t curl_header_validate(char *ptr, size_t size, size_t nmemb, void *userdata);
	size_t curl_body(char *ptr, size_t size, size_t nmemb, void *userdata);
	
	int curl_download(char *url, curl_data_t *data);
	
	char * clean_filename(char *file);
	char * trim(char *str, unsigned int len);
	char * anti_hl(char *nick);
	char * repost();
	
	int handle_url(char *nick, char *url);
	int handle_url_dispatch(char *url);
	int handle_url_image(char *url, curl_data_t *curl);
	
	char * url_extract_title(char *body, char *title);
	void handle_url_title(char *url);
#endif
