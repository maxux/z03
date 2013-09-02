/* z03 - webservices like weather, radio station, google, wikipedia, ...
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
#include "../common/bot.h"
#include "../core/init.h"
#include "database.h"
#include "core.h"
#include "actions.h"
#include "chart.h"
#include "ircmisc.h"
#include "actions_charts.h"

//
// registering commands
//

static request_t __action_chart = {
	.match    = ".chart",
	.callback = action_chart,
	.man      = "print a chart about url usage",
	.hidden   = 0,
	.syntaxe  = "",
};

static request_t __action_chartlog = {
	.match    = ".chartlog",
	.callback = action_chartlog,
	.man      = "chart of lines log",
	.hidden   = 0,
	.syntaxe  = "",
};

__registrar actions_charts() {
	request_register(&__action_chart);
	request_register(&__action_chartlog);
}

//
// commands implementation
//

void action_chart(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	int nbrows = 0, row, i;
	char *sqlquery;
	char *days = NULL;
	int *values = NULL, lines = 6;
	char **chart, first_date[32], last_date[32];
	char temp[256];
	(void) args;
	
	/* Flood Protection */
	if(time(NULL) - (60 * 10) < message->channel->last_chart_request) {
		irc_privmsg(message->chan, "Avoiding flood, bitch !");
		return;
		
	} else message->channel->last_chart_request = time(NULL);
	
	/* Working */          
	sqlquery = sqlite3_mprintf(
		"SELECT count(id), date(time, 'unixepoch') d, strftime('%%w', time, 'unixepoch') w "
		"FROM url WHERE chan = '%q' AND time > 0 "
		"GROUP BY d ORDER BY d DESC LIMIT 31",
		message->chan
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		/* sqlite3_column_int auto-finalize */
		nbrows = db_sqlite_num_rows(stmt);
		values = (int *) malloc(sizeof(int) * nbrows);
		days   = (char *) malloc(sizeof(char) * nbrows);
	
		printf("[ ] Action: Chart: %u rows fetched.\n", nbrows);
		
		if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
			i = nbrows - 1;
			
			while((row = sqlite3_step(stmt)) == SQLITE_ROW) {
				values[i] = sqlite3_column_int(stmt, 0);	/* count value */
				days[i]   = (char) sqlite3_column_int(stmt, 2);	/* day of week */
				
				printf("[ ] Action: Chart: Day %s (url %d) is %d\n", 
				       sqlite3_column_text(stmt, 1), values[i], days[i]);
				
				if(i == 0)
					strcpy(first_date, (char *) sqlite3_column_text(stmt, 1));
					
				if(i == nbrows - 1)
					strcpy(last_date, (char *) sqlite3_column_text(stmt, 1));
				
				i--;
			}
		}
			
	} else fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	chart = ascii_chart(values, nbrows, lines, days);
	
	/* Chart Title */
	zsnprintf(temp, "Chart: urls per day from %s to %s", first_date, last_date);
	irc_privmsg(message->chan, temp);
	
	/* Chart data */
	for(i = lines - 1; i >= 0; i--)
		irc_privmsg(message->chan, *(chart + i));
	
	free(values);
	free(days);
}

void action_chartlog(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	int nbrows = 0, row, i;
	char *sqlquery;
	char *days = NULL;
	int *values = NULL, lines = 6;
	char **chart, first_date[32], last_date[32];
	char temp[256];
	time_t now;
	(void) args;
	
	/* Flood Protection */
	if(time(NULL) - (60 * 10) < message->channel->last_chart_request) {
		irc_privmsg(message->chan, "Avoiding flood, bitch !");
		return;
		
	} else message->channel->last_chart_request = time(NULL);
	
	/* Working */
	now = today() - (60 * 24 * 60 * 60); // days paste
	sqlquery = sqlite3_mprintf(
		"SELECT count(id), date(timestamp, 'unixepoch') d, strftime('%%w', timestamp, 'unixepoch') w "
		"FROM logs WHERE chan = '%q' AND timestamp > %u "
		"GROUP BY d ORDER BY d DESC LIMIT 31",
		message->chan, now
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		/* sqlite3_column_int auto-finalize */
		nbrows = db_sqlite_num_rows(stmt);
		values = (int *) malloc(sizeof(int) * nbrows);
		days   = (char *) malloc(sizeof(char) * nbrows);
	
		printf("[ ] Action: Chart: %u rows fetched.\n", nbrows);
		
		if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
			i = nbrows - 1;
			
			while((row = sqlite3_step(stmt)) == SQLITE_ROW) {
				values[i] = sqlite3_column_int(stmt, 0);	/* count value */
				days[i]   = (char) sqlite3_column_int(stmt, 2);	/* day of week */
				
				printf("[ ] Action: Chart: Day %s (url %d) is %d\n", 
				       sqlite3_column_text(stmt, 1), values[i], days[i]);
				
				if(i == 0)
					strcpy(first_date, (char *) sqlite3_column_text(stmt, 1));
					
				if(i == nbrows - 1)
					strcpy(last_date, (char *) sqlite3_column_text(stmt, 1));
				
				i--;
			}
		}
			
	} else fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	chart = ascii_chart(values, nbrows, lines, days);
	
	/* Chart Title */
	zsnprintf(temp, "Chart: lines per day from %s to %s", first_date, last_date);
	irc_privmsg(message->chan, temp);
	
	/* Chart data */
	for(i = lines - 1; i >= 0; i--)
		irc_privmsg(message->chan, *(chart + i));
	
	free(values);
	free(days);
}
