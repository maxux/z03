/* z03 - small bot with some network features
 * Author: Daniel Maxime (root@maxux.net)
 * Contributor: Darky, mortecouille, RaphaelJ, somic, ghilan
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>
#include <dlfcn.h>
#include <signal.h>
#include <setjmp.h>
#include <execinfo.h>
#include <sys/mman.h>
#include <pthread.h>
#include "../common/bot.h"
#include "init.h"

ssl_socket_t *ssl;

global_core_t *global_core;
jmp_buf segfault_env;
char lastchan[32] = "Maxux";

void diep(char *str) {
	perror(str);
	exit(EXIT_FAILURE);
}

int signal_intercept(int signal, void (*function)(int)) {
	struct sigaction sig;
	int ret;
	
	/* Building empty signal set */
	sigemptyset(&sig.sa_mask);
	
	/* Building Signal */
	if(signal == SIGCHLD) {
		sig.sa_handler = SIG_IGN;
		sig.sa_flags   = SA_NOCLDWAIT;
	
	} else {
		sig.sa_handler = function;
		sig.sa_flags   = 0;
	}
	
	/* Installing Signal */
	if((ret = sigaction(signal, &sig, NULL)) == -1)
		perror("sigaction");
	
	return ret;
}

void sighandler(int signal) {
	void * buffer[1024];
	char raw[1024];
	int calls;
	
	switch(signal) {
		case SIGSEGV:
			fprintf(stderr, "[-] --- Segmentation fault ---\n");
			calls = backtrace(buffer, sizeof(buffer) / sizeof(void *));
			backtrace_symbols_fd(buffer, calls, 1);
	
			sprintf(raw, "PRIVMSG %s :[System] Warning: segmentation fault, reloading.", lastchan);
			raw_socket(raw);
			
			fprintf(stderr, "[-] ---    Rolling back    ---\n");
			
			sleep(5);
			
			siglongjmp(segfault_env, 1);
		break;
	}
}

void loadlib(codemap_t *codemap) {
	char *error;
	
	/* Unloading previously loaded lib */
	if(codemap->handler) {
		printf("[+] Core: destructing...\n");
		codemap->destruct();
		
		printf("[+] Core: unlinking library...\n");
		if(dlclose(codemap->handler)) {
			fprintf(stderr, "[-] Core: %s\n", dlerror());
			return;
		}
			
		codemap->handler = NULL;
		codemap->main    = NULL;
	}
	
	/* Loading new library */
	printf("[+] Core: linking library...\n");
	
	codemap->handler = dlopen(codemap->filename, RTLD_NOW);
	if(!codemap->handler) {
		fprintf(stdout, "[-] Core: dlopen: %s\n", dlerror());
		exit(EXIT_FAILURE);
	}
	
	/* Linking main */
	codemap->main = dlsym(codemap->handler, "main_core");
	if((error = dlerror()) != NULL) {
		fprintf(stderr, "[-] Core: dlsym: %s\n", error);
		exit(EXIT_FAILURE);
	}
	
	/* Linking destruct */
	codemap->construct = dlsym(codemap->handler, "main_construct");
	if((error = dlerror()) != NULL) {
		fprintf(stderr, "[-] Core: dlsym: %s\n", error);
		exit(EXIT_FAILURE);
	}
	
	/* Linking destruct */
	codemap->destruct = dlsym(codemap->handler, "main_destruct");
	if((error = dlerror()) != NULL) {
		fprintf(stderr, "[-] Core: dlsym: %s\n", error);
		exit(EXIT_FAILURE);
	}
	
	global_core->rehash_time = time(NULL);
	global_core->rehash_count++;
	
	printf("[+] Core: calling constructor...\n");
	codemap->construct();
	
	printf("[+] Core: link ready\n");
}

void core_handle_private_message(char *data, codemap_t *codemap) {
	char remote[256], *request;
	char *diff = NULL;
	unsigned char length;
	
	if((diff = strstr(data, "PRIVMSG"))) {
		length = diff - data - 1;
		
		strncpy(remote, data, length);
		remote[length] = '\0';
		
	} else return;
	
	if(!strcmp(remote, IRC_ADMIN_HOST)) {
		request = strstr(data, ":");
		
		if(request++) {
			printf("[+] Admin <%s> request: <%s>\n", remote, request);
			
			if(!strncmp(request, ".rehash", 7)) {
				printf("[+] Core: rehashing code...\n");
				loadlib(codemap);
				
			} else if(request[0] != '.')
				raw_socket(request);
		}
		
	} else printf("[-] Host <%s> is not admin\n", remote);
}

/*
 * IRC Protocol
 */
char *skip_server(char *data) {
	int i, j;
	
	j = strlen(data);
	for(i = 0; i < j; i++)
		if(*(data+i) == ' ')
			return data + i + 1;
	
	return NULL;
}

int init_socket(char *server, int port) {
	int fd = -1, connresult;
	struct sockaddr_in server_addr;
	struct hostent *he;
	struct timeval tv = {tv.tv_sec = SOCKET_TIMEOUT, tv.tv_usec = 0};
	
	/* Resolving name */
	if((he = gethostbyname(server)) == NULL)
		perror("[-] socket: gethostbyname");
	
	bcopy(he->h_addr, &server_addr.sin_addr, he->h_length);

	server_addr.sin_family = AF_INET; 
	server_addr.sin_port = htons(port);

	/* Creating Socket */
	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("[-] socket: socket");
		return -1;
	}

	/* Init Connection */
	if((connresult = connect(fd, (struct sockaddr *) &server_addr, sizeof(server_addr))) < 0) {
		perror("[-] socket: connect");
		close(fd);
		return -1;
	}
	
	return fd;
}

void raw_socket(char *message) {
	char *sending = (char *) malloc(sizeof(char *) * strlen(message) + 3);
	
	printf("[+] IRC: << %s\n", message);
	
	strcpy(sending, message);
	strcat(sending, "\r\n");
	
	if(ssl_write(ssl, sending) == -1)
		perror("[-] IRC: send");
	
	free(sending);
}

int remain_client() {
	int back;
	
	pthread_mutex_lock(&global_core->mutex_client);
	back = global_core->extraclient;
	pthread_mutex_unlock(&global_core->mutex_client);
	
	return back;
}

int read_socket(ssl_socket_t *ssl, char *data, char *next) {
	char buff[MAXBUFF];
	int rlen, i, tlen;
	char *temp = NULL;
	
	buff[0] = '\0';		// Be sure that buff is empty
	data[0] = '\0';		// Be sure that data is empty	
	
	while(1)  {
		free(temp);
		temp = (char *) malloc(sizeof(char *) * (strlen(next) + MAXBUFF));
		
		tlen = sprintf(temp, "%s%s", next, buff);
		
		for(i = 0; i < tlen; i++) {			// Checking if next (+buff), there is not \r\n
			if(temp[i] == '\r' && temp[i+1] == '\n') {
				strncpy(data, temp, i);		// Saving Current Data
				data[i] = '\0';
				
				if(temp[i+2] != '\0' && temp[i+1] != '\0' && temp[i] != '\0') {		// If the paquet is not finished, saving the rest
					strncpy(next, temp+i+2, tlen - i);
					
				} else next[0] = '\0';
				
				free(temp);
				return 0;
			}
		}
		
		if((rlen = ssl_read(ssl, buff, MAXBUFF)) >= 0) {
			if(rlen == 0)
				continue;
			/* // ssl_error();
				printf("[ ] Core: Warning: nothing read from socket\n");
				diep("[-] core: recv");
			} */
				
			buff[rlen] = '\0';
			
		} else if(errno != EAGAIN)
			ssl_error();
	}
	
	return 0;
}

int main(void) {
	char *data = (char *) calloc(sizeof(char), (2 * MAXBUFF));
	char *next = (char *) calloc(sizeof(char), (2 * MAXBUFF));
	char *request;
	codemap_t codemap = {
		.filename = "./libz03.so",
		.handler  = NULL,
		.main     = NULL,
		.destruct = NULL,
	};
	
	printf("[+] core: loading...\n");
	
	/* Init random */
	srand(time(NULL));
	
	/* Initializing global variables */
	/* mmap and mutex */
	global_core = mmap(NULL, sizeof(global_core_t), PROT_READ | PROT_WRITE,
	                   MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	
	if(!global_core)
		diep("mmap");
	
	// init mutex
	pthread_mutex_init(&global_core->mutex_ssl, NULL);
	pthread_mutex_init(&global_core->mutex_client, NULL);
	
	// settings variable
	global_core->startup_time = time(NULL);
	global_core->rehash_count = 0;
	global_core->auth         = 0;
	global_core->extraclient  = 0;
	
	/* signals */
	signal_intercept(SIGSEGV, sighandler);
	signal_intercept(SIGCHLD, sighandler);
	signal_intercept(SIGPIPE, sighandler);
	
	/* Loading dynamic code */
	loadlib(&codemap);
	
	printf("[+] core: connecting...\n");
	
	// connect the basic socket
	if((global_core->sockfd = init_socket(IRC_SERVER, IRC_PORT)) < 0) {
		fprintf(stderr, "[-] core: cannot create socket\n");
		exit(EXIT_FAILURE);
	}
	
	// enable ssl layer
	if(IRC_USE_SSL) {
		printf("[+] core: init ssl layer\n");
		if(!(ssl = init_socket_ssl(global_core->sockfd, &global_core->ssl))) {
			fprintf(stderr, "[-] core: cannot link ssl to socket\n");
			exit(EXIT_FAILURE);
		}
		
	} else printf("[-] core: ssl layer disabled\n");
	
	printf("[+] core: connected\n");
	
	while(1) {
		/* Reloading lib on segmentation fault */
		if(sigsetjmp(segfault_env, 1) == 1) {
			loadlib(&codemap);
			continue;
		}
		
		read_socket(ssl, data, next);
		printf("[ ] IRC: >> %s\n", data);
		
		if((request = skip_server(data)) == NULL) {
			printf("[-] IRC: Something wrong with protocol...\n");
			continue;
		}
		
		if(!strncmp(request, "PRIVMSG " IRC_NICK, sizeof("PRIVMSG " IRC_NICK) - 1))
			core_handle_private_message(data + 1, &codemap);
		
		if(!global_core->auth && !strncmp(request, "NOTICE AUTH", 11)) {
			raw_socket("NICK " IRC_NICK);
			raw_socket("USER " IRC_USERNAME " " IRC_USERNAME " " IRC_USERNAME " :" IRC_REALNAME);
			
			global_core->auth = 1;
			continue;
		}
		
		codemap.main(data, request);
	}
	 
	free(data);
	free(next);
	
	return 0;
}
