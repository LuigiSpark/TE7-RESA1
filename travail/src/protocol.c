#include "protocol.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


const char *msg_type_str[] = {
	"NICKNAME_NEW",
	"NICKNAME_LIST",
	"NICKNAME_INFOS",
	"ECHO_SEND",
	"UNICAST_SEND",
	"BROADCAST_SEND",
	"MULTICAST_CREATE",
	"MULTICAST_LIST",
	"MULTICAST_JOIN",
	"MULTICAST_SEND",
	"MULTICAST_QUIT",
	"FILE_REQUEST",
	"FILE_ACCEPT",
	"FILE_REJECT",
	"FILE_SEND",
	"FILE_ACK"
};


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
        die(ret, "Error while writing");
		sent += ret;
    }while(sent != length);
    return ret + 1;
}

// Return 1 if the socket connection is closed, 0 when all bytes were received.
int protocol_recv_all(int socket_fd, void *buffer, size_t length){
    int ret;
    size_t received = 0;
    while(received < length){
        ret = read(socket_fd, (char*)buffer + received, length - received);
        die(ret, "Error while reading");

        if(ret == 0){
            return 1;
        }
        received += ret;
    }
    return 0;
}


int protocol_send_message(int socket_fd, const struct message *message, const void *payload){
    protocol_send_all(socket_fd, message, sizeof(struct message));
    if(message->pld_len != 0){
        protocol_send_all(socket_fd, payload, message->pld_len);
    }
    return EXIT_SUCCESS;
}

// Struct message should be initialised before. 
/* 1 = connexion fermée. 0 = message complet. */
int protocol_recv_message(int socket_fd, struct message *message, void **payload){
    int ret = protocol_recv_all(socket_fd, message, sizeof(struct message));
    if(ret == 1) return 1;

    if(message->pld_len == 0){
        *payload = NULL;
        return EXIT_SUCCESS;
    }

    *payload = malloc((size_t)message->pld_len);
    if(*payload == NULL) return 1;

    ret = protocol_recv_all(socket_fd, *payload, (size_t)message->pld_len);
    if(ret == 1){
        free(*payload);
        *payload = NULL;
        return 1;
    }
    return EXIT_SUCCESS;
}

int protocol_validate_message(const struct message *message){  
    if (message->type < NICKNAME_NEW || message->type > FILE_ACK){
        fprintf(stderr, "[Protocole Error]: Message type unknown\n");
        return EXIT_FAILURE;
    }

    if (message->pld_len > PROTO_MAX_PAYLOAD){
        fprintf(stderr, "[Protocole Error]: Payload length %u exceeds limit\n", message->pld_len);
        return EXIT_FAILURE;
    }

    int infos_has_null= 1;
    int nick_has_null= 1;
    for (int i=0; i<128; i++){
        if (message->infos[i]== '\0'){
            infos_has_null=0;
        }
        if (message->nick_sender[i]== '\0'){
            nick_has_null= 0;
        }
    }
    if (infos_has_null + nick_has_null > 0 ){
        fprintf(stderr, "[Protocol Error]: String fields must be null-terminated within 128 bytes\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
