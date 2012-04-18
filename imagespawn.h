#ifndef __HEADER_IMAGESPAWN_H	
	#define __HEADER_IMAGESPAWN_H
	
	#define MAXBUFF			4096
	
	#define OUTPUT_PATH		"/var/www/perso/imagespawn/data/"
	#define CURL_USERAGENT		"Mozilla/5.0 (Windows NT 6.1; WOW64; rv:9.0.1; spawnimg) Gecko/20100101 Firefox/9.0.1"
	
	#define SQL_DATABASE_FILE	"sp0wimg.sqlite3"
	
	typedef struct curl_data_t {
		char *data;
		size_t length;
		
	} curl_data_t;
	
	sqlite3 * db_sqlite_init();
	
	int init_irc_socket(char *server, int port);
	void raw_socket(int sockfd, char *message);
	int read_socket(int sockfd, char *data, char *next);
	char *skip_server(char *data);
	
	size_t curl_header(char *ptr, size_t size, size_t nmemb, void *userdata);
	size_t curl_body(char *ptr, size_t size, size_t nmemb, void *userdata);
	int curl_download(char *url, curl_data_t *data);
	int handle_url(char *nick, char *url);
	
	int youtube_extract_title(char *body, char *title);
	void handle_youtube(char *url);
	
	char * clean_filename(char *file);
	char * trim(char *str, unsigned int len);
	char * anti_hl(char *nick);
	
	char * repost();
#endif
