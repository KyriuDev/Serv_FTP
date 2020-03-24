/*
 * echoclient.c - An echo client
 */
#include "csapp.h"
#include <sys/ioctl.h>
#include <sys/time.h>

#define PORT 2121
#define MAX_FILE_SIZE 1000

long int get_file_size(FILE* f);

int check_buffer(char*);

void add_to_file(char*, char*);

void add_file_size(char* oldbuf, char* newbuf);

void strcut(char*, char*);

int main(int argc, char **argv)
{
    int clientfd;
    char *host, buf[MAXLINE], buf_rec[MAX_FILE_SIZE + 1];
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

    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
		if (check_buffer(buf)) {
			//On met dans le buffer uniquement le nom du fichier et pas le get

			char nbuf[strlen(buf) - 4];

			strcut(buf, nbuf);

			/*
				On ajout au buffer la taille du fichier demandé, s'il existe déjà en interne.
				Ainsi, si une des transactions précédentes s'est mal passée, on peut reprendre
				au bon endroit (et ne pas tout recommencer).
			*/
			
			char* fbuf = malloc(strlen(nbuf));
			add_file_size(nbuf, fbuf);

			struct timespec stop, start;
			clock_gettime(CLOCK_MONOTONIC_RAW, &start);
		
			//On envoie nbuf qui contient le nom de notre fichier
			Rio_writen(clientfd, nbuf, strlen(buf));

			int size_rec;
			int size_tot = 0;

			/*
				Tant qu'on reçoi des données du serveur, on les recupère par
				paquets de 1000 bytes via "recv" puis on les ajoute au 
				fichier souhaité.
			*/
			
			while ((size_rec = recv(clientfd, buf_rec, MAX_FILE_SIZE, 0)) > 0) {
				buf_rec[size_rec] = '\0';

				add_to_file(buf_rec, nbuf);

				printf("Paquet de taille %i octets recu\n", size_rec);
				size_tot += size_rec;
			}

			clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
			uint64_t delta = (stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_nsec - start.tv_nsec) / 1000;

			printf("%i bytes reçu en %li µsecondes (%f Mo/s)\n", size_tot, delta, ((double) size_tot / (double) delta));
		} else {
			printf("La commande renseignee n'a pas ete reconnue par le systeme. Veuillez reessayer\n");
		}
    }

    Close(clientfd);
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

void add_file_size(char* oldbuf, char* newbuf) {
	unsigned int taille_nom = strlen(oldbuf);
	
	//On regarde si notre fichier existe ou pas en local
	if (access(oldbuf, F_OK) != -1) {
		//Le fichier existe : on ajoute la taille du fichier dans le nouveau buffer
		FILE* f = fopen(oldbuf, "r");
		long int taille = get_file_size(f);

		//On récupère la "longueur" de notre long int, i.e. son nombre de caractères
		int longueur = (taille == 0 ? 1 : (int) (log10(taille) + 1));

		//On realloue notre nouveau tableau de la taille du nom + 1 + longueur + 1
		int longueur_tot = taille_nom + longueur + 2;
		newbuf = malloc(longueur_tot);
		newbuf[longueur_tot - 1] = '\0';

		//On rajoute le nom et la taille dans notre nouveau buffer
		memcpy(newbuf, oldbuf, taille_nom);
		newbuf[taille_nom] = ' ';
		sprintf(&newbuf[taille_nom + 1], "%li", taille);
		
	} else {
		//Le fichier n'existe pas : on ajoute une taille de 0 dans le nouveau buffer
		newbuf = malloc(taille_nom + 3);
		newbuf[taille_nom + 2] = '\0';
		newbuf[taille_nom + 1] = '0';
		newbuf[taille_nom] = ' ';
	}
}








