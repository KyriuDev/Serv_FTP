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

long int get_file_size(FILE* f);

void get_file(int*, int, char*);

int check_buffer(char*);

void add_to_file(char*, char*);

char* add_file_size(char* oldbuf, char* interrupted_file);

void strcut(char*, char*);

int get_slave_fd(int clientfd);

void get_interrupted_file(char*, char*);

int is_not_in_list(char*, char*);

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
    printf("client connected to server OS\n");

	int nclientfd = get_slave_fd(clientfd);
	Close(clientfd);
    
	Rio_readinitb(&rio, nclientfd);

	//On recupère la liste de nos fichiers interrompus (s'il y en a)

	char backup_name[8] = ".backup";
	backup_name[7] = '\0';

	char interrupted_file[MAX_SIZE_NAME + 1];
	interrupted_file[MAX_SIZE_NAME] = '\0';
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
				break;
			}
		}

		//On vérifie que la commande tapée au clavier est "correcte"

		if (check_buffer(buf)) {
			/*
				On supprime le "get" du buffer pour ne garder que le nom du fichier demandé.
				On ajoute au buffer la taille du fichier, s'il existe déjà en interne.
				Ainsi, si une des transactions précédentes s'est mal passée, on peut reprendre
				au bon endroit (et ne pas tout recommencer).
			*/

			char filename[strlen(buf) - 4];
			strcut(buf, filename);

			//printf("filename : %s\n", filename);

			/*
				On crée un fichier temporaire qui contient le nom du fichier demandé
				(ou on ajoute au fichier temporaire si celui-ci existe déjà) sauf si
				le nom est déjà dans la liste
			*/

			printf("on passe la\n");

			if (is_not_in_list(filename, interrupted_file) == 1) {
				printf("mais pas la\n");
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
	
			//On récupère notre fichier distant
			int size_tot = 0;
			get_file(&size_tot, nclientfd, filename);

			clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
			uint64_t delta = (stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_nsec - start.tv_nsec) / 1000;

			printf("%i bytes reçu en %li µsecondes (%f Mo/s)\n", size_tot, delta, ((double) size_tot / (double) delta));
			remove(backup_name);
		} else {
			printf("La commande renseignee n'a pas ete reconnue par le systeme. Veuillez reessayer\n");
		}
    }

	/*
		On supprime notre fichier (ou au moins la ligne qu'on vient d'ecrire)
		puisque le transfert s'est bien passé si on arrive ici.
	*/

    Close(nclientfd);
    exit(0);
}

int check_buffer(char* buf) {
	int buf_valid = 0;
	
	if (strncmp(buf, "get ", 4) == 0) {
		buf_valid = 1;
	}

	return buf_valid;
}

void add_to_file(char* contain, char* filename) {
	FILE* f = fopen(filename, "a");
	
	if (f != NULL) {
		uint64_t to_write_size = strlen(contain);
		printf("Valeur contain dans add_to_file : |%s|\n", contain);
		fwrite(contain, to_write_size, 1, f);
	} else {
		printf("Une erreur s'est produite lors de l'ouverture du fichier.\n");
	}

	fclose(f);
}

void strcut(char* buf, char* nbuf) {
    memcpy(nbuf, &buf[4], strlen(buf));

    nbuf[strlen(nbuf) - 1] = '\0';
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
		les fichiers que l'on souhaite récupérer
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

	printf("newbuf : %s\n", newbuf);
	
	return newbuf;
}

void get_file(int* size_tot, int clientfd, char* filename) {	
	int size_rec;
	char buffer[MAX_FILE_SIZE + 1];

	/*
		Tant qu'on reçoit des données du serveur, on les recupère par
		paquets de 1000 bytes via "recv" puis on les ajoute au 
		fichier souhaité.
	*/
	
	while ((size_rec = recv(clientfd, buffer, MAX_FILE_SIZE, 0)) > 0) {
		buffer[size_rec] = '\0';

		add_to_file(buffer, filename);

		printf("Paquet de taille %i octets recu\n", size_rec);
		(*size_tot) += size_rec;
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

	printf("taille reçue : %i\n", size_rec);

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

	printf("ip transmise : |%s|\n", ip_address);
	printf("port transmis : |%li|\n", portl);

	int newfd = Open_clientfd(ip_address, portl);

	return newfd;
}

void get_interrupted_file(char* backupname, char* interrupted_file) {
	if (access(backupname, F_OK) != -1) {
		//Le fichier existe donc on récupère ses données 
		FILE* f = fopen(backupname, "r");
		
		struct stat stats;
		stat(backupname, &stats);

		fgets(interrupted_file, stats.st_size, f);

		interrupted_file[stats.st_size] = '\0';

		fclose(f);
	} else {
		//Le fichier n'existe pas
		interrupted_file[0] = '\0';
	}

	printf("fichier interrompu : |%s|\n", interrupted_file);
}

int is_not_in_list(char* filename, char* interrupted_file) {
	int not_in = 1;

	if (strcmp(filename, interrupted_file) == 0) {
		not_in = 0;
	}

	return not_in;
}

/*
	NB : Lors des tests en local, pensez à commenter les lignes mentionnées "TODO"
*/
