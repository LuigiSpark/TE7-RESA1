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

void client_die(int ret, struct client *client,  char* msg){
	if(ret < 0){
		perror(msg);
		client_close(client);
		exit(EXIT_FAILURE);
	}
}


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

int client_send_message(struct client *client, const struct message *message, const void *payload){
	protocol_send_message(client->fd, message, payload);

	printf("Message sent!\n");
	return EXIT_SUCCESS;
}

int client_receive_message(struct client *client){
	struct message message;
	char* payload;
	int ret = protocol_recv_message(client->fd, &message, (void**)&payload);

	if(ret == 1){
		printf("Serveur deconnecté.\n");
		client_close(client);
		exit(EXIT_SUCCESS);
	}
	printf("Received: %s\n", payload);
	printf("Message: ");
	fflush(stdout);
	
	free(payload);
	return EXIT_SUCCESS;
}

int client_run(struct client *client){
	struct pollfd fds[2];

	//initiation descripteur stdin
	fds[0].fd=STDIN_FILENO;
	fds[0].events=POLLIN;
	fds[0].revents=0;
	
	//initiation descripteur socket connectée
	fds[1].fd = client->fd;
	fds[1].events=POLLIN;
	fds[1].revents=0;

	printf("Message: ");
	fflush(stdout);
	
	while(1){
		
		int nb_active_fd = poll(fds, 2, -1);
		client_die(nb_active_fd, client, "On polling...");

		if(fds[0].revents & POLLIN){
			//Get the message on stdin. 
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

			struct message message;
			message.pld_len = size;

			if(strcmp(payload, "/quit") == 0){
				client_send_message(client, &message, payload);
				printf("Disconnected.\n");
				client_close(client);
			}else{
				client_send_message(client, &message, payload);
			}
		}
		if(fds[1].revents & POLLIN){
			client_receive_message(client);
		}
	}
}

void client_close(struct client *client){
	if(client->fd != -1){
		close(client->fd);
	}
	client->fd = -1;
	exit(EXIT_SUCCESS);
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
	client_run(&client);
	client_close(&client);

	return EXIT_SUCCESS;
}

