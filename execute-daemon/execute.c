/* Simple TCP server, for Linux, multi-threaded, with local specific
 * address to bind. Echo service for clients.
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
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>

#define LISTEN_PORT	45871
#define LISTEN_ADDR	"0.0.0.0"
#define TARGET_FILE	"execute-code.template"
#define TARGET_FILE_HS	"execute-code-hs.template"
#define TARGET_FILE_PHP	"execute-code-php.template"
#define TARGET_FILE_PY	"execute-code-py.template"

typedef struct thread_data_t {
	int sockfd;
	struct sockaddr_in addr_client;
	pthread_t thread;
	
} thread_data_t;

int yes = 1;

void * working(void *thread);

void diep(char *str) {
	perror(str);
	exit(EXIT_FAILURE);
}

int signal_ignore(int signal) {
	struct sigaction sig;
	int ret;
	
	/* Building empty signal set */
	sigemptyset(&sig.sa_mask);
	
	/* Building Signal */
	sig.sa_handler = SIG_IGN;
	sig.sa_flags   = SA_NOCLDWAIT;
	
	/* Installing Signal */
	if((ret = sigaction(signal, &sig, NULL)) == -1)
		perror("sigaction");
	
	return ret;
}

int main(void) {
	int sockfd, new_fd;
	struct sockaddr_in addr_listen, addr_client;
	socklen_t addr_client_len;
	char *client_ip;
	thread_data_t *thread_data;
	
	srand(time(NULL));
	
	signal_ignore(SIGPIPE);
	signal_ignore(SIGCHLD);

	/* Creating Server Socket */
	addr_listen.sin_family      = AF_INET;
	addr_listen.sin_port        = htons(LISTEN_PORT);
	addr_listen.sin_addr.s_addr = inet_addr(LISTEN_ADDR);
	
	/* Init Client Socket Length */
	addr_client_len = sizeof(addr_client);
	
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		diep("[-] socket");

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		diep("[-] setsockopt");

	if(bind(sockfd, (struct sockaddr*) &addr_listen, sizeof(addr_listen)) == -1)
		diep("[-] bind");

	if(listen(sockfd, 32) == -1)
		diep("[-] listen");

	/* Socket ready, waiting clients */
	while(1) {
		printf("[+] Waiting new connection...\n");
		
		if((new_fd = accept(sockfd, (struct sockaddr *)&addr_client, &addr_client_len)) == -1)
			perror("[-] accept");

		client_ip = inet_ntoa(addr_client.sin_addr);
		printf("[+] Connection from %s\n", client_ip);
		
		/* Warning: memory leak */
		thread_data = (thread_data_t*) malloc(sizeof(thread_data_t));
		
		thread_data->sockfd      = new_fd;
		thread_data->addr_client = addr_client;
		
		/* Starting new thread with our new client */
		if(pthread_create(&thread_data->thread, NULL, working, (void *) thread_data))
			perror("[-] pthread_create");
		
		if(pthread_detach(thread_data->thread))
			perror("[-] pthread_detach");
	}

	return 0;
}

char *fileread_light(const char *file, char *buffer, size_t bufflen) {
	FILE *fp;
	size_t len;
	
	if(!(fp = fopen(file, "r"))) {
		perror("[-] fopen");
		return NULL;
	}
	
	if((len = fread(buffer, sizeof(char), bufflen, fp)) < 1) {
		perror("[-] fread");
		return NULL;
	}
	
	buffer[len] = '\0';
	fclose(fp);
	
	return buffer;
}

void execute_py(thread_data_t *thread_data, char *code) {
	FILE *cmd, *out;
	char buffer[4096];
	char cmdline[1024], tmpfile[128];
	
	/* Writing python file */
	if(!fileread_light(TARGET_FILE_PY, buffer, sizeof(buffer)))
		return;
	
	sprintf(tmpfile, "/tmp/z03.code-%d.py", rand());
	
	if(!(out = fopen(tmpfile, "w"))) {
		perror("[-] fopen");
		return;
	}
	
	fprintf(out, buffer, code);
	fclose(out);

	/* Compiling */
	sprintf(cmdline, "timeout 10 python %s 2>&1", tmpfile);
	printf("[+] Running: %s\n", cmdline);
	
	if(!(cmd = popen(cmdline, "r"))) {
		perror("[-] popen");
		return;
	}
	
	while(fgets(buffer, sizeof(buffer), cmd)) {
		printf("[*] %s", buffer);
		if(send(thread_data->sockfd, buffer, strlen(buffer), 0) < 0)
			break;
	}
	
	pclose(cmd);
	unlink(tmpfile);
}

void execute_hs(thread_data_t *thread_data, char *code) {
	FILE *out, *cmd;
	char buffer[4096];
	char tmpfile[128], cmdline[256];
	
	/* Writing haskell file */
	if(!fileread_light(TARGET_FILE_HS, buffer, sizeof(buffer)))
		return;
	
	sprintf(tmpfile, "/tmp/z03.code-%d.hs", rand());
	
	if(!(out = fopen(tmpfile, "w"))) {
		perror("[-] fopen");
		return;
	}
	
	fprintf(out, buffer, code);
	fclose(out);
	
	/* Executing */
	sprintf(cmdline, "timeout 10 runghc %s 2>&1", tmpfile);
	// sprintf(cmdline, "runghc %s %s 2>&1", code, tmpfile);
	printf("[+] Running: %s\n", cmdline);
	
	if(!(cmd = popen(cmdline, "r"))) {
		perror("[-] popen");
		return;
	}
	
	while(fgets(buffer, sizeof(buffer), cmd)) {
		printf("[*] %s", buffer);
		if(send(thread_data->sockfd, buffer, strlen(buffer), 0) < 0)
			break;
	}
	
	pclose(cmd);
	
	/* Cleaning */
	unlink(tmpfile);
}

void execute_php(thread_data_t *thread_data, char *code) {
	FILE *out, *cmd;
	char buffer[4096];
	char tmpfile[128], cmdline[256];
	
	/* Writing php file */
	if(!fileread_light(TARGET_FILE_PHP, buffer, sizeof(buffer)))
		return;
	
	sprintf(tmpfile, "/tmp/z03.code-%d.php", rand());
	
	if(!(out = fopen(tmpfile, "w"))) {
		perror("[-] fopen");
		return;
	}
	
	fprintf(out, buffer, code);
	fclose(out);
	
	/* Executing */
	sprintf(cmdline, "timeout 10 php %s 2>&1", tmpfile);
	printf("[+] Running: %s\n", cmdline);
	
	if(!(cmd = popen(cmdline, "r"))) {
		perror("[-] popen");
		return;
	}
	
	while(fgets(buffer, sizeof(buffer), cmd)) {
		printf("[*] %s", buffer);
		if(send(thread_data->sockfd, buffer, strlen(buffer), 0) < 0)
			break;
	}
	
	pclose(cmd);
	
	/* Cleaning */
	unlink(tmpfile);
}

void execute_c(thread_data_t *thread_data, char *code) {
	FILE *out, *cmd;
	char buffer[4096];
	char tmpfile[128], cmdline[256];
	
	/* Writing C file */
	if(!fileread_light(TARGET_FILE, buffer, sizeof(buffer)))
		return;
	
	sprintf(tmpfile, "/tmp/z03.code-%d.c", rand());
	
	if(!(out = fopen(tmpfile, "w"))) {
		perror("[-] fopen");
		return;
	}
	
	fprintf(out, buffer, code);
	fclose(out);
	
	/* Compiling */
	sprintf(cmdline, "gcc -W -Wall -O2 -lm -std=gnu99 %s -o %s.run 2>&1", tmpfile, tmpfile);
	printf("[+] Compiling: %s\n", cmdline);
	
	if(!(cmd = popen(cmdline, "r"))) {
		perror("[-] popen");
		return;
	}
	
	while(fgets(buffer, sizeof(buffer), cmd)) {
		printf("[*] %s", buffer);
		send(thread_data->sockfd, buffer, strlen(buffer), 0);
	}
	
	pclose(cmd);
	
	/* Checking */
	sprintf(cmdline, "%s.run", tmpfile);
	if(!(out = fopen(cmdline, "r")))
		return;
	
	fclose(out);
	
	/* Executing */
	sprintf(cmdline, "timeout 10 %s.run 2>&1", tmpfile);
	printf("[+] Running: %s\n", tmpfile);
	
	if(!(cmd = popen(cmdline, "r"))) {
		perror("[-] popen");
		return;
	}
	
	while(fgets(buffer, sizeof(buffer), cmd)) {
		printf("[*] %s", buffer);
		if(send(thread_data->sockfd, buffer, strlen(buffer), 0) < 0)
			break;
	}
	
	pclose(cmd);
	
	/* Cleaning */
	unlink(tmpfile);
	
	sprintf(cmdline, "%s.run", tmpfile);
	unlink(cmdline);
}

void * working(void *thread) {
	char buffer[1024];
	thread_data_t *thread_data = (thread_data_t*) thread;
	int length;
	
	printf("[+] Thread (fd %d) created\n", thread_data->sockfd);
	
	/* Echo Service */
	if((length = recv(thread_data->sockfd, buffer, sizeof(buffer), 0)) > 0) {
		buffer[length--] = '\0';
		
		while(buffer[length] == '\n' || buffer[length] == '\r')
			buffer[length--] = '\0';
		
		if(!strncmp(buffer, "PY ", 3)) {
			printf("[ ] Lang: Python\n");
			printf("[ ] Code: <%s>\n", buffer + 3);
			execute_py(thread_data, buffer + 3);
			
		} else if(!strncmp(buffer, "C ", 2)) {
			printf("[ ] Lang: C\n");
			printf("[ ] Code: <%s>\n", buffer + 2);
			execute_c(thread_data, buffer + 2);
		
		} else if(!strncmp(buffer, "HS ", 3)) {
			printf("[ ] Lang: Haskell\n");
			printf("[ ] Code: <%s>\n", buffer + 3);
			execute_hs(thread_data, buffer + 3);
		
		} else if(!strncmp(buffer, "PHP ", 4)) {
			printf("[ ] Lang: PHP\n");
			printf("[ ] Code: <%s>\n", buffer + 4);
			execute_php(thread_data, buffer + 4);
				
		} else printf("[-] Unknown lang: %s\n", buffer);
		
	} else printf("[-] Nothing to read\n");
	
	close(thread_data->sockfd);
		
	printf("[+] Thread (fd %d) closed\n", thread_data->sockfd);
	
	/* Clearing */
	free(thread);
	
	return 0;
}
