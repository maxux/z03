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
#include "bot.h"
#include "core_init.h"

ssl_socket_t *ssl;

global_core_t global_core;
jmp_buf segfault_env;

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
	int calls;

	switch(signal) {
		case SIGSEGV:
			fprintf(stderr, "[-] --- Segmentation fault ---\n");
			calls = backtrace(buffer, sizeof(buffer) / sizeof(void *));
			backtrace_symbols_fd(buffer, calls, 1);

			raw_socket("PRIVMSG " IRC_HARDCHAN " :[System] Warning: segmentation fault, reloading.");
			fprintf(stderr, "[-] ---    Rolling back    ---\n");

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
	
	global_core.rehash_time = time(NULL);
	global_core.rehash_count++;
	
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

ssl_socket_t *init_socket(char *server, int port) {
	int fd = -1, connresult;
	struct sockaddr_in server_addr;
	struct hostent *he;
	struct timeval tv = {tv.tv_sec = 30, tv.tv_usec = 0};
	
	/* Resolving name */
	if((he = gethostbyname(server)) == NULL)
		perror("[-] socket: gethostbyname");
	
	bcopy(he->h_addr, &server_addr.sin_addr, he->h_length);

	server_addr.sin_family = AF_INET; 
	server_addr.sin_port = htons(port);

	/* Creating Socket */
	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("[-] socket: socket");
		return NULL;
	}

	/* Init Connection */
	if((connresult = connect(fd, (struct sockaddr *) &server_addr, sizeof(server_addr))) < 0) {
		perror("[-] socket: connect");
		close(fd);
		return NULL;
	}
	
	if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(tv)))
		diep("setsockopt SO_RCVTIMEO");
	
	return ssl_init(fd);
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
			if(rlen == 0) {
				printf("[ ] Core: Warning: nothing read from socket\n");
				usleep(120000);
			}
				
			buff[rlen] = '\0';
			
		} else if(errno != EAGAIN)
			perror("[-] core: recv");
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
	global_core.startup_time = time(NULL);
	global_core.rehash_count = 0;
	global_core.auth         = 0;
	
	signal_intercept(SIGSEGV, sighandler);
	signal_intercept(SIGCHLD, sighandler);
	
	/* Loading dynamic code */
	loadlib(&codemap);
	
	printf("[+] core: connecting...\n");

	if(!(ssl = init_socket(IRC_SERVER, IRC_PORT))) {
		fprintf(stderr, "[-] core: cannot create socket\n");
		exit(EXIT_FAILURE);
	}
	
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
		
		if(!global_core.auth && !strncmp(request, "NOTICE AUTH", 11)) {
			raw_socket("NICK " IRC_NICK);
			raw_socket("USER " IRC_USERNAME " " IRC_USERNAME " " IRC_USERNAME " :" IRC_REALNAME);
			
			global_core.auth = 1;
			continue;
		}
		
		codemap.main(data, request);
	}
	 
	free(data);
	free(next);
	
	return 0;
}
