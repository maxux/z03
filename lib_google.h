#ifndef __LIB_GOOGLE_ZO3_H
	#define __LIB_GOOGLE_ZO3_H
	
	typedef struct google_result_t {
		char *title;
		char *url;
		
	} google_result_t;
	
	typedef struct google_search_t {
		unsigned int length;
		struct google_result_t *result;
		
	} google_search_t;
	
	void google_free(google_search_t *search);
	google_search_t * google_search(char *keywords);
#endif
