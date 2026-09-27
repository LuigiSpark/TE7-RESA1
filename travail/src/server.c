#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>

#include "protocol.h"
#include "server.h"
#include "user_list.h"





int echo_server(int sockfd) {

	struct message message;
	char*payload;
	int ret = protocol_recv_message(sockfd, &message, (void**)&payload);
	if(ret == 1) return 1; //Code for closing, 0 is used by EXIT_SUCCESS.


	if(strncmp(payload, "/quit", 5) == 0){
		free(payload);
		return 1;
	}

	printf("Received: %s\n", payload);

	protocol_send_message(sockfd, &message, payload);

	printf("Message sent!\n");
	
	free(payload);
	return EXIT_SUCCESS;
}

int handle_bind(char *PORT_NUMBER) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));

	// AF_UNSPEC -> AF_INET car accept() remplit un sockaddr_in (IPv4).
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
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
		else{
			int yes = 1;
    		setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
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

void accept_init_fds(struct pollfd* fds, int sfd, user_list* users){
	struct sockaddr_in cli;
	socklen_t len = sizeof(cli);

	int client_fd = accept(sfd, (struct sockaddr*) &cli, &len);
	die(client_fd, "Error while accepting");

	char ip_client[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(cli.sin_addr), ip_client, INET_ADDRSTRLEN);

	int port_client = ntohs(cli.sin_port);

	struct user user;
	user.fd = client_fd;
	strcpy(user.IPv4, ip_client);
	user.port = port_client;

	user_list_add(users, user);

	for(int i = 0; i < SOMAXCONN+1; i++){
		if(fds[i].fd == -1){
			fds[i].fd = client_fd;
			fds[i].events = POLLIN;
			fds[i].revents = 0;
			break;
		}
	}
}

int main(int argc, char* argv[]) {

	// On vérfie les paramètres en entrée.
	if(argc != 2){
		printf("Error : invalid number of arguments.\n");
		return EXIT_FAILURE;
	}
	char *PORT_NUMBER = argv[1];
	
	int connect_fd = handle_bind(PORT_NUMBER);

	int yes = 1;
    setsockopt(connect_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	int ret = listen(connect_fd, SOMAXCONN);
	die(ret, "Error while listenning");

	user_list users = NULL;

	struct pollfd fds[SOMAXCONN+1] = {0};
	fds[0].fd = connect_fd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	// Initialise the fd to -1
	for(int i = 1; i < SOMAXCONN+1; i++){
		fds[i].fd = -1; 
	}

	while(1){
		int ret = poll(fds, SOMAXCONN + 1, -1);
		die(ret, "Error while polling");

		for(int i = 0; i < SOMAXCONN + 1; i++){
			if(fds[i].revents & POLLIN){
				if(0 == i){ //if it's a new connection. 
					accept_init_fds(fds, connect_fd, &users);
				}else{ // client sending message. 
					int ret = echo_server(fds[i].fd);
					if(1 == ret){
						printf("disconnected");
						close(fds[i].fd);
						user_list_remove(&users, fds[i].fd);
						fds[i].fd = -1;
					}
					fds[i].revents = 0;
				}
			}
		}
	}


	
	return EXIT_SUCCESS;
}

