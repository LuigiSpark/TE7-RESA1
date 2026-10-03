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
#include "msg_struct.h"
#include "protocol.h"

#include <stdbool.h>
#include <ctype.h>

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
	return EXIT_SUCCESS;
}

int client_receive_message(struct client *client){
	struct message message;
	char* payload = NULL;
	int ret = protocol_recv_message(client->fd, &message, (void**)&payload);

	if(ret == 1){
		printf("Serveur deconnecté.\n");
		client_close(client);

		exit(EXIT_SUCCESS);
	}
	switch(message.type){
		case FILE_REQUEST:{
			printf("%s wants you to accept the transfer of the file named \"%s\".\n", message.infos, payload);
			int size = 0;
			char answer[3] = {0};

			while(strcmp(answer, "Y\n") != 0 && strcmp(answer, "N\n") != 0){
				printf("Do you accept? [Y/N]\n");
				size = 0;
				while ((answer[size++] = getchar()) != '\n'){
					if(size > 2)
						size = 2;
				}
				answer[size] = '\0';
			}
			struct message message_send;
			memset(&message_send, 0, sizeof(message_send));
			message_send.pld_len= 0;

			if(strcmp(answer, "Y\n") == 0){
				message_send.type = FILE_ACCEPT;
			}else{
				message_send.type = FILE_REJECT;
			}
			client_send_message(client, &message_send, NULL);
			break;
		}
		default:{
			printf("%s\n", payload);
			break;
		}
	}	

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

			struct message message_send;
			memset(&message_send, 0, sizeof(message_send));

			// Le serveur reconnaît /quit dans le payload, pas dans le type.
			if(strcmp(payload, "/quit") == 0){
				message_send.pld_len = size;
				message_send.type = ECHO_SEND;
				
				client_send_message(client, &message_send, payload);
				printf("Disconnected.\n");
				client_close(client);
				return EXIT_SUCCESS; // quitter la fct pour retourner au main()

			//Command /who
			}else if(strcmp(payload, "/who") == 0){
				message_send.pld_len= 0;
				message_send.type = NICKNAME_LIST;
				client_send_message(client, &message_send, NULL);

			//Commande /whois User1
			}else if(strncmp(payload, "/whois ", 7) == 0){
				message_send.pld_len= 0;
				message_send.type = NICKNAME_INFOS;
				
				char* nickname = (payload + 7);
				strncpy(message_send.infos, nickname, INFOS_LEN-1);
				client_send_message(client, &message_send, NULL);

			//Commande /msgall Hello
			}else if(strncmp(payload, "/msgall ", 8) == 0){
				char* msg = payload + 8; //décalage du pointeur: pointe ver le msg directe
				message_send.pld_len = strlen(msg) + 1 ;//inclusion du '\0'
				message_send.type = BROADCAST_SEND;
				client_send_message(client, &message_send, msg);

			//Commande /msg user1 Hello
			}else if(strncmp(payload, "/msg ", 5) == 0){
				char* pseudo = payload + 5; 
				char* payload_msg = strchr(pseudo, ' '); // On cherche le 2eme espace, celui qui separt le msg du pseudo
				if(payload_msg == NULL){
					fprintf(stderr,"[Server] : /msg <pseudo> <message>\n");
					continue;
				}else{
					*payload_msg = '\0';
					payload_msg++; // Le msg commence juste après.

					message_send.pld_len = strlen(payload_msg) + 1 ;//inclusion du '\0'
					message_send.type = UNICAST_SEND;
					
					strncpy(message_send.infos, pseudo, INFOS_LEN-1);
					client_send_message(client, &message_send, payload_msg);
				}

			//Command /nick
			}else if(strncmp(payload, "/nick ", 6) == 0){

				char* nickname = (payload + 6);
				
				printf("%s\n",nickname);
				if(strlen(nickname) >= NICK_LEN){
					fprintf(stderr,"Invalid nickname (too long)\n");
					fflush(stdout);
					continue;
				}
				else if(strlen(nickname) == 0 || not_contain_only_digits_or_letters(nickname)){
					fprintf(stderr," [Server] : only digits and letters are accepted.\n"); 
					fflush(stdout);
					continue;
				}
				else{

					message_send.pld_len = 0;
					message_send.type = NICKNAME_NEW;

					strncpy(message_send.infos, nickname, INFOS_LEN-1);	
					client_send_message(client, &message_send, NULL);
				
				}
			//ECHO normale
			}else if(strncmp(payload, "/send ", 6) == 0){
				char* pseudo = payload + 6; 
				char* file_name = strchr(pseudo, ' '); // On cherche le 2eme espace, celui qui separt le msg du pseudo
				if(file_name == NULL){
					fprintf(stderr,"[Server] : /send <pseudo> <file name>\n");
					continue;
				}else{
					*file_name = '\0';
					file_name++;  // Le nom du fichier commence juste après.

					message_send.pld_len = strlen(file_name) + 1 ;//inclusion du '\0'
					message_send.type = FILE_REQUEST;

					strncpy(message_send.infos, pseudo, INFOS_LEN-1);
					client_send_message(client, &message_send, file_name);
				}

			//Command /nick
			}
			else{
				message_send.type = ECHO_SEND;
				message_send.pld_len = strlen(payload) + 1;
				client_send_message(client, &message_send, payload);
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
		client->fd = -1; // Sécurité : évite les doubles close
	}
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

