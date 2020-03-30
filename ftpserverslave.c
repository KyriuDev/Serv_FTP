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
#define MAX_PATH_SIZE 256
#define MASTER_IP "localhost"
#define TAILLE_PORT 4

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

void send_ls_pwd_result(int, char*);

void send_pwd_result(int);

void change_working_repository(int, char*);

void create_repository(int, char*);

void rm_file_repo(int, char*);

void write_file(int, char*);

void send_file_to_server(int, char*);

void sigchild_handler(int sig) {
    int status;
    waitpid(-1, &status, WNOHANG | WUNTRACED);
}

int port;

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
					printf("commande reçue : |%s|\n", buffer);
					
					if (strcmp("bye", buffer) == 0) {
						//On ferme proprement la connexion
						char* close_msg = "connexion closed.";
						send(connfd, close_msg, strlen(close_msg), 0);
						nb_proc_curr--;
						break;
					} else if (strcmp("ls", buffer) == 0) {
						//On renvoie les fichiers du dossier courant
						system("ls > .files.txt");
						send_ls_pwd_result(connfd, ".files.txt");
						remove(".files.txt");
					} else if (strcmp("pwd", buffer) == 0){
						//On renvoie le chemin courant du serveur FTP
						system("pwd > .path.txt");
						send_ls_pwd_result(connfd, ".path.txt");
						remove(".path.txt");
					} else if (strncmp("cd", buffer, 2) == 0) {
						//On change de dossier sur le serveur distant
						change_working_repository(connfd, buffer);
					} else if (strncmp("mkdir ", buffer, 6) == 0 || strncmp("mkdir ", &(buffer[5]), 6) == 0) {
						//On crée le dossier et on transmet au maitre si le message vient du client
						create_repository(connfd, buffer);
					
						if(strncmp("mkdir ", buffer, 6) != 0) {
							//C'est le maitre qui avait envoyé la commande, donc on break
							nb_proc_curr--;
							break;
						}
					} else if (strncmp("rm ", buffer, 3) == 0 || strncmp("rm ", &(buffer[5]), 3) == 0) {
						//On supprime le fichier/dossier et on transmet au maitre si besoin
						rm_file_repo(connfd, buffer);
						
						if(strncmp("rm ", buffer, 3) != 0) {
							//C'est le maitre qui avait envoyé la commande, donc on break
							nb_proc_curr--;
							break;
						}
					} else if (strncmp("put ", buffer, 4) == 0 || strncmp("put ", &(buffer[5]), 4) == 0) {
						//On ajoute le fichier a l'esclave courant et on envoie au maitre si besoin
						write_file(connfd, buffer);
						
						if(strncmp("put ", buffer, 4) != 0) {
							//C'est le maitre qui avait envoyé la commande, donc on break
							nb_proc_curr--;
							break;
						}
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

void send_ls_pwd_result(int connfd, char* filename) {
	/*
		On ouvre notre fichier contenant le résultat de notre "ls"
		et on met le résultat dans une string, avec un '\n' entre
		chaque nom de fichier
	*/
	
	FILE* my_file = fopen(filename, "r");
	
	char contenu[MAX_FILE_SIZE + 1];
	contenu[MAX_FILE_SIZE] = '\0';
	ssize_t sent_size = 0;
	
	int file_size = get_file_size(my_file) - 1;
	int file_size_rem = file_size;
	int longueur = (file_size == 0 ? 1 : (int) (log10(file_size) + 1));
	char longueur_str[longueur + 2];

	sprintf(longueur_str, "%i", file_size);
	longueur_str[longueur] = '\n';
	longueur_str[longueur + 1] = '\0';

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
			if (file_size_rem == file_size) {
				//Si c'est le premier paquet, on gère la taille qu'on a rajouté
	
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
			} else {
				//Sinon, on n'a pas la taille dans ce paquet
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

	fclose(my_file);
}

void send_pwd_result(int descriptor) {
	FILE* f = fopen(".path.txt", "r");

	int taille = get_file_size(f);

	char buf[taille + 1];
	fread(buf, taille, 1, f);

	printf("taille lue : %i\n", taille);

	buf[taille] = '\0';

	printf("buffer : |%s|\n", buf);

	for (int i = 0; i < taille; i++) {
		printf("char actuel : |%c/%i|\n", buf[i], buf[i]);
	}
}

void change_working_repository(int descriptor, char* command) {
	/*
		On a 3 cas à traiter : 
			- "cd" nous fait revenir au repertoire "home"
			- "cd .." nous fait revenir au repertoire précédent
			- "cd <path>" nous emmène au chemin indiqué s'il est disponible 
	*/

	unsigned int reussite;

	if (strcmp(command, "cd") == 0) {
		//On revient au repertoire "home"

		reussite = chdir(getenv("HOME"));
		
		if (reussite == 0) {
			printf("Changement de répertoire réussi.\n");
		} else {
			printf("Le changement de répertoire a échoué\n");
		}
	} else if (strcmp(command, "cd ..") == 0) {
		//On revient au répertoire précédent
		char dir[MAX_PATH_SIZE];
		getcwd(dir, MAX_PATH_SIZE);

		//On récupère l'indice du dernier '/' du chemin actuel

		int i;

		for (i = strlen(dir); i >=0; i--) {
			if (dir[i] == '/' && i != strlen(dir)) {
				//On est arrivé au dernier slash donc le chemin s'arrête ici
				break;
			}
		}

		//On coupe le chemin actuel a partir de cet indice pour avoir notre chemin

		if (i != 0) {
			char ndir[i + 1];
			memcpy(ndir, dir, i);
			ndir[i] = '\0';

			reussite = chdir(ndir);
			
			if (reussite == 0) {
				printf("Changement de répertoire réussi.\nRépertoir courant : |%s|\n", ndir);
			} else {
				printf("Le changement de répertoire a échoué\n");
			}
		} else {
			reussite = 1;
			printf("On est déjà à la racine !\n");
		}
	} else {
		if (strncmp(command, "cd /", 4) == 0) {
			//On va au répertoire correspondant au chemin
			
			char path[strlen(command) - 2];
			memcpy(path, &(command[3]), strlen(command) - 3);
			path[strlen(command) - 3] = '\0';
			
			reussite = chdir(path);

			if (reussite == 0) {
				//Ici en théorie, path == dir mais on s'en assure

				char dir[MAX_PATH_SIZE];
				getcwd(dir, MAX_PATH_SIZE);
				
				printf("Changement de répertoire réussi.\nRépertoir courant : |%s|\n", dir);
				printf("Répertoire demandé : |%s|\n", path);
			} else {
				printf("Le changement de répertoire a échoué\n");
			}
		} else {
			reussite = 1;
			printf("Le chemin reçu n'a pas le format attendu. Format attendu : '/<..>'\n");
		}
	}

	char buf[2];
	buf[0] = '0' + reussite;
	buf[1] = '\0';

	int sent = send(descriptor, buf, 2, 0);

	if (sent == -1) {
		printf("Erreur lors de l'envoi.\n");
	}
}

void create_repository(int descriptor, char* command) {
	/*
		On doit créer le dossier sur le serveur esclave mais également notifier le maitre
		de cette création afin de créer ce dossier sur tous les autres serveur esclaves.
		Deux possibilités : 
			- mkdir <filename> : on crée le dossier dans le répertoire courant
			- mkdir <path> : on crée le dossier dans le répertoire concerné
		Au début du buffer, on a (ou non) un port renseigné.
		Si le port est renseigné, c'est le maitre qui nous envoie l'information
		du dossier a creer, donc on ne doit pas lui envoyer de message apres avoir
		créé le dossier
	*/

	unsigned int reussite;
	unsigned int command_size;
	unsigned int client;

	if (strncmp(command, "mkdir", 5) == 0) {
		//Le message vient du client
		command_size = strlen(command);
		client = 0;
	} else {
		//Le message vient du maitre -> on enleve donc la taille du port + 1
		command_size = strlen(command) - 5;
		client = 1;
	}

	printf("\ntaille commande : %i\n\n", command_size);

	//On récupère le nom du dossier a créer (on enlève mkdir)

	char dir[command_size - 6];

	if (client == 0) {
		memcpy(dir, &(command[6]), command_size - 6);
	} else {
		memcpy(dir, &(command[11]), command_size - 6);
	}

	dir[command_size - 6] = '\0';

	//Pas de spécification particulière, on crée le dossier avec tous les droits

	reussite = mkdir(dir, ACCESSPERMS);

	if (reussite == 0) {
		printf("Le dossier a bien été créé.\n\n");
		
		if (client == 0) {
			//On envoie les infos au maitre
			
			char fpath[MAX_PATH_SIZE + 1];
			
			if (strncmp(command, "mkdir /", 7) == 0) {
				//On nous a passé un chemin -> On transmet la commande telle quelle

				printf("Chemin ou on doit créer le répertoire : |%s|\n\n", &(command[6]));
				memcpy(fpath, &(command[6]), strlen(command) - 6);
			} else {
				//On nous a passé un nom de fichier -> On rajoute le chemin courant
				char path[MAX_PATH_SIZE];
				getcwd(path, MAX_PATH_SIZE);
				int path_size = strlen(path);

				printf("taille du path : |%s| %li\n\n", path, strlen(path));

				memcpy(fpath, path, path_size);

				//On rajoute un '/' a la fin du chemin et avant le nom du dossier
				fpath[path_size] = '/';

				memcpy(&(fpath[path_size + 1]), &(command[6]), command_size - 6);
				fpath[path_size + command_size - 5] = '\0';

				printf("Chemin ou on doit créer le repertoire : |%s|\n\n", fpath);
			}

			//On est dans le client donc on ouvre une connexion avec le serveur maitre

			int masterfd = Open_clientfd(MASTER_IP, PORT);
			char ppath[strlen(fpath) + 12];
			char portc[TAILLE_PORT];
			sprintf(portc, "%i", port);

			printf("port str : |%s|\n\n", portc);

			memcpy(ppath, portc, 4);
			ppath[4] = ':';
			memcpy(&(ppath[5]), "mkdir ", 6);
			memcpy(&(ppath[11]), fpath, strlen(fpath));

			ppath[strlen(fpath) + 11] = '\0';
			
			printf("path length : %li\n\n", strlen(fpath));
			printf("fpath : |%s|\n\n", fpath);
			printf("final path : |%s|\n\n", ppath);

			Rio_writen(masterfd, ppath, strlen(ppath));
		}
	} else {
		//Si on n'a pas réussi, on renvoie une erreur au client
		printf("Une erreur est survenue lors de la création du dossier.\n");
	}
	
	char buf[2];
	buf[0] = '0' + reussite;
	buf[1] = '\0';

	int sent = send(descriptor, buf, 2, 0);

	if (sent == -1) {
		printf("Erreur lors de l'envoi.\n");
	}
}

void rm_file_repo(int descriptor, char* command) {
	/*
		On doit supprimer le fichier ou le dossier sur le serveur esclave mais
		également notifier le maitre de cette suppression afin de supprimer ce
		dossier sur tous les autres serveurs esclaves.
		Quatre possibilités : 
			- rm <filename> : on supprime le fichier du répertoire courant
			- rm <path> : on supprime le fichier du répertoire concerné
			- rm -r <dirname> : on supprime le dossier du répertoire courant
			- rm -r <path> : on supprime le dossier du répertoire concerné
		Au début du buffer, on a (ou non) un port renseigné.
		Si le port est renseigné, c'est le maitre qui nous envoie l'information
		donc on ne doit rien lui renvoyer après avoir effectué la suppression
	*/

	unsigned int reussite;
	unsigned int command_size;
	unsigned int client;
	unsigned int file;
	unsigned int ind_cmd;

	/*
		On indique si le message vient du client ou du serveur et si on veut
		supprimer un fichier ou un dossier
	*/

	if (strncmp(command, "rm", 2) == 0) {
		//Le message vient du client
		command_size = strlen(command);
		client = 0;
		
		if (strncmp(command, "rm -r ", 6) == 0) {
			file = 1;
			ind_cmd = 6;
		} else {
			file = 0;
			ind_cmd = 3;
		}
	} else {
		//Le message vient du maitre -> on enleve donc la taille du port + 1
		command_size = strlen(command) - 5;
		client = 1;
		
		if (strncmp(&(command[5]), "rm -r ", 6) == 0) {
			file = 1;
		} else {
			file = 0;
		}
	}

	printf("\ntaille commande : %i\n\n", command_size);

	//On récupère le nom du dossier/fichier a supprimer (on enlève rm/rm -r)

	char* elem;

	if (client == 0) {
		if (file == 0) {
			elem = malloc(sizeof(command_size - 3));
			memcpy(elem, &(command[3]), command_size - 3);
			elem[command_size - 3] = '\0';
		} else {
			elem = malloc(sizeof(command_size));
			memcpy(elem, command, command_size);
			elem[command_size] = '\0';
		}
	} else {
		if (file == 0) {
			elem = malloc(sizeof(command_size - 3));
			memcpy(elem, &(command[8]), command_size - 3);
			elem[command_size - 3] = '\0';
		} else {
			elem = malloc(sizeof(command_size));
			memcpy(elem, &(command[5]), command_size);
			elem[command_size] = '\0';
		}
	}

	printf("element a supprimer ou commande pour un dossier : |%s|\n", elem);

	//On effectue la suppression

	if (file == 0) {
		//Au cas ou, on empeche la suppression de fichier n'étant pas au moins dans "Documents"
		if (strncmp(elem, "rm /", 4) != 0 || strncmp(elem, "rm /home/qni/Documents", 22) == 0) {
			reussite = remove(elem);
		} else {
			printf("Vous ne pouvez pas supprimer de fichier aussi profond !\n");
			reussite = 1;
		}
	} else {
		//Au cas ou, on empeche la suppression de dossier n'étant pas au moins dans "Documents"
		if (strncmp(elem, "rm -r /", 7) != 0 || strncmp(elem, "rm -r /home/qni/Documents", 25) == 0) {
			if (system(elem) == 0) {
				reussite = 0;
			} else {
				reussite = 1;
			}
		} else {
			printf("Vous ne pouvez pas supprimer de dossier aussi profond !\n");
			reussite = 1;
		}
	}

	printf("reussite : %i\n", reussite);

	//On print côté serveur la réussite ou l'échec de la suppression

	if (reussite == 0) {
		printf("Le dossier/fichier a bien été supprimé.\n\n");
		
		//Si besoin on transmet l'information au maitre

		if (client == 0) {
			char fpath[MAX_PATH_SIZE + 1];
			
			if (strncmp(command, "rm /", 4) == 0 || strncmp(command, "rm -r /", 7) == 0) {
				//On nous a passé un chemin -> On transmet la commande telle quelle

				printf("Chemin ou on doit créer le répertoire : |%s|\n\n", &(command[ind_cmd]));
				memcpy(fpath, &(command[ind_cmd]), strlen(command) - ind_cmd);
			} else {
				//On nous a passé un nom de fichier -> On rajoute le chemin courant
				char path[MAX_PATH_SIZE];
				getcwd(path, MAX_PATH_SIZE);
				int path_size = strlen(path);

				printf("taille du path : |%s| %li\n\n", path, strlen(path));

				memcpy(fpath, path, path_size);

				//On rajoute un '/' a la fin du chemin et avant le nom du dossier
				fpath[path_size] = '/';

				memcpy(&(fpath[path_size + 1]), &(command[ind_cmd]), command_size - ind_cmd);
				fpath[path_size + command_size - ind_cmd + 1] = '\0';

				printf("Chemin ou on doit créer le repertoire : |%s|\n\n", fpath);
			}

			//On est dans le client donc on ouvre une connexion avec le serveur maitre

			int masterfd = Open_clientfd(MASTER_IP, PORT);
			char ppath[strlen(fpath) + 12];
			char portc[TAILLE_PORT];
			sprintf(portc, "%i", port);
			char* cmd;

			if (file == 0) {
				cmd = "rm ";
			} else {
				cmd = "rm -r ";
			}

			printf("port str : |%s|\n\n", portc);

			memcpy(ppath, portc, 4);
			ppath[4] = ':';
			memcpy(&(ppath[5]), cmd, ind_cmd);
			memcpy(&(ppath[ind_cmd + 5]), fpath, strlen(fpath));

			ppath[strlen(fpath) + ind_cmd + 5] = '\0';
			
			printf("path length : %li\n\n", strlen(fpath));
			printf("fpath : |%s|\n\n", fpath);
			printf("final path : |%s|\n\n", ppath);

			Rio_writen(masterfd, ppath, strlen(ppath));
		}
	} else {
		//Si on n'a pas réussi, on renvoie une erreur au client
		printf("Une erreur est survenue lors de la suppression du fichier/dossier.\n");
	}
	
	char buf[2];
	buf[0] = '0' + reussite;
	buf[1] = '\0';

	int sent = send(descriptor, buf, 2, 0);

	if (sent == -1) {
		printf("Erreur lors de l'envoi.\n");
	}
}

void write_file(int descriptor, char* command) {
	//On envoie un message au client pour dire qu'on est pret a recevoir le fichier
	send(descriptor, "0", 1, 0);

	unsigned int client;
	unsigned int reussite;
	char* filename;

	//On regarde si le message provient d'un client ou du maitre
	if (strncmp(command, "put", 3) == 0) {
		//Client
		client = 0;
		
		filename = malloc(strlen(command) - 3);
		memcpy(filename, &(command[4]), strlen(command) - 4);
		filename[strlen(command) - 4] = '\0';
	} else {
		client = 1;
		
		filename = malloc(strlen(command) - 8);
		memcpy(filename, &(command[9]), strlen(command) - 9);
		filename[strlen(command) - 9] = '\0';
	}

	//On récupère le nom du fichier
	
	printf("nom du fichier : |%s|\n", filename);

	//On crée nos valeurs "bornes" : size_tot et size_rec
	long long size_tot = -1;
	unsigned int size_rec_tot = 0;

	/*
		Pour l'instant, on ne se préoccupe pas de savoir si le fichier
		existe ou non sur le serveur, on l'écrase quoi qu'il arrive
	*/

	/*
		On écrit dans un fichier de nom différent puisqu'on travaille en local.
		Pour l'utilisation serveur, commenter les 3 lignes suivantes.
	*/
	char portc[5];
	sprintf(portc, "%i", port);

	char filename2[strlen(filename) + 5];
	memcpy(filename2, filename, strlen(filename));
	memcpy(&filename2[strlen(filename)], portc, 4);
	filename2[strlen(filename) + 4] = '\0';

	FILE* f = fopen(filename2, "w");

	if (f != NULL) {
		//On boucle tant qu'on n'a pas récupéré tout le fichier du client

		while (size_rec_tot != size_tot) {
			/*
				On lit nos paquets
			*/

			char buf[MAX_FILE_SIZE + 1];
			ssize_t size_rec = recv(descriptor, buf, MAX_FILE_SIZE, 0);
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

				//On écrit le reste du buffer dans notre fichier

				fwrite(&(buf[i + 1]), size_rec - i - 1, 1, f);
		
				//On incrémente notre taille reçue totale de la taille du buffer
				size_rec_tot += size_rec - i - 1;
			} else {
				fwrite(buf, size_rec, 1, f);
				size_rec_tot += size_rec;
			}
		}

		fclose(f);
		reussite = 0;

		printf("Le fichier a bien été uploadé sur le serveur.\n");
	} else {
		reussite = 1;
		printf("Une erreur est survenue lors de l'ouverture du fichier. Veuillez réessayer.\n");
	}
	
	//On envoie le résultat de l'upload au client

	char buf[2];
	buf[0] = '0' + reussite;
	buf[1] = '\0';

	int sent = send(descriptor, buf, 2, 0);

	if (sent == -1) {
		printf("Erreur lors de l'envoi.\n");
	}

	/*
		On a fini la transaction avec le client, on s'occupe maintenant des échanges
		entre le maitre et les autres esclaves qui n'ont pas le fichier mais on le fait dans
		autre processus pour ne pas bloquer le client
	*/

	if (client == 0) {
		//On est dans le client donc on ouvre une connexion avec le serveur maitre

		printf("on passe ici\n");

		int masterfd = Open_clientfd(MASTER_IP, PORT);
		char ncmd[strlen(command) + 6];
		char portc[TAILLE_PORT + 1];
		sprintf(portc, "%i", port);
		portc[TAILLE_PORT] = '\0';

		memcpy(ncmd, portc, 4);
		ncmd[4] = ':';
		memcpy(&(ncmd[5]), command, strlen(command));
		ncmd[strlen(command) + 5] = '\0';

		Rio_writen(masterfd, ncmd, strlen(ncmd));
	
		//On transmet le fichier au maitre comme si on était le client et lui l'esclave
	
		send_file_to_server(masterfd, command);

		printf("on est apres le send_file\n");

		//On recommence tant que le maitre nous indique qu'il y a des esclaves sur lesquels
		//faire l'ajout

		char stop[2];
		recv(masterfd, stop, 1, 0);
		stop[1] = '\0';

		printf("on est avant le while\n");

		while (strcmp(stop, "1") == 0) {
			send_file_to_server(masterfd, command);
			recv(masterfd, stop, 1, 0);
		}

		printf("on est après le while\n");

		close(masterfd);
	}

	printf("on a terminé l'envoi proprement\n");
}

void send_file_to_server(int descriptor, char* command) {
	//On attend que le serveur soit pret a recevoir la donnée
	char ready[2];
	recv(descriptor, ready, 1, 0);
	ready[1] = '\0';

	while (strcmp(ready, "0") != 0) {
		recv(descriptor, ready, 1, 0);
	}

	//On récupère notre nom de fichier
	char filename[strlen(command) - 3];
	memcpy(filename, &(command[4]), strlen(command) - 4);
	filename[strlen(command) - 4] = '\0';
	
	//On vérifie si le fichier demandé existe
	if (access(filename, F_OK) != -1) {
		//S'il existe, on l'ouvre en lecture
		FILE* f = fopen(filename, "r");
		int file_size = get_file_size(f);
		int file_size_rem = file_size;
		char buf[MAX_FILE_SIZE + 1];

		while (file_size_rem != 0) {
			//Si c'est la première itération, on ajoute la taille du fichier a envoyer
			size_t read_size;

			if (file_size_rem == file_size) {
				//On récupère la "longueur" de notre long int, i.e. son nombre de caractères
				int longueur = (file_size == 0 ? 1 : (int) (log10(file_size) + 1));
				char taille[longueur + 1];
				sprintf(taille, "%i", file_size);
				taille[longueur] = '\0';

				memcpy(buf, taille, longueur);

				buf[longueur] = '\n';
	
				read_size = fread(&(buf[longueur + 1]), 1, MAX_FILE_SIZE - longueur - 1, f);
				buf[read_size + longueur + 1] = '\0';
				
				ssize_t sent_size = send(descriptor, buf, read_size + longueur + 1, 0);

				if (sent_size != read_size + longueur + 1) {
					printf("Une erreur est survenue lors de l'envoi du fichier. Veuillez réessayer.\n");
					break;
				}

				file_size_rem -= sent_size - longueur - 1;
			} else {
				read_size = fread(buf, 1, MAX_FILE_SIZE, f);
				buf[read_size] = '\0';
				
				ssize_t sent_size = send(descriptor, buf, read_size, 0);

				if (sent_size != read_size) {
					printf("Une erreur est survenue lors de l'envoi du fichier. Veuillez réessayer.\n");
					break;
				}

				file_size_rem -= sent_size;
			}
		}

	} else {
		printf("Le fichier demandé n'existe pas. Veuillez saisir un nom de fichier valide.\n");
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

