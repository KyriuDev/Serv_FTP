#include "csapp.h"
#include <stdio.h>
#include <string.h>

#define NB_SLAVE 5
#define MAX_NAME_LEN 256
#define PORT 2121
#define TAILLE_PORT 4
#define MAX_PACK_SIZE 1000

void send_slave_infos(int, const char**, int);

void transmit_to_slaves(char*, const char**);

void send_file_to_slaves(int, char*, const char**);

void send_file(int, int, char*);

void fill_ip_addresses(const char**);

int main(int argc, char** argv) {
	//Doit connaitre les ip/ports des esclaves
	
	int listenfd, connfd;
	int last_slave = -1;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[MAX_NAME_LEN];
	
	//Ca sera notre tableau d'ip/ports pour l'instant avec NB_SLAVE elements

	const char* ip_addresses[NB_SLAVE];
	fill_ip_addresses(ip_addresses);

    clientlen = (socklen_t) sizeof(clientaddr);

    listenfd = Open_listenfd(PORT);

    while (1) {
        connfd = Accept(listenfd, (SA*) &clientaddr, &clientlen);

        /* determine the name of the client */
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAX_NAME_LEN, 0, 0, 0);

        /* determine the textual representation of the client's IP address */
        Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string, INET_ADDRSTRLEN);

		//On doit déterminer s'il s'agit d'un client ou d'un retour d'un des esclaves

		char buffer[MAX_NAME_LEN + 1];
		
		ssize_t size_rec = recv(connfd, buffer, MAX_NAME_LEN, 0);
		buffer[size_rec] = '\0';

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
		unsigned int file_sent_correctly = 0;

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

				//On attend que le client nous indique qu'il est pret

				char ready[2];
				recv(clientfd, ready, 1, 0);
				ready[1] = '\0';

				while (strcmp(ready, "") == 0) {
					recv(clientfd, ready, 1, 0);
				}

				//On envoie le fichier a l'esclave courant

				send_file(slave_fd, clientfd, nbuf);
				
				//On vérifie que la transmission avec l'esclave courant s'est bien passée

				char done[2];
				recv(clientfd, done, 1, 0);
				done[1] = '\0';

				if (strcmp(done, "0") != 0) {
					file_sent_correctly = 1;
				}

				//On ferme proprement le fils
		
				close(clientfd);
			}
		}

		/*
			Une fois qu'on a fait tous les esclaves, on renvoie a l'esclave un bit a 0
			pour lui indiquer d'arreter l'envoi du fichier
		*/

		if (file_sent_correctly == 0) {
			printf("Le fichier a bien été transmis à tous les esclaves.\n");
		} else {
			printf("Au moins un des esclaves a rencontré une erreur lors de la récupération du fichier.\n");
		}

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

		char buf[MAX_PACK_SIZE + 1];
		ssize_t size_rec = recv(slave_fd, buf, MAX_PACK_SIZE, 0);
		buf[MAX_PACK_SIZE] = '\0';

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

void fill_ip_addresses(const char** ip_addresses) {
	char* filename = ".slaveinf.txt";
	
	//On vérifie si notre fichier de configuration existe

	if (access(filename, F_OK) != -1) {
		//Le fichier existe donc on le lit
		FILE* f = fopen(filename, "r");
		char* line = NULL;
		size_t line_size = 0;
		ssize_t size;
		unsigned int nb_line = 0;

		/*
			On met toutes les lignes lues dans notre table d'adresses
			Si on en lit plus ou moins que le nombre d'esclaves prévu,
			on print un message d'erreur (bloquant ou non)
		*/
		
		while ((size = getline(&line, &line_size, f)) > 0) {
			if (nb_line < NB_SLAVE) {
				line[size - 1] = '\0';
				ip_addresses[nb_line] = line;
				nb_line++;
				line = NULL;
				line_size = 0;
			} else {
				nb_line++;
				break;
			}
		}

		if (nb_line < NB_SLAVE) {
			//Il manque des informations sur certains esclaves : on ne peut pas continuer
			printf("Certaines données nécessaires au bon fonctionnement du serveur n'ont pas pu être lues. Le serveur va se fermer\n");
			exit(1);
		} else if (nb_line > NB_SLAVE){
			printf("Le fichier de configuration plus d'IP que d'esclaves autorisés par le serveur. Seuls les %i premières IP ont été prises en compte.\n", NB_SLAVE);
		} else {
			printf("Les informations des esclaves ont bien été récupérées\n");
		}
	} else {
		//Pas de fichier de configuration donc pas de lien vers les esclaves, on quitte
		printf("Impossible de lire le fichier de configuration. Le serveur va se fermer.\n");
		exit(1);
	}
}
