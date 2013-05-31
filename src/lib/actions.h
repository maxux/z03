#ifndef __Z03_BOT_ACTIONS_H
	#define __Z03_BOT_ACTIONS_H
	
	char *action_check_args(char *args);
	char *action_parse_args(ircmessage_t *message, char *args);
	void action_help(ircmessage_t *message, char *args);
	void action_man(ircmessage_t *message, char *args);
#endif
