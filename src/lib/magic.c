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
#include <string.h>
#include <magic.h>
#include "magic.h"

static char *magic_error_close(magic_t *magic_cookie) {
	magic_close(*magic_cookie);
	return NULL;
}
	
char *magic(char *buffer, size_t length) {
	const char *magic_full;
	char *str;
	magic_t magic_cookie;
	
	if(!(magic_cookie = magic_open(MAGIC_NONE)))
		return NULL;
	
	if(magic_load(magic_cookie, NULL) != 0) {
		fprintf(stderr, "[-] lib/magic: %s\n", magic_error(magic_cookie));
		return magic_error_close(&magic_cookie);
	}
	
	if(!(magic_full = magic_buffer(magic_cookie, buffer, length)))
		return magic_error_close(&magic_cookie);
	
	// duplicate string, closing magic stuff
	str = strdup(magic_full);	
	magic_close(magic_cookie);
	
	return str;
}
