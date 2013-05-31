/* z03 - small bot with some network features - whois support
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
#include "../core/init.h"
#include "core.h"
#include "whois.h"
#include "ircmisc.h"

whois_t *whois_init() {
	whois_t *whois;
	
	whois = (whois_t*) malloc(sizeof(whois));
	
	whois->host  = NULL;
	whois->ip    = NULL;
	whois->modes = NULL;
	
	return whois;
}

void whois_free(whois_t *whois) {
	if(whois) {
		free(whois->host);
		free(whois->ip);
	}
	
	free(whois);
}

whois_t *irc_whois(char *nick) {
	char *data = (char *) malloc(sizeof(char *) * (2 * MAXBUFF));
	char *next = (char *) malloc(sizeof(char *) * (2 * MAXBUFF));
	char *request, temp[128];
	whois_t *whois;
	
	// Init
	*data = '\0';
	*next = '\0';
	if(!(whois = whois_init()))
		return NULL;
	
	snprintf(temp, sizeof(temp), "WHOIS %s", nick);
	raw_socket(temp);
	
	printf("[+] Misc: whois request: %s\n", nick);
	
	while(1) {
		read_socket(ssl, data, next);
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
