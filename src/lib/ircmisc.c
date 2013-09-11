/* z03 - irc miscallinious functions (anti highlights, ...)
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
#include <openssl/md5.h>
#include <openssl/sha.h>
#include "../core/init.h"
#include "database.h"
#include "core.h"
#include "downloader.h"
#include "ircmisc.h"

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
		
	return 0; /* invalid utf-8 */
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

char *clean_filename(char *file) {
	int i = 0;
	
	while(*(file + i)) {
		if(*(file + i) == ':' || *(file + i) == '/' || *(file + i) == '#' || *(file + i) == '?')
			*(file + i) = '_';
		
		i++;
	}
	
	return file;
}

char *anti_hl(char *nick) {
	char temp[64];
	
	strcpy(temp, nick);
	sprintf(nick, "%c\u200b%s", *temp, temp + 1);
	
	return nick;
}

char *anti_hl_alloc(char *nick) {
	char *newnick;
	
	if(!(newnick = (char *) malloc(sizeof(char) * strlen(nick) + 5)))
		return NULL;
	
	strcpy(newnick, nick);
	anti_hl(newnick);
	
	return newnick;
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

char *anti_hl_each_words(char *str, size_t len, charset_t charset) {
	char *stripped = NULL, *convert = NULL;
	int allocation, s;
	char *read, *write;
	iconv_data_t iconvme;
	
	/* \u200b is 3 bytes length, str is normally not long, just allocating 3x */
	allocation = ((len * 3) + 8);
	printf("[ ] hl-protect/eachwords: allocating: %d bytes (original: %zu)\n", allocation, len);
	
	stripped = (char *) malloc(sizeof(char) * allocation);
	bzero(stripped, allocation);
	
	/* Checking if string is probably an iso string */
	if(charset != UNKNOWN_CHARSET && charset != UTF_8) {
		convert = (char *) malloc(sizeof(char) * allocation);
		
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
		
		if(!iconvit(&iconvme)) {
			free(convert);
			free(stripped);
			return NULL;
		}
			
		read = convert;

	} else read = str;
	
	write = stripped;
	
	/* debug(read, strlen(read)); */
	
	/* escape first word */
	if(*read && *read != '&')
		anti_hl_append(&write, &read);
	
	while(*read) {
		// avoid infinite loop, skip wrong char
		if(!(s = chrcpy_utf8(write, read))) {
			read++;
			continue;
		}
			
		write += s;
		
		// if space, and next is valid, and next is not space (avoid ' - ')
		// skip if contains & (html entities)
		if(*read == ' ' && *(read + 1) && *(read + 1) != '&' && *(read + 2) && *(read + 2) != ' ') {
			// copy space
			*write = *read++;
			anti_hl_append(&write, &read);
			
		} else read += s;
		
		// printf("<%s>\n", stripped);
	}
	
	// debug(stripped, allocation);
	
	printf("[.] hl-protect/eachwords: <%s>\n", stripped);
	
	/* Freeing "convert", should be NULL if not converted */
	free(convert);
	
	return stripped;
}

char *ltrim(char *str) {
	while(isspace(*str))
		str++;
		
	return str;
}

char *rtrim(char *str) {
	char *back = str + strlen(str);
	
	if(!*str)
		return str;
		
	while(isspace(*--back));
		*(back + 1) = '\0';
		
	return str;
}

char *crlftrim(char *str) {
	char *keep = str;
	
	while(*str) {
		if(*str == '\r' || *str == '\n')
			*str = ' ';
		
		str++;
	}
	
	return keep;
}

void intswap(int *a, int *b) {
	int c = *b;
	
	*b = *a;
	*a = c;
}

char *time_elapsed(time_t time) {
	char *output = (char *) malloc(sizeof(char) * 64);
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

char *irc_mstrncpy(char *src, size_t len) {
	char *str;
	
	if(!(str = (char *) malloc(sizeof(char) * len + 1)))
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

char *string_index(char *str, unsigned int index) {
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

char *skip_header(char *data) {
	char *match;
	
	if((match = strchr(data + 1, ':')))
		return match + 1;
		
	else return NULL;
}

char *space_encode(char *str) {
	char *new;
	
	for(new = str; *new; new++)
		if(*new == ' ')
			*new = '+';
	
	return str;
}

char *url_encode(char *url) {
	char *temp, *swap;
	
	if(!(temp = (char *) calloc(strlen(url) * 4, sizeof(char))))
		return NULL;
	
	swap = temp;
	
	while(*url) {
		if(*url == ' ') {
			*swap++ = '+';
		
		} else if(*url == '+') {
			strncpy(swap, "%2B", 3);
			swap += 3;
		
		} else if(*url == '=') {
			strncpy(swap, "%3D", 3);
			swap += 3;
			
		} else if(*url == '?') {
			strncpy(swap, "%3F", 3);
			swap += 3;
		
		} else *swap++ = *url;
		
		url++;
	}
	
	return temp;
}

int progression_match(size_t value) {
	size_t match = 250;
	
	while(match < value && match < 10000)
		match *= 2;
	
	if(match == value)
		return 1;
		
	return !(value % 20000);
}

size_t words_count(char *str) {
	size_t words = 0;
	
	// empty string
	if(!*str)
		return 0;
	
	while(*str) {
		if(isspace(*str) && !isspace(*(str + 1)))
			words++;
		
		str++;
	}
	
	// last char was not a space
	if(!isspace(*(str - 1)))
		words++;
	
	return words;
}

time_t today() {
	time_t now;
	struct tm *timeinfo;
	
	time(&now);
	timeinfo = localtime(&now);
	
	timeinfo->tm_sec  = 0;
	timeinfo->tm_min  = 0;
	timeinfo->tm_hour = 0;
	
	return mktime(timeinfo);
}

char *list_nick_implode(list_t *list) {
	char *implode = NULL;
	char buffer[128];
	size_t length = 0;
	
	if(!list->length)
		return strdup(" ");
	
	/* allocating string and re-iterate list */
	implode = (char *) calloc(1, sizeof(char));
	
	list_foreach(list, node) {
		implode = (char *) realloc(implode, length + strlen(node->name) + 3);
		
		// appending anti-hled nick
		zsnprintf(buffer, "%s", node->name);
		anti_hl(buffer);
		
		strcat(implode, buffer);
		strcat(implode, ", ");
		
		length = strlen(implode);
		node = node->next;
	}
	
	// removing last coma
	*(implode + length - 2) = '\0';
	
	return implode;
}

char *md5_ascii(char *source) {
	unsigned char result[MD5_DIGEST_LENGTH];
	const unsigned char *str;
	char *output, tmp[3];
	int i;
	
	printf("[+] ircmisc/md5: hashing <%s>\n", source);
	
	str = (unsigned char *) source;
	
	MD5(str, strlen(source), result);
	
	output = (char *) calloc(sizeof(char), (MD5_DIGEST_LENGTH * 2) + 1);
	
	for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
		sprintf(tmp, "%02x", result[i]);
		strcat(output, tmp);
	}
	
	return output;
}

char *sha1_string(unsigned char *sha1_hexa, char *sha1_char) {
	char sha1_hex[3] = {0};
	unsigned int i;
	
	*sha1_char = '\0';
	
	for(i = 0; i < SHA_DIGEST_LENGTH; i++) {
		sprintf(sha1_hex, "%02x", sha1_hexa[i]);
		strcat(sha1_char, sha1_hex);
	}
	
	return sha1_char;
}

int file_write(const char *filename, char *buffer, size_t length) {
	FILE *fp;
	size_t written;
	
	if(!(fp = fopen(filename, "w"))) {
		perror(filename);
		return 0;
	}
	
	if((written = fwrite(buffer, 1, length, fp)) != length)
		perror("[-] fwrite");
	
	fclose(fp);
	
	return written;
}
