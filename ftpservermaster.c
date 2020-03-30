#include "csapp.h"
#include <stdio.h>
#include <string.h>

#define NB_SLAVE 5
#define MAX_NAME_LEN 256
#define PORT 2121
#define TAILLE_PORT 4
#define MAX_FILE_SIZE 1000

void send_slave_infos(int, const char**, int);

void transmit_to_slaves(char*, const char**);

void send_file_to_slaves(int, char*, const char**);

void send_file(int, int, char*);

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

        /* determine the name of the client */
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAX_NAME_LEN, 0, 0, 0);

        /* determine the textual representation of the client's IP address */
        Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string, INET_ADDRSTRLEN);

		printf("adresse ip client : |%s|\n", client_ip_string);

		//On doit déterminer s'il s'agit d'un client ou d'un retour d'un des esclaves

		char buffer[MAX_NAME_LEN + 1];
		
		ssize_t size_rec = recv(connfd, buffer, MAX_NAME_LEN, 0);
		buffer[size_rec] = '\0';

		printf("buffer : |%s|\n", buffer);
		
		if (strcmp(buffer, "0") == 0) {
			//Convention qui nous indique qu'il s'agit d'un client
			last_slave = (last_slave + 1) % 5;
			send_slave_infos(connfd, ip_addresses, last_slave);
		} else {
			/*
				Si on a reçu autre chose il s'agit d'un des esclaves qui nous envoie
				des instructions a effectuer
			*/
			if (strncmp(&(buffer[TAILLE_PORT + 1]), "mkdir", 5) == 0 ||
				strncmp(&(buffer[TAILLE_PORT + 1]), "rm", 2) == 0) {
				
				/*
					Les esclaves doivent supprimer/créer le dossier/fichier donc on
					leur envoie l'information
				*/

				transmit_to_slaves(buffer, ip_addresses);
			} else if (strncmp(&(buffer[TAILLE_PORT + 1]), "put", 3) == 0) {
				/*
					On doit lire le fichier ajouté sur l'esclave courant et
					l'ajouter sur tous les autres esclaves
				*/

				send_file_to_slaves(connfd, buffer, ip_addresses);
			}
		}

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

void transmit_to_slaves(char* buffer, const char** ip_addresses) {
	if (Fork() == 0) {
		//Pour ne pas bloquer le maitre on fork et on effectue le travail dans le fils

		char port_esclave[5];
		memcpy(port_esclave, buffer, 4);
		port_esclave[4] = '\0';

		printf("buffer size : %li\n", strlen(buffer));

		for (int i = 0; i < NB_SLAVE; i++) {
			const char* slave = ip_addresses[i];
	
			if (strcmp(port_esclave, &(slave[strlen(slave) - 4])) != 0) {
				/*
					Si on est sur un autre esclave que celui qui nous a
					envoyé l'info, on transmet
				*/
				
				int j = 0;

				while (slave[j] != ':') {
					j++;
				}

				char ip[MAX_NAME_LEN];
				int port;

				memcpy(ip, slave, j);
				sscanf(&(slave[j + 1]), "%i", &port);
				ip[j] = '\0';

				/*
					On remplace le port reçu par notre port dans le buffer
					afin de notifier l'esclave qu'on est le maitre et pas
					un autre client, sinon on tourne en rond : le maitre envoie
					la demande a l'esclave qui la renvoie au maitre, etc.
				*/

				char nbuf[strlen(buffer) + 1];
				memcpy(nbuf, "2121:", TAILLE_PORT + 1);
				memcpy(&(nbuf[5]), &(buffer[TAILLE_PORT + 1]), strlen(&(buffer[TAILLE_PORT + 1]))), 
				nbuf[strlen(buffer)] = '\0';

				//On établit la connexion

				int clientfd = Open_clientfd(ip, port);

				//On envoie la commande

				printf("\nbuffer envoyé aux slaves : |%s|\n\n", nbuf);

				Rio_writen(clientfd, nbuf, strlen(nbuf));

				//On ferme proprement le fils
			
				close(clientfd);
			}
		}
	}
}

void send_file_to_slaves(int slave_fd, char* buffer, const char** ip_addresses) {
	if (Fork() == 0) {
		char port_esclave[5];
		memcpy(port_esclave, buffer, 4);
		port_esclave[4] = '\0';

		for (int i = 0; i < NB_SLAVE; i++) {
			const char* slave = ip_addresses[i];
	
			if (strcmp(port_esclave, &(slave[strlen(slave) - 4])) != 0) {
				//On notifie l'esclave détenteur du fichier qu'on est pret a le recevoir
				
				send(slave_fd, "0", 1, 0);
				
				/*
					Si on est sur un autre esclave que celui qui nous a
					envoyé l'info, on transmet
				*/
				
				int j = 0;

				while (slave[j] != ':') {
					j++;
				}

				char ip[MAX_NAME_LEN];
				int port;

				memcpy(ip, slave, j);
				sscanf(&(slave[j + 1]), "%i", &port);
				ip[j] = '\0';

				/*
					On remplace le port reçu par notre port dans le buffer
					afin de notifier l'esclave qu'on est le maitre et pas
					un autre client, sinon on tourne en rond : le maitre envoie
					la demande a l'esclave qui la renvoie au maitre, etc.
				*/

				char nbuf[strlen(buffer) + 1];
				memcpy(nbuf, "2121:", TAILLE_PORT + 1);
				memcpy(&(nbuf[5]), &(buffer[TAILLE_PORT + 1]), strlen(&(buffer[TAILLE_PORT + 1]))), 
				nbuf[strlen(buffer)] = '\0';

				//On établit la connexion

				int clientfd = Open_clientfd(ip, port);

				//On envoie la commande

				Rio_writen(clientfd, nbuf, strlen(nbuf));
				
				//On envoie le fichier a l'esclave courant

				send_file(slave_fd, clientfd, nbuf);

				//On ferme proprement le fils
		
				printf("dans le maitre on ferme\n");

				close(clientfd);

				//On renvoie un byte indiquant a l'esclave s'il reste ou non des esclaves a traiter
					
				send(slave_fd, "1", 1, 0);
			}
		}

		/*
			Une fois qu'on a fait tous les esclaves, on renvoie a l'esclave un bit a 0
			pour lui indiquer d'arreter l'envoi du fichier
		*/

		send(slave_fd, "0", 1, 0);
		close(slave_fd);
	}

	close(slave_fd);
}

void send_file(int slave_fd, int descriptor, char* command) {
	//On attend que l'esclave actuel soit pret a recevoir la donnée
	char ready[2];
	recv(descriptor, ready, 1, 0);
	ready[1] = '\0';

	while (strcmp(ready, "0") != 0) {
		recv(descriptor, ready, 1, 0);
	}

	long long size_tot = -1;
	unsigned long size_rec_tot = 0;

	while (size_rec_tot != size_tot) {
		/*
			On lit nos paquets
		*/

		char buf[MAX_FILE_SIZE + 1];
		ssize_t size_rec = recv(slave_fd, buf, MAX_FILE_SIZE, 0);
		buf[MAX_FILE_SIZE] = '\0';

		if (size_tot == -1) {
			/*
				Première itération, on doit récupérer la taille du fichier
				contenue dans le premier paquet reçu
			*/

			//On récupère l'indice du '\n' situé juste après la fin de la taille

			unsigned int i = 0; 
			while (i < size_rec && buf[i] != '\n') {
				i++;
			}

			//On récupère notre char* taille et on le transforme en int

			char s[i + 1]; 
			memcpy(s, buf, i);
			s[i] = '\0';

			sscanf(s, "%lli", &size_tot);

			/*
				Comme on n'écrit pas le fichier, et qu'on fait uniquement un
				transfert, on peut considérer que sa taille totale comprend sa
				taille réelle et le caractère '\n' (informations contenues au 
				début du premier paquet)
			*/

			size_tot = size_tot + i + 1;
		}

		ssize_t size_sent = send(descriptor, buf, size_rec, 0);

		if (size_sent != size_rec) {
			printf("Une erreur est survenue. Veuillez recommencer.\n");
			return;
		}

    	size_rec_tot += size_rec;
	}
}
