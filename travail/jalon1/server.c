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


int echo_server(int sockfd) {
	// Read size
	size_t size;
	int ret = secure_read(sockfd, &size, sizeof(size));
	if(ret == 0) return 1; //Code for closing, 0 is used by EXIT_SUCCESS.

	// Read message.
	char* message = (char*)malloc(sizeof(char)*size);

	ret = secure_read(sockfd, message, size);
	if(ret == 0) return 1;

	if(strncmp(message, "/quit", 5) == 0){
		return 1;
	}

	printf("Received: %s", message);

	// Sending message (ECHO)
	secure_write(sockfd, &size, sizeof(size));
	secure_write(sockfd, message, size);

	printf("Message sent!\n");
	
	free(message);
	return EXIT_SUCCESS;
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

void accept_init_fds(struct pollfd* fds, int sfd){
	struct sockaddr cli;
	socklen_t len = sizeof(cli);

	int client_fd = accept(sfd, (struct sockaddr*) &cli, &len);
	die(client_fd, "Error while accepting");

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
					accept_init_fds(fds, connect_fd);
				}else{ // client sending message. 
					int ret = echo_server(fds[i].fd);
					if(1 == ret){
						printf("disconnected");
						close(fds[i].fd);
						fds[i].fd = -1;
					}
					fds[i].revents = 0;
				}
			}
		}
	}


	
	return EXIT_SUCCESS;
}

