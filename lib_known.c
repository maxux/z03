/* z03 - small bot with some network features - whois support
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
#include "core_init.h"
#include "lib_core.h"
#include "lib_database.h"
#include "lib_list.h"
#include "lib_known.h"
#include "lib_ircmisc.h"

char *list_implode(list_t *list, size_t limit) {
	size_t alloc = 0, now = 0;
	list_node_t *node;
	char *implode;
	char buffer[128];
	
	/* compute the full list length */
	node = list->nodes;
		
	while(node) {
		alloc += strlen(node->name) + 8;
		node = node->next;
	}
	
	/* allocating string and re-iterate list */
	implode = (char *) calloc(sizeof(char), alloc);
	node = list->nodes;
	
	while(node && ++now) {
		// appending anti-hled nick
		zsnprintf(buffer, "%s", node->name);
		anti_hl(buffer);
		strcat(implode, buffer);
		
		if(now >= limit) {
			zsnprintf(buffer, " and %u others hidden",
			                  list->length - now);

			implode = (char *) realloc(implode, alloc + 64);
			strcat(implode, buffer);
			return implode;
			
		} else if(node->next)
			strcat(implode, ", ");
			
		node = node->next;
	}
	
	return implode;
}

char *irc_knownuser(char *nick, char *host) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char *thisnick = NULL, *nickcast;
	list_t *nicklist;
	int row;
	
	/* Query database */
	sqlquery = sqlite3_mprintf(
		"SELECT nick FROM hosts WHERE host = '%q'",
		host
	);
	
	/* Building list */
	nicklist = list_init(NULL);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) == SQLITE_ROW) {
			nickcast = (char *) sqlite3_column_text(stmt, 0);
			
			// Skipping same name
			if(!strcmp(nickcast, nick))
				continue;
			
			thisnick = (char *) realloc(thisnick, strlen(nickcast) + 8);
			strcpy(thisnick, nickcast);

			printf("[ ] Action/known: appending: <%s>\n", thisnick);
			list_append(nicklist, thisnick, thisnick);
		}
	}
	
	free(thisnick);
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	// list is empty
	if(!nicklist->length) {
		list_free(nicklist);
		return NULL;
	}
	
	thisnick = list_implode(nicklist, 4);
	list_free(nicklist);
	
	return thisnick;
}

void __action_known_add(char *nick, char *username, char *host, char *chan) {
	char *sqlquery, nick2[64];
	char output[1024], *list;
	
	/* Insert to db */
	sqlquery = sqlite3_mprintf("INSERT INTO hosts (nick, username, host, chan) VALUES ('%q', '%q', '%q', '%q')", nick, username, host, chan);

	if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
		if((list = irc_knownuser(nick, host))) {
			zsnprintf(nick2, "%s", nick);
			anti_hl(nick2);
			
			zsnprintf(output, "%s (host: %s) is also known as: %s", nick2, host, list);
			irc_privmsg(chan, output);
			
			free(list);
		}
		
	} else printf("[-] lib/Join: cannot update db, probably because nick already exists.\n");
		
	/* Clearing */
	sqlite3_free(sqlquery);
}
