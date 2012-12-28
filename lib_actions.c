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
#include <unistd.h>
#include "bot.h"
#include "core_init.h"
#include "core_database.h"
#include "lib_list.h"
#include "lib_core.h"
#include "lib_urlmanager.h"
#include "lib_actions.h"
#include "lib_chart.h"
#include "lib_ircmisc.h"
#include "lib_weather.h"
#include "lib_somafm.h"
#include "lib_google.h"
#include "lib_run.h"
#include "lib_wiki.h"

void action_weather(ircmessage_t *message, char *args) {
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
				
			sprintf(cmdline, "PRIVMSG %s :Stations list: %s (Default: %s)", message->chan, list, weather_stations[id].ref);
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
	
	weather_handle(message->chan, (weather_stations + id));
}

void action_ping(ircmessage_t *message, char *args) {
	time_t t;
	struct tm * timeinfo;
	char buffer[128];
	(void) args;
	
	time(&t);
	timeinfo = localtime(&t);
	
	sprintf(buffer, "PRIVMSG %s :Pong. Ping request received at %02d:%02d:%02d.", message->chan, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	raw_socket(sockfd, buffer);
}

void action_time(ircmessage_t *message, char *args) {
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[128], out[256];
	(void) args;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(buffer, sizeof(buffer), "%A %d %B %X %Y", timeinfo);
	sprintf(out, "PRIVMSG %s :%s", message->chan, buffer);
	
	raw_socket(sockfd, out);
}

void action_random(ircmessage_t *message, char *args) {
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
	
	sprintf(answer, "PRIVMSG %s :Random (%d, %d): %d", message->chan, min, max, random);
	raw_socket(sockfd, answer);
}

void action_help(ircmessage_t *message, char *args) {
	char list[1024];
	unsigned int i;
	(void) args;
	
	sprintf(list, "PRIVMSG %s :Commands: ", message->chan);
	
	for(i = 0; i < __request_count; i++) {
		strcat(list, __request[i].match);
		strcat(list, " ");
	}
	
	raw_socket(sockfd, list);
}

void action_stats(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char msg[256];
	int count = 0, cnick = 0, chits = 0;
	int row;
	(void) args;
	
	sqlquery = sqlite3_mprintf("SELECT COUNT(id), COUNT(DISTINCT nick), SUM(hit) FROM url WHERE chan = '%q'", message->chan);
	
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
			count = sqlite3_column_int(stmt, 0);
			cnick = sqlite3_column_int(stmt, 1);
			chits = sqlite3_column_int(stmt, 2);
		}
			
		sprintf(msg, "PRIVMSG %s :Got %d url on database for %d nicks and %d total hits", message->chan, count, cnick, chits);
		raw_socket(sockfd, msg);
	
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
	char temp[512];
	(void) args;
	
	/* Flood Protection */
	if(time(NULL) - (60 * 10) < message->channel->last_chart_request) {
		snprintf(temp, sizeof(temp), "PRIVMSG %s :Avoiding flood, bitch !", message->chan);
		raw_socket(sockfd, temp);
		return;
		
	} else message->channel->last_chart_request = time(NULL);
	
	/* Working */          
	sqlquery = sqlite3_mprintf("SELECT count(id), date(time, 'unixepoch') d, strftime('%%w', time, 'unixepoch') w FROM url WHERE chan = '%q' AND time > 0 GROUP BY d ORDER BY d DESC LIMIT 31", message->chan);
	
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		/* sqlite3_column_int auto-finalize */
		nbrows = db_sqlite_num_rows(stmt);
		values = (int*) malloc(sizeof(int) * nbrows);
		days   = (char*) malloc(sizeof(char) * nbrows);
	
		printf("[ ] Action: Chart: %u rows fetched.\n", nbrows);
		
		if((stmt = db_select_query(sqlite_db, sqlquery))) {
			i = nbrows - 1;
			
			while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
				values[i] = sqlite3_column_int(stmt, 0);	/* count value */
				days[i]   = (char) sqlite3_column_int(stmt, 2);	/* day of week */
				
				printf("[ ] Action: Chart: Day %s (url %d) is %d\n", sqlite3_column_text(stmt, 1), values[i], days[i]);
				
				if(i == 0)
					strcpy(first_date, (char*) sqlite3_column_text(stmt, 1));
					
				if(i == nbrows - 1)
					strcpy(last_date, (char*) sqlite3_column_text(stmt, 1));
				
				i--;
			}
		}
			
	} else fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	chart = ascii_chart(values, nbrows, lines, days);
	
	/* Chart Title */
	snprintf(temp, sizeof(temp), "PRIVMSG %s :Chart: urls per day from %s to %s", message->chan, first_date, last_date);
	raw_socket(sockfd, temp);
	
	/* Chart data */
	for(i = lines - 1; i >= 0; i--) {
		snprintf(temp, sizeof(temp), "PRIVMSG %s :%s", message->chan, *(chart + i));
		raw_socket(sockfd, temp);
	}
	
	free(values);
	free(days);
}

void action_uptime(ircmessage_t *message, char *args) {
	time_t now;
	char msg[256];
	char *uptime, *rehash;
	(void) args;
	
	now  = time(NULL);
		
	uptime = time_elapsed(now - global_core.startup_time);
	rehash = time_elapsed(now - global_core.rehash_time);
	
	sprintf(msg, "PRIVMSG %s :Bot uptime: %s (rehashed %d times, rehash uptime: %s)", message->chan, uptime, global_core.rehash_count, rehash);
	raw_socket(sockfd, msg);
	
	free(uptime);
	free(rehash);
}

void action_backlog_url(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char *msg;
	const unsigned char *row_url, *row_nick, *row_title;
	char *url_nick;
	int row;
	size_t len;
	(void) args;
	
	/* Flood Protection */
	if(time(NULL) - (60 * 10) < message->channel->last_backurl_request) {
		irc_privmsg(message->chan, "Avoiding flood, bitch !");
		return;
		
	} else message->channel->last_backurl_request = time(NULL);
	
	sqlquery = sqlite3_mprintf("SELECT url, nick, title FROM url WHERE chan = '%q' ORDER BY id DESC LIMIT 5", message->chan);
	
	if((stmt = db_select_query(sqlite_db, sqlquery))) {	
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
			row_url   = sqlite3_column_text(stmt, 0);
			row_nick  = sqlite3_column_text(stmt, 1);
			row_title = sqlite3_column_text(stmt, 2);
			
			url_nick = (char*) malloc(sizeof(char) * strlen((char*) row_nick) + 4);
			strcpy(url_nick, (char*) row_nick);
			
			if(!row_title)
				row_title = (unsigned char*) "(No title)";
			
			len = strlen((char*) row_url) * strlen(url_nick) * strlen((char*) row_title) + 64;
			msg = (char*) malloc(sizeof(char) * len + 1);
				
			snprintf(msg, len, "PRIVMSG %s :<%s> %s | %s", message->chan, anti_hl(url_nick), row_url, row_title);
			
			raw_socket(sockfd, msg);
			
			free(url_nick);
			free(msg);
		}
	
	} else fprintf(stderr, "[-] URL Parser: cannot select url\n");
	
	/* Clearing */
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
}

void action_seen(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	const unsigned char *msg;
	char *output;
	time_t timestamp;
	int row, len;
	struct tm * timeinfo;
	char buffer[64], found = 0;
	
	if(!*args) {
		irc_privmsg(message->chan, "Missing arguments");
		return;
	}
	
	/* Trim last spaces */
	short_trim(args);
	
	sqlquery = sqlite3_mprintf("SELECT timestamp, message FROM logs WHERE chan = '%q' AND nick = '%q' ORDER BY timestamp DESC LIMIT 1", message->chan, args);
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] Action/Seen: SQL Error\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
		found = 1;
		
		timestamp = sqlite3_column_int(stmt, 0);
		msg       = sqlite3_column_text(stmt, 1);
		
		printf("[ ] Action/Seen: message: <%s>\n", msg);
		
		len = strlen((char*) msg) + strlen(args) + sizeof(buffer) + 16;
		output = (char*) malloc(sizeof(char) * len);
		
		timeinfo = localtime(&timestamp);
		strftime(buffer, sizeof(buffer), "%A %d %B %X %Y", timeinfo);
		
		snprintf(output, len, "PRIVMSG %s :[%s] <%s> %s", message->chan, buffer, anti_hl(args), msg);
		raw_socket(sockfd, output);
		
		free(output);
	}
	
	if(!found) {
		output = (char*) malloc(sizeof(char) * 128);
		snprintf(output, 128, "PRIVMSG %s :No match found for <%s>", message->chan, args);
		raw_socket(sockfd, output);
		free(output);
	}
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

void action_somafm(ircmessage_t *message, char *args) {
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
				
			sprintf(cmdline, "PRIVMSG %s :Stations list: %s (Default: %s)", message->chan, list, somafm_stations[id].ref);
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
	
	somafm_handle(message->chan, (somafm_stations + id));
}

void action_dns(ircmessage_t *message, char *args) {
	struct hostent *he;
	char reply[256];
	char *ipbuf;
	
	if(!*args) {
		irc_privmsg(message->chan, "Missing arguments");
		return;
	}
		
	if((he = gethostbyname(args)) == NULL) {
		sprintf(reply, "PRIVMSG %s :Cannot resolve host", message->chan);
		raw_socket(sockfd, reply);
		return;
	}
	
	
	ipbuf = inet_ntoa(*((struct in_addr *)he->h_addr_list[0]));
	
	snprintf(reply, sizeof(reply), "PRIVMSG %s :%s -> %s", message->chan, args, ipbuf);
	raw_socket(sockfd, reply);
}

void action_count(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char output[256];
	int row, count1, count2;
	char found = 0;
	
	if(!*args)
		args = message->nick;
	
	/* Trim last spaces */
	short_trim(args);
	
	sqlquery = sqlite3_mprintf("SELECT COUNT(*) as match, (SELECT COUNT(*) FROM logs WHERE chan = '%q') as total FROM logs WHERE nick = '%q' AND chan = '%q';", message->chan, args, message->chan);
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] Action/Count: SQL Error\n");
	
	while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
		count1 = sqlite3_column_int(stmt, 0);
		count2 = sqlite3_column_int(stmt, 1);
		
		if(count1)
			found = 1;
		
		printf("[ ] Action/Count: %d / %d\n", count1, count2);
		
		if(count1 && count2) {
			snprintf(output, sizeof(output), "PRIVMSG %s :Got %d lines for <%s>, which is %.2f%% of the %d total lines", message->chan, count1, anti_hl(args), ((float) count1 / count2) * 100, count2);
			raw_socket(sockfd, output);
		}
	}
	
	if(!found) {
		snprintf(output, sizeof(output), "PRIVMSG %s :No match found for <%s>", message->chan, args);
		raw_socket(sockfd, output);
	}
	
	/* Clearing */
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

void action_known(ircmessage_t *message, char *args) {
	char *list, *listhl;
	char output[256];
	whois_t *whois;
		
	if(!*args) {
		irc_privmsg(message->chan, "Missing arguments");
		return;
	}
	
	short_trim(args);
	
	if((whois = irc_whois(args))) {
		if((list = irc_knownuser(args, whois->host))) {
			listhl = anti_hl_each_words(list, strlen(list), UTF_8);
			snprintf(output, sizeof(output), "PRIVMSG %s :%s (host: %s) is also known as: %s", message->chan, anti_hl(args), whois->host, listhl);
			
			free(listhl);
			free(list);
			
			whois_free(whois);
			
		} else snprintf(output, sizeof(output), "PRIVMSG %s :<%s> has no previous known host", message->chan, anti_hl(args));
		
	} else snprintf(output, sizeof(output), "PRIVMSG %s :<%s> is not connected", message->chan, args);
	
	raw_socket(sockfd, output);
}

void action_url(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char *output = NULL, *title, *url, *nick;
	time_t time;
	struct tm * timeinfo;
	char date[128], *url_nick;
	
	int row, len;
	
	if(!*args) {
		irc_privmsg(message->chan, "Missing arguments");
		return;
	}
	
	/* Trim last spaces */
	short_trim(args);
	
	sqlquery = sqlite3_mprintf("SELECT url, title, nick, time FROM url WHERE (url LIKE '%%%q%%' OR title LIKE '%%%q%%') AND chan = '%q' ORDER BY time DESC LIMIT 5;", args, args, message->chan);
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
			url   = (char*) sqlite3_column_text(stmt, 0);
			title = (char*) sqlite3_column_text(stmt, 1);
			nick  = (char*) sqlite3_column_text(stmt, 2);
			time  = (time_t) sqlite3_column_int(stmt, 3);
			
			timeinfo = localtime(&time);
			strftime(date, sizeof(date), "%x %X", timeinfo);
			
			if(!title)
				title = "Unknown title";				
			
			url_nick = (char*) malloc(sizeof(char) * strlen((char*) nick) + 4);
			strcpy(url_nick, nick);
				
			len = strlen(url) + strlen(title) + strlen(nick) + 128;
			output = (char*) malloc(sizeof(char) * len);
				
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

void action_google(ircmessage_t *message, char *args) {
	unsigned int max;
	char msg[1024], *url;
	google_search_t *google;
	unsigned int i;

	if(!*args) {
		irc_privmsg(message->chan, "Missing arguments");
		return;
	}
	
	/* Trim last spaces */
	short_trim(args);
	
	google = google_search(args);
	if(google->length) {
		if(!strncmp(message->command, ".google", 7))
			max = (google->length < 3) ? google->length : 3;

		else max = 1;

		for(i = 0; i < max; i++) {
			if(!(url = shurl(google->result[i].url)))
				url = google->result[i].url;

			snprintf(msg, sizeof(msg), "%u) %s | %s", i + 1, google->result[i].title, url);
			irc_privmsg(message->chan, msg);
			free(url);
		}

	} else irc_privmsg(message->chan, "No result");
	
	google_free(google);
}

void action_man(ircmessage_t *message, char *args) {
	char reply[1024];
	unsigned int i;
	
	if(!*args) {
		irc_privmsg(message->chan, "Missing arguments");
		return;
	}
	
	for(i = 0; i < __request_count; i++) {
		if(match_prefix(args, __request[i].match + 1)) {
			sprintf(reply, "PRIVMSG %s :%s: %s", message->chan, args, __request[i].man);
			raw_socket(sockfd, reply);
			return;
		}
	}
}

void action_notes(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery, *msg;
	int row, count = 0;
	
	if(!*args || !(msg = strchr(args, ' '))) {
		irc_privmsg(message->chan, "Missing arguments");
		return;
	}
	
	*(msg++) = '\0';
	
	sqlquery = sqlite3_mprintf("SELECT COUNT(id) FROM notes WHERE chan = '%q' AND tnick = '%q' AND seen = 0", message->chan, args);
	if((stmt = db_select_query(sqlite_db, sqlquery))) {
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
			if((count = sqlite3_column_int(stmt, 0)) >= MAX_NOTES)
				irc_privmsg(message->chan, "Message queue full");
		}
	
	} else fprintf(stderr, "[-] Action/URL: SQL Error\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	if(count >= MAX_NOTES)
		return;
	
	/* Inserting */
	sqlquery = sqlite3_mprintf("INSERT INTO notes (fnick, tnick, chan, message, seen, ts) VALUES ('%q', '%q', '%q', '%q', 0, %u)", message->nick, args, message->chan, msg, time(NULL));
	if(db_simple_query(sqlite_db, sqlquery)) {
		irc_privmsg(message->chan, "Message saved");
		
	} else irc_privmsg(message->chan, "Cannot sent your message. Try again later.");
	
	sqlite3_free(sqlquery);
}

void action_run_c(ircmessage_t *message, char *args) {
	lib_run_init(message, args, C);
}

void action_run_py(ircmessage_t *message, char *args) {
	lib_run_init(message, args, PYTHON);
}

void action_run_hs(ircmessage_t *message, char *args) {
	lib_run_init(message, args, HASKELL);
}

void action_run_php(ircmessage_t *message, char *args) {
	lib_run_init(message, args, PHP);
}

void action_backlog(ircmessage_t *message, char *args) {
	sqlite3_stmt *stmt;
	char *sqlquery, *msg;
	const unsigned char *row_nick, *row_msg;
	time_t row_time;
	struct tm * timeinfo;
	char date[128], *log_nick;
	size_t len, row;
	(void) args;
	
	/* Flood Protection */
	if(time(NULL) - (60 * 10) < message->channel->last_backlog_request) {
		irc_privmsg(message->chan, "Avoiding flood, bitch !");
		return;
		
	} else message->channel->last_backlog_request = time(NULL);
	
	sqlquery = sqlite3_mprintf("SELECT nick, timestamp, message FROM "
				   "  (SELECT nick, timestamp, message FROM logs WHERE chan = '%q' ORDER BY id DESC LIMIT 1, 7) "
				   "ORDER BY timestamp ASC", message->chan);
	
	if((stmt = db_select_query(sqlite_db, sqlquery))) {	
		while((row = sqlite3_step(stmt)) != SQLITE_DONE && row == SQLITE_ROW) {
			row_nick  = sqlite3_column_text(stmt, 0);
			row_time  = sqlite3_column_int(stmt, 1);
			row_msg   = sqlite3_column_text(stmt, 2);
			
			/* Date formating */
			timeinfo = localtime(&row_time);
			strftime(date, sizeof(date), "%X", timeinfo);
			
			log_nick = (char*) malloc(sizeof(char) * strlen((char*) row_nick) + 4);
			strcpy(log_nick, (char*) row_nick);
			
			len = strlen(log_nick) * strlen((char*) row_msg) + 64;
			msg = (char*) malloc(sizeof(char) * len + 1);
				
			snprintf(msg, len, "PRIVMSG %s :[%s] <%s> %s", message->chan, date, anti_hl(log_nick), row_msg);
			
			raw_socket(sockfd, msg);
			
			free(log_nick);
			free(msg);
		}
	
	} else fprintf(stderr, "[-] URL Parser: cannot select logs\n");
	
	/* Clearing */
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
}

void action_wiki(ircmessage_t *message, char *args) {
	google_search_t *google;
	char *data = NULL;
	char reply[1024];

	if(!*args) {
		irc_privmsg(message->chan, "Missing arguments");
		return;
	}

	/* Trim last spaces */
	short_trim(args);

	/* Using 'reply' for request and answer */
	snprintf(reply, sizeof(reply), "%s wiki", args);
	google = google_search(reply);

	if(google->length) {
		if((data = wiki_head(google->result[0].url))) {
			if(strlen(data) > 290)
				strcpy(data + 280, " [...]");

			snprintf(reply, sizeof(reply), "Wiki: %s [%s]", data, google->result[0].url);
			irc_privmsg(message->chan, reply);

		} else irc_privmsg(message->chan, "Wiki: cannot grab data from wikipedia");
	} else irc_privmsg(message->chan, "Wiki: not found");

	free(data);
	google_free(google);
}
