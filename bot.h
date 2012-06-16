#ifndef __IMAGESPAWN_BOT_HEADER
	#define __IMAGESPAWN_BOT_HEADER
	
	#define IRC_SERVER		"192.168.20.1"
	#define IRC_PORT		6667
	
	#define IRC_REALNAME		"Zoé"
	#define IRC_USERNAME		"z03"
	
	#ifdef __DEBUG__
		#define IRC_NICK		"z03`test"
		#define IRC_CHANNEL		"#test"
		#define IRC_NICKSERV		0
		#define IRC_NICKSERV_PASS	""

	#else
		#define IRC_NICK		"z03"
		#define IRC_CHANNEL		"#inpres"
		#define IRC_NICKSERV		1
		#define IRC_NICKSERV_PASS	"z03_passwd"
	#endif
	
	#define IRC_ADMIN_HOST		"Maxux!maxux@maxux.net"
	
	/* Modules */
	#define NICK_MAX_LENGTH		15
#endif
