/* z03 - small bot with some network features - what.cd api
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
#include <string.h>
#include <unistd.h>
#include "../common/bot.h"
#include "../core/init.h"
#include "list.h"
#include "database.h"
#include "core.h"
#include "downloader.h"
#include "ircmisc.h"
#include "settings.h"
#include "whatcd.h"

//
// FIXME: rewrite me.
//

void periodic_whatcd(list_t *tracking) {
	whatcd_t *whatcd;
	list_node_t *node2;
	whatcd_request_t *request;
	whatcd_release_t *release;
	char buffer[2048], *output;
	list_t *users;
	int *trackval; /* store value for check how many errors/users waz */
	
	if(!(users = settings_by_key("whatsession", PRIVATE)))
		return;
	
	list_foreach(users, node) {
		whatcd = whatcd_new((char *) node->data);
		
		/* load error count */
		if(!(trackval = (int *) list_search(tracking, node->name))) {
			trackval = (int *) calloc(1, sizeof(int));
			list_append(tracking, node->name, trackval);
		}
		
		if(!(output = settings_get(node->name, "whatchan", PUBLIC)))
			output = node->name;
		
		/* notification failed */
		if(!(request = whatcd_notification(whatcd))) {
			printf("[-] periodic/whatcd: trackval error count for %s: %d\n", node->name, ++*trackval);
			
			if(*trackval == 4)
				irc_notice(node->name, "cannot grab your notifications, please check your whatcd session");
			
			if(*trackval == 50)
				irc_notice(node->name, "whatcd session seems really down, please check your whatcd session or unset it with: .unset whatsession");
				
			goto next_node;
		}
		
		if(request->error) {
			printf("[-] periodic/whatcd: error: %s\n", request->error);
			goto next_node;
		}
		
		node2 = request->response->nodes;
		
		while(node2) {
			release = (whatcd_release_t *) node2->data;
			node2 = node2->next;
			
			if(!release->unread)
				continue;
				
			zsnprintf(buffer,
				  "%s: [%s/%s] %s - %s (%.2f Mo): " WHATCD_TORRENT "%.0f&torrentid=%.0f",
				  node->name,
				  release->format, release->media, release->artist, release->groupname, 
				  release->size / 1024 / 1024, release->groupid,
				  release->torrentid
			);
			
			irc_privmsg(output, buffer);
			*trackval = 0;
		}
		
		next_node:
			whatcd_request_free(request);		
			whatcd_free(whatcd);
	}
	
	list_free(users);
}

void periodic_delay() {
	sqlite3_stmt *stmt;
	char output[1024];
	char *sqlquery, *nick, *chan, *message;
	int id = 0;
	
	/* Checking notes */
	sqlquery = sqlite3_mprintf(
		"SELECT id, nick, chan, message FROM delay "
		"WHERE timestamp < %ld AND finished = 0",
		time(NULL)
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			id      = sqlite3_column_int(stmt, 0);
			nick    = (char *) sqlite3_column_text(stmt, 1);
			chan    = (char *) sqlite3_column_text(stmt, 2);
			message = (char *) sqlite3_column_text(stmt, 3);
			
			zsnprintf(output, "[alarm] %s: %s", nick, message);
			irc_privmsg(chan, output);
			
			
			sqlquery = sqlite3_mprintf("UPDATE delay SET finished = 1 WHERE id = %d", id);
			
			if(!db_sqlite_simple_query(sqlite_db, sqlquery))
				printf("[-] lib/notes: cannot mark as read\n");
			
			sqlite3_free(sqlquery);
		}
		
		sqlite3_finalize(stmt);
	
	} else fprintf(stderr, "[-] lib/delay: sql error\n");
}

void *periodic_each_minutes(void *dummy) {
	// list_t *tracking;
	
	// FIXME: add destructor
	/* if(!(tracking = list_init(NULL))) {
		fprintf(stderr, "[-] periodic: cannot init list\n");
		return dummy;
	} */
	
	while(1) {
		sleep(60);
		
		printf("[+] periodic/minute: starting cycle\n");
		global_core->extraclient++;
		
		// periodic_whatcd(tracking);
		periodic_delay();
		
		global_core->extraclient--;
		printf("[+] periodic/minute: end of cycle\n");
	}
	
	return dummy;
}
