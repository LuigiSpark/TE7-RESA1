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

bool not_contain_only_digits_or_letters(char string[]){
	int s_len = strlen(string);
	for(int i = 0; i < s_len; i++){
		if(!isalnum(string[i])) {
			return true; 
		}
	}
	return false;
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

			struct message message_rcv;
			message_rcv.pld_len = size;

			//Commande /quit
			if(strcmp(payload, "/quit") == 0){
				client_send_message(client, &message_rcv, payload);
				printf("Disconnected.\n");
				client_close(client);
				printf("succes de commande\n");

			//Command /who
			}else if(strncmp(payload, "/who ", 5) == 0){
				memset(&message_rcv, 0, sizeof(message_rcv));
				message_rcv.type = NICKNAME_LIST;
				client_send_message(client, &message_rcv, NULL);

			//Commande /whois User1
			}else if(strncmp(payload, "/whois ", 7) == 0){
				char* cible = (payload + 7);
				memset(&message_rcv, 0, sizeof(message_rcv));
				message_rcv.type = NICKNAME_INFOS;
				strncpy(message_rcv.infos, cible, INFOS_LEN-1);
				client_send_message(client, &message_rcv, NULL);

			//Commande /msgall Hello
			}else if(strncmp(payload, "/msg ", 5) == 0){
				memset(&message_rcv, 0, sizeof(message_rcv));
				message_rcv.pld_len = size;
				message_rcv.type = BROADCAST_SEND;
				client_send_message(client, &message_rcv, payload);

			//Commande /msg user1 Hello
			}else if(strncmp(payload, "/msg ", 5) == 0){
				char* destinataire = (payload + 5);
				memset(&message_rcv, 0, sizeof(message_rcv));
				message_rcv.pld_len = size;
				message_rcv.type = UNICAST_SEND;
				strncpy(message_rcv.infos, destinataire, INFOS_LEN-1);
				client_send_message(client, &message_rcv, payload);

			//Command /nick
			}else if(strncmp(payload, "/nick ", 6) == 0){

				char* nickname = (payload + 6);
				
				printf("%s\n",nickname);
				if(strlen(nickname) >= NICK_LEN ){
					printf("Invalid nickname (too long)\n");
				}
				else if(not_contain_only_digits_or_letters(nickname)){
					printf("Invalid nickname (invalid characters)\n");
				}
				else{
					printf("valide name!\n");
					struct message message;
					memset(&message, 0, sizeof(message));
					message.pld_len = 0;

					message.type = NICKNAME_NEW;

					strncpy(message.infos, nickname, INFOS_LEN-1);	
					client_send_message(client, &message, NULL);
				
				}
			//ECHO normale
			}else{
				client_send_message(client, &message_rcv, payload);
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

