/* z03 - small bot with some network features - url handling/mirroring/management
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
#include <openssl/crypto.h>
#include <openssl/sha.h>
#include "database.h"
#include "core.h"
#include "downloader.h"
#include "urlmanager.h"
#include "urlrepost.h"
#include "ircmisc.h"

repost_t *repost_new() {
	return calloc(1, sizeof(repost_t));
}

void repost_free(repost_t *repost) {
	free(repost->op);
	free(repost->url);
	free(repost->title);
	free(repost->sha1);
	free(repost);
}

static char *repost_timestring(time_t timestamp, char *timestring) {
	struct tm * timeinfo;

	if(timestamp > 0) {
		timeinfo = localtime(&timestamp);

		sprintf(timestring,
			"%02d/%02d/%02d %02d:%02d:%02d",
			timeinfo->tm_mday,
			timeinfo->tm_mon + 1,
			(timeinfo->tm_year + 1900 - 2000),
			timeinfo->tm_hour,
			timeinfo->tm_min,
			timeinfo->tm_sec
		);

	} else strcpy(timestring, "unknown");

	return timestring;
}

//
// load initital repost data from database, based on url
// fill in repost_t struct
//
repost_t *url_repost_hit(char *url, ircmessage_t *message, repost_t *repost) {
	sqlite3_stmt *stmt;
	char *sqlquery;

	sqlquery = sqlite3_mprintf(
		"SELECT id, nick, hit, time FROM url WHERE url = '%q' AND chan = '%q'",
		url, message->chan
	);

	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery)) == NULL) {
		fprintf(stderr, "[-] urlmanager/parser: cannot select url\n");
		sqlite3_free(sqlquery);
		return NULL;
	}

	// loading database previous informations
	if(sqlite3_step(stmt) == SQLITE_ROW) {
		repost->id  = sqlite3_column_int(stmt, 0);
		repost->op  = strdup((char *) sqlite3_column_text(stmt, 1));
		repost->hit = sqlite3_column_int(stmt, 2);

		repost->timestamp = sqlite3_column_int(stmt, 3);
	}

	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);

	/* updating/inserting database */
	if(repost->op) {
		// not send from original poster
		if(strcmp(repost->op, message->nick)) {
			repost->type = URL_MATCH;

		} else repost->type = NICK_IS_OP;

		printf("[+] urlmanager/parser: url already on database, updating...\n");
		sqlquery = sqlite3_mprintf("UPDATE url SET hit = hit + 1 WHERE id = %d", repost->id);

	} else {
		printf("[+] urlmanager/new url: inserting...\n");

		repost->type = NOT_REPOST;
		repost->op   = strdup(message->nick);
		repost->hit  = 1;
		repost->timestamp = time(NULL);

		sqlquery = sqlite3_mprintf(
			"INSERT INTO url (nick, url, hit, time, chan) "
			"VALUES ('%q', '%q', 1, %d, '%q')", 
			repost->op, url, repost->timestamp, message->chan
		);
	}

	if(!db_sqlite_simple_query(sqlite_db, sqlquery))
		printf("[-] urlmanager/parser: cannot update db\n");

	sqlite3_free(sqlquery);

	return repost;
}

repost_t *url_repost_advanced(curl_data_t *curl, ircmessage_t *message, repost_t *repost) {
	sqlite3_stmt *stmt;
	char *sqlquery, timing[128];
	char buffer[512], ophl[64];
	repost_t *newrepost;
	unsigned char sha1_hexa[SHA_DIGEST_LENGTH];
	char sha1_char[(SHA_DIGEST_LENGTH * 2) + 1];

	// saving sha1 for the url, if it's not already a repost
	if(repost->type == NOT_REPOST) {
		sqlquery = sqlite3_mprintf(
			"UPDATE url SET sha1 = '%q' WHERE url = '%q'",
			repost->sha1, curl->url
		);
	}

	// building sha1 for curl data
	SHA1((unsigned char *) curl->data, curl->length, sha1_hexa);
	repost->sha1 = strdup(sha1_string(sha1_hexa, sha1_char));

	// check if the repost is already set on URL_MATCH
	// it's means that the user is not the original poster
	// and that the url is already on database
	if(repost->type == URL_MATCH) {
		strcpy(ophl, repost->op);
		repost_timestring(repost->timestamp, timing);

		zsnprintf(buffer,
			"[url match, OP is %s (%s), hit %d times]",
			anti_hl(ophl), timing, repost->hit + 1
		);

		irc_privmsg(message->chan, buffer);

		return NULL;
	}

	// now we lookup for a corresponding title or checksum
	// from the new data on curl buffer
	if(curl->type == IMAGE_ALL) {
		sqlquery = sqlite3_mprintf(
			"SELECT id, nick, hit, time, url, sha1 "
			"FROM url                              "
			"WHERE chan = '%q'                     "
			"  AND sha1 = '%s'                     "
			"  AND nick != '%q'                    ",
			message->chan, repost->sha1, message->nick
		);
		
	} else {
		sqlquery = sqlite3_mprintf(
			"SELECT id, nick, hit, time, url, sha1 "
			"FROM url                              "
			"WHERE chan = '%q'                     "
			"  AND (title = '%q' OR sha1 = '%q')   "
			"  AND nick != '%q'                    ",
			message->chan, repost->title, repost->sha1, message->nick
		);
	}

	// building new repost, previous is useless
	// if the query return soemthing, that means that a positif match
	// exists and it's not from the original poster
	newrepost = repost_new();

	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery)) == NULL) {
		fprintf(stderr, "[-] urlmanager/parser: cannot select url\n");
		sqlite3_free(sqlquery);
		return repost;
	}

	// loading 
	if(sqlite3_step(stmt) == SQLITE_ROW) {
		newrepost->id  = sqlite3_column_int(stmt, 0);
		newrepost->op  = strdup((char *) sqlite3_column_text(stmt, 1));
		newrepost->hit = sqlite3_column_int(stmt, 2);

		newrepost->timestamp = sqlite3_column_int(stmt, 3);
		newrepost->url  = strdup((char *) sqlite3_column_text(stmt, 4));

		if(sqlite3_column_text(stmt, 5))
			newrepost->sha1 = strdup((char *) sqlite3_column_text(stmt, 5));

		repost_timestring(newrepost->timestamp, timing);
	}

	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);

	// if the repost op is null, we don't have any match, leaving
	if(!newrepost->op) {
		printf("[-] lib/repost: no new repost op, skipping\n");
		repost_free(newrepost);
		return repost;
	}

	// copy original poster on local char to anti-hl it
	strcpy(ophl, newrepost->op);

	// if sha1 is not null and type is image, that means that's a checksum repost
	// we don't need to check the title
	if(newrepost->sha1 && curl->type == IMAGE_ALL) {
		zsnprintf(buffer,
			"[checksum match, OP is %s (%s), hit %d times. URL waz: %s]",
			anti_hl(ophl), timing, newrepost->hit + 1, newrepost->url
		);

		irc_privmsg(message->chan, buffer);

		repost_free(newrepost);
		return repost;
	}

	// there is only the title match possibility
	// skipping string compar
	zsnprintf(buffer,
		"[title match, OP is %s (%s), hit %d times. URL waz: %s]",
		anti_hl(ophl), timing, newrepost->hit + 1, newrepost->url
	);

	irc_privmsg(message->chan, buffer);

	repost_free(newrepost);

	return repost;
}
