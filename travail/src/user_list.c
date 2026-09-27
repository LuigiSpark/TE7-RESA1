#include <stdlib.h>
#include <stdio.h>

#include "user_list.h"

struct user *user_list_find_by_nickname(user_list *users, const char *nickname);

struct user *user_list_find_by_socket(user_list *users, int socket_fd);

int user_list_add(user_list *users, struct user user){
    user_list new_node = (struct maillon*)malloc(sizeof(struct maillon));
	if(new_node == NULL) return EXIT_FAILURE;
	new_node->user = user;
    new_node->next = *users;
    *users = new_node;
	return EXIT_SUCCESS;
}

int user_list_remove(user_list *users, int socket_fd){
	user_list current = *users;
	user_list previous = NULL;
	if(*users == NULL){
		printf("Error while deleting: empty chain.");
		return EXIT_FAILURE;
	}
	while (current != NULL && current->user.fd != socket_fd) {
        previous = current;
        current = current->next;
    }

	if (current == NULL) {
        return -1;
    }

   	if (previous == NULL) { // Fd is the first element of the list.
    	*users = current->next;
	} else {
		previous->next = current->next;
	}
    free(current);
    return EXIT_SUCCESS;
}

void user_list_destroy(user_list *users){
	if(*users != NULL){
		user_list_destroy(&(*users)->next);
		free(*users);
	}
}
