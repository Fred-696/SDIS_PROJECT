#include "broker.h"

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

void receive_publish(int client_socket) {
    char buffer[1024];
    int valread = read(client_socket, buffer, 1024);
    printf("valread: %d\n", valread);
    if (valread > 0) {
        buffer[valread] = '\0';
        // Processar a mensagem publicada
        for(size_t i = 0; i< valread; i++){
            printf(" %02X", (unsigned char)buffer[i]);

        }
        process_publish(buffer);
        
    }
}

void process_publish(const char *message) {
    // Extrair o tópico e a mensagem
    char topic[256];
    char payload[768];
    sscanf(message, "%s %s", topic, payload);
    // printf("%s, %s")

    // Encaminhar a mensagem para os assinantes do tópico
    distribute_message(topic, payload);
}

void distribute_message(const char *topic, const char *message) {
    printf("Topico: %s", topic);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (is_subscribed(topic1->client_sockets[i], topic)) {
            send(topic1->client_sockets[i], message, strlen(message), 0);
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


void print_local_ip() {
    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }

    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, ip, INET_ADDRSTRLEN);
            if (strcmp(ip, "127.0.0.1") != 0) {
                printf("Server is running at IP: %s || Port: %d\n", ip, PORT);
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
}