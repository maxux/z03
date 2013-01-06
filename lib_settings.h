#ifndef __LIB_SETTINGS_ZO3_H
	#define __LIB_SETTINGS_ZO3_H
	
	int settings_set(char *nick, char *key, char *value);
	char *settings_get(char *nick, char *key);
#endif
