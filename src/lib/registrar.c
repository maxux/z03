#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define __LIB_CORE_C
#include "../core/init.h"
#include "list.h"
#include "core.h"

__attribute__ ((constructor (101)))
void main_construct() {
	printf("[+] lib: initializing...\n");

	global_lib.commands = list_init(NULL);	
	lib_construct();
}

__attribute__ ((destructor))
void main_destruct() {
	printf("[+] lib: destructing...\n");
	lib_destruct();
	
	list_free(global_lib.channels);
	list_free(global_lib.threads);
}

void request_register(request_t *request) {
	printf("[+] lib: registering: %s\n", request->match);
	list_append(global_lib.commands, request->match, request);
}
