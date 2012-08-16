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
#include "core_database.h"
#include "lib_core.h"
#include "lib_urlmanager.h"
#include "lib_actions.h"
#include "lib_chart.h"
#include "lib_ircmisc.h"
#include "lib_weather.h"
#include "lib_somafm.h"

time_t last_chart_request = 0;
time_t last_backurl_request = 0;

void action_weather(char *chan, char *args) {
	char cmdline[256], *list;
	unsigned int i;
	int id = 0;	// default
	
	/* Checking arguments */
	if(*args) {
		/* Building List */
		if(!strcmp(args, "list")) {
			list = weather_station_list();
			if(!list)
				return;
				
			sprintf(cmdline, "PRIVMSG %s :Stations list: %s (Default: %s)", chan, list, weather_stations[id].ref);
			raw_socket(sockfd, cmdline);
			
			free(list);
			
			return;
		}
		
		/* Searching station */
		for(i = 0; i < weather_stations_count; i++) {
			if(!strcmp(args, weather_stations[i].ref)) {
				id = i;
				break;
			}
		}
	}
	
	weather_handle(chan, (weather_stations + id));
	
	/* if(weather_read_data(buffer, sizeof(buffer), id)) {
		sprintf(cmdline, "PRIVMSG %s :%s: %s", chan, weather_id[id].name, buffer);
		raw_socket(sockfd, cmdline);
	} */
}

void action_ping(char *chan, char *args) {
	time_t t;
	struct tm * timeinfo;
	char buffer[128];
	
	time(&t);
	timeinfo = localtime(&t);
	
	sprintf(buffer, "PRIVMSG %s :Pong. Ping request received at %02d:%02d:%02d.", chan, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	raw_socket(sockfd, buffer);
}

void action_time(char *chan, char *args) {
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[128], out[256];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(buffer, sizeof(buffer), "%A %d %B %X %Y", timeinfo);
	sprintf(out, "PRIVMSG %s :%s", chan, buffer);
	
	raw_socket(sockfd, out);
}

void action_random(char *chan, char *args) {
	char answer[512];
	int random, max = 100;
	
	/* Randomize */
	srand(time(NULL));
	
	if(*args)
		max = (atoi(args) <= 0) ? 100 : atoi(args);
	
	random = rand() % max;
	
	sprintf(answer, "PRIVMSG %s :%d", chan, random);
	raw_socket(sockfd, answer);
}

void action_help(char *chan, char *args) {
	char list[1024];
	unsigned int i;
	
	sprintf(list, "PRIVMSG %s :Commands: ", chan);
	
	for(i = 0; i < __request_count; i++) {
		strcat(list, __request[i].match);
		strcat(list, " ");
	}
	
	raw_socket(sockfd, list);
}

void action_stats(char *chan, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery = "SELECT COUNT(id), COUNT(DISTINCT nick), SUM(hit) FROM url;";
	char msg[256];
	int count = 0, cnick = 0, chits = 0;
	int row;
	
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW) {
			count = sqlite3_column_int(stmt, 0);
			cnick = sqlite3_column_int(stmt, 1);
			chits = sqlite3_column_int(stmt, 2);
		}
	}
	
	sprintf(msg, "PRIVMSG %s :Got %d url on database for %d nicks and %d total hits", chan, count, cnick, chits);
	raw_socket(sockfd, msg);
	
	/* Clearing */
	sqlite3_finalize(stmt);
}

void action_chart(char *chan, char *args) {
	sqlite3_stmt *stmt;
	int nbrows = 0, row, i;
	char *sqlquery;
	
	char *days = NULL;
	int *values = NULL, lines = 6;
	char **chart, first_date[32], last_date[32];
	char temp[512];
	
	/* Flood Protection */
	if(time(NULL) - (60 * 10) < last_chart_request) {
		snprintf(temp, sizeof(temp), "PRIVMSG %s :Avoiding flood, bitch !", chan);
		raw_socket(sockfd, temp);
		return;
		
	} else last_chart_request = time(NULL);
	
	/* Working */
	args     = NULL;	            
	sqlquery = "SELECT count(id), date(time, 'unixepoch') d, strftime('%w', time, 'unixepoch') w FROM url WHERE time > 0 GROUP BY d ORDER BY d DESC LIMIT 31";
	
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	/* sqlite3_column_int auto-finalize */
	nbrows = db_sqlite_num_rows(stmt);
	values = (int*) malloc(sizeof(int) * nbrows);
	days   = (char*) malloc(sizeof(char) * nbrows);
	
	printf("[ ] Action: Chart: %u rows fetched.\n", nbrows);
	
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	i = nbrows - 1;
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW) {
			values[i] = sqlite3_column_int(stmt, 0);	/* count value */
			days[i]   = (char) sqlite3_column_int(stmt, 2);	/* day of week */
			
			printf("[ ] Action: Chart: Day %s (url %d) is %d\n", sqlite3_column_text(stmt, 1), values[i], days[i]);
			
			if(i == 0)
				strcpy(first_date, (char*) sqlite3_column_text(stmt, 1));
				
			else if(i == nbrows - 1)
				strcpy(last_date, (char*) sqlite3_column_text(stmt, 1));
			
			i--;
		}
	}
	
	sqlite3_finalize(stmt);
	
	chart = ascii_chart(values, nbrows, lines, days);
	
	/* Chart Title */
	snprintf(temp, sizeof(temp), "PRIVMSG %s :Chart: urls per day from %s to %s", chan, first_date, last_date);
	raw_socket(sockfd, temp);
	
	/* Chart data */
	for(i = lines - 1; i >= 0; i--) {
		snprintf(temp, sizeof(temp), "PRIVMSG %s :%s", chan, *(chart + i));
		raw_socket(sockfd, temp);
	}
	
	free(values);
	free(days);
}

void action_uptime(char *chan, char *args) {
	time_t now;
	char message[256];
	char *uptime, *rehash;
	
	now  = time(NULL);
		
	uptime = time_elapsed(now - global_core.startup_time);
	rehash = time_elapsed(now - global_core.rehash_time);
	
	sprintf(message, "PRIVMSG %s :Bot uptime: %s (rehashed %d times, rehash uptime: %s)", chan, uptime, global_core.rehash_count, rehash);
	raw_socket(sockfd, message);
	
	free(uptime);
	free(rehash);
}

void action_backlog_url(char *chan, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery = "SELECT id, url, nick FROM url ORDER BY id DESC LIMIT 5";
	char *message;
	const unsigned char *row_url, *row_nick;
	char *url_nick;
	int row, url_id;
	size_t len;
	char temp[256];
	
	/* Flood Protection */
	if(time(NULL) - (60 * 10) < last_backurl_request) {
		snprintf(temp, sizeof(temp), "PRIVMSG %s :Avoiding flood, bitch !", chan);
		raw_socket(sockfd, temp);
		return;
		
	} else last_backurl_request = time(NULL);
	
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW) {
			url_id   = sqlite3_column_int(stmt, 0);
			row_url  = sqlite3_column_text(stmt, 1);
			row_nick = sqlite3_column_text(stmt, 2);
			url_nick = (char*) malloc(sizeof(char) * strlen((char*) row_nick) + 1);
			strcpy(url_nick, (char*) row_nick);
			
			len = strlen((char*) row_url) * strlen(url_nick) + 64;
			message = (char*) malloc(sizeof(char) * len);
			
			snprintf(message, len, "PRIVMSG %s :<%s> URL %d: %s", chan, anti_hl(url_nick), url_id, row_url);
			raw_socket(sockfd, message);
			
			free(url_nick);
		}
	}
	
	/* Clearing */
	sqlite3_finalize(stmt);
}

void action_seen(char *chan, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	const unsigned char *message;
	char *output;
	time_t timestamp;
	int row, len;
	struct tm * timeinfo;
	char buffer[128];
	
	if(!*args)
		return;
	
	sqlquery = sqlite3_mprintf("SELECT timestamp, message FROM logs WHERE chan = '%q' AND nick = '%q' ORDER BY timestamp DESC LIMIT 1", chan, args);
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW) {
			timestamp = sqlite3_column_int(stmt, 0);
			message   = sqlite3_column_text(stmt, 1);
			
			len = strlen((char*) message) + 64;
			output = (char*) malloc(sizeof(char) * len);
			
			timeinfo = localtime(&timestamp);
			strftime(buffer, sizeof(buffer), "%A %d %B %X %Y", timeinfo);
			
			snprintf(output, len, "PRIVMSG %s :[%s] <%s> %s", chan, buffer, anti_hl(args), message);
			raw_socket(sockfd, output);
			
			free(output);
		}
	}
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

void action_somafm(char *chan, char *args) {
	char cmdline[256], *list;
	unsigned int i;
	int id = 0;	// default
	
	/* Checking arguments */
	if(*args) {
		/* Building List */
		if(!strcmp(args, "list")) {
			list = somafm_station_list();
			if(!list)
				return;
				
			sprintf(cmdline, "PRIVMSG %s :Stations list: %s (Default: %s)", chan, list, somafm_stations[id].ref);
			raw_socket(sockfd, cmdline);
			
			free(list);
			
			return;
		}
		
		/* Searching station */
		for(i = 0; i < somafm_stations_count; i++) {
			if(!strcmp(args, somafm_stations[i].ref)) {
				id = i;
				break;
			}
		}
	}
	
	somafm_handle(chan, (somafm_stations + id));
}
