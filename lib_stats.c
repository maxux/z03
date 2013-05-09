/* z03 - small bot with some network features
 * Author: Daniel Maxime (root@maxux.net)
 * Contributor: Darky, mortecouille, RaphaelJ, somic, ghilan
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
#include <unistd.h>
#include "core_init.h"
#include "lib_database.h"
#include "lib_list.h"
#include "lib_core.h"
#include "lib_stats.h"
#include "lib_ircmisc.h"

list_t *stats_words_read(char *chan) {
	sqlite3_stmt *stmt;
	char *sqlquery, *nickname;
	list_t *nicks;
	nick_t *nick;
	
	/* Creating nick list */
	nicks = list_init(NULL);
	
	/* Reading nick list */
	sqlquery = sqlite3_mprintf(
		"SELECT nick, words FROM stats WHERE chan = '%q'",
		chan
	);
	
	printf("[ ] lib/stats: loading nick words list for <%s>...\n", chan);
		
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			if(!(nick = (nick_t*) malloc(sizeof(nick_t))))
				diep("malloc");
				
			nickname       = (char *) sqlite3_column_text(stmt, 0);
			nick->words    = (size_t) sqlite3_column_int(stmt, 1);;
			
			list_append(nicks, nickname, nick);
		}
	}
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return nicks;
}

list_t *stats_nick_read(char *chan) {
	sqlite3_stmt *stmt;
	char *sqlquery, *nickname;
	list_t *nicks, *nicks2;
	nick_t *nick, *nick2;
	
	nicks2 = stats_words_read(chan);
	
	/* Creating nick list */
	nicks = list_init(NULL);
	
	/* Reading nick list */
	sqlquery = sqlite3_mprintf(
		"SELECT nick, lines FROM ("
		"  SELECT nick, count(*) lines, chan FROM logs "
		"  GROUP BY nick, chan) "
		"WHERE chan = '%q'",
		chan
	);
	
	printf("[ ] lib/stats: loading nick list for <%s>...\n", chan);
		
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			if(!(nick = (nick_t*) malloc(sizeof(nick_t))))
				diep("malloc");
				
			nickname       = (char *) sqlite3_column_text(stmt, 0);
			nick->lines    = (size_t) sqlite3_column_int(stmt, 1);
			nick->lasttime = 0;
			nick->online   = 0; // FIXME: check real status
			
			if((nick2 = list_search(nicks2, nickname)))
				nick->words = nick2->words;
				
			else nick->words = 0;
			
			list_append(nicks, nickname, nick);
		}
	}
	
	list_free(nicks2);
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return nicks;
}

channel_t *stats_channel_read(char *chan) {
	sqlite3_stmt *stmt;
	channel_t *channel;
	char *sqlquery;
	
	if(!(channel = (channel_t*) calloc(1, sizeof(channel_t))))
		diep("malloc");
	
	/* Reading channel stats */
	sqlquery = sqlite3_mprintf(
		"SELECT count(id) FROM logs WHERE chan = '%q'",
		chan
	);
	
	printf("[ ] lib/stats: loading channel <%s> length...\n", chan);
		
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery)))
		while(sqlite3_step(stmt) == SQLITE_ROW)
			channel->lines = (size_t) sqlite3_column_int(stmt, 0);
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return channel;
}

size_t stats_get_lasttime(char *nick, char *chan) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	size_t lasttime = 0;
	
	sqlquery = sqlite3_mprintf(
		"SELECT MAX(timestamp) FROM logs "
		"WHERE chan = '%q' AND nick = '%q'",
		chan, nick
	);
	
	printf("[ ] lib/stats: loading <%s/%s> lastime...\n", chan, nick);
		
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery)))
		while(sqlite3_step(stmt) == SQLITE_ROW)	
			lasttime = (size_t) sqlite3_column_int(stmt, 0);
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return lasttime;
}

size_t stats_get_words(char *nick, char *chan) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	size_t words = 0;
	
	sqlquery = sqlite3_mprintf(
		"SELECT words FROM stats WHERE chan = '%q' AND nick = '%q'",
		chan, nick
	);
		
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery)))
		while(sqlite3_step(stmt) == SQLITE_ROW)	
			words = (size_t) sqlite3_column_int(stmt, 0);
	
	printf("[+] lib/stats: %u words for <%s/%s>\n", words, chan, nick);
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return words;
}

channel_t *stats_channel_load(char *chan) {
	channel_t *channel;
	// char buffer[512];
	
	printf("[+] lib/stats: channel: creating <%s> environment...\n", chan);
	channel = stats_channel_read(chan);
	channel->nicks = stats_nick_read(chan);
	
	/* zsnprintf(buffer, "[table rehashed: got %u total lines for %u nicks on database]",
	                  channel->lines, channel->nicks->length);
	irc_privmsg(chan, buffer); */
	printf("[+] lib/stats: %s: table rehashed: got %u total lines for %u nicks\n",
	       chan, channel->lines, channel->nicks->length);
	
	return channel;
}

void stats_update(ircmessage_t *message, nick_t *nick, char new) {
	char *sqlquery;
	
	if(new) {
		sqlquery = sqlite3_mprintf(
			"INSERT INTO stats (nick, chan, words, lines) VALUES "
			"('%q', '%q', '%u', '%u')",
			message->nick, message->chan, nick->words, nick->lines
		);
		
	} else {
		sqlquery = sqlite3_mprintf(
			"UPDATE stats SET words = %u, lines = %u "
			"WHERE nick = '%q' AND chan = '%q'",
			nick->words, nick->lines, message->nick, message->chan
		);
	}
	
	db_sqlite_simple_query(sqlite_db, sqlquery);
	
	/* Clearing */
	sqlite3_free(sqlquery);
}

size_t stats_words_count(char *nick, char *chan) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	size_t words = 0;
	
	sqlquery = sqlite3_mprintf(
		"SELECT message FROM logs WHERE chan = '%q' AND nick = '%q'",
		chan, nick
	);
	
	printf("[ ] lib/stats: loading <%s/%s> words...\n", chan, nick);
		
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery)))
		while(sqlite3_step(stmt) == SQLITE_ROW)	
			words += words_count((char *) sqlite3_column_text(stmt, 0));
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return words;
}

void stats_set_words(char *nick, char *chan, size_t words) {
	char *sqlquery;
	
	sqlquery = sqlite3_mprintf(
		"UPDATE stats SET words = %u WHERE nick = '%q' AND chan = '%q'",
		words, nick, chan
	);
	
	db_sqlite_simple_query(sqlite_db, sqlquery);
	
	/* Clearing */
	sqlite3_free(sqlquery);
}

void stats_clear_db() {
	db_sqlite_simple_query(sqlite_db, "DELETE FROM stats");
}

void stats_build_db() {
	db_sqlite_simple_query(sqlite_db,
		"INSERT INTO stats (nick, chan, words, lines) "
		"SELECT nick, chan, 0, count(*) FROM logs GROUP BY nick, chan"
	);
}

// FIXME: it's not all, just words at this time.
void stats_rebuild_all(list_t *channels) {
	list_node_t *node_channel, *node_nick;
	channel_t *channel;
	nick_t *nick;
	
	// clearing table
	stats_clear_db();
	
	// building lines count table
	stats_build_db();
	
	// building and updating current words
	node_channel = channels->nodes;
	
	db_sqlite_simple_query(sqlite_db, "BEGIN TRANSACTION");
	
	while(node_channel) {
		channel = node_channel->data;
		node_nick = channel->nicks->nodes;
		
		// foreach nicks foreach each channel
		while(node_nick) {
			nick = node_nick->data;
			printf("[+] lib/stats: rebuild: %s/%s\n", node_channel->name, node_nick->name);
			
			// counting words
			nick->words = stats_words_count(node_nick->name, node_channel->name);
			printf("[+] lib/stats: rebuild: %s/%s: %u\n", node_channel->name, node_nick->name,
			                                              nick->words);

			// saving on database
			stats_set_words(node_nick->name, node_channel->name, nick->words);
			
			node_nick = node_nick->next;
		}
		
		node_channel = node_channel->next;
	}
	
	db_sqlite_simple_query(sqlite_db, "END TRANSACTION");
	
	printf("[+] lib/stats: table rebuilt\n");
}

void stats_load_all(list_t *root) {
	sqlite3_stmt *stmt;
	char *channel;
	channel_t *newchan;
	
	printf("[ ] lib/stats: reloading all...\n");
		
	if((stmt = db_sqlite_select_query(sqlite_db, "SELECT DISTINCT chan FROM stats"))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			channel = (char *) sqlite3_column_text(stmt, 0);
			newchan = stats_channel_load(channel);
			
			list_append(root, channel, newchan);
		}
	}
	
	sqlite3_finalize(stmt);
}

void stats_daily_update() {
	sqlite3_stmt *stmt;
	char *sqlquery, *chan;
	int nicks, lines;
	char buffer[512];
	
	printf("[ ] lib/stats: daily update database\n");
	
	// updating database
	db_sqlite_simple_query(sqlite_db,
		"INSERT INTO stats_fast (nick, chan, lines, day) "
		" SELECT nick, chan, COUNT(*), DATE('now', '-1 day', 'localtime') "
		" FROM logs "
		" WHERE timestamp >= strftime('%s', date('now', '-1 day', 'localtime'), 'utc') "
		"   AND timestamp < strftime('%s', date('now', 'localtime'), 'utc') "
		" GROUP BY nick, chan"
	);
	
	// printing summary
	sqlquery = "SELECT chan, SUM(lines) lines, COUNT(*) nicks FROM stats_fast "
	           "WHERE day = date('now', '-1 day', 'localtime') GROUP BY chan";
		
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW)	{
			chan  = (char *) sqlite3_column_text(stmt, 0);
			lines = sqlite3_column_int(stmt, 1);
			nicks = sqlite3_column_int(stmt, 2);
			
			zsnprintf(buffer, "Day changed, yesterday: %d nicks said %d lines", nicks, lines);			
			irc_privmsg(chan, buffer);
		}
	}
	
	sqlite3_finalize(stmt);
}
