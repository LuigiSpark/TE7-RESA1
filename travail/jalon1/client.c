#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

struct message{
	char data[MSG_LEN];
    int size; // SIZE
};

void echo_client(int sockfd) {
	struct message clientmsg;
	struct message servermsg;
	
	while (1) {
		// Cleaning memory
		memset(clientmsg.data, 0, MSG_LEN);
		// Getting message from client
		printf("Message: ");
		clientmsg.size = 0;
		while ((clientmsg.data[clientmsg.size++] = getchar()) != '\n') {} // trailing '\n' will be sent
		printf("%d\n",clientmsg.size);

		//send client msg size
		if (send(sockfd, &clientmsg.size, sizeof(int), 0) <= 0) {
			break;
		}

		// client Sending message (ECHO)
		if (send(sockfd, clientmsg.data, strlen(clientmsg.data), 0) <= 0) {
			break;
		}
		printf("Message sent!\n");
		// Cleaning memory
		memset(servermsg.data, 0, MSG_LEN);
		// Receiving message size
		servermsg.size = 0;
		if (recv(sockfd, &servermsg.size, sizeof(int), 0) <= 0) {
			break;
		}
		// Receiving message
		if (recv(sockfd, servermsg.data, MSG_LEN, 0) <= 0) {
			break;
		}
		printf("Received: %s", servermsg.data);
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
	}
	char *domain = argv[1];
	char *port = argv[2];
	int sfd;
	sfd = handle_connect(domain, port);


	echo_client(sfd);

	close(sfd);
	return EXIT_SUCCESS;
}

