#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <errno.h>

#define BROKER_PORT 1883
#define MAX_CLIENTS 10
#define MAX_TOPICS 5

typedef struct {
    char topic[256];
    int client_sockets[MAX_CLIENTS];
} Topic;

//function creates server at local ip and given port
int init_server_socket(int port);

//Function Prototypes
void handle_client(int client_socket);
// void receive_publish(int client_socket);
// void process_publish(const char *message);
// void distribute_message(const char *topic, const char *message);
// void receive_subscribe(int client_socket);
// void process_subscribe(int client_socket, const char *message);
// void add_subscription(int client_socket, const char *topic);
// int is_subscribed(int client_socket, const char *topic);