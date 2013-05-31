#ifndef __LIB_SETTINGS_ZO3_H
	#define __LIB_SETTINGS_ZO3_H
	
	typedef enum settings_view {
		PUBLIC  = 0,
		PRIVATE = 1
	} settings_view;
	
	int settings_set(char *nick, char *key, char *value, settings_view view);
	char *settings_get(char *nick, char *key, settings_view view);
	int settings_unset(char *nick, char *key);
	
	settings_view settings_getview(char *nick, char *key);
	list_t *settings_by_key(char *key, settings_view view);
#endif
