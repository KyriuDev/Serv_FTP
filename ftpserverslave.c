/*
 * echoserveri.c - An iterative echo server
 */

#include "csapp.h"
#include "unistd.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PORT 2121
#define MAX_NAME_LEN 256
#define MAX_FILE_SIZE 1000
#define NB_PROC_MAX 2
#define PROC_UTIL 0
#define PROC_INUTIL 1

typedef struct {
    pid_t proc_pid;
    unsigned int utilisable;
} Process;

void send_file(char*, char*, int);

FILE* get_file(char*);

long int get_file_size(FILE* f);

void init_processes(Process**);

void fill_buff(char*, char*, unsigned long*);

Process* get_usable_process(Process**);

void send_ls_result(int);

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
	int nb_proc_curr = 0;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
	char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[MAX_NAME_LEN];
	char buf[MAX_NAME_LEN + 1];
//    Process** processes = malloc(sizeof(void*) * NB_PROC);
  //  init_processes(processes);

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	port = atoi(argv[1]);

	Signal(SIGCHLD, sigchild_handler);

    clientlen = (socklen_t) sizeof(clientaddr);

    listenfd = Open_listenfd(port);

    while (1) {
		connfd = Accept(listenfd, (SA*) &clientaddr, &clientlen);

		if (nb_proc_curr++ < NB_PROC_MAX && Fork() == 0) {
    	    /* determine the name of the client */
        	Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAX_NAME_LEN, 0, 0, 0);

       		/* determine the textual representation of the client's IP address */
       		Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string, INET_ADDRSTRLEN);

        	printf("server connected to %s (%s)\n", client_hostname, client_ip_string);

			/*
				Dès qu'une connexion est établie avec un client, on rentre dans un
				while infini dont on ne sort que lorsque le client envoie la commande
				"bye", indiquant qu'il a terminé sa transaction
			*/

			char buffer[MAX_NAME_LEN + 1];
			
			while(1) {
				ssize_t taille = recv(connfd, buffer, MAX_NAME_LEN, 0);
				buffer[taille] = '\0';

				if (taille != 0) {
					if (strcmp("bye", buffer) == 0) {
						//On ferme proprement la connexion
						char* close_msg = "connexion closed.";
						send(connfd, close_msg, strlen(close_msg), 0);
						break;
					} else if (strcmp("ls", buffer) == 0) {
						//On renvoie les fichiers du dossier courant
						system("ls > .files.txt");
						send_ls_result(connfd);
					} else {
						//On renvoie le fichier demandé s'il existe, un message d'erreur sinon
						send_file(buffer, buf, connfd);
					}
				}

			}

			printf("On termine dans le fils\n");
			close(connfd);
		}

		close(connfd);
	}

    exit(0);
}

void fill_buff(char* requete, char* buf, unsigned long* client_file_size) {
	ssize_t taille = strlen(requete);
	memcpy(buf, requete, strlen(requete));

	/*
		On récupère auss/i la taille du fichier (attention, ici on se base sur le
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

			int j = ++i;

			while (i < taille) {
				taille_str[i - j] = buf[i];
				i++;
			}

			taille_str[taille - j] = '\0';

			//On fait un long de notre taille string

			sscanf(taille_str, "%li", client_file_size);
		}
	}

	/*
		Maintenant que la taille du fichier est récupérée, on coupe
		le buffer contenant le nom du fichier à l'indice du caractère
		"espace"
	*/

	buf[taille - strlen(taille_str) - 1] = '\0';
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

void send_file(char* requete, char* buf, int connfd) {
	/*
		On récupère le nom du fichier transmis, sa taille côté client, et
		on ouvre le fichier côté serveur.
		S'il n'existe pas, on renvoie une erreur et on exit(0).
	*/

	unsigned long taille_fichier_client;
	
	fill_buff(requete, buf, &taille_fichier_client);
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

	unsigned long file_size = get_file_size(my_file); //TODO
	//unsigned long file_size = get_file_size(my_file) - taille_fichier_client;
	unsigned long file_size_rem = file_size;

	/*
		Pour ne plus fermer le descripteur de fichier a chaque fin de transaction,
		on va baser la lecture sur la taille du fichier et plus sur le resultat de
		recv() côté client, donc on doit envoyer la taille du fichier a recuperer
		au client, ce qui deviendra notre base d'arret
	*/

	int longueur = (file_size == 0 ? 1 : (int) (log10(file_size) + 1));
	char longueur_str[longueur + 1];

	sprintf(longueur_str, "%li", file_size);
	longueur_str[longueur] = '\0';

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

	//fseek(my_file, taille_fichier_client, SEEK_SET); //TODO

	/*	
		Tant qu'on n'a pas lu tout le fichier, on continue a le parcourir
		et a ajouter les bytes lus dans notre buffer puis a envoyer ce buffer
		a notre client.
	*/

	while(file_size_rem > 0) {
		/*
			Si c'est le premier paquet transmis, il faut lui ajouter la
			taille du fichier transmis au début, ainsi qu'un caractère spécial
			(ici '\n') pour situer la fin de la taille
		*/

		if (file_size_rem == file_size) {
			/*
				La taille totale vaut la taille restante a envoyer, donc
				il s'agit de la première itération
			*/

			if (longueur < MAX_FILE_SIZE - 1) {
				//On met la longueur dans notre premier paquet
				for (int i = 0; i < longueur; i++) {
					contenu[i] = longueur_str[i];
				}

				//On ajoute notre caractère '\n'
				contenu[longueur] = '\n';

				//On ajoute le contenu de notre fichier au buffer
				fread(&(contenu[longueur + 1]), MAX_FILE_SIZE - longueur - 1, 1, my_file);
			} else {
				printf("Une taille de fichier qui contient plus de 999 chiffres ?????\n");
				return;
			}
		} else {
			fread(contenu, MAX_FILE_SIZE, 1, my_file);
		}

		/*
			Si la taille "restante" du fichier est inférieur à la taille max du buffer
			on ne va pas remplir le buffer, donc on met un caractère de fin de string
			a l'indice correspondant, et on envoie ce paquet la
		*/

		if (file_size_rem < MAX_FILE_SIZE) {
			contenu[file_size_rem + longueur + 1] = '\0';
			int tailleTransmise = send(connfd, contenu, file_size_rem + longueur + 1, 0);
		
			/*
				Si send renvoie -1, on a un problème (on simplifiera ici en admettant
				que la valeur -1 est toujours synonyme d'un problème côté client) donc
				on sort de notre boucle, on ferme notre fichier et on affiche une erreur.
			*/

			if (tailleTransmise == -1) {
				printf("The data sending encountered an issue. Please try again.\n");
				break;
			}

			sent_size += tailleTransmise - longueur - 1;
			file_size_rem -= tailleTransmise - longueur - 1;
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

void send_ls_result(int connfd) {
	/*
		On ouvre notre fichier contenant le résultat de notre "ls"
		et on met le résultat dans une string, avec un '\n' entre
		chaque nom de fichier
	*/

	FILE* f = fopen(".files.txt", "r");
	struct stat stats;
	stat(".files.txt", &stats);

	char ls_inlined[stats.st_size];

	fread(ls_inlined, stats.st_size - 1, 1, f);

	ls_inlined[stats.st_size] = '\0';

	printf("ls value : |%s|\n", ls_inlined);

	//On doit ajouter la "taille" de notre "ls" au debut de notre string
	
	int longueur = ((stats.st_size - 1) == 0 ? 1 : (int) (log10(stats.st_size - 1) + 1));
	char longueur_str[longueur + 2];

	sprintf(longueur_str, "%li", (stats.st_size - 1));
	longueur_str[longueur] = '\n';
	longueur_str[longueur + 1] = '\0';

	printf("longueur : |%s|\n", longueur_str);

	char ls_with_size[stats.st_size + longueur + 1];

	memcpy(ls_with_size, longueur_str, longueur + 1);
	memcpy(&(ls_with_size[longueur + 1]), ls_inlined, stats.st_size - 1);
	ls_with_size[stats.st_size + longueur] = '\0';

	printf("\nls ready : |%s|\n", ls_with_size);
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

