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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
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
#include "lib_google.h"

time_t last_chart_request = 0;
time_t last_backurl_request = 0;

void action_weather(char *chan, char *args) {
	char cmdline[256], *list;
	unsigned int i;
	int id = 2;	// default
	
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
	(void) args;
	
	time(&t);
	timeinfo = localtime(&t);
	
	sprintf(buffer, "PRIVMSG %s :Pong. Ping request received at %02d:%02d:%02d.", chan, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	raw_socket(sockfd, buffer);
}

void action_time(char *chan, char *args) {
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[128], out[256];
	(void) args;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(buffer, sizeof(buffer), "%A %d %B %X %Y", timeinfo);
	sprintf(out, "PRIVMSG %s :%s", chan, buffer);
	
	raw_socket(sockfd, out);
}

void action_random(char *chan, char *args) {
	char answer[512], *x;
	int random, min = 0, max = 100;
	
	/* Trim last spaces */
	short_trim(args);
	
	if(*args) {
		/* Min/Max or just Max */
		if((x = strchr(args, ' '))) {
			min = (atoi(args) <= 0) ? 0 : atoi(args);
			max = (atoi(args) <= 0) ? 100 : atoi(x);
			
		} else max = (atoi(args) <= 0) ? 100 : atoi(args);
	}
	
	printf("[ ] Action/Random: min: %d, max: %d\n", min, max);
	
	random = (rand() % (max - min + 1) + min);
	
	sprintf(answer, "PRIVMSG %s :Random (%d, %d): %d", chan, min, max, random);
	raw_socket(sockfd, answer);
}

void action_help(char *chan, char *args) {
	char list[1024];
	unsigned int i;
	(void) args;
	
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
	(void) args;
	
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
	(void) args;
	
	/* Flood Protection */
	if(time(NULL) - (60 * 10) < last_chart_request) {
		snprintf(temp, sizeof(temp), "PRIVMSG %s :Avoiding flood, bitch !", chan);
		raw_socket(sockfd, temp);
		return;
		
	} else last_chart_request = time(NULL);
	
	/* Working */          
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
	(void) args;
	
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
	char *sqlquery = "SELECT url, nick, title FROM url ORDER BY id DESC LIMIT 5";
	char *message;
	const unsigned char *row_url, *row_nick, *row_title;
	char *url_nick;
	int row;
	size_t len;
	char temp[256];
	(void) args;
	
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
			row_url   = sqlite3_column_text(stmt, 0);
			row_nick  = sqlite3_column_text(stmt, 1);
			row_title = sqlite3_column_text(stmt, 2);
			
			url_nick = (char*) malloc(sizeof(char) * strlen((char*) row_nick) + 4);
			strcpy(url_nick, (char*) row_nick);
			
			if(!row_title)
				row_title = (unsigned char*) "(No title)";
			
			len = strlen((char*) row_url) * strlen(url_nick) * strlen((char*) row_title) + 64;
			message = (char*) malloc(sizeof(char) * len + 1);
				
			snprintf(message, len, "PRIVMSG %s :<%s> [%s] %s", chan, anti_hl(url_nick), row_url, row_title);
			
			raw_socket(sockfd, message);
			
			free(url_nick);
			free(message);
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
	char buffer[64], found = 0;
	
	if(!*args)
		return;
	
	/* Trim last spaces */
	short_trim(args);
	
	sqlquery = sqlite3_mprintf("SELECT timestamp, message FROM logs WHERE chan = '%q' AND nick = '%q' ORDER BY timestamp DESC LIMIT 1", chan, args);
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] Action/Seen: SQL Error\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW) {
			found = 1;
			
			timestamp = sqlite3_column_int(stmt, 0);
			message   = sqlite3_column_text(stmt, 1);
			
			printf("[ ] Action/Seen: message: <%s>\n", message);
			
			len = strlen((char*) message) + strlen(args) + sizeof(buffer) + 16;
			output = (char*) malloc(sizeof(char) * len);
			
			timeinfo = localtime(&timestamp);
			strftime(buffer, sizeof(buffer), "%A %d %B %X %Y", timeinfo);
			
			snprintf(output, len, "PRIVMSG %s :[%s] <%s> %s", chan, buffer, anti_hl(args), message);
			raw_socket(sockfd, output);
			
			free(output);
		}
	}
	
	if(!found) {
		output = (char*) malloc(sizeof(char) * 128);
		snprintf(output, 128, "PRIVMSG %s :No match found for <%s>", chan, args);
		raw_socket(sockfd, output);
		free(output);
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
			if(!strncmp(args, somafm_stations[i].ref, strlen(somafm_stations[i].ref))) {
				id = i;
				break;
			}
		}
	}
	
	somafm_handle(chan, (somafm_stations + id));
}

void action_dns(char *chan, char *args) {
	struct hostent *he;
	char reply[256];
	char *ipbuf;
	
	if(!*args)
		return;
		
	if((he = gethostbyname(args)) == NULL) {
		sprintf(reply, "PRIVMSG %s :Cannot resolve host", chan);
		raw_socket(sockfd, reply);
		return;
	}
	
	
	ipbuf = inet_ntoa(*((struct in_addr *)he->h_addr_list[0]));
	
	snprintf(reply, sizeof(reply), "PRIVMSG %s :%s -> %s", chan, args, ipbuf);
	raw_socket(sockfd, reply);
}

void action_count(char *chan, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char output[256];
	int row, count1, count2;
	char found = 0;
	
	if(!*args)
		return;
	
	/* Trim last spaces */
	short_trim(args);
	
	sqlquery = sqlite3_mprintf("SELECT COUNT(*) as match, (SELECT COUNT(*) FROM logs WHERE chan = '%q') as total FROM logs WHERE nick = '%q' AND chan = '%q';", chan, args, chan);
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] Action/Count: SQL Error\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW) {
			count1 = sqlite3_column_int(stmt, 0);
			count2 = sqlite3_column_int(stmt, 1);
			
			if(count1)
				found = 1;
			
			printf("[ ] Action/Count: %d / %d\n", count1, count2);
			
			if(count1 && count2) {
				snprintf(output, sizeof(output), "PRIVMSG %s :Got %d lines for <%s>, which is %.2f%% of the %d total lines", chan, count1, anti_hl(args), ((float) count1 / count2) * 100, count2);
				raw_socket(sockfd, output);
			}
		}
	}
	
	if(!found) {
		snprintf(output, sizeof(output), "PRIVMSG %s :No match found for <%s>", chan, args);
		raw_socket(sockfd, output);
	}
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

void action_known(char *chan, char *args) {
	char *list, *listhl;
	char output[256];
	whois_t *whois;
		
	if(!*args)
		return;
	
	short_trim(args);
	
	if((whois = irc_whois(args))) {
		if((list = irc_knownuser(args, whois->host))) {
			listhl = anti_hl_each_words(list, strlen(list), UTF_8);
			snprintf(output, sizeof(output), "PRIVMSG %s :%s (host: %s) is also known as: %s", chan, anti_hl(args), whois->host, listhl);
			
			free(listhl);
			free(list);
			
			whois_free(whois);
			
		} else snprintf(output, sizeof(output), "PRIVMSG %s :<%s> has no previous known host", chan, anti_hl(args));
		
	} else snprintf(output, sizeof(output), "PRIVMSG %s :<%s> is not connected", chan, args);
	
	raw_socket(sockfd, output);
}

void action_url(char *chan, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char *output, *title, *url;
	int row, len;
	
	if(!*args)
		return;
	
	/* Trim last spaces */
	short_trim(args);
	
	sqlquery = sqlite3_mprintf("SELECT url, title FROM url WHERE url LIKE '%%%q%%' OR title LIKE '%%%q%%' ORDER BY time DESC LIMIT 5;", args, args);
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] Action/URL: SQL Error\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW) {
			url   = (char*) sqlite3_column_text(stmt, 0);
			title = (char*) sqlite3_column_text(stmt, 1);
			
			if(!title)
				title = "Unknown title";
				
			len = strlen(url) + strlen(title) + 64;
			output = (char*) malloc(sizeof(char) * len);
				
			snprintf(output, len, "PRIVMSG %s :%s (%s)", chan, url, title);
			raw_socket(sockfd, output);
			
			free(output);
		}
	}
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

void action_goo(char *chan, char *args) {
	if(!*args)
		return;
	
	/* Trim last spaces */
	short_trim(args);
	
	google_search(chan, args, 1);
}

void action_google(char *chan, char *args) {
	if(!*args)
		return;
	
	/* Trim last spaces */
	short_trim(args);
	
	google_search(chan, args, 3);
}

void action_man(char *chan, char *args) {
	char reply[1024];
	unsigned int i;
	
	if(!*args)
		return;
	
	for(i = 0; i < __request_count; i++) {
		if(match_prefix(args, __request[i].match + 1)) {
			sprintf(reply, "PRIVMSG %s :%s: %s", chan, args, __request[i].man);
			raw_socket(sockfd, reply);
			return;
		}
	}
}
