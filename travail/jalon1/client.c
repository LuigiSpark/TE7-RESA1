#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "common.h"

void die(int ret, char* msg){
	if(ret < 0){
		perror(msg);
		exit(EXIT_FAILURE);
	}
}

int secure_read(int socket, void* buffer, size_t size_buffer){
    int ret;
    size_t received = 0;
    while(received < size_buffer){
        ret = read(socket, (char*)buffer + received, size_buffer - received);
        die(ret, "Error while reading");

        if(ret == 0){
            break;
        }
        received += ret;
    }
    return received;
}

int secure_write(int socket, void*buffer, size_t size_msg){
    int ret;
    size_t sent = 0;
    do{
        ret = write(socket, (char*)buffer + sent, size_msg - sent);
        sent += ret;
        die(ret, "Error while wrinting");
    }while(sent != size_msg);
    return ret;
}



void echo_client(int sockfd) {
	char message[MSG_LEN];
	size_t size;

	struct pollfd fds[2];
	//initiation descripteur stdin
	fds[0].fd=STDIN_FILENO;
	fds[0].events=POLLIN;
	fds[0].revents=0;
	
	//initiation descripteur socket connectée
	fds[1].fd=sockfd;
	fds[1].events=POLLIN;
	fds[1].revents=0;
	
	while (1) {
		
		int nb_active_fd = poll(fds, 2, -1);
		die(nb_active_fd, "On polling...");
		if(fds[0].revents & POLLIN){
			// 1. Lire l'entrée clavier (boucle getchar() jusqu'à '\n')
			// 2. Envoyer la taille (int) au serveur avec send()
			// 3. Envoyer la chaîne au serveur avec send()


			// Cleaning memory
			memset(message, 0, MSG_LEN);
			// Getting message from client
			printf("Message: ");
			size = 0;
			while ((message[size++] = getchar()) != '\n') {
				if(size > MSG_LEN - 2){
					printf("Le message depasse la limite");
					break;
				}
			}


			secure_write(sockfd, &size, sizeof(size));
			secure_write(sockfd, message, size);

			if(strncmp(message, "/quit", 5) == 0){
				printf("déconnecté\n");
				close(sockfd);
				exit(EXIT_SUCCESS);
			}			

			printf("Message sent!\n");
		}

		else if (fds[1].revents & POLLIN){
			// 1. Lire la taille (int) envoyée par le serveur
			// 3. Lire la chaîne de caractères de cette taille exacte
			// 4. Afficher le message reçu

			secure_read(sockfd, &size, sizeof(size));

			// Read message.
			char* message = (char*)malloc(sizeof(char)*size);

			secure_read(sockfd, message, size);

			printf("Received: %s", message);
			free(message);
		}
	}
}

int handle_connect(char* domain, char *port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(domain, port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char *argv[]) {
	if (argc!=3){
		fprintf(stderr,"Erreur:\n./client <server_name> <server_port>\n");
		exit(EXIT_FAILURE);
	}
	char *domain = argv[1];
	char *port = argv[2];
	int sfd;
	sfd = handle_connect(domain, port);


	echo_client(sfd);

	close(sfd);
	return EXIT_SUCCESS;
}

