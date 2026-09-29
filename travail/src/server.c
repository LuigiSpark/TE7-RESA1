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
#include <time.h>

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

void server_die_ptr(void *ptr, struct server *server, char *msg){
    if(ptr == NULL){
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

	printf("Listening on port %s\n", port);
	return EXIT_SUCCESS;
}

int server_run(struct server *server){

	server->users = NULL;

	server->fds = (struct pollfd*)calloc((SOMAXCONN+1), sizeof(struct pollfd));
	server_die_ptr(server->fds, server, "Error while allocating pollfd structure");
	

	struct pollfd* fds = server->fds;
	fds[0].fd = server->listen_fd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	// Initialise the fd to -1
	for(int i = 1; i < SOMAXCONN+1; i++){
		fds[i].fd = -1; 
	}
	printf("Server is running...\n");
	
	while(1){
		int ret = poll(fds, SOMAXCONN + 1, -1);
		server_die(ret, server, "Error while polling");

		for(int i = 0; i < SOMAXCONN + 1; i++){
			if(fds[i].revents & POLLIN){
				if(0 == i){ //if it's a new connection. 
					server_accept_client(server);
				}else{ // client sending message. 
					struct user* sender = user_list_find_by_socket(&server->users, fds[i].fd);

					server_receive_message(server, sender);

					fds[i].revents = 0;
				}
			}
		}
	}
}

int server_handle_message(struct server *server, struct user *sender, const struct message *message, void *payload){
	
	if(payload != NULL && strcmp(payload, "/quit") == 0){
		printf("%s quit\n", sender->nickname);
		free(payload);
		return 1;
	}
	struct message m_resp = {0};
	char payload_resp[1024] = {0};
	switch(message->type){

		case NICKNAME_NEW:{ // /nick. 

			struct user* user = user_list_find_by_nickname(&(server->users), message->infos);

			if(user == NULL){  //Nikename available.
				char* ret_ptr = strcpy(sender->nickname,message->infos);
				server_die_ptr(ret_ptr, server,"Error while copying nickname"); 
				printf("Nick set: %s\n", sender->nickname);

				m_resp.pld_len = snprintf(payload_resp, sizeof(payload_resp), "[Server] : Welcome %s\n", sender->nickname);
			}else{ // Nickname already taken.
				char* ret_ptr = strcpy(payload_resp, "[Serveur] : nickname already exists.\n");
				server_die_ptr(ret_ptr, server,"Error while copying nickname");
				printf("Nick refused: %s\n", message->infos);
			}
			m_resp.pld_len = strlen(payload_resp);
			m_resp.type = NICKNAME_NEW; 
			protocol_send_message(sender->fd, &m_resp, payload_resp);

			break;
		}
		case NICKNAME_LIST:{ // /who. 
			printf("%s asked /who\n", sender->nickname);
			size_t written = 0; 
			written += snprintf(payload_resp + written, sizeof(payload_resp) - written, "[Serveur] : the connected users are : \n");

			user_list current = server->users;
			while(current != NULL){
				written += snprintf(payload_resp + written, sizeof(payload_resp) - written, "%s\n", current->user.nickname);
				current = current->next;
			}
			m_resp.type = NICKNAME_LIST;
			m_resp.pld_len = written;
			protocol_send_message(sender->fd, &m_resp, payload_resp);
			break;
		}
		case NICKNAME_INFOS:{ // /whois <pseudo>
			printf("%s asked /whois %s\n", sender->nickname, message->infos);
			struct user* target = user_list_find_by_nickname(&(server->users), message->infos);
			if(target == NULL){
				m_resp.pld_len = snprintf(payload_resp, sizeof(payload_resp),"[Server] : this nickname doesn't exist.");
			}else{
				m_resp.pld_len = snprintf(payload_resp, sizeof(payload_resp),
				"[Server] : %s connected since %s with IP address %s and port number %d\n",
				target->nickname, ctime(&target->connect_time), target->IPv4, target->port);
			}
			m_resp.type = NICKNAME_INFOS; 
			protocol_send_message(sender->fd, &m_resp, payload_resp);

			break;
		}
		case ECHO_SEND: // without command
			m_resp.type = ECHO_SEND;
			if(sender->nickname[0] != '\0'){
				printf("Echo from %s\n", sender->nickname);
				m_resp.pld_len = snprintf(payload_resp, sizeof(payload_resp),"[%s] : %s", sender->nickname, (char *)payload);
			}else{
				printf("Echo from unknown user\n");
				m_resp.pld_len = snprintf(payload_resp, sizeof(payload_resp), "[Me] : %s", (char*)payload);

			}
			protocol_send_message(sender->fd, &m_resp, payload_resp);
			break;

		case UNICAST_SEND:{ // /msg <pseudo> <message>
			if(sender->nickname[0] == '\0'){  // if nickname is not define. 
				m_resp.pld_len = snprintf(payload_resp, sizeof(payload_resp),"[Server] : create a pseudo with /nick first.");
				protocol_send_message(sender->fd, &m_resp, payload_resp);
			}else{
				printf("%s sent /msg to %s\n", sender->nickname, message->infos);
				struct user* target = user_list_find_by_nickname(&(server->users), message->infos);
				m_resp.type = UNICAST_SEND; 
				if(target == NULL){
					m_resp.pld_len = snprintf(payload_resp, sizeof(payload_resp),"[Server] : this nickname doesn't exist.");
					protocol_send_message(sender->fd, &m_resp, payload_resp);

				}else{
					m_resp.pld_len = snprintf(payload_resp, sizeof(payload_resp),"[%s] : %s", sender->nickname, (char*)payload);
					protocol_send_message(target->fd, &m_resp, payload_resp);

				}
			}
			break;
		}
		case BROADCAST_SEND:{ // /msgall <message>
			if(sender->nickname[0] == '\0'){  // if nickname is not define. 
				m_resp.pld_len = snprintf(payload_resp, sizeof(payload_resp),"[Server] : create a pseudo with /nick first.");
				protocol_send_message(sender->fd, &m_resp, payload_resp);

			}else{
				printf("%s sent /msgall\n", sender->nickname);
				user_list current = server->users;
				while(current != NULL){
					if(current->user.fd != sender->fd){
						memset(payload_resp, 0, sizeof(payload_resp));
						m_resp.pld_len = snprintf(payload_resp, sizeof(payload_resp),"[%s] : %s", sender->nickname, (char*)payload);
						m_resp.type = BROADCAST_SEND; 
						protocol_send_message(current->user.fd, &m_resp, payload_resp);
					}
					current = current->next;
				}
			}
			break;
		}

		default:
			printf("Unsupported message type %s\n", msg_type_str[message->type]);
			break;
	}

	
	
	free(payload);
	return EXIT_SUCCESS;
}


int server_receive_message(struct server *server, struct user *sender){
	struct message message;
	char*payload;

	int ret = protocol_recv_message(sender->fd, &message, (void**)&payload);
	if(1 == ret){
		printf("Client disconnected\n");
		close(sender->fd);
        server->fds[sender->index].fd = -1;
		user_list_remove(&server->users, sender->fd); 
		return EXIT_SUCCESS;
	}
	return server_handle_message(server, sender, &message, payload);
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
	user.connect_time = time(NULL);
	user.nickname[0] = '\0';

	struct pollfd* fds = server->fds;
	for(int i = 0; i < SOMAXCONN+1; i++){
		if(fds[i].fd == -1){
			fds[i].fd = client_fd;
			fds[i].events = POLLIN;
			fds[i].revents = 0;
			user.index = i;
			break;
		}
	}

	user_list_add(&server->users, user);
	printf("Client connected: %s:%d\n", ip_client, port_client);

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

