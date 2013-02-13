/* z03 - small bot with some network features - irc channel bot actions
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
#include <string.h>
#include <time.h>
#include "bot.h"
#include "core_init.h"
#include "lib_database.h"
#include "lib_list.h"
#include "lib_core.h"

int log_privmsg(char *chan, char *nick, char *message) {
	char *sqlquery;
	time_t ts = 0;
	
	ts = time(NULL);
	
	sqlquery = sqlite3_mprintf("INSERT INTO `logs` (id, chan, timestamp, nick, message) VALUES (NULL, '%q', %u, '%q', '%q')", chan, ts, nick, message);
	
	if(!db_sqlite_simple_query(sqlite_db, sqlquery)) {
		printf("[-] URL Parser: cannot update db\n");
		ts = 0;
	}
		
	sqlite3_free(sqlquery);
	
	return (int) ts;
}
