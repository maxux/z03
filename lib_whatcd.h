#ifndef __LIB_WHATCD_ZO3_H
	#define __LIB_WHATCD_ZO3_H
	
	typedef struct whatcd_t {
		char *session;
		
	} whatcd_t;
	
	typedef struct whatcd_release_t {		
		double torrentid;
		double groupid;
		char *groupname;
		char *format;
		char *media;
		double size;
		int unread;
		
	} whatcd_release_t;
	
	typedef struct whatcd_request_t {
		char *error;
		list_t *response;
		
	} whatcd_request_t;
	
	whatcd_t *whatcd_new(char *session);
	void whatcd_free(whatcd_t *whatcd);
	
	whatcd_release_t *whatcd_release_new();
	void whatcd_release_free(whatcd_release_t *release);
	
	whatcd_request_t *whatcd_request_new();
	void whatcd_request_free(whatcd_request_t *request);
	
	whatcd_request_t *whatcd_notification(whatcd_t *whatcd);
	
	#define WHATCD_API_BASE    "https://what.cd/ajax.php?action="
	#define WHATCD_TORRENT     "https://what.cd/torrents.php?id="
#endif
