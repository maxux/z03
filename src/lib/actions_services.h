#ifndef __Z03_BOT_ACTIONS_SERVICES_H
	#define __Z03_BOT_ACTIONS_SERVICES_H
	
	void __action_notes_checknew(char *chan, char *nick);
	void action_notes(ircmessage_t *message, char *args);
	void action_delay(ircmessage_t *message, char *args);
	
	#define MAX_NOTES	4
#endif
