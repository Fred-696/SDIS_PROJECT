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

#include <fcntl.h>
#include <pthread.h>

#ifndef _ARPA_INET_H_
#define _ARPA_INET_H_

#endif
//=============================================================//

#define BROKER_PORT 1883
#define MAX_CLIENTS 3
#define MAX_TOPICS 5
#define QOS 1

#define BUFFER_SIZE 1024

//packet structure
typedef struct {
    //fixed header
    uint8_t pck_type;
    uint8_t flag;
    ssize_t remaining_len;

    //variable Header (depends on the packet type)
    uint8_t *variable_header;

    //packet id
    uint16_t packet_id;

    //payload
    ssize_t payload_len;
    uint8_t *payload;

    int conn_fd;                   //connection file descriptor
} mqtt_pck;

//session required arguments to save
typedef struct {
    int conn_fd;                   //connection file descriptor
    int keepalive;                //time between finishing 1 package and next package, in seconds
    bool connected;               //connected or not
    char topic[MAX_TOPICS][256];  //Client's subscripted topics (at maximum all topics)

    char* client_id;
    mqtt_pck packet;
} session;

//for each thread
typedef struct {
    int conn_fd;
    session *running_sessions;
} thread_data;

#ifndef MQTT_RETURN_CODES_H
#define MQTT_RETURN_CODES_H

//MQTT Connect Return Code Responses
#define MQTT_CONN_ACCEPTED                  0x00  // Connection accepted
#define MQTT_CONN_REFUSED_ID_REJECTED       0x02  // Connection Refused, identifier rejected

#endif // MQTT_RETURN_CODES_H

//function creates server at local ip and given port
int create_tcpserver(int *server_fd, struct sockaddr_in *address, int *addrlen);
//main loop function, for each thread
void *client_handler(void *arg);

//handle(interprets) CONNECT packet
int connect_handler(mqtt_pck *received_pck, session* running_session);
//Prepares and sends connack package
int send_connack(session* running_session, int return_code, int session_present);
//Sends PingResp package(no need for handler before)
int send_pingresp(mqtt_pck *received_pck);

// Send SUBACK response
int send_suback(mqtt_pck *received_pck, session *running_session, int num_topics, uint8_t *topic_qos);
//handle SUBSCRIBE packet
int sub_handler(mqtt_pck *received_pck, session* running_session);

//determine type of packet and process
int mqtt_process_pck(uint8_t *buffer, mqtt_pck received_pck, session* running_session);
