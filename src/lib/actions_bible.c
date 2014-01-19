/* z03 - bible based actions (backlog, url search, ...)
 * Author: Somic (g.somic@gmail.com),
 * Contributor: Maxux
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
#include "ircmisc.h"
#include "actions_bible.h"

#ifdef ENABLE_HOLY_BIBLE

static sqlite3 *verse_db;

//
// registering commands
//

static request_t __action_bible = {
        .match    = ".bible",
        .callback = action_bible,
        .man      = "print random quote from The Holy Bible",
        .hidden   = 0,
        .syntaxe  = ".bible <book [[chapter] [verse]]>, .bible list",
};

__registrar actions_bible() {
        request_register(&__action_bible);
}

//
// commands implementation
//

// return a new empty verse
verse_t *verse_new() {
	return (verse_t *) calloc(1, sizeof(verse_t));
}

// free a allocated verse
void verse_free(verse_t *verse) {
	free(verse->book_title);
	free(verse->text);
	free(verse);
}

// return a random id of verse
// the verse text is placed on given verse_t struct
static char *bible_random_verse(int book_id, int chapter, verse_t *verse) {
	sqlite3_stmt *stmt;
	char *sqlquery, *temp = NULL;
	
	sqlquery = sqlite3_mprintf(
		"SELECT verse, verse_txt FROM verses "
		"WHERE book_id = %d                  "
		"  AND chapter = %d                  "
		"ORDER BY RANDOM() LIMIT 1           ",
		book_id, chapter
	);

	if((stmt = db_sqlite_select_query(verse_db, sqlquery))) {
		// sets verse book title and book id
		if(sqlite3_step(stmt) == SQLITE_ROW) {
			verse->verse = sqlite3_column_int(stmt, 0);
			temp = strdup((char *) sqlite3_column_text(stmt, 1));
		}
	
	} else fprintf(stderr, "[-] bible/chapter: cannot random chapter\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return temp;
}

// return a verse from database
// return NULL if not found
static char *bible_verse(int book_id, int chapter, int verse_id) {
	sqlite3_stmt *stmt;
	char *sqlquery, *verse = NULL;
	
	sqlquery = sqlite3_mprintf(
		"SELECT verse_txt FROM verses "
		"WHERE book_id = %d           "
		"  AND chapter = %d           "
		"  AND verse = %d             ",
		book_id, chapter, verse_id
	);
	
	if((stmt = db_sqlite_select_query(verse_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW)
			verse = strdup((char *) sqlite3_column_text(stmt, 0));
	
	} else fprintf(stderr, "[-] bible/chapter: cannot select chapter\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return verse;
}

// return a random book chapter for a given book id
// if book doesn't exists, chapter 0 will be returned
static int bible_random_chapter(int book_id) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	int chapter = 0;
	
	sqlquery = sqlite3_mprintf(
		"SELECT chapter FROM verses          "
		"WHERE book_id = %d GROUP BY chapter "
		"ORDER BY RANDOM() LIMIT 1           ",
		book_id
	);

	if((stmt = db_sqlite_select_query(verse_db, sqlquery))) {
		// sets verse book title and book id
		if(sqlite3_step(stmt) == SQLITE_ROW)
			chapter = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] bible/chapter: cannot random chapter\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return chapter;
}

// check on database if book-chapter exists
// return chapter if exists, return 0 if not exists
static int bible_chapter(int book_id, int chapter) {
	sqlite3_stmt *stmt;
	int temp = 0;
	char *sqlquery;
	
	sqlquery = sqlite3_mprintf(
		"SELECT chapter FROM verses          "
		"WHERE book_id = %d AND chapter = %d "
		"LIMIT 1                             ",
		book_id, chapter
	);
	
	if((stmt = db_sqlite_select_query(verse_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW)
			temp = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] bible/chapter: cannot select chapter\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return temp;
}

// return a random book id and set book name on verse_t 
static int bible_random_book(verse_t *verse) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	
	sqlquery = "SELECT book_id, title FROM books "
	           "ORDER BY RANDOM() LIMIT 1        ";

	if((stmt = db_sqlite_select_query(verse_db, sqlquery))) {
		// sets verse book title and book id
		if(sqlite3_step(stmt) == SQLITE_ROW) {
			verse->book_id    = sqlite3_column_int(stmt, 0);
			verse->book_title = strdup((char *) sqlite3_column_text(stmt, 1));
		}
	
	} else fprintf(stderr, "[-] bible/book: cannot random book\n");
	
	sqlite3_finalize(stmt);
	
	return verse->book_id;
}

// return a book_id for gived name
// return 0 if boot not found
static int bible_book(char *name) {
	sqlite3_stmt *stmt;
	int bookid = 0;
	char *sqlquery;
	
	sqlquery = sqlite3_mprintf(
		"SELECT book_id FROM books       "
		"WHERE LOWER(title) = LOWER('%q')",
		name
	);
	
	if((stmt = db_sqlite_select_query(verse_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW)
			bookid = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] bible/book: cannot select book id\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return bookid;
}

// display on channel an error message
// verse_t is read to find where the error is
void bible_error(verse_t *verse, ircmessage_t *message) {
	char buffer[512];
	
	if(!verse->book_id) {
		irc_privmsg(message->chan, "no book found");
		return;
	}
	
	if(!verse->chapter) {
		zsnprintf(buffer, "chapter not found for [%s]", verse->book_title);
		irc_privmsg(message->chan, buffer);
		return;
	}
	
	if(!verse->text) {
		zsnprintf(buffer, "verse #%d not found for [%s], chapter #%d",
		          verse->verse, verse->book_title, verse->chapter);

		irc_privmsg(message->chan, buffer);
		return;
	}
}

// converts args into a valid verse requests
// unknown fields are filled by random valid values
// return NULL if something is wrong on the args (non-exists field)
// at least first field must exists
static verse_t *args_to_verse(char *args, verse_t *verse) {
	char *match, *temp;

	// got we something after the book title ?
	if((match = strchr(args, ' '))) {
		temp = strndup(args, match - args);
		
	} else temp = strdup(args);
	
	// grab book id, if not found, stop here.
	if(!(verse->book_id = bible_book(temp))) {
		free(temp);
		return NULL;
		
	} else verse->book_title = temp;
	
	// if match is NULL, we don't have anything after
	// the book name
	if(!match) {
		verse->chapter = bible_random_chapter(verse->book_id);
		verse->text = bible_random_verse(verse->book_id, verse->chapter, verse);
		return verse;
	}
	
	// grabbing chapter, checking if it exists
	verse->chapter = atoi(++match);
	if(!(verse->chapter = bible_chapter(verse->book_id, verse->chapter)))
		return NULL;
	
	// checking if we got a verse number
	if(!(match = strchr(match, ' '))) {
		verse->text = bible_random_verse(verse->book_id, verse->chapter, verse);
		return verse;
	}
	
	// grab verse id, check if it exists
	verse->verse = atoi(++match);
	if(!(verse->text = bible_verse(verse->book_id, verse->chapter, verse->verse)))
		return NULL;
	
	return verse;
}

void action_bible(ircmessage_t *message, char *args) {
	char buffer[1024];
	verse_t *verse;
	
	if(!strcmp(args, "list")) {
		irc_privmsg(message->chan, "[The Holy Bible]: https://paste.maxux.net/xG7Z1mc");
		return;
	}

        if(sqlite3_open(BIBLE_DATABASE_FILE, &verse_db) != SQLITE_OK) {
		fprintf(stderr, "[-] sqlite: cannot open sqlite databse: <%s>\n", sqlite3_errmsg(verse_db));
		irc_privmsg(message->chan, "sorry, cannot open the holy bible right now :(");
		return;
	}
	
	verse = verse_new();
        
        // fill the verse
        if(*args) {
		args = rtrim(args);
		
		if(!(args_to_verse(args, verse))) {
			bible_error(verse, message);
			verse_free(verse);
			return;
		}
	
	// fill random value if no arguments
	} else {
		// random book id/name
		bible_random_book(verse);
		verse->chapter = bible_random_chapter(verse->book_id);
		verse->text    = bible_random_verse(verse->book_id, verse->chapter, verse);
	}	
	
        zsnprintf(buffer, "%s [%d, %d]: %s", verse->book_title, verse->chapter, verse->verse, verse->text);
        irc_privmsg(message->chan, buffer);
        
	// freeing stuff
	verse_free(verse);
        
        if(sqlite3_close(verse_db) != SQLITE_OK)
		fprintf(stderr, "[-] sqlite: cannot close database: %s\n", sqlite3_errmsg(verse_db));
}
#endif
