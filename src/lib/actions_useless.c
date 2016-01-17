/* z03 - useless/easter eggs commands
 * Author: Daniel Maxime (root@maxux.net)
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <stdio.h> 
#include <stdlib.h>
#include "core.h"
#include "database.h"
#include "run.h"
#include "actions_useless.h"
#include "settings.h"
#include "downloader.h"
#include "actions_backlog.h"
#include "actions.h"

//
// registering commands
//

static request_t __action_blowjob = {
	.match    = ".blowjob",
	.callback = action_useless_blowjob,
	.man      = "",
	.hidden   = 1,
	.syntaxe  = "",
};

static request_t __action_km = {
	.match    = ".km",
	.callback = action_useless_km,
	.man      = "",
	.hidden   = 1,
	.syntaxe  = "",
};

static request_t __action_kp = {
	.match    = ".kp",
	.callback = action_useless_km,
	.man      = "",
	.hidden   = 1,
	.syntaxe  = "",
};


static request_t __action_hodor = {
	.match    = ".hodor",
	.callback = action_useless_hodor,
	.man      = "hodor hodor hodor",
	.hidden   = 1,
	.syntaxe  = "",
};

static request_t __action_plusone = {
	.match    = "+1",
	.callback = action_useless_plusone,
	.man      = "+1 !",
	.hidden   = 1,
	.syntaxe  = "",
};

static request_t __action_minusone = {
	.match    = "-1",
	.callback = action_useless_minusone,
	.man      = "-1 !",
	.hidden   = 1,
	.syntaxe  = "",
};

static request_t __action_popcorn = {
	.match    = ".popcorn",
	.callback = action_useless_popcorn,
	.man      = "ONOMONOMONOM",
	.hidden   = 1,
	.syntaxe  = "",
};

static request_t __action_fake = {
	.match    = "fake",
	.callback = action_useless_fake,
	.man      = "",
	.hidden   = 1,
	.syntaxe  = "",
};

__registrar actions_useless() {
	request_register(&__action_blowjob);
	request_register(&__action_km);
	request_register(&__action_hodor);
	request_register(&__action_kp);
	request_register(&__action_plusone);
	request_register(&__action_minusone);
	request_register(&__action_popcorn);
	request_register(&__action_fake);
}

//
// commands implementation
//
static char *__blowjobs_messages[] = {
	"Pas touche, connard !",
	"VTFF",
	"Puis quoi encore ?!",
	"La pute, c'est Paglops, pas moi !",
	"Demande à Paglops plutôt",
	"Celles de Paglops vallent 250€, tu veux pas elle plutôt ?",
	"J'suis pas la pute du chan, biatch"
};

void action_useless_blowjob(ircmessage_t *message, char *args) {
	(void) args;
	int index = rand() % (sizeof(__blowjobs_messages) / sizeof(char *));
	irc_kick(message->chan, message->nick, __blowjobs_messages[index]);
}

void action_useless_km(ircmessage_t *message, char *args) {
	(void) args;
	char buffer[1024];
	
	zsnprintf(buffer, "take that biatch");
	irc_kick(message->chan, "Malabar", buffer);
}


void action_useless_hodor(ircmessage_t *message, char *args) {
	(void) args;
	char output[2048];
	unsigned int i, value;
	
	// initializing
	value = (rand() % 10) + 1;
	*output = '\0';
	
	// copy hodor random time
	for(i = 0; i < value; i++)
		strcat(output, "hodor ");
	
	// trim last space
	*(output + strlen(output) - 1) = '\0';
	
	irc_privmsg(message->chan, output);
}

/* void action_sudo(ircmessage_t *message, char *args) {
	(void) args;
	irc_privmsg(message->chan, "sudo: you are not sudoers");
} */

void action_useless_one(ircmessage_t *message, char *args, char *howmany) {
	sqlite3_stmt *stmt;
	char *sqlquery, *match;
	int count = 0;
	char msg[256];
	
	if(!strlen(action_check_args(args)))
		return;
	
	if((match = strchr(args, ' ')))
		*match = '\0';
	
	if(!strcmp(message->nick, args)) {
		irc_kick(message->chan, message->nick, "No.");
		return;
	}
	
	//
	// ADD DELAY
	//
	
	sqlquery = sqlite3_mprintf(
		"SELECT id FROM logs WHERE nick = '%q' AND chan = '%q' LIMIT 1",
		args, message->chan
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		if(sqlite3_step(stmt) != SQLITE_ROW) {
			fprintf(stderr, "[-] plusone: unknown user\n");
			
			sqlite3_free(sqlquery);
			sqlite3_finalize(stmt);
			return;
		}
	
	} else fprintf(stderr, "[-] plusone: cannot select nick\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	//
	// insert initial count, if already exists, ignoring
	//
	sqlquery = sqlite3_mprintf(
		"INSERT OR IGNORE INTO plusone (nick, chan, value, activity) "
		"VALUES ('%q', '%q', 0, %d);",
		args, message->chan, time(NULL)
	);
	
	if(!db_sqlite_simple_query(sqlite_db, sqlquery))
		fprintf(stderr, "[-] plusone: cannot insert nick\n");
	
	sqlite3_free(sqlquery);
	
	//
	// security (flood) check
	//
	sqlquery = sqlite3_mprintf(
		"SELECT activity FROM plusone WHERE nick = '%q' AND chan = '%q'",
		args, message->chan
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		if(sqlite3_step(stmt) == SQLITE_ROW)
			count = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] plusone: cannot select value\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	if(count > time(NULL) - 60)
		return;
	
	//
	// updating current count
	//
	sqlquery = sqlite3_mprintf(
		"UPDATE plusone SET value = value %s, activity = %d "
		"WHERE nick = '%q' AND chan = '%q'",
		howmany, time(NULL), args, message->chan
	);
	
	if(!db_sqlite_simple_query(sqlite_db, sqlquery))
		fprintf(stderr, "[-] plusone: cannot update nick\n");
	
	sqlite3_free(sqlquery);
	
	//
	// getting current count
	//
	sqlquery = sqlite3_mprintf(
		"SELECT value FROM plusone WHERE nick = '%q' AND chan = '%q'",
		args, message->chan
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		if(sqlite3_step(stmt) == SQLITE_ROW)
			count = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] plusone: cannot select value\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
			
	// zsnprintf(msg, "The honor of %s is now evaluated at [%d]", args, count);
	// irc_privmsg(message->chan, msg);
	
	if(count == 1337) {
		zsnprintf(msg, "Well done, %s just won a t-shirt !!", args);
		irc_privmsg(message->chan, msg);
	}
}

void action_useless_plusone(ircmessage_t *message, char *args) {
	action_useless_one(message, args, "+ 1");
}

void action_useless_minusone(ircmessage_t *message, char *args) {
	action_useless_one(message, args, "- 1");
}

void action_useless_popcorn(ircmessage_t *message, char *args) {
	(void) args;
	irc_privmsg(message->chan, "https://i.imgur.com/agJIP.gif");
}

void action_useless_fake(ircmessage_t *message, char *args) {
	(void) args;
	irc_privmsg(message->chan, "Nathan !");
}
