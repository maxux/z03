/* z03 - small bot with some network features - irc channel bot actions
 * Author: Daniel Maxime (maxux.unix@gmail.com)
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
#include "core.h"
#include "actions.h"
#include "bot.h"
#include "database.h"
#include "chart.h"

time_t last_chart_request = 0;
char *weather_url = "curl -s 'http://www.meteobelgium.be/service/city/city.php?zone=0&stationid=%d&language=en' | grep 'class=\"degre' | tr '<' '>' | awk -F '>' '{ print $3 }'";

weather_station_t weather_id[] = {
	{.id = 29,  .ref = "liege",   .name = "Thier-à-Liège"},
	{.id = 96,  .ref = "slins",   .name = "Slins"},
	{.id = 125, .ref = "oupeye",  .name = "Oupeye"},
	{.id = 14,  .ref = "lille",   .name = "Lille (France)"},
	{.id = 77,  .ref = "knokke",  .name = "Knokke"},
	{.id = 80,  .ref = "seraing", .name = "Boncelles (Seraing)"},
	{.id = 106, .ref = "namur",   .name = "Floriffoux (Namur)"},
	{.id = 48,  .ref = "spa",     .name = "Spa"},
};

char * weather_read_data(char *buffer, size_t size, int id) {
	FILE *fp;
	char cmdline[256];
	
	/* Building command line */
	sprintf(cmdline, weather_url, weather_id[id].id);
	
	fp = popen(cmdline, "r");
	if(!fp) {
		perror("popen");
		return NULL;
	}
	
	if(fgets(buffer, size, fp) == NULL)
		strcpy(buffer, "(unknown)");
	
	fclose(fp);
	
	return buffer;
}

char * weather_station_list() {
	char *list;
	unsigned int i, len = 0;
	
	for(i = 0; i < sizeof(weather_id) / sizeof(weather_station_t); i++)
		len += strlen(weather_id[i].ref) + 1;
	
	list = (char*) malloc(sizeof(char) * len + 1);
	if(!list)
		return NULL;
	
	*list = '\0';
	
	for(i = 0; i < sizeof(weather_id) / sizeof(weather_station_t); i++) {
		strcat(list, weather_id[i].ref);
		strcat(list, " ");
	}
	
	/* Remove last space */
	*(list + len - 1) = '\0';
	
	return list;
}

void action_weather(char *chan, char *args) {
	char cmdline[256], buffer[512], *list;
	unsigned int i;
	int id = 0;	// Default: Oupeye
	
	/* Checking arguments */
	if(*args) {
		/* Building List */
		if(!strcmp(args, "list")) {
			list = weather_station_list();
			if(!list)
				return;
				
			sprintf(cmdline, "PRIVMSG %s :Station list: %s (Default: %s)", chan, list, weather_id[id].ref);
			raw_socket(sockfd, cmdline);
			
			free(list);
			
			return;
		}
		
		/* Searching station */
		for(i = 0; i < sizeof(weather_id) / sizeof(weather_station_t); i++) {
			if(!strcmp(args, weather_id[i].ref)) {
				id = i;
				break;
			}
		}
	}
	
	if(weather_read_data(buffer, sizeof(buffer), id)) {
		sprintf(cmdline, "PRIVMSG %s :%s: %s", chan, weather_id[id].name, buffer);
		raw_socket(sockfd, cmdline);
	}
}

void action_ping(char *chan, char *args) {
	time_t t;
	struct tm * timeinfo;
	char buffer[128];
	
	/* Fix Warnings */
	args = NULL;
	
	time(&t);
	timeinfo = localtime(&t);
	
	sprintf(buffer, "PRIVMSG %s :Pong. Ping request received at %02d:%02d:%02d.", chan, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	raw_socket(sockfd, buffer);
}

void action_time(char *chan, char *args) {
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[128], out[256];
	
	/* Fix Warnings */
	args = NULL;

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
	
	/* Fix Warnings */
	args = NULL;
	
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
	
	/* Fix Warnings */
	args = NULL;
	
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
	snprintf(temp, sizeof(temp), "PRIVMSG %s :Chart: urls per day since %s to %s", chan, first_date, last_date);
	raw_socket(sockfd, temp);
	
	/* Chart data */
	for(i = lines - 1; i >= 0; i--) {
		snprintf(temp, sizeof(temp), "PRIVMSG %s :%s", chan, *(chart + i));
		raw_socket(sockfd, temp);
	}
}
