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

//function to decode the remaining length
int decode_remaining_length(uint8_t *buffer, uint8_t *remaining_length, int *offset) {
    int multiplier = 1;
    uint8_t encoded_byte;
    *remaining_length = 0;
    *offset = 1; //
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

//function to easily made package(only fill a variable of type structure mqtt_pck)
int send_pck(mqtt_pck *package) {
    // Buffer to encode the Remaining Length (max 4 bytes)
    uint8_t remaining_length_encoded[4];
    int remaining_length_size = encode_remaining_length(remaining_length_encoded, package->remaining_len);
    if (remaining_length_size < 1 || remaining_length_size > 4) {
        printf("Failed to encode Remaining Length\n");
        return -1;
    }

    // Calculate total size of the packet
    size_t total_size = 1 + remaining_length_size; // Fixed Header (1 byte for pck_type + flags) + Remaining Length
    if (package->variable_header) {
        total_size += package->remaining_len; // Variable Header size
    }
    if (package->payload) {
        total_size += package->payload_len; // Payload size
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
        memcpy(&buffer[offset], package->variable_header, package->remaining_len);
        offset += package->remaining_len;
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
    return 0;
}

//determine type of packet and process
int mqtt_process_pck(uint8_t *buffer, mqtt_pck received_pck, session* running_session){
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
        received_pck.variable_header = malloc(10); //allocate 10 bytes (CONNECT variable header)
        if (received_pck.variable_header == NULL) {
            perror("Failed to allocate memory for variable header");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.variable_header, buffer + offset, 10); //copy 10 bytes from buffer starting at offset
        
        //compute payload len
        received_pck.payload_len = received_pck.remaining_len - 10;

        //fill payload
        received_pck.payload = malloc(received_pck.payload_len); //allocate 10 bytes (CONNECT variable header)
        if (received_pck.payload == NULL) {
            perror("Failed to allocate memory for payload");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.payload, buffer + offset + 10, received_pck.payload_len); //copy X bytes from buffer starting after variable header

        return connect_handler(&received_pck, running_session); //interpret connect command
    
    case 3: //PUBLISH
        printf("PUBLISH\n");
        //fill variable header
        received_pck.variable_header = malloc(7); //allocate 7 bytes (PUBLISH variable header)
        if (received_pck.variable_header == NULL) {
            perror("Failed to allocate memory for variable header");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.variable_header, buffer + offset, 10); //copy 7 bytes from buffer starting at offset
        
        //compute payload len
        received_pck.payload_len = received_pck.remaining_len - 10;

        //fill payload
        received_pck.payload = malloc(received_pck.payload_len); //allocate 10 bytes (CONNECT variable header)
        if (received_pck.payload == NULL) {
            perror("Failed to allocate memory for payload");
            exit(EXIT_FAILURE);
        }
        memcpy(received_pck.payload, buffer + offset + 7, received_pck.payload_len); //copy X bytes from buffer starting after variable header

        return publish_handler(&received_pck, running_session); //interpret publish command
    
    case 4: //PUBLISH ACKNOWLEDGE
        printf("PUBACK\n");
        return -1;

    case 8: //SUBSCRIBE
        printf("SUBSCRIBE\n");
        return -1;

    case 12:
        printf("PING Request\n");
        return send_pingresp(&received_pck);
    default:
        return -1;
    }
    return 0;
}

//Prepares and sends connack package
int send_connack(session* running_session, int return_code, int session_present) {
    mqtt_pck connack_packet;

    //fixed Header
    connack_packet.flag = 0;
    connack_packet.pck_type = 2;
    connack_packet.remaining_len = 2;

    //variable Header
    connack_packet.variable_header = malloc(2); //allocate 7 bytes (PUBLISH variable header)
    connack_packet.variable_header[0] = session_present & 0x01; // Reserved(0000) || SessionPresent(which is 1 or 0)
    connack_packet.variable_header[1] = return_code; //Connect Return Code (only 0x00 or 0x01)

    //payload
    connack_packet.payload_len = 0; //n payload
    connack_packet.payload = NULL;  //set pointer to NULL

    //conn_fd
    connack_packet.conn_fd = running_session->conn_fd;
    if (send_pck(&connack_packet) < 0){
        printf("Failed to send CONNACK\n");
        return -1;
    }
    printf("CONNACK sent successfully\n");
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

// int publish_handler
///////////////////////////////////////// int puback_handler
// int subscribe_handler
// int send_suback
///////////////////////////////////////// int send_puback
// int send_publish

//handle(interprets) PUBISH packet
int publish_handler(mqtt_pck *received_pck, session* running_session){
    int QOS_lvl = (received_pck->flag >> 1) & 0x03;
    printf("QOS_lvl = %d\n", QOS_lvl);
    return -1;
}