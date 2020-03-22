/*
 * echoserveri.c - An iterative echo server
 */

#include "csapp.h"
#include "unistd.h"

#define PORT 2121
#define MAX_NAME_LEN 256
#define NB_PROC 2
#define PROC_UTIL 0
#define PROC_INUTIL 1

typedef struct {
    pid_t proc_pid;
    unsigned int utilisable;
} Process;

FILE* get_file(char*);

long int get_file_size(FILE* f);

void init_processes(Process**);

void fill_buff(int, char*);

Process* get_usable_process(Process**);

void sigchild_handler(int sig) {
    int status;
    waitpid(-1, &status, WNOHANG | WUNTRACED);
}

/* 
 * Note that this code only works with IPv4 addresses
 * (IPv6 is not supported)
 */

int main(int argc, char **argv)
{
    int listenfd, connfd;
	int nb_proc_curr = 0;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[MAX_NAME_LEN];
	char buf[MAX_NAME_LEN + 1];
//    Process** processes = malloc(sizeof(void*) * NB_PROC);
  //  init_processes(processes);

	Signal(SIGCHLD, sigchild_handler);

    clientlen = (socklen_t) sizeof(clientaddr);

    listenfd = Open_listenfd(PORT);

    while (1) {
		connfd = Accept(listenfd, (SA*) &clientaddr, &clientlen);

		if (nb_proc_curr++ < NB_PROC && Fork() == 0) {
    	    /* determine the name of the client */
        	Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAX_NAME_LEN, 0, 0, 0);

       		/* determine the textual representation of the client's IP address */
       		Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string, INET_ADDRSTRLEN);

        	printf("server connected to %s (%s)\n", client_hostname, client_ip_string);

			fill_buff(connfd, buf);
			FILE* my_file = get_file(buf);

			if(my_file == NULL) {
				printf("le fichier demandé n'existe pas ou n'est pas accessible\n");
				exit(0);
			}

			long int file_size = get_file_size(my_file);

			printf("\ntaille fichier demandé : %li\n\n", file_size);

			char* contenu = malloc(sizeof(char*) * file_size);
        	
			fread(contenu, file_size, 1, my_file);

			ssize_t sent_size = send(connfd, contenu, file_size, 0);

			if (sent_size != file_size) {
				printf("Un problème est survenu lors de la transaction...\n");
			} else {
				printf("La transaction s'est effectuée sans problème\n");
			}

			close(connfd);
		}
    
		close(connfd);
	}

    exit(0);
}

void fill_buff(int descriptor, char* buf) {
	ssize_t taille = recv(descriptor, buf, MAX_NAME_LEN, 0);
	
	buf[taille] = '\0';
}

FILE* get_file(char* buf) {
	FILE* file;

	if (access(buf, F_OK) != -1) {
		file = fopen(buf, "r");
	} else {
		file = NULL;
	}

	return file;
}

long int get_file_size(FILE* f) {
	fseek(f, 0L, SEEK_END);
	
	long int size = ftell(f);

	rewind(f);

	return size;
}

Process* get_usable_process(Process** processes) {
    for (int i = 0; i < NB_PROC; i++) {
        if (processes[i]->utilisable == PROC_UTIL) {
            //On rend le process actuel inutilisable
            processes[i]->utilisable = PROC_INUTIL;
            return processes[i];
        }
    }

    return NULL;
}

void init_processes(Process** processes) {
    for (int i = 0; i < NB_PROC; i++) {
        processes[i] = malloc(sizeof(Process));

  //      processes[i]->proc_pid = Fork();
        processes[i]->utilisable = PROC_UTIL;
    }

	for (int i = 0; i < NB_PROC; i++) {
		pid_t pid = 0;

		if (pid == 0) break;
	
		processes[i]->proc_pid = pid;
		printf("%i : \n", pid);
	}
}
