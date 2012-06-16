#ifndef __Z03_BOT_ACTIONS_H
	#define __Z03_BOT_ACTIONS_H
	
	typedef struct weather_station_t {
		int id;
		char *ref;
		char *name;
	
	} weather_station_t;
	
	void action_weather(char *chan, char *args);
	void action_ping(char *chan, char *args);
	void action_time(char *chan, char *args);
	void action_random(char *chan, char *args);
	void action_help(char *chan, char *args);
	void action_stats(char *chan, char *args);
	void action_chart(char *chan, char *args);
	void action_uptime(char *chan, char *args);
#endif
