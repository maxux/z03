#ifndef __ZO3_LIB_URL
	#define __ZO3_LIB_URL
	
	#define CURL_WARN_SIZE          10 * 1024 * 1024        /* in bytes */
	
	extern char *__host_ignore[];
	extern request_t __url_request;
	
	char *extract_url(char *url);
	char *extract_host(char *url);
		
	void url_manager(ircmessage_t *message, char *args);
	int url_error(int errcode, curl_data_t *curl);
	
	int url_magic(curl_data_t *curl, ircmessage_t *message);
	
	int url_format_log(char *sqlquery, ircmessage_t *message);
	
	char *url_ender(char *src);
#endif
