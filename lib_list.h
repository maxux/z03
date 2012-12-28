#ifndef __LIB_LIST_H
	#define __LIB_LIST_H
	
	typedef struct list_node_t {
		char *name;
		void *data;
		struct list_node_t *next;
		
	} list_node_t;

	typedef struct list_t {
		void (*destruct)(void *data);
		struct list_node_t *nodes;
		unsigned int length;
		
	} list_t;
	
	list_t * list_init(void (*destruct)(void *));
	
	void * list_append(list_t *list, char *name, void *data);
	void * list_search(list_t *list, char *name);
	
	int list_remove(list_t *list, char *name);
	void list_free(list_t *list);
#endif
