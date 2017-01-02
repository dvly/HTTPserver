#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/stat.h>
#include <mqueue.h>

#include "../include/struct.h"
#include "../include/initialisation.h"
#include "../include/t_routine.h"
#include "../include/globals.h"

/*Le serveur prend en argument le numero de port sur lequel il ecoute,
  le nombre de clients qu'il peut accepter simultanement
  et un argument indiquant la quantite max d'octets qu'une ip peut recevoir
  en une minute*/

mime mimes[800];
int size_mimes = 0;
int fd_log;
char filename_log[30] = "tmp/http_2900793_3100300.log";
pthread_mutex_t fd_log_mutex = PTHREAD_MUTEX_INITIALIZER;

mqd_t mq_des;
struct mq_attr attr;
pthread_mutex_t fd_mq_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char * argv[]){
	struct sockaddr_in sin;
	int sock_connexion, PORTSERV, nb_clients_max, seuil_octets;
	unsigned int taille_addr = sizeof(sin);
	pthread_t *th;
	pthread_t watcher_tid;
	requete *req;
	struct stat stat_info;
	struct stat stat_info_dir;

	if(argc != 4){
		perror("Le programme prend en argument le numéro de port sur lequel le serveur écoute; le nombre de client qu'il peut accepter simultanément; et le nombre limite d'octets transférables en une minute pour une ip.\n");
		return EXIT_FAILURE;
	}

	PORTSERV = atoi(argv[1]);
	nb_clients_max = atoi(argv[2]);
	seuil_octets = atoi(argv[3]);

	/*Initialisation de la file de message*/

	printf("avant mqopen\n");
	if((mq_des = mq_open("/file3", O_RDWR | O_CREAT, 0644, NULL)) == -1){
		perror("Erreur de création du file");
		return EXIT_FAILURE;
	}

	printf("avant getaddr\n");
	if(mq_getattr(mq_des, &attr) == -1){
		perror("Erreur de recuperation des attributs du mq");
		return errno;
	}

	printf("mq_des valeur dans main : %d\n", mq_des);
	printf("apres getaddr\n");
	/*buf = malloc(attr.mq_msgsize);*/

	addTypes();

	/*Initialisation des log*/
	if (stat("./tmp", &stat_info_dir) == -1) {
		mkdir("./tmp", 0744);
	}

	if ( stat(filename_log, &stat_info) == -1) {
		/*creation du fichier*/
		if((fd_log = open(filename_log, O_CREAT|O_RDWR, 0644)) == -1){
			perror("Erreur de creation du fichier");
			return errno;
		}
	} else {
		/*ouverture du fichier de log*/
		if((fd_log = open(filename_log, O_RDWR, 0644)) == -1){
			perror("Erreur d'ouverture du fichier");
			return errno;
		}
		lseek(fd_log, 0, SEEK_END);/*offset à la fin*/
	}

	/*Creation de la thread watcher, qui va surveiller les donnees envoyees a
		chaque ip*/
	if (pthread_create(&watcher_tid, NULL, routine_watcher, NULL) != 0) {
		printf("pthread_create\n");
		exit(1);
	}


	printf("apres creation watcher\n");

	/*Mise en place de la socket*/
	if( (sock_connexion = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("Erreur de creation de socket\n");
		return errno;
	}

	/* Initialisation de la socket */
	memset((char *) &sin, 0, sizeof(sin));
	sin.sin_addr.s_addr =  htonl(INADDR_ANY);/* inet_addr("127.0.0.1");*/
	sin.sin_port = htons(PORTSERV);
	sin.sin_family = AF_INET;

	if( bind(sock_connexion, (struct sockaddr *) &sin, sizeof(sin)) == -1){
		perror("Erreur de nommage de la socket\n");
		return errno;
	}
	/* Fin initialisation de socket */

	/*On ecoute sur la socket*/
	listen(sock_connexion, nb_clients_max);

	th = malloc( sizeof(pthread_t) );

	while(1){
		req = malloc(sizeof(struct requete));
		if( (req->soc_com = accept(sock_connexion, (struct sockaddr*) &(req->sin), &taille_addr)) == -1 ){
			perror("Erreur accept\n");
			return errno;
		}

		if (pthread_create(th, NULL, routine_read_req, (void*)req) != 0) {
			printf("pthread_create\n");
			exit(1);
		}
	}

	close(sock_connexion);
	close(fd_log);

	printf("Fin de communication.\nTerminaison du serveur.\n");
	return 0;
}
