/* z03 - bible based actions (backlog, url search, ...)
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
#include "ircmisc.h"
#include "actions_3filles.h"

#ifdef ENABLE_3FILLES_BOOK

static sqlite3 *book_3filles_db;

//
// registering commands
//

static request_t __action_3filles = {
        .match    = ".3filles",
        .callback = action_3filles,
        .man      = "print random quote from book Les Trois Filles de leur Mère",
        .hidden   = 0,
        .syntaxe  = ".3filles [chapter] [quote line]",
};

__registrar actions_3filles() {
        request_register(&__action_3filles);
}

//
// commands implementation
//

// return a new empty book
book_t *book_new() {
	return (book_t *) calloc(1, sizeof(book_t));
}

// free a allocated book
void book_free(book_t *book) {
	free(book->text);
	free(book);
}

// return a book data from chapter and line
// return NULL if not found
static char *book_data(int chapter, int line) {
	sqlite3_stmt *stmt;
	char *sqlquery, *data = NULL;
	
	sqlquery = sqlite3_mprintf(
		"SELECT data FROM book    "
		"WHERE chapter = %d       "
		"  AND line = %d          "
		"ORDER BY RANDOM() LIMIT 1",
		chapter, line
	);

	if((stmt = db_sqlite_select_query(book_3filles_db, sqlquery))) {
		if(sqlite3_step(stmt) == SQLITE_ROW)
			data = strdup((char *) sqlite3_column_text(stmt, 0));
	
	} else fprintf(stderr, "[-] book/data: cannot get book data\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return data;
}

// return a random book line for a given book chapter
// if chapter doesn't exists, line 0 will be returned
static int book_random_line(int chapter) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	int line = 0;
	
	sqlquery = sqlite3_mprintf(
		"SELECT line FROM book    "
		"WHERE chapter = %d       "
		"ORDER BY RANDOM() LIMIT 1",
		chapter
	);

	if((stmt = db_sqlite_select_query(book_3filles_db, sqlquery))) {
		if(sqlite3_step(stmt) == SQLITE_ROW)
			line = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] book/line: cannot random line\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return line;
}


// check on database if book-chapter exists
// return chapter if exists, return 0 if not exists
static int book_line(int chapter, int line) {
	sqlite3_stmt *stmt;
	int temp = 0;
	char *sqlquery;
	
	sqlquery = sqlite3_mprintf(
		"SELECT line FROM book "
		"WHERE chapter = %d    "
		"  AND line = %d       "
		"LIMIT 1               ",
		chapter, line
	);
	
	if((stmt = db_sqlite_select_query(book_3filles_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW)
			temp = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] book/line: cannot select line\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return temp;
}

// return a random book chapter for a given book id
// if book doesn't exists, chapter 0 will be returned
static int book_random_chapter() {
	sqlite3_stmt *stmt;
	char *sqlquery;
	int chapter = 0;
	
	sqlquery = "SELECT chapter FROM book ORDER BY RANDOM() LIMIT 1";

	if((stmt = db_sqlite_select_query(book_3filles_db, sqlquery))) {
		// sets book book title and book id
		if(sqlite3_step(stmt) == SQLITE_ROW)
			chapter = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] bible/chapter: cannot random chapter\n");
	
	sqlite3_finalize(stmt);
	
	return chapter;
}

// check on database if book-chapter exists
// return chapter if exists, return 0 if not exists
static int book_chapter(int chapter) {
	sqlite3_stmt *stmt;
	int temp = 0;
	char *sqlquery;
	
	sqlquery = sqlite3_mprintf(
		"SELECT chapter FROM book WHERE chapter = %d LIMIT 1",
		chapter
	);
	
	if((stmt = db_sqlite_select_query(book_3filles_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW)
			temp = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] book/chapter: cannot select chapter\n");
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	return temp;
}

// display on channel an error message
// book_t is read to find where the error is
void book_error(book_t *book, ircmessage_t *message) {
	char buffer[512];
	
	if(!book->chapter) {
		irc_privmsg(message->chan, "chapter not found");
		return;
	}
	
	if(!book->line) {
		zsnprintf(buffer, "line not found for chapter [%d]", book->chapter);
		irc_privmsg(message->chan, buffer);
		return;
	}
}

// converts args into a valid book requests
// unknown fields are filled by random valid values
// return NULL if something is wrong on the args (non-exists field)
// at least first field must exists
static book_t *args_to_book(char *args, book_t *book) {
	char *match;
	int temp;

	// reading chapter id
	temp = atoi(args);
	
	// grab chapter, if not found, stop here.
	if(!(book->chapter = book_chapter(temp)))
		return NULL;
		
	else book->chapter = temp;
	
	// if match is NULL, we don't have anything afther chapter
	if(!(match = strchr(args, ' '))) {
		book->line = book_random_line(book->chapter);
		book->text = book_data(book->chapter, book->line);
		return book;
	}
	
	// grabbing line, checking if it exists
	book->line = atoi(++match);	
	if(!(book->line = book_line(book->chapter, book->line)))
		return NULL;
	
	if(!(book->text = book_data(book->chapter, book->line)))
		return NULL;
	
	return book;
}

void action_3filles(ircmessage_t *message, char *args) {
	char buffer[1024];
	book_t *book;

        if(sqlite3_open(TROISFILLES_DATABASE_FILE, &book_3filles_db) != SQLITE_OK) {
		fprintf(stderr, "[-] sqlite: cannot open sqlite databse: <%s>\n", sqlite3_errmsg(book_3filles_db));
		irc_privmsg(message->chan, "sorry, cannot open the book right now :(");
		return;
	}
	
	book = book_new();
        
        // fill the book
        if(*args) {
		args = rtrim(args);
		
		if(!(args_to_book(args, book))) {
			book_error(book, message);
			book_free(book);
			return;
		}
	
	// fill random value if no arguments
	} else {
		// random stuff
		book->chapter = book_random_chapter();
		book->line    = book_random_line(book->chapter);
		book->text    = book_data(book->chapter, book->line);
	}	
	
        zsnprintf(buffer, "[Chapitre %d, %d]: %s", book->chapter, book->line, book->text);
        irc_privmsg(message->chan, buffer);
        
	// freeing stuff
	book_free(book);
        
        if(sqlite3_close(book_3filles_db) != SQLITE_OK)
		fprintf(stderr, "[-] sqlite: cannot close database: %s\n", sqlite3_errmsg(book_3filles_db));
}
#endif
