// #include "broker.h"

// int main() {
//     int server_socket, max_sd;
//     struct sockaddr_in address;
//     int addrlen = sizeof(address);
//     fd_set readfds;
//     int client_sockets[MAX_CLIENTS] = {0};

//     // ============================Initialize broker server==================================//
//     server_socket = create_websocket();
//     // =====================================================================================//

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

//         // activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

//         if (FD_ISSET(server_socket, &readfds)) {
//             int new_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen);
//             for (int i = 0; i < MAX_CLIENTS; i++) {
//                 if (client_sockets[i] == 0) {
//                     client_sockets[i] = new_socket;
//                     break;
//                 }
//             }
//         }

//         // for (int i = 0; i < MAX_CLIENTS; i++) {
//         //     int sd = client_sockets[i];
//         //     if (FD_ISSET(sd, &readfds)) {
//         //         // Verificar se é uma mensagem de publicação ou assinatura
//         //         // e chamar a função apropriada
//         //         printf("RECEBIDO\n");
//         //         receive_publish(sd);
//         //         receive_subscribe(sd);
//         //     }
//         // }

//     }

//     return 0;
// }

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
#include <errno.h>

#define PORT 1883
#define MAX_CLIENTS 10
#define MAX_TOPICS 5

typedef struct {
    char topic[256];
    int client_sockets[MAX_CLIENTS];
} Topic;

Topic topics[MAX_TOPICS];


//Function Prototypes
void handle_client(int client_socket);
void receive_publish(int client_socket);
void process_publish(const char *message);
void distribute_message(const char *topic, const char *message);
void receive_subscribe(int client_socket);
void process_subscribe(int client_socket, const char *message);
void add_subscription(int client_socket, const char *topic);
int is_subscribed(int client_socket, const char *topic);





int main() {
    int server_socket, client_socket, max_sd, activity, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;
    int client_sockets[MAX_CLIENTS] = {0};

    // Cria o socket do servidor
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Associa o socket ao endereço e porta
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Escuta por conexões
    if (listen(server_socket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        max_sd = server_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_socket, &readfds)) {
            int new_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                // Verificar se é uma mensagem de publicação ou assinatura
                // e chamar a função apropriada
                receive_publish(sd);
                receive_subscribe(sd);
            }
        }
    }

    return 0;
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

void receive_publish(int client_socket) {
    char buffer[1024];
    int valread = read(client_socket, buffer, 1024);
    if (valread > 0) {
        buffer[valread] = '\0';
        // Processar a mensagem publicada
        process_publish(buffer);
    }
}

void process_publish(const char *message) {
    // Extrair o tópico e a mensagem
    char topic[256];
    char payload[768];
    sscanf(message, "%s %s", topic, payload);

    // Encaminhar a mensagem para os assinantes do tópico
    distribute_message(topic, payload);
}

void distribute_message(const char *topic, const char *message) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (is_subscribed(topics->client_sockets[i], topic)) {
            send(topics->client_sockets[i], message, strlen(message), 0);
        }
    }
}
void receive_subscribe(int client_socket) {
    char buffer[1024];
    int valread = read(client_socket, buffer, 1024);
    if (valread > 0) {
        buffer[valread] = '\0';
        // Processar a solicitação de assinatura
        process_subscribe(client_socket, buffer);
    }
}

void process_subscribe(int client_socket, const char *message) {
    char topic[256];
    sscanf(message, "%s", topic);

    // Adicionar o cliente à lista de assinantes do tópico
    add_subscription(client_socket, topic);
}
void add_subscription(int client_socket, const char *topic) {
    // Adicionar lógica para armazenar a assinatura
    // Pode ser uma lista ou um mapa de tópicos para clientes
}

int is_subscribed(int client_socket, const char *topic) {
    // Verificar se o cliente está inscrito no tópico
    // Retornar 1 se estiver inscrito, 0 caso contrário
}