#ifndef __Z03_BOT_ACTIONS_H
	#define __Z03_BOT_ACTIONS_H
	
	void action_weather(ircmessage_t *message, char *args);
	void action_ping(ircmessage_t *message, char *args);
	void action_time(ircmessage_t *message, char *args);
	void action_random(ircmessage_t *message, char *args);
	void action_help(ircmessage_t *message, char *args);
	void action_stats(ircmessage_t *message, char *args);
	void action_chart(ircmessage_t *message, char *args);
	void action_uptime(ircmessage_t *message, char *args);
	void action_backlog_url(ircmessage_t *message, char *args);
	void action_seen(ircmessage_t *message, char *args);
	void action_somafm(ircmessage_t *message, char *args);
	void action_dns(ircmessage_t *message, char *args);
	void action_count(ircmessage_t *message, char *args);
	void action_known(ircmessage_t *message, char *args);
	void action_url(ircmessage_t *message, char *args);
	void action_google(ircmessage_t *message, char *args);
	void action_man(ircmessage_t *message, char *args);
	void action_notes(ircmessage_t *message, char *args);
	void action_run_c(ircmessage_t *message, char *args);
	void action_run_py(ircmessage_t *message, char *args);
	void action_run_hs(ircmessage_t *message, char *args);
	void action_run_php(ircmessage_t *message, char *args);
	void action_backlog(ircmessage_t *message, char *args);
	void action_wiki(ircmessage_t *message, char *args);
	void action_lastfm(ircmessage_t *message, char *args);
	void action_set(ircmessage_t *message, char *args);
	// void action_get(ircmessage_t *message, char *args);
	
	#define MAX_NOTES	4
#endif
