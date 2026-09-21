#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

void echo_server(int sockfd) {
	char buff[MSG_LEN];
	while (1) {
		// Cleaning memory
		memset(buff, 0, MSG_LEN);
		// Receiving message
		if (recv(sockfd, buff, MSG_LEN, 0) <= 0) {
			break;
		}
		printf("Received: %s", buff);
		// Sending message (ECHO)
		if (send(sockfd, buff, strlen(buff), 0) <= 0) {
			break;
		}
		printf("Message sent!\n");
	}
}

int handle_bind(char *PORT_NUMBER) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));

	//IPv4 or IPv6.
	hints.ai_family = AF_UNSPEC;
	// Use TCP.
	hints.ai_socktype = SOCK_STREAM;
	// Bind sur localhost. 
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, PORT_NUMBER, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}

	// On teste plusieurs adresses renvoyées jusqu'à en trouver une qui fonctionne.
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}
	// On libère les addresses potentielles.
	freeaddrinfo(result);

	return sfd;
}

int main(int argc, char* argv[]) {
	struct sockaddr cli;
	int sfd, connfd;
	socklen_t len;

	// On vérfie les paramètres en entrée.
	if(argc != 2){
		printf("Error : invalid number of arguments.\n");
		return EXIT_FAILURE;
	}
	char *PORT_NUMBER = argv[1];
	
	sfd = handle_bind(PORT_NUMBER);
	if ((listen(sfd, SOMAXCONN)) != 0) {
		perror("listen()\n");
		exit(EXIT_FAILURE);
	}
	len = sizeof(cli);
	if ((connfd = accept(sfd, (struct sockaddr*) &cli, &len)) < 0) {
		perror("accept()\n");
		exit(EXIT_FAILURE);
	}
	echo_server(connfd);
	close(sfd);
	return EXIT_SUCCESS;
}

