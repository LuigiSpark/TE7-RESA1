#ifndef CLIENT_H
#define CLIENT_H

struct client;
struct message;

int client_connect(struct client *client, const char *host, const char *port); // Implemented.
int client_run(struct client *client);
int cut_in_half(char*str, char sep, char**first_half, char**second_half);
int client_send_message(struct client *client, const struct message *message, const void *payload);
int client_receive_message(struct client *client); 
int client_open_file_listener(struct client *client, const char *port);
void client_close(struct client *client);

#endif
