/*
 * echoclient.c - An echo client
 */
#include "csapp.h"
#include <sys/ioctl.h>
#include <sys/time.h>

#define PORT 2121
#define MAX_FILE_SIZE 1000

int check_buffer(char*);

void add_to_file(char*, char*);

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
			char nbuf[strlen(buf) - 4];

			strcut(buf, nbuf);

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

				printf("%i\n", size_rec);
				size_tot += size_rec;
			}

			clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
			uint64_t delta = (stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_nsec - start.tv_nsec) / 1000;

			printf("%i bytes reçu en %li µsecondes (%ld bytes/s)\n", size_tot, delta, (size_tot * 1000000) / delta);
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
		int succeed = fputs(contain, f);
		
		if (succeed > 0) {
			printf("Le fichier a bien été généré puis écrit.\n");
		} else {
			printf("Une erreur s'est produite lors de l'écriture des données dans le fichier.\n");
		}
	} else {
		printf("Une erreur s'est produite lors de la création du fichier.\n");
	}

	fclose(f);
}

void strcut(char* buf, char* nbuf) {
    memcpy(nbuf, &buf[4], strlen(buf));

    nbuf[strlen(nbuf) - 1] = '\0';
}

