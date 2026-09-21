#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"



void echo_client(int sockfd) {
	char data[MSG_LEN];
	size_t size;

	while (1) {
		// Cleaning memory
		memset(data, 0, MSG_LEN);
		// Getting message from client
		printf("Message: ");
		size = 0;
		while ((data[size++] = getchar()) != '\n') {
			if(size==MSG_LEN-1){
				printf("Le message depasse la limite");
				exit(EXIT_FAILURE);
			}
		} // trailing '\n' will be sent


		//send client msg size
		if (send(sockfd, &size, sizeof(size),0) <= 0) {
			break;
		}

		// client Sending message (ECHO)
		if (send(sockfd, data, size,0) <= 0) {
			break;
		}
		printf("Message sent!\n");
		// Cleaning memory
		memset(data, 0, MSG_LEN);
		// Receiving message size
		size = 0;
		if (recv(sockfd, &size, sizeof(size),0) <= 0) {
			break;
		}
		// Receiving message
		if (recv(sockfd, data, size,0) <= 0) {
			break;
		}
		printf("Received: %s", data);
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

