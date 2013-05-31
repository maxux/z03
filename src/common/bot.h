#ifndef __IMAGESPAWN_BOT_HEADER
	#define __IMAGESPAWN_BOT_HEADER
	
	#include "private.h"
	
	#define IRC_SERVER		"192.168.20.1"
	#define IRC_PORT		6697
	#define IRC_USE_SSL             1
	
	#define IRC_REALNAME		"Zoé"
	#define IRC_USERNAME		"z03"
	
	#ifdef __DEBUG__
		#define IRC_NICK		"z03`test"
		#define IRC_NICKSERV		0
		#define IRC_NICKSERV_PASS	""
		
		#ifdef __LIB_CORE_C
			static const char *IRC_CHANNEL[] = {"#test", "#test2"};
		#endif

	#else
		#define IRC_NICK		"z03"
		#define IRC_NICKSERV		1
		#define IRC_NICKSERV_PASS	PRIVATE_NICKSERV
		#define IRC_OPER		1
		#define IRC_OPER_PASS		PRIVATE_OPERSERV
		
		#ifdef __LIB_CORE_C
			static const char *IRC_CHANNEL[] = {"#inpres", "#bapteme", "#test"};
		#endif
	#endif
	
	#define IRC_ADMIN_HOST		"Maxux!maxux@fingerprint.certified"
	
	/* Modules */
	#define NICK_MAX_LENGTH		15
#endif
