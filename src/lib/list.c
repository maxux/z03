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
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "core.h"
#include "ircmisc.h"

list_t * list_init(void (*destruct)(void *)) {
	list_t *list;
	
	if(!(list = (list_t*) malloc(sizeof(list_t))))
		return NULL;
	
	list->destruct  = destruct;
	list->nodes     = NULL;
	list->length    = 0;
	
	return list;
}

void list_free(list_t *list) {
	list_node_t *node = list->nodes, *temp;
	
	/* Free resident data */
	while(node) {
		if(list->destruct)
			list->destruct(node->data);
			
		free(node->name);
		
		temp = node;
		node = node->next;
		
		free(temp);
	}
	
	free(list);
}

void * list_append(list_t *list, char *name, void *data) {
	list_node_t *node;
	
	if(!(node = (list_node_t*) malloc(sizeof(list_node_t))))
		return NULL;
	
	node->name = strdup(name);
	node->data = data;
	
	node->next  = list->nodes;	
	list->nodes = node;
	
	list->length++;
	
	return data;
}

void * list_search(list_t *list, char *name) {
	list_node_t *node = list->nodes;
	
	while(node && strcmp(node->name, name))
		node = node->next;
	
	return (node) ? node->data : NULL;
}

int list_remove(list_t *list, char *name) {
	list_node_t *node = list->nodes;
	list_node_t *prev = list->nodes;
	
	while(node && strcmp(node->name, name)) {
		prev = node;
		node = node->next;
	}
	
	if(!node)
		return 1;
	
	if(node == list->nodes)
		list->nodes = node->next;
		
	else prev->next = node->next;
	
	list->length--;
	
	/* Call user destructor */
	if(list->destruct)
		list->destruct(node->data);
	
	free(node->name);	
	free(node);
	
	return 0;
}

char *list_implode(list_t *list, size_t limit) {
	size_t alloc = 0, now = 0;
	list_node_t *node;
	char *implode;
	char buffer[128];
	
	/* compute the full list length */
	node = list->nodes;
		
	while(node) {
		alloc += strlen(node->name) + 8;
		node = node->next;
	}
	
	/* allocating string and re-iterate list */
	implode = (char *) calloc(sizeof(char), alloc);
	node = list->nodes;
	
	while(node && ++now) {
		// appending anti-hled nick
		zsnprintf(buffer, "%s", node->name);
		anti_hl(buffer);
		strcat(implode, buffer);
		
		if(now > limit) {
			zsnprintf(buffer, " and %zu others hidden", list->length - now);
			implode = (char *) realloc(implode, alloc + 64);
			strcat(implode, buffer);
			
			return implode;
			
		} else if(node->next)
			strcat(implode, ", ");
			
		node = node->next;
	}
	
	return implode;
}
