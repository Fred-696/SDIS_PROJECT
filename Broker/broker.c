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
    switch (received_pck.pck_type)
    {
    case 1: //CONNECT
        printf("CONNECT Packet\n");
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
        printf("PUBLISH Packet\n");
        return -1;
    
    case 4: //PUBLISH ACKNOWLEDGE
        printf("PUBACK Packet\n");
        return -1;

    case 8: //SUBSCRIBE
        printf("SUBSCRIBE Packet\n");
        return -1;

    default:
        return -1;
    }
    return 0;
}


int connect_handler(mqtt_pck *received_pck, session* running_session){
    int return_code = 0; 
    //check variable header
    uint8_t expected_protocol[8] = {0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x04, 0x02};
    if (memcmp(received_pck->variable_header, expected_protocol, 8) != 0){
        printf("Invalid protocol\n");
        return_code = 1;
    }
    running_session->keepalive = received_pck->variable_header[9];
    printf("Valid Protocol || Keepalive: %d\n", running_session->keepalive);
    
    //Check payload
    return send_connack(running_session, return_code);

}

int send_connack(session* running_session, int return_code) {
    uint8_t connack_packet[4];

    //fixed Header
    connack_packet[0] = 0x20; // MQTT Control Packet (0010) || Reserved(0000)
    connack_packet[1] = 0x02; // Remaining Length (0000) || (0010)

    //variable Header
    connack_packet[2] = session_present & 0x01; // Reserved(0000) || SessionPresent(which is 1 or 0)
    connack_packet[3] = return_code; //Connect Return Code (only 0x00 or 0x01)
}




void handle_client(int client_socket) {
    char buffer[1024];
    int valread = read(client_socket, buffer, 1024);
    if (valread == 0) {
        close(client_socket);
    } else {
        buffer[valread] = '\0';
        printf("Message received: %s\n", buffer);
    }
}

// void receive_publish(int client_socket) {
//     char buffer[1024];
//     int valread = read(client_socket, buffer, 1024);
//     printf("valread: %d\n", valread);
//     if (valread > 0) {
//         buffer[valread] = '\0';
//         // Processar a mensagem publicada
//         for(size_t i = 0; i< valread; i++){
//             printf(" %02X", (unsigned char)buffer[i]);

//         }
//         process_publish(buffer);
        
//     }
// }

// void process_publish(const char *message) {
//     // Extrair o tópico e a mensagem
//     char topic[256];
//     char payload[768];
//     sscanf(message, "%s %s", topic, payload);
//     // printf("%s, %s")

//     // Encaminhar a mensagem para os assinantes do tópico
//     distribute_message(topic, payload);
// }

// void distribute_message(const char *topic, const char *message) {
//     printf("Topico: %s", topic);
//     for (int i = 0; i < MAX_CLIENTS; i++) {
//         if (is_subscribed(topic1->client_sockets[i], topic)) {
//             send(topic1->client_sockets[i], message, strlen(message), 0);
//         }
//     }
// }
// void receive_subscribe(int client_socket) {
//     char buffer[1024];
//     int valread = read(client_socket, buffer, 1024);
//     if (valread > 0) {
//         buffer[valread] = '\0';
//         // Processar a solicitação de assinatura
//         process_subscribe(client_socket, buffer);
//     }
// }

// void process_subscribe(int client_socket, const char *message) {
//     char topic[256];
//     sscanf(message, "%s", topic);

//     // Adicionar o cliente à lista de assinantes do tópico
//     add_subscription(client_socket, topic);
// }
// void add_subscription(int client_socket, const char *topic) {
//     // Adicionar lógica para armazenar a assinatura
//     // Pode ser uma lista ou um mapa de tópicos para clientes
//     printf("add_subscription function");
// }

// int is_subscribed(int client_socket, const char *topic) {
//     // Verificar se o cliente está inscrito no tópico
//     // Retornar 1 se estiver inscrito, 0 caso contrário
//     printf("is_subscribed function");
// }
