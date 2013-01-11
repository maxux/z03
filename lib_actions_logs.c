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
#include "bot.h"
#include "core_init.h"
#include "lib_database.h"
#include "lib_core.h"
#include "lib_actions.h"
#include "lib_chart.h"
#include "lib_ircmisc.h"
#include "lib_actions_logs.h"

void action_stats(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char msg[256];
	int count = 0, cnick = 0, chits = 0;
	int row;
	(void) args;
	
	sqlquery = sqlite3_mprintf(
		"SELECT COUNT(id), COUNT(DISTINCT nick), SUM(hit) "
		"FROM url WHERE chan = '%q'",
		message->chan
	);
	
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
			count = sqlite3_column_int(stmt, 0);
			cnick = sqlite3_column_int(stmt, 1);
			chits = sqlite3_column_int(stmt, 2);
		}
			
		zsnprintf(msg, "Got %d url on database for %d nicks and %d total hits",
		               count, cnick, chits);
		irc_privmsg(message->chan, msg);
	
	} else fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

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
	
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		/* sqlite3_column_int auto-finalize */
		nbrows = db_sqlite_num_rows(stmt);
		values = (int *) malloc(sizeof(int) * nbrows);
		days   = (char *) malloc(sizeof(char) * nbrows);
	
		printf("[ ] Action: Chart: %u rows fetched.\n", nbrows);
		
		if((stmt = db_select_query(sqlite_db, sqlquery))) {
			i = nbrows - 1;
			
			while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
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

void action_count(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char output[256];
	int row, count1, count2;
	char found = 0;
	
	if(!action_parse_args(message, args))
		return;
	
	sqlquery = sqlite3_mprintf(
		"SELECT COUNT(*) as match, ("
		"    SELECT COUNT(*) FROM logs "
		"    WHERE chan = '%q') as total "
		"FROM logs WHERE nick = '%q' AND chan = '%q'",
		message->chan, args, message->chan
	);
	                           
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] Action/Count: SQL Error\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
		count1 = sqlite3_column_int(stmt, 0);
		count2 = sqlite3_column_int(stmt, 1);
		
		if(count1)
			found = 1;
		
		printf("[ ] Action/Count: %d / %d\n", count1, count2);
		
		if(count1 && count2) {
			zsnprintf(output, "Got %d lines for <%s>, which is %.2f%% of the %d total lines", 
			                  count1, anti_hl(args), ((float) count1 / count2) * 100, count2);
			irc_privmsg(message->chan, output);
		}
	}
	
	if(!found) {
		zsnprintf(output, "No match found for <%s>", args);
		irc_privmsg(message->chan, output);
	}
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

void action_known(ircmessage_t *message, char *args) {
	char *list, *listhl;
	char output[256];
	whois_t *whois;
		
	if(!action_parse_args(message, args))
		return;
	
	if((whois = irc_whois(args))) {
		if((list = irc_knownuser(args, whois->host))) {
			listhl = anti_hl_each_words(list, strlen(list), UTF_8);
			zsnprintf(output, "%s (host: %s) is also known as: %s",
			                  anti_hl(args), whois->host, listhl);
			
			free(listhl);
			free(list);
			
			whois_free(whois);
			
		} else zsnprintf(output, "<%s> has no previous known host", anti_hl(args));
		
	} else zsnprintf(output, "<%s> is not connected", args);
	
	irc_privmsg(message->chan, output);
}

void action_url(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char *output = NULL, *title, *url, *nick;
	time_t time;
	struct tm * timeinfo;
	char date[128], *url_nick;
	int row, len;
	
	if(!action_parse_args(message, args))
		return;
	
	sqlquery = sqlite3_mprintf(
		"SELECT url, title, nick, time "
		"FROM url WHERE (url LIKE '%%%q%%' OR title LIKE '%%%q%%') "
		"           AND chan = '%q' "
		"ORDER BY time DESC LIMIT 5",
		args, args, message->chan
	);
	
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
			url   = (char *) sqlite3_column_text(stmt, 0);
			title = (char *) sqlite3_column_text(stmt, 1);
			nick  = (char *) sqlite3_column_text(stmt, 2);
			time  = (time_t) sqlite3_column_int(stmt, 3);
			
			timeinfo = localtime(&time);
			strftime(date, sizeof(date), "%d/%m/%Y %X", timeinfo);
			
			if(!title)
				title = "Unknown title";				
			
			url_nick = (char *) malloc(sizeof(char) * strlen((char *) nick) + 4);
			strcpy(url_nick, nick);
				
			len = strlen(url) + strlen(title) + strlen(nick) + 128;
			output = (char *) malloc(sizeof(char) * len);
				
			snprintf(output, len, "PRIVMSG %s :[%s] <%s> %s | %s", message->chan, date, anti_hl(url_nick), url, title);
			raw_socket(sockfd, output);
			
			free(output);
			free(url_nick);
		}
	
	} else fprintf(stderr, "[-] Action/URL: SQL Error\n");
	
	if(!output)
		irc_privmsg(message->chan, "No match found");
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

void action_backlog(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery, *msg;
	const unsigned char *row_nick, *row_msg;
	time_t row_time;
	struct tm * timeinfo;
	char date[128], *log_nick, found = 0;
	char notfound[256];
	size_t len, row;
	
	if(*args) {
		short_trim(args);
		sqlquery = sqlite3_mprintf(
			"SELECT nick, timestamp, message FROM "
			"  (SELECT nick, timestamp, message FROM logs "
			"   WHERE chan = '%q' AND nick = '%q' "
			"   ORDER BY id DESC LIMIT 4) "
			"ORDER BY timestamp ASC",
			message->chan, args
		);
				   
	} else {
		/* Flood Protection */
		if(time(NULL) - (60 * 10) < message->channel->last_backlog_request) {
			irc_privmsg(message->chan, "Avoiding flood, bitch !");
			return;
			
		} else message->channel->last_backlog_request = time(NULL);
		
		sqlquery = sqlite3_mprintf(
			"SELECT nick, timestamp, message FROM "
			"  (SELECT nick, timestamp, message "
			"   FROM logs WHERE chan = '%q' "
			"   ORDER BY id DESC LIMIT 1, 7) "
			"ORDER BY timestamp ASC",
			message->chan
		);
	}
	
	if((stmt = db_select_query(sqlite_db, sqlquery))) {	
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
			found = 1;
			
			row_nick  = sqlite3_column_text(stmt, 0);
			row_time  = sqlite3_column_int(stmt, 1);
			row_msg   = sqlite3_column_text(stmt, 2);
			
			/* Date formating */
			timeinfo = localtime(&row_time);
			strftime(date, sizeof(date), "%d/%m/%Y %X", timeinfo);
			
			log_nick = (char *) malloc(sizeof(char) * strlen((char *) row_nick) + 4);
			strcpy(log_nick, (char *) row_nick);
			
			len = strlen(log_nick) * strlen((char *) row_msg) + 64;
			msg = (char *) malloc(sizeof(char) * len + 1);
				
			snprintf(msg, len, "[%s] <%s> %s", date, anti_hl(log_nick), row_msg);
			
			irc_privmsg(message->chan, msg);
			
			free(log_nick);
			free(msg);
		}
	
	} else fprintf(stderr, "[-] URL Parser: cannot select logs\n");
	
	if(!found) {
		zsnprintf(notfound, "No match found for <%s>", args);
		irc_privmsg(message->chan, notfound);
	}
	
	/* Clearing */
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
}
