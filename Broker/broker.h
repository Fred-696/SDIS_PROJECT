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


#ifndef MQTT_RETURN_CODES_H
#define MQTT_RETURN_CODES_H

//MQTT Connect Return Code Responses
#define MQTT_CONN_ACCEPTED                  0x00  // Connection accepted
#define MQTT_CONN_REFUSED_ID_REJECTED       0x02  // Connection Refused, identifier rejected

#endif // MQTT_RETURN_CODES_H




//function creates server at local ip and given port
int create_websocket(void);

//Function Prototypes
void handle_client(int client_socket);
// void receive_publish(int client_socket);
// void process_publish(const char *message);
// void distribute_message(const char *topic, const char *message);
// void receive_subscribe(int client_socket);
// void process_subscribe(int client_socket, const char *message);
// void add_subscription(int client_socket, const char *topic);
// int is_subscribed(int client_socket, const char *topic);