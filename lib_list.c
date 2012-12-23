#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib_list.h"

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
