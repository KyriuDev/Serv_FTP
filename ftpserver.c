/*
 * echoserveri.c - An iterative echo server
 */

#include "csapp.h"

#define MAX_NAME_LEN 256
#define NB_PROC 2
#define PROC_UTIL 0
#define PROC_INUTIL 1

typedef struct {
    pid_t proc_pid;
    unsigned int utilisable;
} Process;

void echo(int connfd);

void init_processes(Process**);

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
    int listenfd, connfd, port;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[MAX_NAME_LEN];
    Process** processes = malloc(sizeof(void*) * NB_PROC);
    init_processes(processes);

	Signal(SIGCHLD, sigchild_handler);

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    port = atoi(argv[1]);

    clientlen = (socklen_t) sizeof(clientaddr);

    listenfd = Open_listenfd(port);

	Process* curr_proc;

    while (1) {
    	if ((curr_proc = get_usable_process(processes)) != NULL && curr_proc->proc_pid > 0) {
	
		connfd = Accept(listenfd, (SA*) &clientaddr, &clientlen);

        /* determine the name of the client */
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAX_NAME_LEN, 0, 0, 0);

        /* determine the textual representation of the client's IP address */
        Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string, INET_ADDRSTRLEN);

        printf("server connected to %s (%s)\n", client_hostname, client_ip_string);

        echo(connfd);
        Close(connfd);
		}
    }

    exit(0);
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
		pid_t pid = Fork();

		if (pid == 0) break;
	
		processes[i]->proc_pid = pid;
		printf("%i : \n", pid);
	}
}

