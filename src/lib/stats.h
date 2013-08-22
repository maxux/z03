#ifndef __Z03_BOT_STATS_H
	#define __Z03_BOT_STATS_H
	
	list_t *stats_nick_read(char *chan);
	channel_t *stats_channel_read(char *chan);
	channel_t *stats_channel_load(char *chan);
	size_t stats_get_lasttime(char *nick, char *chan);
	size_t stats_get_words(char *nick, char *chan);
	void stats_update(ircmessage_t *message, nick_t *nick, char new);
	
	void stats_rebuild_all(list_t *channels);
	void stats_load_all(list_t *root);
	
	void stats_daily_update();
	
	int stats_url_count(char *chan);
#endif
