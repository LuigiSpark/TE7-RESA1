#ifndef USER_LIST_H
#define USER_LIST_H

#include <arpa/inet.h>
#include "msg_struct.h" // For NICK_LEN.

struct user{
    int fd;
	int index; // in fds;
	char IPv4[INET_ADDRSTRLEN];
	int port; 
	char nickname[NICK_LEN];
	time_t connect_time;
};

typedef struct maillon{
	struct user user;
	struct maillon* next; 
}*user_list;

struct user *user_list_find_by_nickname(user_list *users, const char *nickname);
struct user *user_list_find_by_socket(user_list *users, int socket_fd);
int user_list_add(user_list *users, struct user user);
int user_list_remove(user_list *users, int socket_fd);
void user_list_destroy(user_list *users);

#endif
