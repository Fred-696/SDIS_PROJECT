#include "broker.h"

//function creates server at local ip and given port
int create_websocket(void) {
    int server_socket;
    struct sockaddr_in address;

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(BROKER_PORT);

    // Bind the socket to the specified address and port
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Socket binding failed");
        close(server_socket); // Clean up the socket before exiting
        exit(EXIT_FAILURE);
    }

    //============================= Print the public IP address======================//
    char public_ip[INET_ADDRSTRLEN];
    struct ifaddrs *ifap, *ifa;
    void *tmp_addr_ptr;

    // Get the list of network interfaces
    if (getifaddrs(&ifap) == -1) {
        perror("getifaddrs failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Iterate through the interfaces
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family == AF_INET) { // Check for IPv4
            tmp_addr_ptr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            // Exclude loopback addresses (127.0.0.1)
            if (strcmp(ifa->ifa_name, "lo") != 0) {
                inet_ntop(AF_INET, tmp_addr_ptr, public_ip, INET_ADDRSTRLEN);
                printf("Server created at IP: %s, Port: %d\n", public_ip, BROKER_PORT);
                break;  // We found a valid public address, no need to continue
            }
        }
    }
    freeifaddrs(ifap);  // Free the allocated memory
    //===============================================================================//
    return server_socket;
}

void send_connack(int client_socket, uint8_t session_present, uint8_t return_code) {
    uint8_t connack_packet[4];

    // Fixed Header
    connack_packet[0] = 0x20; // MQTT Control Packet (0010) || Reserved(0000)
    connack_packet[1] = 0x02; // Remaining Length (0000) || (0010)

    // Variable Header
    connack_packet[2] = session_present & 0x01; // Reserved(0000) || SessionPresent(which is 1 or 0)
    connack_packet[3] = return_code; // Connect Return Code (only 0x00 or 0x02)

    // Send the packet
    send(client_socket, connack_packet, 4, 0); //CONNACK allways has 4 bytes
}












Topic topic1[MAX_TOPICS];

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
