#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "client.h"
#include "protocol.h"

#define MSG_LEN 1024

struct client{
	int fd; 
};


int client_connect(struct client *client, const char *host, const char *port){
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	// AF_UNSPEC -> AF_INET car accept() remplit un sockaddr_in (IPv4).
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(host, port, &hints, &result) != 0) {
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
	client->fd = sfd;
	return EXIT_SUCCESS;
}


void send_stdin_data(int sockfd) {
	char payload[MSG_LEN];
	int size;

	// Cleaning memory
	memset(payload, 0, MSG_LEN);
	// Getting message from client
	size = 0;
	while ((payload[size++] = getchar()) != '\n') {
		if(size > MSG_LEN - 2){
			printf("Le message dépasse la limite.");
			break;
		}
	}
	payload[size-1] = '\0'; // replace \n by '\0'

	struct message m;
	m.pld_len = size;
	protocol_send_message(sockfd, &m, payload);

	if(strncmp(payload, "/quit", 5) == 0){
		printf("Disconnected.\n");
		close(sockfd);
		exit(EXIT_SUCCESS);
	}			
	printf("Message sent!\n");
}

void receive_data(int sfd){


	struct message message;
	char* payload;
	int ret = protocol_recv_message(sfd, &message, (void**)&payload);

	if(ret == 1){
		printf("Serveur deconnecté.\n");
		close(sfd);
		exit(EXIT_SUCCESS);
	}

	printf("Received: %s\n", payload);
	printf("Message: ");
	fflush(stdout);
	
	free(payload);
}


int main(int argc, char *argv[]) {
	if (argc!=3){
		fprintf(stderr,"Erreur:\n./client <server_name> <server_port>\n");
		exit(EXIT_FAILURE);
	}
	char *host = argv[1];
	char *port = argv[2];

	struct client client; 
	client_connect(&client, host, port);
	


	struct pollfd fds[2];

	//initiation descripteur stdin
	fds[0].fd=STDIN_FILENO;
	fds[0].events=POLLIN;
	fds[0].revents=0;
	
	//initiation descripteur socket connectée
	fds[1].fd = client.fd;
	fds[1].events=POLLIN;
	fds[1].revents=0;

	printf("Message: ");
	fflush(stdout);
	
	while(1){
		
		int nb_active_fd = poll(fds, 2, -1);
		die(nb_active_fd, "On polling...");

		if(fds[0].revents & POLLIN){
			send_stdin_data(client.fd);
		}
		if(fds[1].revents & POLLIN){
			receive_data(client.fd);
		}
	}

	close(client.fd);
	return EXIT_SUCCESS;
}

