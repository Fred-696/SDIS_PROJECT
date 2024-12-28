#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <errno.h>

#ifndef _ARPA_INET_H_
#define _ARPA_INET_H_

#endif
//=============================================================//

#define BROKER_PORT 1883
#define MAX_CLIENTS 4
#define MAX_TOPICS 5
#define QOS 1

#define BUFFER_SIZE 1024

typedef struct {
    //fixed header
    uint8_t pck_type;
    uint8_t flag;
    ssize_t remaining_len;

    //variable Header (depends on the packet type)
    uint8_t *variable_header;

    //payload
    ssize_t payload_len;
    uint8_t *payload;
} mqtt_pck;


typedef struct {
    int connfd;                   //connection file descriptor
    bool connected;               //connected or not
    char topic[MAX_TOPICS][256];  //Client's subscripted topics (maximum all topics)

    mqtt_pck packet;
} session;

#ifndef MQTT_RETURN_CODES_H
#define MQTT_RETURN_CODES_H

//MQTT Connect Return Code Responses
#define MQTT_CONN_ACCEPTED                  0x00  // Connection accepted
#define MQTT_CONN_REFUSED_ID_REJECTED       0x02  // Connection Refused, identifier rejected

#endif // MQTT_RETURN_CODES_H




//function creates server at local ip and given port
int create_tcpserver(int *server_fd, struct sockaddr_in *address, int *addrlen);
int mqtt_process_pck(uint8_t *buffer, mqtt_pck received_pck, session* running_session);
int connect_handler(mqtt_pck *received_pck, session* running_session);


//Function Prototypes
void handle_client(int client_socket);
// void receive_publish(int client_socket);
// void process_publish(const char *message);
// void distribute_message(const char *topic, const char *message);
// void receive_subscribe(int client_socket);
// void process_subscribe(int client_socket, const char *message);
// void add_subscription(int client_socket, const char *topic);
// int is_subscribed(int client_socket, const char *topic);