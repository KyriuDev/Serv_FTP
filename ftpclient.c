/*
 * echoclient.c - An echo client
 */
#include "csapp.h"
#include <sys/ioctl.h>
#include <sys/time.h>

#define PORT 2121
#define MAX_SIZE_NAME 256
#define MAX_FILE_SIZE 1000
#define MAX_IP_ADDR_SIZE 15
#define CONNEXION_CLOSED "connexion closed."

long int get_file_size(FILE* f);

void get_file(int*, int, char*);

int command_in_list(char*);

void add_to_file(char*, char*);

char* add_file_size(char* oldbuf, char* interrupted_file);

void strcut(char*, char*);

int get_slave_fd(int clientfd);

void get_interrupted_file(char*, char*);

int is_not_in_list(char*, char*);

void print_ls_pwd_result(int);

void put_file_on_server(int, char*);

int main(int argc, char **argv)
{
    int clientfd;
    char *host, buf[MAXLINE];
    rio_t rio;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <host>\n", argv[0]);
        exit(0);
    }

    host = argv[1];

    /*
     * Note that the 'host' can be a name or an IP address.
     * If necessary, Open_clientfd will perform the name resolution
     * to obtain the IP address.
     */
    clientfd = Open_clientfd(host, PORT);

    /*
     * At this stage, the connection is established between the client
     * and the server OS ... but it is possible that the server application
     * has not yet called "Accept" for this connection
     */
    printf("client connected to server OS\n\n");

	//On envoie au maitre un "0" pour dire qu'on est client et pas esclave
	Rio_writen(clientfd, "0", 1);
	int nclientfd = get_slave_fd(clientfd);
	Close(clientfd);
    
	Rio_readinitb(&rio, nclientfd);

	//On recupère la liste de nos fichiers interrompus (s'il y en a)

	char backup_name[8] = ".backup";
	backup_name[7] = '\0';

	char interrupted_file[MAX_SIZE_NAME + 1];
	get_interrupted_file(backup_name, interrupted_file);

    while (1) {
		//On récupère la commande tapée au clavier

		char* val = Fgets(buf, MAXLINE, stdin);
	
		if (val == NULL) {
			/*
				Si Fgets renvoie NULL, soit on a atteint la fin de la saisie clavier,
				soit on a eu une erreur (interruption par exemple)
			*/

			if (errno == EINTR) {
				//On a eu une interruption (erreur)
				printf("On a eu une interruption !\n");
				break;
			} else {
				//On a atteint la fin de la saisie clavier (standard)
			}
		}

		buf[strlen(buf) - 1] = '\0';

		//On vérifie que la commande tapée au clavier est "correcte"

		if (command_in_list(buf)) {
			if (strcmp(buf, "bye") == 0) {
				Rio_writen(nclientfd, buf, strlen(buf));
				
				char close_message[sizeof(CONNEXION_CLOSED) + 1];

				recv(nclientfd, close_message, sizeof(CONNEXION_CLOSED), 0);

				close_message[sizeof(CONNEXION_CLOSED)] = '\0';

				if (strcmp(close_message, CONNEXION_CLOSED) == 0) {
					printf("Distant server has been shutdowned well. Client closing.\n");
					exit(0);
				} else {
					printf("A problem has occurred during server shutdowning. Please try again.\n");
				}
			} else if (strcmp(buf, "ls") == 0 ||strcmp(buf, "pwd") == 0) {
				Rio_writen(nclientfd, buf, strlen(buf));
				
				print_ls_pwd_result(nclientfd);
			} else if (strncmp(buf, "cd", 2) == 0) {
				Rio_writen(nclientfd, buf, strlen(buf));
			
				char buff[2];
				recv(nclientfd, buff, 2, 0);
				buff[1] = '\0';

				while(strcmp(buff, "") == 0) {
					recv(nclientfd, buff, 2, 0);
				}
				
				if (strcmp(buff, "0") == 0) {
					printf("Le dossier courant a bien été modifié sur le serveur.\n\n");
				} else {
					printf("Une erreur est survenue lors de la modification du dossier courant sur le serveur. Veuillez réessayer.\n");
				}
			} else if (strncmp(buf, "mkdir ", 6) == 0) {
				Rio_writen(nclientfd, buf, strlen(buf));

				char buff[2];
				recv(nclientfd, buff, 2, 0);
				buff[1] = '\0';

				while(strcmp(buff, "") == 0) {
					recv(nclientfd, buff, 2, 0);
				}
				
				if (strcmp(buff, "0") == 0) {
					printf("Le dossier a bien été créé sur le serveur.\n\n");
				} else {
					printf("Une erreur est survenue lors de la création du dossier sur le serveur. Veuillez vous assurer d'être bien connecté, puis réessayez.\n");
				}
			} else if (strncmp(buf, "rm ", 3) == 0) {
				Rio_writen(nclientfd, buf, strlen(buf));

				char buff[2];
				recv(nclientfd, buff, 2, 0);
				buff[1] = '\0';
				char* msgr;
				char* msgf;

				while(strcmp(buff, "") == 0) {
					recv(nclientfd, buff, 2, 0);
				}
				
				if (strncmp(buf, "rm -r ", 6) == 0) {
					msgr = "Le dossier ainsi que tous ses fichiers a bien été supprimé.\n";
					msgf = "Une erreur est survenue lors de la suppression du dossier sur le serveur. Veuillez vous assurer d'être bien connecté, puis réessayez\n\n";
				} else {
					msgr = "Le fichier a bien été supprimé.\n";
					msgf = "Une erreur est survenue lors de la suppression du fichier sur le serveur. Veuillez vous assurer d'être bien connecté, puis réessayez.\n\n";
				}

				if (strcmp(buff, "0") == 0) {
					printf("%s\n", msgr);
				} else {
					printf("%s\n", msgf);
				}
			} else if (strncmp(buf, "put ", 4) == 0) {
				put_file_on_server(nclientfd, buf);
			} else if (strncmp(buf, "connect ", 8) == 0) {
				Rio_writen(nclientfd, buf, strlen(buf));
				
				char buff[2];
				recv(nclientfd, buff, 2, 0);
				buff[1] = '\0';

				//On attend la réponse du serveur

				while(strcmp(buff, "") == 0) {
					recv(nclientfd, buff, 2, 0);
				}

				printf("buffer : |%s|\n", buff);

				if (strcmp(buff, "0") == 0) {
					printf("Vous êtes bien connecté au serveur distant.\n\n");
				} else {
					printf("Une erreur est survenue lors de la connexion au serveur distant. Assurez-vous de bien avoir renseigné vos identifiants, et le cas échéant de bien avoir un compte.\n");
				}
			} else {
				/*
					On supprime le "get" du buffer pour ne garder que le nom du fichier demandé.
					On ajoute au buffer la taille du fichier, s'il existe déjà en interne.
					Ainsi, si une des transactions précédentes s'est mal passée, on peut reprendre
					au bon endroit (et ne pas tout recommencer).
				*/

				char filename[strlen(buf) - 4];
				strcut(buf, filename);

				/*
					On crée un fichier temporaire qui contient le nom du fichier demandé
					(ou on ajoute au fichier temporaire si celui-ci existe déjà) sauf si
					le nom est déjà dans la liste
				*/

				if (is_not_in_list(filename, interrupted_file) == 1) {
					add_to_file(filename, backup_name);
				}

				//On ajoute la taille du fichier a notre requete

				char* fbuf = add_file_size(filename, interrupted_file);

				//printf("buffer : %s\n", fbuf);
				//printf("size buffer : %li\n", strlen(fbuf));
				//printf("last : %i\n", fbuf[strlen(fbuf)]);
				
				struct timespec stop, start;
				clock_gettime(CLOCK_MONOTONIC_RAW, &start);
			
				//On envoie fbuf qui contient le nom de notre fichier et sa taille
				Rio_writen(nclientfd, fbuf, strlen(fbuf));

				//On check le byte renvoyé par le serveur pour savoir si notre fichier existe

				char exists[2];
				recv(nclientfd, exists, 2, 0);
				exists[1] = '\0';

				//On attend la réponse du serveur

				while(strcmp(exists, "") == 0) {
					recv(nclientfd, exists, 2, 0);
				}

				//Si le fichier n'existe pas, on print un message d'erreur et on recommence l'attente
				if (strcmp(exists, "0") != 0) {
					printf("Le fichier demandé n'existe pas sur le serveur.\n\n");
					continue;
				}

				//On récupère notre fichier distant
				int size_tot = 0;

				get_file(&size_tot, nclientfd, filename);

				clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
				uint64_t delta = (stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_nsec - start.tv_nsec) / 1000;

				printf("\n%i bytes reçu en %li µsecondes (%f Mo/s)\n\n", size_tot, delta, ((double) size_tot / (double) delta));
				
				//remove(backup_name);
			}
		} else {
			printf("La commande renseignée n'a pas été reconnue pas le système. Veuillez recommencer avec l'une des commandes suivante :\n- get <filename>\n- cd <path>\n- pwd\n- ls\n- mkdir\n- rm <dependencies> <filename>\n- put <filename>\n");
		}
    }

	/*
		On supprime notre fichier (ou au moins la ligne qu'on vient d'ecrire)
		puisque le transfert s'est bien passé si on arrive ici.
	*/

    Close(nclientfd);
    exit(0);
}

int command_in_list(char* buf) {
	int buf_valid = 0;

	if (strncmp(buf, "get ", 4) == 0 ||
		strcmp(buf, "bye") == 0 ||
		strcmp(buf, "ls") == 0 ||
		strcmp(buf, "pwd") == 0 ||
		strncmp(buf, "cd", 2) == 0 ||
		strncmp(buf, "mkdir ", 6) == 0 ||
		strncmp(buf, "rm ", 3) == 0 ||
		strncmp(buf, "put ", 4) == 0 ||
		strncmp(buf, "connect ", 8) == 0)
	{
		buf_valid = 1;
	}

	return buf_valid;
}

void add_to_file(char* contain, char* filename) {
	FILE* f = fopen(filename, "a");

	if (f != NULL) {
		uint64_t to_write_size = strlen(contain);
		fwrite(contain, to_write_size, 1, f);
	} else {
		printf("Une erreur s'est produite lors de l'ouverture du fichier.\n");
	}

	fclose(f);
}

void strcut(char* buf, char* nbuf) {
    memcpy(nbuf, &buf[4], strlen(buf));

    nbuf[strlen(nbuf)] = '\0';
}

long int get_file_size(FILE* f) {
    fseek(f, 0L, SEEK_END);

    long int size = ftell(f);

    rewind(f);

    return size;
}

char* add_file_size(char* oldbuf, char* interrupted_file) {
	//On calcule la taille du nom du fichier
	unsigned int taille_nom = strlen(oldbuf);
	char* newbuf;

	//On vérifie si notre fichier est dans la liste des fichiers interrompus
	if (strcmp(oldbuf, interrupted_file) == 0) {
		//On a trouvé un fichier de même nom dans la liste des fichiers interrompus

		//On regarde si notre fichier existe ou pas en local
		if (access(oldbuf, F_OK) != -1) {
			//Le fichier existe : on ajoute la taille du fichier dans le nouveau buffer
			FILE* f = fopen(oldbuf, "r");
			long int taille = get_file_size(f);
			fclose(f);

			//On récupère la "longueur" de notre long int, i.e. son nombre de caractères
			int longueur = (taille == 0 ? 1 : (int) (log10(taille) + 1));

			//On realloue notre nouveau tableau de la taille du nom + 1 + longueur + 1
			int longueur_tot = taille_nom + longueur + 2;
			newbuf = malloc(longueur_tot);

			//On rajoute le nom et la taille dans notre nouveau buffer
			memcpy(newbuf, oldbuf, taille_nom);
			newbuf[taille_nom] = ' ';
			sprintf(&newbuf[taille_nom + 1], "%li", taille);

			newbuf[longueur_tot - 1] = '\0';
		} else {
			/*
				Le fichier n'existe pas : on ajoute une taille de 0 dans le nouveau buffer
				NB : Normalement, on ne passe ici, hormis si on a supprimé à la main le fichier
				concerné, et qu'il n'a donc pas été supprimé du .backup
			*/

			newbuf = malloc(taille_nom + 3);

			memcpy(newbuf, oldbuf, taille_nom);

			newbuf[taille_nom] = ' ';
			newbuf[taille_nom + 1] = '0';
			newbuf[taille_nom + 2] = '\0';
		}
	} else {
		/*
			Si le nom de notre fichier n'est pas dans la liste des fichiers
			interrompus, on part du principe que même si on possède un fichier en
			local qui a le même nom, on veut récupérer le fichier distant, donc
			on indique une taille de 0 pour notre fichier local afin qu'il soit
			écrasé en local
		*/
		
		/*
		TODO Commenter pour tester en local (sinon on supprime systematiquement
		les fichiers que l'on souhaite récupérer)
		*/

		/*
		if (access(oldbuf, F_OK) != -1) {
			//	Si on arrive là, c'est que notre fichier existe en local,
			//	mais qu'on veut récupérer la version distante, donc on
			//	supprime la version locale.

			int removed = remove(oldbuf);

			if (removed != 0) {
				printf("Une erreur s'est produite lors de la suppression du fichier avant récupération.\n");
			}
		}
		*/

		//Fin TODO

		newbuf = malloc(taille_nom + 3);

		memcpy(newbuf, oldbuf, taille_nom);

		newbuf[taille_nom] = ' ';
		newbuf[taille_nom + 1] = '0';
		newbuf[taille_nom + 2] = '\0';
	}

	return newbuf;
}

void get_file(int* size_tot, int clientfd, char* filename) {	
	long file_size = -1;
	int size_rec;
	char buffer[MAX_FILE_SIZE + 1];

	/*
		tant qu'on reçoit des données du serveur, on les recupère par
		paquets de 1000 bytes via "recv" puis on les ajoute au 
		fichier souhaité.
	*/

	while ((*size_tot) != file_size) {
		size_rec = recv(clientfd, buffer, MAX_FILE_SIZE, 0);
		
		if (file_size == -1) {
			//première itération de la boucle : on récupère la taille du fichier distant
			char size[MAX_FILE_SIZE + 1];
			unsigned int i = 0;

			while (i < size_rec && buffer[i] != '\n') {
				size[i] = buffer[i];
				i++;
			}

			size[i] = '\0';

			sscanf(size, "%li", &file_size);

			char nbuf[MAX_FILE_SIZE - i];
			int real_size = size_rec - i - 1;
			memcpy(nbuf, &(buffer[i + 1]), real_size);
			nbuf[real_size] = '\0';

			add_to_file(nbuf, filename);

			//printf("paquet de taille %i octets reçu\n", real_size);
			(*size_tot) += real_size;
		} else {	
			buffer[size_rec] = '\0';

			add_to_file(buffer, filename);

			//printf("paquet de taille %i octets recu\n", size_rec);
			(*size_tot) += size_rec;
		}
	}
}

int get_slave_fd(int clientfd) {
	char buffer[MAX_SIZE_NAME + 1];
	char ip_address[MAX_IP_ADDR_SIZE + 1];

	int size_rec = recv(clientfd, buffer, MAX_SIZE_NAME, 0);

	if (size_rec == -1) {
		printf("An error has occurred during the transaction. Please try again.\n");
		return -1;
	}

	buffer[size_rec] = '\0';
	
	int i = 0;

	while(buffer[i] != ':') {
		ip_address[i] = buffer[i];
		i++;
	}

	ip_address[i] = '\0';

	char port[size_rec - i - 1];
	int j = ++i;

	while (buffer[i] != '\0') {
		port[i - j] = buffer[i];
		i++;
	}

	port[size_rec - j] = '\0';

	unsigned long portl;

	sscanf(port, "%li", &portl);

	int newfd = Open_clientfd(ip_address, portl);

	return newfd;
}

void get_interrupted_file(char* backupname, char* interrupted_file) {
	if (access(backupname, F_OK) != -1) {
		//Le fichier existe donc on récupère ses données 
		FILE* f = fopen(backupname, "r");
		
		struct stat stats;
		stat(backupname, &stats);

		fread(interrupted_file, 1, stats.st_size, f);
		interrupted_file[stats.st_size] = '\0';

		fclose(f);
	} else {
		//Le fichier n'existe pas
		interrupted_file[0] = '\0';
	}
}

int is_not_in_list(char* filename, char* interrupted_file) {
	int not_in = 1;

	if (strcmp(filename, interrupted_file) == 0) {
		not_in = 0;
	}

	return not_in;
}

void print_ls_pwd_result(int descriptor) {
	long size_tot = 0;
	long file_size = -1;
	int size_rec;
	char buffer[MAX_FILE_SIZE + 1];
	char* res;

	/*
		tant qu'on reçoit des données du serveur, on les recupère par
		paquets de 1000 bytes via "recv" puis on les ajoute au 
		fichier souhaité.
	*/
	
	while (size_tot != file_size) {
		size_rec = recv(descriptor, buffer, MAX_FILE_SIZE, 0);

		if (file_size == -1) {
			//première itération de la boucle : on récupère la taille du fichier distant
			char size[MAX_FILE_SIZE + 1];
			unsigned int i = 0;

			while (i < size_rec && buffer[i] != '\n') {
				size[i] = buffer[i];
				i++;
			}

			size[i] = '\0';

			sscanf(size, "%li", &file_size);

			int real_size = size_rec - i - 1;
		
			res = malloc(real_size + 1);
			
			memcpy(res, &(buffer[i + 1]), real_size);
			res[real_size] = '\0';

			size_tot += real_size;
		} else {	
			res = realloc(res, size_tot + size_rec + 1);
			
			memcpy(&(res[size_tot + 1]), buffer, size_rec);

			res[size_tot + size_rec] = '\0';

			size_tot += size_rec;
		}
	}

	printf("\n%s\n\n", res);
}

void put_file_on_server(int descriptor, char* command) {
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

		//On envoie la commande au serveur

		Rio_writen(descriptor, command, strlen(command));

		//On vérifie que le serveur a bien accepté la commande (on est bien authentifié)

		char success[2];
		recv(descriptor, success, 1, 0);
		success[1] = '\0';

		while (strcmp(success, "") == 0) {
			recv(descriptor, success, 1, 0);
		}

		if (strcmp(success, "1") == 0) {
			//Le serveur a refusé la commande, on print un message d'erreur et on return
			printf("Le serveur a refusé l'accès. Veuillez vous assurer d'être bien connecté, et réessayez\n");
			fclose(f);
			return;
		}
		
		/*
			On attend que le serveur soit pret a recevoir la donnée mais si le serveur a 
			accepté la commande, on a peut-être déjà reçu l'information nous indiquant si
			le serveur est prêt ou pas, donc si success == 0, on ne fait pas le recv
		*/

		while (strcmp(success, "0") != 0) {
			recv(descriptor, success, 1, 0);
		}
	
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
	
		char buff[2];
		recv(descriptor, buff, 2, 0);
		buff[1] = '\0';

		if (strcmp(buff, "0") == 0) {
			printf("Le fichier a bien été uploadé sur le serveur.\n\n");
		} else {
			printf("Une erreur est survenue lors de l'upload du fichier sur le serveur. Veuillez réessayer.\n");
		}
	} else {
		printf("Le fichier demandé n'existe pas. Veuillez saisir un nom de fichier valide.\n\n");
	}
}

/*
	NB : Lors des tests en local, pensez à commenter les lignes mentionnées "TODO"
*/
