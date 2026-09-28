#include "protocol.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


void die(int ret, char* msg){
	if(ret < 0){
		perror(msg);
		exit(EXIT_FAILURE);
	}
}

int protocol_send_all(int socket_fd, const void *buffer, size_t length){
    int ret;
    size_t sent = 0;
    do{
        ret = write(socket_fd, (char*)buffer + sent, length - sent);
        die(ret, "Error while wrinting");
		sent += ret;
    }while(sent != length);
    return ret + 1;
}

// Return 1 if the socket connection is closed.
int protocol_recv_all(int socket_fd, void *buffer, size_t length){
    int ret;
    size_t received = 0;
    while(received < length){
        ret = read(socket_fd, (char*)buffer + received, length - received);
        die(ret, "Error while reading");

        if(ret == 0){
            break;
        }
        received += ret;
    }
    return received;
}


int protocol_send_message(int socket_fd, const struct message *message, const void *payload){
    protocol_send_all(socket_fd, message, sizeof(struct message));
	protocol_send_all(socket_fd, payload, message->pld_len);
    return EXIT_SUCCESS;
}

// Struct message should be initialised before. 
int protocol_recv_message(int socket_fd, struct message *message, void **payload){
    int ret = protocol_recv_all(socket_fd, message, sizeof(struct message));
    if(ret == 1) return 1; //Prevent from reading again if connection is closed.

	*payload = (char*)malloc(sizeof(char)*message->pld_len);

	ret = protocol_recv_all(socket_fd, *payload, sizeof(char)*message->pld_len);
    return ret; 
}

int protocol_validate_message(const struct message *message);