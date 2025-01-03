#include "broker.h"

//function creates server at local ip and given port
int create_tcpserver(int *server_fd, struct sockaddr_in *address, int *addrlen) {
    //create socket
    if ((*server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { //IPv4, stream-oriented (TCP)
        perror("Socket creation failed");
        return -1;
    }

    int opt = 1;
    //set SO_REUSEADDR to allow reusing the address to avoid init errors
    if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(*server_fd);
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
            printf("Client disconnected: conn_fd: %d | forcing connection close\n", conn_fd);
            close(conn_fd);
            pthread_exit(NULL);
        }

        mqtt_pck received_pck = {0};
        received_pck.conn_fd = conn_fd;

        //Process MQTT packet
        if (mqtt_process_pck(buffer, received_pck, running_sessions) < 0) {
            printf("MQTT Process Error\n");
        }
        printf("|||||||||||||||||||||||\n");
        usleep(10);
    }
    return NULL;
}

//function to decode the remaining length
int decode_remaining_length(uint8_t *buffer, uint8_t *remaining_length, int *offset) {
    int multiplier = 1;
    uint8_t encoded_byte;
    *remaining_length = 0;
    *offset = 1;
    for (int i = 0; i < 4; i++) { //remaining Length can take up to 4 bytes
        encoded_byte = buffer[*offset];
        *remaining_length += (encoded_byte & 127) * multiplier;
        multiplier *= 128;

        if (multiplier > 128 * 128 * 128) { //malformed packet check
            printf("Malformed Remaining Length\n");
            return -1;
        }

        (*offset)++;

        if ((encoded_byte & 128) == 0) { //MSB = 0 indicates end of length encoding
            break;
        }
    }
    return 0; 
}

//function to encode the remaining length
int encode_remaining_length(uint8_t *buffer, size_t remaining_len) {
    int bytes_written = 0;

    do {
        uint8_t encoded_byte = remaining_len % 128;
        remaining_len /= 128;
        //if there is more data to encode, set the continuation bit (MSB = 1)
        if (remaining_len > 0) {
            encoded_byte |= 128;
        }
        buffer[bytes_written++] = encoded_byte;
    } while (remaining_len > 0 && bytes_written < 4);

    //return number of bytes used for the Remaining Length
    return bytes_written;
}

//function to easily made package(only from a filled variable of type structure mqtt_pck)
int send_pck(mqtt_pck *package) {
    //buffer to encode the Remaining Length (max 4 bytes)
    uint8_t remaining_length_encoded[4];
    int remaining_length_size = encode_remaining_length(remaining_length_encoded, package->remaining_len);
    if (remaining_length_size < 1 || remaining_length_size > 4) {
        printf("Failed to encode Remaining Length\n");
        return -1;
    }

    //calculate total size of the packet
    size_t total_size = 1 + remaining_length_size; //fixed Header (1 byte for pck_type + flags) + Remaining Length
    if (package->variable_header) {
        total_size += package->variable_len; //variable Header size
    }
    if (package->payload) {
        total_size += package->payload_len; //payload size
    }

    // Allocate a buffer for the serialized packet
    uint8_t *buffer = malloc(total_size);
    if (!buffer) {
        perror("Failed to allocate buffer for packet serialization");
        return -1;
    }

    //get offset
    size_t offset = 0;
    buffer[offset++] = (package->pck_type << 4) | (package->flag & 0x0F); //packet Type and flags

    memcpy(&buffer[offset], remaining_length_encoded, remaining_length_size);
    offset += remaining_length_size;

    //serialize the Variable Header
    if (package->variable_header) {
        memcpy(&buffer[offset], package->variable_header, package->variable_len);
        offset += package->variable_len;
    }

    //serialize the Payload
    if (package->payload) {
        memcpy(&buffer[offset], package->payload, package->payload_len);
        offset += package->payload_len;
    }

    //send the serialized packet
    ssize_t bytes_sent = send(package->conn_fd, buffer, offset, 0);
    if (bytes_sent < 0) {
        perror("Failed to send packet");
        free(buffer);
        return -1;
    }

    //clean up
    free(buffer);
    printf("Packet sent to conn_fd %d || ", package->conn_fd);
    return 0;
}

//determine type of packet and process
int mqtt_process_pck(uint8_t *buffer, mqtt_pck received_pck, session* running_sessions){
    //======================Analise Fixed Header 1st byte=======================================//
    received_pck.flag = buffer[0] & 0x0F;             //0->4 flag
    received_pck.pck_type = (buffer[0] >> 4) & 0x0F;  //4->7 control packet type
    
    //==============================Decode remaining package length=============================//
    int offset = 1;
    uint8_t remaining_length;
    if (decode_remaining_length(buffer, &remaining_length, &offset) < 0) {
        return -1; //error decoding Remaining Length
    }
    received_pck.remaining_len = remaining_length;
    printf("Packet Received || conn_fd: %d || ", received_pck.conn_fd);
    // printf("Flag: %d || Package Type: %d || Remaining Length: %ld || ", received_pck.flag, received_pck.pck_type, received_pck.remaining_len);

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
        received_pck.variable_len = 10;
        received_pck.variable_header = malloc(received_pck.variable_len); //allocate 10 bytes (CONNECT variable header)
        if (received_pck.variable_header == NULL) {
            perror("Failed to allocate memory for variable header");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.variable_header, buffer + offset, received_pck.variable_len); //copy 10 bytes from buffer starting at offset
        
        //compute payload len
        received_pck.payload_len = received_pck.remaining_len - received_pck.variable_len;

        //fill payload
        received_pck.payload = malloc(received_pck.payload_len); //allocate 10 bytes (CONNECT variable header)
        if (received_pck.payload == NULL) {
            perror("Failed to allocate memory for payload");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.payload, buffer + offset + received_pck.variable_len, received_pck.payload_len); //copy X bytes from buffer starting after variable header

        return connect_handler(&received_pck, running_sessions); //interpret connect command
    
    case 3: //PUBLISH
        printf("PUBLISH\n");
        //fill variable header
        received_pck.topic_len = (buffer[offset] << 8) | buffer[offset + 1];
        received_pck.variable_len = received_pck.topic_len + 4;

        received_pck.variable_header = malloc(received_pck.variable_len); //allocate bytes (PUBLISH variable header)
        if (received_pck.variable_header == NULL) {
            perror("Failed to allocate memory for variable header");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.variable_header, buffer + offset, received_pck.variable_len); //copy bytes from buffer starting at offset
        
        //compute payload len
        received_pck.payload_len = received_pck.remaining_len - received_pck.variable_len;

        //fill payload
        received_pck.payload = malloc(received_pck.payload_len); //allocate bytes (PUBLISH variable header)
        if (received_pck.payload == NULL) {
            perror("Failed to allocate memory for payload");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.payload, buffer + offset + received_pck.variable_len, received_pck.payload_len); //copy X bytes from buffer starting after variable header

        return publish_handler(&received_pck, running_sessions); //interpret publish command
    
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
        received_pck.variable_len = 2;
        //fill variable header
        received_pck.variable_header = malloc(received_pck.variable_len); // Allocate var_head bytes (SUBSCRIBE variable header, CONTAINS PACKET ID!!!)
        if (received_pck.variable_header == NULL) {
            perror("Failed to allocate memory for variable header");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.variable_header, buffer + offset, received_pck.variable_len); //copy var_head bytes from buffer starting at offset
        
        // Extract Packet ID
        received_pck.pck_id = (buffer[offset] << 8) | buffer[offset + 1]; //copy packet id bytes to mqtt_pck

        //compute payload len
        received_pck.payload_len = received_pck.remaining_len - received_pck.variable_len;

        //fill payload
        received_pck.payload = malloc(received_pck.payload_len); // Allocate var_head bytes (CONNECT variable header)
        if (received_pck.payload == NULL) {
            perror("Failed to allocate memory for payload");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.payload, buffer + offset + received_pck.variable_len, received_pck.payload_len); //copy X bytes from buffer starting after variable header

        return subscribe_handler(&received_pck, running_sessions);

    case 12:
        printf("PING Request\n");
        return send_pingresp(&received_pck);

    case 14:
        printf("DISCONNECT\n");
        return disconnect_handler(&received_pck, running_sessions);
    default:
        return -1;
    }
    return 0;
}

//disconnects client properly
int disconnect_handler(mqtt_pck *received_pck, session* running_sessions){
    //find the running session with matching conn_fd
    session *current_session = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (running_sessions[i].conn_fd == received_pck->conn_fd) {
            current_session = &running_sessions[i];
            break;
        }
    }
    if (current_session == NULL) {
        printf("Session not found for conn_fd: %d\n", received_pck->conn_fd);
        return -1;
    }

    printf("DISCONNECTION || conn_fd: %d || Client_ID: '%s'\n", current_session->conn_fd, current_session->client_id);
    current_session->last_pck_received_id = 0; //reset last packet id
    close(current_session->conn_fd);
    pthread_exit(NULL);
}

//handle(interprets) CONNECT packet
int connect_handler(mqtt_pck *received_pck, session* running_sessions){
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
        if (running_sessions[i].client_id != NULL) {
            //compare existing session client_id with the received client_id
            if (strcmp(running_sessions[i].client_id, client_id) == 0) {
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
    running_sessions[session_idx].client_id = client_id;
    running_sessions[session_idx].conn_fd = received_pck->conn_fd;
    running_sessions[session_idx].keepalive = keepalive;

    printf("Valid Protocol || Keepalive: %d || Client_ID: %s || SessionIdx: %d\n", keepalive, client_id, session_idx);

    //assign the new connection to the corresponding session
    return send_connack(&running_sessions[session_idx], return_code, session_present);
}

//Prepares and sends connack package
int send_connack(session* current_session, int return_code, int session_present) {
    mqtt_pck connack_packet;

    //fixed Header
    connack_packet.flag = 0;
    connack_packet.pck_type = 2;
    connack_packet.remaining_len = 2;

    //variable Header
    connack_packet.variable_len = 2;
    connack_packet.variable_header = malloc(connack_packet.variable_len); //allocate 7 bytes (PUBLISH variable header)
    if (!connack_packet.variable_header) {
        perror("Failed to allocate memory for CONNACK variable header");
        return -1;
    }
    connack_packet.variable_header[0] = session_present & 0x01; // Reserved(0000) || SessionPresent(which is 1 or 0)
    connack_packet.variable_header[1] = return_code; //Connect Return Code (only 0x00 or 0x01)

    //payload
    connack_packet.payload_len = 0; //n payload
    connack_packet.payload = NULL;  //set pointer to NULL

    //conn_fd
    connack_packet.conn_fd = current_session->conn_fd;
    if (send_pck(&connack_packet) < 0){
        printf("Failed to send CONNACK\n");
        free(connack_packet.variable_header);
        return -1;
    }
    printf("CONNACK sent successfully\n");
    return 0;
}


//Sends PingResp package(no need for handler before)
int send_pingresp(mqtt_pck *received_pck) {
    mqtt_pck pingresp_packet;

    //fixed Header
    pingresp_packet.flag = 0;
    pingresp_packet.pck_type = 13; //PINGRESP packet type
    pingresp_packet.remaining_len = 0; //remaining length is 0

    //no Variable Header
    pingresp_packet.variable_header = NULL;

    //no Payload
    pingresp_packet.payload = NULL;
    pingresp_packet.payload_len = 0;

    //connection file descriptor
    pingresp_packet.conn_fd = received_pck->conn_fd;

    if (send_pck(&pingresp_packet) < 0) {
        perror("Failed to send PING packet");
        return -1;
    }

    printf("PINGRESP sent successfully\n");
    return 0;
}

//handle(interprets) PUBISH packet
int publish_handler(mqtt_pck *received_pck, session* running_sessions) {
    //find the running session with matching conn_fd
    session *current_session = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (running_sessions[i].conn_fd == received_pck->conn_fd) {
            current_session = &running_sessions[i];
            break;
        }
    }
    if (current_session == NULL) {
        printf("Session not found for conn_fd: %d\n", received_pck->conn_fd);
        return -1;
    }

    int Retain = received_pck->flag & 0x01;
    if (Retain != 0) {
        printf("Invalid Retain\n");
    }
    int QOS_lvl = (received_pck->flag >> 1) & 0x03;
    if (QOS_lvl != 1) {
        printf("Invalid QOS level\n");
    }

    //check if its first time the client sent the message
    int DUP = (received_pck->flag >> 3) & 0x01;

    char topic[received_pck->topic_len + 1];

    memcpy(topic, received_pck->variable_header + 2, received_pck->topic_len);
    topic[received_pck->topic_len] = '\0';

    printf("Topic: %s\n", topic);

    int pck_id_offset = 2 + received_pck->topic_len;
    int pck_id = (received_pck->variable_header[pck_id_offset] << 8) |
                 received_pck->variable_header[pck_id_offset + 1];
    
    printf("DUP: %d || Topic: '%s' || pck_id: %d\n", DUP, topic, pck_id);

    // verify it wasn't received before
    if (pck_id != current_session->last_pck_received_id) {
        printf("New message to publish\n");
        current_session->last_pck_received_id = pck_id;

        // Find subscribed clients and send the message
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (running_sessions[i].client_id != NULL) {
                for (int j = 0; j < MAX_TOPICS; j++) {
                    if (strcmp(running_sessions[i].topic[j], topic) == 0) {
                        printf("Sending message to client '%s' of conn_fd %d and subscribed to topic '%s'\n", running_sessions[i].client_id, running_sessions[i].conn_fd, topic);
                        send_publish(received_pck, &running_sessions[i]);
                    }
                }
            }
        }
    } else {
        printf("Duplicated message\n");
        return 0;
    }
    return send_puback(current_session, pck_id);
}

int send_publish(mqtt_pck *received_pck, session* running_session){
    //copy the received pck and save it 
    running_session->pck_to_send = *received_pck;        //associate pending message with subsribed client's session
    running_session->pck_to_send.conn_fd = running_session->conn_fd; //destination of packet associated with found subscribed client's session
    //foward message
    if (send_pck(&running_session->pck_to_send) < 0){
        printf("Failed to send PUBLISH\n");
        free(running_session->pck_to_send.variable_header);
        free(running_session->pck_to_send.payload);
        return -1;
    }
    //do not clean untill puback received
    printf("PUBLISH sent successfully\n");
    return 0;
}

int send_puback(session* current_session, int pck_id){
    mqtt_pck puback_packet;

    //fixed Header
    puback_packet.flag = 0;
    puback_packet.pck_type = 4;
    puback_packet.remaining_len = 2;

    //variable Header
    puback_packet.variable_len = 2;
    puback_packet.variable_header = malloc(puback_packet.variable_len); //allocate 7 bytes (PUBLISH variable header)
    if (!puback_packet.variable_header) {
        perror("Failed to allocate memory for PUBACK variable header");
        return -1;
    }
    puback_packet.variable_header[0] = (pck_id >> 8) & 0xFF; // MSB of pck_id
    puback_packet.variable_header[1] = pck_id & 0xFF;        // LSB of pck_id

    //payload
    puback_packet.payload_len = 0; //n payload
    puback_packet.payload = NULL;  //set pointer to NULL

    //conn_fd
    puback_packet.conn_fd = current_session->conn_fd;
    if (send_pck(&puback_packet) < 0){
        printf("Failed to send PUBACK\n");
        free(puback_packet.variable_header);
        free(puback_packet.payload);
        return -1;
    }

    //Clean up allocated memory
    free(puback_packet.variable_header);
    free(puback_packet.payload);
    printf("PUBACK sent successfully\n");
    return 0;
}

//handle SUBSCRIBE packet
int subscribe_handler(mqtt_pck *received_pck, session *running_sessions) {
    //find the running session with matching conn_fd
    session *current_session = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (running_sessions[i].conn_fd == received_pck->conn_fd) {
            current_session = &running_sessions[i];
            break;
        }
    }
    if (current_session == NULL) {
        printf("Session not found for conn_fd: %d\n", received_pck->conn_fd);
        return -1;
    }

    // Check packet ID
    uint16_t pck_id = received_pck->pck_id;
    printf("Packet ID: %d\n", pck_id);

    // Process the payload
    int offset = 0;
    int num_topics = 0;

    while (offset < received_pck->payload_len) {
        // Check topic length from the first two bytes of the payload
        uint16_t topic_len = (received_pck->payload[offset] << 8) | received_pck->payload[offset + 1];
        offset += 2;

        // Ensure topic length is within valid range
        if (topic_len <= 0 || topic_len > received_pck->payload_len - offset) {
            printf("Invalid topic length: %d\n", topic_len);
            return -1;
        }

        // Topic name extraction
        char topic[topic_len + 1];  // +1 for null-terminator
        memcpy(topic, received_pck->payload + offset, topic_len);
        topic[topic_len] = '\0'; // Properly terminate the topic string
        offset += topic_len;

        // Check if the QoS is valid
        uint8_t qos = received_pck->payload[offset];
        if (qos != 1) { // Only supporting QoS level 1 for now
            printf("Ignoring topic '%s' with unsupported QoS level: %d\n", topic, qos);
            offset += 1;
            continue;
        }
        offset++; // Move past the QoS byte

        // Check if the topic already exists in the client's session
        bool topic_exists = false;
        for (int i = 0; i < MAX_TOPICS; i++) {
            if (strcmp(current_session->topic[i], topic) == 0) {
                topic_exists = true;
                printf("Topic '%s' already exists in the session\n", topic);
                break;
            }
        }

        // Store the topic if it's new
        if (!topic_exists) {
            for (int i = 0; i < MAX_TOPICS; i++) {
                if (current_session->topic[i][0] == '\0') { // Empty slot found
                    strncpy(current_session->topic[i], topic, sizeof(current_session->topic[i]) - 1);
                    current_session->topic[i][sizeof(current_session->topic[i]) - 1] = '\0';  // Ensure null termination
                    printf("Stored new topic: '%s' in the session\n", topic);
                    break;
                }
            }
        }

        num_topics++;
    }

    // Send a SUBACK packet back to the client after processing all topics
    return send_suback(current_session, pck_id, num_topics);
}

//send SUBACK
int send_suback(session *current_session, int pck_id, int num_topics) {
    mqtt_pck suback_packet;

    //fixed Header
    suback_packet.flag = 0;
    suback_packet.pck_type = 9; // SUBACK control packet type
    suback_packet.remaining_len = 2 + num_topics; // Packet Identifier (2 bytes) + Payload

    //variable Header (Packet Identifier)
    suback_packet.variable_len = 2;
    suback_packet.variable_header = malloc(suback_packet.variable_len); 
    if (!suback_packet.variable_header) {
        perror("Failed to allocate memory for SUBACK variable header");
        return -1;
    }
    suback_packet.variable_header[0] = (pck_id >> 8) & 0xFF; // MSB of pck_id
    suback_packet.variable_header[1] = pck_id & 0xFF;        // LSB of pck_id

    //payload (QoS Levels for each topic)
    suback_packet.payload_len = num_topics;
    suback_packet.payload = malloc(num_topics);
    if (!suback_packet.payload) {
        perror("Failed to allocate memory for SUBACK payload");
        free(suback_packet.variable_header);
        return -1;
    }
    
    //setting QoS level for each topic (set to QoS 1 for all)
    for (int i = 0; i < num_topics; i++) {
        suback_packet.payload[i] = 0x01;  // QoS 1 for each topic
    }

    //assign the connection file descriptor
    suback_packet.conn_fd = current_session->conn_fd;

    //Send the SUBACK packet using send_pck
    if (send_pck(&suback_packet) < 0) {
        printf("Failed to send SUBACK\n");
        free(suback_packet.variable_header);
        free(suback_packet.payload);
        return -1;
    }

    //Clean up allocated memory
    free(suback_packet.variable_header);
    free(suback_packet.payload);

    printf("SUBACK sent successfully for Packet_ID: %d\n", pck_id);
    return 0;
}