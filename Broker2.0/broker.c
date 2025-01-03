#include "broker.h"

//function creates server at local ip and given port
int create_tcpserver(int *server_fd, struct sockaddr_in *address, int *addrlen) {
    //create socket
    if ((*server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { //IPv4, stream-oriented (TCP)
        perror("Socket creation failed");
        return -1;
    }

    //setup address
    address->sin_family = AF_INET;            //address family to IPv4
    address->sin_addr.s_addr = INADDR_ANY;    //accepting connectons on any available interface
    address->sin_port = htons(BROKER_PORT);   //set port in network byte order

    //bind the socket to the specified address and port
    if (bind(*server_fd, (struct sockaddr *)address, *addrlen) < 0) { //cast to simple struct sockaddr
        perror("Socket binding failed");
        close(*server_fd); //clean up the socket before exiting
        return -1;
    }

    //listen for incoming connections
    if (listen(*server_fd, MAX_CLIENTS) < 0){ 
        perror("Listening failed\n");
        close(*server_fd); //clean up the socket before exiting
        return -1;
    }
    printf("Server created sucessfully and listening\n");
    return 0;
}

//main loop function, for each thread
void *client_handler(void *arg) {
    printf("Thread created\n");
    thread_data *t_data = (thread_data *)arg; //cast to thread data type again
    int conn_fd = t_data->conn_fd;
    session *running_sessions = t_data->running_sessions;
    free(t_data); //no longer needed, free

    uint8_t buffer[BUFFER_SIZE] = {0};

    // Handle client connection
    while (1) {
        ssize_t valread = read(conn_fd, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            printf("Client disconnected: conn_fd: %d\n", conn_fd);
            close(conn_fd);
            pthread_exit(NULL);
        }

        mqtt_pck received_pck = {0};
        received_pck.conn_fd = conn_fd;

        // Process MQTT packet
        if (mqtt_process_pck(buffer, received_pck, running_sessions) < 0) {
            printf("MQTT Process Error\n");
        }

        usleep(10);
    }
    return NULL;
}

//determine type of packet and process
int mqtt_process_pck(uint8_t *buffer, mqtt_pck received_pck, session* running_session){
    //======================Analise Fixed Header 1st byte=======================================//
    received_pck.flag = buffer[0] & 0x0F;             //0->4 flag
    received_pck.pck_type = (buffer[0] >> 4) & 0x0F;  //4->7 control packet type
    
    //==============================Decode remaining package length=============================//
    int multiplier = 1;
    uint8_t encoded_byte;
    int offset = 1; //start after 1st byte
    for (int i = 0; i < 4; i++){ //encoded remaining bytes info takes max 4 bytes
        encoded_byte = buffer[offset + i];
        received_pck.remaining_len += (encoded_byte & 127) * multiplier;
        multiplier *= 128;
        if (multiplier > 128*128*128){
            printf("Malformed Remaining Length\n");
            return -1;
        }
        if ((encoded_byte & 128) == 0){ //if MSB=0 (128 = b'10000000')
            offset += i + 1; //to know where variable header starts
            break;
        }
    }
    printf("Flag: %d || Package Type: %d || Remaining Length: %ld\n", received_pck.flag, received_pck.pck_type, received_pck.remaining_len);

    //=============Determine packet type received from a Client=================//
    printf("Packet Type: ");
    switch (received_pck.pck_type)
    {
    case 1: //CONNECT
        printf("CONNECT\n");
        if (received_pck.flag != 0){ //flag must be 0 for CONNECT
            printf("Invalid flag for CONNECT\n");
            return -1;
        }
        //fill variable header
        received_pck.variable_header = malloc(10); // Allocate 10 bytes (CONNECT variable header)
        if (received_pck.variable_header == NULL) {
            perror("Failed to allocate memory for variable header");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.variable_header, buffer + offset, 10); //copy 10 bytes from buffer starting at offset
        
        //compute payload len
        received_pck.payload_len = received_pck.remaining_len - 10;

        //fill payload
        received_pck.payload = malloc(received_pck.payload_len); // Allocate 10 bytes (CONNECT variable header)
        if (received_pck.payload == NULL) {
            perror("Failed to allocate memory for payload");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.payload, buffer + offset + 10, received_pck.payload_len); //copy X bytes from buffer starting after variable header
        
        return connect_handler(&received_pck, running_session); //interpret connect command
    
    case 3: //PUBLISH
        printf("PUBLISH\n");
        return -1;
    
    case 4: //PUBLISH ACKNOWLEDGE
        printf("PUBACK\n");
        return -1;

    case 8: //SUBSCRIBE
        printf("SUBSCRIBE\n");
        if (received_pck.flag != 2){ //flag must be 0b1000 for SUBSCRIBE
            printf("Invalid flag for SUBSCRIBE\n");
            return -1;
        }
        //size of variable header for this packet
        int variable_header_lenght = 2;
        //fill variable header
        received_pck.variable_header = malloc(variable_header_lenght); // Allocate var_head bytes (SUBSCRIBE variable header, CONTAINS PACKET ID!!!)
        if (received_pck.variable_header == NULL) {
            perror("Failed to allocate memory for variable header");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.variable_header, buffer + offset, variable_header_lenght); //copy var_head bytes from buffer starting at offset
        
        // Extract Packet ID
        received_pck.packet_id = (buffer[offset] << 8) | buffer[offset + 1]; //copy packet id bytes to mqtt_pck

        //compute payload len
        received_pck.payload_len = received_pck.remaining_len - variable_header_lenght;

        //fill payload
        received_pck.payload = malloc(received_pck.payload_len); // Allocate var_head bytes (CONNECT variable header)
        if (received_pck.payload == NULL) {
            perror("Failed to allocate memory for payload");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.payload, buffer + offset + variable_header_lenght, received_pck.payload_len); //copy X bytes from buffer starting after variable header

        return sub_handler(&received_pck, running_session);

    case 12:
        printf("PING Request\n");
        return send_pingresp(&received_pck);
    default:
        return -1;
    }
    return 0;
}

//handle(interprets) CONNECT packet
int connect_handler(mqtt_pck *received_pck, session* running_session){
    int return_code = 0; 
    int session_present = 0;

    //check variable header
    uint8_t expected_protocol[8] = {0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x04, 0x02};
    if (memcmp(received_pck->variable_header, expected_protocol, 8) != 0){
        printf("Invalid protocol\n");
        return_code = 1;
    }
    int keepalive = received_pck->variable_header[9];
    
    //Check payload
    int id_len = (received_pck->payload[0] << 8)  | received_pck->payload[1];

    char* client_id = malloc(id_len);
    if (client_id == NULL) {
        perror("Failed to allocate memory for client id");
        exit(EXIT_FAILURE);
    }
    memcpy(client_id, received_pck->payload + 2, id_len);

    //check if client_id exists in any session
    int session_idx = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (running_session[i].client_id != NULL) {
            //compare existing session client_id with the received client_id
            if (strcmp(running_session[i].client_id, client_id) == 0) {
                printf("Ongoing session found for Client_ID: %s at index %d\n", client_id, i);
                session_idx = i;
                session_present = 1; // Mark session as present
                break;
            }
        } 
        else if (session_idx == -1) {
            //save first available slot for a new session
            session_idx = i;
        }
    }
    //associate client info with session
    running_session[session_idx].client_id = client_id;
    running_session[session_idx].conn_fd = received_pck->conn_fd;
    running_session[session_idx].keepalive = keepalive;

    printf("Valid Protocol || Keepalive: %d || Client_ID: %s || SessionIdx: %d\n", keepalive, client_id, session_idx);

    //assign the new connection to the corresponding session
    return send_connack(&running_session[session_idx], return_code, session_present);
}

//Prepares and sends connack package
int send_connack(session* running_session, int return_code, int session_present) {
    uint8_t connack_packet[4];

    //fixed Header
    connack_packet[0] = 0x20; // MQTT Control Packet (0010) || Reserved(0000)
    connack_packet[1] = 0x02; // Remaining Length (0000) || (0010)

    //variable Header
    connack_packet[2] = session_present & 0x01; // Reserved(0000) || SessionPresent(which is 1 or 0)
    connack_packet[3] = return_code; //Connect Return Code (only 0x00 or 0x01)
    if (send(running_session->conn_fd, connack_packet, sizeof(connack_packet), 0) < 0){
        printf("Failed to send CONNACK\n");
        return -1;
    }
    printf("CONNACK sent successfully\n");
    return 0;
}

//Sends PingResp package(no need for handler before)
int send_pingresp(mqtt_pck *received_pck) {
    uint8_t ping_packet[2] = {0};
    ping_packet[0] = 0xd0; // PINGRESP fixed header
    ping_packet[1] = 0x00; // Remaining length
    if (send(received_pck->conn_fd, ping_packet, sizeof(ping_packet), 0) < 0) {
        perror("Failed to send PING packet");
        return -1;
    }

    printf("PINGRESP sent successfully\n");
    return 0;
}

// Send SUBACK response
int send_suback(mqtt_pck *received_pck, session *running_session, int num_topics, uint8_t *topic_qos)  {
    uint8_t suback_packet[4 + num_topics]; // Fixed header (2 bytes) + Variable header (2 bytes) + Payload (num_topics bytes)

    // Fixed Header
    suback_packet[0] = 0x90; // SUBACK control packet type
    suback_packet[1] = 2 + num_topics; // Remaining length

    // Variable Header
    suback_packet[2] = (received_pck->packet_id >> 8) & 0xFF; // Packet Identifier MSB
    suback_packet[3] = received_pck->packet_id & 0xFF;        // Packet Identifier LSB

    // Payload
    for (int i = 0; i < num_topics; i++) {
        suback_packet[4 + i] = topic_qos[i]; // QoS level for each topic
    }

    // Debugging output
    printf("SUBACK packet: ");
    for (int i = 0; i < sizeof(suback_packet); i++) {
        printf("%02x ", suback_packet[i]);
    }
    printf("\n");

    // Send SUBACK packet
    if (send(running_session->conn_fd, suback_packet, sizeof(suback_packet), 0) < 0) {
        perror("Failed to send SUBACK");
        return -1;
    }

    printf("SUBACK sent successfully\n");
    return 0;
}



//handle SUBSCRIBE packet
int sub_handler(mqtt_pck *received_pck, session* running_session){

    //check packet id
    uint16_t packet_id = received_pck->packet_id;
    printf("Packet ID: %d\n", packet_id);

    // Process the payload
    // Process the payload
    int offset = 0;
    int num_topics = 0;
    uint8_t topic_qos[MAX_TOPICS];


    while (offset < received_pck->payload_len) {
        // Check topic lenght on 2bytes of variable header
        uint16_t topic_len = (received_pck->payload[offset] << 8) | received_pck->payload[offset + 1];
        offset += 2;

        // Topic name
        char topic[topic_len + 1];  //+1 for '\0' ################################### CHECK IF IT's NECESSARY
        memcpy(topic, received_pck->payload + offset, topic_len);
        topic[topic_len] = '\0';  // '\0' in last byte ##############################
        offset += topic_len;
        
        // QoS level
        uint8_t qos = received_pck->payload[offset];
        topic_qos[num_topics] = qos; // Store QoS of topic for SUBACK response

        offset += 1;

        // Check if the topic already exists
        bool topic_exists = false;
        for (int i = 0; i < MAX_TOPICS; i++) {
            if (strcmp(running_session->topic[i], topic) == 0) {
                topic_exists = true;
                printf("Topic %s already exists in the session\n", topic);
                break;
            }
        }

        // Store the subscription in the client's session if the topic does not exist
        if (!topic_exists) {
            for (int i = 0; i < MAX_TOPICS; i++) {
                if (running_session->topic[i][0] == '\0') { // Find an empty slot
                    strncpy(running_session->topic[i], topic, sizeof(running_session->topic[i]) - 1);
                    running_session->topic[i][sizeof(running_session->topic[i]) - 1] = '\0'; // Ensure null-termination
                    printf("Topic: %s stored in the session with QoS %d\n", topic, qos);
                    break;
                }
            }
        }

        num_topics++;
    }

    // Send SUBACK response
    return send_suback(received_pck, running_session, num_topics, topic_qos);
}

int send_pck(mqtt_pck *package, session* running_session);