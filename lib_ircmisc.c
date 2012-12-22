/* z03 - small bot with some network features - irc miscallinious functions (anti highlights, ...)
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
#include <ctype.h>
#include <iconv.h>
#include "core_init.h"
#include "core_database.h"
#include "lib_urlmanager.h"
#include "lib_ircmisc.h"

typedef struct iconv_data_t {
	iconv_t ic;
	
	const char *inchar;      // input charset
	const char *outchar;     // output charset
	char *instr;             // input string
	size_t insize;	         // input string length
	char *outstr;            // output string
	char *__outstr;          // internal output string save pointer
	size_t outsize;          // output string length
	
} iconv_data_t;

inline short utf8_charlen(unsigned char c) {
	if(c < 0x80)
		return 1;         /* 0xxxxxxx */
		
	if((c & 0xe0) == 0xc0)
		return 2;         /* 110xxxxx */

	if((c & 0xf0) == 0xe0)
		return 3;         /* 1110xxxx */
		
	if((c & 0xf8) == 0xf0 && (c <= 0xf4))
		return 4;         /* 11110xxx */
		
	return 0; /* invalid UTF8 */
}

inline size_t chrcpy_utf8(char *dst, char *src) {
	short i, j;
	
	i = utf8_charlen(*src);
	
	for(j = i; i >= 0; i--)
		dst[i] = src[i];
	
	return j;
}

void debug(char *data, size_t len) {
	unsigned int i;
	
	printf("[ ] Dump: ");
	
	for(i = 0; i < len; i++)
		printf("%02x ", (unsigned char) data[i]);
	
	printf("\n");
}

char * clean_filename(char *file) {
	int i = 0;
	
	while(*(file + i)) {
		if(*(file + i) == ':' || *(file + i) == '/' || *(file + i) == '#' || *(file + i) == '?')
			*(file + i) = '_';
		
		i++;
	}
	
	return file;
}

char * anti_hl(char *nick) {
	char temp[64];
	
	strcpy(temp, nick);
	sprintf(nick, "%c\u200b%s", *temp, temp + 1);
	
	return nick;
}

void anti_hl_append(char **write, char **read) {
	int s;
	
	// Copy next char
	s = chrcpy_utf8(*write, *read);
	*write += s;
	*read  += s;
	
	// Copy special char (3 bytes)
	sprintf(*write, "\u200b");
	*write += 3;
}

size_t iconvit(iconv_data_t *ico) {
	size_t result = ico->outsize;
	
	// Saving current output buffer
	ico->__outstr = ico->outstr;
	
	printf("[+] iconv: converting from %s to %s...\n", ico->inchar, ico->outchar);
	
	// Opening iconv handler
	if((ico->ic = iconv_open(ico->outchar, ico->inchar)) == (iconv_t) -1) {
		perror("[-] iconv_open");
		return 0;
	}
	
	if(iconv(ico->ic, &ico->instr, &ico->insize, &ico->outstr, &ico->outsize) == (size_t) -1) {
		perror("[-] iconv");
		return 0;
	}
	
	iconv_close(ico->ic);
	
	// Writing end of line
	result -= ico->outsize;
	ico->__outstr[result] = '\0';
	
	return result;
}

char * anti_hl_each_words(char *str, size_t len, charset_t charset) {
	char *stripped = NULL, *convert = NULL;
	int allocation, s;
	char *read, *write;
	iconv_data_t iconvme;
	
	/* \u200b is 3 bytes length, str is normally not long, just allocating 3x */
	allocation = ((len * 3) + 8);
	printf("[.] AntiHL: Allocating: %u bytes (original: %u)\n", allocation, len);
	
	stripped = (char*) malloc(sizeof(char) * allocation);
	bzero(stripped, allocation);
	
	/* Checking if string is probably an iso string */
	if(charset != UNKNOWN_CHARSET && charset != UTF_8) {
		convert = (char*) malloc(sizeof(char) * allocation);
		
		iconvme.outchar = "utf-8";
		iconvme.instr   = str;
		iconvme.insize  = strlen(str);
		iconvme.outstr  = convert;
		iconvme.outsize = allocation;
		
		switch(charset) {
			case WIN_1252:
				iconvme.inchar = "windows-1252";
			break;
			
			case ISO_8859:
			default:
				iconvme.inchar = "iso-8859-1";
		}
		
		if(!iconvit(&iconvme))
			return NULL;
			
		read = convert;

	} else read  = str;
	
	write = stripped;
	
	// debug(read, strlen(read));
	
	// Escape first word
	if(*read && *read != '&')
		anti_hl_append(&write, &read);
	
	while(*read) {
		s = chrcpy_utf8(write, read);
		write += s;
		
		// If space, and next is valid, and next is not space (avoid ' - ')
		// Skip if contains & (html entities)
		if(*read == ' ' && *(read + 1) && *(read + 1) != '&' && *(read + 2) && *(read + 2) != ' ') {
			// Copy Space
			*write = *read++;
			anti_hl_append(&write, &read);
			
		} else read += s;
		
		// printf("<%s>\n", stripped);
	}
	
	// debug(stripped, allocation);
	
	printf("[.] AntiHL: <%s>\n", stripped);
	
	/* Freeing "convert", should be NULL if not converted */
	free(convert);
	
	return stripped;
}

char * ltrim(char *str) {
	while(isspace(*str))
		str++;
		
	return str;
}

char * rtrim(char *str) {
	char *back = str + strlen(str);
	
	while(isspace(*--back));
		*(back+1) = '\0';
		
	return str;
}

char * crlftrim(char *str) {
	char *keep = str;
	
	while(*str) {
		if(*str == '\r' || *str == '\n')
			*str = ' ';
		
		str++;
	}
	
	return keep;
}

char * time_elapsed(time_t time) {
	char *output = (char*) malloc(sizeof(char) * 64);
	unsigned int days, hours, min;
	
	if(time < 60) {
		sprintf(output, "%lu seconds", time);
		return output;
	}
	
	days  = time / (24 * 60 * 60);
	time %= (24 * 60 * 60);
	
	hours = time / (60 * 60);
	time %= (60 * 60);
	
	min   = time / 60;
	time %= 60;
	
	if(!days)
		sprintf(output, "%02u:%02u", hours, min);
		
	else sprintf(output, "%u days, %02u:%02u", days, hours, min);
	
	return output;
}

char * short_trim(char *str) {
	size_t len = strlen(str);
	
	while(isspace(*(str + len - 1)))
		len--;
	
	*(str + len) = '\0';
		
	return str;
}

char * irc_mstrncpy(char *src, size_t len) {
	char *str;
	
	if(!(str = (char*) malloc(sizeof(char) * len + 1)))
		return NULL;
	
	strncpy(str, src, len);
	str[len] = '\0';
	
	return str;
}

int irc_extract_userdata(char *data, char **nick, char **username, char **host) {
	char *match;
	
	/* Extract Nick */
	if(!(match = strchr(data, '!')))
		return 0;
	
	*nick = irc_mstrncpy(data, match - data);
	data = match + 1;
	
	/* Extract Username */
	if(!(match = strchr(match, '@'))) {
		free(*nick);
		return 0;
	}
	
	*username = irc_mstrncpy(data, match - data);
	data = match + 1;
	
	/* Extract Host */
	if(!(match = strchr(match, ' '))) {
		free(*nick);
		free(*username);
		return 0;
	}
	
	*host = irc_mstrncpy(data, match - data);
	
	return 1;
}

char * string_index(char *str, unsigned int index) {
	unsigned int i;
	char *match;
	
	for(i = 0; i < index; i++) {
		if(!(match = strchr(str, ' ')))
			return NULL;
		
		str = match + 1;
	}
	
	if(strlen(str) > 0) {
		if((match = strchr(str, ' '))) {
			match = irc_mstrncpy(str, match - str);
			
		} else match = irc_mstrncpy(str, strlen(str));
		
		printf("[+] Index: <%s>\n", match);
		
		return match;
		
	} else return NULL;
}

char * irc_knownuser(char *nick, char *host) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	size_t size = 0, nicklen;
	char *thisnick, *nicklist = NULL;
	int row, z;
	
	/* Init List */
	nicklist = (char*) malloc(sizeof(char));
	*nicklist = '\0';
	
	nicklen = strlen(nick);
	
	/* Query database */
	sqlquery = sqlite3_mprintf("SELECT nick FROM hosts WHERE host = '%q'", host);
	if((stmt = db_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] Action/Count: SQL Error\n");
	
	/* Building list */
	z = 0;
	while((row = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(row == SQLITE_ROW) {
			z++;
			thisnick = (char*) sqlite3_column_text(stmt, 0);
			
			// Skipping same name
			if(!strncmp(thisnick, nick, nicklen))
				continue;
			
			printf("[ ] Action/known: appending: <%s>\n", thisnick);
			
			size += strlen((char*) thisnick) + 3;
			nicklist = (char*) realloc(nicklist, size);
			
			strcat(nicklist, thisnick);
			strcat(nicklist, ", ");
		}
	}
	
	if(z > 5) {
		free(nicklist);
		nicklist = (char*) malloc(sizeof(char) * 128);
		sprintf(nicklist, "(list hidden, %d rows found)  ", z);
	}
	
	if(size == 0) {
		free(nicklist);
		return NULL;
		
	} else nicklist[strlen(nicklist) - 2] = '\0';	
	
	return nicklist;
}

char * skip_header(char *data) {
	char *match;
	
	if((match = strchr(data + 1, ':')))
		return match + 1;
		
	else return NULL;
}

whois_t * whois_init() {
	whois_t *whois;
	
	whois = (whois_t*) malloc(sizeof(whois));
	
	whois->host = NULL;
	whois->ip   = NULL;
	
	return whois;
}

void whois_free(whois_t *whois) {
	if(whois) {
		free(whois->host);
		free(whois->ip);
	}
	
	free(whois);
}

whois_t * irc_whois(char *nick) {
	char *data = (char*) malloc(sizeof(char*) * (2 * MAXBUFF));
	char *next = (char*) malloc(sizeof(char*) * (2 * MAXBUFF));
	char *request, temp[128];
	whois_t *whois;
	
	// Init
	*data = '\0';
	*next = '\0';
	if(!(whois = whois_init()))
		return NULL;
	
	snprintf(temp, sizeof(temp), "WHOIS %s", nick);
	raw_socket(sockfd, temp);
	
	printf("[+] Misc: whois request: %s\n", nick);
	
	while(1) {
		read_socket(sockfd, data, next);
		printf("[ ] IRC: >> %s\n", data);
		
		if((request = skip_server(data)) == NULL) {
			printf("[-] IRC: Something wrong with protocol...\n");
			continue;
		}
		
		if(!strncmp(request, "311", 3)) {
			whois->host = string_index(request, 4);
			
		} else if(!strncmp(request, "378", 3)) {
			whois->ip = string_index(request, 7);
			
		} else if(!strncmp(request, "318", 3))
			break;
	}
	
	printf("[+] Misc: end of whois\n");
	
	return whois;
}

char * space_encode(char *str) {
	char *new = str;
	
	while(*new) {
		if(*new == ' ')
			*new = '+';
			
		new++;
	}
	
	return str;
}

void irc_privmsg(char *chan, char *message) {
	char buffer[2048];
	
	snprintf(buffer, sizeof(buffer), "PRIVMSG %s :%s", chan, message);
	raw_socket(sockfd, buffer);
}
