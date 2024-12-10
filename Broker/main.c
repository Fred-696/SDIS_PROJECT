#include "broker.h"

int main() {
    int server_socket, max_sd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;
    int client_sockets[MAX_CLIENTS] = {0};

    // ============================Initialize broker server==================================//
    server_socket = create_websocket();
    // =====================================================================================//

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

        // activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_socket, &readfds)) {
            int new_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
        }

        // for (int i = 0; i < MAX_CLIENTS; i++) {
        //     int sd = client_sockets[i];
        //     if (FD_ISSET(sd, &readfds)) {
        //         // Verificar se é uma mensagem de publicação ou assinatura
        //         // e chamar a função apropriada
        //         printf("RECEBIDO\n");
        //         receive_publish(sd);
        //         receive_subscribe(sd);
        //     }
        // }

    }

    return 0;
}