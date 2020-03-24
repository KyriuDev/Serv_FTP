/*
 * echoserveri.c - An iterative echo server
 */

#include "csapp.h"
#include "unistd.h"
#include <math.h>
#include <stdio.h>
#include <poll.h>

#define PORT 2121
#define MAX_NAME_LEN 256
#define MAX_FILE_SIZE 1000
#define NB_PROC 2
#define PROC_UTIL 0
#define PROC_INUTIL 1

typedef struct {
    pid_t proc_pid;
    unsigned int utilisable;
} Process;

void send_file(int, char*);

FILE* get_file(char*);

long int get_file_size(FILE* f);

void init_processes(Process**);

void fill_buff(int, char*, unsigned long*);

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

			send_file(connfd, buf);

			close(connfd);
		}
    
		close(connfd);
	}

    exit(0);
}

void fill_buff(int descriptor, char* buf, unsigned long* client_file_size) {
	ssize_t taille = recv(descriptor, buf, MAX_NAME_LEN, 0);
	
	/*
		On récupère aussi la taille du fichier (attention, ici on se base sur le
		première caractère "espace" rencontré, et donc sur le principe que les
		noms de fichiers demandés ne contiennent pas d'espaces.
	*/

	char* taille_str;

	for (int i = 0; i < taille; i++) {
		if (buf[i] == ' ') {
			/*
				On sait que les charactères suivants sont les chiffres composants
				la taille du fichier.
				On malloc notre taille de la taille totale - la position actuelle
				(caractère espace) - 1 (?).
			*/

			taille_str = malloc(taille - i);

			//On ajoute les caractères restants a notre taille

			while (buf[i] != '\0') {
				taille_str[i - taille] = buf[i];
			}

			taille_str[taille - i - 1] = '\0';

			//On fait un long de notre taille string

			sscanf(taille_str, "%li", client_file_size);
			
			break;
		}
	}

	/*
		Maintenant que la taille du fichier est récupérée, on coupe
		le buffer contenant le nom du fichier à l'indice du caractère
		"espace"
	*/

	buf[taille - strlen(taille_str) - 1] = '\0';

	printf("Nom final de mon fichier : %s\n", buf);
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

void send_file(int connfd, char* buf) {
	/*
		On récupère le nom du fichier transmis, sa taille côté client, et
		on ouvre le fichier côté serveur.
		S'il n'existe pas, on renvoie une erreur et on exit(0).
	*/

	unsigned long taille_fichier_client;
	
	fill_buff(connfd, buf, &taille_fichier_client);
	FILE* my_file = get_file(buf);
		
	if (my_file == NULL) {
		printf("le fichier demandé n'existe pas ou n'est pas accessible\n");
		exit(0);
	}

	/*
		Si on est ici, c'est que le fichier existe, donc on récupère sa taille
		et on crée une version removable de sa taille, pour savoir quand arrêter
		l'envoi (taille == 0)
	*/

	//unsigned long file_size = get_file_size(my_file); //TODO
	unsigned long file_size = get_file_size(my_file) - taille_fichier_client;
	unsigned long file_size_rem = file_size;

	printf("\nTaille du fichier demandé (- sa taille côté client) : %li\n\n", file_size);

	/*
		On crée notre buffer de contenu de fichier qui contiendra au maximum
		MAX_FILE_SIZE (= 1000 bytes) en même temps et on instancie la taille
		transmise a 0.
	*/

	char contenu[MAX_FILE_SIZE + 1];
	contenu[MAX_FILE_SIZE] = '\0';
	ssize_t sent_size = 0;
	
	//On se déplace dans le fichier du nombre d'octets déjà lus et envoyés au client

	fseek(my_file, taille_fichier_client, SEEK_SET); //TODO

	/*	
		Tant qu'on n'a pas lu tout le fichier, on continue a le parcourir
		et a ajouter les bytes lus dans notre buffer puis a envoyer ce buffer
		a notre client.
	*/

	while(file_size_rem > 0) {
		fread(contenu, MAX_FILE_SIZE, 1, my_file);

		/*
			Si la taille "restante" du fichier est inférieur à la taille max du buffer
			on ne va pas remplir le buffer, donc on met un caractère de fin de string
			a l'indice correspondant, et on envoie ce paquet la
		*/

		if (file_size_rem < MAX_FILE_SIZE) {
			contenu[file_size_rem] = '\0';
			int tailleTransmise = send(connfd, contenu, file_size_rem, 0);
			
			/*
				Si send renvoie -1, on a un problème (on simplifiera ici en admettant
				que la valeur -1 est toujours synonyme d'un problème côté client) donc
				on sort de notre boucle, on ferme notre fichier et on affiche une erreur.
			*/

			if (tailleTransmise == -1) {
				printf("The data sending encountered an issue. Please try again.\n");
				break;
			}

			sent_size += tailleTransmise;
			file_size_rem -= tailleTransmise;
		} 
		
		/*
			Sinon, on envoie le buffer plein.
		*/

		else {
			contenu[MAX_FILE_SIZE] = '\0';
			int tailleTransmise = send(connfd, contenu, MAX_FILE_SIZE, 0);

			/*
				Si send renvoie -1, on a un problème (on simplifiera ici en admettant
				que la valeur -1 est toujours synonyme d'un problème côté client) donc
				on sort de notre boucle, on ferme notre fichier et on affiche une erreur.
			*/

			if (tailleTransmise == -1) {
				printf("The data sending encountered an issue. Please try again.\n");
				break;
			}
			
			sent_size += tailleTransmise;
			file_size_rem -= tailleTransmise;
		}
	}
	
	//On ferme notre fichier

	fclose(my_file);
	
	/*
		On vérifie que la taille transmise est bien égale à la taille du fichier
		et on renvoie une erreur si ce n'est pas le cas.
	*/

	if (sent_size != file_size) {
		printf("Un problème est survenu lors de la transaction...\n");
	} else {
		printf("La transaction s'est effectuée sans problème\n");
	}
}

/*
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
}*/

/* NOTA BENE :
	
	En local, il faut bien commenter les lignes portant la mention TODO
	sinon, comme on recopie des fichiers a partir du dossier courant vers
	le dossier courant, les fichiers sont toujours "pleins" et on ne copie rien.
	Sur serveur distant il faut bien décommenter ces (2) lignes.
*/

