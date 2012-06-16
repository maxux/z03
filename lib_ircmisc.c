/* z03 - small bot with some network features - irc miscallinious functions (anti highlights, ...)
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
#include <ctype.h>
#include "lib_ircmisc.h"

inline short utf8_charlen(unsigned char c) {
	if(c < 0x80)
		return 1;         /* 0xxxxxxx */
		
	if((c & 0xe0) == 0xc0)
		return 2;         /* 110xxxxx */
		
	if((c & 0xf0) == 0xe0)
		return 3;         /* 1110xxxx */
		
	if((c & 0xf8)==0xf0 && (c <= 0xf4))
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
	sprintf(nick, "%c\ufeff%s", *temp, temp + 1);
	
	return nick;
}

char * anti_hl_each_words(char *str, size_t len) {
	char *stripped = NULL, *useme = str;
	int i = 0, allocation;
	char *read, *write;
	int s;
	
	while(*useme) {
		if(*(useme++) == ' ')
			i++;
	}
	
	printf("[.] AntiHL: Found %d spaces occurence\n", i);
	
	allocation = (len + (i * 3)) + 8;
	printf("[.] AntiHL: Allocating: %u bytes (original: %u)\n", allocation, len);
	
	/* \ufeff is 3 bytes length */
	stripped  = (char*) malloc(sizeof(char) * allocation);
	bzero(stripped, allocation);
	
	read  = str;
	write = stripped;
	
	// debug(read, strlen(read));
	
	while(*read) {
		s = chrcpy_utf8(write, read);
		write += s;
			
		if(*read == ' ' && *(read + 1)) {
			// Copy Space
			*write = *read++;
			
			// Copy next char
			s = chrcpy_utf8(write, read);
			write += s;
			read  += s;
			
			// Copy special char (3 bytes)
			sprintf(write, "\ufeff");
			write += 3;
			
		} else read += s;
		
		// printf("<%s>\n", stripped);
	}
	
	// debug(stripped, allocation);
	
	printf("[.] AntiHL: <%s>\n", stripped);
	
	return stripped;
}

char * trim(char *str, unsigned int len) {
	char *read, *write;
	unsigned int i;
	
	read  = str;
	write = str;
	
	/* Strip \n */
	for(i = 0; i < len; i++) {
		if(*read != '\n') {
			*write = *read;
			write++;
		}
		
		read++;
	}
	
	/* New line limit */
	*write = '\0';
	
	/* Trim spaces before/after */
	read  = str;
	write = str;
	
	/* Before */
	while(isspace(*read))
		read++;
	
	/* Copy */
	while(*read)
		*write++ = *read++;
	
	/* After */
	while(isspace(*write))
		write--;
		
	*write = '\0';
	
	/* Removing double spaces */
	while((read = strstr(str, "  ")))
		strcpy(read, read + 1);
	
	return str;
}
