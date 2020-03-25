#include "csapp.h"
#include <stdio.h>
#include <string.h>

#define NB_SLAVE 5
#define MAX_NAME_LEN 256
#define PORT 2121

void send_slave_infos(int, const char**, int);

int main(int argc, char** argv) {
	//Doit connaitre les ip/ports des esclaves
	
	int listenfd, connfd;
	int last_slave = -1;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[MAX_NAME_LEN];
	
	//Ca sera notre tableau d'ip/ports pour l'instant avec NB_SLAVE elements

	const char* ip_addresses[5];
	ip_addresses[0] = "localhost:3000";
	ip_addresses[1] = "localhost:3001";
	ip_addresses[2] = "localhost:3002";
	ip_addresses[3] = "localhost:3003";
	ip_addresses[4] = "localhost:3004";

    clientlen = (socklen_t) sizeof(clientaddr);

    listenfd = Open_listenfd(PORT);

    while (1) {
        connfd = Accept(listenfd, (SA*) &clientaddr, &clientlen);

		last_slave = (last_slave + 1) % 5;

        /* determine the name of the client */
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAX_NAME_LEN, 0, 0, 0);

        /* determine the textual representation of the client's IP address */
        Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string, INET_ADDRSTRLEN);

		send_slave_infos(connfd, ip_addresses, last_slave);

        close(connfd);
    }

    exit(0);	
}

void send_slave_infos(int connfd, const char** ip_addresses, int last_slave) {
	const char* ip_address = ip_addresses[last_slave];

	int taille_transmise = send(connfd, ip_address, strlen(ip_address), 0);

	if (taille_transmise == -1) {
		printf("The data sending encountered an issue. Please try again.\n");
		return;
	}

	if (taille_transmise != strlen(ip_address)) {
		printf("Un problème est survenu lors de la transaction....\n");
	} else {
		printf("La transmission des informations de l'esclave s'est effectuée sans problème\n");
	}
}
