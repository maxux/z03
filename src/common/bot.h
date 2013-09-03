#ifndef __Z03_BOT_HEADER
	#define __Z03_BOT_HEADER
	
	#include "private.h"
	
	// irc server host/ip
	#define IRC_SERVER              "irc.maxux.net"
	
	// irc server port
	#define IRC_PORT                6697
	
	// use ssl for connection
	#define IRC_USE_SSL             1
	
	// basic information
	#define IRC_REALNAME            "Zoé"
	#define IRC_USERNAME            "z03"
	
	#ifndef __DEBUG__
		#define IRC_NICK                "z03"
		
		// if ircnick is enabled, nickserv *must* authentificate
		// the bot, otherwise the bot will not auto-join channels
		#define IRC_NICKSERV            1
		#define IRC_NICKSERV_PASS       PRIVATE_NICKSERV
		
		#define IRC_OPER                0
		#define IRC_OPER_PASS           PRIVATE_OPERSERV
		
		#ifdef __LIB_CORE_C // define required for include protection
			static const char *IRC_CHANNEL[] = {"#channel1", "#channel2"};
		#endif
		
		// sqlite database filename
		#define SQL_DATABASE_FILE       "z03.sqlite3"
		
	#else
		#define IRC_NICK                "z03`test"
		#define IRC_NICKSERV            0
		#define IRC_NICKSERV_PASS       ""
		#define IRC_OPER                0
		#define IRC_OPER_PASS           ""
		
		#ifdef __LIB_CORE_C // define required for include protection
			static const char *IRC_CHANNEL[] = {"#test"};
		#endif
		
		#define SQL_DATABASE_FILE       "z03-test.sqlite3"
	#endif
	
	// mirroring images to this path
	#define ENABLE_MIRRORING
	#define MIRROR_PATH             "/tmp/"
	
	// time format used by strftime (man 3 strftime)
	#define TIME_FORMAT             "%A %d %B %X %Y UTC%z (%Z)"
	
	// admin format: nick!username@host
	// admin is able to sent bot remote raw command
	#define IRC_ADMIN_HOST          ""
	
	// kickban nick longer than this limit
	#define ENABLE_NICK_MAX_LENGTH
	#define NICK_MAX_LENGTH		15           // 15 bytes
	
	// notificate when a user has not been seen since this time
	#define ENABLE_NICK_LASTTIME
	#define NICK_LASTTIME     5 * 24 * 60 * 60   // 5 days
#endif
