#ifndef __Z03_BOT_ACTIONS_WEBSERVICES_H
	#define __Z03_BOT_ACTIONS_WEBSERVICES_H
	
	void action_weather(ircmessage_t *message, char *args);
	void action_wiki(ircmessage_t *message, char *args);
	void action_google(ircmessage_t *message, char *args);
	void action_calc(ircmessage_t *message, char *args);
	
	// exception from notice
	void action_whatcd(ircmessage_t *message, char *args);
#endif
