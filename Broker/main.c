#include "broker.h"

void print_bytes(const uint8_t *buffer, ssize_t length) {
    printf("Received Bytes:\n");
    for (ssize_t i = 0; i < length; i++) {
        printf("%02X ", buffer[i]); // Print each byte in hexadecimal
    }
    printf("\n");
}

int main() {
    int server_fd; //file descriptor
    struct sockaddr_in address;    //struct with family, port, address
    int addrlen = sizeof(address);
    session running_session[MAX_CLIENTS] = {0};                //1 session for each node

    //loop variables
    uint8_t  buffer[BUFFER_SIZE] = {0};    //buffer
    int connfd;
    int session_id;

    // ============================Initialize broker server==================================//
    if (create_tcpserver(&server_fd, &address, &addrlen) < 0){
        exit(EXIT_FAILURE);
    };
    // =====================================================================================//
       
    while (1) {
        printf("looping\n"); //just for testing remove later
        if ((connfd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0){
            perror("Connection accepted error");
            exit(EXIT_FAILURE);
        }
        printf("New connection: connfd = %d\n", connfd);
        //check if ongoing session
        session_id = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            //check for an ongoing session
            if (running_session[i].connfd == connfd) {
                printf("Ongoing session found at index %d\n", i);
                session_id = i;
                break;
            }
            //if sessionid not found, use the first available session slot
            else if (session_id == -1 && running_session[i].connfd == 0) {
                session_id = i;
            }
        }

        //assign the new connection to the corresponding session
        if (running_session[session_id].connfd == 0) {
            printf("Assigning connfd %d to session ID %d\n", connfd, session_id);
            running_session[session_id].connfd = connfd;
            running_session[session_id].connected = true; //register session as connected
        }

        //read data from client
        ssize_t valread;
        if ((valread = read(running_session[session_id].connfd, buffer, BUFFER_SIZE)) < 0){
            perror("Reading error\n");
        }

        //Process MQTT packet
        if (mqtt_process_pck(buffer) < 0){
            printf("Process error\n");
        };

        printf("size: %ld\n", valread);
        print_bytes(buffer, valread); // Print the data in byte form
        memset(buffer, 0, sizeof(buffer)); //set memory of buffer to 0
        printf("finished\n");
        sleep(1);
    }

    // return 0;
}

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// //#include <arpa/inet.h>
// #include <sys/select.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <errno.h>

// #define PORT 1883
// #define MAX_CLIENTS 10
// #define MAX_TOPICS 5

// typedef struct {
//     char topic[256];
//     int client_sockets[MAX_CLIENTS];
// } Topic;

// Topic topics[MAX_TOPICS];


// //Function Prototypes
// void handle_client(int client_socket);
// void receive_publish(int client_socket);
// void process_publish(const char *message);
// void distribute_message(const char *topic, const char *message);
// void receive_subscribe(int client_socket);
// void process_subscribe(int client_socket, const char *message);
// void add_subscription(int client_socket, const char *topic);
// int is_subscribed(int client_socket, const char *topic);





// int main() {
//     int server_socket, client_socket, max_sd, activity, new_socket;
//     struct sockaddr_in address;
//     int addrlen = sizeof(address);
//     fd_set readfds;
//     int client_sockets[MAX_CLIENTS] = {0};

//     // Cria o socket do servidor
//     if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
//         perror("socket failed");
//         exit(EXIT_FAILURE);
//     }

//     address.sin_family = AF_INET;
//     address.sin_addr.s_addr = INADDR_ANY;
//     address.sin_port = htons(PORT);

//     // Associa o socket ao endereço e porta
//     if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
//         perror("bind failed");
//         exit(EXIT_FAILURE);
//     }

//     // Escuta por conexões
//     if (listen(server_socket, 3) < 0) {
//         perror("listen");
//         exit(EXIT_FAILURE);
//     }

//     while (1) {
//         FD_ZERO(&readfds);
//         FD_SET(server_socket, &readfds);
//         max_sd = server_socket;

//         for (int i = 0; i < MAX_CLIENTS; i++) {
//             int sd = client_sockets[i];
//             if (sd > 0) FD_SET(sd, &readfds);
//             if (sd > max_sd) max_sd = sd;
//         }

//         activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

//         if (FD_ISSET(server_socket, &readfds)) {
//             int new_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen);
//             for (int i = 0; i < MAX_CLIENTS; i++) {
//                 if (client_sockets[i] == 0) {
//                     client_sockets[i] = new_socket;
//                     break;
//                 }
//             }
//         }

//         for (int i = 0; i < MAX_CLIENTS; i++) {
//             int sd = client_sockets[i];
//             if (FD_ISSET(sd, &readfds)) {
//                 // Verificar se é uma mensagem de publicação ou assinatura
//                 // e chamar a função apropriada
//                 receive_publish(sd);
//                 receive_subscribe(sd);
//             }
//         }
//     }

//     return 0;
// }

// void handle_client(int client_socket) {
//     char buffer[1024];
//     int valread = read(client_socket, buffer, 1024);
//     if (valread == 0) {
//         close(client_socket);
//     } else {
//         buffer[valread] = '\0';
//         printf("Message received: %s\n", buffer);
//     }
// }

// void receive_publish(int client_socket) {
//     char buffer[1024];
//     int valread = read(client_socket, buffer, 1024);
//     if (valread > 0) {
//         buffer[valread] = '\0';
//         // Processar a mensagem publicada
//         process_publish(buffer);
//     }
// }

// void process_publish(const char *message) {
//     // Extrair o tópico e a mensagem
//     char topic[256];
//     char payload[768];
//     sscanf(message, "%s %s", topic, payload);

//     // Encaminhar a mensagem para os assinantes do tópico
//     distribute_message(topic, payload);
// }

// void distribute_message(const char *topic, const char *message) {
//     for (int i = 0; i < MAX_CLIENTS; i++) {
//         if (is_subscribed(topics->client_sockets[i], topic)) {
//             send(topics->client_sockets[i], message, strlen(message), 0);
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
// }

// int is_subscribed(int client_socket, const char *topic) {
//     // Verificar se o cliente está inscrito no tópico
//     // Retornar 1 se estiver inscrito, 0 caso contrário
// }