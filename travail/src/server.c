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

struct server{
    int listen_fd;
    struct pollfd *fds;  
    user_list users;    
};

void server_die(int ret, struct server *server,  char* msg){
	if(ret < 0){
		perror(msg);
		server_close(server);
		exit(EXIT_FAILURE);
	}
}

int handle_bind(const char *PORT_NUMBER){
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

int server_listen(struct server *server, const char *port){
	server->listen_fd = handle_bind(port);

	int yes = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	int ret = listen(server->listen_fd, SOMAXCONN);
	server_die(ret, server, "Error while listenning");

	return EXIT_SUCCESS;
}

int server_run(struct server *server){

	server->users = NULL;

	server->fds = (struct pollfd*)calloc((SOMAXCONN+1), sizeof(struct pollfd));
	if(server->fds == NULL){
		perror("Error while allocating pollfd structure");
		server_close(server);
		exit(EXIT_FAILURE);
	}
	struct pollfd* fds = server->fds;
	fds[0].fd = server->listen_fd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	// Initialise the fd to -1
	for(int i = 1; i < SOMAXCONN+1; i++){
		fds[i].fd = -1; 
	}

	while(1){
		int ret = poll(fds, SOMAXCONN + 1, -1);
		server_die(ret, server, "Error while polling");

		for(int i = 0; i < SOMAXCONN + 1; i++){
			if(fds[i].revents & POLLIN){
				if(0 == i){ //if it's a new connection. 
					server_accept_client(server);
				}else{ // client sending message. 
					struct user* sender = user_list_find_by_socket(&server->users, fds[i].fd);
					int ret = server_receive_message(server, sender);
					if(1 == ret){
						printf("disconnected");
						close(fds[i].fd);
						user_list_remove(&server->users, fds[i].fd);
						fds[i].fd = -1;
					}
					fds[i].revents = 0;
				}
			}
		}
	}
}

int server_receive_message(struct server *server, struct user *sender){

	struct message message;
	char*payload;
	int ret = protocol_recv_message(sender->fd, &message, (void**)&payload);
	if(ret == 1) return 1; //Code for closing, 0 is used by EXIT_SUCCESS.


	if(strncmp(payload, "/quit", 5) == 0){
		free(payload);
		return 1;
	}

	printf("Received: %s\n", payload);

	protocol_send_message(sender->fd, &message, payload);

	printf("Message sent!\n");
	
	free(payload);
	return EXIT_SUCCESS;
}

int server_accept_client(struct server *server){

	struct sockaddr_in cli;
	socklen_t len = sizeof(cli);

	int client_fd = accept(server->listen_fd, (struct sockaddr*) &cli, &len);
	server_die(client_fd, server,"Error while accepting");

	char ip_client[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(cli.sin_addr), ip_client, INET_ADDRSTRLEN);

	int port_client = ntohs(cli.sin_port);

	struct user user;
	user.fd = client_fd;
	strcpy(user.IPv4, ip_client);
	user.port = port_client;

	user_list_add(&server->users, user);

	struct pollfd* fds = server->fds;
	for(int i = 0; i < SOMAXCONN+1; i++){
		if(fds[i].fd == -1){
			fds[i].fd = client_fd;
			fds[i].events = POLLIN;
			fds[i].revents = 0;
			break;
		}
	}

	return EXIT_SUCCESS;
}

void server_close(struct server *server){
	if(server->listen_fd != -1){
		for(int i = 0; i < SOMAXCONN + 1; i++){
			if(server->fds[i].fd != -1){
				close(server->fds[i].fd);
				server->fds[i].fd = -1;
			}
		}
		free(server->fds);
		user_list_destroy(&server->users);
	}
}

int main(int argc, char* argv[]) {

	// On vérfie les paramètres en entrée.
	if(argc != 2){
		printf("Error : invalid number of arguments.\n");
		return EXIT_FAILURE;
	}
	const char *PORT_NUMBER = argv[1];
	
	struct server server; 

	server_listen(&server, PORT_NUMBER);
	server_run(&server);
	server_close(&server);
	
	return EXIT_SUCCESS;
}

